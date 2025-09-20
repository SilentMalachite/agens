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
};

std::filesystem::path default_config_path();
bool load_config(AppConfig& cfg, std::string* err = nullptr);
bool save_config(const AppConfig& cfg, std::string* err = nullptr);

