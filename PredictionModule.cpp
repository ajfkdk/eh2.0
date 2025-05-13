// PredictionModule.cpp
#include "PredictionModule.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <limits>

// �������㷨ʵ��
void HungarianAlgorithm::Solve(const cv::Mat& costMatrix, std::vector<int>& assignment) {
    int rows = costMatrix.rows;
    int cols = costMatrix.cols;

    assignment.resize(rows, -1);

    if (rows == 0 || cols == 0) {
        return;
    }

    // �򻯰�̰���㷨ʵ�����ݹ���
    // ����ÿ���켣���ҵ������δ������
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

// TrackManagerʵ��
TrackManager::TrackManager() : nextTrackId(0), lockedTrackId(-1), lockCounter(0) {
    // ʹ��Ĭ�ϲ�����ʼ��
    params = KFParams();
}

void TrackManager::setParams(const KFParams& p) {
    params = p;

    // �����������й켣�Ŀ������˲�������
    for (auto& track : tracks) {
        setKalmanNoiseParams(track.kf);
    }
}

KFParams TrackManager::getParams() const {
    return params;
}

void TrackManager::initKalmanFilter(cv::KalmanFilter& kf, const cv::Point2f& initialPos) {
    // ʹ�ó���ģ��(λ��+�ٶ�)
    kf.init(4, 2, 0);  // ״̬[x,y,vx,vy]������[x,y]

    // ״̬ת�ƾ��� (F)
    kf.transitionMatrix = (cv::Mat_<float>(4, 4) <<
        1, 0, 1, 0,  // x' = x + vx
        0, 1, 0, 1,  // y' = y + vy
        0, 0, 1, 0,  // vx' = vx
        0, 0, 0, 1); // vy' = vy

    // �������� (H)
    kf.measurementMatrix = (cv::Mat_<float>(2, 4) <<
        1, 0, 0, 0,   // ����x
        0, 1, 0, 0);  // ����y

    // ���ó�ʼ״̬
    kf.statePost.at<float>(0) = initialPos.x;
    kf.statePost.at<float>(1) = initialPos.y;
    kf.statePost.at<float>(2) = 0;  // ��ʼvx=0
    kf.statePost.at<float>(3) = 0;  // ��ʼvy=0

    // ������������
    setKalmanNoiseParams(kf);
}

void TrackManager::setKalmanNoiseParams(cv::KalmanFilter& kf) {
    // ��������Э���� (Q)
    cv::setIdentity(kf.processNoiseCov, cv::Scalar::all(0));
    kf.processNoiseCov.at<float>(0, 0) = params.q_pos;
    kf.processNoiseCov.at<float>(1, 1) = params.q_pos;
    kf.processNoiseCov.at<float>(2, 2) = params.q_vel;
    kf.processNoiseCov.at<float>(3, 3) = params.q_vel;

    // ��������Э���� (R)
    cv::setIdentity(kf.measurementNoiseCov, cv::Scalar::all(params.r_meas));

    // �������Э����
    cv::setIdentity(kf.errorCovPost, cv::Scalar::all(1));
}

void TrackManager::associateDetectionsToTracks(
    const std::vector<DetectionResult>& detections,
    std::vector<std::pair<int, int>>& assignments,
    std::vector<int>& unassignedTracks,
    std::vector<int>& unassignedDetections) {

    // ��ʼ�����
    assignments.clear();
    unassignedTracks.clear();
    unassignedDetections.clear();

    // ���û�й켣����
    if (tracks.empty() || detections.empty()) {
        // ���й켣��δ����
        for (int i = 0; i < tracks.size(); i++) {
            unassignedTracks.push_back(i);
        }
        // ���м�ⶼδ����
        for (int i = 0; i < detections.size(); i++) {
            unassignedDetections.push_back(i);
        }
        return;
    }

    // �������۾���
    cv::Mat costMatrix(tracks.size(), detections.size(), CV_32F);

    // ����ÿ���켣�ͼ��֮��ľ���
    for (int i = 0; i < tracks.size(); i++) {
        for (int j = 0; j < detections.size(); j++) {
            cv::Point2f predPos = tracks[i].predicted;
            cv::Point2f detPos(detections[j].x, detections[j].y);

            // ����ŷ�Ͼ���
            float dist = cv::norm(predPos - detPos);
            costMatrix.at<float>(i, j) = dist;
        }
    }

    // ʹ���������㷨���з���
    std::vector<int> assignment;
    HungarianAlgorithm::Solve(costMatrix, assignment);

    // ���������
    for (int i = 0; i < tracks.size(); i++) {
        // ���������Ƿ���Ч���Ҿ���С����ֵ
        if (assignment[i] >= 0 && costMatrix.at<float>(i, assignment[i]) <= params.gate_th) {
            assignments.push_back(std::make_pair(i, assignment[i]));
        }
        else {
            unassignedTracks.push_back(i);
        }
    }

    // �ҳ�δ����ļ��
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
    // 1. Ԥ�����й켣
    for (auto& track : tracks) {
        cv::Mat prediction = track.kf.predict();
        track.predicted = cv::Point2f(prediction.at<float>(0), prediction.at<float>(1));
        track.age++;
    }

    // 2. ���ݹ���
    std::vector<std::pair<int, int>> assignments;
    std::vector<int> unassignedTracks;
    std::vector<int> unassignedDetections;

    associateDetectionsToTracks(detections, assignments, unassignedTracks, unassignedDetections);

    // 3. ����ƥ��Ĺ켣
    for (auto& pair : assignments) {
        int trackIdx = pair.first;
        int detIdx = pair.second;

        // ׼������
        cv::Mat measurement = (cv::Mat_<float>(2, 1) <<
            detections[detIdx].x,
            detections[detIdx].y);

        // ���¿������˲���
        tracks[trackIdx].kf.correct(measurement);
        tracks[trackIdx].lastPos = cv::Point2f(detections[detIdx].x, detections[detIdx].y);
        tracks[trackIdx].lost = 0;
        tracks[trackIdx].score = detections[detIdx].confidence;
        tracks[trackIdx].classId = detections[detIdx].classId;

        // �����ȶ���
        tracks[trackIdx].stability = 0.8f * tracks[trackIdx].stability + 0.2f;
    }

    // 4. ����δƥ��Ĺ켣
    for (int trackIdx : unassignedTracks) {
        tracks[trackIdx].lost++;
        // �����ȶ���
        tracks[trackIdx].stability = 0.95f * tracks[trackIdx].stability;
    }

    // 5. �����¹켣
    for (int detIdx : unassignedDetections) {
        // ɸѡ��Ч�ļ���� (ֻ��עͷ�����)
        if (detections[detIdx].classId == 1 || detections[detIdx].classId == 3) {
            Track newTrack;
            newTrack.id = nextTrackId++;
            newTrack.age = 1;
            newTrack.lost = 0;
            newTrack.stability = 0.5f;
            newTrack.classId = detections[detIdx].classId;
            newTrack.score = detections[detIdx].confidence;
            newTrack.lastPos = cv::Point2f(detections[detIdx].x, detections[detIdx].y);

            // ��ʼ���������˲���
            initKalmanFilter(newTrack.kf, newTrack.lastPos);

            tracks.push_back(newTrack);
        }
    }

    // 6. ɾ����ʱ��δƥ��Ĺ켣
    tracks.erase(
        std::remove_if(tracks.begin(), tracks.end(),
            [this](const Track& t) {
                return t.lost > params.lost_max;
            }),
        tracks.end()
    );

    // 7. ִ�������켣����
    updateLockedTrack();
}

void TrackManager::updateLockedTrack() {
    // ����������Ĺ켣
    if (lockedTrackId >= 0) {
        // ���ҵ�ǰ�����Ĺ켣
        auto it = std::find_if(tracks.begin(), tracks.end(),
            [this](const Track& t) { return t.id == lockedTrackId; });

        // ����켣��Ȼ�����Ҷ�ʧ֡��������Χ��
        if (it != tracks.end() && it->lost <= params.lock_keep) {
            // ��������
            return;
        }

        // �켣�����ڻ�ʧ̫�ã��ͷ�����
        lockedTrackId = -1;
        lockCounter = 0;
    }

    // Ѱ���µĿ������켣

    // 1. ɸѡ�ȶ����㹻�Ĺ켣
    std::vector<Track*> candidates;
    for (auto& track : tracks) {
        if (track.stability >= params.stab_min && track.lost == 0) {
            candidates.push_back(&track);
        }
    }

    if (candidates.empty()) {
        return; // û�к��ʵĺ�ѡ�켣
    }

    // 2. �ҳ���ѹ켣(�����������)
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

    // 3. Ӧ���ͺ��л�����
    if (bestTrack) {
        if (lockedTrackId < 0) {
            // û�������켣��ֱ������
            lockedTrackId = bestTrack->id;
            lockCounter = 0;
        }
        else if (bestTrack->id != lockedTrackId) {
            // �и��õĹ켣������
            lockCounter++;
            if (lockCounter >= params.switch_hys) {
                // �����ͺ���ֵ���л��켣
                lockedTrackId = bestTrack->id;
                lockCounter = 0;
            }
        }
        else {
            // ��ǰ��Ѿ��������켣�����ü���
            lockCounter = 0;
        }
    }
}

PredictionResult TrackManager::getPrediction() {
    PredictionResult result;
    result.timestamp = std::chrono::high_resolution_clock::now();

    // �ҵ������Ĺ켣
    auto it = std::find_if(tracks.begin(), tracks.end(),
        [this](const Track& t) { return t.id == lockedTrackId; });

    if (it != tracks.end()) {
        // ��ȡ��ǰ״̬
        cv::Mat state = it->kf.statePost;
        float x = state.at<float>(0);
        float y = state.at<float>(1);
        float vx = state.at<float>(2);
        float vy = state.at<float>(3);

        // ����Ԥ�⣺λ�� + �ٶ�*ʱ��
        float predTime = params.pred_time;
        float predX = x + vx * predTime;
        float predY = y + vy * predTime;

        result.x = static_cast<int>(std::round(predX));
        result.y = static_cast<int>(std::round(predY));
    }
    else {
        // ����ЧԤ��
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
    // ȫ�ֱ���
    std::atomic<bool> g_running{ false };
    std::atomic<bool> g_debugMode{ false };

    // �켣������
    TrackManager g_trackManager;

    // ȫ�ֲ���ԭ�ӱ��� - Ϊ�˼����Ա���
    std::atomic<float> g_gFactor{ 0.2f };
    std::atomic<float> g_hFactor{ 0.16f };
    std::atomic<float> g_predictionTime{ 0.0f };

    // �������˲�����
    KFParams g_kfParams;
    std::mutex g_paramsMutex;

    // ���λ����������ڴ洢Ԥ����
    constexpr size_t BUFFER_SIZE = 100;
    RingBuffer<PredictionResult, BUFFER_SIZE> g_predictionBuffer;

    // Ԥ��ģ�鹤������
    void PredictionModuleWorker() {
        std::cout << "Prediction module worker started with Kalman filter tracking" << std::endl;

        // ���ó�ʼ�������˲�����
        {
            std::lock_guard<std::mutex> lock(g_paramsMutex);
            g_trackManager.setParams(g_kfParams);
        }

        while (g_running) {
            try {
                // ��ȡ���м��Ŀ��
                std::vector<DetectionResult> targets = DetectionModule::GetAllResults();

                // ����ǰ֡
                g_trackManager.processFrame(targets);

                // ��ȡԤ����
                PredictionResult prediction = g_trackManager.getPrediction();

                // ��Ԥ����д�뻷�λ�����
                g_predictionBuffer.write(prediction);

                // debugģʽ���� - ����ģ���ṩԤ�����Ϣ
                if (g_debugMode.load() && prediction.x != 999 && prediction.y != 999) {
                    // ���ü��ģ��Ļ���Ԥ��㺯��
                    DetectionModule::DrawPredictionPoint(prediction.x, prediction.y);

                    // ��debugģʽ����ʾ���й켣��Ϣ
                    auto tracks = g_trackManager.getTracks();
                    int lockedId = g_trackManager.getLockedTrackId();

                    // �ڿ���̨���������Ϣ
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

                // ���¿������˲����� (����б仯)
                {
                    std::lock_guard<std::mutex> lock(g_paramsMutex);
                    g_trackManager.setParams(g_kfParams);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error in prediction module: " << e.what() << std::endl;
            }

            // �������ߣ�����CPUռ�ù���
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::cout << "Prediction module worker stopped" << std::endl;
    }

    // �����ӿ�ʵ��
    std::thread Initialize() {
        if (g_running) {
            std::cerr << "Prediction module already running" << std::endl;
            return std::thread();
        }

        std::cout << "Initializing prediction module with Kalman filter tracking" << std::endl;

        // ����Ĭ�ϵĿ������˲�����
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

    // ���õ���ģʽ
    void SetDebugMode(bool enabled) {
        g_debugMode = enabled;
    }

    // ��ȡ����ģʽ״̬
    bool IsDebugModeEnabled() {
        return g_debugMode.load();
    }

    // �������˲���������
    void SetKalmanFilterParams(const KFParams& params) {
        std::lock_guard<std::mutex> lock(g_paramsMutex);
        g_kfParams = params;
    }

    // ��ȡ��ǰ�������˲�����
    KFParams GetKalmanFilterParams() {
        std::lock_guard<std::mutex> lock(g_paramsMutex);
        return g_kfParams;
    }

    // ���º�������Ϊ�ӿڼ���
    void SetGFactor(float g) {
        g_gFactor = g;

        // ���¿������˲�����
        std::lock_guard<std::mutex> lock(g_paramsMutex);
        g_kfParams.q_pos = g * 0.005f;  // ӳ�䵽λ�ù�������
        g_kfParams.q_vel = g * 0.05f;   // ӳ�䵽�ٶȹ�������
    }

    void SetHFactor(float h) {
        g_hFactor = h;

        // ���¿������˲�����
        std::lock_guard<std::mutex> lock(g_paramsMutex);
        g_kfParams.r_meas = h;  // ֱ��ӳ�䵽��������
    }

    void SetPredictionTime(float seconds) {
        g_predictionTime = seconds;

        // ���¿������˲�����
        std::lock_guard<std::mutex> lock(g_paramsMutex);
        g_kfParams.pred_time = seconds;  // ֱ��ӳ�䵽Ԥ��ʱ��
    }
}