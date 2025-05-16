#include "PredictionModule.h"
#include "ConfigModule.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <opencv2/opencv.hpp>

namespace PredictionModule {
    std::atomic<bool> g_running{ false };
    std::atomic<bool> g_debugMode{ false };

    std::atomic<int> g_lastMouseMoveX{ 0 };
    std::atomic<int> g_lastMouseMoveY{ 0 };

    //无法达到的距离
    constexpr int INVALID_DISTANCE = 999;


    // 环形缓冲区
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    void RecordMouseMove(int dx, int dy) {
        g_lastMouseMoveX += dx;
        g_lastMouseMoveY += dy;
    }

    // 判断目标类别是否为有效目标
    bool IsTargetClassValid(int classId) {
        auto targetClasses = ConfigModule::GetTargetClasses();
        return std::find(targetClasses.begin(), targetClasses.end(), classId) != targetClasses.end();
    }

    DetectionResult FindNearestTarget(const std::vector<DetectionResult>& targets) {
        if (targets.empty()) {
            DetectionResult emptyResult;
            emptyResult.classId = -1;
            emptyResult.x = 0;
            emptyResult.y = 0;
            return emptyResult;
        }

        const int centerX = 160;
        const int centerY = 160;

        DetectionResult nearest = targets[0];
        float minDistance = INVALID_DISTANCE;

        for (const auto& target : targets) {
            if (target.classId >= 0 && IsTargetClassValid(target.classId)) {
                float dx = target.x - centerX;
                float dy = target.y - centerY;
                float distance = std::sqrt(dx * dx + dy * dy);

                if (distance < minDistance) {
                    minDistance = distance;
                    nearest = target;
                }
            }
        }

        if (minDistance == INVALID_DISTANCE) {
            nearest.classId = -1;
        }

        return nearest;
    }

    void PredictionModuleWorker() {
        std::cout << "Prediction module worker started" << std::endl;

        bool hadValidTarget = false;

        while (g_running) {
            try {
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                PredictionResult prediction;
                prediction.timestamp = std::chrono::high_resolution_clock::now();

                DetectionResult nearestTarget = FindNearestTarget(targets);

                if (nearestTarget.classId >= 0 && nearestTarget.x != 999) {
                    prediction.x = nearestTarget.x;
                    prediction.y = nearestTarget.y;
                    hadValidTarget = true;
                }
                else {
                    prediction.x = 999;
                    prediction.y = 999;
                    hadValidTarget = false;
                }

                g_predictionBuffer.write(prediction);

                if (g_debugMode.load() && hadValidTarget) {
                    DetectionModule::DrawPredictionPoint(prediction.x, prediction.y);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error in prediction module: " << e.what() << std::endl;
            }
        }
        std::cout << "Prediction module worker stopped" << std::endl;
    }

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

    void SetDebugMode(bool enabled) {
        g_debugMode = enabled;
    }

    bool IsDebugModeEnabled() {
        return g_debugMode.load();
    }

    void NotifyMouseMovement(int dx, int dy) {
        RecordMouseMove(dx, dy);
    }


}