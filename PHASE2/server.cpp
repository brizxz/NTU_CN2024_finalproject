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
#include "audio_streaming.hpp"
#include "ssl.h"
#include "file_transfer_relay.hpp"

#define PORT 11115
#define BUFFER_SIZE 4096
#define FRAMES_PER_BUFFER 2048
#define CHUNK_SIZE 4096
#define THREAD_POOL_SIZE 4


SSL* ssl;
SSL_CTX* ctx;

std::unordered_map<std::string, std::string> users;
std::unordered_map<std::string, int> connectedClients;
std::unordered_map<std::string, SSL*> connectedClientSSLs;

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
            "2. SEND_FILE <username> <filepath>\n" +
            "3. STREAM AUDIO\n" +
            "4. DIRECT_MSG <targetUser> <message>\n" +
            "5. LOGOUT\n" + 
            "6. EXIT");

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

        pthread_t thread;
        SSL* sslPtr = ssl;
        pthread_create(&thread, nullptr, handleClient, (void*)sslPtr);
        pthread_detach(thread);
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
            size_t spacePos = command.find(' ', 9);
            if (spacePos != std::string::npos) {
                std::string username = command.substr(9, spacePos - 9);
                std::string password = command.substr(spacePos + 1);
                
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
            
            size_t spacePos = command.find(' ', 6);
            if (spacePos != std::string::npos) {
                std::string username = command.substr(6, spacePos - 6);
                std::string password = command.substr(spacePos + 1);
                pthread_mutex_lock(&clientsMutex);
                if (users.find(username) != users.end() && users[username] == password
                    && connectedClientSSLs.find(username) == connectedClientSSLs.end()) {
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
            size_t spacePos = command.find(' ', 8);
            std::string recipient = command.substr(8, spacePos - 8);
            std::string message = command.substr(spacePos + 1);
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
            sendAudioStream(ssl);
        } else {
            SSL_write(ssl, "Invalid command", 15);
        }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    return nullptr;
}