#ifndef TRAJECTORY_GENERATOR_H
#define TRAJECTORY_GENERATOR_H

#include <vector>
#include "HumanizeEngine.h"

namespace ActionModule {

    class TrajectoryGenerator {
    public:
        // 根据场景和拟人化设置生成路径
        static std::vector<Point> GenerateTrajectory(
            const Point& start,
            const Point& end,
            const std::string& scene,
            HumanizeEngine* humanizer
        );

        // 生成针对移动目标的预判路径
        static std::vector<Point> GenerateMovingTargetTrajectory(
            const Point& start,
            const Point& end,
            const Point& targetVelocity,
            HumanizeEngine* humanizer
        );

        // 生成多目标切换的路径
        static std::vector<Point> GenerateMultiTargetTrajectory(
            const Point& start,
            const std::vector<Point>& targets,
            HumanizeEngine* humanizer
        );

    private:
        // 根据场景调整曲率
        static double GetCurvatureForScene(const std::string& scene);

        // 根据场景调整步数
        static size_t GetStepsForScene(const std::string& scene, double distance);
    };

} // namespace ActionModule

#endif // TRAJECTORY_GENERATOR_H#pragma once
