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

    // 卡尔曼滤波器参数
    std::atomic<float> g_processNoise{ 0.01f };     // 过程噪声 - 较小值使预测更平滑但反应更慢
    std::atomic<float> g_measurementNoise{ 0.1f };  // 测量噪声 - 较大值减少抖动但增加延迟

    // 目标跟踪参数
    std::atomic<float> g_maxTargetSwitchDistance{ 50.0f }; // 最大目标切换距离（像素）
    std::atomic<int> g_maxLostFrames{ 30 };               // 最大丢失帧数量（30帧≈1秒@30fps）

    // 环形缓冲区，用于存储预测结果
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // 卡尔曼滤波器相关变量
    cv::KalmanFilter g_kalmanFilter(4, 2, 0);   // 状态变量(x,y,vx,vy)，测量变量(x,y)
    cv::Mat g_kalmanMeasurement;                // 测量矩阵

    // 跟踪状态变量
    bool g_isTracking = false;           // 是否正在跟踪目标
    int g_lostFrameCount = 0;            // 连续丢失帧计数
    int g_currentTargetClassId = -1;     // 当前跟踪的目标类型
    float g_lastX = 0.0f, g_lastY = 0.0f; // 上一次预测的位置

    // 初始化卡尔曼滤波器
    void InitKalmanFilter() {
        // 状态转移矩阵 - 考虑位置和速度的线性模型
        // [1, 0, 1, 0]
        // [0, 1, 0, 1]
        // [0, 0, 1, 0]
        // [0, 0, 0, 1]
        g_kalmanFilter.transitionMatrix = (cv::Mat_<float>(4, 4) <<
            1, 0, 1, 0,
            0, 1, 0, 1,
            0, 0, 1, 0,
            0, 0, 0, 1);

        // 测量矩阵 - 只测量位置，不测量速度
        // [1, 0, 0, 0]
        // [0, 1, 0, 0]
        g_kalmanFilter.measurementMatrix = (cv::Mat_<float>(2, 4) <<
            1, 0, 0, 0,
            0, 1, 0, 0);

        // 过程噪声协方差矩阵
        float q = g_processNoise.load();
        g_kalmanFilter.processNoiseCov = (cv::Mat_<float>(4, 4) <<
            q, 0, 0, 0,
            0, q, 0, 0,
            0, 0, q * 2, 0,
            0, 0, 0, q * 2);

        // 测量噪声协方差矩阵
        float r = g_measurementNoise.load();
        g_kalmanFilter.measurementNoiseCov = (cv::Mat_<float>(2, 2) <<
            r, 0,
            0, r);

        // 后验错误协方差矩阵
        g_kalmanFilter.errorCovPost = cv::Mat::eye(4, 4, CV_32F);

        // 初始化测量矩阵
        g_kalmanMeasurement = cv::Mat::zeros(2, 1, CV_32F);
    }

    // 重新初始化卡尔曼滤波器，用于目标切换
    void ResetKalmanFilter(float x, float y, float vx = 0.0f, float vy = 0.0f) {
        // 重置状态矩阵，设置初始位置和速度
        g_kalmanFilter.statePost.at<float>(0) = x;
        g_kalmanFilter.statePost.at<float>(1) = y;
        g_kalmanFilter.statePost.at<float>(2) = vx;
        g_kalmanFilter.statePost.at<float>(3) = vy;

        // 重置后验错误协方差
        g_kalmanFilter.errorCovPost = cv::Mat::eye(4, 4, CV_32F);
    }

    // 计算两点间的欧氏距离
    float CalculateDistance(float x1, float y1, float x2, float y2) {
        float dx = x1 - x2;
        float dy = y1 - y2;
        return std::sqrt(dx * dx + dy * dy);
    }

    // 从检测结果中查找最佳目标
    DetectionResult FindBestTarget(const std::vector<DetectionResult>& targets) {
        if (targets.empty()) {
            DetectionResult emptyResult;
            emptyResult.classId = -1;
            return emptyResult;
        }

        // 屏幕中心坐标 (基于320x320的图像)
        const int centerX = 160;
        const int centerY = 160;

        // 过滤出有效目标（只考虑classId=1或3的锁头目标）
        std::vector<DetectionResult> validTargets;
        for (const auto& target : targets) {
            if (target.classId == 1 || target.classId == 3) {
                validTargets.push_back(target);
            }
        }

        if (validTargets.empty()) {
            DetectionResult emptyResult;
            emptyResult.classId = -1;
            return emptyResult;
        }

        // 如果已经在跟踪目标
        if (g_isTracking) {
            // 尝试找到与当前跟踪目标最接近的目标
            DetectionResult closestTarget;
            float minDistance = g_maxTargetSwitchDistance.load();
            bool foundMatch = false;

            for (const auto& target : validTargets) {
                float distance = CalculateDistance(g_lastX, g_lastY, target.x, target.y);
                if (distance < minDistance) {
                    minDistance = distance;
                    closestTarget = target;
                    foundMatch = true;
                }
            }

            // 如果找到匹配的目标，返回它
            if (foundMatch) {
                return closestTarget;
            }
        }

        // 如果没有在跟踪目标，或没有找到匹配的目标，查找离中心最近的目标
        DetectionResult nearest = validTargets[0];
        float minDistance = std::numeric_limits<float>::max();

        for (const auto& target : validTargets) {
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

        // 初始化卡尔曼滤波器
        InitKalmanFilter();

        // 重置状态变量
        g_isTracking = false;
        g_lostFrameCount = 0;
        g_currentTargetClassId = -1;

        while (g_running) {
            try {
                // 获取所有检测目标
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                // 创建预测结果
                PredictionResult prediction;
                prediction.timestamp = std::chrono::high_resolution_clock::now();
                prediction.isTracking = false;
                prediction.targetClassId = -1;

                // 卡尔曼滤波器预测步骤
                cv::Mat predictedState = g_kalmanFilter.predict();
                float predictedX = predictedState.at<float>(0);
                float predictedY = predictedState.at<float>(1);
                float predictedVX = predictedState.at<float>(2);
                float predictedVY = predictedState.at<float>(3);

                // 查找最佳目标
                DetectionResult bestTarget = FindBestTarget(targets);

                // 如果找到有效目标
                if (bestTarget.classId >= 0) {
                    // 目标有效
                    g_lostFrameCount = 0;

                    // 如果不是正在跟踪的目标，或者目标类型变化
                    if (!g_isTracking || g_currentTargetClassId != bestTarget.classId) {
                        // 重新初始化卡尔曼滤波器
                        ResetKalmanFilter(bestTarget.x, bestTarget.y);
                        g_isTracking = true;
                        g_currentTargetClassId = bestTarget.classId;
                    }
                    else {
                        // 卡尔曼滤波器更新步骤
                        g_kalmanMeasurement.at<float>(0) = bestTarget.x;
                        g_kalmanMeasurement.at<float>(1) = bestTarget.y;
                        cv::Mat correctedState = g_kalmanFilter.correct(g_kalmanMeasurement);

                        // 从校正后的状态获取位置和速度
                        predictedX = correctedState.at<float>(0);
                        predictedY = correctedState.at<float>(1);
                        predictedVX = correctedState.at<float>(2);
                        predictedVY = correctedState.at<float>(3);
                    }

                    // 保存预测结果
                    prediction.x = static_cast<int>(std::round(predictedX));
                    prediction.y = static_cast<int>(std::round(predictedY));
                    prediction.vx = predictedVX;
                    prediction.vy = predictedVY;
                    prediction.isTracking = true;
                    prediction.targetClassId = bestTarget.classId;

                    // 更新上一次的位置
                    g_lastX = predictedX;
                    g_lastY = predictedY;
                }
                else {
                    // 目标丢失
                    g_lostFrameCount++;

                    // 如果丢失帧数未超过阈值，使用卡尔曼预测的位置
                    if (g_isTracking && g_lostFrameCount <= g_maxLostFrames.load()) {
                        // 使用预测的位置
                        prediction.x = static_cast<int>(std::round(predictedX));
                        prediction.y = static_cast<int>(std::round(predictedY));
                        prediction.vx = predictedVX;
                        prediction.vy = predictedVY;
                        prediction.isTracking = true;
                        prediction.targetClassId = g_currentTargetClassId;

                        // 更新上一次的位置
                        g_lastX = predictedX;
                        g_lastY = predictedY;
                    }
                    else {
                        // 目标彻底丢失，重置跟踪状态
                        prediction.x = 999;
                        prediction.y = 999;
                        prediction.vx = 0;
                        prediction.vy = 0;
                        prediction.isTracking = false;
                        prediction.targetClassId = -1;
                        g_isTracking = false;
                    }
                }

                // 将预测结果写入环形缓冲区
                g_predictionBuffer.write(prediction);

                // debug模式处理 - 向检测模块提供预测点信息
                if (g_debugMode.load() && prediction.isTracking) {
                    // 调用检测模块的绘制预测点函数
                    DetectionModule::DrawPredictionPoint(prediction.x, prediction.y);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error in prediction module: " << e.what() << std::endl;
            }

            // 控制循环频率，避免过高CPU使用率
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

    // 设置卡尔曼滤波器参数
    void SetProcessNoise(float q) {
        g_processNoise = q;
        InitKalmanFilter(); // 重新初始化卡尔曼滤波器
    }

    void SetMeasurementNoise(float r) {
        g_measurementNoise = r;
        InitKalmanFilter(); // 重新初始化卡尔曼滤波器
    }

    // 设置目标跟踪参数
    void SetMaxTargetSwitchDistance(float distance) {
        g_maxTargetSwitchDistance = distance;
    }

    void SetMaxLostFrames(int frames) {
        g_maxLostFrames = frames;
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