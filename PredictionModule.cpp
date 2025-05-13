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

    // �������˲�������
    std::atomic<float> g_processNoise{ 0.01f };     // �������� - ��СֵʹԤ���ƽ������Ӧ����
    std::atomic<float> g_measurementNoise{ 0.1f };  // �������� - �ϴ�ֵ���ٶ����������ӳ�

    // Ŀ����ٲ���
    std::atomic<float> g_maxTargetSwitchDistance{ 50.0f }; // ���Ŀ���л����루���أ�
    std::atomic<int> g_maxLostFrames{ 30 };               // ���ʧ֡������30֡��1��@30fps��

    // ���λ����������ڴ洢Ԥ����
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // �������˲�����ر���
    cv::KalmanFilter g_kalmanFilter(4, 2, 0);   // ״̬����(x,y,vx,vy)����������(x,y)
    cv::Mat g_kalmanMeasurement;                // ��������

    // ����״̬����
    bool g_isTracking = false;           // �Ƿ����ڸ���Ŀ��
    int g_lostFrameCount = 0;            // ������ʧ֡����
    int g_currentTargetClassId = -1;     // ��ǰ���ٵ�Ŀ������
    float g_lastX = 0.0f, g_lastY = 0.0f; // ��һ��Ԥ���λ��

    // ��ʼ���������˲���
    void InitKalmanFilter() {
        // ״̬ת�ƾ��� - ����λ�ú��ٶȵ�����ģ��
        // [1, 0, 1, 0]
        // [0, 1, 0, 1]
        // [0, 0, 1, 0]
        // [0, 0, 0, 1]
        g_kalmanFilter.transitionMatrix = (cv::Mat_<float>(4, 4) <<
            1, 0, 1, 0,
            0, 1, 0, 1,
            0, 0, 1, 0,
            0, 0, 0, 1);

        // �������� - ֻ����λ�ã��������ٶ�
        // [1, 0, 0, 0]
        // [0, 1, 0, 0]
        g_kalmanFilter.measurementMatrix = (cv::Mat_<float>(2, 4) <<
            1, 0, 0, 0,
            0, 1, 0, 0);

        // ��������Э�������
        float q = g_processNoise.load();
        g_kalmanFilter.processNoiseCov = (cv::Mat_<float>(4, 4) <<
            q, 0, 0, 0,
            0, q, 0, 0,
            0, 0, q * 2, 0,
            0, 0, 0, q * 2);

        // ��������Э�������
        float r = g_measurementNoise.load();
        g_kalmanFilter.measurementNoiseCov = (cv::Mat_<float>(2, 2) <<
            r, 0,
            0, r);

        // �������Э�������
        g_kalmanFilter.errorCovPost = cv::Mat::eye(4, 4, CV_32F);

        // ��ʼ����������
        g_kalmanMeasurement = cv::Mat::zeros(2, 1, CV_32F);
    }

    // ���³�ʼ���������˲���������Ŀ���л�
    void ResetKalmanFilter(float x, float y, float vx = 0.0f, float vy = 0.0f) {
        // ����״̬�������ó�ʼλ�ú��ٶ�
        g_kalmanFilter.statePost.at<float>(0) = x;
        g_kalmanFilter.statePost.at<float>(1) = y;
        g_kalmanFilter.statePost.at<float>(2) = vx;
        g_kalmanFilter.statePost.at<float>(3) = vy;

        // ���ú������Э����
        g_kalmanFilter.errorCovPost = cv::Mat::eye(4, 4, CV_32F);
    }

    // ����������ŷ�Ͼ���
    float CalculateDistance(float x1, float y1, float x2, float y2) {
        float dx = x1 - x2;
        float dy = y1 - y2;
        return std::sqrt(dx * dx + dy * dy);
    }

    // �Ӽ�����в������Ŀ��
    DetectionResult FindBestTarget(const std::vector<DetectionResult>& targets) {
        if (targets.empty()) {
            DetectionResult emptyResult;
            emptyResult.classId = -1;
            return emptyResult;
        }

        // ��Ļ�������� (����320x320��ͼ��)
        const int centerX = 160;
        const int centerY = 160;

        // ���˳���ЧĿ�ֻ꣨����classId=1��3����ͷĿ�꣩
        std::vector<DetectionResult> validTargets;
        for (const auto& target : targets) {
            if (target.classId == 1 || target.classId == 3) {
                validTargets.push_back(target);
            }
        }

        if (validTargets.empty()) {
            DetectionResult emptyResult;
            emptyResult.classId = -1;
            return emptyResult;
        }

        // ����Ѿ��ڸ���Ŀ��
        if (g_isTracking) {
            // �����ҵ��뵱ǰ����Ŀ����ӽ���Ŀ��
            DetectionResult closestTarget;
            float minDistance = g_maxTargetSwitchDistance.load();
            bool foundMatch = false;

            for (const auto& target : validTargets) {
                float distance = CalculateDistance(g_lastX, g_lastY, target.x, target.y);
                if (distance < minDistance) {
                    minDistance = distance;
                    closestTarget = target;
                    foundMatch = true;
                }
            }

            // ����ҵ�ƥ���Ŀ�꣬������
            if (foundMatch) {
                return closestTarget;
            }
        }

        // ���û���ڸ���Ŀ�꣬��û���ҵ�ƥ���Ŀ�꣬���������������Ŀ��
        DetectionResult nearest = validTargets[0];
        float minDistance = std::numeric_limits<float>::max();

        for (const auto& target : validTargets) {
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

        // ��ʼ���������˲���
        InitKalmanFilter();

        // ����״̬����
        g_isTracking = false;
        g_lostFrameCount = 0;
        g_currentTargetClassId = -1;

        while (g_running) {
            try {
                // ��ȡ���м��Ŀ��
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                // ����Ԥ����
                PredictionResult prediction;
                prediction.timestamp = std::chrono::high_resolution_clock::now();
                prediction.isTracking = false;
                prediction.targetClassId = -1;

                // �������˲���Ԥ�ⲽ��
                cv::Mat predictedState = g_kalmanFilter.predict();
                float predictedX = predictedState.at<float>(0);
                float predictedY = predictedState.at<float>(1);
                float predictedVX = predictedState.at<float>(2);
                float predictedVY = predictedState.at<float>(3);

                // �������Ŀ��
                DetectionResult bestTarget = FindBestTarget(targets);

                // ����ҵ���ЧĿ��
                if (bestTarget.classId >= 0) {
                    // Ŀ����Ч
                    g_lostFrameCount = 0;

                    // ����������ڸ��ٵ�Ŀ�꣬����Ŀ�����ͱ仯
                    if (!g_isTracking || g_currentTargetClassId != bestTarget.classId) {
                        // ���³�ʼ���������˲���
                        ResetKalmanFilter(bestTarget.x, bestTarget.y);
                        g_isTracking = true;
                        g_currentTargetClassId = bestTarget.classId;
                    }
                    else {
                        // �������˲������²���
                        g_kalmanMeasurement.at<float>(0) = bestTarget.x;
                        g_kalmanMeasurement.at<float>(1) = bestTarget.y;
                        cv::Mat correctedState = g_kalmanFilter.correct(g_kalmanMeasurement);

                        // ��У�����״̬��ȡλ�ú��ٶ�
                        predictedX = correctedState.at<float>(0);
                        predictedY = correctedState.at<float>(1);
                        predictedVX = correctedState.at<float>(2);
                        predictedVY = correctedState.at<float>(3);
                    }

                    // ����Ԥ����
                    prediction.x = static_cast<int>(std::round(predictedX));
                    prediction.y = static_cast<int>(std::round(predictedY));
                    prediction.vx = predictedVX;
                    prediction.vy = predictedVY;
                    prediction.isTracking = true;
                    prediction.targetClassId = bestTarget.classId;

                    // ������һ�ε�λ��
                    g_lastX = predictedX;
                    g_lastY = predictedY;
                }
                else {
                    // Ŀ�궪ʧ
                    g_lostFrameCount++;

                    // �����ʧ֡��δ������ֵ��ʹ�ÿ�����Ԥ���λ��
                    if (g_isTracking && g_lostFrameCount <= g_maxLostFrames.load()) {
                        // ʹ��Ԥ���λ��
                        prediction.x = static_cast<int>(std::round(predictedX));
                        prediction.y = static_cast<int>(std::round(predictedY));
                        prediction.vx = predictedVX;
                        prediction.vy = predictedVY;
                        prediction.isTracking = true;
                        prediction.targetClassId = g_currentTargetClassId;

                        // ������һ�ε�λ��
                        g_lastX = predictedX;
                        g_lastY = predictedY;
                    }
                    else {
                        // Ŀ�곹�׶�ʧ�����ø���״̬
                        prediction.x = 999;
                        prediction.y = 999;
                        prediction.vx = 0;
                        prediction.vy = 0;
                        prediction.isTracking = false;
                        prediction.targetClassId = -1;
                        g_isTracking = false;
                    }
                }

                // ��Ԥ����д�뻷�λ�����
                g_predictionBuffer.write(prediction);

                // debugģʽ���� - ����ģ���ṩԤ�����Ϣ
                if (g_debugMode.load() && prediction.isTracking) {
                    // ���ü��ģ��Ļ���Ԥ��㺯��
                    DetectionModule::DrawPredictionPoint(prediction.x, prediction.y);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error in prediction module: " << e.what() << std::endl;
            }

            // ����ѭ��Ƶ�ʣ��������CPUʹ����
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

    // ���ÿ������˲�������
    void SetProcessNoise(float q) {
        g_processNoise = q;
        InitKalmanFilter(); // ���³�ʼ���������˲���
    }

    void SetMeasurementNoise(float r) {
        g_measurementNoise = r;
        InitKalmanFilter(); // ���³�ʼ���������˲���
    }

    // ����Ŀ����ٲ���
    void SetMaxTargetSwitchDistance(float distance) {
        g_maxTargetSwitchDistance = distance;
    }

    void SetMaxLostFrames(int frames) {
        g_maxLostFrames = frames;
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