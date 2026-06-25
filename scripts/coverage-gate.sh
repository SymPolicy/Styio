#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/coverage-gate.sh [options]

Build Styio with LLVM source-based coverage instrumentation, run the core test
labels, and fail when project source line coverage is below the threshold.

Options:
  --build-dir <dir>       Coverage build dir (default: build/coverage)
  --threshold <percent>   Minimum line coverage percentage (default: 95)
  --jobs <n>              Build parallelism (default: STYIO_COVERAGE_JOBS,
                          CMAKE_BUILD_PARALLEL_LEVEL, or memory-capped auto)
  --llvm-cov <path>       llvm-cov executable (default: llvm-cov-18/17/16/llvm-cov)
  --llvm-profdata <path>  llvm-profdata executable (default: llvm-profdata-18/17/16/llvm-profdata)
  --cc <path>             C compiler for coverage build (default: clang-18/17/16/clang)
  --cxx <path>            C++ compiler for coverage build (default: clang++-18/17/16/clang++)
  --label <ctest-label>   Add a CTest label to run; may be repeated
  --all-tests             Run all CTest tests instead of the default label set
  --include-soak          Add soak_smoke and soak_deep labels to the default set
  --include-performance   Add performance label to the default set
  --keep-profiles         Do not remove existing coverage profiles before running
  -h, --help              Show this help
USAGE
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

find_tool() {
  local explicit="$1"
  shift
  if [[ -n "$explicit" ]]; then
    command -v "$explicit"
    return
  fi
  local candidate
  for candidate in "$@"; do
    if command -v "$candidate" >/dev/null 2>&1; then
      command -v "$candidate"
      return
    fi
  done
  return 1
}

positive_integer() {
  [[ "$1" =~ ^[1-9][0-9]*$ ]]
}

detect_build_jobs() {
  local explicit="${STYIO_COVERAGE_JOBS:-${CMAKE_BUILD_PARALLEL_LEVEL:-}}"
  local cpu_jobs mem_kib mem_jobs jobs

  if [[ -n "$explicit" ]]; then
    if ! positive_integer "$explicit"; then
      echo "coverage gate failed: build jobs must be a positive integer: ${explicit}" >&2
      return 2
    fi
    printf '%s\n' "$explicit"
    return 0
  fi

  cpu_jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 2)"
  if ! positive_integer "$cpu_jobs"; then
    cpu_jobs=2
  fi

  mem_jobs=4
  if [[ -r /proc/meminfo ]]; then
    mem_kib="$(awk '/^MemTotal:/ { print $2; exit }' /proc/meminfo)"
    if positive_integer "${mem_kib:-}"; then
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

BUILD_DIR="build/coverage"
THRESHOLD="95"
JOBS=""
LLVM_COV="${LLVM_COV:-}"
LLVM_PROFDATA="${LLVM_PROFDATA:-}"
CC_BIN="${CC:-}"
CXX_BIN="${CXX:-}"
RUN_ALL=0
KEEP_PROFILES=0
INCLUDE_SOAK=0
INCLUDE_PERFORMANCE=0
LABELS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --threshold)
      THRESHOLD="$2"
      shift 2
      ;;
    --jobs)
      JOBS="$2"
      shift 2
      ;;
    --llvm-cov)
      LLVM_COV="$2"
      shift 2
      ;;
    --llvm-profdata)
      LLVM_PROFDATA="$2"
      shift 2
      ;;
    --cc)
      CC_BIN="$2"
      shift 2
      ;;
    --cxx)
      CXX_BIN="$2"
      shift 2
      ;;
    --label)
      LABELS+=("$2")
      shift 2
      ;;
    --all-tests)
      RUN_ALL=1
      shift
      ;;
    --include-soak)
      INCLUDE_SOAK=1
      shift
      ;;
    --include-performance)
      INCLUDE_PERFORMANCE=1
      shift
      ;;
    --keep-profiles)
      KEEP_PROFILES=1
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

if [[ -z "$JOBS" ]]; then
  JOBS="$(detect_build_jobs)"
elif ! positive_integer "$JOBS"; then
  echo "coverage gate failed: build jobs must be a positive integer: ${JOBS}" >&2
  exit 2
