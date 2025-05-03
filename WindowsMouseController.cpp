#include "WindowsMouseController.h"
#include <Windows.h>

void WindowsMouseController::MoveTo(int x, int y) {
    // �������ľ���λ��
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
    // ģ�����������
    INPUT input[2] = {};

    // ����Ϊ�������
    input[0].type = INPUT_MOUSE;
    input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    input[1].type = INPUT_MOUSE;
    input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    // ��������
    SendInput(2, input, sizeof(INPUT));
}

void WindowsMouseController::LeftDown() {
    // ģ������������
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &input, sizeof(INPUT));
}

void WindowsMouseController::LeftUp() {
    // ģ���������ͷ�
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
}

void WindowsMouseController::RightClick() {
    // ģ������Ҽ����
    INPUT input[2] = {};

    // ����Ϊ�������
    input[0].type = INPUT_MOUSE;
    input[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;

    input[1].type = INPUT_MOUSE;
    input[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;

    // ��������
    SendInput(2, input, sizeof(INPUT));
}

void WindowsMouseController::MoveRelative(int deltaX, int deltaY) {
    // ģ���������ƶ�
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = deltaX;
    input.mi.dy = deltaY;
    SendInput(1, &input, sizeof(INPUT));
}