#include "web_search.hpp"
#include "utils.hpp"
#include <algorithm>
#include <sstream>

using namespace std;

static string url_encode(const string& s) {
    static const char hex[] = "0123456789ABCDEF";
    string o; o.reserve(s.size()*3);
    for (unsigned char c : s) {
        if (isalnum(c) || c=='-' || c=='_' || c=='.' || c=='~') o += c;
        else { o += '%'; o += hex[c>>4]; o += hex[c&15]; }
    }
    return o;
}

vector<WebResult> web_search(IHttp& http, const string& query, int max_results) {
    vector<WebResult> out;
    string url = string("https://api.duckduckgo.com/?q=") + url_encode(query) + "&format=json&no_html=1&skip_disambig=1&t=agens&kl=jp-jp";
    auto body_opt = http.get(url);
    if (!body_opt) return out;
    const string& body = *body_opt;

    // AbstractText / AbstractURL / Heading
    string abstract, aurl, heading;
    utils::json_find_first_string_value(body, "AbstractText", abstract);
    utils::json_find_first_string_value(body, "AbstractURL", aurl);
    utils::json_find_first_string_value(body, "Heading", heading);
    if (!abstract.empty() || !aurl.empty()) {
        WebResult r; r.title = heading; r.text = abstract; r.url = aurl; out.push_back(r);
    }
    // RelatedTopics: collect Text / FirstURL and pair by index
    auto texts = utils::json_collect_string_values(body, "Text");
    auto urls  = utils::json_collect_string_values(body, "FirstURL");
    size_t n = min(texts.size(), urls.size());
    for (size_t i=0; i<n && (int)out.size()<max_results; ++i) {
        if (texts[i].empty() || urls[i].empty()) continue;
        out.push_back(WebResult{string(), texts[i], urls[i]});
    }
    if ((int)out.size()>max_results) out.resize(max_results);
    return out;
}

