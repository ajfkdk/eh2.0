#ifndef MOUSE_CONTROLLER_H
#define MOUSE_CONTROLLER_H

// ���������ӿ�
class MouseController {
public:
    virtual ~MouseController() = default;

    // �ƶ���굽ָ��λ��
    virtual void MoveTo(int x, int y) = 0;

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
};

#endif // MOUSE_CONTROLLER_H