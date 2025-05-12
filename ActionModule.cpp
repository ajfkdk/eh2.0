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

    // 启动鼠标监听
    if (mouseController) {
        mouseController->StartMonitor();
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
    bool isAutoAimEnabled = false;  // 自瞄功能开关
    bool isAutoFireEnabled = false; // 自动开火功能开关

    bool prevSideButton1State = false; // 用于检测侧键1状态变化
    bool prevSideButton2State = false; // 用于检测侧键2状态变化

    auto lastFireTime = std::chrono::steady_clock::now();
    bool fireState = false; // false表示不开火，true表示开火

    // 处理主循环
    while (running.load()) {
        // 处理自瞄开关（侧键1）
        bool currentSideButton1State = mouseController->IsSideButton1Down();
        if (currentSideButton1State && !prevSideButton1State) {
            isAutoAimEnabled = !isAutoAimEnabled; // 切换自瞄状态
            std::cout << "自瞄功能: " << (isAutoAimEnabled ? "开启" : "关闭") << std::endl;
        }
        prevSideButton1State = currentSideButton1State;

        // 处理自动开火开关（侧键2）
        bool currentSideButton2State = mouseController->IsSideButton2Down();
        if (currentSideButton2State && !prevSideButton2State) {
            isAutoFireEnabled = !isAutoFireEnabled; // 切换自动开火状态

            // 如果关闭自动开火，确保鼠标左键释放
            if (!isAutoFireEnabled && mouseController->IsLeftButtonDown()) {
                mouseController->LeftUp();
                fireState = false;
            }

            std::cout << "自动开火功能: " << (isAutoFireEnabled ? "开启" : "关闭") << std::endl;
        }
        prevSideButton2State = currentSideButton2State;

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

            // 计算从屏幕中心到目标的相对距离
            float centerToTargetX = static_cast<float>(targetX - screenCenterX);
            float centerToTargetY = static_cast<float>(targetY - screenCenterY);

            // 计算长度
            float length = std::sqrt(centerToTargetX * centerToTargetX + centerToTargetY * centerToTargetY);

            // 处理自瞄功能
            if (isAutoAimEnabled && mouseController && !mouseController->IsMouseMoving()) {
                if (length >= 7.0f) {
                    // 归一化移动值到±10范围
                    auto normalizedMove = NormalizeMovement(centerToTargetX, centerToTargetY, 10.0f);

                    // 使用控制器移动鼠标(相对坐标)
                    mouseController->MoveTo(
                        static_cast<int>(normalizedMove.first),
                        static_cast<int>(normalizedMove.second));

                    // 打印调试信息
                    std::cout << "centerToTarget:" << centerToTargetX << ", " << centerToTargetY
                        << " ---> normal:" << normalizedMove.first << ", " << normalizedMove.second << std::endl;
                }
            }

            // 处理自动开火功能
            if (isAutoFireEnabled) {
                // 当目标与准星距离小于10像素时，触发自动开火
                if (length < 10.0f) {
                    auto currentTime = std::chrono::steady_clock::now();
                    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                        currentTime - lastFireTime).count();

                    if (fireState) { // 当前在开火状态
                        if (elapsedTime >= 200) { // 开火200ms后
                            mouseController->LeftUp(); // 释放左键
                            lastFireTime = currentTime;
                            fireState = false; // 切换到休息状态
                        }
                    }
                    else { // 当前在休息状态
                        if (elapsedTime >= 200) { // 休息200ms后
                            mouseController->LeftDown(); // 按下左键
                            lastFireTime = currentTime;
                            fireState = true; // 切换到开火状态
                        }
                    }
                }
                else if (mouseController->IsLeftButtonDown()) {
                    mouseController->LeftUp(); // 如果目标距离过远且左键已按下，释放它
                    fireState = false;
                }
            }
        }
        else if (isAutoFireEnabled && mouseController->IsLeftButtonDown()) {
            // 如果没有有效目标但左键已按下，释放它
            mouseController->LeftUp();
            fireState = false;
        }

    }
}