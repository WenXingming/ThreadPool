/*
#include "thread_pool.h"

// ==================================================================
/// NOTE: TEST 线程池 main.cpp。最简单的函数：void()
int num = 0;
std::mutex mtx;
void run() {
    auto lock = std::unique_lock<std::mutex>(mtx);
    num++;
    std::cout << "processing(processed) thread id and task id:" << std::dec << std::this_thread::get_id() << '\t' << num << std::endl;
    lock.unlock();
    std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟每个线程需要 1s 执行时间
}

int main() {
    {
        ThreadPool pool;
        for (int i = 0; i < 100; ++i) {
            pool.submitTask(run);
        }
    }

    std::cin.get();
    return 0;
}
*/

#include "thread_pool.h"

// ==================================================================
/// NOTE: TEST 线程池 main.cpp。一般的函数：RetType(Args...)
int num = 0;
std::mutex mtx;
int run(int& _num) {
    auto lock = std::unique_lock<std::mutex>(mtx);
    int res = _num++;
    std::cout << "processing(processed) thread id and task id:" << std::dec << std::this_thread::get_id() << '\t' << num << std::endl;
    lock.unlock();
    std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟每个线程需要 1s 执行时间
    return res;
}

int main() {
    std::vector<std::future<int>> results;
    {
        ThreadPool pool;
        for (int i = 0; i < 100; ++i) {
            std::future<int> res = pool.submit_task(run, std::ref(num));
            results.push_back(std::move(res));
        }

        for (auto& future : results) {
            future.wait();
        }
        std::cout << "==========================================================\n";
        std::cout << "Output the results: \n";
        for (auto& future : results) {
            std::cout << future.get() << '\n';
        }
    }

    std::cin.get();
    return 0;
}
