// server.cpp

#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <fstream>
#include <csignal>

#define PORT 12345
#define BUFFER_SIZE 4096
#define HOST "127.0.0.1"

std::unordered_map<std::string, std::string> users; // 模擬的使用者資料庫

std::string readHtmlFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return "<html><body><h1>404 Not Found</h1></body></html>";
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return content;
}

void handleClient(int clientSocket) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);

    if (bytesReceived <= 0) {
        std::cout << "Connection closed." << std::endl;
        close(clientSocket);
        return;
    }

    // Parse HTTP request
    std::string request(buffer);
    std::string response;

    if (request.find("GET / ") != std::string::npos) {
        // Read HTML content from file
        std::string htmlContent = readHtmlFile("index.html");
        response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + htmlContent;
    } else if (request.find("POST /register") != std::string::npos) {
        // Handle user registration
        size_t usernamePos = request.find("username=");
        size_t passwordPos = request.find("&password=");
        if (usernamePos != std::string::npos && passwordPos != std::string::npos) {
            std::string username = request.substr(usernamePos + 9, passwordPos - (usernamePos + 9));
            std::string password = request.substr(passwordPos + 10);
            users[username] = password;
            response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nUser registered successfully.";
        } else {
            response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid registration data.";
        }
    } else if (request.find("POST /login") != std::string::npos) {
        // Handle user login
        size_t usernamePos = request.find("username=");
        size_t passwordPos = request.find("&password=");
        if (usernamePos != std::string::npos && passwordPos != std::string::npos) {
            std::string username = request.substr(usernamePos + 9, passwordPos - (usernamePos + 9));
            std::string password = request.substr(passwordPos + 10);
            if (users.find(username) != users.end() && users[username] == password) {
                response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nLogin successful.";
            } else {
                response = "HTTP/1.1 401 Unauthorized\r\nContent-Type: text/plain\r\n\r\nInvalid username or password.";
            }
        } else {
            response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid login data.";
        }
    } else if (request.find("POST /message") != std::string::npos) {
        // Handle user message
        size_t messagePos = request.find("message=");
        if (messagePos != std::string::npos) {
            std::string message = request.substr(messagePos + 8);
            response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nMessage received: " + message;
        } else {
            response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid message data.";
        }
    } else {
        response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n404 Not Found";
    }

    send(clientSocket, response.c_str(), response.size(), 0);
    close(clientSocket);
}

void signalHandler(int signum) {
    std::cout << "Signal " << signum << " received, ignoring." << std::endl;
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
    inet_aton(HOST, &serverAddr.sin_addr);
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