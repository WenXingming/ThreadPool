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
    Task() : Task(UINT64_MAX, INT_MIN, nullptr) {
    }

    Task(uint64_t sequenceId, int priority, std::function<void()> func)
        : sequenceId_(sequenceId)
        , priority_(priority)
        , function_(std::move(func)) {
    }

    bool operator<(const Task& other) const {
        // 若其第一参数在弱序中先于其第二参数则返回 true，弱序置于堆低（优先级低的）
        if (priority_ != other.priority_) {
            return priority_ < other.priority_;
        }
        // 同优先级时，序号大的（后来）的是弱序，置于堆低（后来的任务后执行）
        return sequenceId_ > other.sequenceId_;
    }

    void run() {
        if (!function_) {
            throw std::runtime_error("task function is empty, cannot run.");
        }
        function_();
    }

    int get_priority() const {
        return priority_;
    }

    uint64_t get_sequence_id() const {
        return sequenceId_;
    }

private:
    uint64_t sequenceId_;                               // 任务入队递增序号，用于同优先级任务的 FCFS 排序规则
    int priority_;                                      // 优先级，数值越大优先级越高
    std::function<void()> function_;                    // 待执行的任务函数，封装为 std::function<void()> 以支持任意可调用对象
};

}
