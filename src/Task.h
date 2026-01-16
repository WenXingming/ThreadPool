/**
 * @file Task.h
 * @brief Declaration of class: Task（Abstract task）
 * @author wenxingming
 * @note My project address: https://github.com/WenXingming/ThreadPool
 */

#pragma once
#include <iostream>
#include <functional>
#include <chrono>
#include <climits>

namespace wxm {

class Task {
private:
    int priority_;										// 优先级，数字越大优先级越高
    std::chrono::steady_clock::time_point timestamp_;	// 时间戳（不用整数，精度不够），用于 FCFS
    std::function<void()> function_;						// 实际的任务函数

public:
    Task() // 安全：在类定义内部实现的成员函数默认是内联的（多个源文件同时包含 task 类，链接时不会重定义）
        : priority_(INT_MIN)
        , timestamp_(std::chrono::steady_clock::now())
        , function_(nullptr) {
    }

    Task(std::function<void()> func, int priority = 0) // 任务函数是通用 function_. 只有线程池 submit_task() 入口是模板参数列表
        : priority_(priority)
        , timestamp_(std::chrono::steady_clock::now())
        , function_(func) {
    }

    // 注意比较 (Compare) 形参的定义，若其第一参数在弱序中先于其第二参数则返回 true
    // 1. 优先级高的在前（优先级调度）。C++ priority_queue 默认是最大堆，即优先级高的在堆顶。我们想要 priority_ 大的在前，则要使大 priority_ 在弱序排序中处于后序
    // 2. 时间戳小的在前（FCFS 调度）。C++ priority_queue 默认是最大堆，即优先级高的在堆顶。我们想要 timestamp_ 小的在前，则要使小 timestamp_ 在弱序排序中处于后序
    bool operator<(const Task& other) const {
        if (priority_ != other.priority_) {
            return priority_ < other.priority_; // 如果 priority_ < other.priority_，则返回 true，表示小 priority_ 在弱序中先于大 priority_
        }
        else {
            return !(timestamp_ < other.timestamp_); // 根据德摩根定律，!(A < B) 等价于 A >= B。等价于 timestamp_ >= other.timestamp_ 即时间戳大的在弱序中先于时间戳小的（相等时也一样）
        }
    }

    void run() {
        if (function_) {
            function_();
        }
        else throw std::runtime_error("task's function_ is empty, can't not run.");

        // debug, 测试优先级调度和 FCFS。实际使用时注释掉
        std::cout << "priority_ is: " << priority_
            << ". time is: " << timestamp_.time_since_epoch().count() << std::endl;
    }

    int get_priority() const {
        return priority_;
    }

    // 将 time_point 转换为自纪元以来的秒（seconds）计数
    int64_t get_timestamp() const {
        auto duration = timestamp_.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    }
};


}