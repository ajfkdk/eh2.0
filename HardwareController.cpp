#include "HardwareController.h"
#include <iostream>
#include <windows.h>
#include <thread>

namespace ActionModule {

    HardwareController::HardwareController() : connected(false), deviceHandle(nullptr) {
    }

    bool HardwareController::ConnectToDevice() {
        // 这里应该是连接到硬件设备的代码
        // 由于是示例，我们只是模拟连接过程
        std::cout << "Attempting to connect to hardware mouse controller..." << std::endl;

        // 模拟连接延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // 假设无法连接到硬件，实际应用中应该有真正的连接代码
        return false;
    }

    bool HardwareController::SendMoveCommand(int x, int y) {
        if (!connected || !deviceHandle) {
            return false;
        }

        // 这里应该是向硬件发送移动命令的代码
        // 由于是示例，我们只是返回成功
        return true;
    }

    bool HardwareController::SendButtonCommand(bool isDown) {
        if (!connected || !deviceHandle) {
            return false;
        }

        // 这里应该是向硬件发送按键命令的代码
        // 由于是示例，我们只是返回成功
        return true;
    }

    void HardwareController::MoveTo(int x, int y) {
        if (!connected) {
            // 如果未连接到硬件，则使用Windows API
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dx = x * (65535 / GetSystemMetrics(SM_CXSCREEN));
            input.mi.dy = y * (65535 / GetSystemMetrics(SM_CYSCREEN));
            input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
            SendInput(1, &input, sizeof(INPUT));
            return;
        }

        // 使用硬件移动鼠标
        SendMoveCommand(x, y);
    }

    void HardwareController::LeftClick() {
        LeftDown();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        LeftUp();
    }

    void HardwareController::LeftDown() {
        if (!connected) {
            // 如果未连接到硬件，则使用Windows API
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &input, sizeof(INPUT));
            return;
        }

        // 使用硬件按下左键
        SendButtonCommand(true);
    }

    void HardwareController::LeftUp() {
        if (!connected) {
            // 如果未连接到硬件，则使用Windows API
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &input, sizeof(INPUT));
            return;
        }

        // 使用硬件释放左键
        SendButtonCommand(false);
    }

    std::string HardwareController::GetName() const {
        return "Hardware";
    }

    bool HardwareController::Initialize() {
        connected = ConnectToDevice();
        return true; // 即使连接失败也返回true，因为会回退到Windows API
    }

    void HardwareController::Cleanup() {
        if (connected && deviceHandle) {
            // 关闭与硬件的连接
            deviceHandle = nullptr;
        }

        connected = false;
    }

} // namespace ActionModule