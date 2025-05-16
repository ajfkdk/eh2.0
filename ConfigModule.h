#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <mutex>
#include "KeyboardListener.h"

// ���ò����ṹ��
struct HelperConfig {
    // ѹǹ��ر���
    std::atomic<bool> isAutoRecoilEnabled{ false }; // �Զ�ѹǹ����
    std::atomic<float> pressForce{ 3.0f }; // ѹǹ����
    std::atomic<int> pressTime{ 800 }; // ѹǹ����ʱ��(ms)
    std::atomic<bool> needPressDownWhenAim{ true }; // �Ƿ�������ʱ��ѹ
    std::chrono::steady_clock::time_point pressStartTime; // ѹǹ��ʼʱ��
    std::chrono::steady_clock::time_point lastPressCheckTime; // �ϴ�ѹǹ���ʱ��

    // ������ز���
    std::atomic<float> predictAlpha{ 0.3f }; // Ԥ��ϵ����Ĭ��0.3����Χ0.2~0.5
    std::atomic<int> aimFov{ 35 }; // ��׼�ӳ��Ƕ�
    static constexpr int targetValidDurationMs = 200; // Ŀ����Ч����ʱ��(����)
    static constexpr int humanizationFactor = 2; // ���˻����� 
    static constexpr float deadZoneThreshold = 7.f; // ������ֵ


    // Ŀ����� classes{ "ct_body", "ct_head", "t_body", "t_head" };
    std::vector<int> targetClasses{ 1, 3 };
    std::mutex targetClassesMutex;
};

class ConfigModule {
public:
    // ��ʼ������ģ��
    static void Initialize();

    // ��������ģ��
    static void Cleanup();

    // ��ȡ����ʵ��
    static std::shared_ptr<HelperConfig> GetConfig();

    // ����/���ò�������ģʽ
    static void EnableConfigDebug(bool enable);

    // ����������ģʽ�Ƿ�����
    static bool IsConfigDebugEnabled();

    // ������̰����¼�
    static void HandleKeyPress(int key);

    // ��ȡ/����Ŀ�����
    static std::vector<int> GetTargetClasses();
    static void SetTargetClasses(const std::vector<int>& classes);

private:
    static std::unique_ptr<KeyboardListener> keyboardListener;
    static std::shared_ptr<HelperConfig> config;
    static std::atomic<bool> configDebugEnabled;
    static std::thread configDebugThread;

    // ���������̺߳���
    static void ConfigDebugLoop();
};