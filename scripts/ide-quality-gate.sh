#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/ide-quality-gate.sh [options]

Options:
  --build-dir <dir>        Main IDE build dir (default: build-codex)
  --perf-build-dir <dir>   Release perf build dir (default: build-ide-perf)
  --fuzz-build-dir <dir>   Fuzz build dir (default: build-fuzz)
  --skip-build             Skip rebuilding the main IDE targets
  --skip-perf              Skip the release perf gate (requires --waiver)
  --skip-fuzz              Skip fuzz smoke (requires --waiver)
  --waiver <reason>        Explicit waiver reason for a skipped frozen gate
  -h, --help               Show help

This gate runs:
  1. IDE/LSP regression suite
  2. docs-index/docs-audit/git diff hygiene
  3. Release perf gate
  4. libFuzzer smoke suite
USAGE
}

detect_jobs() {
  local jobs
  jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
  if [[ -z "$jobs" ]]; then
    jobs="$(sysctl -n hw.ncpu 2>/dev/null || true)"
  fi
  if [[ -z "$jobs" ]]; then
    jobs="8"
  fi
  printf '%s\n' "$jobs"
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="build-codex"
PERF_BUILD_DIR="build-ide-perf"
FUZZ_BUILD_DIR="build-fuzz"
SKIP_BUILD=0
SKIP_PERF=0
SKIP_FUZZ=0
WAIVER=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --perf-build-dir)
      PERF_BUILD_DIR="$2"
      shift 2
      ;;
    --fuzz-build-dir)
      FUZZ_BUILD_DIR="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --skip-perf)
      SKIP_PERF=1
      shift
      ;;
    --skip-fuzz)
      SKIP_FUZZ=1
      shift
      ;;
    --waiver)
      WAIVER="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if { [[ "$SKIP_PERF" -eq 1 ]] || [[ "$SKIP_FUZZ" -eq 1 ]]; } && [[ -z "$WAIVER" ]]; then
  echo "Skipping a frozen M19 gate requires --waiver <reason>." >&2
  exit 2
fi

cd "$ROOT"
JOBS="$(detect_jobs)"

if [[ "$SKIP_BUILD" -ne 1 ]]; then
  cmake --build "$BUILD_DIR" --target styio_ide_test styio_lspd -j"$JOBS"
fi

ctest --test-dir "$BUILD_DIR" --output-on-failure -L 'ide|lsp'
python3 scripts/docs-index.py --check
python3 scripts/docs-audit.py
git diff --check

if [[ "$SKIP_PERF" -ne 1 ]]; then
  bash scripts/ide-perf-gate.sh --build-dir "$PERF_BUILD_DIR"
else
  echo "[ide-quality-gate] perf gate waived: $WAIVER"
fi

if [[ "$SKIP_FUZZ" -ne 1 ]]; then
  bash scripts/ide-fuzz-gate.sh --build-dir "$FUZZ_BUILD_DIR"
else
  echo "[ide-quality-gate] fuzz gate waived: $WAIVER"
fi
