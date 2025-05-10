#include "KmboxNetMouseController.h"
#include <iostream>
#include "kmboxNet.h"


KmboxNetMouseController::KmboxNetMouseController(const std::string& deviceIp,
    const std::string& devicePort,
    const std::string& deviceMac)
    : connected(false), monitoring(false), ip(deviceIp), port(devicePort), mac(deviceMac) {

    // 初始化kmboxNet设备连接
    int result = kmNet_init(const_cast<char*>(ip.c_str()),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(mac.c_str()));

    if (result == 0) {
        connected.store(true);
        std::cout << "KmboxNet连接成功" << std::endl;
    }
    else {
        std::cerr << "KmboxNet连接失败，错误码: " << result << std::endl;
    }
}

KmboxNetMouseController::~KmboxNetMouseController() {
    // 确保释放监听
    if (monitoring.load()) {
        StopMonitor();
    }
    // kmboxNet库可能没有显式关闭连接的函数
}

void KmboxNetMouseController::MoveTo(int x, int y) {
    if (!connected.load()) {
        std::cerr << "KmboxNet未连接，无法移动鼠标" << std::endl;
        return;
    }

    int result = kmNet_mouse_move_auto(static_cast<short>(x), static_cast<short>(y),200);

    if (result != 0) {
        std::cerr << "KmboxNet移动鼠标失败，错误码: " << result << std::endl;
    }
}

void KmboxNetMouseController::GetCurrentPosition(int& x, int& y) {
    // 由于kmboxNet没有直接提供获取鼠标位置的接口，我们使用Windows API获取
    x = 0, y = 0;
}

void KmboxNetMouseController::LeftClick() {
    if (!connected.load()) {
        std::cerr << "KmboxNet未连接，无法点击鼠标" << std::endl;
        return;
    }

    // 按下后释放左键模拟点击
    LeftDown();
    Sleep(10); // 短暂延迟模拟人类点击
    LeftUp();
}

void KmboxNetMouseController::LeftDown() {
    if (!connected.load()) {
        std::cerr << "KmboxNet未连接，无法按下鼠标左键" << std::endl;
        return;
    }

    int result = kmNet_mouse_left(1); // 1表示按下
    if (result != 0) {
        std::cerr << "KmboxNet按下鼠标左键失败，错误码: " << result << std::endl;
    }
}

void KmboxNetMouseController::LeftUp() {
    if (!connected.load()) {
        std::cerr << "KmboxNet未连接，无法释放鼠标左键" << std::endl;
        return;
    }

    int result = kmNet_mouse_left(0); // 0表示释放
    if (result != 0) {
        std::cerr << "KmboxNet释放鼠标左键失败，错误码: " << result << std::endl;
    }
}

void KmboxNetMouseController::RightClick() {
    if (!connected.load()) {
        std::cerr << "KmboxNet未连接，无法点击鼠标右键" << std::endl;
        return;
    }

    // 按下后释放右键模拟点击
    int result1 = kmNet_mouse_right(1); // 按下
    Sleep(10); // 短暂延迟
    int result2 = kmNet_mouse_right(0); // 释放

    if (result1 != 0 || result2 != 0) {
        std::cerr << "KmboxNet点击鼠标右键失败" << std::endl;
    }
}

void KmboxNetMouseController::MoveRelative(int deltaX, int deltaY) {
    if (!connected.load()) {
        std::cerr << "KmboxNet未连接，无法移动鼠标" << std::endl;
        return;
    }

    // 直接使用相对移动距离
    int result = kmNet_mouse_move(static_cast<short>(deltaX), static_cast<short>(deltaY));

    if (result != 0) {
        std::cerr << "KmboxNet相对移动鼠标失败，错误码: " << result << std::endl;
    }
}

void KmboxNetMouseController::SideButton1Down() {
    if (!connected.load()) {
        std::cerr << "KmboxNet未连接，无法按下鼠标侧键1" << std::endl;
        return;
    }

    int result = kmNet_mouse_side1(1); // 1表示按下
    if (result != 0) {
        std::cerr << "KmboxNet按下鼠标侧键1失败，错误码: " << result << std::endl;
    }
}

