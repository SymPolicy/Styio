#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/ide-perf-gate.sh [options]

Options:
  --build-dir <dir>   Release perf build dir (default: build/ide-perf)
  --skip-configure    Reuse the existing build dir without re-running CMake configure
  -h, --help          Show help

This gate configures a Release build, builds styio_ide_test, and runs:
  StyioIdePerf.EnforcesFrozenLatencyBudgets
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

find_tree_sitter_runtime_source() {
  local candidate

  if [[ -n "${STYIO_TREE_SITTER_RUNTIME_SOURCE:-}" ]] &&
     [[ -d "${STYIO_TREE_SITTER_RUNTIME_SOURCE}/lib/include" ]]; then
    printf '%s\n' "${STYIO_TREE_SITTER_RUNTIME_SOURCE}"
    return 0
  fi

  for candidate in \
    "$ROOT/$BUILD_DIR/_deps/tree_sitter_runtime-src" \
    "$ROOT/build/default/_deps/tree_sitter_runtime-src" \
    "$ROOT/build/fuzz/_deps/tree_sitter_runtime-src"; do
    if [[ -d "$candidate/lib/include" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  candidate="$(
    find "$ROOT" -path '*/_deps/tree_sitter_runtime-src' -type d -print 2>/dev/null |
      head -n 1
  )"
  if [[ -n "$candidate" && -d "$candidate/lib/include" ]]; then
    printf '%s\n' "$candidate"
    return 0
  fi

  return 1
}

find_googletest_source() {
  local candidate

  if [[ -n "${STYIO_GOOGLETEST_SOURCE:-}" ]] &&
     [[ -f "${STYIO_GOOGLETEST_SOURCE}/CMakeLists.txt" ]]; then
    printf '%s\n' "${STYIO_GOOGLETEST_SOURCE}"
    return 0
  fi

  for candidate in \
    "$ROOT/$BUILD_DIR/_deps/googletest-src" \
    "$ROOT/build/default/_deps/googletest-src" \
    "$ROOT/build/fuzz/_deps/googletest-src"; do
    if [[ -f "$candidate/CMakeLists.txt" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  candidate="$(
    find "$ROOT" -path '*/_deps/googletest-src' -type d -print 2>/dev/null |
      head -n 1
  )"
  if [[ -n "$candidate" && -f "$candidate/CMakeLists.txt" ]]; then
    printf '%s\n' "$candidate"
    return 0
  fi

  return 1
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="build/ide-perf"
SKIP_CONFIGURE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --skip-configure)
      SKIP_CONFIGURE=1
      shift
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

cd "$ROOT"
JOBS="$(detect_jobs)"

if [[ "$SKIP_CONFIGURE" -ne 1 ]]; then
  CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE=Release
    -DSTYIO_ENABLE_TREE_SITTER=ON
  )
  if TREE_SITTER_RUNTIME_SOURCE="$(find_tree_sitter_runtime_source)"; then
    CMAKE_ARGS+=("-DFETCHCONTENT_SOURCE_DIR_TREE_SITTER_RUNTIME=$TREE_SITTER_RUNTIME_SOURCE")
  fi
  if GOOGLETEST_SOURCE="$(find_googletest_source)"; then
    CMAKE_ARGS+=("-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$GOOGLETEST_SOURCE")
  fi
  cmake -S . -B "$BUILD_DIR" "${CMAKE_ARGS[@]}"
fi

cmake --build "$BUILD_DIR" --target styio_ide_test -j"$JOBS"
"$BUILD_DIR/bin/styio_ide_test" --gtest_filter='StyioIdePerf.EnforcesFrozenLatencyBudgets'
