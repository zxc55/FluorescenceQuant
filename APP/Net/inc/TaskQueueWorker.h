#ifndef TASKQUEUEWORKER_H
#define TASKQUEUEWORKER_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

/*
 * TaskQueueWorker
 *
 * 作用：
 *   后台任务线程 + 任务队列
 *
 * 特点：
 *   - 使用 std::thread（不依赖 Qt）
 *   - condition_variable 空闲时休眠
 *   - 析构自动停止线程（RAII）
 *   - 串行执行任务（线程安全）
 *
 * 适合：
 *   - libcurl
 *   - 文件 IO
 *   - 设备通信
 */
class TaskQueueWorker {
public:
    using Task = std::function<void()>;

    TaskQueueWorker();
    ~TaskQueueWorker();

    void start();
    void stop();

    void pushTask(Task task);

private:
    void threadFunc();

private:
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<Task> m_queue;
    std::atomic<bool> m_running{false};
};

#endif
