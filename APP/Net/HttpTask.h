#pragma once
#include <string>

enum class TaskType {
    NONE,
    GET,
    POST
};

struct HttpTask {
    TaskType type = TaskType::NONE;
    std::string url;
    std::string body;  // POST json
};
