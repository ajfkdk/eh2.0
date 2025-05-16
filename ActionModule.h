// ActionModule.h
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
struct SharedState {
    std::atomic<bool> isAutoAimEnabled{ false };
    std::atomic<bool> isAutoFireEnabled{ false };
    std::atomic<float> targetDistance{ 999.0f };
    std::chrono::steady_clock::time_point targetValidSince{ std::chrono::steady_clock::now() }; // ���Ŀ���������ʱ���

    // ѹǹ���״̬
    std::atomic<bool> isRecoilControlEnabled{ true }; // ѹǹ����
    std::atomic<float> pressForce{ 10.0f }; // ѹǹ���ȣ�Ĭ��3.0
    std::atomic<int> pressTime{ 2000 }; // ѹǹ����ʱ��(ms)��Ĭ��1000ms
    std::atomic<bool> needPressDownWhenAim{ true }; // ����ʱ�Ƿ���Ҫ��ѹ

    std::mutex mutex;
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

// ѹǹ״̬�ṹ��
struct RecoilControlState {
    std::atomic<bool> isLeftButtonPressed{ false }; // �������Ƿ���
    std::chrono::steady_clock::time_point pressStartTime; // ���¿�ʼʱ��
    std::chrono::steady_clock::time_point lastRecoilTime; // �ϴ�ѹǹʱ��
};

class ActionModule {
private:
    static std::thread actionThread;
    static std::thread fireThread; // ��������߳�
    static std::thread recoilThread; // ѹǹ�����߳�
    static std::thread pidDebugThread; // PID�����߳�
    static std::atomic<bool> running;
    static std::atomic<bool> pidDebugEnabled;
    static std::atomic<bool> usePrediction; // �Ƿ�ʹ��Ԥ�⹦��
    static std::unique_ptr<MouseController> mouseController;
    static std::shared_ptr<SharedState> sharedState; // ����״̬
    static PIDController pidController; // PID������
    static std::unique_ptr<KeyboardListener> keyboardListener; // ���̼�����
    static RecoilControlState recoilState; // ѹǹ״̬

    // Ԥ����ر���
    static float predictAlpha;  // Ԥ��ϵ��
    static Point2D lastTargetPos;  // ��һ֡Ŀ��λ��
    static bool hasLastTarget;  // �Ƿ�����һ֡Ŀ��λ��

    // ��������
    static constexpr int humanizationFactor = 2; // ���˻�����
    static constexpr float deadZoneThreshold = 7.f; // ������ֵ
    static int aimFov; // ��׼�ӳ��Ƕ�
    static constexpr int targetValidDurationMs = 200; // Ŀ����Ч����ʱ��(����)

    // ������ѭ�� (����)
    static void ProcessLoop();

    // ��������̺߳���
    static void FireControlLoop();

    // PID�����̺߳���
    static void PIDDebugLoop();

    // ѹǹ�����̺߳���
    static void RecoilControlLoop();

    // ������̰����¼�
    static void HandleKeyPress(int key);

    // ��һ���ƶ�ֵ��ָ����Χ
    static std::pair<float, float> NormalizeMovement(float x, float y, float maxValue);

    // PID�����㷨
    static std::pair<float, float> ApplyPIDControl(float errorX, float errorY);

    // Ԥ����һ֡λ��
    static Point2D PredictNextPosition(const Point2D& current);

    // Ӧ��ѹǹЧ��
    static float ApplyRecoilControl();

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

    // ����Ԥ��ϵ��
    static void SetPredictAlpha(float alpha);
};

#endif // ACTION_MODULE_H