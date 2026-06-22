#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "DuplicateFinder.h"

namespace {

int print_usage(const char* programName) {
    std::cerr << "Usage: " << programName << " <directory> [--threads N]\n";
    return 1;
}

bool parse_positive_int(const std::string& text, int& value) {
    if (text.empty()) {
        return false;
    }

    for (std::string::const_iterator it = text.begin(); it != text.end(); ++it) {
        if (*it < '0' || *it > '9') {
            return false;
        }
    }

    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0' || parsed <= 0 || parsed > INT_MAX) {
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

bool parse_args(int argc, char* argv[], std::string& directory, DuplicateFinderConfig& config) {
    if (argc != 2 && argc != 4) {
        return false;
    }

    directory = argv[1];
    if (argc == 2) {
        return true;
    }

    if (std::string(argv[2]) != "--threads") {
        return false;
    }

    int threadCount = 0;
    if (!parse_positive_int(argv[3], threadCount) || threadCount > INT_MAX / 4) {
        return false;
    }

    config.threadCount = threadCount;
    config.queueCapacity = threadCount * 4;
    return true;
}

void print_report(const DuplicateReport& report) {
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
    std::cout << "  threads: " << report.threadCount << "\n";
    std::cout << "  duplicate groups: " << report.groups.size() << "\n";
    std::cout << "  errors: " << report.errorCount << "\n";

    if (!report.errors.empty()) {
        std::cout << "\nErrors:\n";
        for (std::vector<std::string>::const_iterator it = report.errors.begin(); it != report.errors.end(); ++it) {
            std::cout << "  " << *it << "\n";
        }
    }
}

} // namespace

int main(int argc, char* argv[]) {
    std::string directory;
    DuplicateFinderConfig config;
    if (!parse_args(argc, argv, directory, config)) {
        return print_usage(argv[0]);
    }

    const DuplicateFinder finder(config);
    const DuplicateReport report = finder.find_duplicates(directory);
    print_report(report);
    return 0;
}
