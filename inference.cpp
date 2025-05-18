#include "inference.h"
#include <regex>

// 性能测试宏，取消注释以启用性能测量
//#define benchmark

// 启用CUDA支持
#define USE_CUDA

// 最小值宏定义
#define min(a,b) (((a) < (b)) ? (a) : (b))

/**
 * 构造函数
 */
YOLO_V8::YOLO_V8() {
    // 成员变量将在CreateSession中初始化
}

/**
 * 析构函数
 * 释放会话资源
 */
YOLO_V8::~YOLO_V8() {
    delete session;
}

#ifdef USE_CUDA
namespace Ort
{
    // 为half类型添加ONNX张量类型映射
    template<>
    struct TypeToTensorType<half> { static constexpr ONNXTensorElementDataType type = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16; };
}
#endif

/**
 * 将OpenCV图像转换为模型输入Blob
 * @param iImg 输入图像
 * @param iBlob 输出Blob
 * @return 成功返回RET_OK
 */
template<typename T>
char* BlobFromImage(cv::Mat& iImg, T& iBlob) {
    int channels = iImg.channels();
    int imgHeight = iImg.rows;
    int imgWidth = iImg.cols;

    // 确保图像数据连续
    if (!iImg.isContinuous()) {
        iImg = iImg.clone();
    }

    const uchar* imgData = iImg.data;
    float scale = 1.0f / 255.0f;  // 归一化系数

    // 逐像素处理，转换为模型所需格式(NCHW)
    for (int c = 0; c < channels; ++c) {
        for (int h = 0; h < imgHeight; ++h) {
            for (int w = 0; w < imgWidth; ++w) {
                iBlob[c * imgWidth * imgHeight + h * imgWidth + w] =
                    typename std::remove_pointer<T>::type(
                        imgData[h * imgWidth * channels + w * channels + c] * scale);
            }
        }
    }
    return RET_OK;
}

/**
 * 图像预处理函数
 * 执行颜色空间转换、缩放和letterbox等操作
 */
char* YOLO_V8::PreProcess(cv::Mat& iImg, std::vector<int> iImgSize, cv::Mat& oImg)
{
    // 颜色空间转换处理
    if (iImg.channels() == 3)
    {
        oImg = iImg.clone();
        cv::cvtColor(oImg, oImg, cv::COLOR_BGR2RGB);  // BGR转RGB
    }
    else
    {
        cv::cvtColor(iImg, oImg, cv::COLOR_GRAY2RGB);  // 灰度转RGB
    }

    // 等比例缩放图像（letterbox算法）
    if (iImg.cols >= iImg.rows)
    {
        // 宽大于高，按宽度缩放
        resizeScales = iImg.cols / (float)iImgSize.at(0);
        cv::resize(oImg, oImg, cv::Size(iImgSize.at(0), int(iImg.rows / resizeScales)));
    }
    else
    {
        // 高大于宽，按高度缩放
        resizeScales = iImg.rows / (float)iImgSize.at(0);
        cv::resize(oImg, oImg, cv::Size(int(iImg.cols / resizeScales), iImgSize.at(1)));
    }

    // 创建letterbox图像（填充黑边）
    cv::Mat tempImg = cv::Mat::zeros(iImgSize.at(0), iImgSize.at(1), CV_8UC3);
    oImg.copyTo(tempImg(cv::Rect(0, 0, oImg.cols, oImg.rows)));
    oImg = tempImg;

    return RET_OK;
}

/**
 * 创建推理会话
 * 初始化ONNX Runtime环境并加载模型
 */
