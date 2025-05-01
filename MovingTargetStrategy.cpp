#include "MovingTargetStrategy.h"

namespace ActionModule {

    MovingTargetStrategy::MovingTargetStrategy() {
        longRangeStrategy.Initialize();
    }

    std::vector<AimPoint> MovingTargetStrategy::CalculateAimingPath(
        int currentX, int currentY,
        int targetX, int targetY,
        HumanizeEngine* humanizer) {

        // ¸´ÓÃLongRangeStrategyµÄÂß¼­
        return longRangeStrategy.CalculateAimingPath(currentX, currentY, targetX, targetY, humanizer);
    }

    bool MovingTargetStrategy::IsApplicableToScene(const std::string& scene) const {
        return scene == "moving_target" || scene == "running";
    }

    std::string MovingTargetStrategy::GetName() const {
        return "MovingTargetStrategy";
    }

    bool MovingTargetStrategy::Initialize() {
        return longRangeStrategy.Initialize();
    }

    void MovingTargetStrategy::Cleanup() {
        longRangeStrategy.Cleanup();
    }

} // namespace ActionModule