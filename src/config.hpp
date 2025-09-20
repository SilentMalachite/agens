#pragma once
#include <string>
#include <vector>
#include <filesystem>

struct AppConfig {
    std::vector<std::string> allow_patterns;
    std::vector<std::string> deny_patterns;
    bool auto_confirm = false;
    bool auto_dry_run = false;
    std::string last_backend; // "ollama" or "lmstudio"
    std::string last_model;
    std::string last_cwd;
    // Apple Silicon等の統合メモリ環境で、GPUが利用可能とみなすRAM比率（0.1〜0.9）
    double unified_gpu_ratio = 0.5;
    // UIメッセージ言語（"ja"|"en"）。空なら既定=ja
    std::string language;
};

std::filesystem::path default_config_path();
bool load_config(AppConfig& cfg, std::string* err = nullptr);
bool save_config(const AppConfig& cfg, std::string* err = nullptr);
