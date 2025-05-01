#ifndef YOLO_DETECTOR_H
#define YOLO_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>
#include "DetectionResult.h"

// YOLO������� - ��װYOLO������
class YOLODetector {
public:
    YOLODetector();
    ~YOLODetector();

    // ��ʼ�������
    bool Initialize(const std::string& modelPath = "models/yolo.onnx");

    // ����ͼ�񲢷��ؼ����
    std::vector<DetectionResult> ProcessImage(const cv::Mat& image);

    // �������Ŷ���ֵ
    void SetConfidenceThreshold(float threshold);

    // ��ȡ���һ�δ���
    std::string GetLastError() const;

    // �����Ƿ���ʾ����
    void SetShowDetections(bool show);

private:
    class Impl; // ǰ������ʵ����
    std::unique_ptr<Impl> pImpl; // PIMPLģʽ
};

#endif // YOLO_DETECTOR_H