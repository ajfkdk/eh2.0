#include "MouseActionLogger.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace ActionModule {

    MouseActionLogger& MouseActionLogger::GetInstance() {
        static MouseActionLogger instance;
        return instance;
    }

    bool MouseActionLogger::Initialize() {
        std::lock_guard<std::mutex> lock(logMutex);

        if (initialized) {
            return true;
        }

        // 确保logs目录存在
        std::filesystem::create_directories("logs");

        // 打开日志文件
        logFile.open("logs/mouseLog.log", std::ios::out | std::ios::app);

        if (!logFile.is_open()) {
            std::cerr << "Failed to open mouse action log file!" << std::endl;
            return false;
        }

        // 写入日志头
        logFile << "========================================" << std::endl;
        logFile << "Mouse Action Logger Initialized: " << GetTimeStamp() << std::endl;
        logFile << "========================================" << std::endl;

        initialized = true;
        return true;
    }

    void MouseActionLogger::LogMouseMovement(
        int fromX, int fromY, int toX, int toY, const std::string& strategyName) {

        std::lock_guard<std::mutex> lock(logMutex);

        if (!initialized || !logFile.is_open()) {
            return;
        }

        logFile << GetTimeStamp() << " | MOVE | "
            << "From: (" << fromX << "," << fromY << ") "
            << "To: (" << toX << "," << toY << ") "
            << "Strategy: " << strategyName << std::endl;
    }

    // 其他方法实现...
}