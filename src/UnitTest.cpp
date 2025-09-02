/**
 * @file UnitTest.cpp
 * @brief 线程池单元测试：测试提交任何类型任务；测试自动扩缩
 * @details 
 * @author wenxingming
 * @date 2025-09-02
 * @note My project address: https://github.com/WenXingming/ThreadPool
 */

#include "ThreadPool.h"
// using namespace wxm;

// ==================================================================
/// NOTE: TEST 线程池 main.cpp。一般的函数：RetType(Args...)

class Task {
public:
    static int num;
    static std::mutex mtx;
    static const int taskNum = 500;

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

    Task task;
    {
        wxm::ThreadPool pool(1);        // Test thread pool auto expand
        // wxm::ThreadPool pool(20);    // Test thread pool auto reduce
        for (int i = 0; i < Task::taskNum; ++i) {
            pool.submit_task(Task::task1);
        }
        std::this_thread::sleep_for(std::chrono::seconds(5)); // 等待处理完成
    }

    std::cout << "Testing no_argument_no_ret success. \n";
}

void test_have_argument_have_ret() {
    std::cout << "==========================================================\n";
    std::cout << "Testing have_argument_have_ret: \n";

    Task task;
    std::vector<std::future<int>> results;
    {
        wxm::ThreadPool pool(1);
        // wxm::ThreadPool pool(20);
        for (int i = 0; i < Task::taskNum; ++i) {
            std::future<int> res = pool.submit_task(&Task::task2, std::ref(Task::num));
            results.push_back(std::move(res));
        }

        for (auto& future : results) {
            future.wait();
        }

        std::cout << "Output the results: \n";
        for (auto& future : results) {
            std::cout << future.get() << '\n';
        }
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
