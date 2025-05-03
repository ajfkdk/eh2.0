#ifndef WINDOWS_MOUSE_CONTROLLER_H
#define WINDOWS_MOUSE_CONTROLLER_H

#include "MouseController.h"
#include <Windows.h>

class WindowsMouseController : public MouseController {
public:
    // �ƶ���굽ָ��λ��
    void MoveTo(int x, int y) override;

    // ��ȡ��ǰ���λ��
    void GetCurrentPosition(int& x, int& y) override;

    // ���������
    void LeftClick() override;

    // ����������
    void LeftDown() override;

    // �ͷ�������
    void LeftUp() override;

    // �������Ҽ�
    void RightClick() override;

    // �ƶ���꣨��Ե�ǰλ�ã�
    void MoveRelative(int deltaX, int deltaY) override;
};

#endif // WINDOWS_MOUSE_CONTROLLER_H#pragma once
