#ifndef LOGITECH_CONTROLLER_H
#define LOGITECH_CONTROLLER_H

#include "IMouseController.h"

namespace ActionModule {

    class LogitechController : public IMouseController {
    public:
        LogitechController();

        // IMouseController�ӿ�ʵ��
        void MoveTo(int x, int y) override;
        void LeftClick() override;
        void LeftDown() override;
        void LeftUp() override;
        std::string GetName() const override;
        bool Initialize() override;
        void Cleanup() override;

    private:
        // ����Ƿ��ʼ���ɹ�
        bool initialized;

        // ���Լ����޼�����
        bool LoadLogitechDriver();
    };

} // namespace ActionModule

#endif // LOGITECH_CONTROLLER_H