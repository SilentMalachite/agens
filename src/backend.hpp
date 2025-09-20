#pragma once
#include <string>
#include <vector>
#include <optional>
#include "ports.hpp"
#include "system_info.hpp"
#include "chat.hpp"

// 各LLMバックエンド（Ollama, LM Studioなど）との通信を担うAPI
namespace backend {

// Ollamaバックエンド用API
namespace ollama {
    /// @brief Ollamaサーバーが起動しているか確認する
    bool probe(IHttp& http);
    /// @brief 利用可能なモデル一覧を取得する
    std::vector<std::string> list_models(IHttp& http);
    /// @brief チャットAPIを呼び出し、アシスタントの応答を取得する
    std::optional<std::string> chat(IHttp& http, const std::string& model, const std::vector<ChatMsg>& msgs, const InferenceTuning& t);
}

// LM Studioバックエンド用API
namespace lmstudio {
    /// @brief LM Studioサーバーが起動しているか確認する
    bool probe(IHttp& http);
    /// @brief 利用可能なモデル一覧を取得する
    std::vector<std::string> list_models(IHttp& http);
    /// @brief チャットAPIを呼び出し、アシスタントの応答を取得する
    std::optional<std::string> chat(IHttp& http, const std::string& model, const std::vector<ChatMsg>& msgs, const InferenceTuning& t);
}

} // namespace backend
