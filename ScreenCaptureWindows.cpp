#include "ScreenCaptureWindows.h"
#include "RingBuffer.h"
#include "CaptureFactory.h"
#include "CaptureRegistry.h"
#include <iostream>
#include "UdpStreamCapture.h"

// ========== ȫ�ֱ����ͽṹ�嶨�� ==========

// ����֡����
struct Frame {
    cv::Mat image;
    std::chrono::high_resolution_clock::time_point timestamp;

    Frame() = default;

    Frame(const cv::Mat& img) : image(img) {
        timestamp = std::chrono::high_resolution_clock::now();
    }
};

// ���λ�������С����
constexpr size_t BUFFER_SIZE = 120;  // 2��@60fps

// ģ���ڲ�ȫ�ֱ���
namespace {
    // ���λ�����ʵ��
    RingBuffer<Frame, BUFFER_SIZE> frameBuffer;

    // ���Ʊ�־
    std::atomic<bool> running{ false };
    std::atomic<bool> debugMode{ false };
    std::atomic<bool> showCaptureDebug{ false }; // �����������Ƿ���ʾ�������Ļ��Ϣ

    // FPS������ر���
    int frameCount = 0;
    float fps = 0.0f;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastFpsTime;

    // �ɼ���ʵ��
    std::shared_ptr<IFrameCapture> capturer;
}

// ========== ScreenCaptureWindows��ʵ�� ==========

ScreenCaptureWindows::ScreenCaptureWindows() : hdesktop(NULL), hdc(NULL) {
    config.width = 320;
    config.height = 320;
    config.captureCenter = true;
}

ScreenCaptureWindows::~ScreenCaptureWindows() {
    release();
}

bool ScreenCaptureWindows::initialize() {
    if (initialized.load()) {
        return true;
    }

    hdesktop = GetDC(NULL);
    if (!hdesktop) {
        lastError = "Failed to get desktop DC";
        if (errorHandler) errorHandler(lastError, -1);
        return false;
    }

    hdc = CreateCompatibleDC(hdesktop);
    if (!hdc) {
        ReleaseDC(NULL, hdesktop);
        hdesktop = NULL;
        lastError = "Failed to create compatible DC";
        if (errorHandler) errorHandler(lastError, -2);
        return false;
    }

    initialized.store(true);
    status.store(CaptureStatus::CAP_INITIALIZED);
    return true;
}

bool ScreenCaptureWindows::start() {
    if (!initialized.load() && !initialize()) {
        return false;
    }

    status.store(CaptureStatus::CAP_RUNNING);
    return true;
}

bool ScreenCaptureWindows::stop() {
    if (status.load() == CaptureStatus::CAP_STOPPED) {
        return true;
    }

    status.store(CaptureStatus::CAP_STOPPED);
    return true;
}

bool ScreenCaptureWindows::pause() {
    if (status.load() != CaptureStatus::CAP_RUNNING) {
        return false;
    }

    status.store(CaptureStatus::CAP_PAUSED);
    return true;
}

bool ScreenCaptureWindows::resume() {
    if (status.load() != CaptureStatus::CAP_PAUSED) {
        return false;
    }

    status.store(CaptureStatus::CAP_RUNNING);
    return true;
}

bool ScreenCaptureWindows::release() {
    if (!initialized.load()) {
        return true;
    }

    stop();

    if (hdc) {
        DeleteDC(hdc);
        hdc = NULL;
    }

    if (hdesktop) {
        ReleaseDC(NULL, hdesktop);
        hdesktop = NULL;
    }

    initialized.store(false);
    status.store(CaptureStatus::CAP_STOPPED);
    return true;
}

CaptureStatus ScreenCaptureWindows::getStatus() const {
    return status.load();
}

bool ScreenCaptureWindows::isRunning() const {
    return status.load() == CaptureStatus::CAP_RUNNING;
}

std::vector<std::string> ScreenCaptureWindows::getCapabilities() const {
    return { "fullscreen", "region", "center" };
}

