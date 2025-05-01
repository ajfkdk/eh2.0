#ifndef WINDOWS_API_CONTROLLER_H
#define WINDOWS_API_CONTROLLER_H

#include "IMouseController.h"
#include <windows.h>

namespace ActionModule {

    class WindowsAPIController : public IMouseController {
    public:
        WindowsAPIController();

        // IMouseController接口实现
        void MoveTo(int x, int y) override;
        void LeftClick() override;
        void LeftDown() override;
        void LeftUp() override;
        std::string GetName() const override;
        bool Initialize() override;
        void Cleanup() override;

    private:
        // 屏幕信息
        int screenWidth;
        int screenHeight;

        // 获取屏幕尺寸
        void UpdateScreenDimensions();
    };

} // namespace ActionModule

#endif // WINDOWS_API_CONTROLLER_H