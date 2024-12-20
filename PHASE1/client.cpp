// client.cpp

#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <csignal>

#define PORT 11115
#define BUFFER_SIZE 1024

int clientSocket;

void displayMenu();
void signalHandler(int signum) {
    std::cout << "Signal " << signum << " received, ignoring." << std::endl;
    displayMenu();
}

void displayMenu() {
    std::cout << "Commands:\n"
              << "1. REGISTER <username> <password>\n"
              << "2. LOGIN <username> <password>\n"
              << "3. LOGOUT\n"
              << "4. MESSAGE <message>\n"
              << "5. EXIT\n";
}

int main() {
    // 設置 signal handler 忽略 SIGINT
    signal(SIGINT, signalHandler);

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

    char buffer[BUFFER_SIZE];
    std::string command;

    displayMenu();

    while (true) {
        std::cout << "> ";
        std::getline(std::cin, command);

        if (command == "EXIT") {
            break;
        }

        // Send the command to the server
        send(clientSocket, command.c_str(), command.size(), 0);

        // Wait for the server's response
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived <= 0) {
            std::cout << "Connection closed by server." << std::endl;
            break;
        }
        std::cout << "Server: " << buffer << std::endl;
    }

    close(clientSocket);
    return 0;
}
