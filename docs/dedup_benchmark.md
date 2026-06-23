# Dedup Benchmark 基线

本文档记录 `dedup` 的 benchmark 方法。目标不是追求某一次漂亮的数字，而是让性能讨论可复现、可解释、可对比。

## 构建

Benchmark 默认不参与普通构建。需要显式开启，并建议使用 Release 模式：

```bash
cmake -S . -B build-bench -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench
```

Google Benchmark 只用于 benchmark target。线程池和 dedup 业务实现仍保持 C++11。

## 正式运行命令

用于人工阅读基线结果时，使用：

```bash
./build-bench/benchmark/dedup/dedupBenchmark \
  --benchmark_min_time=1.0 \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true
```

需要机器可读结果时，使用：

```bash
./build-bench/benchmark/dedup/dedupBenchmark \
  --benchmark_min_time=1.0 \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true \
  --benchmark_format=json \
  > benchmark/dedup/results/latest.json
```

`benchmark/dedup/results/` 目录通过 `.gitkeep` 保留在仓库中，但生成的 JSON 结果会被忽略，因为它们依赖本机硬件、当前负载、文件系统缓存和 CPU 频率调节状态。

## Workload

所有 benchmark 数据都在计时开始前生成到 `/tmp` 下。每个数据集约有 20% 的重复文件，并向下取整为成对重复。

| Workload | 文件数 | 文件大小 | 近似数据量 | 目的 |
| --- | ---: | ---: | ---: | --- |
| `100_files_4KiB` | 100 | 4 KiB | 0.4 MiB | 极小规模 sanity baseline |
| `1000_files_4KiB` | 1000 | 4 KiB | 4 MiB | 观察小文件任务调度、open/close 开销 |
| `10000_files_4KiB` | 10000 | 4 KiB | 40 MiB | many-small-files 场景 |
| `1000_files_256KiB` | 1000 | 256 KiB | 256 MiB | medium-file hash 吞吐场景 |

每个 workload 都会使用以下 hash worker 线程数运行：

```text
1, 2, 4, 8
```

## 环境记录

保留 benchmark 结果用于分析时，建议记录以下信息：

| 项目 | 值 |
| --- | --- |
| 日期 | |
| CPU | |
| 逻辑线程数 | |
| 内存 | |
| 磁盘 / 文件系统 | |
| 编译器 | |
| 构建类型 | Release |
| 是否有 CPU scaling warning | yes / no |
| 后台负载 | |

Google Benchmark 启动时会打印 CPU 拓扑和 load average。分享人工阅读结果时，建议保留这段 header。

## 当前基线观察

以下观察基于 `benchmark/dedup/results/latest.json`：

| 项目 | 值 |
| --- | --- |
| 日期 | 2026-06-23 10:19:45 +08:00 |
| 主机 | `wxm-Precision-7920-Tower` |
| 逻辑 CPU | 24 |
| CPU 频率 | 3500 MHz |
| CPU scaling warning | yes |
| Load average | 0.79, 1.09, 0.91 |

当前 mean 结果如下：

| Workload | 线程数 | Real time | CPU time | Items/s | Bytes/s |
| --- | ---: | ---: | ---: | ---: | ---: |
| `100_files_4KiB` | 1 | 23.19 ms | 3.29 ms | 30.38k/s | 124.44 MB/s |
| `100_files_4KiB` | 2 | 13.56 ms | 3.19 ms | 31.35k/s | 128.41 MB/s |
| `100_files_4KiB` | 4 | 8.66 ms | 3.22 ms | 31.07k/s | 127.26 MB/s |
| `100_files_4KiB` | 8 | 6.34 ms | 3.56 ms | 28.09k/s | 115.07 MB/s |
| `1000_files_4KiB` | 1 | 124.01 ms | 29.09 ms | 34.39k/s | 140.85 MB/s |
| `1000_files_4KiB` | 2 | 77.08 ms | 25.35 ms | 39.45k/s | 161.60 MB/s |
| `1000_files_4KiB` | 4 | 49.10 ms | 23.18 ms | 43.15k/s | 176.75 MB/s |
| `1000_files_4KiB` | 8 | 32.98 ms | 19.38 ms | 51.68k/s | 211.69 MB/s |
| `10000_files_4KiB` | 1 | 1062.83 ms | 235.95 ms | 42.38k/s | 173.60 MB/s |
| `10000_files_4KiB` | 2 | 601.42 ms | 182.45 ms | 54.82k/s | 224.53 MB/s |
| `10000_files_4KiB` | 4 | 366.05 ms | 164.63 ms | 60.80k/s | 249.04 MB/s |
| `10000_files_4KiB` | 8 | 214.65 ms | 129.71 ms | 77.17k/s | 316.08 MB/s |
| `1000_files_256KiB` | 1 | 885.72 ms | 31.97 ms | 31.28k/s | 8.20 GB/s |
| `1000_files_256KiB` | 2 | 458.12 ms | 29.72 ms | 33.67k/s | 8.83 GB/s |
| `1000_files_256KiB` | 4 | 243.67 ms | 28.44 ms | 35.17k/s | 9.22 GB/s |
| `1000_files_256KiB` | 8 | 131.62 ms | 26.22 ms | 38.23k/s | 10.02 GB/s |

