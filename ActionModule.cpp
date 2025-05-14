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
std::atomic<bool> ActionModule::running(false);
std::unique_ptr<MouseController> ActionModule::mouseController = nullptr;
std::shared_ptr<SharedState> ActionModule::sharedState = std::make_shared<SharedState>();

// Ԥ����ز���
float ActionModule::predictAlpha = 0.3f; // Ĭ��ֵ��Ϊ0.3����0.2~0.5��Χ��
Point2D ActionModule::lastTargetPos = { 0, 0 }; // ��һ֡Ŀ��λ��
bool ActionModule::hasLastTarget = false; // �Ƿ�����һ֡Ŀ��λ��

std::thread ActionModule::Initialize() {
    // ���û����������������ʹ��WindowsĬ��ʵ��
    if (!mouseController) {
        // ����Windows��������������ָ��
        mouseController = std::make_unique<KmboxNetMouseController>();
    }

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
}

void ActionModule::Stop() {
    running.store(false);
}

bool ActionModule::IsRunning() {
    return running.load();
}

void ActionModule::SetMouseController(std::unique_ptr<MouseController> controller) {
    mouseController = std::move(controller);
}

// ����Ԥ��ϵ��
void ActionModule::SetPredictAlpha(float alpha) {
    if (alpha >= 0.0f && alpha <= 1.0f) {
        predictAlpha = alpha;
        std::cout << "Ԥ��ϵ��������Ϊ: " << alpha << std::endl;
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

// Ԥ��Ŀ����һ֡λ��
Point2D ActionModule::PredictNextPosition(const Point2D& current) {
    // ���û����һ֡λ�ã��޷�Ԥ�⣬ֱ�ӷ��ص�ǰλ��
    if (!hasLastTarget) {
        hasLastTarget = true;
        lastTargetPos = current;
        return current;
    }

    // ʹ�ü�����Ԥ��: predicted = current + (current - last) * alpha
    Point2D predicted;
    predicted.x = current.x + (current.x - lastTargetPos.x) * predictAlpha;
    predicted.y = current.y + (current.y - lastTargetPos.y) * predictAlpha;

    // ������һ֡λ��
    lastTargetPos = current;

    // ����Ԥ��λ��
    return predicted;
}

// �����߳� - ����������͸���Ŀ��״̬
void ActionModule::ProcessLoop() {
    bool prevSideButton1State = false; // ���ڼ����1״̬�仯
    bool prevSideButton2State = false; // ���ڼ����2״̬�仯

    // ������ѭ��
    while (running.load()) {
        // �������鿪�أ����1��
        bool currentSideButton1State = mouseController->IsSideButton1Down();
        if (currentSideButton1State && !prevSideButton1State) {
            sharedState->isAutoAimEnabled = !sharedState->isAutoAimEnabled; // �л�����״̬
            std::cout << "���鹦��: " << (sharedState->isAutoAimEnabled ? "����" : "�ر�") << std::endl;
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

            // ʹ��Ԥ���Ŀ������
            int targetX = static_cast<int>(offsetX + prediction.x);
            int targetY = static_cast<int>(offsetY + prediction.y);

            // �������Ļ���ĵ�Ŀ�����Ծ���
            float centerToTargetX = static_cast<float>(targetX - screenCenterX);
            float centerToTargetY = static_cast<float>(targetY - screenCenterY);

            // ���㳤��
            float length = std::sqrt(centerToTargetX * centerToTargetX + centerToTargetY * centerToTargetY);

            // ��Ҫ�޸ģ��ȸ���Ŀ��״̬�����۾�����Σ�ȷ����������Ҳ����ȷά��Ŀ��״̬
            sharedState->targetDistance = length;
            sharedState->hasValidTarget = true;

            // �������鹦��
            if (sharedState->isAutoAimEnabled && mouseController) {
                // ����������ֵ��ȷ����ʹĿ��ӽ�����Ҳ����΢С����
                if (length >= deadZoneThreshold && !mouseController->IsMouseMoving()) { 
                    // ��һ���ƶ�ֵ����10��Χ
                    auto normalizedMove = NormalizeMovement(centerToTargetX, centerToTargetY, 10.0f);

                    // ��ǰĿ������
                    Point2D currentTarget = { normalizedMove.first, normalizedMove.second };

                    // ���ƶ������Ͻ��м�Ԥ��
                    Point2D predictedTarget = PredictNextPosition(currentTarget);

                    // ��x��y���������ƫ�ƣ��������˻�
                    // ���ƫ��-2��2
                    predictedTarget.x += (rand() % 5 - 2);
                    predictedTarget.y += (rand() % 5 - 2);

                    // ʹ�ÿ������ƶ����(�������)
                    mouseController->MoveToWithTime(
                        static_cast<int>(predictedTarget.x),
                        static_cast<int>(predictedTarget.y),
                        length * humanizationFactor  // ����������˻�����
                    );

                    // ֪ͨԤ��ģ������ƶ��ˣ����ڲ�����겹������
                    PredictionModule::NotifyMouseMovement(normalizedMove.first, normalizedMove.second);
                }
                // ��ʹ��������Ҳ����Ŀ��״̬����Ч�ԣ�ȷ���Զ���������������
                else if (length < deadZoneThreshold) {
                    // �����ڣ���Ŀ����Ȼ��Ч����������ƶ�
                    sharedState->hasValidTarget = true;
                    sharedState->targetDistance = length;
                }
            }
        }
        else {
            // û����ЧĿ��
            sharedState->hasValidTarget = false;
            sharedState->targetDistance = 999.0f;
            hasLastTarget = false; // ����Ԥ��״̬
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
        Sleep(5);
    }

    // ȷ���˳�ʱ�ͷ���갴��
    if (mouseController->IsLeftButtonDown()) {
        mouseController->LeftUp();
    }
}