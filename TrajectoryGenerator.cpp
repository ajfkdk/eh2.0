#ifndef VELOCITY_PROFILE_H
#define VELOCITY_PROFILE_H

#include <vector>
#include <string>

namespace ActionModule {

    class VelocityProfile {
    public:
        // 生成速度曲线
        static std::vector<double> Generate(
            size_t steps,
            const std::string& profileType,
            int humanizationLevel
        );

        // 预定义的速度曲线类型
        static const std::string SNIPER_PROFILE;     // 狙击手模式：慢-快-慢
        static const std::string TRACKING_PROFILE;   // 追踪模式：中等-稳定
        static const std::string FLICK_PROFILE;      // 快速甩枪：快-慢
        static const std::string SMOOTH_PROFILE;     // 平滑模式：均匀加减速

    private:
        // 生成狙击手速度曲线
        static std::vector<double> GenerateSniperProfile(size_t steps, int humanizationLevel);

        // 生成追踪速度曲线
        static std::vector<double> GenerateTrackingProfile(size_t steps, int humanizationLevel);

        // 生成快速甩枪速度曲线
        static std::vector<double> GenerateFlickProfile(size_t steps, int humanizationLevel);

        // 生成平滑速度曲线
        static std::vector<double> GenerateSmoothProfile(size_t steps, int humanizationLevel);

        // 归一化速度曲线
        static void NormalizeVelocities(std::vector<double>& velocities);
    };

} // namespace ActionModule

#endif // VELOCITY_PROFILE_H