#include "system_info.hpp"
#include "utils.hpp"
#include <cstdlib>
#include <string>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <cctype>
#include "ports.hpp"

using namespace std;

static uint64_t parse_first_uint_in_text_mb(const std::string& text) {
    // 数字列を拾ってMiB相当とみなす（nvidia-smi --format=... nounits想定ならMB）
    uint64_t val = 0;
    size_t i = 0;
    while (i < text.size() && !isdigit((unsigned char)text[i])) ++i;
    while (i < text.size() && isdigit((unsigned char)text[i])) {
        val = val * 10 + (text[i]-'0');
        ++i;
    }
    return val;
}

SystemInfo detect_system_info() {
    SystemInfo si;
#if defined(__APPLE__)
    si.is_macos = true;
#elif defined(_WIN32)
    si.is_windows = true;
#elif defined(__linux__)
    si.is_linux = true;
#endif

    // RAM
    if (si.is_macos) {
        string out = utils::run_shell("sysctl -n hw.memsize 2>/dev/null");
        if (!out.empty()) {
            // bytes
            try {
                si.ram_bytes = std::stoull(out);
            } catch (const std::invalid_argument&) {
                si.ram_bytes = 0;
            } catch (const std::out_of_range&) {
                si.ram_bytes = 0;
            }
        }
        // Fallback: system_profiler SPHardwareDataType の "Memory:" 行から取得（例: "Memory: 32 GB" / 日本語環境: "メモリ: 32 GB"）
        if (si.ram_bytes == 0) {
            string hw = utils::run_shell("system_profiler SPHardwareDataType 2>/dev/null");
            // メモリ値抽出
            {
                istringstream iss(hw); string line;
                while (getline(iss, line)) {
                    string low = line; transform(low.begin(), low.end(), low.begin(), ::tolower);
                    if (low.find("memory:") != string::npos || low.find("メモリ:") != string::npos) {
                        uint64_t v = parse_first_uint_in_text_mb(line);
                        if (line.find("GB") != string::npos && v > 0) si.ram_bytes = v * 1024ull * 1024ull * 1024ull;
                        else if (line.find("MB") != string::npos && v > 0) si.ram_bytes = v * 1024ull * 1024ull;
                        break;
                    }
                }
            }
        }
        string arch = utils::run_shell("uname -m");
        string brand = utils::run_shell("sysctl -n machdep.cpu.brand_string 2>/dev/null");
        std::string brand_l = brand; transform(brand_l.begin(), brand_l.end(), brand_l.begin(), ::tolower);
        si.is_apple_silicon = (arch.find("arm64")!=string::npos) || (brand_l.find("apple")!=string::npos);
        // GPU/VRAM (Apple/AMD/NVIDIA)
        string sp = utils::run_shell("system_profiler SPDisplaysDataType 2>/dev/null");
        // ざっくりVRAM行を拾う
        // 例: "VRAM (合計): 15360 MB" / "VRAM (Total): 24 GB"
        size_t pos = sp.find("VRAM");
        if (pos != string::npos) {
            string tail = sp.substr(pos, 200);
            uint64_t v = parse_first_uint_in_text_mb(tail);
            // GB表記の場合は手直し
            if (tail.find("GB") != string::npos && v > 0) v *= 1024;
            si.vram_mb = v;
        }
        // GPU名（Chipset Model優先。見つからなければ内容のある行のみ採用）
        {
            std::istringstream iss(sp);
            std::string line;
            std::string candidate;
            while (std::getline(iss, line)) {
                auto low = line; transform(low.begin(), low.end(), low.begin(), ::tolower);
                auto has_value_after_colon = [&](){ auto p=line.find(':'); if (p==string::npos) return false; auto rest=utils::trim(line.substr(p+1)); return !rest.empty(); }();
                if (low.find("chipset model") != string::npos && has_value_after_colon) { si.gpu_name = line; break; }
                if (candidate.empty() && (low.find("model:")!=string::npos || low.find("graphics")!=string::npos) && has_value_after_colon) candidate = line;
            }
            if (si.gpu_name.empty()) si.gpu_name = candidate; // 何もなければ空のまま
        }
        // NVIDIA存在確認（外付けeGPUなど）
        string nv = utils::run_shell("which nvidia-smi >/dev/null 2>&1 && nvidia-smi -L 2>/dev/null");
        si.has_nvidia = !nv.empty();
        if (si.vram_mb == 0 && si.has_nvidia) {
            string mem = utils::run_shell("nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>/dev/null");
            auto pos = mem.find('\n');
            if (pos!=string::npos) mem = mem.substr(0,pos);
            si.vram_mb = parse_first_uint_in_text_mb(mem);
        }
    } else if (si.is_linux) {
        // RAM: /proc/meminfo を直接パース
        {
            std::ifstream ifs("/proc/meminfo");
            std::string line;
            uint64_t kb = 0;
            while (std::getline(ifs, line)) {
                if (line.rfind("MemTotal:", 0) == 0) {
                    kb = parse_first_uint_in_text_mb(line);
                    break;
                }
            }
            si.ram_bytes = kb * 1024ULL;
        }
        // NVIDIA
        string nv = utils::run_shell("which nvidia-smi >/dev/null 2>&1 && nvidia-smi -L 2>/dev/null");
        si.has_nvidia = !nv.empty();
        if (si.has_nvidia) {
            string mem = utils::run_shell("nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>/dev/null");
            auto pos = mem.find('\n'); if (pos!=string::npos) mem = mem.substr(0,pos);
            si.vram_mb = parse_first_uint_in_text_mb(mem);
            si.gpu_name = utils::run_shell("nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null");
            auto pos2 = si.gpu_name.find('\n'); if (pos2!=string::npos) si.gpu_name = si.gpu_name.substr(0,pos2);
        }
        // iGPU等はVRAM不明のことが多いので0のまま
    } else if (si.is_windows) {
        // RAM（wmic が無ければ PowerShell CIM を使用）
        string out = utils::run_shell("wmic computersystem get TotalPhysicalMemory /value 2>NUL");
        if (out.find("TotalPhysicalMemory=") == string::npos) {
            out = utils::run_shell("powershell -NoProfile -Command \"(Get-CimInstance -ClassName Win32_ComputerSystem).TotalPhysicalMemory\" 2>NUL");
        }
        {
            std::string digits; for (char c: out) if (isdigit((unsigned char)c)) digits += c;
            if (!digits.empty()) {
                try {
                    si.ram_bytes = std::stoull(digits);
                } catch (const std::invalid_argument&) {
                    si.ram_bytes = 0;
                } catch (const std::out_of_range&) {
                    si.ram_bytes = 0;
                }
            }
        }
        // NVIDIA
        string nv = utils::run_shell("nvidia-smi -L 2>NUL");
        si.has_nvidia = !nv.empty();
        if (si.has_nvidia) {
            string mem = utils::run_shell("nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>NUL");
            auto pos = mem.find('\n'); if (pos!=string::npos) mem = mem.substr(0,pos);
            si.vram_mb = parse_first_uint_in_text_mb(mem);
            si.gpu_name = utils::run_shell("nvidia-smi --query-gpu=name --format=csv,noheader 2>NUL");
            auto pos2 = si.gpu_name.find('\n'); if (pos2!=string::npos) si.gpu_name = si.gpu_name.substr(0,pos2);
        } else {
            // 非NVIDIA: WMIでVRAM/名前を取得
            string json = utils::run_shell("powershell -NoProfile -Command \"Get-CimInstance Win32_VideoController | Select-Object -First 1 AdapterRAM,Name | ConvertTo-Json\" 2>NUL");
            // 数字（AdapterRAM bytes）を抽出
            {
                std::string digits; for (char c: json) if (isdigit((unsigned char)c)) digits += c;
                if (!digits.empty()) {
                    try {
                        uint64_t bytes = std::stoull(digits);
                        si.vram_mb = bytes / (1024ull*1024ull);
                    } catch (const std::invalid_argument&) {
                        si.vram_mb = 0;
                    } catch (const std::out_of_range&) {
                        si.vram_mb = 0;
                    }
                }
            }
            std::string name;
            if (utils::json_find_first_string_value(json, "Name", name)) si.gpu_name = name;
        }
    }
    return si;
}

