#ifndef I_MOUSE_CONTROLLER_H
#define I_MOUSE_CONTROLLER_H

#include <string>

namespace ActionModule {

    class IMouseController {
    public:
        virtual ~IMouseController() = default;

        // 移动鼠标到指定位置
        virtual void MoveTo(int x, int y) = 0;

        // 点击鼠标左键
        virtual void LeftClick() = 0;

        // 按下鼠标左键
        virtual void LeftDown() = 0;

        // 释放鼠标左键
        virtual void LeftUp() = 0;

        // 获取控制器名称
        virtual std::string GetName() const = 0;

        // 初始化控制器
        virtual bool Initialize() = 0;

        // 清理资源
        virtual void Cleanup() = 0;
    };

} // namespace ActionModule

#endif // I_MOUSE_CONTROLLER_H