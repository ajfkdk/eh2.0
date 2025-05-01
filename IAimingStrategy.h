#ifndef I_AIMING_STRATEGY_H
#define I_AIMING_STRATEGY_H

#include <string>
#include <vector>
#include <chrono>
#include "HumanizeEngine.h"

namespace ActionModule {

    struct AimPoint {
        int x;
        int y;
        std::chrono::high_resolution_clock::time_point timestamp;
    };

    class IAimingStrategy {
    public:
        virtual ~IAimingStrategy() = default;

        // 计算瞄准点序列
        virtual std::vector<AimPoint> CalculateAimingPath(
            int currentX, int currentY,
            int targetX, int targetY,
            HumanizeEngine* humanizer) = 0;

        // 判断是否适合当前场景
        virtual bool IsApplicableToScene(const std::string& scene) const = 0;

        // 获取策略名称
        virtual std::string GetName() const = 0;

        // 初始化策略
        virtual bool Initialize() = 0;

        // 清理资源
        virtual void Cleanup() = 0;
    };

} // namespace ActionModule

#endif // I_AIMING_STRATEGY_H