#include "Hasher.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

#include "Fnv1aHash.h"

Hasher::Hasher(size_t bufferSize)
    : bufferSize_(bufferSize == 0 ? 1 : bufferSize) {
}

HashResult Hasher::hash_file(const FileInfo& file) const {
    // 1. 以二进制模式打开文件 (非常重要，防止 Windows 下的 \r\n 转换破坏文件数据)
    std::ifstream input(file.path.c_str(), std::ios::binary);
    if (!input.is_open()) {
        return HashResult{ false, file, 0, FileError{ FileError::Phase::HASH, file.path, "failed to open file." } };
    }

    // 2. 分配读取缓冲区，分块循环读取 (Chunked Reading)
    // 内存友好（OOM 防范）：采用了基于固定大小 Buffer 的分块读取策略。决不会因为大文件撑爆内存。
    Fnv1aHash hash;
    std::vector<char> buffer(bufferSize_);
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytesRead = input.gcount();
        if (bytesRead > 0) {
            hash.update(buffer.data(), static_cast<size_t>(bytesRead)); // 将读到的数据块不断喂给哈希算法，更新 hash 状态
        }
    }

    // 3. 严谨的错误校验
    // 循环退出时，正常情况必须是因为遇到了文件尾 (EOF)，如果不是 EOF 导致的退出，说明发生了 I/O 错误（如磁盘掉线）
    if (!input.eof()) {
        return HashResult{ false, file, 0, FileError{ FileError::Phase::HASH, file.path, "failed to read file." } };
    }

    return HashResult{ true, file, hash.value(), FileError{} };
}
