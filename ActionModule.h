#ifndef ACTION_MODULE_H
#define ACTION_MODULE_H

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include "MouseController.h"
#include "PredictionModule.h"
#include "KeyboardListener.h"

// 定义线程安全的共享状态结构体
struct SharedState {
    std::atomic<bool> isAutoAimEnabled{ false };
    std::atomic<bool> isAutoFireEnabled{ false };
    std::atomic<float> targetDistance{ 999.0f };
    std::atomic<bool> hasValidTarget{ false };
    std::mutex mutex;
};

// PID控制器结构体
struct PIDController {
    // PID参数
    std::atomic<float> kp{ 0.5f };  // 比例系数
    std::atomic<float> ki{ 0.1f }; // 积分系数
    std::atomic<float> kd{ 0.0f };  // 微分系数

    // 积分项限制
    float integralLimit = 10.0f;

    // 上一次的误差
    float previousErrorX = 0.0f;
    float previousErrorY = 0.0f;

    // 积分累计值
    float integralX = 0.0f;
    float integralY = 0.0f;

    // 上一次计算时间点
    std::chrono::steady_clock::time_point lastTime;

    PIDController() {
        Reset();
    }

    void Reset() {
        previousErrorX = 0.0f;
        previousErrorY = 0.0f;
        integralX = 0.0f;
        integralY = 0.0f;
        lastTime = std::chrono::steady_clock::now();
    }
};

class ActionModule {
private:
    static std::thread actionThread;
    static std::thread fireThread; // 点射控制线程
    static std::thread pidDebugThread; // PID调试线程
    static std::atomic<bool> running;
    static std::atomic<bool> pidDebugEnabled;
    static std::unique_ptr<MouseController> mouseController;
    static std::shared_ptr<SharedState> sharedState; // 共享状态
    static PIDController pidController; // PID控制器
    static std::unique_ptr<KeyboardListener> keyboardListener; // 键盘监听器

    // 主处理循环 (自瞄)
    static void ProcessLoop();

    // 点射控制线程函数
    static void FireControlLoop();

    // PID调试线程函数
    static void PIDDebugLoop();

    // 处理键盘按键事件
    static void HandleKeyPress(int key);

    // 归一化移动值到指定范围
    static std::pair<float, float> NormalizeMovement(float x, float y, float maxValue);

    // PID控制算法
    static std::pair<float, float> ApplyPIDControl(float errorX, float errorY);

public:
    // 初始化模块
    static std::thread Initialize();

    // 清理资源
    static void Cleanup();

    // 停止模块
    static void Stop();

    // 判断模块是否运行中
    static bool IsRunning();

    // 设置鼠标控制器
    static void SetMouseController(std::unique_ptr<MouseController> controller);

    // 启用/禁用PID调试
    static void EnablePIDDebug(bool enable);

    // 获取PID调试状态
    static bool IsPIDDebugEnabled();
};

#endif // ACTION_MODULE_H