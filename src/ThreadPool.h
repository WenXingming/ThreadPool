// ============================================================================
// ThreadPool - 线程池类声明
// ============================================================================
// 核心功能和接口：
// 1. 提交普通任务（可有返回值）和带优先级任务。
// 2. 提供自动扩缩容所需的控制接口。
// 3. 查询线程池大小、任务队列大小和相关配置。
// ============================================================================

#pragma once
#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>
#include <cassert>

#include "Task.h"

namespace wxm {

class ThreadPool {
public:
    ThreadPool(int threadCount = 1, int maxTasksSize = 50, bool openAutoExpandReduce = false, int maxWaitTimeMs = 1000);

    ThreadPool(const ThreadPool& other) = delete;
    ThreadPool& operator=(const ThreadPool& other) = delete;
    ThreadPool(ThreadPool&& other) = delete;
    ThreadPool& operator=(ThreadPool&& other) = delete;

    ~ThreadPool();

    template<typename F, typename... Args>
    auto submit_task(F&& func, Args&&... args)
        -> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))>;
    template<typename F, typename... Args>
    auto submit_task(int priority, F&& func, Args&&... args)
        -> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))>;

    int get_thread_pool_size();
    int get_current_tasks_size();
    int get_max_tasks_size() { return maxTasksSize_; }
    void set_max_tasks_size(int size) { assert(size > 0); maxTasksSize_ = size; }
    void enable_auto_expand_reduce() { openAutoExpandReduce_ = true; }
    void disable_auto_expand_reduce() { openAutoExpandReduce_ = false; }
    int get_max_wait_time_ms() { return maxWaitTime_; }
    void set_max_wait_time_ms(int waitMs) { assert(waitMs > 0); maxWaitTime_ = waitMs; }

private:
    void initialize_worker_threads(int threadCount);
    void process_task();
    void expand_thread_pool();
    bool reduce_thread_pool(std::thread::id threadId);
    void cleanup_finished_threads();

private:
    std::vector<std::thread> threads_;          // 线程池中的工作线程
    std::mutex threadsMutex_;                   // 保护线程池中线程列表的互斥锁（主要用于自动扩缩容时修改线程列表）
    std::vector<std::thread> finishedThreads_;  // 已缩容退出、等待 join 清理的工作线程
    std::mutex finishedMutex_;                  // 保护待清理线程列表的互斥锁
    std::priority_queue<Task> tasks_;           // 任务队列，优先级高的任务先执行
    std::mutex tasksMutex_;                     // 保护任务队列的互斥锁
    std::atomic<int> maxTasksSize_;             // 任务队列的最大容量
    std::condition_variable notEmpty_;          // 任务队列非空的条件变量，工作线程在此等待新任务到来
    std::condition_variable notFull_;           // 任务队列未满的条件变量，提交任务时在此等待队列有空间
    std::atomic<bool> stopFlag_;                // 线程池停止标志，控制工作线程退出
    std::atomic<bool> openAutoExpandReduce_;    // 是否启用自动扩缩容功能
    std::atomic<int> maxWaitTime_;              // 等待条件变量的最长时间，单位毫秒
};


template<typename F, typename ...Args>
auto ThreadPool::submit_task(F&& func, Args&& ...args)
-> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))> {

    auto res = this->submit_task(0, std::forward<F>(func), std::forward<Args>(args)...); // 默认优先级为 0
    return res;
}

template<typename F, typename... Args>
auto wxm::ThreadPool::submit_task(int priority, F&& func, Args&& ...args)
-> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))> {

    cleanup_finished_threads();

    using RetType = decltype(std::forward<F>(func)(std::forward<Args>(args)...));
    auto taskPtr = std::make_shared<std::packaged_task<RetType()>>(
        std::bind(std::forward<F>(func), std::forward<Args>(args)...)
    );
    std::future<RetType> res = taskPtr->get_future();

    bool taskSubmitted = false;
    while (!taskSubmitted) {
        std::unique_lock<std::mutex> uniqueLock(tasksMutex_);
        auto pred = [this]() {
            return stopFlag_.load() ||
                (tasks_.size() < static_cast<size_t>(maxTasksSize_.load()));
            };
        bool ready = notFull_.wait_for(uniqueLock, std::chrono::milliseconds(maxWaitTime_.load()), pred); // 阻塞退出时获取锁

        // full && not stop
        if (!ready) {
            uniqueLock.unlock(); // 需要手动解锁后再处理后续逻辑（耗时）。
            if (openAutoExpandReduce_) {
                expand_thread_pool();
            }
            else std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 避免在未启用自动扩容时过于频繁地尝试提交任务导致 CPU 占用过高
            continue; // 下一次循环时会再次尝试提交任务
        }

        // not full or stop
        // stop
        if (stopFlag_.load()) {
            throw std::runtime_error("submit_task on stopped ThreadPool!");
        }
        // not full
        auto task = [taskPtr]() { (*taskPtr)(); };
        auto taskFunc = std::function<void()>(task);
        tasks_.push(Task(taskFunc, priority));
        notEmpty_.notify_one();
        taskSubmitted = true;
    }
    return res;
}

} // namespace wxm
