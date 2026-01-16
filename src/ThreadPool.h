/**
 * @file ThreadPool.h
 * @brief 一个通用、高效的 C++ 线程池实现。
 * @details 该线程池支持提交任何可调用对象作为任务，并能异步获取任务的返回值。利用了 C++11 的 std::packaged_task 和 std::future 实现任务管理和结果获取。
 * @author wenxingming
 * @date 2025-08-26
 * @note My project address: https://github.com/WenXingming/ThreadPool
 * @cite: https://github.com/progschj/ThreadPool/tree/master
 */

#pragma once
#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>				// 多线程互斥访问任务队列（资源竞争）
#include <condition_variable>	// 线程间同步操作：生产者、消费者
#include <atomic>
#include <future>
#include <memory>
#include <chrono>
#include <cassert>
#include <climits>

#include "Task.h"

namespace wxm {

class ThreadPool {
private:
    std::vector<std::thread> threads_;
    std::priority_queue<Task> tasks_;
    std::mutex tasksMutex_;							// 保证对任务队列的操作线程安全
    std::atomic<int> maxTasksSize_;

    std::condition_variable notEmpty_; 		// 处理任务的线程等待和被唤醒
    std::condition_variable notFull_;		// 提交任务的线程等待和被唤醒
    std::atomic<bool> stopFlag_; 					// 线程池停止标志。作用是使线程池各个线程从循环退出，否则各个线程在循环无法退出从而无法 join()

    std::atomic<bool> openAutoExpandReduce_;
    std::atomic<int> maxWaitTime_;					// 设置条件变量等待时间。添加任务和取任务时，如果队列满或空等待超过该时间，则动态扩缩线程池。
    std::mutex threadsMutex_;						// 扩展线程池时保证线程安全

private:
    void process_task();

    void expand_thread_pool();
    void reduce_thread_pool(std::thread::id threadId); // velocity >= (1 / _maxWaitTime) * numOfThreads（单位数量 / s），其中 numOfThreads 是线程池中线程的数量

    // 等待队列非满或 stop；返回 true 表示条件满足（非满或 stop），false 表示超时且仍满
    bool wait_not_full_or_stop(std::unique_lock<std::mutex>& lock);
    bool wait_not_empty_or_stop(std::unique_lock<std::mutex>& lock);

public:
    // threadsSize 线程池线程数量
    // maxTasksSize 任务队列大小
    // openAutoExpandReduce 是否打开自动线程池收缩功能
    // maxWaitTime millseconds，理论上期望处理任务的速率 velocity >= (1 / maxWaitTime) * numOfThread（单位数量 / s），其中 numOfThread 是提交任务的线程的数量。若线程池大小不够，其会自动扩容直到满足该速率；如果扩容到最大核心数也达不到该速度，此时硬件能力不够没有办法，线程池大小保持在最大核心数。例如：如果期望每秒处理至少 5 个任务，单任务提交线程下则 5 = (1 / maxWaitTime) * 1，计算得到应设定 maxWaitTime = 0.2s = 200 ms
    ThreadPool(int threadsSize = 0, int maxTasksSize = 100, bool openAutoExpandReduce = false, int maxWaitTimeMs = 1000);
    ThreadPool();

    ThreadPool(const ThreadPool& other) = delete;
    ThreadPool& operator=(const ThreadPool& other) = delete;
    ThreadPool(const ThreadPool&& other) = delete;
    ThreadPool& operator=(const ThreadPool&& other) = delete;
    
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
};


// 模板函数的定义需要放在头文件中和声明在一起（需要编译时可见，在调用该函数时才会根据参数类型生成一个具体的函数版本）
// 若 func 不加完美转发，则它会把 func 当作一个左值来处理，哪怕调用者传进来的是一个右值（比如临时 lambda）；std::forward 万能引用配合完美转发
template<typename F, typename ...Args>
auto ThreadPool::submit_task(F&& func, Args&& ...args)
-> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))> {

    auto res = this->submit_task(INT_MIN, std::forward<F>(func), std::forward<Args>(args)...); // 调用有优先级参数的重载版本，避免代码重复
    return res;
}


// 函数重载，提交任务时支持设置优先级（int），值越大优先级越高。用户可以将优先级看作一个 rank，或者自己预估的执行时间（最短任务优先调度）） 
template<typename F, typename... Args>
auto wxm::ThreadPool::submit_task(int priority, F&& func, Args&& ...args)
-> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))> {

    // 使用 std::packaged_task 封装任务（无参），以便获取 future。packaged_task 禁用拷贝，所以用指针管理
    using RetType = decltype(std::forward<F>(func)(std::forward<Args>(args)...));
    auto taskPtr = std::make_shared<std::packaged_task<RetType()>>(
        std::bind(std::forward<F>(func), std::forward<Args>(args)...)
    );
    std::future<RetType> res = taskPtr->get_future();

    // 提交任务到任务队列。队列可能满，需要循环尝试提交
    // bool enqueued = false;
    while (/* !enqueued */ true) {
        std::unique_lock<std::mutex> uniqueLock(tasksMutex_);
        bool ready = this->wait_not_full_or_stop(uniqueLock);
        if (!ready) { // 超时，任务队列满了。需要扩增线程池提高处理能力
            uniqueLock.unlock(); // 认为是耗时操作，所以先解锁 taskQue 的锁
            if (openAutoExpandReduce_) {
                expand_thread_pool();
            }
            continue; // 重新尝试提交任务
        }

        // ready == true. 即 stopFlag_ == true 或 tasks_.size() < maxTasksSize_
        // stopFlag_ == true 优先级更高
        if (stopFlag_) {
            throw std::runtime_error("submit_task on stopped ThreadPool!"); // 程序控制流立即返回给调用者，后续代码不会执行
        }
        // tasks_.size() < maxTasksSize_
        auto task = [taskPtr]() { (*taskPtr)(); };
        auto func = std::function<void()>(task);
        tasks_.push(Task(func, priority));
        // enqueued = true;
        break;
    }
    notEmpty_.notify_one();
    return res;
}


}
