#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "FileInfo.h"

struct HashResult {
    bool ok;
    FileInfo file;
    uint64_t hash;
    FileError error;
};

class Hasher {
public:
    explicit Hasher(size_t bufferSize = 1024 * 1024);

    HashResult hash_file(const FileInfo& file) const;

private:
    size_t bufferSize_;
};
