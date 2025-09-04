/**
 * @file UnitTest.cpp
 * @brief 线程池单元测试：测试提交任何任务类型并异步获取任务结果；测试自动扩缩；测试优先级调度等
 * @details
 * @author wenxingming
 * @date 2025-09-02
 * @note My project address: https://github.com/WenXingming/ThreadPool
 */

#include "ThreadPool.h"

class Task {
public:
    static int num;
    static std::mutex mtx;
    static const int taskNum;

    static void task1() { // 无参无返回值
        auto lock = std::unique_lock<std::mutex>(mtx);
        num++;
        std::cout << "processing(processed) thread id:" << std::dec << std::this_thread::get_id() << ", and task id(value of num): " << num - 1 << std::endl;
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟每个线程需要 1s 执行时间
    }

    static int task2(int& _num) { // 有参有返回值
        auto lock = std::unique_lock<std::mutex>(mtx);
        int res = _num++;
        std::cout << "processing(processed) thread id:" << std::dec << std::this_thread::get_id() << ", and task id(value of num): " << num - 1 << std::endl;
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟每个线程需要 1s 执行时间
        return res;
    }
};
int Task::num = 0;
std::mutex Task::mtx;
const int Task::taskNum = 100;



/// @brief 基础测试，向线程池提交无参无返回值任务
void test_no_argument_no_ret() {
    std::cout << "==========================================================\n";
    std::cout << "Testing no_argument_no_ret: \n";

    int initialSize = 24;
    wxm::ThreadPool pool(initialSize, 50, false, 1000);
    initialSize = pool.get_thread_pool_size();
    try {
        for (int i = 0; i < Task::taskNum; ++i) {
            pool.submit_task(&Task::task1);
        }
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    int overSize = pool.get_thread_pool_size();
    assert(initialSize == overSize);
    std::cout << "Testing no_argument_no_ret success. \n";
}


/// @brief 基础测试，向线程池提交有参有返回值任务，异步获取结果
void test_have_argument_have_ret() {
    std::cout << "==========================================================\n";
    std::cout << "Testing have_argument_have_ret: \n";

    int initialSize = 24;
    wxm::ThreadPool pool(initialSize, 50, false, 1000);
    initialSize = pool.get_thread_pool_size();
    try {
        std::vector<std::future<int>> results;
        for (int i = 0; i < Task::taskNum; ++i) {
            // 按理说 num 也该加锁。可以不加，处理任务加锁了
            std::future<int> res = pool.submit_task(&Task::task2, std::ref(Task::num)); // 异步获取结果
            results.push_back(std::move(res));
        }

        for (auto& future : results) {
            future.wait(); // 异步等待结果
        }

        std::cout << "Output the results: \n";
        for (auto& future : results) {
            std::cout << future.get() << ' ';
        }
        std::cout << '\n';
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    int overSize = pool.get_thread_pool_size();
    assert(initialSize == overSize);
    std::cout << "Testing have_argument_have_ret success. \n";
}


/// @brief Test thread pool auto expand。初始时设置线程池大小为 1
void test_thread_pool_auto_expand() {
    std::cout << "==========================================================\n";
    std::cout << "Testing thread_pool_auto_expand: \n";

    int threadPoolInitialSize = 1;
    wxm::ThreadPool pool(1, 50, true, 500); // 开启自动扩缩功能（减小等待时间，期望线程池递增）
    threadPoolInitialSize = pool.get_thread_pool_size();
    assert(threadPoolInitialSize == 1); // 初始线程池大小不能太大，否则无法测试自动扩增功能
    try {
        std::vector<std::future<int>> results;
        for (int i = 0; i < Task::taskNum; ++i) {
            std::future<int> res = pool.submit_task(&Task::task2, std::ref(Task::num));
            results.push_back(std::move(res));
        }

        for (auto& future : results) {
            future.wait();
        }

        std::cout << "Output the results: \n";
        for (auto& future : results) {
            std::cout << future.get() << ' ';
        }
        std::cout << '\n';
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    int threadPoolOverSize = pool.get_thread_pool_size();
    std::cout << "initial Size: " << threadPoolInitialSize << ", over size: " << threadPoolOverSize << std::endl;
    assert(threadPoolInitialSize < threadPoolOverSize); // Test result. Auto expand.
    std::cout << "Testing thread_pool_auto_expand success. \n";
}


/// @brief Test thread pool auto expand。初始时设置线程池大小较大
void test_thread_pool_auto_reduce() {
    std::cout << "==========================================================\n";
    std::cout << "Testing thread_pool_auto_reduce: \n";

    int threadPoolInitialSize = 24;
    wxm::ThreadPool pool(threadPoolInitialSize, 50, true, 1000); // 开启自动扩缩功能
    threadPoolInitialSize = pool.get_thread_pool_size();
    assert(threadPoolInitialSize > 2); // 初始线程池大小不能太小，否则无法测试自动缩减功能
    try {
        std::vector<std::future<int>> results;
        for (int i = 0; i < Task::taskNum; ++i) {
            std::future<int> res = pool.submit_task(&Task::task2, std::ref(Task::num));
            results.push_back(std::move(res));
            std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Test thread pool auto reduce（减慢提交任务速度，期望线程池缩减）
        }

        for (auto& future : results) {
            future.wait();
        }

        std::cout << "Output the results: \n";
        for (auto& future : results) {
            std::cout << future.get() << ' ';
        }
        std::cout << '\n';
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    int threadPoolOverSize = pool.get_thread_pool_size();
    std::cout << "initial Size: " << threadPoolInitialSize << ", over size: " << threadPoolOverSize << std::endl;
    assert(threadPoolInitialSize > threadPoolOverSize); // Test result. Auto reduce.
    std::cout << "Testing thread_pool_auto_reduce success. \n";
}


/// @brief Test thread pool priority schedule（优先级调度）。
/// @brief 不太好测，毕竟只要一提交任务线程池就自动调度执行任务了。想到的测试方法是减小线程池处理能力，提高提交任务次数，看打印结果（大多数情况下大数字在小数字前）
void test_thread_pool_priority_schedule() {
    std::cout << "==========================================================\n";
    std::cout << "Testing test_thread_pool_priority_schedule: \n";

    int threadPoolInitialSize = 1; // 线程池设置为最小
    wxm::ThreadPool pool(threadPoolInitialSize, 100, false, 1000); // 设置队列上限 > taskNum，优先级调度结果更直观（不然打印类似 50,51...99, 49, 48,...1）
    threadPoolInitialSize = pool.get_thread_pool_size();
    assert(threadPoolInitialSize == 1);
    try {
        std::vector<std::future<int>> results;
        for (int i = 0; i < Task::taskNum; ++i) {
            std::future<int> res = pool.submit_task(i, &Task::task2, std::ref(Task::num)); // 添加任务优先级
            results.push_back(std::move(res));
        }

        for (auto& future : results) {
            future.wait();
        }

        std::cout << "Output the results: \n";
        for (auto& future : results) {
            std::cout << future.get() << ' ';
        }
        std::cout << '\n';
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    int threadPoolOverSize = pool.get_thread_pool_size();
    assert(threadPoolInitialSize == threadPoolOverSize);
    std::cout << "Testing test_thread_pool_priority_schedule success. \n";
}


int main() {
    test_no_argument_no_ret();
    std::cout << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    test_have_argument_have_ret();
    std::cout << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    test_thread_pool_auto_expand();
    std::cout << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    test_thread_pool_auto_reduce();
    std::cout << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    test_thread_pool_priority_schedule();
    std::cout << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << "All tests passed.\n";
    return 0;
}
