#ifndef CAPTURE_REGISTRY_H
#define CAPTURE_REGISTRY_H

#include "IFrameCapture.h"
#include <memory>
#include <functional>
#include <string>
#include <map>

// 采集器创建函数类型
using CaptureCreator = std::function<std::shared_ptr<IFrameCapture>(const CaptureConfig&)>;

class CaptureRegistry {
private:
    static std::map<std::string, CaptureCreator>& getRegistry();

public:
    // 注册采集实现
    static bool registerImplementation(const std::string& name, CaptureCreator creator);

    // 创建采集实例
    static std::shared_ptr<IFrameCapture> create(const std::string& name, const CaptureConfig& config = CaptureConfig());

    // 获取所有已注册的实现名称
    static std::vector<std::string> getRegisteredImplementations();
};

#endif // CAPTURE_REGISTRY_H
