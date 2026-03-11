// ============================================================================
// ThreadPoolTests - ThreadPool 单元测试
// ============================================================================
// 核心功能和接口：
// 1. 验证线程池构造、任务执行和返回值行为。
// 2. 验证优先级调度与同优先级 FIFO 行为。
// ============================================================================

#include "ThreadPool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

TEST(ThreadPoolTest, ConstructorNormalizesThreadCount) {
    wxm::ThreadPool pool(0, 16, false, 1000);
    EXPECT_GE(pool.get_thread_pool_size(), 1);
}

TEST(ThreadPoolTest, SubmitTaskReturnsExpectedValue) {
    wxm::ThreadPool pool(2, 16, false, 1000);
    std::future<int> result = pool.submit_task([](int a, int b) {
        return a + b;
        }, 10, 32);

    EXPECT_EQ(result.get(), 42);
}

TEST(ThreadPoolTest, AllSubmittedTasksAreExecuted) {
    wxm::ThreadPool pool(4, 128, false, 1000);
    std::atomic<int> counter(0);
    std::vector<std::future<void>> results;

    for (int i = 0; i < 50; ++i) {
        results.push_back(pool.submit_task([&counter]() {
            counter.fetch_add(1);
            }));
    }

    for (auto& result : results) {
        result.wait();
    }

    EXPECT_EQ(counter.load(), 50);
}

TEST(ThreadPoolTest, HigherPriorityTaskRunsFirstWhenQueued) {
    wxm::ThreadPool pool(1, 16, false, 1000);

    std::promise<void> releaseBlocker;
    std::shared_future<void> blockerFuture(releaseBlocker.get_future());
    std::vector<int> executionOrder;
    std::mutex orderMutex;

    auto blocker = pool.submit_task(0, [&blockerFuture]() {
        blockerFuture.wait();
        });

    auto lowPriority = pool.submit_task(1, [&executionOrder, &orderMutex]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(1);
        });
    auto highPriority = pool.submit_task(100, [&executionOrder, &orderMutex]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(100);
        });

    // submit_task 返回时任务已经入队，释放 blocker 后应先执行高优先级任务。
    releaseBlocker.set_value();

    blocker.wait();
    lowPriority.wait();
    highPriority.wait();

    ASSERT_EQ(executionOrder.size(), 2u);
    EXPECT_EQ(executionOrder[0], 100);
    EXPECT_EQ(executionOrder[1], 1);
}

TEST(ThreadPoolTest, SamePriorityUsesFifoOrderWhenQueued) {
    wxm::ThreadPool pool(1, 16, false, 1000);

    std::promise<void> releaseBlocker;
    std::shared_future<void> blockerFuture(releaseBlocker.get_future());
    std::vector<int> executionOrder;
    std::mutex orderMutex;

    auto blocker = pool.submit_task(0, [&blockerFuture]() {
        blockerFuture.wait();
        });

    auto first = pool.submit_task(10, [&executionOrder, &orderMutex]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(1);
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    auto second = pool.submit_task(10, [&executionOrder, &orderMutex]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(2);
        });

    releaseBlocker.set_value();

    blocker.wait();
    first.wait();
    second.wait();

    ASSERT_EQ(executionOrder.size(), 2u);
    EXPECT_EQ(executionOrder[0], 1);
    EXPECT_EQ(executionOrder[1], 2);
}
