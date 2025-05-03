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

// ��̬��Ա��ʼ��
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
    // ȷ����Χ��1-100֮��
    humanFactor = max(1, min(100, humanFactor));
    humanizationFactor.store(humanFactor);

    // ������־Ŀ¼
    std::filesystem::create_directories("logs");

    // ��ʼ�����˻��ƶ�
    HumanLikeMovement::Initialize();

    // ���û����������������ʹ��WindowsĬ��ʵ��
    if (!mouseController) {
        //����Windows��������������ָ��
        mouseController = std::make_unique<WindowsMouseController>();
    }

    // ��ȡ��ǰ���λ��
    UpdateCurrentMousePosition();

    // ��¼��ʼ����Ϣ
    std::stringstream ss;
    ss << "ActionModule initialized with humanization factor: " << humanFactor;
    LogMouseMovement(ss.str());

    // �������б�־
    running.store(true);

    // ���������߳�
    actionThread = std::thread(ProcessLoop);

    // return actionThread; std::thread ���ܱ�������copy����ֻ�ܱ��ƶ���move�� ������Ҫʹ��std::move ��thread������Ȩת�Ƶ�������
    return std::move(actionThread);;
}

void ActionModule::Cleanup() {
    // ֹͣģ��
    Stop();

    // �ȴ��߳̽���
    if (actionThread.joinable()) {
        actionThread.join();
    }

    // ��¼������Ϣ
    LogMouseMovement("ActionModule cleaned up");
}

void ActionModule::Stop() {
    running.store(false);
}

bool ActionModule::IsRunning() {
    return running.load();
}

void ActionModule::SetHumanizationFactor(int factor) {
    // ȷ����Χ��1-100֮��
    factor = max(1, min(100, factor));
    humanizationFactor.store(factor);

    // ��¼�������
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

    // ��¼����״̬���
    std::stringstream ss;
    ss << "Auto fire " << (enable ? "enabled" : "disabled");
    LogMouseMovement(ss.str());
}

bool ActionModule::IsAutoFireEnabled() {
    return fireEnabled.load();
}

void ActionModule::LogMouseMovement(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);

    // ��ȡ��ǰʱ��
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm now_tm;
    localtime_s(&now_tm, &now_time_t);

    // ����־�ļ�
    std::ofstream logFile("logs/mouseLog.log", std::ios::app);
    if (logFile.is_open()) {
        // д��ʱ�������Ϣ
        logFile << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count()
            << " - " << message << std::endl;
        logFile.close();
    }
}

