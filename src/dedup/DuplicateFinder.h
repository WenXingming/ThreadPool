#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "FileInfo.h"
#include "FileWalker.h"
#include "Hasher.h"

namespace wxm {
class ThreadPool;
}

struct DuplicateGroup {
    uint64_t size;
    uint64_t hash;
    std::vector<std::string> paths;
};

struct DuplicateReport {
    size_t scannedFiles = 0;
    size_t hashedFiles = 0;
    std::vector<DuplicateGroup> groups;
    std::vector<FileError> errors;
};

class DuplicateFinder {
public:
    explicit DuplicateFinder(wxm::ThreadPool& pool, const FileWalker& fileWalker, const Hasher& hasher);

    DuplicateReport find_duplicates(const std::string& rootPath) const;

private:
    std::map<uint64_t, std::vector<FileInfo> > group_files_by_size(const std::vector<FileInfo>& files) const;
    std::vector<FileInfo> collect_hash_candidates(const std::map<uint64_t, std::vector<FileInfo>>& filesBySize) const;
    std::vector<HashResult> hash_candidates(const std::vector<FileInfo>& candidates) const;
    std::map<std::pair<uint64_t, uint64_t>, std::vector<std::string> > group_by_hash_and_size(const std::vector<HashResult>& results) const;
    std::vector<DuplicateGroup> extract_duplicate_groups(const std::map<std::pair<uint64_t, uint64_t>, std::vector<std::string> >& buckets) const;
    DuplicateReport assemble_report(const FileWalkResult& walkResult, const std::vector<HashResult>& hashResults, const std::vector<DuplicateGroup>& duplicateGroups) const;

    wxm::ThreadPool& pool_;
    const FileWalker& fileWalker_;
    const Hasher& hasher_;
};
