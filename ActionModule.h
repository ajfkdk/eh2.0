#ifndef ACTION_MODULE_H
#define ACTION_MODULE_H

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <chrono>
#include "MouseController.h"
#include "PredictionModule.h"
#include "KeyboardListener.h"

// ����2D��ṹ��
struct Point2D {
    float x;
    float y;
};

// �����̰߳�ȫ�Ĺ���״̬�ṹ��
// SharedState �ṹ��
struct SharedState {
    // ����״̬
    std::atomic<bool> isAutoAimEnabled{ false }; // �Զ���׼����
    std::atomic<bool> isAutoFireEnabled{ false }; // �Զ����𿪹�
    std::atomic<float> targetDistance{ 999.0f }; // Ŀ�����
    std::chrono::steady_clock::time_point targetValidSince; // Ŀ���������ʱ��
};

// PID�������ṹ��
struct PIDController {
    // PID����
    std::atomic<float> kp{ 0.5f };  // ����ϵ��
    std::atomic<float> ki{ 0.1f }; // ����ϵ��
    std::atomic<float> kd{ 0.0f };  // ΢��ϵ��
    std::atomic<float> yControlFactor{ 1.0f }; // Y���������ϵ�������ڱ�ͷ�ʿ���

    // ����������
    float integralLimit = 10.0f;

    // ��һ�ε����
    float previousErrorX = 0.0f;
    float previousErrorY = 0.0f;

    // �����ۼ�ֵ
    float integralX = 0.0f;
    float integralY = 0.0f;

    // ��һ�μ���ʱ���
    std::chrono::steady_clock::time_point lastTime;

    PIDController() {
        Reset();
    }

    void Reset() {
        previousErrorX = 0.0f;
        previousErrorY = 0.0f;
        integralX = 0.0f;
        integralY = 0.0f;
        lastTime = std::chrono::steady_clock::now();
    }
};

class ActionModule {
private:
    static std::thread actionThread;
    static std::thread fireThread; // ��������߳�
    static std::thread pidDebugThread; // PID�����߳�
    static std::atomic<bool> running;
    static std::atomic<bool> pidDebugEnabled;
    static std::atomic<bool> usePrediction; // �Ƿ�ʹ��Ԥ�⹦��
    static std::unique_ptr<MouseController> mouseController;
    static std::shared_ptr<SharedState> sharedState; // ����״̬
    static PIDController pidController; // PID������
    static std::unique_ptr<KeyboardListener> keyboardListener; // ���̼�����

    // Ԥ����ر���
    static Point2D lastTargetPos;  // ��һ֡Ŀ��λ��
    static bool hasLastTarget;  // �Ƿ�����һ֡Ŀ��λ��



    // ������ѭ�� (����)
    static void ProcessLoop();

    // ��������̺߳���
    static void FireControlLoop();

    // PID�����̺߳���
    static void PIDDebugLoop();

    // ������̰����¼�
    static void HandleKeyPress(int key);

    // ��һ���ƶ�ֵ��ָ����Χ
    static std::pair<float, float> NormalizeMovement(float x, float y, float maxValue);

    // PID�����㷨
    static std::pair<float, float> ApplyPIDControl(float errorX, float errorY);

    // Ԥ����һ֡λ��
    static Point2D PredictNextPosition(const Point2D& current);

    // ����ѹǹ�ƶ���
    static float CalculateRecoilCompensation();

    // ����ѹǹ״̬
    static void ResetRecoilState();

    // ����ѹǹ״̬
    static void UpdateRecoilState(bool isLeftButtonDown);

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

    // ����/����PID����
    static void EnablePIDDebug(bool enable);

    // ��ȡPID����״̬
    static bool IsPIDDebugEnabled();

    // ����ʹ��Ԥ���PID
    static void SetUsePrediction(bool enable);

    // ��ȡ��ǰʹ��ģʽ
    static bool IsUsingPrediction();

   
};

#endif // ACTION_MODULE_H