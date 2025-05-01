#ifndef VELOCITY_PROFILE_H
#define VELOCITY_PROFILE_H

#include <vector>
#include <string>

namespace ActionModule {

    class VelocityProfile {
    public:
        // �����ٶ�����
        static std::vector<double> Generate(
            size_t steps,
            const std::string& profileType,
            int humanizationLevel
        );

        // Ԥ������ٶ���������
        static const std::string SNIPER_PROFILE;     // �ѻ���ģʽ����-��-��
        static const std::string TRACKING_PROFILE;   // ׷��ģʽ���е�-�ȶ�
        static const std::string FLICK_PROFILE;      // ����˦ǹ����-��
        static const std::string SMOOTH_PROFILE;     // ƽ��ģʽ�����ȼӼ���

    private:
        // ���ɾѻ����ٶ�����
        static std::vector<double> GenerateSniperProfile(size_t steps, int humanizationLevel);

        // ����׷���ٶ�����
        static std::vector<double> GenerateTrackingProfile(size_t steps, int humanizationLevel);

        // ���ɿ���˦ǹ�ٶ�����
        static std::vector<double> GenerateFlickProfile(size_t steps, int humanizationLevel);

        // ����ƽ���ٶ�����
        static std::vector<double> GenerateSmoothProfile(size_t steps, int humanizationLevel);

        // ��һ���ٶ�����
        static void NormalizeVelocities(std::vector<double>& velocities);
    };

} // namespace ActionModule

#endif // VELOCITY_PROFILE_H