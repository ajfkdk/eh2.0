// PredictionModule.cpp
#include "PredictionModule.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace PredictionModule {
    // 全局变量
    std::atomic<bool> g_running{ false };

    // g-h滤波器参数
    std::atomic<float> g_gFactor{ 0.2f };    // 位置增益
    std::atomic<float> g_hFactor{ 0.16f };   // 速度增益
    std::atomic<float> g_predictionTime{ 0.2f }; // 预测时间 (秒)

    // 环形缓冲区，用于存储预测结果
    constexpr size_t BUFFER_SIZE = 100;  // 设置适当的缓冲区大小
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // g-h滤波器状态
    struct FilterState {
        float xEst;    // 估计的X位置
        float yEst;    // 估计的Y位置
        float vxEst;   // 估计的X速度
        float vyEst;   // 估计的Y速度
        std::chrono::high_resolution_clock::time_point lastUpdateTime;  // 上次更新时间
        bool initialized;  // 是否已初始化

        FilterState() : xEst(0), yEst(0), vxEst(0), vyEst(0), initialized(false) {}
    };

    FilterState g_filterState;

    // 上次获取到有效目标的时间
    std::chrono::high_resolution_clock::time_point g_lastValidTargetTime;

    // 重置滤波器状态
    void ResetFilterState(const DetectionResult& target) {
        g_filterState.xEst = target.x;
        g_filterState.yEst = target.y;
        g_filterState.vxEst = 0;
        g_filterState.vyEst = 0;
        g_filterState.lastUpdateTime = std::chrono::high_resolution_clock::now();
        g_filterState.initialized = true;

        g_lastValidTargetTime = std::chrono::high_resolution_clock::now();

        std::cout << "Filter state reset with target at (" << target.x << "," << target.y << ")" << std::endl;
    }

    // 更新g-h滤波器状态
    void UpdateFilter(const DetectionResult& target) {
        auto currentTime = std::chrono::high_resolution_clock::now();

        // 如果滤波器未初始化，则初始化它
        if (!g_filterState.initialized) {
            ResetFilterState(target);
            return;
        }

        // 计算时间增量(秒)
        float deltaT = std::chrono::duration<float>(currentTime - g_filterState.lastUpdateTime).count();
        if (deltaT <= 0) return; // 防止除以零

        // 1. 预测步骤 - 根据上一次的估计和速度预测当前位置
        float xPred = g_filterState.xEst + g_filterState.vxEst * deltaT;
        float yPred = g_filterState.yEst + g_filterState.vyEst * deltaT;

        // 2. 更新步骤 - 使用实际测量值和预测值之间的残差更新估计
        // 计算残差（实际位置与预测位置的差）
        float residualX = target.x - xPred;
        float residualY = target.y - yPred;

        // 更新位置估计: 预测位置 + g * 残差
        float g = g_gFactor.load();
        float h = g_hFactor.load();

        float xEst = xPred + g * residualX;
        float yEst = yPred + g * residualY;

        // 更新速度估计: 上次速度估计 + h * 残差 / 时间间隔
        float vxEst = g_filterState.vxEst + h * (residualX / deltaT);
        float vyEst = g_filterState.vyEst + h * (residualY / deltaT);

        // 更新滤波器状态
        g_filterState.xEst = xEst;
        g_filterState.yEst = yEst;
        g_filterState.vxEst = vxEst;
        g_filterState.vyEst = vyEst;
        g_filterState.lastUpdateTime = currentTime;

        g_lastValidTargetTime = currentTime;
    }

    // 预测未来位置
    PredictionResult PredictFuturePosition() {
        PredictionResult result;
        result.timestamp = std::chrono::high_resolution_clock::now();

        // 检查目标是否丢失 (超过预测时间没有更新)
        auto timeSinceLastValidTarget = std::chrono::duration<float>(
            result.timestamp - g_lastValidTargetTime).count();

        if (!g_filterState.initialized || timeSinceLastValidTarget > g_predictionTime.load()) {
            // 目标丢失，输出特殊值
            result.x = 999;
            result.y = 999;
            return result;
        }

        // 计算预测时间点的位置
        float predictionTimeSeconds = g_predictionTime.load();
        result.x = static_cast<int>(g_filterState.xEst + g_filterState.vxEst * predictionTimeSeconds);
        result.y = static_cast<int>(g_filterState.yEst + g_filterState.vyEst * predictionTimeSeconds);

        return result;
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
            float dx = target.x - centerX;
            float dy = target.y - centerY;
            float distance = std::sqrt(dx * dx + dy * dy);

            if (distance < minDistance) {
                minDistance = distance;
                nearest = target;
            }
        }

        return nearest;
    }

    // 预测模块工作函数
    void PredictionModuleWorker() {
        std::cout << "Prediction module worker started" << std::endl;

        // 计算循环间隔时间 (目标60FPS+)
        const auto loopInterval = std::chrono::microseconds(16000); // 约为16.7ms，确保>60FPS

        while (g_running) {
            auto loopStart = std::chrono::high_resolution_clock::now();

            try {
                // 获取所有检测目标
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                // 检查是否有有效目标
                if (!targets.empty()) {
                    // 查找离中心最近的目标
                    DetectionResult nearestTarget = FindNearestTarget(targets);

                    // 如果有有效目标
                    if (nearestTarget.classId >= 0) {
                        // 判断是否需要重置滤波器 (如果之前目标丢失) .count()表示：得到这个浮点数，表示“过去了多少秒”。
                        auto timeSinceLastValidTarget = std::chrono::duration<float>(
                            std::chrono::high_resolution_clock::now() - g_lastValidTargetTime).count();

                        if (!g_filterState.initialized || timeSinceLastValidTarget > g_predictionTime.load()) {
                            ResetFilterState(nearestTarget);
                        }
                        else {
                            UpdateFilter(nearestTarget);
                        }
                    }
                }

                // 预测未来位置
                PredictionResult prediction = PredictFuturePosition();

                // 将预测结果写入环形缓冲区
                g_predictionBuffer.write(prediction);

            }
            catch (const std::exception& e) {
                std::cerr << "Error in prediction module: " << e.what() << std::endl;
            }

            // 控制循环频率
            auto loopEnd = std::chrono::high_resolution_clock::now();
            auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(loopEnd - loopStart);

            if (processingTime < loopInterval) {
                std::this_thread::sleep_for(loopInterval - processingTime);
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
        g_lastValidTargetTime = std::chrono::high_resolution_clock::now() - std::chrono::seconds(10); // 初始设为10秒前
        g_filterState = FilterState(); // 重置滤波器状态

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
        return g_predictionBuffer.read(result, false);
    }

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