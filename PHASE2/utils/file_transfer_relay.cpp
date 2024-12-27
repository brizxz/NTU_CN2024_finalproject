#include "const.h"
#include "file_transfer_relay.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <fstream>
#include <string>
#include <filesystem>  // C++17 以上可用

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

    // 傳送檔案內容
    while (file) {
        memset(buffer, 0, BUFFER_SIZE);
        file.read(buffer, BUFFER_SIZE);
        std::streamsize bytesRead = file.gcount();
        // 這裡單純印出文字可能會有 binary 資料被轉成字串
        // 如果需要，可省略或做 hex dump
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

    // 先讀「對方傳來的檔名」(可能含路徑)
    SSL_read(ssl, buffer, BUFFER_SIZE);
    std::string fileName(buffer);

    // 確保本地的 "received_files" 資料夾存在 (單層)
    // 若需要多層，可改用 std::filesystem::create_directories(FILE_SAVE_DIR)
    mkdir(FILE_SAVE_DIR, 0777);

    // 只取對方傳來的檔名最後一段
    // e.g. 若對方給 "test_files/test.txt"，則只取 "test.txt"
    std::filesystem::path p(fileName);
    std::string onlyName = p.filename().string();

    // 最終我們只要存到 "received_files/test.txt"
    std::filesystem::path finalPath = std::filesystem::path(FILE_SAVE_DIR) / onlyName;
    std::string finalPathStr = finalPath.string();

    std::cout << "Save path is: " << finalPathStr << std::endl;

    std::ofstream outFile(finalPathStr, std::ios::binary);
    if (!outFile) {
        std::cerr << "Failed to create file for saving: " << onlyName << std::endl;
        return;
    }

    std::cout << "Receiving file: " << fileName << std::endl;

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytesRead = SSL_read(ssl, buffer, BUFFER_SIZE);
        if (bytesRead <= 0 
            || std::string(buffer).find("FILE_TRANSFER_FAILED") != std::string::npos) {
            std::cout << "File reception failed! End reception..." << std::endl;
            // 刪除已建立的部分檔案
            std::remove(finalPathStr.c_str());
            break;
        } else if (std::string(buffer).find("FILE_TRANSFER_END") != std::string::npos) {
            outFile.close();
            std::cout << "File received and saved in " << finalPathStr << std::endl;
            break; // 記得要 break 離開 while 迴圈
        } else {
            // 注意：原程式中多做一次 SSL_read(...) -> 會吃掉資料
            // 正常狀況不需要多做一次 read
            outFile.write(buffer, bytesRead);
        }
    }
}

void relayFile(SSL* senderSsl, const std::string& recipient, const std::string& fileName,
    std::unordered_map<std::string, int> connectedClients, 
    std::unordered_map<std::string, SSL*> connectedClientSSLs, 
    pthread_mutex_t& clientsMutex) 
{
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    pthread_mutex_lock(&clientsMutex);
    int targetSocket = connectedClients[recipient];
    SSL* receiverSsl = connectedClientSSLs[recipient];
    pthread_mutex_unlock(&clientsMutex);

    // 通知 sender / receiver 要開始傳檔
    SSL_write(senderSsl, "FILE_TRANSFER_START_SEND", 24);
    SSL_write(senderSsl, fileName.c_str(), fileName.size());
    SSL_write(receiverSsl, "FILE_TRANSFER_START_RECEIVE", 27);
    SSL_write(receiverSsl, fileName.c_str(), fileName.size());

    while (true) {
        std::cout << "Transferring file to " + recipient << std::endl;
        memset(buffer, 0, BUFFER_SIZE);
        int bytesRead = SSL_read(senderSsl, buffer, BUFFER_SIZE);
        if (bytesRead <= 0 
            || std::string(buffer).find("FILE_TRANSFER_FAILED") != std::string::npos) {
            SSL_write(receiverSsl, "FILE_TRANSFER_FAILED", 20);
            break;
        } else if (std::string(buffer).find("FILE_TRANSFER_END") != std::string::npos) {
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
