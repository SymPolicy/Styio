#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/checkpoint-health.sh [options]

Options:
  --build-dir <dir>       CMake build dir for normal tests (default: build)
  --asan-build-dir <dir>  CMake build dir for ASan/UBSan tests (default: build-asan-ubsan)
  --no-asan               Skip ASan/UBSan verification
  -h, --help              Show this help
USAGE
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="build"
ASAN_BUILD_DIR="build-asan-ubsan"
RUN_ASAN=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --asan-build-dir)
      ASAN_BUILD_DIR="$2"
      shift 2
      ;;
    --no-asan)
      RUN_ASAN=0
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

echo "[checkpoint-health] build dir: ${BUILD_DIR}"
cmake -S . -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target styio_test styio_soak_test -j8

echo "[checkpoint-health] parser default + state inline diagnostics"
ctest --test-dir "$BUILD_DIR" \
  -R '^Styio(ParserEngine\.DefaultEngineIsNewInShadowArtifact|Diagnostics\.(SingleArgStateFunctionInliningUsesCallArgument|BlockStateFunctionInliningUsesCallArgument|StateInlineMatchCasesFunctionUsesCallArgument|StateInlineInfiniteLiteralFunctionUsesCallArgument))$' \
  --output-on-failure

echo "[checkpoint-health] soak smoke (state inline focus)"
ctest --test-dir "$BUILD_DIR" \
  -R '^StyioSoakSingleThread\.(StateInlineHelperProgramLoop|StateInlineMatchCasesProgramLoop|StateInlineInfiniteProgramLoop)$' \
  --output-on-failure

echo "[checkpoint-health] deep soak (state inline focus)"
ctest --test-dir "$BUILD_DIR" \
  -R '^soak_deep_state_inline_(program|matchcases_program|infinite_program)$' \
  --output-on-failure

echo "[checkpoint-health] pipeline + security labels"
ctest --test-dir "$BUILD_DIR" -L styio_pipeline --output-on-failure
ctest --test-dir "$BUILD_DIR" -L security --output-on-failure

if [[ "$RUN_ASAN" -eq 1 ]]; then
  echo "[checkpoint-health] asan build dir: ${ASAN_BUILD_DIR}"
  cmake --build "$ASAN_BUILD_DIR" --target styio_test -j8
  ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1' \
  UBSAN_OPTIONS='print_stacktrace=1:halt_on_error=1' \
  ctest --test-dir "$ASAN_BUILD_DIR" \
    -R '^StyioDiagnostics\.(StateInlineMatchCasesFunctionUsesCallArgument|StateInlineInfiniteLiteralFunctionUsesCallArgument)$' \
    --output-on-failure
fi

echo "[checkpoint-health] all checks passed"
