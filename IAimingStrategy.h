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

        // ������׼������
        virtual std::vector<AimPoint> CalculateAimingPath(
            int currentX, int currentY,
            int targetX, int targetY,
            HumanizeEngine* humanizer) = 0;

        // �ж��Ƿ��ʺϵ�ǰ����
        virtual bool IsApplicableToScene(const std::string& scene) const = 0;

        // ��ȡ��������
        virtual std::string GetName() const = 0;

        // ��ʼ������
        virtual bool Initialize() = 0;

        // ������Դ
        virtual void Cleanup() = 0;
    };

} // namespace ActionModule

#endif // I_AIMING_STRATEGY_H