fi

LLVM_COV_BIN="$(find_tool "$LLVM_COV" llvm-cov-18 llvm-cov-17 llvm-cov-16 llvm-cov)" || {
  echo "coverage gate failed: llvm-cov not found; set LLVM_COV or pass --llvm-cov" >&2
  exit 2
}
LLVM_PROFDATA_BIN="$(find_tool "$LLVM_PROFDATA" llvm-profdata-18 llvm-profdata-17 llvm-profdata-16 llvm-profdata)" || {
  echo "coverage gate failed: llvm-profdata not found; set LLVM_PROFDATA or pass --llvm-profdata" >&2
  exit 2
}
CC_BIN="$(find_tool "$CC_BIN" clang-18 clang-17 clang-16 clang)" || {
  echo "coverage gate failed: clang C compiler not found; set CC or pass --cc" >&2
  exit 2
}
CXX_BIN="$(find_tool "$CXX_BIN" clang++-18 clang++-17 clang++-16 clang++)" || {
  echo "coverage gate failed: clang++ C++ compiler not found; set CXX or pass --cxx" >&2
  exit 2
}

case "$THRESHOLD" in
  ''|*[!0-9.]*)
    echo "coverage gate failed: --threshold must be a numeric percentage" >&2
    exit 2
    ;;
esac

if [[ "$RUN_ALL" -eq 0 && "${#LABELS[@]}" -eq 0 ]]; then
  LABELS=(
    language_feature
    styio_pipeline
    security
    resource_topology
    algorithm_equivalence
    ide
  )
  if [[ "$INCLUDE_SOAK" -eq 1 ]]; then
    LABELS+=(soak_smoke soak_deep)
  fi
  if [[ "$INCLUDE_PERFORMANCE" -eq 1 ]]; then
    LABELS+=(performance)
  fi
fi

echo "[coverage-gate] build dir: ${BUILD_DIR}"
echo "[coverage-gate] threshold: ${THRESHOLD}%"
echo "[coverage-gate] llvm-cov: ${LLVM_COV_BIN}"
echo "[coverage-gate] llvm-profdata: ${LLVM_PROFDATA_BIN}"
echo "[coverage-gate] compiler: ${CC_BIN} / ${CXX_BIN}"
echo "[coverage-gate] build jobs: ${JOBS}"

cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER="$CC_BIN" \
  -DCMAKE_CXX_COMPILER="$CXX_BIN" \
  -DSTYIO_ENABLE_COVERAGE=ON \
  -DSTYIO_BUILD_NANO=ON

BUILD_TARGETS=(
  styio
  styio_nano
  styio_test
  styio_security_test
  styio_typeinfer_internal_test
  styio_lowering_internal_test
  styio_newparser_internal_test
  styio_parser_internal_test
  styio_native_interop_internal_test
  styio_externlib_internal_test
  styio_codegen_internal_test
  styio_syntax_check_internal_test
  styio_main_contract_test
  styio_resource_topology_test
  styio_ide_test
  styio_algorithm_equivalence_test
)

if [[ "$RUN_ALL" -eq 1 || "$INCLUDE_SOAK" -eq 1 ]]; then
  BUILD_TARGETS+=(styio_soak_test)
fi
if [[ "$RUN_ALL" -eq 1 || "$INCLUDE_PERFORMANCE" -eq 1 ]]; then
  BUILD_TARGETS+=(styio_task_scheduler_perf_test)
fi

cmake --build "$BUILD_DIR" --parallel "$JOBS" --target "${BUILD_TARGETS[@]}"

PROFILE_DIR="$BUILD_DIR/coverage-profiles"
if [[ "$KEEP_PROFILES" -eq 0 ]]; then
  rm -rf "$PROFILE_DIR"
fi
mkdir -p "$PROFILE_DIR"

# Use LLVM's merge-pool pattern so repeated CLI subprocesses do not create one
# raw profile per process. This keeps coverage gates bounded in CI.
export LLVM_PROFILE_FILE="$PROFILE_DIR/%8m.profraw"

