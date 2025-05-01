#include <iostream>
#include <thread>
#include "ScreenCaptureWindows.h"
#include <opencv2/opencv.hpp>

// 从ScreenCaptureWindows.cpp中定义的Frame结构体，需要在这里声明
struct Frame {
    cv::Mat image;
    std::chrono::high_resolution_clock::time_point timestamp;

    Frame() = default;

    Frame(const cv::Mat& img) : image(img) {
        timestamp = std::chrono::high_resolution_clock::now();
    }
};

// 用于获取帧的外部函数声明
namespace CaptureModule {
    bool WaitForFrame(Frame& frame);
}

int main() {
    std::thread captureThread;

    try {
        // 初始化采集模块
        captureThread = CaptureModule::Initialize();

        // TODO: 初始化其他模块
        // ProcessModule::Initialize();
        // DetectionModule::Initialize();
        // TrackingModule::Initialize();
        // RenderModule::Initialize();

        // 主循环
        Frame frame;
        while (CaptureModule::IsRunning()) {
            if (CaptureModule::WaitForFrame(frame)) {
                if (!frame.image.empty()) {
                    cv::imshow("Captured Frame", frame.image);

                    // 按ESC键退出
                    if (cv::waitKey(1) == 27) {
                        CaptureModule::Stop();
                        break;
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    // 清理资源
    CaptureModule::Cleanup();

    // 等待线程结束
    if (captureThread.joinable()) {
        captureThread.join();
    }

    // 释放资源
    cv::destroyAllWindows();

    std::cout << "Program terminated normally" << std::endl;
    return 0;
}
