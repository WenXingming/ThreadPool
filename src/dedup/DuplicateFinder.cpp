#include "DuplicateFinder.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <utility>

namespace {

typedef std::pair<uint64_t, uint64_t> SizeHashKey;

std::string format_walk_error(const WalkError& error) {
    std::ostringstream oss;
    oss << error.path << ": " << error.message;
    return oss.str();
}

std::string format_hash_error(const HashResult& result) {
    std::ostringstream oss;
    oss << result.file.path << ": " << result.error;
    return oss.str();
}

void sort_report(DuplicateReport& report) {
    for (std::vector<DuplicateGroup>::iterator it = report.groups.begin(); it != report.groups.end(); ++it) {
        std::sort(it->paths.begin(), it->paths.end());
    }

    std::sort(report.groups.begin(), report.groups.end(), [](const DuplicateGroup& left, const DuplicateGroup& right) {
        if (left.size != right.size) {
            return left.size < right.size;
        }
        if (left.hash != right.hash) {
            return left.hash < right.hash;
        }
        if (left.paths.empty() || right.paths.empty()) {
            return left.paths.size() < right.paths.size();
        }
        return left.paths[0] < right.paths[0];
        });

    std::sort(report.errors.begin(), report.errors.end());
}

} // namespace

DuplicateReport DuplicateFinder::find_duplicates(const std::string& rootPath) const {
    DuplicateReport report;
    report.scannedFiles = 0;
    report.hashedFiles = 0;
    report.errorCount = 0;

    const FileWalkResult walkResult = fileWalker_.collect_files(rootPath);
    report.scannedFiles = walkResult.files.size();

    for (std::vector<WalkError>::const_iterator it = walkResult.errors.begin(); it != walkResult.errors.end(); ++it) {
        report.errors.push_back(format_walk_error(*it));
    }

    std::map<uint64_t, std::vector<FileInfo> > filesBySize;
    for (std::vector<FileInfo>::const_iterator it = walkResult.files.begin(); it != walkResult.files.end(); ++it) {
        filesBySize[it->size].push_back(*it);
    }

    std::map<SizeHashKey, std::vector<std::string> > filesBySizeAndHash;
    for (std::map<uint64_t, std::vector<FileInfo> >::const_iterator groupIt = filesBySize.begin(); groupIt != filesBySize.end(); ++groupIt) {
        const std::vector<FileInfo>& files = groupIt->second;
        if (files.size() < 2) {
            continue;
        }

        for (std::vector<FileInfo>::const_iterator fileIt = files.begin(); fileIt != files.end(); ++fileIt) {
            const HashResult hashResult = hasher_.hash_file(*fileIt);
            if (!hashResult.ok) {
                report.errors.push_back(format_hash_error(hashResult));
                continue;
            }

            ++report.hashedFiles;
            filesBySizeAndHash[SizeHashKey(fileIt->size, hashResult.hash)].push_back(fileIt->path);
        }
    }

    for (std::map<SizeHashKey, std::vector<std::string> >::const_iterator it = filesBySizeAndHash.begin(); it != filesBySizeAndHash.end(); ++it) {
        if (it->second.size() < 2) {
            continue;
        }

        DuplicateGroup group;
        group.size = it->first.first;
        group.hash = it->first.second;
        group.paths = it->second;
        report.groups.push_back(group);
    }

    report.errorCount = report.errors.size();
    sort_report(report);
    return report;
}
