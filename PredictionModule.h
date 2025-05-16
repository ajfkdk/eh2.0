#ifndef PREDICTION_MODULE_H
#define PREDICTION_MODULE_H

#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include "DetectionModule.h"
#include "RingBuffer.h"

// 预测结果结构体
struct PredictionResult {
    int x;
    int y;
    std::chrono::high_resolution_clock::time_point timestamp;
};

namespace PredictionModule {
    std::thread Initialize();
    bool IsRunning();
    void Stop();
    void Cleanup();
    bool GetLatestPrediction(PredictionResult& result);
    void SetDebugMode(bool enabled);
    bool IsDebugModeEnabled();
    void NotifyMouseMovement(int dx, int dy);

}

#endif // PREDICTION_MODULE_H