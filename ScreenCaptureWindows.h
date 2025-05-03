#ifndef SCREEN_CAPTURE_WINDOWS_H
#define SCREEN_CAPTURE_WINDOWS_H

#include "IFrameCapture.h"
#include <windows.h>
#include <atomic>
#include <thread>
#include <chrono>

// �ṩ��ʼ���ӿ�
namespace CaptureModule {
    // ��ʼ���ɼ�ģ�飬���زɼ��߳�
    std::thread Initialize();

    // ������Դ
    void Cleanup();

    // ����Ƿ���������
    bool IsRunning();

    // ֹͣ�ɼ�
    void Stop();

    // ���õ���ģʽ
    void SetDebugMode(bool enabled);

    // ���ò������ģʽ��������ʾ�ɼ�������Ļ��Ϣ
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

    // IFrameCaptureʵ��
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