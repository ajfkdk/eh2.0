#ifndef LONG_RANGE_STRATEGY_H
#define LONG_RANGE_STRATEGY_H

#include "IAimingStrategy.h"

namespace ActionModule {

    class LongRangeStrategy : public IAimingStrategy {
    public:
        // ������׼������
        std::vector<AimPoint> CalculateAimingPath(
            int currentX, int currentY,
            int targetX, int targetY,
            HumanizeEngine* humanizer) override;

        // �ж��Ƿ��ʺϵ�ǰ����
        bool IsApplicableToScene(const std::string& scene) const override;

        // ��ȡ��������
        std::string GetName() const override;

        // ��ʼ������
        bool Initialize() override;

        // ������Դ
        void Cleanup() override;
    };

} // namespace ActionModule

#endif // LONG_RANGE_STRATEGY_H#pragma once
