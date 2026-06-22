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
    std::vector<DuplicateGroup> groups;
    std::vector<std::string> errors;
};

class DuplicateFinder {
public:
    DuplicateReport find_duplicates(const std::string& rootPath) const;

private:
    FileWalker fileWalker_; // TODO: 注入依赖
    Hasher hasher_;
};
