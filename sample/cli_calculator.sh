#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STYIO_BIN="${STYIO_BIN:-$ROOT_DIR/build/bin/styio}"

usage() {
  cat <<'EOF'
Usage:
  ./sample/cli_calculator.sh "1 + 2 * (3 + 4)"

Notes:
  - This wrapper delegates expression parsing to the Styio compiler itself.
  - Supported characters are limited to digits, spaces, parentheses, decimal
    points, and the operators + - * /.
  - Override the compiler path with STYIO_BIN if needed.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ $# -lt 1 ]]; then
  usage >&2
  exit 1
fi

if [[ ! -x "$STYIO_BIN" ]]; then
  echo "error: styio binary not found at $STYIO_BIN" >&2
  echo "build it first, for example:" >&2
  echo "  cmake -S . -B build -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm" >&2
  echo "  cmake --build build" >&2
  exit 1
fi

expr="$*"

if ! printf '%s\n' "$expr" | grep -Eq '^[0-9[:space:]+*/().-]+$'; then
  echo "error: unsupported characters in expression: $expr" >&2
  echo "allowed: digits, spaces, (), ., +, -, *, /" >&2
  exit 2
fi

tmp_file="$(mktemp "${TMPDIR:-/tmp}/styio_calc_XXXXXX")"
trap 'rm -f "$tmp_file"' EXIT

printf '>_(%s)\n' "$expr" > "$tmp_file"
"$STYIO_BIN" --file "$tmp_file"
