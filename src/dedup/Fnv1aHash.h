#pragma once

#include <cstddef>
#include <cstdint>

// 采用了适合本地去重场景的 FNV - 1a 算法，避免了复杂的密码学计算带来的 CPU 瓶颈，使得整个哈希过程大概率是受限于硬盘的 I/O 速度，而不是 CPU 性能。
// TODO: 有碰撞风险，后续增加逐字节比对确认

class Fnv1aHash {
public:
    Fnv1aHash()
        : value_(OFFSET_BASIS) {
    }

    void update(const char* data, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            value_ ^= static_cast<unsigned char>(data[i]);
            value_ *= FNV_PRIME;
        }
    }

    uint64_t value() const {
        return value_;
    }

private:
    static const uint64_t OFFSET_BASIS = 14695981039346656037ULL;
    static const uint64_t FNV_PRIME = 1099511628211ULL;

    uint64_t value_;
};
