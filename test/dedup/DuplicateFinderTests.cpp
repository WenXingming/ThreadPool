#include "DuplicateFinder.h"

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

    const DuplicateFinder finder;
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

    const DuplicateFinder finder;
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

    const DuplicateFinder finder;
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

    const DuplicateFinder finder;
    const DuplicateReport report = finder.find_duplicates(root);

    EXPECT_EQ(report.scannedFiles, 3u);
    EXPECT_EQ(report.hashedFiles, 2u);
    EXPECT_EQ(report.errorCount, 0u);

    remove_file_tree(root, std::vector<std::string>{ first, second, third });
}
