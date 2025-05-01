#ifndef ACTION_MODULE_H
#define ACTION_MODULE_H

#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>

// 预定义模块接口
namespace ActionModule {
    class IMouseController;
    class IAimingStrategy;
    class HumanizeEngine;
}

// 引入预测模块接口
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
        // 单例访问
        static ActionModule& GetInstance();

        // 禁止拷贝和移动
        ActionModule(const ActionModule&) = delete;
        ActionModule& operator=(const ActionModule&) = delete;

        // 初始化模块（供main函数调用）
        static std::thread Initialize(
            ControllerType controllerType = ControllerType::WINDOWS_API,
            StrategyType strategyType = StrategyType::AUTO_DETECT,
            int humanizationLevel = 50);

        // 清理资源
        static void Cleanup();

        // 检查模块是否在运行
        static bool IsRunning();

        // 停止模块
        static void Stop();

        // 设置拟人化级别（1-100）
        static void SetHumanizationLevel(int level);

        // 获取当前拟人化级别
        static int GetHumanizationLevel();

        // 设置控制器类型
        static void SetControllerType(ControllerType type);

        // 设置瞄准策略类型
        static void SetStrategyType(StrategyType type);

        // 开火控制
        static void Fire();
        static void StartFiring();
        static void StopFiring();

    private:
        ActionModule();
        ~ActionModule();

        // 模块工作线程
        void WorkerThread();

        // 处理预测结果
        void ProcessPrediction(const PredictionResult& prediction);

        // 移动鼠标到目标
        void MoveToTarget(int targetX, int targetY);

        // 处理目标丢失情况
        void HandleTargetLoss();

        // 判断当前场景并选择合适的策略
        std::string DetectCurrentScene();

        // 基于场景选择最佳策略
        void SelectBestStrategy(const std::string& scene);

        // 获取屏幕坐标
        std::pair<int, int> GetCurrentMousePosition();

        // 将预测坐标转换为屏幕坐标
        std::pair<int, int> ConvertToScreenCoordinates(int predictionX, int predictionY);

        // 单例实例
        static ActionModule* instance;
        static std::mutex instanceMutex;

        // 线程相关
        std::thread workerThread;
        std::atomic<bool> running;
        std::mutex controlMutex;
        std::condition_variable controlCV;

        // 模块组件
        std::unique_ptr<IMouseController> mouseController;
        std::vector<std::unique_ptr<IAimingStrategy>> strategies;
        IAimingStrategy* currentStrategy;
        std::unique_ptr<HumanizeEngine> humanizeEngine;

        // 状态变量
        std::atomic<int> humanizationLevel;
        ControllerType currentControllerType;
        StrategyType currentStrategyType;
        std::atomic<bool> firing;
        std::chrono::high_resolution_clock::time_point lastTargetTime;
        std::pair<int, int> lastValidTarget;
        int screenWidth;
        int screenHeight;
        int imageSize;

        // 创建控制器
        std::unique_ptr<IMouseController> CreateController(ControllerType type);

        // 加载所有策略
        void LoadStrategies();
    };

} // namespace ActionModule

#endif // ACTION_MODULE_H