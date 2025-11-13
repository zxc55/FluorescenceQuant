#ifndef CARDWATCHERSTD_H_
#define CARDWATCHERSTD_H_
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

class KeysProxy;

class CardWatcherStd {
public:
    // evdevPath 为空则按名字包含匹配 "gpio" && "keys"
    explicit CardWatcherStd(KeysProxy* proxy, const std::string& evdevPath = {});
    ~CardWatcherStd();

    void setWatchedCode(unsigned code);  // 例如 KEY_ENTER=28、KEY_POWER=116
    void setDebounceMs(int ms);          // 软件去抖（ms），0 关闭
    bool start();                        // 起 std::thread
    void stop();                         // 请求退出并 join

private:
    void threadFunc();

    // 打开/读取
    int openEventByPath(const std::string& path);
    int openEventByNameContains(const char* key1, const char* key2);  // 名称包含匹配
    int readLevelByIoctl(int fd, unsigned watchedCode, bool& outPressed);

    // 投递到 GUI 线程（KeysProxy）
    void postInserted();
    void postRemoved();
    void postInsertedChanged(bool on);

private:
    KeysProxy* proxy_ = nullptr;  // 仅用于 invokeMethod 回 GUI 线程
    std::string path_;

    std::atomic<bool> running_{false};
    std::thread th_;

    unsigned watchedCode_ = 28;  // 缺省 KEY_ENTER
    int debounceMs_ = 120;       // 去抖窗口

    // 去抖/同态去重
    int stableValue_ = -1;
    uint64_t stableTsMs_ = 0;
    bool inserted_ = false;
};
#endif  // CARDWATCHERSTD_H_