InferenceTuning decide_tuning(const SystemInfo& si) {
    InferenceTuning t;
    // メモリ/VRAMに応じた大雑把なヒューリスティクス
    const uint64_t ram_gb = si.ram_bytes / (1024ull*1024ull*1024ull);
    const uint64_t vram = si.vram_mb;

    if (vram >= 20000) {
        t.context = 8192;
        t.max_tokens = 1024;
        t.temperature = 0.7;
        t.top_p = 0.9;
        t.gpu_layers = 100; // LM Studio向け: ほぼ全レイヤオフロード（実際はモデル依存で丸められる）
    } else if (vram >= 12000) {
        t.context = 6144;
        t.max_tokens = 900;
        t.gpu_layers = 60;
    } else if (vram >= 8000) {
        t.context = 4096;
        t.max_tokens = 768;
        t.gpu_layers = 45;
    } else if (vram >= 4000) {
        t.context = 3072;
        t.max_tokens = 640;
        t.gpu_layers = 30;
    } else {
        // GPUなし or 低VRAM
        if (si.is_macos && si.is_apple_silicon) {
            t.context = 3072;
            t.max_tokens = 640;
            t.gpu_layers = 35; // Metalである程度オフロード可能
        } else {
            t.context = 2048;
            t.max_tokens = 512;
            t.gpu_layers = 0;  // CPU推奨
        }
        t.temperature = 0.7;
        t.top_p = 0.9;
    }

    // RAMが少なければ控えめに
    if (ram_gb <= 8) {
        t.context = std::min(t.context, 2048);
        t.max_tokens = std::min(t.max_tokens, 512);
    }
    return t;
}

