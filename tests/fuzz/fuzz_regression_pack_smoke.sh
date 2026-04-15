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
