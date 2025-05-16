// ActionModule.cpp
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
std::thread ActionModule::recoilThread;
std::thread ActionModule::pidDebugThread;
std::atomic<bool> ActionModule::running(false);
std::atomic<bool> ActionModule::pidDebugEnabled(false);
std::atomic<bool> ActionModule::usePrediction(false); // 默认使用PID控制
std::unique_ptr<MouseController> ActionModule::mouseController = nullptr;
std::shared_ptr<SharedState> ActionModule::sharedState = std::make_shared<SharedState>();
PIDController ActionModule::pidController;
std::unique_ptr<KeyboardListener> ActionModule::keyboardListener = nullptr;
RecoilControlState ActionModule::recoilState;

// 预测调控参数
float ActionModule::predictAlpha = 0.3f; // 默认值设为0.3，在0.2~0.5范围内
Point2D ActionModule::lastTargetPos = { 0, 0 }; // 上一帧目标位置
bool ActionModule::hasLastTarget = false; // 是否有上一帧目标位置
int ActionModule::aimFov = 35; // 默认瞄准视场角度
constexpr int ActionModule::targetValidDurationMs; // 目标有效持续时间(毫秒)

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

    // 初始化压枪状态
    recoilState.isLeftButtonPressed = false;
    recoilState.pressStartTime = std::chrono::steady_clock::now();
    recoilState.lastRecoilTime = std::chrono::steady_clock::now();

    // 启动键盘监听
    keyboardListener->Start();

    // 启动自瞄处理线程
    actionThread = std::thread(ProcessLoop);

    // 启动点射控制线程
    fireThread = std::thread(FireControlLoop);

    // 启动压枪控制线程
    recoilThread = std::thread(RecoilControlLoop);

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

    if (recoilThread.joinable()) {
        recoilThread.join();
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
        std::cout << "Q/W: 调整aimFov  - 当前值: " << aimFov << std::endl;
        std::cout << "A/S: 调整Y轴控制力度 - 当前值: " << pidController.yControlFactor.load() << std::endl;
        std::cout << "Z/X: 调整Kd (微分系数) - 当前值: " << pidController.kd.load() << std::endl;
        std::cout << "E/R: 调整压枪时间 - 当前值: " << sharedState->pressTime.load() << "ms" << std::endl;
        std::cout << "D/F: 调整压枪力度 - 当前值: " << sharedState->pressForce.load() << std::endl;
        std::cout << "C: 切换压枪开关 - 当前状态: " << (sharedState->isRecoilControlEnabled.load() ? "开启" : "关闭") << std::endl;

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

void ActionModule::SetUsePrediction(bool enable) {
    usePrediction.store(enable);
    std::cout << "瞄准模式已切换为: " << (enable ? "预测模式" : "PID控制模式") << std::endl;

    // 如果切换为PID模式，重置PID控制器
    if (!enable) {
        pidController.Reset();
    }
    else {
        // 如果切换为预测模式，重置预测状态
        hasLastTarget = false;
    }
}

bool ActionModule::IsUsingPrediction() {
    return usePrediction.load();
}

void ActionModule::SetPredictAlpha(float alpha) {
    if (alpha >= 0.0f && alpha <= 1.0f) {
        predictAlpha = alpha;
        std::cout << "预测系数已设置为: " << alpha << std::endl;
    }
}

void ActionModule::HandleKeyPress(int key) {
    float kp = pidController.kp.load();
    float ki = pidController.ki.load();
    float kd = pidController.kd.load();
    float yControlFactor = pidController.yControlFactor.load();
    int pressTime = sharedState->pressTime.load();
    float pressForce = sharedState->pressForce.load();

    const float pStep = 0.05f;
    const float iStep = 0.01f;
    const float dStep = 0.01f;
    const float yFactorStep = 0.1f;
    const float pressForceStep = 0.5f;
    const int pressTimeStep = 20; // 20ms步进

    switch (key) {
    case 'Q':
        aimFov = max(0, aimFov - 1);
        std::cout << "aimFov 调整为: " << aimFov << std::endl;
        break;
    case 'W':
        aimFov += 1;
        std::cout << "aimFov 调整为: " << aimFov << std::endl;
        break;
    case 'A':
        yControlFactor = max(0.0f, yControlFactor - yFactorStep);
        pidController.yControlFactor.store(yControlFactor);
        std::cout << "Y轴控制力度 调整为: " << yControlFactor << std::endl;
        break;
    case 'S':
        yControlFactor += yFactorStep;
        pidController.yControlFactor.store(yControlFactor);
        std::cout << "Y轴控制力度 调整为: " << yControlFactor << std::endl;
        break;
    case 'Z':
        kd = max(0.0f, kd - dStep);
        pidController.kd.store(kd);
        std::cout << "Kd 调整为: " << kd << std::endl;
        break;
    case 'X':
        kd += dStep;
        pidController.kd.store(kd);
        std::cout << "Kd 调整为: " << kd << std::endl;
        break;
    case 'P': // 切换预测模式和PID模式
        SetUsePrediction(!IsUsingPrediction());
        break;
        // 压枪相关按键
    case 'E': // 减少压枪时间
        pressTime = max(0, pressTime - pressTimeStep);
        sharedState->pressTime.store(pressTime);
        std::cout << "压枪时间 调整为: " << pressTime << "ms" << std::endl;
        break;
    case 'R': // 增加压枪时间
        pressTime += pressTimeStep;
        sharedState->pressTime.store(pressTime);
        std::cout << "压枪时间 调整为: " << pressTime << "ms" << std::endl;
        break;
    case 'D': // 减少压枪力度
        pressForce = max(0.0f, pressForce - pressForceStep);
        sharedState->pressForce.store(pressForce);
        std::cout << "压枪力度 调整为: " << pressForce << std::endl;
        break;
    case 'F': // 增加压枪力度
        pressForce += pressForceStep;
        sharedState->pressForce.store(pressForce);
        std::cout << "压枪力度 调整为: " << pressForce << std::endl;
        break;
    case 'C': // 切换压枪开关
    {
        bool newState = !sharedState->isRecoilControlEnabled.load();
        sharedState->isRecoilControlEnabled.store(newState);
        std::cout << "压枪功能: " << (newState ? "开启" : "关闭") << std::endl;
    }
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

    // 计算微分项 在FPS直接通过YOLO观测会有大量的噪声，而PID中的D会放大噪声，所以一般设0
    float derivativeX = (errorX - pidController.previousErrorX) / dt;
    float derivativeY = (errorY - pidController.previousErrorY) / dt;

    // 更新积分项 (带限制，防止积分饱和)
    pidController.integralX += errorX * dt;
    pidController.integralY += errorY * dt;

    // 积分限制
    pidController.integralX = max(-pidController.integralLimit,
        min(pidController.integralX, pidController.integralLimit));
    pidController.integralY = max(-pidController.integralLimit,
        min(pidController.integralY, pidController.integralLimit));

    // 计算PID输出
    float outputX = kp * errorX + ki * pidController.integralX + kd * derivativeX;
    float outputY = kp * errorY + ki * pidController.integralY + kd * derivativeY;

    // 保存当前误差供下次使用
    pidController.previousErrorX = errorX;
    pidController.previousErrorY = errorY;

    return { outputX, outputY };
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

// 应用压枪效果，返回Y轴下压量
float ActionModule::ApplyRecoilControl() {
    // 检查压枪功能是否开启
    if (!sharedState->isRecoilControlEnabled.load()) {
        return 0.0f;
    }

    // 获取当前时间点
    auto currentTime = std::chrono::steady_clock::now();

    // 计算自鼠标按下后的时间差
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        currentTime - recoilState.pressStartTime).count();

    // 获取压枪持续时间
    int pressTime = sharedState->pressTime.load();

    // 如果超过压枪持续时间，停止压枪
    if (elapsedTime > pressTime) {
        return 0.0f;
    }

    // 返回压枪力度
    return sharedState->pressForce.load();
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
            hasLastTarget = false; // 重置预测状态
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

            // 更新目标距离和有效目标标志到共享状态
            sharedState->targetDistance = length;
            // 更新目标最近出现时间点
            sharedState->targetValidSince = std::chrono::steady_clock::now();

            // 处理自瞄功能
            if (sharedState->isAutoAimEnabled && mouseController && !mouseController->IsMouseMoving() && length < aimFov) {
                bool isPredictionMode = usePrediction.load();

                // 获取压枪下压量
                float recoilYOffset = 0.0f;
                if (sharedState->needPressDownWhenAim.load() && sharedState->isRecoilControlEnabled.load() &&
                    (mouseController->IsLeftButtonDown() || recoilState.isLeftButtonPressed.load())) {
                    recoilYOffset = ApplyRecoilControl();
                }

                // 根据当前模式选择使用PID控制或预测功能
                if (isPredictionMode) {
                    // 预测模式
                    if (length >= deadZoneThreshold) {
                        // 归一化移动值到±10范围
                        auto normalizedMove = NormalizeMovement(centerToTargetX, centerToTargetY + recoilYOffset, 15.0f);

                        // 当前目标坐标
                        Point2D currentTarget = { normalizedMove.first, normalizedMove.second };

                        // 在移动层面上进行简单预测
                        Point2D predictedTarget = PredictNextPosition(currentTarget);

                        // 使用控制器移动鼠标(相对坐标)
                        mouseController->MoveToWithTime(
                            static_cast<int>(predictedTarget.x),
                            static_cast<int>(predictedTarget.y),
                            length * humanizationFactor  // 距离乘以拟人化因子
                        );

                        // 通知预测模块鼠标移动了，用于补充鼠标补偿计算
                        PredictionModule::NotifyMouseMovement(normalizedMove.first, normalizedMove.second);
                    }
                }
                else {
                    // PID控制模式
                    if (length >= deadZoneThreshold) {
                        // 使用PID控制器计算移动值
                        auto pidOutput = ApplyPIDControl(centerToTargetX, centerToTargetY);

                        // 获取Y轴控制力度
                        float yControlFactor = pidController.yControlFactor.load();

                        // 应用Y轴控制力度并加入压枪下压量
                        pidOutput.second = pidOutput.second * yControlFactor + recoilYOffset;

                        // 归一化移动值到±10范围
                        auto normalizedMove = NormalizeMovement(pidOutput.first, pidOutput.second, 10.0f);

                        // 使用控制器移动鼠标(相对坐标)
                        mouseController->MoveToWithTime(
                            static_cast<int>(normalizedMove.first),
                            static_cast<int>(normalizedMove.second),
                            length * humanizationFactor  // 距离乘以拟人化因子
                        );
                    }
                }
            }
            else if (!sharedState->isAutoAimEnabled) {
                // 当自瞄关闭时重置控制器状态
                if (usePrediction.load()) {
                    hasLastTarget = false;
                }
                else {
                    pidController.Reset();
                }
            }
        }
    }
}


