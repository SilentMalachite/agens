#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cctype>

#include "utils.hpp"
#include "system_info.hpp"
#include "chat.hpp"
#include "backend.hpp"
#include "web_search.hpp"
#include "file_finder.hpp"
#include "agent_mode.hpp"
#include "config.hpp"

using namespace std;


// ビルダー類は chat.cpp へ分離


struct Messages {
    std::string lang = "ja";
    std::string usage() const { return lang=="en" ? "Usage: agens [-b backend] [-m model] [-p prompt]" : "使い方: agens [-b backend] [-m model] [-p prompt]"; }
    std::string backend_hint() const { return lang=="en" ? "  backend: ollama|lmstudio" : "  backend: ollama|lmstudio"; }
    std::string starting() const { return lang=="en" ? "Starting local LLM agent. Replies in Japanese." : "ローカルLLMエージェントを起動します。常に日本語で応答します。"; }
    std::string api_missing1() const { return lang=="en" ? "No local API found for Ollama(11434) or LM Studio(1234)." : "Ollama(11434)またはLM Studio(1234)のローカルAPIが見つかりません。"; }
    std::string api_missing2() const { return lang=="en" ? "Please start Ollama or LM Studio server and try again." : "OllamaやLM Studioのサーバーを起動してから再度お試しください。"; }
    std::string label_reco() const { return lang=="en" ? "[Recommended]" : "[推奨パラメータ]"; }
    std::string label_sys()  const { return lang=="en" ? "[System]"      : "[システム検出]"; }
    std::string label_ram_about() const { return lang=="en" ? ", RAM~" : ", RAM約"; }
    std::string label_vram_unknown() const { return lang=="en" ? ", VRAM unknown" : ", VRAM不明"; }
    std::string label_integrated_prefix() const { return lang=="en" ? ", Unified memory (est. GPU ~" : ", 統合メモリ(推定GPU利用~"; }
};

