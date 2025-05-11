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

    // 环形缓冲区，用于存储预测结果
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // 获取检测模块的debug帧
    bool GetDetectionDebugFrame(cv::Mat& frame) {
        return DetectionModule::GetDebugFrame(frame);
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

        // 计算循环间隔时间 (50ms)
        const auto loopInterval = std::chrono::milliseconds(50);

        while (g_running) {
            auto loopStart = std::chrono::high_resolution_clock::now();

            try {
                // 获取所有检测目标
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                // 创建预测结果
                PredictionResult prediction;
                prediction.timestamp = std::chrono::high_resolution_clock::now();

                // 检查是否有有效目标
                if (!targets.empty()) {
                    // 查找离中心最近的目标
                    DetectionResult nearestTarget = FindNearestTarget(targets);

                    // 如果有有效目标
                    if (nearestTarget.classId >= 0) {
                        // 直接使用最近目标的坐标
                        prediction.x = nearestTarget.x;
                        prediction.y = nearestTarget.y;
                    }
                    else {
                        // 无效目标，设置为目标丢失
                        prediction.x = 999;
                        prediction.y = 999;
                    }
                }
                else {
                    // 没有目标，设置为目标丢失
                    prediction.x = 999;
                    prediction.y = 999;
                }

                // 将预测结果写入环形缓冲区
                g_predictionBuffer.write(prediction);

                // debug模式处理 - 在检测模块的debug画面上绘制预测点
                if (g_debugMode.load() && prediction.x != 999 && prediction.y != 999) {
                    // 首先检查检测模块是否在debug模式下
                    if (DetectionModule::IsDebugModeEnabled()) {
                        // 获取当前最新的检测帧
                        cv::Mat debugFrame;
                        if (GetDetectionDebugFrame(debugFrame) && !debugFrame.empty()) {
                            // 画出预测点(蓝色点，半径5像素)
                            cv::circle(debugFrame, cv::Point(prediction.x, prediction.y), 5, cv::Scalar(255, 0, 0), -1);

                            // 在画面左上角添加文字标注
                            cv::putText(debugFrame, "Prediction Target", cv::Point(10, 20),
                                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 1);

                            // 显示带有预测点的frame
                            cv::imshow("Detection Debug", debugFrame);
                        }
                    }
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error in prediction module: " << e.what() << std::endl;
            }

            // 控制循环频率为50ms
            auto loopEnd = std::chrono::high_resolution_clock::now();
            auto processingTime = std::chrono::duration_cast<std::chrono::milliseconds>(loopEnd - loopStart);

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
        return g_predictionBuffer.read(result, true);
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