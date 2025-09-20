#include "backend.hpp"
#include "utils.hpp"
#include "chat.hpp"
#include <algorithm>

using namespace std;

bool probe_ollama(IHttp& http) {
    string body = http.get("http://localhost:11434/api/version");
    return !body.empty() && body.find("version") != string::npos;
}

bool probe_lmstudio(IHttp& http) {
    string body = http.get("http://localhost:1234/v1/models", {"Authorization: Bearer lm-studio"});
    if (body.empty() || body.find("error") != string::npos) {
        body = http.get("http://localhost:1234/v1/models");
    }
    return !body.empty() && (body.find("data") != string::npos || body.find("object") != string::npos);
}

vector<string> list_ollama_models(IHttp& http) {
    string body = http.get("http://localhost:11434/api/tags");
    auto names = utils::json_collect_string_values(body, "name");
    sort(names.begin(), names.end());
    names.erase(unique(names.begin(), names.end()), names.end());
    return names;
}

vector<string> list_lmstudio_models(IHttp& http) {
    string body = http.get("http://localhost:1234/v1/models", {"Authorization: Bearer lm-studio"});
    if (body.find("error")!=string::npos || body.empty()) body = http.get("http://localhost:1234/v1/models");
    auto ids = utils::json_collect_string_values(body, "id");
    sort(ids.begin(), ids.end());
    ids.erase(unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

optional<string> chat_ollama(IHttp& http, const string& model, const vector<ChatMsg>& msgs, const InferenceTuning& t) {
    string body = build_ollama_chat_body(model, msgs, t);
    string resp = http.post_json("http://localhost:11434/api/chat", body, {});
    if (resp.empty()) return nullopt;
    string content;
    if (utils::json_find_first_string_value(resp, "content", content)) return content;
    return resp;
}

optional<string> chat_lmstudio(IHttp& http, const string& model, const vector<ChatMsg>& msgs, const InferenceTuning& t) {
    string body = build_lmstudio_chat_body(model, msgs, t);
    string resp = http.post_json("http://localhost:1234/v1/chat/completions", body, {"Authorization: Bearer lm-studio"});
    if (resp.empty() || (resp.find("error") != string::npos && resp.find("choices") == string::npos)) {
        resp = http.post_json("http://localhost:1234/v1/chat/completions", body, {});
    }
    if (resp.empty()) return nullopt;
    
    // Check for error in response
    if (resp.find("error") != string::npos && resp.find("choices") == string::npos) {
        return nullopt;
    }
    
    string content;
    if (utils::json_find_first_string_value(resp, "content", content)) return content;
    return nullopt; // Don't return raw response on parse failure
}

