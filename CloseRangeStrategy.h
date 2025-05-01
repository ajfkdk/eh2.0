#ifndef CLOSE_RANGE_STRATEGY_H
#define CLOSE_RANGE_STRATEGY_H

#include "IAimingStrategy.h"
#include "LongRangeStrategy.h"

namespace ActionModule {

    class CloseRangeStrategy : public IAimingStrategy {
    public:
        CloseRangeStrategy();

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

    private:
        LongRangeStrategy longRangeStrategy;
    };

} // namespace ActionModule

#endif // CLOSE_RANGE_STRATEGY_H#pragma once
