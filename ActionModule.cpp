#include "ActionModule.h"
#include <iostream>
#include <cmath>
#include <Windows.h>
#include <random>
#include "PredictionModule.h"
#include "KmboxNetMouseController.h"

// ��̬��Ա��ʼ��
std::thread ActionModule::actionThread;
std::thread ActionModule::fireThread;
std::thread ActionModule::pidDebugThread;
std::atomic<bool> ActionModule::running(false);
std::atomic<bool> ActionModule::pidDebugEnabled(false);
std::unique_ptr<MouseController> ActionModule::mouseController = nullptr;
std::shared_ptr<SharedState> ActionModule::sharedState = std::make_shared<SharedState>();
PIDController ActionModule::pidController;
std::unique_ptr<KeyboardListener> ActionModule::keyboardListener = nullptr;

std::thread ActionModule::Initialize() {
    // ���û����������������ʹ��WindowsĬ��ʵ��
    if (!mouseController) {
        // ����Windows��������������ָ��
        mouseController = std::make_unique<KmboxNetMouseController>();
    }

    // �������̼�����
    keyboardListener = std::make_unique<KeyboardListener>();
    keyboardListener->onKeyPress = HandleKeyPress;

    // ����������
    if (mouseController) {
        mouseController->StartMonitor();
    }

    // �������б�־
    running.store(true);

    // �������鴦���߳�
    actionThread = std::thread(ProcessLoop);

    // ������������߳�
    fireThread = std::thread(FireControlLoop);

    // �������̶߳��� (ʹ��std::moveת������Ȩ)
    return std::move(actionThread);
}

void ActionModule::Cleanup() {
    // ֹͣģ��
    Stop();

    // �ȴ��߳̽���
    if (actionThread.joinable()) {
        actionThread.join();
    }

    if (fireThread.joinable()) {
        fireThread.join();
    }

    if (pidDebugThread.joinable()) {
        pidDebugThread.join();
    }

    // ֹͣ���̼���
    if (keyboardListener) {
        keyboardListener->Stop();
    }
}

void ActionModule::Stop() {
    running.store(false);
    pidDebugEnabled.store(false);
}

bool ActionModule::IsRunning() {
    return running.load();
}

void ActionModule::SetMouseController(std::unique_ptr<MouseController> controller) {
    mouseController = std::move(controller);
}

void ActionModule::EnablePIDDebug(bool enable) {
    bool currentState = pidDebugEnabled.load();

    // ���״̬û�䣬�����κβ���
    if (currentState == enable) return;

    pidDebugEnabled.store(enable);

    // ���������PID����
    if (enable) {
        // �������̼���
        keyboardListener->Start();

        // ��ӡ��ǰPID����
        std::cout << "PID����ģʽ������" << std::endl;
        std::cout << "ʹ�����¼�����PID������" << std::endl;
        std::cout << "Q/W: ����Kp (����ϵ��) - ��ǰֵ: " << pidController.kp.load() << std::endl;
        std::cout << "A/S: ����Ki (����ϵ��) - ��ǰֵ: " << pidController.ki.load() << std::endl;
        std::cout << "Z/X: ����Kd (΢��ϵ��) - ��ǰֵ: " << pidController.kd.load() << std::endl;

        // ����PID�����߳�
        pidDebugThread = std::thread(PIDDebugLoop);
    }
    else {
        // ֹͣ���̼���
        keyboardListener->Stop();

        std::cout << "PID����ģʽ�ѽ���" << std::endl;
    }
}

bool ActionModule::IsPIDDebugEnabled() {
    return pidDebugEnabled.load();
}

void ActionModule::HandleKeyPress(int key) {
    float kp = pidController.kp.load();
    float ki = pidController.ki.load();
    float kd = pidController.kd.load();

    const float pStep = 0.05f;
    const float iStep = 0.01f;
    const float dStep = 0.01f;

    switch (key) {
    case 'Q':
        kp = std::max(0.0f, kp - pStep);
        pidController.kp.store(kp);
        std::cout << "Kp ����Ϊ: " << kp << std::endl;
        break;
    case 'W':
        kp += pStep;
        pidController.kp.store(kp);
        std::cout << "Kp ����Ϊ: " << kp << std::endl;
        break;
    case 'A':
        ki = std::max(0.0f, ki - iStep);
        pidController.ki.store(ki);
        std::cout << "Ki ����Ϊ: " << ki << std::endl;
        break;
    case 'S':
        ki += iStep;
        pidController.ki.store(ki);
        std::cout << "Ki ����Ϊ: " << ki << std::endl;
        break;
    case 'Z':
        kd = std::max(0.0f, kd - dStep);
        pidController.kd.store(kd);
        std::cout << "Kd ����Ϊ: " << kd << std::endl;
        break;
    case 'X':
        kd += dStep;
        pidController.kd.store(kd);
        std::cout << "Kd ����Ϊ: " << kd << std::endl;
        break;
    }
}

