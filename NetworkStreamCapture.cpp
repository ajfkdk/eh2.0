#include "NetworkStreamCapture.h"
#include <iostream>

// UdpReceiver实现
UdpReceiver::UdpReceiver(int port) : port_(port), serverSocket_(INVALID_SOCKET), isInitialized_(false) {
    initialize();
}

UdpReceiver::~UdpReceiver() {
    if (isInitialized_) {
        closesocket(serverSocket_);
        WSACleanup();
    }
}

bool UdpReceiver::initialize() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }

    serverSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket_ == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);

    if (bind(serverSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket_);
        WSACleanup();
        return false;
    }

    // 设置缓冲区大小
    int recvBuffSize = 1024 * 1024; // 1MB
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_RCVBUF, (char*)&recvBuffSize, sizeof(recvBuffSize)) == SOCKET_ERROR) {
        std::cerr << "Failed to set receive buffer size: " << WSAGetLastError() << std::endl;
    }

    isInitialized_ = true;
    return true;
}

int UdpReceiver::receiveData(char* buffer, int bufferSize) {
    if (!isInitialized_) return -1;

    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    int bytesReceived = recvfrom(serverSocket_, buffer, bufferSize, 0,
        (struct sockaddr*)&clientAddr, &clientAddrLen);

    return bytesReceived;
}

// NetworkStreamCapture实现
NetworkStreamCapture::NetworkStreamCapture(int port)
    : receiver(port), packetBuffer(MAX_PACKET_SIZE) {
    config.width = 320;
    config.height = 320;
    config.captureCenter = true;
}

NetworkStreamCapture::~NetworkStreamCapture() {
    release();
}

bool NetworkStreamCapture::initialize() {
    if (initialized.load()) {
        return true;
    }

    if (!receiver.isInitialized()) {
        lastError = "Failed to initialize UDP receiver";
        if (errorHandler) errorHandler(lastError, -1);
        return false;
    }

    initialized.store(true);
    status.store(CaptureStatus::CAP_INITIALIZED);
    return true;
}

bool NetworkStreamCapture::start() {
    if (!initialized.load() && !initialize()) {
        return false;
    }

    if (threadRunning.load()) {
        return true; // 已经在运行
    }

    threadRunning.store(true);
    receiveThread = std::thread(&NetworkStreamCapture::receiveLoop, this);
    status.store(CaptureStatus::CAP_RUNNING);
    return true;
}

bool NetworkStreamCapture::stop() {
    if (status.load() == CaptureStatus::CAP_STOPPED) {
        return true;
    }

    threadRunning.store(false);
    if (receiveThread.joinable()) {
        receiveThread.join();
    }

    status.store(CaptureStatus::CAP_STOPPED);
    return true;
}

bool NetworkStreamCapture::pause() {
    if (status.load() != CaptureStatus::CAP_RUNNING) {
        return false;
    }

    status.store(CaptureStatus::CAP_PAUSED);
    return true;
}

bool NetworkStreamCapture::resume() {
    if (status.load() != CaptureStatus::CAP_PAUSED) {
        return false;
    }

    status.store(CaptureStatus::CAP_RUNNING);
    return true;
}

bool NetworkStreamCapture::release() {
    if (!initialized.load()) {
        return true;
    }

    stop();
    initialized.store(false);
    status.store(CaptureStatus::CAP_STOPPED);
    return true;
}

CaptureStatus NetworkStreamCapture::getStatus() const {
    return status.load();
}

bool NetworkStreamCapture::isRunning() const {
    return status.load() == CaptureStatus::CAP_RUNNING;
}

std::vector<std::string> NetworkStreamCapture::getCapabilities() const {
    return { "stream", "network" };
}

bool NetworkStreamCapture::configure(const CaptureConfig& newConfig) {
    config = newConfig;
    return true;
}

CaptureConfig NetworkStreamCapture::getConfiguration() const {
    return config;
}

void NetworkStreamCapture::registerErrorHandler(ErrorCallback handler) {
    errorHandler = handler;
}

std::string NetworkStreamCapture::getLastError() const {
    return lastError;
}

void NetworkStreamCapture::receiveLoop() {
    uint32_t lastFrameId = 0;

    while (threadRunning.load()) {
        // 接收数据前记录时间戳
        int64_t receiveStartTime = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // 接收数据
        int bytesReceived = receiver.receiveData(packetBuffer.data(), packetBuffer.size());

        // 完成接收后立即记录时间戳
        int64_t receiveEndTime = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (bytesReceived > static_cast<int>(sizeof(PacketHeader))) {
            // 解析头
            PacketHeader* header = reinterpret_cast<PacketHeader*>(packetBuffer.data());

            // 如果是新的帧，才记录和处理
            if (header->frameId != lastFrameId) {
                lastFrameId = header->frameId;

                // 更新接收端的时间戳
                header->timestamps[static_cast<size_t>(TimestampStage::START_RECEIVING)] = receiveStartTime;
                header->timestamps[static_cast<size_t>(TimestampStage::FINISH_RECEIVING)] = receiveEndTime;

                // 开始解码
                header->timestamps[static_cast<size_t>(TimestampStage::START_DECODING)] = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();

                // 解码图像
                std::vector<uchar> imageData(
                    packetBuffer.data() + sizeof(PacketHeader),
                    packetBuffer.data() + sizeof(PacketHeader) + header->dataSize);

                cv::Mat image = cv::imdecode(imageData, cv::IMREAD_COLOR);

                // 完成解码
                header->timestamps[static_cast<size_t>(TimestampStage::FINISH_DECODING)] = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();

                // 计算网络延迟
                double networkDelay = 0.0;
                if (header->timestamps[static_cast<size_t>(TimestampStage::START_SENDING)] > 0) {
                    networkDelay = (receiveStartTime - header->timestamps[static_cast<size_t>(TimestampStage::START_SENDING)]) / 1000.0;
                }

                if (!image.empty()) {
                    // 在图像上显示信息
                    std::ostringstream infoText;
                    infoText << "Frame: " << header->frameId;
                    if (networkDelay > 0) {
                        infoText << " | Delay: " << std::fixed << std::setprecision(1) << networkDelay << "ms";
                    }

                    cv::putText(image, infoText.str(), cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                        cv::Scalar(0, 0, 255), 2);

                    // 更新当前帧
                    std::lock_guard<std::mutex> lock(frameMutex);
                    currentFrame = image.clone();
                }
                else {
                    std::cerr << "图像解码失败! 数据大小: " << header->dataSize << " 字节" << std::endl;
                }
            }
        }
        else if (bytesReceived > 0) {
            std::cerr << "接收到不完整的数据包: " << bytesReceived << " 字节" << std::endl;
        }

        // 控制循环频率
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

cv::Mat NetworkStreamCapture::captureFrame() {
    if (!initialized.load() || status.load() != CaptureStatus::CAP_RUNNING) {
        return cv::Mat();
    }

    std::lock_guard<std::mutex> lock(frameMutex);
    return currentFrame.clone();
}