static void print_tuning(const InferenceTuning& t, const Messages& m) {
    cout << m.label_reco() << " context=" << t.context
         << ", max_tokens=" << t.max_tokens
         << ", temperature=" << t.temperature
         << ", top_p=" << t.top_p
         << ", gpu_layers=" << t.gpu_layers
         << "\n";
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    // コマンドライン
    Messages msg;
    // 早期に環境変数でヘルプ言語を切り替え
    if (const char* envlang0 = getenv("AGENS_LANG")) { string v=envlang0; transform(v.begin(), v.end(), v.begin(), ::tolower); if (v=="en"||v=="ja") msg.lang=v; }
    string prefer_backend; // "ollama" or "lmstudio"
    string prefer_model;
    string one_prompt;
    for (int i=1;i<argc;++i) {
        string a = argv[i];
        if ((a=="-b"||a=="--backend") && i+1<argc) { prefer_backend = argv[++i]; }
        else if ((a=="-m"||a=="--model") && i+1<argc) { prefer_model = argv[++i]; }
        else if ((a=="-p"||a=="--prompt") && i+1<argc) { one_prompt = argv[++i]; }
        else if (a=="-h"||a=="--help") {
            cout << msg.usage() << "\n";
            cout << msg.backend_hint() << "\n";
            return 0;
        }
    }

    // 設定ロード
    AppConfig config;
    load_config(config);
    // 言語（設定→環境変数で上書き）
    if (!config.language.empty()) msg.lang = config.language;
    if (const char* envlang = getenv("AGENS_LANG")) { string v=envlang; transform(v.begin(), v.end(), v.begin(), ::tolower); if (v=="en"||v=="ja") msg.lang=v; }

    cout << msg.starting() << "\n";
    cout.flush();
    if (!config.last_cwd.empty()) {
        std::error_code ec; std::filesystem::current_path(config.last_cwd, ec);
    }

    // システム検出
    auto si = detect_system_info();
    auto tune = decide_tuning(si);
    cout << msg.label_sys() << ' ';
    if (si.is_macos) cout << "macOS"; else if (si.is_linux) cout << "Linux"; else if (si.is_windows) cout << "Windows"; else cout << "Unknown";
    cout << msg.label_ram_about() << (si.ram_bytes/(1024ull*1024ull*1024ull)) << "GB";
    {
        double unified_ratio = config.unified_gpu_ratio;
        if (const char* env = getenv("AGENS_UNIFIED_GPU_RATIO")) { try { unified_ratio = stod(env); } catch (...) {} }
        if (!(unified_ratio>0.0 && unified_ratio<1.0)) unified_ratio = 0.5;
        if (si.vram_mb>0) {
            cout << ", VRAM約" << (si.vram_mb/1024) << "GB";
        } else if (si.is_macos && si.is_apple_silicon) {
            uint64_t ram_gb = si.ram_bytes/(1024ull*1024ull*1024ull);
            uint64_t est_gb = (ram_gb>=1) ? static_cast<uint64_t>(ram_gb*unified_ratio) : 1;
            if (est_gb==0) est_gb = 1;
            cout << msg.label_integrated_prefix() << est_gb << "GB)";
        } else {
            cout << msg.label_vram_unknown();
        }
    }
    if (si.is_apple_silicon) cout << ", Apple Silicon";
    if (!si.gpu_name.empty()) cout << ", GPU: " << si.gpu_name;
    cout << "\n";
    print_tuning(tune, msg);
    cout.flush();

    // バックエンド検出
    default_ports::Http http;
    bool has_ollama = backend::ollama::probe(http);
    bool has_lms = backend::lmstudio::probe(http);
    vector<string> backends;
    if (has_ollama) backends.push_back("ollama");
    if (has_lms) backends.push_back("lmstudio");
    if (backends.empty()) {
        cout << msg.api_missing1() << "\n";
        cout << msg.api_missing2() << "\n";
        cout.flush();
        return 1;
    }

    string backend;
    if (!prefer_backend.empty()) backend = prefer_backend;
    else if (!config.last_backend.empty()) backend = config.last_backend;
    else if (backends.size()==1) backend = backends[0];
    else {
        cout << "利用するバックエンドを選択してください: \n";
        for (size_t i=0;i<backends.size();++i) cout << "  ["<< (i+1) << "] " << backends[i] << "\n";
        cout << "> 番号: ";
        string s; getline(cin, s); 
        size_t idx = 1;
        if (!s.empty()) {
            try {
                idx = stoul(s);
            } catch (const exception&) {
                cout << "[警告] 無効な入力です。1番を選択します。\n";
                idx = 1;
            }
        }
        if (idx<1 || idx>backends.size()) idx = 1;
        backend = backends[idx-1];
    }
    cout << "選択: " << backend << "\n";
    config.last_backend = backend; save_config(config);

    // モデル一覧
    vector<string> models;
    if (backend=="ollama") models = backend::ollama::list_models(http);
    else models = backend::lmstudio::list_models(http);

    string model;
    if (!prefer_model.empty()) {
        model = prefer_model;
    } else if (!config.last_model.empty()) {
        model = config.last_model;
    } else if (!models.empty()) {
        cout << "利用可能なモデル:\n";
        for (size_t i=0;i<models.size();++i) cout << "  ["<<(i+1)<<"] "<<models[i]<<"\n";
        cout << "> モデル番号を選択（空Enterで1番）: ";
        string s; getline(cin, s); 
        size_t idx = 1;
        if (!s.empty()) {
            try {
                idx = stoul(s);
            } catch (const exception&) {
                cout << "[警告] 無効な入力です。1番を選択します。\n";
                idx = 1;
            }
        }
        if (idx<1 || idx>models.size()) idx = 1;
        model = models[idx-1];
    } else {
        cout << "モデル一覧を取得できませんでした。手入力してください。\n> モデル名: ";
        getline(cin, model);
    }
    if (model.empty()) {
        cerr << "モデル名が空です。終了します。\n"; return 1;
    }
    cout << "モデル: " << model << "\n";
    // 設定反映
    bool auto_mode = false;
    bool auto_confirm = config.auto_confirm;
    bool auto_dry_run = config.auto_dry_run;
    std::vector<std::string> allow_patterns = config.allow_patterns;
    std::vector<std::string> deny_patterns  = config.deny_patterns;

    // システムプロンプト（常に日本語で応答）
    string system_jp = "あなたは有能なローカルAIアシスタントです。常に日本語で、簡潔かつ丁寧に回答してください。";

    // 単発プロンプト or REPL
    auto do_chat_once = [&](const string& user)->optional<string>{
        vector<ChatMsg> msgs = {{"system", system_jp}, {"user", user}};
        if (backend=="ollama") return backend::ollama::chat(http, model, msgs, tune);
        return backend::lmstudio::chat(http, model, msgs, tune);
    };

    if (!one_prompt.empty()) {
        auto ans = do_chat_once(one_prompt);
        if (!ans) { cerr << "推論に失敗しました。\n"; return 2; }
        cout << *ans << "\n";
        return 0;
    }

    cout << "対話を開始します。/exit または /quit で終了。/auto 設計書に基づく自律実行。/model 変更。/web 検索。/target ファイル。/cd で作業ディレクトリ変更。/allow・/deny で許可/拒否。/sh・/prog 実行。/temp 等。\n";
    // ↑ 既に設定から初期化済み
    vector<AgentDoc> agent_docs;
    vector<string> agent_tasks;
    while (true) {
        cout << "あなた> ";
        string user; if (!getline(cin, user)) { cin.clear(); continue; }
        // 空行は無視
        user = utils::trim(user);
        if (user.empty()) continue;
        if (user=="/exit" || user=="/quit") break;
        if (user.rfind("/model",0)==0) {
            string arg = utils::trim(user.substr(6));
            if (arg.empty() || arg=="?" || arg=="list") {
                vector<string> models2 = (backend=="ollama") ? backend::ollama::list_models(http) : backend::lmstudio::list_models(http);
                if (models2.empty()) { cout << "[警告] モデル一覧を取得できませんでした。/model <名前> で直接指定してください。\n"; continue; }
                cout << "利用可能なモデル:\n";
                for (size_t i=0;i<models2.size();++i) cout << "  ["<<(i+1)<<"] "<<models2[i]<<"\n";
                cout << "> 番号またはモデル名: ";
                string sel; if (!getline(cin, sel)) { cin.clear(); continue; }
                sel = utils::trim(sel);
                if (sel.empty()) { cout << "[取消] 変更なし。\n"; continue; }
                // 数字ならインデックス
                bool all_digit = !sel.empty() && all_of(sel.begin(), sel.end(), [](unsigned char c){ return std::isdigit(c); });
                string chosen = sel;
                if (all_digit) {
                    try {
                        size_t idx = stoul(sel); 
                        if (idx<1 || idx>models2.size()) idx = 1; 
                        chosen = models2[idx-1];
                    } catch (const exception&) {
                        cout << "[警告] 無効な番号です。そのまま名前として使用します。\n";
                        chosen = sel;
                    }
                }
                model = chosen; cout << "モデルを変更しました: "<< model <<"\n"; config.last_model = model; save_config(config);
            } else {
                model = arg; cout << "モデルを変更しました: "<< model <<"\n"; config.last_model = model; save_config(config);
            }
            continue;
        }
        if (user.rfind("/config",0)==0) {
            string sub = utils::trim(user.substr(7));
            if (sub=="save") { save_config(config); cout<<"[設定] 保存しました: "<< default_config_path().string() <<"\n"; continue; }
            if (sub=="reload") { AppConfig tmp; if (load_config(tmp)) { config = tmp; cout<<"[設定] リロードしました\n"; } else cout<<"[設定] リロード失敗\n"; continue; }
            if (sub=="path") { cout << default_config_path().string() << "\n"; continue; }
            cout << "使い方: /config path|save|reload\n"; continue;
        }
        if (user.rfind("/cd",0)==0) {
            auto path = utils::trim(user.substr(3));
            if (path.empty()) { cout << std::filesystem::current_path().string() << "\n"; continue; }
            // ~ 展開
            if (path[0]=='~') {
                const char* home = getenv(
#ifdef _WIN32
                "USERPROFILE"
#else
                "HOME"
#endif
                );
                if (home) path = std::string(home) + path.substr(1);
            }
            std::error_code ec;
            std::filesystem::path p(path);
            if (std::filesystem::is_directory(p, ec)) {
                std::filesystem::current_path(p, ec);
                if (ec) { cout << "[エラー] ディレクトリ変更に失敗: " << ec.message() << "\n"; }
                else { cout << "cwd: " << std::filesystem::current_path().string() << "\n"; config.last_cwd = std::filesystem::current_path().string(); save_config(config); }
            } else {
                cout << "[エラー] ディレクトリが見つかりません: " << path << "\n";
            }
            continue;
        }
        if (user.rfind("/allow",0)==0 || user.rfind("/deny",0)==0) {
            bool is_allow = (user[1]=='a');
            string sub = utils::trim(user.substr(is_allow?6:5));
            auto parts = [&](){ vector<string> v; string cur; istringstream iss(sub); while(iss>>cur) v.push_back(cur); return v; }();
            auto& lst = is_allow ? allow_patterns : deny_patterns;
            if (parts.empty() || parts[0]=="list") {
                cout << (is_allow?"[許可]":"[拒否]") << " パターン:\n";
                if (lst.empty()) cout << "  (空)\n"; else for (auto& s: lst) cout << "  - "<<s<<"\n";
                continue;
            }
            if (parts[0]=="add" && parts.size()>=2) {
                string pat = sub.substr(sub.find("add")+3); pat = utils::trim(pat);
                lst.push_back(pat); cout << (is_allow?"[許可追加] ":"[拒否追加] ") << pat << "\n"; if (is_allow) config.allow_patterns=allow_patterns; else config.deny_patterns=deny_patterns; save_config(config); continue;
            }
            if ((parts[0]=="rm"||parts[0]=="remove") && parts.size()>=2) {
                string pat = sub.substr(sub.find(parts[0])+parts[0].size()); pat = utils::trim(pat);
                auto it = std::remove(lst.begin(), lst.end(), pat); bool removed = (it!=lst.end()); lst.erase(it, lst.end());
                cout << ((removed)?"[削除] ":"[未一致]") << pat << "\n"; if (is_allow) config.allow_patterns=allow_patterns; else config.deny_patterns=deny_patterns; save_config(config); continue;
            }
            if (parts[0]=="clear") { lst.clear(); cout << "[クリア] 空になりました\n"; if (is_allow) config.allow_patterns.clear(); else config.deny_patterns.clear(); save_config(config); continue; }
            cout << "使い方: /" << (is_allow?"allow":"deny") << " list|add <pat>|rm <pat>|clear\n"; continue;
        }
        if (user.rfind("/agents",0)==0) {
            if (agent_docs.empty()) agent_docs = find_agent_docs(".");
            if (agent_docs.empty()) { cout << "AGENT(S).md が見つかりません。プロジェクト直下に配置してください。\n"; continue; }
            cout << "検出した設計書:\n";
            for (size_t i=0;i<agent_docs.size();++i) cout << "  ["<<(i+1)<<"] "<<agent_docs[i].path.string()<<"\n";
            continue;
        }
        if (user.rfind("/plan",0)==0) {
            if (agent_docs.empty()) agent_docs = find_agent_docs(".");
            agent_tasks.clear();
            for (auto& d : agent_docs) {
                auto t = extract_tasks(d.content);
                agent_tasks.insert(agent_tasks.end(), t.begin(), t.end());
            }
            if (agent_tasks.empty()) { cout << "設計書から明示的なタスクは抽出できませんでした。\n"; continue; }
            cout << "計画（抽出タスク）:\n";
            for (size_t i=0;i<agent_tasks.size();++i) cout << "  - "<<agent_tasks[i]<<"\n";
            continue;
        }
        if (user.rfind("/auto",0)==0) {
            string arg = user.size()>5 ? user.substr(5) : string();
            arg = utils::trim(arg);
            if (arg=="off"||arg=="stop") {
                auto_mode=false; cout<<"自動モード: OFF\n";
                system_jp = "あなたは有能なローカルAIアシスタントです。常に日本語で、簡潔かつ丁寧に回答してください。";
                continue;
            }
            if (arg.rfind("confirm",0)==0) {
                string rest = utils::trim(arg.substr(7));
                if (rest=="off"||rest=="disable") { auto_confirm=false; cout<<"/auto confirm: OFF\n"; }
                else { auto_confirm=true; cout<<"/auto confirm: ON（各出力の適用前に確認します）\n"; }
                config.auto_confirm = auto_confirm; save_config(config);
                if (!auto_mode) cout<<"（ヒント）/auto で自動モードをONにしてください。\n";
                continue;
            }
            if (arg.rfind("dry",0)==0) {
                string rest = utils::trim(arg.substr(3));
                if (rest=="off"||rest=="disable") { auto_dry_run=false; cout<<"/auto dry: OFF\n"; }
                else { auto_dry_run=true; cout<<"/auto dry: ON（ファイルは書き込みません）\n"; }
                config.auto_dry_run = auto_dry_run; save_config(config);
                if (!auto_mode) cout<<"（ヒント）/auto で自動モードをONにしてください。\n";
                continue;
            }
            if (arg=="status") {
                cout << "自動モード: " << (auto_mode?"ON":"OFF")
                     << ", confirm=" << (auto_confirm?"ON":"OFF")
                     << ", dry=" << (auto_dry_run?"ON":"OFF") << "\n";
                continue;
            }
            // /auto または /auto on
            if (agent_docs.empty()) agent_docs = find_agent_docs(".");
            if (agent_docs.empty()) { cout << "AGENT(S).md を見つけられません。/agents で確認してください。\n"; continue; }
            system_jp = build_auto_system_prompt(agent_docs);
            auto_mode = true;
            cout << "自動モード: ON（" << (auto_dry_run?"dry":"apply") << ", confirm=" << (auto_confirm?"ON":"OFF") << ")\n";
            continue;
        }
        auto matches_any = [](const string& s, const vector<string>& pats){ for (auto& p: pats) if (!p.empty() && s.find(p)!=string::npos) return true; return false; };
        auto is_blocked = [&](const string& cmd){ if (matches_any(cmd, deny_patterns)) return string("deny"); if (!allow_patterns.empty() && !matches_any(cmd, allow_patterns)) return string("not-allowed"); return string(); };
        if (user.rfind("/sh!",0)==0 || user.rfind("/prog!",0)==0) {
            string cmd = utils::trim(user.substr(user[1]=='s'?4:6));
            if (cmd.empty()) { cout << "使い方: /sh! <コマンド> または /prog! <プログラム> [引数]" << "\n"; continue; }
            auto why = is_blocked(cmd);
            if (!why.empty()) { cout << (why=="deny"?"[拒否] 拒否リストに一致: ":"[未許可] 許可リストに未一致: ") << cmd << "\n"; cout << "必要なら /allow add <パターン> を追加してください。\n"; continue; }
            string out; int rc = utils::run_shell_with_status(cmd + " 2>&1", out);
            cout << out;
            cout << "[exit=" << rc << "]\n";
            continue;
        }
        if (user.rfind("/sh",0)==0 || user.rfind("/prog",0)==0) {
            string cmd = utils::trim(user.substr(user[1]=='s'?3:5));
            if (cmd.empty()) { cout << "使い方: /sh <コマンド> または /prog <プログラム> [引数]" << "\n"; continue; }
            auto why = is_blocked(cmd);
            if (!why.empty()) { cout << (why=="deny"?"[拒否] 拒否リストに一致: ":"[未許可] 許可リストに未一致: ") << cmd << "\n"; cout << "必要なら /allow add <パターン> を追加してください。\n"; continue; }
            cout << "[確認] コマンドを実行しますか？ [y/N]: " << cmd << "\n> ";
            string yn; if (!getline(cin, yn)) { cin.clear(); yn.clear(); }
            auto t = utils::trim(yn); transform(t.begin(), t.end(), t.begin(), ::tolower);
            if (t=="y"||t=="yes") {
                string out; int rc = utils::run_shell_with_status(cmd + " 2>&1", out);
                cout << out;
                cout << "[exit=" << rc << "]\n";
            } else {
                cout << "[キャンセル] 実行しませんでした。\n";
            }
            continue;
        }
        if (user.rfind("/web",0)==0) {
            string q = utils::trim(user.substr(4));
            if (q.empty()) { cout << "使い方: /web <検索語>\n"; continue; }
            vector<WebResult> r = web_search(http, q, 6);
            if (r.empty()) { cout << "検索結果が見つかりませんでした。\n"; continue; }
            cout << "[Web検索結果]" << "\n";
            for (size_t i=0;i<r.size();++i) {
                cout << "  ["<<(i+1)<<"] "
                     << (!r[i].title.empty()?r[i].title:r[i].text.substr(0,60))
                     << "\n     " << r[i].url << "\n";
            }
            continue;
        }
        if (user.rfind("/target",0)==0 || user.rfind("/files",0)==0) {
            string q = utils::trim(user.substr(user[1]=='t'?7:6));
            if (q.empty()) { cout << "使い方: /target <キーワード>\n"; continue; }
            auto hits = find_relevant_files(".", q, 10);
            if (hits.empty()) { cout << "該当するファイルが見つかりません。\n"; continue; }
            cout << "[候補ファイル]" << "\n";
            for (size_t i=0;i<hits.size();++i) {
                cout << "  ["<<(i+1)<<"] score="<<hits[i].score<<" "<<hits[i].path<<"\n";
            }
            continue;
        }
        if (user.rfind("/temp",0)==0) {
            istringstream iss(user.substr(5)); 
            double v; 
            if (iss>>v && v >= 0.0 && v <= 2.0) { 
                tune.temperature=v; 
                cout<<"temperature="<<tune.temperature<<"\n"; 
            } else {
                cout << "[エラー] temperatureは0.0-2.0の範囲で指定してください\n";
            }
            continue;
        }
        if (user.rfind("/top_p",0)==0) {
            istringstream iss(user.substr(6)); 
            double v; 
            if (iss>>v && v >= 0.0 && v <= 1.0) { 
                tune.top_p=v; 
                cout<<"top_p="<<tune.top_p<<"\n"; 
            } else {
                cout << "[エラー] top_pは0.0-1.0の範囲で指定してください\n";
            }
            continue;
        }
        if (user.rfind("/ctx",0)==0) {
            istringstream iss(user.substr(4)); 
            int v; 
            if (iss>>v && v >= 512 && v <= 131072) { 
                tune.context=v; 
                cout<<"context="<<tune.context<<"\n"; 
            } else {
                cout << "[エラー] contextは512-131072の範囲で指定してください\n";
            }
            continue;
        }
        if (user.rfind("/max",0)==0) {
            istringstream iss(user.substr(4)); 
            int v; 
            if (iss>>v && v >= 1 && v <= 8192) { 
                tune.max_tokens=v; 
                cout<<"max_tokens="<<tune.max_tokens<<"\n"; 
            } else {
                cout << "[エラー] max_tokensは1-8192の範囲で指定してください\n";
            }
            continue;
        }

        auto ans = do_chat_once(user);
        if (!ans) { cout << "[エラー] 応答を取得できませんでした。\n"; continue; }
        if (auto_mode) {
            auto preview = apply_file_blocks(*ans, true);
            if (!preview.written.empty()) {
                if (auto_dry_run) {
                    cout << "[ドライラン] 以下を適用予定です:\n" << preview.log;
                } else if (auto_confirm) {
                    cout << "[確認] 以下の変更を適用しますか？ [y/N]:\n" << preview.log << "> ";
                    string yn; if (!getline(cin, yn)) { cin.clear(); yn.clear(); }
                    auto t = utils::trim(yn); transform(t.begin(), t.end(), t.begin(), ::tolower);
                    if (t=="y"||t=="yes") {
                        auto applied = apply_file_blocks(*ans, false);
                        cout << "[適用完了]\n" << applied.log;
                    } else {
                        cout << "[キャンセル] 適用しませんでした。\n";
                    }
                } else {
                    auto applied = apply_file_blocks(*ans, false);
                    cout << "[自動適用]\n" << applied.log;
                }
            }
        }
        cout << "アシスタント> " << *ans << "\n";
    }
    cout << "終了します。\n";
    return 0;
}
