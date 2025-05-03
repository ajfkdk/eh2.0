#include "ScreenCaptureWindows.h"
#include "RingBuffer.h"
#include "CaptureFactory.h"
#include "CaptureRegistry.h"
#include <iostream>

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

ScreenCaptureWindows::ScreenCaptureWindows() : d3dDevice(nullptr), d3dContext(nullptr),
dxgiOutputDuplication(nullptr), dxgiOutput1(nullptr), dxgiAdapter(nullptr), dxgiAdapter1(nullptr) {
    config.width = 320;
    config.height = 320;
    config.captureCenter = true;
}

ScreenCaptureWindows::~ScreenCaptureWindows() {
    release();
}

bool ScreenCaptureWindows::initializeDXGI() {
    // ����D3D�豸
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &d3dDevice,
        &featureLevel,
        &d3dContext
    );

    if (FAILED(hr)) {
        lastError = "Failed to create D3D11 device: " + std::to_string(hr);
        if (errorHandler) errorHandler(lastError, -1);
        return false;
    }

    // ��ȡDXGI�豸
    IDXGIDevice* dxgiDevice = nullptr;
    hr = d3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (FAILED(hr)) {
        lastError = "Failed to get DXGI device: " + std::to_string(hr);
        if (errorHandler) errorHandler(lastError, -2);
        releaseDXGI();
        return false;
    }

    // ��ȡDXGI������
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    dxgiDevice->Release();
    if (FAILED(hr)) {
        lastError = "Failed to get DXGI adapter: " + std::to_string(hr);
        if (errorHandler) errorHandler(lastError, -3);
        releaseDXGI();
        return false;
    }

    // ��ѡ����ȡIDXGIAdapter1�ӿ�
    hr = dxgiAdapter->QueryInterface(__uuidof(IDXGIAdapter1), reinterpret_cast<void**>(&dxgiAdapter1));
    if (FAILED(hr)) {
        // �ⲻ�����ش������ǿ��ܲ���ҪAdapter1�Ĺ���
        if (debugMode.load()) {
            std::cout << "Warning: Failed to get IDXGIAdapter1 interface: " << hr << std::endl;
        }
    }

    // ��ȡ����ʾ�����
    IDXGIOutput* dxgiOutput = nullptr;
    hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    if (FAILED(hr)) {
        lastError = "Failed to get DXGI output: " + std::to_string(hr);
        if (errorHandler) errorHandler(lastError, -4);
        releaseDXGI();
        return false;
    }

    // ��ȡDXGI Output1�ӿ�
    hr = dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&dxgiOutput1));
    dxgiOutput->Release();
    if (FAILED(hr)) {
        lastError = "Failed to get DXGI Output1 interface: " + std::to_string(hr);
        if (errorHandler) errorHandler(lastError, -5);
        releaseDXGI();
        return false;
    }

    // ��ȡ���渴�ƽӿ�
    hr = dxgiOutput1->DuplicateOutput(d3dDevice, &dxgiOutputDuplication);
    if (FAILED(hr)) {
        lastError = "Failed to duplicate output: " + std::to_string(hr);
        if (errorHandler) errorHandler(lastError, -6);
        releaseDXGI();
        return false;
    }

    return true;
}

void ScreenCaptureWindows::releaseDXGI() {
    if (dxgiOutputDuplication) {
        dxgiOutputDuplication->Release();
        dxgiOutputDuplication = nullptr;
    }
    if (dxgiOutput1) {
        dxgiOutput1->Release();
        dxgiOutput1 = nullptr;
    }
    if (dxgiAdapter1) {
        dxgiAdapter1->Release();
        dxgiAdapter1 = nullptr;
    }
    if (dxgiAdapter) {
        dxgiAdapter->Release();
        dxgiAdapter = nullptr;
    }
    if (d3dContext) {
        d3dContext->Release();
        d3dContext = nullptr;
    }
    if (d3dDevice) {
        d3dDevice->Release();
        d3dDevice = nullptr;
    }
}

