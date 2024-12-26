#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include <portaudio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
struct WAVHeader {
    char riff[4];
    uint32_t fileSize;
    char wave[4];
    char fmt[4];
    uint32_t fmtSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4];
    uint32_t dataSize;
};
std::string headerInfo(const WAVHeader& header);
void sendAudioStream(SSL* ssl);
void receiveAudioStream(SSL* ssl);