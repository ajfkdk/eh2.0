#include "ActionModule.h"
#include "HumanLikeMovement.h"
#include "KmboxNetMouseController.h"
#include <iostream>
#include <cmath>
#include <Windows.h>
#include "PredictionModule.h"

// ��̬��Ա��ʼ��
std::thread ActionModule::actionThread;
std::atomic<bool> ActionModule::running(false);
std::unique_ptr<MouseController> ActionModule::mouseController = nullptr;

std::thread ActionModule::Initialize() {
    // ��ʼ��HumanLikeMovement
    HumanLikeMovement::Initialize();

    // ���û����������������Ĭ��ʹ��KmboxNetMouseController
    if (!mouseController) {
       
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

// ����ģ����Ҫ�Ĵ���ѭ��
void ActionModule::ProcessLoop() {
    // �������ڻ���·����ı���
    std::vector<std::pair<float, float>> currentPath;
    size_t pathIndex = 0;

    // ������ѭ��
    while (running.load()) {
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

      /*      std::cout << "��Ļ�ֱ���: " << screenWidth << "x" << screenHeight << std::endl;
            std::cout << "ԭʼԤ��λ��: (" << prediction.x << ", " << prediction.y << ")" << std::endl;
            std::cout << "ת�����Ŀ��λ��: (" << targetX << ", " << targetY << ")" << std::endl;*/

            // �������Ļ���ĵ�Ŀ�����Ծ���
            int centerToTargetX = targetX - screenCenterX;
            int centerToTargetY = targetY - screenCenterY;

            float distance = std::sqrt(
                std::pow(centerToTargetX, 2) + std::pow(centerToTargetY, 2));

            // ��������㹻�󣬻��ߵ�ǰ·���Ѿ�ִ����ϣ�������·��
            if (distance > 5 || currentPath.empty() || pathIndex >= currentPath.size()) {
                // ʹ�ñ�������������·����
                // ע�⣺�������Ļ����(0,0)��Ŀ������λ������·��
                currentPath = HumanLikeMovement::GenerateBezierPath(
                    0, 0, centerToTargetX, centerToTargetY, 50,
                    max(10, static_cast<int>(distance / 5)));

                pathIndex = 0;

                std::cout << "��·�����ɣ���(0,0)�����Ŀ��: ("
                    << centerToTargetX << ", " << centerToTargetY
                    << "), ����: " << distance << std::endl;
            }

            // �����·���㣬�ƶ�����һ����
            while (pathIndex < currentPath.size()) {
                auto nextPoint = currentPath[pathIndex];

                // ʹ��KMBOX�������ƶ����(�������)
                mouseController->MoveRelative(
                    static_cast<int>(nextPoint.first),
                    static_cast<int>(nextPoint.second));

                std::cout << "�ƶ����(�������): ("
                    << nextPoint.first << ", " << nextPoint.second << ")" << std::endl;

                pathIndex++;

                // ��һС��·���������ѭ�����´μ�����
                if (pathIndex > 10) {
                    break;
                }
            }
        }

        // ����ѭ��Ƶ�ʣ�ÿ5msִ��һ��
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}