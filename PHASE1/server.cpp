// server.cpp

#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <csignal>

#define PORT 11115
#define BUFFER_SIZE 1024

std::unordered_map<std::string, std::string> users; // 模擬的使用者資料庫

void handleClient(int clientSocket) {
    char buffer[BUFFER_SIZE];
    bool loggedIn = false;
    std::string currentUser;

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived <= 0) {
            std::cout << "Connection closed." << std::endl;
            break;
        }

        std::string command(buffer);
        if (command.substr(0, 8) == "REGISTER") {
            size_t spacePos = command.find(' ', 9);
            if (spacePos != std::string::npos) {
                std::string username = command.substr(9, spacePos - 9);
                std::string password = command.substr(spacePos + 1);
                if (users.find(username) == users.end()) {
                    users[username] = password;
                    send(clientSocket, "REGISTERED", 10, 0);
                } else {
                    send(clientSocket, "USER_EXISTS", 11, 0);
                }
            } else {
                send(clientSocket, "INVALID_REGISTER", 16, 0);
            }
        } else if (command.substr(0, 5) == "LOGIN") {
            size_t spacePos = command.find(' ', 6);
            if (spacePos != std::string::npos) {
                std::string username = command.substr(6, spacePos - 6);
                std::string password = command.substr(spacePos + 1);
                if (users.find(username) != users.end() && users[username] == password) {
                    loggedIn = true;
                    currentUser = username;
                    send(clientSocket, "LOGIN_SUCCESS", 13, 0);
                } else {
                    send(clientSocket, "LOGIN_FAIL", 10, 0);
                }
            } else {
                send(clientSocket, "INVALID_LOGIN", 12, 0);
            }
        } else if (command == "LOGOUT") {
            if (loggedIn) {
                loggedIn = false;
                send(clientSocket, "LOGOUT_SUCCESS", 14, 0);
            } else {
                send(clientSocket, "NOT_LOGGED_IN", 14, 0);
            }
        } else if (command.substr(0, 7) == "MESSAGE") {
            if (command.size() > 8) {
                std::string message = command.substr(8);
                std::cout << "Client says: " << message << std::endl;
                std::cout << "Enter response: ";
                std::string response;
                std::getline(std::cin, response);
                send(clientSocket, response.c_str(), response.size(), 0);
            } else {
                send(clientSocket, "INVALID_MESSAGE", 15, 0);
            }
        } else {
            send(clientSocket, "UNKNOWN_COMMAND", 15, 0);
        }
    }
    close(clientSocket);
}

void signalHandler(int signum) {
    std::cout << "Signal " << signum << " received, ignoring." << std::endl;
    return;
}

int main() {
    // 設置 signal handler 忽略 SIGINT
    signal(SIGINT, signalHandler);

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

    std::cout << "Server is listening on port " << PORT << std::endl;

    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept client connection." << std::endl;
            continue;
        }

        std::cout << "Client connected." << std::endl;
        handleClient(clientSocket);
    }

    close(serverSocket);
    return 0;
}
