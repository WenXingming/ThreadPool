#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "FileInfo.h"
#include "FileWalker.h"
#include "Hasher.h"

struct DuplicateGroup {
    uint64_t size;
    uint64_t hash;
    std::vector<std::string> paths;
};

struct DuplicateReport {
    size_t scannedFiles;
    size_t hashedFiles;
    size_t errorCount;
    int threadCount;
    std::vector<DuplicateGroup> groups;
    std::vector<std::string> errors;
};

struct DuplicateFinderConfig {
    DuplicateFinderConfig();

    int threadCount;
    int queueCapacity;
    int waitTimeoutMs;
};

class DuplicateFinder {
public:
    DuplicateFinder();
    explicit DuplicateFinder(const DuplicateFinderConfig& config);

    DuplicateReport find_duplicates(const std::string& rootPath) const;

private:
    DuplicateFinderConfig config_;
    FileWalker fileWalker_; // TODO: 注入依赖
    Hasher hasher_;
};
