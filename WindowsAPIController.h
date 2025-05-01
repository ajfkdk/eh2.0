#ifndef WINDOWS_API_CONTROLLER_H
#define WINDOWS_API_CONTROLLER_H

// ����NOMINMAX��ֹwindows.h����min��max��
#define NOMINMAX
#include <windows.h>
#include "IMouseController.h"

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
        // ��Ļ����
        int screenWidth;
        int screenHeight;

        // ��ȡ��Ļ�ߴ�
        void UpdateScreenDimensions();
    };

} // namespace ActionModule

#endif // WINDOWS_API_CONTROLLER_H