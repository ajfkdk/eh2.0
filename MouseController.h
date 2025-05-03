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
};

#endif // MOUSE_CONTROLLER_H