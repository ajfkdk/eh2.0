#ifndef TRAJECTORY_GENERATOR_H
#define TRAJECTORY_GENERATOR_H

#include <vector>
#include "HumanizeEngine.h"

namespace ActionModule {

    class TrajectoryGenerator {
    public:
        // ���ݳ��������˻���������·��
        static std::vector<Point> GenerateTrajectory(
            const Point& start,
            const Point& end,
            const std::string& scene,
            HumanizeEngine* humanizer
        );

        // ��������ƶ�Ŀ���Ԥ��·��
        static std::vector<Point> GenerateMovingTargetTrajectory(
            const Point& start,
            const Point& end,
            const Point& targetVelocity,
            HumanizeEngine* humanizer
        );

        // ���ɶ�Ŀ���л���·��
        static std::vector<Point> GenerateMultiTargetTrajectory(
            const Point& start,
            const std::vector<Point>& targets,
            HumanizeEngine* humanizer
        );

    private:
        // ���ݳ�����������
        static double GetCurvatureForScene(const std::string& scene);

        // ���ݳ�����������
        static size_t GetStepsForScene(const std::string& scene, double distance);
    };

} // namespace ActionModule

#endif // TRAJECTORY_GENERATOR_H#pragma once
