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

    // YOLO�����
    std::unique_ptr<YOLODetector> detector;
}

// ����̺߳���
void detectionThreadFunc() {
    std::cout << "Detection thread started" << std::endl;

    ////    // �����߳����ȼ�
    //#ifdef _WIN32
    //    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    //#endif

        // ������ʱ�����ڿ���֡��
    auto lastProcessTime = std::chrono::high_resolution_clock::now();
  

    // Ԥ��ѭ��
    while (running.load()) {       

         // �Ӳɼ�ģ���ȡ֡
        Frame frame;
        if (CaptureModule::WaitForFrame(frame)) {
            if (!frame.image.empty()) {
                // ����ͼ���СΪ320x320
                cv::Mat resizedImage;
                cv::resize(frame.image, resizedImage, cv::Size(320, 320));

                // ����ʼʱ��
                auto start = std::chrono::high_resolution_clock::now();

                // ����Ŀ����
                std::vector<DetectionResult> results = detector->ProcessImage(resizedImage);

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

                // �������ʱ��
                auto end = std::chrono::high_resolution_clock::now();
                lastProcessTime = end;

                // �����������ģʽ���������ʱ��
                if (debugMode.load()) {
                    std::chrono::duration<double, std::milli> elapsed = end - start;
                    std::cout << "Detection time: " << elapsed.count() << " ms" << std::endl;

                    if (!results.empty()) {
                        std::cout << "Detected " << results.size() << " objects:" << std::endl;
                        for (const auto& r : results) {
                            std::cout << "  - " << r.className << " at (" << r.x << "," << r.y
                                << ") conf: " << r.confidence << std::endl;
                        }
                    }
                    else {
                        std::cout << "No objects detected" << std::endl;
                    }
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