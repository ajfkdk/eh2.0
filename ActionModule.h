#ifndef ACTION_MODULE_H
#define ACTION_MODULE_H

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include "MouseController.h"
#include "PredictionModule.h"

// 定义线程安全的共享状态结构体
struct SharedState {
    std::atomic<bool> isAutoAimEnabled{ false };
    std::atomic<bool> isAutoFireEnabled{ false };
    std::atomic<float> targetDistance{ 999.0f };
    std::atomic<bool> hasValidTarget{ false };
    std::mutex mutex;
};

class ActionModule {
private:
    static std::thread actionThread;
    static std::thread fireThread; // 点射控制线程
    static std::atomic<bool> running;
    static std::unique_ptr<MouseController> mouseController;
    static std::shared_ptr<SharedState> sharedState; // 共享状态
    // 丝滑时间
    static std::atomic<int> smoothTimeAlpha;
    // 主处理循环 (自瞄)
    static void ProcessLoop();

    // 点射控制线程函数
    static void FireControlLoop();

    // 归一化移动值到指定范围
    static std::pair<float, float> NormalizeMovement(float x, float y, float maxValue);

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
};

#endif // ACTION_MODULE_H