bool ScreenCaptureWindows::configure(const CaptureConfig& newConfig) {
    // ����������
    config = newConfig;
    return true;
}

CaptureConfig ScreenCaptureWindows::getConfiguration() const {
    return config;
}

void ScreenCaptureWindows::registerErrorHandler(ErrorCallback handler) {
    errorHandler = handler;
}

std::string ScreenCaptureWindows::getLastError() const {
    return lastError;
}

BITMAPINFOHEADER ScreenCaptureWindows::createBitmapHeader(int width, int height) {
    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; // ���߶ȱ�ʾ���϶���λͼ
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;
    return bi;
}

RECT ScreenCaptureWindows::getCenterRegion() {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    RECT rect;
    rect.left = (screenWidth - config.width) / 2;
    rect.top = (screenHeight - config.height) / 2;
    rect.right = rect.left + config.width;
    rect.bottom = rect.top + config.height;

    return rect;
}

cv::Mat ScreenCaptureWindows::captureFrame() {
    if (!initialized.load() || status.load() != CaptureStatus::CAP_RUNNING) {
        return cv::Mat();
    }

    int left, top, width, height;

    if (config.captureCenter) {
        RECT rect = getCenterRegion();
        left = rect.left;
        top = rect.top;
        width = config.width;
        height = config.height;
    }
    else {
        left = config.left;
        top = config.top;
        width = config.right - config.left;
        height = config.bottom - config.top;
    }

    try {
        HBITMAP hbmp = CreateCompatibleBitmap(hdesktop, width, height);
        if (!hbmp) {
            lastError = "Failed to create compatible bitmap";
            if (errorHandler) errorHandler(lastError, -3);
            return cv::Mat();
        }

        HGDIOBJ oldObject = SelectObject(hdc, hbmp);
        BitBlt(hdc, 0, 0, width, height, hdesktop, left, top, SRCCOPY);

        BITMAPINFO bmi = { 0 };
        bmi.bmiHeader = createBitmapHeader(width, height);

        std::vector<BYTE> buffer(width * height * 4);
        GetDIBits(hdc, hbmp, 0, height, buffer.data(), &bmi, DIB_RGB_COLORS);

        cv::Mat image(height, width, CV_8UC4, buffer.data());
        cv::Mat result;
        cv::cvtColor(image, result, cv::COLOR_BGRA2BGR);

        SelectObject(hdc, oldObject);
        DeleteObject(hbmp);

        return result;
    }
    catch (const std::exception& e) {
        lastError = std::string("Exception in captureFrame: ") + e.what();
        if (errorHandler) errorHandler(lastError, -4);
        status.store(CaptureStatus::CAP_ERROR);
        return cv::Mat();
    }
}

// ========== ģ�麯��ʵ�� ==========

// ������ص�
void handleCaptureError(const std::string& message, int code) {
    std::cerr << "Capture error [" << code << "]: " << message << std::endl;
}

// ע���Զ���ɼ�ʵ��
void registerCustomCaptures() {
    // ע��Windows��Ļ�ɼ�
    CaptureRegistry::registerImplementation("WindowsScreen",
        [](const CaptureConfig& config) -> std::shared_ptr<IFrameCapture> {
            auto capture = std::make_shared<ScreenCaptureWindows>();
            capture->configure(config);
            capture->initialize();
            return capture;
        }
    );

    // ע��UDP���ɼ�
    CaptureRegistry::registerImplementation("UdpStream",
        [](const CaptureConfig& config) -> std::shared_ptr<IFrameCapture> {
            auto capture = std::make_shared<UdpStreamCapture>();
            capture->configure(config);
            capture->initialize();
            return capture;
        }
    );

    // ����������ע�������ɼ�ʵ��
}


