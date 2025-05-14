#ifndef MOUSE_CONTROLLER_H
#define MOUSE_CONTROLLER_H

// 鼠标控制器接口
class MouseController {
public:
    virtual ~MouseController() = default;

    // 移动鼠标到指定位置
    virtual void MoveToWithTime(int x, int y,int during) = 0;

    // 获取当前鼠标位置
    virtual void GetCurrentPosition(int& x, int& y) = 0;

    // 点击鼠标左键
    virtual void LeftClick() = 0;

    // 按下鼠标左键
    virtual void LeftDown() = 0;

    // 释放鼠标左键
    virtual void LeftUp() = 0;

    // 点击鼠标右键
    virtual void RightClick() = 0;

    // 移动鼠标（相对当前位置）
    virtual void MoveRelative(int deltaX, int deltaY) = 0;

    // 按下鼠标侧键1
    virtual void SideButton1Down() = 0;

    // 释放鼠标侧键1
    virtual void SideButton1Up() = 0;

    // 按下鼠标侧键2
    virtual void SideButton2Down() = 0;

    // 释放鼠标侧键2
    virtual void SideButton2Up() = 0;

    // 监听相关方法
    // 启动鼠标监听
    virtual bool StartMonitor(int port = 1000) = 0;

    // 停止鼠标监听
    virtual void StopMonitor() = 0;

    // 检查鼠标左键是否按下
    virtual bool IsLeftButtonDown() = 0;

    // 检查鼠标中键是否按下
    virtual bool IsMiddleButtonDown() = 0;

    // 检查鼠标右键是否按下
    virtual bool IsRightButtonDown() = 0;

    // 检查鼠标侧键1是否按下
    virtual bool IsSideButton1Down() = 0;

    // 检查鼠标侧键2是否按下
    virtual bool IsSideButton2Down() = 0;

    // 检查鼠标是否移动
    virtual bool IsMouseMoving() = 0;
};

#endif // MOUSE_CONTROLLER_H