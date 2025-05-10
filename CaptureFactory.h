#ifndef CAPTURE_FACTORY_H
#define CAPTURE_FACTORY_H

#include "IFrameCapture.h"
#include <memory>
#include <string>

// �ɼ�������ö��
enum class CaptureType {
    WINDOWS_SCREEN,
    DIRECTX,
    OPENCV_CAMERA,
    NETWORK_STREAM,  // ��������������
    // ����չ��������
};

class CaptureFactory {
public:
    // �������ʹ����ɼ���ʵ��
    static std::shared_ptr<IFrameCapture> createCapture(CaptureType type, const CaptureConfig& config = CaptureConfig());

    // �������ƴ����ɼ���ʵ�������ڲ��ϵͳ��
    static std::shared_ptr<IFrameCapture> createCapture(const std::string& name, const CaptureConfig& config = CaptureConfig());
};

#endif // CAPTURE_FACTORY_H