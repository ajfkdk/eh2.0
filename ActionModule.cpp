#include "ActionModule.h"
#include "HumanLikeMovement.h"
#include "WindowsMouseController.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <Windows.h>
#include "PredictionModule.h"

// 静态成员初始化
std::thread ActionModule::actionThread;
std::atomic<bool> ActionModule::running(false);
std::unique_ptr<MouseController> ActionModule::mouseController = nullptr;
std::atomic<int> ActionModule::humanizationFactor(50);
std::atomic<bool> ActionModule::fireEnabled(false);
std::mutex ActionModule::logMutex;
int ActionModule::currentX = 0;
int ActionModule::currentY = 0;
int ActionModule::targetX = 0;
int ActionModule::targetY = 0;
std::chrono::high_resolution_clock::time_point ActionModule::lastUpdateTime;

std::thread ActionModule::Initialize(int humanFactor) {
    // 确保范围在1-100之间
    humanFactor = max(1, min(100, humanFactor));
    humanizationFactor.store(humanFactor);

    // 创建日志目录
    std::filesystem::create_directories("logs");

    // 初始化拟人化移动
    HumanLikeMovement::Initialize();

    // 如果没有设置鼠标控制器，使用Windows默认实现
    if (!mouseController) {
        //创建Windows鼠标控制器的智能指针
        mouseController = std::make_unique<WindowsMouseController>();
    }

    // 获取当前鼠标位置
    UpdateCurrentMousePosition();

    // 记录初始化信息
    std::stringstream ss;
    ss << "ActionModule initialized with humanization factor: " << humanFactor;
    LogMouseMovement(ss.str());

    // 设置运行标志
    running.store(true);

    // 启动处理线程
    actionThread = std::thread(ProcessLoop);

    // return actionThread; std::thread 不能被拷贝（copy），只能被移动（move） 所以需要使用std::move 把thread的所有权转移到调用者
    return std::move(actionThread);;
}

void ActionModule::Cleanup() {
    // 停止模块
    Stop();

    // 等待线程结束
    if (actionThread.joinable()) {
        actionThread.join();
    }

    // 记录清理信息
    LogMouseMovement("ActionModule cleaned up");
}

void ActionModule::Stop() {
    running.store(false);
}

bool ActionModule::IsRunning() {
    return running.load();
}

void ActionModule::SetHumanizationFactor(int factor) {
    // 确保范围在1-100之间
    factor = max(1, min(100, factor));
    humanizationFactor.store(factor);

    // 记录参数变更
    std::stringstream ss;
    ss << "Humanization factor set to: " << factor;
    LogMouseMovement(ss.str());
}

int ActionModule::GetHumanizationFactor() {
    return humanizationFactor.load();
}

void ActionModule::SetMouseController(std::unique_ptr<MouseController> controller) {
    mouseController = std::move(controller);
}

void ActionModule::EnableAutoFire(bool enable) {
    fireEnabled.store(enable);

    // 记录开火状态变更
    std::stringstream ss;
    ss << "Auto fire " << (enable ? "enabled" : "disabled");
    LogMouseMovement(ss.str());
}

bool ActionModule::IsAutoFireEnabled() {
    return fireEnabled.load();
}

void ActionModule::LogMouseMovement(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);

    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm now_tm;
    localtime_s(&now_tm, &now_time_t);

    // 打开日志文件
    std::ofstream logFile("logs/mouseLog.log", std::ios::app);
    if (logFile.is_open()) {
        // 写入时间戳和消息
        logFile << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count()
            << " - " << message << std::endl;
        logFile.close();
    }
}

