#pragma once

// 基本返回码定义
#define RET_OK nullptr      // 操作成功返回值
#define RET_ERR "ERROR"     // 操作失败返回值

// 是否启用CUDA
#define USE_CUDA

// Windows平台特定头文件
#ifdef _WIN32
#include <Windows.h>
#include <direct.h>
#include <io.h>
#endif

// 标准库与OpenCV
#include <string>
#include <vector>
#include <cstdio>
#include <opencv2/opencv.hpp>
#include "onnxruntime_cxx_api.h"

// CUDA支持
#ifdef USE_CUDA
#include <cuda_fp16.h>
#endif

// 模型输入的默认尺寸
#define INPUT_SIZE 320

/**
 * 模型类型枚举
 * 包含FP32和FP16两种精度类型的模型
 */
enum MODEL_TYPE
{
    // 单精度浮点模型 (FP32)
    YOLO_DETECT_V8 = 1,     // YOLOv8检测模型
    YOLO_POSE = 2,          // YOLO姿态估计模型
    YOLO_CLS = 3,           // YOLO分类模型

    // 半精度浮点模型 (FP16)
    YOLO_DETECT_V8_HALF = 4, // YOLOv8检测模型(FP16)
    YOLO_POSE_V8_HALF = 5,   // YOLO姿态估计模型(FP16)
    YOLO_CLS_HALF = 6,       // YOLO分类模型(FP16)

    YOLO_DETECT_V10 = 7,     // YOLOv10检测模型
};

/**
 * YOLO模型初始化参数结构体
 */
typedef struct _DL_INIT_PARAM
{
    std::string modelPath;                  // 模型文件路径
    MODEL_TYPE modelType = YOLO_DETECT_V10; // 模型类型，默认为YOLOv10
    std::vector<int> imgSize = { INPUT_SIZE, INPUT_SIZE }; // 输入图像尺寸
    float rectConfidenceThreshold = 0.8;    // 检测框置信度阈值
    float iouThreshold = 0.5;               // IOU阈值(用于NMS)
    int keyPointsNum = 2;                   // 关键点数量(用于姿态估计)
    bool cudaEnable = true;                 // 是否启用CUDA加速
    int logSeverityLevel = 3;               // 日志级别
    int intraOpNumThreads = 4;              // 内部线程数
} DL_INIT_PARAM;

/**
 * YOLO模型检测结果结构体
 */
typedef struct _DL_RESULT
{
    int classId;                    // 类别ID
    float confidence;               // 置信度
    cv::Rect box;                   // 边界框
    std::vector<cv::Point2f> keyPoints; // 关键点坐标(用于姿态估计)
} DL_RESULT;

/**
 * YOLO模型推理核心类
 * 支持YOLOv8、YOLOv10等多种模型
 */
class YOLO_V8
{
public:
    /**
     * 构造函数
     */
    YOLO_V8();

    /**
     * 析构函数
     */
    ~YOLO_V8();

public:
    /**
     * 创建推理会话
     * @param iParams 初始化参数
     * @return 成功返回RET_OK，失败返回错误信息
     */
    const char* CreateSession(DL_INIT_PARAM& iParams);

    /**
     * 运行推理会话
     * @param iImg 输入图像
     * @param oResult 输出结果向量
     * @return 成功返回RET_OK，失败返回错误信息
     */
    char* RunSession(cv::Mat& iImg, std::vector<DL_RESULT>& oResult);

    /**
     * 会话预热函数，用于CUDA初始化
     * @return 成功返回RET_OK，失败返回错误信息
     */
    char* WarmUpSession();

    /**
     * 张量处理函数模板
     * 适用于不同数据类型(如float, half)的推理过程
     * @param starttime_1 开始时间(用于性能测量)
     * @param iImg 输入图像
     * @param blob 图像数据块
     * @param inputNodeDims 输入节点维度
     * @param oResult 输出结果向量
     * @return 成功返回RET_OK，失败返回错误信息
     */
    template<typename N>
    char* TensorProcess(clock_t& starttime_1, cv::Mat& iImg, N& blob, std::vector<int64_t>& inputNodeDims,
        std::vector<DL_RESULT>& oResult);

    /**
     * 图像预处理函数
     * @param iImg 输入图像
     * @param iImgSize 目标图像尺寸
     * @param oImg 输出处理后的图像
     * @return 成功返回RET_OK，失败返回错误信息
     */
    char* PreProcess(cv::Mat& iImg, std::vector<int> iImgSize, cv::Mat& oImg);

    // 类别名称向量
    std::vector<std::string> classes{};

private:
    Ort::Env env;                   // ONNX运行时环境
    Ort::Session* session;          // ONNX会话
    bool cudaEnable;                // 是否启用CUDA
    Ort::RunOptions options;        // 运行选项
    std::vector<const char*> inputNodeNames;   // 输入节点名称
    std::vector<const char*> outputNodeNames;  // 输出节点名称

    // 模型参数
    float* blob;                    // 图像数据块
    size_t blobSize;                // 数据块大小
    MODEL_TYPE modelType;           // 模型类型
    std::vector<int> imgSize;       // 图像尺寸
    float rectConfidenceThreshold;  // 置信度阈值
    float iouThreshold;             // IOU阈值
    float resizeScales;             // letterbox缩放比例
};