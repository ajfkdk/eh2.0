#ifndef MOUSE_ACTION_LOGGER_H
#define MOUSE_ACTION_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>

namespace ActionModule {

    class MouseActionLogger {
    public:
        static MouseActionLogger& GetInstance();

        // 禁止拷贝和移动
        MouseActionLogger(const MouseActionLogger&) = delete;
        MouseActionLogger& operator=(const MouseActionLogger&) = delete;

        // 初始化和清理
        bool Initialize(const std::string& logPath = "logs/mouseLog.log");
        void Cleanup();

        // 记录鼠标动作
        void LogMouseMove(int fromX, int fromY, int toX, int toY,
            const std::string& controllerName,
            const std::string& strategyName);

        // 记录鼠标点击
        void LogMouseClick(int x, int y,
            const std::string& controllerName,
            const std::string& buttonType);

        // 记录一般事件
        void LogEvent(const std::string& eventType, const std::string& description);

    private:
        MouseActionLogger() = default;
        ~MouseActionLogger();

        std::mutex logMutex;
        std::ofstream logFile;
        bool isInitialized = false;

        // 获取当前时间戳
        std::string GetTimestamp();
    };

} // namespace ActionModule

#endif // MOUSE_ACTION_LOGGER_H