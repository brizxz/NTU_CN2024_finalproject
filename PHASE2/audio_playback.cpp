#include <iostream>
#include <fstream>
#include <vector>
#include <portaudio.h>
#include <cstdint>
#include <cstring>

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 2048
#define CHUNK_SIZE 4096

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

std::ifstream wavFile;

static int callback(const void* inputBuffer, void* outputBuffer,
                    unsigned long framesPerBuffer,
                    const PaStreamCallbackTimeInfo* timeInfo,
                    PaStreamCallbackFlags statusFlags,
                    void* userData) {
    std::cout << "Callback" << std::endl;
    std::vector<uint8_t>* buffer = static_cast<std::vector<uint8_t>*>(userData);
    uint8_t* out = static_cast<uint8_t*>(outputBuffer);

    if (wavFile.read(reinterpret_cast<char*>(buffer->data()), buffer->size())) {
        std::copy(buffer->begin(), buffer->end(), out);
    } else {
        std::fill(out, out + framesPerBuffer * sizeof(uint16_t), 0);
        return paComplete;
    }
    return paContinue;
}

int main() {
    Pa_Initialize();

    // Open the WAV file
    wavFile.open("/usr/share/sounds/alsa/Front_Center.wav", std::ios::binary);
    if (!wavFile.is_open()) {
        std::cerr << "Failed to open WAV file." << std::endl;
        return -1;
    }

    // Read WAV header
    WAVHeader header;
    wavFile.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));

    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        std::cerr << "Invalid WAV file." << std::endl;
        return -1;
    }

    // Prepare buffer
    std::vector<uint8_t> buffer(CHUNK_SIZE);

    // PortAudio Stream Setup
    PaStream* stream;
    Pa_OpenDefaultStream(&stream, 0, header.numChannels, paInt16,
                         header.sampleRate, FRAMES_PER_BUFFER, callback, &buffer);
    Pa_StartStream(stream);

    // Playback loop
    while (Pa_IsStreamActive(stream)) {
        Pa_Sleep(100);
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    wavFile.close();

    std::cout << "Playback finished." << std::endl;
    return 0;
}
