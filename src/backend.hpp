#pragma once
#include <string>
#include <vector>
#include <optional>
#include "ports.hpp"
#include "system_info.hpp"
#include "chat.hpp"

// バックエンド検出
bool probe_ollama(IHttp& http);
bool probe_lmstudio(IHttp& http);

// モデル一覧
std::vector<std::string> list_ollama_models(IHttp& http);
std::vector<std::string> list_lmstudio_models(IHttp& http);

// チャット
std::optional<std::string> chat_ollama(IHttp& http, const std::string& model, const std::vector<ChatMsg>& msgs, const InferenceTuning& t);
std::optional<std::string> chat_lmstudio(IHttp& http, const std::string& model, const std::vector<ChatMsg>& msgs, const InferenceTuning& t);

