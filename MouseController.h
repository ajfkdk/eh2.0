#ifndef MOUSE_CONTROLLER_H
#define MOUSE_CONTROLLER_H

// 鼠标控制器接口
class MouseController {
public:
    virtual ~MouseController() = default;

    // 移动鼠标到指定位置
    virtual void MoveTo(int x, int y) = 0;

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
};

#endif // MOUSE_CONTROLLER_H