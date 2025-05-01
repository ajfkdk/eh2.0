#ifndef CLOSE_RANGE_STRATEGY_H
#define CLOSE_RANGE_STRATEGY_H

#include "IAimingStrategy.h"
#include "LongRangeStrategy.h"

namespace ActionModule {

    class CloseRangeStrategy : public IAimingStrategy {
    public:
        CloseRangeStrategy();

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

    private:
        LongRangeStrategy longRangeStrategy;
    };

} // namespace ActionModule

#endif // CLOSE_RANGE_STRATEGY_H#pragma once