// ����ģ����Ҫ�Ĵ���ѭ��
void ActionModule::ProcessLoop() {
    // ���ô���ѭ������ʼʱ��
    lastUpdateTime = std::chrono::high_resolution_clock::now();//  ---����ȡϵͳ��ǰʱ���Ĺ��ߣ����ȸߣ��ʺ����ڲ�������ִ��ʱ�����ʱ���

    // �������ڻ���·����ı���
    std::vector<std::pair<float, float>> currentPath;
    size_t pathIndex = 0;
    bool targetLost = false;

    // ������ѭ��
    while (running.load()) {
        // ��ȡ��ǰʱ��
        auto currentTime = std::chrono::high_resolution_clock::now();

        // ��������ϴθ��¾�����ʱ��
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastUpdateTime).count();

        // ��ȡ�������λ��
        UpdateCurrentMousePosition();

        // ��ȡ����Ԥ����
        PredictionResult prediction;
        bool hasPrediction = PredictionModule::GetLatestPrediction(prediction);

        // �ж�Ŀ���Ƿ�ʧ
        if (!hasPrediction || prediction.x == 999 || prediction.y == 999) {
            // ����ǵ�һ�ζ�ʧĿ��
            if (!targetLost) {
                targetLost = true;

                // �������˻���ʧĿ����Ϊ
                currentPath = HumanLikeMovement::GenerateTargetLossMovement(
                    currentX, currentY, humanizationFactor.load(), 30);
                pathIndex = 0;

                LogMouseMovement("Target lost, generating human-like movement");
            }

            // ������꿪��״̬
            ControlMouseFire(false);

            // �������·���У�����ִ�ж�ʧĿ������˻��ƶ�
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
            // Ŀ��δ��ʧ
            targetLost = false;

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
            targetX = static_cast<int>(offsetX + prediction.x);
            targetY = static_cast<int>(offsetY + prediction.y);


            std::cout << "Screen resolution: " << screenWidth << "x" << screenHeight << std::endl;
            std::cout << "Raw prediction: (" << prediction.x << ", " << prediction.y << ")" << std::endl;
            std::cout << "Converted target: (" << targetX << ", " << targetY << ")" << std::endl;

            // ��־��¼�µ�����ת������
            std::stringstream ssCoord;
            ssCoord << "Using direct prediction coordinates: (" << targetX << ", " << targetY << ")";
            LogMouseMovement(ssCoord.str());

            // �������
            float distance = std::sqrt(
                std::pow(targetX - currentX, 2) + std::pow(targetY - currentY, 2));

            // ����: �������Ƿ��Ѿ��㹻����Ŀ��
            if (distance < AIM_THRESHOLD) {
                // �������С����ֵ����Ϊ�Ѿ���׼��Ŀ��
                std::stringstream ss;
                ss << "Target already aimed. Distance: " << distance << " (below threshold " << AIM_THRESHOLD << ")";
                LogMouseMovement(ss.str());

                // ������꿪��
                ControlMouseFire(true);

                // ����Ҫ�����µ�·��
                currentPath.clear();
                pathIndex = 0;
            }
            // ��������㹻�������µ����˻�·��
            else if (distance > 5 || currentPath.empty() || pathIndex >= currentPath.size()) {
                int human = humanizationFactor.load();
                currentPath = HumanLikeMovement::GenerateBezierPath(
                    currentX, currentY, targetX, targetY, human,
                    max(10, static_cast<int>(distance / 5)));

                // Ӧ���ٶ�����
                currentPath = HumanLikeMovement::ApplySpeedProfile(currentPath, human);
                pathIndex = 0;

                std::stringstream ss;
                ss << "New path generated to target: (" << targetX << ", " << targetY
                    << "), distance: " << distance;
                LogMouseMovement(ss.str());
            }

            // �����·���㣬�ƶ�����һ����
            while (pathIndex < currentPath.size()) {
                auto nextPoint = currentPath[pathIndex];

                // ���΢С����
                auto jitteredPoint = HumanLikeMovement::AddHumanJitter(
                    nextPoint.first, nextPoint.second, distance, humanizationFactor.load());

                // �ƶ����
                mouseController->MoveTo(
                    static_cast<int>(jitteredPoint.first), static_cast<int>(jitteredPoint.second));

                std::stringstream ss;
                ss << "Moving to: (" << jitteredPoint.first << ", " << jitteredPoint.second << ")";
                std::cout << ss.str() << std::endl;
                LogMouseMovement(ss.str());

                pathIndex++;

                // ������꿪��
                bool closeToTarget = (pathIndex > currentPath.size() * 0.8);
                ControlMouseFire(closeToTarget);

                // ��һС�ξͺã���Ҫ����
                if (pathIndex > 10) {
                    break;
                }
            }
        }

        // ���������ʱ��
        lastUpdateTime = currentTime;

        // ����ѭ��Ƶ�ʣ���Լ60fps  ��Ϊ5���Կ�
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

    // ����������Զ�����
    if (fireEnabled.load()) {
        // ���Ŀ��ɼ����ҿ���Ŀ�꣬�ҵ�ǰû�п���
        if (targetVisible && !firing) {
            mouseController->LeftDown();
            firing = true;
            LogMouseMovement("Mouse left button down");
        }
        // ���Ŀ�궪ʧ����Զ��Ŀ�꣬�ҵ�ǰ���ڿ���
        else if (!targetVisible && firing) {
            mouseController->LeftUp();
            firing = false;
            LogMouseMovement("Mouse left button up");
        }
    }
    // ����������Զ�����ȷ���������ͷ�
    else if (firing) {
        mouseController->LeftUp();
        firing = false;
        LogMouseMovement("Mouse left button up (auto fire disabled)");
    }
}