include(GoogleTest)

add_test(
  NAME docs_audit
  COMMAND python3 "${CMAKE_SOURCE_DIR}/scripts/docs-audit.py"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(docs_audit PROPERTIES LABELS "docs")

add_test(
  NAME bootstrap_dev_env_dependency_scope
  COMMAND bash -c "bash -n \"${CMAKE_SOURCE_DIR}/scripts/bootstrap-dev-env.sh\" && \"${CMAKE_SOURCE_DIR}/scripts/bootstrap-dev-env.sh\" --help | grep -q 'installs dependencies only' && \"${CMAKE_SOURCE_DIR}/scripts/bootstrap-dev-env.sh\" --help | grep -q 'does not configure, build, test' && grep -q 'Bootstrap scope:' \"${CMAKE_SOURCE_DIR}/docs/BUILD-AND-DEV-ENV.md\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(bootstrap_dev_env_dependency_scope PROPERTIES LABELS "docs;workflow")

# Language feature .styio sources live under tests/features/<feature>.
set(STYIO_FEATURES "${CMAKE_CURRENT_LIST_DIR}/features")
set(STYIO_EXE "$<TARGET_FILE:styio>")

add_test(
  NAME readme_glimpse_job_parallel_signal
  COMMAND bash -c "printf '101\\n94\\n' | \"${STYIO_EXE}\" --file \"${CMAKE_SOURCE_DIR}/example/job_parallel_signal.styio\" | cmp -s - \"${CMAKE_SOURCE_DIR}/example/expected/job_parallel_signal.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(readme_glimpse_job_parallel_signal PROPERTIES LABELS "docs;example;styio_pipeline")

add_test(
  NAME example_hello_world
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${CMAKE_SOURCE_DIR}/example/hello_world.styio\" | cmp -s - \"${CMAKE_SOURCE_DIR}/example/expected/hello_world.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(example_hello_world PROPERTIES LABELS "example;styio_pipeline")

add_test(
  NAME example_bubble_sort
  COMMAND bash -c "printf '[3, 1, 2]\\n' | \"${STYIO_EXE}\" --file \"${CMAKE_SOURCE_DIR}/example/algorithms/bubble_sort.styio\" | cmp -s - \"${CMAKE_SOURCE_DIR}/example/expected/bubble_sort.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(example_bubble_sort PROPERTIES LABELS "example;styio_pipeline;algorithm_equivalence")

add_test(
  NAME example_cli_calculator
  COMMAND bash -c "STYIO_BIN=\"${STYIO_EXE}\" \"${CMAKE_SOURCE_DIR}/example/cli_calculator.sh\" '1 + 2 * (3 + 4)' | cmp -s - \"${CMAKE_SOURCE_DIR}/example/expected/cli_calculator.out\""
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
    add_test(
      NAME "${LABEL}_${_NAME}"
      COMMAND bash -c "\"${STYIO_EXE}\" --file \"${_F}\" | cmp -s - \"${_EXP}\""
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties("${LABEL}_${_NAME}" PROPERTIES LABELS "language_feature;${LABEL}")
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
      COMMAND bash -c "tmp_err=\"${CMAKE_BINARY_DIR}/${LABEL}_${_ENAME}.stderr.tmp\"; tmp_out=\"${CMAKE_BINARY_DIR}/${LABEL}_${_ENAME}.stdout.tmp\"; if \"${STYIO_EXE}\" --file \"${_F}\" >\"\$tmp_out\" 2>\"\$tmp_err\"; then cat \"\$tmp_out\"; cat \"\$tmp_err\" >&2; exit 1; fi; grep -Fq -- \"$(cat \"${_EXP}\")\" \"\$tmp_err\""
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties("${LABEL}_err_${_ENAME}" PROPERTIES LABELS "language_feature;${LABEL};${LABEL}_semantic")
  endforeach()
endfunction()

styio_feature_stdout_golden_test(scalar_expressions "t*.styio" scalar_expressions)
styio_feature_stdout_golden_test(functions "t*.styio" functions)
styio_feature_stdout_golden_test(control_flow "t*.styio" control_flow)
styio_feature_stdout_golden_test(wave_dispatch "t*.styio" wave_dispatch)
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
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/stdio_output/t09_stderr_string.styio\" 2>&1 >/dev/null | cmp -s - \"${STYIO_FEATURES}/stdio_output/expected/t09_stderr_string.err\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(stdio_output_t09_stderr_string PROPERTIES LABELS "language_feature;stdio_output")

add_test(
  NAME stdio_output_t10_stderr_int
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/stdio_output/t10_stderr_int.styio\" 2>&1 >/dev/null | cmp -s - \"${STYIO_FEATURES}/stdio_output/expected/t10_stderr_int.err\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(stdio_output_t10_stderr_int PROPERTIES LABELS "language_feature;stdio_output")

# Mixed stdout+stderr test verifies both streams separately.
add_test(
  NAME stdio_output_t11_mixed
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/stdio_output/t11_mixed.styio\" 2>\"${CMAKE_BINARY_DIR}/stdio_output_t11_stderr.tmp\" | cmp -s - \"${STYIO_FEATURES}/stdio_output/expected/t11_mixed.out\" && cmp -s \"${CMAKE_BINARY_DIR}/stdio_output_t11_stderr.tmp\" \"${STYIO_FEATURES}/stdio_output/expected/t11_mixed.err\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(stdio_output_t11_mixed PROPERTIES LABELS "language_feature;stdio_output")

# --- stdin resource tests (piped input) ---
add_test(
  NAME stdio_input_t01_stdin_echo
  COMMAND bash -c "cat \"${STYIO_FEATURES}/stdio_input/data/echo_input.txt\" | \"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/stdio_input/t01_stdin_echo.styio\" | cmp -s - \"${STYIO_FEATURES}/stdio_input/expected/t01_stdin_echo.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(stdio_input_t01_stdin_echo PROPERTIES LABELS "language_feature;stdio_input")

add_test(
  NAME styio_build_native_executable_stdin_echo
  COMMAND bash -c "rm -f \"${CMAKE_BINARY_DIR}/styio-native-stdin-echo\" \"${CMAKE_BINARY_DIR}/styio-native-stdin-echo.profile.json\" && \"${STYIO_EXE}\" build \"${STYIO_FEATURES}/stdio_input/t01_stdin_echo.styio\" -o \"${CMAKE_BINARY_DIR}/styio-native-stdin-echo\" && test -x \"${CMAKE_BINARY_DIR}/styio-native-stdin-echo\" && cat \"${STYIO_FEATURES}/stdio_input/data/echo_input.txt\" | STYIO_NATIVE_PROFILE_OUT=\"${CMAKE_BINARY_DIR}/styio-native-stdin-echo.profile.json\" \"${CMAKE_BINARY_DIR}/styio-native-stdin-echo\" | cmp -s - \"${STYIO_FEATURES}/stdio_input/expected/t01_stdin_echo.out\" && grep -q '\"tool\": \"styio-native-profiler\"' \"${CMAKE_BINARY_DIR}/styio-native-stdin-echo.profile.json\" && grep -q '\"name\": \"execute\"' \"${CMAKE_BINARY_DIR}/styio-native-stdin-echo.profile.json\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(styio_build_native_executable_stdin_echo PROPERTIES
  LABELS "native_artifact;language_feature;stdio_input"
)

add_test(
  NAME stdio_input_t02_stdin_pull
  COMMAND bash -c "cat \"${STYIO_FEATURES}/stdio_input/data/pull_input.txt\" | \"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/stdio_input/t02_stdin_pull.styio\" | cmp -s - \"${STYIO_FEATURES}/stdio_input/expected/t02_stdin_pull.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(stdio_input_t02_stdin_pull PROPERTIES LABELS "language_feature;stdio_input")

add_test(
  NAME stdio_input_t03_stdin_eof
  COMMAND bash -c "cat \"${STYIO_FEATURES}/stdio_input/data/empty_input.txt\" | \"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/stdio_input/t03_stdin_eof.styio\" | cmp -s - \"${STYIO_FEATURES}/stdio_input/expected/t03_stdin_eof.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(stdio_input_t03_stdin_eof PROPERTIES LABELS "language_feature;stdio_input")

add_test(
  NAME stdio_input_t04_stdin_multi_pull
  COMMAND bash -c "cat \"${STYIO_FEATURES}/stdio_input/data/multi_input.txt\" | \"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/stdio_input/t04_stdin_multi_pull.styio\" | cmp -s - \"${STYIO_FEATURES}/stdio_input/expected/t04_stdin_multi_pull.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(stdio_input_t04_stdin_multi_pull PROPERTIES LABELS "language_feature;stdio_input")

add_test(
  NAME stdio_input_t05_stdin_transform
  COMMAND bash -c "cat \"${STYIO_FEATURES}/stdio_input/data/transform_input.txt\" | \"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/stdio_input/t05_stdin_transform.styio\" | cmp -s - \"${STYIO_FEATURES}/stdio_input/expected/t05_stdin_transform.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(stdio_input_t05_stdin_transform PROPERTIES LABELS "language_feature;stdio_input")

add_test(
  NAME stdio_input_t06_stdin_mixed_output
  COMMAND bash -c "cat \"${STYIO_FEATURES}/stdio_input/data/mixed_input.txt\" | \"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/stdio_input/t06_stdin_mixed_output.styio\" 2>\"${CMAKE_BINARY_DIR}/stdio_input_t06_stderr.tmp\" | cmp -s - \"${STYIO_FEATURES}/stdio_input/expected/t06_stdin_mixed_output.out\" && cmp -s \"${CMAKE_BINARY_DIR}/stdio_input_t06_stderr.tmp\" \"${STYIO_FEATURES}/stdio_input/expected/t06_stdin_mixed_output.err\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(stdio_input_t06_stdin_mixed_output PROPERTIES LABELS "language_feature;stdio_input")

add_test(
  NAME stdio_input_t07_stdin_typed_tuple_pull
  COMMAND bash -c "cat \"${STYIO_FEATURES}/stdio_input/data/typed_tuple_input.txt\" | \"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/stdio_input/t07_stdin_typed_tuple_pull.styio\" | cmp -s - \"${STYIO_FEATURES}/stdio_input/expected/t07_stdin_typed_tuple_pull.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(stdio_input_t07_stdin_typed_tuple_pull PROPERTIES LABELS "language_feature;stdio_input")

add_test(
  NAME stdio_input_t08_stdin_typed_f64_list_pull
  COMMAND bash -c "cat \"${STYIO_FEATURES}/stdio_input/data/typed_f64_list_input.txt\" | \"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/stdio_input/t08_stdin_typed_f64_list_pull.styio\" | cmp -s - \"${STYIO_FEATURES}/stdio_input/expected/t08_stdin_typed_f64_list_pull.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(stdio_input_t08_stdin_typed_f64_list_pull PROPERTIES LABELS "language_feature;stdio_input")

# Semantic failure tests must check the diagnostic contract, not only nonzero exit.
styio_feature_stderr_failure_golden_test(stdio_input stdio_input)
styio_feature_stderr_failure_golden_test(task_resources task_resources)
styio_feature_stderr_failure_golden_test(state_resources state_resources)
styio_feature_stderr_failure_golden_test(final_bindings final_bindings)

# --- file resource stdout goldens + special file / exit-code cases ---
add_test(
  NAME file_resources_t01_read_file
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/file_resources/t01_read_file.styio\" | cmp -s - \"${STYIO_FEATURES}/file_resources/expected/t01_read_file.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(file_resources_t01_read_file PROPERTIES LABELS "language_feature;file_resources")

add_test(
  NAME file_resources_t02_write_file
  COMMAND bash -c "rm -f /tmp/styio_out.txt && \"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/file_resources/t02_write_file.styio\" && test \"$(cat /tmp/styio_out.txt)\" = \"Hello from Styio\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(file_resources_t02_write_file PROPERTIES LABELS "language_feature;file_resources")

add_test(
  NAME file_resources_t04_raii
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/file_resources/t04_raii.styio\" | cmp -s - \"${STYIO_FEATURES}/file_resources/expected/t04_raii.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(file_resources_t04_raii PROPERTIES LABELS "language_feature;file_resources")

add_test(
  NAME file_resources_t05_auto_detect
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/file_resources/t05_auto_detect.styio\" | cmp -s - \"${STYIO_FEATURES}/file_resources/expected/t05_auto_detect.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(file_resources_t05_auto_detect PROPERTIES LABELS "language_feature;file_resources")

add_test(
  NAME file_resources_t06_fail_fast
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/file_resources/t06_fail_fast.styio\"; test \$? -ne 0"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(file_resources_t06_fail_fast PROPERTIES LABELS "language_feature;file_resources")

add_test(
  NAME file_resources_t07_redirect
  COMMAND bash -c "rm -f /tmp/styio_val.txt && \"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/file_resources/t07_redirect.styio\" && test \"$(cat /tmp/styio_val.txt)\" = \"42\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(file_resources_t07_redirect PROPERTIES LABELS "language_feature;file_resources")

add_test(
  NAME file_resources_t08_pipe_func
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_FEATURES}/file_resources/t08_pipe_func.styio\" | cmp -s - \"${STYIO_FEATURES}/file_resources/expected/t08_pipe_func.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
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
      COMMAND bash -c "rm -f /tmp/styio_doubled.txt && \"${STYIO_EXE}\" --file \"${STREAM_PROCESSING_FILE}\" && cmp -s /tmp/styio_doubled.txt \"${STYIO_FEATURES}/stream_processing/expected/${STREAM_PROCESSING_NAME}.out\""
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
  else()
    add_test(
      NAME "stream_processing_${STREAM_PROCESSING_NAME}"
      COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STREAM_PROCESSING_FILE}\" | cmp -s - \"${STYIO_FEATURES}/stream_processing/expected/${STREAM_PROCESSING_NAME}.out\""
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
  endif()
  set_tests_properties("stream_processing_${STREAM_PROCESSING_NAME}" PROPERTIES LABELS "language_feature;stream_processing")
endforeach()

add_test(
  NAME parser_shadow_gate_scalar_expressions_zero_fallback_and_internal_bridges
  COMMAND bash -c "tmp_dir=\$(mktemp -d) && trap 'rm -rf \"\$tmp_dir\"' EXIT && \"${CMAKE_SOURCE_DIR}/benchmark/parser-shadow-suite-gate.sh\" --require-zero-fallback --require-zero-internal-bridges \"${STYIO_EXE}\" \"${STYIO_FEATURES}/scalar_expressions\" \"\$tmp_dir\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_scalar_expressions_zero_fallback_and_internal_bridges PROPERTIES
  LABELS "styio_pipeline;shadow_gate;scalar_expressions"
)

add_test(
  NAME parser_shadow_gate_functions_zero_fallback_and_internal_bridges
  COMMAND bash -c "tmp_dir=\$(mktemp -d) && trap 'rm -rf \"\$tmp_dir\"' EXIT && \"${CMAKE_SOURCE_DIR}/benchmark/parser-shadow-suite-gate.sh\" --require-zero-fallback --require-zero-internal-bridges \"${STYIO_EXE}\" \"${STYIO_FEATURES}/functions\" \"\$tmp_dir\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_functions_zero_fallback_and_internal_bridges PROPERTIES
  LABELS "styio_pipeline;shadow_gate;functions"
)

add_test(
  NAME parser_shadow_gate_file_resources_dual_zero_expected_nonzero
  COMMAND bash -c "tmp_dir=\$(mktemp -d) && trap 'rm -rf \"\$tmp_dir\"' EXIT && \"${CMAKE_SOURCE_DIR}/benchmark/parser-shadow-suite-gate.sh\" --require-zero-fallback --require-zero-internal-bridges \"${STYIO_EXE}\" \"${STYIO_FEATURES}/file_resources\" \"\$tmp_dir\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_file_resources_dual_zero_expected_nonzero PROPERTIES
  LABELS "styio_pipeline;shadow_gate;file_resources"
)

add_test(
  NAME parser_shadow_gate_stream_processing_zero_fallback
  COMMAND bash -c "tmp_dir=\$(mktemp -d) && trap 'rm -rf \"\$tmp_dir\"' EXIT && \"${CMAKE_SOURCE_DIR}/benchmark/parser-shadow-suite-gate.sh\" --require-zero-fallback \"${STYIO_EXE}\" \"${STYIO_FEATURES}/stream_processing\" \"\$tmp_dir\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_stream_processing_zero_fallback PROPERTIES
  LABELS "styio_pipeline;shadow_gate;stream_processing"
)

add_test(
  NAME parser_shadow_gate_stream_processing_zero_internal_bridges
  COMMAND bash -c "tmp_dir=\$(mktemp -d) && trap 'rm -rf \"\$tmp_dir\"' EXIT && \"${CMAKE_SOURCE_DIR}/benchmark/parser-shadow-suite-gate.sh\" --require-zero-fallback --require-zero-internal-bridges \"${STYIO_EXE}\" \"${STYIO_FEATURES}/stream_processing\" \"\$tmp_dir\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_stream_processing_zero_internal_bridges PROPERTIES
  LABELS "styio_pipeline;shadow_gate;stream_processing"
)

add_test(
  NAME parser_legacy_entry_audit
  COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/parser-legacy-entry-audit.sh"
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
  "STYIO_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\""
  "STYIO_COMPILER_EXE=\"$<TARGET_FILE:styio>\""
)
add_dependencies(styio_test styio)
if(TARGET styio_nano)
  target_compile_definitions(styio_test PRIVATE
    "STYIO_NANO_COMPILER_EXE=\"$<TARGET_FILE:styio_nano>\""
  )
  add_dependencies(styio_test styio_nano)
endif()

add_executable(styio_algorithm_equivalence_test
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
  "STYIO_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\""
  "STYIO_COMPILER_EXE=\"$<TARGET_FILE:styio>\""
)
target_include_directories(styio_algorithm_equivalence_test PRIVATE "${CMAKE_SOURCE_DIR}")
add_dependencies(styio_algorithm_equivalence_test styio)

add_executable(styio_security_test
  security/styio_security_test.cpp
)
target_link_libraries(styio_security_test PRIVATE styio_core GTest::gtest_main)
# Security tests transitively include codegen/session headers that reference
# LLVM types. Use the shared helper so LLVM headers stay behind the standard
# library on Debian/libstdc++.
styio_apply_llvm_compile_settings(styio_security_test)

add_executable(styio_resource_topology_test
  resource_topology_test.cpp
)
target_link_libraries(styio_resource_topology_test PRIVATE styio_core GTest::gtest_main)
styio_apply_llvm_compile_settings(styio_resource_topology_test)

add_executable(styio_ide_test
  ide/styio_ide_test.cpp
  "${CMAKE_SOURCE_DIR}/src/StyioLSP/Server.cpp"
)
target_link_libraries(styio_ide_test PRIVATE styio_ide_core GTest::gtest_main ${LLVM_LIBS})
styio_apply_llvm_compile_settings(styio_ide_test)
if(STYIO_ENABLE_TREE_SITTER)
  target_compile_definitions(styio_ide_test PRIVATE STYIO_HAS_TREE_SITTER)
endif()

include(GoogleTest)
gtest_discover_tests(styio_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "styio_pipeline\\;unit"
)

gtest_discover_tests(styio_algorithm_equivalence_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "styio_pipeline\\;algorithm_equivalence\\;unit"
)

gtest_discover_tests(styio_security_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "security\\;safety\\;lexer\\;ffi"
)

gtest_discover_tests(styio_resource_topology_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "styio_pipeline\\;resource_topology\\;topology_v2\\;safety\\;unit"
)

gtest_discover_tests(styio_ide_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "ide\\;lsp\\;unit"
)

option(STYIO_ENABLE_FUZZ "Build libFuzzer targets" OFF)
if(STYIO_ENABLE_FUZZ)
  add_subdirectory(fuzz)
endif()
