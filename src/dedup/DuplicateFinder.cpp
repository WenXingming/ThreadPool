#include "DuplicateFinder.h"
#include "ThreadPool.h"

#include <algorithm>
#include <future>
#include <map>
#include <sstream>
#include <thread>
#include <utility>

namespace {

typedef std::pair<uint64_t, uint64_t> SizeHashKey;

std::string format_walk_error(const WalkError& error) {
    std::ostringstream oss;
    oss << error.path << ": " << error.message;
    return oss.str();
}

std::string format_hash_error(const HashResult& result) {
    std::ostringstream oss;
    oss << result.file.path << ": " << result.error;
    return oss.str();
}

void sort_report(DuplicateReport& report) {
    for (std::vector<DuplicateGroup>::iterator it = report.groups.begin(); it != report.groups.end(); ++it) {
        std::sort(it->paths.begin(), it->paths.end());
    }

    std::sort(report.groups.begin(), report.groups.end(), [](const DuplicateGroup& left, const DuplicateGroup& right) {
        if (left.size != right.size) {
            return left.size < right.size;
        }
        if (left.hash != right.hash) {
            return left.hash < right.hash;
        }
        if (left.paths.empty() || right.paths.empty()) {
            return left.paths.size() < right.paths.size();
        }
        return left.paths[0] < right.paths[0];
        });

    std::sort(report.errors.begin(), report.errors.end());
}

int default_thread_count() {
    const unsigned int hardwareThreads = std::thread::hardware_concurrency();
    return hardwareThreads == 0 ? 2 : static_cast<int>(hardwareThreads);
}

DuplicateFinderConfig normalize_config(DuplicateFinderConfig config) {
    if (config.threadCount <= 0) {
        config.threadCount = default_thread_count();
    }
    if (config.queueCapacity <= 0) {
        config.queueCapacity = config.threadCount * 4;
    }
    if (config.waitTimeoutMs <= 0) {
        config.waitTimeoutMs = 1000;
    }
    return config;
}

} // namespace

DuplicateFinderConfig::DuplicateFinderConfig()
    : threadCount(default_thread_count()),
      queueCapacity(threadCount * 4),
      waitTimeoutMs(1000) {
}

DuplicateFinder::DuplicateFinder()
    : config_() {
}

DuplicateFinder::DuplicateFinder(const DuplicateFinderConfig& config)
    : config_(normalize_config(config)) {
}

DuplicateReport DuplicateFinder::find_duplicates(const std::string& rootPath) const {
    DuplicateReport report;
    report.scannedFiles = 0;
    report.hashedFiles = 0;
    report.errorCount = 0;
    report.threadCount = config_.threadCount;

    const FileWalkResult walkResult = fileWalker_.collect_files(rootPath);
    report.scannedFiles = walkResult.files.size();

    for (std::vector<WalkError>::const_iterator it = walkResult.errors.begin(); it != walkResult.errors.end(); ++it) {
        report.errors.push_back(format_walk_error(*it));
    }

    std::map<uint64_t, std::vector<FileInfo> > filesBySize;
    for (std::vector<FileInfo>::const_iterator it = walkResult.files.begin(); it != walkResult.files.end(); ++it) {
        filesBySize[it->size].push_back(*it);
    }

    std::vector<FileInfo> candidates;
    for (std::map<uint64_t, std::vector<FileInfo> >::const_iterator groupIt = filesBySize.begin(); groupIt != filesBySize.end(); ++groupIt) {
        const std::vector<FileInfo>& files = groupIt->second;
        if (files.size() < 2) { // 性能优化：大小唯一的文件不可能是重复的，直接跳过，避免计算哈希
            continue;
        }

        for (std::vector<FileInfo>::const_iterator fileIt = files.begin(); fileIt != files.end(); ++fileIt) {
            candidates.push_back(*fileIt);
        }
    }

    wxm::ThreadPool pool(config_.threadCount, config_.queueCapacity, false, config_.waitTimeoutMs);
    std::vector<std::future<HashResult> > futures;
    futures.reserve(candidates.size());

    for (std::vector<FileInfo>::const_iterator it = candidates.begin(); it != candidates.end(); ++it) {
        const FileInfo file = *it;
        futures.push_back(pool.submit_task([this, file]() {
            return hasher_.hash_file(file);
            }));
    }

    std::map<SizeHashKey, std::vector<std::string> > filesBySizeAndHash;
    for (std::vector<std::future<HashResult> >::iterator it = futures.begin(); it != futures.end(); ++it) {
        const HashResult hashResult = it->get();
        if (!hashResult.ok) {
            report.errors.push_back(format_hash_error(hashResult));
            continue;
        }

        ++report.hashedFiles;
        filesBySizeAndHash[SizeHashKey(hashResult.file.size, hashResult.hash)].push_back(hashResult.file.path);
    }

    for (std::map<SizeHashKey, std::vector<std::string> >::const_iterator it = filesBySizeAndHash.begin(); it != filesBySizeAndHash.end(); ++it) {
        if (it->second.size() < 2) {
            continue;
        }

        DuplicateGroup group;
        group.size = it->first.first;
        group.hash = it->first.second;
        group.paths = it->second;
        report.groups.push_back(group);
    }

    report.errorCount = report.errors.size();
    sort_report(report);
    return report;
}
