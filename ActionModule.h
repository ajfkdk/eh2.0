#ifndef ACTION_MODULE_H
#define ACTION_MODULE_H

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include "MouseController.h"
#include "PredictionModule.h"

// �����̰߳�ȫ�Ĺ���״̬�ṹ��
struct SharedState {
    std::atomic<bool> isAutoAimEnabled{ false };
    std::atomic<bool> isAutoFireEnabled{ false };
    std::atomic<float> targetDistance{ 999.0f };
    std::atomic<bool> hasValidTarget{ false };
    std::mutex mutex;
};

class ActionModule {
private:
    static std::thread actionThread;
    static std::thread fireThread; // ��������߳�
    static std::atomic<bool> running;
    static std::unique_ptr<MouseController> mouseController;
    static std::shared_ptr<SharedState> sharedState; // ����״̬
    // ˿��ʱ��
    static std::atomic<int> smoothTimeAlpha;
    // ������ѭ�� (����)
    static void ProcessLoop();

    // ��������̺߳���
    static void FireControlLoop();

    // ��һ���ƶ�ֵ��ָ����Χ
    static std::pair<float, float> NormalizeMovement(float x, float y, float maxValue);

public:
    // ��ʼ��ģ��
    static std::thread Initialize();

    // ������Դ
    static void Cleanup();

    // ֹͣģ��
    static void Stop();

    // �ж�ģ���Ƿ�������
    static bool IsRunning();

    // ������������
    static void SetMouseController(std::unique_ptr<MouseController> controller);
};

#endif // ACTION_MODULE_H