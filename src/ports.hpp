#pragma once
#include <string>
#include <vector>
#include <functional>
#include <optional>

// 依存関係逆転の原則（DIP）に基づき、外部環境（シェル、HTTP通信）への依存を抽象化するインターフェース群。
// これにより、ビジネスロジックと具体的な実装を分離し、テスト容易性を向上させる。
/// @brief シェルコマンド実行の抽象インターフェース
struct IShell {
    virtual ~IShell() = default;
    virtual std::string run(const std::string& cmd) = 0;
};

/// @brief HTTP通信の抽象インターフェース
struct IHttp {
    virtual ~IHttp() = default;
    virtual std::optional<std::string> get(const std::string& url, const std::vector<std::string>& headers = {}) = 0;
    virtual std::optional<std::string> post_json(const std::string& url, const std::string& json, const std::vector<std::string>& headers = {}) = 0;
};

// `utils`内の関数を利用する、インターフェースの標準実装。
namespace default_ports {
        /// @brief `utils::run_shell` を使用する `IShell` の標準実装
    struct Shell : IShell {
        std::string run(const std::string& cmd) override;
    };
        /// @brief `utils::http_get` と `utils::http_post_json` を使用する `IHttp` の標準実装
    struct Http : IHttp {
        std::optional<std::string> get(const std::string& url, const std::vector<std::string>& headers = {}) override;
        std::optional<std::string> post_json(const std::string& url, const std::string& json, const std::vector<std::string>& headers = {}) override;
    };
}

