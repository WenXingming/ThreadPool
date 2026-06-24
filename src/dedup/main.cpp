#include <iostream>
#include <string>
#include <vector>
#include <thread>

#include <CLI/CLI.hpp>

#include "DuplicateFinder.h"
#include "ThreadPool.h"

namespace {

void print_report(const DuplicateReport& report, int threadCount) {
    if (report.groups.empty()) {
        std::cout << "No duplicate files found.\n\n";
    }
    else {
        for (size_t i = 0; i < report.groups.size(); ++i) {
            const DuplicateGroup& group = report.groups[i];
            std::cout << "Duplicate group #" << (i + 1)
                << ", size=" << group.size
                << ", count=" << group.paths.size()
                << "\n";
            for (std::vector<std::string>::const_iterator it = group.paths.begin(); it != group.paths.end(); ++it) {
                std::cout << "  " << *it << "\n";
            }
            std::cout << "\n";
        }
    }

    std::cout << "Summary:\n";
    std::cout << "  scanned files: " << report.scannedFiles << "\n";
    std::cout << "  hashed files: " << report.hashedFiles << "\n";
    std::cout << "  threads: " << threadCount << "\n";
    std::cout << "  duplicate groups: " << report.groups.size() << "\n";
    std::cout << "  errors: " << report.errors.size() << "\n";

    if (!report.errors.empty()) {
        std::cout << "\nErrors:\n";
        for (std::vector<FileError>::const_iterator it = report.errors.begin(); it != report.errors.end(); ++it) {
            std::cout << "  [" << (it->phase == FileError::Phase::SCAN ? "SCAN" : "HASH") << "] "
                << it->path << ": " << it->message << "\n";
        }
    }
}

} // namespace

int main(int argc, char* argv[]) {
    std::string directory;
    const unsigned int hardwareThreads = std::thread::hardware_concurrency();
    int threadCount = hardwareThreads == 0 ? 2 : static_cast<int>(hardwareThreads);

    CLI::App app{ "Duplicate File Finder" };

    app.add_option("directory", directory, "Directory to scan for duplicates")
        ->required()
        ->check(CLI::ExistingDirectory);

    app.add_option("--threads,-t", threadCount, "Number of threads to use for hashing")
        ->check(CLI::PositiveNumber);

    CLI11_PARSE(app, argc, argv);

    wxm::ThreadPool pool(threadCount, threadCount * 4, false, 1000);
    FileWalker walker;
    Hasher hasher;
    const DuplicateFinder finder(pool, walker, hasher);
    const DuplicateReport report = finder.find_duplicates(directory);
    print_report(report, threadCount);
    return 0;
}
