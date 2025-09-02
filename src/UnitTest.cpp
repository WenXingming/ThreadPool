/**
 * @file UnitTest.cpp
 * @brief 线程池单元测试：测试提交任何类型任务；测试自动扩缩
 * @details
 * @author wenxingming
 * @date 2025-09-02
 * @note My project address: https://github.com/WenXingming/ThreadPool
 */

#include "ThreadPool.h"

 // ==================================================================
 /// NOTE: TEST 线程池 main.cpp。一般的函数：RetType(Args...)

const bool IS_TEST_AUTO_EXPAND_REDUCE = true; // 测试自动扩张、缩减
const bool IS_AUTO_EXPAND = true; // true 测试扩张、false 测试缩减

class Task {
public:
    static int num;
    static std::mutex mtx;
    static const int taskNum = 200;

    static void task1() { // 无参无返回值
        auto lock = std::unique_lock<std::mutex>(mtx);
        num++;
        std::cout << "processing(processed) thread id and task id:" << std::dec << std::this_thread::get_id() << '\t' << num << std::endl;
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟每个线程需要 1s 执行时间
    }

    static int task2(int& _num) { // 有参有返回值
        auto lock = std::unique_lock<std::mutex>(mtx);
        int res = _num++;
        std::cout << "processing(processed) thread id:" << std::dec << std::this_thread::get_id() << "\tand task id: " << num << std::endl;
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟每个线程需要 1s 执行时间
        return res;
    }
};
int Task::num = 0;
std::mutex Task::mtx;


void test_no_argument_no_ret() {
    std::cout << "==========================================================\n";
    std::cout << "Testing no_argument_no_ret: \n";

    try {
        Task task;
        {
            if (IS_TEST_AUTO_EXPAND_REDUCE && !IS_AUTO_EXPAND) {
                wxm::ThreadPool pool(20, 50, IS_TEST_AUTO_EXPAND_REDUCE, 1000);       // Test thread pool auto reduce
                for (int i = 0; i < Task::taskNum; ++i) {
                    pool.submit_task(Task::task1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Test thread pool auto reduce
                }
            }
            else{
                wxm::ThreadPool pool(2, 50, IS_TEST_AUTO_EXPAND_REDUCE, 1000); // Test thread pool auto expand or no IS_TEST_AUTO_EXPAND_REDUCE
                for (int i = 0; i < Task::taskNum; ++i) {
                    pool.submit_task(Task::task1);
                }
            }
        }
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    std::cout << "Testing no_argument_no_ret success. \n";
}

void test_have_argument_have_ret() {
    std::cout << "==========================================================\n";
    std::cout << "Testing have_argument_have_ret: \n";

    try {
        Task task;
        std::vector<std::future<int>> results;
        {
            if (IS_TEST_AUTO_EXPAND_REDUCE && !IS_AUTO_EXPAND) {
                wxm::ThreadPool pool(20); // Test thread pool auto reduce
                for (int i = 0; i < Task::taskNum; ++i) {
                    std::future<int> res = pool.submit_task(&Task::task2, std::ref(Task::num));
                    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Test thread pool auto reduce
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
            else {
                wxm::ThreadPool pool(2, 50, IS_TEST_AUTO_EXPAND_REDUCE, 1000); // Test thread pool auto expand or no IS_TEST_AUTO_EXPAND_REDUCE
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
        }
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    std::cout << "Testing have_argument_have_ret success. \n";
}

int main() {
    test_have_argument_have_ret();
    std::cout << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(5));

    test_no_argument_no_ret();
    std::cout << std::endl;

    std::cout << "All tests passed.\n";
    return 0;
}
