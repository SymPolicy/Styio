#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: benchmark/perf-route.sh [options]

Options:
  --build-dir <dir>     CMake build dir (default: build)
  --phase-iters <n>     Compiler stage benchmark iterations (default: 5000)
  --micro-iters <n>     Compiler micro benchmark iterations (default: phase-iters, 0 to skip)
  --execute-iters <n>   Full-stack benchmark iterations (default: derived from phase-iters, 0 to skip)
  --skip-build          Skip cmake --build
  --deep-soak           Run soak_deep in addition to soak_smoke
  --quick               Skip deep parser shadow gates and soak_deep
  -h, --help            Show help

This script runs the current parser/compiler performance route:
  1. Build perf-related binaries
  2. Compiler stage benchmark matrix
  3. Compiler micro benchmark matrix
  4. Full-stack workload matrix
  5. Parser engine regression suite
  6. Parser/security/pipeline guard rails
  7. Parser shadow gates
  8. Soak smoke, and optionally soak_deep
USAGE
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="build"
PHASE_ITERS="5000"
MICRO_ITERS=""
EXECUTE_ITERS=""
SKIP_BUILD=0
RUN_DEEP_SOAK=0
QUICK_MODE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --phase-iters)
      PHASE_ITERS="$2"
      shift 2
      ;;
    --micro-iters)
      MICRO_ITERS="$2"
      shift 2
      ;;
    --execute-iters)
      EXECUTE_ITERS="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --deep-soak)
      RUN_DEEP_SOAK=1
      shift
      ;;
    --quick)
      QUICK_MODE=1
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

if ! [[ "$PHASE_ITERS" =~ ^[0-9]+$ ]]; then
  echo "--phase-iters must be an integer" >&2
  exit 2
fi

if [[ -z "$MICRO_ITERS" ]]; then
  MICRO_ITERS="$PHASE_ITERS"
fi

if ! [[ "$MICRO_ITERS" =~ ^[0-9]+$ ]]; then
  echo "--micro-iters must be an integer" >&2
  exit 2
fi

if [[ -z "$EXECUTE_ITERS" ]]; then
  EXECUTE_ITERS=$(( PHASE_ITERS / 250 ))
  if (( EXECUTE_ITERS < 5 )); then
    EXECUTE_ITERS=5
  fi
  if (( EXECUTE_ITERS > 50 )); then
    EXECUTE_ITERS=50
  fi
fi

if ! [[ "$EXECUTE_ITERS" =~ ^[0-9]+$ ]]; then
  echo "--execute-iters must be an integer" >&2
  exit 2
fi

section() {
  echo
  echo "[perf-route] $1"
}

if [[ "$SKIP_BUILD" -eq 0 ]]; then
  section "build (${BUILD_DIR})"
  cmake --build "$BUILD_DIR" --target styio styio_test styio_security_test styio_soak_test -j8
fi

section "compiler stage benchmark"
STYIO_SOAK_PHASE_BENCH_ITERS="$PHASE_ITERS" \
  "$ROOT/${BUILD_DIR}/bin/styio_soak_test" \
  --gtest_filter=StyioSoakSingleThread.FrontendPhaseBreakdownReport

if [[ "$MICRO_ITERS" -gt 0 ]]; then
  section "compiler micro benchmark"
  STYIO_SOAK_MICRO_BENCH_ITERS="$MICRO_ITERS" \
    "$ROOT/${BUILD_DIR}/bin/styio_soak_test" \
    --gtest_filter=StyioSoakSingleThread.CompilerMicroBenchmarksReport
fi

if [[ "$EXECUTE_ITERS" -gt 0 ]]; then
  section "full-stack workload matrix"
  STYIO_SOAK_EXECUTE_BENCH_ITERS="$EXECUTE_ITERS" \
    "$ROOT/${BUILD_DIR}/bin/styio_soak_test" \
    --gtest_filter=StyioSoakSingleThread.FullStackWorkloadMatrixReport
fi

section "parser engine suite"
ctest --test-dir "$BUILD_DIR" --output-on-failure -R '^StyioParserEngine\.'

section "pipeline guard rail"
ctest --test-dir "$BUILD_DIR" --output-on-failure \
  -R '^StyioFiveLayerPipeline\.(P05_snapshot_accum|P09_full_pipeline|P13_stdin_transform|P14_stdin_pull|P15_stdin_mixed_output)$'

section "parser/security guard rail"
ctest --test-dir "$BUILD_DIR" --output-on-failure \
  -R '^StyioSecurity(NightlyParserStmt|NightlyParserExpr|ParserContext)\.'

if [[ "$QUICK_MODE" -eq 0 ]]; then
  section "parser shadow gates"
  ctest --test-dir "$BUILD_DIR" --output-on-failure \
    -R '^parser_shadow_gate_m(1|2)_zero_fallback_and_internal_bridges$|^parser_shadow_gate_m5_dual_zero_expected_nonzero$|^parser_shadow_gate_m7_zero_fallback$|^parser_shadow_gate_m7_zero_internal_bridges$'
fi

section "soak smoke"
ctest --test-dir "$BUILD_DIR" --output-on-failure -L soak_smoke

if [[ "$RUN_DEEP_SOAK" -eq 1 && "$QUICK_MODE" -eq 0 ]]; then
  section "soak deep"
  ctest --test-dir "$BUILD_DIR" --output-on-failure -L soak_deep
fi

section "done"
