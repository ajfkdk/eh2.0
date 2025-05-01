#ifndef DETECTION_RESULT_H
#define DETECTION_RESULT_H

#include <string>
#include <chrono>

// ������ṹ�� - ����Ŀ��λ�á��������Ŷ���Ϣ
struct DetectionResult {
    int x;              // Ŀ������X����
    int y;              // Ŀ������Y����
    int width;          // Ŀ����
    int height;         // Ŀ��߶�
    std::string className; // Ŀ���������
    int classId;        // Ŀ�����ID (-1��ʾ��Ŀ��)
    float confidence;   // ���Ŷ�
    std::chrono::high_resolution_clock::time_point timestamp; // ʱ���

    // Ĭ�Ϲ��캯�� - ��Ŀ��״̬
    DetectionResult() :
        x(0), y(0), width(0), height(0),
        className("none"), classId(-1), confidence(0.0f) {
        timestamp = std::chrono::high_resolution_clock::now();
    }

    // �������Ĺ��캯��
    DetectionResult(int _x, int _y, int _width, int _height,
        const std::string& _className, int _classId, float _confidence) :
        x(_x), y(_y), width(_width), height(_height),
        className(_className), classId(_classId), confidence(_confidence) {
        timestamp = std::chrono::high_resolution_clock::now();
    }
};

#endif // DETECTION_RESULT_H