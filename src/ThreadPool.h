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
namespace wxm {

	/// =======================================================================
	/// NOTE: Declaration of class: Task（Abstract task）
	class Task {
	private:
		int priority;										// 优先级，数字越大优先级越高
		std::chrono::steady_clock::time_point timestamp;	// 时间戳（不用整数，精度不够），用于 FCFS
		std::function<void()> function;						// 实际的任务函数

	public:
		// 安全：在类定义内部实现的成员函数默认是内联的（多个源文件同时包含 task 类，链接时不会重定义）
		Task() : priority(INT_MIN), function(nullptr) {
			timestamp = std::chrono::steady_clock::now();
		}

		Task(std::function<void()> _func, int _priority = 0) // 任务函数是通用 function（只有线程池 submit_task() 入口是模板参数）
			: priority(_priority), function(_func) {
			timestamp = std::chrono::steady_clock::now();
		}

		// 注意比较 (Compare) 形参的定义，使得若其第一参数在弱序中先于其第二参数则返回 true
		bool operator<(const Task& other) const {
			if (priority != other.priority) {
				return priority < other.priority;		// 优先级高的在前
			}
			else {
				return timestamp > other.timestamp;		// 时间戳小的在前（FCFS）
			}
		}

		void execute() {
			if (function) {
				function();
				if (priority != INT_MIN && priority != 0) std::cout << "priority is: " << priority << std::endl; // debug
			}
			else throw std::runtime_error("task's function is empty, can't not execute.");
		}

		int getPriority() const { return priority; }
		/// @brief 将 time_point 转换为自纪元以来的秒（seconds）计数
		int64_t getTimestamp() const {
			auto duration = timestamp.time_since_epoch();
			return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
		}
	};


	/// =======================================================================
	/// NOTE: Declaration of class: ThreadPool
	class ThreadPool {
	private:
		std::vector<std::thread> threads;
		std::priority_queue<Task> tasks;
		std::mutex tasksMutex;							// 保证对任务队列的操作线程安全
		const int maxTasksSize;

		std::condition_variable conditionProcess; 		// 处理任务的线程等待和被唤醒
		std::condition_variable conditionSubmit;		// 提交任务的线程等待和被唤醒
		std::atomic<bool> stopFlag; 					// 线程池停止标志。作用是使线程池各个线程从循环退出，否则各个线程在循环无法退出从而无法 join()

		bool openAutoExpandReduce;
		const int maxWaitTime;							// 设置条件变量等待时间。添加任务和取任务时，如果队列满或空等待超过该时间，则动态扩缩线程池。
		std::mutex threadsMutex;						// 扩展线程池时保证线程安全

	public:
		/// @param _threadsSize 线程池线程数量
		/// @param _maxTasksSize 任务队列大小
		/// @param _isAutoExpandReduce 是否打开自动线程池收缩功能
		/// @param _maxWaitTime millseconds，理论上期望处理任务的速率 velocity >= (1 / _maxWaitTime) * numOfThread（单位数量 / s），其中 numOfThread 是提交任务的线程的数量。若线程池大小不够，其会自动扩容直到满足该速率；如果扩容到最大核心数也达不到该速度，此时硬件能力不够没有办法，线程池大小保持在最大核心数。例如：如果期望每秒处理至少 5 个任务，单任务提交线程下则 5 = (1 / _maxWaitTime) * 1，计算得到应设定 _maxWaitTime = 0.2s = 200 ms
		ThreadPool(int _threadsSize, int _maxTasksSize = 50, bool _openAutoExpandReduce = false, int _maxWaitTime = 1000); // 
		ThreadPool();
		ThreadPool(const ThreadPool& other) = delete;
		ThreadPool& operator=(const ThreadPool& other) = delete;
		ThreadPool(const ThreadPool&& other) = delete;
		ThreadPool& operator=(const ThreadPool&& other) = delete;
		~ThreadPool();
		int get_thread_pool_size();

