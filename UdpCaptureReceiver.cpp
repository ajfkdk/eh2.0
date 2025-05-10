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
        // 从配置中获取端口，默认8888
        int port = config.fps;  // 使用fps字段存储端口号，可根据实际需求调整
        if (port <= 0 || port > 65535) {
            port = 8888; // 默认端口
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
    // 在已初始化状态下，更改某些配置可能需要重新初始化
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
        // 接收数据
        int bytesReceived = receiver->receiveData(packetBuffer.data(), packetBuffer.size());

        if (bytesReceived > static_cast<int>(sizeof(PacketHeader))) {
            // 解析头
            PacketHeader* header = reinterpret_cast<PacketHeader*>(packetBuffer.data());

            // 检查是否是新帧
            if (header->frameId != lastFrameId) {
                lastFrameId = header->frameId;

                // 确保数据大小合理
                if (header->dataSize <= bytesReceived - sizeof(PacketHeader)) {
                    // 从接收的数据中提取图像数据
                    std::vector<uchar> imageData(
                        packetBuffer.data() + sizeof(PacketHeader),
                        packetBuffer.data() + sizeof(PacketHeader) + header->dataSize);

                    // 解码图像
                    cv::Mat decodedImage = cv::imdecode(imageData, cv::IMREAD_COLOR);

                    if (!decodedImage.empty()) {
                        // 更新当前帧
                        std::lock_guard<std::mutex> lock(frameMutex);
                        currentFrame = decodedImage.clone();
                    }
                }
            }
        }

        // 返回当前帧（可能是之前的帧，如果没有收到新帧）
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