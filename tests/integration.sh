#!/usr/bin/env bash
set -u
exe="$1"
shift || true

fail() { echo "[FAIL] $*" >&2; exit 1; }
ok() { echo "[OK] $*"; }

# 1) 英語ヘルプ
out=$(AGENS_LANG=en "$exe" -h 2>&1)
echo "$out" | grep -q "^Usage: agens" || fail "English help not shown"
echo "$out" | grep -q "backend: ollama|lmstudio" || fail "Backend hint missing (en)"
echo "$out" | grep -vq "使い方:" || true
ok "help (en)"

# 2) 日本語ヘルプ
out=$(AGENS_LANG=ja "$exe" -h 2>&1)
echo "$out" | grep -q "^使い方: agens" || fail "Japanese help not shown"
ok "help (ja)"

# 3) 統合メモリ比率（英語表示でラベル確認）
set +e
out=$(AGENS_LANG=en AGENS_UNIFIED_GPU_RATIO=0.3 "$exe" -p hi 2>&1)
rc=$?
set -e
[[ $rc -eq 0 || $rc -eq 1 ]] || fail "unexpected exit code: $rc"
echo "$out" | grep -q ", Unified memory (est. GPU ~" || fail "Unified memory label missing (en)"
ok "unified memory label (en)"

# 4) バックエンド未起動時メッセージ（英語）または正常起動メッセージ
if echo "$out" | grep -q "No local API found for Ollama(11434) or LM Studio(1234)."; then
    echo "$out" | grep -q "Please start Ollama or LM Studio server and try again." || fail "missing start server message (en)"
    ok "api missing (en)"
elif echo "$out" | grep -q "Starting local LLM agent"; then
    ok "api available (en) - backend is running, this is acceptable"
else
    fail "neither api missing message nor startup message found"
fi

# 5) curl stderr 抑制（"curl:" が出ない）
echo "$out" | grep -q "curl:" && fail "curl stderr leaked"
ok "curl stderr suppressed"

# 6) 比率が異常値でもラベルが出る（クランプ）
set +e
out=$(AGENS_LANG=en AGENS_UNIFIED_GPU_RATIO=abc "$exe" -p hi 2>&1)
set -e
echo "$out" | grep -q ", Unified memory (est. GPU ~" || fail "Unified memory label missing with invalid ratio"
ok "invalid ratio clamped"

echo "Integration OK"
exit 0

