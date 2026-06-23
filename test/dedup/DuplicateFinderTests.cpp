#include "DuplicateFinder.h"
#include "ThreadPool.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

std::string make_temp_dir() {
    std::string pattern = "/tmp/threadpool_duplicate_finder_test_XXXXXX";
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

} // namespace

TEST(DuplicateFinderTest, FindsDuplicateFilesByContent) {
    const std::string root = make_temp_dir();
    const std::string first = root + "/a.txt";
    const std::string second = root + "/b.txt";
    const std::string third = root + "/c.txt";
    write_file(first, "same");
    write_file(second, "same");
    write_file(third, "different");

    wxm::ThreadPool pool(2, 8, false, 1000);
    FileWalker walker;
    Hasher hasher;
    const DuplicateFinder finder(pool, walker, hasher);
    const DuplicateReport report = finder.find_duplicates(root);

    ASSERT_EQ(report.groups.size(), 1u);
    EXPECT_EQ(report.groups[0].size, 4u);
    ASSERT_EQ(report.groups[0].paths.size(), 2u);
    EXPECT_EQ(report.groups[0].paths[0], first);
    EXPECT_EQ(report.groups[0].paths[1], second);

    remove_file_tree(root, std::vector<std::string>{ first, second, third });
}

TEST(DuplicateFinderTest, IgnoresFilesWithDifferentSizes) {
    const std::string root = make_temp_dir();
    const std::string first = root + "/a.txt";
    const std::string second = root + "/b.txt";
    write_file(first, "a");
    write_file(second, "aa");

    wxm::ThreadPool pool(2, 8, false, 1000);
    FileWalker walker;
    Hasher hasher;
    const DuplicateFinder finder(pool, walker, hasher);
    const DuplicateReport report = finder.find_duplicates(root);

    EXPECT_EQ(report.scannedFiles, 2u);
    EXPECT_EQ(report.hashedFiles, 0u);
    EXPECT_TRUE(report.groups.empty());

    remove_file_tree(root, std::vector<std::string>{ first, second });
}

TEST(DuplicateFinderTest, DoesNotReportUniqueSameSizeDifferentContent) {
    const std::string root = make_temp_dir();
    const std::string first = root + "/a.txt";
    const std::string second = root + "/b.txt";
    write_file(first, "ab");
    write_file(second, "cd");

    wxm::ThreadPool pool(2, 8, false, 1000);
    FileWalker walker;
    Hasher hasher;
    const DuplicateFinder finder(pool, walker, hasher);
    const DuplicateReport report = finder.find_duplicates(root);

    EXPECT_EQ(report.scannedFiles, 2u);
    EXPECT_EQ(report.hashedFiles, 2u);
    EXPECT_TRUE(report.groups.empty());

    remove_file_tree(root, std::vector<std::string>{ first, second });
}

TEST(DuplicateFinderTest, ReportsScannedAndHashedCounts) {
    const std::string root = make_temp_dir();
    const std::string first = root + "/a.txt";
    const std::string second = root + "/b.txt";
    const std::string third = root + "/c.txt";
    write_file(first, "one");
    write_file(second, "two");
    write_file(third, "larger");

    wxm::ThreadPool pool(2, 8, false, 1000);
    FileWalker walker;
    Hasher hasher;
    const DuplicateFinder finder(pool, walker, hasher);
    const DuplicateReport report = finder.find_duplicates(root);

    EXPECT_EQ(report.scannedFiles, 3u);
    EXPECT_EQ(report.hashedFiles, 2u);
    EXPECT_EQ(report.errors.size(), 0u);

    remove_file_tree(root, std::vector<std::string>{ first, second, third });
}

TEST(DuplicateFinderTest, HandlesManyDuplicateCandidates) {
    const std::string root = make_temp_dir();
    std::vector<std::string> files;

    for (int i = 0; i < 10; ++i) {
        const std::string path = root + "/same_" + std::to_string(i) + ".txt";
        write_file(path, "same-content");
        files.push_back(path);
    }

    for (int i = 0; i < 10; ++i) {
        const std::string path = root + "/unique_" + std::to_string(i) + ".txt";
        write_file(path, "unique-" + std::to_string(i));
        files.push_back(path);
    }

    wxm::ThreadPool pool(2, 8, false, 1000);
    FileWalker walker;
    Hasher hasher;
    const DuplicateFinder finder(pool, walker, hasher);
    const DuplicateReport report = finder.find_duplicates(root);

    ASSERT_EQ(report.groups.size(), 1u);
    EXPECT_EQ(report.groups[0].paths.size(), 10u);
    EXPECT_EQ(report.hashedFiles, 20u);

    remove_file_tree(root, files);
}

TEST(DuplicateFinderTest, UsesInjectedThreadPool) {
    const std::string root = make_temp_dir();
    const std::string first = root + "/a.txt";
    const std::string second = root + "/b.txt";
    write_file(first, "same");
    write_file(second, "same");

    wxm::ThreadPool pool(1, 2, false, 1000);
    FileWalker walker;
    Hasher hasher;
    const DuplicateFinder finder(pool, walker, hasher);
    const DuplicateReport report = finder.find_duplicates(root);
    ASSERT_EQ(report.groups.size(), 1u);
    EXPECT_EQ(report.groups[0].paths.size(), 2u);

    remove_file_tree(root, std::vector<std::string>{ first, second });
}

TEST(DuplicateFinderTest, ReportsWalkErrors) {
    const std::string missingPath = "/tmp/threadpool_duplicate_finder_missing_path_for_test";

    wxm::ThreadPool pool(2, 8, false, 1000);
    FileWalker walker;
    Hasher hasher;
    const DuplicateFinder finder(pool, walker, hasher);
    const DuplicateReport report = finder.find_duplicates(missingPath);

    EXPECT_EQ(report.scannedFiles, 0u);
    EXPECT_EQ(report.hashedFiles, 0u);
    EXPECT_TRUE(report.groups.empty());
    ASSERT_EQ(report.errors.size(), 1u);
    EXPECT_EQ(report.errors[0].phase, FileError::Phase::SCAN);
    EXPECT_EQ(report.errors[0].path, missingPath);
    EXPECT_NE(report.errors[0].message.find("lstat failed"), std::string::npos);
}
