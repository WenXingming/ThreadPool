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
#include <mutex>				// 互斥访问任务队列
#include <condition_variable>	// 线程间同步操作：生产者、消费者
#include <atomic>
#include <future>
#include <memory>
#include <chrono>
namespace wxm {


	/// =======================================================================
	/// NOTE: Declaration of class: ThreadPool

	using Task = std::function<void()>; // 任务抽象化，通用 Task，无参无返回值

	class ThreadPool {
	private:
		std::vector<std::thread> threads;
		std::queue<Task> tasks;
		std::mutex tasksMutex;

		std::condition_variable condition; 	// 线程池中线程唤醒
		std::atomic<bool> stopFlag; 		// 构造时初始化为 false；析构时，置为 true。作用是使各个线程退出，否则各个线程在死循环，无法退出从而无法 join()

	public:
		ThreadPool();
		ThreadPool(int _size);
		ThreadPool(const ThreadPool& other) = delete;
		ThreadPool& operator=(const ThreadPool& other) = delete;
		// ThreadPool(const ThreadPool&& other) = delete;
		// ThreadPool& operator=(const ThreadPool&& other) = delete;
		~ThreadPool();


		void consume_task();

		template<typename F, typename... Args>
		auto submit_task(F&& func, Args&&... args)
			-> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))>;
	};



	/// =======================================================================
	/// NOTE: Definition of class's member functions

	/// @attention 模板函数的定义需要放在头文件中和声明在一起。如果放在 cpp 中不会实例化编译，只有在调用该函数时才会根据参数类型生成一个具体的函数版本
	/// @brief func 不加完美转发，则它会把 func 当作一个左值来处理，哪怕调用者传进来的是一个右值（比如临时 lambda）；万能引用配合完美转发：std::forward
	template<typename F, typename ...Args>
	auto ThreadPool::submit_task(F&& func, Args && ...args)
		-> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))> {

		using RetType = decltype(std::forward<F>(func)(std::forward<Args>(args)...));

		// 使用 std::packaged_task 封装任务，以便获取 future
		auto taskPtr = std::make_shared<std::packaged_task<RetType()>>( // packaged_task 禁用拷贝，所以用指针管理
			std::bind(std::forward<F>(func), std::forward<Args>(args)...)
		);
		std::future<RetType> res = taskPtr->get_future();

		{
			std::unique_lock<std::mutex> lock(tasksMutex);
			if (stopFlag) {
				throw std::runtime_error("submit_task on stopped ThreadPool!");
			}

			auto func = [taskPtr]() { // 无参，无返回值的 lambda
				(*taskPtr)();
				};
			tasks.push(std::function<void()>(func));
		}

		condition.notify_one();
		return res;
	}


}