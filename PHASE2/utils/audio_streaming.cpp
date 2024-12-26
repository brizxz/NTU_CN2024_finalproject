#include "audio_streaming.hpp"
#include "const.h"
#include <fstream>
#include <string>

SSL* receiveSsl;

std::ifstream wavFile;
std::string headerInfo(const WAVHeader& header) {
    // Format the header as a string "numChannels:sampleRate"
    std::string headerString = std::to_string(header.numChannels) + ":" + std::to_string(header.sampleRate);
    return headerString;
}

static int callback(const void* inputBuffer, void* outputBuffer,
                    unsigned long framesPerBuffer,
                    const PaStreamCallbackTimeInfo* timeInfo,
                    PaStreamCallbackFlags statusFlags,
                    void* userData) {
    //std::cout << "Callback" << std::endl;
    char commandBuffer[BUFFER_SIZE] = {};
    SSL_read(receiveSsl, &commandBuffer, BUFFER_SIZE);
    if (strcmp(commandBuffer, "STREAMING") != 0) {
        std::cout << "Connection Corrupted, interrupting playback..." << std::endl;
        return paComplete;
    }

    std::vector<uint8_t>* buffer = static_cast<std::vector<uint8_t>*>(userData);
    uint8_t* out = static_cast<uint8_t*>(outputBuffer);

    uint16_t numChannels;
    SSL_read(receiveSsl, &numChannels, 1);

    int bytesToRead = framesPerBuffer * sizeof(uint16_t) * numChannels;  // Adjust to match buffer size
    buffer->resize(bytesToRead);  // Ensure buffer is the correct size

    int bytesRead = SSL_read(receiveSsl, buffer->data(), bytesToRead);
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


void sendAudioStream(SSL* ssl, const std::string& filePath) {
    SSL_write(ssl, "START_AUDIO_STREAMING", 21);
    std::cout << "Start streaming, opening file..." << std::endl;
    // Open the WAV file
    wavFile.open(filePath, std::ios::binary);
    if (!wavFile.is_open()) {
        std::cerr << "Failed to open WAV file." << std::endl;
        return;
    }
    std::cout << "File opened" << std::endl;
    // Read WAV header
    WAVHeader header;
    wavFile.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));
    std::cout << "Header read" << std::endl;
    
    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        std::cerr << "Invalid WAV file." << std::endl;
        return;
    }
    std::cout << "Writing header to client..." << std::endl;
    std::string WAVHeaderInfo = headerInfo(header);
    int bytesWritten = SSL_write(ssl, WAVHeaderInfo.c_str(), WAVHeaderInfo.size());
    std::cout << "Finish writing" << std::endl;
    if (bytesWritten <= 0) {
        int sslError = SSL_get_error(ssl, bytesWritten);
        std::cerr << "SSL_write failed, error: " << sslError << std::endl;
        return;
    }
    std::cout << "Start sending data" << std::endl;

    while (true) {
        
        std::vector<uint8_t> buffer(CHUNK_SIZE * header.numChannels);
        std::streamsize bytesRead = wavFile.read(reinterpret_cast<char*>(buffer.data()), buffer.size()).gcount();
        // std::cout << "Read a chunk" << std::endl;
        if (bytesRead <= 0) {
            break;
        }
        SSL_write(ssl, "STREAMING", 9);
        SSL_write(ssl, &header.numChannels, 1);
        SSL_write(ssl, buffer.data(), bytesRead);
        
    }
    wavFile.close();

    std::cout << "Streaming finished." << std::endl;
}

void receiveAudioStream(SSL* ssl) {
    receiveSsl = ssl;
    Pa_Initialize();
    std::cout << "Start streaming" << std::endl;
    std::string WAVHeaderInfo;
    char headerBuffer[256];  // Buffer to receive the data
    int bytesRead = SSL_read(receiveSsl, headerBuffer, sizeof(headerBuffer) - 1);

    if (bytesRead <= 0) {
        std::cerr << "SSL_read failed" << std::endl;
        return;
    }

    headerBuffer[bytesRead] = '\0';
    std::string receivedData(headerBuffer);
    uint16_t numChannels;
    uint32_t sampleRate;

    size_t delimiterPos = receivedData.find(':');
    numChannels = static_cast<uint16_t>(std::stoi(receivedData.substr(0, delimiterPos)));
    sampleRate = static_cast<uint32_t>(std::stoi(receivedData.substr(delimiterPos + 1)));


    std::cout << (int)numChannels << " " << (int)sampleRate << std::endl;
    std::vector<uint8_t> buffer(CHUNK_SIZE * numChannels);

    // PortAudio Stream Setup
    PaStream* stream;
    
    PaError err = Pa_OpenDefaultStream(&stream, 0, numChannels, paInt16,
                                sampleRate, FRAMES_PER_BUFFER, callback, &buffer);
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