// client.cpp

#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 12345
#define BUFFER_SIZE 4096
#define HOST "127.0.0.1"

void sendHttpRequest(const std::string& request) {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    inet_aton(HOST, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(PORT);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed." << std::endl;
        close(clientSocket);
        return;
    }

    send(clientSocket, request.c_str(), request.size(), 0);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
    if (bytesReceived > 0) {
        std::cout << "Server Response: " << buffer << std::endl;
    } else {
        std::cerr << "Failed to receive response from server." << std::endl;
    }

    close(clientSocket);
}

int main() {
    while (true) {
        std::cout << "Options: \n";
        std::cout << "1. Register \n";
        std::cout << "2. Login \n";
        std::cout << "3. Send Message \n";
        std::cout << "4. Exit \n";
        std::cout << "Select an option: ";
        int option;
        std::cin >> option;
        std::cin.ignore(); // Ignore newline character after the option input

        if (option == 4) {
            break;
        }

        std::string username, password, message;
        std::string request;

        switch (option) {
            case 1:
                std::cout << "Enter username: ";
                std::getline(std::cin, username);
                std::cout << "Enter password: ";
                std::getline(std::cin, password);
                request = "POST /register HTTP/1.1\r\n"
                          "Host: " HOST "\r\n"
                          "Content-Type: application/x-www-form-urlencoded\r\n"
                          "Content-Length: " + std::to_string(username.length() + password.length() + 19) + "\r\n"
                          "\r\nusername=" + username + "&password=" + password;
                break;
            case 2:
                std::cout << "Enter username: ";
                std::getline(std::cin, username);
                std::cout << "Enter password: ";
                std::getline(std::cin, password);
                request = "POST /login HTTP/1.1\r\n"
                          "Host: " HOST "\r\n"
                          "Content-Type: application/x-www-form-urlencoded\r\n"
                          "Content-Length: " + std::to_string(username.length() + password.length() + 19) + "\r\n"
                          "\r\nusername=" + username + "&password=" + password;
                break;
            case 3:
                std::cout << "Enter message: ";
                std::getline(std::cin, message);
                request = "POST /message HTTP/1.1\r\n"
                          "Host: " HOST "\r\n"
                          "Content-Type: application/x-www-form-urlencoded\r\n"
                          "Content-Length: " + std::to_string(message.length() + 8) + "\r\n"
                          "\r\nmessage=" + message;
                break;
            default:
                std::cerr << "Invalid option." << std::endl;
                continue;
        }

        sendHttpRequest(request);
    }

    return 0;
}