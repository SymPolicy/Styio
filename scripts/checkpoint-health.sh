#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/checkpoint-health.sh [options]

Options:
  --build-dir <dir>       CMake build dir for normal tests (default: build)
  --asan-build-dir <dir>  CMake build dir for ASan/UBSan tests (default: build-asan-ubsan)
  --fuzz-build-dir <dir>  CMake build dir for fuzz smoke (default: auto-detect build-fuzz)
  --no-asan               Skip ASan/UBSan verification
  --no-fuzz               Skip fuzz smoke verification
  -h, --help              Show this help
USAGE
}

fuzz_build_has_ctest_latest() {
  local dir="$1"
  [[ -f "$dir/CTestTestfile.cmake" || -f "$dir/tests/CTestTestfile.cmake" ]]
}

configure_build_dir_latest() {
  local requested="$1"
  local fallback="$2"

  if cmake -S . -B "$requested"; then
    printf '%s\n' "$requested"
    return 0
  fi

  if [[ "$requested" == "$fallback" ]]; then
    return 1
  fi

  echo "[checkpoint-health] build dir ${requested} unusable; falling back to ${fallback}" >&2
  cmake -S . -B "$fallback" >&2
  printf '%s\n' "$fallback"
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="build"
ASAN_BUILD_DIR="build-asan-ubsan"
FUZZ_BUILD_DIR="build-fuzz"
RUN_ASAN=1
RUN_FUZZ="auto"

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
    --fuzz-build-dir)
      FUZZ_BUILD_DIR="$2"
      RUN_FUZZ="1"
      shift 2
      ;;
    --no-asan)
      RUN_ASAN=0
      shift
      ;;
    --no-fuzz)
      RUN_FUZZ="0"
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

BUILD_DIR="$(configure_build_dir_latest "$BUILD_DIR" "build-codex")"
echo "[checkpoint-health] build dir: ${BUILD_DIR}"
cmake --build "$BUILD_DIR" --target styio_test styio_soak_test -j8

echo "[checkpoint-health] docs audit"
ctest --test-dir "$BUILD_DIR" -L docs --output-on-failure

echo "[checkpoint-health] parser default + state inline diagnostics"
ctest --test-dir "$BUILD_DIR" \
  -R '^Styio(ParserEngine\.DefaultEngineIsNightlyInShadowArtifact|Diagnostics\.(SingleArgStateFunctionInliningUsesCallArgument|BlockStateFunctionInliningUsesCallArgument|StateInlineMatchCasesFunctionUsesCallArgument|StateInlineInfiniteLiteralFunctionUsesCallArgument))$' \
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

echo "[checkpoint-health] parser legacy entry audit"
ctest --test-dir "$BUILD_DIR" \
  -R '^parser_legacy_entry_audit$' \
  --output-on-failure

echo "[checkpoint-health] m1/m2 parser shadow dual-zero gates"
ctest --test-dir "$BUILD_DIR" \
  -R '^parser_shadow_gate_m(1|2)_zero_fallback_and_internal_bridges$' \
  --output-on-failure

echo "[checkpoint-health] m5 parser shadow dual-zero gate with expected nonzero manifest"
ctest --test-dir "$BUILD_DIR" \
  -R '^parser_shadow_gate_m5_dual_zero_expected_nonzero$' \
  --output-on-failure

echo "[checkpoint-health] m7 parser shadow zero-fallback gate"
ctest --test-dir "$BUILD_DIR" \
  -R '^parser_shadow_gate_m7_zero_fallback$' \
  --output-on-failure

echo "[checkpoint-health] m7 parser shadow zero-internal-bridges gate"
ctest --test-dir "$BUILD_DIR" \
  -R '^parser_shadow_gate_m7_zero_internal_bridges$' \
  --output-on-failure

if [[ "$RUN_FUZZ" == "1" ]]; then
  echo "[checkpoint-health] fuzz build dir: ${FUZZ_BUILD_DIR}"
  cmake --build "$FUZZ_BUILD_DIR" --target styio_fuzz_lexer styio_fuzz_parser -j8
  echo "[checkpoint-health] fuzz smoke"
  ctest --test-dir "$FUZZ_BUILD_DIR" -L fuzz_smoke --output-on-failure
elif [[ "$RUN_FUZZ" == "auto" ]]; then
  if fuzz_build_has_ctest_latest "$FUZZ_BUILD_DIR"; then
    echo "[checkpoint-health] fuzz build dir: ${FUZZ_BUILD_DIR}"
    cmake --build "$FUZZ_BUILD_DIR" --target styio_fuzz_lexer styio_fuzz_parser -j8
    echo "[checkpoint-health] fuzz smoke"
    ctest --test-dir "$FUZZ_BUILD_DIR" -L fuzz_smoke --output-on-failure
  else
    echo "[checkpoint-health] fuzz smoke skipped (no fuzz build detected at ${FUZZ_BUILD_DIR})"
  fi
fi

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
