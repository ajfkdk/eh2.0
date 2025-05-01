#include "YOLODetector.h"
#include "inference.h"
#include <iostream>
#include <filesystem>

// PIMPLģʽ��ʵ����
class YOLODetector::Impl {
public:
    YOLO_V8* yoloDetector;
    std::vector<std::string> classes{ "ct_body", "ct_head", "t_body", "t_head" };
    float confidenceThreshold = 0.5f;
    std::string modelPath;
    std::string lastError;
    bool showDetections = false;

    Impl() : yoloDetector(nullptr) {}

    ~Impl() {
        if (yoloDetector) {
            delete yoloDetector;
        }
    }

    // ��ʼ��YOLO�����
    bool Initialize(const std::string& path) {
        modelPath = path;

        try {
            // ����YOLO�����
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
            params.iouThreshold = 0.5;
            params.modelPath = modelPath;
            params.imgSize = { INPUT_SIZE, INPUT_SIZE };
            params.cudaEnable = true;
            params.modelType = YOLO_DETECT_V8;

            // ��ʼ��������Ự
            yoloDetector->classes = classes;
            const char* result = yoloDetector->CreateSession(params);

            if (result != RET_OK) {
                lastError = std::string(result);
                std::cerr << lastError << std::endl;
                return false;
            }

            std::cout << "YOLO detector initialized successfully" << std::endl;
            return true;
        }
        catch (const std::exception& e) {
            lastError = std::string("Exception in Initialize: ") + e.what();
            std::cerr << lastError << std::endl;
            return false;
        }
    }

    // ����ͼ��
    std::vector<DetectionResult> ProcessImage(const cv::Mat& image) {
        std::vector<DetectionResult> results;

        if (!yoloDetector) {
            lastError = "YOLO detector not initialized";
            return results;
        }

        try {
            cv::Mat processedImage = image.clone();
            std::vector<DL_RESULT> dlResults;

            // ����YOLO����
            yoloDetector->RunSession(processedImage, dlResults);

            // ת�������ʽ
            for (const auto& dlr : dlResults) {
                // �������ĵ�����
                int centerX = dlr.box.x + dlr.box.width / 2;
                int centerY = dlr.box.y + dlr.box.height / 2;

                DetectionResult result(
                    centerX, centerY,
                    dlr.box.width, dlr.box.height,
                    yoloDetector->classes[dlr.classId],
                    dlr.classId,
                    dlr.confidence
                );

                results.push_back(result);
            }

            // ��ʾ�����
            if (showDetections && !image.empty()) {
                cv::Mat displayImage = image.clone();

                for (const auto& dlr : dlResults) {
                    cv::Scalar color(156, 219, 250);
                    cv::rectangle(displayImage, dlr.box, color, 2);

                    float confidence = std::floor(100 * dlr.confidence) / 100;
                    std::string label = yoloDetector->classes[dlr.classId] + " " +
                        std::to_string(confidence).substr(0, std::to_string(confidence).size() - 4);

                    cv::rectangle(
                        displayImage,
                        cv::Point(dlr.box.x, dlr.box.y - 25),
                        cv::Point(dlr.box.x + label.length() * 15, dlr.box.y),
                        color,
                        cv::FILLED
                    );

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

                // ��ʾͼ��
                cv::namedWindow("Detection Results", cv::WINDOW_NORMAL);
                cv::setWindowProperty("Detection Results", cv::WND_PROP_TOPMOST, 1);
                cv::imshow("Detection Results", displayImage);
                cv::waitKey(1);
            }
        }
        catch (const std::exception& e) {
            lastError = std::string("Exception in ProcessImage: ") + e.what();
            std::cerr << lastError << std::endl;
        }

        return results;
    }
};

// YOLODetector��ķ���ʵ��
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