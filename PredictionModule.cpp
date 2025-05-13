// PredictionModule.cpp
#include "PredictionModule.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <opencv2/opencv.hpp>

namespace PredictionModule {
    // ȫ�ֱ���
    std::atomic<bool> g_running{ false };
    std::atomic<bool> g_debugMode{ false };

    // �������ڲ���
    std::atomic<int> g_windowSize{ 5 };  // Ĭ�ϱ���5֡��ʷ����
    std::atomic<float> g_distanceThreshold{ 30.0f };  // Ĭ�Ͼ�����ֵΪ30����
    std::atomic<int> g_lostFrameThreshold{ 5 };  // Ĭ��5֡δ��⵽��Ϊ��ʧ

    // Ԥ��ʱ����� - Ϊ�˼����Ա�����������ʹ��
    std::atomic<float> g_gFactor{ 0.2f };
    std::atomic<float> g_hFactor{ 0.16f };
    std::atomic<float> g_predictionTime{ 0.0f };

    // ���λ����������ڴ洢Ԥ����
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // Ŀ����ʷ��¼��״̬
    struct {
        std::deque<std::pair<int, int>> positionHistory;  // ��ʷλ�ô���
        int lastValidX = -1;  // ��һ����Ч��X����
        int lastValidY = -1;  // ��һ����Ч��Y����
        int lostFrameCount = 0;  // Ŀ�궪ʧ֡����
        int targetClassId = -1;  // ��ǰ׷�ٵ�Ŀ�����ID
    } g_targetState;

