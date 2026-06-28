include(GoogleTest)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(MSVC)
  add_compile_options(
    "$<$<COMPILE_LANGUAGE:C>:/utf-8>"
    "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>"
    "$<$<COMPILE_LANGUAGE:CXX>:/bigobj>"
  )
  target_compile_definitions(styio_core PUBLIC STYIO_CODEGEN_INTERNAL_ACCESS=public)
endif()
target_compile_definitions(styio_ide_core PUBLIC STYIO_IDE_INTERNAL_ACCESS=public)

set(STYIO_TEST_RUNNER "${CMAKE_CURRENT_LIST_DIR}/run_styio_case.py")
set(STYIO_SHADOW_GATE_RUNNER "${CMAKE_SOURCE_DIR}/benchmark/parser-shadow-suite-gate.py")

if(WIN32)
  # Several long-lived parser/runtime fixture strings intentionally contain
  # Unix-style /tmp paths because those literals are part of the golden contract.
  # Windows treats /tmp as a root-relative path on the current drive, so create
  # that directory for tests that exercise those fixtures through the runtime.
  file(MAKE_DIRECTORY "/tmp")
endif()

add_test(
  NAME docs_audit
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/docs-audit.py"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(docs_audit PROPERTIES LABELS "docs")

add_test(
  NAME stdlib_manifest_gate
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/stdlib-manifest-gate.py"
          --repo-root "${CMAKE_SOURCE_DIR}"
          --manifest "${CMAKE_SOURCE_DIR}/library/manifest.json"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(stdlib_manifest_gate PROPERTIES LABELS "docs;stdlib;workflow")

add_test(
  NAME runtime_surface_gate
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/runtime-surface-gate.py"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(runtime_surface_gate PROPERTIES LABELS "security;safety;runtime_surface;unit")

add_test(
  NAME architecture_layer_gate
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/architecture-layer-gate.py"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(architecture_layer_gate PROPERTIES LABELS "architecture;workflow;unit")

add_test(
  NAME bootstrap_dev_env_dependency_scope
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          bootstrap-scope
          --repo-root "${CMAKE_SOURCE_DIR}"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(bootstrap_dev_env_dependency_scope PROPERTIES LABELS "docs;workflow")

add_test(
  NAME benchmark_evidence_test
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/benchmark_evidence_test.py"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(benchmark_evidence_test PROPERTIES LABELS "benchmark;unit;workflow")

add_test(
  NAME styio_core_bench_json_smoke
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_CURRENT_LIST_DIR}/benchmark_core_bench_json_smoke.py"
          --binary "$<TARGET_FILE:styio_core_bench>"
          --output "${CMAKE_BINARY_DIR}/benchmark/core/core-benchmark-json-smoke.json"
          --warmup 1
          --measured 1
)
set_tests_properties(styio_core_bench_json_smoke PROPERTIES
  LABELS "benchmark;workflow;core_internal;smoke"
  ENVIRONMENT "STYIO_BENCH_WARMUP=1;STYIO_BENCH_MEASURED=1"
)

add_test(
  NAME styio_lspd_stdio_framing
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_CURRENT_LIST_DIR}/lsp_stdio_framing_test.py"
          --binary "$<TARGET_FILE:styio_lspd>"
)
set_tests_properties(styio_lspd_stdio_framing PROPERTIES
  LABELS "ide;lsp;transport;smoke"
)

# Language feature .styio sources live under tests/features/<feature>.
set(STYIO_FEATURES "${CMAKE_CURRENT_LIST_DIR}/features")
set(STYIO_EXE "$<TARGET_FILE:styio>")

function(styio_add_stdout_golden_test TEST_NAME SOURCE_PATH EXPECTED_PATH LABELS_VALUE)
  add_test(
    NAME "${TEST_NAME}"
    COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
            run
            --styio "${STYIO_EXE}"
            --source "${SOURCE_PATH}"
            --cwd "${CMAKE_SOURCE_DIR}"
            --expect-stdout "${EXPECTED_PATH}"
            ${ARGN}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
  set_tests_properties("${TEST_NAME}" PROPERTIES LABELS "${LABELS_VALUE}")
endfunction()

function(styio_add_stderr_golden_test TEST_NAME SOURCE_PATH EXPECTED_PATH LABELS_VALUE)
  add_test(
    NAME "${TEST_NAME}"
    COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
            run
            --styio "${STYIO_EXE}"
            --source "${SOURCE_PATH}"
            --cwd "${CMAKE_SOURCE_DIR}"
            --discard-stdout
            --expect-stderr "${EXPECTED_PATH}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
  set_tests_properties("${TEST_NAME}" PROPERTIES LABELS "${LABELS_VALUE}")
endfunction()

add_test(
  NAME readme_glimpse_job_parallel_signal
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          run
          --styio "${STYIO_EXE}"
          --source "${CMAKE_SOURCE_DIR}/example/job_parallel_signal.styio"
          --cwd "${CMAKE_SOURCE_DIR}"
          --stdin-text "101\n94\n"
          --expect-stdout "${CMAKE_SOURCE_DIR}/example/expected/job_parallel_signal.out"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(readme_glimpse_job_parallel_signal PROPERTIES LABELS "docs;example;styio_pipeline")

add_test(
  NAME example_hello_world
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          run
          --styio "${STYIO_EXE}"
          --source "${CMAKE_SOURCE_DIR}/example/hello_world.styio"
          --cwd "${CMAKE_SOURCE_DIR}"
          --expect-stdout "${CMAKE_SOURCE_DIR}/example/expected/hello_world.out"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(example_hello_world PROPERTIES LABELS "example;styio_pipeline")

add_test(
  NAME example_bubble_sort
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          run
          --styio "${STYIO_EXE}"
          --source "${CMAKE_SOURCE_DIR}/example/algorithms/bubble_sort.styio"
          --cwd "${CMAKE_SOURCE_DIR}"
          --stdin-text "[3, 1, 2]\n"
          --expect-stdout "${CMAKE_SOURCE_DIR}/example/expected/bubble_sort.out"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(example_bubble_sort PROPERTIES LABELS "example;styio_pipeline;algorithm_equivalence")

add_test(
  NAME example_eight_queens
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          run
          --styio "${STYIO_EXE}"
          --source "${CMAKE_SOURCE_DIR}/example/algorithms/eight_queens.styio"
          --cwd "${CMAKE_SOURCE_DIR}"
          --expect-stdout "${CMAKE_SOURCE_DIR}/example/expected/eight_queens.out"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(example_eight_queens PROPERTIES LABELS "example;styio_pipeline;algorithm_equivalence")

add_test(
  NAME example_cli_calculator
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          calc
          --styio "${STYIO_EXE}"
          --cwd "${CMAKE_SOURCE_DIR}"
          --expression "1 + 2 * (3 + 4)"
          --expect-stdout "${CMAKE_SOURCE_DIR}/example/expected/cli_calculator.out"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(example_cli_calculator PROPERTIES LABELS "example;styio_pipeline")

add_test(
  NAME styio_profiler_frontend_smoke
  COMMAND "${CMAKE_COMMAND}"
          "-DSTYIO_BIN=${STYIO_EXE}"
          "-DSTYIO_SOURCE=${STYIO_FEATURES}/task_resources/t04_task_capture_i64.styio"
          "-DSTYIO_PROFILE_OUT=${CMAKE_BINARY_DIR}/profiler/styio-profiler-smoke.json"
          -P "${CMAKE_CURRENT_LIST_DIR}/cmake/styio_profiler_smoke.cmake"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(styio_profiler_frontend_smoke PROPERTIES
  LABELS "styio_pipeline;profiler;unit"
)

# --- Helper: stdout golden (styio --file X | cmp -s - expected/X.out) ---
function(styio_feature_stdout_golden_test FEATURE TEST_GLOB LABEL)
  file(GLOB _STYIO_FILES CONFIGURE_DEPENDS "${STYIO_FEATURES}/${FEATURE}/${TEST_GLOB}")
  list(SORT _STYIO_FILES)
  foreach(_F IN LISTS _STYIO_FILES)
    get_filename_component(_NAME ${_F} NAME_WE)
    set(_EXP "${STYIO_FEATURES}/${FEATURE}/expected/${_NAME}.out")
    if(NOT EXISTS "${_EXP}")
      message(FATAL_ERROR "Missing golden: ${_EXP} (required by ${_NAME})")
    endif()
    styio_add_stdout_golden_test(
      "${LABEL}_${_NAME}"
      "${_F}"
      "${_EXP}"
      "language_feature;${LABEL}"
    )
  endforeach()
endfunction()

function(styio_feature_stderr_failure_golden_test FEATURE LABEL)
  file(GLOB _STYIO_FAIL_FILES CONFIGURE_DEPENDS "${STYIO_FEATURES}/${FEATURE}/e*.styio")
  list(SORT _STYIO_FAIL_FILES)
  foreach(_F IN LISTS _STYIO_FAIL_FILES)
    get_filename_component(_ENAME ${_F} NAME_WE)
    set(_EXP "${STYIO_FEATURES}/${FEATURE}/expected/${_ENAME}.err")
    if(NOT EXISTS "${_EXP}")
      message(FATAL_ERROR "Missing diagnostic golden: ${_EXP} (required by ${_ENAME})")
    endif()
    add_test(
      NAME "${LABEL}_err_${_ENAME}"
      COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
              run
              --styio "${STYIO_EXE}"
              --source "${_F}"
              --cwd "${CMAKE_SOURCE_DIR}"
              --expect-failure
              --stderr-contains-file "${_EXP}"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties("${LABEL}_err_${_ENAME}" PROPERTIES LABELS "language_feature;${LABEL};${LABEL}_semantic")
  endforeach()
endfunction()

styio_feature_stdout_golden_test(scalar_expressions "t*.styio" scalar_expressions)
styio_feature_stdout_golden_test(functions "t*.styio" functions)
styio_feature_stdout_golden_test(control_flow "t*.styio" control_flow)
styio_feature_stdout_golden_test(wave_dispatch "t*.styio" wave_dispatch)
styio_feature_stderr_failure_golden_test(wave_dispatch wave_dispatch)
styio_feature_stdout_golden_test(state_resources "t*.styio" state_resources)
styio_feature_stdout_golden_test(final_bindings "t*.styio" final_bindings)
styio_feature_stdout_golden_test(native_interop "t*.styio" native_interop)
styio_feature_stdout_golden_test(task_resources "t*.styio" task_resources)

# --- stdout/stderr resource tests ---
styio_feature_stdout_golden_test(stdio_output "t01_*.styio" stdio_output)
styio_feature_stdout_golden_test(stdio_output "t02_*.styio" stdio_output)
styio_feature_stdout_golden_test(stdio_output "t03_*.styio" stdio_output)
styio_feature_stdout_golden_test(stdio_output "t04_*.styio" stdio_output)
styio_feature_stdout_golden_test(stdio_output "t05_*.styio" stdio_output)
styio_feature_stdout_golden_test(stdio_output "t06_*.styio" stdio_output)
styio_feature_stdout_golden_test(stdio_output "t08_*.styio" stdio_output)
styio_feature_stdout_golden_test(stdio_output "t12_*.styio" stdio_output)

# Stderr tests capture stderr and compare against .err golden.
add_test(
  NAME stdio_output_t09_stderr_string
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          run
          --styio "${STYIO_EXE}"
          --source "${STYIO_FEATURES}/stdio_output/t09_stderr_string.styio"
          --cwd "${CMAKE_SOURCE_DIR}"
          --discard-stdout
          --expect-stderr "${STYIO_FEATURES}/stdio_output/expected/t09_stderr_string.err"
)
set_tests_properties(stdio_output_t09_stderr_string PROPERTIES LABELS "language_feature;stdio_output")

add_test(
  NAME stdio_output_t10_stderr_int
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          run
          --styio "${STYIO_EXE}"
          --source "${STYIO_FEATURES}/stdio_output/t10_stderr_int.styio"
          --cwd "${CMAKE_SOURCE_DIR}"
          --discard-stdout
          --expect-stderr "${STYIO_FEATURES}/stdio_output/expected/t10_stderr_int.err"
)
set_tests_properties(stdio_output_t10_stderr_int PROPERTIES LABELS "language_feature;stdio_output")

# Mixed stdout+stderr test verifies both streams separately.
add_test(
  NAME stdio_output_t11_mixed
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          run
          --styio "${STYIO_EXE}"
          --source "${STYIO_FEATURES}/stdio_output/t11_mixed.styio"
          --cwd "${CMAKE_SOURCE_DIR}"
          --expect-stdout "${STYIO_FEATURES}/stdio_output/expected/t11_mixed.out"
          --expect-stderr "${STYIO_FEATURES}/stdio_output/expected/t11_mixed.err"
)
set_tests_properties(stdio_output_t11_mixed PROPERTIES LABELS "language_feature;stdio_output")

# --- stdin resource tests (piped input) ---
add_test(
  NAME stdio_input_t01_stdin_echo
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          run
          --styio "${STYIO_EXE}"
          --source "${STYIO_FEATURES}/stdio_input/t01_stdin_echo.styio"
          --cwd "${CMAKE_SOURCE_DIR}"
          --stdin-file "${STYIO_FEATURES}/stdio_input/data/echo_input.txt"
          --expect-stdout "${STYIO_FEATURES}/stdio_input/expected/t01_stdin_echo.out"
)
set_tests_properties(stdio_input_t01_stdin_echo PROPERTIES LABELS "language_feature;stdio_input")

add_test(
  NAME styio_build_native_executable_stdin_echo
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          build-run
          --styio "${STYIO_EXE}"
          --source "${STYIO_FEATURES}/stdio_input/t01_stdin_echo.styio"
          --cwd "${CMAKE_SOURCE_DIR}"
          --output "${CMAKE_BINARY_DIR}/styio-native-stdin-echo${CMAKE_EXECUTABLE_SUFFIX}"
          --stdin-file "${STYIO_FEATURES}/stdio_input/data/echo_input.txt"
          --expect-stdout "${STYIO_FEATURES}/stdio_input/expected/t01_stdin_echo.out"
          --profile-out "${CMAKE_BINARY_DIR}/styio-native-stdin-echo.profile.json"
          --profile-contains "\"tool\": \"styio-native-profiler\""
          --profile-contains "\"name\": \"execute\""
)
set_tests_properties(styio_build_native_executable_stdin_echo PROPERTIES
  LABELS "native_artifact;language_feature;stdio_input"
)

add_test(
  NAME styio_build_native_executable_native_interop
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          build-run
          --styio "${STYIO_EXE}"
          --source "${STYIO_FEATURES}/native_interop/t07_native_hybrid_inline_reference_artifact.styio"
          --cwd "${CMAKE_SOURCE_DIR}"
          --output "${CMAKE_BINARY_DIR}/styio-native-hybrid${CMAKE_EXECUTABLE_SUFFIX}"
          --expect-stdout "${STYIO_FEATURES}/native_interop/expected/t07_native_hybrid_inline_reference_artifact.out"
)
set_tests_properties(styio_build_native_executable_native_interop PROPERTIES
  LABELS "native_artifact;native_interop;language_feature"
)

add_test(
  NAME stdio_input_t02_stdin_pull
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}" run --styio "${STYIO_EXE}" --source "${STYIO_FEATURES}/stdio_input/t02_stdin_pull.styio" --cwd "${CMAKE_SOURCE_DIR}" --stdin-file "${STYIO_FEATURES}/stdio_input/data/pull_input.txt" --expect-stdout "${STYIO_FEATURES}/stdio_input/expected/t02_stdin_pull.out"
)
set_tests_properties(stdio_input_t02_stdin_pull PROPERTIES LABELS "language_feature;stdio_input")

add_test(
  NAME stdio_input_t03_stdin_eof
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}" run --styio "${STYIO_EXE}" --source "${STYIO_FEATURES}/stdio_input/t03_stdin_eof.styio" --cwd "${CMAKE_SOURCE_DIR}" --stdin-file "${STYIO_FEATURES}/stdio_input/data/empty_input.txt" --expect-stdout "${STYIO_FEATURES}/stdio_input/expected/t03_stdin_eof.out"
)
set_tests_properties(stdio_input_t03_stdin_eof PROPERTIES LABELS "language_feature;stdio_input")

add_test(
  NAME stdio_input_t04_stdin_multi_pull
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}" run --styio "${STYIO_EXE}" --source "${STYIO_FEATURES}/stdio_input/t04_stdin_multi_pull.styio" --cwd "${CMAKE_SOURCE_DIR}" --stdin-file "${STYIO_FEATURES}/stdio_input/data/multi_input.txt" --expect-stdout "${STYIO_FEATURES}/stdio_input/expected/t04_stdin_multi_pull.out"
)
set_tests_properties(stdio_input_t04_stdin_multi_pull PROPERTIES LABELS "language_feature;stdio_input")

add_test(
  NAME stdio_input_t05_stdin_transform
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}" run --styio "${STYIO_EXE}" --source "${STYIO_FEATURES}/stdio_input/t05_stdin_transform.styio" --cwd "${CMAKE_SOURCE_DIR}" --stdin-file "${STYIO_FEATURES}/stdio_input/data/transform_input.txt" --expect-stdout "${STYIO_FEATURES}/stdio_input/expected/t05_stdin_transform.out"
)
set_tests_properties(stdio_input_t05_stdin_transform PROPERTIES LABELS "language_feature;stdio_input")

add_test(
  NAME stdio_input_t06_stdin_mixed_output
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}" run --styio "${STYIO_EXE}" --source "${STYIO_FEATURES}/stdio_input/t06_stdin_mixed_output.styio" --cwd "${CMAKE_SOURCE_DIR}" --stdin-file "${STYIO_FEATURES}/stdio_input/data/mixed_input.txt" --expect-stdout "${STYIO_FEATURES}/stdio_input/expected/t06_stdin_mixed_output.out" --expect-stderr "${STYIO_FEATURES}/stdio_input/expected/t06_stdin_mixed_output.err"
)
set_tests_properties(stdio_input_t06_stdin_mixed_output PROPERTIES LABELS "language_feature;stdio_input")

add_test(
  NAME stdio_input_t07_stdin_typed_tuple_pull
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}" run --styio "${STYIO_EXE}" --source "${STYIO_FEATURES}/stdio_input/t07_stdin_typed_tuple_pull.styio" --cwd "${CMAKE_SOURCE_DIR}" --stdin-file "${STYIO_FEATURES}/stdio_input/data/typed_tuple_input.txt" --expect-stdout "${STYIO_FEATURES}/stdio_input/expected/t07_stdin_typed_tuple_pull.out"
)
set_tests_properties(stdio_input_t07_stdin_typed_tuple_pull PROPERTIES LABELS "language_feature;stdio_input")

add_test(
  NAME stdio_input_t08_stdin_typed_f64_list_pull
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}" run --styio "${STYIO_EXE}" --source "${STYIO_FEATURES}/stdio_input/t08_stdin_typed_f64_list_pull.styio" --cwd "${CMAKE_SOURCE_DIR}" --stdin-file "${STYIO_FEATURES}/stdio_input/data/typed_f64_list_input.txt" --expect-stdout "${STYIO_FEATURES}/stdio_input/expected/t08_stdin_typed_f64_list_pull.out"
)
set_tests_properties(stdio_input_t08_stdin_typed_f64_list_pull PROPERTIES LABELS "language_feature;stdio_input")

# Semantic failure tests must check the diagnostic contract, not only nonzero exit.
styio_feature_stderr_failure_golden_test(stdio_input stdio_input)
styio_feature_stderr_failure_golden_test(task_resources task_resources)
styio_feature_stderr_failure_golden_test(state_resources state_resources)
styio_feature_stderr_failure_golden_test(resource_pressure resource_pressure)
styio_feature_stderr_failure_golden_test(final_bindings final_bindings)

# --- file resource stdout goldens + special file / exit-code cases ---
add_test(
  NAME file_resources_t01_read_file
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          run
          --styio "${STYIO_EXE}"
          --source "${STYIO_FEATURES}/file_resources/t01_read_file.styio"
          --cwd "${CMAKE_SOURCE_DIR}"
          --expect-stdout "${STYIO_FEATURES}/file_resources/expected/t01_read_file.out"
)
set_tests_properties(file_resources_t01_read_file PROPERTIES LABELS "language_feature;file_resources")

add_test(
  NAME file_resources_t02_write_file
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          run
          --styio "${STYIO_EXE}"
          --source "${STYIO_FEATURES}/file_resources/t02_write_file.styio"
          --cwd "${CMAKE_SOURCE_DIR}"
          --replace-literal "/tmp/styio_out.txt" "${CMAKE_BINARY_DIR}/styio_out.txt"
          --cleanup "${CMAKE_BINARY_DIR}/styio_out.txt"
          --expect-file-text "${CMAKE_BINARY_DIR}/styio_out.txt" "Hello from Styio"
)
set_tests_properties(file_resources_t02_write_file PROPERTIES LABELS "language_feature;file_resources")

add_test(
  NAME file_resources_t04_raii
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}" run --styio "${STYIO_EXE}" --source "${STYIO_FEATURES}/file_resources/t04_raii.styio" --cwd "${CMAKE_SOURCE_DIR}" --expect-stdout "${STYIO_FEATURES}/file_resources/expected/t04_raii.out"
)
set_tests_properties(file_resources_t04_raii PROPERTIES LABELS "language_feature;file_resources")

add_test(
  NAME file_resources_t05_auto_detect
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}" run --styio "${STYIO_EXE}" --source "${STYIO_FEATURES}/file_resources/t05_auto_detect.styio" --cwd "${CMAKE_SOURCE_DIR}" --expect-stdout "${STYIO_FEATURES}/file_resources/expected/t05_auto_detect.out"
)
set_tests_properties(file_resources_t05_auto_detect PROPERTIES LABELS "language_feature;file_resources")

add_test(
  NAME file_resources_t06_fail_fast
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}" run --styio "${STYIO_EXE}" --source "${STYIO_FEATURES}/file_resources/t06_fail_fast.styio" --cwd "${CMAKE_SOURCE_DIR}" --expect-failure
)
set_tests_properties(file_resources_t06_fail_fast PROPERTIES LABELS "language_feature;file_resources")

add_test(
  NAME file_resources_t07_redirect
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
          run
          --styio "${STYIO_EXE}"
          --source "${STYIO_FEATURES}/file_resources/t07_redirect.styio"
          --cwd "${CMAKE_SOURCE_DIR}"
          --replace-literal "/tmp/styio_val.txt" "${CMAKE_BINARY_DIR}/styio_val.txt"
          --cleanup "${CMAKE_BINARY_DIR}/styio_val.txt"
          --expect-file-text "${CMAKE_BINARY_DIR}/styio_val.txt" "42"
)
set_tests_properties(file_resources_t07_redirect PROPERTIES LABELS "language_feature;file_resources")

add_test(
  NAME file_resources_t08_pipe_func
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}" run --styio "${STYIO_EXE}" --source "${STYIO_FEATURES}/file_resources/t08_pipe_func.styio" --cwd "${CMAKE_SOURCE_DIR}" --expect-stdout "${STYIO_FEATURES}/file_resources/expected/t08_pipe_func.out"
)
set_tests_properties(file_resources_t08_pipe_func PROPERTIES LABELS "language_feature;file_resources")

# --- stream processing: stdout or artifact golden ---
file(GLOB STREAM_PROCESSING_STYIO_TESTS CONFIGURE_DEPENDS "${STYIO_FEATURES}/stream_processing/t*.styio")
list(SORT STREAM_PROCESSING_STYIO_TESTS)
foreach(STREAM_PROCESSING_FILE IN LISTS STREAM_PROCESSING_STYIO_TESTS)
  get_filename_component(STREAM_PROCESSING_NAME ${STREAM_PROCESSING_FILE} NAME_WE)
  if(STREAM_PROCESSING_NAME STREQUAL "t10_full_pipeline")
    add_test(
      NAME "stream_processing_${STREAM_PROCESSING_NAME}"
      COMMAND "${Python3_EXECUTABLE}" "${STYIO_TEST_RUNNER}"
              run
              --styio "${STYIO_EXE}"
              --source "${STREAM_PROCESSING_FILE}"
              --cwd "${CMAKE_SOURCE_DIR}"
              --replace-literal "/tmp/styio_doubled.txt" "${CMAKE_BINARY_DIR}/styio_doubled.txt"
              --cleanup "${CMAKE_BINARY_DIR}/styio_doubled.txt"
              --expect-file-equals "${CMAKE_BINARY_DIR}/styio_doubled.txt" "${STYIO_FEATURES}/stream_processing/expected/${STREAM_PROCESSING_NAME}.out"
    )
  else()
    styio_add_stdout_golden_test(
      "stream_processing_${STREAM_PROCESSING_NAME}"
      "${STREAM_PROCESSING_FILE}"
      "${STYIO_FEATURES}/stream_processing/expected/${STREAM_PROCESSING_NAME}.out"
      "language_feature;stream_processing"
    )
  endif()
  set_tests_properties("stream_processing_${STREAM_PROCESSING_NAME}" PROPERTIES LABELS "language_feature;stream_processing")
endforeach()
styio_feature_stderr_failure_golden_test(stream_processing stream_processing)

add_test(
  NAME parser_shadow_gate_scalar_expressions_zero_fallback_and_internal_bridges
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_SHADOW_GATE_RUNNER}"
          --require-zero-fallback
          --require-zero-internal-bridges
          "${STYIO_EXE}"
          "${STYIO_FEATURES}/scalar_expressions"
          "${CMAKE_BINARY_DIR}/parser-shadow/scalar_expressions"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_scalar_expressions_zero_fallback_and_internal_bridges PROPERTIES
  LABELS "styio_pipeline;shadow_gate;scalar_expressions"
)

add_test(
  NAME parser_shadow_gate_functions_zero_fallback_and_internal_bridges
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_SHADOW_GATE_RUNNER}"
          --require-zero-fallback
          --require-zero-internal-bridges
          "${STYIO_EXE}"
          "${STYIO_FEATURES}/functions"
          "${CMAKE_BINARY_DIR}/parser-shadow/functions"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_functions_zero_fallback_and_internal_bridges PROPERTIES
  LABELS "styio_pipeline;shadow_gate;functions"
)

