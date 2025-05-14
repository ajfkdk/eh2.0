#ifndef ACTION_MODULE_H
#define ACTION_MODULE_H

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include "MouseController.h"
#include "PredictionModule.h"

struct Point2D {
    float x;
    float y;
};


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

    // 预测相关变量
    static float predictAlpha;  // 预测系数
    static Point2D lastTargetPos;  // 上一帧目标位置
    static bool hasLastTarget;  // 是否有上一帧目标位置

    // 其他变量
    static constexpr int humanizationFactor = 2; // 拟人化因子
    //死区阈值
    static constexpr float deadZoneThreshold = 7.f; // 死区阈值

    // 主处理循环 (自瞄)
    static void ProcessLoop();

    // 点射控制线程函数
    static void FireControlLoop();

    // 归一化移动值到指定范围
    static std::pair<float, float> NormalizeMovement(float x, float y, float maxValue);

    // 预测下一帧位置
    static Point2D PredictNextPosition(const Point2D& current);

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

    // 设置预测系数
    static void SetPredictAlpha(float alpha);



};

#endif // ACTION_MODULE_H