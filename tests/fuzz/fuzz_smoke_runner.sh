#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo "Usage: $0 <fuzz-target-bin> <corpus-dir> [runs]" >&2
  exit 2
fi

TARGET_BIN="$1"
CORPUS_DIR="$2"
RUNS="${3:-500}"

if [[ ! -x "$TARGET_BIN" ]]; then
  echo "Fuzz target not executable: $TARGET_BIN" >&2
  exit 2
fi
if [[ ! -d "$CORPUS_DIR" ]]; then
  echo "Corpus dir not found: $CORPUS_DIR" >&2
  exit 2
fi

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/styio-fuzz-smoke.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

TMP_CORPUS="$TMP_DIR/corpus"
mkdir -p "$TMP_CORPUS"
cp -R "$CORPUS_DIR"/. "$TMP_CORPUS"/

"$TARGET_BIN" -runs="$RUNS" "$TMP_CORPUS"
