#pragma once
#ifndef NETWORK_STREAM_CAPTURE_H
#define NETWORK_STREAM_CAPTURE_H

#include "IFrameCapture.h"
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

// 定义一个时间戳枚举，表示不同的处理阶段
enum class TimestampStage : uint8_t {
    START_CAPTURE = 0,      // 开始截图
    FINISH_CAPTURE,         // 完成截图
    START_COMPRESSION,      // 开始压缩
    FINISH_COMPRESSION,     // 完成压缩
    START_SENDING,          // 开始发送
    FINISH_SENDING,         // 完成发送
    START_RECEIVING,        // 开始接收
    FINISH_RECEIVING,       // 完成接收
    START_DECODING,         // 开始解码
    FINISH_DECODING,        // 完成解码
    STAGE_COUNT             // 阶段总数
};

// 数据包头结构
#pragma pack(push, 1)
struct PacketHeader {
    uint32_t frameId;                                               // 帧ID
    uint32_t dataSize;                                              // 图像数据大小
    int64_t timestamps[static_cast<size_t>(TimestampStage::STAGE_COUNT)]; // 各阶段时间戳
};
#pragma pack(pop)

// 最大数据包大小
constexpr int MAX_PACKET_SIZE = 320 * 320 * 3 + sizeof(PacketHeader);

class UdpReceiver {
public:
    UdpReceiver(int port);
    ~UdpReceiver();

    int receiveData(char* buffer, int bufferSize);
    bool isInitialized() const { return isInitialized_; }

private:
    bool initialize();
    int port_;
    SOCKET serverSocket_;
    bool isInitialized_;
};

class NetworkStreamCapture : public IFrameCapture {
private:
    CaptureConfig config;
    std::atomic<CaptureStatus> status{ CaptureStatus::CAP_STOPPED };
    std::string lastError;
    ErrorCallback errorHandler;

    UdpReceiver receiver;
    std::vector<char> packetBuffer;
    std::mutex frameMutex;
    cv::Mat currentFrame;

    std::atomic<bool> initialized{ false };
    std::atomic<bool> threadRunning{ false };
    std::thread receiveThread;

    void receiveLoop();

public:
    NetworkStreamCapture(int port = 8888);
    ~NetworkStreamCapture();

    // IFrameCapture实现
    bool initialize() override;
    bool start() override;
    bool stop() override;
    bool pause() override;
    bool resume() override;
    bool release() override;

    CaptureStatus getStatus() const override;
    bool isRunning() const override;
    std::vector<std::string> getCapabilities() const override;

    bool configure(const CaptureConfig& config) override;
    CaptureConfig getConfiguration() const override;

    void registerErrorHandler(ErrorCallback handler) override;
    std::string getLastError() const override;

    cv::Mat captureFrame() override;
};

#endif // NETWORK_STREAM_CAPTURE_H