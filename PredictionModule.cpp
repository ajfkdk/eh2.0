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
    // ��PredictionModule�����ռ����ȫ�ֱ���
    std::atomic<int> g_currentTargetClassId{ -1 };  // ��ǰ���ٵ�Ŀ�����ID
    std::atomic<int> g_currentTargetId{ -1 };       // ��ǰ���ٵ�Ŀ��ID (ʹ���ڼ���е�����)
    std::chrono::steady_clock::time_point g_lastTargetSwitchTime;  // �ϴ��л�Ŀ���ʱ��
    std::atomic<int> g_targetSwitchCooldownMs{ 500 };  // Ŀ���л���ȴʱ��(����)
    // Ԥ��ʱ����� - Ϊ�˼����Ա�����������ʹ��
    std::atomic<float> g_gFactor{ 0.2f };
    std::atomic<float> g_hFactor{ 0.16f };
    std::atomic<float> g_predictionTime{ 0.0f };

    // ���λ����������ڴ洢Ԥ����
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // ��������Ļ���������Ŀ��
    DetectionResult FindNearestTarget(const std::vector<DetectionResult>& targets) {
        if (targets.empty()) {
            // ��Ŀ��ʱ���õ�ǰĿ��
            g_currentTargetId = -1;
            g_currentTargetClassId = -1;

            DetectionResult emptyResult;
            emptyResult.classId = -1;
            emptyResult.x = 0;
            emptyResult.y = 0;
            return emptyResult;
        }

        // ��Ļ��������
        const int centerX = 160;
        const int centerY = 160;

        // ��ǰʱ��
        auto currentTime = std::chrono::steady_clock::now();

        // ��������Ŀ�����ҵ���ǰ���ڸ��ٵ�Ŀ��
        int currentTargetIdx = -1;
        if (g_currentTargetId >= 0 && g_currentTargetClassId >= 0) {
            for (int i = 0; i < targets.size(); i++) {
                // ���������Ӹ����ӵ�Ŀ��ƥ���߼�������ʹ��Ŀ���λ�úʹ�С
                if (targets[i].classId == g_currentTargetClassId) {
                    // ��ƥ�䣺�ҵ���ͬ����Ŀ��
                    currentTargetIdx = i;
                    break;
                }
            }
        }

        // ����ҵ���ǰĿ�꣬��δ������ȴʱ�䣬�������ٵ�ǰĿ��
        if (currentTargetIdx >= 0 &&
            std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - g_lastTargetSwitchTime).count() < g_targetSwitchCooldownMs) {
            return targets[currentTargetIdx];
        }

        // �������������Ŀ��
        DetectionResult nearest = targets[0];
        float minDistance = std::numeric_limits<float>::max();
        int nearestIdx = -1;

        for (int i = 0; i < targets.size(); i++) {
            const auto& target = targets[i];
            // ֻ������Ч��Ŀ�꣨classId = 1 �� 3) ��ͷ
            if (target.classId >= 0 && (target.classId == 1 || target.classId == 3)) {
                float dx = target.x - centerX;
                float dy = target.y - centerY;
                float distance = std::sqrt(dx * dx + dy * dy);

                if (distance < minDistance) {
                    minDistance = distance;
                    nearest = target;
                    nearestIdx = i;
                }
            }
        }

        // ����ҵ�����Ŀ�꣬���µ�ǰĿ����Ϣ���л�ʱ��
        if (nearestIdx >= 0 && nearest.classId >= 0) {
            if (nearestIdx != currentTargetIdx) {
                g_currentTargetId = nearestIdx;
                g_currentTargetClassId = nearest.classId;
                g_lastTargetSwitchTime = currentTime;
            }
        }
        else {
            // û���ҵ���ЧĿ��
            nearest.classId = -1;
            g_currentTargetId = -1;
            g_currentTargetClassId = -1;
        }

        return nearest;
    }

    // Ԥ��ģ�鹤������
    void PredictionModuleWorker() {
        std::cout << "Prediction module worker started" << std::endl;

        // �����һ���Ƿ���Ŀ��
        bool hadValidTarget = false;

        while (g_running) {
            try {
                // ��ȡ���м��Ŀ��
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                // ����Ԥ����
                PredictionResult prediction;
                prediction.timestamp = std::chrono::high_resolution_clock::now();

                // ���������������Ŀ��
                DetectionResult nearestTarget = FindNearestTarget(targets);

                // �������ЧĿ��
                if (nearestTarget.classId >= 0) {
                    // ֱ��ʹ�����Ŀ�������
                    prediction.x = nearestTarget.x;
                    prediction.y = nearestTarget.y;
                    hadValidTarget = true;  // ��¼��ЧĿ��
                }
                else {
                    // ��ЧĿ�꣬����ΪĿ�궪ʧ
                    prediction.x = 999;
                    prediction.y = 999;
                    hadValidTarget = false;  // ��¼Ŀ�궪ʧ
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