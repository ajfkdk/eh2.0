// ActionModule.cpp
#include "ActionModule.h"
#include <iostream>
#include <cmath>
#include <Windows.h>
#include <random>
#include "PredictionModule.h"
#include "KmboxNetMouseController.h"

// ��̬��Ա��ʼ��
std::thread ActionModule::actionThread;
std::thread ActionModule::fireThread;
std::thread ActionModule::recoilThread;
std::thread ActionModule::pidDebugThread;
std::atomic<bool> ActionModule::running(false);
std::atomic<bool> ActionModule::pidDebugEnabled(false);
std::atomic<bool> ActionModule::usePrediction(false); // Ĭ��ʹ��PID����
std::unique_ptr<MouseController> ActionModule::mouseController = nullptr;
std::shared_ptr<SharedState> ActionModule::sharedState = std::make_shared<SharedState>();
PIDController ActionModule::pidController;
std::unique_ptr<KeyboardListener> ActionModule::keyboardListener = nullptr;
RecoilControlState ActionModule::recoilState;

// Ԥ����ز���
float ActionModule::predictAlpha = 0.3f; // Ĭ��ֵ��Ϊ0.3����0.2~0.5��Χ��
Point2D ActionModule::lastTargetPos = { 0, 0 }; // ��һ֡Ŀ��λ��
bool ActionModule::hasLastTarget = false; // �Ƿ�����һ֡Ŀ��λ��
int ActionModule::aimFov = 35; // Ĭ����׼�ӳ��Ƕ�
constexpr int ActionModule::targetValidDurationMs; // Ŀ����Ч����ʱ��(����)

std::thread ActionModule::Initialize() {
    // ���û����������������ʹ��WindowsĬ��ʵ��
    if (!mouseController) {
        // ����Windows��������������ָ��
        mouseController = std::make_unique<KmboxNetMouseController>();
    }

    // �������̼�����
    keyboardListener = std::make_unique<KeyboardListener>();
    keyboardListener->onKeyPress = HandleKeyPress;

    // ����������
    if (mouseController) {
        mouseController->StartMonitor();
    }

    // �������б�־
    running.store(true);

    // ��ʼ��ѹǹ״̬
    recoilState.isLeftButtonPressed = false;
    recoilState.pressStartTime = std::chrono::steady_clock::now();
    recoilState.lastRecoilTime = std::chrono::steady_clock::now();

    // �������̼���
    keyboardListener->Start();

    // �������鴦���߳�
    actionThread = std::thread(ProcessLoop);

    // ������������߳�
    fireThread = std::thread(FireControlLoop);

    // ����ѹǹ�����߳�
    recoilThread = std::thread(RecoilControlLoop);

    // �������̶߳��� (ʹ��std::moveת������Ȩ)
    return std::move(actionThread);
}

void ActionModule::Cleanup() {
    // ֹͣģ��
    Stop();

    // �ȴ��߳̽���
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

    // ֹͣ���̼���
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

    // ���״̬û�䣬�����κβ���
    if (currentState == enable) return;

    pidDebugEnabled.store(enable);

    // ���������PID����
    if (enable) {
        // �������̼���
        keyboardListener->Start();

        // ��ӡ��ǰPID����
        std::cout << "PID����ģʽ������" << std::endl;
        std::cout << "ʹ�����¼�����PID������" << std::endl;
        std::cout << "Q/W: ����aimFov  - ��ǰֵ: " << aimFov << std::endl;
        std::cout << "A/S: ����Y��������� - ��ǰֵ: " << pidController.yControlFactor.load() << std::endl;
        std::cout << "Z/X: ����Kd (΢��ϵ��) - ��ǰֵ: " << pidController.kd.load() << std::endl;
        std::cout << "E/R: ����ѹǹʱ�� - ��ǰֵ: " << sharedState->pressTime.load() << "ms" << std::endl;
        std::cout << "D/F: ����ѹǹ���� - ��ǰֵ: " << sharedState->pressForce.load() << std::endl;
        std::cout << "C: �л�ѹǹ���� - ��ǰ״̬: " << (sharedState->isRecoilControlEnabled.load() ? "����" : "�ر�") << std::endl;

        // ����PID�����߳�
        pidDebugThread = std::thread(PIDDebugLoop);
    }
    else {
        // ֹͣ���̼���
        keyboardListener->Stop();

        std::cout << "PID����ģʽ�ѽ���" << std::endl;
    }
}

