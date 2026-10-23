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
  --llvm-prefix <dir>     LLVM 18.1.x prefix containing bin/ and lib/cmake/llvm
                          (default: LLVM_PREFIX/LLVM_ROOT, Homebrew llvm@18,
                          llvm-config-18, or a supported llvm-config)
  --llvm-cov <path>       Explicit llvm-cov from the selected LLVM 18.1.x bin dir
  --llvm-profdata <path>  Explicit llvm-profdata from the selected LLVM 18.1.x bin dir
  --cc <path>             Explicit clang from the selected LLVM 18.1.x bin dir
  --cxx <path>            Explicit clang++ from the selected LLVM 18.1.x bin dir
  --label <ctest-label>   Add a CTest label to run; may be repeated
  --all-tests             Run all CTest tests instead of the default label set
  --include-soak          Add soak_smoke and soak_deep labels to the default set
  --include-performance   Add performance label to the default set
  --check-toolchain       Validate the unified LLVM toolchain, then exit
  --keep-profiles         Do not remove existing coverage profiles before running
  -h, --help              Show this help
USAGE
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

resolve_executable() {
  local requested="$1"
  local resolved
  resolved="$(command -v "$requested" 2>/dev/null)" || return 1
  local resolved_dir
  resolved_dir="$(cd "$(dirname "$resolved")" && pwd -P)" || return 1
  printf '%s/%s\n' "$resolved_dir" "$(basename "$resolved")"
}

executable_bin_dir() {
  local executable="$1"
  local resolved
  resolved="$(resolve_executable "$executable")" || return 1
  dirname "$resolved"
}

llvm_version_is_supported() {
  [[ "$1" =~ ^18\.1\.[0-9]+([-.][0-9A-Za-z.]+)?$ ]]
}

extract_llvm_version() {
  local executable="$1"
  local output
  output="$("$executable" --version 2>/dev/null)" || return 1
  if [[ "$output" =~ ([0-9]+\.[0-9]+\.[0-9]+([-.][0-9A-Za-z.]+)?) ]]; then
    printf '%s\n' "${BASH_REMATCH[1]}"
    return 0
  fi
  return 1
}

find_tool_in_bin() {
  local bin_dir="$1"
  local explicit="$2"
  shift 2

  if [[ -n "$explicit" ]]; then
    local resolved explicit_bin_dir
    resolved="$(resolve_executable "$explicit")" || return 1
    explicit_bin_dir="$(dirname "$resolved")"
    if [[ "$explicit_bin_dir" != "$bin_dir" ]]; then
      return 1
    fi
    printf '%s\n' "$resolved"
    return 0
  fi

  local candidate
  for candidate in "$@"; do
    if [[ -x "$bin_dir/$candidate" ]]; then
      printf '%s\n' "$bin_dir/$candidate"
      return
    fi
  done
  return 1
}

llvm_bin_has_supported_toolchain() {
  local bin_dir="$1"
  local llvm_config
  llvm_config="$(find_tool_in_bin "$bin_dir" "" llvm-config-18 llvm-config)" || return 1
  local version
  version="$("$llvm_config" --version 2>/dev/null)" || return 1
  llvm_version_is_supported "$version" || return 1
  find_tool_in_bin "$bin_dir" "" clang-18 clang >/dev/null || return 1
  find_tool_in_bin "$bin_dir" "" clang++-18 clang++ >/dev/null || return 1
  find_tool_in_bin "$bin_dir" "" llvm-cov-18 llvm-cov >/dev/null || return 1
  find_tool_in_bin "$bin_dir" "" llvm-profdata-18 llvm-profdata >/dev/null || return 1
}

add_llvm_bin_candidate() {
  local candidate="$1"
  if [[ -z "$candidate" || ! -d "$candidate" ]]; then
    return 0
  fi
  local normalized
  normalized="$(cd "$candidate" && pwd -P)" || return 0
  LLVM_BIN_CANDIDATES+=("$normalized")
}

positive_integer() {
  [[ "$1" =~ ^[1-9][0-9]*$ ]]
}

