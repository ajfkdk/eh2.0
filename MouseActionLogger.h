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

        // ��ֹ�������ƶ�
        MouseActionLogger(const MouseActionLogger&) = delete;
        MouseActionLogger& operator=(const MouseActionLogger&) = delete;

        // ��ʼ��������
        bool Initialize(const std::string& logPath = "logs/mouseLog.log");
        void Cleanup();

        // ��¼��궯��
        void LogMouseMove(int fromX, int fromY, int toX, int toY,
            const std::string& controllerName,
            const std::string& strategyName);

        // ��¼�����
        void LogMouseClick(int x, int y,
            const std::string& controllerName,
            const std::string& buttonType);

        // ��¼һ���¼�
        void LogEvent(const std::string& eventType, const std::string& description);

    private:
        MouseActionLogger() = default;
        ~MouseActionLogger();

        std::mutex logMutex;
        std::ofstream logFile;
        bool isInitialized = false;

        // ��ȡ��ǰʱ���
        std::string GetTimestamp();
    };

} // namespace ActionModule

#endif // MOUSE_ACTION_LOGGER_H