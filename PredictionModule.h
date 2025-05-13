// PredictionModule.h
#ifndef PREDICTION_MODULE_H
#define PREDICTION_MODULE_H

#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <opencv2/opencv.hpp>
#include "DetectionModule.h" // 引入DetectionResult结构
#include "RingBuffer.h" // 引入环形缓冲区

// 预测结果结构体
struct PredictionResult {
    int x;      // 预测的目标X坐标 (999表示目标丢失)
    int y;      // 预测的目标Y坐标 (999表示目标丢失)
    float vx;   // 预测的目标X速度
    float vy;   // 预测的目标Y速度
    bool isTracking; // 是否正在跟踪目标
    int targetClassId; // 当前跟踪的目标类型
    std::chrono::high_resolution_clock::time_point timestamp; // 时间戳
};

namespace PredictionModule {
    // 初始化预测模块，返回工作线程
    std::thread Initialize();

    // 检查预测模块是否在运行
    bool IsRunning();

    // 停止预测模块
    void Stop();

    // 清理资源
    void Cleanup();

    // 获取最新预测结果
    bool GetLatestPrediction(PredictionResult& result);

    // 设置调试模式
    void SetDebugMode(bool enabled);

    // 获取调试模式状态
    bool IsDebugModeEnabled();

    // 卡尔曼滤波器参数设置
    void SetProcessNoise(float q);
    void SetMeasurementNoise(float r);

    // 目标跟踪参数设置
    void SetMaxTargetSwitchDistance(float distance);
    void SetMaxLostFrames(int frames);

    // 这些函数保留为接口兼容，但不再使用
    void SetGFactor(float g);
    void SetHFactor(float h);
    void SetPredictionTime(float seconds);
}

#endif // PREDICTION_MODULE_H