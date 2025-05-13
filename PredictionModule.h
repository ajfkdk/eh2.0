// PredictionModule.h
#ifndef PREDICTION_MODULE_H
#define PREDICTION_MODULE_H

#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <opencv2/opencv.hpp>
#include "DetectionModule.h" // ����DetectionResult�ṹ
#include "RingBuffer.h" // ���뻷�λ�����

// Ԥ�����ṹ��
struct PredictionResult {
    int x;      // Ԥ���Ŀ��X���� (999��ʾĿ�궪ʧ)
    int y;      // Ԥ���Ŀ��Y���� (999��ʾĿ�궪ʧ)
    float vx;   // Ԥ���Ŀ��X�ٶ�
    float vy;   // Ԥ���Ŀ��Y�ٶ�
    bool isTracking; // �Ƿ����ڸ���Ŀ��
    int targetClassId; // ��ǰ���ٵ�Ŀ������
    std::chrono::high_resolution_clock::time_point timestamp; // ʱ���
};

namespace PredictionModule {
    // ��ʼ��Ԥ��ģ�飬���ع����߳�
    std::thread Initialize();

    // ���Ԥ��ģ���Ƿ�������
    bool IsRunning();

    // ֹͣԤ��ģ��
    void Stop();

    // ������Դ
    void Cleanup();

    // ��ȡ����Ԥ����
    bool GetLatestPrediction(PredictionResult& result);

    // ���õ���ģʽ
    void SetDebugMode(bool enabled);

    // ��ȡ����ģʽ״̬
    bool IsDebugModeEnabled();

    // �������˲�����������
    void SetProcessNoise(float q);
    void SetMeasurementNoise(float r);

    // Ŀ����ٲ�������
    void SetMaxTargetSwitchDistance(float distance);
    void SetMaxLostFrames(int frames);

    // ��Щ��������Ϊ�ӿڼ��ݣ�������ʹ��
    void SetGFactor(float g);
    void SetHFactor(float h);
    void SetPredictionTime(float seconds);
}

#endif // PREDICTION_MODULE_H