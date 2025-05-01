#include "WindowsAPIController.h"
#include <thread>
#include <algorithm> 

namespace ActionModule {

    // WindowsAPIController��ʵ��
    WindowsAPIController::WindowsAPIController() : screenWidth(0), screenHeight(0) {
    }

    // �ƶ���굽ָ������
    void WindowsAPIController::MoveTo(int x, int y) {
        // ȷ����������Ļ��Χ��
        x = std::max(0, std::min(x, screenWidth - 1));
        y = std::max(0, std::min(y, screenHeight - 1));

        // ������ת��ΪINPUT�ṹ
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dx = static_cast<LONG>(x * (65535.0f / screenWidth));
        input.mi.dy = static_cast<LONG>(y * (65535.0f / screenHeight));
        input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

        // ��������ƶ��¼�
        SendInput(1, &input, sizeof(INPUT));
    }

    void WindowsAPIController::LeftClick() {
        // ���²��ͷ����
        LeftDown();
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // �����ӳ�
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
        // û����Ҫ�������Դ
    }

    void WindowsAPIController::UpdateScreenDimensions() {
        screenWidth = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);
    }

} // namespace ActionModule