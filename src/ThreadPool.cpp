/**
 * @file ThreadPool.cpp
 * @brief 一个通用、高效的 C++ 线程池实现。
 * @details 该线程池支持提交任何可调用对象作为任务，并能异步获取任务的返回值。利用了 C++11 的 std::packaged_task 和 std::future 实现任务管理和结果获取。
 * @author wenxingming
 * @date 2025-08-26
 * @note My project address: https://github.com/WenXingming/ThreadPool
 * @cite: https://github.com/progschj/ThreadPool/tree/master
 */

#include "ThreadPool.h"
namespace wxm {

ThreadPool::ThreadPool(int threadsSize, int maxTasksSize, bool openAutoExpandReduce, int maxWaitTimeMs)
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
    if (threadsSize < 1) {
        threadsSize = 1;
        std::cout << "threadsSize set to 1 because input value is less than 1." << std::endl;
    }
    else if (threadsSize > 2 * hardwareSize) {
        threadsSize = 2 * hardwareSize;
        std::cout << "threadsSize set to 2 * hardwareSize because input value is too large (greater than 2 * hardwareSize)." << std::endl;
    }

    for (int i = 0; i < threadsSize; ++i) {
        // auto threadFunc = [this]() {
        //     this->process_task();
        //     };
        // std::thread t(threadFunc);
        std::thread t(&ThreadPool::process_task, this); // 另一种绑定成员函数的方式
        threads_.push_back(std::move(t)); // thread　对象只支持　move，一个线程最多只能由一个 thread 对象持有
    }
    std::cout << "thread pool is created success, size is: " << threads_.size() << std::endl;
}

// 默认构造函数调用有参构造函数
ThreadPool::ThreadPool() : ThreadPool(1, 100, false, 1000) {

}

ThreadPool::~ThreadPool() {
    stopFlag_ = true;
    notEmpty_.notify_all(); // 唤醒所有等待的线程，线程池要终止了
    for (auto& thread : threads_) { 	// 一个线程只能被一个 std::thread 对象管理, 复制构造/赋值被禁用。所以用 &
        if (thread.joinable()) {
            thread.join();
        }
    }
    std::cout << "current thread pool size: " << threads_.size() << ", all threads joined." << std::endl;
    std::cout << "thread pool is destructed success, and tasks are all finished." << std::endl;
}

int ThreadPool::get_thread_pool_size() {
    std::lock_guard<std::mutex> guardLock(threadsMutex_);
    return threads_.size();
}

int ThreadPool::get_current_tasks_size() {
    std::lock_guard<std::mutex> guardLock(tasksMutex_);
    return tasks_.size();
}

void ThreadPool::process_task() {
    while (true) {
        Task task;

        std::unique_lock<std::mutex> uniqueLock(tasksMutex_);
        
        bool ready = this->wait_not_empty_or_stop(uniqueLock);
        // ready == true 表示条件满足（非空或 stop），false 表示超时且任务队列仍空
        // 特判 ready == false 的情况
        if (ready == false) {
            uniqueLock.unlock();
            if (openAutoExpandReduce_) {
                reduce_thread_pool(std::this_thread::get_id()); // 超时，线程池处理速度高于任务提交速度，需要缩减线程池。被认为是耗时操作，手动解锁
            }
            break; // 退出死循环，等待后续 join() 或 detach() 由操作系统回收资源
        }
        // ready == true，条件满足（非空或 stop）
        if (!tasks_.empty()) {						// 优先级高。要先把任务处理完
            task = std::move(tasks_.top());		// std::packaged_task<> 只支持 move，禁止拷贝
            tasks_.pop();
        }
        else return; // 线程池终止。退出循环（线程），后续 join()

        if (task.get_priority() != INT_MIN) { // 取出了任务
            notFull_.notify_one();
            task.run();
        }
    }
}


void ThreadPool::expand_thread_pool() {
    std::unique_lock<std::mutex> uniqueLock(threadsMutex_);

    // 判定异常情况
    int hardwareSize = std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency();
    if (threads_.size() >= 2 * hardwareSize) { // 线程池最大线程数量设定为 2 * hardwareSize（如需要自定义设置再修改类，增添变量 maxThreadsSize）
        std::cout << "thread_pool is MAX_SIZE（2 * hardwareSize）: " << threads_.size() << ", can't be expanded."
            << " you'd better slow down the speed of submitting task.\n";
        return;
    }
    // 扩展线程池
    auto threadFunc = [this]() {
        this->process_task();
        };
    std::thread t(threadFunc);
    threads_.push_back(std::move(t));
    std::cout << "thread_pool auto expand successful, now size is: " << threads_.size() << std::endl;
}

// 将调用该函数的线程从线程池中移除
void ThreadPool::reduce_thread_pool(std::thread::id threadId) {
    std::unique_lock<std::mutex> uniqueLock(threadsMutex_);

    if (threads_.size() <= 1) {
        std::cout << "thread_pool is MIN_SIZE: " << threads_.size() << ", can't be reduced.\n";
        return;
    }

    int index = -1;
    for (int i = 0; i < threads_.size(); ++i) {
        if (threads_[i].get_id() == threadId) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        std::cout << "can't find the thread in thread_pool to reduce.\n";
        return;
    }

    std::thread t = std::move(threads_[index]);
    threads_.erase(threads_.begin() + index);
    uniqueLock.unlock();

    t.detach(); // 需要被销毁的线程本身调用 reduce_thread_pool，它并不能 join() 自己。所以让操作系统回收线程
    std::cout << "thread_pool auto reduce successful, now size is: " << threads_.size() << std::endl;
}

// 等待队列非空或 stop；返回 true 表示条件满足（非空或 stop），false 表示超时且任务队列仍空
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

// 等待队列非满或 stop；返回 true 表示条件满足（非满或 stop），false 表示超时且任务队列仍满
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