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
#include "audio_streaming.hpp"
#include "ssl.h"
#include "file_transfer_relay.hpp"

#define PORT 11115
#define BUFFER_SIZE 4096
#define FRAMES_PER_BUFFER 2048
#define CHUNK_SIZE 4096
#define FILE_SAVE_DIR "received_files/"


SSL* ssl;
SSL_CTX* ctx;

int clientSocket;
bool running = true;

void displayMenu();
void* receiveMessages(void*);

void signalHandler(int signum) {
    std::cout << "Signal " << signum << " received, shutting down." << std::endl;
    running = false;
    cleanupSSLClient(ssl, ctx);
    exit(signum);
}


int main() {
    signal(SIGINT, signalHandler);
    
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
    return 0;
}

void* receiveMessages(void*) {
    
    char buffer[BUFFER_SIZE];
    while (running) {
        memset(buffer, 0, BUFFER_SIZE);
        SSL_read(ssl, buffer, BUFFER_SIZE);
        std::string menuStr(buffer);
        std::cout << menuStr << std::endl;
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
        } else if (response.substr(0, 15) == "START_STREAMING") {
            receiveAudioStream(ssl);
        } else {
            std::cout << "Server: " << response << std::endl;
        }
    }
    return nullptr;
}