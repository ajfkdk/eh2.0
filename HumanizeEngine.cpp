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
        humanizationLevel = std::clamp(level, 1, 100);//使用std::clamp函数将输入值限制在1到100的范围内
    }

    int HumanizeEngine::GetHumanizationLevel() const {
        return humanizationLevel;
    }

    /// <summary>
    /// 这个方法GenerateHumanizedPath用于生成一条模拟人类移动鼠标的路径，从起点到终点。

   /* 主要功能：

        如果未指定步数，会调用之前的CalculateStepsBasedOnDistance方法计算适当的步数
        确保步数至少为2（有起点和终点）
        根据拟人化级别计算曲率因子，决定路径的弯曲程度
        创建起点和终点对象
        生成贝塞尔曲线的两个控制点，这些控制点决定了曲线的形状
        计算速度曲线，模拟人类移动速度的变化（可能是先慢后快再慢的过程）
        使用三次贝塞尔曲线算法生成路径上的各个点：
        根据速度曲线确定非均匀的t值（曲线参数）
        计算每个t值对应的贝塞尔曲线上的点
        对中间点添加微小的抖动，模拟人手不稳定性*/
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

        // 确保至少有2步
        steps = std::max(size_t(2), steps);

        // 计算曲率因子 (0.0 - 1.0)，受拟人化级别影响
        double curveFactor = humanizationLevel / 100.0 * 0.5;

        Point start{ startX, startY };
        Point end{ endX, endY };

        // 生成控制点
        auto controlPoints = GenerateControlPoints(start, end, curveFactor);
        Point control1 = controlPoints.first;
        Point control2 = controlPoints.second;

        // 生成路径点
        std::vector<Point> path;
        path.reserve(steps);

        // 计算速度曲线
        std::vector<double> velocities = CalculateVelocityProfile(steps);

        // 生成贝塞尔曲线上的点
        double cumulativeDistance = 0.0;
        for (size_t i = 0; i < steps; ++i) {
            // 非均匀t值，基于速度曲线
            cumulativeDistance += velocities[i];
            // 确保t值在[0,1]范围内
            double t = std::min(1.0, cumulativeDistance);

            Point point = CalculateBezierPoint(t, start, control1, control2, end);

            // 添加微抖动，抖动量随拟人化级别变化
            if (i > 0 && i < steps - 1) {
                point = AddJitter(point.x, point.y);
            }

            path.push_back(point);
        }

        // 确保最后一个点是目标点
        if (!path.empty()) {
            path.back() = end;
        }

        return path;
    }

    Point HumanizeEngine::AddJitter(double x, double y) {
        // 抖动范围随拟人化级别变化
        double jitterAmount = 0.1 + (humanizationLevel / 100.0) * 0.9;

        std::normal_distribution<double> dist(0.0, jitterAmount);

        return Point{
            x + dist(rng),
            y + dist(rng)
        };
    }

    void HumanizeEngine::SimulateReactionDelay() {
        // 基础反应时间100ms，额外延迟随拟人化级别变化
        int additionalDelay = static_cast<int>(humanizationLevel / 100.0 * 50.0); // 最多增加50ms
        int delayMs = 100 + additionalDelay;

        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }

    /// <summary>
    /// 计算从一个点移动到另一个点时需要的步数，并考虑了拟人化级别。

    /*具体功能：

        接收两个点的坐标：起点(startX, startY)和终点(endX, endY)
        使用欧几里得距离公式(std::hypot)计算两点间的直线距离
        初步将距离转换为基础步数，基本规则是每10像素一步
        根据类的humanizationLevel成员变量增加步数：
        拟人化级别为0时，乘数为1.0（不增加）
        拟人化级别为100时，乘数为1.5（增加50 % ）
        中间值按比例计算
        最后确保步数在2到60之间，既不会太少也不会过多*/
    /// </summary>
    /// <param name="startX"></param>
    /// <param name="startY"></param>
    /// <param name="endX"></param>
    /// <param name="endY"></param>
    /// <returns></returns>
    size_t HumanizeEngine::CalculateStepsBasedOnDistance(
        double startX, double startY,
        double endX, double endY) const {

        // 计算欧氏距离
        double distance = std::hypot(endX - startX, endY - startY);

        // 基础步数（每10像素一步）
        size_t baseSteps = static_cast<size_t>(distance / 10.0);

        // 根据拟人化级别增加步数
        double stepMultiplier = 1.0 + (humanizationLevel / 100.0) * 0.5; // 最多增加50%

        // 确保至少有2步，最多60步
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

        // 基础路径向量
        double dx = end.x - start.x;
        double dy = end.y - start.y;

        // 距离用于缩放控制点
        double distance = std::hypot(dx, dy);

        // 控制点向量的随机偏移
        std::uniform_real_distribution<double> dist(-curveFactor, curveFactor);
        double offset1 = dist(rng) * distance;
        double offset2 = dist(rng) * distance;

        // 第一个控制点
        Point control1{
            start.x + dx * 0.3,
            start.y + dy * 0.3 + offset1
        };

        // 第二个控制点
        Point control2{
            start.x + dx * 0.7,
            start.y + dy * 0.7 + offset2
        };

        return { control1, control2 };
    }

    std::vector<double> HumanizeEngine::CalculateVelocityProfile(size_t steps) {
        std::vector<double> velocities(steps);

        if (steps <= 1) {
            // 如果只有一步，给它一个有效的速度值
            if (steps == 1) {
                velocities[0] = 1.0;
            }
            return velocities;
        }

        // 根据拟人化级别调整速度曲线
        double accelRatio = 0.3 - (humanizationLevel / 100.0) * 0.1;  // 加速段比例
        double decelRatio = 0.3 - (humanizationLevel / 100.0) * 0.1;  // 减速段比例

        // 确保accelRatio + decelRatio <= 0.9
        if (accelRatio + decelRatio > 0.9) {
            double sum = accelRatio + decelRatio;
            accelRatio = 0.9 * accelRatio / sum;
            decelRatio = 0.9 * decelRatio / sum;
        }

        size_t accelEnd = static_cast<size_t>(steps * accelRatio);
        size_t decelStart = static_cast<size_t>(steps * (1.0 - decelRatio));

        // 确保decelStart > accelEnd
        if (decelStart <= accelEnd) {
            decelStart = accelEnd + 1;
        }
        // 确保不越界
        decelStart = std::min(decelStart, steps - 1);

        // 最小速度和最大速度比例
        double minVelocity = 0.5;
        double maxVelocity = 1.0;

        // 计算各阶段的速度曲线
        for (size_t i = 0; i < steps; ++i) {
            if (i < accelEnd) {
                // 加速阶段 - 使用平方根加速曲线
                double ratio = static_cast<double>(i) / accelEnd;
                velocities[i] = minVelocity + (maxVelocity - minVelocity) * std::sqrt(ratio);
            }
            else if (i >= decelStart) {
                // 减速阶段 - 使用二次减速曲线
                double ratio = static_cast<double>(i - decelStart) / (steps - decelStart);
                velocities[i] = maxVelocity - (maxVelocity - minVelocity) * (ratio * ratio);
            }
            else {
                // 匀速阶段
                velocities[i] = maxVelocity;
            }
        }

        // 计算总步长
        double totalDistance = 0.0;
        for (double v : velocities) {
            totalDistance += v;
        }

        // 归一化，确保总步长为1.0
        if (totalDistance > 0.0) {
            for (double& v : velocities) {
                v /= totalDistance;
            }
        }

        return velocities;
    }

} // namespace ActionModule