const char* YOLO_V8::CreateSession(DL_INIT_PARAM& iParams) {
    const char* Ret = RET_OK;

    // 检查模型路径中是否有中文字符（可能导致问题）
    std::regex pattern("[\u4e00-\u9fa5]");
    bool result = std::regex_search(iParams.modelPath, pattern);

    if (result)
    {
        Ret = "[YOLO_V8]:Your model path is error. Change your model path without chinese characters.";
        std::cout << Ret << std::endl;
        return Ret;
    }

    try
    {
        // 初始化类成员变量
        rectConfidenceThreshold = iParams.rectConfidenceThreshold;
        iouThreshold = iParams.iouThreshold;
        imgSize = iParams.imgSize;
        modelType = iParams.modelType;

        // 创建ONNX Runtime环境
        env = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "Yolo");
        Ort::SessionOptions sessionOption;

        // 配置CUDA执行提供程序（如果启用）
        if (iParams.cudaEnable)
        {
            cudaEnable = iParams.cudaEnable;
            OrtCUDAProviderOptions cudaOption;
            cudaOption.device_id = 0;
            sessionOption.AppendExecutionProvider_CUDA(cudaOption);
        }

        // 设置图优化级别和线程数
        sessionOption.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        sessionOption.SetIntraOpNumThreads(iParams.intraOpNumThreads);
        sessionOption.SetLogSeverityLevel(iParams.logSeverityLevel);

#ifdef _WIN32
        // Windows平台处理宽字符路径
        int ModelPathSize = MultiByteToWideChar(CP_UTF8, 0, iParams.modelPath.c_str(), static_cast<int>(iParams.modelPath.length()), nullptr, 0);
        wchar_t* wide_cstr = new wchar_t[ModelPathSize + 1];
        MultiByteToWideChar(CP_UTF8, 0, iParams.modelPath.c_str(), static_cast<int>(iParams.modelPath.length()), wide_cstr, ModelPathSize);
        wide_cstr[ModelPathSize] = L'\0';
        const wchar_t* modelPath = wide_cstr;
#else
        // 非Windows平台使用标准字符路径
        const char* modelPath = iParams.modelPath.c_str();
#endif // _WIN32

        // 创建ONNX会话并加载模型
        session = new Ort::Session(env, modelPath, sessionOption);
        Ort::AllocatorWithDefaultOptions allocator;

        // 获取并存储输入节点名称
        size_t inputNodesNum = session->GetInputCount();
        for (size_t i = 0; i < inputNodesNum; i++)
        {
            Ort::AllocatedStringPtr input_node_name = session->GetInputNameAllocated(i, allocator);
            char* temp_buf = new char[50];
            strcpy(temp_buf, input_node_name.get());
            inputNodeNames.push_back(temp_buf);
        }

        // 获取并存储输出节点名称
        size_t OutputNodesNum = session->GetOutputCount();
        for (size_t i = 0; i < OutputNodesNum; i++)
        {
            Ort::AllocatedStringPtr output_node_name = session->GetOutputNameAllocated(i, allocator);
            char* temp_buf = new char[10];
            strcpy(temp_buf, output_node_name.get());
            outputNodeNames.push_back(temp_buf);
        }

        // 设置运行选项并进行预热
        options = Ort::RunOptions{ nullptr };
        WarmUpSession();

        return RET_OK;
    }
    catch (const std::exception& e)
    {
        // 异常处理
        const char* str1 = "[YOLO_V8]:";
        const char* str2 = e.what();
        std::string result = std::string(str1) + std::string(str2);
        char* merged = new char[result.length() + 1];
        std::strcpy(merged, result.c_str());
        std::cout << merged << std::endl;
        delete[] merged;
        return "[YOLO_V8]:Create session failed.";
    }
}

/**
 * 运行推理会话
 * 处理输入图像并执行推理
 */
char* YOLO_V8::RunSession(cv::Mat& iImg, std::vector<DL_RESULT>& oResult) {
    // 记录开始时间（用于性能测量）
    clock_t starttime_1 = clock();
    char* Ret = RET_OK;

    // 图像预处理
    cv::Mat processedImg;
    PreProcess(iImg, imgSize, processedImg);

    // 为预处理图像分配内存
    float* blob = new float[processedImg.total() * 3];

    // 转换为blob格式
    BlobFromImage(processedImg, blob);

    // 设置输入节点维度
    std::vector<int64_t> inputNodeDims = { 1, 3, imgSize.at(0), imgSize.at(1) };

    // 执行张量处理（推理）
    TensorProcess(starttime_1, iImg, blob, inputNodeDims, oResult);

    return Ret;
}

/**
 * 张量处理函数模板
 * 执行模型推理并处理输出结果
 */
template<typename N>
char* YOLO_V8::TensorProcess(clock_t& starttime_1, cv::Mat& iImg, N& blob, std::vector<int64_t>& inputNodeDims, std::vector<DL_RESULT>& oResult) {
    // 创建输入张量
    Ort::Value inputTensor = Ort::Value::CreateTensor<typename std::remove_pointer<N>::type>(
        Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU), blob, 3 * imgSize.at(0) * imgSize.at(1),
        inputNodeDims.data(), inputNodeDims.size());

#ifdef benchmark
    // 记录推理开始时间
    clock_t starttime_2 = clock();
#endif // benchmark

    // 运行推理
    auto outputTensor = session->Run(options, inputNodeNames.data(), &inputTensor, 1, outputNodeNames.data(),
        outputNodeNames.size());

#ifdef benchmark
    // 记录推理结束时间
    clock_t starttime_3 = clock();
