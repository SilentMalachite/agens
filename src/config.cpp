#include "config.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

static string json_escape(const string& s) {
    string out; out.reserve(s.size()+8);
    for (char c : s) {
        switch (c) { case '"': out+="\\\""; break; case '\\': out+="\\\\"; break; case '\n': out+="\\n"; break; default: out+=c; }
    }
    return out;
}

static string to_json(const AppConfig& c) {
    ostringstream o;
    o << "{\n";
    auto write_arr = [&](const char* key, const vector<string>& v){
        o << "  \""<<key<<"\":[";
        for (size_t i=0;i<v.size();++i) { if (i) o << ","; o << "\""<<json_escape(v[i])<<"\""; }
        o << "],\n";
    };
    write_arr("allow_patterns", c.allow_patterns);
    write_arr("deny_patterns",  c.deny_patterns);
    o << "  \"auto_confirm\": " << (c.auto_confirm?"true":"false") << ",\n";
    o << "  \"auto_dry_run\": " << (c.auto_dry_run?"true":"false") << ",\n";
    o << "  \"last_backend\": \"" << json_escape(c.last_backend) << "\",\n";
    o << "  \"last_model\": \""   << json_escape(c.last_model)   << "\",\n";
    o << "  \"last_cwd\": \""     << json_escape(c.last_cwd)     << "\",\n";
    o << "  \"unified_gpu_ratio\": " << (c.unified_gpu_ratio)       << ",\n";
    o << "  \"language\": \""     << json_escape(c.language)     << "\"\n";
    o << "}\n";
    return o.str();
}

static bool parse_bool(const string& text, const string& key, bool& out) {
    auto pos = text.find("\""+key+"\""); if (pos==string::npos) return false;
    pos = text.find(':', pos); if (pos==string::npos) return false;
    auto tail = text.substr(pos+1);
    if (tail.find("true")!=string::npos) { out = true; return true; }
    if (tail.find("false")!=string::npos){ out = false; return true; }
    return false;
}

static vector<string> parse_string_array(const string& text, const string& key) {
    vector<string> out;
    auto pos = text.find("\""+key+"\""); if (pos==string::npos) return out;
    pos = text.find('[', pos); if (pos==string::npos) return out;
    size_t i = pos+1; bool in_str=false, esc=false; string cur;
    for (; i<text.size(); ++i) {
        char c = text[i];
        if (!in_str) {
            if (c==']') break;
            if (c=='"') { in_str=true; cur.clear(); }
            continue;
        } else {
            if (esc) { cur += c; esc=false; continue; }
            if (c=='\\') { esc=true; continue; }
            if (c=='"') { out.push_back(cur); in_str=false; continue; }
            cur += c;
        }
    }
    return out;
}

static bool parse_number(const string& text, const string& key, double& out) {
    auto pos = text.find("\""+key+"\""); if (pos==string::npos) return false;
    pos = text.find(':', pos); if (pos==string::npos) return false;
    size_t i = pos+1;
    while (i<text.size() && isspace(static_cast<unsigned char>(text[i]))) ++i;
    size_t j = i; bool any=false;
    while (j<text.size()) {
        char c = text[j];
        if ((c>='0'&&c<='9') || c=='-' || c=='+' || c=='.' || c=='e' || c=='E') { any=true; ++j; continue; }
        break;
    }
    if (!any) return false;
    try { out = stod(text.substr(i, j-i)); return true; } catch (...) { return false; }
}

static string parse_string(const string& text, const string& key) {
    auto pos = text.find("\""+key+"\""); if (pos==string::npos) return string();
    pos = text.find(':', pos); if (pos==string::npos) return string();
    pos = text.find('"', pos); if (pos==string::npos) return string();
    ++pos; string out; bool esc=false; for (; pos<text.size(); ++pos) { char c=text[pos]; if (esc){ out+=c; esc=false; continue;} if (c=='\\'){esc=true; continue;} if (c=='"') break; out+=c; }
    return out;
}

fs::path default_config_path() {
#if defined(_WIN32)
    const char* appdata = getenv("APPDATA");
    fs::path base = appdata ? fs::path(appdata) : fs::path(".");
    return base / "agens" / "config.json";
#else
    const char* xdg = getenv("XDG_CONFIG_HOME");
    const char* home = getenv("HOME");
    fs::path base;
    if (xdg) {
        base = fs::path(xdg);
    } else if (home) {
        base = fs::path(home) / ".config";
    } else {
        base = fs::path("."); // Fallback to current directory
    }
    return base / "agens" / "config.json";
#endif
}

bool load_config(AppConfig& cfg, string* err) {
    auto path = default_config_path();
    std::error_code ec; fs::create_directories(path.parent_path(), ec);
    ifstream ifs(path, ios::binary);
    if (!ifs.good()) {
        // 初回は空の設定を保存
        return save_config(cfg, err);
    }
    ostringstream oss; oss << ifs.rdbuf(); string body = oss.str();
    cfg.allow_patterns = parse_string_array(body, "allow_patterns");
    cfg.deny_patterns  = parse_string_array(body, "deny_patterns");
    bool b;
    if (parse_bool(body, "auto_confirm", b)) cfg.auto_confirm = b;
    if (parse_bool(body, "auto_dry_run", b)) cfg.auto_dry_run = b;
    cfg.last_backend = parse_string(body, "last_backend");
    cfg.last_model   = parse_string(body, "last_model");
    cfg.last_cwd     = parse_string(body, "last_cwd");
    double d;
    if (parse_number(body, "unified_gpu_ratio", d)) cfg.unified_gpu_ratio = d;
    cfg.language = parse_string(body, "language");
    return true;
}

bool save_config(const AppConfig& cfg, string* err) {
    auto path = default_config_path();
    std::error_code ec; fs::create_directories(path.parent_path(), ec);
    ofstream ofs(path, ios::binary);
    if (!ofs) { if (err) *err = string("failed to write ")+path.string(); return false; }
    ofs << to_json(cfg);
    return true;
}
