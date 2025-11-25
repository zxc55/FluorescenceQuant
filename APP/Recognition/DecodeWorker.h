#ifndef _DECODEWORKER_H_
#define _DECODEWORKER_H_

#include <QImage>
#include <QString>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

class DecodeWorker final {
public:
    using ResultCallback = std::function<void(const QString&)>;

    explicit DecodeWorker(ResultCallback cb);
    ~DecodeWorker();

    void start();
    void stop();
    void enqueueFrame(const QImage& img);

private:
    void loop();
    // bool processFrameZXing(const QImage& img, QString& outText);
    bool processFrameQuirc(const QImage& img, QString& outText);

private:
    ResultCallback m_onResult;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::queue<QImage> m_queue;
    std::mutex m_mtx;
    std::condition_variable m_cv;
    int m_queueSize = 1;  // 最大队列长度
};
#endif  // _DECODEWORKER_H_