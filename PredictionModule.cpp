// PredictionModule.cpp
#include "PredictionModule.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <opencv2/opencv.hpp>

namespace PredictionModule {
    // 全局变量
    std::atomic<bool> g_running{ false };
    std::atomic<bool> g_debugMode{ false };

    // 滑动窗口参数
    std::atomic<int> g_windowSize{ 5 };  // 默认保存5帧历史数据
    std::atomic<float> g_distanceThreshold{ 30.0f };  // 默认距离阈值为30像素
    std::atomic<int> g_lostFrameThreshold{ 5 };  // 默认5帧未检测到视为丢失

    // 预测时间参数 - 为了兼容性保留，但不再使用
    std::atomic<float> g_gFactor{ 0.2f };
    std::atomic<float> g_hFactor{ 0.16f };
    std::atomic<float> g_predictionTime{ 0.0f };

    // 环形缓冲区，用于存储预测结果
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // 目标历史记录和状态
    struct {
        std::deque<std::pair<int, int>> positionHistory;  // 历史位置窗口
        int lastValidX = -1;  // 上一个有效的X坐标
        int lastValidY = -1;  // 上一个有效的Y坐标
        int lostFrameCount = 0;  // 目标丢失帧计数
        int targetClassId = -1;  // 当前追踪的目标类别ID
    } g_targetState;

    // 计算两点间距离
    float CalculateDistance(int x1, int y1, int x2, int y2) {
        float dx = static_cast<float>(x1 - x2);
        float dy = static_cast<float>(y1 - y2);
        return std::sqrt(dx * dx + dy * dy);
    }

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
            // 只考虑有效的目标（classId =1 , 3) 锁头
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

    // 查找与上一目标最接近的目标（距离阈值锁定）
    DetectionResult FindClosestToLastTarget(const std::vector<DetectionResult>& targets) {
        // 如果没有历史记录，或者目标已长时间丢失，使用最近目标
        if (g_targetState.lastValidX < 0 || g_targetState.lastValidY < 0 ||
            g_targetState.lostFrameCount > g_lostFrameThreshold.load()) {
            return FindNearestTarget(targets);
        }

        // 查找距离上一个目标最近的目标
        DetectionResult closest;
        closest.classId = -1;
        float minDistance = std::numeric_limits<float>::max();

        for (const auto& target : targets) {
            // 只考虑有效的目标（classId = 1或3）
            if (target.classId >= 0 && (target.classId == 1 || target.classId == 3)) {
                float distance = CalculateDistance(
                    g_targetState.lastValidX, g_targetState.lastValidY,
                    target.x, target.y
                );

                // 如果距离小于阈值且是最近的
                if (distance < g_distanceThreshold.load() && distance < minDistance) {
                    minDistance = distance;
                    closest = target;
                }
            }
        }

        // 如果找到符合条件的目标，返回该目标
        if (closest.classId >= 0) {
            return closest;
        }

        // 否则，返回距离中心最近的目标（重新锁定）
        return FindNearestTarget(targets);
    }

    // 应用滑动窗口加权平均去抖动
    std::pair<int, int> ApplyWindowAveraging(int currentX, int currentY) {
        // 将当前位置添加到历史记录
        g_targetState.positionHistory.push_back(std::make_pair(currentX, currentY));

        // 保持窗口大小不超过设定值
        while (g_targetState.positionHistory.size() > static_cast<size_t>(g_windowSize.load())) {
            g_targetState.positionHistory.pop_front();
        }

        // 如果历史记录为空，直接返回当前位置
        if (g_targetState.positionHistory.empty()) {
            return std::make_pair(currentX, currentY);
        }

        // 计算加权平均 - 最近的帧权重更大
        float sumX = 0, sumY = 0;
        float totalWeight = 0;

        const int historySize = static_cast<int>(g_targetState.positionHistory.size());
        for (int i = 0; i < historySize; i++) {
            // 计算权重 - 线性增长，最新的权重最大
            float weight = static_cast<float>(i + 1);

            sumX += g_targetState.positionHistory[i].first * weight;
            sumY += g_targetState.positionHistory[i].second * weight;
            totalWeight += weight;
        }

        // 计算加权平均坐标
        int avgX = static_cast<int>(sumX / totalWeight);
        int avgY = static_cast<int>(sumY / totalWeight);

        return std::make_pair(avgX, avgY);
    }

