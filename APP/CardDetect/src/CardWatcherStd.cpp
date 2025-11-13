#include "CardWatcherStd.h"

#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <QDebug>
#include <QMetaObject>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

#include "KeysProxy.h"

static inline uint64_t to_ms(const timeval& tv) {
    return static_cast<uint64_t>(tv.tv_sec) * 1000ULL +
           static_cast<uint64_t>(tv.tv_usec) / 1000ULL;
}

CardWatcherStd::CardWatcherStd(KeysProxy* proxy, const std::string& evdevPath)
    : proxy_(proxy), path_(evdevPath) {}

CardWatcherStd::~CardWatcherStd() { stop(); }

void CardWatcherStd::setWatchedCode(unsigned code) { watchedCode_ = code; }
void CardWatcherStd::setDebounceMs(int ms) { debounceMs_ = (ms < 0 ? 0 : ms); }

bool CardWatcherStd::start() {
    if (running_.exchange(true))
        return true;
    try {
        th_ = std::thread(&CardWatcherStd::threadFunc, this);
        return true;
    } catch (...) {
        running_.store(false);
        qWarning() << "[CardWatcherStd] start failed";
        return false;
    }
}

void CardWatcherStd::stop() {
    if (!running_.exchange(false))
        return;
    if (th_.joinable())
        th_.join();
}

int CardWatcherStd::openEventByPath(const std::string& path) {
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0)
        qWarning("open %s failed: %s", path.c_str(), strerror(errno));
    return fd;
}

// 名称包含匹配（比如 "sunxi-gpio-keys" 也能命中）
int CardWatcherStd::openEventByNameContains(const char* key1, const char* key2) {
    char dev[32], name[256];
    for (int i = 0; i < 64; ++i) {
        std::snprintf(dev, sizeof(dev), "/dev/input/event%d", i);
        int fd = ::open(dev, O_RDONLY);
        if (fd < 0)
            continue;
        std::memset(name, 0, sizeof(name));
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        std::string s = name;
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        std::string k1 = key1 ? key1 : "";
        std::string k2 = key2 ? key2 : "";
        std::transform(k1.begin(), k1.end(), k1.begin(), ::tolower);
        std::transform(k2.begin(), k2.end(), k2.begin(), ::tolower);
        if ((!k1.empty() && s.find(k1) != std::string::npos) &&
            (!k2.empty() && s.find(k2) != std::string::npos)) {
            qInfo() << "Matched by contains:" << name << "->" << dev;
            return fd;  // 命中直接返回（调用方持有）
        }
        ::close(fd);
    }
    return -1;
}

int CardWatcherStd::readLevelByIoctl(int fd, unsigned watchedCode, bool& outPressed) {
#ifndef BITS_PER_LONG
#define BITS_PER_LONG (sizeof(unsigned long) * 8u)
#endif
#define NBITS(n) ((n) + BITS_PER_LONG - 1u)
#define NLONGS(n) (NBITS(n) / BITS_PER_LONG)

    constexpr unsigned NKEYBITS = KEY_MAX + 1u;
    unsigned long keyBits[NLONGS(NKEYBITS)];
    std::memset(keyBits, 0, sizeof(keyBits));
    if (ioctl(fd, EVIOCGKEY(sizeof(keyBits)), keyBits) < 0)
        return -1;

    auto testBit = [](unsigned code, const unsigned long* bits) -> bool {
        return bits[code / BITS_PER_LONG] & (1UL << (code % BITS_PER_LONG));
    };
    outPressed = testBit(watchedCode, keyBits);
    return 0;
}

void CardWatcherStd::postInserted() {
    if (proxy_)
        QMetaObject::invokeMethod(proxy_, "cardInserted", Qt::QueuedConnection);
}
void CardWatcherStd::postRemoved() {
    if (proxy_)
        QMetaObject::invokeMethod(proxy_, "cardRemoved", Qt::QueuedConnection);
}
void CardWatcherStd::postInsertedChanged(bool on) {
    if (proxy_)
        QMetaObject::invokeMethod(proxy_, "insertedChanged", Qt::QueuedConnection, Q_ARG(bool, on));
}

void CardWatcherStd::threadFunc() {
    const int poll_timeout_ms = 300;  // 有限等待，stop() 响应更快
    const bool kDebugLogAll = false;  // 调试开关：打印所有事件

    while (running_.load()) {
        int fd = -1;
        if (!path_.empty()) {
            fd = openEventByPath(path_);
        } else {
            // 名字包含 gpio + keys 的设备（兼容 sunxi-gpio-keys 等）
            fd = openEventByNameContains("gpio", "keys");
        }

        if (fd < 0) {
            for (int i = 0; i < 10 && running_.load(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // 初值（建立稳定态，并告知 UI 当前状态）
        bool initPressed = false;
        if (readLevelByIoctl(fd, watchedCode_, initPressed) == 0) {
            int v = initPressed ? 1 : 0;
            stableValue_ = v;
            stableTsMs_ = 0;
            inserted_ = (v == 1);
            postInsertedChanged(inserted_);
            if (inserted_)
                postInserted();
            else
                postRemoved();
        }

        std::vector<input_event> buf(64);
        struct pollfd pfd{fd, POLLIN, 0};

        while (running_.load()) {
            pfd.revents = 0;
            int pr = ::poll(&pfd, 1, poll_timeout_ms);
            if (!running_.load())
                break;
            if (pr < 0) {
                if (errno == EINTR)
                    continue;
                qWarning("poll error: %s", strerror(errno));
                break;
            }
            if (pr == 0)
                continue;
            if (!(pfd.revents & POLLIN))
                continue;

            const ssize_t n = ::read(fd, buf.data(), buf.size() * sizeof(input_event));
            if (n <= 0) {
                if (n < 0 && (errno == EAGAIN || errno == EINTR))
                    continue;
                qWarning("evdev read error/EOF");
                break;
            }

            const size_t cnt = static_cast<size_t>(n) / sizeof(input_event);
            for (size_t i = 0; i < cnt; ++i) {
                const input_event& ev = buf[i];

                // 1) 调试输出：先于任何过滤
                if (kDebugLogAll) {
                    qInfo() << "[EV]" << ev.type << ev.code << ev.value;
                }

                // 2) 键类事件优先（若看到 EV_SW，可按需扩展）
                if (ev.type != EV_KEY)
                    continue;

                // 3) 如果你还没确定 code，先注释掉下一行（不过滤）
                // if (ev.code != watchedCode_) continue;

                // 忽略自动连发
                if (ev.value == 2)
                    continue;

                const uint64_t ts = to_ms(ev.time);

                // 4) 去抖 + 同态去重（排错时可先 setDebounceMs(0) 关闭）
                if (debounceMs_ > 0 && stableValue_ != -1) {
                    if (ev.value == stableValue_)
                        continue;
                    const uint64_t dt = (ts > stableTsMs_) ? (ts - stableTsMs_) : 0;
                    if (dt < static_cast<uint64_t>(debounceMs_))
                        continue;
                }
                stableValue_ = ev.value;
                stableTsMs_ = ts;

                // 5) 投递“插/拔卡”到 GUI
                if (ev.value == 1) {
                    if (!inserted_) {
                        inserted_ = true;
                        postInsertedChanged(true);
                        postInserted();
                    }
                } else {  // 0
                    if (inserted_) {
                        inserted_ = false;
                        postInsertedChanged(false);
                        postRemoved();
                    }
                }
            }
        }

        ::close(fd);
        if (running_.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}
