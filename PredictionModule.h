// PredictionModule.h
#ifndef PREDICTION_MODULE_H
#define PREDICTION_MODULE_H

#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include "DetectionModule.h" // 引入DetectionResult结构
#include "RingBuffer.h" // 引入环形缓冲区

// 预测结果结构体
struct PredictionResult {
    int x;      // 预测的目标X坐标 (999表示目标丢失)
    int y;      // 预测的目标Y坐标 (999表示目标丢失)
    std::chrono::high_resolution_clock::time_point timestamp; // 时间戳
};

// 卡尔曼滤波参数(可动态调整)
struct KFParams {
    float q_pos = 1e-3;      // 位置过程噪声
    float q_vel = 1e-2;      // 速度过程噪声
    float r_meas = 1e-1;     // 测量噪声
    float gate_th = 30.0;    // 关联距离阈值
    int lost_max = 10;       // 最大丢失帧数 
    int lock_keep = 5;       // 锁定轨迹保持帧数
    int switch_hys = 5;      // 切换轨迹滞后帧数
    float stab_min = 0.5;    // 最低稳定度阈值
    float pred_time = 0.05;  // 预测时间(秒)
};

// 轨迹类定义
struct Track {
    int id;                  // 唯一ID
    cv::KalmanFilter kf;     // 卡尔曼滤波器
    int age;                 // 存活帧数
    int lost;                // 连续未匹配帧数
    float stability;         // 稳定性评分
    int classId;             // 目标类别

    cv::Point2f predicted;   // 预测点
    cv::Point2f lastPos;     // 上次观测
    float score;             // 置信度

    Track() : kf(4, 2, 0), id(-1), age(0), lost(0), stability(0.0f),
        classId(-1), predicted(0, 0), lastPos(0, 0), score(0.0f) {}
};

// 匈牙利算法类 (用于数据关联)
class HungarianAlgorithm {
public:
    static void Solve(const cv::Mat& costMatrix, std::vector<int>& assignment);
};

// 轨迹管理类
class TrackManager {
private:
    std::vector<Track> tracks;          // 活跃轨迹
    int nextTrackId = 0;                // 下一个轨迹ID
    int lockedTrackId = -1;             // 当前锁定轨迹
    int lockCounter = 0;                // 锁定计数器
    KFParams params;                    // 卡尔曼滤波参数

    // 初始化卡尔曼滤波器
    void initKalmanFilter(cv::KalmanFilter& kf, const cv::Point2f& initialPos);

    // 设置卡尔曼滤波器的噪声参数
    void setKalmanNoiseParams(cv::KalmanFilter& kf);

    // 数据关联
    void associateDetectionsToTracks(const std::vector<DetectionResult>& detections,
        std::vector<std::pair<int, int>>& assignments,
        std::vector<int>& unassignedTracks,
        std::vector<int>& unassignedDetections);

    // 锁定策略更新
    void updateLockedTrack();

public:
    TrackManager();

    // 设置参数
    void setParams(const KFParams& p);

    // 获取参数
    KFParams getParams() const;

    // 处理一帧检测结果
    void processFrame(const std::vector<DetectionResult>& detections);

    // 获取预测结果
    PredictionResult getPrediction();

    // 获取调试信息
    std::vector<Track> getTracks() const;
    int getLockedTrackId() const;
};

namespace PredictionModule {
    // 初始化预测模块，返回工作线程
    std::thread Initialize();

    // 检查预测模块是否在运行
    bool IsRunning();

    // 停止预测模块
    void Stop();

    // 清理资源
    void Cleanup();

    // 获取最新预测结果
    bool GetLatestPrediction(PredictionResult& result);

    // 设置调试模式
    void SetDebugMode(bool enabled);

    // 获取调试模式状态
    bool IsDebugModeEnabled();

    // 卡尔曼滤波参数设置函数
    void SetKalmanFilterParams(const KFParams& params);

    // 获取当前卡尔曼滤波参数
    KFParams GetKalmanFilterParams();

    // 这些函数保留为接口兼容，但不再使用
    void SetGFactor(float g);
    void SetHFactor(float h);
    void SetPredictionTime(float seconds);
}

#endif // PREDICTION_MODULE_H