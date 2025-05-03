#include "UdpStreamCapture.h"
#include <iostream>

UdpStreamCapture::UdpStreamCapture() {
    // Ĭ��UDP��ַ
    udpAddress = "udp://0.0.0.0:8848";  

    // Ĭ������
    config.width = 640;
    config.height = 480;
    config.fps = 30;
}

UdpStreamCapture::~UdpStreamCapture() {
    release();
}

// UdpStreamCapture.cpp��initialize������
bool UdpStreamCapture::initialize() {
    if (initialized.load()) {
        return true;
    }

    try {
        std::cout << "���ڳ��Դ�UDP��: " << udpAddress << std::endl;

        // ʹ��FFMPEG��˴�UDP��
        cap.open(udpAddress, cv::CAP_FFMPEG);

        if (!cap.isOpened()) {
            std::cout << "�޷���UDP�������������ַ��ʽ..." << std::endl;

            // ���������ַ��ʽ
            std::string altAddress = "udp://:8848";
            std::cout << "����: " << altAddress << std::endl;
            cap.open(altAddress, cv::CAP_FFMPEG);

            if (!cap.isOpened()) {
                lastError = "Failed to open UDP stream at " + udpAddress;
                if (errorHandler) errorHandler(lastError, -1);
                std::cout << "���г��Ծ�ʧ��" << std::endl;
                return false;
            }
        }

        std::cout << "�ɹ���UDP��" << std::endl;

        // ���ò�������(�����Ҫ)
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
        std::cout << "��ʼ��ʱ�����쳣: " << e.what() << std::endl;
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
    // ����Ѿ���ʼ������Ҫ���³�ʼ����Ӧ���µ�ַ
    if (initialized.load()) {
        release();
        initialize();
    }
}

std::string UdpStreamCapture::getUdpAddress() const {
    return udpAddress;
}