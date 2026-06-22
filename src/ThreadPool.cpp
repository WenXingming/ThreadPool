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
    : workerThreads_()
    , workerThreadsMutex_()
    , exitedWorkerThreads_()
    , exitedWorkerThreadsMutex_()
    , taskQueue_()
    , taskQueueMutex_()
    , taskQueueCapacity_(maxTasksSize)
    , nextTaskSequenceId_(0)
    , notEmptyCv_()
    , notFullCv_()
    , autoScalingEnabled_(openAutoExpandReduce)
    , waitTimeoutMs_(maxWaitTimeMs)
    , stopFlag_(false) {

    if (maxTasksSize <= 0) throw std::invalid_argument("maxTasksSize must be positive.");
    if (maxWaitTimeMs <= 0) throw std::invalid_argument("maxWaitTimeMs must be positive.");

    int hardwareSize = std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency();
    if (threadCount < 1) {
        threadCount = 1;
    }
    else if (threadCount > 2 * hardwareSize) {
        threadCount = 2 * hardwareSize;
    }

    for (int i = 0; i < threadCount; ++i) {
        // std::thread worker(&ThreadPool::process_task, this);
        // workerThreads_.push_back(std::move(worker));
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
    join_exited_worker_threads();
}

int ThreadPool::get_pool_size() {
    join_exited_worker_threads();

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
        throw std::invalid_argument("maxTasksSize must be positive.");
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
        throw std::invalid_argument("maxWaitTimeMs must be positive.");
    }
    waitTimeoutMs_ = waitMs;
}

void ThreadPool::process_task() {
    while (true) {
        Task task;
        std::unique_lock<std::mutex> uniqueLock(taskQueueMutex_);
        auto pred = [this]() {
            return stopFlag_.load() || (!taskQueue_.empty());
            };
        bool ready = notEmptyCv_.wait_for(uniqueLock, std::chrono::milliseconds(waitTimeoutMs_.load()), pred);

        // empty && not stop
        if (!ready) {
            uniqueLock.unlock(); // 提前解锁，因为后续逻辑可能较耗时
            if (autoScalingEnabled_) {
                if (scale_down(std::this_thread::get_id())) {
                    break;
                }
            }
            else std::this_thread::yield(); // 普通模式或未启用自动缩容时让出 CPU，避免频繁无效轮询
            continue; // 下一次循环继续取任务执行
        }

        // stop || not empty
        if (stopFlag_.load() && taskQueue_.empty()) {
            break;
        }
        task = std::move(taskQueue_.top());
        taskQueue_.pop();
        uniqueLock.unlock(); // 提前解锁，因为后续执行任务可能较耗时

        notFullCv_.notify_one();
        task.run();
    }
}

void ThreadPool::scale_up() {
    join_exited_worker_threads();

    std::unique_lock<std::mutex> uniqueLock(workerThreadsMutex_);

    int hardwareSize = std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency();
    if (workerThreads_.size() >= static_cast<size_t>(2 * hardwareSize)) {
        return;
    }
    std::thread worker(&ThreadPool::process_task, this);
    workerThreads_.push_back(std::move(worker));
}

bool ThreadPool::scale_down(std::thread::id threadId) {
    std::unique_lock<std::mutex> uniqueLock(workerThreadsMutex_);

    if (workerThreads_.size() <= 1) {
        return false;
    }

    int removeIndex = -1;
    for (size_t i = 0; i < workerThreads_.size(); ++i) {
        if (workerThreads_[i].get_id() == threadId) {
            removeIndex = static_cast<int>(i);
            break;
        }
    }
    if (removeIndex == -1) {
        return false;
    }

    std::thread worker = std::move(workerThreads_[removeIndex]);
    workerThreads_.erase(workerThreads_.begin() + removeIndex);
    uniqueLock.unlock();

    {
        std::lock_guard<std::mutex> guardLock(exitedWorkerThreadsMutex_);
        exitedWorkerThreads_.push_back(std::move(worker));
    }

    return true;
}

void ThreadPool::join_exited_worker_threads() {
    std::vector<std::thread> threadsToJoin;
    {
        std::lock_guard<std::mutex> guardLock(exitedWorkerThreadsMutex_);
        threadsToJoin.swap(exitedWorkerThreads_);
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
        std::lock_guard<std::mutex> guardLock(exitedWorkerThreadsMutex_);
        for (auto& thread : deferredThreads) {
            exitedWorkerThreads_.push_back(std::move(thread));
        }
    }
}

} // namespace wxm