#endif // benchmark

    // 获取输出张量信息
    Ort::TypeInfo typeInfo = outputTensor.front().GetTypeInfo();
    auto tensor_info = typeInfo.GetTensorTypeAndShapeInfo();
    std::vector<int64_t> outputNodeDims = tensor_info.GetShape();
    auto output = outputTensor.front().GetTensorMutableData<typename std::remove_pointer<N>::type>();

    // 释放输入数据内存
    delete[] blob;

    // 根据模型类型处理输出
    switch (modelType)
    {
    case YOLO_DETECT_V8:
    case YOLO_DETECT_V8_HALF:
    {
        // YOLOv8检测模型输出处理
        int numDetections = outputNodeDims[2]; // 检测框数量(8400)
        int numAttributes = outputNodeDims[1]; // 每个检测框的属性数量(类别数+4)

        std::vector<int> class_ids;     // 类别ID
        std::vector<float> confidences; // 置信度
        std::vector<cv::Rect> boxes;    // 边界框

        // 根据模型精度处理输出数据
        cv::Mat rawData;
        if (modelType == YOLO_DETECT_V8) {
            // FP32模型
            rawData = cv::Mat(numAttributes, numDetections, CV_32F, output);
        }
        else {
            // FP16模型需要转换
            rawData = cv::Mat(numDetections, numAttributes, CV_16F, output);
            rawData.convertTo(rawData, CV_32F);
        }

        // 转置和重塑矩阵以便于处理
        cv::Mat transposedData;
        cv::transpose(rawData, transposedData);
        transposedData = transposedData.reshape(1, { numDetections, numAttributes });

        float* data = (float*)transposedData.data;

        // 处理每个检测框
        for (int i = 0; i < numDetections; ++i) {
            // 获取类别置信度
            float* classes_scores = data + 4;
            float max_score = *std::max_element(classes_scores, classes_scores + (numAttributes - 4));

            // 如果置信度超过阈值
            if (max_score >= rectConfidenceThreshold) {
                // 确定类别ID
                int class_id = std::distance(classes_scores, std::max_element(classes_scores, classes_scores + (numAttributes - 4)));

                // 获取边界框坐标和尺寸
                float x = data[0]; // 中心x
                float y = data[1]; // 中心y
                float w = data[2]; // 宽度
                float h = data[3]; // 高度

                // 计算边界框实际坐标（考虑缩放比例）
                int left = int((x - 0.5 * w) * resizeScales);
                int top = int((y - 0.5 * h) * resizeScales);
                int width = int(w * resizeScales);
                int height = int(h * resizeScales);

                // 保存检测结果
                boxes.emplace_back(left, top, width, height);
                confidences.push_back(max_score);
                class_ids.push_back(class_id);
            }
            data += numAttributes; // 移动到下一个检测框
        }

        // 应用非极大值抑制(NMS)去除重叠框
        std::vector<int> nmsResult;
        if (boxes.size() == confidences.size()) {
            cv::dnn::NMSBoxes(boxes, confidences, rectConfidenceThreshold, iouThreshold, nmsResult);

            // 保存最终检测结果
            for (int i = 0; i < nmsResult.size(); ++i) {
                int idx = nmsResult[i];
                DL_RESULT result;
                result.classId = class_ids[idx];
                result.confidence = confidences[idx];
                result.box = boxes[idx];
                oResult.push_back(result);
            }
        }
        else {
            std::cerr << "Error: The number of boxes and confidences must be equal before applying NMS." << std::endl;
            return RET_OK;
        }

#ifdef benchmark
        // 性能测量代码
        clock_t starttime_4 = clock();
        double pre_process_time = (double)(starttime_2 - starttime_1) / CLOCKS_PER_SEC * 1000;
        double process_time = (double)(starttime_3 - starttime_2) / CLOCKS_PER_SEC * 1000;
        double post_process_time = (double)(starttime_4 - starttime_3) / CLOCKS_PER_SEC * 1000;

        if (cudaEnable)
        {
            std::cout << "[YOLO_V8(CUDA)]: " << pre_process_time << "ms pre-process, " << process_time << "ms inference, " << post_process_time << "ms post-process." << std::endl;
        }
        else
        {
            std::cout << "[YOLO_V8(CPU)]: " << pre_process_time << "ms pre-process, " << process_time << "ms inference, " << post_process_time << "ms post-process." << std::endl;
        }
#endif // benchmark

        break;
    }
    case YOLO_DETECT_V10:
    {
        // YOLOv10检测模型输出处理
        int numDetections = outputNodeDims[1]; // 检测框数量(300)
        int numAttributes = outputNodeDims[2]; // 每个检测框的属性数量(6)

        // 处理每个检测结果
        for (int i = 0; i < numDetections; ++i) {
            // 提取检测框信息
            float x1 = output[i * numAttributes + 0]; // 左上角x
            float y1 = output[i * numAttributes + 1]; // 左上角y
            float x2 = output[i * numAttributes + 2]; // 右下角x
            float y2 = output[i * numAttributes + 3]; // 右下角y
            float score = output[i * numAttributes + 4]; // 置信度
            int class_id = static_cast<int>(output[i * numAttributes + 5]); // 类别ID

            // 检查置信度是否超过阈值
            if (score >= rectConfidenceThreshold) {
                // 计算边界框实际坐标（考虑缩放比例）
                int left = static_cast<int>(x1 * resizeScales);
                int top = static_cast<int>(y1 * resizeScales);
                int width = static_cast<int>((x2 - x1) * resizeScales);
                int height = static_cast<int>((y2 - y1) * resizeScales);

                // 创建并保存检测结果
                DL_RESULT result;
                result.classId = class_id;
                result.confidence = score;
                result.box = cv::Rect(left, top, width, height);
                oResult.push_back(result);
            }
        }
#ifdef benchmark
        // 性能测量代码
        clock_t starttime_4 = clock();
        double pre_process_time = (double)(starttime_2 - starttime_1) / CLOCKS_PER_SEC * 1000;
        double process_time = (double)(starttime_3 - starttime_2) / CLOCKS_PER_SEC * 1000;
        double post_process_time = (double)(starttime_4 - starttime_3) / CLOCKS_PER_SEC * 1000;

        if (cudaEnable)
        {
            std::cout << "[YOLO_V8(CUDA)]: " << pre_process_time << "ms pre-process, " << process_time << "ms inference, " << post_process_time << "ms post-process." << std::endl;
        }
        else
        {
            std::cout << "[YOLO_V8(CPU)]: " << pre_process_time << "ms pre-process, " << process_time << "ms inference, " << post_process_time << "ms post-process." << std::endl;
        }
#endif // benchmark
    }

    return RET_OK;
    }
}

