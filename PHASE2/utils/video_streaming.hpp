#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include <openssl/ssl.h>
#include <openssl/err.h>

/**
 * VideoHeader 用來存放基本的影片參數，例如:
 *  - width, height: 畫面大小
 *  - fps: 幀率
 * 若有需要可再增加 codec 之類的欄位。
 */
struct VideoHeader {
    uint32_t width;
    uint32_t height;
    float fps;
};

/**
 * 將 VideoHeader 轉換成字串 (例如 "width:height:fps")
 */
std::string videoHeaderInfo(const VideoHeader& header);

/**
 * 由「Server端」呼叫，讀取本地影片並經由 SSL 發送給 Client。
 */
void sendVideoStream(SSL* ssl, std::string filename);

/**
 * 由「Client端」呼叫，接收 Server 傳來的影片資料，並即時播放。
 */
void receiveVideoStream(SSL* ssl);
