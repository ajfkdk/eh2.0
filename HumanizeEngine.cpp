#include "HumanizeEngine.h"
#include <cmath>
#include <algorithm>
#include <thread>

namespace ActionModule {
    HumanizeEngine::HumanizeEngine()
        : humanizationLevel(50),
        rng(std::random_device{}()) {
    }

    void HumanizeEngine::SetHumanizationLevel(int level) {
        humanizationLevel = std::clamp(level, 1, 100);//ʹ��std::clamp����������ֵ������1��100�ķ�Χ��
    }

    int HumanizeEngine::GetHumanizationLevel() const {
        return humanizationLevel;
    }

    /// <summary>
    /// �������GenerateHumanizedPath��������һ��ģ�������ƶ�����·��������㵽�յ㡣

   /* ��Ҫ���ܣ�

        ���δָ�������������֮ǰ��CalculateStepsBasedOnDistance���������ʵ��Ĳ���
        ȷ����������Ϊ2���������յ㣩
        �������˻���������������ӣ�����·���������̶�
        ���������յ����
        ���ɱ��������ߵ��������Ƶ㣬��Щ���Ƶ���������ߵ���״
        �����ٶ����ߣ�ģ�������ƶ��ٶȵı仯��������������������Ĺ��̣�
        ʹ�����α����������㷨����·���ϵĸ����㣺
        �����ٶ�����ȷ���Ǿ��ȵ�tֵ�����߲�����
        ����ÿ��tֵ��Ӧ�ı����������ϵĵ�
        ���м�����΢С�Ķ�����ģ�����ֲ��ȶ���*/
    /// </summary>
    /// <param name="startX"></param>
    /// <param name="startY"></param>
    /// <param name="endX"></param>
    /// <param name="endY"></param>
    /// <param name="steps"></param>
    /// <returns></returns>
    std::vector<Point> HumanizeEngine::GenerateHumanizedPath(
        double startX, double startY,
        double endX, double endY,
        size_t steps) {

        if (steps == 0) {
            steps = CalculateStepsBasedOnDistance(startX, startY, endX, endY);
        }

        // ȷ��������2��
        steps = std::max(size_t(2), steps);

        // ������������ (0.0 - 1.0)�������˻�����Ӱ��
        double curveFactor = humanizationLevel / 100.0 * 0.5;

        Point start{ startX, startY };
        Point end{ endX, endY };

        // ���ɿ��Ƶ�
        auto controlPoints = GenerateControlPoints(start, end, curveFactor);
        Point control1 = controlPoints.first;
        Point control2 = controlPoints.second;

        // ����·����
        std::vector<Point> path;
        path.reserve(steps);

        // �����ٶ�����
        std::vector<double> velocities = CalculateVelocityProfile(steps);

        // ���ɱ����������ϵĵ�
        double cumulativeDistance = 0.0;
        for (size_t i = 0; i < steps; ++i) {
            // �Ǿ���tֵ�������ٶ�����
            cumulativeDistance += velocities[i];
            // ȷ��tֵ��[0,1]��Χ��
            double t = std::min(1.0, cumulativeDistance);

            Point point = CalculateBezierPoint(t, start, control1, control2, end);

            // ���΢�����������������˻�����仯
            if (i > 0 && i < steps - 1) {
                point = AddJitter(point.x, point.y);
            }

            path.push_back(point);
        }

        // ȷ�����һ������Ŀ���
        if (!path.empty()) {
            path.back() = end;
        }

        return path;
    }

    Point HumanizeEngine::AddJitter(double x, double y) {
        // ������Χ�����˻�����仯
        double jitterAmount = 0.1 + (humanizationLevel / 100.0) * 0.9;

        std::normal_distribution<double> dist(0.0, jitterAmount);

        return Point{
            x + dist(rng),
            y + dist(rng)
        };
    }

    void HumanizeEngine::SimulateReactionDelay() {
        // ������Ӧʱ��100ms�������ӳ������˻�����仯
        int additionalDelay = static_cast<int>(humanizationLevel / 100.0 * 50.0); // �������50ms
        int delayMs = 100 + additionalDelay;

        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }

    /// <summary>
    /// �����һ�����ƶ�����һ����ʱ��Ҫ�Ĳ����������������˻�����

    /*���幦�ܣ�

        ��������������꣺���(startX, startY)���յ�(endX, endY)
        ʹ��ŷ����þ��빫ʽ(std::hypot)����������ֱ�߾���
        ����������ת��Ϊ��������������������ÿ10����һ��
        �������humanizationLevel��Ա�������Ӳ�����
        ���˻�����Ϊ0ʱ������Ϊ1.0�������ӣ�
        ���˻�����Ϊ100ʱ������Ϊ1.5������50 % ��
        �м�ֵ����������
        ���ȷ��������2��60֮�䣬�Ȳ���̫��Ҳ�������*/
    /// </summary>
    /// <param name="startX"></param>
    /// <param name="startY"></param>
    /// <param name="endX"></param>
    /// <param name="endY"></param>
    /// <returns></returns>
    size_t HumanizeEngine::CalculateStepsBasedOnDistance(
        double startX, double startY,
        double endX, double endY) const {

        // ����ŷ�Ͼ���
        double distance = std::hypot(endX - startX, endY - startY);

        // ����������ÿ10����һ����
        size_t baseSteps = static_cast<size_t>(distance / 10.0);

        // �������˻��������Ӳ���
        double stepMultiplier = 1.0 + (humanizationLevel / 100.0) * 0.5; // �������50%

        // ȷ��������2�������60��
        return std::clamp(
            static_cast<size_t>(baseSteps * stepMultiplier),
            size_t(2),
            size_t(60)
        );
    }

    Point HumanizeEngine::CalculateBezierPoint(
        double t,
        const Point& start,
        const Point& control1,
        const Point& control2,
        const Point& end) {

        double t2 = t * t;
        double t3 = t2 * t;
        double mt = 1.0 - t;
        double mt2 = mt * mt;
        double mt3 = mt2 * mt;

        return Point{
            start.x * mt3 + 3.0 * control1.x * mt2 * t + 3.0 * control2.x * mt * t2 + end.x * t3,
            start.y * mt3 + 3.0 * control1.y * mt2 * t + 3.0 * control2.y * mt * t2 + end.y * t3
        };
    }

    std::pair<Point, Point> HumanizeEngine::GenerateControlPoints(
        const Point& start,
        const Point& end,
        double curveFactor) {

        // ����·������
        double dx = end.x - start.x;
        double dy = end.y - start.y;

        // �����������ſ��Ƶ�
        double distance = std::hypot(dx, dy);

        // ���Ƶ����������ƫ��
        std::uniform_real_distribution<double> dist(-curveFactor, curveFactor);
        double offset1 = dist(rng) * distance;
        double offset2 = dist(rng) * distance;

        // ��һ�����Ƶ�
        Point control1{
            start.x + dx * 0.3,
            start.y + dy * 0.3 + offset1
        };

        // �ڶ������Ƶ�
        Point control2{
            start.x + dx * 0.7,
            start.y + dy * 0.7 + offset2
        };

        return { control1, control2 };
    }

    std::vector<double> HumanizeEngine::CalculateVelocityProfile(size_t steps) {
        std::vector<double> velocities(steps);

        if (steps <= 1) {
            // ���ֻ��һ��������һ����Ч���ٶ�ֵ
            if (steps == 1) {
                velocities[0] = 1.0;
            }
            return velocities;
        }

        // �������˻���������ٶ�����
        double accelRatio = 0.3 - (humanizationLevel / 100.0) * 0.1;  // ���ٶα���
        double decelRatio = 0.3 - (humanizationLevel / 100.0) * 0.1;  // ���ٶα���

        // ȷ��accelRatio + decelRatio <= 0.9
        if (accelRatio + decelRatio > 0.9) {
            double sum = accelRatio + decelRatio;
            accelRatio = 0.9 * accelRatio / sum;
            decelRatio = 0.9 * decelRatio / sum;
        }

        size_t accelEnd = static_cast<size_t>(steps * accelRatio);
        size_t decelStart = static_cast<size_t>(steps * (1.0 - decelRatio));

        // ȷ��decelStart > accelEnd
        if (decelStart <= accelEnd) {
            decelStart = accelEnd + 1;
        }
        // ȷ����Խ��
        decelStart = std::min(decelStart, steps - 1);

        // ��С�ٶȺ�����ٶȱ���
        double minVelocity = 0.5;
        double maxVelocity = 1.0;

        // ������׶ε��ٶ�����
        for (size_t i = 0; i < steps; ++i) {
            if (i < accelEnd) {
                // ���ٽ׶� - ʹ��ƽ������������
                double ratio = static_cast<double>(i) / accelEnd;
                velocities[i] = minVelocity + (maxVelocity - minVelocity) * std::sqrt(ratio);
            }
            else if (i >= decelStart) {
                // ���ٽ׶� - ʹ�ö��μ�������
                double ratio = static_cast<double>(i - decelStart) / (steps - decelStart);
                velocities[i] = maxVelocity - (maxVelocity - minVelocity) * (ratio * ratio);
            }
            else {
                // ���ٽ׶�
                velocities[i] = maxVelocity;
            }
        }

        // �����ܲ���
        double totalDistance = 0.0;
        for (double v : velocities) {
            totalDistance += v;
        }

        // ��һ����ȷ���ܲ���Ϊ1.0
        if (totalDistance > 0.0) {
            for (double& v : velocities) {
                v /= totalDistance;
            }
        }

        return velocities;
    }

} // namespace ActionModule