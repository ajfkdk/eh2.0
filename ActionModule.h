#ifndef ACTION_MODULE_H
#define ACTION_MODULE_H

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include "MouseController.h"
#include "PredictionModule.h"

struct Point2D {
    float x;
    float y;
};


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

    // Ԥ����ر���
    static float predictAlpha;  // Ԥ��ϵ��
    static Point2D lastTargetPos;  // ��һ֡Ŀ��λ��
    static bool hasLastTarget;  // �Ƿ�����һ֡Ŀ��λ��

    // ��������
    static constexpr int humanizationFactor = 2; // ���˻�����
    //������ֵ
    static constexpr float deadZoneThreshold = 7.f; // ������ֵ

    // ������ѭ�� (����)
    static void ProcessLoop();

    // ��������̺߳���
    static void FireControlLoop();

    // ��һ���ƶ�ֵ��ָ����Χ
    static std::pair<float, float> NormalizeMovement(float x, float y, float maxValue);

    // Ԥ����һ֡λ��
    static Point2D PredictNextPosition(const Point2D& current);

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

    // ����Ԥ��ϵ��
    static void SetPredictAlpha(float alpha);



};

#endif // ACTION_MODULE_H