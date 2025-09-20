#pragma once
#include <string>
#include <vector>
#include "ports.hpp"

struct WebResult {
    std::string title; // 可能なら
    std::string text;
    std::string url;
};

std::vector<WebResult> web_search(IHttp& http, const std::string& query, int max_results = 5);

