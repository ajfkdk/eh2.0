#include "LogitechController.h"
#include <windows.h>
#include <iostream>

namespace ActionModule {

    // 定义罗技驱动的函数签名
    typedef bool (*LogiLedInitializeType)();
    typedef bool (*LogiLedMoveMouseType)(int x, int y);
    typedef bool (*LogiLedMouseButtonType)(int button, bool down);
    typedef void (*LogiLedShutdownType)();

    // 静态变量保存函数指针
    static HMODULE hLogiDll = nullptr;
    static LogiLedInitializeType pLogiLedInitialize = nullptr;
    static LogiLedMoveMouseType pLogiLedMoveMouse = nullptr;
    static LogiLedMouseButtonType pLogiLedMouseButton = nullptr;
    static LogiLedShutdownType pLogiLedShutdown = nullptr;

    LogitechController::LogitechController() : initialized(false) {
    }

    bool LogitechController::LoadLogitechDriver() {
        // 尝试加载罗技驱动DLL
        hLogiDll = LoadLibraryA("LogitechGAPI.dll"); // 实际DLL名称可能不同

        if (!hLogiDll) {
            std::cerr << "Failed to load Logitech driver DLL" << std::endl;
            return false;
        }

        // 获取函数指针
        pLogiLedInitialize = (LogiLedInitializeType)GetProcAddress(hLogiDll, "LogiLedInitialize");
        pLogiLedMoveMouse = (LogiLedMoveMouseType)GetProcAddress(hLogiDll, "LogiLedMoveMouse");
        pLogiLedMouseButton = (LogiLedMouseButtonType)GetProcAddress(hLogiDll, "LogiLedMouseButton");
        pLogiLedShutdown = (LogiLedShutdownType)GetProcAddress(hLogiDll, "LogiLedShutdown");

        if (!pLogiLedInitialize || !pLogiLedMoveMouse ||
            !pLogiLedMouseButton || !pLogiLedShutdown) {
            std::cerr << "Failed to get function pointers from Logitech driver DLL" << std::endl;
            FreeLibrary(hLogiDll);
            hLogiDll = nullptr;
            return false;
        }

        // 初始化罗技驱动
        if (!pLogiLedInitialize()) {
            std::cerr << "Failed to initialize Logitech driver" << std::endl;
            FreeLibrary(hLogiDll);
            hLogiDll = nullptr;
            return false;
        }

        return true;
    }

    void LogitechController::MoveTo(int x, int y) {
        if (!initialized) {
            // 如果无法使用罗技驱动，则使用Windows API
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dx = x * (65535 / GetSystemMetrics(SM_CXSCREEN));
            input.mi.dy = y * (65535 / GetSystemMetrics(SM_CYSCREEN));
            input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
            SendInput(1, &input, sizeof(INPUT));
            return;
        }

        // 使用罗技驱动移动鼠标
        pLogiLedMoveMouse(x, y);
    }

    void LogitechController::LeftClick() {
        LeftDown();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        LeftUp();
    }

    void LogitechController::LeftDown() {
        if (!initialized) {
            // 如果无法使用罗技驱动，则使用Windows API
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &input, sizeof(INPUT));
            return;
        }

        // 使用罗技驱动按下左键
        pLogiLedMouseButton(0, true); // 假设0是左键
    }

    void LogitechController::LeftUp() {
        if (!initialized) {
            // 如果无法使用罗技驱动，则使用Windows API
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &input, sizeof(INPUT));
            return;
        }

        // 使用罗技驱动释放左键
        pLogiLedMouseButton(0, false); // 假设0是左键
    }

    std::string LogitechController::GetName() const {
        return "Logitech";
    }

    bool LogitechController::Initialize() {
        initialized = LoadLogitechDriver();
        return true; // 即使初始化失败也返回true，因为会回退到Windows API
    }

    void LogitechController::Cleanup() {
        if (initialized && pLogiLedShutdown) {
            pLogiLedShutdown();
        }

        if (hLogiDll) {
            FreeLibrary(hLogiDll);
            hLogiDll = nullptr;
        }

        initialized = false;
    }

} // namespace ActionModule