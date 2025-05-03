#include "DetectionModule.h"
#include "YOLODetector.h"
#include <iostream>
#include <chrono>
#include <memory>

struct Frame {
    cv::Mat image;
    std::chrono::high_resolution_clock::time_point timestamp;

    Frame() = default;

    Frame(const cv::Mat& img) : image(img) {
        timestamp = std::chrono::high_resolution_clock::now();
    }
};

// 从CaptureModule导入帧结构体
namespace CaptureModule {
  

    bool GetLatestCaptureFrame(Frame& frame);
    bool WaitForFrame(Frame& frame);
    bool IsRunning();
}

// 定义自己的环形缓冲区来存储检测结果
#include "RingBuffer.h"


// 检测结果缓冲区大小
constexpr size_t RESULT_BUFFER_SIZE = 60;  // 2秒@30fps

// 模块内部全局变量
namespace {
    // 单目标结果缓冲区 - 存储最高置信度目标
    RingBuffer<DetectionResult, RESULT_BUFFER_SIZE> singleResultBuffer;

    // 多目标结果缓冲区 - 存储所有检测到的目标
    RingBuffer<std::vector<DetectionResult>, RESULT_BUFFER_SIZE> multiResultBuffer;

    // 控制标志
    std::atomic<bool> running{ false };
    std::atomic<bool> debugMode{ false };
    std::atomic<bool> showDetections{ false };

    // YOLO检测器
    std::unique_ptr<YOLODetector> detector;
}

// 检测线程函数
void detectionThreadFunc() {
    std::cout << "Detection thread started" << std::endl;
    while (running.load()) {
        Frame frame;
        if (CaptureModule::GetLatestCaptureFrame(frame)) {
            if (!frame.image.empty()) {
                cv::Mat resizedImage;
                cv::resize(frame.image, resizedImage, cv::Size(320, 320));
                auto start = std::chrono::high_resolution_clock::now();
                std::vector<DetectionResult> results = detector->ProcessImage(resizedImage);
                if (!results.empty()) {
                    multiResultBuffer.write(results);
                    auto bestResult = *std::max_element(results.begin(), results.end(),
                        [](const auto& a, const auto& b) { return a.confidence < b.confidence; });
                    singleResultBuffer.write(bestResult);
                }
                else {
                    DetectionResult emptyResult;
                    singleResultBuffer.write(emptyResult);
                    std::vector<DetectionResult> emptyResults;
                    multiResultBuffer.write(emptyResults);
                }
                auto end = std::chrono::high_resolution_clock::now();
                if (debugMode.load()) {
                    std::chrono::duration<double, std::milli> elapsed = end - start;
                    std::cout << "Detection time: " << elapsed.count() << " ms" << std::endl;
                }
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 仅在无帧时短暂等待
        }
    }
    std::cout << "Detection thread stopped" << std::endl;
}

// 模块接口实现
namespace DetectionModule {
    std::thread Initialize(const std::string& modelPath) {
        // 创建检测器
        detector = std::make_unique<YOLODetector>();

        // 初始化检测器
        if (!detector->Initialize(modelPath)) {
            throw std::runtime_error("Failed to initialize detector: " + detector->GetLastError());
        }

        // 准备启动
        running.store(true);
        singleResultBuffer.open();
        multiResultBuffer.open();

        // 显示设置
        detector->SetShowDetections(showDetections.load());

        // 启动检测线程
        return std::thread(detectionThreadFunc);
    }

    bool GetLatestDetectionResult(DetectionResult& result) {
        return singleResultBuffer.read(result, false);
    }

    bool WaitForResult(DetectionResult& result) {
        return singleResultBuffer.read(result, true);
    }

    std::vector<DetectionResult> GetAllResults() {
        std::vector<DetectionResult> results;
        multiResultBuffer.read(results, false);
        return results;
    }

    void Cleanup() {
        running.store(false);
        singleResultBuffer.close();
        multiResultBuffer.close();
        detector.reset();
    }

    bool IsRunning() {
        return running.load();
    }

    void Stop() {
        running.store(false);
    }

    void SetDebugMode(bool enabled) {
        debugMode.store(enabled);
    }

    void SetShowDetections(bool show) {
        showDetections.store(show);
        if (detector) {
            detector->SetShowDetections(show);
        }
    }

    void SetConfidenceThreshold(float threshold) {
        if (detector) {
            detector->SetConfidenceThreshold(threshold);
        }
    }
}