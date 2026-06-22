#include "FileWalker.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

std::string make_temp_dir() {
    std::string pattern = "/tmp/threadpool_dedup_test_XXXXXX";
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

void remove_tree(const std::string& root) {
    const FileWalker walker;
    FileWalkResult result = walker.collect_files(root);
    for (std::vector<FileInfo>::const_iterator it = result.files.begin(); it != result.files.end(); ++it) {
        std::remove(it->path.c_str());
    }
    rmdir((root + "/nested").c_str());
    rmdir(root.c_str());
}

} // namespace

TEST(FileWalkerTest, CollectsRegularFilesRecursively) {
    const std::string root = make_temp_dir();
    const std::string nested = root + "/nested";
    ASSERT_EQ(mkdir(nested.c_str(), 0700), 0);

    write_file(root + "/a.txt", "alpha");
    write_file(nested + "/b.txt", "bravo!");

    const FileWalker walker;
    FileWalkResult result = walker.collect_files(root);

    ASSERT_EQ(result.errors.size(), 0u);
    ASSERT_EQ(result.files.size(), 2u);
    EXPECT_EQ(result.files[0].path, root + "/a.txt");
    EXPECT_EQ(result.files[0].size, 5u);
    EXPECT_EQ(result.files[1].path, nested + "/b.txt");
    EXPECT_EQ(result.files[1].size, 6u);

    remove_tree(root);
}

TEST(FileWalkerTest, CollectsSingleRegularFileRoot) {
    const std::string root = make_temp_dir();
    const std::string filePath = root + "/single.bin";
    write_file(filePath, "content");

    const FileWalker walker;
    FileWalkResult result = walker.collect_files(filePath);

    ASSERT_EQ(result.errors.size(), 0u);
    ASSERT_EQ(result.files.size(), 1u);
    EXPECT_EQ(result.files[0].path, filePath);
    EXPECT_EQ(result.files[0].size, 7u);

    remove_tree(root);
}

TEST(FileWalkerTest, ReportsMissingPathAsError) {
    const FileWalker walker;
    FileWalkResult result = walker.collect_files("/tmp/threadpool_dedup_missing_path_for_test");

    EXPECT_TRUE(result.files.empty());
    ASSERT_EQ(result.errors.size(), 1u);
}
