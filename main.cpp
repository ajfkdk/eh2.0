#include <iostream>
#include <thread>
#include "ScreenCaptureWindows.h"
#include "DetectionModule.h"
#include <opencv2/opencv.hpp>



int main() {
    std::thread captureThread;
    std::thread detectionThread;

    try {
        // ��ʼ���ɼ�ģ��
        captureThread = CaptureModule::Initialize();

        // ��ʼ�����ģ��
        detectionThread = DetectionModule::Initialize("./123.onnx");

        // ��ѡ�����õ���ģʽ����ʾ����
        DetectionModule::SetDebugMode(true);
        DetectionModule::SetShowDetections(true);

        // ��ѭ��
        while (CaptureModule::IsRunning() && DetectionModule::IsRunning()) {
            // ��ȡ���µļ����
            DetectionResult result;
            if (DetectionModule::GetLatestDetectionResult(result)) {
                if (result.classId >= 0) {
                    std::cout << "Detected: " << result.className
                        << " at (" << result.x << "," << result.y << ")" << std::endl;
                }
            }

            // ��ESC���˳�
            if (cv::waitKey(1) == 27) {
                CaptureModule::Stop();
                DetectionModule::Stop();
                break;
            }

            // ������ѭ��Ƶ��
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    // ������Դ
    CaptureModule::Cleanup();
    DetectionModule::Cleanup();

    // �ȴ��߳̽���
    if (captureThread.joinable()) {
        captureThread.join();
    }

    if (detectionThread.joinable()) {
        detectionThread.join();
    }

    // �ͷ���Դ
    cv::destroyAllWindows();

    std::cout << "Program terminated normally" << std::endl;
    return 0;
}