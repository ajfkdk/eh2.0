#include "HumanLikeMovement.h"
#include <cmath>
#include <random>
#include <algorithm>

std::mt19937 HumanLikeMovement::rng(std::random_device{}());

void HumanLikeMovement::Initialize() {
    // 使用随机设备初始化梅森旋转算法
    rng.seed(std::random_device{}());
}

// 生成贝塞尔曲线上的点
std::vector<std::pair<float, float>> HumanLikeMovement::GenerateBezierPath(
    float startX, float startY, float endX, float endY, int humanizationFactor, int numPoints) {

    // 计算距离
    float distance = std::sqrt(std::pow(endX - startX, 2) + std::pow(endY - startY, 2));

    // 根据距离和拟人因子决定控制点数量
    int controlPointsCount = CalculateControlPointsCount(distance, humanizationFactor);

    // 生成控制点
    auto controlPoints = GenerateControlPoints(startX, startY, endX, endY, controlPointsCount, humanizationFactor);

    // 生成贝塞尔曲线上的点
    std::vector<std::pair<float, float>> pathPoints;
    for (int i = 0; i <= numPoints; ++i) {
        float t = static_cast<float>(i) / numPoints;
        pathPoints.push_back(BezierPoint(controlPoints, t));
    }

    return pathPoints;
}

std::vector<std::pair<float, float>> HumanLikeMovement::ApplySpeedProfile(
    const std::vector<std::pair<float, float>>& points, int humanizationFactor) {

    std::vector<std::pair<float, float>> speedAdjustedPoints;
    int totalPoints = points.size();

    // 根据拟人因子调整速度曲线参数
    float accelPhase = 0.3f - (humanizationFactor / 500.0f); // 0.1 to 0.3
    float decelPhase = 0.7f + (humanizationFactor / 500.0f); // 0.7 to 0.9

    for (int i = 0; i < totalPoints; ++i) {
        float progress = static_cast<float>(i) / (totalPoints - 1);
        float speedFactor;

        // 启动阶段 - 加速
        if (progress < accelPhase) {
            speedFactor = std::pow(progress / accelPhase, 2.0f - humanizationFactor / 100.0f);
        }
        // 减速阶段
        else if (progress > decelPhase) {
            speedFactor = std::pow((1.0f - progress) / (1.0f - decelPhase), 2.0f - humanizationFactor / 100.0f);
        }
        // 巡航阶段
        else {
            speedFactor = 1.0f;

            // 高拟人因子时添加微小速度波动
            if (humanizationFactor > 50) {
                std::uniform_real_distribution<float> dist(0.85f, 1.15f);
                speedFactor *= dist(rng);
            }
        }

        // 应用速度因子，重新采样点的位置
        int adjustedIndex = std::min(static_cast<int>(i * speedFactor), totalPoints - 1);
        speedAdjustedPoints.push_back(points[adjustedIndex]);
    }

    return speedAdjustedPoints;
}

std::pair<float, float> HumanLikeMovement::AddHumanJitter(
    float x, float y, float distance, int humanizationFactor) {
    // 确保 humanizationFactor 非负
    float hFactor = std::max(0.0f, static_cast<float>(humanizationFactor));
    // 计算 maxJitter 并设置最小值
    float maxJitter = std::max(0.0001f, std::min(3.0f, distance * 0.03f) * (hFactor / 50.0f));
    if (hFactor < 30) {
        maxJitter = std::max(0.0001f, maxJitter * (hFactor / 30.0f));
    }
    std::normal_distribution<float> jitterDist(0.0f, maxJitter / 3.0f);
    float jitterX = jitterDist(rng);
    float jitterY = jitterDist(rng);
    return { x + jitterX, y + jitterY };
}


// 生成丢失目标时的拟人化移动
std::vector<std::pair<float, float>> HumanLikeMovement::GenerateTargetLossMovement(
    float startX, float startY, int humanizationFactor, int steps) {

    std::vector<std::pair<float, float>> lossPath;

    // 生成随机方向
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
    std::uniform_real_distribution<float> distDist(5.0f, 20.0f + humanizationFactor * 0.3f);

    float angle = angleDist(rng);
    float distance = distDist(rng);

    // 计算终点
    float endX = startX + std::cos(angle) * distance;
    float endY = startY + std::sin(angle) * distance;

    // 生成贝塞尔移动路径
    return GenerateBezierPath(startX, startY, endX, endY, humanizationFactor, steps);
}

int HumanLikeMovement::CalculateControlPointsCount(float distance, int humanizationFactor) {
    // 基础控制点数量为2（起点和终点）
    int baseCount = 2;

    // 根据距离增加控制点
    int distancePoints = static_cast<int>(std::sqrt(distance) / 10);

    // 根据拟人因子增加控制点
    int humanPoints = humanizationFactor / 25;

    // 计算总控制点数量
    return baseCount + distancePoints + humanPoints;
}

std::vector<std::pair<float, float>> HumanLikeMovement::GenerateControlPoints(
    float startX, float startY, float endX, float endY, int count, int humanizationFactor) {
    std::vector<std::pair<float, float>> controlPoints;
    controlPoints.push_back({ startX, startY });
    float deltaX = endX - startX;
    float deltaY = endY - startY;
    float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    // 确保 humanizationFactor 非负
    float hFactor = std::max(0.0f, static_cast<float>(humanizationFactor));
    // 计算 maxOffset 并设置最小值
    float maxOffset = std::max(0.0001f, distance * (0.1f + hFactor / 200.0f));
    for (int i = 1; i < count - 1; ++i) {
        float ratio = static_cast<float>(i) / (count - 1);
        float lineX = startX + deltaX * ratio;
        float lineY = startY + deltaY * ratio;
        std::normal_distribution<float> offsetDist(0.0f, maxOffset / 3.0f);
        float offsetX = offsetDist(rng);
        float offsetY = offsetDist(rng);
        controlPoints.push_back({ lineX + offsetX, lineY + offsetY });
    }
    controlPoints.push_back({ endX, endY });
    return controlPoints;
}

std::pair<float, float> HumanLikeMovement::BezierPoint(
    const std::vector<std::pair<float, float>>& controlPoints, float t) {

    // 递归计算贝塞尔曲线上的点
    if (controlPoints.size() == 1) {
        return controlPoints[0];
    }

    std::vector<std::pair<float, float>> newPoints;
    for (size_t i = 0; i < controlPoints.size() - 1; ++i) {
        float x = (1 - t) * controlPoints[i].first + t * controlPoints[i + 1].first;
        float y = (1 - t) * controlPoints[i].second + t * controlPoints[i + 1].second;
        newPoints.push_back({ x, y });
    }

    return BezierPoint(newPoints, t);
}