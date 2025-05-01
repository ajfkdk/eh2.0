#include "CaptureRegistry.h"

std::map<std::string, CaptureCreator>& CaptureRegistry::getRegistry() {
    static std::map<std::string, CaptureCreator> registry;
    return registry;
}

bool CaptureRegistry::registerImplementation(const std::string& name, CaptureCreator creator) {
    if (!creator) {
        return false;
    }
    // 把采集器注册到注册表中
    getRegistry()[name] = creator;
    return true;
}

std::shared_ptr<IFrameCapture> CaptureRegistry::create(const std::string& name, const CaptureConfig& config) {
    auto& registry = getRegistry();
    auto it = registry.find(name);

    // 如果没有找到对应的采集器，返回nullptr
    if (it == registry.end()) {
        return nullptr;
    }

    return it->second(config);
}

std::vector<std::string> CaptureRegistry::getRegisteredImplementations() {
    std::vector<std::string> names;
    auto& registry = getRegistry();

    for (const auto& entry : registry) {
        names.push_back(entry.first);
    }

    return names;
}