add_test(
  NAME parser_shadow_gate_file_resources_dual_zero_expected_nonzero
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_SHADOW_GATE_RUNNER}"
          --require-zero-fallback
          --require-zero-internal-bridges
          "${STYIO_EXE}"
          "${STYIO_FEATURES}/file_resources"
          "${CMAKE_BINARY_DIR}/parser-shadow/file_resources"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_file_resources_dual_zero_expected_nonzero PROPERTIES
  LABELS "styio_pipeline;shadow_gate;file_resources"
)

add_test(
  NAME parser_shadow_gate_stream_processing_zero_fallback
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_SHADOW_GATE_RUNNER}"
          --require-zero-fallback
          "${STYIO_EXE}"
          "${STYIO_FEATURES}/stream_processing"
          "${CMAKE_BINARY_DIR}/parser-shadow/stream_processing_zero_fallback"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_stream_processing_zero_fallback PROPERTIES
  LABELS "styio_pipeline;shadow_gate;stream_processing"
)

add_test(
  NAME parser_shadow_gate_stream_processing_zero_internal_bridges
  COMMAND "${Python3_EXECUTABLE}" "${STYIO_SHADOW_GATE_RUNNER}"
          --require-zero-fallback
          --require-zero-internal-bridges
          "${STYIO_EXE}"
          "${STYIO_FEATURES}/stream_processing"
          "${CMAKE_BINARY_DIR}/parser-shadow/stream_processing_zero_internal_bridges"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_stream_processing_zero_internal_bridges PROPERTIES
  LABELS "styio_pipeline;shadow_gate;stream_processing"
)

