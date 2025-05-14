#ifndef WINDOWS_MOUSE_CONTROLLER_H
#define WINDOWS_MOUSE_CONTROLLER_H

#include "MouseController.h"
#include <Windows.h>

class WindowsMouseController : public MouseController {
public:
    // �ƶ���굽ָ��λ��
    void MoveToWithTime(int x, int y,int during) override;

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

    // ���������1
    void SideButton1Down() override;

    // �ͷ������1
    void SideButton1Up() override;

    // ���������2
    void SideButton2Down() override;

    // �ͷ������2
    void SideButton2Up() override;

    bool StartMonitor(int port = 1000) override;
    void StopMonitor() override;
    bool IsLeftButtonDown() override;
    bool IsMiddleButtonDown() override;
    bool IsRightButtonDown() override;
    bool IsSideButton1Down() override;
    bool IsSideButton2Down() override;
};

#endif // WINDOWS_MOUSE_CONTROLLER_H