#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/checkpoint-health.sh [options]

Options:
  --build-dir <dir>       CMake build dir for normal tests (default: build/default)
  --asan-build-dir <dir>  CMake build dir for ASan/UBSan tests (default: build/asan-ubsan)
  --fuzz-build-dir <dir>  CMake build dir for fuzz smoke (default: auto-detect build/fuzz)
  --coverage-build-dir <dir>
                           CMake build dir for coverage gate (default: build/coverage)
  --coverage-threshold <percent>
                           Minimum line coverage percentage (default: 95)
  --build-jobs <jobs>      Build parallelism for all cmake --build calls
                           (default: STYIO_CHECKPOINT_BUILD_JOBS,
                           CMAKE_BUILD_PARALLEL_LEVEL, or memory-capped auto)
  --no-asan               Skip ASan/UBSan verification
  --no-fuzz               Skip fuzz smoke verification
  -h, --help              Show this help
USAGE
}

positive_integer_latest() {
  [[ "$1" =~ ^[1-9][0-9]*$ ]]
}

detect_build_jobs_latest() {
  local explicit="${STYIO_CHECKPOINT_BUILD_JOBS:-${CMAKE_BUILD_PARALLEL_LEVEL:-}}"
  local cpu_jobs mem_kib mem_jobs jobs

  if [[ -n "$explicit" ]]; then
    if ! positive_integer_latest "$explicit"; then
      echo "Build jobs must be a positive integer: ${explicit}" >&2
      return 2
    fi
    printf '%s\n' "$explicit"
    return 0
  fi

  cpu_jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 2)"
  if ! positive_integer_latest "$cpu_jobs"; then
    cpu_jobs=2
  fi

  # Styio's LLVM-heavy C++ targets can exceed worker memory when all cores build
  # at once. Use total RAM for a stable cold-start cap and keep an env override.
  mem_jobs=4
  if [[ -r /proc/meminfo ]]; then
    mem_kib="$(awk '/^MemTotal:/ { print $2; exit }' /proc/meminfo)"
    if positive_integer_latest "${mem_kib:-}"; then
      mem_jobs=$((mem_kib / 3145728))
      if (( mem_jobs < 1 )); then
        mem_jobs=1
      fi
    fi
  fi

  jobs="$cpu_jobs"
  if (( jobs > mem_jobs )); then
    jobs="$mem_jobs"
  fi
  if (( jobs > 4 )); then
    jobs=4
  fi
  if (( jobs < 1 )); then
    jobs=1
  fi
  printf '%s\n' "$jobs"
}

cmake_build_latest() {
  local dir="$1"
  shift
  cmake --build "$dir" --parallel "$BUILD_JOBS" --target "$@"
}

fuzz_build_has_ctest_latest() {
  local dir="$1"
  [[ -f "$dir/CTestTestfile.cmake" || -f "$dir/tests/CTestTestfile.cmake" ]]
}

cmake_target_exists_latest() {
  local dir="$1"
  local target="$2"
  cmake --build "$dir" --target help 2>/dev/null \
    | grep -Eq "(^|[[:space:]])${target}([[:space:]:]|$)"
}

configure_build_dir_latest() {
  local requested="$1"
  local fallback="$2"

  if cmake -S . -B "$requested" >&2; then
    printf '%s\n' "$requested"
    return 0
  fi

  if [[ "$requested" == "$fallback" ]]; then
    return 1
  fi

  echo "[checkpoint-health] build dir ${requested} unusable; falling back to ${fallback}" >&2
  if ! cmake -S . -B "$fallback" >&2; then
    return 1
  fi
  printf '%s\n' "$fallback"
}

configure_asan_build_dir_latest() {
  local requested="$1"
  if [[ -f "$requested/CMakeCache.txt" ]]; then
    printf '%s\n' "$requested"
    return 0
  fi

  cmake -S . -B "$requested" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
    -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' >&2
  printf '%s\n' "$requested"
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="build/default"
ASAN_BUILD_DIR="build/asan-ubsan"
FUZZ_BUILD_DIR="build/fuzz"
COVERAGE_BUILD_DIR="build/coverage"
COVERAGE_THRESHOLD="95"
BUILD_JOBS=""
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
    --coverage-build-dir)
      COVERAGE_BUILD_DIR="$2"
      shift 2
      ;;
    --coverage-threshold)
      COVERAGE_THRESHOLD="$2"
      shift 2
      ;;
    --build-jobs)
      STYIO_CHECKPOINT_BUILD_JOBS="$2"
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

BUILD_JOBS="$(detect_build_jobs_latest)"
echo "[checkpoint-health] pre-test cutover self-check: for functional changes, read workflows/FEATURE-CUTOVER-WORKFLOW.md before treating final tests as complete."
echo "[checkpoint-health] commit readiness self-check: before commit or handoff, read workflows/FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md and verify targeted feature behavior plus upstream/downstream surfaces, check version-style naming against the feature or transformation result, or record objective unable-to-verify blockers."
echo "[checkpoint-health] build jobs: ${BUILD_JOBS}"
BUILD_DIR="$(configure_build_dir_latest "$BUILD_DIR" "build/default")"
echo "[checkpoint-health] build dir: ${BUILD_DIR}"
cmake_build_latest "$BUILD_DIR" styio_test styio_security_test styio_ide_test

HAS_SOAK_TARGET=0
if cmake_target_exists_latest "$BUILD_DIR" "styio_soak_test"; then
  HAS_SOAK_TARGET=1
  cmake_build_latest "$BUILD_DIR" styio_soak_test
