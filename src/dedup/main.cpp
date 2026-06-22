#include <iostream>
#include <string>

#include "DuplicateFinder.h"

namespace {

int print_usage(const char* programName) {
    std::cerr << "Usage: " << programName << " <directory>\n";
    return 1;
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
    if (argc != 2) {
        return print_usage(argv[0]);
    }

    const std::string directory = argv[1];

    const DuplicateFinder finder;
    const DuplicateReport report = finder.find_duplicates(directory);
    print_report(report);
    return 0;
}