add_test(
  NAME parser_legacy_entry_audit
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/parser-legacy-entry-audit.py"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_legacy_entry_audit PROPERTIES
  LABELS "styio_pipeline;workflow"
)

add_executable(styio_test
  styio_test.cpp
)
target_link_libraries(
  styio_test
  styio_core
  GTest::gtest_main
)
target_compile_definitions(styio_test PRIVATE
  "STYIO_SOURCE_DIR=\"${STYIO_DEFINE_SOURCE_DIR}\""
  "STYIO_COMPILER_EXE=\"$<TARGET_FILE:styio>\""
  "STYIO_TEST_C_COMPILER=\"${STYIO_DEFINE_CMAKE_C_COMPILER}\""
)
add_dependencies(styio_test styio)
if(TARGET styio_nano)
  target_compile_definitions(styio_test PRIVATE
    "STYIO_NANO_COMPILER_EXE=\"$<TARGET_FILE:styio_nano>\""
  )
  add_dependencies(styio_test styio_nano)
endif()

add_executable(styio_algorithm_equivalence_test
  "${CMAKE_SOURCE_DIR}/src/StyioPlatform/Platform.cpp"
  algorithms/.common/CxxReferenceEquivalence.cpp
  algorithms/accumulate_sum/reference.cpp
  algorithms/accumulate_sum/test.cpp
  algorithms/adjacent_difference/reference.cpp
  algorithms/adjacent_difference/test.cpp
  algorithms/adjacent_equal_index/reference.cpp
  algorithms/adjacent_equal_index/test.cpp
  algorithms/all_positive_flag/reference.cpp
  algorithms/all_positive_flag/test.cpp
  algorithms/any_negative_flag/reference.cpp
  algorithms/any_negative_flag/test.cpp
  algorithms/binary_search/reference.cpp
  algorithms/binary_search/test.cpp
  algorithms/bubble_sort/reference.cpp
  algorithms/bubble_sort/test.cpp
  algorithms/count_value/reference.cpp
  algorithms/count_value/test.cpp
  algorithms/equal_flag/reference.cpp
  algorithms/equal_flag/test.cpp
  algorithms/equal_range_bounds/reference.cpp
  algorithms/equal_range_bounds/test.cpp
  algorithms/euclidean_gcd/reference.cpp
  algorithms/euclidean_gcd/test.cpp
  algorithms/exclusive_scan_sum/reference.cpp
  algorithms/exclusive_scan_sum/test.cpp
  algorithms/factorial/reference.cpp
  algorithms/factorial/test.cpp
  algorithms/inclusive_scan_sum/reference.cpp
  algorithms/inclusive_scan_sum/test.cpp
  algorithms/inner_product/reference.cpp
  algorithms/inner_product/test.cpp
  algorithms/is_sorted_flag/reference.cpp
  algorithms/is_sorted_flag/test.cpp
  algorithms/lexicographical_compare_flag/reference.cpp
  algorithms/lexicographical_compare_flag/test.cpp
  algorithms/linear_search/reference.cpp
  algorithms/linear_search/test.cpp
  algorithms/max_element_value/reference.cpp
  algorithms/max_element_value/test.cpp
  algorithms/minmax_pair/reference.cpp
  algorithms/minmax_pair/test.cpp
  algorithms/min_element_value/reference.cpp
  algorithms/min_element_value/test.cpp
  algorithms/mismatch_index/reference.cpp
  algorithms/mismatch_index/test.cpp
  algorithms/none_zero_flag/reference.cpp
  algorithms/none_zero_flag/test.cpp
  algorithms/prefix_sum/reference.cpp
  algorithms/prefix_sum/test.cpp
  algorithms/selection_sort/reference.cpp
  algorithms/selection_sort/test.cpp
  algorithms/subrange_find_end_index/reference.cpp
  algorithms/subrange_find_end_index/test.cpp
  algorithms/subrange_search_index/reference.cpp
  algorithms/subrange_search_index/test.cpp
  algorithms/transform_reduce_square_sum/reference.cpp
  algorithms/transform_reduce_square_sum/test.cpp
  algorithms/upper_bound_index/reference.cpp
  algorithms/upper_bound_index/test.cpp
)
target_link_libraries(
  styio_algorithm_equivalence_test
  GTest::gtest_main
)
target_compile_definitions(styio_algorithm_equivalence_test PRIVATE
  "STYIO_SOURCE_DIR=\"${STYIO_DEFINE_SOURCE_DIR}\""
  "STYIO_COMPILER_EXE=\"$<TARGET_FILE:styio>\""
)
target_include_directories(styio_algorithm_equivalence_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/src"
)
add_dependencies(styio_algorithm_equivalence_test styio)

