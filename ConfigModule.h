#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <mutex>
#include "KeyboardListener.h"

// 配置参数结构体
struct HelperConfig {
    // 压枪相关变量
    std::atomic<bool> isAutoRecoilEnabled{ false }; // 自动压枪开关
    std::atomic<float> pressForce{ 3.0f }; // 压枪力度
    std::atomic<int> pressTime{ 800 }; // 压枪持续时间(ms)
    std::atomic<bool> needPressDownWhenAim{ true }; // 是否在自瞄时下压
    std::chrono::steady_clock::time_point pressStartTime; // 压枪开始时间
    std::chrono::steady_clock::time_point lastPressCheckTime; // 上次压枪检查时间

    // 自瞄相关参数
    std::atomic<float> predictAlpha{ 0.3f }; // 预测系数，默认0.3，范围0.2~0.5
    std::atomic<int> aimFov{ 35 }; // 瞄准视场角度
    static constexpr int targetValidDurationMs = 200; // 目标有效持续时间(毫秒)
    static constexpr int humanizationFactor = 2; // 拟人化因子 
    static constexpr float deadZoneThreshold = 7.f; // 死区阈值


    // 目标类别 classes{ "ct_body", "ct_head", "t_body", "t_head" };
    std::vector<int> targetClasses{ 1, 3 };
    std::mutex targetClassesMutex;
};

class ConfigModule {
public:
    // 初始化配置模块
    static void Initialize();

    // 清理配置模块
    static void Cleanup();

    // 获取配置实例
    static std::shared_ptr<HelperConfig> GetConfig();

    // 启用/禁用参数调试模式
    static void EnableConfigDebug(bool enable);

    // 检查参数调试模式是否启用
    static bool IsConfigDebugEnabled();

    // 处理键盘按键事件
    static void HandleKeyPress(int key);

    // 获取/设置目标类别
    static std::vector<int> GetTargetClasses();
    static void SetTargetClasses(const std::vector<int>& classes);

private:
    static std::unique_ptr<KeyboardListener> keyboardListener;
    static std::shared_ptr<HelperConfig> config;
    static std::atomic<bool> configDebugEnabled;
    static std::thread configDebugThread;

    // 参数调试线程函数
    static void ConfigDebugLoop();
};