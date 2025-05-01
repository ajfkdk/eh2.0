#include "ActionModule.h"
#include "MouseController/IMouseController.h"
#include "MouseController/WindowsAPIController.h"
#include "MouseController/LogitechController.h"
#include "MouseController/HardwareController.h"
#include "AimingStrategy/IAimingStrategy.h"
#include "AimingStrategy/LongRangeStrategy.h"
#include "AimingStrategy/MidRangeStrategy.h"
#include "AimingStrategy/CloseRangeStrategy.h"
#include "AimingStrategy/MultiTargetStrategy.h"
#include "AimingStrategy/MovingTargetStrategy.h"
#include "HumanizeEngine/HumanizeEngine.h"
#include "Logger/MouseActionLogger.h"
#include <iostream>
#include <windows.h>
#include <chrono>

// 包含预测模块头文件
namespace PredictionModule {
    // 前向声明供ActionModule使用的预测模块接口
    bool GetLatestPrediction(PredictionResult& result);
    bool IsRunning();
}

namespace ActionModule {

    // 静态成员初始化
    ActionModule* ActionModule::instance = nullptr;
    std::mutex ActionModule::instanceMutex;

    ActionModule& ActionModule::GetInstance() {
        std::lock_guard<std::mutex> lock(instanceMutex);
        if (!instance) {
            instance = new ActionModule();
        }
        return *instance;
    }

