#pragma once
#ifndef HARDWARE_CONTROLLER_H
#define HARDWARE_CONTROLLER_H

#include "IMouseController.h"
#include <string>

namespace ActionModule {

    class HardwareController : public IMouseController {
    public:
        HardwareController();

        // IMouseController�ӿ�ʵ��
        void MoveTo(int x, int y) override;
        void LeftClick() override;
        void LeftDown() override;
        void LeftUp() override;
        std::string GetName() const override;
        bool Initialize() override;
        void Cleanup() override;

    private:
        // �Ƿ������ӵ�Ӳ���豸
        bool connected;

        // Ӳ���豸���
        void* deviceHandle;

        // ���ӵ�Ӳ���豸
        bool ConnectToDevice();

        // ���豸�����ƶ�����
        bool SendMoveCommand(int x, int y);

        // ���豸���Ͱ�������
        bool SendButtonCommand(bool isDown);
    };

} // namespace ActionModule

#endif // HARDWARE_CONTROLLER_H