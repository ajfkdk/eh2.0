#include "ActionModule.h"
#include <iostream>
#include <cmath>
#include <Windows.h>
#include "PredictionModule.h"
#include "KmboxNetMouseController.h"

// 静态成员初始化
std::thread ActionModule::actionThread;
std::atomic<bool> ActionModule::running(false);
std::unique_ptr<MouseController> ActionModule::mouseController = nullptr;

std::thread ActionModule::Initialize() {
    // 如果没有设置鼠标控制器，使用Windows默认实现
    if (!mouseController) {
        // 创建Windows鼠标控制器的智能指针
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

// 归一化移动值到指定范围
std::pair<float, float> ActionModule::NormalizeMovement(float x, float y, float maxValue) {
    // 计算向量长度
    float length = std::sqrt(x * x + y * y);

    // 如果长度为0，返回(0,0)
    if (length < 0.001f) {
        return { 0.0f, 0.0f };
    }

    // 计算归一化因子
    float factor = min(length, maxValue) / length;

    // 返回归一化后的值
    return { x * factor, y * factor };
}

// 动作模块主要的处理循环
void ActionModule::ProcessLoop() {
    // 处理主循环
    while (running.load()) {
        // 只有当鼠标侧键1按下时才进行鼠标移动控制
        if (mouseController && mouseController->IsSideButton1Down()) {
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

                std::cout << "屏幕分辨率: " << screenWidth << "x" << screenHeight << std::endl;
                std::cout << "原始预测位置: (" << prediction.x << ", " << prediction.y << ")" << std::endl;
                std::cout << "转换后的目标位置: (" << targetX << ", " << targetY << ")" << std::endl;

                // 计算从屏幕中心到目标的相对距离
                float centerToTargetX = static_cast<float>(targetX - screenCenterX);
                float centerToTargetY = static_cast<float>(targetY - screenCenterY);

                // 计算总距离(用于调试输出)
                float distance = std::sqrt(centerToTargetX * centerToTargetX + centerToTargetY * centerToTargetY);

                // 归一化移动值到±10范围
                auto normalizedMove = NormalizeMovement(centerToTargetX, centerToTargetY, 10.0f);

                // 使用KMBOX控制器移动鼠标(相对坐标)
                mouseController->MoveRelative(
                    static_cast<int>(normalizedMove.first),
                    static_cast<int>(normalizedMove.second));

                std::cout << "移动鼠标(相对坐标): ("
                    << normalizedMove.first << ", " << normalizedMove.second
                    << "), 原始距离: " << distance << std::endl;
            }
        }

        // 控制循环频率，每2ms执行一次
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}