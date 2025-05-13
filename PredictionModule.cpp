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

    // 预测时间参数 - 为了兼容性保留，但不再使用
    std::atomic<float> g_gFactor{ 0.2f };
    std::atomic<float> g_hFactor{ 0.16f };
    std::atomic<float> g_predictionTime{ 0.0f };

    // 新增：平滑和稳定性控制参数
    std::atomic<float> g_positionThreshold{ 50.0f }; // 位置变化阈值（像素）
    std::atomic<int> g_stabilityTimeThreshold{ 150 }; // 稳定性时间阈值（毫秒）
    std::atomic<float> g_smoothingFactor{ 0.7f }; // 平滑因子 (0-1)，越大越平滑

    // 环形缓冲区，用于存储预测结果
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // 新增：上次有效的目标位置和时间
    struct {
        int x = 0;
        int y = 0;
        bool valid = false;
        std::chrono::high_resolution_clock::time_point lastUpdateTime;
        std::chrono::high_resolution_clock::time_point positionChangeTime;
        bool positionChanging = false;
    } g_lastTarget;

    // 查找离屏幕中心最近的目标
    DetectionResult FindNearestTarget(const std::vector<DetectionResult>& targets) {
        if (targets.empty()) {
            DetectionResult emptyResult;
            emptyResult.classId = -1;
            emptyResult.x = 0;
            emptyResult.y = 0;
            return emptyResult;
        }

        // 屏幕中心坐标 (基于320x320的图像)
        const int centerX = 160;
        const int centerY = 160;

        // 查找最近的目标
        DetectionResult nearest = targets[0];
        float minDistance = std::numeric_limits<float>::max();

        for (const auto& target : targets) {
            // 只考虑有效的目标（classId = 1 或 3）锁头
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

        // 如果没有找到有效目标，返回无效目标
        if (minDistance == std::numeric_limits<float>::max()) {
            nearest.classId = -1;
        }

        return nearest;
    }

    // 新增：计算两点之间的距离
    float CalculateDistance(int x1, int y1, int x2, int y2) {
        float dx = static_cast<float>(x1 - x2);
        float dy = static_cast<float>(y1 - y2);
        return std::sqrt(dx * dx + dy * dy);
    }

    // 新增：平滑移动函数
    void SmoothPosition(int& currentX, int& currentY, int targetX, int targetY) {
        float smoothingFactor = g_smoothingFactor.load();
        currentX = static_cast<int>(currentX * smoothingFactor + targetX * (1.0f - smoothingFactor));
        currentY = static_cast<int>(currentY * smoothingFactor + targetY * (1.0f - smoothingFactor));
    }

    // 预测模块工作函数
    void PredictionModuleWorker() {
        std::cout << "Prediction module worker started" << std::endl;

        // 标记上一次是否有目标
        bool hadValidTarget = false;

        // 初始化最后有效目标时间
        g_lastTarget.lastUpdateTime = std::chrono::high_resolution_clock::now();

        while (g_running) {
            try {
                // 获取所有检测目标
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                // 创建预测结果
                PredictionResult prediction;
                prediction.timestamp = std::chrono::high_resolution_clock::now();

                // 查找离中心最近的目标
                DetectionResult nearestTarget = FindNearestTarget(targets);

                // 当前时间
                auto currentTime = std::chrono::high_resolution_clock::now();

                // 如果有有效目标
                if (nearestTarget.classId >= 0) {
                    // 如果之前有过有效目标
                    if (g_lastTarget.valid) {
                        // 计算位置变化
                        float distance = CalculateDistance(
                            g_lastTarget.x, g_lastTarget.y,
                            nearestTarget.x, nearestTarget.y);

                        // 检查位置变化是否超过阈值
                        if (distance > g_positionThreshold.load()) {
                            // 如果位置正在变化，更新位置变化时间
                            if (!g_lastTarget.positionChanging) {
                                g_lastTarget.positionChanging = true;
                                g_lastTarget.positionChangeTime = currentTime;
                            }

                            // 计算经过的时间
                            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                currentTime - g_lastTarget.positionChangeTime).count();

                            // 如果未超过稳定性时间阈值，保持旧位置
                            if (elapsedMs < g_stabilityTimeThreshold.load()) {
                                // 保持旧位置，并平滑过渡
                                SmoothPosition(g_lastTarget.x, g_lastTarget.y,
                                    nearestTarget.x, nearestTarget.y);

                                prediction.x = g_lastTarget.x;
                                prediction.y = g_lastTarget.y;
                            }
                            else {
                                // 已经稳定足够长时间，接受新位置
                                g_lastTarget.positionChanging = false;
                                g_lastTarget.x = nearestTarget.x;
                                g_lastTarget.y = nearestTarget.y;
                                prediction.x = nearestTarget.x;
                                prediction.y = nearestTarget.y;
                            }
                        }
                        else {
                            // 位置变化在阈值内，正常更新
                            g_lastTarget.positionChanging = false;
                            // 应用平滑
                            SmoothPosition(g_lastTarget.x, g_lastTarget.y,
                                nearestTarget.x, nearestTarget.y);
                            prediction.x = g_lastTarget.x;
                            prediction.y = g_lastTarget.y;
                        }
                    }
                    else {
                        // 第一次获取有效目标
                        g_lastTarget.x = nearestTarget.x;
                        g_lastTarget.y = nearestTarget.y;
                        g_lastTarget.valid = true;
                        g_lastTarget.positionChanging = false;
                        prediction.x = nearestTarget.x;
                        prediction.y = nearestTarget.y;
                    }

                    g_lastTarget.lastUpdateTime = currentTime;
                    hadValidTarget = true;
                }
                else {
                    // 无检测到有效目标
                    auto elapsedSinceLastValidMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        currentTime - g_lastTarget.lastUpdateTime).count();

                    // 判断目标丢失时间是否超过阈值（例如500ms）
                    if (g_lastTarget.valid && elapsedSinceLastValidMs < 500) {
                        // 短时间内目标丢失，保持上一个有效位置
                        prediction.x = g_lastTarget.x;
                        prediction.y = g_lastTarget.y;
                    }
                    else {
                        // 长时间无目标，标记为目标丢失
                        prediction.x = 999;
                        prediction.y = 999;
                        g_lastTarget.valid = false;
                    }

                    hadValidTarget = false;
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

            // 适当休眠，减少CPU占用
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
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

    // 新增：设置位置变化阈值
    void SetPositionThreshold(float threshold) {
        g_positionThreshold = threshold;
    }

    // 新增：设置稳定性时间阈值
    void SetStabilityTimeThreshold(int milliseconds) {
        g_stabilityTimeThreshold = milliseconds;
    }

    // 新增：设置平滑因子
    void SetSmoothingFactor(float factor) {
        // 限制在0-1范围内
        factor = std::clamp(factor, 0.0f, 1.0f);
        g_smoothingFactor = factor;
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