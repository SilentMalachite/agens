#include "file_finder.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

using namespace std;
namespace fs = std::filesystem;

static string lower(string s){ transform(s.begin(), s.end(), s.begin(), ::tolower); return s; }

static bool is_binary_prefix(const string& data) {
    int nontext = 0; int total = 0;
    for (unsigned char c : data) { ++total; if ((c<9) || (c>13 && c<32)) ++nontext; if (total>1024) break; }
    return nontext > total/16; // heuristic
}

static vector<string> split_words(const string& q) {
    vector<string> t; string cur;
    for (char c : q) { if (isspace((unsigned char)c)) { if(!cur.empty()){t.push_back(lower(cur)); cur.clear();} } else cur+=c; }
    if (!cur.empty()) t.push_back(lower(cur));
    return t;
}

vector<FileHit> find_relevant_files(const string& base_dir, const string& query, int max_results) {
    vector<FileHit> hits;
    auto tokens = split_words(query);
    if (tokens.empty()) return hits;

    vector<string> ignore_dirs = {".git","build","dist","node_modules",".venv","venv","target","bin","obj",".next",".cache"};

    for (auto it = fs::recursive_directory_iterator(base_dir, fs::directory_options::skip_permission_denied);
         it != fs::recursive_directory_iterator(); ++it) {
        if (it->is_directory()) {
            string name = it->path().filename().string();
            string lname = lower(name);
            if (find_if(ignore_dirs.begin(), ignore_dirs.end(), [&](const string& d){return lname==d;}) != ignore_dirs.end()) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (!it->is_regular_file()) continue;
        string path = it->path().string();
        string lpath = lower(path);
        int score = 0;
        for (auto& tk : tokens) if (lpath.find(tk)!=string::npos) score += 5;
        // read small prefix to determine binary and collect snippets
        ifstream ifs(path, ios::binary);
        if (!ifs) continue;
        string content_prefix; content_prefix.resize(64*1024);
        ifs.read(content_prefix.data(), content_prefix.size()); size_t rd = ifs.gcount(); content_prefix.resize(rd);
        if (is_binary_prefix(content_prefix)) continue;
        string all = content_prefix;
        // if small file, read all
        if (!ifs.eof()) {
            // leave as prefix only to limit cost
        }
        // simple line scanning
        istringstream iss(all); string line; int added=0; string snip;
        for (int ln=1; ln<=300 && std::getline(iss, line); ++ln) {
            string l = lower(line);
            bool matched=false; for (auto& tk: tokens) if (l.find(tk)!=string::npos) { matched=true; score += 3; }
            if (matched && added<3) { snip += line + "\n"; ++added; }
        }
        if (score>0) hits.push_back(FileHit{path, score, snip});
    }
    sort(hits.begin(), hits.end(), [](const FileHit& a, const FileHit& b){ return a.score>b.score; });
    if ((int)hits.size()>max_results) hits.resize(max_results);
    return hits;
}

