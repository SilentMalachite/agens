#include "agent_mode.hpp"
#include <regex>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

static string lower(string s){ transform(s.begin(), s.end(), s.begin(), ::tolower); return s; }

vector<AgentDoc> find_agent_docs(const fs::path& root) {
    vector<AgentDoc> docs;
    vector<string> names = {"AGENTS.md","AGENT.md","agents.md","agent.md"};
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied);
         it != fs::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file()) continue;
        auto name = it->path().filename().string();
        string lname = lower(name);
        bool match = false; for (auto& n : names) if (lname==lower(n)) { match=true; break; }
        if (!match) continue;
        ifstream ifs(it->path(), ios::binary);
        if (!ifs) continue;
        ostringstream oss; oss << ifs.rdbuf();
        docs.push_back(AgentDoc{it->path(), oss.str()});
    }
    // 近いパスを優先：浅い階層順
    sort(docs.begin(), docs.end(), [](const AgentDoc& a, const AgentDoc& b){ return a.path.string().size() < b.path.string().size(); });
    return docs;
}

vector<string> extract_tasks(const string& md) {
    vector<string> out;
    istringstream iss(md); string line; bool in_code=false;
    while (getline(iss, line)) {
        if (line.rfind("```",0)==0) { in_code = !in_code; continue; }
        if (in_code) continue;
        string s = line;
        // bullets or numbered list in JP/EN
        auto l = s; l.erase(remove_if(l.begin(), l.end(), [](unsigned char c){return c=='\r';}), l.end());
        auto trim = [](string x){
            auto ns=[](int ch){return !isspace(ch);} ;
            x.erase(x.begin(), find_if(x.begin(), x.end(), ns));
            x.erase(find_if(x.rbegin(), x.rend(), ns).base(), x.end()); return x; };
        s = trim(s);
        if (s.rfind("- ",0)==0 || s.rfind("* ",0)==0) { out.push_back(s.substr(2)); continue; }
        // 1. task, １．タスク, 1) task
        if (regex_match(s, regex(R"(^[0-9]+[\.|\)]\s+.+$)"))) { out.push_back(regex_replace(s, regex(R"(^[0-9]+[\.|\)]\s+)"), "")); continue; }
    }
    return out;
}

string build_auto_system_prompt(const vector<AgentDoc>& docs) {
    string prompt =
        "あなたはローカルで動作する自律エージェントです。以下の設計書(AGENT(S).md)に従い、必要なファイルを作成/更新してください。"\
        "出力は必ず日本語。変更はファイルブロックで明示してください。フォーマットは次のいずれか: \n"\
        "```file: 相対/パス\n<内容>\n```\n"\
        "または \n"\
        "```agens:file=相対/パス\n<内容>\n```\n"\
        "既存ファイルを置換する場合も同様に出力してください。工程の説明は簡潔に。\n";
    // 添付: 要約と本文（長すぎる場合は先頭のみ）
    for (const auto& d : docs) {
        prompt += "\n--- 設計書: "; prompt += d.path.string(); prompt += "\n";
        // 先頭 ~4000文字まで
        string body = d.content.substr(0, 4000);
        prompt += body;
        if (d.content.size() > body.size()) prompt += "\n(続きはローカルに存在)";
    }
    return prompt;
}

ApplyResult apply_file_blocks(const string& output, bool dry_run) {
    ApplyResult ar; ostringstream log;
    // 検出: ```file: path 〜 ``` または ```agens:file=path 〜 ```
    // 簡易パーサ（貪欲すぎない）
    size_t pos = 0; size_t n = output.size();
    while (pos < n) {
        size_t fence = output.find("```", pos);
        if (fence == string::npos) break;
        size_t eol = output.find('\n', fence);
        if (eol == string::npos) break;
        string head = output.substr(fence+3, eol-(fence+3));
        head.erase(remove(head.begin(), head.end(), '\r'), head.end());
        string path;
        auto h = head;
        if (h.rfind("file:",0)==0) {
            path = string(h.begin()+5, h.end());
        } else if (h.rfind("agens:file=",0)==0) {
            path = string(h.begin()+11, h.end());
        } else { pos = eol+1; continue; }
        // 本文の終端 ``` を探す
        size_t fence2 = output.find("```", eol+1);
        if (fence2 == string::npos) break;
        string body = output.substr(eol+1, fence2-(eol+1));
        // 書き込み
        fs::path p = fs::path(path);
        if (!dry_run) {
            std::error_code ec;
            fs::create_directories(p.parent_path(), ec);
            if (ec) {
                log << "[error] Failed to create directory: " << p.parent_path().string() << "\n";
                pos = fence2 + 3;
                continue;
            }
            ofstream ofs(p, ios::binary); 
            if (!ofs) {
                log << "[error] Failed to open file for writing: " << p.string() << "\n";
                pos = fence2 + 3;
                continue;
            }
            ofs << body; 
            ofs.close();
            if (ofs.fail()) {
                log << "[error] Failed to write file: " << p.string() << "\n";
                pos = fence2 + 3;
                continue;
            }
        }
        ar.written.push_back(p);
        log << (dry_run?"[plan] ":"[write] ") << p.string() << " (" << body.size() << " bytes)\n";
        pos = fence2 + 3;
    }
    ar.log = log.str();
    return ar;
}
