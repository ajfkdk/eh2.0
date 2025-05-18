#include "inference.h"
#include <regex>

// ���ܲ��Ժ꣬ȡ��ע�����������ܲ���
//#define benchmark

// ����CUDA֧��
#define USE_CUDA

// ��Сֵ�궨��
#define min(a,b) (((a) < (b)) ? (a) : (b))

/**
 * ���캯��
 */
YOLO_V8::YOLO_V8() {
    // ��Ա��������CreateSession�г�ʼ��
}

/**
 * ��������
 * �ͷŻỰ��Դ
 */
YOLO_V8::~YOLO_V8() {
    delete session;
}

#ifdef USE_CUDA
namespace Ort
{
    // Ϊhalf�������ONNX��������ӳ��
    template<>
    struct TypeToTensorType<half> { static constexpr ONNXTensorElementDataType type = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16; };
}
#endif

/**
 * ��OpenCVͼ��ת��Ϊģ������Blob
 * @param iImg ����ͼ��
 * @param iBlob ���Blob
 * @return �ɹ�����RET_OK
 */
template<typename T>
char* BlobFromImage(cv::Mat& iImg, T& iBlob) {
    int channels = iImg.channels();
    int imgHeight = iImg.rows;
    int imgWidth = iImg.cols;

    // ȷ��ͼ����������
    if (!iImg.isContinuous()) {
        iImg = iImg.clone();
    }

    const uchar* imgData = iImg.data;
    float scale = 1.0f / 255.0f;  // ��һ��ϵ��

    // �����ش���ת��Ϊģ�������ʽ(NCHW)
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
 * ͼ��Ԥ������
 * ִ����ɫ�ռ�ת�������ź�letterbox�Ȳ���
 */
char* YOLO_V8::PreProcess(cv::Mat& iImg, std::vector<int> iImgSize, cv::Mat& oImg)
{
    // ��ɫ�ռ�ת������
    if (iImg.channels() == 3)
    {
        oImg = iImg.clone();
        cv::cvtColor(oImg, oImg, cv::COLOR_BGR2RGB);  // BGRתRGB
    }
    else
    {
        cv::cvtColor(iImg, oImg, cv::COLOR_GRAY2RGB);  // �Ҷ�תRGB
    }

    // �ȱ�������ͼ��letterbox�㷨��
    if (iImg.cols >= iImg.rows)
    {
        // ����ڸߣ����������
        resizeScales = iImg.cols / (float)iImgSize.at(0);
        cv::resize(oImg, oImg, cv::Size(iImgSize.at(0), int(iImg.rows / resizeScales)));
    }
    else
    {
        // �ߴ��ڿ����߶�����
        resizeScales = iImg.rows / (float)iImgSize.at(0);
        cv::resize(oImg, oImg, cv::Size(int(iImg.cols / resizeScales), iImgSize.at(1)));
    }

    // ����letterboxͼ�����ڱߣ�
    cv::Mat tempImg = cv::Mat::zeros(iImgSize.at(0), iImgSize.at(1), CV_8UC3);
    oImg.copyTo(tempImg(cv::Rect(0, 0, oImg.cols, oImg.rows)));
    oImg = tempImg;

    return RET_OK;
}

/**
 * ��������Ự
 * ��ʼ��ONNX Runtime����������ģ��
 */
const char* YOLO_V8::CreateSession(DL_INIT_PARAM& iParams) {
    const char* Ret = RET_OK;

    // ���ģ��·�����Ƿ��������ַ������ܵ������⣩
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
        // ��ʼ�����Ա����
        rectConfidenceThreshold = iParams.rectConfidenceThreshold;
        iouThreshold = iParams.iouThreshold;
        imgSize = iParams.imgSize;
        modelType = iParams.modelType;

        // ����ONNX Runtime����
        env = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "Yolo");
        Ort::SessionOptions sessionOption;

        // ����CUDAִ���ṩ����������ã�
        if (iParams.cudaEnable)
        {
            cudaEnable = iParams.cudaEnable;
            OrtCUDAProviderOptions cudaOption;
            cudaOption.device_id = 0;
            sessionOption.AppendExecutionProvider_CUDA(cudaOption);
        }

        // ����ͼ�Ż�������߳���
        sessionOption.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        sessionOption.SetIntraOpNumThreads(iParams.intraOpNumThreads);
        sessionOption.SetLogSeverityLevel(iParams.logSeverityLevel);

#ifdef _WIN32
        // Windowsƽ̨������ַ�·��
        int ModelPathSize = MultiByteToWideChar(CP_UTF8, 0, iParams.modelPath.c_str(), static_cast<int>(iParams.modelPath.length()), nullptr, 0);
        wchar_t* wide_cstr = new wchar_t[ModelPathSize + 1];
        MultiByteToWideChar(CP_UTF8, 0, iParams.modelPath.c_str(), static_cast<int>(iParams.modelPath.length()), wide_cstr, ModelPathSize);
        wide_cstr[ModelPathSize] = L'\0';
        const wchar_t* modelPath = wide_cstr;
#else
        // ��Windowsƽ̨ʹ�ñ�׼�ַ�·��
        const char* modelPath = iParams.modelPath.c_str();
#endif // _WIN32

        // ����ONNX�Ự������ģ��
        session = new Ort::Session(env, modelPath, sessionOption);
        Ort::AllocatorWithDefaultOptions allocator;

        // ��ȡ���洢����ڵ�����
        size_t inputNodesNum = session->GetInputCount();
        for (size_t i = 0; i < inputNodesNum; i++)
        {
            Ort::AllocatedStringPtr input_node_name = session->GetInputNameAllocated(i, allocator);
            char* temp_buf = new char[50];
            strcpy(temp_buf, input_node_name.get());
            inputNodeNames.push_back(temp_buf);
        }

        // ��ȡ���洢����ڵ�����
        size_t OutputNodesNum = session->GetOutputCount();
        for (size_t i = 0; i < OutputNodesNum; i++)
        {
            Ort::AllocatedStringPtr output_node_name = session->GetOutputNameAllocated(i, allocator);
            char* temp_buf = new char[10];
            strcpy(temp_buf, output_node_name.get());
            outputNodeNames.push_back(temp_buf);
        }

        // ��������ѡ�����Ԥ��
        options = Ort::RunOptions{ nullptr };
        WarmUpSession();

        return RET_OK;
    }
    catch (const std::exception& e)
    {
        // �쳣����
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
 * ��������Ự
 * ��������ͼ��ִ������
 */
char* YOLO_V8::RunSession(cv::Mat& iImg, std::vector<DL_RESULT>& oResult) {
    // ��¼��ʼʱ�䣨�������ܲ�����
    clock_t starttime_1 = clock();
    char* Ret = RET_OK;

    // ͼ��Ԥ����
    cv::Mat processedImg;
    PreProcess(iImg, imgSize, processedImg);

    // ΪԤ����ͼ������ڴ�
    float* blob = new float[processedImg.total() * 3];

    // ת��Ϊblob��ʽ
    BlobFromImage(processedImg, blob);

    // ��������ڵ�ά��
    std::vector<int64_t> inputNodeDims = { 1, 3, imgSize.at(0), imgSize.at(1) };

    // ִ��������������
    TensorProcess(starttime_1, iImg, blob, inputNodeDims, oResult);

    return Ret;
}

/**
 * ����������ģ��
 * ִ��ģ����������������
 */
template<typename N>
char* YOLO_V8::TensorProcess(clock_t& starttime_1, cv::Mat& iImg, N& blob, std::vector<int64_t>& inputNodeDims, std::vector<DL_RESULT>& oResult) {
    // ������������
    Ort::Value inputTensor = Ort::Value::CreateTensor<typename std::remove_pointer<N>::type>(
        Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU), blob, 3 * imgSize.at(0) * imgSize.at(1),
        inputNodeDims.data(), inputNodeDims.size());

#ifdef benchmark
    // ��¼����ʼʱ��
    clock_t starttime_2 = clock();
#endif // benchmark

    // ��������
    auto outputTensor = session->Run(options, inputNodeNames.data(), &inputTensor, 1, outputNodeNames.data(),
        outputNodeNames.size());

#ifdef benchmark
    // ��¼�������ʱ��
    clock_t starttime_3 = clock();
#endif // benchmark

    // ��ȡ���������Ϣ
    Ort::TypeInfo typeInfo = outputTensor.front().GetTypeInfo();
    auto tensor_info = typeInfo.GetTensorTypeAndShapeInfo();
    std::vector<int64_t> outputNodeDims = tensor_info.GetShape();
    auto output = outputTensor.front().GetTensorMutableData<typename std::remove_pointer<N>::type>();

    // �ͷ����������ڴ�
    delete[] blob;

    // ����ģ�����ʹ������
    switch (modelType)
    {
    case YOLO_DETECT_V8:
    case YOLO_DETECT_V8_HALF:
    {
        // YOLOv8���ģ���������
        int numDetections = outputNodeDims[2]; // ��������(8400)
        int numAttributes = outputNodeDims[1]; // ÿ���������������(�����+4)

        std::vector<int> class_ids;     // ���ID
        std::vector<float> confidences; // ���Ŷ�
        std::vector<cv::Rect> boxes;    // �߽��

        // ����ģ�;��ȴ����������
        cv::Mat rawData;
        if (modelType == YOLO_DETECT_V8) {
            // FP32ģ��
            rawData = cv::Mat(numAttributes, numDetections, CV_32F, output);
        }
        else {
            // FP16ģ����Ҫת��
            rawData = cv::Mat(numDetections, numAttributes, CV_16F, output);
            rawData.convertTo(rawData, CV_32F);
        }

        // ת�ú����ܾ����Ա��ڴ���
        cv::Mat transposedData;
        cv::transpose(rawData, transposedData);
        transposedData = transposedData.reshape(1, { numDetections, numAttributes });

        float* data = (float*)transposedData.data;

        // ����ÿ������
        for (int i = 0; i < numDetections; ++i) {
            // ��ȡ������Ŷ�
            float* classes_scores = data + 4;
            float max_score = *std::max_element(classes_scores, classes_scores + (numAttributes - 4));

            // ������Ŷȳ�����ֵ
            if (max_score >= rectConfidenceThreshold) {
                // ȷ�����ID
                int class_id = std::distance(classes_scores, std::max_element(classes_scores, classes_scores + (numAttributes - 4)));

                // ��ȡ�߽������ͳߴ�
                float x = data[0]; // ����x
                float y = data[1]; // ����y
                float w = data[2]; // ���
                float h = data[3]; // �߶�

                // ����߽��ʵ�����꣨�������ű�����
                int left = int((x - 0.5 * w) * resizeScales);
                int top = int((y - 0.5 * h) * resizeScales);
                int width = int(w * resizeScales);
                int height = int(h * resizeScales);

                // ��������
                boxes.emplace_back(left, top, width, height);
                confidences.push_back(max_score);
                class_ids.push_back(class_id);
            }
            data += numAttributes; // �ƶ�����һ������
        }

        // Ӧ�÷Ǽ���ֵ����(NMS)ȥ���ص���
        std::vector<int> nmsResult;
        if (boxes.size() == confidences.size()) {
            cv::dnn::NMSBoxes(boxes, confidences, rectConfidenceThreshold, iouThreshold, nmsResult);

            // �������ռ����
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
        // ���ܲ�������
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
        // YOLOv10���ģ���������
        int numDetections = outputNodeDims[1]; // ��������(300)
        int numAttributes = outputNodeDims[2]; // ÿ���������������(6)

        // ����ÿ�������
        for (int i = 0; i < numDetections; ++i) {
            // ��ȡ������Ϣ
            float x1 = output[i * numAttributes + 0]; // ���Ͻ�x
            float y1 = output[i * numAttributes + 1]; // ���Ͻ�y
            float x2 = output[i * numAttributes + 2]; // ���½�x
            float y2 = output[i * numAttributes + 3]; // ���½�y
            float score = output[i * numAttributes + 4]; // ���Ŷ�
            int class_id = static_cast<int>(output[i * numAttributes + 5]); // ���ID

            // ������Ŷ��Ƿ񳬹���ֵ
            if (score >= rectConfidenceThreshold) {
                // ����߽��ʵ�����꣨�������ű�����
                int left = static_cast<int>(x1 * resizeScales);
                int top = static_cast<int>(y1 * resizeScales);
                int width = static_cast<int>((x2 - x1) * resizeScales);
                int height = static_cast<int>((y2 - y1) * resizeScales);

                // ��������������
                DL_RESULT result;
                result.classId = class_id;
                result.confidence = score;
                result.box = cv::Rect(left, top, width, height);
                oResult.push_back(result);
            }
        }
#ifdef benchmark
        // ���ܲ�������
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
 * �ỰԤ�Ⱥ���
 * ִ��һ�ο������Գ�ʼ��CUDA��Դ
 */
char* YOLO_V8::WarmUpSession() {
    clock_t starttime_1 = clock();

    // �����հ�ͼ��
    cv::Mat iImg = cv::Mat(cv::Size(imgSize.at(0), imgSize.at(1)), CV_8UC3);
    cv::Mat processedImg;

    // Ԥ����ͼ��
    PreProcess(iImg, imgSize, processedImg);

    // ����ģ������ִ�в�ͬ��Ԥ������
    if (modelType < 4) // FP32ģ��
    {
        float* blob = new float[iImg.total() * 3];
        BlobFromImage(processedImg, blob);

        // ��������ά�Ȳ�������������
        std::vector<int64_t> YOLO_input_node_dims = { 1, 3, imgSize.at(0), imgSize.at(1) };
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU), blob, 3 * imgSize.at(0) * imgSize.at(1),
            YOLO_input_node_dims.data(), YOLO_input_node_dims.size());

        // ִ������
        auto output_tensors = session->Run(options, inputNodeNames.data(), &input_tensor, 1, outputNodeNames.data(),
            outputNodeNames.size());

        delete[] blob;

        // ���Ԥ��ʱ��
        clock_t starttime_4 = clock();
        double post_process_time = (double)(starttime_4 - starttime_1) / CLOCKS_PER_SEC * 1000;
        if (cudaEnable)
        {
            std::cout << "[YOLO_V8(CUDA)]: " << "Cuda warm-up cost " << post_process_time << " ms. " << std::endl;
        }
    }
    else // FP16ģ��
    {
#ifdef USE_CUDA
        half* blob = new half[iImg.total() * 3];
        BlobFromImage(processedImg, blob);

        // ��������ά�Ȳ�������������
        std::vector<int64_t> YOLO_input_node_dims = { 1,3,imgSize.at(0),imgSize.at(1) };
        Ort::Value input_tensor = Ort::Value::CreateTensor<half>(
            Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU), blob, 3 * imgSize.at(0) * imgSize.at(1),
            YOLO_input_node_dims.data(), YOLO_input_node_dims.size());

        // ִ������
        auto output_tensors = session->Run(options, inputNodeNames.data(), &input_tensor, 1, outputNodeNames.data(),
            outputNodeNames.size());

        delete[] blob;

        // ���Ԥ��ʱ��
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