#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STYIO_BIN="${STYIO_BIN:-$ROOT_DIR/build/bin/styio}"
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/styio_calc_XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

usage() {
  cat <<'EOF'
Usage:
  ./sample/cli_calculator.sh
  ./sample/cli_calculator.sh "1 + 2 * (3 + 4)"
  ./sample/cli_calculator.sh --interactive

Notes:
  - This wrapper delegates expression parsing to the Styio compiler itself.
  - With no arguments, it starts an interactive REPL.
  - Supported characters are limited to digits, spaces, parentheses, decimal
    points, and the operators + - * /.
  - In interactive mode, type `exit`, `quit`, or `:q` to leave.
  - Override the compiler path with STYIO_BIN if needed.
EOF
}

ensure_styio_bin() {
  if [[ -x "$STYIO_BIN" ]]; then
    return 0
  fi
  echo "error: styio binary not found at $STYIO_BIN" >&2
  echo "build it first, for example:" >&2
  echo "  cmake -S . -B build -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm" >&2
  echo "  cmake --build build" >&2
  exit 1
}

validate_expr() {
  local expr="$1"

  if printf '%s\n' "$expr" | grep -Eq '^[0-9[:space:]+*/().-]+$'; then
    return 0
  fi

  echo "error: unsupported characters in expression: $expr" >&2
  echo "allowed: digits, spaces, (), ., +, -, *, /" >&2
  return 2
}

run_expr() {
  local expr="$1"
  local mode="${2:-raw}"
  local program_file="$TMP_DIR/program.styio"
  local output
  local status

  if ! validate_expr "$expr"; then
    return $?
  fi

  printf '>_(%s)\n' "$expr" > "$program_file"
  if ! output="$("$STYIO_BIN" --file "$program_file" 2>&1)"; then
    status=$?
    printf '%s\n' "$output" >&2
    return "$status"
  fi

  output="${output%$'\n'}"
  if [[ "$mode" == "repl" ]]; then
    printf '= %s\n' "$output"
  else
    printf '%s\n' "$output"
  fi

  return 0
}

repl() {
  local expr
  local status

  if [[ -t 0 ]]; then
    cat <<'EOF'
Styio Calculator REPL
Type an arithmetic expression, or `exit` to quit.
EOF
  fi

  while true; do
    if [[ -t 0 ]]; then
      printf 'styio-calc> '
    fi

    if ! IFS= read -r expr; then
      if [[ -t 0 ]]; then
        printf '\n'
      fi
      break
    fi

    if [[ -z "${expr//[[:space:]]/}" ]]; then
      continue
    fi

    case "$expr" in
      exit|quit|:q)
        break
        ;;
      help)
        usage
        continue
        ;;
    esac

    if ! run_expr "$expr" repl; then
      status=$?
      if [[ $status -ne 2 ]]; then
        echo "error: expression execution failed (exit $status)" >&2
      fi
    fi
  done
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

ensure_styio_bin

if [[ $# -eq 0 || "${1:-}" == "-i" || "${1:-}" == "--interactive" ]]; then
  repl
  exit 0
fi

run_expr "$*" raw
