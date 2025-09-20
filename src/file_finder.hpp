#pragma once
#include <string>
#include <vector>

struct FileHit {
    std::string path;
    int score = 0;
    std::string snippet; // 代表的な一致行（先頭数件を連結）
};

std::vector<FileHit> find_relevant_files(const std::string& base_dir, const std::string& query, int max_results = 10);

