#include "DuplicateFinder.h"
#include "ThreadPool.h"

#include <algorithm>
#include <future>

DuplicateFinder::DuplicateFinder(wxm::ThreadPool& pool, const FileWalker& fileWalker, const Hasher& hasher)
    : pool_(pool), fileWalker_(fileWalker), hasher_(hasher) {
}

DuplicateReport DuplicateFinder::find_duplicates(const std::string& rootPath) const {
    const FileWalkResult walkResult = fileWalker_.collect_files(rootPath);
    const std::map<uint64_t, std::vector<FileInfo> > filesBySize = group_files_by_size(walkResult.files);
    const std::vector<FileInfo> candidates = collect_hash_candidates(filesBySize);
    const std::vector<HashResult> hashResults = hash_candidates(candidates);
    const std::map<std::pair<uint64_t, uint64_t>, std::vector<std::string> > buckets = group_by_hash_and_size(hashResults);
    const std::vector<DuplicateGroup> duplicateGroups = extract_duplicate_groups(buckets);

    return assemble_report(walkResult, hashResults, duplicateGroups);
}

std::map<uint64_t, std::vector<FileInfo> > DuplicateFinder::group_files_by_size(const std::vector<FileInfo>& files) const {
    std::map<uint64_t, std::vector<FileInfo> > filesBySize;
    for (std::vector<FileInfo>::const_iterator it = files.begin(); it != files.end(); ++it) {
        filesBySize[it->size].push_back(*it);
    }
    return filesBySize;
}

std::vector<FileInfo> DuplicateFinder::collect_hash_candidates(const std::map<uint64_t, std::vector<FileInfo> >& filesBySize) const {
    std::vector<FileInfo> candidates;
    for (std::map<uint64_t, std::vector<FileInfo> >::const_iterator groupIt = filesBySize.begin(); groupIt != filesBySize.end(); ++groupIt) {
        const std::vector<FileInfo>& files = groupIt->second;
        if (files.size() < 2) { // 性能优化：大小唯一的文件不可能是重复的，直接跳过，避免计算哈希
            continue;
        }

        for (std::vector<FileInfo>::const_iterator fileIt = files.begin(); fileIt != files.end(); ++fileIt) {
            candidates.push_back(*fileIt);
        }
    }
    return candidates;
}

std::vector<HashResult> DuplicateFinder::hash_candidates(const std::vector<FileInfo>& candidates) const {
    std::vector<std::future<HashResult> > futures;
    futures.reserve(candidates.size());

    for (std::vector<FileInfo>::const_iterator it = candidates.begin(); it != candidates.end(); ++it) {
        const FileInfo file = *it;
        futures.push_back(pool_.submit_task([this, file]() {
            return hasher_.hash_file(file);
            }));
    }

    std::vector<HashResult> results;
    results.reserve(futures.size());
    for (std::vector<std::future<HashResult> >::iterator it = futures.begin(); it != futures.end(); ++it) {
        results.push_back(it->get());
    }
    return results;
}

std::map<std::pair<uint64_t, uint64_t>, std::vector<std::string> > DuplicateFinder::group_by_hash_and_size(const std::vector<HashResult>& results) const {
    std::map<std::pair<uint64_t, uint64_t>, std::vector<std::string> > filesBySignature;
    for (std::vector<HashResult>::const_iterator it = results.begin(); it != results.end(); ++it) {
        if (!it->ok) {
            continue;
        }

        filesBySignature[std::make_pair(it->file.size, it->hash)].push_back(it->file.path);
    }

    return filesBySignature;
}

std::vector<DuplicateGroup> DuplicateFinder::extract_duplicate_groups(const std::map<std::pair<uint64_t, uint64_t>, std::vector<std::string> >& buckets) const {
    std::vector<DuplicateGroup> duplicateGroups;
    for (std::map<std::pair<uint64_t, uint64_t>, std::vector<std::string> >::const_iterator it = buckets.begin(); it != buckets.end(); ++it) {
        if (it->second.size() < 2) {
            continue;
        }

        DuplicateGroup group;
        group.size = it->first.first;
        group.hash = it->first.second;
        group.paths = it->second;
        duplicateGroups.push_back(group);
    }
    return duplicateGroups;
}

DuplicateReport DuplicateFinder::assemble_report(const FileWalkResult& walkResult, const std::vector<HashResult>& hashResults, const std::vector<DuplicateGroup>& duplicateGroups) const {
    DuplicateReport report;
    report.scannedFiles = walkResult.files.size();
    report.errors.insert(report.errors.end(), walkResult.errors.begin(), walkResult.errors.end());

    for (std::vector<HashResult>::const_iterator it = hashResults.begin(); it != hashResults.end(); ++it) {
        if (!it->ok) {
            report.errors.push_back(it->error);
        } else {
            ++report.hashedFiles;
        }
    }

    report.groups = duplicateGroups;

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

    std::sort(report.errors.begin(), report.errors.end(), [](const FileError& left, const FileError& right) {
        if (left.phase != right.phase) {
            return left.phase < right.phase;
        }
        return left.path < right.path;
        });

    return report;
}
