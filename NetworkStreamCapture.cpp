#include "NetworkStreamCapture.h"
#include <iostream>

// UdpReceiverʵ��
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

    // ���û�������С
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

// NetworkStreamCaptureʵ��
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
        return true; // �Ѿ�������
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
        // ��������ǰ��¼ʱ���
        int64_t receiveStartTime = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // ��������
        int bytesReceived = receiver.receiveData(packetBuffer.data(), packetBuffer.size());

        // ��ɽ��պ�������¼ʱ���
        int64_t receiveEndTime = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (bytesReceived > static_cast<int>(sizeof(PacketHeader))) {
            // ����ͷ
            PacketHeader* header = reinterpret_cast<PacketHeader*>(packetBuffer.data());

            // ������µ�֡���ż�¼�ʹ���
            if (header->frameId != lastFrameId) {
                lastFrameId = header->frameId;

                // ���½��ն˵�ʱ���
                header->timestamps[static_cast<size_t>(TimestampStage::START_RECEIVING)] = receiveStartTime;
                header->timestamps[static_cast<size_t>(TimestampStage::FINISH_RECEIVING)] = receiveEndTime;

                // ��ʼ����
                header->timestamps[static_cast<size_t>(TimestampStage::START_DECODING)] = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();

                // ����ͼ��
                std::vector<uchar> imageData(
                    packetBuffer.data() + sizeof(PacketHeader),
                    packetBuffer.data() + sizeof(PacketHeader) + header->dataSize);

                cv::Mat image = cv::imdecode(imageData, cv::IMREAD_COLOR);

                // ��ɽ���
                header->timestamps[static_cast<size_t>(TimestampStage::FINISH_DECODING)] = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();

                // ���������ӳ�
                double networkDelay = 0.0;
                if (header->timestamps[static_cast<size_t>(TimestampStage::START_SENDING)] > 0) {
                    networkDelay = (receiveStartTime - header->timestamps[static_cast<size_t>(TimestampStage::START_SENDING)]) / 1000.0;
                }

                if (!image.empty()) {
                    // ��ͼ������ʾ��Ϣ
                    std::ostringstream infoText;
                    infoText << "Frame: " << header->frameId;
                    if (networkDelay > 0) {
                        infoText << " | Delay: " << std::fixed << std::setprecision(1) << networkDelay << "ms";
                    }

                    cv::putText(image, infoText.str(), cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                        cv::Scalar(0, 0, 255), 2);

                    // ���µ�ǰ֡
                    std::lock_guard<std::mutex> lock(frameMutex);
                    currentFrame = image.clone();
                }
                else {
                    std::cerr << "ͼ�����ʧ��! ���ݴ�С: " << header->dataSize << " �ֽ�" << std::endl;
                }
            }
        }
        else if (bytesReceived > 0) {
            std::cerr << "���յ������������ݰ�: " << bytesReceived << " �ֽ�" << std::endl;
        }

        // ����ѭ��Ƶ��
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