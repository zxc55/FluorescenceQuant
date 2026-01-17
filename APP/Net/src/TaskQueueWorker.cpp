#include "TaskQueueWorker.h"

TaskQueueWorker::TaskQueueWorker() {
}

TaskQueueWorker::~TaskQueueWorker() {
    stop();
}

void TaskQueueWorker::start() {
    if (m_running)
        return;

    m_running = true;
    m_thread = std::thread(&TaskQueueWorker::threadFunc, this);
}

void TaskQueueWorker::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running)
            return;

        m_running = false;

        /* 停止时清空剩余任务，防止析构后还执行 */
        while (!m_queue.empty())
            m_queue.pop();
    }

    m_cv.notify_all();

    if (m_thread.joinable())
        m_thread.join();
}

void TaskQueueWorker::pushTask(Task task) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        /* 已停止的 worker 不再接受任务 */
        if (!m_running)
            return;

        m_queue.push(std::move(task));
    }

    m_cv.notify_one();
}

void TaskQueueWorker::threadFunc() {
    while (true) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_cv.wait(lock, [this]() {
                return !m_queue.empty() || !m_running;
            });

            /* stop 且队列为空，安全退出线程 */
            if (!m_running && m_queue.empty())
                break;

            if (!m_queue.empty()) {
                task = std::move(m_queue.front());
                m_queue.pop();
            }
        }

        if (task)
            task();
    }
}
