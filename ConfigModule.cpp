#include "ConfigModule.h"
#include <iostream>
#include <algorithm>

// ��̬��Ա��ʼ��
std::unique_ptr<KeyboardListener> ConfigModule::keyboardListener = nullptr;
std::shared_ptr<HelperConfig> ConfigModule::config = std::make_shared<HelperConfig>();
std::atomic<bool> ConfigModule::configDebugEnabled(false);
std::thread ConfigModule::configDebugThread;

void ConfigModule::Initialize() {
    keyboardListener = std::make_unique<KeyboardListener>();
    keyboardListener->onKeyPress = HandleKeyPress;
    std::cout << "����ģ���ѳ�ʼ��" << std::endl;
}

void ConfigModule::Cleanup() {
    if (keyboardListener) {
        keyboardListener->Stop();
    }

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

    if (currentState == enable) return;

    configDebugEnabled.store(enable);

    if (enable) {
        keyboardListener->Start();

        std::cout << "��������ģʽ������" << std::endl;
        std::cout << "ʹ�����¼�����������" << std::endl;
        std::cout << "Q/W: ����aimFov  - ��ǰֵ: " << config->aimFov.load() << std::endl;
        std::cout << "C: �л�ѹǹ���� - ��ǰ״̬: " << (config->isAutoRecoilEnabled.load() ? "����" : "�ر�") << std::endl;
        std::cout << "D/F: ����ѹǹ���� - ��ǰֵ: " << config->pressForce.load() << std::endl;
        std::cout << "E/R: ����ѹǹ����ʱ�� - ��ǰֵ: " << config->pressTime.load() << "ms" << std::endl;
        std::cout << "T: �л���ʾ���� - ��ǰ״̬: " << (config->showDetections.load() ? "����" : "�ر�") << std::endl;
        std::cout << "Y: �л���ʾԤ�� - ��ǰ״̬: " << (config->showPredict.load() ? "����" : "�ر�") << std::endl;
        std::cout << "U/I: �������Ŷ���ֵ - ��ǰֵ: " << config->confidenceThreshold.load() << std::endl;
        std::cout << std::endl;

        configDebugThread = std::thread(ConfigDebugLoop);
    }
    else {
        keyboardListener->Stop();
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
    float confidenceThreshold = config->confidenceThreshold.load();

    const float pressForceStep = 0.5f;
    const int pressTimeStep = 20;
    const float confidenceStep = 0.05f;

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
    case 'C':
        config->isAutoRecoilEnabled = !config->isAutoRecoilEnabled.load();
        std::cout << "�Զ�ѹǹ����: " << (config->isAutoRecoilEnabled.load() ? "����" : "�ر�") << std::endl;
        break;
    case 'D':
        pressForce = max(0.5f, pressForce - pressForceStep);
        config->pressForce.store(pressForce);
        std::cout << "ѹǹ���� ����Ϊ: " << pressForce << std::endl;
        break;
    case 'F':
        pressForce += pressForceStep;
        config->pressForce.store(pressForce);
        std::cout << "ѹǹ���� ����Ϊ: " << pressForce << std::endl;
        break;
    case 'E':
        pressTime = max(100, pressTime - pressTimeStep);
        config->pressTime.store(pressTime);
        std::cout << "ѹǹ����ʱ�� ����Ϊ: " << pressTime << "ms" << std::endl;
        break;
    case 'R':
        pressTime += pressTimeStep;
        config->pressTime.store(pressTime);
        std::cout << "ѹǹ����ʱ�� ����Ϊ: " << pressTime << "ms" << std::endl;
        break;
    case 'T':
        config->showDetections = !config->showDetections.load();
        std::cout << "��ʾ����: " << (config->showDetections.load() ? "����" : "�ر�") << std::endl;
        break;
    case 'Y':
        config->showPredict = !config->showPredict.load();
        std::cout << "��ʾԤ��: " << (config->showPredict.load() ? "����" : "�ر�") << std::endl;
        break;
    case 'U':
        confidenceThreshold = max(0.1f, confidenceThreshold - confidenceStep);
        config->confidenceThreshold.store(confidenceThreshold);
        std::cout << "���Ŷ���ֵ ����Ϊ: " << confidenceThreshold << std::endl;
        break;
    case 'I':
        confidenceThreshold = min(0.95f, confidenceThreshold + confidenceStep);
        config->confidenceThreshold.store(confidenceThreshold);
        std::cout << "���Ŷ���ֵ ����Ϊ: " << confidenceThreshold << std::endl;
        break;
    }
}

void ConfigModule::ConfigDebugLoop() {
    while (configDebugEnabled.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

std::vector<int> ConfigModule::GetTargetClasses() {
    std::lock_guard<std::mutex> lock(config->targetClassesMutex);
    return config->targetClasses;
}

void ConfigModule::SetTargetClasses(const std::vector<int>& classes) {
    std::lock_guard<std::mutex> lock(config->targetClassesMutex);
    config->targetClasses = classes;
}

float ConfigModule::GetConfidenceThreshold() {
    return config->confidenceThreshold.load();
}

void ConfigModule::SetConfidenceThreshold(float threshold) {
    config->confidenceThreshold.store(threshold);
}

bool ConfigModule::GetShowDetections() {
    return config->showDetections.load();
}

void ConfigModule::SetShowDetections(bool show) {
    config->showDetections.store(show);
}

bool ConfigModule::GetShowPredict() {
    return config->showPredict.load();
}

void ConfigModule::SetShowPredict(bool show) {
    config->showPredict.store(show);
}