		template<typename F, typename... Args>
		auto submit_task(F&& func, Args&&... args)
			-> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))>;
		template<typename F, typename... Args>
		auto submit_task(int _priority, F&& func, Args&&... args)
			-> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))>;
		void process_task();


		void expand_thread_pool();
		void reduce_thread_pool(std::thread::id threadId); // velocity <= (1 / _maxWaitTime) * numOfThreads（单位数量 / s），其中 numOfThreads 是线程池中线程的数量

	};





	/// @attention 模板函数的定义需要放在头文件中和声明在一起（需要编译时可见，在调用该函数时才会根据参数类型生成一个具体的函数版本）
	/// @brief func 不加完美转发，则它会把 func 当作一个左值来处理，哪怕调用者传进来的是一个右值（比如临时 lambda）；std::forward 万能引用配合完美转发
	template<typename F, typename ...Args>
	auto ThreadPool::submit_task(F&& func, Args && ...args)
		-> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))> {

		// 使用 std::packaged_task 封装任务，以便获取 future。packaged_task 禁用拷贝，所以用指针管理
		using RetType = decltype(std::forward<F>(func)(std::forward<Args>(args)...));
		auto taskPtr = std::make_shared<std::packaged_task<RetType()>>(
			std::bind(std::forward<F>(func), std::forward<Args>(args)...)
		);
		std::future<RetType> res = taskPtr->get_future();

		// 提交任务到任务队列
		{
			std::unique_lock<std::mutex> uniqueLock(tasksMutex);
			bool retWait = conditionSubmit.wait_for(uniqueLock, std::chrono::milliseconds(maxWaitTime), [this]() {
				return (tasks.size() < maxTasksSize) || stopFlag;
				}
			);

			if (retWait) {
				if (tasks.size() < maxTasksSize) {
					auto funcLambda = [taskPtr]() { (*taskPtr)(); }; // 无参，无返回值的 lambda
					auto func = std::function<void()>(funcLambda);
					auto task = std::move(Task(func));
					tasks.push(task);
				}
				else throw std::runtime_error("submit_task on stopped ThreadPool!");
			}
			else { // 超时，需要扩增线程池提高处理能力
				auto funcLambda = [taskPtr]() { (*taskPtr)(); };
				auto func = std::function<void()>(funcLambda);
				auto task = std::move(Task(func));
				tasks.push(task);

				uniqueLock.unlock();
				if (openAutoExpandReduce) expand_thread_pool(); // 认为是耗时操作，所以先解锁 taskQue 的锁
			}
		}
		conditionProcess.notify_one();

		return res;
	}

	/// @brief 函数重载，提交任务时支持设置优先级（int），值越大优先级越高。用户可以将优先级看作一个 rank，或者自己预估的执行时间（最短任务优先调度）） 
	template<typename F, typename... Args>
	auto wxm::ThreadPool::submit_task(int _priority, F&& func, Args && ...args)
		-> std::future<decltype(std::forward<F>(func)(std::forward<Args>(args)...))> {

		using RetType = decltype(std::forward<F>(func)(std::forward<Args>(args)...));
		auto taskPtr = std::make_shared<std::packaged_task<RetType()>>(
			std::bind(std::forward<F>(func), std::forward<Args>(args)...)
		);
		std::future<RetType> res = taskPtr->get_future();

		{
			std::unique_lock<std::mutex> uniqueLock(tasksMutex);
			bool retWait = conditionSubmit.wait_for(uniqueLock, std::chrono::milliseconds(maxWaitTime), [this]() {
				return (tasks.size() < maxTasksSize) || stopFlag;
				}
			);

			if (retWait) {
				if (tasks.size() < maxTasksSize) {
					auto funcLambda = [taskPtr]() { (*taskPtr)(); };
					auto func = std::function<void()>(funcLambda);
					auto task = std::move(Task(func, _priority));
					tasks.push(task);
				}
				else throw std::runtime_error("submit_task on stopped ThreadPool!");
			}
			else {
				auto funcLambda = [taskPtr]() { (*taskPtr)(); };
				auto func = std::function<void()>(funcLambda);
				auto task = std::move(Task(func, _priority));
				tasks.push(task);

				uniqueLock.unlock();
				if (openAutoExpandReduce) expand_thread_pool();
			}
		}
		conditionProcess.notify_one();

		return res;
	}

}