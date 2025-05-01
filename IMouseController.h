#ifndef I_MOUSE_CONTROLLER_H
#define I_MOUSE_CONTROLLER_H

#include <string>

namespace ActionModule {

    class IMouseController {
    public:
        virtual ~IMouseController() = default;

        // �ƶ���굽ָ��λ��
        virtual void MoveTo(int x, int y) = 0;

        // ���������
        virtual void LeftClick() = 0;

        // ����������
        virtual void LeftDown() = 0;

        // �ͷ�������
        virtual void LeftUp() = 0;

        // ��ȡ����������
        virtual std::string GetName() const = 0;

        // ��ʼ��������
        virtual bool Initialize() = 0;

        // ������Դ
        virtual void Cleanup() = 0;
    };

} // namespace ActionModule

#endif // I_MOUSE_CONTROLLER_H