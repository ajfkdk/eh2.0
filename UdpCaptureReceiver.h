#ifndef UDP_CAPTURE_RECEIVER_H
#define UDP_CAPTURE_RECEIVER_H

#include "IFrameCapture.h"

#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>
#include <opencv2/opencv.hpp>

#pragma comment(lib, "ws2_32.lib")

// 数据包头结构
#pragma pack(push, 1)
struct PacketHeader {
    uint32_t frameId;                             // 帧ID
    uint32_t dataSize;                            // 图像数据大小
    int64_t timestamps[10];                       // 各阶段时间戳
};
#pragma pack(pop)

// 最大数据包大小
constexpr int MAX_PACKET_SIZE = 320 * 320 * 3 + sizeof(PacketHeader);

class UdpReceiver {
public:
    UdpReceiver(int port) : port(port), isInitialized(false) {
        initialize();
    }

    ~UdpReceiver() {
        if (isInitialized) {
            closesocket(serverSocket);
            WSACleanup();
        }
    }

    int receiveData(char* buffer, int bufferSize) {
        if (!isInitialized) return -1;

        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);

        int bytesReceived = recvfrom(serverSocket, buffer, bufferSize, 0,
            (struct sockaddr*)&clientAddr, &clientAddrLen);

        return bytesReceived;
    }

private:
    bool initialize() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return false;
        }

        serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (serverSocket == INVALID_SOCKET) {
            WSACleanup();
            return false;
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(serverSocket);
            WSACleanup();
            return false;
        }

        // 设置缓冲区大小
        int recvBuffSize = 1024 * 1024; // 1MB
        setsockopt(serverSocket, SOL_SOCKET, SO_RCVBUF, (char*)&recvBuffSize, sizeof(recvBuffSize));

        isInitialized = true;
        return true;
    }

    int port;
    SOCKET serverSocket;
    bool isInitialized;
};

class UdpCaptureReceiver : public IFrameCapture {
private:
    CaptureConfig config;
    std::atomic<CaptureStatus> status{ CaptureStatus::CAP_STOPPED };
    std::string lastError;
    ErrorCallback errorHandler;

    std::unique_ptr<UdpReceiver> receiver;
    std::vector<char> packetBuffer;
    std::mutex frameMutex;
    cv::Mat currentFrame;
    uint32_t lastFrameId;
    std::atomic<bool> initialized{ false };

public:
    UdpCaptureReceiver();
    ~UdpCaptureReceiver();

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

#endif // UDP_CAPTURE_RECEIVER_H#pragma once
