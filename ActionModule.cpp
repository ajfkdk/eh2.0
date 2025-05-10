#include "ActionModule.h"
#include "HumanLikeMovement.h"
#include "KmboxNetMouseController.h"
#include <iostream>
#include <cmath>
#include <Windows.h>
#include "PredictionModule.h"

// 静态成员初始化
std::thread ActionModule::actionThread;
std::atomic<bool> ActionModule::running(false);
std::unique_ptr<MouseController> ActionModule::mouseController = nullptr;

std::thread ActionModule::Initialize() {
    // 初始化HumanLikeMovement
    HumanLikeMovement::Initialize();

    // 如果没有设置鼠标控制器，默认使用KmboxNetMouseController
    if (!mouseController) {
       
        mouseController = std::make_unique<KmboxNetMouseController>();
    }

    // 设置运行标志
    running.store(true);

    // 启动处理线程
    actionThread = std::thread(ProcessLoop);

    // 返回线程对象 (使用std::move转移所有权)
    return std::move(actionThread);
}

void ActionModule::Cleanup() {
    // 停止模块
    Stop();

    // 等待线程结束
    if (actionThread.joinable()) {
        actionThread.join();
    }
}

void ActionModule::Stop() {
    running.store(false);
}

bool ActionModule::IsRunning() {
    return running.load();
}

void ActionModule::SetMouseController(std::unique_ptr<MouseController> controller) {
    mouseController = std::move(controller);
}

// 动作模块主要的处理循环
void ActionModule::ProcessLoop() {
    // 创建用于缓存路径点的变量
    std::vector<std::pair<float, float>> currentPath;
    size_t pathIndex = 0;

    // 处理主循环
    while (running.load()) {
        // 获取最新预测结果
        PredictionResult prediction;
        bool hasPrediction = PredictionModule::GetLatestPrediction(prediction);

        // 如果有有效预测结果
        if (hasPrediction && prediction.x != 999 && prediction.y != 999) {
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
            int targetX = static_cast<int>(offsetX + prediction.x);
            int targetY = static_cast<int>(offsetY + prediction.y);

      /*      std::cout << "屏幕分辨率: " << screenWidth << "x" << screenHeight << std::endl;
            std::cout << "原始预测位置: (" << prediction.x << ", " << prediction.y << ")" << std::endl;
            std::cout << "转换后的目标位置: (" << targetX << ", " << targetY << ")" << std::endl;*/

            // 计算从屏幕中心到目标的相对距离
            int centerToTargetX = targetX - screenCenterX;
            int centerToTargetY = targetY - screenCenterY;

            float distance = std::sqrt(
                std::pow(centerToTargetX, 2) + std::pow(centerToTargetY, 2));

            // 如果距离足够大，或者当前路径已经执行完毕，生成新路径
            if (distance > 5 || currentPath.empty() || pathIndex >= currentPath.size()) {
                // 使用贝塞尔曲线生成路径点
                // 注意：这里从屏幕中心(0,0)到目标的相对位置生成路径
                currentPath = HumanLikeMovement::GenerateBezierPath(
                    0, 0, centerToTargetX, centerToTargetY, 50,
                    max(10, static_cast<int>(distance / 5)));

                pathIndex = 0;

                std::cout << "新路径生成，从(0,0)到相对目标: ("
                    << centerToTargetX << ", " << centerToTargetY
                    << "), 距离: " << distance << std::endl;
            }

            // 如果有路径点，移动到下一个点
            while (pathIndex < currentPath.size()) {
                auto nextPoint = currentPath[pathIndex];

                // 使用KMBOX控制器移动鼠标(相对坐标)
                mouseController->MoveRelative(
                    static_cast<int>(nextPoint.first),
                    static_cast<int>(nextPoint.second));

                std::cout << "移动鼠标(相对坐标): ("
                    << nextPoint.first << ", " << nextPoint.second << ")" << std::endl;

                pathIndex++;

                // 走一小段路径点就跳出循环，下次继续走
                if (pathIndex > 10) {
                    break;
                }
            }
        }

        // 控制循环频率，每5ms执行一次
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}