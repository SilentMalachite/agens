#include "utils.hpp"
#include <filesystem>
#include <stdexcept>
#include <array>
#include <fstream>
#include <random>
#include <chrono>
#include <algorithm>
#include <cctype> // for isspace

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/wait.h>
#endif

namespace utils {

std::string shell_escape_single_quotes(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    out += '\'';
    for (char c : s) {
        if (c == '\'') {
            out += "'\"'\"'"; // close ', add '"'"', reopen '
        } else {
            out += c;
        }
    }
    out += '\'';
    return out;
}

int run_shell_with_status(const std::string& cmd, std::string& result) {
    result.clear();
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

std::string run_shell(const std::string& cmd) {
    std::string result;
    run_shell_with_status(cmd, result);
    return result;
}

std::string escape_double_quotes(const std::string& s) {
    std::string out; out.reserve(s.size()+8);
    for (char c : s) out += (c=='"') ? std::string("\\\"") : std::string(1, c);
    return out;
}


std::optional<std::string> http_get(const std::string& url, const std::vector<std::string>& headers) {
    const std::string kMarker = "__STATUS_CODE__:";
    std::string cmd = "curl -sS --connect-timeout 2 --max-time 10 --fail-with-body -w \"" + kMarker + "%{http_code}\" -H \"Content-Type: application/json\"";
    for (const auto& h : headers) {
        cmd += " -H \"" + escape_double_quotes(h) + "\"";
    }
    cmd += " \"" + escape_double_quotes(url) + "\"";
    // curlのエラーメッセージを標準エラーへ出すが、コンソール汚染を避けるため捨てる
#if defined(_WIN32)
    cmd += " 2>NUL";
#else
    cmd += " 2>/dev/null";
#endif
    std::string result = run_shell(cmd);
    size_t pos = result.rfind(kMarker);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    std::string body = result.substr(0, pos);
    std::string status = result.substr(pos + kMarker.size());
    int code = 0;
    try {
        code = std::stoi(status);
    } catch (...) {
        return std::nullopt;
    }
    if (200 <= code && code < 300) {
        return body;
    }
    return std::nullopt;
}

std::optional<std::string> http_post_json(const std::string& url, const std::string& json_body, const std::vector<std::string>& headers) {
    auto path_str = temp_json_path();
    {
        std::ofstream ofs(path_str, std::ios::binary);
        if (!ofs) return std::nullopt;
        ofs << json_body;
    }
    const std::string kMarker = "__STATUS_CODE__:";
    std::string cmd = "curl -sS --connect-timeout 2 --max-time 60 --fail-with-body -w \"" + kMarker + "%{http_code}\" -X POST";
    cmd += " -H \"Content-Type: application/json\"";
    for (const auto& h : headers) {
        cmd += " -H \"" + escape_double_quotes(h) + "\"";
    }
    cmd += " --data-binary @\"" + escape_double_quotes(path_str) + "\" \"" + escape_double_quotes(url) + "\"";
    // エラーメッセージは捨てる（ステータスは -w で取得）
#if defined(_WIN32)
    cmd += " 2>NUL";
#else
    cmd += " 2>/dev/null";
#endif
    std::string result = run_shell(cmd);
    std::error_code ec; std::filesystem::remove(path_str, ec);
    size_t pos = result.rfind(kMarker);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    std::string body = result.substr(0, pos);
    std::string status = result.substr(pos + kMarker.size());
    int code = 0;
    try {
        code = std::stoi(status);
    } catch (...) {
        return std::nullopt;
    }
    if (200 <= code && code < 300) {
        return body;
    }
    return std::nullopt;
}

std::string trim(const std::string& s) {
    auto notspace = [](int ch){ return !std::isspace(static_cast<unsigned char>(ch)); };
    auto it = std::find_if(s.begin(), s.end(), notspace);
    auto rit = std::find_if(s.rbegin(), s.rend(), notspace);
    return std::string(it, rit.base());
}

bool json_find_first_string_value(const std::string& text, const std::string& key, std::string& out_value) {
    const std::string pat = "\"" + key + "\"";
    size_t pos = text.find(pat);
    if (pos == std::string::npos) return false;
    pos = text.find(':', pos);
    if (pos == std::string::npos) return false;
    pos = text.find('"', pos);
    if (pos == std::string::npos) return false;
    ++pos;
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

std::vector<std::string> json_collect_string_values(const std::string& text, const std::string& key) {
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

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string temp_json_path() {
    auto tp = std::chrono::high_resolution_clock::now().time_since_epoch();
    uint64_t d = std::chrono::duration_cast<std::chrono::microseconds>(tp).count();
    std::mt19937_64 rng(d);
    std::uniform_int_distribution<uint64_t> dist;
    std::filesystem::path path = std::filesystem::temp_directory_path();
    path /= "agens-" + std::to_string(dist(rng)) + ".json";
    return path.string();
}

} // namespace utils
