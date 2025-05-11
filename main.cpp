// 确保这些宏定义在任何Windows头文件之前
#define WIN32_LEAN_AND_MEAN  // 减少Windows头文件包含的内容
#define NOMINMAX             // 防止Windows定义min/max宏与STL冲突

// 根据编译器检测目标架构
#if defined(_MSC_VER)        // 仅Visual Studio
#if defined(_M_X64) || defined(_M_AMD64)
#define _AMD64_      // 64位x86
#elif defined(_M_IX86)
#define _X86_        // 32位x86
#elif defined(_M_ARM64)
#define _ARM64_      // 64位ARM
#elif defined(_M_ARM)
#define _ARM_        // 32位ARM
#else
#error "Unknown architecture"
#endif
#endif

#include <iostream>
#include <thread>
#include <Windows.h>
#include <processthreadsapi.h>
#include <psapi.h>
#include <mmsystem.h>        // 用于timeBeginPeriod/timeEndPeriod

#pragma comment(lib, "winmm.lib")  // 链接timeBeginPeriod/timeEndPeriod所需库

#include "ScreenCaptureWindows.h"
#include "DetectionModule.h"
#include "PredictionModule.h"
#include "ActionModule.h"
#include <opencv2/opencv.hpp>
#include "WindowsMouseController.h"
#include "HumanLikeMovement.h"
#include "CaptureFactory.h"
#include "KmboxNetMouseController.h"

// 设置高优先级和资源分配
bool SetHighPriorityAndResources() {
    HANDLE hProcess = GetCurrentProcess();

    // 1. 设置进程优先级为高于普通优先级
    if (!SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS)) {
        std::cerr << "Failed to set process priority. Error code: " << GetLastError() << std::endl;
        return false;
    }

    // 2. 设置内存工作集大小（预留更多内存）
    SIZE_T minWorkingSetSize = 100 * 1024 * 1024; // 100MB
    SIZE_T maxWorkingSetSize = 1024 * 1024 * 1024; // 1GB
    if (!SetProcessWorkingSetSize(hProcess, minWorkingSetSize, maxWorkingSetSize)) {
        std::cerr << "Failed to set working set size. Error code: " << GetLastError() << std::endl;
        // 继续执行，这只是优化，失败不影响主要功能
    }

    // 3. 设置处理器亲和性 - 使用所有可用核心
    DWORD_PTR processAffinityMask;
    DWORD_PTR systemAffinityMask;

    if (GetProcessAffinityMask(hProcess, &processAffinityMask, &systemAffinityMask)) {
        // 使用系统可用的所有核心
        if (!SetProcessAffinityMask(hProcess, systemAffinityMask)) {
            std::cerr << "Failed to set processor affinity. Error code: " << GetLastError() << std::endl;
        }
    }

    // 4. 设置IO优先级 - 注意：这行代码可能导致问题，如果出错请尝试注释它
    // PROCESS_MODE_BACKGROUND_BEGIN 用于降低I/O优先级，这里可能与我们的目标相反
    // 如果您想提高I/O优先级，可能需要使用其他函数
    /*if (!SetPriorityClass(hProcess, PROCESS_MODE_BACKGROUND_BEGIN)) {
        std::cerr << "Failed to set I/O priority. Error code: " << GetLastError() << std::endl;
    }*/

    std::cout << "Process priority and resource settings applied successfully." << std::endl;
    return true;
}

// 设置线程的优先级
void SetThreadHighPriority(std::thread& thread) {
    HANDLE hThread = thread.native_handle();
    if (!SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST)) {
        std::cerr << "Failed to set thread priority. Error code: " << GetLastError() << std::endl;
    }
}

