#include <iostream>
#include <thread>
#include "ScreenCaptureWindows.h"
#include <opencv2/opencv.hpp>

// ��ScreenCaptureWindows.cpp�ж����Frame�ṹ�壬��Ҫ����������
struct Frame {
    cv::Mat image;
    std::chrono::high_resolution_clock::time_point timestamp;

    Frame() = default;

    Frame(const cv::Mat& img) : image(img) {
        timestamp = std::chrono::high_resolution_clock::now();
    }
};

// ���ڻ�ȡ֡���ⲿ��������
namespace CaptureModule {
    bool WaitForFrame(Frame& frame);
}

int main() {
    std::thread captureThread;

    try {
        // ��ʼ���ɼ�ģ��
        captureThread = CaptureModule::Initialize();

        // TODO: ��ʼ������ģ��
        // ProcessModule::Initialize();
        // DetectionModule::Initialize();
        // TrackingModule::Initialize();
        // RenderModule::Initialize();

        // ��ѭ��
        Frame frame;
        while (CaptureModule::IsRunning()) {
            if (CaptureModule::WaitForFrame(frame)) {
                if (!frame.image.empty()) {
                    cv::imshow("Captured Frame", frame.image);

                    // ��ESC���˳�
                    if (cv::waitKey(1) == 27) {
                        CaptureModule::Stop();
                        break;
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    // ������Դ
    CaptureModule::Cleanup();

    // �ȴ��߳̽���
    if (captureThread.joinable()) {
        captureThread.join();
    }

    // �ͷ���Դ
    cv::destroyAllWindows();

    std::cout << "Program terminated normally" << std::endl;
    return 0;
}
