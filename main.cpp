#include <iostream>
#include <thread>
#include "ScreenCaptureWindows.h"
#include "DetectionModule.h"
#include "PredictionModule.h"
#include "ActionModule.h"
#include <opencv2/opencv.hpp>
#include "WindowsMouseController.h"
#include "HumanLikeMovement.h"

int main() {
    std::thread captureThread;
    std::thread detectionThread;
    std::thread predictionThread;
    std::thread actionThread;

    try {
        // ��ʼ���ɼ�ģ��
        captureThread = CaptureModule::Initialize();

        // ��ʼ�����ģ��
        detectionThread = DetectionModule::Initialize("./123.onnx");

        // ��ʼ��Ԥ��ģ��
        predictionThread = PredictionModule::Initialize();

        // ��ʼ������ģ�飨���˲�����Ϊ50��
        actionThread = ActionModule::Initialize(50);

        // ��ѡ�������Զ�����
        ActionModule::EnableAutoFire(false);

        // ��ѡ���ɼ�ģ��-->���õ���ģʽ����ʾ����
        CaptureModule::SetCaptureDebug(false);

        // ��ѡ�����õ���ģʽ����ʾ����
        DetectionModule::SetDebugMode(false);
        DetectionModule::SetShowDetections(true);

        // ��ѭ��
        while (CaptureModule::IsRunning() &&
            DetectionModule::IsRunning() &&
            PredictionModule::IsRunning() &&
            ActionModule::IsRunning()) {

            // ��ESC���˳�
            if (cv::waitKey(1) == 27) {
                CaptureModule::Stop();
                DetectionModule::Stop();
                PredictionModule::Stop();
                ActionModule::Stop();
                break;
            }

            // ������ѭ��Ƶ��
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    // ֹͣ����ģ��
    CaptureModule::Stop();
    DetectionModule::Stop();
    PredictionModule::Stop();
    ActionModule::Stop();

    // ������Դ
    CaptureModule::Cleanup();
    DetectionModule::Cleanup();
    PredictionModule::Cleanup();
    ActionModule::Cleanup();

    // �ȴ��߳̽���
    if (captureThread.joinable()) {
        captureThread.join();
    }

    if (detectionThread.joinable()) {
        detectionThread.join();
    }

    if (predictionThread.joinable()) {
        predictionThread.join();
    }

    if (actionThread.joinable()) {
        actionThread.join();
    }

    // �ͷ���Դ
    cv::destroyAllWindows();

    std::cout << "Program terminated normally" << std::endl;
    return 0;
}

// ��������ƶ�����
void TestMouseMovement(int targetX, int targetY, int humanizationFactor = 50) {
    std::cout << "��ʼ����ƶ�����..." << std::endl;
    std::cout << "Ŀ��λ��: (" << targetX << ", " << targetY << ")" << std::endl;
    std::cout << "���˻�����: " << humanizationFactor << std::endl;

    // ������������
    std::unique_ptr<MouseController> mouseController = std::make_unique<WindowsMouseController>();

    // ��ȡ��ǰ���λ��
    int currentX = 0, currentY = 0;
    mouseController->GetCurrentPosition(currentX, currentY);
    std::cout << "��ǰ���λ��: (" << currentX << ", " << currentY << ")" << std::endl;

    // �������
    float distance = std::sqrt(std::pow(targetX - currentX, 2) + std::pow(targetY - currentY, 2));
    std::cout << "��Ŀ��ľ���: " << distance << std::endl;

    // ���ɱ�����·��
    std::vector<std::pair<float, float>> path = HumanLikeMovement::GenerateBezierPath(
        currentX, currentY, targetX, targetY, humanizationFactor,
        max(10, static_cast<int>(distance / 5)));

    std::cout << "����·��������: " << path.size() << std::endl;

    // Ӧ���ٶ�����
    path = HumanLikeMovement::ApplySpeedProfile(path, humanizationFactor);
    std::cout << "Ӧ���ٶ����ߺ�·��������: " << path.size() << std::endl;

    // ����·���㣬Ӧ�ö������ƶ����
    for (size_t i = 0; i < path.size(); ++i) {
        // ���΢С����
        auto jitteredPoint = HumanLikeMovement::AddHumanJitter(
            path[i].first, path[i].second, distance, humanizationFactor);

        // �ƶ���굽�������λ��
        mouseController->MoveTo(
            static_cast<int>(jitteredPoint.first), static_cast<int>(jitteredPoint.second));

        // ��ӡ������Ϣ
        if (i % 5 == 0 || i == path.size() - 1) {  // ÿ5��������һ�������һ��
            std::cout << "�ƶ���: (" << jitteredPoint.first << ", " << jitteredPoint.second
                << ") [ԭʼ·����: (" << path[i].first << ", " << path[i].second << ")]" << std::endl;
        }

        // �����ƶ��ٶ�
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }

    // ��ȡ�������λ��
    mouseController->GetCurrentPosition(currentX, currentY);
    std::cout << "������ɡ��������λ��: (" << currentX << ", " << currentY << ")" << std::endl;
}

int main1() {
    int targetX, targetY, humanFactor;

    std::cout << "����Ŀ��X����: ";
    std::cin >> targetX;

    std::cout << "����Ŀ��Y����: ";
    std::cin >> targetY;

    std::cout << "�������˻�����(1-100): ";
    std::cin >> humanFactor;
    humanFactor = max(1, min(100, humanFactor));

    // ִ�в���
    TestMouseMovement(targetX, targetY, humanFactor);
    return 0;
}