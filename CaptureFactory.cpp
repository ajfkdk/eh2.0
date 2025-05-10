#include "CaptureFactory.h"
#include "ScreenCaptureWindows.h"
#include "NetworkStreamCapture.h"  // �����µ�ͷ�ļ�
#include "CaptureRegistry.h"

std::shared_ptr<IFrameCapture> CaptureFactory::createCapture(CaptureType type, const CaptureConfig& config) {
    std::shared_ptr<IFrameCapture> capture = nullptr;

    switch (type) {
    case CaptureType::WINDOWS_SCREEN:
        capture = std::make_shared<ScreenCaptureWindows>();
        break;
    case CaptureType::NETWORK_STREAM:
        capture = std::make_shared<NetworkStreamCapture>();  // ʹ��Ĭ�϶˿�
        break;
        // �������͵�ʵ��...
    default:
        // δ֪���ͷ��ؿ�ָ��
        return nullptr;
    }

    if (capture) {
        capture->configure(config);
        capture->initialize();
    }

    return capture;
}

std::shared_ptr<IFrameCapture> CaptureFactory::createCapture(const std::string& name, const CaptureConfig& config) {
    // ��ע����д���
    return CaptureRegistry::create(name, config);
}