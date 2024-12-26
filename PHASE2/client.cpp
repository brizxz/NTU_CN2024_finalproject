#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <csignal>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include <portaudio.h>
#include "utils/audio_streaming.hpp"
#include "utils/video_streaming.hpp"
#include "utils/ssl.h"
#include "utils/file_transfer_relay.hpp"
#include "utils/const.h"


SSL* ssl;
SSL_CTX* ctx;

int clientSocket;
bool running = true;

int p2pPort;


void* receiveMessages(void*);
void* p2pListener(void* arg);


void signalHandler(int signum) {
    std::cout << "Signal " << signum << " received, shutting down." << std::endl;
    running = false;
    cleanupSSLClient(ssl, ctx);
    exit(signum);
}


int main() {
    signal(SIGINT, signalHandler);
    
    std::cout << "Please determine your port: ";
    std::cin >> p2pPort;

    initSSLClient(ssl, ctx);
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return 1;
    }
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address." << std::endl;
        return 1;
    }
    
    std::cout << "Check" << std::endl;
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed." << std::endl;
        return 1;
    }
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, clientSocket);
    
    
    if (SSL_connect(ssl) <= 0) {
        std::cerr << "SSL connection failed." << std::endl;
        ERR_print_errors_fp(stderr);
        return 1;
    }

    std::cout << "Connected with " << SSL_get_cipher(ssl) << " encryption" << std::endl;

    pthread_t receiverThread;
    pthread_create(&receiverThread, nullptr, receiveMessages, nullptr);

    // 啟動一個 P2P 監聽的執行緒
    pthread_t p2pThread;
    pthread_create(&p2pThread, nullptr, p2pListener, nullptr);


    std::string command;

    while (running) {
        std::getline(std::cin, command);
        if (command == "EXIT") {
            running = false;
            break;
        }
        SSL_write(ssl, command.c_str(), command.size());
    }

    close(clientSocket);
    pthread_cancel(receiverThread);
    pthread_join(receiverThread, nullptr);
    cleanupSSLClient(ssl, ctx);

    // 關閉 p2p thread
    pthread_cancel(p2pThread);
    pthread_join(p2pThread, nullptr);

    return 0;
}

void* receiveMessages(void*) {
    
    char buffer[BUFFER_SIZE];
    while (running) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = SSL_read(ssl, buffer, BUFFER_SIZE);
        if (bytesReceived <= 0) {
            std::cout << "Disconnected from server." << std::endl;
            running = false;
            break;
        }
        std::string response(buffer);

        if (response.substr(0, 24) == "FILE_TRANSFER_START_SEND") {
            sendFileRelay(ssl);
        } else if (response.substr(0, 27) == "FILE_TRANSFER_START_RECEIVE") {
            receiveFileRelay(ssl);
        } else if (response.substr(0, 21) == "START_AUDIO_STREAMING") {
            receiveAudioStream(ssl);
        } else if (response.substr(0, 21) == "START_VIDEO_STREAMING") {
            receiveVideoStream(ssl);
        } else if (response.substr(0, 9) == "PEER_INFO") {
            // 格式: PEER_INFO <username> <ip> <port> <message>
            // 先簡單用空白切割:
            std::vector<std::string> tokens;
            {
                size_t start = 0, end = 0;
                while ((end = response.find(' ', start)) != std::string::npos) {
                    tokens.push_back(response.substr(start, end - start));
                    start = end + 1;
                }
                // 最後一塊
                if (start < response.size()) {
                    tokens.push_back(response.substr(start));
                }
            }
            // tokens[0] = "PEER_INFO"
            // tokens[1] = <targetUser>
            // tokens[2] = <ip>
            // tokens[3] = <port>
            // tokens[4] = <message>（若訊息本身沒有空白才行；若有空白則要更複雜的 parsing）

            if (tokens.size() < 5) {
                std::cerr << "[Error] Invalid PEER_INFO format\n";
                continue;
            }

            std::string peerUser = tokens[1];
            std::string peerIP   = tokens[2];
            int peerPort         = std::stoi(tokens[3]);
            std::string msgToSend = tokens[4];  

            // 與對方直接連線，送出訊息
            int peerSock = socket(AF_INET, SOCK_STREAM, 0);
            if (peerSock < 0) {
                std::cerr << "[Error] Failed to create peer socket\n";
                continue;
            }

            sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(peerPort);
            if (inet_pton(AF_INET, peerIP.c_str(), &dest.sin_addr) <= 0) {
                std::cerr << "[Error] Invalid peer IP\n";
                close(peerSock);
                continue;
            }

            if (connect(peerSock, (sockaddr*)&dest, sizeof(dest)) < 0) {
                std::cerr << "[Error] Failed to connect to peer\n";
                close(peerSock);
                continue;
            }

            // 把訊息傳給對方
            int sent = send(peerSock, msgToSend.c_str(), msgToSend.size(), 0);
            if (sent < 0) {
                std::cerr << "[Error] Failed to send message to peer\n";
            }
            close(peerSock);

            // 顯示一下
            std::cout << "[P2P] Sent to " << peerUser 
                      << " (" << peerIP << ":" << peerPort 
                      << ") => " << msgToSend << std::endl;
        } else {
            std::cout << "Server: " << response << std::endl;
        }
    }
    return nullptr;
}


// 新增一個函式：用來在 port (或你想要的) 上監聽直連
void* p2pListener(void* arg) {
    int p2pSock = socket(AF_INET, SOCK_STREAM, 0);
    if (p2pSock < 0) {
        std::cerr << "Failed to create P2P socket\n";
        pthread_exit(nullptr);
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(p2pPort);

    if (bind(p2pSock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind P2P socket\n";
        close(p2pSock);
        pthread_exit(nullptr);
    }

    if (listen(p2pSock, 5) < 0) {
        std::cerr << "Failed to listen on P2P socket\n";
        close(p2pSock);
        pthread_exit(nullptr);
    }

    std::cout << "P2P listener started on port " << p2pPort << std::endl;

    while (running) {
        sockaddr_in peerAddr;
        socklen_t peerLen = sizeof(peerAddr);
        int peerSocket = accept(p2pSock, (sockaddr*)&peerAddr, &peerLen);
        if (peerSocket < 0) {
            if (!running) break; // 若程式結束，就跳出
            std::cerr << "Failed to accept P2P connection\n";
            continue;
        }

        // 收取對方傳來的訊息 (這裡只示範文字)
        char buf[BUFFER_SIZE];
        memset(buf, 0, BUFFER_SIZE);
        int bytes = read(peerSocket, buf, BUFFER_SIZE);
        if (bytes > 0) {
            std::cout << "[P2P Received] " << buf << std::endl;
        }

        close(peerSocket);
    }

    close(p2pSock);
    pthread_exit(nullptr);
}
