#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <array>
#include <fstream>
#include <filesystem>
#include <random>
#include <chrono>

namespace utils {

inline std::string shell_escape_single_quotes(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        if (c == '\'') {
            out += "'\"'\"'"; // close ', add '"'"', reopen '
        } else {
            out += c;
        }
    }
    return out;
}

inline std::string run_shell(const std::string& cmd) {
    std::array<char, 4096> buffer{};
    std::string result;
#if defined(_WIN32)
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) throw std::runtime_error("popen() failed");
    while (true) {
        size_t n = fread(buffer.data(), 1, buffer.size(), pipe);
        if (n > 0) result.append(buffer.data(), n);
        if (n < buffer.size()) break;
    }
    int rc;
#if defined(_WIN32)
    rc = _pclose(pipe);
#else
    rc = pclose(pipe);
#endif
    (void)rc; // ignore rc; many curl commands return 0/22 etc depending on HTTP code
    return result;
}

inline int run_shell_with_status(const std::string& cmd, std::string& result) {
    result = run_shell(cmd);
    // 実際のリターンコードを取得するために、run_shell内でrcを返すように変更するか、
    // 別途コマンドを実行してステータスを得る
    std::array<char, 4096> buffer{};
#if defined(_WIN32)
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) throw std::runtime_error("popen() failed");
    while (true) {
        size_t n = fread(buffer.data(), 1, buffer.size(), pipe);
        if (n > 0) result.append(buffer.data(), n);
        if (n < buffer.size()) break;
    }
    int rc;
#if defined(_WIN32)
    rc = _pclose(pipe);
#else
    rc = pclose(pipe);
#endif
    return rc;
}

inline std::string escape_double_quotes(const std::string& s) {
    std::string out; out.reserve(s.size()+8);
    for (char c : s) out += (c=='"') ? std::string("\\\"") : std::string(1, c);
    return out;
}

inline std::filesystem::path temp_json_path() {
    namespace fs = std::filesystem;
    auto dir = fs::temp_directory_path();
    std::mt19937_64 rng{std::random_device{}()};
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto name = std::string("agens_req_") + std::to_string(now) + "_" + std::to_string(rng()) + ".json";
    return dir / name;
}

// Clean up old temporary files on startup
inline void cleanup_temp_files() {
    namespace fs = std::filesystem;
    try {
        auto temp_dir = fs::temp_directory_path();
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(temp_dir, ec)) {
            if (entry.is_regular_file(ec)) {
                const auto& path = entry.path();
                if (path.filename().string().find("agens_req_") == 0) {
                    auto ftime = fs::last_write_time(path, ec);
                    auto now = fs::file_time_type::clock::now();
                    if (!ec && (now - ftime) > std::chrono::hours(1)) {
                        fs::remove(path, ec); // Remove files older than 1 hour
                    }
                }
            }
        }
    } catch (...) {
        // Ignore cleanup errors
    }
}

inline std::string http_get(const std::string& url, const std::vector<std::string>& headers = {}) {
    // Windows でも安全に判定できるよう、curl の write-out にマーカーを付与して末尾から分離する
    const std::string kMarker = "__STATUS_CODE__:";
    std::string cmd = "curl -sS --connect-timeout 2 --max-time 10 --fail-with-body -w \"" + kMarker + "%{http_code}\"";
    for (const auto& h : headers) {
        cmd += " -H \"" + escape_double_quotes(h) + "\"";
    }
    cmd += " \"" + escape_double_quotes(url) + "\"";

    std::string result = run_shell(cmd);

    // マーカーで分離してステータスコードを取得
    size_t pos = result.rfind(kMarker);
    if (pos == std::string::npos) {
        return ""; // マーカーが見つからない＝失敗扱い
    }
    std::string body = result.substr(0, pos);
    std::string status = result.substr(pos + kMarker.size());

    // 数値化して 2xx を成功とする
    int code = 0;
    try {
        code = std::stoi(status);
    } catch (...) {
        return "";
    }
    if (200 <= code && code < 300) {
        return body;
    }
    return ""; // HTTP エラーは空を返す
}

