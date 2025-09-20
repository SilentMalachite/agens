#pragma once
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

struct AgentDoc { std::filesystem::path path; std::string content; };

// 設計書(AGENT.md/AGENTS.md)の探索
std::vector<AgentDoc> find_agent_docs(const std::filesystem::path& root);

// 箇条書きや番号付きリストからタスク抽出
std::vector<std::string> extract_tasks(const std::string& md);

// LLMへ渡す自動モード用システム指示を生成
std::string build_auto_system_prompt(const std::vector<AgentDoc>& docs);

// LLMの出力からファイルブロックを検出し、作成/更新を適用
// フォーマット（いずれか）:
// ```file: path/to/file.ext\n<content>\n```
// ```agens:file=path/to/file.ext\n<content>\n```
struct ApplyResult { std::vector<std::filesystem::path> written; std::string log; };
ApplyResult apply_file_blocks(const std::string& llm_output, bool dry_run=false);

