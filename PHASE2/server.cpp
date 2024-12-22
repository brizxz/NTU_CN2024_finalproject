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

std::unordered_map<std::string, int> connectedClients;
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

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = SSL_read(ssl, buffer, BUFFER_SIZE);
        if (bytesReceived <= 0) {
            std::cout << "Client disconnected." << std::endl;
            break;
        }

        std::string command(buffer);
        std::cout << "Received: " << command << std::endl;

        if (command.substr(0, 5) == "LOGIN") {
            loggedIn = true;
            currentUser = command.substr(6); // Extract username
            pthread_mutex_lock(&clientsMutex);
            connectedClients[currentUser] = SSL_get_fd(ssl);
            pthread_mutex_unlock(&clientsMutex);
            SSL_write(ssl, "LOGIN_SUCCESS", 13);
        } else if (command == "LOGOUT") {
            loggedIn = false;
            pthread_mutex_lock(&clientsMutex);
            connectedClients.erase(currentUser);
            pthread_mutex_unlock(&clientsMutex);
            SSL_write(ssl, "LOGOUT_SUCCESS", 14);
        } else if (command.substr(0, 7) == "MESSAGE") {
            pthread_mutex_lock(&clientsMutex);
            size_t spacePos = command.find(' ', 8);
            std::string recipient = command.substr(8, spacePos - 8);
            std::string message = command.substr(spacePos + 1);
            if (connectedClients.find(recipient) != connectedClients.end()) {
                int targetSocket = connectedClients[recipient];
                SSL* targetSSL = SSL_new(ctx);
                SSL_set_fd(targetSSL, targetSocket);
                SSL_write(targetSSL, message.c_str(), message.size());
                SSL_free(targetSSL);
            } else {
                SSL_write(ssl, "USER_NOT_ONLINE", 15);
            }
            pthread_mutex_unlock(&clientsMutex);
        }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    return nullptr;
}
