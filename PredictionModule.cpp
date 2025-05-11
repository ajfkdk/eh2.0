// PredictionModule.cpp
#include "PredictionModule.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <opencv2/opencv.hpp>

namespace PredictionModule {
    // ȫ�ֱ���
    std::atomic<bool> g_running{ false };
    std::atomic<bool> g_debugMode{ false };

    // Ԥ��ʱ����� - Ϊ�˼����Ա�����������ʹ��
    std::atomic<float> g_gFactor{ 0.2f };
    std::atomic<float> g_hFactor{ 0.16f };
    std::atomic<float> g_predictionTime{ 0.0f };

    // ���λ����������ڴ洢Ԥ����
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // ��ȡ���ģ���debug֡
    bool GetDetectionDebugFrame(cv::Mat& frame) {
        return DetectionModule::GetDebugFrame(frame);
    }

    // ��������Ļ���������Ŀ��
    DetectionResult FindNearestTarget(const std::vector<DetectionResult>& targets) {
        if (targets.empty()) {
            DetectionResult emptyResult;
            emptyResult.classId = -1;
            emptyResult.x = 0;
            emptyResult.y = 0;
            return emptyResult;
        }

        // ��Ļ�������� (����320x320��ͼ��)
        const int centerX = 160;
        const int centerY = 160;

        // ���������Ŀ��
        DetectionResult nearest = targets[0];
        float minDistance = std::numeric_limits<float>::max();

        for (const auto& target : targets) {
            float dx = target.x - centerX;
            float dy = target.y - centerY;
            float distance = std::sqrt(dx * dx + dy * dy);

            if (distance < minDistance) {
                minDistance = distance;
                nearest = target;
            }
        }

        return nearest;
    }

    // Ԥ��ģ�鹤������
    void PredictionModuleWorker() {
        std::cout << "Prediction module worker started" << std::endl;

        // ����ѭ�����ʱ�� (50ms)
        const auto loopInterval = std::chrono::milliseconds(50);

        while (g_running) {
            auto loopStart = std::chrono::high_resolution_clock::now();

            try {
                // ��ȡ���м��Ŀ��
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                // ����Ԥ����
                PredictionResult prediction;
                prediction.timestamp = std::chrono::high_resolution_clock::now();

                // ����Ƿ�����ЧĿ��
                if (!targets.empty()) {
                    // ���������������Ŀ��
                    DetectionResult nearestTarget = FindNearestTarget(targets);

                    // �������ЧĿ��
                    if (nearestTarget.classId >= 0) {
                        // ֱ��ʹ�����Ŀ�������
                        prediction.x = nearestTarget.x;
                        prediction.y = nearestTarget.y;
                    }
                    else {
                        // ��ЧĿ�꣬����ΪĿ�궪ʧ
                        prediction.x = 999;
                        prediction.y = 999;
                    }
                }
                else {
                    // û��Ŀ�꣬����ΪĿ�궪ʧ
                    prediction.x = 999;
                    prediction.y = 999;
                }

                // ��Ԥ����д�뻷�λ�����
                g_predictionBuffer.write(prediction);

                // debugģʽ���� - �ڼ��ģ���debug�����ϻ���Ԥ���
                if (g_debugMode.load() && prediction.x != 999 && prediction.y != 999) {
                    // ���ȼ����ģ���Ƿ���debugģʽ��
                    if (DetectionModule::IsDebugModeEnabled()) {
                        // ��ȡ��ǰ���µļ��֡
                        cv::Mat debugFrame;
                        if (GetDetectionDebugFrame(debugFrame) && !debugFrame.empty()) {
                            // ����Ԥ���(��ɫ�㣬�뾶5����)
                            cv::circle(debugFrame, cv::Point(prediction.x, prediction.y), 5, cv::Scalar(255, 0, 0), -1);

                            // �ڻ������Ͻ�������ֱ�ע
                            cv::putText(debugFrame, "Prediction Target", cv::Point(10, 20),
                                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 1);

                            // ��ʾ����Ԥ����frame
                            cv::imshow("Detection Debug", debugFrame);
                        }
                    }
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error in prediction module: " << e.what() << std::endl;
            }

            // ����ѭ��Ƶ��Ϊ50ms
            auto loopEnd = std::chrono::high_resolution_clock::now();
            auto processingTime = std::chrono::duration_cast<std::chrono::milliseconds>(loopEnd - loopStart);

            if (processingTime < loopInterval) {
                std::this_thread::sleep_for(loopInterval - processingTime);
            }
        }

        std::cout << "Prediction module worker stopped" << std::endl;
    }

    // �����ӿ�ʵ��
    std::thread Initialize() {
        if (g_running) {
            std::cerr << "Prediction module already running" << std::endl;
            return std::thread();
        }

        std::cout << "Initializing prediction module" << std::endl;

        g_running = true;
        g_predictionBuffer.clear();

        return std::thread(PredictionModuleWorker);
    }

    bool IsRunning() {
        return g_running;
    }

    void Stop() {
        if (!g_running) {
            return;
        }

        std::cout << "Stopping prediction module" << std::endl;
        g_running = false;
    }

    void Cleanup() {
        g_predictionBuffer.clear();
        std::cout << "Prediction module cleanup completed" << std::endl;
    }

    bool GetLatestPrediction(PredictionResult& result) {
        return g_predictionBuffer.read(result, true);
    }

    // ���õ���ģʽ
    void SetDebugMode(bool enabled) {
        g_debugMode = enabled;
    }

    // ��ȡ����ģʽ״̬
    bool IsDebugModeEnabled() {
        return g_debugMode.load();
    }

    // ���º�������Ϊ�ӿڼ��ݣ���ʵ���ϲ���ʹ��
    void SetGFactor(float g) {
        g_gFactor = g;
    }

    void SetHFactor(float h) {
        g_hFactor = h;
    }

    void SetPredictionTime(float seconds) {
        g_predictionTime = seconds;
    }
}