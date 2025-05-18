#include "YOLODetector.h"
#include "inference.h"
#include <iostream>
#include <filesystem>
#include <chrono>

/**
 * YOLODetectorʵ���ࣨPIMPLģʽ��
 */
class YOLODetector::Impl {
public:
    YOLO_V8* yoloDetector;                    // YOLO�����ʵ��
    std::vector<std::string> classes{         // ������
        "ct_body", "ct_head", "t_body", "t_head"
    };
    float confidenceThreshold = 0.5f;         // ���Ŷ���ֵ
    std::string modelPath;                    // ģ��·��
    std::string lastError;                    // ���һ�δ���
    bool showDetections = false;              // �Ƿ���ʾ�����

    // FPS������ر���
    int frameCount = 0;                       // ֡����
    float fps = 0.0f;                         // FPSֵ
    std::chrono::time_point<std::chrono::high_resolution_clock> lastFpsTime; // �ϴ�FPS����ʱ��

    /**
     * ���캯��
     */
    Impl() : yoloDetector(nullptr) {
        // ��ʼ��FPS��ʱ
        lastFpsTime = std::chrono::high_resolution_clock::now();
    }

    /**
     * ��������
     */
    ~Impl() {
        if (yoloDetector) {
            delete yoloDetector;
        }
    }

    /**
     * ��ʼ��YOLO�����
     * @param path ģ��·��
     * @return ��ʼ���Ƿ�ɹ�
     */
    bool Initialize(const std::string& path) {
        modelPath = path;

        try {
            // ����YOLO�����ʵ��
            yoloDetector = new YOLO_V8();

            // ���ģ���ļ��Ƿ����
            if (!std::filesystem::exists(modelPath)) {
                lastError = "Model file does not exist: " + modelPath;
                std::cerr << lastError << std::endl;
                return false;
            }

            // ����ģ�Ͳ���
            DL_INIT_PARAM params;
            params.rectConfidenceThreshold = confidenceThreshold;
            params.iouThreshold = 0.6;
            params.modelPath = modelPath;
            params.imgSize = { INPUT_SIZE, INPUT_SIZE };
            params.cudaEnable = true;
            params.modelType = YOLO_DETECT_V8;

            // ��ʼ����������ͻỰ
            yoloDetector->classes = classes;
            const char* result = yoloDetector->CreateSession(params);

            // ����ʼ�����
            if (result != RET_OK) {
                lastError = std::string(result);
                std::cerr << lastError << std::endl;
                return false;
            }

            std::cout << "YOLO detector initialized successfully" << std::endl;
            return true;
        }
        catch (const std::exception& e) {
            // �쳣����
            lastError = std::string("Exception in Initialize: ") + e.what();
            std::cerr << lastError << std::endl;
            return false;
        }
    }

    /**
     * ����ͼ�񲢷��ؼ����
     * @param image ����ͼ��
     * @return ���������
     */
    std::vector<DetectionResult> ProcessImage(const cv::Mat& image) {
        std::vector<DetectionResult> results;

        // ��������Ƿ��ʼ��
        if (!yoloDetector) {
            lastError = "YOLO detector not initialized";
            return results;
        }

        try {
            // ��¡����ͼ���Ա����޸�ԭʼͼ��
            cv::Mat processedImage = image.clone();
            std::vector<DL_RESULT> dlResults;

            // ִ��YOLO����
            yoloDetector->RunSession(processedImage, dlResults);

            // ת�������ʽ
            for (const auto& dlr : dlResults) {
                // ����Ŀ�����ĵ�����
                int centerX = dlr.box.x + dlr.box.width / 2;
                int centerY = dlr.box.y + dlr.box.height / 2;

                // �������������
                DetectionResult result(
                    centerX, centerY,
                    dlr.box.width, dlr.box.height,
                    yoloDetector->classes[dlr.classId],
                    dlr.classId,
                    dlr.confidence
                );

                results.push_back(result);
            }

            // ���ӻ��������������ã�
            if (showDetections && !image.empty()) {
                // ����FPS����
                frameCount++;
                auto now = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> elapsed = now - lastFpsTime;

                // ÿ�����һ��FPS
                if (elapsed.count() >= 1.0) {
                    fps = frameCount / elapsed.count();
                    frameCount = 0;
                    lastFpsTime = now;
                }

                // ���ӻ�����
                cv::Mat displayImage = image.clone();

                // ����ÿ������
                for (const auto& dlr : dlResults) {
                    cv::Scalar color(156, 219, 250);  // �߽����ɫ
                    cv::rectangle(displayImage, dlr.box, color, 2);

                    // ��ʽ�����Ŷ���ʾ
                    float confidence = std::floor(100 * dlr.confidence) / 100;
                    std::string label = yoloDetector->classes[dlr.classId] + " " +
                        std::to_string(confidence).substr(0, std::to_string(confidence).size() - 4);

                    // ���Ʊ�ǩ����
                    cv::rectangle(
                        displayImage,
                        cv::Point(dlr.box.x, dlr.box.y - 25),
                        cv::Point(dlr.box.x + label.length() * 15, dlr.box.y),
                        color,
                        cv::FILLED
                    );

                    // ���Ʊ�ǩ�ı�
                    cv::putText(
                        displayImage,
                        label,
                        cv::Point(dlr.box.x, dlr.box.y - 5),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.75,
                        cv::Scalar(0, 0, 0),
                        2
                    );
                }

                // ��ͼ�����Ͻ���ʾFPS
                std::string fpsText = "FPS: " + std::to_string(static_cast<int>(fps));
                cv::putText(displayImage, fpsText, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 255, 0), 2);

                // ��ʾ���ͼ��
                cv::namedWindow("Detection Results", cv::WINDOW_NORMAL);
                cv::setWindowProperty("Detection Results", cv::WND_PROP_TOPMOST, 1);
                cv::imshow("Detection Results", displayImage);
                cv::waitKey(1);
            }
        }
        catch (const std::exception& e) {
            // �쳣����
            lastError = std::string("Exception in ProcessImage: ") + e.what();
            std::cerr << lastError << std::endl;
        }

        return results;
    }
};

// YOLODetector��ķ���ʵ�֣�PIMPLģʽ�ⲿ�ӿڣ�

YOLODetector::YOLODetector() : pImpl(std::make_unique<Impl>()) {}

YOLODetector::~YOLODetector() = default;

bool YOLODetector::Initialize(const std::string& modelPath) {
    return pImpl->Initialize(modelPath);
}

std::vector<DetectionResult> YOLODetector::ProcessImage(const cv::Mat& image) {
    return pImpl->ProcessImage(image);
}

void YOLODetector::SetConfidenceThreshold(float threshold) {
    pImpl->confidenceThreshold = threshold;
}

std::string YOLODetector::GetLastError() const {
    return pImpl->lastError;
}

void YOLODetector::SetShowDetections(bool show) {
    pImpl->showDetections = show;
}