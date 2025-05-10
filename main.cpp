// ȷ����Щ�궨�����κ�Windowsͷ�ļ�֮ǰ
#define WIN32_LEAN_AND_MEAN  // ����Windowsͷ�ļ�����������
#define NOMINMAX             // ��ֹWindows����min/max����STL��ͻ

// ���ݱ��������Ŀ��ܹ�
#if defined(_MSC_VER)        // ��Visual Studio
#if defined(_M_X64) || defined(_M_AMD64)
#define _AMD64_      // 64λx86
#elif defined(_M_IX86)
#define _X86_        // 32λx86
#elif defined(_M_ARM64)
#define _ARM64_      // 64λARM
#elif defined(_M_ARM)
#define _ARM_        // 32λARM
#else
#error "Unknown architecture"
#endif
#endif

#include <iostream>
#include <thread>
#include <Windows.h>
#include <processthreadsapi.h>
#include <psapi.h>
#include <mmsystem.h>        // ����timeBeginPeriod/timeEndPeriod

#pragma comment(lib, "winmm.lib")  // ����timeBeginPeriod/timeEndPeriod�����

#include "ScreenCaptureWindows.h"
#include "DetectionModule.h"
#include "PredictionModule.h"
#include "ActionModule.h"
#include <opencv2/opencv.hpp>
#include "WindowsMouseController.h"
#include "HumanLikeMovement.h"
#include "CaptureFactory.h"

// ���ø����ȼ�����Դ����
bool SetHighPriorityAndResources() {
    HANDLE hProcess = GetCurrentProcess();

    // 1. ���ý������ȼ�Ϊ������ͨ���ȼ�
    if (!SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS)) {
        std::cerr << "Failed to set process priority. Error code: " << GetLastError() << std::endl;
        return false;
    }

    // 2. �����ڴ湤������С��Ԥ�������ڴ棩
    SIZE_T minWorkingSetSize = 100 * 1024 * 1024; // 100MB
    SIZE_T maxWorkingSetSize = 1024 * 1024 * 1024; // 1GB
    if (!SetProcessWorkingSetSize(hProcess, minWorkingSetSize, maxWorkingSetSize)) {
        std::cerr << "Failed to set working set size. Error code: " << GetLastError() << std::endl;
        // ����ִ�У���ֻ���Ż���ʧ�ܲ�Ӱ����Ҫ����
    }

    // 3. ���ô������׺��� - ʹ�����п��ú���
    DWORD_PTR processAffinityMask;
    DWORD_PTR systemAffinityMask;

    if (GetProcessAffinityMask(hProcess, &processAffinityMask, &systemAffinityMask)) {
        // ʹ��ϵͳ���õ����к���
        if (!SetProcessAffinityMask(hProcess, systemAffinityMask)) {
            std::cerr << "Failed to set processor affinity. Error code: " << GetLastError() << std::endl;
        }
    }

    // 4. ����IO���ȼ� - ע�⣺���д�����ܵ������⣬��������볢��ע����
    // PROCESS_MODE_BACKGROUND_BEGIN ���ڽ���I/O���ȼ���������������ǵ�Ŀ���෴
    // ����������I/O���ȼ���������Ҫʹ����������
    /*if (!SetPriorityClass(hProcess, PROCESS_MODE_BACKGROUND_BEGIN)) {
        std::cerr << "Failed to set I/O priority. Error code: " << GetLastError() << std::endl;
    }*/

    std::cout << "Process priority and resource settings applied successfully." << std::endl;
    return true;
}

// �����̵߳����ȼ�
void SetThreadHighPriority(std::thread& thread) {
    HANDLE hThread = thread.native_handle();
    if (!SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST)) {
        std::cerr << "Failed to set thread priority. Error code: " << GetLastError() << std::endl;
    }
}

