// ============================================================================
// ThreadPool - 线程池类声明
// ============================================================================
// 核心功能和接口：
// 1. 提交普通任务和带优先级任务。
// 2. 查询线程池大小、任务队列大小和相关配置。
// 3. 提供自动扩缩容所需的控制接口。
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

#include "Task.h"

namespace wxm {

class ThreadPool {
public:
    ThreadPool(int threadCount = 1, int maxTasksSize = 100, bool openAutoExpandReduce = false, int maxWaitTimeMs = 1000);
    ThreadPool();

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
    void set_max_tasks_size(int size) { maxTasksSize_ = size; }
    void enable_auto_expand_reduce() { openAutoExpandReduce_ = true; }
    void disable_auto_expand_reduce() { openAutoExpandReduce_ = false; }
    int get_max_wait_time_ms() { return maxWaitTime_; }
    void set_max_wait_time_ms(int waitMs) { maxWaitTime_ = waitMs; }

private:
    void initialize_worker_threads(int threadCount);
    void process_task();
    void expand_thread_pool();
    void reduce_thread_pool(std::thread::id threadId);
    bool wait_not_full_or_stop(std::unique_lock<std::mutex>& lock);
    bool wait_not_empty_or_stop(std::unique_lock<std::mutex>& lock);

private:
    std::vector<std::thread> threads_;
    std::priority_queue<Task> tasks_;
    std::mutex tasksMutex_;
    std::atomic<int> maxTasksSize_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    std::atomic<bool> stopFlag_;
    std::atomic<bool> openAutoExpandReduce_;
    std::atomic<int> maxWaitTime_;
    std::mutex threadsMutex_;
};

template<typename F, typename ...Args>
auto ThreadPool::submit_task(F&& func, Args&& ...args)
-> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))> {
    auto res = this->submit_task(0, std::forward<F>(func), std::forward<Args>(args)...);
    return res;
}

template<typename F, typename... Args>
auto wxm::ThreadPool::submit_task(int priority, F&& func, Args&& ...args)
-> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))> {
    using RetType = decltype(std::forward<F>(func)(std::forward<Args>(args)...));
    auto taskPtr = std::make_shared<std::packaged_task<RetType()>>(
        std::bind(std::forward<F>(func), std::forward<Args>(args)...)
    );
    std::future<RetType> res = taskPtr->get_future();

    while (true) {
        std::unique_lock<std::mutex> uniqueLock(tasksMutex_);
        bool ready = this->wait_not_full_or_stop(uniqueLock);
        if (!ready) {
            uniqueLock.unlock();
            if (openAutoExpandReduce_) {
                expand_thread_pool();
            }
            continue;
        }

        if (stopFlag_) {
            throw std::runtime_error("submit_task on stopped ThreadPool!");
        }
        auto task = [taskPtr]() { (*taskPtr)(); };
        auto taskFunc = std::function<void()>(task);
        tasks_.push(Task(taskFunc, priority));
        break;
    }
    notEmpty_.notify_one();
    return res;
}

}