detect_build_jobs() {
  local explicit="${STYIO_COVERAGE_JOBS:-${CMAKE_BUILD_PARALLEL_LEVEL:-}}"
  local cpu_jobs mem_kib mem_bytes mem_jobs jobs

  if [[ -n "$explicit" ]]; then
    if ! positive_integer "$explicit"; then
      echo "coverage gate failed: build jobs must be a positive integer: ${explicit}" >&2
      return 2
    fi
    printf '%s\n' "$explicit"
    return 0
  fi

  if [[ "$(uname -s)" == "Darwin" ]]; then
    cpu_jobs="$(sysctl -n hw.logicalcpu 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)"
  else
    cpu_jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 2)"
  fi
  if ! positive_integer "$cpu_jobs"; then
    cpu_jobs=2
  fi

  mem_jobs=4
  if [[ "$(uname -s)" == "Darwin" ]]; then
    mem_bytes="$(sysctl -n hw.memsize 2>/dev/null || true)"
    if positive_integer "${mem_bytes:-}"; then
      mem_jobs=$((mem_bytes / 3221225472))
      if (( mem_jobs < 1 )); then
        mem_jobs=1
      fi
    fi
  elif [[ -r /proc/meminfo ]]; then
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
LLVM_PREFIX="${LLVM_PREFIX:-${LLVM_ROOT:-}}"
LLVM_COV="${LLVM_COV:-}"
LLVM_PROFDATA="${LLVM_PROFDATA:-}"
CC_BIN="${CC:-}"
CXX_BIN="${CXX:-}"
RUN_ALL=0
KEEP_PROFILES=0
INCLUDE_SOAK=0
INCLUDE_PERFORMANCE=0
CHECK_TOOLCHAIN=0
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
    --llvm-prefix)
      LLVM_PREFIX="$2"
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
    --check-toolchain)
      CHECK_TOOLCHAIN=1
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

