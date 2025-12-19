#include "HttpWorker.h"

#include <iostream>

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::string* output = (std::string*)userp;
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

HttpWorker::HttpWorker() {}

HttpWorker::~HttpWorker() {
    stop();
}

void HttpWorker::start() {
    running = true;
    worker = std::thread(&HttpWorker::run, this);
}

void HttpWorker::stop() {
    running = false;
    cv.notify_all();
    if (worker.joinable())
        worker.join();
}

void HttpWorker::enqueueGet(const std::string& url) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        cmdQueue.push(url);
    }
    cv.notify_one();
}

void HttpWorker::run() {
    while (running) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]() { return !cmdQueue.empty() || !running; });

        if (!running)
            break;

        std::string url = cmdQueue.front();
        cmdQueue.pop();
        lock.unlock();

        CURL* curl = curl_easy_init();
        if (!curl)
            continue;

        std::string response;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/cacert.pem");

        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);

        CURLcode res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            std::cout << "[HTTP] GET success: " << response << std::endl;
        } else {
            std::cout << "[HTTP] GET failed: "
                      << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
    }
}
