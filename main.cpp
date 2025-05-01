#include <iostream>
#include <thread>
#include "ScreenCaptureWindows.h"
#include "DetectionModule.h"
#include <opencv2/opencv.hpp>



int main() {
    std::thread captureThread;
    std::thread detectionThread;

    try {
        // 初始化采集模块
        captureThread = CaptureModule::Initialize();

        // 初始化检测模块
        detectionThread = DetectionModule::Initialize("./123.onnx");

        // 可选：设置调试模式和显示检测框
        DetectionModule::SetDebugMode(true);
        DetectionModule::SetShowDetections(true);

        // 主循环
        while (CaptureModule::IsRunning() && DetectionModule::IsRunning()) {
            // 获取最新的检测结果
            DetectionResult result;
            if (DetectionModule::GetLatestDetectionResult(result)) {
                if (result.classId >= 0) {
                    std::cout << "Detected: " << result.className
                        << " at (" << result.x << "," << result.y << ")" << std::endl;
                }
            }

            // 按ESC键退出
            if (cv::waitKey(1) == 27) {
                CaptureModule::Stop();
                DetectionModule::Stop();
                break;
            }

            // 控制主循环频率
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    // 清理资源
    CaptureModule::Cleanup();
    DetectionModule::Cleanup();

    // 等待线程结束
    if (captureThread.joinable()) {
        captureThread.join();
    }

    if (detectionThread.joinable()) {
        detectionThread.join();
    }

    // 释放资源
    cv::destroyAllWindows();

    std::cout << "Program terminated normally" << std::endl;
    return 0;
}