    // 预测模块工作函数
    void PredictionModuleWorker() {
        std::cout << "Prediction module worker started" << std::endl;

        // 初始化目标状态
        g_targetState.lastValidX = -1;
        g_targetState.lastValidY = -1;
        g_targetState.lostFrameCount = 0;
        g_targetState.targetClassId = -1;
        g_targetState.positionHistory.clear();

        while (g_running) {
            try {
                // 获取所有检测目标
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                // 创建预测结果
                PredictionResult prediction;
                prediction.timestamp = std::chrono::high_resolution_clock::now();

                // 查找当前应跟踪的目标
                DetectionResult currentTarget = FindClosestToLastTarget(targets);

                // 如果有有效目标
                if (currentTarget.classId >= 0) {
                    // 目标有效，更新最后有效位置并重置丢失计数
                    g_targetState.lastValidX = currentTarget.x;
                    g_targetState.lastValidY = currentTarget.y;
                    g_targetState.lostFrameCount = 0;
                    g_targetState.targetClassId = currentTarget.classId;

                    // 应用滑动窗口加权平均去抖动
                    auto smoothedPosition = ApplyWindowAveraging(currentTarget.x, currentTarget.y);

                    // 使用平滑后的位置作为预测结果
                    prediction.x = smoothedPosition.first;
                    prediction.y = smoothedPosition.second;
                }
                else {
                    // 目标丢失，增加丢失计数
                    g_targetState.lostFrameCount++;

                    // 如果丢失时间不长，使用历史数据提供预测
                    if (g_targetState.lostFrameCount <= g_lostFrameThreshold.load() &&
                        g_targetState.lastValidX >= 0 && g_targetState.lastValidY >= 0) {
                        // 使用最后一次平滑结果作为预测
                        if (!g_targetState.positionHistory.empty()) {
                            prediction.x = g_targetState.positionHistory.back().first;
                            prediction.y = g_targetState.positionHistory.back().second;
                        }
                        else {
                            prediction.x = g_targetState.lastValidX;
                            prediction.y = g_targetState.lastValidY;
                        }
                    }
                    else {
                        // 目标完全丢失，设置为无效目标
                        prediction.x = 999;
                        prediction.y = 999;

                        // 重置目标状态，准备重新锁定
                        g_targetState.lastValidX = -1;
                        g_targetState.lastValidY = -1;
                        g_targetState.targetClassId = -1;
                        g_targetState.positionHistory.clear();
                    }
                }

                // 将预测结果写入环形缓冲区
                g_predictionBuffer.write(prediction);

                // debug模式处理 - 向检测模块提供预测点信息
                if (g_debugMode.load() && (prediction.x != 999 && prediction.y != 999)) {
                    // 调用检测模块的绘制预测点函数
                    DetectionModule::DrawPredictionPoint(prediction.x, prediction.y);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error in prediction module: " << e.what() << std::endl;
            }

            // 短暂休眠，避免CPU占用过高
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
        g_targetState.positionHistory.clear();
        std::cout << "Prediction module cleanup completed" << std::endl;
    }

    bool GetLatestPrediction(PredictionResult& result) {
        return g_predictionBuffer.readLatest(result);
    }

    // 设置滑动窗口大小
    void SetWindowSize(int size) {
        if (size > 0) {
            g_windowSize = size;
        }
    }

    // 获取滑动窗口大小
    int GetWindowSize() {
        return g_windowSize.load();
    }

    // 设置距离阈值
    void SetDistanceThreshold(float threshold) {
        if (threshold > 0) {
            g_distanceThreshold = threshold;
        }
    }

    // 获取距离阈值
    float GetDistanceThreshold() {
        return g_distanceThreshold.load();
    }

    // 设置目标丢失容忍帧数
    void SetLostFrameThreshold(int frames) {
        if (frames >= 0) {
            g_lostFrameThreshold = frames;
        }
    }

    // 获取目标丢失容忍帧数
    int GetLostFrameThreshold() {
        return g_lostFrameThreshold.load();
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