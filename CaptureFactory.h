#ifndef CAPTURE_FACTORY_H
#define CAPTURE_FACTORY_H

#include "IFrameCapture.h"
#include <memory>
#include <string>

// 采集器类型枚举
enum class CaptureType {
    WINDOWS_SCREEN,
    DIRECTX,
    OPENCV_CAMERA,
    NETWORK_STREAM,  // 新增网络流类型
    // 可扩展更多类型
};

class CaptureFactory {
public:
    // 根据类型创建采集器实例
    static std::shared_ptr<IFrameCapture> createCapture(CaptureType type, const CaptureConfig& config = CaptureConfig());

    // 根据名称创建采集器实例（用于插件系统）
    static std::shared_ptr<IFrameCapture> createCapture(const std::string& name, const CaptureConfig& config = CaptureConfig());
};

#endif // CAPTURE_FACTORY_H