#ifndef ACTION_MODULE_H
#define ACTION_MODULE_H

#include <thread>
#include <atomic>
#include <memory>
#include "MouseController.h"
#include "PredictionModule.h"

class ActionModule {
private:
    static std::thread actionThread;
    static std::atomic<bool> running;
    static std::unique_ptr<MouseController> mouseController;

    // ������ѭ��
    static void ProcessLoop();

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
    static std::thread fireThread; // �����ĵ����߳�
    static void FireControlLoop(); // �����ĵ�������̺߳���
    static std::shared_ptr<SharedState> sharedState; // �����Ĺ���״̬
};

#endif // ACTION_MODULE_H