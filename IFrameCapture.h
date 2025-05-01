#ifndef I_FRAME_CAPTURE_H
#define I_FRAME_CAPTURE_H

#include <opencv2/opencv.hpp>
#include <string>
#include <functional>
#include <memory>

// �ɼ���״̬ö��
enum class CaptureStatus {
    CAP_INITIALIZED,
    CAP_RUNNING,
    CAP_PAUSED,
    CAP_STOPPED,
    CAP_ERROR
};

// �ɼ������ýṹ
struct CaptureConfig {
    int width = 320;
    int height = 320;
    bool captureCenter = true;
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    int fps = 60;
};

// ������ص�����
typedef std::function<void(const std::string&, int)> ErrorCallback;

// ֡�ɼ��ӿ�
class IFrameCapture {
public:
    virtual ~IFrameCapture() = default;

    // �������ڹ���
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool pause() = 0;
    virtual bool resume() = 0;
    virtual bool release() = 0;

    // ״̬��ѯ
    virtual CaptureStatus getStatus() const = 0;
    virtual bool isRunning() const = 0;
    virtual std::vector<std::string> getCapabilities() const = 0;

    // ���ù���
    virtual bool configure(const CaptureConfig& config) = 0;
    virtual CaptureConfig getConfiguration() const = 0;

    // ������
    virtual void registerErrorHandler(ErrorCallback handler) = 0;
    virtual std::string getLastError() const = 0;

    // ֡��ȡ - ʵ�ֿ���ѡ�����ʺϵķ�ʽ
    virtual cv::Mat captureFrame() = 0;
};

#endif // I_FRAME_CAPTURE_H
