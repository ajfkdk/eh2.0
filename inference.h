#pragma once

// ���������붨��
#define RET_OK nullptr      // �����ɹ�����ֵ
#define RET_ERR "ERROR"     // ����ʧ�ܷ���ֵ

// �Ƿ�����CUDA
#define USE_CUDA

// Windowsƽ̨�ض�ͷ�ļ�
#ifdef _WIN32
#include <Windows.h>
#include <direct.h>
#include <io.h>
#endif

// ��׼����OpenCV
#include <string>
#include <vector>
#include <cstdio>
#include <opencv2/opencv.hpp>
#include "onnxruntime_cxx_api.h"

// CUDA֧��
#ifdef USE_CUDA
#include <cuda_fp16.h>
#endif

// ģ�������Ĭ�ϳߴ�
#define INPUT_SIZE 320

/**
 * ģ������ö��
 * ����FP32��FP16���־������͵�ģ��
 */
enum MODEL_TYPE
{
    // �����ȸ���ģ�� (FP32)
    YOLO_DETECT_V8 = 1,     // YOLOv8���ģ��
    YOLO_POSE = 2,          // YOLO��̬����ģ��
    YOLO_CLS = 3,           // YOLO����ģ��

    // �뾫�ȸ���ģ�� (FP16)
    YOLO_DETECT_V8_HALF = 4, // YOLOv8���ģ��(FP16)
    YOLO_POSE_V8_HALF = 5,   // YOLO��̬����ģ��(FP16)
    YOLO_CLS_HALF = 6,       // YOLO����ģ��(FP16)

    YOLO_DETECT_V10 = 7,     // YOLOv10���ģ��
};

/**
 * YOLOģ�ͳ�ʼ�������ṹ��
 */
typedef struct _DL_INIT_PARAM
{
    std::string modelPath;                  // ģ���ļ�·��
    MODEL_TYPE modelType = YOLO_DETECT_V10; // ģ�����ͣ�Ĭ��ΪYOLOv10
    std::vector<int> imgSize = { INPUT_SIZE, INPUT_SIZE }; // ����ͼ��ߴ�
    float rectConfidenceThreshold = 0.8;    // �������Ŷ���ֵ
    float iouThreshold = 0.5;               // IOU��ֵ(����NMS)
    int keyPointsNum = 2;                   // �ؼ�������(������̬����)
    bool cudaEnable = true;                 // �Ƿ�����CUDA����
    int logSeverityLevel = 3;               // ��־����
    int intraOpNumThreads = 4;              // �ڲ��߳���
} DL_INIT_PARAM;

/**
 * YOLOģ�ͼ�����ṹ��
 */
typedef struct _DL_RESULT
{
    int classId;                    // ���ID
    float confidence;               // ���Ŷ�
    cv::Rect box;                   // �߽��
    std::vector<cv::Point2f> keyPoints; // �ؼ�������(������̬����)
} DL_RESULT;

/**
 * YOLOģ�����������
 * ֧��YOLOv8��YOLOv10�ȶ���ģ��
 */
class YOLO_V8
{
public:
    /**
     * ���캯��
     */
    YOLO_V8();

    /**
     * ��������
     */
    ~YOLO_V8();

public:
    /**
     * ��������Ự
     * @param iParams ��ʼ������
     * @return �ɹ�����RET_OK��ʧ�ܷ��ش�����Ϣ
     */
    const char* CreateSession(DL_INIT_PARAM& iParams);

    /**
     * ��������Ự
     * @param iImg ����ͼ��
     * @param oResult ����������
     * @return �ɹ�����RET_OK��ʧ�ܷ��ش�����Ϣ
     */
    char* RunSession(cv::Mat& iImg, std::vector<DL_RESULT>& oResult);

    /**
     * �ỰԤ�Ⱥ���������CUDA��ʼ��
     * @return �ɹ�����RET_OK��ʧ�ܷ��ش�����Ϣ
     */
    char* WarmUpSession();

    /**
     * ����������ģ��
     * �����ڲ�ͬ��������(��float, half)���������
     * @param starttime_1 ��ʼʱ��(�������ܲ���)
     * @param iImg ����ͼ��
     * @param blob ͼ�����ݿ�
     * @param inputNodeDims ����ڵ�ά��
     * @param oResult ����������
     * @return �ɹ�����RET_OK��ʧ�ܷ��ش�����Ϣ
     */
    template<typename N>
    char* TensorProcess(clock_t& starttime_1, cv::Mat& iImg, N& blob, std::vector<int64_t>& inputNodeDims,
        std::vector<DL_RESULT>& oResult);

    /**
     * ͼ��Ԥ������
     * @param iImg ����ͼ��
     * @param iImgSize Ŀ��ͼ��ߴ�
     * @param oImg ���������ͼ��
     * @return �ɹ�����RET_OK��ʧ�ܷ��ش�����Ϣ
     */
    char* PreProcess(cv::Mat& iImg, std::vector<int> iImgSize, cv::Mat& oImg);

    // �����������
    std::vector<std::string> classes{};

private:
    Ort::Env env;                   // ONNX����ʱ����
    Ort::Session* session;          // ONNX�Ự
    bool cudaEnable;                // �Ƿ�����CUDA
    Ort::RunOptions options;        // ����ѡ��
    std::vector<const char*> inputNodeNames;   // ����ڵ�����
    std::vector<const char*> outputNodeNames;  // ����ڵ�����

    // ģ�Ͳ���
    float* blob;                    // ͼ�����ݿ�
    size_t blobSize;                // ���ݿ��С
    MODEL_TYPE modelType;           // ģ������
    std::vector<int> imgSize;       // ͼ��ߴ�
    float rectConfidenceThreshold;  // ���Ŷ���ֵ
    float iouThreshold;             // IOU��ֵ
    float resizeScales;             // letterbox���ű���
};