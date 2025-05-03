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

    // 添加：预测有效范围限制
    constexpr int MAX_VALID_COORDINATE = 1000;  // 合理的最大坐标值
    constexpr float MAX_VALID_VELOCITY = 300.0f; // 合理的最大速度值(像素/秒)

    // 添加：连续无效预测计数
    int g_consecutiveInvalidPredictions = 0;
    constexpr int MAX_INVALID_PREDICTIONS = 5;  // 允许的最大连续无效预测次数

    // 重置滤波器状态
    void ResetFilterState(const DetectionResult& target) {
        g_filterState.xEst = target.x;
        g_filterState.yEst = target.y;
        g_filterState.vxEst = 0;
        g_filterState.vyEst = 0;
        g_filterState.lastUpdateTime = std::chrono::high_resolution_clock::now();
        g_filterState.initialized = true;
        g_consecutiveInvalidPredictions = 0;  // 重置无效预测计数

        g_lastValidTargetTime = std::chrono::high_resolution_clock::now();

        std::cout << "Filter state reset with target at (" << target.x << "," << target.y << ")" << std::endl;
    }

    // 检查速度估计是否有效
    bool IsVelocityValid(float vx, float vy) {
        float speedMagnitude = std::sqrt(vx * vx + vy * vy);
        return speedMagnitude <= MAX_VALID_VELOCITY;
    }

    // 限制值在合理范围内
    float ClampValue(float value, float min, float max) {
        if (value < min) return min;
        if (value > max) return max;
        return value;
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
        if (deltaT <= 0.0001f) return; // 防止除以极小值

        // 如果时间间隔太大，说明可能是断断续续的处理，重置滤波器
        if (deltaT > 0.5f) {  // 如果超过0.5秒没有更新
            ResetFilterState(target);
            return;
        }

        // 1. 预测步骤 - 根据上一次的估计和速度预测当前位置
        float xPred = g_filterState.xEst + g_filterState.vxEst * deltaT;
        float yPred = g_filterState.yEst + g_filterState.vyEst * deltaT;

        // 2. 更新步骤 - 使用实际测量值和预测值之间的残差更新估计
        // 计算残差（实际位置与预测位置的差）
        float residualX = target.x - xPred;
        float residualY = target.y - yPred;

        // 添加：限制残差大小，防止突变导致速度估计失控
        float maxResidual = 50.0f;  // 根据你的应用场景调整
        residualX = ClampValue(residualX, -maxResidual, maxResidual);
        residualY = ClampValue(residualY, -maxResidual, maxResidual);

        // 更新位置估计: 预测位置 + g * 残差
        float g = g_gFactor.load();
        float h = g_hFactor.load();

        float xEst = xPred + g * residualX;
        float yEst = yPred + g * residualY;

        // 更新速度估计: 上次速度估计 + h * 残差 / 时间间隔
        float vxEst = g_filterState.vxEst + h * (residualX / deltaT);
        float vyEst = g_filterState.vyEst + h * (residualY / deltaT);

        // 添加：限制速度估计的大小
        if (!IsVelocityValid(vxEst, vyEst)) {
            // 如果速度估计不合理，限制它们
            float speedMagnitude = std::sqrt(vxEst * vxEst + vyEst * vyEst);
            if (speedMagnitude > MAX_VALID_VELOCITY) {
                float scale = MAX_VALID_VELOCITY / speedMagnitude;
                vxEst *= scale;
                vyEst *= scale;

                // 记录速度被限制的情况
                std::cout << "Velocity limited from " << speedMagnitude
                    << " to " << MAX_VALID_VELOCITY << std::endl;

                // 增加无效预测计数
                g_consecutiveInvalidPredictions++;

                // 如果连续多次出现无效速度，考虑重置滤波器
                if (g_consecutiveInvalidPredictions > MAX_INVALID_PREDICTIONS) {
                    ResetFilterState(target);
                    return;
                }
            }
        }
        else {
            // 正常速度，重置计数器
            g_consecutiveInvalidPredictions = 0;
        }

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

        if (!g_filterState.initialized || timeSinceLastValidTarget > g_predictionTime.load() * 1.5f) {
            // 目标丢失，输出特殊值
            result.x = 999;
            result.y = 999;
            return result;
        }

        // 计算预测时间点的位置
        float predictionTimeSeconds = g_predictionTime.load();
        float predictedX = g_filterState.xEst + g_filterState.vxEst * predictionTimeSeconds;
        float predictedY = g_filterState.yEst + g_filterState.vyEst * predictionTimeSeconds;

        // 添加：验证预测值是否在合理范围内
        if (std::isnan(predictedX) || std::isnan(predictedY) ||
            std::abs(predictedX) > MAX_VALID_COORDINATE ||
            std::abs(predictedY) > MAX_VALID_COORDINATE) {

            // 预测值异常，返回目标丢失信号
            std::cout << "Invalid prediction detected: (" << predictedX << "," << predictedY << ")" << std::endl;
            result.x = 999;
            result.y = 999;

            // 预测异常可能表明滤波器状态有问题，增加重置可能性
            g_consecutiveInvalidPredictions++;
            return result;
        }

        // 转换为整数坐标
        result.x = static_cast<int>(predictedX);
        result.y = static_cast<int>(predictedY);

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
        const auto loopInterval = std::chrono::microseconds(16667); // 约为16.7ms，确保>60FPS

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
                        // 判断是否需要重置滤波器 (如果之前目标丢失)
                        auto timeSinceLastValidTarget = std::chrono::duration<float>(
                            std::chrono::high_resolution_clock::now() - g_lastValidTargetTime).count();

                        if (!g_filterState.initialized ||
                            timeSinceLastValidTarget > g_predictionTime.load() * 1.5f ||
                            g_consecutiveInvalidPredictions > MAX_INVALID_PREDICTIONS) {
                            ResetFilterState(nearestTarget);
                        }
                        else {
                            UpdateFilter(nearestTarget);
                        }
                    }
                }

                // 如果有连续无效预测但没有新目标，考虑重置滤波器状态
                if (g_consecutiveInvalidPredictions > MAX_INVALID_PREDICTIONS && g_filterState.initialized) {
                    // 将滤波器标记为未初始化，强制下一个有效目标时重置
                    g_filterState.initialized = false;
                    std::cout << "Filter marked as uninitialized due to consecutive invalid predictions" << std::endl;

                    // 发送目标丢失信号
                    PredictionResult lostSignal;
                    lostSignal.x = 999;
                    lostSignal.y = 999;
                    lostSignal.timestamp = std::chrono::high_resolution_clock::now();
                    g_predictionBuffer.write(lostSignal);

                    // 重置计数器
                    g_consecutiveInvalidPredictions = 0;
                    continue;
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
        g_consecutiveInvalidPredictions = 0; // 重置无效预测计数

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