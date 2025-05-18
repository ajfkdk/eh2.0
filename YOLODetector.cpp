#include "YOLODetector.h"
#include "inference.h"
#include <iostream>
#include <filesystem>
#include <chrono>

/**
 * YOLODetector实现类（PIMPL模式）
 */
class YOLODetector::Impl {
public:
    YOLO_V8* yoloDetector;                    // YOLO检测器实例
    std::vector<std::string> classes{         // 检测类别
        "ct_body", "ct_head", "t_body", "t_head"
    };
    float confidenceThreshold = 0.5f;         // 置信度阈值
    std::string modelPath;                    // 模型路径
    std::string lastError;                    // 最后一次错误
    bool showDetections = false;              // 是否显示检测结果

    // FPS计算相关变量
    int frameCount = 0;                       // 帧计数
    float fps = 0.0f;                         // FPS值
    std::chrono::time_point<std::chrono::high_resolution_clock> lastFpsTime; // 上次FPS计算时间

    /**
     * 构造函数
     */
    Impl() : yoloDetector(nullptr) {
        // 初始化FPS计时
        lastFpsTime = std::chrono::high_resolution_clock::now();
    }

    /**
     * 析构函数
     */
    ~Impl() {
        if (yoloDetector) {
            delete yoloDetector;
        }
    }

    /**
     * 初始化YOLO检测器
     * @param path 模型路径
     * @return 初始化是否成功
     */
    bool Initialize(const std::string& path) {
        modelPath = path;

        try {
            // 创建YOLO检测器实例
            yoloDetector = new YOLO_V8();

            // 检查模型文件是否存在
            if (!std::filesystem::exists(modelPath)) {
                lastError = "Model file does not exist: " + modelPath;
                std::cerr << lastError << std::endl;
                return false;
            }

            // 配置模型参数
            DL_INIT_PARAM params;
            params.rectConfidenceThreshold = confidenceThreshold;
            params.iouThreshold = 0.6;
            params.modelPath = modelPath;
            params.imgSize = { INPUT_SIZE, INPUT_SIZE };
            params.cudaEnable = true;
            params.modelType = YOLO_DETECT_V8;

            // 初始化检测器类别和会话
            yoloDetector->classes = classes;
            const char* result = yoloDetector->CreateSession(params);

            // 检查初始化结果
            if (result != RET_OK) {
                lastError = std::string(result);
                std::cerr << lastError << std::endl;
                return false;
            }

            std::cout << "YOLO detector initialized successfully" << std::endl;
            return true;
        }
        catch (const std::exception& e) {
            // 异常处理
            lastError = std::string("Exception in Initialize: ") + e.what();
            std::cerr << lastError << std::endl;
            return false;
        }
    }

    /**
     * 处理图像并返回检测结果
     * @param image 输入图像
     * @return 检测结果向量
     */
    std::vector<DetectionResult> ProcessImage(const cv::Mat& image) {
        std::vector<DetectionResult> results;

        // 检查检测器是否初始化
        if (!yoloDetector) {
            lastError = "YOLO detector not initialized";
            return results;
        }

        try {
            // 克隆输入图像以避免修改原始图像
            cv::Mat processedImage = image.clone();
            std::vector<DL_RESULT> dlResults;

            // 执行YOLO推理
            yoloDetector->RunSession(processedImage, dlResults);

            // 转换结果格式
            for (const auto& dlr : dlResults) {
                // 计算目标中心点坐标
                int centerX = dlr.box.x + dlr.box.width / 2;
                int centerY = dlr.box.y + dlr.box.height / 2;

                // 创建检测结果对象
                DetectionResult result(
                    centerX, centerY,
                    dlr.box.width, dlr.box.height,
                    yoloDetector->classes[dlr.classId],
                    dlr.classId,
                    dlr.confidence
                );

                results.push_back(result);
            }

            // 可视化检测结果（如果启用）
            if (showDetections && !image.empty()) {
                // 更新FPS计算
                frameCount++;
                auto now = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> elapsed = now - lastFpsTime;

                // 每秒更新一次FPS
                if (elapsed.count() >= 1.0) {
                    fps = frameCount / elapsed.count();
                    frameCount = 0;
                    lastFpsTime = now;
                }

                // 可视化处理
                cv::Mat displayImage = image.clone();

                // 绘制每个检测框
                for (const auto& dlr : dlResults) {
                    cv::Scalar color(156, 219, 250);  // 边界框颜色
                    cv::rectangle(displayImage, dlr.box, color, 2);

                    // 格式化置信度显示
                    float confidence = std::floor(100 * dlr.confidence) / 100;
                    std::string label = yoloDetector->classes[dlr.classId] + " " +
                        std::to_string(confidence).substr(0, std::to_string(confidence).size() - 4);

                    // 绘制标签背景
                    cv::rectangle(
                        displayImage,
                        cv::Point(dlr.box.x, dlr.box.y - 25),
                        cv::Point(dlr.box.x + label.length() * 15, dlr.box.y),
                        color,
                        cv::FILLED
                    );

                    // 绘制标签文本
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

                // 在图像左上角显示FPS
                std::string fpsText = "FPS: " + std::to_string(static_cast<int>(fps));
                cv::putText(displayImage, fpsText, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 255, 0), 2);

                // 显示结果图像
                cv::namedWindow("Detection Results", cv::WINDOW_NORMAL);
                cv::setWindowProperty("Detection Results", cv::WND_PROP_TOPMOST, 1);
                cv::imshow("Detection Results", displayImage);
                cv::waitKey(1);
            }
        }
        catch (const std::exception& e) {
            // 异常处理
            lastError = std::string("Exception in ProcessImage: ") + e.what();
            std::cerr << lastError << std::endl;
        }

        return results;
    }
};

// YOLODetector类的方法实现（PIMPL模式外部接口）

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