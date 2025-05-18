#ifndef YOLO_DETECTOR_H
#define YOLO_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>
#include "DetectionResult.h"

/**
 * YOLO检测器类 - 高级封装
 * 使用PIMPL模式隐藏实现细节
 */
class YOLODetector {
public:
    /**
     * 构造函数
     */
    YOLODetector();
    
    /**
     * 析构函数
     */
    ~YOLODetector();

    /**
     * 初始化检测器
     * @param modelPath 模型文件路径
     * @return 初始化是否成功
     */
    bool Initialize(const std::string& modelPath = "models/yolo.onnx");

    /**
     * 处理图像并返回检测结果
     * @param image 输入图像
     * @return 检测结果向量
     */
    std::vector<DetectionResult> ProcessImage(const cv::Mat& image);

    /**
     * 设置置信度阈值
     * @param threshold 新的置信度阈值
     */
    void SetConfidenceThreshold(float threshold);

    /**
     * 获取最后一次错误信息
     * @return 错误信息字符串
     */
    std::string GetLastError() const;

    /**
     * 设置是否显示检测结果
     * @param show 是否显示
     */
    void SetShowDetections(bool show);

private:
    class Impl; // 前置声明实现类
    std::unique_ptr<Impl> pImpl; // PIMPL模式
};

#endif // YOLO_DETECTOR_H