// ============================================================================
// Task - 线程池任务封装类
// ============================================================================
// 核心功能和接口：
// 1. 封装待执行任务、任务优先级和入队序号。
// 2. 提供 run() 执行接口。
// 3. 提供优先级和同优先级 FCFS 的比较规则。
// ============================================================================

#pragma once
#include <climits>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace wxm {

class Task {
public:
    Task()
        : priority_(INT_MIN)
        , sequenceId_(UINT64_MAX)
        , function_(nullptr) {
    }

    Task(std::function<void()> func, int priority = 0, uint64_t sequenceId = 0)
        : priority_(priority)
        , sequenceId_(sequenceId)
        , function_(std::move(func)) {
    }

    bool operator<(const Task& other) const {
        if (priority_ != other.priority_) {
            return priority_ < other.priority_; // 若其第一参数在弱序中先于其第二参数则返回 true，弱序置于堆低（优先级低的）
        }
        return sequenceId_ > other.sequenceId_; // 同优先级时，序号大的（后来）的是弱序，置于堆低（后来的任务后执行）
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

    uint64_t get_sequence_id() const {
        return sequenceId_;
    }

private:
    int priority_;                                      // 优先级，数值越大优先级越高
    uint64_t sequenceId_;                               // 任务入队递增序号，用于同优先级任务的 FCFS 排序规则
    std::function<void()> function_;                    // 待执行的任务函数，封装为 std::function<void()> 以支持任意可调用对象
};

}
