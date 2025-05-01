#include "WindowsAPIController.h"
#include <thread>
#include <algorithm> 

namespace ActionModule {

    // WindowsAPIController类实现
    WindowsAPIController::WindowsAPIController() : screenWidth(0), screenHeight(0) {
    }

    // 移动鼠标到指定坐标
    void WindowsAPIController::MoveTo(int x, int y) {
        // 确保坐标在屏幕范围内
        x = std::max(0, std::min(x, screenWidth - 1));
        y = std::max(0, std::min(y, screenHeight - 1));

        // 将坐标转换为INPUT结构
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dx = static_cast<LONG>(x * (65535.0f / screenWidth));
        input.mi.dy = static_cast<LONG>(y * (65535.0f / screenHeight));
        input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

        // 发送鼠标移动事件
        SendInput(1, &input, sizeof(INPUT));
    }

    void WindowsAPIController::LeftClick() {
        // 按下并释放左键
        LeftDown();
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 短暂延迟
        LeftUp();
    }

    void WindowsAPIController::LeftDown() {
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &input, sizeof(INPUT));
    }

    void WindowsAPIController::LeftUp() {
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));
    }

    std::string WindowsAPIController::GetName() const {
        return "WindowsAPI";
    }

    bool WindowsAPIController::Initialize() {
        UpdateScreenDimensions();
        return true;
    }

    void WindowsAPIController::Cleanup() {
        // 没有需要清理的资源
    }

    void WindowsAPIController::UpdateScreenDimensions() {
        screenWidth = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);
    }

} // namespace ActionModule