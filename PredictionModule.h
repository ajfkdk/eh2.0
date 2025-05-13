// PredictionModule.h
#ifndef PREDICTION_MODULE_H
#define PREDICTION_MODULE_H

#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include "DetectionModule.h" // ����DetectionResult�ṹ
#include "RingBuffer.h" // ���뻷�λ�����

// Ԥ�����ṹ��
struct PredictionResult {
    int x;      // Ԥ���Ŀ��X���� (999��ʾĿ�궪ʧ)
    int y;      // Ԥ���Ŀ��Y���� (999��ʾĿ�궪ʧ)
    std::chrono::high_resolution_clock::time_point timestamp; // ʱ���
};

// �������˲�����(�ɶ�̬����)
struct KFParams {
    float q_pos = 1e-3;      // λ�ù�������
    float q_vel = 1e-2;      // �ٶȹ�������
    float r_meas = 1e-1;     // ��������
    float gate_th = 30.0;    // ����������ֵ
    int lost_max = 10;       // ���ʧ֡�� 
    int lock_keep = 5;       // �����켣����֡��
    int switch_hys = 5;      // �л��켣�ͺ�֡��
    float stab_min = 0.5;    // ����ȶ�����ֵ
    float pred_time = 0.05;  // Ԥ��ʱ��(��)
};

// �켣�ඨ��
struct Track {
    int id;                  // ΨһID
    cv::KalmanFilter kf;     // �������˲���
    int age;                 // ���֡��
    int lost;                // ����δƥ��֡��
    float stability;         // �ȶ�������
    int classId;             // Ŀ�����

    cv::Point2f predicted;   // Ԥ���
    cv::Point2f lastPos;     // �ϴι۲�
    float score;             // ���Ŷ�

    Track() : kf(4, 2, 0), id(-1), age(0), lost(0), stability(0.0f),
        classId(-1), predicted(0, 0), lastPos(0, 0), score(0.0f) {}
};

// �������㷨�� (�������ݹ���)
class HungarianAlgorithm {
public:
    static void Solve(const cv::Mat& costMatrix, std::vector<int>& assignment);
};

// �켣������
class TrackManager {
private:
    std::vector<Track> tracks;          // ��Ծ�켣
    int nextTrackId = 0;                // ��һ���켣ID
    int lockedTrackId = -1;             // ��ǰ�����켣
    int lockCounter = 0;                // ����������
    KFParams params;                    // �������˲�����

    // ��ʼ���������˲���
    void initKalmanFilter(cv::KalmanFilter& kf, const cv::Point2f& initialPos);

    // ���ÿ������˲�������������
    void setKalmanNoiseParams(cv::KalmanFilter& kf);

    // ���ݹ���
    void associateDetectionsToTracks(const std::vector<DetectionResult>& detections,
        std::vector<std::pair<int, int>>& assignments,
        std::vector<int>& unassignedTracks,
        std::vector<int>& unassignedDetections);

    // �������Ը���
    void updateLockedTrack();

public:
    TrackManager();

    // ���ò���
    void setParams(const KFParams& p);

    // ��ȡ����
    KFParams getParams() const;

    // ����һ֡�����
    void processFrame(const std::vector<DetectionResult>& detections);

    // ��ȡԤ����
    PredictionResult getPrediction();

    // ��ȡ������Ϣ
    std::vector<Track> getTracks() const;
    int getLockedTrackId() const;
};

namespace PredictionModule {
    // ��ʼ��Ԥ��ģ�飬���ع����߳�
    std::thread Initialize();

    // ���Ԥ��ģ���Ƿ�������
    bool IsRunning();

    // ֹͣԤ��ģ��
    void Stop();

    // ������Դ
    void Cleanup();

    // ��ȡ����Ԥ����
    bool GetLatestPrediction(PredictionResult& result);

    // ���õ���ģʽ
    void SetDebugMode(bool enabled);

    // ��ȡ����ģʽ״̬
    bool IsDebugModeEnabled();

    // �������˲��������ú���
    void SetKalmanFilterParams(const KFParams& params);

    // ��ȡ��ǰ�������˲�����
    KFParams GetKalmanFilterParams();

    // ��Щ��������Ϊ�ӿڼ��ݣ�������ʹ��
    void SetGFactor(float g);
    void SetHFactor(float h);
    void SetPredictionTime(float seconds);
}

#endif // PREDICTION_MODULE_H