if [[ "$RUN_ALL" -eq 1 ]]; then
  echo "[coverage-gate] ctest: all tests"
  ctest --test-dir "$BUILD_DIR" --output-on-failure
else
  LABEL_REGEX="$(IFS='|'; echo "${LABELS[*]}")"
  echo "[coverage-gate] ctest labels: ${LABEL_REGEX}"
  ctest --test-dir "$BUILD_DIR" -L "$LABEL_REGEX" --output-on-failure
fi

mapfile -t PROFRAW_FILES < <(find "$PROFILE_DIR" -type f -name '*.profraw' | sort)
if [[ "${#PROFRAW_FILES[@]}" -eq 0 ]]; then
  echo "coverage gate failed: no .profraw files were produced" >&2
  exit 1
fi

PROFDATA="$BUILD_DIR/styio-coverage.profdata"
"$LLVM_PROFDATA_BIN" merge -sparse "${PROFRAW_FILES[@]}" -o "$PROFDATA"

COVERAGE_OBJECTS=()
for object in \
  "$BUILD_DIR/bin/styio" \
  "$BUILD_DIR/bin/styio-nano" \
  "$BUILD_DIR/bin/styio_test" \
  "$BUILD_DIR/bin/styio_security_test" \
  "$BUILD_DIR/bin/styio_typeinfer_internal_test" \
  "$BUILD_DIR/bin/styio_lowering_internal_test" \
  "$BUILD_DIR/bin/styio_newparser_internal_test" \
  "$BUILD_DIR/bin/styio_parser_internal_test" \
  "$BUILD_DIR/bin/styio_native_interop_internal_test" \
  "$BUILD_DIR/bin/styio_externlib_internal_test" \
  "$BUILD_DIR/bin/styio_codegen_internal_test" \
  "$BUILD_DIR/bin/styio_syntax_check_internal_test" \
  "$BUILD_DIR/bin/styio_main_contract_test" \
  "$BUILD_DIR/bin/styio_resource_topology_test" \
  "$BUILD_DIR/bin/styio_ide_test" \
  "$BUILD_DIR/bin/styio_algorithm_equivalence_test" \
  "$BUILD_DIR/bin/styio_soak_test" \
  "$BUILD_DIR/bin/styio_task_scheduler_perf_test"
do
  if [[ -x "$object" ]]; then
    COVERAGE_OBJECTS+=("-object=$object")
  fi
done

if [[ "${#COVERAGE_OBJECTS[@]}" -eq 0 ]]; then
  echo "coverage gate failed: no coverage objects found under ${BUILD_DIR}/bin" >&2
  exit 1
fi

IGNORE_REGEX='(^|/)(build|tests|benchmark|styio-benchmark|grammar/tree-sitter-styio)(/|$)|/src/include/'
SUMMARY_JSON="$BUILD_DIR/styio-coverage-summary.json"

"$LLVM_COV_BIN" report \
  -instr-profile="$PROFDATA" \
  "${COVERAGE_OBJECTS[@]}" \
  -ignore-filename-regex="$IGNORE_REGEX"

"$LLVM_COV_BIN" export \
  -summary-only \
  -format=text \
  -instr-profile="$PROFDATA" \
  "${COVERAGE_OBJECTS[@]}" \
  -ignore-filename-regex="$IGNORE_REGEX" \
  > "$SUMMARY_JSON"

python3 - "$SUMMARY_JSON" "$THRESHOLD" <<'PY'
import json
import sys

summary_path = sys.argv[1]
try:
    threshold = float(sys.argv[2])
except ValueError:
    print("coverage gate failed: --threshold must be a numeric percentage", file=sys.stderr)
    sys.exit(2)

with open(summary_path, "r", encoding="utf-8") as fh:
    data = json.load(fh)

percent = float(data["data"][0]["totals"]["lines"]["percent"])
print(f"[coverage-gate] line coverage: {percent:.2f}%", flush=True)
if percent + 1e-9 < threshold:
    print(
        f"coverage gate failed: line coverage {percent:.2f}% is below {threshold:.2f}%",
        file=sys.stderr,
    )
    sys.exit(1)
PY

echo "[coverage-gate] coverage threshold satisfied"
