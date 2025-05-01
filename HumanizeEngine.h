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

        // �������˻�ǿ�ȣ�1-100��
        void SetHumanizationLevel(int level);

        // �������˻��ƶ�·��
        std::vector<Point> GenerateHumanizedPath(
            double startX, double startY,
            double endX, double endY,
            size_t steps);

        // ��ȡ��ǰ���˻�����
        int GetHumanizationLevel() const;

        // ���΢С����������
        Point AddJitter(double x, double y);

        // ģ�ⷴӦ�ӳ�
        void SimulateReactionDelay();

        // ������ھ���Ĳ���
        size_t CalculateStepsBasedOnDistance(double startX, double startY,
            double endX, double endY) const;

    private:
        int humanizationLevel;  // 1-100
        std::mt19937 rng;       // �����������

        // ���������ߵ����
        Point CalculateBezierPoint(double t,
            const Point& start,
            const Point& control1,
            const Point& control2,
            const Point& end);

        // ���ɿ��Ƶ�
        std::pair<Point, Point> GenerateControlPoints(
            const Point& start,
            const Point& end,
            double curveFactor);

        // �����ٶ�����
        std::vector<double> CalculateVelocityProfile(size_t steps);
    };

} // namespace ActionModule

#endif // HUMANIZE_ENGINE_H