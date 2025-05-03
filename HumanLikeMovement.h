#ifndef HUMAN_LIKE_MOVEMENT_H
#define HUMAN_LIKE_MOVEMENT_H

#include <vector>
#include <utility>
#include <random>

class HumanLikeMovement {
private:
    static std::mt19937 rng;

public:
    // 初始化随机数生成器
    static void Initialize();

    // 生成贝塞尔曲线路径点
    static std::vector<std::pair<float, float>> GenerateBezierPath(
        float startX, float startY, float endX, float endY, int humanizationFactor, int numPoints);

    // 应用速度曲线
    static std::vector<std::pair<float, float>> ApplySpeedProfile(
        const std::vector<std::pair<float, float>>& points, int humanizationFactor);

    // 添加微小抖动
    static std::pair<float, float> AddHumanJitter(
        float x, float y, float distance, int humanizationFactor);

    // 生成丢失目标时的拟人移动
    static std::vector<std::pair<float, float>> GenerateTargetLossMovement(
        float startX, float startY, int humanizationFactor, int steps);

    // 计算贝塞尔曲线控制点数量基于距离和拟人因子
    static int CalculateControlPointsCount(float distance, int humanizationFactor);

    // 生成随机贝塞尔曲线控制点
    static std::vector<std::pair<float, float>> GenerateControlPoints(
        float startX, float startY, float endX, float endY, int count, int humanizationFactor);

    // 计算贝塞尔曲线上的点
    static std::pair<float, float> BezierPoint(
        const std::vector<std::pair<float, float>>& controlPoints, float t);
};

#endif // HUMAN_LIKE_MOVEMENT_H#pragma once
