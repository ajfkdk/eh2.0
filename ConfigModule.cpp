#include "ConfigModule.h"
#include <iostream>
#include <algorithm>

// ��̬��Ա��ʼ��
std::unique_ptr<KeyboardListener> ConfigModule::keyboardListener = nullptr;
std::shared_ptr<HelperConfig> ConfigModule::config = std::make_shared<HelperConfig>();
std::atomic<bool> ConfigModule::configDebugEnabled(false);
std::thread ConfigModule::configDebugThread;

void ConfigModule::Initialize() {
    // �������̼�����
    keyboardListener = std::make_unique<KeyboardListener>();
    keyboardListener->onKeyPress = HandleKeyPress;

    std::cout << "����ģ���ѳ�ʼ��" << std::endl;
}

void ConfigModule::Cleanup() {
    // ֹͣ���̼���
    if (keyboardListener) {
        keyboardListener->Stop();
    }

    // ֹͣ�����߳�
    if (configDebugEnabled.load()) {
        configDebugEnabled.store(false);
        if (configDebugThread.joinable()) {
            configDebugThread.join();
        }
    }

    std::cout << "����ģ��������" << std::endl;
}

std::shared_ptr<HelperConfig> ConfigModule::GetConfig() {
    return config;
}

void ConfigModule::EnableConfigDebug(bool enable) {
    bool currentState = configDebugEnabled.load();

    // ���״̬û�䣬�����κβ���
    if (currentState == enable) return;

    configDebugEnabled.store(enable);

    // ������������õ���
    if (enable) {
        // �������̼���
        keyboardListener->Start();

        // ��ӡ��ǰ���ò���
        std::cout << "��������ģʽ������" << std::endl;
        std::cout << "ʹ�����¼�����������" << std::endl;
        std::cout << "Q/W: ����aimFov  - ��ǰֵ: " << config->aimFov.load() << std::endl;
        std::cout << "C: �л�ѹǹ���� - ��ǰ״̬: " << (config->isAutoRecoilEnabled.load() ? "����" : "�ر�") << std::endl;
        std::cout << "D/F: ����ѹǹ���� - ��ǰֵ: " << config->pressForce.load() << std::endl;
        std::cout << "E/R: ����ѹǹ����ʱ�� - ��ǰֵ: " << config->pressTime.load() << "ms" << std::endl;

        // �������õ����߳�
        configDebugThread = std::thread(ConfigDebugLoop);
    }
    else {
        // ֹͣ���̼���
        keyboardListener->Stop();

        // �ȴ��߳̽���
        if (configDebugThread.joinable()) {
            configDebugThread.join();
        }

        std::cout << "��������ģʽ�ѽ���" << std::endl;
    }
}

bool ConfigModule::IsConfigDebugEnabled() {
    return configDebugEnabled.load();
}

void ConfigModule::HandleKeyPress(int key) {
    float pressForce = config->pressForce.load();
    int pressTime = config->pressTime.load();
    int aimFov = config->aimFov.load();
    float predictAlpha = config->predictAlpha.load();

    const float pressForceStep = 0.5f;
    const int pressTimeStep = 20;

    switch (key) {
    case 'Q':
        aimFov = max(0, aimFov - 1);
        config->aimFov.store(aimFov);
        std::cout << "aimFov ����Ϊ: " << aimFov << std::endl;
        break;
    case 'W':
        aimFov += 1;
        config->aimFov.store(aimFov);
        std::cout << "aimFov ����Ϊ: " << aimFov << std::endl;
        break;
    case 'C': // �л�ѹǹ����
        config->isAutoRecoilEnabled = !config->isAutoRecoilEnabled.load();
        std::cout << "�Զ�ѹǹ����: " << (config->isAutoRecoilEnabled.load() ? "����" : "�ر�") << std::endl;
        break;
    case 'D': // ��Сѹǹ����
        pressForce = max(0.5f, pressForce - pressForceStep);
        config->pressForce.store(pressForce);
        std::cout << "ѹǹ���� ����Ϊ: " << pressForce << std::endl;
        break;
    case 'F': // ����ѹǹ����
        pressForce += pressForceStep;
        config->pressForce.store(pressForce);
        std::cout << "ѹǹ���� ����Ϊ: " << pressForce << std::endl;
        break;
    case 'E': // ����ѹǹ����ʱ��
        pressTime = max(100, pressTime - pressTimeStep);
        config->pressTime.store(pressTime);
        std::cout << "ѹǹ����ʱ�� ����Ϊ: " << pressTime << "ms" << std::endl;
        break;
    case 'R': // ����ѹǹ����ʱ��
        pressTime += pressTimeStep;
        config->pressTime.store(pressTime);
        std::cout << "ѹǹ����ʱ�� ����Ϊ: " << pressTime << "ms" << std::endl;
        break;
    }
}

void ConfigModule::ConfigDebugLoop() {
    while (configDebugEnabled.load()) {
        // �̶߳���˯�ߣ�����CPUʹ��
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}