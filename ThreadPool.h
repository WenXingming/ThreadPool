/// NOTE: 通用线程池。支持携带任何参数的任务函数（其返回值也可获取）
#pragma once
#include <queue>
#include <functional>
#include <thread>
#include <future>
#include <vector>
#include <mutex>				
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <chrono>

class ThreadPool {
private:
	std::vector<std::thread> threads;
	std::queue<std::packaged_task<void()>> tasks; // 为了将任务抽象化，参数返回值都是空
	std::mutex tasksMutex;

	std::condition_variable condition; 	
	std::atomic<bool> stopFlag; 		// 析构时，置 stopFlag 为 true，使各个线程退出。否则各个线程在死循环，无法退出从而无法 join()

public:
	ThreadPool(int _size = std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency()); // 在 C++ 中，默认参数只能在函数声明中指定，而不能在函数定义中再次指定。这样做是为了确保在调用函数时只有一个唯一的默认参数值
	~ThreadPool();
	// 禁止拷贝构造和拷贝赋值
	ThreadPool(const ThreadPool& other) = delete;
	ThreadPool& operator=(const ThreadPool& other) = delete;

	void consumeTask();

	template<typename FunctionType, typename... ArgsTypes>
	auto submitTask(FunctionType&& func, ArgsTypes&&... args)
		-> std::future<decltype(std::forward<FunctionType>(func)(std::forward<ArgsTypes>(args)...))>; // 传入的函数指针、参数；至于返回值使用自动后置推导
};

ThreadPool::ThreadPool(int _size) : stopFlag(false) {
	int hardwareThreadsNum = std::thread::hardware_concurrency() == 0 ? 2 : std::thread::hardware_concurrency();
	int size = std::min(hardwareThreadsNum, _size);
	for (int i = 0; i < size; ++i) {
		std::thread t(&ThreadPool::consumeTask, this);
		threads.push_back(std::move(t)); 	// thread　对象只支持　move，一个线程最多只能由一个 thread 对象持有
	}
	std::cout << "thread pool size:" << threads.size() << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
}

ThreadPool::~ThreadPool() {
	stopFlag = true;
	condition.notify_all(); 		// 唤醒所有线程，让它们退出
	for (auto& thread : threads) { 	// 一个线程只能被一个 std::thread 对象管理, 复制构造/赋值被禁用。所以用 &
		thread.join();
	}
	std::cout << "thread pool is destructed success, and tasks are all finished." << std::endl;
}

void ThreadPool::consumeTask() {
	while (true) {
		std::packaged_task<void()> task;
		{
			std::unique_lock<std::mutex> uniqueLock(tasksMutex);
			condition.wait(uniqueLock, [this]() {
				return (!tasks.empty() || stopFlag); // 当 stopFlag 时也要停止阻塞，此时要进行处理退出线程
				});
			if (tasks.empty() /* && stopFlag */) return;
			// 队列不空
			task = std::move(tasks.front()); // std::packaged_task<> 只支持 move
			tasks.pop();
		}
		task();
	}
}

// template<typename FunctionType, typename... Args>
// auto ThreadPool::submitTask(FunctionType&& func, Args&&... args)
// -> std::future<typename std::result_of<FunctionType(Args...)>::type> { 
template<typename FunctionType, typename... ArgsTypes>
auto ThreadPool::submitTask(FunctionType&& func, ArgsTypes&&... args)
-> std::future<decltype(std::forward<FunctionType>(func)(std::forward<ArgsTypes>(args)...))> {
	using RetType = decltype(std::forward<FunctionType>(func)(std::forward<ArgsTypes>(args)...));
	// 将传入的带任何参数的函数（返回值自动推导并交由 future 在未来获取），封装为 std::packaged_task<>（为了通用，无参）
	// std::bind() 将有参数的函数重新封装为一个没有参数的可调用对象（指针）
	std::packaged_task<RetType()> task{ std::bind(std::forward<FunctionType>(func), std::forward<ArgsTypes>(args)...) };
	std::future<RetType> res = task.get_future();
	{
		std::unique_lock<std::mutex> uniqueLock(tasksMutex);
		if (stopFlag) throw std::runtime_error("submitTask on stopped ThreadPool!");
		tasks.push(static_cast<std::packaged_task<void()>>(std::move(task))); // tasks 中是无参、无返回值的 std::packaged_task; 若使用隐式类型转换：tasks.push(std::packaged_task<void()>{ std::move(task) });
	}
	condition.notify_one();
	return res; // 返回 future，未来某一时刻调用 res.get() 时阻塞获得结果
}