    // ������������
    float CalculateDistance(int x1, int y1, int x2, int y2) {
        float dx = static_cast<float>(x1 - x2);
        float dy = static_cast<float>(y1 - y2);
        return std::sqrt(dx * dx + dy * dy);
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
            // ֻ������Ч��Ŀ�꣨classId =1 , 3) ��ͷ
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

    // ��������һĿ����ӽ���Ŀ�꣨������ֵ������
    DetectionResult FindClosestToLastTarget(const std::vector<DetectionResult>& targets) {
        // ���û����ʷ��¼������Ŀ���ѳ�ʱ�䶪ʧ��ʹ�����Ŀ��
        if (g_targetState.lastValidX < 0 || g_targetState.lastValidY < 0 ||
            g_targetState.lostFrameCount > g_lostFrameThreshold.load()) {
            return FindNearestTarget(targets);
        }

        // ���Ҿ�����һ��Ŀ�������Ŀ��
        DetectionResult closest;
        closest.classId = -1;
        float minDistance = std::numeric_limits<float>::max();

        for (const auto& target : targets) {
            // ֻ������Ч��Ŀ�꣨classId = 1��3��
            if (target.classId >= 0 && (target.classId == 1 || target.classId == 3)) {
                float distance = CalculateDistance(
                    g_targetState.lastValidX, g_targetState.lastValidY,
                    target.x, target.y
                );

                // �������С����ֵ���������
                if (distance < g_distanceThreshold.load() && distance < minDistance) {
                    minDistance = distance;
                    closest = target;
                }
            }
        }

        // ����ҵ�����������Ŀ�꣬���ظ�Ŀ��
        if (closest.classId >= 0) {
            return closest;
        }

        // ���򣬷��ؾ������������Ŀ�꣨����������
        return FindNearestTarget(targets);
    }

    // Ӧ�û������ڼ�Ȩƽ��ȥ����
    std::pair<int, int> ApplyWindowAveraging(int currentX, int currentY) {
        // ����ǰλ����ӵ���ʷ��¼
        g_targetState.positionHistory.push_back(std::make_pair(currentX, currentY));

        // ���ִ��ڴ�С�������趨ֵ
        while (g_targetState.positionHistory.size() > static_cast<size_t>(g_windowSize.load())) {
            g_targetState.positionHistory.pop_front();
        }

        // �����ʷ��¼Ϊ�գ�ֱ�ӷ��ص�ǰλ��
        if (g_targetState.positionHistory.empty()) {
            return std::make_pair(currentX, currentY);
        }

        // �����Ȩƽ�� - �����֡Ȩ�ظ���
        float sumX = 0, sumY = 0;
        float totalWeight = 0;

        const int historySize = static_cast<int>(g_targetState.positionHistory.size());
        for (int i = 0; i < historySize; i++) {
            // ����Ȩ�� - �������������µ�Ȩ�����
            float weight = static_cast<float>(i + 1);

            sumX += g_targetState.positionHistory[i].first * weight;
            sumY += g_targetState.positionHistory[i].second * weight;
            totalWeight += weight;
        }

        // �����Ȩƽ������
        int avgX = static_cast<int>(sumX / totalWeight);
        int avgY = static_cast<int>(sumY / totalWeight);

        return std::make_pair(avgX, avgY);
    }

    // Ԥ��ģ�鹤������
    void PredictionModuleWorker() {
        std::cout << "Prediction module worker started" << std::endl;

        // ��ʼ��Ŀ��״̬
        g_targetState.lastValidX = -1;
        g_targetState.lastValidY = -1;
        g_targetState.lostFrameCount = 0;
        g_targetState.targetClassId = -1;
        g_targetState.positionHistory.clear();

        while (g_running) {
            try {
                // ��ȡ���м��Ŀ��
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                // ����Ԥ����
                PredictionResult prediction;
                prediction.timestamp = std::chrono::high_resolution_clock::now();

                // ���ҵ�ǰӦ���ٵ�Ŀ��
                DetectionResult currentTarget = FindClosestToLastTarget(targets);

                // �������ЧĿ��
                if (currentTarget.classId >= 0) {
                    // Ŀ����Ч�����������Чλ�ò����ö�ʧ����
                    g_targetState.lastValidX = currentTarget.x;
                    g_targetState.lastValidY = currentTarget.y;
                    g_targetState.lostFrameCount = 0;
                    g_targetState.targetClassId = currentTarget.classId;

                    // Ӧ�û������ڼ�Ȩƽ��ȥ����
                    auto smoothedPosition = ApplyWindowAveraging(currentTarget.x, currentTarget.y);

                    // ʹ��ƽ�����λ����ΪԤ����
                    prediction.x = smoothedPosition.first;
                    prediction.y = smoothedPosition.second;
                }
                else {
                    // Ŀ�궪ʧ�����Ӷ�ʧ����
                    g_targetState.lostFrameCount++;

                    // �����ʧʱ�䲻����ʹ����ʷ�����ṩԤ��
                    if (g_targetState.lostFrameCount <= g_lostFrameThreshold.load() &&
                        g_targetState.lastValidX >= 0 && g_targetState.lastValidY >= 0) {
                        // ʹ�����һ��ƽ�������ΪԤ��
                        if (!g_targetState.positionHistory.empty()) {
                            prediction.x = g_targetState.positionHistory.back().first;
                            prediction.y = g_targetState.positionHistory.back().second;
                        }
                        else {
                            prediction.x = g_targetState.lastValidX;
                            prediction.y = g_targetState.lastValidY;
                        }
                    }
                    else {
                        // Ŀ����ȫ��ʧ������Ϊ��ЧĿ��
                        prediction.x = 999;
                        prediction.y = 999;

                        // ����Ŀ��״̬��׼����������
                        g_targetState.lastValidX = -1;
                        g_targetState.lastValidY = -1;
                        g_targetState.targetClassId = -1;
                        g_targetState.positionHistory.clear();
                    }
                }

                // ��Ԥ����д�뻷�λ�����
                g_predictionBuffer.write(prediction);

                // debugģʽ���� - ����ģ���ṩԤ�����Ϣ
                if (g_debugMode.load() && (prediction.x != 999 && prediction.y != 999)) {
                    // ���ü��ģ��Ļ���Ԥ��㺯��
                    DetectionModule::DrawPredictionPoint(prediction.x, prediction.y);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error in prediction module: " << e.what() << std::endl;
            }

            // �������ߣ�����CPUռ�ù���
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
        g_targetState.positionHistory.clear();
        std::cout << "Prediction module cleanup completed" << std::endl;
    }

    bool GetLatestPrediction(PredictionResult& result) {
        return g_predictionBuffer.readLatest(result);
    }

    // ���û������ڴ�С
    void SetWindowSize(int size) {
        if (size > 0) {
            g_windowSize = size;
        }
    }

    // ��ȡ�������ڴ�С
    int GetWindowSize() {
        return g_windowSize.load();
    }

    // ���þ�����ֵ
    void SetDistanceThreshold(float threshold) {
        if (threshold > 0) {
            g_distanceThreshold = threshold;
        }
    }

    // ��ȡ������ֵ
    float GetDistanceThreshold() {
        return g_distanceThreshold.load();
    }

    // ����Ŀ�궪ʧ����֡��
    void SetLostFrameThreshold(int frames) {
        if (frames >= 0) {
            g_lostFrameThreshold = frames;
        }
    }

    // ��ȡĿ�궪ʧ����֡��
    int GetLostFrameThreshold() {
        return g_lostFrameThreshold.load();
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