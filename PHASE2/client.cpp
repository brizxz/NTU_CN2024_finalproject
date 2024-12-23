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

#define PORT 11115
#define BUFFER_SIZE 1024
#define FILE_SAVE_DIR "received_files/"

int clientSocket;
bool running = true;
SSL* ssl;
SSL_CTX* ctx;

void displayMenu();
void* receiveMessages(void*);
void sendFile();
void receiveFile();
void initSSL();
void cleanupSSL();

void signalHandler(int signum) {
    std::cout << "Signal " << signum << " received, shutting down." << std::endl;
    running = false;
    cleanupSSL();
    exit(signum);
}

void displayMenu() {
    std::cout << "Commands:\n"
              << "1. REGISTER <username> <password>\n"
              << "2. LOGIN <username> <password>\n"
              << "3. LOGOUT\n"
              << "4. MESSAGE <username> <message>\n"
              << "5. SEND_FILE <username> <filepath>\n"
              << "6. EXIT\n";
}

void initSSL() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        std::cerr << "Unable to create SSL context" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void cleanupSSL() {
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    EVP_cleanup();
}

int main() {
    signal(SIGINT, signalHandler);
    initSSL();

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

    displayMenu();
    std::string command;

    while (running) {
        std::cout << "> ";
        std::getline(std::cin, command);
        SSL_write(ssl, command.c_str(), command.size());

        if (command == "EXIT") {
            running = false;
            break;
        }
    }

    close(clientSocket);
    pthread_cancel(receiverThread);
    pthread_join(receiverThread, nullptr);
    cleanupSSL();
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
            sendFile();
        } else if (response.substr(0, 27) == "FILE_TRANSFER_START_RECEIVE") {
            receiveFile();
        } else {
            std::cout << "Server: " << response << std::endl;
        }
    }
    return nullptr;
}

void sendFile() {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    SSL_read(ssl, buffer, BUFFER_SIZE);
    std::string filePath(buffer);
     // Open the file for reading
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Unable to open file " << filePath << std::endl;
        SSL_write(ssl, "FILE_TRANSFER_FAILED", strlen("FILE_TRANSFER_FAILED"));
        return;
    }

    while (file) {
        memset(buffer, 0, BUFFER_SIZE);
        file.read(buffer, BUFFER_SIZE);
        std::streamsize bytesRead = file.gcount();
        std::cout << (std::string)buffer << std::endl;
        if (bytesRead > 0) {
            int bytesWritten = SSL_write(ssl, buffer, bytesRead);
            if (bytesWritten <= 0) {
                std::cerr << "Error writing to server during file transfer" << std::endl;
                break;
            }
        }
    }
    SSL_write(ssl, "FILE_TRANSFER_END", 17);
    std::cout << "File transfer ended." << std::endl;
}

void receiveFile() {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    SSL_read(ssl, buffer, BUFFER_SIZE);
    std::string fileName(buffer);

    // POSIX alternative to create directories
    mkdir(FILE_SAVE_DIR, 0777);

    std::ofstream outFile(std::string(FILE_SAVE_DIR) + fileName, std::ios::binary);
    if (!outFile) {
        std::cerr << "Failed to create file for saving: " << fileName << std::endl;
        return;
    }

    std::cout << "Receiving file: " << fileName << std::endl;

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytesRead = SSL_read(ssl, buffer, BUFFER_SIZE);
        if (bytesRead <= 0 || std::string(buffer).find("FILE_TRANSFER_END") != std::string::npos) {
            break;
        }
        outFile.write(buffer, bytesRead);
    }

    outFile.close();
    std::cout << "File received and saved in " << FILE_SAVE_DIR << fileName << std::endl;
}