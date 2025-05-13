// PredictionModule.cpp
#include "PredictionModule.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <limits>

// 匈牙利算法实现
void HungarianAlgorithm::Solve(const cv::Mat& costMatrix, std::vector<int>& assignment) {
    int rows = costMatrix.rows;
    int cols = costMatrix.cols;

    assignment.resize(rows, -1);

    if (rows == 0 || cols == 0) {
        return;
    }

    // 简化版贪心算法实现数据关联
    // 对于每个轨迹，找到最近的未分配检测
    std::vector<bool> assigned(cols, false);

    for (int i = 0; i < rows; i++) {
        float minDist = std::numeric_limits<float>::max();
        int bestIdx = -1;

        for (int j = 0; j < cols; j++) {
            if (!assigned[j] && costMatrix.at<float>(i, j) < minDist) {
                minDist = costMatrix.at<float>(i, j);
                bestIdx = j;
            }
        }

        if (bestIdx >= 0) {
            assignment[i] = bestIdx;
            assigned[bestIdx] = true;
        }
    }
}

// TrackManager实现
TrackManager::TrackManager() : nextTrackId(0), lockedTrackId(-1), lockCounter(0) {
    // 使用默认参数初始化
    params = KFParams();
}

void TrackManager::setParams(const KFParams& p) {
    params = p;

    // 更新所有现有轨迹的卡尔曼滤波器参数
    for (auto& track : tracks) {
        setKalmanNoiseParams(track.kf);
    }
}

KFParams TrackManager::getParams() const {
    return params;
}

void TrackManager::initKalmanFilter(cv::KalmanFilter& kf, const cv::Point2f& initialPos) {
    // 使用常速模型(位置+速度)
    kf.init(4, 2, 0);  // 状态[x,y,vx,vy]，测量[x,y]

    // 状态转移矩阵 (F)
    kf.transitionMatrix = (cv::Mat_<float>(4, 4) <<
        1, 0, 1, 0,  // x' = x + vx
        0, 1, 0, 1,  // y' = y + vy
        0, 0, 1, 0,  // vx' = vx
        0, 0, 0, 1); // vy' = vy

    // 测量矩阵 (H)
    kf.measurementMatrix = (cv::Mat_<float>(2, 4) <<
        1, 0, 0, 0,   // 测量x
        0, 1, 0, 0);  // 测量y

    // 设置初始状态
    kf.statePost.at<float>(0) = initialPos.x;
    kf.statePost.at<float>(1) = initialPos.y;
    kf.statePost.at<float>(2) = 0;  // 初始vx=0
    kf.statePost.at<float>(3) = 0;  // 初始vy=0

    // 设置噪声参数
    setKalmanNoiseParams(kf);
}

void TrackManager::setKalmanNoiseParams(cv::KalmanFilter& kf) {
    // 过程噪声协方差 (Q)
    cv::setIdentity(kf.processNoiseCov, cv::Scalar::all(0));
    kf.processNoiseCov.at<float>(0, 0) = params.q_pos;
    kf.processNoiseCov.at<float>(1, 1) = params.q_pos;
    kf.processNoiseCov.at<float>(2, 2) = params.q_vel;
    kf.processNoiseCov.at<float>(3, 3) = params.q_vel;

    // 测量噪声协方差 (R)
    cv::setIdentity(kf.measurementNoiseCov, cv::Scalar::all(params.r_meas));

    // 后验错误协方差
    cv::setIdentity(kf.errorCovPost, cv::Scalar::all(1));
}

