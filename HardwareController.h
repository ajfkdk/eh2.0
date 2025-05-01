#pragma once
#ifndef HARDWARE_CONTROLLER_H
#define HARDWARE_CONTROLLER_H

#include "IMouseController.h"
#include <string>

namespace ActionModule {

    class HardwareController : public IMouseController {
    public:
        HardwareController();

        // IMouseController接口实现
        void MoveTo(int x, int y) override;
        void LeftClick() override;
        void LeftDown() override;
        void LeftUp() override;
        std::string GetName() const override;
        bool Initialize() override;
        void Cleanup() override;

    private:
        // 是否已连接到硬件设备
        bool connected;

        // 硬件设备句柄
        void* deviceHandle;

        // 连接到硬件设备
        bool ConnectToDevice();

        // 向设备发送移动命令
        bool SendMoveCommand(int x, int y);

        // 向设备发送按键命令
        bool SendButtonCommand(bool isDown);
    };

} // namespace ActionModule

#endif // HARDWARE_CONTROLLER_H