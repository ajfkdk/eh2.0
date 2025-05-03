#ifndef SCREEN_CAPTURE_WINDOWS_H
#define SCREEN_CAPTURE_WINDOWS_H

#include "IFrameCapture.h"
#include <windows.h>
#include <atomic>
#include <thread>
#include <chrono>

// 提供初始化接口
namespace CaptureModule {
    // 初始化采集模块，返回采集线程
    std::thread Initialize();

    // 清理资源
    void Cleanup();

    // 检查是否正在运行
    bool IsRunning();

    // 停止采集
    void Stop();

    // 设置调试模式
    void SetDebugMode(bool enabled);

    // 设置捕获调试模式，用于显示采集到的屏幕信息
    void SetCaptureDebug(bool enabled);
}

class ScreenCaptureWindows : public IFrameCapture {
private:
    CaptureConfig config;
    std::atomic<CaptureStatus> status{ CaptureStatus::CAP_STOPPED };
    std::string lastError;
    ErrorCallback errorHandler;

    HDC hdesktop;
    HDC hdc;
    std::atomic<bool> initialized{ false };

    BITMAPINFOHEADER createBitmapHeader(int width, int height);
    RECT getCenterRegion();

public:
    ScreenCaptureWindows();
    ~ScreenCaptureWindows();

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
};

#endif // SCREEN_CAPTURE_WINDOWS_H