void TrackManager::associateDetectionsToTracks(
    const std::vector<DetectionResult>& detections,
    std::vector<std::pair<int, int>>& assignments,
    std::vector<int>& unassignedTracks,
    std::vector<int>& unassignedDetections) {

    // 初始化结果
    assignments.clear();
    unassignedTracks.clear();
    unassignedDetections.clear();

    // 如果没有轨迹或检测
    if (tracks.empty() || detections.empty()) {
        // 所有轨迹都未分配
        for (int i = 0; i < tracks.size(); i++) {
            unassignedTracks.push_back(i);
        }
        // 所有检测都未分配
        for (int i = 0; i < detections.size(); i++) {
            unassignedDetections.push_back(i);
        }
        return;
    }

    // 创建代价矩阵
    cv::Mat costMatrix(tracks.size(), detections.size(), CV_32F);

    // 计算每个轨迹和检测之间的距离
    for (int i = 0; i < tracks.size(); i++) {
        for (int j = 0; j < detections.size(); j++) {
            cv::Point2f predPos = tracks[i].predicted;
            cv::Point2f detPos(detections[j].x, detections[j].y);

            // 计算欧氏距离
            float dist = cv::norm(predPos - detPos);
            costMatrix.at<float>(i, j) = dist;
        }
    }

    // 使用匈牙利算法进行分配
    std::vector<int> assignment;
    HungarianAlgorithm::Solve(costMatrix, assignment);

    // 处理分配结果
    for (int i = 0; i < tracks.size(); i++) {
        // 检查分配结果是否有效，且距离小于阈值
        if (assignment[i] >= 0 && costMatrix.at<float>(i, assignment[i]) <= params.gate_th) {
            assignments.push_back(std::make_pair(i, assignment[i]));
        }
        else {
            unassignedTracks.push_back(i);
        }
    }

    // 找出未分配的检测
    std::vector<bool> detectionAssigned(detections.size(), false);
    for (auto& pair : assignments) {
        detectionAssigned[pair.second] = true;
    }

    for (int i = 0; i < detections.size(); i++) {
        if (!detectionAssigned[i]) {
            unassignedDetections.push_back(i);
        }
    }
}

void TrackManager::processFrame(const std::vector<DetectionResult>& detections) {
    // 1. 预测所有轨迹
    for (auto& track : tracks) {
        cv::Mat prediction = track.kf.predict();
        track.predicted = cv::Point2f(prediction.at<float>(0), prediction.at<float>(1));
        track.age++;
    }

    // 2. 数据关联
    std::vector<std::pair<int, int>> assignments;
    std::vector<int> unassignedTracks;
    std::vector<int> unassignedDetections;

    associateDetectionsToTracks(detections, assignments, unassignedTracks, unassignedDetections);

    // 3. 更新匹配的轨迹
    for (auto& pair : assignments) {
        int trackIdx = pair.first;
        int detIdx = pair.second;

        // 准备测量
        cv::Mat measurement = (cv::Mat_<float>(2, 1) <<
            detections[detIdx].x,
            detections[detIdx].y);

        // 更新卡尔曼滤波器
        tracks[trackIdx].kf.correct(measurement);
        tracks[trackIdx].lastPos = cv::Point2f(detections[detIdx].x, detections[detIdx].y);
        tracks[trackIdx].lost = 0;
        tracks[trackIdx].score = detections[detIdx].confidence;
        tracks[trackIdx].classId = detections[detIdx].classId;

        // 更新稳定性
        tracks[trackIdx].stability = 0.8f * tracks[trackIdx].stability + 0.2f;
    }

    // 4. 处理未匹配的轨迹
    for (int trackIdx : unassignedTracks) {
        tracks[trackIdx].lost++;
        // 降低稳定性
        tracks[trackIdx].stability = 0.95f * tracks[trackIdx].stability;
    }

    // 5. 创建新轨迹
    for (int detIdx : unassignedDetections) {
        // 筛选有效的检测结果 (只关注头部类别)
        if (detections[detIdx].classId == 1 || detections[detIdx].classId == 3) {
            Track newTrack;
            newTrack.id = nextTrackId++;
            newTrack.age = 1;
            newTrack.lost = 0;
            newTrack.stability = 0.5f;
            newTrack.classId = detections[detIdx].classId;
            newTrack.score = detections[detIdx].confidence;
            newTrack.lastPos = cv::Point2f(detections[detIdx].x, detections[detIdx].y);

            // 初始化卡尔曼滤波器
            initKalmanFilter(newTrack.kf, newTrack.lastPos);

            tracks.push_back(newTrack);
        }
    }

    // 6. 删除长时间未匹配的轨迹
    tracks.erase(
        std::remove_if(tracks.begin(), tracks.end(),
            [this](const Track& t) {
                return t.lost > params.lost_max;
            }),
        tracks.end()
    );

    // 7. 执行锁定轨迹策略
    updateLockedTrack();
}

