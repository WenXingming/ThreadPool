# C++ 通用线程池

一个简单实用的线程池，使用方式及样例见 UnitTest.cpp。**该线程池的特性有**：

1. **使用方式非常简单**！只需将 `ThreadPool.h` 和 `ThreadPool.cpp` 文件添加到你的项目中，在使用到的地方将头文件 `ThreadPool.h` 包含进来即可。然后就可以使用`wxm::ThreadPool pool;`（可指定构造函数的参数）创建线程池，然后提交任务了！

   ```C++
   // UnitTest.cpp 中的示例：无参无返回值任务的提交
   wxm::ThreadPool pool(2, 50, IS_TEST_AUTO_EXPAND_REDUCE, 1000);
   for (int i = 0; i < Task::taskNum; ++i) {
       pool.submit_task(Task::task1);
   }
   
   // UnitTest.cpp 中的示例：有参有返回值任务的提交以及结果的获取
   wxm::ThreadPool pool(2, 50, IS_TEST_AUTO_EXPAND_REDUCE, 1000);
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
   ```

2. **支持任何任务的异步执行和结果获取**。使用了 `std::packaged_task`、`std::future` 来异步获取任务的返回值。

3. **支持设定期望的任务处理速率，线程池据此动态调整线程池大小**。该选项在创建线程池时可以进行参数（`_maxWaitTime`）设置，线程池会根据该参数动态调整线程池的大小，以满足预期的任务处理速率。如果任务的平均处理时间超过了该值，线程池会自动增加工作线程的数量；反之，如果任务的处理时间远低于该值，线程池会减少工作线程的数量，从而提高资源利用率。详细解释一下该参数：

   - **理论上期望处理任务的速率 velocity >= (1 / _maxWaitTime) * numOfThread（单位数量 / s）**，其中 numOfThread 是**提交任务的线程的数量**（非线程池大小）。若线程池大小不够，其会自动扩容直到满足该速率；如果扩容到最大核心数也达不到该速度，此时硬件能力不够没有办法，线程池大小将保持在最大核心数。**例如：如果期望每秒处理至少 5 个任务，如果只有单个任务提交线程，则 5 = (1 / _maxWaitTime) * 1，计算得到应设定 _maxWaitTime = 0.2 s = 200 ms**
   - **理论上 velocity <= (1 / _maxWaitTime) * numOfThreads（单位数量 / s）**，其中 numOfThreads 是线程池中线程的数量。如果线程池的处理能力大于任务提交的速率，线程池会自动减少工作线程的数量，直到线程池大小达到满足 velocity 条件下线程池大小的下界。

4. **More features await coding**...

**Tips**：代码中使用了 C++ 11 的一些新特性。<ins>代码注释完善易于理解，如果对你有用，请给个 Star 🤞🤞🤞，非常感谢！</ins>
