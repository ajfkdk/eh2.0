#include "LongRangeStrategy.h"
#include <cmath>
#include <algorithm>

namespace ActionModule {

    std::vector<AimPoint> LongRangeStrategy::CalculateAimingPath(
        int currentX, int currentY,
        int targetX, int targetY,
        HumanizeEngine* humanizer) {

        // ����ŷ�Ͼ���
        double distance = std::hypot(targetX - currentX, targetY - currentY);

        // Զ�������ʹ�õĲ���
        size_t steps = humanizer->CalculateStepsBasedOnDistance(currentX, currentY, targetX, targetY);

        // ���Ӳ���ȷ������ȷ���ƶ�
        steps = static_cast<size_t>(steps * 1.2);
        steps = std::clamp(steps, size_t(5), size_t(60));

        // ����·��
        std::vector<Point> path = humanizer->GenerateHumanizedPath(
            currentX, currentY, targetX, targetY, steps);

        // ת��ΪAimPoint
        std::vector<AimPoint> aimPath;
        aimPath.reserve(path.size());

        auto now = std::chrono::high_resolution_clock::now();
        double timePerStep = 1000.0 / 60.0; // ����60fps

        for (size_t i = 0; i < path.size(); ++i) {
            AimPoint point;
            point.x = static_cast<int>(std::round(path[i].x));
            point.y = static_cast<int>(std::round(path[i].y));

            // Ϊÿ�������һ��ʱ�����ģ��60fps�����
            auto timestamp = now + std::chrono::milliseconds(static_cast<long long>(i * timePerStep));
            point.timestamp = timestamp;

            aimPath.push_back(point);
        }

        return aimPath;
    }

    bool LongRangeStrategy::IsApplicableToScene(const std::string& scene) const {
        return scene == "long_range" || scene == "sniper";
    }

    std::string LongRangeStrategy::GetName() const {
        return "LongRangeStrategy";
    }

    bool LongRangeStrategy::Initialize() {
        return true;
    }

    void LongRangeStrategy::Cleanup() {
        // û����Ҫ�������Դ
    }

} // namespace ActionModule