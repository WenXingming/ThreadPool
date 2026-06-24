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

#include "Task.h"

namespace wxm {

class ThreadPool {
public:
    ThreadPool(int threadCount = 1, int queueCapacity = 50, bool autoScalingEnabled = false, int waitTimeoutMs = 1000);

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

    int get_pool_size();
    int get_queue_size();
    int get_queue_capacity();
    void set_queue_capacity(int size);
    void enable_auto_scaling();
    void disable_auto_scaling();
    int get_wait_timeout_ms();
    void set_wait_timeout_ms(int waitMs);

private:
    void process_task();
    void scale_up();
    bool scale_down(std::thread::id threadId);
    void cleanup_exited_worker_threads();

private:
    std::vector<std::thread> workerThreads_;            // 线程池中的工作线程
    std::mutex workerThreadsMutex_;                     // 保护线程池中线程列表的互斥锁（主要用于自动扩缩容时修改线程列表）
    std::vector<std::thread> exitedWorkerThreads_;      // 已缩容退出、等待 join 清理的工作线程
    std::mutex exitedWorkerThreadsMutex_;               // 保护待清理线程列表的互斥锁

    std::priority_queue<Task> taskQueue_;               // 任务队列，优先级高的任务先执行
    std::mutex taskQueueMutex_;                         // 保护任务队列的互斥锁
    std::atomic<int> taskQueueCapacity_;                // 任务队列的最大容量
    std::atomic<uint64_t> nextTaskSequenceId_;          // 下一个任务入队序号，用于同优先级 FCFS

    std::condition_variable notEmptyCv_;                // 任务队列非空的条件变量，工作线程在此等待新任务到来
    std::condition_variable notFullCv_;                 // 任务队列未满的条件变量，提交任务时在此等待队列有空间

    std::atomic<bool> autoScalingEnabled_;              // 是否启用自动扩缩容功能
    std::atomic<int> waitTimeoutMs_;                    // 等待条件变量的最长时间，单位毫秒

    std::atomic<bool> stopFlag_;                        // 线程池停止标志，控制工作线程退出
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

    using RetType = decltype(std::forward<F>(func)(std::forward<Args>(args)...));
    auto taskPtr = std::make_shared<std::packaged_task<RetType()>>(
        std::bind(std::forward<F>(func), std::forward<Args>(args)...)
    );
    std::future<RetType> res = taskPtr->get_future();

    while (true) {
        {
            std::unique_lock<std::mutex> uniqueLock(taskQueueMutex_);
            auto queueHasSpaceOrStopped = [this]() {
                return taskQueue_.size() < static_cast<size_t>(taskQueueCapacity_.load()) ||
                    stopFlag_.load();
                };
            bool ready = notFullCv_.wait_for(uniqueLock, std::chrono::milliseconds(waitTimeoutMs_.load()), queueHasSpaceOrStopped);

            // full && not stop
            if (!ready) {
                uniqueLock.unlock(); // 需要手动解锁后再处理后续逻辑（耗时）。
                if (stopFlag_.load()) {
                    throw std::runtime_error("submit_task on stopped ThreadPool!");
                }

                if (!autoScalingEnabled_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 避免在未启用自动扩容时过于频繁地尝试提交任务导致 CPU 占用过高
                    continue;
                }
                scale_up();
                continue; // 下一次循环时会再次尝试提交任务
            }

            // not full or stop
            if (stopFlag_.load()) {
                throw std::runtime_error("submit_task on stopped ThreadPool!");
            }
            auto task = [taskPtr]() { (*taskPtr)(); };
            uint64_t sequenceId = nextTaskSequenceId_.fetch_add(1);
            taskQueue_.push(Task(sequenceId, priority, std::function<void()>(task)));
        }
        notEmptyCv_.notify_one();
        break;
    }
    return res;
}

} // namespace wxm
