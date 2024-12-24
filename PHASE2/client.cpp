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
#include <vector>
#include <portaudio.h>
#include "audio_related.h"

#define PORT 11115
#define BUFFER_SIZE 1024
#define FRAMES_PER_BUFFER 2048
#define CHUNK_SIZE 4096
#define FILE_SAVE_DIR "received_files/"

int clientSocket;
bool running = true;
SSL* ssl;
SSL_CTX* ctx;

void displayMenu();
void* receiveMessages(void*);
void sendFile();
void receiveFile();
void handleStream();
void initSSL();
void cleanupSSL();

static int callback(const void* inputBuffer, void* outputBuffer,
                    unsigned long framesPerBuffer,
                    const PaStreamCallbackTimeInfo* timeInfo,
                    PaStreamCallbackFlags statusFlags,
                    void* userData) {
    // std::cout << "Callback" << std::endl;
    std::vector<uint8_t>* buffer = static_cast<std::vector<uint8_t>*>(userData);
    uint8_t* out = static_cast<uint8_t*>(outputBuffer);

    int bytesToRead = framesPerBuffer * sizeof(uint16_t);  // Adjust to match buffer size
    buffer->resize(bytesToRead);  // Ensure buffer is the correct size

    int bytesRead = SSL_read(ssl, buffer->data(), bytesToRead);
    // std::cout << bytesRead << std::endl;
    if (bytesRead > 0) {
        // Copy the received data to the output buffer
        std::copy(buffer->begin(), buffer->begin() + bytesRead, out);

        // Zero out the remaining part of the buffer if bytesRead < bytesToRead
        if (bytesRead < bytesToRead) {
            std::fill(out + bytesRead, out + bytesToRead, 0);
            return paComplete;
        }
    } else {
        // SSL_read error or connection closed
        std::fill(out, out + framesPerBuffer * sizeof(uint16_t), 0);
        return paComplete;
    }
    return paContinue;
}

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
              << "6. STREAMING\n"
              << "7. EXIT\n";
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
        } else if (response.substr(0, 15) == "START_STREAMING") {
            handleStream();
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

void handleStream() {
    Pa_Initialize();
    std::cout << "Start streaming" << std::endl;
    WAVHeader header;
    size_t totalBytesRead = 0;
    while (totalBytesRead < sizeof(WAVHeader)) {
        int bytes = SSL_read(ssl, reinterpret_cast<char*>(&header) + totalBytesRead, sizeof(WAVHeader) - totalBytesRead);
        if (bytes <= 0) {
            int sslError = SSL_get_error(ssl, bytes);
            std::cerr << "SSL_read failed or connection closed, error: " << sslError << std::endl;
            return;
        }
        totalBytesRead += bytes;
    }
    std::cout << "Header Read." << std::endl;
    std::cout << (int)header.numChannels << " " << (int)header.sampleRate << std::endl;
    std::vector<uint8_t> buffer(CHUNK_SIZE);

    // PortAudio Stream Setup
    PaStream* stream;
    
    PaError err = Pa_OpenDefaultStream(&stream, 0, header.numChannels, paInt16,
                                header.sampleRate, FRAMES_PER_BUFFER, callback, &buffer);
    if (err != paNoError) { 
        std::cerr << "Failed to open stream: " << Pa_GetErrorText(err) << std::endl;
        return;
    }
    Pa_StartStream(stream);

    // Playback loop
    while (Pa_IsStreamActive(stream)) {
        std::cout << "Playbacking..." << std::endl;
        Pa_Sleep(100);
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
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