#include <benchmark/benchmark.h>

#include "DuplicateFinder.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

std::string make_temp_dir() {
    std::string pattern = "/tmp/threadpool_dedup_bench_XXXXXX";
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');

    char* path = mkdtemp(buffer.data());
    if (path == nullptr) {
        throw std::runtime_error("mkdtemp failed");
    }
    return std::string(path);
}

std::string make_content(const std::string& seed, int size) {
    std::string content;
    content.reserve(static_cast<size_t>(size));

    while (static_cast<int>(content.size()) < size) {
        content += seed;
        content += "|";
    }
    content.resize(static_cast<size_t>(size));
    return content;
}

void write_file(const std::string& path, const std::string& content) {
    std::ofstream output(path.c_str(), std::ios::binary);
    if (!output) {
        throw std::runtime_error("failed to create benchmark file");
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
}

void generate_dataset(const std::string& root, int fileCount, int fileSize, std::vector<std::string>& files) {
    const int duplicateFiles = (fileCount / 5) & ~1; // 20%, rounded down to pairs.
    files.reserve(static_cast<size_t>(fileCount));

    for (int i = 0; i < duplicateFiles; ++i) {
        const std::string path = root + "/duplicate_" + std::to_string(i) + ".dat";
        const std::string content = make_content("duplicate-group-" + std::to_string(i / 2), fileSize);
        write_file(path, content);
        files.push_back(path);
    }

    for (int i = duplicateFiles; i < fileCount; ++i) {
        const std::string path = root + "/unique_" + std::to_string(i) + ".dat";
        const std::string content = make_content("unique-file-" + std::to_string(i), fileSize);
        write_file(path, content);
        files.push_back(path);
    }
}

void remove_dataset(const std::string& root, const std::vector<std::string>& files) {
    for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it) {
        std::remove(it->c_str());
    }
    rmdir(root.c_str());
}

void run_dedup_benchmark(benchmark::State& state) {
    const int fileCount = static_cast<int>(state.range(0));
    const int fileSize = static_cast<int>(state.range(1));
    const int threadCount = static_cast<int>(state.range(2));

    std::string root;
    std::vector<std::string> files;

    DuplicateFinderConfig config;
    config.threadCount = threadCount;
    config.queueCapacity = threadCount * 4;
    const DuplicateFinder finder(config);

    for (auto _ : state) {
        if (root.empty()) {
            state.PauseTiming();
            root = make_temp_dir();
            generate_dataset(root, fileCount, fileSize, files);
            state.ResumeTiming();
        }

        const DuplicateReport report = finder.find_duplicates(root);
        benchmark::DoNotOptimize(report.scannedFiles);
        benchmark::DoNotOptimize(report.hashedFiles);
        benchmark::DoNotOptimize(report.groups.size());

        if (!report.errors.empty()) {
            state.SkipWithError("dedup reported errors");
            break;
        }
    }

    state.PauseTiming();
    if (!root.empty()) {
        remove_dataset(root, files);
    }
    state.SetItemsProcessed(state.iterations() * fileCount);
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(fileCount) * fileSize);
}

void register_workload(const std::string& name, int fileCount, int fileSize) {
    const int threadCounts[] = { 1, 2, 4, 8 };
    for (size_t i = 0; i < sizeof(threadCounts) / sizeof(threadCounts[0]); ++i) {
        const int threadCount = threadCounts[i];
        const std::string benchmarkName = "DuplicateFinder/" + name + "/threads_" + std::to_string(threadCount);
        benchmark::RegisterBenchmark(benchmarkName.c_str(), run_dedup_benchmark)
            ->Args({ fileCount, fileSize, threadCount })
            ->Unit(benchmark::kMillisecond);
    }
}

void register_benchmarks() {
    register_workload("100_files_4KiB", 100, 4 * 1024);
    register_workload("1000_files_4KiB", 1000, 4 * 1024);
    register_workload("10000_files_4KiB", 10000, 4 * 1024);
    register_workload("1000_files_256KiB", 1000, 256 * 1024);
}

} // namespace

int main(int argc, char** argv) {
    register_benchmarks();
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
