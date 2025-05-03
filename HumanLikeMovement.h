#ifndef HUMAN_LIKE_MOVEMENT_H
#define HUMAN_LIKE_MOVEMENT_H

#include <vector>
#include <utility>
#include <random>

class HumanLikeMovement {
private:
    static std::mt19937 rng;

public:
    // ��ʼ�������������
    static void Initialize();

    // ���ɱ���������·����
    static std::vector<std::pair<float, float>> GenerateBezierPath(
        float startX, float startY, float endX, float endY, int humanizationFactor, int numPoints);

    // Ӧ���ٶ�����
    static std::vector<std::pair<float, float>> ApplySpeedProfile(
        const std::vector<std::pair<float, float>>& points, int humanizationFactor);

    // ���΢С����
    static std::pair<float, float> AddHumanJitter(
        float x, float y, float distance, int humanizationFactor);

    // ���ɶ�ʧĿ��ʱ�������ƶ�
    static std::vector<std::pair<float, float>> GenerateTargetLossMovement(
        float startX, float startY, int humanizationFactor, int steps);

    // ���㱴�������߿��Ƶ��������ھ������������
    static int CalculateControlPointsCount(float distance, int humanizationFactor);

    // ����������������߿��Ƶ�
    static std::vector<std::pair<float, float>> GenerateControlPoints(
        float startX, float startY, float endX, float endY, int count, int humanizationFactor);

    // ���㱴���������ϵĵ�
    static std::pair<float, float> BezierPoint(
        const std::vector<std::pair<float, float>>& controlPoints, float t);
};

#endif // HUMAN_LIKE_MOVEMENT_H#pragma once