// 依存注入・OS指定付きのユニットテスト向けAPI
static std::string read_file_default(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    std::ostringstream oss; oss << ifs.rdbuf(); return oss.str();
}

SystemInfo detect_system_info_with(IShell& shell,
                                   const std::function<std::string(const std::string&)>& read_file,
                                   bool is_macos, bool is_linux, bool is_windows) {
    SystemInfo si;
    si.is_macos = is_macos; si.is_linux = is_linux; si.is_windows = is_windows;

    if (si.is_macos) {
        std::string out = shell.run("sysctl -n hw.memsize 2>/dev/null");
        if (!out.empty()) { 
            try { si.ram_bytes = std::stoull(out); } catch (const std::exception&) { si.ram_bytes = 0; }
        }
        if (si.ram_bytes == 0) {
            std::string hw = shell.run("system_profiler SPHardwareDataType 2>/dev/null");
            std::istringstream iss(hw); std::string line;
            while (std::getline(iss, line)) {
                std::string low = line; std::transform(low.begin(), low.end(), low.begin(), ::tolower);
                if (low.find("memory:") != std::string::npos || low.find("メモリ:") != std::string::npos) {
                    uint64_t v = parse_first_uint_in_text_mb(line);
                    if (line.find("GB") != std::string::npos && v > 0) si.ram_bytes = v * 1024ull * 1024ull * 1024ull;
                    else if (line.find("MB") != std::string::npos && v > 0) si.ram_bytes = v * 1024ull * 1024ull;
                    break;
                }
            }
        }
        std::string arch = shell.run("uname -m");
        std::string brand = shell.run("sysctl -n machdep.cpu.brand_string 2>/dev/null");
        std::string brand_l = brand; std::transform(brand_l.begin(), brand_l.end(), brand_l.begin(), ::tolower);
        si.is_apple_silicon = (arch.find("arm64")!=std::string::npos) || (brand_l.find("apple")!=std::string::npos);
        std::string sp = shell.run("system_profiler SPDisplaysDataType 2>/dev/null");
        size_t pos = sp.find("VRAM");
        if (pos != std::string::npos) {
            std::string tail = sp.substr(pos, 200);
            uint64_t v = parse_first_uint_in_text_mb(tail);
            if (tail.find("GB") != std::string::npos && v > 0) v *= 1024;
            si.vram_mb = v;
        }
        {
            std::istringstream iss(sp); std::string line; std::string candidate;
            while (std::getline(iss, line)) {
                auto low = line; std::transform(low.begin(), low.end(), low.begin(), ::tolower);
                auto has_value_after_colon = [&](){ auto p=line.find(':'); if (p==std::string::npos) return false; auto rest=utils::trim(line.substr(p+1)); return !rest.empty(); }();
                if (low.find("chipset model") != std::string::npos && has_value_after_colon) { si.gpu_name = line; break; }
                if (candidate.empty() && (low.find("model:")!=std::string::npos || low.find("graphics")!=std::string::npos) && has_value_after_colon) candidate = line;
            }
            if (si.gpu_name.empty()) si.gpu_name = candidate;
        }
        std::string nv = shell.run("which nvidia-smi >/dev/null 2>&1 && nvidia-smi -L 2>/dev/null");
        si.has_nvidia = !nv.empty();
        if (si.vram_mb == 0 && si.has_nvidia) {
            std::string mem = shell.run("nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>/dev/null");
            auto nl = mem.find('\n'); if (nl!=std::string::npos) mem = mem.substr(0,nl);
            si.vram_mb = parse_first_uint_in_text_mb(mem);
        }
    } else if (si.is_linux) {
        {
            std::string content = read_file("/proc/meminfo");
            std::istringstream ifs(content); std::string line; uint64_t kb=0;
            while (std::getline(ifs, line)) {
                if (line.rfind("MemTotal:", 0) == 0) { kb = parse_first_uint_in_text_mb(line); break; }
            }
            si.ram_bytes = kb * 1024ULL;
        }
        std::string nv = shell.run("which nvidia-smi >/dev/null 2>&1 && nvidia-smi -L 2>/dev/null");
        si.has_nvidia = !nv.empty();
        if (si.has_nvidia) {
            std::string mem = shell.run("nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>/dev/null");
            auto nl = mem.find('\n'); if (nl!=std::string::npos) mem = mem.substr(0,nl);
            si.vram_mb = parse_first_uint_in_text_mb(mem);
            si.gpu_name = shell.run("nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null");
            auto nl2 = si.gpu_name.find('\n'); if (nl2!=std::string::npos) si.gpu_name = si.gpu_name.substr(0,nl2);
        }
    } else if (si.is_windows) {
        std::string out = shell.run("wmic computersystem get TotalPhysicalMemory /value 2>NUL");
        if (out.find("TotalPhysicalMemory=") == std::string::npos) {
            out = shell.run("powershell -NoProfile -Command \"(Get-CimInstance -ClassName Win32_ComputerSystem).TotalPhysicalMemory\" 2>NUL");
        }
        { 
            std::string digits; 
            for (char c: out) if (isdigit((unsigned char)c)) digits += c; 
            if (!digits.empty()) { 
                try { 
                    si.ram_bytes = std::stoull(digits); 
                } catch(const std::exception&) {
                    si.ram_bytes = 0;
                }
            }
        }
        std::string nv = shell.run("nvidia-smi -L 2>NUL");
        si.has_nvidia = !nv.empty();
        if (si.has_nvidia) {
            std::string mem = shell.run("nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>NUL");
            auto nl = mem.find('\n'); if (nl!=std::string::npos) mem = mem.substr(0,nl);
            si.vram_mb = parse_first_uint_in_text_mb(mem);
            si.gpu_name = shell.run("nvidia-smi --query-gpu=name --format=csv,noheader 2>NUL");
            auto nl2 = si.gpu_name.find('\n'); if (nl2!=std::string::npos) si.gpu_name = si.gpu_name.substr(0,nl2);
        } else {
            std::string json = shell.run("powershell -NoProfile -Command \"Get-CimInstance Win32_VideoController | Select-Object -First 1 AdapterRAM,Name | ConvertTo-Json\" 2>NUL");
            { 
                std::string digits; 
                for (char c: json) if (isdigit((unsigned char)c)) digits += c; 
                if (!digits.empty()) { 
                    try { 
                        uint64_t bytes = std::stoull(digits); 
                        si.vram_mb = bytes/(1024ull*1024ull); 
                    } catch(const std::exception&) {
                        si.vram_mb = 0;
                    }
                }
            }
            std::string name; if (utils::json_find_first_string_value(json, "Name", name)) si.gpu_name = name;
        }
    }
    return si;
}
