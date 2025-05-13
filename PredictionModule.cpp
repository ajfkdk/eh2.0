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

    // ������ƽ�����ȶ��Կ��Ʋ���
    std::atomic<float> g_positionThreshold{ 50.0f }; // λ�ñ仯��ֵ�����أ�
    std::atomic<int> g_stabilityTimeThreshold{ 150 }; // �ȶ���ʱ����ֵ�����룩
    std::atomic<float> g_smoothingFactor{ 0.7f }; // ƽ������ (0-1)��Խ��Խƽ��

    // ���λ����������ڴ洢Ԥ����
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // �������ϴ���Ч��Ŀ��λ�ú�ʱ��
    struct {
        int x = 0;
        int y = 0;
        bool valid = false;
        std::chrono::high_resolution_clock::time_point lastUpdateTime;
        std::chrono::high_resolution_clock::time_point positionChangeTime;
        bool positionChanging = false;
    } g_lastTarget;

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
            // ֻ������Ч��Ŀ�꣨classId = 1 �� 3����ͷ
            if (target.classId >= 0 && (target.classId == 1 || target.classId == 3)) {
                float dx = target.x - centerX;
                float dy = target.y - centerY;
                float distance = std::sqrt(dx * dx + dy * dy);

                if (distance < minDistance) {
                    minDistance = distance;
                    nearest = target;
                }
            }
        }

        // ���û���ҵ���ЧĿ�꣬������ЧĿ��
        if (minDistance == std::numeric_limits<float>::max()) {
            nearest.classId = -1;
        }

        return nearest;
    }

    // ��������������֮��ľ���
    float CalculateDistance(int x1, int y1, int x2, int y2) {
        float dx = static_cast<float>(x1 - x2);
        float dy = static_cast<float>(y1 - y2);
        return std::sqrt(dx * dx + dy * dy);
    }

    // ������ƽ���ƶ�����
    void SmoothPosition(int& currentX, int& currentY, int targetX, int targetY) {
        float smoothingFactor = g_smoothingFactor.load();
        currentX = static_cast<int>(currentX * smoothingFactor + targetX * (1.0f - smoothingFactor));
        currentY = static_cast<int>(currentY * smoothingFactor + targetY * (1.0f - smoothingFactor));
    }

    // Ԥ��ģ�鹤������
    void PredictionModuleWorker() {
        std::cout << "Prediction module worker started" << std::endl;

        // �����һ���Ƿ���Ŀ��
        bool hadValidTarget = false;

        // ��ʼ�������ЧĿ��ʱ��
        g_lastTarget.lastUpdateTime = std::chrono::high_resolution_clock::now();

        while (g_running) {
            try {
                // ��ȡ���м��Ŀ��
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                // ����Ԥ����
                PredictionResult prediction;
                prediction.timestamp = std::chrono::high_resolution_clock::now();

                // ���������������Ŀ��
                DetectionResult nearestTarget = FindNearestTarget(targets);

                // ��ǰʱ��
                auto currentTime = std::chrono::high_resolution_clock::now();

                // �������ЧĿ��
                if (nearestTarget.classId >= 0) {
                    // ���֮ǰ�й���ЧĿ��
                    if (g_lastTarget.valid) {
                        // ����λ�ñ仯
                        float distance = CalculateDistance(
                            g_lastTarget.x, g_lastTarget.y,
                            nearestTarget.x, nearestTarget.y);

                        // ���λ�ñ仯�Ƿ񳬹���ֵ
                        if (distance > g_positionThreshold.load()) {
                            // ���λ�����ڱ仯������λ�ñ仯ʱ��
                            if (!g_lastTarget.positionChanging) {
                                g_lastTarget.positionChanging = true;
                                g_lastTarget.positionChangeTime = currentTime;
                            }

                            // ���㾭����ʱ��
                            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                currentTime - g_lastTarget.positionChangeTime).count();

                            // ���δ�����ȶ���ʱ����ֵ�����־�λ��
                            if (elapsedMs < g_stabilityTimeThreshold.load()) {
                                // ���־�λ�ã���ƽ������
                                SmoothPosition(g_lastTarget.x, g_lastTarget.y,
                                    nearestTarget.x, nearestTarget.y);

                                prediction.x = g_lastTarget.x;
                                prediction.y = g_lastTarget.y;
                            }
                            else {
                                // �Ѿ��ȶ��㹻��ʱ�䣬������λ��
                                g_lastTarget.positionChanging = false;
                                g_lastTarget.x = nearestTarget.x;
                                g_lastTarget.y = nearestTarget.y;
                                prediction.x = nearestTarget.x;
                                prediction.y = nearestTarget.y;
                            }
                        }
                        else {
                            // λ�ñ仯����ֵ�ڣ���������
                            g_lastTarget.positionChanging = false;
                            // Ӧ��ƽ��
                            SmoothPosition(g_lastTarget.x, g_lastTarget.y,
                                nearestTarget.x, nearestTarget.y);
                            prediction.x = g_lastTarget.x;
                            prediction.y = g_lastTarget.y;
                        }
                    }
                    else {
                        // ��һ�λ�ȡ��ЧĿ��
                        g_lastTarget.x = nearestTarget.x;
                        g_lastTarget.y = nearestTarget.y;
                        g_lastTarget.valid = true;
                        g_lastTarget.positionChanging = false;
                        prediction.x = nearestTarget.x;
                        prediction.y = nearestTarget.y;
                    }

                    g_lastTarget.lastUpdateTime = currentTime;
                    hadValidTarget = true;
                }
                else {
                    // �޼�⵽��ЧĿ��
                    auto elapsedSinceLastValidMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        currentTime - g_lastTarget.lastUpdateTime).count();

                    // �ж�Ŀ�궪ʧʱ���Ƿ񳬹���ֵ������500ms��
                    if (g_lastTarget.valid && elapsedSinceLastValidMs < 500) {
                        // ��ʱ����Ŀ�궪ʧ��������һ����Чλ��
                        prediction.x = g_lastTarget.x;
                        prediction.y = g_lastTarget.y;
                    }
                    else {
                        // ��ʱ����Ŀ�꣬���ΪĿ�궪ʧ
                        prediction.x = 999;
                        prediction.y = 999;
                        g_lastTarget.valid = false;
                    }

                    hadValidTarget = false;
                }

                // ��Ԥ����д�뻷�λ�����
                g_predictionBuffer.write(prediction);

                // debugģʽ���� - ����ģ���ṩԤ�����Ϣ
                if (g_debugMode.load() && hadValidTarget) {
                    // ���ü��ģ��Ļ���Ԥ��㺯��
                    DetectionModule::DrawPredictionPoint(prediction.x, prediction.y);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error in prediction module: " << e.what() << std::endl;
            }

            // �ʵ����ߣ�����CPUռ��
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
        return g_predictionBuffer.readLatest(result);
    }

    // ���õ���ģʽ
    void SetDebugMode(bool enabled) {
        g_debugMode = enabled;
    }

    // ��ȡ����ģʽ״̬
    bool IsDebugModeEnabled() {
        return g_debugMode.load();
    }

    // ����������λ�ñ仯��ֵ
    void SetPositionThreshold(float threshold) {
        g_positionThreshold = threshold;
    }

    // �����������ȶ���ʱ����ֵ
    void SetStabilityTimeThreshold(int milliseconds) {
        g_stabilityTimeThreshold = milliseconds;
    }

    // ����������ƽ������
    void SetSmoothingFactor(float factor) {
        // ������0-1��Χ��
        factor = std::clamp(factor, 0.0f, 1.0f);
        g_smoothingFactor = factor;
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