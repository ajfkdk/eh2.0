#include "LongRangeStrategy.h"
#include <cmath>
#include <algorithm>

namespace ActionModule {

    std::vector<AimPoint> LongRangeStrategy::CalculateAimingPath(
        int currentX, int currentY,
        int targetX, int targetY,
        HumanizeEngine* humanizer) {

        // 计算欧氏距离
        double distance = std::hypot(targetX - currentX, targetY - currentY);

        // 远距离策略使用的步数
        size_t steps = humanizer->CalculateStepsBasedOnDistance(currentX, currentY, targetX, targetY);

        // 增加步数确保更精确的移动
        steps = static_cast<size_t>(steps * 1.2);
        steps = std::clamp(steps, size_t(5), size_t(60));

        // 生成路径
        std::vector<Point> path = humanizer->GenerateHumanizedPath(
            currentX, currentY, targetX, targetY, steps);

        // 转换为AimPoint
        std::vector<AimPoint> aimPath;
        aimPath.reserve(path.size());

        auto now = std::chrono::high_resolution_clock::now();
        double timePerStep = 1000.0 / 60.0; // 假设60fps

        for (size_t i = 0; i < path.size(); ++i) {
            AimPoint point;
            point.x = static_cast<int>(std::round(path[i].x));
            point.y = static_cast<int>(std::round(path[i].y));

            // 为每个点分配一个时间戳，模拟60fps的输出
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
        // 没有需要清理的资源
    }

} // namespace ActionModule