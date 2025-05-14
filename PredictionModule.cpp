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

    // Ŀ����ٱ���
    std::atomic<int> g_currentTargetId{ -1 };  // ��ǰ���ٵ�Ŀ��ID
    std::atomic<bool> g_isTargetLocked{ false }; // �Ƿ�������Ŀ��
    std::atomic<int> g_lastTargetX{ 0 }; // �ϴ�����Ŀ���X����
    std::atomic<int> g_lastTargetY{ 0 }; // �ϴ�����Ŀ���Y����
    std::chrono::steady_clock::time_point g_targetLostTime; // Ŀ�궪ʧʱ��
    std::atomic<int> g_validateTargetDistance{ 30 }; // Ŀ����֤����(����)
    std::atomic<int> g_targetLostTimeoutMs{ 300 }; // Ŀ�궪ʧ��ʱ(����)
    std::atomic<int> g_lastMouseMoveX{ 0 }; // �ϴ����X�����ƶ�
    std::atomic<int> g_lastMouseMoveY{ 0 }; // �ϴ����Y�����ƶ�

    // Ԥ��ʱ����� - Ϊ�˼����Ա�����������ʹ��
    std::atomic<float> g_gFactor{ 0.2f };
    std::atomic<float> g_hFactor{ 0.16f };
    std::atomic<float> g_predictionTime{ 0.0f };

    // ���λ����������ڴ洢Ԥ����
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // ��¼����ƶ�
    void RecordMouseMove(int dx, int dy) {
        g_lastMouseMoveX += dx;
        g_lastMouseMoveY += dy;
    }

    // ��������Ļ���������Ŀ��
    DetectionResult FindNearestTarget(const std::vector<DetectionResult>& targets) {
        // ��Ļ��������
        const int centerX = 160;
        const int centerY = 160;

        // ��ǰʱ��
        auto currentTime = std::chrono::steady_clock::now();

        // ���û��Ŀ�꣬��������״̬
        if (targets.empty()) {
            // ���֮ǰ������Ŀ�꣬��¼��ʧʱ��
            if (g_isTargetLocked) {
                g_targetLostTime = currentTime;
            }
            g_isTargetLocked = false;
            g_currentTargetId = -1;

            DetectionResult emptyResult;
            emptyResult.classId = -1;
            emptyResult.x = 999;
            emptyResult.y = 999;
            return emptyResult;
        }

        // ��������ƶ����µ�Ŀ��λ�ñ仯
        int compensatedLastX = g_lastTargetX - static_cast<int>(g_lastMouseMoveX * 0.5);
        int compensatedLastY = g_lastTargetY - static_cast<int>(g_lastMouseMoveY * 0.5);

        // ��������ƶ�����
        g_lastMouseMoveX = 0;
        g_lastMouseMoveY = 0;

        // �����ǰ������Ŀ��
        if (g_isTargetLocked) {
            // ������Ŀ����ΧѰ��ƥ��Ŀ��
            std::vector<DetectionResult> candidatesInRange;

            for (const auto& target : targets) {
                // ֻ������Ч��Ŀ�꣨classId = 1 �� 3)
                if (target.classId >= 0 && (target.classId == 1 || target.classId == 3)) {
                    // �������ϴ�λ�õľ���
                    float dx = target.x - compensatedLastX;
                    float dy = target.y - compensatedLastY;
                    float distance = std::sqrt(dx * dx + dy * dy);

                    // �������Ч��Χ�ڣ������ѡ
                    if (distance <= g_validateTargetDistance) {
                        candidatesInRange.push_back(target);
                    }
                }
            }

            // ����ڷ�Χ���ҵ��˺�ѡĿ��
            if (!candidatesInRange.empty()) {
                // ����Ŀ�����ʱ��
                g_targetLostTime = currentTime;

                // ���ֻ��һ����ѡ��ֱ��ʹ��
                if (candidatesInRange.size() == 1) {
                    auto& target = candidatesInRange[0];

                    // �����ϴ�λ��
                    g_lastTargetX = target.x;
                    g_lastTargetY = target.y;

                    return target;
                }
                // ����ж����ѡ��ѡ������������ģ����ݲ������Ч����
                else {
                    DetectionResult nearest = candidatesInRange[0];
                    float minDistance = std::numeric_limits<float>::max();

                    for (const auto& target : candidatesInRange) {
                        float dx = target.x - centerX;
                        float dy = target.y - centerY;
                        float distance = std::sqrt(dx * dx + dy * dy);

                        if (distance < minDistance) {
                            minDistance = distance;
                            nearest = target;
                        }
                    }

                    // �����ϴ�λ��
                    g_lastTargetX = nearest.x;
                    g_lastTargetY = nearest.y;

                    // ��Ȼ�ҵ���Ŀ�꣬����Ϊ�����ѡ���ܵ��¶�����������Ч����
                    DetectionResult result = nearest;
                    result.x = 999;
                    result.y = 999;
                    return result;
                }
            }
            // ����ڷ�Χ��û���ҵ���ѡĿ��
            else {
                // ����Ƿ񳬹�Ŀ�궪ʧ��ʱ
                auto lostDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    currentTime - g_targetLostTime).count();

                // ���������ʱʱ�䣬�������
                if (lostDuration > g_targetLostTimeoutMs) {
                    g_isTargetLocked = false;
                    g_currentTargetId = -1;
                }

                // ������ЧĿ��
                DetectionResult emptyResult;
                emptyResult.classId = -1;
                emptyResult.x = 999;
                emptyResult.y = 999;
                return emptyResult;
            }
        }

        // �����ǰû������Ŀ�꣬Ѱ����Ŀ��
        DetectionResult nearest;
        nearest.classId = -1;
        float minDistance = std::numeric_limits<float>::max();

        for (const auto& target : targets) {
            // ֻ������Ч��Ŀ�꣨classId = 1 �� 3)
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

        // ����ҵ�����Ŀ�꣬������
        if (nearest.classId >= 0) {
            g_isTargetLocked = true;
            g_lastTargetX = nearest.x;
            g_lastTargetY = nearest.y;
            g_targetLostTime = currentTime;
            return nearest;
        }

        // û���ҵ���ЧĿ��
        DetectionResult emptyResult;
        emptyResult.classId = -1;
        emptyResult.x = 999;
        emptyResult.y = 999;
        return emptyResult;
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
                if (nearestTarget.classId >= 0 && nearestTarget.x != 999) {
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

    // ��¼����ƶ��Ľӿ�
    void NotifyMouseMovement(int dx, int dy) {
        RecordMouseMove(dx, dy);
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