void TrackManager::updateLockedTrack() {
    // 如果有锁定的轨迹
    if (lockedTrackId >= 0) {
        // 查找当前锁定的轨迹
        auto it = std::find_if(tracks.begin(), tracks.end(),
            [this](const Track& t) { return t.id == lockedTrackId; });

        // 如果轨迹仍然存在且丢失帧数在允许范围内
        if (it != tracks.end() && it->lost <= params.lock_keep) {
            // 保持锁定
            return;
        }

        // 轨迹不存在或丢失太久，释放锁定
        lockedTrackId = -1;
        lockCounter = 0;
    }

    // 寻找新的可锁定轨迹

    // 1. 筛选稳定性足够的轨迹
    std::vector<Track*> candidates;
    for (auto& track : tracks) {
        if (track.stability >= params.stab_min && track.lost == 0) {
            candidates.push_back(&track);
        }
    }

    if (candidates.empty()) {
        return; // 没有合适的候选轨迹
    }

    // 2. 找出最佳轨迹(距离中心最近)
    const int centerX = 160;
    const int centerY = 160;

    Track* bestTrack = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    for (auto* track : candidates) {
        float dx = track->predicted.x - centerX;
        float dy = track->predicted.y - centerY;
        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < minDistance) {
            minDistance = distance;
            bestTrack = track;
        }
    }

    // 3. 应用滞后切换策略
    if (bestTrack) {
        if (lockedTrackId < 0) {
            // 没有锁定轨迹，直接锁定
            lockedTrackId = bestTrack->id;
            lockCounter = 0;
        }
        else if (bestTrack->id != lockedTrackId) {
            // 有更好的轨迹，计数
            lockCounter++;
            if (lockCounter >= params.switch_hys) {
                // 超过滞后阈值，切换轨迹
                lockedTrackId = bestTrack->id;
                lockCounter = 0;
            }
        }
        else {
            // 当前最佳就是锁定轨迹，重置计数
            lockCounter = 0;
        }
    }
}

PredictionResult TrackManager::getPrediction() {
    PredictionResult result;
    result.timestamp = std::chrono::high_resolution_clock::now();

    // 找到锁定的轨迹
    auto it = std::find_if(tracks.begin(), tracks.end(),
        [this](const Track& t) { return t.id == lockedTrackId; });

    if (it != tracks.end()) {
        // 获取当前状态
        cv::Mat state = it->kf.statePost;
        float x = state.at<float>(0);
        float y = state.at<float>(1);
        float vx = state.at<float>(2);
        float vy = state.at<float>(3);

        // 短期预测：位置 + 速度*时间
        float predTime = params.pred_time;
        float predX = x + vx * predTime;
        float predY = y + vy * predTime;

        result.x = static_cast<int>(std::round(predX));
        result.y = static_cast<int>(std::round(predY));
    }
    else {
        // 无有效预测
        result.x = 999;
        result.y = 999;
    }

    return result;
}

std::vector<Track> TrackManager::getTracks() const {
    return tracks;
}

int TrackManager::getLockedTrackId() const {
    return lockedTrackId;
}

namespace PredictionModule {
    // 全局变量
    std::atomic<bool> g_running{ false };
    std::atomic<bool> g_debugMode{ false };

    // 轨迹管理器
    TrackManager g_trackManager;

    // 全局参数原子变量 - 为了兼容性保留
    std::atomic<float> g_gFactor{ 0.2f };
    std::atomic<float> g_hFactor{ 0.16f };
    std::atomic<float> g_predictionTime{ 0.0f };

    // 卡尔曼滤波参数
    KFParams g_kfParams;
    std::mutex g_paramsMutex;

