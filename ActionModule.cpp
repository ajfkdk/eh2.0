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
    // ������ѭ��
    while (running.load()) {
        // ֻ�е������1����ʱ�Ž�������ƶ�����
        if (mouseController && mouseController->IsSideButton1Down()) {
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

                std::cout << "��Ļ�ֱ���: " << screenWidth << "x" << screenHeight << std::endl;
                std::cout << "ԭʼԤ��λ��: (" << prediction.x << ", " << prediction.y << ")" << std::endl;
                std::cout << "ת�����Ŀ��λ��: (" << targetX << ", " << targetY << ")" << std::endl;

                // �������Ļ���ĵ�Ŀ�����Ծ���
                float centerToTargetX = static_cast<float>(targetX - screenCenterX);
                float centerToTargetY = static_cast<float>(targetY - screenCenterY);

                // �����ܾ���(���ڵ������)
                float distance = std::sqrt(centerToTargetX * centerToTargetX + centerToTargetY * centerToTargetY);

                // ��һ���ƶ�ֵ����10��Χ
                auto normalizedMove = NormalizeMovement(centerToTargetX, centerToTargetY, 10.0f);

                // ʹ��KMBOX�������ƶ����(�������)
                mouseController->MoveRelative(
                    static_cast<int>(normalizedMove.first),
                    static_cast<int>(normalizedMove.second));

                std::cout << "�ƶ����(�������): ("
                    << normalizedMove.first << ", " << normalizedMove.second
                    << "), ԭʼ����: " << distance << std::endl;
            }
        }

        // ����ѭ��Ƶ�ʣ�ÿ2msִ��һ��
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}