bool ActionModule::IsPIDDebugEnabled() {
    return pidDebugEnabled.load();
}

void ActionModule::SetUsePrediction(bool enable) {
    usePrediction.store(enable);
    std::cout << "��׼ģʽ���л�Ϊ: " << (enable ? "Ԥ��ģʽ" : "PID����ģʽ") << std::endl;

    // ����л�ΪPIDģʽ������PID������
    if (!enable) {
        pidController.Reset();
    }
    else {
        // ����л�ΪԤ��ģʽ������Ԥ��״̬
        hasLastTarget = false;
    }
}

bool ActionModule::IsUsingPrediction() {
    return usePrediction.load();
}

void ActionModule::SetPredictAlpha(float alpha) {
    if (alpha >= 0.0f && alpha <= 1.0f) {
        predictAlpha = alpha;
        std::cout << "Ԥ��ϵ��������Ϊ: " << alpha << std::endl;
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
    const int pressTimeStep = 20; // 20ms����

    switch (key) {
    case 'Q':
        aimFov = max(0, aimFov - 1);
        std::cout << "aimFov ����Ϊ: " << aimFov << std::endl;
        break;
    case 'W':
        aimFov += 1;
        std::cout << "aimFov ����Ϊ: " << aimFov << std::endl;
        break;
    case 'A':
        yControlFactor = max(0.0f, yControlFactor - yFactorStep);
        pidController.yControlFactor.store(yControlFactor);
        std::cout << "Y��������� ����Ϊ: " << yControlFactor << std::endl;
        break;
    case 'S':
        yControlFactor += yFactorStep;
        pidController.yControlFactor.store(yControlFactor);
        std::cout << "Y��������� ����Ϊ: " << yControlFactor << std::endl;
        break;
    case 'Z':
        kd = max(0.0f, kd - dStep);
        pidController.kd.store(kd);
        std::cout << "Kd ����Ϊ: " << kd << std::endl;
        break;
    case 'X':
        kd += dStep;
        pidController.kd.store(kd);
        std::cout << "Kd ����Ϊ: " << kd << std::endl;
        break;
    case 'P': // �л�Ԥ��ģʽ��PIDģʽ
        SetUsePrediction(!IsUsingPrediction());
        break;
        // ѹǹ��ذ���
    case 'E': // ����ѹǹʱ��
        pressTime = max(0, pressTime - pressTimeStep);
        sharedState->pressTime.store(pressTime);
        std::cout << "ѹǹʱ�� ����Ϊ: " << pressTime << "ms" << std::endl;
        break;
    case 'R': // ����ѹǹʱ��
        pressTime += pressTimeStep;
        sharedState->pressTime.store(pressTime);
        std::cout << "ѹǹʱ�� ����Ϊ: " << pressTime << "ms" << std::endl;
        break;
    case 'D': // ����ѹǹ����
        pressForce = max(0.0f, pressForce - pressForceStep);
        sharedState->pressForce.store(pressForce);
        std::cout << "ѹǹ���� ����Ϊ: " << pressForce << std::endl;
        break;
    case 'F': // ����ѹǹ����
        pressForce += pressForceStep;
        sharedState->pressForce.store(pressForce);
        std::cout << "ѹǹ���� ����Ϊ: " << pressForce << std::endl;
        break;
    case 'C': // �л�ѹǹ����
    {
        bool newState = !sharedState->isRecoilControlEnabled.load();
        sharedState->isRecoilControlEnabled.store(newState);
        std::cout << "ѹǹ����: " << (newState ? "����" : "�ر�") << std::endl;
    }
    break;
    }
}

void ActionModule::PIDDebugLoop() {
    while (running.load() && pidDebugEnabled.load()) {
        // �̶߳���˯�ߣ�����CPUʹ��
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ��һ���ƶ�ֵ��ָ����Χ
std::pair<float, float> ActionModule::NormalizeMovement(float x, float y, float maxValue) {
    // ������������
    float length = std::sqrt(x * x + y * y);

    // �������Ϊ0������(0,0)
    if (length < 0.001f) {
        return { 0.0f, 0.0f };
    }

    // �����һ������
    float factor = min(length, maxValue) / length;

    // ���ع�һ�����ֵ
    return { x * factor, y * factor };
}

// PID�����㷨
std::pair<float, float> ActionModule::ApplyPIDControl(float errorX, float errorY) {
    auto currentTime = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(currentTime - pidController.lastTime).count();
    pidController.lastTime = currentTime;

    // ��ֹdt��С��Ϊ�㵼�¼�������
    if (dt < 0.001f) dt = 0.001f;

    // ��ȡPID����
    float kp = pidController.kp.load();
    float ki = pidController.ki.load();
    float kd = pidController.kd.load();

    // ����΢���� ��FPSֱ��ͨ��YOLO�۲���д�������������PID�е�D��Ŵ�����������һ����0
    float derivativeX = (errorX - pidController.previousErrorX) / dt;
    float derivativeY = (errorY - pidController.previousErrorY) / dt;

    // ���»����� (�����ƣ���ֹ���ֱ���)
    pidController.integralX += errorX * dt;
    pidController.integralY += errorY * dt;

    // ��������
    pidController.integralX = max(-pidController.integralLimit,
        min(pidController.integralX, pidController.integralLimit));
    pidController.integralY = max(-pidController.integralLimit,
        min(pidController.integralY, pidController.integralLimit));

    // ����PID���
    float outputX = kp * errorX + ki * pidController.integralX + kd * derivativeX;
    float outputY = kp * errorY + ki * pidController.integralY + kd * derivativeY;

    // ���浱ǰ���´�ʹ��
    pidController.previousErrorX = errorX;
    pidController.previousErrorY = errorY;

    return { outputX, outputY };
}

// Ԥ��Ŀ����һ֡λ��
Point2D ActionModule::PredictNextPosition(const Point2D& current) {
    // ���û����һ֡λ�ã��޷�Ԥ�⣬ֱ�ӷ��ص�ǰλ��
    if (!hasLastTarget) {
        hasLastTarget = true;
        lastTargetPos = current;
        return current;
    }

    // ʹ�ü�����Ԥ��: predicted = current + (current - last) * alpha
    Point2D predicted;
    predicted.x = current.x + (current.x - lastTargetPos.x) * predictAlpha;
    predicted.y = current.y + (current.y - lastTargetPos.y) * predictAlpha;

    // ������һ֡λ��
    lastTargetPos = current;

    // ����Ԥ��λ��
    return predicted;
}

// Ӧ��ѹǹЧ��������Y����ѹ��
float ActionModule::ApplyRecoilControl() {
    // ���ѹǹ�����Ƿ���
    if (!sharedState->isRecoilControlEnabled.load()) {
        return 0.0f;
    }

    // ��ȡ��ǰʱ���
    auto currentTime = std::chrono::steady_clock::now();

    // ��������갴�º��ʱ���
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        currentTime - recoilState.pressStartTime).count();

    // ��ȡѹǹ����ʱ��
    int pressTime = sharedState->pressTime.load();

    // �������ѹǹ����ʱ�䣬ֹͣѹǹ
    if (elapsedTime > pressTime) {
        return 0.0f;
    }

    // ����ѹǹ����
    return sharedState->pressForce.load();
}

// �����߳� - ����������͸���Ŀ��״̬
void ActionModule::ProcessLoop() {
    bool prevSideButton1State = false; // ���ڼ����1״̬�仯
    bool prevSideButton2State = false; // ���ڼ����2״̬�仯

    // ����PID������
    pidController.Reset();

    // ������ѭ��
    while (running.load()) {
        // �������鿪�أ����1��
        bool currentSideButton1State = mouseController->IsSideButton1Down();
        if (currentSideButton1State && !prevSideButton1State) {
            sharedState->isAutoAimEnabled = !sharedState->isAutoAimEnabled; // �л�����״̬
            std::cout << "���鹦��: " << (sharedState->isAutoAimEnabled ? "����" : "�ر�") << std::endl;

            // ÿ�ο�������ʱ����PID������
            pidController.Reset();
            hasLastTarget = false; // ����Ԥ��״̬
        }
        prevSideButton1State = currentSideButton1State;

        // �����Զ����𿪹أ����2��
        bool currentSideButton2State = mouseController->IsSideButton2Down();
        if (currentSideButton2State && !prevSideButton2State) {
            bool newState = !sharedState->isAutoFireEnabled.load();
            sharedState->isAutoFireEnabled = newState; // �л��Զ�����״̬

            std::cout << "�Զ�������: " << (newState ? "����" : "�ر�") << std::endl;
        }
        prevSideButton2State = currentSideButton2State;

        // ��ȡ����Ԥ����
        PredictionResult prediction;
        bool hasPrediction = PredictionModule::GetLatestPrediction(prediction);

        // �������ЧԤ����
        if (hasPrediction && prediction.x != 999 && prediction.y != 999) {
            // ��ȡ��Ļ�ֱ���
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);

            // ������Ļ���ĵ�
            int screenCenterX = screenWidth / 2;
            int screenCenterY = screenHeight / 2;

            // ͼ�����ĵ����Ͻ�ƫ�Ƶ���Ļ���ĵ�ͼ�����Ͻ�
            float offsetX = screenCenterX - 320.0f / 2;
            float offsetY = screenCenterY - 320.0f / 2;

            // ����Ŀ������
            int targetX = static_cast<int>(offsetX + prediction.x);
            int targetY = static_cast<int>(offsetY + prediction.y);

            // �������Ļ���ĵ�Ŀ�����Ծ���
            float centerToTargetX = static_cast<float>(targetX - screenCenterX);
            float centerToTargetY = static_cast<float>(targetY - screenCenterY);

            // ���㳤��
            float length = std::sqrt(centerToTargetX * centerToTargetX + centerToTargetY * centerToTargetY);

            // ����Ŀ��������ЧĿ���־������״̬
            sharedState->targetDistance = length;
            // ����Ŀ���������ʱ���
            sharedState->targetValidSince = std::chrono::steady_clock::now();

            // �������鹦��
            if (sharedState->isAutoAimEnabled && mouseController && !mouseController->IsMouseMoving() && length < aimFov) {
                bool isPredictionMode = usePrediction.load();

                // ��ȡѹǹ��ѹ��
                float recoilYOffset = 0.0f;
                if (sharedState->needPressDownWhenAim.load() && sharedState->isRecoilControlEnabled.load() &&
                    (mouseController->IsLeftButtonDown() || recoilState.isLeftButtonPressed.load())) {
                    recoilYOffset = ApplyRecoilControl();
                }

                // ���ݵ�ǰģʽѡ��ʹ��PID���ƻ�Ԥ�⹦��
                if (isPredictionMode) {
                    // Ԥ��ģʽ
                    if (length >= deadZoneThreshold) {
                        // ��һ���ƶ�ֵ����10��Χ
                        auto normalizedMove = NormalizeMovement(centerToTargetX, centerToTargetY + recoilYOffset, 15.0f);

                        // ��ǰĿ������
                        Point2D currentTarget = { normalizedMove.first, normalizedMove.second };

                        // ���ƶ������Ͻ��м�Ԥ��
                        Point2D predictedTarget = PredictNextPosition(currentTarget);

                        // ʹ�ÿ������ƶ����(�������)
                        mouseController->MoveToWithTime(
                            static_cast<int>(predictedTarget.x),
                            static_cast<int>(predictedTarget.y),
                            length * humanizationFactor  // ����������˻�����
                        );

                        // ֪ͨԤ��ģ������ƶ��ˣ����ڲ�����겹������
                        PredictionModule::NotifyMouseMovement(normalizedMove.first, normalizedMove.second);
                    }
                }
                else {
                    // PID����ģʽ
                    if (length >= deadZoneThreshold) {
                        // ʹ��PID�����������ƶ�ֵ
                        auto pidOutput = ApplyPIDControl(centerToTargetX, centerToTargetY);

                        // ��ȡY���������
                        float yControlFactor = pidController.yControlFactor.load();

                        // Ӧ��Y��������Ȳ�����ѹǹ��ѹ��
                        pidOutput.second = pidOutput.second * yControlFactor + recoilYOffset;

                        // ��һ���ƶ�ֵ����10��Χ
                        auto normalizedMove = NormalizeMovement(pidOutput.first, pidOutput.second, 10.0f);

                        // ʹ�ÿ������ƶ����(�������)
                        mouseController->MoveToWithTime(
                            static_cast<int>(normalizedMove.first),
                            static_cast<int>(normalizedMove.second),
                            length * humanizationFactor  // ����������˻�����
                        );
                    }
                }
            }
            else if (!sharedState->isAutoAimEnabled) {
                // ������ر�ʱ���ÿ�����״̬
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


// ��־ʱ�����������
std::string NowTimeString() {
    auto now = std::chrono::system_clock::now();
    auto t_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t_c), "%H:%M:%S");
    return ss.str();
}

// �򵥵� debug log����ʱ��
#define LogDebug(msg) std::cout << "[" << NowTimeString() << "][DEBUG] " << msg << std::endl

void ActionModule::RecoilControlLoop() {
    bool prevLeftButtonState = false;

    LogDebug("RecoilControlLoop started.");

    while (running.load()) {
        bool isRecoilEnabled = sharedState->isRecoilControlEnabled.load();
        bool currentLeftButtonState = mouseController->IsLeftButtonDown();

        // ����������/�ɿ�
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

        // ����Ƿ���Ҫѹǹ
        if (isRecoilEnabled && recoilState.isLeftButtonPressed.load()) {

            auto currentTime = std::chrono::steady_clock::now();
            auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - recoilState.pressStartTime).count();
            auto timeSinceLastRecoil = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - recoilState.lastRecoilTime).count();

            // ��ѹǹ�ж�
            if (elapsedTime <= sharedState->pressTime.load() && timeSinceLastRecoil >= 16) {
                float pressForce = sharedState->pressForce.load();
                mouseController->MoveToWithTime(0, static_cast<int>(pressForce), 100);
                recoilState.lastRecoilTime = currentTime;
            }
        }

        // �߳�����
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    LogDebug("RecoilControlLoop ended.");
}

// ��������߳� - ������ƿ�����Ϊ
void ActionModule::FireControlLoop() {
    // ����״̬��״̬
    enum class FireState {
        IDLE,           // ����״̬
        BURST_ACTIVE,   // ���伤��״̬
        BURST_COOLDOWN  // ������ȴ״̬
    };

    FireState currentState = FireState::IDLE;

    // �����������
    std::random_device rd;
    std::mt19937 gen(rd());

    // �������ʱ��ֲ� (80ms-150ms)
    std::uniform_int_distribution<> burstDuration(80, 150);

    // ������ȴʱ��ֲ� (180ms-400ms)
    std::uniform_int_distribution<> burstCooldown(180, 400);

    // ��������ͣ�� (20ms-50ms)
    std::uniform_int_distribution<> microPause(20, 50);

    auto lastStateChangeTime = std::chrono::steady_clock::now();
    int currentDuration = 0;

    while (running.load()) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastStateChangeTime).count();

        // ��ȡ��ǰ����״̬
        bool autoFireEnabled = sharedState->isAutoFireEnabled.load();

        float targetDistance = sharedState->targetDistance.load();

        // ����Ŀ���������ʱ���뵱ǰʱ��Ĳ�ֵ
        auto targetTimeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - sharedState->targetValidSince).count();

        // �ж�Ŀ���Ƿ�����Чʱ�䴰���� (��ǰʱ�� - ���Ŀ��ʱ�� < ��Ч����ʱ��)
        bool targetInValidTimeWindow = targetTimeDiff < targetValidDurationMs;

        // ����Զ�����رգ�ȷ������ͷŲ�����״̬
        if (!autoFireEnabled) {
            if (mouseController->IsLeftButtonDown()) {
                mouseController->LeftUp();
                // ���������ʱ������ѹǹ״̬����
               // recoilState.isLeftButtonPressed = false;
            }
            currentState = FireState::IDLE;
            Sleep(50); // ����CPUʹ��
            continue;
        }

        // ״̬���߼�
        switch (currentState) {
        case FireState::IDLE:
            // �ӿ���״̬תΪ����״̬�����������Ŀ���Ƿ�����Чʱ�䴰�����Ҿ������
            if (autoFireEnabled && targetInValidTimeWindow && targetDistance < deadZoneThreshold) {
                mouseController->LeftDown();
                // ģ�ⰴ�����ʱ������ѹǹ״̬
                if (sharedState->isRecoilControlEnabled.load()) {
                    // recoilState.isLeftButtonPressed = true;
                   // recoilState.pressStartTime = std::chrono::steady_clock::now();
                   // recoilState.lastRecoilTime = std::chrono::steady_clock::now();
                }
                currentState = FireState::BURST_ACTIVE;
                currentDuration = burstDuration(gen); // ����������ʱ��
                lastStateChangeTime = currentTime;
                std::cout << "��ʼ���䣬����: " << currentDuration << "ms" << std::endl;
            }
            else {
                Sleep(10); // ����CPUʹ��
            }
            break;

        case FireState::BURST_ACTIVE:
            // �������ʱ�����
            if (elapsedTime >= currentDuration) {
                mouseController->LeftUp();
                // ���������ʱ������ѹǹ״̬����
              //   recoilState.isLeftButtonPressed = false;
                currentState = FireState::BURST_COOLDOWN;
                currentDuration = burstCooldown(gen); // �����ȴʱ��
                lastStateChangeTime = currentTime;
                std::cout << "�����������ȴ: " << currentDuration << "ms" << std::endl;
            }
            // ֻ�е�Ŀ�겻����Чʱ�䴰���Ҿ����Զʱ����ֹͣ����
            else if (!targetInValidTimeWindow && targetDistance >= 10.0f) {
                mouseController->LeftUp();
                // ���������ʱ������ѹǹ״̬����
             //    recoilState.isLeftButtonPressed = false;
                currentState = FireState::IDLE;
                std::cout << "Ŀ�궪ʧ��ֹͣ����" << std::endl;
            }
            break;

        case FireState::BURST_COOLDOWN:
            // ��ȴʱ�����
            if (elapsedTime >= currentDuration) {
                // ���Ŀ������Чʱ�䴰�����Ҿ�����ʣ���ʼ��һ�ֵ���
                if (targetInValidTimeWindow && targetDistance < 10.0f) {
                    // ��С���ʲ������ͣ�٣�����������
                    if (gen() % 3 == 0) { // Լ33%�ĸ���
                        Sleep(microPause(gen));
                    }

                    mouseController->LeftDown();
                    // ģ�ⰴ�����ʱ������ѹǹ״̬
                    if (sharedState->isRecoilControlEnabled.load()) {
                        //     recoilState.isLeftButtonPressed = true;
                   //      recoilState.pressStartTime = std::chrono::steady_clock::now();
                  //       recoilState.lastRecoilTime = std::chrono::steady_clock::now();
                    }
                    currentState = FireState::BURST_ACTIVE;
                    currentDuration = burstDuration(gen);
                    lastStateChangeTime = currentTime;
                    std::cout << "��ʼ�µ��䣬����: " << currentDuration << "ms" << std::endl;
                }
                else {
                    currentState = FireState::IDLE;
                }
            }
            break;
        }

        // ����˯�ߣ�����CPUʹ����
        Sleep(1);
    }

    // ȷ���˳�ʱ�ͷ���갴��
    if (mouseController->IsLeftButtonDown()) {
        mouseController->LeftUp();
        // ���������ʱ������ѹǹ״̬����
     //    recoilState.isLeftButtonPressed = false;
    }
}