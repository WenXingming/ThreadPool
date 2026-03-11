// ============================================================================
// TaskTests - Task 单元测试
// ============================================================================
// 核心功能和接口：
// 1. 验证任务执行和空任务异常行为。
// 2. 验证任务优先级和同优先级 FIFO 排序规则。
// ============================================================================

#include "Task.h"

#include <gtest/gtest.h>

#include <chrono>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

TEST(TaskTest, DefaultTaskUsesSentinelPriority) {
    wxm::Task task;

    EXPECT_EQ(task.get_priority(), INT_MIN);
}

TEST(TaskTest, RunExecutesStoredCallable) {
    int counter = 0;
    wxm::Task task([&counter]() {
        ++counter;
        }, 3);

    task.run();

    EXPECT_EQ(counter, 1);
}

TEST(TaskTest, RunThrowsWhenCallableIsEmpty) {
    wxm::Task task;

    EXPECT_THROW(task.run(), std::runtime_error);
}

TEST(TaskTest, PriorityQueuePopsHigherPriorityFirst) {
    std::vector<int> executionOrder;
    std::priority_queue<wxm::Task> taskQueue;

    taskQueue.push(wxm::Task([&executionOrder]() {
        executionOrder.push_back(1);
        }, 1));
    taskQueue.push(wxm::Task([&executionOrder]() {
        executionOrder.push_back(10);
        }, 10));

    while (!taskQueue.empty()) {
        wxm::Task task = taskQueue.top();
        taskQueue.pop();
        task.run();
    }

    ASSERT_EQ(executionOrder.size(), 2u);
    EXPECT_EQ(executionOrder[0], 10);
    EXPECT_EQ(executionOrder[1], 1);
}

TEST(TaskTest, PriorityQueueUsesFifoWhenPriorityMatches) {
    std::vector<int> executionOrder;
    std::priority_queue<wxm::Task> taskQueue;

    taskQueue.push(wxm::Task([&executionOrder]() {
        executionOrder.push_back(1);
        }, 5));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    taskQueue.push(wxm::Task([&executionOrder]() {
        executionOrder.push_back(2);
        }, 5));

    while (!taskQueue.empty()) {
        wxm::Task task = taskQueue.top();
        taskQueue.pop();
        task.run();
    }

    ASSERT_EQ(executionOrder.size(), 2u);
    EXPECT_EQ(executionOrder[0], 1);
    EXPECT_EQ(executionOrder[1], 2);
}