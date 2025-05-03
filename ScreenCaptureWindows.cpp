#include "ScreenCaptureWindows.h"
#include "RingBuffer.h"
#include "CaptureFactory.h"
#include "CaptureRegistry.h"
#include <iostream>

// ========== 全局变量和结构体定义 ==========

// 定义帧类型
struct Frame {
    cv::Mat image;
    std::chrono::high_resolution_clock::time_point timestamp;

    Frame() = default;

    Frame(const cv::Mat& img) : image(img) {
        timestamp = std::chrono::high_resolution_clock::now();
    }
};

// 环形缓冲区大小设置
constexpr size_t BUFFER_SIZE = 120;  // 2秒@60fps

// 模块内部全局变量
namespace {
    // 环形缓冲区实例
    RingBuffer<Frame, BUFFER_SIZE> frameBuffer;

    // 控制标志
    std::atomic<bool> running{ false };
    std::atomic<bool> debugMode{ false };
    std::atomic<bool> showCaptureDebug{ false }; // 新增：控制是否显示捕获的屏幕信息

    // FPS计算相关变量
    int frameCount = 0;
    float fps = 0.0f;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastFpsTime;

    // 采集器实例
    std::shared_ptr<IFrameCapture> capturer;
}

// ========== ScreenCaptureWindows类实现 ==========

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
    // 创建D3D设备
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

    // 获取DXGI设备
    IDXGIDevice* dxgiDevice = nullptr;
    hr = d3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (FAILED(hr)) {
        lastError = "Failed to get DXGI device: " + std::to_string(hr);
        if (errorHandler) errorHandler(lastError, -2);
        releaseDXGI();
        return false;
    }

    // 获取DXGI适配器
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    dxgiDevice->Release();
    if (FAILED(hr)) {
        lastError = "Failed to get DXGI adapter: " + std::to_string(hr);
        if (errorHandler) errorHandler(lastError, -3);
        releaseDXGI();
        return false;
    }

    // 可选：获取IDXGIAdapter1接口
    hr = dxgiAdapter->QueryInterface(__uuidof(IDXGIAdapter1), reinterpret_cast<void**>(&dxgiAdapter1));
    if (FAILED(hr)) {
        // 这不是严重错误，我们可能不需要Adapter1的功能
        if (debugMode.load()) {
            std::cout << "Warning: Failed to get IDXGIAdapter1 interface: " << hr << std::endl;
        }
    }

    // 获取主显示器输出
    IDXGIOutput* dxgiOutput = nullptr;
    hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    if (FAILED(hr)) {
        lastError = "Failed to get DXGI output: " + std::to_string(hr);
        if (errorHandler) errorHandler(lastError, -4);
        releaseDXGI();
        return false;
    }

    // 获取DXGI Output1接口
    hr = dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&dxgiOutput1));
    dxgiOutput->Release();
    if (FAILED(hr)) {
        lastError = "Failed to get DXGI Output1 interface: " + std::to_string(hr);
        if (errorHandler) errorHandler(lastError, -5);
        releaseDXGI();
        return false;
    }

    // 获取桌面复制接口
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
    // 保存新配置
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
        // 获取下一帧
        DXGI_OUTDUPL_FRAME_INFO frameInfo;
        IDXGIResource* desktopResource = nullptr;
        HRESULT hr = dxgiOutputDuplication->AcquireNextFrame(100, &frameInfo, &desktopResource);

        // 如果超时或失败，返回空帧
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            return cv::Mat();
        }
        else if (FAILED(hr)) {
            lastError = "Failed to acquire next frame: " + std::to_string(hr);
            if (errorHandler) errorHandler(lastError, -7);
            return cv::Mat();
        }

        // 获取桌面表面
        ID3D11Texture2D* desktopTexture = nullptr;
        hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&desktopTexture));
        desktopResource->Release();

        if (FAILED(hr)) {
            lastError = "Failed to get desktop texture: " + std::to_string(hr);
            if (errorHandler) errorHandler(lastError, -8);
            dxgiOutputDuplication->ReleaseFrame();
            return cv::Mat();
        }

        // 获取桌面纹理描述
        D3D11_TEXTURE2D_DESC desc;
        desktopTexture->GetDesc(&desc);

        // 创建可以映射的纹理
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

        // 复制纹理
        d3dContext->CopyResource(copyTexture, desktopTexture);
        desktopTexture->Release();

        // 锁定纹理以供读取
        D3D11_MAPPED_SUBRESOURCE mappedSubresource;
        hr = d3dContext->Map(copyTexture, 0, D3D11_MAP_READ, 0, &mappedSubresource);
        if (FAILED(hr)) {
            lastError = "Failed to map texture: " + std::to_string(hr);
            if (errorHandler) errorHandler(lastError, -10);
            copyTexture->Release();
            dxgiOutputDuplication->ReleaseFrame();
            return cv::Mat();
        }

        // 计算捕获区域
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

        // 确保坐标在屏幕范围内
        left = max(0, min(left, static_cast<int>(desc.Width - 1)));
        top = max(0, min(top, static_cast<int>(desc.Height - 1)));
        width = min(width, static_cast<int>(desc.Width - left));
        height = min(height, static_cast<int>(desc.Height - top));

        // 创建OpenCV图像
        cv::Mat fullScreenImage(desc.Height, desc.Width, CV_8UC4, mappedSubresource.pData, mappedSubresource.RowPitch);
        cv::Mat result;

        // 提取所需区域
        cv::Rect regionRect(left, top, width, height);
        cv::Mat regionImage = fullScreenImage(regionRect).clone();

        // 转换颜色格式
        cv::cvtColor(regionImage, result, cv::COLOR_BGRA2BGR);

        // 解锁纹理
        d3dContext->Unmap(copyTexture, 0);
        copyTexture->Release();

        // 释放帧
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

