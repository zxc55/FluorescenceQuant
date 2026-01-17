#ifndef QRIMAGEPROVIDER_H
#define QRIMAGEPROVIDER_H

#include <QQuickImageProvider>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>

#include "QrScanner.h"
// QrScanner.cpp

class TimeTick final {
public:
    // 每次调用：打印与上次 Tick 的时间差，并更新“上次时间”
    static void Tick(const char* tag = "Tick") {
        using clock = std::chrono::steady_clock;
        using namespace std::chrono;

        const auto now = clock::now();

        std::lock_guard<std::mutex> lk(mutex_());
        if (!has_last_()) {
            last_() = now;
            has_last_() = true;
            std::printf("[TimeTick] %s first\n", tag ? tag : "");
            std::fflush(stdout);
            return;
        }

        const auto diff_us = duration_cast<microseconds>(now - last_()).count();
        last_() = now;

        std::printf("[TimeTick] %s dt: %lld us (%.3f ms)\n",
                    tag ? tag : "",
                    (long long)diff_us,
                    diff_us / 1000.0);
        std::fflush(stdout);
    }

    // 可选：手动重置“上次时间”
    static void Reset() {
        std::lock_guard<std::mutex> lk(mutex_());
        has_last_() = false;
    }

private:
    // 用函数内 static 避免静态初始化顺序问题
    static std::mutex& mutex_() {
        static std::mutex m;
        return m;
    }
    static bool& has_last_() {
        static bool v = false;
        return v;
    }
    static std::chrono::steady_clock::time_point& last_() {
        static std::chrono::steady_clock::time_point t;
        return t;
    }
};
// 简单版：直接拿一个 QrScanner 指针
class QrImageProvider : public QQuickImageProvider {
public:
    // 构造时传入 scanner
    explicit QrImageProvider(QrScanner* scanner)
        : QQuickImageProvider(QQuickImageProvider::Image), m_scanner(scanner) {}

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override {
        Q_UNUSED(id)
        Q_UNUSED(requestedSize)
        TimeTick::Tick("tick");
        if (!m_scanner)
            return QImage();

        QImage img = m_scanner->lastFrame();
        if (size)
            *size = img.size();
        return img;
    }

private:
    QrScanner* m_scanner;
};

#endif  // QRIMAGEPROVIDER_H
