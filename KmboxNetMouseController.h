#ifndef KMBOXNET_MOUSE_CONTROLLER_H
#define KMBOXNET_MOUSE_CONTROLLER_H

#include "MouseController.h"
#include <atomic>
#include <string>

class KmboxNetMouseController : public MouseController {
private:
    // �洢��ǰ���λ�ã���ΪkmboxNetû��ֱ���ṩ��ȡ���λ�õĹ���
    std::atomic<int> currentX;
    std::atomic<int> currentY;

    // ����״̬
    std::atomic<bool> connected;

    // �豸��Ϣ
    std::string ip;
    std::string port;
    std::string mac;

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

    // �������״̬
    bool IsConnected() const;

    // ���������豸
    bool Reconnect();
};

#endif // KMBOXNET_MOUSE_CONTROLLER_H#pragma once
