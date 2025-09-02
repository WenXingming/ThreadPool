#include "ThreadPool.h"
// using namespace wxm;

// ==================================================================
/// NOTE: TEST 线程池 main.cpp。一般的函数：RetType(Args...)

class Task {
public:
    static int num;
    static std::mutex mtx;

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
        wxm::ThreadPool pool(4);
        for (int i = 0; i < 200; ++i) {
            pool.submit_task(Task::task1);
        }
    }

    std::cout << "Testing no_argument_no_ret success. \n";
}

void test_have_argument_have_ret() {
    std::cout << "==========================================================\n";
    std::cout << "Testing have_argument_have_ret: \n";

    Task task;
    std::vector<std::future<int>> results;
    {
        wxm::ThreadPool pool(4);
        for (int i = 0; i < 200; ++i) {
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
