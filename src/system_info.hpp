#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include "ports.hpp"

struct SystemInfo {
    bool is_linux = false;
    bool is_macos = false;
    bool is_windows = false;
    bool has_nvidia = false;
    bool is_apple_silicon = false;
    std::string gpu_name;
    uint64_t ram_bytes = 0; // 物理メモリ
    uint64_t vram_mb = 0;   // 推定VRAM(MiB)
};

SystemInfo detect_system_info();
SystemInfo detect_system_info_with(IShell& shell,
                                   const std::function<std::string(const std::string&)>& read_file,
                                   bool is_macos, bool is_linux, bool is_windows);

// パラメータ推奨値（バックエンド非依存の抽象）
struct InferenceTuning {
    int context = 4096;
    int max_tokens = 512;
    double temperature = 0.7;
    double top_p = 0.9;
    int gpu_layers = -1; // LM Studio用。-1=自動/全オフロード
};

InferenceTuning decide_tuning(const SystemInfo& si);
