#include "ports.hpp"
#include "utils.hpp"

namespace default_ports {

    std::string Shell::run(const std::string& cmd) {
            return utils::run_shell(cmd);
        }

    std::optional<std::string> Http::get(const std::string& url, const std::vector<std::string>& headers) {
        return utils::http_get(url, headers);
    }

    std::optional<std::string> Http::post_json(const std::string& url, const std::string& json, const std::vector<std::string>& headers) {
        return utils::http_post_json(url, json, headers);
    }

}
