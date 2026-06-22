#include "Hasher.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

namespace {

// 采用了适合本地去重场景的 FNV - 1a 算法，避免了复杂的密码学计算带来的 CPU 瓶颈，使得整个哈希过程大概率是受限于硬盘的 I / O 速度，而不是 CPU 性能。

const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
const uint64_t FNV_PRIME = 1099511628211ULL;

std::string error_message(const std::string& prefix) {
    std::ostringstream oss;
    oss << prefix << ": " << std::strerror(errno);
    return oss.str();
}

uint64_t update_fnv1a(uint64_t hash, const char* data, std::streamsize size) {
    for (std::streamsize i = 0; i < size; ++i) {
        hash ^= static_cast<unsigned char>(data[i]);
        hash *= FNV_PRIME;
    }
    return hash;
}

} // namespace

Hasher::Hasher(size_t bufferSize)
    : bufferSize_(bufferSize == 0 ? 1 : bufferSize) {
}

HashResult Hasher::hash_file(const FileInfo& file) const {
    // 1. 以二进制模式打开文件 (非常重要，防止 Windows 下的 \r\n 转换破坏文件数据)
    std::ifstream input(file.path.c_str(), std::ios::binary);
    if (!input.is_open()) {
        return HashResult{ false, file, 0, error_message("open failed") };
    }

    // 2. 分配读取缓冲区，分块循环读取 (Chunked Reading)
    // 内存友好（OOM 防范）：采用了基于固定大小 Buffer 的分块读取策略。决不会因为大文件撑爆内存。
    uint64_t hash = FNV_OFFSET_BASIS;
    std::vector<char> buffer(bufferSize_);
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytesRead = input.gcount();
        if (bytesRead > 0) {
            hash = update_fnv1a(hash, buffer.data(), bytesRead);  // 将读到的数据块不断喂给哈希算法，更新 hash 状态
        }
    }

    // 3. 严谨的错误校验
    // 循环退出时，正常情况必须是因为遇到了文件尾 (EOF)，如果不是 EOF 导致的退出，说明发生了 I/O 错误（如磁盘掉线）
    if (!input.eof()) {
        return HashResult{ false, file, 0, error_message("read failed") };
    }

    return HashResult{ true, file, hash, "" };
}
