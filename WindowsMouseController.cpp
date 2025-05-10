#include "WindowsMouseController.h"
#include <Windows.h>

void WindowsMouseController::MoveTo(int x, int y) {
    // 设置鼠标的绝对位置
    SetCursorPos(x, y);
}

void WindowsMouseController::GetCurrentPosition(int& x, int& y) {
    POINT cursorPos;
    if (GetCursorPos(&cursorPos)) {
        x = cursorPos.x;
        y = cursorPos.y;
    }
}

void WindowsMouseController::LeftClick() {
    // 模拟鼠标左键点击
    INPUT input[2] = {};

    // 设置为鼠标输入
    input[0].type = INPUT_MOUSE;
    input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    input[1].type = INPUT_MOUSE;
    input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    // 发送输入
    SendInput(2, input, sizeof(INPUT));
}

void WindowsMouseController::LeftDown() {
    // 模拟鼠标左键按下
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &input, sizeof(INPUT));
}

void WindowsMouseController::LeftUp() {
    // 模拟鼠标左键释放
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
}

void WindowsMouseController::RightClick() {
    // 模拟鼠标右键点击
    INPUT input[2] = {};

    // 设置为鼠标输入
    input[0].type = INPUT_MOUSE;
    input[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;

    input[1].type = INPUT_MOUSE;
    input[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;

    // 发送输入
    SendInput(2, input, sizeof(INPUT));
}

void WindowsMouseController::MoveRelative(int deltaX, int deltaY) {
    // 模拟鼠标相对移动
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = deltaX;
    input.mi.dy = deltaY;
    SendInput(1, &input, sizeof(INPUT));
}

void WindowsMouseController::SideButton1Down() {
    // 模拟鼠标侧键1按下 (XBUTTON1)
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_XDOWN;
    input.mi.mouseData = XBUTTON1;
    SendInput(1, &input, sizeof(INPUT));
}

void WindowsMouseController::SideButton1Up() {
    // 模拟鼠标侧键1释放 (XBUTTON1)
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_XUP;
    input.mi.mouseData = XBUTTON1;
    SendInput(1, &input, sizeof(INPUT));
}

void WindowsMouseController::SideButton2Down() {
    // 模拟鼠标侧键2按下 (XBUTTON2)
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_XDOWN;
    input.mi.mouseData = XBUTTON2;
    SendInput(1, &input, sizeof(INPUT));
}

void WindowsMouseController::SideButton2Up() {
    // 模拟鼠标侧键2释放 (XBUTTON2)
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_XUP;
    input.mi.mouseData = XBUTTON2;
    SendInput(1, &input, sizeof(INPUT));
}