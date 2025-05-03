#ifndef ACTION_MODULE_H
#define ACTION_MODULE_H

#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <vector>
#include <string>
#include <memory>
#include "MouseController.h"
#include "PredictionModule.h"

class ActionModule {
private:
    static std::thread actionThread;
    static std::atomic<bool> running;
    static std::unique_ptr<MouseController> mouseController;
    static std::atomic<int> humanizationFactor; // 1-100�����˲���
    static std::atomic<bool> fireEnabled;
    static std::mutex logMutex;

    // ��ǰ��Ŀ��λ��
    static int currentX, currentY;
    static int targetX, targetY;
    static std::chrono::high_resolution_clock::time_point lastUpdateTime;

    // ����ƶ���¼��־
    static void LogMouseMovement(const std::string& message);

    // ����ʧĿ������˶���
    static void PerformHumanLikeMovementOnTargetLoss();

    // �������˻�·����
    static std::vector<std::pair<float, float>> GenerateHumanLikePath(
        float startX, float startY, float endX, float endY, int humanFactor);

    // �����ٶ�����
    static float CalculateSpeedFactor(float progress, int humanFactor);

    // ���΢С����
    static std::pair<float, float> AddJitter(float x, float y, float distance, int humanFactor);

    // ������ѭ��
    static void ProcessLoop();

    // ��ȡ��ǰ���λ��
    static void UpdateCurrentMousePosition();

    // ��������������
    static void ControlMouseFire(bool targetVisible);

public:
    // ��ʼ��ģ��
    static std::thread Initialize(int humanFactor = 50);

    // ������Դ
    static void Cleanup();

    // ֹͣģ��
    static void Stop();

    // �ж�ģ���Ƿ�������
    static bool IsRunning();

    // �������˲��� (1-100)
    static void SetHumanizationFactor(int factor);

    // ��ȡ���˲���
    static int GetHumanizationFactor();

    // ������������
    static void SetMouseController(std::unique_ptr<MouseController> controller);

    // ����/�����Զ�����
    static void EnableAutoFire(bool enable);

    // �ж��Զ������Ƿ�����
    static bool IsAutoFireEnabled();
};

#endif // ACTION_MODULE_H#pragma once
