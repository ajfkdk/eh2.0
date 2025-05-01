#ifndef DETECTION_RESULT_H
#define DETECTION_RESULT_H

#include <string>
#include <chrono>

// 检测结果结构体 - 包含目标位置、类别和置信度信息
struct DetectionResult {
    int x;              // 目标中心X坐标
    int y;              // 目标中心Y坐标
    int width;          // 目标宽度
    int height;         // 目标高度
    std::string className; // 目标类别名称
    int classId;        // 目标类别ID (-1表示无目标)
    float confidence;   // 置信度
    std::chrono::high_resolution_clock::time_point timestamp; // 时间戳

    // 默认构造函数 - 无目标状态
    DetectionResult() :
        x(0), y(0), width(0), height(0),
        className("none"), classId(-1), confidence(0.0f) {
        timestamp = std::chrono::high_resolution_clock::now();
    }

    // 带参数的构造函数
    DetectionResult(int _x, int _y, int _width, int _height,
        const std::string& _className, int _classId, float _confidence) :
        x(_x), y(_y), width(_width), height(_height),
        className(_className), classId(_classId), confidence(_confidence) {
        timestamp = std::chrono::high_resolution_clock::now();
    }
};

#endif // DETECTION_RESULT_H