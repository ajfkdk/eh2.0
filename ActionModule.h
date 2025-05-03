#ifndef ACTION_MODULE_H
#define ACTION_MODULE_H

#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <vector>
#include <string>
#include <memory>
#include "MouseController.h"
#include "PredictionModule.h"

class ActionModule {
private:
    static std::thread actionThread;
    static std::atomic<bool> running;
    static std::unique_ptr<MouseController> mouseController;
    static std::atomic<int> humanizationFactor; // 1-100的拟人参数
    static std::atomic<bool> fireEnabled;
    static std::mutex logMutex;

    // 当前和目标位置
    static int currentX, currentY;
    static int targetX, targetY;
    static std::chrono::high_resolution_clock::time_point lastUpdateTime;

    // 鼠标移动记录日志
    static void LogMouseMovement(const std::string& message);

    // 处理丢失目标的拟人动作
    static void PerformHumanLikeMovementOnTargetLoss();

    // 生成拟人化路径点
    static std::vector<std::pair<float, float>> GenerateHumanLikePath(
        float startX, float startY, float endX, float endY, int humanFactor);

    // 计算速度曲线
    static float CalculateSpeedFactor(float progress, int humanFactor);

    // 添加微小抖动
    static std::pair<float, float> AddJitter(float x, float y, float distance, int humanFactor);

    // 主处理循环
    static void ProcessLoop();

    // 获取当前鼠标位置
    static void UpdateCurrentMousePosition();

    // 控制鼠标左键开火
    static void ControlMouseFire(bool targetVisible);

public:
    // 初始化模块
    static std::thread Initialize(int humanFactor = 50);

    // 清理资源
    static void Cleanup();

    // 停止模块
    static void Stop();

    // 判断模块是否运行中
    static bool IsRunning();

    // 设置拟人参数 (1-100)
    static void SetHumanizationFactor(int factor);

    // 获取拟人参数
    static int GetHumanizationFactor();

    // 设置鼠标控制器
    static void SetMouseController(std::unique_ptr<MouseController> controller);

    // 启用/禁用自动开火
    static void EnableAutoFire(bool enable);

    // 判断自动开火是否启用
    static bool IsAutoFireEnabled();
};

#endif // ACTION_MODULE_H#pragma once
