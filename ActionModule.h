// ActionModule.h
#ifndef ACTION_MODULE_H
#define ACTION_MODULE_H

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <chrono>
#include "MouseController.h"
#include "PredictionModule.h"
#include "KeyboardListener.h"

// 定义2D点结构体
struct Point2D {
    float x;
    float y;
};

// 定义线程安全的共享状态结构体
struct SharedState {
    std::atomic<bool> isAutoAimEnabled{ false };
    std::atomic<bool> isAutoFireEnabled{ false };
    std::atomic<float> targetDistance{ 999.0f };
    std::chrono::steady_clock::time_point targetValidSince{ std::chrono::steady_clock::now() }; // 添加目标最近出现时间点

    // 压枪相关状态
    std::atomic<bool> isRecoilControlEnabled{ true }; // 压枪开关
    std::atomic<float> pressForce{ 10.0f }; // 压枪力度，默认3.0
    std::atomic<int> pressTime{ 2000 }; // 压枪持续时间(ms)，默认1000ms
    std::atomic<bool> needPressDownWhenAim{ true }; // 自瞄时是否需要下压

    std::mutex mutex;
};

// PID控制器结构体
struct PIDController {
    // PID参数
    std::atomic<float> kp{ 0.5f };  // 比例系数
    std::atomic<float> ki{ 0.1f }; // 积分系数
    std::atomic<float> kd{ 0.0f };  // 微分系数
    std::atomic<float> yControlFactor{ 1.0f }; // Y轴控制力度系数，用于爆头率控制

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

// 压枪状态结构体
struct RecoilControlState {
    std::atomic<bool> isLeftButtonPressed{ false }; // 鼠标左键是否按下
    std::chrono::steady_clock::time_point pressStartTime; // 按下开始时间
    std::chrono::steady_clock::time_point lastRecoilTime; // 上次压枪时间
};

class ActionModule {
private:
    static std::thread actionThread;
    static std::thread fireThread; // 点射控制线程
    static std::thread recoilThread; // 压枪控制线程
    static std::thread pidDebugThread; // PID调试线程
    static std::atomic<bool> running;
    static std::atomic<bool> pidDebugEnabled;
    static std::atomic<bool> usePrediction; // 是否使用预测功能
    static std::unique_ptr<MouseController> mouseController;
    static std::shared_ptr<SharedState> sharedState; // 共享状态
    static PIDController pidController; // PID控制器
    static std::unique_ptr<KeyboardListener> keyboardListener; // 键盘监听器
    static RecoilControlState recoilState; // 压枪状态

    // 预测相关变量
    static float predictAlpha;  // 预测系数
    static Point2D lastTargetPos;  // 上一帧目标位置
    static bool hasLastTarget;  // 是否有上一帧目标位置

    // 其他变量
    static constexpr int humanizationFactor = 2; // 拟人化因子
    static constexpr float deadZoneThreshold = 7.f; // 死区阈值
    static int aimFov; // 瞄准视场角度
    static constexpr int targetValidDurationMs = 200; // 目标有效持续时间(毫秒)

    // 主处理循环 (自瞄)
    static void ProcessLoop();

    // 点射控制线程函数
    static void FireControlLoop();

    // PID调试线程函数
    static void PIDDebugLoop();

    // 压枪控制线程函数
    static void RecoilControlLoop();

    // 处理键盘按键事件
    static void HandleKeyPress(int key);

    // 归一化移动值到指定范围
    static std::pair<float, float> NormalizeMovement(float x, float y, float maxValue);

    // PID控制算法
    static std::pair<float, float> ApplyPIDControl(float errorX, float errorY);

    // 预测下一帧位置
    static Point2D PredictNextPosition(const Point2D& current);

    // 应用压枪效果
    static float ApplyRecoilControl();

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

    // 设置使用预测或PID
    static void SetUsePrediction(bool enable);

    // 获取当前使用模式
    static bool IsUsingPrediction();

    // 设置预测系数
    static void SetPredictAlpha(float alpha);
};

#endif // ACTION_MODULE_H