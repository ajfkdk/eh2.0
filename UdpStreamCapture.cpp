#include "UdpStreamCapture.h"
#include <iostream>

UdpStreamCapture::UdpStreamCapture() {
    // 默认UDP地址
    udpAddress = "udp://0.0.0.0:8848";  

    // 默认配置
    config.width = 640;
    config.height = 480;
    config.fps = 30;
}

UdpStreamCapture::~UdpStreamCapture() {
    release();
}

// UdpStreamCapture.cpp的initialize方法中
bool UdpStreamCapture::initialize() {
    if (initialized.load()) {
        return true;
    }

    try {
        std::cout << "正在尝试打开UDP流: " << udpAddress << std::endl;

        // 使用FFMPEG后端打开UDP流
        cap.open(udpAddress, cv::CAP_FFMPEG);

        if (!cap.isOpened()) {
            std::cout << "无法打开UDP流，尝试替代地址格式..." << std::endl;

            // 尝试替代地址格式
            std::string altAddress = "udp://:8848";
            std::cout << "尝试: " << altAddress << std::endl;
            cap.open(altAddress, cv::CAP_FFMPEG);

            if (!cap.isOpened()) {
                lastError = "Failed to open UDP stream at " + udpAddress;
                if (errorHandler) errorHandler(lastError, -1);
                std::cout << "所有尝试均失败" << std::endl;
                return false;
            }
        }

        std::cout << "成功打开UDP流" << std::endl;

        // 设置捕获属性(如果需要)
        if (config.width > 0) cap.set(cv::CAP_PROP_FRAME_WIDTH, config.width);
        if (config.height > 0) cap.set(cv::CAP_PROP_FRAME_HEIGHT, config.height);
        if (config.fps > 0) cap.set(cv::CAP_PROP_FPS, config.fps);

        initialized.store(true);
        status.store(CaptureStatus::CAP_INITIALIZED);
        return true;
    }
    catch (const std::exception& e) {
        lastError = std::string("Exception in initialize: ") + e.what();
        if (errorHandler) errorHandler(lastError, -2);
        std::cout << "初始化时发生异常: " << e.what() << std::endl;
        return false;
    }
}
bool UdpStreamCapture::start() {
    if (!initialized.load() && !initialize()) {
        return false;
    }

    status.store(CaptureStatus::CAP_RUNNING);
    return true;
}

bool UdpStreamCapture::stop() {
    if (status.load() == CaptureStatus::CAP_STOPPED) {
        return true;
    }

    status.store(CaptureStatus::CAP_STOPPED);
    return true;
}

bool UdpStreamCapture::pause() {
    if (status.load() != CaptureStatus::CAP_RUNNING) {
        return false;
    }

    status.store(CaptureStatus::CAP_PAUSED);
    return true;
}

bool UdpStreamCapture::resume() {
    if (status.load() != CaptureStatus::CAP_PAUSED) {
        return false;
    }

    status.store(CaptureStatus::CAP_RUNNING);
    return true;
}

bool UdpStreamCapture::release() {
    if (!initialized.load()) {
        return true;
    }

    stop();

    cap.release();
    initialized.store(false);
    status.store(CaptureStatus::CAP_STOPPED);
    return true;
}

CaptureStatus UdpStreamCapture::getStatus() const {
    return status.load();
}

bool UdpStreamCapture::isRunning() const {
    return status.load() == CaptureStatus::CAP_RUNNING;
}

std::vector<std::string> UdpStreamCapture::getCapabilities() const {
    return { "udp_stream" };
}

bool UdpStreamCapture::configure(const CaptureConfig& newConfig) {
    config = newConfig;
    return true;
}

CaptureConfig UdpStreamCapture::getConfiguration() const {
    return config;
}

void UdpStreamCapture::registerErrorHandler(ErrorCallback handler) {
    errorHandler = handler;
}

std::string UdpStreamCapture::getLastError() const {
    return lastError;
}

cv::Mat UdpStreamCapture::captureFrame() {
    if (!initialized.load() || status.load() != CaptureStatus::CAP_RUNNING) {
        return cv::Mat();
    }

    try {
        cv::Mat frame;
        if (!cap.read(frame)) {
            lastError = "Failed to read frame from UDP stream";
            if (errorHandler) errorHandler(lastError, -3);
            return cv::Mat();
        }

        return frame;
    }
    catch (const std::exception& e) {
        lastError = std::string("Exception in captureFrame: ") + e.what();
        if (errorHandler) errorHandler(lastError, -4);
        status.store(CaptureStatus::CAP_ERROR);
        return cv::Mat();
    }
}

void UdpStreamCapture::setUdpAddress(const std::string& address) {
    udpAddress = address;
    // 如果已经初始化，需要重新初始化以应用新地址
    if (initialized.load()) {
        release();
        initialize();
    }
}

std::string UdpStreamCapture::getUdpAddress() const {
    return udpAddress;
}