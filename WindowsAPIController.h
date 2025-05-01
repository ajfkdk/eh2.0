#ifndef WINDOWS_API_CONTROLLER_H
#define WINDOWS_API_CONTROLLER_H

#include "IMouseController.h"
#include <windows.h>

namespace ActionModule {

    class WindowsAPIController : public IMouseController {
    public:
        WindowsAPIController();

        // IMouseController�ӿ�ʵ��
        void MoveTo(int x, int y) override;
        void LeftClick() override;
        void LeftDown() override;
        void LeftUp() override;
        std::string GetName() const override;
        bool Initialize() override;
        void Cleanup() override;

    private:
        // ��Ļ��Ϣ
        int screenWidth;
        int screenHeight;

        // ��ȡ��Ļ�ߴ�
        void UpdateScreenDimensions();
    };

} // namespace ActionModule

#endif // WINDOWS_API_CONTROLLER_H