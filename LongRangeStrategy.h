#ifndef LONG_RANGE_STRATEGY_H
#define LONG_RANGE_STRATEGY_H

#include "IAimingStrategy.h"

namespace ActionModule {

    class LongRangeStrategy : public IAimingStrategy {
    public:
        // 计算瞄准点序列
        std::vector<AimPoint> CalculateAimingPath(
            int currentX, int currentY,
            int targetX, int targetY,
            HumanizeEngine* humanizer) override;

        // 判断是否适合当前场景
        bool IsApplicableToScene(const std::string& scene) const override;

        // 获取策略名称
        std::string GetName() const override;

        // 初始化策略
        bool Initialize() override;

        // 清理资源
        void Cleanup() override;
    };

} // namespace ActionModule

#endif // LONG_RANGE_STRATEGY_H#pragma once
