#ifndef DETECTION_MODULE_H
#define DETECTION_MODULE_H

#include <thread>
#include <atomic>
#include <vector>
#include "DetectionResult.h"

namespace DetectionModule {
    // ��ʼ�����ģ�飬���ؼ���߳�
    std::thread Initialize(const std::string& modelPath = "./123.onnx");

    // ��ȡ���µļ����������Ŀ�� - ������Ŷȣ�
    bool GetLatestDetectionResult(DetectionResult& result);

    // �ȴ�����ȡ�����
    bool WaitForResult(DetectionResult& result);

    // ��ȡ���м�⵽��Ŀ��
    std::vector<DetectionResult> GetAllResults();

    // ������Դ
    void Cleanup();

    // ���ģ���Ƿ�������
    bool IsRunning();

    // ֹͣģ��
    void Stop();

    // ���õ���ģʽ
    void SetDebugMode(bool enabled);

    // �����Ƿ���ʾ����
    void SetShowDetections(bool show);

    // �������Ŷ���ֵ
    void SetConfidenceThreshold(float threshold);
}

#endif // DETECTION_MODULE_H