#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <lexer-smoke-bin> <parser-smoke-bin>" >&2
  exit 2
fi

LEXER_BIN="$1"
PARSER_BIN="$2"

if [[ ! -x "$LEXER_BIN" || ! -x "$PARSER_BIN" ]]; then
  echo "Smoke binaries are not executable." >&2
  exit 2
fi

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/styio-fuzz-lifetime.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

LEXER_CORPUS="$TMP_DIR/lexer"
PARSER_CORPUS="$TMP_DIR/parser"
mkdir -p "$LEXER_CORPUS" "$PARSER_CORPUS"

python3 - "$LEXER_CORPUS/repeated.txt" <<'PY'
from pathlib import Path
import sys
Path(sys.argv[1]).write_bytes(b"a" * 1024)
PY
: > "$PARSER_CORPUS/empty.styio"
printf '>>' > "$PARSER_CORPUS/iterator.styio"

export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_container_overflow=0}"

"$LEXER_BIN" "$LEXER_CORPUS" 256
"$PARSER_BIN" "$PARSER_CORPUS" 64
