#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <csignal>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define PORT 11115
#define BUFFER_SIZE 1024

int clientSocket;
bool running = true;
bool loggedIn = false;
SSL* ssl;
SSL_CTX* ctx;

void displayMenu();
void* receiveMessages(void*);
void signalHandler(int signum) {
    std::cout << "Signal " << signum << " received, ignoring." << std::endl;
    displayMenu();
}

void displayMenu() {
    std::cout << "Commands:\n"
              << "1. REGISTER <username> <password>\n"
              << "2. LOGIN <username> <password>\n"
              << "3. LOGOUT\n"
              << "4. MESSAGE <username> <message>\n"
              << "5. P2P <username> <message>  //TODO: Implement peer-to-peer messaging\n"
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

    char buffer[BUFFER_SIZE];
    std::string command;

    displayMenu();

    while (running) {
        std::cout << "> ";
        std::getline(std::cin, command);

        if (command.substr(0, 5) == "LOGIN") {
            loggedIn = true;
        } else if (command == "LOGOUT") {
            loggedIn = false;
        } else if (command.substr(0, 7) == "MESSAGE") {
            if (!loggedIn) {
                std::cout << "You must be logged in to send messages." << std::endl;
                continue;
            }
        } else if (command.substr(0, 3) == "P2P") {
            std::cout << "Peer-to-peer messaging is not yet implemented." << std::endl;
            continue;
        }

        if (command == "EXIT") {
            running = false;
            break;
        }

        SSL_write(ssl, command.c_str(), command.size());
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
        if (response == "USER_NOT_ONLINE") {
            std::cout << "The recipient is not online." << std::endl;
        } else {
            std::cout << "\nMessage: " << buffer << std::endl;
        }
        std::cout << "> ";
        fflush(stdout);
    }
    return nullptr;
}
