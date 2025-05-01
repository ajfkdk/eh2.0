#ifndef HUMANIZE_ENGINE_H
#define HUMANIZE_ENGINE_H

#include <vector>
#include <random>
#include <chrono>

namespace ActionModule {

    struct Point {
        double x;
        double y;
    };

    class HumanizeEngine {
    public:
        HumanizeEngine();

        // 设置拟人化强度（1-100）
        void SetHumanizationLevel(int level);

        // 生成拟人化移动路径
        std::vector<Point> GenerateHumanizedPath(
            double startX, double startY,
            double endX, double endY,
            size_t steps);

        // 获取当前拟人化级别
        int GetHumanizationLevel() const;

        // 添加微小抖动到坐标
        Point AddJitter(double x, double y);

        // 模拟反应延迟
        void SimulateReactionDelay();

        // 计算基于距离的步数
        size_t CalculateStepsBasedOnDistance(double startX, double startY,
            double endX, double endY) const;

    private:
        int humanizationLevel;  // 1-100
        std::mt19937 rng;       // 随机数生成器

        // 贝塞尔曲线点计算
        Point CalculateBezierPoint(double t,
            const Point& start,
            const Point& control1,
            const Point& control2,
            const Point& end);

        // 生成控制点
        std::pair<Point, Point> GenerateControlPoints(
            const Point& start,
            const Point& end,
            double curveFactor);

        // 计算速度曲线
        std::vector<double> CalculateVelocityProfile(size_t steps);
    };

} // namespace ActionModule

#endif // HUMANIZE_ENGINE_H