#include "HumanLikeMovement.h"
#include <cmath>
#include <random>
#include <algorithm>

std::mt19937 HumanLikeMovement::rng(std::random_device{}());

void HumanLikeMovement::Initialize() {
    // ʹ������豸��ʼ��÷ɭ��ת�㷨
    rng.seed(std::random_device{}());
}

// ���ɱ���������·�����������50���㣬����ǰ���Ƕ�С����
std::vector<std::pair<float, float>> HumanLikeMovement::GenerateBezierPath(
    float startX, float startY, float endX, float endY, int humanizationFactor, int numPoints) {

    // �������
    float distance = std::sqrt(std::pow(endX - startX, 2) + std::pow(endY - startY, 2));

    // ���ݾ�����������Ӿ������Ƶ�����
    int controlPointsCount = CalculateControlPointsCount(distance, humanizationFactor);

    // ���ɿ��Ƶ�
    auto controlPoints = GenerateControlPoints(startX, startY, endX, endY, controlPointsCount, humanizationFactor);

    // ����ʵ�����ɵĵ��������50��
    int maxPoints = std::min(numPoints, 50);

    std::vector<std::pair<float, float>> pathPoints;
    if (numPoints <= 50) {
        // ��������
        for (int i = 0; i <= numPoints; ++i) {
            float t = static_cast<float>(i) / numPoints;
            pathPoints.push_back(BezierPoint(controlPoints, t));
        }
    }
    else {
        // ֻȡǰ��һС�Σ�t��0��maxPoints/numPoints
        for (int i = 0; i <= maxPoints; ++i) {
            float t = static_cast<float>(i) / numPoints;
            pathPoints.push_back(BezierPoint(controlPoints, t));
        }
    }

    return pathPoints;
}

// �����ٶ����ߣ������µ�·���� - ����Խ�󣬿�ʼ�졢������������ԽС���ٶ�Խ����
std::vector<std::pair<float, float>> HumanLikeMovement::ApplySpeedProfile(
    const std::vector<std::pair<float, float>>& points, int humanizationFactor) {

    if (points.size() < 2) return points;

    std::vector<std::pair<float, float>> speedAdjustedPoints;
    int desiredOutputPoints = points.size(); // �������������������ͬ

    // ����ԭʼ·�����ܳ���
    float totalDistance = 0.0f;
    std::vector<float> cumulativeDistances = { 0.0f };

    for (size_t i = 1; i < points.size(); i++) {
        float dx = points[i].first - points[i - 1].first;
        float dy = points[i].second - points[i - 1].second;
        float segmentDistance = std::sqrt(dx * dx + dy * dy);
        totalDistance += segmentDistance;
        cumulativeDistances.push_back(totalDistance);
    }

    // ���ھ�������ٶ����߲���������Խ��ָ��Խ�󣬿�ʼԽ�죬����Խ��
    // ��׼���������� (�����������뷶ΧΪ0-1000����)
    float normalizedDistance = std::min(1.0f, totalDistance / 1000.0f);

    // �ٶ����߲��� (2.0��3.0��Χ)
    // ����Խ��ָ��Խ�ӽ�3.0����������Խ���ͣ���ʼ�죬��������
    // ����ԽС��ָ��Խ�ӽ�2.0�����������ٶȸ�����
    float speedCurveExponent = 2.0f + normalizedDistance + (humanizationFactor / 100.0f);

    // ȷ����2.0-3.0��Χ��
    speedCurveExponent = std::max(2.0f, std::min(3.0f, speedCurveExponent));

    // ʹ�÷����Էֲ����²�����
    for (int i = 0; i < desiredOutputPoints; i++) {
        float t = static_cast<float>(i) / (desiredOutputPoints - 1);

        // ������ӳ�䣺�����ʱ����ʼ�죬������
        float adjustedT = 1.0f - std::pow(1.0f - t, speedCurveExponent);

        // �ҵ���Ӧ��λ��
        float targetDistance = adjustedT * totalDistance;

        // ���ֲ����ҵ������ԭʼ��
        auto it = std::lower_bound(cumulativeDistances.begin(), cumulativeDistances.end(), targetDistance);
        int segmentIndex = (it == cumulativeDistances.begin()) ? 0 : std::distance(cumulativeDistances.begin(), it) - 1;

        // �����ֵ����
        float segmentStartDist = cumulativeDistances[segmentIndex];
        float segmentEndDist = cumulativeDistances[segmentIndex + 1];
        float segmentProgress = (targetDistance - segmentStartDist) / (segmentEndDist - segmentStartDist);

        // ����segmentProgress��[0,1]��Χ��
        segmentProgress = std::max(0.0f, std::min(1.0f, segmentProgress));

        // ���Բ�ֵ�����µ�λ��
        float x = points[segmentIndex].first + segmentProgress * (points[segmentIndex + 1].first - points[segmentIndex].first);
        float y = points[segmentIndex].second + segmentProgress * (points[segmentIndex + 1].second - points[segmentIndex].second);

        speedAdjustedPoints.push_back(std::make_pair(x, y));
    }

    return speedAdjustedPoints;
}

