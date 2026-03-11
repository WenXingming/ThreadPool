// ============================================================================
// Task - 线程池任务封装类
// ============================================================================
// 核心功能和接口：
// 1. 封装待执行任务、任务优先级和入队时间戳。
// 2. 提供 run() 执行接口。
// 3. 提供优先级和同优先级 FCFS 的比较规则。
// ============================================================================

#pragma once
#include <chrono>
#include <climits>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace wxm {

class Task {
public:
    Task()
        : priority_(INT_MIN)
        , timestamp_(std::chrono::steady_clock::now())
        , function_(nullptr) {
    }

    Task(std::function<void()> func, int priority = 0)
        : priority_(priority)
        , timestamp_(std::chrono::steady_clock::now())
        , function_(std::move(func)) {
    }

    bool operator<(const Task& other) const {
        if (priority_ != other.priority_) {
            return priority_ < other.priority_;
        }
        return other.timestamp_ < timestamp_;
    }

    void run() {
        if (function_) {
            function_();
        }
        else {
            throw std::runtime_error("task function is empty, cannot run.");
        }
    }

    int get_priority() const {
        return priority_;
    }

    int64_t get_timestamp() const {
        auto duration = timestamp_.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    }

private:
    int priority_;
    std::chrono::steady_clock::time_point timestamp_;
    std::function<void()> function_;
};

}