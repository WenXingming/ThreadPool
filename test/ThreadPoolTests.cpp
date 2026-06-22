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

TEST(ThreadPoolTest, DestructorFinishesQueuedTasks) {
    std::atomic<int> counter(0);

    std::promise<void> blockerStarted;
    std::future<void> blockerStartedFuture = blockerStarted.get_future();

    std::promise<void> releaseBlocker;
    std::shared_future<void> blockerFuture(releaseBlocker.get_future());

    {
        wxm::ThreadPool pool(1, 16, false, 1000);

        pool.submit_task([&]() {
            blockerStarted.set_value();
            blockerFuture.wait();
            counter.fetch_add(1);
            });

        ASSERT_EQ(blockerStartedFuture.wait_for(std::chrono::seconds(1)), std::future_status::ready);

        for (int i = 0; i < 5; ++i) {
            pool.submit_task([&counter]() {
                counter.fetch_add(1);
                });
        }

        releaseBlocker.set_value();
    }

    EXPECT_EQ(counter.load(), 6);
}

TEST(ThreadPoolTest, HigherPriorityTaskRunsFirstAfterWorkerIsBlocked) {
    wxm::ThreadPool pool(1, 16, false, 1000);

    std::promise<void> blockerStarted;
    std::future<void> blockerStartedFuture = blockerStarted.get_future();

    std::promise<void> releaseBlocker;
    std::shared_future<void> blockerFuture(releaseBlocker.get_future());

    std::vector<int> executionOrder;
    std::mutex orderMutex;

    // 让 blocker 优先级最高，保证被优先取到并阻塞 worker
    auto blocker = pool.submit_task(1000, [&]() {
        blockerStarted.set_value();
        blockerFuture.wait();
        });

    // 确认 worker 已进入 blocker
    ASSERT_EQ(blockerStartedFuture.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    auto lowPriority = pool.submit_task(1, [&executionOrder, &orderMutex]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(1);
        });

    auto highPriority = pool.submit_task(100, [&executionOrder, &orderMutex]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(100);
        });

    releaseBlocker.set_value();

    ASSERT_EQ(blocker.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    ASSERT_EQ(lowPriority.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    ASSERT_EQ(highPriority.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    ASSERT_EQ(executionOrder.size(), 2u);
    EXPECT_EQ(executionOrder[0], 100);
    EXPECT_EQ(executionOrder[1], 1);
}

TEST(ThreadPoolTest, SamePriorityUsesFifoAfterWorkerIsBlocked) {
    wxm::ThreadPool pool(1, 16, false, 1000);

    std::promise<void> blockerStarted;
    std::future<void> blockerStartedFuture = blockerStarted.get_future();

    std::promise<void> releaseBlocker;
    std::shared_future<void> blockerFuture(releaseBlocker.get_future());

    std::vector<int> executionOrder;
    std::mutex orderMutex;

    auto blocker = pool.submit_task(1000, [&]() {
        blockerStarted.set_value();
        blockerFuture.wait();
        });

    ASSERT_EQ(blockerStartedFuture.wait_for(std::chrono::seconds(1)), std::future_status::ready);

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

    ASSERT_EQ(blocker.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    ASSERT_EQ(first.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    ASSERT_EQ(second.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    ASSERT_EQ(executionOrder.size(), 2u);
    EXPECT_EQ(executionOrder[0], 1);
    EXPECT_EQ(executionOrder[1], 2);
}
