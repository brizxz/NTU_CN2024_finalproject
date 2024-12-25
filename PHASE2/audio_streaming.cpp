#include "audio_streaming.hpp"
#include <string>


std::string headerInfo(const WAVHeader& header) {
    // Format the header as a string "numChannels:sampleRate"
    std::string headerString = std::to_string(header.numChannels) + ":" + std::to_string(header.sampleRate);
    return headerString;
}