void ActionModule::PIDDebugLoop() {
    while (running.load() && pidDebugEnabled.load()) {
        // �̶߳���˯�ߣ�����CPUʹ��
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ��һ���ƶ�ֵ��ָ����Χ
std::pair<float, float> ActionModule::NormalizeMovement(float x, float y, float maxValue) {
    // ������������
    float length = std::sqrt(x * x + y * y);

    // �������Ϊ0������(0,0)
    if (length < 0.001f) {
        return { 0.0f, 0.0f };
    }

    // �����һ������
    float factor = min(length, maxValue) / length;

    // ���ع�һ�����ֵ
    return { x * factor, y * factor };
}

// PID�����㷨
std::pair<float, float> ActionModule::ApplyPIDControl(float errorX, float errorY) {
    auto currentTime = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(currentTime - pidController.lastTime).count();
    pidController.lastTime = currentTime;

    // ��ֹdt��С��Ϊ�㵼�¼�������
    if (dt < 0.001f) dt = 0.001f;

    // ��ȡPID����
    float kp = pidController.kp.load();
    float ki = pidController.ki.load();
    float kd = pidController.kd.load();

    // ����΢����
    float derivativeX = (errorX - pidController.previousErrorX) / dt;
    float derivativeY = (errorY - pidController.previousErrorY) / dt;

    // ���»����� (�����ƣ���ֹ���ֱ���)
    pidController.integralX += errorX * dt;
    pidController.integralY += errorY * dt;

    // ��������
    pidController.integralX = std::max(-pidController.integralLimit,
        std::min(pidController.integralX, pidController.integralLimit));
    pidController.integralY = std::max(-pidController.integralLimit,
        std::min(pidController.integralY, pidController.integralLimit));

    // ����PID���
    float outputX = kp * errorX + ki * pidController.integralX + kd * derivativeX;
    float outputY = kp * errorY + ki * pidController.integralY + kd * derivativeY;

    // ���浱ǰ���´�ʹ��
    pidController.previousErrorX = errorX;
    pidController.previousErrorY = errorY;

    return { outputX, outputY };
}

// �����߳� - ����������͸���Ŀ��״̬
void ActionModule::ProcessLoop() {
    bool prevSideButton1State = false; // ���ڼ����1״̬�仯
    bool prevSideButton2State = false; // ���ڼ����2״̬�仯

    // ����PID������
    pidController.Reset();

    // ������ѭ��
    while (running.load()) {
        // �������鿪�أ����1��
        bool currentSideButton1State = mouseController->IsSideButton1Down();
        if (currentSideButton1State && !prevSideButton1State) {
            sharedState->isAutoAimEnabled = !sharedState->isAutoAimEnabled; // �л�����״̬
            std::cout << "���鹦��: " << (sharedState->isAutoAimEnabled ? "����" : "�ر�") << std::endl;

            // ÿ�ο�������ʱ����PID������
            pidController.Reset();
        }
        prevSideButton1State = currentSideButton1State;

        // �����Զ����𿪹أ����2��
        bool currentSideButton2State = mouseController->IsSideButton2Down();
        if (currentSideButton2State && !prevSideButton2State) {
            bool newState = !sharedState->isAutoFireEnabled.load();
            sharedState->isAutoFireEnabled = newState; // �л��Զ�����״̬

            std::cout << "�Զ�������: " << (newState ? "����" : "�ر�") << std::endl;
        }
        prevSideButton2State = currentSideButton2State;

        // ��ȡ����Ԥ����
        PredictionResult prediction;
        bool hasPrediction = PredictionModule::GetLatestPrediction(prediction);

        // �������ЧԤ����
        if (hasPrediction && prediction.x != 999 && prediction.y != 999) {
            // ��ȡ��Ļ�ֱ���
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);

            // ������Ļ���ĵ�
            int screenCenterX = screenWidth / 2;
            int screenCenterY = screenHeight / 2;

            // ͼ�����ĵ����Ͻ�ƫ�Ƶ���Ļ���ĵ�ͼ�����Ͻ�
            float offsetX = screenCenterX - 320.0f / 2;
            float offsetY = screenCenterY - 320.0f / 2;

            // ����Ŀ������
            int targetX = static_cast<int>(offsetX + prediction.x);
            int targetY = static_cast<int>(offsetY + prediction.y);

            // �������Ļ���ĵ�Ŀ�����Ծ���
            float centerToTargetX = static_cast<float>(targetX - screenCenterX);
            float centerToTargetY = static_cast<float>(targetY - screenCenterY);

            // ���㳤��
            float length = std::sqrt(centerToTargetX * centerToTargetX + centerToTargetY * centerToTargetY);

            // ����Ŀ����뵽����״̬
            sharedState->targetDistance = length;
            sharedState->hasValidTarget = true;

            // �������鹦��
            if (sharedState->isAutoAimEnabled && mouseController && !mouseController->IsMouseMoving()) {
                if (length >= 7.0f) {
                    // ʹ��PID�����������ƶ�ֵ
                    auto pidOutput = ApplyPIDControl(centerToTargetX, centerToTargetY);

                    // ��һ���ƶ�ֵ����10��Χ
                    auto normalizedMove = NormalizeMovement(pidOutput.first, pidOutput.second, 10.0f);

                    // ʹ�ÿ������ƶ����(�������)
                    mouseController->MoveTo(
                        static_cast<int>(normalizedMove.first),
                        static_cast<int>(normalizedMove.second));

                    // ��ӡ������Ϣ
                    if (pidDebugEnabled.load()) {
                        std::cout << "Error:" << centerToTargetX << "," << centerToTargetY
                            << " PID:" << pidOutput.first << "," << pidOutput.second
                            << " Move:" << normalizedMove.first << "," << normalizedMove.second << std::endl;
                    }
                }
            }
            else if (!sharedState->isAutoAimEnabled) {
                // ������ر�ʱ����PID������������������ۻ�
                pidController.Reset();
            }
        }
        else {
            // û����ЧĿ��
            sharedState->hasValidTarget = false;
            sharedState->targetDistance = 999.0f;

            // ����PID������
            pidController.Reset();
        }
    }
}

// ��������߳� - ������ƿ�����Ϊ
void ActionModule::FireControlLoop() {
    // ����״̬��״̬
    enum class FireState {
        IDLE,           // ����״̬
        BURST_ACTIVE,   // ���伤��״̬
        BURST_COOLDOWN  // ������ȴ״̬
    };

    FireState currentState = FireState::IDLE;

    // �����������
    std::random_device rd;
    std::mt19937 gen(rd());

    // �������ʱ��ֲ� (80ms-150ms)
    std::uniform_int_distribution<> burstDuration(80, 150);

    // ������ȴʱ��ֲ� (180ms-400ms)
    std::uniform_int_distribution<> burstCooldown(180, 400);

    // ��������ͣ�� (20ms-50ms)
    std::uniform_int_distribution<> microPause(20, 50);

    auto lastStateChangeTime = std::chrono::steady_clock::now();
    int currentDuration = 0;

    while (running.load()) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastStateChangeTime).count();

        // ��ȡ��ǰ����״̬
        bool autoFireEnabled = sharedState->isAutoFireEnabled.load();
        bool hasValidTarget = sharedState->hasValidTarget.load();
        float targetDistance = sharedState->targetDistance.load();

        // ����Զ�����رգ�ȷ������ͷŲ�����״̬
        if (!autoFireEnabled) {
            if (mouseController->IsLeftButtonDown()) {
                mouseController->LeftUp();
            }
            currentState = FireState::IDLE;
            Sleep(50); // ����CPUʹ��
            continue;
        }

        // ״̬���߼�
        switch (currentState) {
        case FireState::IDLE:
            // �ӿ���״̬תΪ����״̬������
            if (autoFireEnabled && hasValidTarget && targetDistance < 10.0f) {
                mouseController->LeftDown();
                currentState = FireState::BURST_ACTIVE;
                currentDuration = burstDuration(gen); // ����������ʱ��
                lastStateChangeTime = currentTime;
                std::cout << "��ʼ���䣬����: " << currentDuration << "ms" << std::endl;
            }
            else {
                Sleep(10); // ����CPUʹ��
            }
            break;

        case FireState::BURST_ACTIVE:
            // �������ʱ�����
            if (elapsedTime >= currentDuration) {
                mouseController->LeftUp();
                currentState = FireState::BURST_COOLDOWN;
                currentDuration = burstCooldown(gen); // �����ȴʱ��
                lastStateChangeTime = currentTime;
                std::cout << "�����������ȴ: " << currentDuration << "ms" << std::endl;
            }
            // ���Ŀ����Ч������Զ������ֹͣ����
            else if (!hasValidTarget || targetDistance >= 10.0f) {
                mouseController->LeftUp();
                currentState = FireState::IDLE;
                std::cout << "Ŀ�궪ʧ��ֹͣ����" << std::endl;
            }
            break;

        case FireState::BURST_COOLDOWN:
            // ��ȴʱ�����
            if (elapsedTime >= currentDuration) {
                // ���Ŀ����Ȼ��Ч���ڷ�Χ�ڣ���ʼ��һ�ֵ���
                if (hasValidTarget && targetDistance < 10.0f) {
                    // ��С���ʲ������ͣ�٣�����������
                    if (gen() % 3 == 0) { // Լ33%�ĸ���
                        Sleep(microPause(gen));
                    }

                    mouseController->LeftDown();
                    currentState = FireState::BURST_ACTIVE;
                    currentDuration = burstDuration(gen);
                    lastStateChangeTime = currentTime;
                    std::cout << "��ʼ�µ��䣬����: " << currentDuration << "ms" << std::endl;
                }
                else {
                    currentState = FireState::IDLE;
                }
            }
            break;
        }

        // ����˯�ߣ�����CPUʹ����
        Sleep(1);
    }

    // ȷ���˳�ʱ�ͷ���갴��
    if (mouseController->IsLeftButtonDown()) {
        mouseController->LeftUp();
    }
}