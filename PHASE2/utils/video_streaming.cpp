#include "video_streaming.hpp"
#include <sys/stat.h>  // for stat()
#include "const.h"  // 若你有共用的 BUFFER_SIZE 等定義，可放這裡
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>

// 全域儲存，供 receiveVideoStream() 時使用
static SSL* receiveVideoSsl = nullptr;

/**
 * 將 VideoHeader 轉成 "width:height:fps" 的字串
 */
std::string videoHeaderInfo(const VideoHeader& header) {
    // 例如 "640:480:30"
    return std::to_string(header.width) + ":" +
           std::to_string(header.height) + ":" +
           std::to_string(static_cast<int>(header.fps));
}

/**
 * Server 端：讀取影片檔並傳送
 */
void sendVideoStream(SSL* ssl, std::string filename) {
    SSL_write(ssl, "START_VIDEO_STREAMING", 21);

    struct stat fileStat;
    if (stat(filename.c_str(), &fileStat) != 0) {
        std::cerr << "[Server] ERROR: File does not exist => " << filename << std::endl;
        // 回傳錯誤給 Client
        SSL_write(ssl, "AUDIO_STREAMING_FAILED: File does not exist", 44);
        return;
    }

    cv::VideoCapture cap(filename);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open video file.\n";
        return;
    }

    VideoHeader header;
    header.width = static_cast<uint32_t>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    header.height = static_cast<uint32_t>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    header.fps = static_cast<float>(cap.get(cv::CAP_PROP_FPS));
    if (header.fps < 1.0f) {
        header.fps = 30.0f;
    }

    std::string info = videoHeaderInfo(header);
    if (SSL_write(ssl, info.c_str(), info.size()) <= 0) {
        cap.release();
        return;
    }

    cv::Mat frame;
    while (true) {
        bool ret = cap.read(frame);
        if (!ret || frame.empty()) {
            // 影片讀完 => 結束
            break;
        }
        // 壓縮成 JPEG
        std::vector<uchar> encodedBuf;
        std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };
        if (!cv::imencode(".jpg", frame, encodedBuf, params)) {
            std::cerr << "Failed to encode frame.\n";
            break;
        }
        // 傳指令
        SSL_write(ssl, "VIDEO_STREAMING", 15);
        // 傳 frame size
        uint32_t frameSize = static_cast<uint32_t>(encodedBuf.size());
        SSL_write(ssl, reinterpret_cast<char*>(&frameSize), sizeof(frameSize));
        // 傳 frame 資料
        int totalSent = 0;
        while (totalSent < static_cast<int>(frameSize)) {
            int sent = SSL_write(ssl, &encodedBuf[totalSent], frameSize - totalSent);
            if (sent <= 0) {
                std::cerr << "Failed to send frame data.\n";
                break;
            }
            totalSent += sent;
        }
    }
    // 結束
    cap.release();
    SSL_write(ssl, "END_VIDEO_STREAMING", 19);
    std::cout << "Video streaming finished." << std::endl;
}


/**
 * Client 端：接收影片串流，解碼並顯示
 */
void receiveVideoStream(SSL* ssl) {
    receiveVideoSsl = ssl;
    std::cout << "Start receiving video stream..." << std::endl;

    // 讀 header (如 "640:480:30")
    char headerBuffer[256] = {};
    int bytesRead = SSL_read(receiveVideoSsl, headerBuffer, sizeof(headerBuffer)-1);
    if (bytesRead <= 0) {
        std::cerr << "receiveVideoStream: no header data.\n";
        return;
    }
    headerBuffer[bytesRead] = '\0';
    std::string headerStr(headerBuffer);

    // 解析
    size_t pos1 = headerStr.find(':');
    size_t pos2 = headerStr.find(':', pos1 + 1);
    if (pos1 == std::string::npos || pos2 == std::string::npos) {
        std::cerr << "Invalid video header." << std::endl;
        return;
    }
    uint32_t width = std::stoi(headerStr.substr(0, pos1));
    uint32_t height = std::stoi(headerStr.substr(pos1 + 1, pos2 - (pos1 + 1)));
    float fps = std::stof(headerStr.substr(pos2 + 1));
    std::cout << "Video resolution: " << width << "x" << height 
              << ", fps: " << fps << std::endl;

    cv::namedWindow("Video Stream", cv::WINDOW_AUTOSIZE);

    while (true) {
        // 先讀指令
        char cmdBuffer[BUFFER_SIZE] = {};
        int cmdLen = SSL_read(receiveVideoSsl, cmdBuffer, BUFFER_SIZE);
        if (cmdLen <= 0) {
            // 連線關閉或出錯
            std::cout << "[Video] Streaming ended (connection closed)." << std::endl;
            break;
        }
        std::string cmd(cmdBuffer, cmdLen);

        if (cmd.rfind("VIDEO_STREAMING", 0) == 0) {
            // 讀 frame size
            uint32_t frameSize;
            int sizeBytes = SSL_read(receiveVideoSsl, reinterpret_cast<char*>(&frameSize),
                                     sizeof(frameSize));
            if (sizeBytes != sizeof(frameSize)) {
                std::cerr << "Failed to read frameSize.\n";
                break;
            }
            // 讀壓縮資料
            std::vector<uchar> encodedFrame(frameSize);
            int totalReceived = 0;
            while (totalReceived < static_cast<int>(frameSize)) {
                int rec = SSL_read(receiveVideoSsl,
                                   &encodedFrame[totalReceived],
                                   frameSize - totalReceived);
                if (rec <= 0) {
                    std::cerr << "Failed to read frame data.\n";
                    break;
                }
                totalReceived += rec;
            }
            if (totalReceived < static_cast<int>(frameSize)) {
                // 資料不足
                break;
            }

            cv::Mat frame = cv::imdecode(encodedFrame, cv::IMREAD_COLOR);
            if (frame.empty()) {
                std::cerr << "Failed to decode frame.\n";
                break;
            }

            cv::imshow("Video Stream", frame);
            if (cv::waitKey(1) == 27) { // ESC
                std::cout << "[Video] ESC pressed, stop streaming." << std::endl;
                break;
            }
        }
        else if (cmd.rfind("END_VIDEO_STREAMING", 0) == 0) {
            std::cout << "[Video] Received END_VIDEO_STREAMING. Done." << std::endl;
            break;
        }
        else {
            // 收到未知指令 => 結束
            std::cout << "[Video] Unknown cmd: " << cmd << std::endl;
            break;
        }
    }
    // 跳出迴圈後
    for (int i = 0; i < 5; i++) {
        cv::waitKey(1);
    }
    cv::destroyWindow("Video Stream");
    // 再等一下，確保事件處理結束
    for (int i = 0; i < 5; i++) {
        cv::waitKey(1);
    }
    std::cout << "Video streaming ended.\n";
}

