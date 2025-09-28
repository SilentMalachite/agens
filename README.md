# Agens ローカルLLMエージェント（C++20）

インストール済みの OLLAMA と LM Studio のローカルAPIに接続し、
システム要件（GPU/VRAM/RAM/Apple Silicon）を検出して推論パラメータを自動調整する、日本語応答専用の対話エージェントです。

- 対応バックエンド
  - Ollama: `http://localhost:11434`
  - LM Studio (OpenAI互換API): `http://localhost:1234/v1`
- 常に日本語で応答（systemプロンプトを付与）
- 自動パラメータ調整（context, max_tokens, temperature, top_p, gpu_layers）
- 対話REPLと単発実行に対応
- 追加機能
  - Web検索（DuckDuckGo Instant Answer APIベース）: `/web <検索語>`
  - ターゲットファイル特定（カレント配下の関連ファイル抽出）: `/target <キーワード>`
  - 設計書AGENT(S).mdを読み取り、自律実行（ファイル生成/更新）: `/auto`
  - OSコマンド・プログラム実行: `/sh <cmd>`, 即時実行 `/sh! <cmd>`、`/prog <exe> [args]`, `/prog! <exe> [args]`
  - 作業ディレクトリ変更: `/cd <path>`（未指定で現在のディレクトリを表示）
  - 実行許可/拒否リスト管理: `/allow ...` `/deny ...`
  - 設定ファイルのロード/保存/パス表示: `/config reload|save|path`

## 開発・品質管理

### ビルドとテスト
- 継続的インテグレーション: GitHub Actions（Linux/macOS/Windows）
- AddressSanitizer/UndefinedBehaviorSanitizer（デバッグビルド時）
- 静的解析: clang-tidy
- ユニットテスト及び統合テスト

### コード品質
- C++20標準準拠
- Memory safety（RAII、スマートポインタ）
- エラーハンドリング（std::optional、例外安全）
- クロスプラットフォーム対応

### 既知の制限・改善点
- 大規模なLLMモデルでは推論時間が長くなる可能性
- Web検索はDuckDuckGo Instant Answer APIに依存
- JSON解析は軽量実装のため、複雑な構造では制限あり

## ビルド

- 依存: CMake(>=3.16), C++20対応コンパイラ, `curl` コマンド
  - Windows: `curl.exe`（Windows 10 以降に同梱）、PowerShell（標準搭載）
  - Linux: `curl` をインストール（例: `sudo apt install curl`）
  - macOS: `curl` は同梱

```
mkdir -p build && cd build
cmake ..
cmake --build . -j
```

生成物: `build/agens`

## 使い方

- バックエンド自動検出（Ollama/LM Studio のどちらか起動しておく）
```
./agens
```
- バックエンドとモデルを指定
```
./agens -b ollama   -m mistral:7b-instruct-q4_k_m
./agens -b lmstudio -m TheBloke/Mistral-7B-Instruct-GGUF
```
- 単発プロンプト
```
./agens -b ollama -m llama3:instruct -p "日本語で自己紹介して"
```

REPL中のコマンド:
- `/exit` 終了
- `/quit` 終了（`/exit`と同義）
- `/temp 0.7` 温度変更
- `/top_p 0.9` top_p変更
- `/ctx 4096` コンテキスト長変更
- `/max 512` 生成トークン数変更
- `/model` モデル変更（一覧表示→番号/名前で選択）
- `/model llama3:instruct` のように直接指定も可能
- `/web 量子化 LLM` Web検索結果の要約/関連候補を表示
- `/target http client` キーワードに関連が高いローカルファイルを列挙
- `/agents` 検出したAGENT(S).mdの一覧を表示
- `/plan` AGENT(S).mdから抽出したタスク一覧を表示
- `/auto` 自動モードON（AGENT(S).mdを読み込み、LLM出力内のファイルブロックを自動適用）
- `/auto off` 自動モードOFF
- `/auto confirm` 各応答の適用前に確認（y/N）
- `/auto confirm off` 確認なしで自動適用
- `/auto dry` ドライラン（書き込みせず計画のみ表示）
- `/auto dry off` ドライラン停止
- `/auto status` 現在のON/OFF・confirm・dryの状態を表示
- `/cd <path>` 作業ディレクトリを変更（`~`展開対応）。`/cd`のみで現在のディレクトリを表示。
- `/allow list` 許可リスト表示、`/allow add <pat>` 追加、`/allow rm <pat>` 削除、`/allow clear` クリア。
- `/deny list` 拒否リスト表示、`/deny add <pat>` 追加、`/deny rm <pat>` 削除、`/deny clear` クリア。