// 日志时间戳辅助函数
std::string NowTimeString() {
    auto now = std::chrono::system_clock::now();
    auto t_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t_c), "%H:%M:%S");
    return ss.str();
}

// 简单的 debug log，带时间
#define LogDebug(msg) std::cout << "[" << NowTimeString() << "][DEBUG] " << msg << std::endl

void ActionModule::RecoilControlLoop() {
    bool prevLeftButtonState = false;

    LogDebug("RecoilControlLoop started.");

    while (running.load()) {
        bool isRecoilEnabled = sharedState->isRecoilControlEnabled.load();
        bool currentLeftButtonState = mouseController->IsLeftButtonDown();

        // 检测左键按下/松开
        if (currentLeftButtonState && !prevLeftButtonState) {
            if (isRecoilEnabled) {
                recoilState.isLeftButtonPressed = true;
                recoilState.pressStartTime = std::chrono::steady_clock::now();
                recoilState.lastRecoilTime = std::chrono::steady_clock::now();
            }
        }
        else if (!currentLeftButtonState && prevLeftButtonState) {
            recoilState.isLeftButtonPressed = false;
        }
        prevLeftButtonState = currentLeftButtonState;

        // 检查是否需要压枪
        if (isRecoilEnabled && recoilState.isLeftButtonPressed.load()) {

            auto currentTime = std::chrono::steady_clock::now();
            auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - recoilState.pressStartTime).count();
            auto timeSinceLastRecoil = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - recoilState.lastRecoilTime).count();

            // 可压枪判定
            if (elapsedTime <= sharedState->pressTime.load() && timeSinceLastRecoil >= 16) {
                float pressForce = sharedState->pressForce.load();
                mouseController->MoveToWithTime(0, static_cast<int>(pressForce), 100);
                recoilState.lastRecoilTime = currentTime;
            }
        }

        // 线程休眠
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    LogDebug("RecoilControlLoop ended.");
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

        float targetDistance = sharedState->targetDistance.load();

        // 计算目标最近出现时间与当前时间的差值
        auto targetTimeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - sharedState->targetValidSince).count();

        // 判断目标是否在有效时间窗口内 (当前时间 - 最近目标时间 < 有效持续时间)
        bool targetInValidTimeWindow = targetTimeDiff < targetValidDurationMs;

        // 如果自动开火关闭，确保鼠标释放并重置状态
        if (!autoFireEnabled) {
            if (mouseController->IsLeftButtonDown()) {
                mouseController->LeftUp();
                // 当点射结束时，触发压枪状态重置
               // recoilState.isLeftButtonPressed = false;
            }
            currentState = FireState::IDLE;
            Sleep(50); // 减少CPU使用
            continue;
        }

        // 状态机逻辑
        switch (currentState) {
        case FireState::IDLE:
            // 从空闲状态转为点射状态的条件：检查目标是否在有效时间窗口内且距离合适
            if (autoFireEnabled && targetInValidTimeWindow && targetDistance < deadZoneThreshold) {
                mouseController->LeftDown();
                // 模拟按下左键时，设置压枪状态
                if (sharedState->isRecoilControlEnabled.load()) {
                    // recoilState.isLeftButtonPressed = true;
                   // recoilState.pressStartTime = std::chrono::steady_clock::now();
                   // recoilState.lastRecoilTime = std::chrono::steady_clock::now();
                }
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
                // 当点射结束时，触发压枪状态重置
              //   recoilState.isLeftButtonPressed = false;
                currentState = FireState::BURST_COOLDOWN;
                currentDuration = burstCooldown(gen); // 随机冷却时间
                lastStateChangeTime = currentTime;
                std::cout << "点射结束，冷却: " << currentDuration << "ms" << std::endl;
            }
            // 只有当目标不在有效时间窗口且距离过远时，才停止点射
            else if (!targetInValidTimeWindow && targetDistance >= 10.0f) {
                mouseController->LeftUp();
                // 当点射结束时，触发压枪状态重置
             //    recoilState.isLeftButtonPressed = false;
                currentState = FireState::IDLE;
                std::cout << "目标丢失，停止点射" << std::endl;
            }
            break;

        case FireState::BURST_COOLDOWN:
            // 冷却时间结束
            if (elapsedTime >= currentDuration) {
                // 如果目标在有效时间窗口内且距离合适，开始新一轮点射
                if (targetInValidTimeWindow && targetDistance < 10.0f) {
                    // 有小概率插入短暂停顿，增加拟人性
                    if (gen() % 3 == 0) { // 约33%的概率
                        Sleep(microPause(gen));
                    }

                    mouseController->LeftDown();
                    // 模拟按下左键时，设置压枪状态
                    if (sharedState->isRecoilControlEnabled.load()) {
                        //     recoilState.isLeftButtonPressed = true;
                   //      recoilState.pressStartTime = std::chrono::steady_clock::now();
                  //       recoilState.lastRecoilTime = std::chrono::steady_clock::now();
                    }
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
        // 当点射结束时，触发压枪状态重置
     //    recoilState.isLeftButtonPressed = false;
    }
}