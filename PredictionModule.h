// PredictionModule.h
#ifndef PREDICTION_MODULE_H
#define PREDICTION_MODULE_H

#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include "DetectionModule.h" // ����DetectionResult�ṹ
#include "RingBuffer.h" // ���뻷�λ�����

// Ԥ�����ṹ��
struct PredictionResult {
    int x;      // Ԥ���Ŀ��X���� (999��ʾĿ�궪ʧ)
    int y;      // Ԥ���Ŀ��Y���� (999��ʾĿ�궪ʧ)
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

    // ����������λ�ñ仯��ֵ
    void SetPositionThreshold(float threshold);

    // �����������ȶ���ʱ����ֵ
    void SetStabilityTimeThreshold(int milliseconds);

    // ����������ƽ������
    void SetSmoothingFactor(float factor);

    // ��Щ��������Ϊ�ӿڼ��ݣ���ʵ���ϲ���ʹ��
    void SetGFactor(float g);
    void SetHFactor(float h);
    void SetPredictionTime(float seconds);
}

#endif // PREDICTION_MODULE_H