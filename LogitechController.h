#ifndef LOGITECH_CONTROLLER_H
#define LOGITECH_CONTROLLER_H

#include "IMouseController.h"

namespace ActionModule {

    class LogitechController : public IMouseController {
    public:
        LogitechController();

        // IMouseController接口实现
        void MoveTo(int x, int y) override;
        void LeftClick() override;
        void LeftDown() override;
        void LeftUp() override;
        std::string GetName() const override;
        bool Initialize() override;
        void Cleanup() override;

    private:
        // 标记是否初始化成功
        bool initialized;

        // 尝试加载罗技驱动
        bool LoadLogitechDriver();
    };

} // namespace ActionModule

#endif // LOGITECH_CONTROLLER_H