if [[ "$BUILD_DIR" != /* ]]; then
  BUILD_DIR="$ROOT/$BUILD_DIR"
fi

if [[ -z "$JOBS" ]]; then
  JOBS="$(detect_build_jobs)"
elif ! positive_integer "$JOBS"; then
  echo "coverage gate failed: build jobs must be a positive integer: ${JOBS}" >&2
  exit 2
fi

LLVM_BIN_CANDIDATES=()
if [[ -n "$LLVM_PREFIX" ]]; then
  add_llvm_bin_candidate "$LLVM_PREFIX/bin"
fi

EXPLICIT_TOOL_BIN=""
for explicit_tool in "$CC_BIN" "$CXX_BIN" "$LLVM_COV" "$LLVM_PROFDATA"; do
  if [[ -z "$explicit_tool" ]]; then
    continue
  fi
  explicit_bin="$(executable_bin_dir "$explicit_tool")" || {
    echo "coverage gate failed: explicit LLVM tool not found: ${explicit_tool}" >&2
    exit 2
  }
  if [[ -n "$EXPLICIT_TOOL_BIN" && "$explicit_bin" != "$EXPLICIT_TOOL_BIN" ]]; then
    echo "coverage gate failed: all explicit LLVM tools must come from one bin directory" >&2
    exit 2
  fi
  EXPLICIT_TOOL_BIN="$explicit_bin"
done
add_llvm_bin_candidate "$EXPLICIT_TOOL_BIN"

if command -v brew >/dev/null 2>&1; then
  HOMEBREW_LLVM_PREFIX="$(brew --prefix llvm@18 2>/dev/null || true)"
  if [[ -n "$HOMEBREW_LLVM_PREFIX" ]]; then
    add_llvm_bin_candidate "$HOMEBREW_LLVM_PREFIX/bin"
  fi
fi
for llvm_config_name in llvm-config-18 llvm-config; do
  if command -v "$llvm_config_name" >/dev/null 2>&1; then
    llvm_config_path="$(resolve_executable "$llvm_config_name")"
    llvm_config_version="$("$llvm_config_path" --version 2>/dev/null || true)"
    if llvm_version_is_supported "$llvm_config_version"; then
      add_llvm_bin_candidate "$(dirname "$llvm_config_path")"
    fi
  fi
done
if command -v clang-18 >/dev/null 2>&1; then
  add_llvm_bin_candidate "$(executable_bin_dir clang-18)"
fi
add_llvm_bin_candidate "/usr/lib/llvm-18/bin"

LLVM_BIN_DIR=""
for llvm_bin_candidate in "${LLVM_BIN_CANDIDATES[@]}"; do
  if llvm_bin_has_supported_toolchain "$llvm_bin_candidate"; then
    LLVM_BIN_DIR="$llvm_bin_candidate"
    break
  fi
done
if [[ -z "$LLVM_BIN_DIR" ]]; then
  echo "coverage gate failed: a complete LLVM 18.1.x toolchain was not found; pass --llvm-prefix" >&2
  exit 2
fi
if [[ -n "$EXPLICIT_TOOL_BIN" && "$LLVM_BIN_DIR" != "$EXPLICIT_TOOL_BIN" ]]; then
  echo "coverage gate failed: explicit tools do not belong to the selected LLVM 18.1.x toolchain" >&2
  exit 2
fi

LLVM_CONFIG_BIN="$(find_tool_in_bin "$LLVM_BIN_DIR" "" llvm-config-18 llvm-config)" || {
  echo "coverage gate failed: llvm-config is missing from the selected LLVM toolchain" >&2
  exit 2
}
CC_BIN="$(find_tool_in_bin "$LLVM_BIN_DIR" "$CC_BIN" clang-18 clang)" || {
  echo "coverage gate failed: clang is missing from the selected LLVM toolchain" >&2
  exit 2
}
CXX_BIN="$(find_tool_in_bin "$LLVM_BIN_DIR" "$CXX_BIN" clang++-18 clang++)" || {
  echo "coverage gate failed: clang++ is missing from the selected LLVM toolchain" >&2
  exit 2
}
LLVM_COV_BIN="$(find_tool_in_bin "$LLVM_BIN_DIR" "$LLVM_COV" llvm-cov-18 llvm-cov)" || {
  echo "coverage gate failed: llvm-cov is missing from the selected LLVM toolchain" >&2
  exit 2
}
LLVM_PROFDATA_BIN="$(find_tool_in_bin "$LLVM_BIN_DIR" "$LLVM_PROFDATA" llvm-profdata-18 llvm-profdata)" || {
  echo "coverage gate failed: llvm-profdata is missing from the selected LLVM toolchain" >&2
  exit 2
}

for selected_compiler in "$CC_BIN" "$CXX_BIN"; do
  CLANG_VERSION_OUTPUT="$("$selected_compiler" --version 2>/dev/null)" || {
    echo "coverage gate failed: selected Clang compiler cannot report its version" >&2
    exit 2
  }
  if [[ "$CLANG_VERSION_OUTPUT" == *"Apple clang"* ]]; then
    echo "coverage gate failed: AppleClang is unsupported; use upstream LLVM 18.1.x" >&2
    exit 2
  fi
done

LLVM_VERSION="$("$LLVM_CONFIG_BIN" --version 2>/dev/null)" || {
  echo "coverage gate failed: selected llvm-config cannot report its version" >&2
  exit 2
}
if ! llvm_version_is_supported "$LLVM_VERSION"; then
  echo "coverage gate failed: Styio coverage requires LLVM 18.1.x; found ${LLVM_VERSION}" >&2
  exit 2
fi
for versioned_tool in "$CC_BIN" "$CXX_BIN" "$LLVM_COV_BIN" "$LLVM_PROFDATA_BIN"; do
  tool_version="$(extract_llvm_version "$versioned_tool")" || {
    echo "coverage gate failed: LLVM tool cannot report a semantic version: ${versioned_tool}" >&2
    exit 2
  }
  if [[ "$tool_version" != "$LLVM_VERSION" ]]; then
    echo "coverage gate failed: LLVM tools must share version ${LLVM_VERSION}; found ${tool_version}" >&2
    exit 2
  fi
done

LLVM_DIR="$("$LLVM_CONFIG_BIN" --cmakedir 2>/dev/null || true)"
if [[ -z "$LLVM_DIR" || ! -f "$LLVM_DIR/LLVMConfig.cmake" ]]; then
  LLVM_CONFIG_PREFIX="$("$LLVM_CONFIG_BIN" --prefix 2>/dev/null || true)"
  LLVM_DIR="$LLVM_CONFIG_PREFIX/lib/cmake/llvm"
fi
if [[ ! -f "$LLVM_DIR/LLVMConfig.cmake" ]]; then
  echo "coverage gate failed: LLVMConfig.cmake was not found for the selected toolchain" >&2
  exit 2
fi

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

if [[ "$CHECK_TOOLCHAIN" -eq 1 ]]; then
  echo "[coverage-gate] LLVM 18.1.x toolchain is coherent"
  exit 0
fi

CMAKE_ARGS=(
  -S .
  -B "$BUILD_DIR"
  -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_C_COMPILER="$CC_BIN"
  -DCMAKE_CXX_COMPILER="$CXX_BIN"
  -DLLVM_DIR="$LLVM_DIR"
  -DSTYIO_ENABLE_COVERAGE=ON
  -DSTYIO_BUILD_NANO=ON
)
if [[ "$(uname -s)" == "Darwin" ]]; then
  if ! command -v xcrun >/dev/null 2>&1; then
    echo "coverage gate failed: xcrun is required for the macOS SDK" >&2
    exit 2
  fi
  MACOS_SDK="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null)" || {
    echo "coverage gate failed: unable to resolve the macOS SDK with xcrun" >&2
    exit 2
  }
  CMAKE_ARGS+=("-DCMAKE_OSX_SYSROOT=$MACOS_SDK")
  CMAKE_ARGS+=("-DSTYIO_ENABLE_TREE_SITTER=OFF")
fi
cmake "${CMAKE_ARGS[@]}"

BUILD_TARGETS=(
  styio
  styio_lspd
  styio_nano
  styio_test
  styio_security_test
  styio_platform_internal_test
  styio_native_interop_internal_test
  styio_newparser_internal_test
  styio_parser_internal_test
  styio_resource_topology_test
  styio_ide_test
  styio_algorithm_equivalence_test
)

EXTERNAL_BENCHMARK_ROOT="$(sed -n 's/^STYIO_BENCHMARK_ROOT:PATH=//p' "$BUILD_DIR/CMakeCache.txt" | head -n 1)"
EXTERNAL_BENCHMARK_AVAILABLE=0
if [[ -n "$EXTERNAL_BENCHMARK_ROOT" &&
      -f "$EXTERNAL_BENCHMARK_ROOT/styio-probes/styio_soak_test.cpp" &&
      -f "$EXTERNAL_BENCHMARK_ROOT/styio-probes/styio_task_scheduler_perf_test.cpp" ]]; then
  EXTERNAL_BENCHMARK_AVAILABLE=1
fi
if [[ "$EXTERNAL_BENCHMARK_AVAILABLE" -eq 1 &&
      ( "$RUN_ALL" -eq 1 || "$INCLUDE_SOAK" -eq 1 ) ]]; then
  BUILD_TARGETS+=(styio_soak_test)
elif [[ "$INCLUDE_SOAK" -eq 1 ]]; then
  echo "coverage gate failed: --include-soak requires an external styio-benchmark checkout" >&2
  exit 2
fi
if [[ "$RUN_ALL" -eq 1 || "$INCLUDE_PERFORMANCE" -eq 1 ]]; then
  BUILD_TARGETS+=(styio_core_bench)
fi
if [[ "$EXTERNAL_BENCHMARK_AVAILABLE" -eq 1 &&
      ( "$RUN_ALL" -eq 1 || "$INCLUDE_PERFORMANCE" -eq 1 ) ]]; then
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
  ctest --test-dir "$BUILD_DIR" --output-on-failure --no-tests=error
else
  LABEL_REGEX="$(IFS='|'; echo "${LABELS[*]}")"
  echo "[coverage-gate] ctest labels: ${LABEL_REGEX}"
  ctest --test-dir "$BUILD_DIR" -L "$LABEL_REGEX" --output-on-failure --no-tests=error
fi

PROFRAW_FILES=()
while IFS= read -r profraw_file; do
  PROFRAW_FILES+=("$profraw_file")
done < <(find "$PROFILE_DIR" -type f -name '*.profraw' | sort)
if [[ "${#PROFRAW_FILES[@]}" -eq 0 ]]; then
  echo "coverage gate failed: no .profraw files were produced" >&2
  exit 1
fi

PROFDATA="$BUILD_DIR/styio-coverage.profdata"
"$LLVM_PROFDATA_BIN" merge -sparse "${PROFRAW_FILES[@]}" -o "$PROFDATA"

COVERAGE_OBJECTS=()
for object in \
  "$BUILD_DIR/bin/styio" \
  "$BUILD_DIR/bin/styio_lspd" \
  "$BUILD_DIR/bin/styio-nano" \
  "$BUILD_DIR/bin/styio_test" \
  "$BUILD_DIR/bin/styio_security_test" \
  "$BUILD_DIR/bin/styio_platform_internal_test" \
  "$BUILD_DIR/bin/styio_native_interop_internal_test" \
  "$BUILD_DIR/bin/styio_newparser_internal_test" \
  "$BUILD_DIR/bin/styio_parser_internal_test" \
  "$BUILD_DIR/bin/styio_resource_topology_test" \
  "$BUILD_DIR/bin/styio_ide_test" \
  "$BUILD_DIR/bin/styio_algorithm_equivalence_test" \
  "$BUILD_DIR/bin/styio_core_bench" \
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
