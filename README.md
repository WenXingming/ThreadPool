# C++ 通用线程池

一个基于 C++11 实现的通用线程池组件，支持有界任务队列、异步任务提交、`std::future` 结果获取、优先级调度和可选自动扩缩容。

```mermaid
%%{init: {
    "theme": "default",
    "themeVariables": {
        "fontFamily": "Times New Roman",
        "fontSize": "20px"
    }
}}%%

sequenceDiagram
    
    actor one
    participant ThreadPool

    one ->> ThreadPool: std::future<type> res = submit_task(func, args...)
    one ->> ThreadPool:
    one ->> ThreadPool:
    one ->> ThreadPool:

    activate ThreadPool
    ThreadPool->>ThreadPool: process_task()
    ThreadPool ->> one: std::future<type> res
    deactivate ThreadPool

    ThreadPool ->> one:
    ThreadPool ->> one:
    ThreadPool ->> one:

```

使用示例详见 [examples/](examples/)，单元测试位于 [test](test) 目录。

## 快速使用

创建线程池后，可以提交任意可调用对象。无显式优先级时，任务默认优先级为 `0`。

```cpp
int initialSize = 3;
wxm::ThreadPool pool(initialSize, 32, false, 1000);

std::future<int> result = pool.submit_task([](int left, int right) {
    return left * right;
}, 6, 7);

std::cout << result.get() << std::endl;
```

也可以提交带优先级的任务。优先级数值越大，越先被调度；同优先级任务按提交顺序 FIFO 调度。

```cpp
std::future<int> highPriorityTask = pool.submit_task(10, []() {
    return 100;
});
```

如果任务内部抛出异常，异常会通过 `future.get()` 传播给调用方。

```cpp
std::future<void> failed = pool.submit_task([]() {
    throw std::runtime_error("task failed");
});

failed.get(); // throws std::runtime_error
```

## 功能特性

- **有界任务队列**：队列满时，提交线程会等待队列释放空间，形成背压。
- **异步任务提交**：支持任意可调用对象，并通过 `std::future` 获取返回值。
- **优先级调度**：任务可指定 `int` 优先级，数值越大越先被调度。
- **同优先级 FIFO**：同优先级任务使用递增入队序号保证提交顺序。
- **优雅析构**：析构时停止接收新任务，并等待已提交任务执行完成。
- **可选自动扩缩容**：可通过构造参数或接口启用/关闭自动扩缩容。
- **多生产者提交**：支持多个外部线程并发调用 `submit_task()`。
- **异常传播**：任务内部异常由 `std::packaged_task` 保存，并在 `future.get()` 时重新抛出。


## 自动扩缩容语义

构造函数参数如下：

```cpp
ThreadPool(int threadCount = 1,
           int queueCapacity = 50,
           bool autoScalingEnabled = false,
           int waitTimeoutMs = 1000);
```

当 `autoScalingEnabled` 为 `true` 时：

- 如果任务队列已满，提交线程等待超过 `waitTimeoutMs` 后，线程池会尝试扩容。
- 如果 worker 在 `waitTimeoutMs` 内没有等到任务，线程池会尝试缩容。
- 线程池最小保留 `1` 个 worker。
- 线程池最大扩展到 `2 * std::thread::hardware_concurrency()`。
- 缩容线程会自然退出并被安全 `join`，不会使用 `detach()`。

自动扩缩容也可以通过接口控制：

```cpp
pool.enable_auto_scaling();
pool.disable_auto_scaling();
```

>[!NOTE]
`waitTimeoutMs` 同时影响扩容和缩容的触发敏感度：值越小，线程池越快响应队列满或 worker 空闲；值越大，线程池越稳定，但扩缩容响应更慢。

## 测试覆盖

当前测试覆盖：

- 任务执行与空任务异常；
- 优先级调度与同优先级 FIFO；
- 返回值获取与异常传播；
- 任务异常后 worker 继续处理后续任务；
- 有界队列背压；
- 多生产者并发提交；
- 析构时执行完已提交任务；
- 析构时唤醒阻塞提交线程；
- 自动扩容、自动缩容以及开关控制；
- 配置参数合法/非法路径。

## 说明

本项目当前保持 C++11 标准，核心库默认不向 stdout/stderr 打印日志，适合作为后续并行压缩器等上层工具的任务调度组件。
