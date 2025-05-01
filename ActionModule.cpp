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
#include "PredictionModule.h" // ����Ԥ��ģ��ͷ�ļ�
#include <iostream>
#include <windows.h>
#include <chrono>

// ����Ԥ��ģ��ͷ�ļ�
namespace PredictionModule {
    // ǰ��������ActionModuleʹ�õ�Ԥ��ģ��ӿ�
    bool GetLatestPrediction(PredictionResult& result);
    bool IsRunning();
}

namespace ActionModule {

    // ��̬��Ա��ʼ��
    ActionModule* ActionModule::instance = nullptr;
    std::mutex ActionModule::instanceMutex;

    ActionModule& ActionModule::GetInstance() {
        std::lock_guard<std::mutex> lock(instanceMutex); //1. ��������ֹ���߳�ͬʱ����ʵ��
        if (!instance) {
            instance = new ActionModule(); // 3. �������򴴽���ʵ��
        }
        return *instance;
    }

    ActionModule::ActionModule()
        : running(false),                               // 1. ��ʼ�� running Ϊ false
        currentStrategy(nullptr),                       // 2. ��ʼ�� currentStrategy Ϊ nullptr
        humanizationLevel(50),                          // 3. ��ʼ�� humanizationLevel Ϊ 50
        currentControllerType(ControllerType::WINDOWS_API),       // 4. ��ʼ�� currentControllerType
        currentStrategyType(StrategyType::AUTO_DETECT),           // 5. ��ʼ�� currentStrategyType
        firing(false),                                  // 6. ��ʼ�� firing Ϊ false
        screenWidth(1920),                              // 7. ��ʼ�� screenWidth Ϊ 1920
        screenHeight(1080),                             // 8. ��ʼ�� screenHeight Ϊ 1080
        imageSize(320)                                  // 9. ��ʼ�� imageSize Ϊ 320
    {
        // ��ȡ��Ļ�ߴ�
        screenWidth = GetSystemMetrics(SM_CXSCREEN);    // 10. ��ȡ��ǰ��Ļ���
        screenHeight = GetSystemMetrics(SM_CYSCREEN);   // 11. ��ȡ��ǰ��Ļ�߶�
    }

    ActionModule::~ActionModule() {
        Stop();
        Cleanup();
    }

    //��ʼ������ģ�飬���տ��������͡��������ͺ����˻�ǿ��
    std::thread ActionModule::Initialize(ControllerType controllerType, StrategyType strategyType,int humanizationLevel) {

        auto& moduleInstance = GetInstance();

        // ���ò���
        moduleInstance.currentControllerType = controllerType;
        moduleInstance.currentStrategyType = strategyType;
        moduleInstance.SetHumanizationLevel(humanizationLevel);


        // ����������
        moduleInstance.mouseController = moduleInstance.CreateController(controllerType);
        if (!moduleInstance.mouseController) {
            moduleInstance.mouseController = std::make_unique<WindowsAPIController>(); // ʹ��Ĭ�Ͽ�����
        }

        // ��ʼ��������
        if (!moduleInstance.mouseController->Initialize()) {
            // ��ӡ������Ϣ
            std::cerr << "Failed to initialize mouse controller" << std::endl;
        }

        // ������׼����
        moduleInstance.LoadStrategies();

        // �������˻�����
        moduleInstance.humanizeEngine = std::make_unique<HumanizeEngine>();
        moduleInstance.humanizeEngine->SetHumanizationLevel(humanizationLevel);

        // ���������߳�
        moduleInstance.running = true;
        return std::thread(&ActionModule::WorkerThread, &moduleInstance);
    }

