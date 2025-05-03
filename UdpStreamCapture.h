#pragma once
#ifndef UDP_STREAM_CAPTURE_H
#define UDP_STREAM_CAPTURE_H

#include "IFrameCapture.h"
#include <opencv2/opencv.hpp>
#include <atomic>
#include <string>

class UdpStreamCapture : public IFrameCapture {
private:
    CaptureConfig config;
    std::atomic<CaptureStatus> status{ CaptureStatus::CAP_STOPPED };
    std::string lastError;
    ErrorCallback errorHandler;
    std::atomic<bool> initialized{ false };

    cv::VideoCapture cap;
    std::string udpAddress;

public:
    UdpStreamCapture();
    ~UdpStreamCapture();

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

    // 特有方法
    void setUdpAddress(const std::string& address);
    std::string getUdpAddress() const;
};

#endif // UDP_STREAM_CAPTURE_H