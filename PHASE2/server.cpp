#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <csignal>
#include <pthread.h>
#include <queue>
#include <vector>

#define PORT 11115
#define BUFFER_SIZE 1024
#define THREAD_POOL_SIZE 4

std::unordered_map<std::string, std::string> users;
std::unordered_map<std::string, int> connectedClients;
pthread_mutex_t usersMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queueMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t conditionVar = PTHREAD_COND_INITIALIZER;

std::queue<int> clientQueue;

void* handleClient(void* clientSock) {
    int clientSocket = *(int*)clientSock;
    delete (int*)clientSock;  // Free memory after extracting socket

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
                
                pthread_mutex_lock(&usersMutex);
                if (users.find(username) == users.end()) {
                    users[username] = password;
                    pthread_mutex_unlock(&usersMutex);
                    send(clientSocket, "REGISTERED", 10, 0);
                } else {
                    pthread_mutex_unlock(&usersMutex);
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

                pthread_mutex_lock(&usersMutex);
                if (users.find(username) != users.end() && users[username] == password) {
                    loggedIn = true;
                    currentUser = username;
                    pthread_mutex_unlock(&usersMutex);

                    pthread_mutex_lock(&clientsMutex);
                    connectedClients[username] = clientSocket;
                    pthread_mutex_unlock(&clientsMutex);
                    
                    send(clientSocket, "LOGIN_SUCCESS", 13, 0);
                } else {
                    pthread_mutex_unlock(&usersMutex);
                    send(clientSocket, "LOGIN_FAIL", 10, 0);
                }
            } else {
                send(clientSocket, "INVALID_LOGIN", 12, 0);
            }
        } else if (command == "LOGOUT") {
            if (loggedIn) {
                loggedIn = false;
                pthread_mutex_lock(&clientsMutex);
                connectedClients.erase(currentUser);
                pthread_mutex_unlock(&clientsMutex);
                send(clientSocket, "LOGOUT_SUCCESS", 14, 0);
            } else {
                send(clientSocket, "NOT_LOGGED_IN", 14, 0);
            }
        } else if (command.substr(0, 7) == "MESSAGE") {
            size_t spacePos = command.find(' ', 8);
            if (spacePos != std::string::npos) {
                std::string recipient = command.substr(8, spacePos - 8);
                std::string message = command.substr(spacePos + 1);
                pthread_mutex_lock(&clientsMutex);
                if (connectedClients.find(recipient) != connectedClients.end()) {
                    int targetSocket = connectedClients[recipient];
                    std::string relayMessage = currentUser + ": " + message;
                    send(targetSocket, relayMessage.c_str(), relayMessage.size(), 0);
                } else {
                    send(clientSocket, "USER_NOT_ONLINE", 15, 0);
                }
                pthread_mutex_unlock(&clientsMutex);
            } else {
                send(clientSocket, "INVALID_MESSAGE", 15, 0);
            }
        } else {
            send(clientSocket, "UNKNOWN_COMMAND", 15, 0);
        }
    }
    close(clientSocket);
    if (loggedIn) {
        pthread_mutex_lock(&clientsMutex);
        connectedClients.erase(currentUser);
        pthread_mutex_unlock(&clientsMutex);
    }
    return nullptr;
}

void* threadPoolWorker(void*) {
    while (true) {
        pthread_mutex_lock(&queueMutex);
        while (clientQueue.empty()) {
            pthread_cond_wait(&conditionVar, &queueMutex);
        }
        int* clientSockPtr = new int(clientQueue.front());
        clientQueue.pop();
        pthread_mutex_unlock(&queueMutex);
        
        handleClient((void*)clientSockPtr);
    }
    return nullptr;
}

void signalHandler(int signum) {
    std::cout << "Signal " << signum << " received, ignoring." << std::endl;
    return;
}

int main() {
    signal(SIGINT, signalHandler);

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return 1;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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

    pthread_t pool[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
        pthread_create(&pool[i], nullptr, threadPoolWorker, nullptr);
    }

    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept client connection." << std::endl;
            continue;
        }

        pthread_mutex_lock(&queueMutex);
        clientQueue.push(clientSocket);
        pthread_mutex_unlock(&queueMutex);
        pthread_cond_signal(&conditionVar);
    }

    close(serverSocket);
    pthread_mutex_destroy(&usersMutex);
    pthread_mutex_destroy(&clientsMutex);
    pthread_mutex_destroy(&queueMutex);
    pthread_cond_destroy(&conditionVar);
    return 0;
}
