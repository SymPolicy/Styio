#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

search_sources() {
  local pattern="$1"
  local target="$2"

  if command -v rg >/dev/null 2>&1; then
    rg -n "$pattern" "$target" || true
    return
  fi

  grep -R -n -E -- "$pattern" "$target" || true
}

report_hits_latest() {
  local title="$1"
  local hits="$2"
  echo "[parser-legacy-audit] ${title}" >&2
  printf '%s\n' "$hits" >&2
}

violations=0

legacy_entry_hits_latest="$(search_sources 'parse_main_block_legacy\(' src)"
legacy_entry_hits_latest="$(
  printf '%s\n' "$legacy_entry_hits_latest" \
    | grep -vE '^src/StyioParser/Parser\.(cpp|hpp):' || true
)"
if [[ -n "$legacy_entry_hits_latest" ]]; then
  report_hits_latest \
    "unexpected direct parse_main_block_legacy(...) callsites outside parser core" \
    "$legacy_entry_hits_latest"
  violations=1
fi

testing_harness_hits_latest="$(search_sources 'StyioParserEngine::Legacy|parse_main_block_legacy\(' src/StyioTesting)"
if [[ -n "$testing_harness_hits_latest" ]]; then
  report_hits_latest \
    "src/StyioTesting must stay nightly-first; legacy routing is not allowed here" \
    "$testing_harness_hits_latest"
  violations=1
fi

main_entry_hits_latest="$(search_sources 'parse_main_block_with_engine_latest\(' src/main.cpp)"
if [[ -z "$main_entry_hits_latest" ]]; then
  echo "[parser-legacy-audit] src/main.cpp no longer routes through parse_main_block_with_engine_latest(...)" >&2
  violations=1
fi

if [[ "$violations" -ne 0 ]]; then
  exit 1
fi

echo "[parser-legacy-audit] ok"
