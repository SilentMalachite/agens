#include "chat.hpp"
#include <sstream>

using namespace std;

string json_escape(const string& s) {
    string out; out.reserve(s.size()+8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

string build_ollama_chat_body(const string& model, const vector<ChatMsg>& msgs, const InferenceTuning& t) {
    ostringstream oss;
    oss << "{";
    oss << "\"model\":\"" << json_escape(model) << "\",";
    oss << "\"stream\":false,";
    // messages
    oss << "\"messages\":[";
    for (size_t i=0;i<msgs.size();++i) {
        const auto& m = msgs[i];
        if (i) oss << ",";
        oss << "{\"role\":\"" << json_escape(m.role) << "\",\"content\":\"" << json_escape(m.content) << "\"}";
    }
    oss << "],";
    // options
    oss << "\"options\":{";
    oss << "\"temperature\":" << t.temperature << ",";
    oss << "\"top_p\":" << t.top_p << ",";
    oss << "\"num_ctx\":" << t.context << ",";
    oss << "\"num_predict\":" << t.max_tokens;
    oss << "}";
    oss << "}";
    return oss.str();
}

string build_lmstudio_chat_body(const string& model, const vector<ChatMsg>& msgs, const InferenceTuning& t) {
    ostringstream oss;
    oss << "{";
    oss << "\"model\":\"" << json_escape(model) << "\",";
    oss << "\"stream\":false,";
    oss << "\"temperature\":" << t.temperature << ",";
    oss << "\"top_p\":" << t.top_p << ",";
    oss << "\"max_tokens\":" << t.max_tokens << ",";
    // messages
    oss << "\"messages\":[";
    for (size_t i=0;i<msgs.size();++i) {
        const auto& m = msgs[i];
        if (i) oss << ",";
        oss << "{\"role\":\"" << json_escape(m.role) << "\",\"content\":\"" << json_escape(m.content) << "\"}";
    }
    oss << "]";
    if (t.gpu_layers >= 0) {
        oss << ",\"extra\":{\"gpu_layers\":" << t.gpu_layers << "}";
    }
    oss << "}";
    return oss.str();
}