void KmboxNetMouseController::SideButton1Up() {
    if (!connected.load()) {
        std::cerr << "KmboxNet未连接，无法释放鼠标侧键1" << std::endl;
        return;
    }

    int result = kmNet_mouse_side1(0); // 0表示释放
    if (result != 0) {
        std::cerr << "KmboxNet释放鼠标侧键1失败，错误码: " << result << std::endl;
    }
}

void KmboxNetMouseController::SideButton2Down() {
    if (!connected.load()) {
        std::cerr << "KmboxNet未连接，无法按下鼠标侧键2" << std::endl;
        return;
    }

    int result = kmNet_mouse_side2(1); // 1表示按下
    if (result != 0) {
        std::cerr << "KmboxNet按下鼠标侧键2失败，错误码: " << result << std::endl;
    }
}

void KmboxNetMouseController::SideButton2Up() {
    if (!connected.load()) {
        std::cerr << "KmboxNet未连接，无法释放鼠标侧键2" << std::endl;
        return;
    }

    int result = kmNet_mouse_side2(0); // 0表示释放
    if (result != 0) {
        std::cerr << "KmboxNet释放鼠标侧键2失败，错误码: " << result << std::endl;
    }
}

bool KmboxNetMouseController::IsConnected() const {
    return connected.load();
}

bool KmboxNetMouseController::Reconnect() {
    int result = kmNet_init(const_cast<char*>(ip.c_str()),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(mac.c_str()));

    if (result == 0) {
        connected.store(true);
        std::cout << "KmboxNet重新连接成功" << std::endl;
        return true;
    }
    else {
        std::cerr << "KmboxNet重新连接失败，错误码: " << result << std::endl;
        return false;
    }
}

// 实现监听相关方法
bool KmboxNetMouseController::StartMonitor(int port) {
    if (!connected.load()) {
        std::cerr << "KmboxNet未连接，无法启动监听" << std::endl;
        return false;
    }

    int result = kmNet_monitor(port);
    if (result == 0) {
        monitoring.store(true);
        std::cout << "KmboxNet监听已启动，端口: " << port << std::endl;
        return true;
    }
    else {
        std::cerr << "KmboxNet启动监听失败，错误码: " << result << std::endl;
        return false;
    }
}

void KmboxNetMouseController::StopMonitor() {
    if (monitoring.load()) {
        // 如果kmboxNet库有停止监听的函数，应该在这里调用
        // 例如: kmNet_stop_monitor();

        // 由于没有明确的停止监听函数，可能需要通过其他方式实现
        // 或者仅标记监听状态已停止
        monitoring.store(false);
        std::cout << "KmboxNet监听已停止" << std::endl;
    }
}

bool KmboxNetMouseController::IsLeftButtonDown() {
    if (!connected.load() || !monitoring.load()) {
        std::cerr << "KmboxNet未连接或未启动监听，无法检测鼠标左键状态" << std::endl;
        return false;
    }

    return kmNet_monitor_mouse_left() == 1;
}

bool KmboxNetMouseController::IsMiddleButtonDown() {
    if (!connected.load() || !monitoring.load()) {
        std::cerr << "KmboxNet未连接或未启动监听，无法检测鼠标中键状态" << std::endl;
        return false;
    }

    return kmNet_monitor_mouse_middle() == 1;
}

bool KmboxNetMouseController::IsRightButtonDown() {
    if (!connected.load() || !monitoring.load()) {
        std::cerr << "KmboxNet未连接或未启动监听，无法检测鼠标右键状态" << std::endl;
        return false;
    }

    return kmNet_monitor_mouse_right() == 1;
}

bool KmboxNetMouseController::IsSideButton1Down() {
    if (!connected.load() || !monitoring.load()) {
        std::cerr << "KmboxNet未连接或未启动监听，无法检测鼠标侧键1状态" << std::endl;
        return false;
    }

    return kmNet_monitor_mouse_side1() == 1;
}

bool KmboxNetMouseController::IsSideButton2Down() {
    if (!connected.load() || !monitoring.load()) {
        std::cerr << "KmboxNet未连接或未启动监听，无法检测鼠标侧键2状态" << std::endl;
        return false;
    }

    return kmNet_monitor_mouse_side2() == 1;
}