// PredictionModule.cpp
#include "PredictionModule.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <opencv2/opencv.hpp>

namespace PredictionModule {
    // 全局变量
    std::atomic<bool> g_running{ false };
    std::atomic<bool> g_debugMode{ false };

    // 目标跟踪变量
    std::atomic<int> g_currentTargetId{ -1 };  // 当前跟踪的目标ID
    std::atomic<bool> g_isTargetLocked{ false }; // 是否锁定了目标
    std::atomic<int> g_lastTargetX{ 0 }; // 上次锁定目标的X坐标
    std::atomic<int> g_lastTargetY{ 0 }; // 上次锁定目标的Y坐标
    std::chrono::steady_clock::time_point g_targetLostTime; // 目标丢失时间
    std::atomic<int> g_validateTargetDistance{ 30 }; // 目标验证距离(像素)
    std::atomic<int> g_targetLostTimeoutMs{ 300 }; // 目标丢失超时(毫秒)
    std::atomic<int> g_lastMouseMoveX{ 0 }; // 上次鼠标X方向移动
    std::atomic<int> g_lastMouseMoveY{ 0 }; // 上次鼠标Y方向移动

    // 预测时间参数 - 为了兼容性保留，但不再使用
    std::atomic<float> g_gFactor{ 0.2f };
    std::atomic<float> g_hFactor{ 0.16f };
    std::atomic<float> g_predictionTime{ 0.0f };

    // 环形缓冲区，用于存储预测结果
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // 记录鼠标移动
    void RecordMouseMove(int dx, int dy) {
        g_lastMouseMoveX += dx;
        g_lastMouseMoveY += dy;
    }

    // 查找离屏幕中心最近的目标
    DetectionResult FindNearestTarget(const std::vector<DetectionResult>& targets) {
        // 屏幕中心坐标
        const int centerX = 160;
        const int centerY = 160;

        // 当前时间
        auto currentTime = std::chrono::steady_clock::now();

        // 如果没有目标，重置锁定状态
        if (targets.empty()) {
            // 如果之前有锁定目标，记录丢失时间
            if (g_isTargetLocked) {
                g_targetLostTime = currentTime;
            }
            g_isTargetLocked = false;
            g_currentTargetId = -1;

            DetectionResult emptyResult;
            emptyResult.classId = -1;
            emptyResult.x = 999;
            emptyResult.y = 999;
            return emptyResult;
        }

        // 补偿鼠标移动导致的目标位置变化
        int compensatedLastX = g_lastTargetX - static_cast<int>(g_lastMouseMoveX * 0.5);
        int compensatedLastY = g_lastTargetY - static_cast<int>(g_lastMouseMoveY * 0.5);

        // 重置鼠标移动补偿
        g_lastMouseMoveX = 0;
        g_lastMouseMoveY = 0;

        // 如果当前有锁定目标
        if (g_isTargetLocked) {
            // 在锁定目标周围寻找匹配目标
            std::vector<DetectionResult> candidatesInRange;

            for (const auto& target : targets) {
                // 只考虑有效的目标（classId = 1 或 3)
                if (target.classId >= 0 && (target.classId == 1 || target.classId == 3)) {
                    // 计算与上次位置的距离
                    float dx = target.x - compensatedLastX;
                    float dy = target.y - compensatedLastY;
                    float distance = std::sqrt(dx * dx + dy * dy);

                    // 如果在有效范围内，加入候选
                    if (distance <= g_validateTargetDistance) {
                        candidatesInRange.push_back(target);
                    }
                }
            }

            // 如果在范围内找到了候选目标
            if (!candidatesInRange.empty()) {
                // 更新目标存在时间
                g_targetLostTime = currentTime;

                // 如果只有一个候选，直接使用
                if (candidatesInRange.size() == 1) {
                    auto& target = candidatesInRange[0];

                    // 更新上次位置
                    g_lastTargetX = target.x;
                    g_lastTargetY = target.y;

                    return target;
                }
                // 如果有多个候选，选择离中心最近的，但暂不输出有效坐标
                else {
                    DetectionResult nearest = candidatesInRange[0];
                    float minDistance = std::numeric_limits<float>::max();

                    for (const auto& target : candidatesInRange) {
                        float dx = target.x - centerX;
                        float dy = target.y - centerY;
                        float distance = std::sqrt(dx * dx + dy * dy);

                        if (distance < minDistance) {
                            minDistance = distance;
                            nearest = target;
                        }
                    }

                    // 更新上次位置
                    g_lastTargetX = nearest.x;
                    g_lastTargetY = nearest.y;

                    // 虽然找到了目标，但因为多个候选可能导致抖动，返回无效坐标
                    DetectionResult result = nearest;
                    result.x = 999;
                    result.y = 999;
                    return result;
                }
            }
            // 如果在范围内没有找到候选目标
            else {
                // 检查是否超过目标丢失超时
                auto lostDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    currentTime - g_targetLostTime).count();

                // 如果超过超时时间，解除锁定
                if (lostDuration > g_targetLostTimeoutMs) {
                    g_isTargetLocked = false;
                    g_currentTargetId = -1;
                }

                // 返回无效目标
                DetectionResult emptyResult;
                emptyResult.classId = -1;
                emptyResult.x = 999;
                emptyResult.y = 999;
                return emptyResult;
            }
        }

