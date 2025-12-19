#pragma once
#include <curl/curl.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class HttpWorker {
public:
    HttpWorker();
    ~HttpWorker();

    void start();
    void stop();

    // 添加 GET 请求
    void enqueueGet(const std::string& url);

private:
    void run();

private:
    std::thread worker;
    std::mutex mtx;
    std::condition_variable cv;

    std::queue<std::string> cmdQueue;  // 只做 GET 示例
    std::atomic<bool> running{false};
};