std::pair<float, float> HumanLikeMovement::AddHumanJitter(
    float x, float y, float distance, int humanizationFactor) {
    // ȷ�� humanizationFactor �Ǹ�
    float hFactor = std::max(0.0f, static_cast<float>(humanizationFactor));
    // ���� maxJitter ��������Сֵ
    float maxJitter = std::max(0.0001f, std::min(3.0f, distance * 0.03f) * (hFactor / 50.0f));
    if (hFactor < 30) {
        maxJitter = std::max(0.0001f, maxJitter * (hFactor / 30.0f));
    }
    std::normal_distribution<float> jitterDist(0.0f, maxJitter / 3.0f);
    float jitterX = jitterDist(rng);
    float jitterY = jitterDist(rng);
    return { x + jitterX, y + jitterY };
}


// ���ɶ�ʧĿ��ʱ�����˻��ƶ�
std::vector<std::pair<float, float>> HumanLikeMovement::GenerateTargetLossMovement(
    float startX, float startY, int humanizationFactor, int steps) {

    std::vector<std::pair<float, float>> lossPath;

    // �����������
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
    std::uniform_real_distribution<float> distDist(5.0f, 20.0f + humanizationFactor * 0.3f);

    float angle = angleDist(rng);
    float distance = distDist(rng);

    // �����յ�
    float endX = startX + std::cos(angle) * distance;
    float endY = startY + std::sin(angle) * distance;

    // ���ɱ������ƶ�·��
    return GenerateBezierPath(startX, startY, endX, endY, humanizationFactor, steps);
}

int HumanLikeMovement::CalculateControlPointsCount(float distance, int humanizationFactor) {
    // �������Ƶ�����Ϊ2�������յ㣩
    int baseCount = 2;

    // ���ݾ������ӿ��Ƶ�
    int distancePoints = static_cast<int>(std::sqrt(distance) / 10);

    // ���������������ӿ��Ƶ�
    int humanPoints = humanizationFactor / 25;

    // �����ܿ��Ƶ�����
    return baseCount + distancePoints + humanPoints;
}

std::vector<std::pair<float, float>> HumanLikeMovement::GenerateControlPoints(
    float startX, float startY, float endX, float endY, int count, int humanizationFactor) {
    std::vector<std::pair<float, float>> controlPoints;
    controlPoints.push_back({ startX, startY });
    float deltaX = endX - startX;
    float deltaY = endY - startY;
    float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    // ȷ�� humanizationFactor �Ǹ�
    float hFactor = std::max(0.0f, static_cast<float>(humanizationFactor));
    // ���� maxOffset ��������Сֵ
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

    // �ݹ���㱴���������ϵĵ�
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