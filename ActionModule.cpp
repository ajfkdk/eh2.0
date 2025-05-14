#include "ActionModule.h"
#include <iostream>
#include <cmath>
#include <Windows.h>
#include <random>
#include "PredictionModule.h"
#include "KmboxNetMouseController.h"

// 静态成员初始化
std::thread ActionModule::actionThread;
std::thread ActionModule::fireThread;
std::atomic<bool> ActionModule::running(false);
std::unique_ptr<MouseController> ActionModule::mouseController = nullptr;
std::shared_ptr<SharedState> ActionModule::sharedState = std::make_shared<SharedState>();

// 预测调控参数
float ActionModule::predictAlpha = 0.3f; // 默认值设为0.3，在0.2~0.5范围内
Point2D ActionModule::lastTargetPos = { 0, 0 }; // 上一帧目标位置
bool ActionModule::hasLastTarget = false; // 是否有上一帧目标位置

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

    // 启动自瞄处理线程
    actionThread = std::thread(ProcessLoop);

    // 启动点射控制线程
    fireThread = std::thread(FireControlLoop);

    // 返回主线程对象 (使用std::move转移所有权)
    return std::move(actionThread);
}

void ActionModule::Cleanup() {
    // 停止模块
    Stop();

    // 等待线程结束
    if (actionThread.joinable()) {
        actionThread.join();
    }

    if (fireThread.joinable()) {
        fireThread.join();
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

// 设置预测系数
void ActionModule::SetPredictAlpha(float alpha) {
    if (alpha >= 0.0f && alpha <= 1.0f) {
        predictAlpha = alpha;
        std::cout << "预测系数已设置为: " << alpha << std::endl;
    }
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

// 预测目标下一帧位置
Point2D ActionModule::PredictNextPosition(const Point2D& current) {
    // 如果没有上一帧位置，无法预测，直接返回当前位置
    if (!hasLastTarget) {
        hasLastTarget = true;
        lastTargetPos = current;
        return current;
    }

    // 使用简单线性预测: predicted = current + (current - last) * alpha
    Point2D predicted;
    predicted.x = current.x + (current.x - lastTargetPos.x) * predictAlpha;
    predicted.y = current.y + (current.y - lastTargetPos.y) * predictAlpha;

    // 更新上一帧位置
    lastTargetPos = current;

    // 返回预测位置
    return predicted;
}

// 自瞄线程 - 负责处理自瞄和更新目标状态
void ActionModule::ProcessLoop() {
    bool prevSideButton1State = false; // 用于检测侧键1状态变化
    bool prevSideButton2State = false; // 用于检测侧键2状态变化

    // 处理主循环
    while (running.load()) {
        // 处理自瞄开关（侧键1）
        bool currentSideButton1State = mouseController->IsSideButton1Down();
        if (currentSideButton1State && !prevSideButton1State) {
            sharedState->isAutoAimEnabled = !sharedState->isAutoAimEnabled; // 切换自瞄状态
            std::cout << "自瞄功能: " << (sharedState->isAutoAimEnabled ? "开启" : "关闭") << std::endl;
        }
        prevSideButton1State = currentSideButton1State;

        // 处理自动开火开关（侧键2）
        bool currentSideButton2State = mouseController->IsSideButton2Down();
        if (currentSideButton2State && !prevSideButton2State) {
            bool newState = !sharedState->isAutoFireEnabled.load();
            sharedState->isAutoFireEnabled = newState; // 切换自动开火状态

            std::cout << "自动开火功能: " << (newState ? "开启" : "关闭") << std::endl;
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

            // 使用预测的目标坐标
            int targetX = static_cast<int>(offsetX + prediction.x);
            int targetY = static_cast<int>(offsetY + prediction.y);

            // 计算从屏幕中心到目标的相对距离
            float centerToTargetX = static_cast<float>(targetX - screenCenterX);
            float centerToTargetY = static_cast<float>(targetY - screenCenterY);

            // 计算长度
            float length = std::sqrt(centerToTargetX * centerToTargetX + centerToTargetY * centerToTargetY);

            // 重要修改：先更新目标状态，无论距离如何，确保在死区内也能正确维护目标状态
            sharedState->targetDistance = length;
            sharedState->hasValidTarget = true;

            // 处理自瞄功能
            if (sharedState->isAutoAimEnabled && mouseController) {
                // 降低死区阈值，确保即使目标接近中心也会有微小调整
                if (length >= deadZoneThreshold && !mouseController->IsMouseMoving()) { 
                    // 归一化移动值到±10范围
                    auto normalizedMove = NormalizeMovement(centerToTargetX, centerToTargetY, 10.0f);

                    // 当前目标坐标
                    Point2D currentTarget = { normalizedMove.first, normalizedMove.second };

                    // 在移动层面上进行简单预测
                    Point2D predictedTarget = PredictNextPosition(currentTarget);

                    // 对x、y都进行随机偏移，增加拟人化
                    // 随机偏移-2到2
                    predictedTarget.x += (rand() % 5 - 2);
                    predictedTarget.y += (rand() % 5 - 2);

                    // 使用控制器移动鼠标(相对坐标)
                    mouseController->MoveToWithTime(
                        static_cast<int>(predictedTarget.x),
                        static_cast<int>(predictedTarget.y),
                        length * humanizationFactor  // 距离乘以拟人化因子
                    );

                    // 通知预测模块鼠标移动了，用于补充鼠标补偿计算
                    PredictionModule::NotifyMouseMovement(normalizedMove.first, normalizedMove.second);
                }
                // 即使在死区内也保持目标状态的有效性，确保自动开火功能正常工作
                else if (length < deadZoneThreshold) {
                    // 死区内，但目标依然有效，不做鼠标移动
                    sharedState->hasValidTarget = true;
                    sharedState->targetDistance = length;
                }
            }
        }
        else {
            // 没有有效目标
            sharedState->hasValidTarget = false;
            sharedState->targetDistance = 999.0f;
            hasLastTarget = false; // 重置预测状态
        }
    }
}

// 点射控制线程 - 负责控制开火行为
void ActionModule::FireControlLoop() {
    // 点射状态机状态
    enum class FireState {
        IDLE,           // 空闲状态
        BURST_ACTIVE,   // 点射激活状态
        BURST_COOLDOWN  // 点射冷却状态
    };

    FireState currentState = FireState::IDLE;

    // 随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());

    // 点射持续时间分布 (80ms-150ms)
    std::uniform_int_distribution<> burstDuration(80, 150);

    // 点射冷却时间分布 (180ms-400ms)
    std::uniform_int_distribution<> burstCooldown(180, 400);

    // 点射间短暂停顿 (20ms-50ms)
    std::uniform_int_distribution<> microPause(20, 50);

    auto lastStateChangeTime = std::chrono::steady_clock::now();
    int currentDuration = 0;

    while (running.load()) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastStateChangeTime).count();

        // 获取当前共享状态
        bool autoFireEnabled = sharedState->isAutoFireEnabled.load();
        bool hasValidTarget = sharedState->hasValidTarget.load();
        float targetDistance = sharedState->targetDistance.load();

        // 如果自动开火关闭，确保鼠标释放并重置状态
        if (!autoFireEnabled) {
            if (mouseController->IsLeftButtonDown()) {
                mouseController->LeftUp();
            }
            currentState = FireState::IDLE;
            Sleep(50); // 减少CPU使用
            continue;
        }

        // 状态机逻辑
        switch (currentState) {
        case FireState::IDLE:
            // 从空闲状态转为点射状态的条件
            if (autoFireEnabled && hasValidTarget && targetDistance < 10.0f) {
                mouseController->LeftDown();
                currentState = FireState::BURST_ACTIVE;
                currentDuration = burstDuration(gen); // 随机点射持续时间
                lastStateChangeTime = currentTime;
                std::cout << "开始点射，持续: " << currentDuration << "ms" << std::endl;
            }
            else {
                Sleep(10); // 减少CPU使用
            }
            break;

        case FireState::BURST_ACTIVE:
            // 点射持续时间结束
            if (elapsedTime >= currentDuration) {
                mouseController->LeftUp();
                currentState = FireState::BURST_COOLDOWN;
                currentDuration = burstCooldown(gen); // 随机冷却时间
                lastStateChangeTime = currentTime;
                std::cout << "点射结束，冷却: " << currentDuration << "ms" << std::endl;
            }
            // 如果目标无效或距离过远，立即停止点射
            else if (!hasValidTarget || targetDistance >= 10.0f) {
                mouseController->LeftUp();
                currentState = FireState::IDLE;
                std::cout << "目标丢失，停止点射" << std::endl;
            }
            break;

        case FireState::BURST_COOLDOWN:
            // 冷却时间结束
            if (elapsedTime >= currentDuration) {
                // 如果目标仍然有效且在范围内，开始新一轮点射
                if (hasValidTarget && targetDistance < 10.0f) {
                    // 有小概率插入短暂停顿，增加拟人性
                    if (gen() % 3 == 0) { // 约33%的概率
                        Sleep(microPause(gen));
                    }

                    mouseController->LeftDown();
                    currentState = FireState::BURST_ACTIVE;
                    currentDuration = burstDuration(gen);
                    lastStateChangeTime = currentTime;
                    std::cout << "开始新点射，持续: " << currentDuration << "ms" << std::endl;
                }
                else {
                    currentState = FireState::IDLE;
                }
            }
            break;
        }

        // 短暂睡眠，降低CPU使用率
        Sleep(5);
    }

    // 确保退出时释放鼠标按键
    if (mouseController->IsLeftButtonDown()) {
        mouseController->LeftUp();
    }
}