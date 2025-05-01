#ifndef I_FRAME_CAPTURE_H
#define I_FRAME_CAPTURE_H

#include <opencv2/opencv.hpp>
#include <string>
#include <functional>
#include <memory>

// 采集器状态枚举
enum class CaptureStatus {
    CAP_INITIALIZED,
    CAP_RUNNING,
    CAP_PAUSED,
    CAP_STOPPED,
    CAP_ERROR
};

// 采集器配置结构
struct CaptureConfig {
    int width = 320;
    int height = 320;
    bool captureCenter = true;
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    int fps = 60;
};

// 错误处理回调类型
typedef std::function<void(const std::string&, int)> ErrorCallback;

// 帧采集接口
class IFrameCapture {
public:
    virtual ~IFrameCapture() = default;

    // 生命周期管理
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool pause() = 0;
    virtual bool resume() = 0;
    virtual bool release() = 0;

    // 状态查询
    virtual CaptureStatus getStatus() const = 0;
    virtual bool isRunning() const = 0;
    virtual std::vector<std::string> getCapabilities() const = 0;

    // 配置管理
    virtual bool configure(const CaptureConfig& config) = 0;
    virtual CaptureConfig getConfiguration() const = 0;

    // 错误处理
    virtual void registerErrorHandler(ErrorCallback handler) = 0;
    virtual std::string getLastError() const = 0;

    // 帧获取 - 实现可以选择最适合的方式
    virtual cv::Mat captureFrame() = 0;
};

#endif // I_FRAME_CAPTURE_H
