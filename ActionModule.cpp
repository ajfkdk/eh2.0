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
std::thread ActionModule::pidDebugThread;
std::atomic<bool> ActionModule::running(false);
std::atomic<bool> ActionModule::pidDebugEnabled(false);
std::unique_ptr<MouseController> ActionModule::mouseController = nullptr;
std::shared_ptr<SharedState> ActionModule::sharedState = std::make_shared<SharedState>();
PIDController ActionModule::pidController;
std::unique_ptr<KeyboardListener> ActionModule::keyboardListener = nullptr;

std::thread ActionModule::Initialize() {
    // 如果没有设置鼠标控制器，使用Windows默认实现
    if (!mouseController) {
        // 创建Windows鼠标控制器的智能指针
        mouseController = std::make_unique<KmboxNetMouseController>();
    }

    // 创建键盘监听器
    keyboardListener = std::make_unique<KeyboardListener>();
    keyboardListener->onKeyPress = HandleKeyPress;

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

    if (pidDebugThread.joinable()) {
        pidDebugThread.join();
    }

    // 停止键盘监听
    if (keyboardListener) {
        keyboardListener->Stop();
    }
}

void ActionModule::Stop() {
    running.store(false);
    pidDebugEnabled.store(false);
}

bool ActionModule::IsRunning() {
    return running.load();
}

void ActionModule::SetMouseController(std::unique_ptr<MouseController> controller) {
    mouseController = std::move(controller);
}

void ActionModule::EnablePIDDebug(bool enable) {
    bool currentState = pidDebugEnabled.load();

    // 如果状态没变，不做任何操作
    if (currentState == enable) return;

    pidDebugEnabled.store(enable);

    // 如果是启用PID调试
    if (enable) {
        // 启动键盘监听
        keyboardListener->Start();

        // 打印当前PID参数
        std::cout << "PID调试模式已启用" << std::endl;
        std::cout << "使用以下键调整PID参数：" << std::endl;
        std::cout << "Q/W: 调整Kp (比例系数) - 当前值: " << pidController.kp.load() << std::endl;
        std::cout << "A/S: 调整Ki (积分系数) - 当前值: " << pidController.ki.load() << std::endl;
        std::cout << "Z/X: 调整Kd (微分系数) - 当前值: " << pidController.kd.load() << std::endl;

        // 启动PID调试线程
        pidDebugThread = std::thread(PIDDebugLoop);
    }
    else {
        // 停止键盘监听
        keyboardListener->Stop();

        std::cout << "PID调试模式已禁用" << std::endl;
    }
}

bool ActionModule::IsPIDDebugEnabled() {
    return pidDebugEnabled.load();
}

void ActionModule::HandleKeyPress(int key) {
    float kp = pidController.kp.load();
    float ki = pidController.ki.load();
    float kd = pidController.kd.load();

    const float pStep = 0.05f;
    const float iStep = 0.01f;
    const float dStep = 0.01f;

    switch (key) {
    case 'Q':
        kp = std::max(0.0f, kp - pStep);
        pidController.kp.store(kp);
        std::cout << "Kp 调整为: " << kp << std::endl;
        break;
    case 'W':
        kp += pStep;
        pidController.kp.store(kp);
        std::cout << "Kp 调整为: " << kp << std::endl;
        break;
    case 'A':
        ki = std::max(0.0f, ki - iStep);
        pidController.ki.store(ki);
        std::cout << "Ki 调整为: " << ki << std::endl;
        break;
    case 'S':
        ki += iStep;
        pidController.ki.store(ki);
        std::cout << "Ki 调整为: " << ki << std::endl;
        break;
    case 'Z':
        kd = std::max(0.0f, kd - dStep);
        pidController.kd.store(kd);
        std::cout << "Kd 调整为: " << kd << std::endl;
        break;
    case 'X':
        kd += dStep;
        pidController.kd.store(kd);
        std::cout << "Kd 调整为: " << kd << std::endl;
        break;
    }
}

