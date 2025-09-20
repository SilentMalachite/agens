#pragma once
#include <string>
#include <vector>
#include <functional>

// 依存抽象（テスト容易化のため）
struct IShell {
    virtual ~IShell() = default;
    virtual std::string run(const std::string& cmd) = 0;
};

struct IHttp {
    virtual ~IHttp() = default;
    virtual std::string get(const std::string& url, const std::vector<std::string>& headers = {}) = 0;
    virtual std::string post_json(const std::string& url, const std::string& json, const std::vector<std::string>& headers = {}) = 0;
};

// 既存utilsを使う標準実装
namespace default_ports {
    struct Shell : IShell {
        std::string run(const std::string& cmd) override;
    };
    struct Http : IHttp {
        std::string get(const std::string& url, const std::vector<std::string>& headers = {}) override;
        std::string post_json(const std::string& url, const std::string& json, const std::vector<std::string>& headers = {}) override;
    };
}

