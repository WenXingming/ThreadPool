#include <iostream>
#include <string>

#include "FileWalker.h"

namespace {

int print_usage(const char* programName) {
    std::cerr << "Usage: " << programName << " <directory>\n";
    return 1;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        return print_usage(argv[0]);
    }

    const std::string directory = argv[1];

    const FileWalker walker;
    const FileWalkResult result = walker.collect_files(directory);

    std::cout << "directory: " << directory << "\n";
    std::cout << "scanned files: " << result.files.size() << "\n";
    std::cout << "errors: " << result.errors.size() << "\n";
    return 0;
}
