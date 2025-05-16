#include "ConfigModule.h"
#include <iostream>
#include <algorithm>

// 静态成员初始化
std::unique_ptr<KeyboardListener> ConfigModule::keyboardListener = nullptr;
std::shared_ptr<HelperConfig> ConfigModule::config = std::make_shared<HelperConfig>();
std::atomic<bool> ConfigModule::configDebugEnabled(false);
std::thread ConfigModule::configDebugThread;

void ConfigModule::Initialize() {
    // 创建键盘监听器
    keyboardListener = std::make_unique<KeyboardListener>();
    keyboardListener->onKeyPress = HandleKeyPress;

    std::cout << "配置模块已初始化" << std::endl;
}

void ConfigModule::Cleanup() {
    // 停止键盘监听
    if (keyboardListener) {
        keyboardListener->Stop();
    }

    // 停止调试线程
    if (configDebugEnabled.load()) {
        configDebugEnabled.store(false);
        if (configDebugThread.joinable()) {
            configDebugThread.join();
        }
    }

    std::cout << "配置模块已清理" << std::endl;
}

std::shared_ptr<HelperConfig> ConfigModule::GetConfig() {
    return config;
}

void ConfigModule::EnableConfigDebug(bool enable) {
    bool currentState = configDebugEnabled.load();

    // 如果状态没变，不做任何操作
    if (currentState == enable) return;

    configDebugEnabled.store(enable);

    // 如果是启用配置调试
    if (enable) {
        // 启动键盘监听
        keyboardListener->Start();

        // 打印当前配置参数
        std::cout << "参数调试模式已启用" << std::endl;
        std::cout << "使用以下键调整参数：" << std::endl;
        std::cout << "Q/W: 调整aimFov  - 当前值: " << config->aimFov.load() << std::endl;
        std::cout << "C: 切换压枪开关 - 当前状态: " << (config->isAutoRecoilEnabled.load() ? "开启" : "关闭") << std::endl;
        std::cout << "D/F: 调整压枪力度 - 当前值: " << config->pressForce.load() << std::endl;
        std::cout << "E/R: 调整压枪持续时间 - 当前值: " << config->pressTime.load() << "ms" << std::endl;

        // 启动配置调试线程
        configDebugThread = std::thread(ConfigDebugLoop);
    }
    else {
        // 停止键盘监听
        keyboardListener->Stop();

        // 等待线程结束
        if (configDebugThread.joinable()) {
            configDebugThread.join();
        }

        std::cout << "参数调试模式已禁用" << std::endl;
    }
}

bool ConfigModule::IsConfigDebugEnabled() {
    return configDebugEnabled.load();
}

void ConfigModule::HandleKeyPress(int key) {
    float pressForce = config->pressForce.load();
    int pressTime = config->pressTime.load();
    int aimFov = config->aimFov.load();
    float predictAlpha = config->predictAlpha.load();

    const float pressForceStep = 0.5f;
    const int pressTimeStep = 20;

    switch (key) {
    case 'Q':
        aimFov = max(0, aimFov - 1);
        config->aimFov.store(aimFov);
        std::cout << "aimFov 调整为: " << aimFov << std::endl;
        break;
    case 'W':
        aimFov += 1;
        config->aimFov.store(aimFov);
        std::cout << "aimFov 调整为: " << aimFov << std::endl;
        break;
    case 'C': // 切换压枪开关
        config->isAutoRecoilEnabled = !config->isAutoRecoilEnabled.load();
        std::cout << "自动压枪功能: " << (config->isAutoRecoilEnabled.load() ? "开启" : "关闭") << std::endl;
        break;
    case 'D': // 减小压枪力度
        pressForce = max(0.5f, pressForce - pressForceStep);
        config->pressForce.store(pressForce);
        std::cout << "压枪力度 调整为: " << pressForce << std::endl;
        break;
    case 'F': // 增加压枪力度
        pressForce += pressForceStep;
        config->pressForce.store(pressForce);
        std::cout << "压枪力度 调整为: " << pressForce << std::endl;
        break;
    case 'E': // 减少压枪持续时间
        pressTime = max(100, pressTime - pressTimeStep);
        config->pressTime.store(pressTime);
        std::cout << "压枪持续时间 调整为: " << pressTime << "ms" << std::endl;
        break;
    case 'R': // 增加压枪持续时间
        pressTime += pressTimeStep;
        config->pressTime.store(pressTime);
        std::cout << "压枪持续时间 调整为: " << pressTime << "ms" << std::endl;
        break;
    }
}

void ConfigModule::ConfigDebugLoop() {
    while (configDebugEnabled.load()) {
        // 线程定期睡眠，减少CPU使用
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}