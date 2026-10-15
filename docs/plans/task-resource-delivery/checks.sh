#!/usr/bin/env bash
# Registered engineering entrypoints. Defining a command does not execute it.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$ROOT"
case "${1:-}" in
  baseline)
    cmake -S . -B build/default
    cmake --build build/default --target styio styio_test styio_resource_topology_test styio_lspd styio_main_contract_test styio_observable_runtime_test styio_codegen_internal_test
    build/default/bin/styio --machine-info=json
    ;;
  focused)
    ctest --test-dir build/default --output-on-failure --no-tests=error -R '^(state_resources_|task_resources_|styio_lspd_stdio_framing$)'
    build/default/bin/styio_test --gtest_filter='StyioResourceTopology.*:StyioResourceEffects.*:StyioStreamZip.*:StyioDiagnostics.MachineInfo*:StyioDiagnostics.CompilePlan*'
    build/default/bin/styio_resource_topology_test
    build/default/bin/styio_main_contract_test
    build/default/bin/styio_codegen_internal_test --gtest_filter='StyioCodeGenInternal.Task*:StyioCodeGenInternal.RuntimeReturnHelpersCoverGuardEdges:StyioCodeGenInternal.GetTypeUsesBoundedRingElementTypeForResourceIds'
    STYIO_TASK_QUEUE_CAPACITY=2 STYIO_TASK_THREADS=1 build/default/bin/styio_observable_runtime_test
    ;;
  integration)
    test -n "${PAFIO_ROOT:?Set the selected Pafio source root}"
    python3 "$PAFIO_ROOT/scripts/styio-interface-gate.py" --styio-bin "$PWD/build/default/bin/styio" --pafio-bin "${PAFIO_BIN:?Set the current-source Pafio binary}" --require-compile-plan --json
    ;;
  final)
    # HOST owns the one full run. After a diagnosed failure, replace only the
    # build/test commands below with the documented affected/remaining selection.
    # Each invocation keeps its own evidence; never overwrite the first run.
    mkdir -p build/task-resource-final
    final_log_dir="$(mktemp -d "$ROOT/build/task-resource-final/host-XXXXXXXX")"
    printf '%s\n' "${final_log_dir#"$ROOT/"}" > build/task-resource-final/latest.txt
    trap 'final_exit=$?; trap - EXIT; printf "%s\n" "$final_exit" > "$final_log_dir/check.exit"; exit "$final_exit"' EXIT
    cp docs/plans/task-resource-delivery/checks.sh "$final_log_dir/checks.sh"
    python3 - "$final_log_dir" <<'PY'
import datetime, json, os, pathlib, shutil, sys
out = pathlib.Path(sys.argv[1])
keys = (
    "PATH", "CC", "CXX", "SDKROOT", "MACOSX_DEPLOYMENT_TARGET",
    "DYLD_LIBRARY_PATH", "LD_LIBRARY_PATH", "LANG", "LC_ALL", "TZ", "TMPDIR",
    "CMAKE_BUILD_PARALLEL_LEVEL", "MAKEFLAGS", "CTEST_PARALLEL_LEVEL",
    "CTEST_OUTPUT_ON_FAILURE", "CTEST_TEST_LOAD", "CTEST_PROGRESS_OUTPUT",
    "STYIO_TASK_THREADS", "STYIO_TASK_QUEUE_CAPACITY", "STYIO_BIN",
    "STYIO_LSP_BIN", "PAFIO_BIN", "PAFIO_ROOT", "VITYO_PRODUCT_PLATFORM",
    "GTEST_FILTER", "GTEST_SHARD_INDEX", "GTEST_TOTAL_SHARDS", "GTEST_REPEAT",
)
def selected_stats():
    return {str(p): {"size": p.stat().st_size, "mtime_ns": p.stat().st_mtime_ns}
            for p in (pathlib.Path("build/default/bin/styio"),
                      pathlib.Path("build/default/bin/styio_lspd"))}
(out / "invocation.json").write_text(json.dumps({
    "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    "prepared_inputs": "build/task-resource-final/prepared-inputs",
    "environment": {key: os.environ.get(key) for key in keys},
    "tools": {key: shutil.which(key) for key in ("cmake", "ctest")},
    "selected_executables_before_build": selected_stats(),
}, indent=2) + "\n")
PY
    # The registered entrypoint is the complete engineering check: build every
    # target, then run the full unfiltered CTest suite. A diagnosed failure may
    # justify a per-run affected/remaining selection for that host attempt;
    # that recovery narrows a single run's record and never replaces this form.
    printf '%s\n' 'cmake --build build/default' > "$final_log_dir/build.command"
    # Direct redirection retains stdout/stderr without a pipeline masking exits.
    set +e
    cmake --build build/default > "$final_log_dir/build.log" 2>&1
    build_exit=$?
    set -e
    printf '%s\n' "$build_exit" > "$final_log_dir/build.exit"
    tail -n 40 "$final_log_dir/build.log"
    if (( build_exit != 0 )); then exit "$build_exit"; fi
    cp build/default/CMakeCache.txt "$final_log_dir/CMakeCache.txt"
    python3 - "$final_log_dir" <<'PY'
import json, pathlib, sys
paths = (pathlib.Path("build/default/bin/styio"),
         pathlib.Path("build/default/bin/styio_lspd"))
(pathlib.Path(sys.argv[1]) / "executables-after-build.json").write_text(json.dumps({
    str(p): {"size": p.stat().st_size, "mtime_ns": p.stat().st_mtime_ns}
    for p in paths
}, indent=2) + "\n")
PY
    # Verbose logging and JUnit reporting change reporting only, not selection.
    ctest_command=(ctest --test-dir build/default --output-on-failure --no-tests=error
      --verbose --output-junit "$final_log_dir/ctest.xml")
    printf '%q ' "${ctest_command[@]}" > "$final_log_dir/ctest.command"
    printf '\n' >> "$final_log_dir/ctest.command"
    set +e
    "${ctest_command[@]}" > "$final_log_dir/ctest.log" 2>&1
    ctest_exit=$?
    printf '%s\n' "$ctest_exit" > "$final_log_dir/ctest.exit"
    # Copy before any affected rerun can replace LastTest.log / failure records.
    cp -R build/default/Testing "$final_log_dir/Testing" > "$final_log_dir/retention.log" 2>&1
    retention_exit=$?
    set -e
    printf '%s\n' "$retention_exit" > "$final_log_dir/retention.exit"
    tail -n 60 "$final_log_dir/ctest.log"
    printf 'Complete final-check evidence: %s\n' "${final_log_dir#"$ROOT/"}"
    if (( ctest_exit != 0 )); then exit "$ctest_exit"; fi
    exit "$retention_exit"
    ;;
  review)
    git diff --check
    ;;
  *) echo "usage: checks.sh {baseline|focused|integration|final|review}" >&2; exit 2 ;;
esac
