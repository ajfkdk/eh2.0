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
        // 初始化采集模块
        captureThread = CaptureModule::Initialize();

        // 初始化检测模块
        detectionThread = DetectionModule::Initialize("./123.onnx");

        // 初始化预测模块
        predictionThread = PredictionModule::Initialize();

        // 初始化动作模块 - 使用WindowsAPI控制器，自动检测瞄准策略，拟人化强度50
        actionThread = ActionModule::ActionModule::Initialize(
            ActionModule::ControllerType::WINDOWS_API,
            ActionModule::StrategyType::AUTO_DETECT,
            50);

        // 可选：设置调试模式和显示检测框
        DetectionModule::SetDebugMode(true);
        DetectionModule::SetShowDetections(false);

        // 主循环
        while (CaptureModule::IsRunning() &&
            DetectionModule::IsRunning() &&
            PredictionModule::IsRunning() &&
            ActionModule::ActionModule::IsRunning()) {

            // 按ESC键退出
            if (cv::waitKey(1) == 27) {
                CaptureModule::Stop();
                DetectionModule::Stop();
                PredictionModule::Stop();
                ActionModule::ActionModule::Stop();
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
    ActionModule::ActionModule::Stop();

    // 清理资源
    CaptureModule::Cleanup();
    DetectionModule::Cleanup();
    PredictionModule::Cleanup();
    ActionModule::ActionModule::Cleanup();

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

    if (actionThread.joinable()) {
        actionThread.join();
    }

    // 释放资源
    cv::destroyAllWindows();

    std::cout << "Program terminated normally" << std::endl;
    return 0;
}