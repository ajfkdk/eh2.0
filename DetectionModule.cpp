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

// ��CaptureModule����֡�ṹ��
namespace CaptureModule {
    bool GetLatestCaptureFrame(Frame& frame);
    bool WaitForFrame(Frame& frame);
    bool IsRunning();
}

// �����Լ��Ļ��λ��������洢�����
#include "RingBuffer.h"

// �������������С
constexpr size_t RESULT_BUFFER_SIZE = 60;  // 2��@30fps

// ģ���ڲ�ȫ�ֱ���
namespace {
    // ��Ŀ���������� - �洢������Ŷ�Ŀ��
    RingBuffer<DetectionResult, RESULT_BUFFER_SIZE> singleResultBuffer;

    // ��Ŀ���������� - �洢���м�⵽��Ŀ��
    RingBuffer<std::vector<DetectionResult>, RESULT_BUFFER_SIZE> multiResultBuffer;

    // ���Ʊ�־
    std::atomic<bool> running{ false };
    std::atomic<bool> debugMode{ false };
    std::atomic<bool> showDetections{ false };

    // Ԥ�����ر���
    std::atomic<bool> hasPredictionPoint{ false };
    std::atomic<int> predictionPointX{ 0 };
    std::atomic<int> predictionPointY{ 0 };

    // Debug֡
    std::mutex debugFrameMutex;
    cv::Mat latestDebugFrame;

    // YOLO�����
    std::unique_ptr<YOLODetector> detector;
}

// ����̺߳���
void detectionThreadFunc() {
    std::cout << "Detection thread started" << std::endl;

    // Ԥ��ѭ��
    while (running.load()) {
        // �Ӳɼ�ģ���ȡ֡
        Frame frame;
        if (CaptureModule::GetLatestCaptureFrame(frame)) {
            if (!frame.image.empty()) {
                // ����ͼ���СΪ256
                cv::Mat resizedImage;
                cv::resize(frame.image, resizedImage, cv::Size(256, 256));

                // ����ʼʱ��
                auto start = std::chrono::high_resolution_clock::now();

                // ����Ŀ����
                std::vector<DetectionResult> results = detector->ProcessImage(resizedImage);

                // Ϊdebugģʽ׼��֡
                cv::Mat debugFrame;
                if (debugMode.load() && showDetections.load()) {
                    debugFrame = resizedImage.clone();

                    // ��debug֡�ϻ��Ƽ���
                    for (const auto& result : results) {
                        // ���������Ͻ�����
                        int left = result.x - result.width / 2;
                        int top = result.y - result.height / 2;

                        // ���Ƽ���
                        cv::rectangle(debugFrame,
                            cv::Rect(left, top, result.width, result.height),
                            cv::Scalar(0, 255, 0), 2);

                        // ����������Ŷȱ�ǩ
                        std::string label = result.className + " " +
                            std::to_string(static_cast<int>(result.confidence * 100)) + "%";
                        cv::putText(debugFrame, label, cv::Point(left, top - 5),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
                    }

                    // ���Ԥ���Ļ���
                    if (hasPredictionPoint.load()) {
                        int x = predictionPointX.load();
                        int y = predictionPointY.load();
                        // ����Ԥ���(��ɫ�㣬�뾶5����)
                        cv::circle(debugFrame, cv::Point(x, y), 5, cv::Scalar(255, 0, 0), -1);

                        // �ڻ������Ͻ�������ֱ�ע
                        cv::putText(debugFrame, "Prediction Target", cv::Point(10, 20),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 1);
                    }

                    // ��ʾ�����
                    cv::imshow("Detection Debug", debugFrame);
                }

                // ��������
                if (!results.empty()) {
                    // ��������Ŀ�굽��Ŀ�껺����
                    multiResultBuffer.write(results);

                    // �ҳ�������Ŷȵ�Ŀ��
                    auto bestResult = *std::max_element(results.begin(), results.end(),
                        [](const auto& a, const auto& b) {
                            return a.confidence < b.confidence;
                        });

                    // ����������Ŷ�Ŀ�굽��Ŀ�껺����
                    singleResultBuffer.write(bestResult);
                }
                else {
                    // û�м�⵽Ŀ�꣬д��ս��
                    DetectionResult emptyResult;
                    singleResultBuffer.write(emptyResult);

                    std::vector<DetectionResult> emptyResults;
                    multiResultBuffer.write(emptyResults);
                }
            
            }
        }
        else {
            // �ȴ�һС��ʱ����ٳ��Ի�ȡ��һ֡
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    std::cout << "Detection thread stopped" << std::endl;
}

// ģ��ӿ�ʵ��
namespace DetectionModule {
    std::thread Initialize(const std::string& modelPath) {
        // ���������
        detector = std::make_unique<YOLODetector>();

        // ��ʼ�������
        if (!detector->Initialize(modelPath)) {
            throw std::runtime_error("Failed to initialize detector: " + detector->GetLastError());
        }

        // ׼������
        running.store(true);
        singleResultBuffer.open();
        multiResultBuffer.open();

        // ��ʾ����
        detector->SetShowDetections(showDetections.load());

        // ��������߳�
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
        //Ϊ��ʵʱ�Ը���
        multiResultBuffer.readLatest(results);
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

    bool IsDebugModeEnabled() {
        return debugMode.load();
    }

    bool GetDebugFrame(cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(debugFrameMutex);
        if (latestDebugFrame.empty()) {
            return false;
        }
        latestDebugFrame.copyTo(frame);
        return true;
    }

    void DrawPredictionPoint(int x, int y) {
        predictionPointX.store(x);
        predictionPointY.store(y);
        hasPredictionPoint.store(true);
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