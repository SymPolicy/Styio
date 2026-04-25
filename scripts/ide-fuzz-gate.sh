#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/ide-fuzz-gate.sh [options]

Options:
  --build-dir <dir>   Fuzz build dir (default: build-fuzz)
  --skip-configure    Reuse the existing build dir without re-running CMake configure
  -h, --help          Show help

This gate configures a Clang/libFuzzer build, builds styio_fuzz_suite, and runs:
  ctest -L fuzz_smoke
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
    "$ROOT/build-codex/_deps/tree_sitter_runtime-src" \
    "$ROOT/build-ide-perf/_deps/tree_sitter_runtime-src"; do
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
    "$ROOT/build-codex/_deps/googletest-src" \
    "$ROOT/build-ide-perf/_deps/googletest-src"; do
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

find_llvm_prefix() {
  local candidate

  if [[ -n "${STYIO_LLVM_PREFIX:-}" ]] &&
     [[ -x "${STYIO_LLVM_PREFIX}/bin/clang++" ]] &&
     [[ -d "${STYIO_LLVM_PREFIX}/lib/cmake/llvm" ]]; then
    printf '%s\n' "${STYIO_LLVM_PREFIX}"
    return 0
  fi

  for candidate in \
    <homebrew-prefix>/opt/llvm@18 \
    <homebrew-prefix>/opt/llvm; do
    if [[ -x "$candidate/bin/clang++" && -d "$candidate/lib/cmake/llvm" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="build-fuzz"
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
  if ! LLVM_PREFIX="$(find_llvm_prefix)"; then
    echo "Unable to find a Clang/LLVM 18 toolchain for libFuzzer." >&2
    echo "Set STYIO_LLVM_PREFIX or re-run the aggregate gate with --skip-fuzz --waiver <reason>." >&2
    exit 2
  fi

  CMAKE_ARGS=(
    -DSTYIO_ENABLE_FUZZ=ON
    -DSTYIO_ENABLE_TREE_SITTER=ON
    "-DCMAKE_C_COMPILER=$LLVM_PREFIX/bin/clang"
    "-DCMAKE_CXX_COMPILER=$LLVM_PREFIX/bin/clang++"
    "-DLLVM_DIR=$LLVM_PREFIX/lib/cmake/llvm"
    "-DCMAKE_CXX_FLAGS=-stdlib=libc++"
    "-DCMAKE_EXE_LINKER_FLAGS=-stdlib=libc++"
  )
  if [[ "$(uname -s)" == "Darwin" ]] && command -v xcrun >/dev/null 2>&1; then
    CMAKE_ARGS+=("-DCMAKE_OSX_SYSROOT=$(xcrun --show-sdk-path)")
  fi

  PREFIX_PATHS=("$LLVM_PREFIX")
  if [[ -d <homebrew-prefix>/opt/icu4c@78 ]]; then
    PREFIX_PATHS+=(<homebrew-prefix>/opt/icu4c@78)
  elif [[ -d <homebrew-prefix>/opt/icu4c ]]; then
    PREFIX_PATHS+=(<homebrew-prefix>/opt/icu4c)
  fi
  if [[ "${#PREFIX_PATHS[@]}" -gt 0 ]]; then
    CMAKE_PREFIX_PATH_VALUE="$(IFS=';'; printf '%s' "${PREFIX_PATHS[*]}")"
    CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH_VALUE")
  fi

  if TREE_SITTER_RUNTIME_SOURCE="$(find_tree_sitter_runtime_source)"; then
    CMAKE_ARGS+=("-DFETCHCONTENT_SOURCE_DIR_TREE_SITTER_RUNTIME=$TREE_SITTER_RUNTIME_SOURCE")
  fi
  if GOOGLETEST_SOURCE="$(find_googletest_source)"; then
    CMAKE_ARGS+=("-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$GOOGLETEST_SOURCE")
  fi

  cmake -S . -B "$BUILD_DIR" "${CMAKE_ARGS[@]}"
fi

cmake --build "$BUILD_DIR" --target styio_fuzz_suite -j"$JOBS"
ctest --test-dir "$BUILD_DIR" -L fuzz_smoke --output-on-failure
