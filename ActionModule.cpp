#include "ActionModule.h"
#include "IMouseController.h"
#include "WindowsAPIController.h"
//#include "LogitechController.h"
#include "HardwareController.h"
#include "IAimingStrategy.h"
#include "LongRangeStrategy.h"
#include "MidRangeStrategy.h"
#include "CloseRangeStrategy.h"
#include "MultiTargetStrategy.h"
#include "MovingTargetStrategy.h"
#include "HumanizeEngine.h"
#include "PredictionModule.h" // 引入预测模块头文件
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
        std::lock_guard<std::mutex> lock(instanceMutex); //1. 加锁，防止多线程同时创建实例
        if (!instance) {
            instance = new ActionModule(); // 3. 不存在则创建新实例
        }
        return *instance;
    }

    ActionModule::ActionModule()
        : running(false),                               // 1. 初始化 running 为 false
        currentStrategy(nullptr),                       // 2. 初始化 currentStrategy 为 nullptr
        humanizationLevel(50),                          // 3. 初始化 humanizationLevel 为 50
        currentControllerType(ControllerType::WINDOWS_API),       // 4. 初始化 currentControllerType
        currentStrategyType(StrategyType::AUTO_DETECT),           // 5. 初始化 currentStrategyType
        firing(false),                                  // 6. 初始化 firing 为 false
        screenWidth(1920),                              // 7. 初始化 screenWidth 为 1920
        screenHeight(1080),                             // 8. 初始化 screenHeight 为 1080
        imageSize(320)                                  // 9. 初始化 imageSize 为 320
    {
        // 获取屏幕尺寸
        screenWidth = GetSystemMetrics(SM_CXSCREEN);    // 10. 获取当前屏幕宽度
        screenHeight = GetSystemMetrics(SM_CYSCREEN);   // 11. 获取当前屏幕高度
    }

    ActionModule::~ActionModule() {
        Stop();
        Cleanup();
    }

    //初始化动作模块，接收控制器类型、策略类型和拟人化强度
    std::thread ActionModule::Initialize(ControllerType controllerType, StrategyType strategyType,int humanizationLevel) {

        auto& moduleInstance = GetInstance();

        // 设置参数
        moduleInstance.currentControllerType = controllerType;
        moduleInstance.currentStrategyType = strategyType;
        moduleInstance.SetHumanizationLevel(humanizationLevel);


        // 创建控制器
        moduleInstance.mouseController = moduleInstance.CreateController(controllerType);
        if (!moduleInstance.mouseController) {
            moduleInstance.mouseController = std::make_unique<WindowsAPIController>(); // 使用默认控制器
        }

        // 初始化控制器
        if (!moduleInstance.mouseController->Initialize()) {
            // 打印错误信息
            std::cerr << "Failed to initialize mouse controller" << std::endl;
        }

        // 加载瞄准策略
        moduleInstance.LoadStrategies();

        // 创建拟人化引擎
        moduleInstance.humanizeEngine = std::make_unique<HumanizeEngine>();
        moduleInstance.humanizeEngine->SetHumanizationLevel(humanizationLevel);

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

            // 清理当前控制器
            if (instance->mouseController) {
                instance->mouseController->Cleanup();
            }

            // 创建新控制器
            instance->currentControllerType = type;
            instance->mouseController = instance->CreateController(type);

            if (instance->mouseController) {
                instance->mouseController->Initialize();

                //打印控制器类型
                std::string controllerName = instance->mouseController->GetName();
                std::cout << "Mouse controller initialized: " << controllerName << std::endl;
                
            }
            else {
                std::cerr << "Failed to create mouse controller of type: " << static_cast<int>(type) << std::endl;

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

                   std::cout << "Strategy set to: " << instance->currentStrategy->GetName() << std::endl;
                }
            }
        }
    }

    void ActionModule::Fire() {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (instance && instance->mouseController) {
            instance->mouseController->LeftClick();

            auto [x, y] = instance->GetCurrentMousePosition();
            std::cout << "Mouse clicked at: (" << x << ", " << y << ")" << std::endl;
        }
    }

    void ActionModule::StartFiring() {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (instance && instance->mouseController) {
            instance->firing = true;
            instance->mouseController->LeftDown();

            auto [x, y] = instance->GetCurrentMousePosition();
         
        }
    }

    void ActionModule::StopFiring() {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (instance && instance->mouseController) {
            instance->firing = false;
            instance->mouseController->LeftUp();

            auto [x, y] = instance->GetCurrentMousePosition();

        }
    }

    void ActionModule::WorkerThread() {

        // 设置线程优先级
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

        // 当前鼠标位置
        int currentX = 0;
        int currentY = 0;

        // 上次处理时间
        auto lastProcessTime = std::chrono::high_resolution_clock::now();

        while (running) {
            // 目标帧率控制 
            auto now = std::chrono::high_resolution_clock::now();
            // 计算每帧经过的时间
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastProcessTime).count();

            // 控制帧率
            if (elapsed < 10) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            lastProcessTime = now;

            // 获取当前鼠标位置      这种写法是一种简洁的方式来处理一次性获取多个返回值的情况
            std::tie(currentX, currentY) = GetCurrentMousePosition();

            // 获取预测结果
            PredictionResult prediction;
            if (PredictionModule::GetLatestPrediction(prediction)) {
                ProcessPrediction(prediction);
            }

            // 检查预测模块是否还在运行
            if (!PredictionModule::IsRunning()) {
                std::cerr << "Prediction module stopped, stopping action module" << std::endl;
                running = false;
                break;
            }
        }

        
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
            std::cerr << "No valid strategy found" << std::endl;
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

            // 更新当前位置
            currentX = point.x;
            currentY = point.y;

            // 控制移动速率（60fps = ~16.67ms/frame）
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
                    std::cout << "Strategy changed to: " << currentStrategy->GetName() << std::endl;
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
        /*case ControllerType::LOGITECH:
            return std::make_unique<LogitechController>();*/
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