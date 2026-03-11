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
    , tasks_()
    , tasksMutex_()
    , maxTasksSize_(maxTasksSize)
    , notEmpty_()
    , notFull_()
    , stopFlag_(false)
    , openAutoExpandReduce_(openAutoExpandReduce)
    , maxWaitTime_(maxWaitTimeMs)
    , threadsMutex_() {
    int hardwareSize = std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency();
    if (threadCount < 1) {
        threadCount = 1;
        std::cout << "threadCount set to 1 because input value is less than 1." << std::endl;
    }
    else if (threadCount > 2 * hardwareSize) {
        threadCount = 2 * hardwareSize;
        std::cout << "threadCount set to 2 * hardwareSize because input value is too large." << std::endl;
    }
    initialize_worker_threads(threadCount);
    std::cout << "thread pool is created success, size is: " << threads_.size() << std::endl;
}

ThreadPool::ThreadPool() : ThreadPool(1, 100, false, 1000) {
}

ThreadPool::~ThreadPool() {
    stopFlag_ = true;
    notEmpty_.notify_all();
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    std::cout << "current thread pool size: " << threads_.size() << ", all threads joined." << std::endl;
    std::cout << "thread pool is destructed success, and tasks are all finished." << std::endl;
}

int ThreadPool::get_thread_pool_size() {
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
        bool ready = this->wait_not_empty_or_stop(uniqueLock);
        if (!ready) {
            uniqueLock.unlock();
            if (openAutoExpandReduce_) {
                reduce_thread_pool(std::this_thread::get_id());
            }
            break;
        }
        if (!tasks_.empty()) {
            task = std::move(tasks_.top());
            tasks_.pop();
        }
        else {
            return;
        }

        if (task.get_priority() != INT_MIN) {
            notFull_.notify_one();
            task.run();
        }
    }
}

void ThreadPool::expand_thread_pool() {
    std::unique_lock<std::mutex> uniqueLock(threadsMutex_);

    int hardwareSize = std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency();
    if (threads_.size() >= static_cast<size_t>(2 * hardwareSize)) {
        std::cout << "thread_pool is MAX_SIZE (2 * hardwareSize): " << threads_.size() << ", can't be expanded."
            << " you'd better slow down the speed of submitting task.\n";
        return;
    }
    std::thread worker(&ThreadPool::process_task, this);
    threads_.push_back(std::move(worker));
    std::cout << "thread_pool auto expand successful, now size is: " << threads_.size() << std::endl;
}

void ThreadPool::reduce_thread_pool(std::thread::id threadId) {
    std::unique_lock<std::mutex> uniqueLock(threadsMutex_);

    if (threads_.size() <= 1) {
        std::cout << "thread_pool is MIN_SIZE: " << threads_.size() << ", can't be reduced.\n";
        return;
    }

    int removeIndex = -1;
    for (size_t i = 0; i < threads_.size(); ++i) {
        if (threads_[i].get_id() == threadId) {
            removeIndex = static_cast<int>(i);
            break;
        }
    }
    if (removeIndex == -1) {
        std::cout << "can't find the thread in thread_pool to reduce.\n";
        return;
    }

    std::thread worker = std::move(threads_[removeIndex]);
    threads_.erase(threads_.begin() + removeIndex);
    uniqueLock.unlock();

    worker.detach();
    std::cout << "thread_pool auto reduce successful, now size is: " << threads_.size() << std::endl;
}

bool ThreadPool::wait_not_empty_or_stop(std::unique_lock<std::mutex>& lock) {
    auto pred = [this]() {
        return stopFlag_.load() ||
            (!tasks_.empty());
        };
    return notEmpty_.wait_for(
        lock,
        std::chrono::milliseconds(maxWaitTime_.load()),
        pred
    );
}

bool ThreadPool::wait_not_full_or_stop(std::unique_lock<std::mutex>& lock) {
    auto pred = [this]() {
        return stopFlag_.load() ||
            (tasks_.size() < static_cast<size_t>(maxTasksSize_.load()));
        };
    return notFull_.wait_for(
        lock,
        std::chrono::milliseconds(maxWaitTime_.load()),
        pred
    );
}

}