// ========== 错误处理回调 ==========

void handleCaptureError(const std::string& message, int code) {
    std::cerr << "Capture error [" << code << "]: " << message << std::endl;
}

// ========== 注册自定义采集实现 ==========

void registerCustomCaptures() {
    // 注册Windows屏幕采集
    CaptureRegistry::registerImplementation("WindowsScreen",
        [](const CaptureConfig& config) -> std::shared_ptr<IFrameCapture> {
            auto capture = std::make_shared<ScreenCaptureWindows>();
            capture->configure(config);
            capture->initialize();
            return capture;
        }
    );

    // 可以在这里注册其他采集实现
}

// ========== 采集线程函数 ==========

void captureThreadFunc(std::shared_ptr<IFrameCapture> capturer) {
    std::cout << "Capture thread started" << std::endl;

    // 初始化FPS计时
    lastFpsTime = std::chrono::high_resolution_clock::now();

    // 开始采集
    if (!capturer->start()) {
        std::cerr << "Failed to start capturer: " << capturer->getLastError() << std::endl;
        return;
    }

    while (running.load()) {
        auto startTime = std::chrono::high_resolution_clock::now();

        // 采集帧
        cv::Mat frame = capturer->captureFrame();

        if (!frame.empty()) {
            // 创建帧对象并放入缓冲区
            Frame newFrame(frame);
            frameBuffer.write(newFrame);

            if (debugMode.load()) {
                auto endTime = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> elapsed = endTime - startTime;
                std::cout << "Capture time: " << elapsed.count() << " ms" << std::endl;
            }

            // 如果开启了屏幕捕获调试，显示采集到的屏幕信息
            if (showCaptureDebug.load()) {
                // 更新FPS计算
                frameCount++;
                auto now = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> elapsed = now - lastFpsTime;

                // 每秒更新一次FPS
                if (elapsed.count() >= 1.0) {
                    fps = frameCount / elapsed.count();
                    frameCount = 0;
                    lastFpsTime = now;
                }

                // 在图像左上角显示FPS
                cv::Mat displayFrame = frame.clone();
                std::string fpsText = "FPS: " + std::to_string(static_cast<int>(fps));
                cv::putText(displayFrame, fpsText, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 255, 0), 2);

                // 使用OpenCV显示捕获的图像
                cv::namedWindow("Screen Capture Debug", cv::WINDOW_AUTOSIZE);
                cv::imshow("Screen Capture Debug", displayFrame);
                cv::waitKey(1); // 必要的，保持窗口更新
            }
        }

        // 控制帧率
        std::this_thread::sleep_for(std::chrono::milliseconds(2)); // ~60fps
    }

    // 如果显示窗口打开，关闭它
    if (showCaptureDebug.load()) {
        cv::destroyWindow("Screen Capture Debug");
    }

    // 停止采集
    capturer->stop();
    std::cout << "Capture thread stopped" << std::endl;
}

// ========== 对外接口实现 ==========

namespace CaptureModule {
    // 获取最新帧
    bool GetLatestCaptureFrame(Frame& frame) {
        return frameBuffer.readLatest(frame);
    }

    // 等待并获取帧
    bool WaitForFrame(Frame& frame) {
        return frameBuffer.read(frame, true);
    }

    // 初始化模块
    std::thread Initialize() {
        // 注册采集实现
        registerCustomCaptures();

        // 输出可用的采集实现
        std::cout << "Available capture implementations:" << std::endl;
        for (const auto& name : CaptureRegistry::getRegisteredImplementations()) {
            std::cout << " - " << name << std::endl;
        }

        // 创建采集配置
        CaptureConfig config;
        config.width = 320;
        config.height = 320;
        config.captureCenter = true;

        // 创建采集器
        capturer = CaptureFactory::createCapture(CaptureType::WINDOWS_SCREEN, config);

        if (!capturer) {
            throw std::runtime_error("Failed to create capturer");
        }

        // 注册错误处理器
        capturer->registerErrorHandler(handleCaptureError);

        // 准备启动
        running.store(true);
        frameBuffer.open();

        // 启动采集线程
        return std::thread(captureThreadFunc, capturer);
    }

    // 清理资源
    void Cleanup() {
        running.store(false);
        frameBuffer.close();

        if (capturer) {
            capturer->stop();
            capturer->release();
        }
    }

    // 检查是否在运行
    bool IsRunning() {
        return running.load();
    }

    // 停止采集
    void Stop() {
        running.store(false);
    }

    // 设置调试模式
    void SetDebugMode(bool enabled) {
        debugMode.store(enabled);
    }

    // 设置屏幕捕获调试模式
    void SetCaptureDebug(bool enabled) {
        showCaptureDebug.store(enabled);
    }
}