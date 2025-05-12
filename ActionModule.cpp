#include "ActionModule.h"
#include <iostream>
#include <cmath>
#include <Windows.h>
#include "PredictionModule.h"
#include "KmboxNetMouseController.h"

// ��̬��Ա��ʼ��
std::thread ActionModule::actionThread;
std::atomic<bool> ActionModule::running(false);
std::unique_ptr<MouseController> ActionModule::mouseController = nullptr;

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

    // ���������߳�
    actionThread = std::thread(ProcessLoop);

    // �����̶߳��� (ʹ��std::moveת������Ȩ)
    return std::move(actionThread);
}

void ActionModule::Cleanup() {
    // ֹͣģ��
    Stop();

    // �ȴ��߳̽���
    if (actionThread.joinable()) {
        actionThread.join();
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

// ����ģ����Ҫ�Ĵ���ѭ��
void ActionModule::ProcessLoop() {
    bool isAutoAimEnabled = false;  // ���鹦�ܿ���
    bool isAutoFireEnabled = false; // �Զ������ܿ���

    bool prevSideButton1State = false; // ���ڼ����1״̬�仯
    bool prevSideButton2State = false; // ���ڼ����2״̬�仯

    auto lastFireTime = std::chrono::steady_clock::now();
    bool fireState = false; // false��ʾ������true��ʾ����

    // ������ѭ��
    while (running.load()) {
        // �������鿪�أ����1��
        bool currentSideButton1State = mouseController->IsSideButton1Down();
        if (currentSideButton1State && !prevSideButton1State) {
            isAutoAimEnabled = !isAutoAimEnabled; // �л�����״̬
            std::cout << "���鹦��: " << (isAutoAimEnabled ? "����" : "�ر�") << std::endl;
        }
        prevSideButton1State = currentSideButton1State;

        // �����Զ����𿪹أ����2��
        bool currentSideButton2State = mouseController->IsSideButton2Down();
        if (currentSideButton2State && !prevSideButton2State) {
            isAutoFireEnabled = !isAutoFireEnabled; // �л��Զ�����״̬

            // ����ر��Զ�����ȷ���������ͷ�
            if (!isAutoFireEnabled && mouseController->IsLeftButtonDown()) {
                mouseController->LeftUp();
                fireState = false;
            }

            std::cout << "�Զ�������: " << (isAutoFireEnabled ? "����" : "�ر�") << std::endl;
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

            // �������鹦��
            if (isAutoAimEnabled && mouseController && !mouseController->IsMouseMoving()) {
                if (length >= 7.0f) {
                    // ��һ���ƶ�ֵ����10��Χ
                    auto normalizedMove = NormalizeMovement(centerToTargetX, centerToTargetY, 10.0f);

                    // ʹ�ÿ������ƶ����(�������)
                    mouseController->MoveTo(
                        static_cast<int>(normalizedMove.first),
                        static_cast<int>(normalizedMove.second));

                    // ��ӡ������Ϣ
                    std::cout << "centerToTarget:" << centerToTargetX << ", " << centerToTargetY
                        << " ---> normal:" << normalizedMove.first << ", " << normalizedMove.second << std::endl;
                }
            }

            // �����Զ�������
            if (isAutoFireEnabled) {
                // ��Ŀ����׼�Ǿ���С��10����ʱ�������Զ�����
                if (length < 10.0f) {
                    auto currentTime = std::chrono::steady_clock::now();
                    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                        currentTime - lastFireTime).count();

                    if (fireState) { // ��ǰ�ڿ���״̬
                        if (elapsedTime >= 200) { // ����200ms��
                            mouseController->LeftUp(); // �ͷ����
                            lastFireTime = currentTime;
                            fireState = false; // �л�����Ϣ״̬
                        }
                    }
                    else { // ��ǰ����Ϣ״̬
                        if (elapsedTime >= 200) { // ��Ϣ200ms��
                            mouseController->LeftDown(); // �������
                            lastFireTime = currentTime;
                            fireState = true; // �л�������״̬
                        }
                    }
                }
                else if (mouseController->IsLeftButtonDown()) {
                    mouseController->LeftUp(); // ���Ŀ������Զ������Ѱ��£��ͷ���
                    fireState = false;
                }
            }
        }
        else if (isAutoFireEnabled && mouseController->IsLeftButtonDown()) {
            // ���û����ЧĿ�굫����Ѱ��£��ͷ���
            mouseController->LeftUp();
            fireState = false;
        }

    }
}