// PredictionModule.cpp
#include "PredictionModule.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace PredictionModule {
    // ȫ�ֱ���
    std::atomic<bool> g_running{ false };

    // g-h�˲�������
    std::atomic<float> g_gFactor{ 0.2f };    // λ������
    std::atomic<float> g_hFactor{ 0.16f };   // �ٶ�����
    std::atomic<float> g_predictionTime{ 0.2f }; // Ԥ��ʱ�� (��)

    // ���λ����������ڴ洢Ԥ����
    constexpr size_t BUFFER_SIZE = 100;  // �����ʵ��Ļ�������С
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // g-h�˲���״̬
    struct FilterState {
        float xEst;    // ���Ƶ�Xλ��
        float yEst;    // ���Ƶ�Yλ��
        float vxEst;   // ���Ƶ�X�ٶ�
        float vyEst;   // ���Ƶ�Y�ٶ�
        std::chrono::high_resolution_clock::time_point lastUpdateTime;  // �ϴθ���ʱ��
        bool initialized;  // �Ƿ��ѳ�ʼ��

        FilterState() : xEst(0), yEst(0), vxEst(0), vyEst(0), initialized(false) {}
    };

    FilterState g_filterState;

    // �ϴλ�ȡ����ЧĿ���ʱ��
    std::chrono::high_resolution_clock::time_point g_lastValidTargetTime;

    // �����˲���״̬
    void ResetFilterState(const DetectionResult& target) {
        g_filterState.xEst = target.x;
        g_filterState.yEst = target.y;
        g_filterState.vxEst = 0;
        g_filterState.vyEst = 0;
        g_filterState.lastUpdateTime = std::chrono::high_resolution_clock::now();
        g_filterState.initialized = true;

        g_lastValidTargetTime = std::chrono::high_resolution_clock::now();

        std::cout << "Filter state reset with target at (" << target.x << "," << target.y << ")" << std::endl;
    }

    // ����g-h�˲���״̬
    void UpdateFilter(const DetectionResult& target) {
        auto currentTime = std::chrono::high_resolution_clock::now();

        // ����˲���δ��ʼ�������ʼ����
        if (!g_filterState.initialized) {
            ResetFilterState(target);
            return;
        }

        // ����ʱ������(��)
        float deltaT = std::chrono::duration<float>(currentTime - g_filterState.lastUpdateTime).count();
        if (deltaT <= 0) return; // ��ֹ������

        // 1. Ԥ�ⲽ�� - ������һ�εĹ��ƺ��ٶ�Ԥ�⵱ǰλ��
        float xPred = g_filterState.xEst + g_filterState.vxEst * deltaT;
        float yPred = g_filterState.yEst + g_filterState.vyEst * deltaT;

        // 2. ���²��� - ʹ��ʵ�ʲ���ֵ��Ԥ��ֵ֮��Ĳв���¹���
        // ����вʵ��λ����Ԥ��λ�õĲ
        float residualX = target.x - xPred;
        float residualY = target.y - yPred;

        // ����λ�ù���: Ԥ��λ�� + g * �в�
        float g = g_gFactor.load();
        float h = g_hFactor.load();

        float xEst = xPred + g * residualX;
        float yEst = yPred + g * residualY;

        // �����ٶȹ���: �ϴ��ٶȹ��� + h * �в� / ʱ����
        float vxEst = g_filterState.vxEst + h * (residualX / deltaT);
        float vyEst = g_filterState.vyEst + h * (residualY / deltaT);

        // �����˲���״̬
        g_filterState.xEst = xEst;
        g_filterState.yEst = yEst;
        g_filterState.vxEst = vxEst;
        g_filterState.vyEst = vyEst;
        g_filterState.lastUpdateTime = currentTime;

        g_lastValidTargetTime = currentTime;
    }

    // Ԥ��δ��λ��
    PredictionResult PredictFuturePosition() {
        PredictionResult result;
        result.timestamp = std::chrono::high_resolution_clock::now();

        // ���Ŀ���Ƿ�ʧ (����Ԥ��ʱ��û�и���)
        auto timeSinceLastValidTarget = std::chrono::duration<float>(
            result.timestamp - g_lastValidTargetTime).count();

        if (!g_filterState.initialized || timeSinceLastValidTarget > g_predictionTime.load()) {
            // Ŀ�궪ʧ���������ֵ
            result.x = 999;
            result.y = 999;
            return result;
        }

        // ����Ԥ��ʱ����λ��
        float predictionTimeSeconds = g_predictionTime.load();
        result.x = static_cast<int>(g_filterState.xEst + g_filterState.vxEst * predictionTimeSeconds);
        result.y = static_cast<int>(g_filterState.yEst + g_filterState.vyEst * predictionTimeSeconds);

        return result;
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

        // ����ѭ�����ʱ�� (Ŀ��60FPS+)
        const auto loopInterval = std::chrono::microseconds(16000); // ԼΪ16.7ms��ȷ��>60FPS

        while (g_running) {
            auto loopStart = std::chrono::high_resolution_clock::now();

            try {
                // ��ȡ���м��Ŀ��
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                // ����Ƿ�����ЧĿ��
                if (!targets.empty()) {
                    // ���������������Ŀ��
                    DetectionResult nearestTarget = FindNearestTarget(targets);

                    // �������ЧĿ��
                    if (nearestTarget.classId >= 0) {
                        // �ж��Ƿ���Ҫ�����˲��� (���֮ǰĿ�궪ʧ) .count()��ʾ���õ��������������ʾ����ȥ�˶����롱��
                        auto timeSinceLastValidTarget = std::chrono::duration<float>(
                            std::chrono::high_resolution_clock::now() - g_lastValidTargetTime).count();

                        if (!g_filterState.initialized || timeSinceLastValidTarget > g_predictionTime.load()) {
                            ResetFilterState(nearestTarget);
                        }
                        else {
                            UpdateFilter(nearestTarget);
                        }
                    }
                }

                // Ԥ��δ��λ��
                PredictionResult prediction = PredictFuturePosition();

                // ��Ԥ����д�뻷�λ�����
                g_predictionBuffer.write(prediction);

            }
            catch (const std::exception& e) {
                std::cerr << "Error in prediction module: " << e.what() << std::endl;
            }

            // ����ѭ��Ƶ��
            auto loopEnd = std::chrono::high_resolution_clock::now();
            auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(loopEnd - loopStart);

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
        g_lastValidTargetTime = std::chrono::high_resolution_clock::now() - std::chrono::seconds(10); // ��ʼ��Ϊ10��ǰ
        g_filterState = FilterState(); // �����˲���״̬

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
        return g_predictionBuffer.read(result, false);
    }

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