/**
 * 会话预热函数
 * 执行一次空推理以初始化CUDA资源
 */
char* YOLO_V8::WarmUpSession() {
    clock_t starttime_1 = clock();

    // 创建空白图像
    cv::Mat iImg = cv::Mat(cv::Size(imgSize.at(0), imgSize.at(1)), CV_8UC3);
    cv::Mat processedImg;

    // 预处理图像
    PreProcess(iImg, imgSize, processedImg);

    // 根据模型类型执行不同的预热流程
    if (modelType < 4) // FP32模型
    {
        float* blob = new float[iImg.total() * 3];
        BlobFromImage(processedImg, blob);

        // 设置输入维度并创建输入张量
        std::vector<int64_t> YOLO_input_node_dims = { 1, 3, imgSize.at(0), imgSize.at(1) };
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU), blob, 3 * imgSize.at(0) * imgSize.at(1),
            YOLO_input_node_dims.data(), YOLO_input_node_dims.size());

        // 执行推理
        auto output_tensors = session->Run(options, inputNodeNames.data(), &input_tensor, 1, outputNodeNames.data(),
            outputNodeNames.size());

        delete[] blob;

        // 输出预热时间
        clock_t starttime_4 = clock();
        double post_process_time = (double)(starttime_4 - starttime_1) / CLOCKS_PER_SEC * 1000;
        if (cudaEnable)
        {
            std::cout << "[YOLO_V8(CUDA)]: " << "Cuda warm-up cost " << post_process_time << " ms. " << std::endl;
        }
    }
    else // FP16模型
    {
#ifdef USE_CUDA
        half* blob = new half[iImg.total() * 3];
        BlobFromImage(processedImg, blob);

        // 设置输入维度并创建输入张量
        std::vector<int64_t> YOLO_input_node_dims = { 1,3,imgSize.at(0),imgSize.at(1) };
        Ort::Value input_tensor = Ort::Value::CreateTensor<half>(
            Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU), blob, 3 * imgSize.at(0) * imgSize.at(1),
            YOLO_input_node_dims.data(), YOLO_input_node_dims.size());

        // 执行推理
        auto output_tensors = session->Run(options, inputNodeNames.data(), &input_tensor, 1, outputNodeNames.data(),
            outputNodeNames.size());

        delete[] blob;

        // 输出预热时间
        clock_t starttime_4 = clock();
        double post_process_time = (double)(starttime_4 - starttime_1) / CLOCKS_PER_SEC * 1000;
        if (cudaEnable)
        {
            std::cout << "[YOLO_V8(CUDA)]: " << "Cuda warm-up cost " << post_process_time << " ms. " << std::endl;
        }
#endif
    }
    return RET_OK;
}