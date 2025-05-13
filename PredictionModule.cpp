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
    // 在PredictionModule命名空间添加全局变量
    std::atomic<int> g_currentTargetClassId{ -1 };  // 当前跟踪的目标类别ID
    std::atomic<int> g_currentTargetId{ -1 };       // 当前跟踪的目标ID (使用在检测中的索引)
    std::chrono::steady_clock::time_point g_lastTargetSwitchTime;  // 上次切换目标的时间
    std::atomic<int> g_targetSwitchCooldownMs{ 500 };  // 目标切换冷却时间(毫秒)
    // 预测时间参数 - 为了兼容性保留，但不再使用
    std::atomic<float> g_gFactor{ 0.2f };
    std::atomic<float> g_hFactor{ 0.16f };
    std::atomic<float> g_predictionTime{ 0.0f };

    // 环形缓冲区，用于存储预测结果
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // 查找离屏幕中心最近的目标
    DetectionResult FindNearestTarget(const std::vector<DetectionResult>& targets) {
        if (targets.empty()) {
            // 无目标时重置当前目标
            g_currentTargetId = -1;
            g_currentTargetClassId = -1;

            DetectionResult emptyResult;
            emptyResult.classId = -1;
            emptyResult.x = 0;
            emptyResult.y = 0;
            return emptyResult;
        }

        // 屏幕中心坐标
        const int centerX = 160;
        const int centerY = 160;

        // 当前时间
        auto currentTime = std::chrono::steady_clock::now();

        // 尝试在新目标中找到当前正在跟踪的目标
        int currentTargetIdx = -1;
        if (g_currentTargetId >= 0 && g_currentTargetClassId >= 0) {
            for (int i = 0; i < targets.size(); i++) {
                // 这里可以添加更复杂的目标匹配逻辑，比如使用目标的位置和大小
                if (targets[i].classId == g_currentTargetClassId) {
                    // 简单匹配：找到相同类别的目标
                    currentTargetIdx = i;
                    break;
                }
            }
        }

        // 如果找到当前目标，且未超过冷却时间，继续跟踪当前目标
        if (currentTargetIdx >= 0 &&
            std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - g_lastTargetSwitchTime).count() < g_targetSwitchCooldownMs) {
            return targets[currentTargetIdx];
        }

        // 否则，找最近的新目标
        DetectionResult nearest = targets[0];
        float minDistance = std::numeric_limits<float>::max();
        int nearestIdx = -1;

        for (int i = 0; i < targets.size(); i++) {
            const auto& target = targets[i];
            // 只考虑有效的目标（classId = 1 或 3) 锁头
            if (target.classId >= 0 && (target.classId == 1 || target.classId == 3)) {
                float dx = target.x - centerX;
                float dy = target.y - centerY;
                float distance = std::sqrt(dx * dx + dy * dy);

                if (distance < minDistance) {
                    minDistance = distance;
                    nearest = target;
                    nearestIdx = i;
                }
            }
        }

        // 如果找到了新目标，更新当前目标信息和切换时间
        if (nearestIdx >= 0 && nearest.classId >= 0) {
            if (nearestIdx != currentTargetIdx) {
                g_currentTargetId = nearestIdx;
                g_currentTargetClassId = nearest.classId;
                g_lastTargetSwitchTime = currentTime;
            }
        }
        else {
            // 没有找到有效目标
            nearest.classId = -1;
            g_currentTargetId = -1;
            g_currentTargetClassId = -1;
        }

        return nearest;
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
                if (nearestTarget.classId >= 0) {
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