int main() {
    // ���ø����ȼ�����Դ����
    if (!SetHighPriorityAndResources()) {
        std::cerr << "Warning: Could not set high priority for the process." << std::endl;
        // ����ִ�У�����������
    }

    std::thread captureThread;
    std::thread detectionThread;
    std::thread predictionThread;
    std::thread actionThread;

    try {
        // ��ʼ���ɼ�ģ��
        captureThread = CaptureModule::Initialize(CaptureType::NETWORK_STREAM);
        // �����߳����ȼ�
        SetThreadHighPriority(captureThread);

        // ��ʼ�����ģ��
        detectionThread = DetectionModule::Initialize("./123.onnx");
        // �����߳����ȼ�
        SetThreadHighPriority(detectionThread);

        // ��ʼ��Ԥ��ģ��
        predictionThread = PredictionModule::Initialize();
        // �����߳����ȼ�
        SetThreadHighPriority(predictionThread);

        // ��ʼ������ģ�飨���˲�����Ϊ50��
        actionThread = ActionModule::Initialize(50);
        // �����߳����ȼ�
        SetThreadHighPriority(actionThread);

        // ��ѡ�������Զ�����
        ActionModule::EnableAutoFire(false);

        // ��ѡ���ɼ�ģ��-->���õ���ģʽ����ʾ����
        CaptureModule::SetCaptureDebug(true);

        // ��ѡ�����õ���ģʽ����ʾ����
        DetectionModule::SetDebugMode(false);
        DetectionModule::SetShowDetections(true);

        // ����Windows��ʱ�����ȣ�����ϵͳ����
        timeBeginPeriod(1);

        // ��ѭ��
        while (CaptureModule::IsRunning() &&
            DetectionModule::IsRunning() &&
            PredictionModule::IsRunning() &&
            ActionModule::IsRunning()) {

            // ��ESC���˳�
            if (cv::waitKey(1) == 27) {
                CaptureModule::Stop();
                DetectionModule::Stop();
                PredictionModule::Stop();
                ActionModule::Stop();
                break;
            }

            // ������ѭ��Ƶ��
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // �ָ�Windows��ʱ������
        timeEndPeriod(1);
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    // ֹͣ����ģ��
    CaptureModule::Stop();
    DetectionModule::Stop();
    PredictionModule::Stop();
    ActionModule::Stop();

    // ������Դ
    CaptureModule::Cleanup();
    DetectionModule::Cleanup();
    PredictionModule::Cleanup();
    ActionModule::Cleanup();

    // �ȴ��߳̽���
    if (captureThread.joinable()) {
        captureThread.join();
    }

    if (detectionThread.joinable()) {
        detectionThread.join();
    }

    if (predictionThread.joinable()) {
        predictionThread.join();
    }

    if (actionThread.joinable()) {
        actionThread.join();
    }

    // �ͷ���Դ
    cv::destroyAllWindows();

    std::cout << "Program terminated normally" << std::endl;
    return 0;
}

// ��������ƶ�����
void TestMouseMovement(int targetX, int targetY, int humanizationFactor = 50) {
    std::cout << "��ʼ����ƶ�����..." << std::endl;
    std::cout << "Ŀ��λ��: (" << targetX << ", " << targetY << ")" << std::endl;
    std::cout << "���˻�����: " << humanizationFactor << std::endl;

    // ������������
    std::unique_ptr<MouseController> mouseController = std::make_unique<WindowsMouseController>();

    // ��ȡ��ǰ���λ��
    int currentX = 0, currentY = 0;
    mouseController->GetCurrentPosition(currentX, currentY);
    std::cout << "��ǰ���λ��: (" << currentX << ", " << currentY << ")" << std::endl;

    // �������
    float distance = std::sqrt(std::pow(targetX - currentX, 2) + std::pow(targetY - currentY, 2));
    std::cout << "��Ŀ��ľ���: " << distance << std::endl;

    // ���ɱ�����·��
    std::vector<std::pair<float, float>> path = HumanLikeMovement::GenerateBezierPath(
        currentX, currentY, targetX, targetY, humanizationFactor,
        std::max(10, static_cast<int>(distance / 5)));

    std::cout << "����·��������: " << path.size() << std::endl;

    // Ӧ���ٶ�����
    path = HumanLikeMovement::ApplySpeedProfile(path, humanizationFactor);
    std::cout << "Ӧ���ٶ����ߺ�·��������: " << path.size() << std::endl;

    // ����·���㣬Ӧ�ö������ƶ����
    for (size_t i = 0; i < path.size(); ++i) {
        // ���΢С����
        auto jitteredPoint = HumanLikeMovement::AddHumanJitter(
            path[i].first, path[i].second, distance, humanizationFactor);

        // �ƶ���굽�������λ��
        mouseController->MoveTo(
            static_cast<int>(jitteredPoint.first), static_cast<int>(jitteredPoint.second));

        // ��ӡ������Ϣ
        if (i % 5 == 0 || i == path.size() - 1) {  // ÿ5��������һ�������һ��
            std::cout << "�ƶ���: (" << jitteredPoint.first << ", " << jitteredPoint.second
                << ") [ԭʼ·����: (" << path[i].first << ", " << path[i].second << ")]" << std::endl;
        }

        // �����ƶ��ٶ�
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }

    // ��ȡ�������λ��
    mouseController->GetCurrentPosition(currentX, currentY);
    std::cout << "������ɡ��������λ��: (" << currentX << ", " << currentY << ")" << std::endl;
}

int main1() {
    // ���ø����ȼ�����Դ����
    SetHighPriorityAndResources();

    int targetX, targetY, humanFactor;

    std::cout << "����Ŀ��X����: ";
    std::cin >> targetX;

    std::cout << "����Ŀ��Y����: ";
    std::cin >> targetY;

    std::cout << "�������˻�����(1-100): ";
    std::cin >> humanFactor;
    humanFactor = std::max(1, std::min(100, humanFactor));

    // ִ�в���
    TestMouseMovement(targetX, targetY, humanFactor);
    return 0;
}