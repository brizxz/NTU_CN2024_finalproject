#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <csignal>
#include <pthread.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unordered_map>
#include <fstream>
#include <portaudio.h>
#include "utils/audio_streaming.hpp"
#include "utils/video_streaming.hpp"
#include "utils/ssl.h"
#include "utils/file_transfer_relay.hpp"
#include "utils/const.h"
#include "utils/threadpool.hpp"

SSL* ssl;
SSL_CTX* ctx;

std::unordered_map<std::string, std::string> users;
std::unordered_map<std::string, int> connectedClients;
std::unordered_map<std::string, SSL*> connectedClientSSLs;

// 用來記錄每個 username 對應的 IP
std::unordered_map<std::string, std::string> userIPs;
// 用來記錄每個 username 對應的 p2p port
std::unordered_map<std::string, int> userPorts;

pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;

void* handleClient(void* sslPtr);

void signalHandler(int signum) {
    std::cout << "Signal " << signum << " received, shutting down server." << std::endl;
    cleanupSSLServer(ssl, ctx);
    exit(signum);
}

std::string menuString(bool loggedIn) {
    if (!loggedIn) return std::string(std::string("Commands:\n") +
            "1. REGISTER <username> <password>\n" +
            "2. LOGIN <username> <password> <p2pPort>\n"
            "3. EXIT\n>");
    else return std::string(std::string("Commands:\n") +
            "1. MESSAGE <username> <message>\n" +
            "2. DIRECT_MSG <username> <message>\n" +
            "3. SEND_FILE <username> <filepath>\n" +
            "4. STREAM AUDIO <filepath>\n" +
            "5. STREAM VIDEO <filepath>\n" + 
            "6. LOGOUT\n" + 
            "7. EXIT");

}


int main() {
    signal(SIGINT, signalHandler);
    initSSLServer(ssl, ctx);

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind to port." << std::endl;
        return 1;
    }

    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Failed to listen on socket." << std::endl;
        return 1;
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    ThreadPool pool(20);

    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept client connection." << std::endl;
            continue;
        }

        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, clientSocket);

        if (SSL_accept(ssl) <= 0) {
            std::cerr << "SSL handshake failed." << std::endl;
            ERR_print_errors_fp(stderr);
            close(clientSocket);
            SSL_free(ssl);
            continue;
        }

        // 原本是 pthread_create(&thread, nullptr, handleClient, (void*)sslPtr)
        // 改成使用 ThreadPool
        // enqueue 接受一個 lambda，把 ssl 當參數捕獲進來
        pool.enqueue([ssl]() {
            // 執行 handleClient(ssl)
            handleClient((void*)ssl);
        });
    }

    close(serverSocket);
    cleanupSSLServer(ssl, ctx);
    return 0;
}

