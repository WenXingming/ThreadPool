#include "Hasher.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

std::string make_temp_dir() {
    std::string pattern = "/tmp/threadpool_hasher_test_XXXXXX";
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');

    char* path = mkdtemp(buffer.data());
    if (path == nullptr) {
        throw std::runtime_error("mkdtemp failed");
    }
    return std::string(path);
}

void write_file(const std::string& path, const std::string& content) {
    std::ofstream output(path.c_str(), std::ios::binary);
    output << content;
}

void remove_file_tree(const std::string& root, const std::vector<std::string>& files) {
    for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it) {
        std::remove(it->c_str());
    }
    rmdir(root.c_str());
}

FileInfo file_info(const std::string& path) {
    struct stat status;
    if (stat(path.c_str(), &status) != 0) {
        return FileInfo{ path, 0 };
    }
    return FileInfo{ path, static_cast<uint64_t>(status.st_size) };
}

} // namespace

TEST(HasherTest, SameContentHasSameHash) {
    const std::string root = make_temp_dir();
    const std::string first = root + "/first.txt";
    const std::string second = root + "/second.txt";
    write_file(first, "same content");
    write_file(second, "same content");

    const Hasher hasher;
    const HashResult firstHash = hasher.hash_file(file_info(first));
    const HashResult secondHash = hasher.hash_file(file_info(second));

    ASSERT_TRUE(firstHash.ok);
    ASSERT_TRUE(secondHash.ok);
    EXPECT_EQ(firstHash.hash, secondHash.hash);

    remove_file_tree(root, std::vector<std::string>{ first, second });
}

TEST(HasherTest, DifferentContentHasDifferentHash) {
    const std::string root = make_temp_dir();
    const std::string first = root + "/first.txt";
    const std::string second = root + "/second.txt";
    write_file(first, "alpha");
    write_file(second, "bravo");

    const Hasher hasher;
    const HashResult firstHash = hasher.hash_file(file_info(first));
    const HashResult secondHash = hasher.hash_file(file_info(second));

    ASSERT_TRUE(firstHash.ok);
    ASSERT_TRUE(secondHash.ok);
    EXPECT_NE(firstHash.hash, secondHash.hash);

    remove_file_tree(root, std::vector<std::string>{ first, second });
}

TEST(HasherTest, EmptyFileCanBeHashed) {
    const std::string root = make_temp_dir();
    const std::string file = root + "/empty.bin";
    write_file(file, "");

    const Hasher hasher;
    const HashResult result = hasher.hash_file(file_info(file));

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.error.message, "");

    remove_file_tree(root, std::vector<std::string>{ file });
}

TEST(HasherTest, MissingFileReturnsError) {
    const Hasher hasher;

    const HashResult result = hasher.hash_file(FileInfo{ "/tmp/threadpool_hasher_missing_file_for_test", 0 });

    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.message.empty());
}

TEST(HasherTest, SmallBufferMatchesDefaultBuffer) {
    const std::string root = make_temp_dir();
    const std::string file = root + "/long.txt";
    write_file(file, "abcdefghijklmnopqrstuvwxyz0123456789");

    const Hasher defaultHasher;
    const Hasher smallBufferHasher(3);
    const FileInfo info = file_info(file);

    const HashResult defaultResult = defaultHasher.hash_file(info);
    const HashResult smallBufferResult = smallBufferHasher.hash_file(info);

    ASSERT_TRUE(defaultResult.ok);
    ASSERT_TRUE(smallBufferResult.ok);
    EXPECT_EQ(defaultResult.hash, smallBufferResult.hash);

    remove_file_tree(root, std::vector<std::string>{ file });
}
