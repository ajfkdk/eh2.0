#ifndef KMBOXNET_MOUSE_CONTROLLER_H
#define KMBOXNET_MOUSE_CONTROLLER_H

#include "MouseController.h"
#include <atomic>
#include <string>
#include <chrono>

class KmboxNetMouseController : public MouseController {
private:
    // ����״̬
    std::atomic<bool> connected;

    // ����״̬
    std::atomic<bool> monitoring;

    // �豸��Ϣ
    std::string ip;
    std::string port;
    std::string mac;

    // �ϴ����λ��
    int lastMouseX;
    int lastMouseY;

    // ����ƶ����
    std::atomic<bool> mouseMoving;
    std::chrono::steady_clock::time_point lastMoveTime;
    static constexpr int MOVE_THRESHOLD_MS = 20; // ��꾲ֹ��ֵ�����룩

public:
    // ���캯������ʼ��kmboxNet�豸����
    KmboxNetMouseController(const std::string& deviceIp = "192.168.2.188",
        const std::string& devicePort = "8551",
        const std::string& deviceMac = "6567E04E");

    // ����������������Դ
    ~KmboxNetMouseController() override;

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

    // ���������1
    void SideButton1Down() override;

    // �ͷ������1
    void SideButton1Up() override;

    // ���������2
    void SideButton2Down() override;

    // �ͷ������2
    void SideButton2Up() override;

    // ������ط���ʵ��
    // ����������
    bool StartMonitor(int port = 1000) override;

    // ֹͣ������
    void StopMonitor() override;

    // ����������Ƿ���
    bool IsLeftButtonDown() override;

    // �������м��Ƿ���
    bool IsMiddleButtonDown() override;

    // �������Ҽ��Ƿ���
    bool IsRightButtonDown() override;

    // ��������1�Ƿ���
    bool IsSideButton1Down() override;

    // ��������2�Ƿ���
    bool IsSideButton2Down() override;

    // �������Ƿ��ƶ�
    bool IsMouseMoving() override;

    // ��������ƶ�״̬
    void UpdateMouseMovingState();

    // �������״̬
    bool IsConnected() const;

    // ���������豸
    bool Reconnect();
};

#endif // KMBOXNET_MOUSE_CONTROLLER_H