#pragma once
#include <string>
#include <vector>
#include <optional>

/// @brief 汎用的なヘルパー関数群
namespace utils {

std::string shell_escape_single_quotes(const std::string& s);
int run_shell_with_status(const std::string& cmd, std::string& result);
std::string run_shell(const std::string& cmd);
std::string escape_double_quotes(const std::string& s);
std::optional<std::string> http_get(const std::string& url, const std::vector<std::string>& headers = {});
std::optional<std::string> http_post_json(const std::string& url, const std::string& json_body, const std::vector<std::string>& headers = {});
/// @brief 文字列の先頭と末尾の空白文字を削除する
std::string trim(const std::string& s);
/// @brief JSON風のテキストから、指定されたキーに一致する最初の文字列値を抽出する
/// @note 簡易的なパーサーであり、複雑なJSON構造には対応していない
/// @return 値が見つかった場合はtrue
bool json_find_first_string_value(const std::string& text, const std::string& key, std::string& out_value);
/// @brief JSON風のテキストから、指定されたキーに一致するすべての文字列値を収集する
/// @note 簡易的なパーサーであり、複雑なJSON構造には対応していない
std::vector<std::string> json_collect_string_values(const std::string& text, const std::string& key);
std::string json_escape(const std::string& s);

std::string temp_json_path();

} // namespace utils
