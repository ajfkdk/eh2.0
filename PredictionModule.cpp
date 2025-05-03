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

    // ��ӣ�Ԥ����Ч��Χ����
    constexpr int MAX_VALID_COORDINATE = 1000;  // ������������ֵ
    constexpr float MAX_VALID_VELOCITY = 300.0f; // ���������ٶ�ֵ(����/��)

    // ��ӣ�������ЧԤ�����
    int g_consecutiveInvalidPredictions = 0;
    constexpr int MAX_INVALID_PREDICTIONS = 5;  // ��������������ЧԤ�����

    // �����˲���״̬
    void ResetFilterState(const DetectionResult& target) {
        g_filterState.xEst = target.x;
        g_filterState.yEst = target.y;
        g_filterState.vxEst = 0;
        g_filterState.vyEst = 0;
        g_filterState.lastUpdateTime = std::chrono::high_resolution_clock::now();
        g_filterState.initialized = true;
        g_consecutiveInvalidPredictions = 0;  // ������ЧԤ�����

        g_lastValidTargetTime = std::chrono::high_resolution_clock::now();

        std::cout << "Filter state reset with target at (" << target.x << "," << target.y << ")" << std::endl;
    }

    // ����ٶȹ����Ƿ���Ч
    bool IsVelocityValid(float vx, float vy) {
        float speedMagnitude = std::sqrt(vx * vx + vy * vy);
        return speedMagnitude <= MAX_VALID_VELOCITY;
    }

    // ����ֵ�ں���Χ��
    float ClampValue(float value, float min, float max) {
        if (value < min) return min;
        if (value > max) return max;
        return value;
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
        if (deltaT <= 0.0001f) return; // ��ֹ���Լ�Сֵ

        // ���ʱ����̫��˵�������Ƕ϶������Ĵ��������˲���
        if (deltaT > 0.5f) {  // �������0.5��û�и���
            ResetFilterState(target);
            return;
        }

        // 1. Ԥ�ⲽ�� - ������һ�εĹ��ƺ��ٶ�Ԥ�⵱ǰλ��
        float xPred = g_filterState.xEst + g_filterState.vxEst * deltaT;
        float yPred = g_filterState.yEst + g_filterState.vyEst * deltaT;

        // 2. ���²��� - ʹ��ʵ�ʲ���ֵ��Ԥ��ֵ֮��Ĳв���¹���
        // ����вʵ��λ����Ԥ��λ�õĲ
        float residualX = target.x - xPred;
        float residualY = target.y - yPred;

        // ��ӣ����Ʋв��С����ֹͻ�䵼���ٶȹ���ʧ��
        float maxResidual = 50.0f;  // �������Ӧ�ó�������
        residualX = ClampValue(residualX, -maxResidual, maxResidual);
        residualY = ClampValue(residualY, -maxResidual, maxResidual);

        // ����λ�ù���: Ԥ��λ�� + g * �в�
        float g = g_gFactor.load();
        float h = g_hFactor.load();

        float xEst = xPred + g * residualX;
        float yEst = yPred + g * residualY;

        // �����ٶȹ���: �ϴ��ٶȹ��� + h * �в� / ʱ����
        float vxEst = g_filterState.vxEst + h * (residualX / deltaT);
        float vyEst = g_filterState.vyEst + h * (residualY / deltaT);

        // ��ӣ������ٶȹ��ƵĴ�С
        if (!IsVelocityValid(vxEst, vyEst)) {
            // ����ٶȹ��Ʋ�������������
            float speedMagnitude = std::sqrt(vxEst * vxEst + vyEst * vyEst);
            if (speedMagnitude > MAX_VALID_VELOCITY) {
                float scale = MAX_VALID_VELOCITY / speedMagnitude;
                vxEst *= scale;
                vyEst *= scale;

                // ��¼�ٶȱ����Ƶ����
                std::cout << "Velocity limited from " << speedMagnitude
                    << " to " << MAX_VALID_VELOCITY << std::endl;

                // ������ЧԤ�����
                g_consecutiveInvalidPredictions++;

                // ���������γ�����Ч�ٶȣ����������˲���
                if (g_consecutiveInvalidPredictions > MAX_INVALID_PREDICTIONS) {
                    ResetFilterState(target);
                    return;
                }
            }
        }
        else {
            // �����ٶȣ����ü�����
            g_consecutiveInvalidPredictions = 0;
        }

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

        if (!g_filterState.initialized || timeSinceLastValidTarget > g_predictionTime.load() * 1.5f) {
            // Ŀ�궪ʧ���������ֵ
            result.x = 999;
            result.y = 999;
            return result;
        }

        // ����Ԥ��ʱ����λ��
        float predictionTimeSeconds = g_predictionTime.load();
        float predictedX = g_filterState.xEst + g_filterState.vxEst * predictionTimeSeconds;
        float predictedY = g_filterState.yEst + g_filterState.vyEst * predictionTimeSeconds;

        // ��ӣ���֤Ԥ��ֵ�Ƿ��ں���Χ��
        if (std::isnan(predictedX) || std::isnan(predictedY) ||
            std::abs(predictedX) > MAX_VALID_COORDINATE ||
            std::abs(predictedY) > MAX_VALID_COORDINATE) {

            // Ԥ��ֵ�쳣������Ŀ�궪ʧ�ź�
            std::cout << "Invalid prediction detected: (" << predictedX << "," << predictedY << ")" << std::endl;
            result.x = 999;
            result.y = 999;

            // Ԥ���쳣���ܱ����˲���״̬�����⣬�������ÿ�����
            g_consecutiveInvalidPredictions++;
            return result;
        }

        // ת��Ϊ��������
        result.x = static_cast<int>(predictedX);
        result.y = static_cast<int>(predictedY);

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
        const auto loopInterval = std::chrono::microseconds(16667); // ԼΪ16.7ms��ȷ��>60FPS

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
                        // �ж��Ƿ���Ҫ�����˲��� (���֮ǰĿ�궪ʧ)
                        auto timeSinceLastValidTarget = std::chrono::duration<float>(
                            std::chrono::high_resolution_clock::now() - g_lastValidTargetTime).count();

                        if (!g_filterState.initialized ||
                            timeSinceLastValidTarget > g_predictionTime.load() * 1.5f ||
                            g_consecutiveInvalidPredictions > MAX_INVALID_PREDICTIONS) {
                            ResetFilterState(nearestTarget);
                        }
                        else {
                            UpdateFilter(nearestTarget);
                        }
                    }
                }

                // �����������ЧԤ�⵫û����Ŀ�꣬���������˲���״̬
                if (g_consecutiveInvalidPredictions > MAX_INVALID_PREDICTIONS && g_filterState.initialized) {
                    // ���˲������Ϊδ��ʼ����ǿ����һ����ЧĿ��ʱ����
                    g_filterState.initialized = false;
                    std::cout << "Filter marked as uninitialized due to consecutive invalid predictions" << std::endl;

                    // ����Ŀ�궪ʧ�ź�
                    PredictionResult lostSignal;
                    lostSignal.x = 999;
                    lostSignal.y = 999;
                    lostSignal.timestamp = std::chrono::high_resolution_clock::now();
                    g_predictionBuffer.write(lostSignal);

                    // ���ü�����
                    g_consecutiveInvalidPredictions = 0;
                    continue;
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
        g_consecutiveInvalidPredictions = 0; // ������ЧԤ�����

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