void ActionModule::PIDDebugLoop() {
    while (running.load() && pidDebugEnabled.load()) {
        // 线程定期睡眠，减少CPU使用
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

// PID控制算法
std::pair<float, float> ActionModule::ApplyPIDControl(float errorX, float errorY) {
    auto currentTime = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(currentTime - pidController.lastTime).count();
    pidController.lastTime = currentTime;

    // 防止dt过小或为零导致计算问题
    if (dt < 0.001f) dt = 0.001f;

    // 提取PID参数
    float kp = pidController.kp.load();
    float ki = pidController.ki.load();
    float kd = pidController.kd.load();

    // 计算微分项
    float derivativeX = (errorX - pidController.previousErrorX) / dt;
    float derivativeY = (errorY - pidController.previousErrorY) / dt;

    // 更新积分项 (带限制，防止积分饱和)
    pidController.integralX += errorX * dt;
    pidController.integralY += errorY * dt;

    // 积分限制
    pidController.integralX = std::max(-pidController.integralLimit,
        std::min(pidController.integralX, pidController.integralLimit));
    pidController.integralY = std::max(-pidController.integralLimit,
        std::min(pidController.integralY, pidController.integralLimit));

    // 计算PID输出
    float outputX = kp * errorX + ki * pidController.integralX + kd * derivativeX;
    float outputY = kp * errorY + ki * pidController.integralY + kd * derivativeY;

    // 保存当前误差供下次使用
    pidController.previousErrorX = errorX;
    pidController.previousErrorY = errorY;

    return { outputX, outputY };
}

// 自瞄线程 - 负责处理自瞄和更新目标状态
void ActionModule::ProcessLoop() {
    bool prevSideButton1State = false; // 用于检测侧键1状态变化
    bool prevSideButton2State = false; // 用于检测侧键2状态变化

    // 重置PID控制器
    pidController.Reset();

    // 处理主循环
    while (running.load()) {
        // 处理自瞄开关（侧键1）
        bool currentSideButton1State = mouseController->IsSideButton1Down();
        if (currentSideButton1State && !prevSideButton1State) {
            sharedState->isAutoAimEnabled = !sharedState->isAutoAimEnabled; // 切换自瞄状态
            std::cout << "自瞄功能: " << (sharedState->isAutoAimEnabled ? "开启" : "关闭") << std::endl;

            // 每次开关自瞄时重置PID控制器
            pidController.Reset();
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

            // 计算目标坐标
            int targetX = static_cast<int>(offsetX + prediction.x);
            int targetY = static_cast<int>(offsetY + prediction.y);

            // 计算从屏幕中心到目标的相对距离
            float centerToTargetX = static_cast<float>(targetX - screenCenterX);
            float centerToTargetY = static_cast<float>(targetY - screenCenterY);

            // 计算长度
            float length = std::sqrt(centerToTargetX * centerToTargetX + centerToTargetY * centerToTargetY);

            // 更新目标距离到共享状态
            sharedState->targetDistance = length;
            sharedState->hasValidTarget = true;

            // 处理自瞄功能
            if (sharedState->isAutoAimEnabled && mouseController && !mouseController->IsMouseMoving()) {
                if (length >= 7.0f) {
                    // 使用PID控制器计算移动值
                    auto pidOutput = ApplyPIDControl(centerToTargetX, centerToTargetY);

                    // 归一化移动值到±10范围
                    auto normalizedMove = NormalizeMovement(pidOutput.first, pidOutput.second, 10.0f);

                    // 使用控制器移动鼠标(相对坐标)
                    mouseController->MoveTo(
                        static_cast<int>(normalizedMove.first),
                        static_cast<int>(normalizedMove.second));

                    // 打印调试信息
                    if (pidDebugEnabled.load()) {
                        std::cout << "Error:" << centerToTargetX << "," << centerToTargetY
                            << " PID:" << pidOutput.first << "," << pidOutput.second
                            << " Move:" << normalizedMove.first << "," << normalizedMove.second << std::endl;
                    }
                }
            }
            else if (!sharedState->isAutoAimEnabled) {
                // 当自瞄关闭时重置PID控制器，避免积分项累积
                pidController.Reset();
            }
        }
        else {
            // 没有有效目标
            sharedState->hasValidTarget = false;
            sharedState->targetDistance = 999.0f;

            // 重置PID控制器
            pidController.Reset();
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
        Sleep(1);
    }

    // 确保退出时释放鼠标按键
    if (mouseController->IsLeftButtonDown()) {
        mouseController->LeftUp();
    }
}