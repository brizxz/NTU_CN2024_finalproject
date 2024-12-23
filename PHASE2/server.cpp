#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <csignal>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unordered_map>

#define PORT 11115
#define BUFFER_SIZE 1024
#define THREAD_POOL_SIZE 4

std::unordered_map<std::string, std::string> users;
std::unordered_map<std::string, int> connectedClients;
std::unordered_map<std::string, SSL*> connectedClientSSLs;

pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;
SSL_CTX* ctx;

void* handleClient(void* sslPtr);
void initSSL();
void cleanupSSL();
void signalHandler(int);

void initSSL() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        std::cerr << "Unable to create SSL context" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Load server certificate and key
    if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

void cleanupSSL() {
    SSL_CTX_free(ctx);
    EVP_cleanup();
}

void signalHandler(int signum) {
    std::cout << "Signal " << signum << " received, shutting down server." << std::endl;
    cleanupSSL();
    exit(signum);
}

void transferFile(SSL* senderSsl, const std::string& recipient, const std::string& fileName) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    pthread_mutex_lock(&clientsMutex);
    int targetSocket = connectedClients[recipient];
    SSL* receiverSsl = connectedClientSSLs[recipient];
    pthread_mutex_unlock(&clientsMutex);
    SSL_write(senderSsl, "FILE_TRANSFER_START_SEND", 24);
    SSL_write(senderSsl, fileName.c_str(), fileName.size());
    SSL_write(receiverSsl, "FILE_TRANSFER_START_RECEIVE", 27);
    SSL_write(receiverSsl, fileName.c_str(), fileName.size());
    
    while (true) {
        std::cout << "Transferring file to " + recipient << std::endl;
        memset(buffer, 0, BUFFER_SIZE);
        int bytesRead = SSL_read(senderSsl, buffer, BUFFER_SIZE);
        std::cout << "File content received from sender: " buffer << std::endl;
        if (bytesRead <= 0 || std::string(buffer).find("FILE_TRANSFER_END") != std::string::npos) {
            break;
        }
        SSL_write(receiverSsl, buffer, bytesRead);
    }

    SSL_write(receiverSsl, "FILE_TRANSFER_END", 17);
    std::cout << "File transferred to " << recipient << std::endl;
}

int main() {
    signal(SIGINT, signalHandler);
    initSSL();

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
    cleanupSSL();
    return 0;
}

void* handleClient(void* sslPtr) {
    SSL* ssl = (SSL*)sslPtr;
    char buffer[BUFFER_SIZE];
    bool loggedIn = false;
    std::string currentUser;
    std::string targetUser;
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = SSL_read(ssl, buffer, BUFFER_SIZE);
        if (bytesReceived <= 0) {
            std::cout << "Client disconnected." << std::endl;
            break;
        }

        std::string command(buffer);
        std::cout << "Received: " << command << std::endl;
        if (command.substr(0, 8) == "REGISTER") {
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
        } else if (command.substr(0, 5) == "LOGIN") {
            
            loggedIn = true;
            size_t spacePos = command.find(' ', 6);
            if (spacePos != std::string::npos) {
                std::string username = command.substr(6, spacePos - 6);
                std::string password = command.substr(spacePos + 1);
                pthread_mutex_lock(&clientsMutex);
                if (users.find(username) != users.end() && users[username] == password) {
                    connectedClientSSLs[username] = ssl;
                    loggedIn = true;
                    currentUser = username;
                    connectedClients[currentUser] = SSL_get_fd(ssl);
                    pthread_mutex_unlock(&clientsMutex);
                    SSL_write(ssl, "LOGIN_SUCCESS", 13);
                } else {
                    pthread_mutex_unlock(&clientsMutex);
                    SSL_write(ssl, "LOGIN_FAIL", 10);
                }
            }
            
        } else if (command == "LOGOUT") {
            loggedIn = false;
            pthread_mutex_lock(&clientsMutex);
            connectedClients.erase(currentUser);
            connectedClientSSLs.erase(currentUser);
            pthread_mutex_unlock(&clientsMutex);
            SSL_write(ssl, "LOGOUT_SUCCESS", 14);
        } else if (command.substr(0, 7) == "MESSAGE") {
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
        } else if (command.substr(0, 9) == "SEND_FILE") {
            size_t pos = command.find(' ', 10);
            targetUser = command.substr(10, pos - 10);
            std::string fileName = command.substr(pos + 1, command.length() - pos);
            pthread_mutex_lock(&clientsMutex);
            if (connectedClients.find(targetUser) == connectedClients.end()) {
                std::string errorMsg = "ERROR: " + targetUser + " is not online.";
                SSL_write(ssl, errorMsg.c_str(), errorMsg.size());
                pthread_mutex_unlock(&clientsMutex);
                continue;
            }
            
            
            pthread_mutex_unlock(&clientsMutex);
            transferFile(ssl, targetUser, fileName);
        }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    return nullptr;
}