    void ActionModule::Cleanup() {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (instance) {
            // ֹͣ�߳�
            instance->Stop();

            // ������Դ
            if (instance->mouseController) {
                instance->mouseController->Cleanup();
            }

            for (auto& strategy : instance->strategies) {
                if (strategy) {
                    strategy->Cleanup();
                }
            }


            // ɾ��ʵ��
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

        return 50; // Ĭ��ֵ
    }

    void ActionModule::SetControllerType(ControllerType type) {
        std::lock_guard<std::mutex> lock(instanceMutex);

        if (instance && instance->currentControllerType != type) {

            // ����ǰ������
            if (instance->mouseController) {
                instance->mouseController->Cleanup();
            }

            // �����¿�����
            instance->currentControllerType = type;
            instance->mouseController = instance->CreateController(type);

            if (instance->mouseController) {
                instance->mouseController->Initialize();

                //��ӡ����������
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

            // ���ݵ�ǰ����ѡ�����
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

        // �����߳����ȼ�
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

        // ��ǰ���λ��
        int currentX = 0;
        int currentY = 0;

        // �ϴδ���ʱ��
        auto lastProcessTime = std::chrono::high_resolution_clock::now();

        while (running) {
            // Ŀ��֡�ʿ��� 
            auto now = std::chrono::high_resolution_clock::now();
            // ����ÿ֡������ʱ��
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastProcessTime).count();

            // ����֡��
            if (elapsed < 10) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            lastProcessTime = now;

            // ��ȡ��ǰ���λ��      ����д����һ�ּ��ķ�ʽ������һ���Ի�ȡ�������ֵ�����
            std::tie(currentX, currentY) = GetCurrentMousePosition();

            // ��ȡԤ����
            PredictionResult prediction;
            if (PredictionModule::GetLatestPrediction(prediction)) {
                ProcessPrediction(prediction);
            }

            // ���Ԥ��ģ���Ƿ�������
            if (!PredictionModule::IsRunning()) {
                std::cerr << "Prediction module stopped, stopping action module" << std::endl;
                running = false;
                break;
            }
        }

        
    }

    void ActionModule::ProcessPrediction(const PredictionResult& prediction) {
        // ����Ƿ�����ЧĿ��
        if (prediction.x != 999 && prediction.y != 999) {
            // ���������ЧĿ��ʱ��
            lastTargetTime = prediction.timestamp;

            // ת��Ԥ�����굽��Ļ����
            auto [screenX, screenY] = ConvertToScreenCoordinates(prediction.x, prediction.y);

            // ����������ЧĿ��
            lastValidTarget = { screenX, screenY };

            // �ƶ���굽Ŀ��
            MoveToTarget(screenX, screenY);
        }
        else {
            // Ŀ�궪ʧ����鶪ʧʱ��
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsedSinceLastTarget = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastTargetTime).count();

            // �����ʧ����һ��ʱ�䣨����500ms������ִ��Ŀ�궪ʧ����
            if (elapsedSinceLastTarget > 500) {
                HandleTargetLoss();
            }
        }
    }

    void ActionModule::MoveToTarget(int targetX, int targetY) {
        // ��ȡ��ǰ���λ��
        auto [currentX, currentY] = GetCurrentMousePosition();

        // ��⵱ǰ����
        std::string scene = DetectCurrentScene();

        // ѡ����Ѳ���
        SelectBestStrategy(scene);

        if (!currentStrategy) {
            std::cerr << "No valid strategy found" << std::endl;
            return;
        }

        // ������׼·��
        std::vector<AimPoint> aimPath = currentStrategy->CalculateAimingPath(
            currentX, currentY, targetX, targetY, humanizeEngine.get());

        if (aimPath.empty()) {
            return;
        }

        // ִ������ƶ�
        for (const auto& point : aimPath) {
            if (!running) break;

            // �ƶ����
            mouseController->MoveTo(point.x, point.y);

            // ���µ�ǰλ��
            currentX = point.x;
            currentY = point.y;

            // �����ƶ����ʣ�60fps = ~16.67ms/frame��
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void ActionModule::HandleTargetLoss() {
        // �򵥵�Ŀ�궪ʧ���� - С��������ƶ�ģ������������Ϊ
        auto [currentX, currentY] = GetCurrentMousePosition();

        // ����һ����������С�ƶ�
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distX(-10, 10);
        std::uniform_int_distribution<> distY(-10, 10);

        int offsetX = distX(gen);
        int offsetY = distY(gen);

        int newX = std::clamp(currentX + offsetX, 0, screenWidth - 1);
        int newY = std::clamp(currentY + offsetY, 0, screenHeight - 1);

        // ģ������������Ϊ
        humanizeEngine->SimulateReactionDelay();

        // �����ƶ�����λ��
        mouseController->MoveTo(newX, newY);
    }

    std::string ActionModule::DetectCurrentScene() {
        // �򵥳����ж� - ����Ŀ�������Ļ���ĵľ���
        auto [currentX, currentY] = GetCurrentMousePosition();

        // ������Ļ����
        int centerX = screenWidth / 2;
        int centerY = screenHeight / 2;

        // ���㵱ǰλ�õ����ĵľ���
        double distance = std::hypot(currentX - centerX, currentY - centerY);

        // ���ھ����жϳ���
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
        // ��������˹̶����ԣ���ʹ�øò���
        if (currentStrategyType != StrategyType::AUTO_DETECT) {
            size_t index = static_cast<size_t>(currentStrategyType);

            if (index < strategies.size()) {
                currentStrategy = strategies[index].get();
                return;
            }
        }

        // �����Զ�ѡ�����ʺϵ�ǰ�����Ĳ���
        for (const auto& strategy : strategies) {
            if (strategy->IsApplicableToScene(scene)) {
                if (currentStrategy != strategy.get()) {
                    currentStrategy = strategy.get();
                    std::cout << "Strategy changed to: " << currentStrategy->GetName() << std::endl;
                }
                return;
            }
        }

        // ���û���ҵ����ʵĲ��ԣ�ʹ��Ĭ�ϵ�Զ�������
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
        // Ԥ�������ǻ���320x320�ļ��ͼ��λ����Ļ����
        int screenX = (screenWidth / 2) + (predictionX - imageSize / 2);
        int screenY = (screenHeight / 2) + (predictionY - imageSize / 2);

        // ȷ����������Ļ��Χ��
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
        // ������в���
        strategies.clear();

        // ������������в���
        strategies.push_back(std::make_unique<LongRangeStrategy>());
        strategies.push_back(std::make_unique<MidRangeStrategy>());
        strategies.push_back(std::make_unique<CloseRangeStrategy>());
        strategies.push_back(std::make_unique<MultiTargetStrategy>());
        strategies.push_back(std::make_unique<MovingTargetStrategy>());

        // ��ʼ�����в���
        for (auto& strategy : strategies) {
            strategy->Initialize();
        }

        // Ĭ��ʹ��Զ�������
        currentStrategy = strategies[0].get();
    }

} // namespace ActionModule