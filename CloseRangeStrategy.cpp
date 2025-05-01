#include "CloseRangeStrategy.h"

namespace ActionModule {

    CloseRangeStrategy::CloseRangeStrategy() {
        longRangeStrategy.Initialize();
    }

    std::vector<AimPoint> CloseRangeStrategy::CalculateAimingPath(
        int currentX, int currentY,
        int targetX, int targetY,
        HumanizeEngine* humanizer) {

        // ¸´ÓÃLongRangeStrategyµÄÂß¼­
        return longRangeStrategy.CalculateAimingPath(currentX, currentY, targetX, targetY, humanizer);
    }

    bool CloseRangeStrategy::IsApplicableToScene(const std::string& scene) const {
        return scene == "close_range" || scene == "shotgun";
    }

    std::string CloseRangeStrategy::GetName() const {
        return "CloseRangeStrategy";
    }

    bool CloseRangeStrategy::Initialize() {
        return longRangeStrategy.Initialize();
    }

    void CloseRangeStrategy::Cleanup() {
        longRangeStrategy.Cleanup();
    }

} // namespace ActionModule