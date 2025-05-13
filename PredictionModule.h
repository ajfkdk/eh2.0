// PredictionModule.h
#ifndef PREDICTION_MODULE_H
#define PREDICTION_MODULE_H

#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <deque>
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

    // ���û������ڴ�С���������ʷ֡������
    void SetWindowSize(int size);

    // ��ȡ�������ڴ�С
    int GetWindowSize();

    // ����Ŀ�������ľ�����ֵ����������ֵ����Ϊ��Ŀ�꣩
    void SetDistanceThreshold(float threshold);

    // ��ȡ������ֵ
    float GetDistanceThreshold();

    // ����Ŀ�궪ʧ����֡��������֡���ٳ��ֲ���Ϊ��ʧ��
    void SetLostFrameThreshold(int frames);

    // ��ȡĿ�궪ʧ����֡��
    int GetLostFrameThreshold();

    // ���õ���ģʽ
    void SetDebugMode(bool enabled);

    // ��ȡ����ģʽ״̬
    bool IsDebugModeEnabled();

    // ��Щ��������Ϊ�ӿڼ��ݣ�������ʹ��
    void SetGFactor(float g);
    void SetHFactor(float h);
    void SetPredictionTime(float seconds);
}

#endif // PREDICTION_MODULE_H