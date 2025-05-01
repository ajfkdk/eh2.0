#ifndef YOLO_DETECTOR_H
#define YOLO_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>
#include "DetectionResult.h"

// YOLO检测器类 - 封装YOLO推理功能
class YOLODetector {
public:
    YOLODetector();
    ~YOLODetector();

    // 初始化检测器
    bool Initialize(const std::string& modelPath = "models/yolo.onnx");

    // 处理图像并返回检测结果
    std::vector<DetectionResult> ProcessImage(const cv::Mat& image);

    // 设置置信度阈值
    void SetConfidenceThreshold(float threshold);

    // 获取最后一次错误
    std::string GetLastError() const;

    // 设置是否显示检测框
    void SetShowDetections(bool show);

private:
    class Impl; // 前置声明实现类
    std::unique_ptr<Impl> pImpl; // PIMPL模式
};

#endif // YOLO_DETECTOR_H