    ActionModule::ActionModule()
        : running(false),
        currentStrategy(nullptr),
        humanizationLevel(50),
        currentControllerType(ControllerType::WINDOWS_API),
        currentStrategyType(StrategyType::AUTO_DETECT),
        firing(false),
        screenWidth(1920),
        screenHeight(1080),
        imageSize(320) {

        // 获取屏幕尺寸
        screenWidth = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);
    }

    ActionModule::~ActionModule() {
        Stop();
        Cleanup();
    }

    std::thread ActionModule::Initialize(
        ControllerType controllerType,
        StrategyType strategyType,
        int humanizationLevel) {

        auto& moduleInstance = GetInstance();

        // 设置参数
        moduleInstance.currentControllerType = controllerType;
        moduleInstance.currentStrategyType = strategyType;
        moduleInstance.SetHumanizationLevel(humanizationLevel);

        // 初始化日志
        MouseActionLogger::GetInstance().Initialize();
        MouseActionLogger::GetInstance().LogEvent("INIT", "ActionModule initializing");

        // 创建控制器
        moduleInstance.mouseController = moduleInstance.CreateController(controllerType);
        if (!moduleInstance.mouseController) {
            MouseActionLogger::GetInstance().LogEvent("ERROR", "Failed to create mouse controller");
            moduleInstance.mouseController = std::make_unique<WindowsAPIController>(); // 使用默认控制器
        }

        // 初始化控制器
        if (!moduleInstance.mouseController->Initialize()) {
            MouseActionLogger::GetInstance().LogEvent("ERROR", "Failed to initialize mouse controller");
        }

        // 加载瞄准策略
        moduleInstance.LoadStrategies();

        // 创建拟人化引擎
        moduleInstance.humanizeEngine = std::make_unique<HumanizeEngine>();
        moduleInstance.humanizeEngine->SetHumanizationLevel(humanizationLevel);

        // 记录初始化信息
        MouseActionLogger::GetInstance().LogEvent("INIT", "Using controller: " +
            moduleInstance.mouseController->GetName());
        MouseActionLogger::GetInstance().LogEvent("INIT", "Humanization level: " +
            std::to_string(humanizationLevel));

        // 启动工作线程
        moduleInstance.running = true;
        return std::thread(&ActionModule::WorkerThread, &moduleInstance);
    }

    void ActionModule::Cleanup() {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (instance) {
            // 停止线程
            instance->Stop();

            // 清理资源
            if (instance->mouseController) {
                instance->mouseController->Cleanup();
            }

            for (auto& strategy : instance->strategies) {
                if (strategy) {
                    strategy->Cleanup();
                }
            }

            // 清理日志
            MouseActionLogger::GetInstance().LogEvent("CLEANUP", "ActionModule shutting down");
            MouseActionLogger::GetInstance().Cleanup();

            // 删除实例
            delete instance;
            instance = nullptr;
        }
    }

    bool ActionModule::IsRunning() {
        std::lock_guard<std::mutex> lock(instanceMutex);
        return instance && instance->running;
    }

    void ActionModule::Stop() {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (instance) {
            instance->running = false;
            instance->controlCV.notify_all();

            if (instance->workerThread.joinable()) {
                instance->workerThread.join();
            }
        }
    }

    void ActionModule::SetHumanizationLevel(int level) {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (instance) {
            level = std::clamp(level, 1, 100);
            instance->humanizationLevel = level;

            if (instance->humanizeEngine) {
                instance->humanizeEngine->SetHumanizationLevel(level);
            }

            MouseActionLogger::GetInstance().LogEvent("CONFIG",
                "Humanization level set to: " + std::to_string(level));
        }
    }

    int ActionModule::GetHumanizationLevel() {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (instance) {
            return instance->humanizationLevel;
        }

        return 50; // 默认值
    }

    void ActionModule::SetControllerType(ControllerType type) {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (instance && instance->currentControllerType != type) {
            // 记录变更
            MouseActionLogger::GetInstance().LogEvent("CONFIG",
                "Changing controller type to: " + std::to_string(static_cast<int>(type)));

            // 清理当前控制器
            if (instance->mouseController) {
                instance->mouseController->Cleanup();
            }

            // 创建新控制器
            instance->currentControllerType = type;
            instance->mouseController = instance->CreateController(type);

            if (instance->mouseController) {
                instance->mouseController->Initialize();

                MouseActionLogger::GetInstance().LogEvent("CONFIG",
                    "Controller changed to: " + instance->mouseController->GetName());
            }
            else {
                MouseActionLogger::GetInstance().LogEvent("ERROR",
                    "Failed to create new controller, using default");

                instance->mouseController = std::make_unique<WindowsAPIController>();
                instance->mouseController->Initialize();
            }
        }
    }

    void ActionModule::SetStrategyType(StrategyType type) {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (instance) {
            instance->currentStrategyType = type;

            // 根据当前类型选择策略
            if (type != StrategyType::AUTO_DETECT && !instance->strategies.empty()) {
                size_t index = static_cast<size_t>(type);

                if (index < instance->strategies.size()) {
                    instance->currentStrategy = instance->strategies[index].get();

                    MouseActionLogger::GetInstance().LogEvent("CONFIG",
                        "Aiming strategy set to: " + instance->currentStrategy->GetName());
                }
            }
        }
    }

    void ActionModule::Fire() {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (instance && instance->mouseController) {
            instance->mouseController->LeftClick();

            auto [x, y] = instance->GetCurrentMousePosition();
            MouseActionLogger::GetInstance().LogMouseClick(x, y,
                instance->mouseController->GetName(), "LeftClick");
        }
    }

    void ActionModule::StartFiring() {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (instance && instance->mouseController) {
            instance->firing = true;
            instance->mouseController->LeftDown();

            auto [x, y] = instance->GetCurrentMousePosition();
            MouseActionLogger::GetInstance().LogMouseClick(x, y,
                instance->mouseController->GetName(), "LeftDown");
        }
    }

    void ActionModule::StopFiring() {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (instance && instance->mouseController) {
            instance->firing = false;
            instance->mouseController->LeftUp();

            auto [x, y] = instance->GetCurrentMousePosition();
            MouseActionLogger::GetInstance().LogMouseClick(x, y,
                instance->mouseController->GetName(), "LeftUp");
        }
    }

    void ActionModule::WorkerThread() {
        MouseActionLogger::GetInstance().LogEvent("THREAD", "Worker thread started");

        // 设置线程优先级
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

        // 当前鼠标位置
        int currentX = 0;
        int currentY = 0;

        // 上次处理时间
        auto lastProcessTime = std::chrono::high_resolution_clock::now();

        while (running) {
            // 目标帧率控制 (60fps = ~16.67ms/frame)
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastProcessTime).count();

            if (elapsed < 16) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            lastProcessTime = now;

            // 获取当前鼠标位置
            std::tie(currentX, currentY) = GetCurrentMousePosition();

            // 获取预测结果
            PredictionResult prediction;
            if (PredictionModule::GetLatestPrediction(prediction)) {
                ProcessPrediction(prediction);
            }

            // 检查预测模块是否还在运行
            if (!PredictionModule::IsRunning()) {
                MouseActionLogger::GetInstance().LogEvent("ERROR", "Prediction module stopped, stopping action module");
                running = false;
                break;
            }
        }

        MouseActionLogger::GetInstance().LogEvent("THREAD", "Worker thread stopped");
    }

    void ActionModule::ProcessPrediction(const PredictionResult& prediction) {
        // 检查是否有有效目标
        if (prediction.x != 999 && prediction.y != 999) {
            // 更新最后有效目标时间
            lastTargetTime = prediction.timestamp;

            // 转换预测坐标到屏幕坐标
            auto [screenX, screenY] = ConvertToScreenCoordinates(prediction.x, prediction.y);

            // 保存最后的有效目标
            lastValidTarget = { screenX, screenY };

            // 移动鼠标到目标
            MoveToTarget(screenX, screenY);
        }
        else {
            // 目标丢失，检查丢失时间
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsedSinceLastTarget = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastTargetTime).count();

            // 如果丢失超过一定时间（例如500ms），则执行目标丢失处理
            if (elapsedSinceLastTarget > 500) {
                HandleTargetLoss();
            }
        }
    }

    void ActionModule::MoveToTarget(int targetX, int targetY) {
        // 获取当前鼠标位置
        auto [currentX, currentY] = GetCurrentMousePosition();

        // 检测当前场景
        std::string scene = DetectCurrentScene();

        // 选择最佳策略
        SelectBestStrategy(scene);

        if (!currentStrategy) {
            MouseActionLogger::GetInstance().LogEvent("ERROR", "No aiming strategy selected");
            return;
        }

        // 计算瞄准路径
        std::vector<AimPoint> aimPath = currentStrategy->CalculateAimingPath(
            currentX, currentY, targetX, targetY, humanizeEngine.get());

        if (aimPath.empty()) {
            return;
        }

        // 执行鼠标移动
        for (const auto& point : aimPath) {
            if (!running) break;

            // 移动鼠标
            mouseController->MoveTo(point.x, point.y);

            // 记录日志
            MouseActionLogger::GetInstance().LogMouseMove(
                currentX, currentY, point.x, point.y,
                mouseController->GetName(), currentStrategy->GetName());

            // 更新当前位置
            currentX = point.x;
            currentY = point.y;

            // 控制移动速率（60fps = ~16.67ms/frame）
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    void ActionModule::HandleTargetLoss() {
        // 简单的目标丢失处理 - 小幅度随机移动模拟人类搜索行为
        auto [currentX, currentY] = GetCurrentMousePosition();

        // 生成一个随机方向的小移动
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distX(-10, 10);
        std::uniform_int_distribution<> distY(-10, 10);

        int offsetX = distX(gen);
        int offsetY = distY(gen);

        int newX = std::clamp(currentX + offsetX, 0, screenWidth - 1);
        int newY = std::clamp(currentY + offsetY, 0, screenHeight - 1);

        // 模拟人类搜索行为
        humanizeEngine->SimulateReactionDelay();

        // 缓慢移动到新位置
        mouseController->MoveTo(newX, newY);

        // 记录日志
        MouseActionLogger::GetInstance().LogMouseMove(
            currentX, currentY, newX, newY,
            mouseController->GetName(), "TargetLoss");
    }

    std::string ActionModule::DetectCurrentScene() {
        // 简单场景判断 - 基于目标距离屏幕中心的距离
        auto [currentX, currentY] = GetCurrentMousePosition();

        // 计算屏幕中心
        int centerX = screenWidth / 2;
        int centerY = screenHeight / 2;

        // 计算当前位置到中心的距离
        double distance = std::hypot(currentX - centerX, currentY - centerY);

        // 基于距离判断场景
        if (distance > screenHeight * 0.4) {
            return "long_range";
        }
        else if (distance > screenHeight * 0.2) {
            return "mid_range";
        }
        else {
            return "close_range";
        }
    }

    void ActionModule::SelectBestStrategy(const std::string& scene) {
        // 如果设置了固定策略，则使用该策略
        if (currentStrategyType != StrategyType::AUTO_DETECT) {
            size_t index = static_cast<size_t>(currentStrategyType);

            if (index < strategies.size()) {
                currentStrategy = strategies[index].get();
                return;
            }
        }

        // 否则自动选择最适合当前场景的策略
        for (const auto& strategy : strategies) {
            if (strategy->IsApplicableToScene(scene)) {
                if (currentStrategy != strategy.get()) {
                    currentStrategy = strategy.get();
                    MouseActionLogger::GetInstance().LogEvent("STRATEGY",
                        "Auto-selected strategy: " + currentStrategy->GetName() +
                        " for scene: " + scene);
                }
                return;
            }
        }

        // 如果没有找到合适的策略，使用默认的远距离策略
        if (!strategies.empty()) {
            currentStrategy = strategies[0].get(); // LongRangeStrategy
        }
    }

    std::pair<int, int> ActionModule::GetCurrentMousePosition() {
        POINT point;
        GetCursorPos(&point);
        return { point.x, point.y };
    }

    std::pair<int, int> ActionModule::ConvertToScreenCoordinates(int predictionX, int predictionY) {
        // 预测坐标是基于320x320的检测图像，位于屏幕中心
        int screenX = (screenWidth / 2) + (predictionX - imageSize / 2);
        int screenY = (screenHeight / 2) + (predictionY - imageSize / 2);

        // 确保坐标在屏幕范围内
        screenX = std::clamp(screenX, 0, screenWidth - 1);
        screenY = std::clamp(screenY, 0, screenHeight - 1);

        return { screenX, screenY };
    }

    std::unique_ptr<IMouseController> ActionModule::CreateController(ControllerType type) {
        switch (type) {
        case ControllerType::WINDOWS_API:
            return std::make_unique<WindowsAPIController>();
        case ControllerType::LOGITECH:
            return std::make_unique<LogitechController>();
        case ControllerType::HARDWARE:
            return std::make_unique<HardwareController>();
        default:
            return std::make_unique<WindowsAPIController>();
        }
    }

    void ActionModule::LoadStrategies() {
        // 清空现有策略
        strategies.clear();

        // 创建并添加所有策略
        strategies.push_back(std::make_unique<LongRangeStrategy>());
        strategies.push_back(std::make_unique<MidRangeStrategy>());
        strategies.push_back(std::make_unique<CloseRangeStrategy>());
        strategies.push_back(std::make_unique<MultiTargetStrategy>());
        strategies.push_back(std::make_unique<MovingTargetStrategy>());

        // 初始化所有策略
        for (auto& strategy : strategies) {
            strategy->Initialize();
        }

        // 默认使用远距离策略
        currentStrategy = strategies[0].get();
    }

} // namespace ActionModule