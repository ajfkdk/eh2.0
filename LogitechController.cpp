#include "LogitechController.h"
#include <windows.h>
#include <iostream>

namespace ActionModule {

    // �����޼������ĺ���ǩ��
    typedef bool (*LogiLedInitializeType)();
    typedef bool (*LogiLedMoveMouseType)(int x, int y);
    typedef bool (*LogiLedMouseButtonType)(int button, bool down);
    typedef void (*LogiLedShutdownType)();

    // ��̬�������溯��ָ��
    static HMODULE hLogiDll = nullptr;
    static LogiLedInitializeType pLogiLedInitialize = nullptr;
    static LogiLedMoveMouseType pLogiLedMoveMouse = nullptr;
    static LogiLedMouseButtonType pLogiLedMouseButton = nullptr;
    static LogiLedShutdownType pLogiLedShutdown = nullptr;

    LogitechController::LogitechController() : initialized(false) {
    }

    bool LogitechController::LoadLogitechDriver() {
        // ���Լ����޼�����DLL
        hLogiDll = LoadLibraryA("LogitechGAPI.dll"); // ʵ��DLL���ƿ��ܲ�ͬ

        if (!hLogiDll) {
            std::cerr << "Failed to load Logitech driver DLL" << std::endl;
            return false;
        }

        // ��ȡ����ָ��
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

        // ��ʼ���޼�����
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
            // ����޷�ʹ���޼���������ʹ��Windows API
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dx = x * (65535 / GetSystemMetrics(SM_CXSCREEN));
            input.mi.dy = y * (65535 / GetSystemMetrics(SM_CYSCREEN));
            input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
            SendInput(1, &input, sizeof(INPUT));
            return;
        }

        // ʹ���޼������ƶ����
        pLogiLedMoveMouse(x, y);
    }

    void LogitechController::LeftClick() {
        LeftDown();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        LeftUp();
    }

    void LogitechController::LeftDown() {
        if (!initialized) {
            // ����޷�ʹ���޼���������ʹ��Windows API
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &input, sizeof(INPUT));
            return;
        }

        // ʹ���޼������������
        pLogiLedMouseButton(0, true); // ����0�����
    }

    void LogitechController::LeftUp() {
        if (!initialized) {
            // ����޷�ʹ���޼���������ʹ��Windows API
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &input, sizeof(INPUT));
            return;
        }

        // ʹ���޼������ͷ����
        pLogiLedMouseButton(0, false); // ����0�����
    }

    std::string LogitechController::GetName() const {
        return "Logitech";
    }

    bool LogitechController::Initialize() {
        initialized = LoadLogitechDriver();
        return true; // ��ʹ��ʼ��ʧ��Ҳ����true����Ϊ����˵�Windows API
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