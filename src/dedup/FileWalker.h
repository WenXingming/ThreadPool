#pragma once

#include <string>
#include <vector>

#include "FileInfo.h"

struct WalkError {
    std::string path;
    std::string message;
};

struct FileWalkResult {
    std::vector<FileInfo> files;
    std::vector<WalkError> errors;
};

class FileWalker {
public:
    FileWalkResult collect_files(const std::string& rootPath) const;

private:
    void collect_files_recursive(const std::string& path, FileWalkResult& result) const;
};
