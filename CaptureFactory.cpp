#include "CaptureFactory.h"
#include "ScreenCaptureWindows.h"
#include "UdpStreamCapture.h"  // 包含新的头文件
#include "CaptureRegistry.h"

std::shared_ptr<IFrameCapture> CaptureFactory::createCapture(CaptureType type, const CaptureConfig& config) {
    std::shared_ptr<IFrameCapture> capture = nullptr;

    switch (type) {
    case CaptureType::WINDOWS_SCREEN:
        capture = std::make_shared<ScreenCaptureWindows>();
        break;
    case CaptureType::UDP_STREAM:  // 添加UDP流类型处理
        capture = std::make_shared<UdpStreamCapture>();
        break;
        // 其他类型的实现...
    default:
        // 未知类型返回空指针
        return nullptr;
    }

    if (capture) {
        capture->configure(config);
        capture->initialize();
    }

    return capture;
}

std::shared_ptr<IFrameCapture> CaptureFactory::createCapture(const std::string& name, const CaptureConfig& config) {
    // 从注册表中创建
    return CaptureRegistry::create(name, config);
}