bool ScreenCaptureWindows::initialize() {
    if (initialized.load()) {
        return true;
    }

    if (!initializeDXGI()) {
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
    releaseDXGI();

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

    try {
        // ��ȡ��һ֡
        DXGI_OUTDUPL_FRAME_INFO frameInfo;
        IDXGIResource* desktopResource = nullptr;
        HRESULT hr = dxgiOutputDuplication->AcquireNextFrame(100, &frameInfo, &desktopResource);

        // �����ʱ��ʧ�ܣ����ؿ�֡
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            return cv::Mat();
        }
        else if (FAILED(hr)) {
            lastError = "Failed to acquire next frame: " + std::to_string(hr);
            if (errorHandler) errorHandler(lastError, -7);
            return cv::Mat();
        }

        // ��ȡ�������
        ID3D11Texture2D* desktopTexture = nullptr;
        hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&desktopTexture));
        desktopResource->Release();

        if (FAILED(hr)) {
            lastError = "Failed to get desktop texture: " + std::to_string(hr);
            if (errorHandler) errorHandler(lastError, -8);
            dxgiOutputDuplication->ReleaseFrame();
            return cv::Mat();
        }

        // ��ȡ������������
        D3D11_TEXTURE2D_DESC desc;
        desktopTexture->GetDesc(&desc);

        // ��������ӳ�������
        D3D11_TEXTURE2D_DESC copydesc;
        copydesc.Width = desc.Width;
        copydesc.Height = desc.Height;
        copydesc.MipLevels = 1;
        copydesc.ArraySize = 1;
        copydesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        copydesc.SampleDesc.Count = 1;
        copydesc.SampleDesc.Quality = 0;
        copydesc.Usage = D3D11_USAGE_STAGING;
        copydesc.BindFlags = 0;
        copydesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        copydesc.MiscFlags = 0;

        ID3D11Texture2D* copyTexture = nullptr;
        hr = d3dDevice->CreateTexture2D(&copydesc, nullptr, &copyTexture);
        if (FAILED(hr)) {
            lastError = "Failed to create copy texture: " + std::to_string(hr);
            if (errorHandler) errorHandler(lastError, -9);
            desktopTexture->Release();
            dxgiOutputDuplication->ReleaseFrame();
            return cv::Mat();
        }

        // ��������
        d3dContext->CopyResource(copyTexture, desktopTexture);
        desktopTexture->Release();

        // ���������Թ���ȡ
        D3D11_MAPPED_SUBRESOURCE mappedSubresource;
        hr = d3dContext->Map(copyTexture, 0, D3D11_MAP_READ, 0, &mappedSubresource);
        if (FAILED(hr)) {
            lastError = "Failed to map texture: " + std::to_string(hr);
            if (errorHandler) errorHandler(lastError, -10);
            copyTexture->Release();
            dxgiOutputDuplication->ReleaseFrame();
            return cv::Mat();
        }

        // ���㲶������
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

        // ȷ����������Ļ��Χ��
        left = max(0, min(left, static_cast<int>(desc.Width - 1)));
        top = max(0, min(top, static_cast<int>(desc.Height - 1)));
        width = min(width, static_cast<int>(desc.Width - left));
        height = min(height, static_cast<int>(desc.Height - top));

        // ����OpenCVͼ��
        cv::Mat fullScreenImage(desc.Height, desc.Width, CV_8UC4, mappedSubresource.pData, mappedSubresource.RowPitch);
        cv::Mat result;

        // ��ȡ��������
        cv::Rect regionRect(left, top, width, height);
        cv::Mat regionImage = fullScreenImage(regionRect).clone();

        // ת����ɫ��ʽ
        cv::cvtColor(regionImage, result, cv::COLOR_BGRA2BGR);

        // ��������
        d3dContext->Unmap(copyTexture, 0);
        copyTexture->Release();

        // �ͷ�֡
        dxgiOutputDuplication->ReleaseFrame();

        return result;
    }
    catch (const std::exception& e) {
        lastError = std::string("Exception in captureFrame: ") + e.what();
        if (errorHandler) errorHandler(lastError, -11);
        status.store(CaptureStatus::CAP_ERROR);
        dxgiOutputDuplication->ReleaseFrame();
        return cv::Mat();
    }
}

// ========== ������ص� ==========

void handleCaptureError(const std::string& message, int code) {
    std::cerr << "Capture error [" << code << "]: " << message << std::endl;
}

// ========== ע���Զ���ɼ�ʵ�� ==========

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

    // ����������ע�������ɼ�ʵ��
}

// ========== �ɼ��̺߳��� ==========

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
        capturer = CaptureFactory::createCapture(CaptureType::WINDOWS_SCREEN, config);

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