inline std::string http_post_json(const std::string& url, const std::string& json_body, const std::vector<std::string>& headers = {}) {
    namespace fs = std::filesystem;
    auto path = temp_json_path();
    {
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs) return ""; // File creation failed
        ofs << json_body;
    }
    // Windows でも安全に判定できるよう、curl の write-out にマーカーを付与
    const std::string kMarker = "__STATUS_CODE__:";
    std::string cmd = "curl -sS --connect-timeout 2 --max-time 60 --fail-with-body -w \"" + kMarker + "%{http_code}\" -X POST";
    cmd += " -H \"Content-Type: application/json\"";
    for (const auto& h : headers) {
        cmd += " -H \"" + escape_double_quotes(h) + "\"";
    }
    cmd += " --data-binary @\"" + escape_double_quotes(path.string()) + "\" \"" + escape_double_quotes(url) + "\"";
    std::string result = run_shell(cmd);
    std::error_code ec; fs::remove(path, ec);

    size_t pos = result.rfind(kMarker);
    if (pos == std::string::npos) {
        return ""; // マーカーが見つからない＝失敗扱い
    }
    std::string body = result.substr(0, pos);
    std::string status = result.substr(pos + kMarker.size());

    int code = 0;
    try {
        code = std::stoi(status);
    } catch (...) {
        return "";
    }
    if (200 <= code && code < 300) {
        return body;
    }
    return ""; // HTTP エラーは空を返す
}

// 文字列トリミングヘルパ
inline std::string trim(const std::string& s) {
    auto notspace = [](int ch){ return !isspace(ch); };
    auto it = std::find_if(s.begin(), s.end(), notspace);
    auto rit = std::find_if(s.rbegin(), s.rend(), notspace);
    return std::string(it, rit.base());
}
inline bool json_find_first_string_value(const std::string& text, const std::string& key, std::string& out_value) {
    const std::string pat = "\"" + key + "\"";
    size_t pos = text.find(pat);
    if (pos == std::string::npos) return false;
    pos = text.find(':', pos);
    if (pos == std::string::npos) return false;
    pos = text.find('"', pos);
    if (pos == std::string::npos) return false;
    ++pos; // after opening quote
    std::string val;
    bool escape = false;
    for (; pos < text.size(); ++pos) {
        char c = text[pos];
        if (escape) { val += c; escape = false; continue; }
        if (c == '\\') { escape = true; continue; }
        if (c == '"') break;
        val += c;
    }
    out_value = val;
    return true;
}

inline std::vector<std::string> json_collect_string_values(const std::string& text, const std::string& key) {
    std::vector<std::string> out;
    const std::string pat = "\"" + key + "\"";
    size_t pos = 0;
    while (true) {
        pos = text.find(pat, pos);
        if (pos == std::string::npos) break;
        size_t colon = text.find(':', pos);
        if (colon == std::string::npos) break;
        size_t quote = text.find('"', colon);
        if (quote == std::string::npos) break;
        ++quote;
        std::string val;
        bool escape = false;
        size_t i = quote;
        for (; i < text.size(); ++i) {
            char c = text[i];
            if (escape) { val += c; escape = false; continue; }
            if (c == '\\') { escape = true; continue; }
            if (c == '"') break;
            val += c;
        }
        if (!val.empty()) out.push_back(val);
        pos = i + 1;
    }
    return out;
}

} // namespace utils

// default_ports 実装（utils関数の後に定義）
#include "ports.hpp"
namespace default_ports {
inline std::string Shell::run(const std::string& cmd) { return utils::run_shell(cmd); }
inline std::string Http::get(const std::string& url, const std::vector<std::string>& headers) { return utils::http_get(url, headers); }
inline std::string Http::post_json(const std::string& url, const std::string& json, const std::vector<std::string>& headers) { return utils::http_post_json(url, json, headers); }

// デフォルト引数を提供するオーバーロード（非仮想メソッド）
struct HttpConvenience : Http {
    std::string get(const std::string& url) { return Http::get(url, {}); }
    std::string post_json(const std::string& url, const std::string& json) { return Http::post_json(url, json, {}); }
};
}
