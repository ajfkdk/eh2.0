#ifndef ACTION_MODULE_H
#define ACTION_MODULE_H

#include <thread>
#include <atomic>
#include <memory>
#include "MouseController.h"
#include "PredictionModule.h"

class ActionModule {
private:
    static std::thread actionThread;
    static std::atomic<bool> running;
    static std::unique_ptr<MouseController> mouseController;

    // 主处理循环
    static void ProcessLoop();

    // 归一化移动值到指定范围
    static std::pair<float, float> NormalizeMovement(float x, float y, float maxValue);

public:
    // 初始化模块
    static std::thread Initialize();

    // 清理资源
    static void Cleanup();

    // 停止模块
    static void Stop();

    // 判断模块是否运行中
    static bool IsRunning();

    // 设置鼠标控制器
    static void SetMouseController(std::unique_ptr<MouseController> controller);
    static std::thread fireThread; // 新增的点射线程
    static void FireControlLoop(); // 新增的点射控制线程函数
    static std::shared_ptr<SharedState> sharedState; // 新增的共享状态
};

#endif // ACTION_MODULE_H