        // 如果当前没有锁定目标，寻找新目标
        DetectionResult nearest;
        nearest.classId = -1;
        float minDistance = std::numeric_limits<float>::max();

        for (const auto& target : targets) {
            // 只考虑有效的目标（classId = 1 或 3)
            if (target.classId >= 0 && (target.classId == 1 || target.classId == 3)) {
                float dx = target.x - centerX;
                float dy = target.y - centerY;
                float distance = std::sqrt(dx * dx + dy * dy);

                if (distance < minDistance) {
                    minDistance = distance;
                    nearest = target;
                }
            }
        }

        // 如果找到了新目标，锁定它
        if (nearest.classId >= 0) {
            g_isTargetLocked = true;
            g_lastTargetX = nearest.x;
            g_lastTargetY = nearest.y;
            g_targetLostTime = currentTime;
            return nearest;
        }

        // 没有找到有效目标
        DetectionResult emptyResult;
        emptyResult.classId = -1;
        emptyResult.x = 999;
        emptyResult.y = 999;
        return emptyResult;
    }

    // 预测模块工作函数
    void PredictionModuleWorker() {
        std::cout << "Prediction module worker started" << std::endl;

        // 标记上一次是否有目标
        bool hadValidTarget = false;

        while (g_running) {
            try {
                // 获取所有检测目标
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                // 创建预测结果
                PredictionResult prediction;
                prediction.timestamp = std::chrono::high_resolution_clock::now();

                // 查找离中心最近的目标
                DetectionResult nearestTarget = FindNearestTarget(targets);

                // 如果有有效目标
                if (nearestTarget.classId >= 0 && nearestTarget.x != 999) {
                    // 直接使用最近目标的坐标
                    prediction.x = nearestTarget.x;
                    prediction.y = nearestTarget.y;
                    hadValidTarget = true;  // 记录有效目标
                }
                else {
                    // 无效目标，设置为目标丢失
                    prediction.x = 999;
                    prediction.y = 999;
                    hadValidTarget = false;  // 记录目标丢失
                }

                // 将预测结果写入环形缓冲区
                g_predictionBuffer.write(prediction);

                // debug模式处理 - 向检测模块提供预测点信息
                if (g_debugMode.load() && hadValidTarget) {
                    // 调用检测模块的绘制预测点函数
                    DetectionModule::DrawPredictionPoint(prediction.x, prediction.y);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error in prediction module: " << e.what() << std::endl;
            }
        }

        std::cout << "Prediction module worker stopped" << std::endl;
    }

    // 公共接口实现
    std::thread Initialize() {
        if (g_running) {
            std::cerr << "Prediction module already running" << std::endl;
            return std::thread();
        }

        std::cout << "Initializing prediction module" << std::endl;

        g_running = true;
        g_predictionBuffer.clear();

        return std::thread(PredictionModuleWorker);
    }

    bool IsRunning() {
        return g_running;
    }

    void Stop() {
        if (!g_running) {
            return;
        }

        std::cout << "Stopping prediction module" << std::endl;
        g_running = false;
    }

    void Cleanup() {
        g_predictionBuffer.clear();
        std::cout << "Prediction module cleanup completed" << std::endl;
    }

    bool GetLatestPrediction(PredictionResult& result) {
        return g_predictionBuffer.readLatest(result);
    }

    // 设置调试模式
    void SetDebugMode(bool enabled) {
        g_debugMode = enabled;
    }

    // 获取调试模式状态
    bool IsDebugModeEnabled() {
        return g_debugMode.load();
    }

    // 记录鼠标移动的接口
    void NotifyMouseMovement(int dx, int dy) {
        RecordMouseMove(dx, dy);
    }

    // 以下函数保留为接口兼容，但实际上不再使用
    void SetGFactor(float g) {
        g_gFactor = g;
    }

    void SetHFactor(float h) {
        g_hFactor = h;
    }

    void SetPredictionTime(float seconds) {
        g_predictionTime = seconds;
    }
}