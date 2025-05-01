#include "MidRangeStrategy.h"

namespace ActionModule {

    MidRangeStrategy::MidRangeStrategy() {
        longRangeStrategy.Initialize();
    }

    std::vector<AimPoint> MidRangeStrategy::CalculateAimingPath(
        int currentX, int currentY,
        int targetX, int targetY,
        HumanizeEngine* humanizer) {

        // ¸´ÓÃLongRangeStrategyµÄÂß¼­
        return longRangeStrategy.CalculateAimingPath(currentX, currentY, targetX, targetY, humanizer);
    }

    bool MidRangeStrategy::IsApplicableToScene(const std::string& scene) const {
        return scene == "mid_range" || scene == "assault";
    }

    std::string MidRangeStrategy::GetName() const {
        return "MidRangeStrategy";
    }

    bool MidRangeStrategy::Initialize() {
        return longRangeStrategy.Initialize();
    }

    void MidRangeStrategy::Cleanup() {
        longRangeStrategy.Cleanup();
    }

} // namespace ActionModule