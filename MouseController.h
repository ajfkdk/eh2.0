#ifndef MOUSE_CONTROLLER_H
#define MOUSE_CONTROLLER_H

// ���������ӿ�
class MouseController {
public:
    virtual ~MouseController() = default;

    // �ƶ���굽ָ��λ��
    virtual void MoveToWithTime(int x, int y,int during) = 0;

    // ��ȡ��ǰ���λ��
    virtual void GetCurrentPosition(int& x, int& y) = 0;

    // ���������
    virtual void LeftClick() = 0;

    // ����������
    virtual void LeftDown() = 0;

    // �ͷ�������
    virtual void LeftUp() = 0;

    // �������Ҽ�
    virtual void RightClick() = 0;

    // �ƶ���꣨��Ե�ǰλ�ã�
    virtual void MoveRelative(int deltaX, int deltaY) = 0;

    // ���������1
    virtual void SideButton1Down() = 0;

    // �ͷ������1
    virtual void SideButton1Up() = 0;

    // ���������2
    virtual void SideButton2Down() = 0;

    // �ͷ������2
    virtual void SideButton2Up() = 0;

    // ������ط���
    // ����������
    virtual bool StartMonitor(int port = 1000) = 0;

    // ֹͣ������
    virtual void StopMonitor() = 0;

    // ����������Ƿ���
    virtual bool IsLeftButtonDown() = 0;

    // �������м��Ƿ���
    virtual bool IsMiddleButtonDown() = 0;

    // �������Ҽ��Ƿ���
    virtual bool IsRightButtonDown() = 0;

    // ��������1�Ƿ���
    virtual bool IsSideButton1Down() = 0;

    // ��������2�Ƿ���
    virtual bool IsSideButton2Down() = 0;

    // �������Ƿ��ƶ�
    virtual bool IsMouseMoving() = 0;
};

#endif // MOUSE_CONTROLLER_H