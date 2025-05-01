#ifndef CAPTURE_REGISTRY_H
#define CAPTURE_REGISTRY_H

#include "IFrameCapture.h"
#include <memory>
#include <functional>
#include <string>
#include <map>

// �ɼ���������������
using CaptureCreator = std::function<std::shared_ptr<IFrameCapture>(const CaptureConfig&)>;

class CaptureRegistry {
private:
    static std::map<std::string, CaptureCreator>& getRegistry();

public:
    // ע��ɼ�ʵ��
    static bool registerImplementation(const std::string& name, CaptureCreator creator);

    // �����ɼ�ʵ��
    static std::shared_ptr<IFrameCapture> create(const std::string& name, const CaptureConfig& config = CaptureConfig());

    // ��ȡ������ע���ʵ������
    static std::vector<std::string> getRegisteredImplementations();
};

#endif // CAPTURE_REGISTRY_H
