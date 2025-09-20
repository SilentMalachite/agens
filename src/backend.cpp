#include "backend.hpp"
#include "utils.hpp"
#include "chat.hpp"
#include <algorithm>

using namespace std;

namespace backend {

namespace ollama {

bool probe(IHttp& http) {
    auto body = http.get("http://localhost:11434/api/version");
    return body.has_value() && body->find("version") != string::npos;
}

vector<string> list_models(IHttp& http) {
    auto body = http.get("http://localhost:11434/api/tags");
    if (!body) return {};
    auto names = utils::json_collect_string_values(*body, "name");
    sort(names.begin(), names.end());
    names.erase(unique(names.begin(), names.end()), names.end());
    return names;
}

optional<string> chat(IHttp& http, const string& model, const vector<ChatMsg>& msgs, const InferenceTuning& t) {
    string body = build_ollama_chat_body(model, msgs, t);
    auto resp = http.post_json("http://localhost:11434/api/chat", body, {});
    if (!resp) return nullopt;
    string content;
    if (utils::json_find_first_string_value(*resp, "content", content)) return content;
    return *resp;
}

} // namespace ollama

namespace lmstudio {

bool probe(IHttp& http) {
    auto body = http.get("http://localhost:1234/v1/models", {"Authorization: Bearer lm-studio"});
    if (!body.has_value() || body->find("error") != string::npos) {
        body = http.get("http://localhost:1234/v1/models");
    }
    return body.has_value() && (body->find("data") != string::npos || body->find("object") != string::npos);
}

vector<string> list_models(IHttp& http) {
    auto body = http.get("http://localhost:1234/v1/models", {"Authorization: Bearer lm-studio"});
    if (!body.has_value() || body->find("error")!=string::npos) body = http.get("http://localhost:1234/v1/models");
    if (!body) return {};
    auto ids = utils::json_collect_string_values(*body, "id");
    sort(ids.begin(), ids.end());
    ids.erase(unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

optional<string> chat(IHttp& http, const string& model, const vector<ChatMsg>& msgs, const InferenceTuning& t) {
    string body = build_lmstudio_chat_body(model, msgs, t);
    auto resp = http.post_json("http://localhost:1234/v1/chat/completions", body, {"Authorization: Bearer lm-studio"});
    if (!resp.has_value() || (resp->find("error") != string::npos && resp->find("choices") == string::npos)) {
        resp = http.post_json("http://localhost:1234/v1/chat/completions", body, {});
    }
    if (!resp.has_value()) return nullopt;
    
    // Check for error in response
    if (resp->find("error") != string::npos && resp->find("choices") == string::npos) {
        return nullopt;
    }
    
    string content;
    if (utils::json_find_first_string_value(*resp, "content", content)) return content;
    return *resp;
}

} // namespace lmstudio

} // namespace backend

