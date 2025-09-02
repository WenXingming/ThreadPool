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

	/// @brief 默认构造函数创建硬件核数的线程数量
	ThreadPool::ThreadPool()
		: ThreadPool(std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency()) {}


	ThreadPool::ThreadPool(int _size)
		: stopFlag(false) {

		int hardwareSize = std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency();
		int size = std::min(hardwareSize, _size);
		for (int i = 0; i < size; ++i) {
			auto threadFunc = [this]() {
				this->process_task();
				};
			std::thread t(threadFunc);
			threads.push_back(std::move(t)); // thread　对象只支持　move，一个线程最多只能由一个 thread 对象持有
		}
		std::cout << "thread pool is created success, size is: " << threads.size() << std::endl;
	}


	ThreadPool::~ThreadPool() {
		stopFlag = true;
		conditionProcess.notify_all();
		for (auto& thread : threads) { 	// 一个线程只能被一个 std::thread 对象管理, 复制构造/赋值被禁用。所以用 &
			thread.join();
		}
		std::cout << "thread pool is destructed success, and tasks are all finished." << std::endl;
	}


	void ThreadPool::process_task() {
		while (true) {
			Task task;
			{
				std::unique_lock<std::mutex> lock(tasksMutex);
				conditionProcess.wait(lock, [this]() {
					return (!tasks.empty() || stopFlag);
					});

				if (!tasks.empty()) {                   // 要先把任务处理完
					task = std::move(tasks.front()); 	// std::packaged_task<> 只支持 move，禁止拷贝
					tasks.pop();
				}
				else if (stopFlag) return;
				else throw std::runtime_error("process_task error!");
			}
			task();
			conditionSubmit.notify_one();
		}
	}


}