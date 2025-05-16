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
// SharedState 结构体
struct SharedState {
    // 自瞄状态
    std::atomic<bool> isAutoAimEnabled{ false }; // 自动瞄准开关
    std::atomic<bool> isAutoFireEnabled{ false }; // 自动开火开关
    std::atomic<float> targetDistance{ 999.0f }; // 目标距离
    std::chrono::steady_clock::time_point targetValidSince; // 目标最近出现时间
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

class ActionModule {
private:
    static std::thread actionThread;
    static std::thread fireThread; // 点射控制线程
    static std::thread pidDebugThread; // PID调试线程
    static std::atomic<bool> running;
    static std::atomic<bool> pidDebugEnabled;
    static std::atomic<bool> usePrediction; // 是否使用预测功能
    static std::unique_ptr<MouseController> mouseController;
    static std::shared_ptr<SharedState> sharedState; // 共享状态
    static PIDController pidController; // PID控制器
    static std::unique_ptr<KeyboardListener> keyboardListener; // 键盘监听器

    // 预测相关变量
    static Point2D lastTargetPos;  // 上一帧目标位置
    static bool hasLastTarget;  // 是否有上一帧目标位置



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

    // 预测下一帧位置
    static Point2D PredictNextPosition(const Point2D& current);

    // 计算压枪移动量
    static float CalculateRecoilCompensation();

    // 重置压枪状态
    static void ResetRecoilState();

    // 更新压枪状态
    static void UpdateRecoilState(bool isLeftButtonDown);

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

   
};

#endif // ACTION_MODULE_H