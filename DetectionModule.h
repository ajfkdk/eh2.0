#ifndef DETECTION_MODULE_H
#define DETECTION_MODULE_H

#include <thread>
#include <atomic>
#include <vector>
#include "DetectionResult.h"
#include <opencv2/opencv.hpp>

namespace DetectionModule {
    // 初始化检测模块，返回检测线程
    std::thread Initialize(const std::string& modelPath = "./123.onnx");

    // 获取最新的检测结果（单个目标 - 最高置信度）
    bool GetLatestDetectionResult(DetectionResult& result);

    // 等待并获取检测结果
    bool WaitForResult(DetectionResult& result);

    // 获取所有检测到的目标
    std::vector<DetectionResult> GetAllResults();

    // 清理资源
    void Cleanup();

    // 检查模块是否在运行
    bool IsRunning();

    // 停止模块
    void Stop();

    // 设置调试模式
    void SetDebugMode(bool enabled);

    // 检查是否开启了调试模式
    bool IsDebugModeEnabled();

    // 获取当前debug帧
    bool GetDebugFrame(cv::Mat& frame);

    // 设置是否显示检测框
    void SetShowDetections(bool show);

    // 设置置信度阈值
    void SetConfidenceThreshold(float threshold);

    // 添加绘制预测点的函数
    void DrawPredictionPoint(int x, int y);
}

#endif // DETECTION_MODULE_H