#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 6 ]]; then
  echo "Usage: $0 <syntax-bin> <syntax-corpus> <completion-bin> <completion-corpus> <lsp-bin> <lsp-corpus>" >&2
  exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

bash "$SCRIPT_DIR/fuzz_smoke_runner.sh" "$1" "$2" 250
bash "$SCRIPT_DIR/fuzz_smoke_runner.sh" "$3" "$4" 250
bash "$SCRIPT_DIR/fuzz_smoke_runner.sh" "$5" "$6" 250
