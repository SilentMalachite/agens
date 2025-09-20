#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

#include "system_info.hpp"
#include "chat.hpp"
#include "utils.hpp"
#include "ports.hpp"
#include "backend.hpp"
#include "web_search.hpp"
#include "file_finder.hpp"

// 簡易テストランナー
static int failures = 0;
#define REQUIRE(cond) do { if(!(cond)) { std::cerr << "REQUIRE failed: " #cond " at " << __FILE__ << ':' << __LINE__ << "\n"; ++failures; } } while(0)
#define REQUIRE_EQ(a,b) do { auto _va=(a); auto _vb=(b); if(!((_va)==(_vb))) { std::cerr << "REQUIRE_EQ failed: " #a "==" #b " got ("<<_va<<","<<_vb<<") at "<<__FILE__<<":"<<__LINE__<<"\n"; ++failures; } } while(0)

int main() {
    // json_escape
    {
        std::string s = "\"\\\n\t";
        auto e = json_escape(s);
        REQUIRE(e.find("\\\"")!=std::string::npos);
        REQUIRE(e.find("\\\\")!=std::string::npos);
        REQUIRE(e.find("\\n")!=std::string::npos);
        REQUIRE(e.find("\\t")!=std::string::npos);
    }

    // IHttp モックでAPI応答パースを検証
    struct MockHttp : IHttp {
        std::vector<std::pair<std::string,std::string>> map_get;
        std::vector<std::pair<std::string,std::string>> map_post;
        std::string get(const std::string& url, const std::vector<std::string>& headers = {}) override {
            (void)headers;
            for (auto& kv : map_get) if (kv.first==url) return kv.second; return std::string();
        }
        std::string post_json(const std::string& url, const std::string& json, const std::vector<std::string>& headers = {}) override {
            (void)json; (void)headers;
            for (auto& kv : map_post) if (kv.first==url) return kv.second; return std::string();
        }
        void on_get(const std::string& url, const std::string& resp) { map_get.emplace_back(url, resp); }
        void on_post(const std::string& url, const std::string& resp) { map_post.emplace_back(url, resp); }
    };

    // モデル一覧（Ollama）
    {
        MockHttp http; http.on_get("http://localhost:11434/api/tags", "{\"models\":[{\"name\":\"llama3:instruct\"},{\"name\":\"mistral:7b\"}]} ");
        auto v = list_ollama_models(http);
        REQUIRE(v.size()==2);
    }
    // モデル一覧（LM Studio）
    {
        MockHttp http; http.on_get("http://localhost:1234/v1/models", "{\"data\":[{\"id\":\"TheBloke/Mistral\"},{\"id\":\"local-model\"}]} ");
        auto v = list_lmstudio_models(http);
        REQUIRE(v.size()==2);
    }
    // チャット（Ollama）
    {
        MockHttp http; http.on_post("http://localhost:11434/api/chat", "{\"message\":{\"role\":\"assistant\",\"content\":\"こんにちは世界\"}} ");
        InferenceTuning t; std::vector<ChatMsg> msgs = {{"system","日本語で"},{"user","テスト"}};
        auto out = chat_ollama(http, "m", msgs, t);
        REQUIRE(out.has_value());
        REQUIRE(out->find("こんにちは")!=std::string::npos);
    }
    // チャット（LM Studio）
    {
        MockHttp http; http.on_post("http://localhost:1234/v1/chat/completions", "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"了解です\"}}]} ");
        InferenceTuning t; std::vector<ChatMsg> msgs = {{"system","日本語で"},{"user","テスト"}};
        auto out = chat_lmstudio(http, "m", msgs, t);
        REQUIRE(out.has_value());
        REQUIRE(out->find("了解")!=std::string::npos);
    }

    // detect_system_info_with をモックで検証
    struct MockShell : IShell {
        std::vector<std::pair<std::string,std::string>> rules;
        std::string run(const std::string& cmd) override {
            for (auto& kv : rules) if (cmd == kv.first) return kv.second; return std::string();
        }
        void on(const std::string& k, const std::string& v) { rules.emplace_back(k,v); }
    };
    auto rf = [](const std::string& content){ return content; }; // placeholder for lambda wrapping

    // Linux: NVIDIAなし
    {
        MockShell sh;
        sh.on("which nvidia-smi >/dev/null 2>&1 && nvidia-smi -L 2>/dev/null", "");
        auto read_file = [&](const std::string& path){ (void)path; return std::string("MemTotal:       32824976 kB\n"); };
        auto si = detect_system_info_with(sh, read_file, false, true, false);
        REQUIRE(si.is_linux);
        REQUIRE(!si.has_nvidia);
        REQUIRE_EQ(si.vram_mb, 0);
        REQUIRE(si.ram_bytes > 3ull*1024ull*1024ull*1024ull);
    }

    // Linux: NVIDIAあり
    {
        MockShell sh;
        sh.on("which nvidia-smi >/dev/null 2>&1 && nvidia-smi -L 2>/dev/null", "GPU 0: NVIDIA");
        sh.on("nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>/dev/null", "16384\n");
        sh.on("nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null", "NVIDIA GeForce RTX 4080\n");
        auto read_file = [&](const std::string& path){ (void)path; return std::string("MemTotal:       64000000 kB\n"); };
        auto si = detect_system_info_with(sh, read_file, false, true, false);
        REQUIRE(si.has_nvidia);
        REQUIRE_EQ(si.vram_mb, 16384);
        REQUIRE(si.gpu_name.find("NVIDIA")!=std::string::npos);
        REQUIRE(si.ram_bytes > 32ull*1024ull*1024ull*1024ull);
    }

    // macOS: Apple Silicon + VRAM推定
    {
        MockShell sh;
        sh.on("sysctl -n hw.memsize 2>/dev/null", "34359738368\n"); // 32GB
        sh.on("uname -m", "arm64\n");
        sh.on("sysctl -n machdep.cpu.brand_string 2>/dev/null", "Apple M2\n");
        sh.on("system_profiler SPDisplaysDataType 2>/dev/null", "Chipset Model: Apple M2\nVRAM (合計): 15360 MB\n");
        sh.on("which nvidia-smi >/dev/null 2>&1 && nvidia-smi -L 2>/dev/null", "");
        auto read_file = [&](const std::string& path){ (void)path; return std::string(); };
        auto si = detect_system_info_with(sh, read_file, true, false, false);
        REQUIRE(si.is_macos && si.is_apple_silicon);
        REQUIRE_EQ(si.vram_mb, 15360);
        REQUIRE(si.ram_bytes >= 32ull*1024ull*1024ull*1024ull);
        REQUIRE(si.gpu_name.find("Chipset")!=std::string::npos);
    }

    // Windows: NVIDIAあり
    {
        MockShell sh;
        sh.on("wmic computersystem get TotalPhysicalMemory /value 2>NUL", "TotalPhysicalMemory=17179869184\n");
        sh.on("nvidia-smi -L 2>NUL", "GPU 0: NVIDIA\n");
        sh.on("nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>NUL", "8192\n");
        sh.on("nvidia-smi --query-gpu=name --format=csv,noheader 2>NUL", "NVIDIA RTX A2000\n");
        auto read_file = [&](const std::string& path){ (void)path; return std::string(); };
        auto si = detect_system_info_with(sh, read_file, false, false, true);
        REQUIRE(si.is_windows && si.has_nvidia);
        REQUIRE_EQ(si.vram_mb, 8192);
        REQUIRE(si.gpu_name.find("NVIDIA")!=std::string::npos);
        REQUIRE(si.ram_bytes >= 16ull*1024ull*1024ull*1024ull);
    }

    // Windows: 非NVIDIA（PowerShell CIM）
    {
        MockShell sh;
        sh.on("wmic computersystem get TotalPhysicalMemory /value 2>NUL", "\n");
        sh.on("powershell -NoProfile -Command \"(Get-CimInstance -ClassName Win32_ComputerSystem).TotalPhysicalMemory\" 2>NUL", "8589934592\n");
        sh.on("nvidia-smi -L 2>NUL", "");
        sh.on("powershell -NoProfile -Command \"Get-CimInstance Win32_VideoController | Select-Object -First 1 AdapterRAM,Name | ConvertTo-Json\" 2>NUL",
              "{\"AdapterRAM\":2147483648,\"Name\":\"AMD Radeon Pro\"}\n");
        auto read_file = [&](const std::string& path){ (void)path; return std::string(); };
        auto si = detect_system_info_with(sh, read_file, false, false, true);
        REQUIRE(si.is_windows && !si.has_nvidia);
        REQUIRE(si.vram_mb >= 2048);
        REQUIRE(si.gpu_name.find("AMD")!=std::string::npos);
    }

    // build_ollama_chat_body
    {
        InferenceTuning t; t.context=4096; t.max_tokens=256; t.temperature=0.5; t.top_p=0.9;
        std::vector<ChatMsg> msgs = {{"system","日本語で"},{"user","テスト"}};
        auto body = build_ollama_chat_body("mistral", msgs, t);
        REQUIRE(body.find("\"model\":\"mistral\"")!=std::string::npos);
        REQUIRE(body.find("\"messages\"")!=std::string::npos);
        REQUIRE(body.find("\"options\"")!=std::string::npos);
        REQUIRE(body.find("\"num_ctx\":4096")!=std::string::npos);
        REQUIRE(body.find("\"num_predict\":256")!=std::string::npos);
    }

    // build_lmstudio_chat_body with and without gpu_layers
    {
        InferenceTuning t; t.max_tokens=128; t.temperature=0.7; t.top_p=0.8; t.gpu_layers=-1;
        std::vector<ChatMsg> msgs = {{"system","日本語で"},{"user","テスト"}};
        auto body = build_lmstudio_chat_body("llama3", msgs, t);
        REQUIRE(body.find("\"model\":\"llama3\"")!=std::string::npos);
        REQUIRE(body.find("\"max_tokens\":128")!=std::string::npos);
        REQUIRE(body.find("gpu_layers")==std::string::npos);
        t.gpu_layers=40;
        auto body2 = build_lmstudio_chat_body("llama3", msgs, t);
        REQUIRE(body2.find("\"extra\":{\"gpu_layers\":40}")!=std::string::npos);
    }

    // decide_tuning by VRAM tiers
    {
        SystemInfo s; s.vram_mb=22000; s.ram_bytes=64ull<<30; // 64GB
        auto t = decide_tuning(s);
        REQUIRE_EQ(t.context, 8192);
        REQUIRE_EQ(t.gpu_layers, 100);

        s.vram_mb=12000; auto t2=decide_tuning(s); REQUIRE_EQ(t2.context, 6144); REQUIRE_EQ(t2.gpu_layers, 60);
        s.vram_mb=8000;  auto t3=decide_tuning(s); REQUIRE_EQ(t3.context, 4096); REQUIRE_EQ(t3.gpu_layers, 45);
        s.vram_mb=4000;  auto t4=decide_tuning(s); REQUIRE_EQ(t4.context, 3072); REQUIRE_EQ(t4.gpu_layers, 30);
    }

    // decide_tuning on low VRAM + Apple Silicon vs CPU only
    {
        SystemInfo mac; mac.is_macos=true; mac.is_apple_silicon=true; mac.vram_mb=0; mac.ram_bytes=16ull<<30;
        auto t = decide_tuning(mac);
        REQUIRE_EQ(t.context, 3072);
        REQUIRE_EQ(t.gpu_layers, 35);

        SystemInfo cpu; cpu.is_linux=true; cpu.vram_mb=0; cpu.ram_bytes=16ull<<30;
        auto t2 = decide_tuning(cpu);
        REQUIRE_EQ(t2.context, 2048);
        REQUIRE_EQ(t2.gpu_layers, 0);
    }

    // RAM clamp <= 8GB
    {
        SystemInfo s; s.vram_mb=22000; s.ram_bytes=8ull<<30; // 8GB
        auto t = decide_tuning(s);
        REQUIRE_EQ(t.context, 8192 > 2048 ? 2048 : 8192); // clamp to 2048
        REQUIRE_EQ(t.max_tokens, 512);
    }

    // utils JSON helpers
    {
        std::string body = "{\"models\":[{\"name\":\"a\"},{\"name\":\"b\"}] }";
        auto v = utils::json_collect_string_values(body, "name");
        REQUIRE(v.size()==2);
        REQUIRE(v[0]=="a" || v[1]=="a");

        std::string body2 = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"こんにちは\"}}]}";
        std::string out;
        bool ok = utils::json_find_first_string_value(body2, "content", out);
        REQUIRE(ok && out=="こんにちは");
    }

    // Optional integration tests (enable with env vars)
    if (const char* ollama = std::getenv("LLM_OLLAMA")) {
        std::string base = ollama; // e.g. http://localhost:11434
        auto ver = utils::http_get(base + std::string("/api/version"));
        REQUIRE(!ver.empty());
    }

    // web_search パース（MockHttp）
    {
        struct MockHttp : IHttp {
            std::string body;
            std::string get(const std::string& url, const std::vector<std::string>& headers = {}) override {
                (void)url; (void)headers; return body; }
            std::string post_json(const std::string& url, const std::string& json, const std::vector<std::string>& headers = {}) override {
                (void)url; (void)json; (void)headers; return std::string(); }
        } http;
        http.body = "{\"Heading\":\"テスト\",\"AbstractText\":\"概要\",\"AbstractURL\":\"https://example.com\",\"RelatedTopics\":[{\"FirstURL\":\"https://a\",\"Text\":\"A\"},{\"FirstURL\":\"https://b\",\"Text\":\"B\"}]}";
        auto results = web_search(http, "dummy", 3);
        REQUIRE(results.size()>=2);
        REQUIRE(results[0].url.find("https://example.com")!=std::string::npos);
    }

    // file_finder 小規模テスト（テンポラリ）
    {
        namespace fs = std::filesystem;
        auto dir = fs::temp_directory_path() / "agens_test_files";
        fs::create_directories(dir);
        {
            std::ofstream(dir / "README_TEST.md") << "これはサンプル。ターゲット検索のテストです。\n";
            fs::create_directories(dir / "src");
            std::ofstream(dir / "src" / "alpha.cpp") << "int main(){} // alpha";
        }
        auto hits = find_relevant_files(dir.string(), "ターゲット 検索", 5);
        REQUIRE(!hits.empty());
        REQUIRE(hits[0].path.find("README_TEST")!=std::string::npos);
        // cleanup best-effort
        std::error_code ec; fs::remove_all(dir, ec);
    }
    if (const char* lms = std::getenv("LLM_LMSTUDIO")) {
        std::string base = lms; // e.g. http://localhost:1234/v1
        auto models = utils::http_get(base + std::string("/models"), {"Authorization: Bearer lm-studio"});
        REQUIRE(!models.empty());
    }

    if (failures==0) {
        std::cout << "All tests passed\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