void* handleClient(void* sslPtr) {
    SSL* ssl = (SSL*)sslPtr;
    char buffer[BUFFER_SIZE];
    bool loggedIn = false;
    std::string currentUser;
    std::string targetUser;
    while (true) {
        std::string menuStr = menuString(loggedIn);
        SSL_write(ssl, menuStr.c_str(), menuStr.size());
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = SSL_read(ssl, buffer, BUFFER_SIZE);
        if (bytesReceived <= 0) {
            std::cout << "Client disconnected." << std::endl;
            pthread_mutex_lock(&clientsMutex);
            connectedClients.erase(currentUser);
            connectedClientSSLs.erase(currentUser);
            pthread_mutex_unlock(&clientsMutex);
            break;
        }

        std::string command(buffer);
        std::cout << "Received: " << command << std::endl;
        if (command.substr(0, 8) == "REGISTER" && !loggedIn) {
            size_t firstSpace = command.find(' ', 9);
            if (firstSpace != std::string::npos) {
                std::string username = command.substr(9, firstSpace - 9);
                std::string password = command.substr(firstSpace + 1);
                
                pthread_mutex_lock(&clientsMutex);
                if (users.find(username) == users.end()) {
                    users[username] = password;
                    pthread_mutex_unlock(&clientsMutex);
                    SSL_write(ssl, "REGISTERED", 10);
                } else {
                    pthread_mutex_unlock(&clientsMutex);
                    SSL_write(ssl, "USER_EXISTS", 11);
                }
            } else {
                SSL_write(ssl, "INVALID_REGISTER", 16);
            }
        } else if (command.substr(0, 5) == "LOGIN" && !loggedIn) {
            // 假設 command = "LOGIN <username> <password> <p2pPort>"
            // step1: 找到第一個空白 (user), 第二個空白 (password), 剩下的視為 port
            size_t firstSpace = command.find(' ', 6); // 6 是 "LOGIN " 之後的位置
            if (firstSpace != std::string::npos) {
                size_t secondSpace = command.find(' ', firstSpace + 1);

                if (secondSpace != std::string::npos) {
                    std::string username = command.substr(6, firstSpace - 6);
                    std::string password = command.substr(firstSpace + 1, secondSpace - (firstSpace + 1));
                    std::string portStr  = command.substr(secondSpace + 1);

                    pthread_mutex_lock(&clientsMutex);

                    if (users.find(username) != users.end() && users[username] == password
                        && connectedClientSSLs.find(username) == connectedClientSSLs.end()) {

                        // 取得對端 IP
                        struct sockaddr_in addr;
                        socklen_t len = sizeof(addr);
                        getpeername(SSL_get_fd(ssl), (struct sockaddr*)&addr, &len);
                        std::string ip = inet_ntoa(addr.sin_addr);

                        userIPs[username] = ip;

                        // 將portStr轉成 int，若轉換失敗就給個預設port
                        int p2pPort = 22222; // 或任何你希望的預設值
                        try {
                            p2pPort = std::stoi(portStr);
                        } catch (...) {
                            std::cerr << "Invalid port format, use default 22222\n";
                        }
                        userPorts[username] = p2pPort;
                    
                        connectedClientSSLs[username] = ssl;
                        loggedIn = true;
                        currentUser = username;
                        connectedClients[currentUser] = SSL_get_fd(ssl);
                        pthread_mutex_unlock(&clientsMutex);
                        SSL_write(ssl, "LOGIN_SUCCESS", 13);
                    } else {
                        pthread_mutex_unlock(&clientsMutex);
                        if (users.find(username) == users.end()) SSL_write(ssl, "LOGIN_FAIL: User does not exist", BUFFER_SIZE);
                        else if (users[username] != password) SSL_write(ssl, "LOGIN_FAIL: Wrong password", BUFFER_SIZE);
                        else SSL_write(ssl, "LOGIN_FAIL: The user is already logged in on another client", BUFFER_SIZE);
                    }
                } else {
                    SSL_write(ssl, "LOGIN_FAIL", 10);
                }
            }
            
        } else if (command == "LOGOUT" && loggedIn) {
            loggedIn = false;
            pthread_mutex_lock(&clientsMutex);
            connectedClients.erase(currentUser);
            connectedClientSSLs.erase(currentUser);
            pthread_mutex_unlock(&clientsMutex);
            SSL_write(ssl, "LOGOUT_SUCCESS", 14);
        } else if (command.substr(0, 7) == "MESSAGE" && loggedIn) {
            pthread_mutex_lock(&clientsMutex);
            size_t firstSpace = command.find(' ', 8);
            std::string recipient = command.substr(8, firstSpace - 8);
            std::string message = command.substr(firstSpace + 1);
            message = "New message from " + recipient + ": " + message;
            if (connectedClients.find(recipient) != connectedClients.end()) {
                int targetSocket = connectedClients[recipient];
                SSL* targetSSL = connectedClientSSLs[recipient];
                SSL_set_fd(targetSSL, targetSocket);
                SSL_write(targetSSL, message.c_str(), message.size());
                SSL_free(targetSSL);
            } else {
                SSL_write(ssl, "USER_NOT_ONLINE", 15);
            }
            pthread_mutex_unlock(&clientsMutex);
        } else if (command.substr(0, 9) == "SEND_FILE" && loggedIn) {
            size_t pos = command.find(' ', 10);
            std::string recipient = command.substr(10, pos - 10);
            std::string fileName = command.substr(pos + 1, command.length() - pos);
            pthread_mutex_lock(&clientsMutex);
            if (connectedClients.find(recipient) == connectedClients.end()) {
                std::string errorMsg = "ERROR: " + recipient + " is not online.";
                SSL_write(ssl, errorMsg.c_str(), errorMsg.size());
                pthread_mutex_unlock(&clientsMutex);
                continue;
            }
            pthread_mutex_unlock(&clientsMutex);
            relayFile(ssl, recipient, fileName, connectedClients, connectedClientSSLs, clientsMutex);
        } else if (command.substr(0, 12) == "STREAM AUDIO" && loggedIn) { 
            // 指令格式: STREAM AUDIO <filepath>
            // 先找空白位置
            // "STREAM AUDIO " 長度是 13，或你也可直接找第2個空白
            size_t pos = command.find(' ', 12);
            if (pos == std::string::npos) {
                // 沒帶檔案路徑
                SSL_write(ssl, "STREAM_AUDIO_FAIL: No filepath specified", 40);
                continue;
            }
            // 取出檔案路徑
            std::string filePath = command.substr(pos + 1);

            // 呼叫新的 sendAudioStream(ssl, filePath)
            sendAudioStream(ssl, filePath);
        } else if (command.substr(0, 12) == "STREAM VIDEO" && loggedIn) {
            // 指令格式: STREAM VIDEO <filepath>
            size_t pos = command.find(' ', 12);
            if (pos == std::string::npos) {
                SSL_write(ssl, "STREAM_VIDEO_FAIL: No filepath specified", 40);
                continue;
            }
            std::string filePath = command.substr(pos + 1);

            // 呼叫新的 sendVideoStream(ssl, filePath)
            sendVideoStream(ssl, filePath);
            
        } else if (command.substr(0, 10) == "DIRECT_MSG" && loggedIn) {
            // command = "DIRECT_MSG <targetUser> <message>"
            // 解析 targetUser / message
            size_t firstSpace = command.find(' ', 11);
            if (firstSpace == std::string::npos) {
                // 無效格式
                SSL_write(ssl, "DIRECT_MSG_ERROR", 16);
                continue;
            }
            std::string targetUser = command.substr(11, firstSpace - 11);
            std::string directMsg = command.substr(firstSpace + 1);

            pthread_mutex_lock(&clientsMutex);
            // 檢查 targetUser 是否在線
            if (connectedClients.find(targetUser) != connectedClients.end()) {
                // 找到對方 IP / Port
                std::string peerIP = userIPs[targetUser];
                int peerPort = userPorts[targetUser];

                // 回傳給發送端: "PEER_INFO <targetUser> <peerIP> <peerPort> <原本想傳的 message>"
                // 用空白分隔；實際實作時要注意 parsing。
                std::string info = "PEER_INFO " + targetUser + " " + peerIP + " " 
                                + std::to_string(peerPort) + " " + directMsg;
                pthread_mutex_unlock(&clientsMutex);

                SSL_write(ssl, info.c_str(), info.size());
            } else {
                pthread_mutex_unlock(&clientsMutex);
                SSL_write(ssl, "DIRECT_MSG_FAIL: Recipient not online" , BUFFER_SIZE);
            }
        } else {
            SSL_write(ssl, "Invalid command", 15);
        }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    return nullptr;
}