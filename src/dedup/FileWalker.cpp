#include "FileWalker.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

namespace {

std::string join_path(const std::string& parent, const std::string& child) {
    if (parent.empty() || parent[parent.size() - 1] == '/') {
        return parent + child;
    }
    return parent + "/" + child;
}

std::string errno_message(const std::string& prefix) {
    std::ostringstream oss;
    oss << prefix << ": " << std::strerror(errno);
    return oss.str();
}

} // namespace

FileWalkResult FileWalker::collect_files(const std::string& rootPath) const {
    FileWalkResult result;
    collect_files_recursive(rootPath, result);

    std::sort(result.files.begin(), result.files.end(), [](const FileInfo& left, const FileInfo& right) {
        return left.path < right.path;
        });
    std::sort(result.errors.begin(), result.errors.end(), [](const WalkError& left, const WalkError& right) {
        return left.path < right.path;
        });

    return result;
}

void FileWalker::collect_files_recursive(const std::string& path, FileWalkResult& result) const {
    struct stat status;
    if (lstat(path.c_str(), &status) != 0) {
        result.errors.push_back(WalkError{ path, errno_message("lstat failed") });
        return;
    }

    // 处理普通文件，递归就到达了底端
    if (S_ISREG(status.st_mode)) {
        result.files.push_back(FileInfo{ path, static_cast<uint64_t>(status.st_size) });
        return;
    }

    // 既不是普通文件，也不是目录（那它可能是软链接、管道文件、设备文件等）。直接忽略
    if (!S_ISDIR(status.st_mode)) {
        return;
    }



    ScopedDir dir(opendir(path.c_str()));
    if (!dir) {
        result.errors.push_back(WalkError{ path, errno_message("opendir failed") });
        return;
    }

    while (true) {
        errno = 0;
        dirent* entry = readdir(dir.get()); // 每次调用会返回目录下的一个文件或子文件夹信息。
        if (entry == nullptr) {
            // 返回 nullptr 有两种情况。一是目录读完了（此时  errno  仍为 0），二是读取中途出错（此时  errno  会被设为非 0）。因此在读取前重置  errno = 0 ，可以用来区分这两种情况。
            if (errno != 0) {
                result.errors.push_back(WalkError{ path, errno_message("readdir failed") });
            }
            break;
        }

        const std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }

        collect_files_recursive(join_path(path, name), result);
    }
}
