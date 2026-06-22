// ============================================================================
// ThreadPool - 线程池类实现
// ============================================================================
// 核心功能和接口：
// 1. 管理工作线程的创建、回收和生命周期。
// 2. 负责任务队列的消费、优先级调度和等待逻辑。
// 3. 在启用时执行线程池自动扩缩容。
// ============================================================================

#include "ThreadPool.h"
#include <algorithm>

namespace wxm {

ThreadPool::ThreadPool(int threadCount, int queueCapacity, bool autoScalingEnabled, int waitTimeoutMs)
    : workerThreads_()
    , workerThreadsMutex_()
    , exitedWorkerThreads_()
    , exitedWorkerThreadsMutex_()
    , taskQueue_()
    , taskQueueMutex_()
    , taskQueueCapacity_(queueCapacity)
    , nextTaskSequenceId_(0)
    , notEmptyCv_()
    , notFullCv_()
    , autoScalingEnabled_(autoScalingEnabled)
    , waitTimeoutMs_(waitTimeoutMs)
    , stopFlag_(false) {

    if (queueCapacity <= 0) {
        throw std::invalid_argument("queueCapacity must be positive.");
    }
    if (waitTimeoutMs <= 0) {
        throw std::invalid_argument("waitTimeoutMs must be positive.");
    }

    int hardwareSize = std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency();
    if (threadCount < 1) {
        threadCount = 1;
    }
    else if (threadCount > 2 * hardwareSize) {
        threadCount = 2 * hardwareSize;
    }

    for (int i = 0; i < threadCount; ++i) {
        workerThreads_.emplace_back(&ThreadPool::process_task, this);
    }
}

ThreadPool::~ThreadPool() {
    stopFlag_ = true;
    notEmptyCv_.notify_all();
    notFullCv_.notify_all();

    std::vector<std::thread> threadsToJoin;
    {
        std::lock_guard<std::mutex> guardLock(workerThreadsMutex_);
        threadsToJoin.swap(workerThreads_);
    }

    for (auto& thread : threadsToJoin) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    cleanup_exited_worker_threads();
}

int ThreadPool::get_pool_size() {
    std::lock_guard<std::mutex> guardLock(workerThreadsMutex_);
    return static_cast<int>(workerThreads_.size());
}

int ThreadPool::get_queue_size() {
    std::lock_guard<std::mutex> guardLock(taskQueueMutex_);
    return static_cast<int>(taskQueue_.size());
}

int ThreadPool::get_queue_capacity() {
    return taskQueueCapacity_;
}

void ThreadPool::set_queue_capacity(int size) {
    if (size <= 0) {
        throw std::invalid_argument("queueCapacity must be positive.");
    }
    taskQueueCapacity_ = size;
}

void ThreadPool::enable_auto_scaling() {
    autoScalingEnabled_ = true;
}

void ThreadPool::disable_auto_scaling() {
    autoScalingEnabled_ = false;
}

int ThreadPool::get_wait_timeout_ms() {
    return waitTimeoutMs_;
}

void ThreadPool::set_wait_timeout_ms(int waitMs) {
    if (waitMs <= 0) {
        throw std::invalid_argument("waitTimeoutMs must be positive.");
    }
    waitTimeoutMs_ = waitMs;
}

void ThreadPool::process_task() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> uniqueLock(taskQueueMutex_);
            bool ready = notEmptyCv_.wait_for(uniqueLock, std::chrono::milliseconds(waitTimeoutMs_.load()), [this]() {
                return stopFlag_.load() || (!taskQueue_.empty());
                });

            // not stop && empty
            if (!ready) {
                uniqueLock.unlock(); // 提前解锁，因为后续逻辑可能较耗时

                if (!autoScalingEnabled_) { // 未启用自动缩容时让出 CPU，避免频繁无效轮询
                    std::this_thread::yield();
                    continue;
                }

                if (scale_down(std::this_thread::get_id())) { // 开了就尝试缩容；缩容成功则退出当前线程
                    break;
                }

                std::this_thread::yield(); // 缩容未成功
                continue;
            }

            // stop || not empty
            if (stopFlag_.load() && taskQueue_.empty()) {
                break;
            }
            task = taskQueue_.top();
            taskQueue_.pop();
        }

        notFullCv_.notify_one();
        task.run(); // 提前解锁，因为后续执行任务可能较耗时
    }
}

void ThreadPool::scale_up() {
    cleanup_exited_worker_threads();

    std::unique_lock<std::mutex> uniqueLock(workerThreadsMutex_);

    size_t hardwareSize = std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency();
    size_t maxWorkerThreadSize = 2 * hardwareSize;
    if (workerThreads_.size() >= maxWorkerThreadSize) {
        return;
    }
    workerThreads_.emplace_back(&ThreadPool::process_task, this);
}

bool ThreadPool::scale_down(std::thread::id threadId) {
    std::thread removeThread;
    {
        std::unique_lock<std::mutex> uniqueLock(workerThreadsMutex_);
        if (workerThreads_.size() <= 1) {
            return false;
        }

        auto removeThreadIt = std::find_if(workerThreads_.begin(), workerThreads_.end(),
            [threadId](const std::thread& workerThread) {
                return workerThread.get_id() == threadId;
            }
        );

        if (removeThreadIt == workerThreads_.end()) {
            return false;
        }
        removeThread = std::move(*removeThreadIt);
        workerThreads_.erase(removeThreadIt);
    }
    {
        std::lock_guard<std::mutex> guardLock(exitedWorkerThreadsMutex_);
        exitedWorkerThreads_.push_back(std::move(removeThread));
    }

    return true;
}

void ThreadPool::cleanup_exited_worker_threads() {
    std::vector<std::thread> threadsToJoin;
    {
        std::lock_guard<std::mutex> guardLock(exitedWorkerThreadsMutex_);
        threadsToJoin.swap(exitedWorkerThreads_);
    }

    std::vector<std::thread> deferredThreads;
    for (auto& thread : threadsToJoin) {
        if (thread.get_id() == std::this_thread::get_id()) {
            deferredThreads.push_back(std::move(thread));
            continue;
        }
        if (!thread.joinable()) {
            continue;
        }
        thread.join();
    }

    if (deferredThreads.empty()) {
        return;
    }
    {
        std::lock_guard<std::mutex> guardLock(exitedWorkerThreadsMutex_);
        for (auto& thread : deferredThreads) {
            exitedWorkerThreads_.push_back(std::move(thread));
        }
    }
}

} // namespace wxm
