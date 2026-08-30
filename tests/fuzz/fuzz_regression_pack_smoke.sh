#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/styio-fuzz-pack-smoke.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

ARTIFACT_ROOT="$TMP_DIR/fuzz-artifacts"
OUT_DIR="$TMP_DIR/fuzz-regressions"
CORPUS_ROOT="$TMP_DIR/corpus"

mkdir -p "$ARTIFACT_ROOT/lexer" "$ARTIFACT_ROOT/parser"

printf 'seed-lexer\n' > "$ARTIFACT_ROOT/lexer/crash-lexer-001"
printf 'seed-parser\n' > "$ARTIFACT_ROOT/parser/timeout-parser-001"
printf '[lexer log]\n' > "$ARTIFACT_ROOT/lexer.log"
printf '[parser log]\n' > "$ARTIFACT_ROOT/parser.log"
cat > "$ARTIFACT_ROOT/lexer/replay-options.json" <<'EOF'
{"target":"lexer","seed":1,"timeout":10,"max_len":65536,"dict":"tests/fuzz/styio.dict"}
EOF
cat > "$ARTIFACT_ROOT/parser/replay-options.json" <<'EOF'
{"target":"parser","seed":2,"timeout":10,"max_len":65536,"dict":"tests/fuzz/styio.dict"}
EOF
printf 'replay lexer\n' > "$ARTIFACT_ROOT/lexer/replay-command.txt"
printf 'replay parser\n' > "$ARTIFACT_ROOT/parser/replay-command.txt"

"$ROOT/scripts/fuzz-regression-pack.sh" \
  --artifacts-root "$ARTIFACT_ROOT" \
  --out-dir "$OUT_DIR" \
  --corpus-root "$CORPUS_ROOT" \
  --run-id "smoke-pack" \
  --copy-into-corpus

CASE_DIR="$OUT_DIR/smoke-pack"
[[ -d "$CASE_DIR" ]]
[[ -f "$CASE_DIR/summary.json" ]]
[[ -f "$CASE_DIR/manifest.tsv" ]]
[[ -f "$CASE_DIR/CASE.md" ]]
[[ -x "$CASE_DIR/apply-corpus-backflow.sh" ]]

grep -q '"total": 2' "$CASE_DIR/summary.json"
grep -q '"lexer": 1' "$CASE_DIR/summary.json"
grep -q '"parser": 1' "$CASE_DIR/summary.json"

manifest_lines="$(wc -l < "$CASE_DIR/manifest.tsv" | tr -d ' ')"
[[ "$manifest_lines" -eq 3 ]]

[[ -n "$(find "$CORPUS_ROOT/lexer" -type f -name '*.seed' -print -quit)" ]]
[[ -n "$(find "$CORPUS_ROOT/parser" -type f -name '*.seed' -print -quit)" ]]
[[ -f "$CASE_DIR/lexer/replay-options.json" ]]
[[ -f "$CASE_DIR/parser/replay-options.json" ]]
[[ -f "$CASE_DIR/lexer/replay-command.txt" ]]
[[ -f "$CASE_DIR/parser/replay-command.txt" ]]

DICT="$ROOT/tests/fuzz/styio.dict"
[[ -f "$DICT" ]]
[[ -f "$ROOT/tests/fuzz/corpus/lexer/seed-repeated-source-lifetime.styio" ]]
[[ -f "$ROOT/tests/fuzz/corpus/parser/seed-iterator-no-progress.styio" ]]
[[ -f "$ROOT/tests/fuzz/corpus/parser/seed-empty-input.styio" ]]

LEXER_CACHE="fuzz-work/lexer/corpus"
PARSER_CACHE="fuzz-work/parser/corpus"
[[ "$LEXER_CACHE" != "$PARSER_CACHE" ]]
[[ "$LEXER_CACHE" == *"/lexer/"* ]]
[[ "$PARSER_CACHE" == *"/parser/"* ]]