備考: 許可リストが空の間は制限なし（拒否リストのみ適用）。許可リストが1つ以上ある場合、そのいずれかに部分一致しないコマンドはブロックされます。拒否リストは常に最優先でブロックします。

自動モードで期待されるLLM出力フォーマット:
```
```file: 相対/パス
<内容>
```
```
または
```
```agens:file=相対/パス
<内容>
```
```

注: 空行は無視し、終了コマンド（`/exit`または`/quit`）を受け付けるまで待機し続けます。

## 環境変数

- `AGENS_LANG`: UIメッセージの言語を切り替えます。
  - 値: `ja`（既定）| `en`
  - 例: `AGENS_LANG=en ./agens -h`
- `AGENS_UNIFIED_GPU_RATIO`: Apple Silicon 等の統合メモリ環境で、GPUが利用可能とみなすRAM比率（0<ratio<1）。
  - 既定: `0.5`（RAMの50%を目安にGPU利用可能量として表示）
  - 例: `AGENS_UNIFIED_GPU_RATIO=0.25 ./agens -p "hi"`
  - 設定ファイル（`~/.config/agens/config.json` など）の `unified_gpu_ratio` でも指定可能（環境変数が優先）。

## 実装メモ

- HTTPは `curl` をサブプロセス実行（`src/utils.hpp`）。POSTは一時JSONファイルを介してクロスプラットフォームで安定化。
- JSONは簡易パーサ（安全性より軽量性優先）。主要キー（name/id/content）のみ抽出しています。
- Ollama API: `/api/version`, `/api/tags`, `/api/chat` を利用。`options` に `num_ctx`, `num_predict`, `temperature`, `top_p` を付与。
- LM Studio API: OpenAI互換 `/v1/models`, `/v1/chat/completions` を利用。`extra.gpu_layers` をヒューリスティクスで設定。
- システム検出（`src/system_info.cpp`）
  - macOS: `sysctl`, `system_profiler`, `uname`、必要に応じて `nvidia-smi`
  - Linux: `/proc/meminfo` を直接パース、必要に応じて `nvidia-smi`
  - Windows: `wmic` または PowerShell CIM で RAM、`nvidia-smi` が無い場合は `Win32_VideoController` から VRAM/名前を取得

## 注意事項

- LM Studio の `/v1/models` は環境により未対応・空を返す場合があります。その際はモデル名を手入力してください。
- `gpu_layers`/`context` はモデルと量子化に依存して最適値が変わります。本ツールの値は安全側の目安として調整しています。
- JSON抽出は簡易実装のため、将来のAPI応答変更により失敗する可能性があります。その場合はログを添えてIssue化してください。

## ライセンス

このプロジェクトは Apache License 2.0 の下で提供されます。詳細は `LICENSE` を参照してください。

## テスト

- ビルドと同時にユニットテストを生成します（デフォルトON）。
  - 実行: `cd build && ctest --output-on-failure`
- テスト項目
  - `unit`（`tests/test_main.cpp`）
    - JSONエスケープ（`json_escape`）
    - Ollama/LM Studio リクエストボディ生成の検証（必須キー・パラメータの存在）
    - 推論パラメータ自動調整（`decide_tuning`）のVRAM段階・Apple Silicon/CPUのみ・8GB RAM時の抑制
    - 簡易JSON抽出ヘルパ（`json_find_first_string_value`/`json_collect_string_values`）
  - `cli_help`（実行ファイルの`--help`が正常終了すること）

注: ネットワークやバックエンド実体には依存しない純粋ユニットテストで構成しています。
- `/sh <コマンド>` OSシェルで実行（確認プロンプトあり）
- `/sh! <コマンド>` 確認なしで即時実行
- `/prog <プログラム> [引数...]` 実行（確認プロンプトあり）
- `/prog! <プログラム> [引数...]` 確認なしで即時実行
- 設定ファイル
  - 位置（デフォルト）
    - macOS/Linux: `$XDG_CONFIG_HOME/agens/config.json` または `~/.config/agens/config.json`
    - Windows: `%APPDATA%\agens\config.json`
  - 起動時に自動ロード。存在しなければ自動作成。
  - 設定変更（/allow, /deny, /auto confirm|dry, /model, /cd等）は都度自動保存。
  - 手動操作: `/config save`, `/config reload`, `/config path`