当前基线有几个现象：

- 这台机器上，4 个 workload 的最佳 real time 都出现在 8 个 hash worker。
- `100_files_4KiB` 虽然 8 线程最快，但 CPU time 没有明显下降，吞吐反而略低，说明这个极小数据集更像 sanity case，不适合作为优化决策依据。
- `1000_files_4KiB` 和 `10000_files_4KiB` 随线程数增加持续下降，说明当前实现能从并行 hash 中获益；`10000_files_4KiB` 从 1 线程到 8 线程约提升 4.95 倍。
- `1000_files_256KiB` 的并行收益最明显，从 1 线程到 8 线程约提升 6.73 倍，说明中等文件场景下，多 worker 能有效重叠文件读取和 hash 工作。
- real time 远大于 CPU time，尤其是 `1000_files_256KiB`，说明当前 benchmark 很大程度受 I/O 等待、页缓存行为或文件系统路径影响；不能只看 CPU time 判断性能。
- 由于本次运行存在 CPU scaling warning，这些数字适合作为当前机器的参考基线，不应视为跨机器的稳定结论。

基于这组结果，下一步更值得优先验证的是 **many-small-files 的任务粒度优化**，例如批量 hash 多个文件，观察能否减少任务提交和 future 管理开销；Hasher buffer size 可以作为第二优先级验证。

## 结果表模板

对比改动前后性能时，可以从 aggregate benchmark 输出中填写下表：

| Workload | 线程数 | Real time | CPU time | Items/s | Bytes/s | 观察 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `100_files_4KiB` | 1 | | | | | |
| `100_files_4KiB` | 2 | | | | | |
| `100_files_4KiB` | 4 | | | | | |
| `100_files_4KiB` | 8 | | | | | |
| `1000_files_4KiB` | 1 | | | | | |
| `1000_files_4KiB` | 2 | | | | | |
| `1000_files_4KiB` | 4 | | | | | |
| `1000_files_4KiB` | 8 | | | | | |
| `10000_files_4KiB` | 1 | | | | | |
| `10000_files_4KiB` | 2 | | | | | |
| `10000_files_4KiB` | 4 | | | | | |
| `10000_files_4KiB` | 8 | | | | | |
| `1000_files_256KiB` | 1 | | | | | |
| `1000_files_256KiB` | 2 | | | | | |
| `1000_files_256KiB` | 4 | | | | | |
| `1000_files_256KiB` | 8 | | | | | |

## 如何解读结果

- 优先比较 real time。它最接近用户实际感受到的运行时间。
- 再比较 CPU time。如果 real time 下降但 CPU time 上升，说明更多 worker 可能在帮助重叠 I/O 等待。
- 重点观察 `4 -> 8` 的变化。如果 real time 几乎不再下降，甚至变慢，说明 workload 可能已经碰到 I/O、调度或任务管理瓶颈。
- 小文件场景可能扩展性较差，因为 open/close、目录遍历、任务提交和 future 收集开销可能超过实际 hash 工作。
- 中等文件场景通常更能体现 hash 吞吐，但最佳线程数仍取决于存储设备和缓存状态。

## 优化决策

使用 benchmark 基线决定下一步工程优化方向：

- 如果 many-small-files 表现较弱：考虑将多个文件合并为一个任务，减少任务提交和 future 管理开销。
- 如果 medium-file-hash 表现较弱：比较不同 Hasher buffer size，例如 64 KiB、256 KiB 和 1 MiB。
- 如果所有 workload 在不同线程数下耗时接近：优先检查 FileWalker 和结果聚合逻辑，而不是直接修改线程池。
- 如果 `8` 个 worker 持续输给 `4` 个 worker：保留 `--threads` 可配置，并在文档中说明最佳线程数与硬件相关。

不要基于单次运行做优化判断。代码改动后应重新运行基线，并比较 aggregate 结果。
