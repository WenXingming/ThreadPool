/// @attention: 通用线程池。支持携带任何参数的任务函数（其返回值也可获取）
#pragma once
#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>				// 互斥访问任务队列
#include <condition_variable>	// 线程间同步操作：生产者、消费者
#include <atomic>
#include <future>
#include <memory>
#include <chrono>

using Task = std::function<void()>; // 任务抽象化，通用 Task，无参无返回值

class Thread_Pool {
private:
    static std::unique_ptr<Thread_Pool> threadPool; // 单例模式

    std::vector<std::thread> threads;
    std::queue<Task> tasks;
    std::mutex tasksMutex;

    std::condition_variable condition; 	// 线程池中线程唤醒
    std::atomic<bool> stopFlag; 		// 构造时初始化为 false；析构时，置为 true，使各个线程退出。否则各个线程在死循环，无法退出从而无法 join()

    // 单例模式，构造函数私有
    Thread_Pool() : Thread_Pool(std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency()) {}
    Thread_Pool(int _size) : stopFlag(false) {
        int hardwareSize = std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency();
        int size = std::min(hardwareSize, _size);
        for (int i = 0; i < size; ++i) {
            std::thread t(&Thread_Pool::consume_task, this); // windows 上不取地址没事，ubuntu 上编译报错（为了通用可以用 lambda 或 std::bind）
            threads.push_back(std::move(t)); 	// thread　对象只支持　move，一个线程最多只能由一个 thread 对象持有
        }
        std::cout << "thread pool is created success, size is: " << threads.size() << std::endl;
    }
public:
    Thread_Pool(const Thread_Pool& other) = delete;
    Thread_Pool& operator=(const Thread_Pool& other) = delete;
    Thread_Pool(const Thread_Pool&& other) = delete;
    Thread_Pool& operator=(const Thread_Pool&& other) = delete;

    ~Thread_Pool() {
        stopFlag = true;
        condition.notify_all(); 		// 唤醒所有线程，让它们退出。感觉会有一个潜在的问题，如果此时剩的任务特别多...
        for (auto& thread : threads) { 	// 一个线程只能被一个 std::thread 对象管理, 复制构造/赋值被禁用。所以用 &
            thread.join();
        }
        std::cout << "thread pool is destructed success, and tasks are all finished." << std::endl;
    }

    static Thread_Pool& get_instance() {
        if (threadPool == nullptr) {    // 非线程安全，可以加锁
            threadPool.reset(new Thread_Pool());
        }
        return *threadPool;
    }

    void consume_task() {
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(tasksMutex);
                condition.wait(lock, [this]() {
                    return (!tasks.empty() || stopFlag);
                    });

                if (!tasks.empty()) {                   // 要先把任务处理完
                    task = std::move(tasks.front()); 	// std::packaged_task<> 只支持 move
                    tasks.pop();
                }
                else if (stopFlag) return;
            }
            task();
        }
    }

    // func 不加完美转发，则它会把 func 当作一个左值来处理，哪怕调用者传进来的是一个右值（比如临时 lambda）
    // 万能引用配合完美转发：std::forward
    template<typename F, typename... Args>
    auto submit_task(F&& func, Args&&... args)
        -> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))> {

        using RetType = decltype(std::forward<F>(func)(std::forward<Args>(args)...));

        // 使用 std::packaged_task 封装任务，以便获取 future
        auto task_ptr = std::make_shared<std::packaged_task<RetType()>>( // packaged_task 无法拷贝，所以用指针传入 lambda
            std::bind(std::forward<F>(func), std::forward<Args>(args)...)
        );
        std::future<RetType> res = task_ptr->get_future();

        {
            std::unique_lock<std::mutex> lock(tasksMutex);
            if (stopFlag) {
                throw std::runtime_error("submit_task on stopped Thread_Pool!");
            }

            // 使用 std::function 封装一个无参、无返回值的 lambda
            // 这个 lambda 在被调用时会执行 packaged_task
            std::function<void()> func = std::function<void()>([task_ptr]() {
                (*task_ptr)();
                });
            tasks.push(func);
        }

        condition.notify_one();
        return res;
    }
};

std::unique_ptr<Thread_Pool> Thread_Pool::threadPool = nullptr;