// 动作模块主要的处理循环
void ActionModule::ProcessLoop() {
    // 设置处理循环的起始时间
    lastUpdateTime = std::chrono::high_resolution_clock::now();//  ---》获取系统当前时间点的工具，精度高，适合用于测量代码执行时间或者时间戳

    // 创建用于缓存路径点的变量
    std::vector<std::pair<float, float>> currentPath;
    size_t pathIndex = 0;
    bool targetLost = false;

    // 处理主循环
    while (running.load()) {
        // 获取当前时间
        auto currentTime = std::chrono::high_resolution_clock::now();

        // 计算距离上次更新经过的时间
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastUpdateTime).count();

        // 获取最新鼠标位置
        UpdateCurrentMousePosition();

        // 获取最新预测结果
        PredictionResult prediction;
        bool hasPrediction = PredictionModule::GetLatestPrediction(prediction);

        // 判断目标是否丢失
        if (!hasPrediction || prediction.x == 999 || prediction.y == 999) {
            // 如果是第一次丢失目标
            if (!targetLost) {
                targetLost = true;

                // 生成拟人化丢失目标行为
                currentPath = HumanLikeMovement::GenerateTargetLossMovement(
                    currentX, currentY, humanizationFactor.load(), 30);
                pathIndex = 0;

                LogMouseMovement("Target lost, generating human-like movement");
            }

            // 控制鼠标开火状态
            ControlMouseFire(false);

            // 如果仍在路径中，继续执行丢失目标的拟人化移动
            if (pathIndex < currentPath.size()) {
                auto nextPoint = currentPath[pathIndex];
                mouseController->MoveTo(static_cast<int>(nextPoint.first), static_cast<int>(nextPoint.second));

                std::stringstream ss;
                ss << "Target loss movement: (" << nextPoint.first << ", " << nextPoint.second << ")";
                LogMouseMovement(ss.str());

                pathIndex++;
            }
        }
        else {
            // 目标未丢失
            targetLost = false;

            // 获取屏幕分辨率
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);

            // 计算屏幕中心点
            int screenCenterX = screenWidth / 2;
            int screenCenterY = screenHeight / 2;

            // 图像中心的左上角偏移到屏幕中心的图像左上角
            float offsetX = screenCenterX - 320.0f / 2;
            float offsetY = screenCenterY - 320.0f / 2;

            // 计算目标坐标
            targetX = static_cast<int>(offsetX + prediction.x);
            targetY = static_cast<int>(offsetY + prediction.y);


            std::cout << "Screen resolution: " << screenWidth << "x" << screenHeight << std::endl;
            std::cout << "Raw prediction: (" << prediction.x << ", " << prediction.y << ")" << std::endl;
            std::cout << "Converted target: (" << targetX << ", " << targetY << ")" << std::endl;

            // 日志记录新的坐标转换方法
            std::stringstream ssCoord;
            ssCoord << "Using direct prediction coordinates: (" << targetX << ", " << targetY << ")";
            LogMouseMovement(ssCoord.str());

            // 计算距离
            float distance = std::sqrt(
                std::pow(targetX - currentX, 2) + std::pow(targetY - currentY, 2));

            // 新增: 检查鼠标是否已经足够靠近目标
            if (distance < AIM_THRESHOLD) {
                // 如果距离小于阈值，认为已经瞄准了目标
                std::stringstream ss;
                ss << "Target already aimed. Distance: " << distance << " (below threshold " << AIM_THRESHOLD << ")";
                LogMouseMovement(ss.str());

                // 控制鼠标开火
                ControlMouseFire(true);

                // 不需要生成新的路径
                currentPath.clear();
                pathIndex = 0;
            }
            // 如果距离足够大，生成新的拟人化路径
            else if (distance > 5 || currentPath.empty() || pathIndex >= currentPath.size()) {
                int human = humanizationFactor.load();
                currentPath = HumanLikeMovement::GenerateBezierPath(
                    currentX, currentY, targetX, targetY, human,
                    max(10, static_cast<int>(distance / 5)));

                // 应用速度曲线
                currentPath = HumanLikeMovement::ApplySpeedProfile(currentPath, human);
                pathIndex = 0;

                std::stringstream ss;
                ss << "New path generated to target: (" << targetX << ", " << targetY
                    << "), distance: " << distance;
                LogMouseMovement(ss.str());
            }

            // 如果有路径点，移动到下一个点
            while (pathIndex < currentPath.size()) {
                auto nextPoint = currentPath[pathIndex];

                // 添加微小抖动
                auto jitteredPoint = HumanLikeMovement::AddHumanJitter(
                    nextPoint.first, nextPoint.second, distance, humanizationFactor.load());

                // 移动鼠标
                mouseController->MoveTo(
                    static_cast<int>(jitteredPoint.first), static_cast<int>(jitteredPoint.second));

                std::stringstream ss;
                ss << "Moving to: (" << jitteredPoint.first << ", " << jitteredPoint.second << ")";
                std::cout << ss.str() << std::endl;
                LogMouseMovement(ss.str());

                pathIndex++;

                // 控制鼠标开火
                bool closeToTarget = (pathIndex > currentPath.size() * 0.8);
                ControlMouseFire(closeToTarget);

                // 走一小段就好，不要走完
                if (pathIndex > 10) {
                    break;
                }
            }
        }

        // 更新最后处理时间
        lastUpdateTime = currentTime;

        // 控制循环频率，大约60fps  改为5试试看
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

void ActionModule::UpdateCurrentMousePosition() {
    if (mouseController) {
        mouseController->GetCurrentPosition(currentX, currentY);
    }
}

void ActionModule::ControlMouseFire(bool targetVisible) {
    static bool firing = false;

    // 如果启用了自动开火
    if (fireEnabled.load()) {
        // 如果目标可见并且靠近目标，且当前没有开火
        if (targetVisible && !firing) {
            mouseController->LeftDown();
            firing = true;
            LogMouseMovement("Mouse left button down");
        }
        // 如果目标丢失或者远离目标，且当前正在开火
        else if (!targetVisible && firing) {
            mouseController->LeftUp();
            firing = false;
            LogMouseMovement("Mouse left button up");
        }
    }
    // 如果禁用了自动开火，确保鼠标左键释放
    else if (firing) {
        mouseController->LeftUp();
        firing = false;
        LogMouseMovement("Mouse left button up (auto fire disabled)");
    }
}