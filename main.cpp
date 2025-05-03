#include <iostream>
#include <thread>
#include "ScreenCaptureWindows.h"
#include "DetectionModule.h"
#include "PredictionModule.h"
#include "ActionModule.h"
#include <opencv2/opencv.hpp>
#include "WindowsMouseController.h"
#include "HumanLikeMovement.h"

// 在main.cpp中添加
void TestUdpStream() {
    std::cout << "开始测试UDP流接收..." << std::endl;

    // 尝试不同的UDP地址格式
    std::vector<std::string> addressFormats = {
        "udp://@:8848",
        "udp://0.0.0.0:8848",
        "udp://:8848",
        "udp://127.0.0.1:8848"
    };

    for (const auto& address : addressFormats) {
        std::cout << "尝试地址: " << address << std::endl;

        cv::VideoCapture cap;
        cap.open(address, cv::CAP_FFMPEG);

        if (!cap.isOpened()) {
            std::cout << "  - 打开失败" << std::endl;
            continue;
        }

        std::cout << "  - 打开成功，开始读取帧" << std::endl;

        // 尝试读取10帧或直到失败
        int frameCount = 0;
        bool readSuccess = false;

        for (int i = 0; i < 10; i++) {
            cv::Mat frame;
            if (cap.read(frame)) {
                frameCount++;
                readSuccess = true;

                // 显示帧
                cv::imshow("UDP Test", frame);
                cv::waitKey(1);
            }
            else {
                std::cout << "  - 读取第" << (i + 1) << "帧失败" << std::endl;
                break;
            }

            // 短暂延迟
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        cap.release();

        if (readSuccess) {
            std::cout << "  - 成功读取了" << frameCount << "帧" << std::endl;
            std::cout << "UDP流测试成功，使用地址: " << address << std::endl;

            // 等待用户确认
            std::cout << "按任意键继续..." << std::endl;
            cv::waitKey(0);
            cv::destroyWindow("UDP Test");
            return;
        }
    }

    std::cout << "所有UDP地址格式均测试失败!" << std::endl;
}


int main1() {
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

        // 初始化动作模块（拟人参数设为50）
        actionThread = ActionModule::Initialize(50);

        // 可选：启用自动开火
        ActionModule::EnableAutoFire(false);

        // 可选：采集模块-->设置调试模式和显示检测框
        CaptureModule::SetCaptureDebug(false);

        // 可选：设置调试模式和显示检测框
        DetectionModule::SetDebugMode(false);
        DetectionModule::SetShowDetections(true);

        // 主循环
        while (CaptureModule::IsRunning() &&
            DetectionModule::IsRunning() &&
            PredictionModule::IsRunning() &&
            ActionModule::IsRunning()) {

            // 按ESC键退出
            if (cv::waitKey(1) == 27) {
                CaptureModule::Stop();
                DetectionModule::Stop();
                PredictionModule::Stop();
                ActionModule::Stop();
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
    ActionModule::Stop();

    // 清理资源
    CaptureModule::Cleanup();
    DetectionModule::Cleanup();
    PredictionModule::Cleanup();
    ActionModule::Cleanup();

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

// 测试鼠标移动功能
void TestMouseMovement(int targetX, int targetY, int humanizationFactor = 50) {
    std::cout << "开始鼠标移动测试..." << std::endl;
    std::cout << "目标位置: (" << targetX << ", " << targetY << ")" << std::endl;
    std::cout << "拟人化因子: " << humanizationFactor << std::endl;

    // 创建鼠标控制器
    std::unique_ptr<MouseController> mouseController = std::make_unique<WindowsMouseController>();

    // 获取当前鼠标位置
    int currentX = 0, currentY = 0;
    mouseController->GetCurrentPosition(currentX, currentY);
    std::cout << "当前鼠标位置: (" << currentX << ", " << currentY << ")" << std::endl;

    // 计算距离
    float distance = std::sqrt(std::pow(targetX - currentX, 2) + std::pow(targetY - currentY, 2));
    std::cout << "到目标的距离: " << distance << std::endl;

    // 生成贝塞尔路径
    std::vector<std::pair<float, float>> path = HumanLikeMovement::GenerateBezierPath(
        currentX, currentY, targetX, targetY, humanizationFactor,
        max(10, static_cast<int>(distance / 5)));

    std::cout << "生成路径点数量: " << path.size() << std::endl;

    // 应用速度曲线
    path = HumanLikeMovement::ApplySpeedProfile(path, humanizationFactor);
    std::cout << "应用速度曲线后路径点数量: " << path.size() << std::endl;

    // 遍历路径点，应用抖动并移动鼠标
    for (size_t i = 0; i < path.size(); ++i) {
        // 添加微小抖动
        auto jitteredPoint = HumanLikeMovement::AddHumanJitter(
            path[i].first, path[i].second, distance, humanizationFactor);

        // 移动鼠标到抖动后的位置
        mouseController->MoveTo(
            static_cast<int>(jitteredPoint.first), static_cast<int>(jitteredPoint.second));

        // 打印调试信息
        if (i % 5 == 0 || i == path.size() - 1) {  // 每5个点或最后一个点输出一次
            std::cout << "移动到: (" << jitteredPoint.first << ", " << jitteredPoint.second
                << ") [原始路径点: (" << path[i].first << ", " << path[i].second << ")]" << std::endl;
        }

        // 控制移动速度
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }

    // 获取最终鼠标位置
    mouseController->GetCurrentPosition(currentX, currentY);
    std::cout << "测试完成。最终鼠标位置: (" << currentX << ", " << currentY << ")" << std::endl;
}

int main() {
    TestUdpStream();
    
}