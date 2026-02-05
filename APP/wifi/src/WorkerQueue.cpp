#include "WorkerQueue.h"

WorkerQueue::WorkerQueue() {
    m_thread = std::thread(&WorkerQueue::threadLoop, this);
}

WorkerQueue::~WorkerQueue() {
    stop();
}

void WorkerQueue::stop() {
    m_running = false;
    m_cv.notify_one();

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void WorkerQueue::post(const std::function<void()>& task) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.push(task);
    }
    m_cv.notify_one();
}

void WorkerQueue::threadLoop() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(m_mutex);

            // 没任务时沉睡
            m_cv.wait(lock, [&]() {
                return !m_running || !m_tasks.empty();
            });

            // 停止且无任务，退出线程
            if (!m_running && m_tasks.empty()) {
                break;
            }

            // 取队首任务
            task = m_tasks.front();
            m_tasks.pop();
        }

        // 执行任务（不持锁）
        if (task) {
            task();
        }
    }
}