else
  echo "[checkpoint-health] external soak target skipped (styio_soak_test is not registered)"
fi

echo "[checkpoint-health] docs audit"
ctest --test-dir "$BUILD_DIR" -L docs --output-on-failure

echo "[checkpoint-health] core benchmark smoke"
ctest --test-dir "$BUILD_DIR" \
  -R '^styio_core_benchmark_smoke$' \
  --output-on-failure

echo "[checkpoint-health] parser default + state inline diagnostics"
ctest --test-dir "$BUILD_DIR" \
  -R '^Styio(ParserEngine\.DefaultEngineIsNightlyInShadowArtifact|Diagnostics\.(SingleArgStateFunctionInliningUsesCallArgument|BlockStateFunctionInliningUsesCallArgument|StateInlineMatchCasesFunctionUsesCallArgument|StateInlineInfiniteLiteralFunctionUsesCallArgument))$' \
  --output-on-failure

if [[ "$HAS_SOAK_TARGET" -eq 1 ]]; then
  echo "[checkpoint-health] soak smoke (state inline focus)"
  ctest --test-dir "$BUILD_DIR" \
    -R '^StyioSoakSingleThread\.(StateInlineHelperProgramLoop|StateInlineMatchCasesProgramLoop|StateInlineInfiniteProgramLoop)$' \
    --output-on-failure

  echo "[checkpoint-health] deep soak (state inline focus)"
  ctest --test-dir "$BUILD_DIR" \
    -R '^soak_deep_state_inline_(program|matchcases_program|infinite_program)$' \
    --output-on-failure
else
  echo "[checkpoint-health] soak smoke skipped (external styio-benchmark probes unavailable)"
fi

echo "[checkpoint-health] pipeline + security labels"
ctest --test-dir "$BUILD_DIR" -L styio_pipeline --output-on-failure
ctest --test-dir "$BUILD_DIR" -L security --output-on-failure

echo "[checkpoint-health] IDE/LSP runtime scheduling"
ctest --test-dir "$BUILD_DIR" \
  -R '^StyioLsp(Server|Runtime)\.(RunDrainsRuntimeDiagnostics|RuntimeDrainCanBeBudgetedForScheduling)$' \
  --output-on-failure

echo "[checkpoint-health] parser legacy entry audit"
ctest --test-dir "$BUILD_DIR" \
  -R '^parser_legacy_entry_audit$' \
  --output-on-failure

echo "[checkpoint-health] scalar/functions parser shadow dual-zero gates"
ctest --test-dir "$BUILD_DIR" \
  -R '^parser_shadow_gate_(scalar_expressions|functions)_zero_fallback_and_internal_bridges$' \
  --output-on-failure

echo "[checkpoint-health] file resource parser shadow dual-zero gate with expected nonzero manifest"
ctest --test-dir "$BUILD_DIR" \
  -R '^parser_shadow_gate_file_resources_dual_zero_expected_nonzero$' \
  --output-on-failure

echo "[checkpoint-health] stream processing parser shadow zero-fallback gate"
ctest --test-dir "$BUILD_DIR" \
  -R '^parser_shadow_gate_stream_processing_zero_fallback$' \
  --output-on-failure

echo "[checkpoint-health] stream processing parser shadow zero-internal-bridges gate"
ctest --test-dir "$BUILD_DIR" \
  -R '^parser_shadow_gate_stream_processing_zero_internal_bridges$' \
  --output-on-failure

echo "[checkpoint-health] coverage gate"
scripts/coverage-gate.sh \
  --build-dir "$COVERAGE_BUILD_DIR" \
  --threshold "$COVERAGE_THRESHOLD" \
  --jobs "$BUILD_JOBS"

if [[ "$RUN_FUZZ" == "1" ]]; then
  echo "[checkpoint-health] fuzz build dir: ${FUZZ_BUILD_DIR}"
  cmake_build_latest "$FUZZ_BUILD_DIR" styio_fuzz_suite
  echo "[checkpoint-health] fuzz smoke"
  ctest --test-dir "$FUZZ_BUILD_DIR" -L fuzz_smoke --output-on-failure
elif [[ "$RUN_FUZZ" == "auto" ]]; then
  if fuzz_build_has_ctest_latest "$FUZZ_BUILD_DIR"; then
    echo "[checkpoint-health] fuzz build dir: ${FUZZ_BUILD_DIR}"
    cmake_build_latest "$FUZZ_BUILD_DIR" styio_fuzz_suite
    echo "[checkpoint-health] fuzz smoke"
    ctest --test-dir "$FUZZ_BUILD_DIR" -L fuzz_smoke --output-on-failure
  else
    echo "[checkpoint-health] fuzz smoke skipped (no fuzz build detected at ${FUZZ_BUILD_DIR})"
  fi
fi

if [[ "$RUN_ASAN" -eq 1 ]]; then
  ASAN_BUILD_DIR="$(configure_asan_build_dir_latest "$ASAN_BUILD_DIR")"
  echo "[checkpoint-health] asan build dir: ${ASAN_BUILD_DIR}"
  cmake_build_latest "$ASAN_BUILD_DIR" styio_test
  ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1' \
  UBSAN_OPTIONS='print_stacktrace=1:halt_on_error=1' \
  ctest --test-dir "$ASAN_BUILD_DIR" \
    -R '^StyioDiagnostics\.(StateInlineMatchCasesFunctionUsesCallArgument|StateInlineInfiniteLiteralFunctionUsesCallArgument)$' \
    --output-on-failure
fi

echo "[checkpoint-health] all checks passed"
