#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct FileInfo {
    std::string path;
    uint64_t size;
};

struct WalkError {
    std::string path;
    std::string message;
};

struct FileWalkResult {
    std::vector<FileInfo> files;
    std::vector<WalkError> errors;
};