add_executable(styio_security_test
  security/styio_security_test.cpp
)
target_link_libraries(styio_security_test PRIVATE styio_core GTest::gtest_main)
target_compile_definitions(styio_security_test PRIVATE
  "STYIO_TEST_C_COMPILER=\"${STYIO_DEFINE_CMAKE_C_COMPILER}\""
)
# Security tests transitively include codegen/session headers that reference
# LLVM types. Use the shared helper so LLVM headers stay behind the standard
# library on Debian/libstdc++.
styio_apply_llvm_compile_settings(styio_security_test)

add_executable(styio_typeinfer_internal_test
  typeinfer_internal_test.cpp
  "${CMAKE_SOURCE_DIR}/src/StyioSession/SymbolInterner.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioSession/TypeTable.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioToken/Token.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioUnicode/Unicode.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioParser/Parser.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioParser/ParserLookahead.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioParser/NewParserExpr.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioParser/Tokenizer.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioProfiler/FrontendProfiler.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioPlatform/Platform.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioNative/NativeInterop.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioResourceTopology/ResourceTopology.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioToString/ToString.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioIR/Verifier.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioLowering/AstToStyioIR.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioLowering/StyioIROptimizer.cpp"
)
target_include_directories(styio_typeinfer_internal_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(styio_typeinfer_internal_test PRIVATE
  styio_symbol_core
  styio_runtime_core
  GTest::gtest_main
  ${LLVM_LIBS}
  ${CMAKE_DL_LIBS}
)
styio_apply_llvm_compile_settings(styio_typeinfer_internal_test)

add_executable(styio_lowering_internal_test
  lowering_internal_test.cpp
  "${CMAKE_SOURCE_DIR}/src/StyioSession/SymbolInterner.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioSession/TypeTable.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioToken/Token.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioUnicode/Unicode.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioParser/Parser.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioParser/ParserLookahead.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioParser/NewParserExpr.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioParser/Tokenizer.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioProfiler/FrontendProfiler.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioPlatform/Platform.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioNative/NativeInterop.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioResourceTopology/ResourceTopology.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioToString/ToString.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioIR/Verifier.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioSema/TypeInfer.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioLowering/StyioIROptimizer.cpp"
)
target_include_directories(styio_lowering_internal_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(styio_lowering_internal_test PRIVATE
  styio_symbol_core
  styio_runtime_core
  GTest::gtest_main
  ${LLVM_LIBS}
  ${CMAKE_DL_LIBS}
)
styio_apply_llvm_compile_settings(styio_lowering_internal_test)

add_executable(styio_newparser_internal_test
  newparser_internal_test.cpp
  "${CMAKE_SOURCE_DIR}/src/StyioSession/SymbolInterner.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioSession/TypeTable.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioToken/Token.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioUnicode/Unicode.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioParser/Parser.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioParser/ParserLookahead.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioParser/Tokenizer.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioProfiler/FrontendProfiler.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioPlatform/Platform.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioNative/NativeInterop.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioResourceTopology/ResourceTopology.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioToString/ToString.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioIR/Verifier.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioLowering/AstToStyioIR.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioSema/TypeInfer.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioLowering/StyioIROptimizer.cpp"
)
target_include_directories(styio_newparser_internal_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(styio_newparser_internal_test PRIVATE
  styio_symbol_core
  styio_runtime_core
  GTest::gtest_main
  ${LLVM_LIBS}
  ${CMAKE_DL_LIBS}
)
styio_apply_llvm_compile_settings(styio_newparser_internal_test)

add_executable(styio_parser_internal_test
  parser_internal_test.cpp
  "${CMAKE_SOURCE_DIR}/src/StyioSession/SymbolInterner.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioSession/TypeTable.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioToken/Token.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioUnicode/Unicode.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioParser/NewParserExpr.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioParser/ParserLookahead.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioParser/Tokenizer.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioProfiler/FrontendProfiler.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioPlatform/Platform.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioNative/NativeInterop.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioResourceTopology/ResourceTopology.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioToString/ToString.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioIR/Verifier.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioLowering/AstToStyioIR.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioSema/TypeInfer.cpp"
  "${CMAKE_SOURCE_DIR}/src/StyioLowering/StyioIROptimizer.cpp"
)
target_include_directories(styio_parser_internal_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(styio_parser_internal_test PRIVATE
  styio_symbol_core
  styio_runtime_core
  GTest::gtest_main
  ${LLVM_LIBS}
  ${CMAKE_DL_LIBS}
)
styio_apply_llvm_compile_settings(styio_parser_internal_test)

add_executable(styio_native_interop_internal_test
  native_interop_internal_test.cpp
  "${CMAKE_SOURCE_DIR}/src/StyioPlatform/Platform.cpp"
)
target_include_directories(styio_native_interop_internal_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_BINARY_DIR}/generated"
)
target_compile_definitions(styio_native_interop_internal_test PRIVATE
  "STYIO_TEST_C_COMPILER=\"${STYIO_DEFINE_CMAKE_C_COMPILER}\""
  "STYIO_TEST_CXX_COMPILER=\"${STYIO_DEFINE_CMAKE_CXX_COMPILER}\""
)
target_link_libraries(styio_native_interop_internal_test PRIVATE
  GTest::gtest_main
  ${CMAKE_DL_LIBS}
)
styio_apply_llvm_compile_settings(styio_native_interop_internal_test)

add_executable(styio_externlib_internal_test
  externlib_internal_test.cpp
  "${CMAKE_SOURCE_DIR}/src/StyioRuntime/RuntimeState.cpp"
)
target_include_directories(styio_externlib_internal_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_BINARY_DIR}/generated"
)
target_link_libraries(styio_externlib_internal_test PRIVATE
  GTest::gtest_main
  ${CMAKE_DL_LIBS}
)
styio_apply_llvm_compile_settings(styio_externlib_internal_test)

add_executable(styio_codegen_internal_test
  codegen_internal_test.cpp
)
target_link_libraries(styio_codegen_internal_test PRIVATE styio_core GTest::gtest_main)
styio_apply_llvm_compile_settings(styio_codegen_internal_test)

add_executable(styio_syntax_check_internal_test
  syntax_check_internal_test.cpp
)
target_include_directories(styio_syntax_check_internal_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(styio_syntax_check_internal_test PRIVATE
  styio_frontend_core
  GTest::gtest_main
  ${LLVM_LIBS}
)
target_compile_definitions(styio_syntax_check_internal_test PRIVATE
  "STYIO_PROJECT_VERSION=\"${PROJECT_VERSION}\""
  "STYIO_RELEASE_CHANNEL=\"nightly\""
  "STYIO_EDITION_MAX=\"2026\""
)
styio_apply_llvm_compile_settings(styio_syntax_check_internal_test)

add_executable(styio_source_map_test
  source_map_test.cpp
)
target_include_directories(styio_source_map_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(styio_source_map_test PRIVATE
  styio_frontend_core
  GTest::gtest_main
  ${LLVM_LIBS}
)
styio_apply_llvm_compile_settings(styio_source_map_test)

add_executable(styio_symbol_interner_test
  symbol_interner_test.cpp
)
target_include_directories(styio_symbol_interner_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(styio_symbol_interner_test PRIVATE
  styio_frontend_core
  GTest::gtest_main
  ${LLVM_LIBS}
)
styio_apply_llvm_compile_settings(styio_symbol_interner_test)

add_executable(styio_type_table_test
  type_table_test.cpp
)
target_include_directories(styio_type_table_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(styio_type_table_test PRIVATE
  styio_frontend_core
  GTest::gtest_main
  ${LLVM_LIBS}
)
styio_apply_llvm_compile_settings(styio_type_table_test)

add_executable(styio_runtime_scheduler_test
  runtime_scheduler_test.cpp
)
target_include_directories(styio_runtime_scheduler_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(styio_runtime_scheduler_test PRIVATE
  GTest::gtest_main
)
styio_apply_llvm_compile_settings(styio_runtime_scheduler_test)
gtest_discover_tests(styio_runtime_scheduler_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "runtime;unit;concurrency"
)

add_executable(styio_main_contract_test
  main_contract_test.cpp
)
target_link_libraries(styio_main_contract_test PRIVATE
  styio_core
  styio_cli_contract_core
  GTest::gtest_main
)
target_compile_definitions(styio_main_contract_test PRIVATE
  "STYIO_PROJECT_VERSION=\"${PROJECT_VERSION}\""
  "STYIO_RELEASE_CHANNEL=\"nightly\""
  "STYIO_EDITION_MAX=\"2026\""
  "STYIO_LLVM_DIR=\"${STYIO_DEFINE_LLVM_DIR}\""
  "STYIO_SOURCE_DIR=\"${STYIO_DEFINE_SOURCE_DIR}\""
  "STYIO_CMAKE_C_COMPILER=\"${STYIO_DEFINE_CMAKE_C_COMPILER}\""
  "STYIO_CMAKE_CXX_COMPILER=\"${STYIO_DEFINE_CMAKE_CXX_COMPILER}\""
)
styio_apply_llvm_compile_settings(styio_main_contract_test)

add_executable(styio_benchmark_evidence_test
  benchmark_evidence_test.cpp
)
target_include_directories(styio_benchmark_evidence_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/src"
)
target_link_libraries(styio_benchmark_evidence_test PRIVATE GTest::gtest_main)
styio_apply_llvm_compile_settings(styio_benchmark_evidence_test)
add_test(
  NAME styio_benchmark_evidence_gtest
  COMMAND "$<TARGET_FILE:styio_benchmark_evidence_test>"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(styio_benchmark_evidence_gtest PROPERTIES LABELS "benchmark;unit")

add_executable(styio_resource_topology_test
  resource_topology_test.cpp
)
target_link_libraries(styio_resource_topology_test PRIVATE styio_core GTest::gtest_main)
styio_apply_llvm_compile_settings(styio_resource_topology_test)

add_executable(styio_ide_test
  ide/styio_ide_test.cpp
  "${CMAKE_SOURCE_DIR}/src/StyioServices/StyioLSP/Server.cpp"
)
target_link_libraries(styio_ide_test PRIVATE styio_ide_core GTest::gtest_main ${LLVM_LIBS})
styio_apply_llvm_compile_settings(styio_ide_test)
if(STYIO_ENABLE_TREE_SITTER)
  target_compile_definitions(styio_ide_test PRIVATE STYIO_HAS_TREE_SITTER)
endif()

include(GoogleTest)
if(WIN32)
  # The historical aggregate styio_test binary still contains POSIX shell-heavy
  # CLI/package tests. Native Windows coverage for those flows is provided by
  # the portable Python CTest entries above; keep the target building and run
  # the pure unit slice here without requiring bash/Git Bash.
  add_test(
    NAME styio_test_windows_native_slice
    COMMAND "$<TARGET_FILE:styio_test>" "--gtest_filter=StyioTypes.*"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
  set_tests_properties(styio_test_windows_native_slice PROPERTIES
    LABELS "styio_pipeline;unit;windows_native")
else()
  gtest_discover_tests(styio_test
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PROPERTIES LABELS "styio_pipeline\\;unit"
  )
endif()

gtest_discover_tests(styio_algorithm_equivalence_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "styio_pipeline\\;algorithm_equivalence\\;unit"
)

gtest_discover_tests(styio_security_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "security\\;safety\\;lexer\\;ffi"
)

gtest_discover_tests(styio_typeinfer_internal_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "security\\;safety\\;unit"
)

gtest_discover_tests(styio_lowering_internal_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "security\\;safety\\;unit"
)

gtest_discover_tests(styio_newparser_internal_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "security\\;safety\\;unit"
)

gtest_discover_tests(styio_parser_internal_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "security\\;safety\\;unit"
)

gtest_discover_tests(styio_native_interop_internal_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "security\\;safety\\;unit"
)

gtest_discover_tests(styio_externlib_internal_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "security\\;safety\\;unit"
)

gtest_discover_tests(styio_codegen_internal_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "security\\;safety\\;unit"
)

gtest_discover_tests(styio_syntax_check_internal_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "security\\;safety\\;unit"
)

if(WIN32)
  add_test(
    NAME styio_main_contract_test_windows_native_slice
    COMMAND "$<TARGET_FILE:styio_main_contract_test>"
            "--gtest_filter=StyioMainContractSourceBuildInfo.*:StyioMainContract.EscapingDiagnosticsAndOptionMatchingStayStable:StyioMainContract.ConfigScalarsCommentsAndProjectConfigAreParsedConservatively:StyioMainContract.ConfigEnumFallbacksAndPathHelpersCoverBoundaryBranches:StyioMainContract.NanoConfigParsersHonorSectionsAliasesAndFailures:StyioMainContract.NanoProfileNameUsesProfileFieldAndStemFallback:StyioMainContract.NativeBuildArgsAndExecutableDiscoveryStayFailClosed:StyioMainContract.NativeBuildCompilerAndGeneratedArtifactsAreDeterministic:StyioMainContract.FrontendProfilerSerializesPhasesCountersAndWriteFailures"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
  set_tests_properties(styio_main_contract_test_windows_native_slice PROPERTIES
    LABELS "styio_pipeline;unit;windows_native")
else()
gtest_discover_tests(styio_main_contract_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "styio_pipeline\\;unit"
)
endif()

gtest_discover_tests(styio_benchmark_evidence_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "benchmark\\;unit"
)

gtest_discover_tests(styio_resource_topology_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "styio_pipeline\\;resource_topology\\;resource_topology\\;safety\\;unit"
)

gtest_discover_tests(styio_ide_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "ide\\;lsp\\;unit"
)

gtest_discover_tests(styio_source_map_test
  PROPERTIES LABELS "util\\;unit"
)

gtest_discover_tests(styio_symbol_interner_test
  PROPERTIES LABELS "session\\;unit"
)

gtest_discover_tests(styio_type_table_test
  PROPERTIES LABELS "session\\;unit"
)

option(STYIO_ENABLE_FUZZ "Build libFuzzer targets" OFF)
if(STYIO_ENABLE_FUZZ)
  add_subdirectory(fuzz)
endif()
