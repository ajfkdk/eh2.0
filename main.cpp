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
        // 初始化采集模块
        captureThread = CaptureModule::Initialize();

        // 初始化检测模块
        detectionThread = DetectionModule::Initialize("./123.onnx");

        // 初始化预测模块
        predictionThread = PredictionModule::Initialize();

        // 可选：设置调试模式和显示检测框
        DetectionModule::SetDebugMode(true);
        DetectionModule::SetShowDetections(false);

        // 主循环
        while (CaptureModule::IsRunning() && DetectionModule::IsRunning() && PredictionModule::IsRunning()) {
            // 获取最新的检测结果
     /*       DetectionResult detectionResult;
            if (DetectionModule::GetLatestDetectionResult(detectionResult)) {
                if (detectionResult.classId >= 0) {
                    std::cout << "Detected: " << detectionResult.className
                        << " at (" << detectionResult.x << "," << detectionResult.y << ")" << std::endl;
                }
            }*/

            // 获取最新的预测结果
            PredictionResult predictionResult;
            if (PredictionModule::GetLatestPrediction(predictionResult)) {
                if (predictionResult.x != 999 && predictionResult.y != 999) {
                    std::cout << "Predicted position: (" << predictionResult.x << "," << predictionResult.y << ")" << std::endl;
                }
                else {
                    std::cout << "Target lost!" << std::endl;
                }
            }

            // 按ESC键退出
            if (cv::waitKey(1) == 27) {
                CaptureModule::Stop();
                DetectionModule::Stop();
                PredictionModule::Stop();
                break;
            }

            // 控制主循环频率
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    // 停止所有模块
    CaptureModule::Stop();
    DetectionModule::Stop();
    PredictionModule::Stop();

    // 清理资源
    CaptureModule::Cleanup();
    DetectionModule::Cleanup();
    PredictionModule::Cleanup();

    // 等待线程结束
    if (captureThread.joinable()) {
        captureThread.join();
    }

    if (detectionThread.joinable()) {
        detectionThread.join();
    }

    if (predictionThread.joinable()) {
        predictionThread.join();
    }

    // 释放资源
    cv::destroyAllWindows();

    std::cout << "Program terminated normally" << std::endl;
    return 0;
}