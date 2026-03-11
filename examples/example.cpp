// ============================================================================
// example - 线程池使用示例
// ============================================================================
// 核心功能和接口：
// 1. 演示普通任务、带返回值任务和高优先级任务的提交方式。
// 2. 演示如何通过 std::future 获取任务结果。
// ============================================================================

#include "ThreadPool.h"

#include <future>
#include <iostream>
#include <string>
#include <vector>

// 带返回值的普通任务
int multiply(int left, int right) {
    return left * right;
}

// 无返回值的普通任务
void print_message(const std::string& message) {
    std::cout << "message: " << message << std::endl;
}

int main() {
    wxm::ThreadPool pool(3, 32, false, 1000);

    std::future<void> logTask = pool.submit_task(print_message, std::string("hello from ThreadPool"));
    std::future<int> defaultPriorityTask = pool.submit_task(multiply, 6, 7);
    std::future<int> highPriorityTask = pool.submit_task(10, []() {
        return 100;
        });

    logTask.get();

    std::vector<std::future<int>> results;
    results.push_back(std::move(defaultPriorityTask));
    results.push_back(std::move(highPriorityTask));

    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "result[" << i << "] = " << results[i].get() << std::endl;
    }

    std::cout << "example finished" << std::endl;
    return 0;
}
