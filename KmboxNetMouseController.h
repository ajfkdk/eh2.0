#ifndef KMBOXNET_MOUSE_CONTROLLER_H
#define KMBOXNET_MOUSE_CONTROLLER_H

#include "MouseController.h"
#include <atomic>
#include <string>
#include <chrono>

class KmboxNetMouseController : public MouseController {
private:
    // 连接状态
    std::atomic<bool> connected;

    // 监听状态
    std::atomic<bool> monitoring;

    // 设备信息
    std::string ip;
    std::string port;
    std::string mac;

    // 上次鼠标位置
    int lastMouseX;
    int lastMouseY;

    // 鼠标移动检测
    std::atomic<bool> mouseMoving;
    std::chrono::steady_clock::time_point lastMoveTime;
    static constexpr int MOVE_THRESHOLD_MS = 20; // 鼠标静止阈值（毫秒）

public:
    // 构造函数，初始化kmboxNet设备连接
    KmboxNetMouseController(const std::string& deviceIp = "192.168.2.188",
        const std::string& devicePort = "8551",
        const std::string& deviceMac = "6567E04E");

    // 析构函数，清理资源
    ~KmboxNetMouseController() override;

    // 移动鼠标到指定位置
    void MoveTo(int x, int y) override;

    // 获取当前鼠标位置
    void GetCurrentPosition(int& x, int& y) override;

    // 点击鼠标左键
    void LeftClick() override;

    // 按下鼠标左键
    void LeftDown() override;

    // 释放鼠标左键
    void LeftUp() override;

    // 点击鼠标右键
    void RightClick() override;

    // 移动鼠标（相对当前位置）
    void MoveRelative(int deltaX, int deltaY) override;

    // 按下鼠标侧键1
    void SideButton1Down() override;

    // 释放鼠标侧键1
    void SideButton1Up() override;

    // 按下鼠标侧键2
    void SideButton2Down() override;

    // 释放鼠标侧键2
    void SideButton2Up() override;

    // 监听相关方法实现
    // 启动鼠标监听
    bool StartMonitor(int port = 1000) override;

    // 停止鼠标监听
    void StopMonitor() override;

    // 检查鼠标左键是否按下
    bool IsLeftButtonDown() override;

    // 检查鼠标中键是否按下
    bool IsMiddleButtonDown() override;

    // 检查鼠标右键是否按下
    bool IsRightButtonDown() override;

    // 检查鼠标侧键1是否按下
    bool IsSideButton1Down() override;

    // 检查鼠标侧键2是否按下
    bool IsSideButton2Down() override;

    // 检查鼠标是否移动
    bool IsMouseMoving() override;

    // 更新鼠标移动状态
    void UpdateMouseMovingState();

    // 检查连接状态
    bool IsConnected() const;

    // 重新连接设备
    bool Reconnect();
};

#endif // KMBOXNET_MOUSE_CONTROLLER_H