#pragma once
#include <string>
#include <vector>
#include "system_info.hpp"

struct ChatMsg { std::string role; std::string content; };

std::string json_escape(const std::string& s);
std::string build_ollama_chat_body(const std::string& model, const std::vector<ChatMsg>& msgs, const InferenceTuning& t);
std::string build_lmstudio_chat_body(const std::string& model, const std::vector<ChatMsg>& msgs, const InferenceTuning& t);

