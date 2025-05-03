#ifndef WINDOWS_MOUSE_CONTROLLER_H
#define WINDOWS_MOUSE_CONTROLLER_H

#include "MouseController.h"
#include <Windows.h>

class WindowsMouseController : public MouseController {
public:
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
};

#endif // WINDOWS_MOUSE_CONTROLLER_H#pragma once