int main() {
    // 设置高优先级和资源分配
    if (!SetHighPriorityAndResources()) {
        std::cerr << "Warning: Could not set high priority for the process." << std::endl;
        // 继续执行，不致命错误
    }

    std::thread captureThread;
    std::thread detectionThread;
    std::thread predictionThread;
    std::thread actionThread;

    try {
        // 初始化采集模块
        captureThread = CaptureModule::Initialize(CaptureType::NETWORK_STREAM);
        // 设置线程优先级
        SetThreadHighPriority(captureThread);

        // 初始化检测模块
        detectionThread = DetectionModule::Initialize("./123.onnx");
        // 设置线程优先级
        SetThreadHighPriority(detectionThread);

        // 初始化预测模块
        predictionThread = PredictionModule::Initialize();
        // 设置线程优先级
        SetThreadHighPriority(predictionThread);

        // 初始化动作模块
        actionThread = ActionModule::Initialize();
        // 设置线程优先级
        SetThreadHighPriority(actionThread);


        // 可选：采集模块-->设置调试模式和显示检测框
        CaptureModule::SetCaptureDebug(true);

        // 可选：设置调试模式和显示检测框
        DetectionModule::SetDebugMode(false);
        DetectionModule::SetShowDetections(true);

        // 设置预测模块的调试模式
        PredictionModule::SetDebugMode(true);  // 添加这一行启用预测调试

        // 禁用Windows定时器调度，减少系统干扰
        timeBeginPeriod(1);

        // 主循环
        while (CaptureModule::IsRunning() &&
            DetectionModule::IsRunning() &&
            PredictionModule::IsRunning() &&
            ActionModule::IsRunning()) {

            // 按ESC键退出
            if (cv::waitKey(1) == 27) {
                CaptureModule::Stop();
                DetectionModule::Stop();
                PredictionModule::Stop();
                ActionModule::Stop();
                break;
            }

            // 控制主循环频率
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 恢复Windows定时器调度
        timeEndPeriod(1);
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    // 停止所有模块
    CaptureModule::Stop();
    DetectionModule::Stop();
    PredictionModule::Stop();
    ActionModule::Stop();

    // 清理资源
    CaptureModule::Cleanup();
    DetectionModule::Cleanup();
    PredictionModule::Cleanup();
    ActionModule::Cleanup();

    // 等待线程结束
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

    // 释放资源
    cv::destroyAllWindows();

    std::cout << "Program terminated normally" << std::endl;
    return 0;
}

// 测试鼠标移动功能
void TestMouseMovement(int targetX, int targetY, int humanizationFactor = 50) {
    std::cout << "开始鼠标移动测试..." << std::endl;
    std::cout << "目标位置: (" << targetX << ", " << targetY << ")" << std::endl;
    std::cout << "拟人化因子: " << humanizationFactor << std::endl;

    // 创建鼠标控制器
    std::unique_ptr<MouseController> mouseController = std::make_unique<KmboxNetMouseController>();

    mouseController->MoveTo(targetX, targetY);
}

// 测试鼠标监听功能
void TestMouseMonitoring() {
    std::cout << "开始鼠标监听测试..." << std::endl;
    std::cout << "请分别按下鼠标侧键1和侧键2进行测试" << std::endl;
    std::cout << "测试将在检测到两个侧键都被按下后结束" << std::endl;

    // 创建鼠标控制器
    std::unique_ptr<KmboxNetMouseController> mouseController = std::make_unique<KmboxNetMouseController>();

    // 启动监听
    if (!mouseController->StartMonitor(1000)) {
        std::cerr << "启动鼠标监听失败，退出测试" << std::endl;
        return;
    }

    bool side1Pressed = false;
    bool side2Pressed = false;

    // 监听循环
    while (!(side1Pressed && side2Pressed)) {
        // 检测侧键1
        if (mouseController->IsSideButton1Down() && !side1Pressed) {
            std::cout << "检测到鼠标侧键1按下" << std::endl;
            side1Pressed = true;
            // 等待松开
            while (mouseController->IsSideButton1Down()) {
                Sleep(1);
            }
            std::cout << "检测到鼠标侧键1松开" << std::endl;
        }

        // 检测侧键2
        if (mouseController->IsSideButton2Down() && !side2Pressed) {
            std::cout << "检测到鼠标侧键2按下" << std::endl;
            side2Pressed = true;
            // 等待松开
            while (mouseController->IsSideButton2Down()) {
                Sleep(1);
            }
            std::cout << "检测到鼠标侧键2松开" << std::endl;
        }

        // 避免CPU占用过高
        Sleep(1);
    }

    // 停止监听
    mouseController->StopMonitor();
    std::cout << "鼠标监听测试完成" << std::endl;
}

int main1() {
    // 设置高优先级和资源分配
    SetHighPriorityAndResources();

    // 测试选项
    int testOption;
    std::cout << "请选择测试选项：" << std::endl;
    std::cout << "1. 测试鼠标移动" << std::endl;
    std::cout << "2. 测试鼠标监听" << std::endl;
    std::cin >> testOption;

    if (testOption == 1) {
        int targetX, targetY, humanFactor;

        std::cout << "输入目标X坐标: ";
        std::cin >> targetX;

        std::cout << "输入目标Y坐标: ";
        std::cin >> targetY;

        std::cout << "输入拟人化因子(1-100): ";
        std::cin >> humanFactor;
        humanFactor = std::max(1, std::min(100, humanFactor));

        // 执行鼠标移动测试
        TestMouseMovement(targetX, targetY, humanFactor);
    }
    else if (testOption == 2) {
        // 执行鼠标监听测试
        TestMouseMonitoring();
    }
    else {
        std::cout << "无效选项" << std::endl;
    }

    return 0;
}