#include "MultiTargetStrategy.h"

namespace ActionModule {

    MultiTargetStrategy::MultiTargetStrategy() {
        longRangeStrategy.Initialize();
    }

    std::vector<AimPoint> MultiTargetStrategy::CalculateAimingPath(
        int currentX, int currentY,
        int targetX, int targetY,
        HumanizeEngine* humanizer) {

        // ¸´ÓÃLongRangeStrategyµÄÂß¼­
        return longRangeStrategy.CalculateAimingPath(currentX, currentY, targetX, targetY, humanizer);
    }

    bool MultiTargetStrategy::IsApplicableToScene(const std::string& scene) const {
        return scene == "multi_target" || scene == "group";
    }

    std::string MultiTargetStrategy::GetName() const {
        return "MultiTargetStrategy";
    }

    bool MultiTargetStrategy::Initialize() {
        return longRangeStrategy.Initialize();
    }

    void MultiTargetStrategy::Cleanup() {
        longRangeStrategy.Cleanup();
    }

} // namespace ActionModule