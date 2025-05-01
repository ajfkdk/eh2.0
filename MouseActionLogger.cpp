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

        // ȷ��logsĿ¼����
        std::filesystem::create_directories("logs");

        // ����־�ļ�
        logFile.open("logs/mouseLog.log", std::ios::out | std::ios::app);

        if (!logFile.is_open()) {
            std::cerr << "Failed to open mouse action log file!" << std::endl;
            return false;
        }

        // д����־ͷ
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

    // ��������ʵ��...
}