    // 环形缓冲区，用于存储预测结果
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // 预测模块工作函数
    void PredictionModuleWorker() {
        std::cout << "Prediction module worker started with Kalman filter tracking" << std::endl;

        // 设置初始卡尔曼滤波参数
        {
            std::lock_guard<std::mutex> lock(g_paramsMutex);
            g_trackManager.setParams(g_kfParams);
        }

        while (g_running) {
            try {
                // 获取所有检测目标
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                // 处理当前帧
                g_trackManager.processFrame(targets);

                // 获取预测结果
                PredictionResult prediction = g_trackManager.getPrediction();

                // 将预测结果写入环形缓冲区
                g_predictionBuffer.write(prediction);

                // debug模式处理 - 向检测模块提供预测点信息
                if (g_debugMode.load() && prediction.x != 999 && prediction.y != 999) {
                    // 调用检测模块的绘制预测点函数
                    DetectionModule::DrawPredictionPoint(prediction.x, prediction.y);

                    // 在debug模式下显示所有轨迹信息
                    auto tracks = g_trackManager.getTracks();
                    int lockedId = g_trackManager.getLockedTrackId();

                    // 在控制台输出跟踪信息
                    if (tracks.size() > 0) {
                        std::cout << "Active tracks: " << tracks.size()
                            << ", Locked: " << lockedId << std::endl;

                        for (const auto& track : tracks) {
                            if (track.id == lockedId) {
                                std::cout << "[Locked] ";
                            }

                            std::cout << "ID: " << track.id
                                << ", Pos: (" << track.predicted.x << "," << track.predicted.y << ")"
                                << ", Age: " << track.age
                                << ", Lost: " << track.lost
                                << ", Stab: " << track.stability
                                << std::endl;
                        }
                    }
                }

                // 更新卡尔曼滤波参数 (如果有变化)
                {
                    std::lock_guard<std::mutex> lock(g_paramsMutex);
                    g_trackManager.setParams(g_kfParams);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error in prediction module: " << e.what() << std::endl;
            }

            // 短暂休眠，避免CPU占用过高
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::cout << "Prediction module worker stopped" << std::endl;
    }

    // 公共接口实现
    std::thread Initialize() {
        if (g_running) {
            std::cerr << "Prediction module already running" << std::endl;
            return std::thread();
        }

        std::cout << "Initializing prediction module with Kalman filter tracking" << std::endl;

        // 设置默认的卡尔曼滤波参数
        KFParams defaultParams;
        {
            std::lock_guard<std::mutex> lock(g_paramsMutex);
            g_kfParams = defaultParams;
        }

        g_running = true;
        g_predictionBuffer.clear();

        return std::thread(PredictionModuleWorker);
    }

    bool IsRunning() {
        return g_running;
    }

    void Stop() {
        if (!g_running) {
            return;
        }

        std::cout << "Stopping prediction module" << std::endl;
        g_running = false;
    }

    void Cleanup() {
        g_predictionBuffer.clear();
        std::cout << "Prediction module cleanup completed" << std::endl;
    }

    bool GetLatestPrediction(PredictionResult& result) {
        return g_predictionBuffer.readLatest(result);
    }

    // 设置调试模式
    void SetDebugMode(bool enabled) {
        g_debugMode = enabled;
    }

    // 获取调试模式状态
    bool IsDebugModeEnabled() {
        return g_debugMode.load();
    }

    // 卡尔曼滤波参数设置
    void SetKalmanFilterParams(const KFParams& params) {
        std::lock_guard<std::mutex> lock(g_paramsMutex);
        g_kfParams = params;
    }

    // 获取当前卡尔曼滤波参数
    KFParams GetKalmanFilterParams() {
        std::lock_guard<std::mutex> lock(g_paramsMutex);
        return g_kfParams;
    }

    // 以下函数保留为接口兼容
    void SetGFactor(float g) {
        g_gFactor = g;

        // 更新卡尔曼滤波参数
        std::lock_guard<std::mutex> lock(g_paramsMutex);
        g_kfParams.q_pos = g * 0.005f;  // 映射到位置过程噪声
        g_kfParams.q_vel = g * 0.05f;   // 映射到速度过程噪声
    }

    void SetHFactor(float h) {
        g_hFactor = h;

        // 更新卡尔曼滤波参数
        std::lock_guard<std::mutex> lock(g_paramsMutex);
        g_kfParams.r_meas = h;  // 直接映射到测量噪声
    }

    void SetPredictionTime(float seconds) {
        g_predictionTime = seconds;

        // 更新卡尔曼滤波参数
        std::lock_guard<std::mutex> lock(g_paramsMutex);
        g_kfParams.pred_time = seconds;  // 直接映射到预测时间
    }
}