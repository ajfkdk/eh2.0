#include "HumanLikeMovement.h"
#include <cmath>
#include <random>
#include <algorithm>

std::mt19937 HumanLikeMovement::rng(std::random_device{}());

void HumanLikeMovement::Initialize() {
    // 使用随机设备初始化梅森旋转算法
    rng.seed(std::random_device{}());
}

// 生成贝赛尔曲线路径，但是最多50个点，返回前面那段小距离
std::vector<std::pair<float, float>> HumanLikeMovement::GenerateBezierPath(
    float startX, float startY, float endX, float endY, int humanizationFactor, int numPoints) {

    // 计算距离
    float distance = std::sqrt(std::pow(endX - startX, 2) + std::pow(endY - startY, 2));

    // 根据距离和拟人因子决定控制点数量
    int controlPointsCount = CalculateControlPointsCount(distance, humanizationFactor);

    // 生成控制点
    auto controlPoints = GenerateControlPoints(startX, startY, endX, endY, controlPointsCount, humanizationFactor);

    // 决定实际生成的点数，最多50个
    int maxPoints = std::min(numPoints, 50);

    std::vector<std::pair<float, float>> pathPoints;
    if (numPoints <= 50) {
        // 正常生成
        for (int i = 0; i <= numPoints; ++i) {
            float t = static_cast<float>(i) / numPoints;
            pathPoints.push_back(BezierPoint(controlPoints, t));
        }
    }
    else {
        // 只取前面一小段，t从0到maxPoints/numPoints
        for (int i = 0; i <= maxPoints; ++i) {
            float t = static_cast<float>(i) / numPoints;
            pathPoints.push_back(BezierPoint(controlPoints, t));
        }
    }

    return pathPoints;
}

// 计算速度曲线，返回新的路径点 - 距离越大，开始快、结束慢；距离越小，速度越均匀
std::vector<std::pair<float, float>> HumanLikeMovement::ApplySpeedProfile(
    const std::vector<std::pair<float, float>>& points, int humanizationFactor) {

    if (points.size() < 2) return points;

    std::vector<std::pair<float, float>> speedAdjustedPoints;
    int desiredOutputPoints = points.size(); // 保持输出点数与输入相同

    // 计算原始路径的总长度
    float totalDistance = 0.0f;
    std::vector<float> cumulativeDistances = { 0.0f };

    for (size_t i = 1; i < points.size(); i++) {
        float dx = points[i].first - points[i - 1].first;
        float dy = points[i].second - points[i - 1].second;
        float segmentDistance = std::sqrt(dx * dx + dy * dy);
        totalDistance += segmentDistance;
        cumulativeDistances.push_back(totalDistance);
    }

    // 基于距离调整速度曲线参数：距离越大，指数越大，开始越快，结束越慢
    // 标准化距离因子 (假设正常距离范围为0-1000像素)
    float normalizedDistance = std::min(1.0f, totalDistance / 1000.0f);

    // 速度曲线参数 (2.0到3.0范围)
    // 距离越大，指数越接近3.0，导致曲线越陡峭（开始快，结束慢）
    // 距离越小，指数越接近2.0，导致整体速度更均匀
    float speedCurveExponent = 2.0f + normalizedDistance + (humanizationFactor / 100.0f);

    // 确保在2.0-3.0范围内
    speedCurveExponent = std::max(2.0f, std::min(3.0f, speedCurveExponent));

    // 使用非线性分布重新采样点
    for (int i = 0; i < desiredOutputPoints; i++) {
        float t = static_cast<float>(i) / (desiredOutputPoints - 1);

        // 非线性映射：距离大时，开始快，结束慢
        float adjustedT = 1.0f - std::pow(1.0f - t, speedCurveExponent);

        // 找到对应的位置
        float targetDistance = adjustedT * totalDistance;

        // 二分查找找到最近的原始点
        auto it = std::lower_bound(cumulativeDistances.begin(), cumulativeDistances.end(), targetDistance);
        int segmentIndex = (it == cumulativeDistances.begin()) ? 0 : std::distance(cumulativeDistances.begin(), it) - 1;

        // 计算插值因子
        float segmentStartDist = cumulativeDistances[segmentIndex];
        float segmentEndDist = cumulativeDistances[segmentIndex + 1];
        float segmentProgress = (targetDistance - segmentStartDist) / (segmentEndDist - segmentStartDist);

        // 限制segmentProgress在[0,1]范围内
        segmentProgress = std::max(0.0f, std::min(1.0f, segmentProgress));

        // 线性插值计算新点位置
        float x = points[segmentIndex].first + segmentProgress * (points[segmentIndex + 1].first - points[segmentIndex].first);
        float y = points[segmentIndex].second + segmentProgress * (points[segmentIndex + 1].second - points[segmentIndex].second);

        speedAdjustedPoints.push_back(std::make_pair(x, y));
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