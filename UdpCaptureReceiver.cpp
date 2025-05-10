#include "UdpCaptureReceiver.h"
#include <iostream>

UdpCaptureReceiver::UdpCaptureReceiver() : lastFrameId(0) {
    packetBuffer.resize(MAX_PACKET_SIZE);
    config.width = 320;
    config.height = 320;
}

UdpCaptureReceiver::~UdpCaptureReceiver() {
    release();
}

bool UdpCaptureReceiver::initialize() {
    if (initialized.load()) {
        return true;
    }

    try {
        // �������л�ȡ�˿ڣ�Ĭ��8888
        int port = config.fps;  // ʹ��fps�ֶδ洢�˿ںţ��ɸ���ʵ���������
        if (port <= 0 || port > 65535) {
            port = 8888; // Ĭ�϶˿�
        }

        receiver = std::make_unique<UdpReceiver>(port);
        initialized.store(true);
        status.store(CaptureStatus::CAP_INITIALIZED);
        lastError = "";

        std::cout << "UDP Capture Receiver initialized on port " << port << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        lastError = std::string("Failed to initialize UDP receiver: ") + e.what();
        if (errorHandler) errorHandler(lastError, -1);
        return false;
    }
}

bool UdpCaptureReceiver::start() {
    if (!initialized.load() && !initialize()) {
        lastError = "Failed to initialize receiver";
        if (errorHandler) errorHandler(lastError, -2);
        return false;
    }

    status.store(CaptureStatus::CAP_RUNNING);
    return true;
}

bool UdpCaptureReceiver::stop() {
    status.store(CaptureStatus::CAP_STOPPED);
    return true;
}

bool UdpCaptureReceiver::pause() {
    if (status.load() != CaptureStatus::CAP_RUNNING) {
        return false;
    }

    status.store(CaptureStatus::CAP_PAUSED);
    return true;
}

bool UdpCaptureReceiver::resume() {
    if (status.load() != CaptureStatus::CAP_PAUSED) {
        return false;
    }

    status.store(CaptureStatus::CAP_RUNNING);
    return true;
}

bool UdpCaptureReceiver::release() {
    if (!initialized.load()) {
        return true;
    }

    stop();
    receiver.reset();
    initialized.store(false);
    status.store(CaptureStatus::CAP_STOPPED);

    {
        std::lock_guard<std::mutex> lock(frameMutex);
        currentFrame.release();
    }

    return true;
}

CaptureStatus UdpCaptureReceiver::getStatus() const {
    return status.load();
}

bool UdpCaptureReceiver::isRunning() const {
    return status.load() == CaptureStatus::CAP_RUNNING;
}

std::vector<std::string> UdpCaptureReceiver::getCapabilities() const {
    return { "udp", "remote" };
}

bool UdpCaptureReceiver::configure(const CaptureConfig& newConfig) {
    // ���ѳ�ʼ��״̬�£�����ĳЩ���ÿ�����Ҫ���³�ʼ��
    bool wasInitialized = initialized.load();
    if (wasInitialized) {
        release();
    }

    config = newConfig;

    if (wasInitialized) {
        return initialize();
    }
    return true;
}

CaptureConfig UdpCaptureReceiver::getConfiguration() const {
    return config;
}

void UdpCaptureReceiver::registerErrorHandler(ErrorCallback handler) {
    errorHandler = handler;
}

std::string UdpCaptureReceiver::getLastError() const {
    return lastError;
}

cv::Mat UdpCaptureReceiver::captureFrame() {
    if (!initialized.load() || status.load() != CaptureStatus::CAP_RUNNING) {
        return cv::Mat();
    }

    try {
        // ��������
        int bytesReceived = receiver->receiveData(packetBuffer.data(), packetBuffer.size());

        if (bytesReceived > static_cast<int>(sizeof(PacketHeader))) {
            // ����ͷ
            PacketHeader* header = reinterpret_cast<PacketHeader*>(packetBuffer.data());

            // ����Ƿ�����֡
            if (header->frameId != lastFrameId) {
                lastFrameId = header->frameId;

                // ȷ�����ݴ�С����
                if (header->dataSize <= bytesReceived - sizeof(PacketHeader)) {
                    // �ӽ��յ���������ȡͼ������
                    std::vector<uchar> imageData(
                        packetBuffer.data() + sizeof(PacketHeader),
                        packetBuffer.data() + sizeof(PacketHeader) + header->dataSize);

                    // ����ͼ��
                    cv::Mat decodedImage = cv::imdecode(imageData, cv::IMREAD_COLOR);

                    if (!decodedImage.empty()) {
                        // ���µ�ǰ֡
                        std::lock_guard<std::mutex> lock(frameMutex);
                        currentFrame = decodedImage.clone();
                    }
                }
            }
        }

        // ���ص�ǰ֡��������֮ǰ��֡�����û���յ���֡��
        std::lock_guard<std::mutex> lock(frameMutex);
        return currentFrame.clone();
    }
    catch (const std::exception& e) {
        lastError = std::string("Exception in captureFrame: ") + e.what();
        if (errorHandler) errorHandler(lastError, -3);
        status.store(CaptureStatus::CAP_ERROR);
        return cv::Mat();
    }
}