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