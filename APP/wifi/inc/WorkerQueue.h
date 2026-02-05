#ifndef WORKER_QUEUE_H
#define WORKER_QUEUE_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

/**
 * @brief 后台任务队列
 *
 * 特点：
 *  - FIFO 顺序执行
 *  - 空闲时线程沉睡（0 CPU）
 *  - 可安全退出
 */
class WorkerQueue {
public:
    WorkerQueue();
    ~WorkerQueue();

    // 投递一个任务（线程安全）
    void post(const std::function<void()>& task);

    // 停止线程（执行完队列中已有任务）
    void stop();

private:
    void threadLoop();

private:
    std::thread m_thread;

    std::mutex m_mutex;
    std::condition_variable m_cv;

    std::queue<std::function<void()>> m_tasks;
    std::atomic<bool> m_running{true};
};

#endif  // WORKER_QUEUE_H
