#pragma once

#include <string>
#include <vector>
#include <memory>
#include <dirent.h>
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
    // 自定义删除器，避免 decltype(&closedir) 可能引起的编译器警告 (如 ignoring attributes)
    struct DirDeleter {
        void operator()(DIR* d) const {
            if (d) closedir(d);
        }
    };
    // 使用 std::unique_ptr 封装 DIR*，利用 RAII 机制确保在作用域结束时自动调用 closedir 防泄漏。或者自定义 RAII 包装类
    using ScopedDir = std::unique_ptr<DIR, DirDeleter>; // std::unique_ptr<DIR, decltype(&closedir)>

    void collect_files_recursive(const std::string& path, FileWalkResult& result) const;
};
