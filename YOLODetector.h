#ifndef YOLO_DETECTOR_H
#define YOLO_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>
#include "DetectionResult.h"

/**
 * YOLO������� - �߼���װ
 * ʹ��PIMPLģʽ����ʵ��ϸ��
 */
class YOLODetector {
public:
    /**
     * ���캯��
     */
    YOLODetector();
    
    /**
     * ��������
     */
    ~YOLODetector();

    /**
     * ��ʼ�������
     * @param modelPath ģ���ļ�·��
     * @return ��ʼ���Ƿ�ɹ�
     */
    bool Initialize(const std::string& modelPath = "models/yolo.onnx");

    /**
     * ����ͼ�񲢷��ؼ����
     * @param image ����ͼ��
     * @return ���������
     */
    std::vector<DetectionResult> ProcessImage(const cv::Mat& image);

    /**
     * �������Ŷ���ֵ
     * @param threshold �µ����Ŷ���ֵ
     */
    void SetConfidenceThreshold(float threshold);

    /**
     * ��ȡ���һ�δ�����Ϣ
     * @return ������Ϣ�ַ���
     */
    std::string GetLastError() const;

    /**
     * �����Ƿ���ʾ�����
     * @param show �Ƿ���ʾ
     */
    void SetShowDetections(bool show);

private:
    class Impl; // ǰ������ʵ����
    std::unique_ptr<Impl> pImpl; // PIMPLģʽ
};

#endif // YOLO_DETECTOR_H