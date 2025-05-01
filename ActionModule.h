#ifndef ACTION_MODULE_H
#define ACTION_MODULE_H

#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>

// Ԥ����ģ��ӿ�
namespace ActionModule {
    class IMouseController;
    class IAimingStrategy;
    class HumanizeEngine;
}

// ����Ԥ��ģ��ӿ�
struct PredictionResult;

namespace ActionModule {

    enum class ControllerType {
        WINDOWS_API,
        LOGITECH,
        HARDWARE
    };

    enum class StrategyType {
        LONG_RANGE,
        MID_RANGE,
        CLOSE_RANGE,
        MULTI_TARGET,
        MOVING_TARGET,
        AUTO_DETECT
    };

    class ActionModule {
    public:
        // ��������
        static ActionModule& GetInstance();

        // ��ֹ�������ƶ�
        ActionModule(const ActionModule&) = delete;
        ActionModule& operator=(const ActionModule&) = delete;

        // ��ʼ��ģ�飨��main�������ã�
        static std::thread Initialize(
            ControllerType controllerType = ControllerType::WINDOWS_API,
            StrategyType strategyType = StrategyType::AUTO_DETECT,
            int humanizationLevel = 50);

        // ������Դ
        static void Cleanup();

        // ���ģ���Ƿ�������
        static bool IsRunning();

        // ֹͣģ��
        static void Stop();

        // �������˻�����1-100��
        static void SetHumanizationLevel(int level);

        // ��ȡ��ǰ���˻�����
        static int GetHumanizationLevel();

        // ���ÿ���������
        static void SetControllerType(ControllerType type);

        // ������׼��������
        static void SetStrategyType(StrategyType type);

        // �������
        static void Fire();
        static void StartFiring();
        static void StopFiring();

    private:
        ActionModule();
        ~ActionModule();

        // ģ�鹤���߳�
        void WorkerThread();

        // ����Ԥ����
        void ProcessPrediction(const PredictionResult& prediction);

        // �ƶ���굽Ŀ��
        void MoveToTarget(int targetX, int targetY);

        // ����Ŀ�궪ʧ���
        void HandleTargetLoss();

        // �жϵ�ǰ������ѡ����ʵĲ���
        std::string DetectCurrentScene();

        // ���ڳ���ѡ����Ѳ���
        void SelectBestStrategy(const std::string& scene);

        // ��ȡ��Ļ����
        std::pair<int, int> GetCurrentMousePosition();

        // ��Ԥ������ת��Ϊ��Ļ����
        std::pair<int, int> ConvertToScreenCoordinates(int predictionX, int predictionY);

        // ����ʵ��
        static ActionModule* instance;
        static std::mutex instanceMutex;

        // �߳����
        std::thread workerThread;
        std::atomic<bool> running;
        std::mutex controlMutex;
        std::condition_variable controlCV;

        // ģ�����
        std::unique_ptr<IMouseController> mouseController;
        std::vector<std::unique_ptr<IAimingStrategy>> strategies;
        IAimingStrategy* currentStrategy;
        std::unique_ptr<HumanizeEngine> humanizeEngine;

        // ״̬����
        std::atomic<int> humanizationLevel;
        ControllerType currentControllerType;
        StrategyType currentStrategyType;
        std::atomic<bool> firing;
        std::chrono::high_resolution_clock::time_point lastTargetTime;
        std::pair<int, int> lastValidTarget;
        int screenWidth;
        int screenHeight;
        int imageSize;

        // ����������
        std::unique_ptr<IMouseController> CreateController(ControllerType type);

        // �������в���
        void LoadStrategies();
    };

} // namespace ActionModule

#endif // ACTION_MODULE_H