// �ɼ��̺߳���
void captureThreadFunc(std::shared_ptr<IFrameCapture> capturer) {
    std::cout << "Capture thread started" << std::endl;

    // ��ʼ��FPS��ʱ
    lastFpsTime = std::chrono::high_resolution_clock::now();

    // ��ʼ�ɼ�
    if (!capturer->start()) {
        std::cerr << "Failed to start capturer: " << capturer->getLastError() << std::endl;
        return;
    }

    while (running.load()) {
        auto startTime = std::chrono::high_resolution_clock::now();

        // �ɼ�֡
        cv::Mat frame = capturer->captureFrame();

        if (!frame.empty()) {
            // ����֡���󲢷��뻺����
            Frame newFrame(frame);
            frameBuffer.write(newFrame);

            if (debugMode.load()) {
                auto endTime = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> elapsed = endTime - startTime;
                std::cout << "Capture time: " << elapsed.count() << " ms" << std::endl;
            }

            // �����������Ļ������ԣ���ʾ�ɼ�������Ļ��Ϣ
            if (showCaptureDebug.load()) {
                // ����FPS����
                frameCount++;
                auto now = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> elapsed = now - lastFpsTime;

                // ÿ�����һ��FPS
                if (elapsed.count() >= 1.0) {
                    fps = frameCount / elapsed.count();
                    frameCount = 0;
                    lastFpsTime = now;
                }

                // ��ͼ�����Ͻ���ʾFPS
                cv::Mat displayFrame = frame.clone();
                std::string fpsText = "FPS: " + std::to_string(static_cast<int>(fps));
                cv::putText(displayFrame, fpsText, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 255, 0), 2);

                // ʹ��OpenCV��ʾ�����ͼ��
                cv::namedWindow("Screen Capture Debug", cv::WINDOW_AUTOSIZE);
                cv::imshow("Screen Capture Debug", displayFrame);
                cv::waitKey(1); // ��Ҫ�ģ����ִ��ڸ���
            }
        }

        // ����֡��
        std::this_thread::sleep_for(std::chrono::milliseconds(2)); // ~60fps
    }

    // �����ʾ���ڴ򿪣��ر���
    if (showCaptureDebug.load()) {
        cv::destroyWindow("Screen Capture Debug");
    }

    // ֹͣ�ɼ�
    capturer->stop();
    std::cout << "Capture thread stopped" << std::endl;
}

// ========== ����ӿ�ʵ�� ==========

namespace CaptureModule {
    // ��ȡ����֡
    bool GetLatestCaptureFrame(Frame& frame) {
        return frameBuffer.readLatest(frame);
    }

    // �ȴ�����ȡ֡
    bool WaitForFrame(Frame& frame) {
        return frameBuffer.read(frame, true);
    }

    // ��ʼ��ģ��
    std::thread Initialize() {
        // ע��ɼ�ʵ��
        registerCustomCaptures();

        // ������õĲɼ�ʵ��
        std::cout << "Available capture implementations:" << std::endl;
        for (const auto& name : CaptureRegistry::getRegisteredImplementations()) {
            std::cout << " - " << name << std::endl;
        }

        // �����ɼ�����
        CaptureConfig config;
        config.width = 320;
        config.height = 320;
        config.captureCenter = true;

        // �����ɼ���
        //capturer = CaptureFactory::createCapture(CaptureType::WINDOWS_SCREEN, config);
        capturer = CaptureFactory::createCapture(CaptureType::UDP_STREAM, config);

        if (!capturer) {
            throw std::runtime_error("Failed to create capturer");
        }

        // ע���������
        capturer->registerErrorHandler(handleCaptureError);

        // ׼������
        running.store(true);
        frameBuffer.open();

        // �����ɼ��߳�
        return std::thread(captureThreadFunc, capturer);
    }

    // ������Դ
    void Cleanup() {
        running.store(false);
        frameBuffer.close();

        if (capturer) {
            capturer->stop();
            capturer->release();
        }
    }

    // ����Ƿ�������
    bool IsRunning() {
        return running.load();
    }

    // ֹͣ�ɼ�
    void Stop() {
        running.store(false);
    }

    // ���õ���ģʽ
    void SetDebugMode(bool enabled) {
        debugMode.store(enabled);
    }

    // ������Ļ�������ģʽ
    void SetCaptureDebug(bool enabled) {
        showCaptureDebug.store(enabled);
    }
}