#pragma once

#include <string>

#include "FileInfo.h"

class FileWalker {
public:
    FileWalkResult collect_files(const std::string& rootPath) const;

private:
    void collect_files_recursive(const std::string& path, FileWalkResult& result) const;
};
