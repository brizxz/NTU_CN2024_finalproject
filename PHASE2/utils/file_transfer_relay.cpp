#include "const.h"
#include "file_transfer_relay.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <fstream>
#include <string>

void sendFileRelay(SSL* ssl) {
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



void receiveFileRelay(SSL* ssl) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    SSL_read(ssl, buffer, BUFFER_SIZE);
    std::string fileName(buffer);

    mkdir(FILE_SAVE_DIR, 0777);

    std::ofstream outFile(std::string(FILE_SAVE_DIR) + fileName, std::ios::binary);
    if (!outFile) {
        std::cerr << "Failed to create file for saving: " << fileName << std::endl;
        return;
    }

    std::cout << "Receiving file: " << fileName << std::endl;
    bool fileRead = false;
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytesRead = SSL_read(ssl, buffer, BUFFER_SIZE);
        if (bytesRead <= 0 || std::string(buffer).find("FILE_TRANSFER_FAILED") != std::string::npos) {
            std::cout << "File reception failed! End reception..." << std::endl;
            std::remove((FILE_SAVE_DIR + fileName).c_str());
            break;
        } else if (std::string(buffer).find("FILE_TRANSFER_END") != std::string::npos) {
            outFile.close();
            std::cout << "File received and saved in " << FILE_SAVE_DIR << fileName << std::endl;
        } else {
            memset(buffer, 0, BUFFER_SIZE);
            int bytesRead = SSL_read(ssl, buffer, BUFFER_SIZE);
            outFile.write(buffer, bytesRead);
        }
        
    }

    
}

void relayFile(SSL* senderSsl, const std::string& recipient, const std::string& fileName,
                        std::unordered_map<std::string, int> connectedClients, std::unordered_map<std::string, SSL*> connectedClientSSLs
                        , pthread_mutex_t& clientsMutex) {
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
        if (bytesRead <= 0 || std::string(buffer).find("FILE_TRANSFER_FAILED") != std::string::npos) {
            SSL_write(receiverSsl, "FILE_TRANSFER_FAILED", 20);
            break;
        } else if (std::string(buffer).find("FILE_TRANSFER_END") != std::string::npos ) {
            SSL_write(receiverSsl, "FILE_TRANSFER_END", 17);
            break;
        } else {
            SSL_write(receiverSsl, "FILE_TRANSFERING", 16);
            std::cout << "File content received from sender: " << buffer << std::endl;
            SSL_write(receiverSsl, buffer, bytesRead);
        }
    }
    std::cout << "File transferred to " << recipient << std::endl;
}
