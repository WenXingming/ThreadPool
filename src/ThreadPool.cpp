// ============================================================================
// ThreadPool - 线程池类实现
// ============================================================================
// 核心功能和接口：
// 1. 管理工作线程的创建、回收和生命周期。
// 2. 负责任务队列的消费、优先级调度和等待逻辑。
// 3. 在启用时执行线程池自动扩缩容。
// ============================================================================

#include "ThreadPool.h"

namespace wxm {

ThreadPool::ThreadPool(int threadCount, int maxTasksSize, bool openAutoExpandReduce, int maxWaitTimeMs)
    : threads_()
    , finishedThreads_()
    , tasks_()
    , tasksMutex_()
    , finishedMutex_()
    , maxTasksSize_(maxTasksSize)
    , notEmpty_()
    , notFull_()
    , stopFlag_(false)
    , openAutoExpandReduce_(openAutoExpandReduce)
    , maxWaitTime_(maxWaitTimeMs)
    , threadsMutex_() {

    if (maxTasksSize <= 0) {
        throw std::invalid_argument("maxTasksSize must be positive.");
    }
    if (maxWaitTimeMs <= 0) {
        throw std::invalid_argument("maxWaitTimeMs must be positive.");
    }

    int hardwareSize = std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency();
    if (threadCount < 1) {
        threadCount = 1;
    }
    else if (threadCount > 2 * hardwareSize) {
        threadCount = 2 * hardwareSize;
    }
    initialize_worker_threads(threadCount);
}

ThreadPool::~ThreadPool() {
    stopFlag_ = true;
    notEmpty_.notify_all();
    notFull_.notify_all();

    std::vector<std::thread> threadsToJoin;
    {
        std::lock_guard<std::mutex> guardLock(threadsMutex_);
        threadsToJoin.swap(threads_);
    }

    for (auto& thread : threadsToJoin) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    cleanup_finished_threads();
}

int ThreadPool::get_thread_pool_size() {
    cleanup_finished_threads();

    std::lock_guard<std::mutex> guardLock(threadsMutex_);
    return static_cast<int>(threads_.size());
}

int ThreadPool::get_current_tasks_size() {
    std::lock_guard<std::mutex> guardLock(tasksMutex_);
    return static_cast<int>(tasks_.size());
}

void ThreadPool::initialize_worker_threads(int threadCount) {
    for (int i = 0; i < threadCount; ++i) {
        std::thread worker(&ThreadPool::process_task, this);
        threads_.push_back(std::move(worker));
    }
}

void ThreadPool::process_task() {
    while (true) {
        Task task;
        std::unique_lock<std::mutex> uniqueLock(tasksMutex_);
        auto pred = [this]() {
            return stopFlag_.load() || (!tasks_.empty());
            };
        bool ready = notEmpty_.wait_for(uniqueLock, std::chrono::milliseconds(maxWaitTime_.load()), pred);

        // empty && not stop
        if (!ready) {
            uniqueLock.unlock(); // 提前解锁，因为后续逻辑可能较耗时
            if (openAutoExpandReduce_) {
                if (reduce_thread_pool(std::this_thread::get_id())) {
                    break;
                }
            }
            else std::this_thread::yield(); // 普通模式或未启用自动缩容时让出 CPU，避免频繁无效轮询
            continue; // 下一次循环继续取任务执行
        }

        // stop || not empty
        if (stopFlag_.load() && tasks_.empty()) {
            break;
        }
        task = std::move(tasks_.top());
        tasks_.pop();
        uniqueLock.unlock(); // 提前解锁，因为后续执行任务可能较耗时

        notFull_.notify_one();
        task.run();
    }
}

void ThreadPool::expand_thread_pool() {
    cleanup_finished_threads();

    std::unique_lock<std::mutex> uniqueLock(threadsMutex_);

    int hardwareSize = std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency();
    if (threads_.size() >= static_cast<size_t>(2 * hardwareSize)) {
        return;
    }
    std::thread worker(&ThreadPool::process_task, this);
    threads_.push_back(std::move(worker));
}

bool ThreadPool::reduce_thread_pool(std::thread::id threadId) {
    std::unique_lock<std::mutex> uniqueLock(threadsMutex_);

    if (threads_.size() <= 1) {
        return false;
    }

    int removeIndex = -1;
    for (size_t i = 0; i < threads_.size(); ++i) {
        if (threads_[i].get_id() == threadId) {
            removeIndex = static_cast<int>(i);
            break;
        }
    }
    if (removeIndex == -1) {
        return false;
    }

    std::thread worker = std::move(threads_[removeIndex]);
    threads_.erase(threads_.begin() + removeIndex);
    uniqueLock.unlock();

    {
        std::lock_guard<std::mutex> guardLock(finishedMutex_);
        finishedThreads_.push_back(std::move(worker));
    }

    return true;
}

void ThreadPool::cleanup_finished_threads() {
    std::vector<std::thread> threadsToJoin;
    {
        std::lock_guard<std::mutex> guardLock(finishedMutex_);
        threadsToJoin.swap(finishedThreads_);
    }

    std::vector<std::thread> deferredThreads;
    for (auto& thread : threadsToJoin) {
        if (!thread.joinable()) {
            continue;
        }
        if (thread.get_id() == std::this_thread::get_id()) {
            deferredThreads.push_back(std::move(thread));
            continue;
        }
        thread.join();
    }

    if (!deferredThreads.empty()) {
        std::lock_guard<std::mutex> guardLock(finishedMutex_);
        for (auto& thread : deferredThreads) {
            finishedThreads_.push_back(std::move(thread));
        }
    }
}

} // namespace wxm
