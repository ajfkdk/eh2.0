#include <iostream>
#include <thread>
#include "ScreenCaptureWindows.h"
#include "DetectionModule.h"
#include "PredictionModule.h"
#include <opencv2/opencv.hpp>

int main() {
    std::thread captureThread;
    std::thread detectionThread;
    std::thread predictionThread;

    try {
        // ��ʼ���ɼ�ģ��
        captureThread = CaptureModule::Initialize();

        // ��ʼ�����ģ��
        detectionThread = DetectionModule::Initialize("./123.onnx");

        // ��ʼ��Ԥ��ģ��
        predictionThread = PredictionModule::Initialize();

        // ��ѡ�����õ���ģʽ����ʾ����
        DetectionModule::SetDebugMode(true);
        DetectionModule::SetShowDetections(false);

        // ��ѭ��
        while (CaptureModule::IsRunning() && DetectionModule::IsRunning() && PredictionModule::IsRunning()) {
            // ��ȡ���µļ����
     /*       DetectionResult detectionResult;
            if (DetectionModule::GetLatestDetectionResult(detectionResult)) {
                if (detectionResult.classId >= 0) {
                    std::cout << "Detected: " << detectionResult.className
                        << " at (" << detectionResult.x << "," << detectionResult.y << ")" << std::endl;
                }
            }*/

            // ��ȡ���µ�Ԥ����
            PredictionResult predictionResult;
            if (PredictionModule::GetLatestPrediction(predictionResult)) {
                if (predictionResult.x != 999 && predictionResult.y != 999) {
                    std::cout << "Predicted position: (" << predictionResult.x << "," << predictionResult.y << ")" << std::endl;
                }
                else {
                    std::cout << "Target lost!" << std::endl;
                }
            }

            // ��ESC���˳�
            if (cv::waitKey(1) == 27) {
                CaptureModule::Stop();
                DetectionModule::Stop();
                PredictionModule::Stop();
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

    // ������Դ
    CaptureModule::Cleanup();
    DetectionModule::Cleanup();
    PredictionModule::Cleanup();

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

    // �ͷ���Դ
    cv::destroyAllWindows();

    std::cout << "Program terminated normally" << std::endl;
    return 0;
}