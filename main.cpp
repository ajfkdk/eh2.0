#include <iostream>
#include <thread>
#include "ScreenCaptureWindows.h"
#include "DetectionModule.h"
#include "PredictionModule.h"
#include "ActionModule.h"
#include <opencv2/opencv.hpp>

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
        CaptureModule::SetCaptureDebug(true);

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