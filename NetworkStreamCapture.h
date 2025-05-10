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

// ����һ��ʱ���ö�٣���ʾ��ͬ�Ĵ���׶�
enum class TimestampStage : uint8_t {
    START_CAPTURE = 0,      // ��ʼ��ͼ
    FINISH_CAPTURE,         // ��ɽ�ͼ
    START_COMPRESSION,      // ��ʼѹ��
    FINISH_COMPRESSION,     // ���ѹ��
    START_SENDING,          // ��ʼ����
    FINISH_SENDING,         // ��ɷ���
    START_RECEIVING,        // ��ʼ����
    FINISH_RECEIVING,       // ��ɽ���
    START_DECODING,         // ��ʼ����
    FINISH_DECODING,        // ��ɽ���
    STAGE_COUNT             // �׶�����
};

// ���ݰ�ͷ�ṹ
#pragma pack(push, 1)
struct PacketHeader {
    uint32_t frameId;                                               // ֡ID
    uint32_t dataSize;                                              // ͼ�����ݴ�С
    int64_t timestamps[static_cast<size_t>(TimestampStage::STAGE_COUNT)]; // ���׶�ʱ���
};
#pragma pack(pop)

// ������ݰ���С
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

    // IFrameCaptureʵ��
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