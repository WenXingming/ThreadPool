#pragma once

#include <cstdint>
#include <string>

struct FileInfo {
    std::string path;
    uint64_t size;
};

struct FileError {
    enum class Phase {
        SCAN,
        HASH
    };
    Phase phase;
    std::string path;
    std::string message;
};
