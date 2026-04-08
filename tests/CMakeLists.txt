include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# All milestone .styio sources live under tests/milestones/{m1,m2,...}
set(STYIO_MS "${CMAKE_CURRENT_LIST_DIR}/milestones")
set(STYIO_EXE "$<TARGET_FILE:styio>")

# --- Helper: stdout golden (styio --file X | cmp -s - expected/X.out) ---
function(styio_stdout_golden_test MS_DIR TEST_GLOB LABEL)
  file(GLOB _STYIO_FILES CONFIGURE_DEPENDS "${STYIO_MS}/${MS_DIR}/${TEST_GLOB}")
  list(SORT _STYIO_FILES)
  foreach(_F IN LISTS _STYIO_FILES)
    get_filename_component(_NAME ${_F} NAME_WE)
    set(_EXP "${STYIO_MS}/${MS_DIR}/expected/${_NAME}.out")
    if(NOT EXISTS "${_EXP}")
      message(WARNING "Missing golden: ${_EXP} (skip ${_NAME})")
      continue()
    endif()
    add_test(
      NAME "${MS_DIR}_${_NAME}"
      COMMAND bash -c "\"${STYIO_EXE}\" --file \"${_F}\" | cmp -s - \"${_EXP}\""
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties("${MS_DIR}_${_NAME}" PROPERTIES LABELS "milestone;${LABEL}")
  endforeach()
endfunction()

styio_stdout_golden_test(m1 "t*.styio" m1)
styio_stdout_golden_test(m2 "t*.styio" m2)
styio_stdout_golden_test(m3 "t*.styio" m3)
styio_stdout_golden_test(m4 "t*.styio" m4)
styio_stdout_golden_test(m6 "t*.styio" m6)
styio_stdout_golden_test(m8 "t*.styio" m8)

# --- M9: stdout golden tests (t01-t08, t12 via golden helper; stderr/mixed tests individually) ---
styio_stdout_golden_test(m9 "t01_*.styio" m9)
styio_stdout_golden_test(m9 "t02_*.styio" m9)
styio_stdout_golden_test(m9 "t03_*.styio" m9)
styio_stdout_golden_test(m9 "t04_*.styio" m9)
styio_stdout_golden_test(m9 "t05_*.styio" m9)
styio_stdout_golden_test(m9 "t06_*.styio" m9)
styio_stdout_golden_test(m9 "t07_*.styio" m9)
styio_stdout_golden_test(m9 "t08_*.styio" m9)
styio_stdout_golden_test(m9 "t12_*.styio" m9)

# M9 stderr tests: capture stderr and compare against .err golden
add_test(
  NAME m9_t09_stderr_string
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_MS}/m9/t09_stderr_string.styio\" 2>&1 >/dev/null | cmp -s - \"${STYIO_MS}/m9/expected/t09_stderr_string.err\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m9_t09_stderr_string PROPERTIES LABELS "milestone;m9")

add_test(
  NAME m9_t10_stderr_int
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_MS}/m9/t10_stderr_int.styio\" 2>&1 >/dev/null | cmp -s - \"${STYIO_MS}/m9/expected/t10_stderr_int.err\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m9_t10_stderr_int PROPERTIES LABELS "milestone;m9")

# M9 mixed stdout+stderr test: verify both streams separately
add_test(
  NAME m9_t11_mixed
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_MS}/m9/t11_mixed.styio\" 2>\"${CMAKE_BINARY_DIR}/m9_t11_stderr.tmp\" | cmp -s - \"${STYIO_MS}/m9/expected/t11_mixed.out\" && cmp -s \"${CMAKE_BINARY_DIR}/m9_t11_stderr.tmp\" \"${STYIO_MS}/m9/expected/t11_mixed.err\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m9_t11_mixed PROPERTIES LABELS "milestone;m9")

# --- M10: stdin tests (piped input) ---
add_test(
  NAME m10_t01_stdin_echo
  COMMAND bash -c "cat \"${STYIO_MS}/m10/data/echo_input.txt\" | \"${STYIO_EXE}\" --file \"${STYIO_MS}/m10/t01_stdin_echo.styio\" | cmp -s - \"${STYIO_MS}/m10/expected/t01_stdin_echo.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m10_t01_stdin_echo PROPERTIES LABELS "milestone;m10")

add_test(
  NAME m10_t02_stdin_pull
  COMMAND bash -c "cat \"${STYIO_MS}/m10/data/pull_input.txt\" | \"${STYIO_EXE}\" --file \"${STYIO_MS}/m10/t02_stdin_pull.styio\" | cmp -s - \"${STYIO_MS}/m10/expected/t02_stdin_pull.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m10_t02_stdin_pull PROPERTIES LABELS "milestone;m10")

add_test(
  NAME m10_t03_stdin_eof
  COMMAND bash -c "cat \"${STYIO_MS}/m10/data/empty_input.txt\" | \"${STYIO_EXE}\" --file \"${STYIO_MS}/m10/t03_stdin_eof.styio\" | cmp -s - \"${STYIO_MS}/m10/expected/t03_stdin_eof.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m10_t03_stdin_eof PROPERTIES LABELS "milestone;m10")

add_test(
  NAME m10_t04_stdin_multi_pull
  COMMAND bash -c "cat \"${STYIO_MS}/m10/data/multi_input.txt\" | \"${STYIO_EXE}\" --file \"${STYIO_MS}/m10/t04_stdin_multi_pull.styio\" | cmp -s - \"${STYIO_MS}/m10/expected/t04_stdin_multi_pull.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m10_t04_stdin_multi_pull PROPERTIES LABELS "milestone;m10")

add_test(
  NAME m10_t05_stdin_transform
  COMMAND bash -c "cat \"${STYIO_MS}/m10/data/transform_input.txt\" | \"${STYIO_EXE}\" --file \"${STYIO_MS}/m10/t05_stdin_transform.styio\" | cmp -s - \"${STYIO_MS}/m10/expected/t05_stdin_transform.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m10_t05_stdin_transform PROPERTIES LABELS "milestone;m10")

add_test(
  NAME m10_t06_stdin_mixed_output
  COMMAND bash -c "cat \"${STYIO_MS}/m10/data/mixed_input.txt\" | \"${STYIO_EXE}\" --file \"${STYIO_MS}/m10/t06_stdin_mixed_output.styio\" 2>\"${CMAKE_BINARY_DIR}/m10_t06_stderr.tmp\" | cmp -s - \"${STYIO_MS}/m10/expected/t06_stdin_mixed_output.out\" && cmp -s \"${CMAKE_BINARY_DIR}/m10_t06_stderr.tmp\" \"${STYIO_MS}/m10/expected/t06_stdin_mixed_output.err\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m10_t06_stdin_mixed_output PROPERTIES LABELS "milestone;m10")

# M10 semantic failures: compilation must abort (e*.styio)
file(GLOB M10_FAIL_TESTS CONFIGURE_DEPENDS "${STYIO_MS}/m10/e*.styio")
list(SORT M10_FAIL_TESTS)
foreach(_F IN LISTS M10_FAIL_TESTS)
  get_filename_component(_ENAME ${_F} NAME_WE)
  add_test(
    NAME "m10_err_${_ENAME}"
    COMMAND bash -c "if \"${STYIO_EXE}\" --file \"${_F}\"; then exit 1; else exit 0; fi"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
  set_tests_properties("m10_err_${_ENAME}" PROPERTIES LABELS "milestone;m10;m10_semantic")
endforeach()

# M8 semantic failures: compilation must abort (e*.styio)
file(GLOB M8_FAIL_TESTS CONFIGURE_DEPENDS "${STYIO_MS}/m8/e*.styio")
list(SORT M8_FAIL_TESTS)
foreach(_F IN LISTS M8_FAIL_TESTS)
  get_filename_component(_ENAME ${_F} NAME_WE)
  add_test(
    NAME "m8_err_${_ENAME}"
    COMMAND bash -c "if \"${STYIO_EXE}\" --file \"${_F}\"; then exit 1; else exit 0; fi"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
  set_tests_properties("m8_err_${_ENAME}" PROPERTIES LABELS "milestone;m8;m8_semantic")
endforeach()

# --- M5: stdout goldens + special file / exit-code cases ---
add_test(
  NAME m5_t01_read_file
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_MS}/m5/t01_read_file.styio\" | cmp -s - \"${STYIO_MS}/m5/expected/t01_read_file.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m5_t01_read_file PROPERTIES LABELS "milestone;m5")

add_test(
  NAME m5_t02_write_file
  COMMAND bash -c "rm -f /tmp/styio_out.txt && \"${STYIO_EXE}\" --file \"${STYIO_MS}/m5/t02_write_file.styio\" && test \"$(cat /tmp/styio_out.txt)\" = \"Hello from Styio\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m5_t02_write_file PROPERTIES LABELS "milestone;m5")

add_test(
  NAME m5_t04_raii
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_MS}/m5/t04_raii.styio\" | cmp -s - \"${STYIO_MS}/m5/expected/t04_raii.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m5_t04_raii PROPERTIES LABELS "milestone;m5")

add_test(
  NAME m5_t05_auto_detect
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_MS}/m5/t05_auto_detect.styio\" | cmp -s - \"${STYIO_MS}/m5/expected/t05_auto_detect.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m5_t05_auto_detect PROPERTIES LABELS "milestone;m5")

add_test(
  NAME m5_t06_fail_fast
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_MS}/m5/t06_fail_fast.styio\"; test \$? -ne 0"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m5_t06_fail_fast PROPERTIES LABELS "milestone;m5")

add_test(
  NAME m5_t07_redirect
  COMMAND bash -c "rm -f /tmp/styio_val.txt && \"${STYIO_EXE}\" --file \"${STYIO_MS}/m5/t07_redirect.styio\" && test \"$(cat /tmp/styio_val.txt)\" = \"42\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m5_t07_redirect PROPERTIES LABELS "milestone;m5")

add_test(
  NAME m5_t08_pipe_func
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${STYIO_MS}/m5/t08_pipe_func.styio\" | cmp -s - \"${STYIO_MS}/m5/expected/t08_pipe_func.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m5_t08_pipe_func PROPERTIES LABELS "milestone;m5")

# --- M7: stdout or artifact golden ---
file(GLOB M7_STYIO_TESTS CONFIGURE_DEPENDS "${STYIO_MS}/m7/t*.styio")
list(SORT M7_STYIO_TESTS)
foreach(M7FILE IN LISTS M7_STYIO_TESTS)
  get_filename_component(M7NAME ${M7FILE} NAME_WE)
  if(M7NAME STREQUAL "t10_full_pipeline")
    add_test(
      NAME "m7_${M7NAME}"
      COMMAND bash -c "rm -f /tmp/styio_doubled.txt && \"${STYIO_EXE}\" --file \"${M7FILE}\" && cmp -s /tmp/styio_doubled.txt \"${STYIO_MS}/m7/expected/${M7NAME}.out\""
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
  else()
    add_test(
      NAME "m7_${M7NAME}"
      COMMAND bash -c "\"${STYIO_EXE}\" --file \"${M7FILE}\" | cmp -s - \"${STYIO_MS}/m7/expected/${M7NAME}.out\""
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
  endif()
  set_tests_properties("m7_${M7NAME}" PROPERTIES LABELS "milestone;m7")
endforeach()

add_test(
  NAME parser_shadow_gate_m1_zero_fallback_and_internal_bridges
  COMMAND bash -c "tmp_dir=\$(mktemp -d) && trap 'rm -rf \"\$tmp_dir\"' EXIT && \"${CMAKE_SOURCE_DIR}/scripts/parser-shadow-suite-gate.sh\" --require-zero-fallback --require-zero-internal-bridges \"${STYIO_EXE}\" \"${STYIO_MS}/m1\" \"\$tmp_dir\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_m1_zero_fallback_and_internal_bridges PROPERTIES
  LABELS "styio_pipeline;shadow_gate;m1"
)

add_test(
  NAME parser_shadow_gate_m2_zero_fallback_and_internal_bridges
  COMMAND bash -c "tmp_dir=\$(mktemp -d) && trap 'rm -rf \"\$tmp_dir\"' EXIT && \"${CMAKE_SOURCE_DIR}/scripts/parser-shadow-suite-gate.sh\" --require-zero-fallback --require-zero-internal-bridges \"${STYIO_EXE}\" \"${STYIO_MS}/m2\" \"\$tmp_dir\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_m2_zero_fallback_and_internal_bridges PROPERTIES
  LABELS "styio_pipeline;shadow_gate;m2"
)

add_test(
  NAME parser_shadow_gate_m5_dual_zero_expected_nonzero
  COMMAND bash -c "tmp_dir=\$(mktemp -d) && trap 'rm -rf \"\$tmp_dir\"' EXIT && \"${CMAKE_SOURCE_DIR}/scripts/parser-shadow-suite-gate.sh\" --require-zero-fallback --require-zero-internal-bridges \"${STYIO_EXE}\" \"${STYIO_MS}/m5\" \"\$tmp_dir\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_m5_dual_zero_expected_nonzero PROPERTIES
  LABELS "styio_pipeline;shadow_gate;m5"
)

add_test(
  NAME parser_shadow_gate_m7_zero_fallback
  COMMAND bash -c "tmp_dir=\$(mktemp -d) && trap 'rm -rf \"\$tmp_dir\"' EXIT && \"${CMAKE_SOURCE_DIR}/scripts/parser-shadow-suite-gate.sh\" --require-zero-fallback \"${STYIO_EXE}\" \"${STYIO_MS}/m7\" \"\$tmp_dir\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_m7_zero_fallback PROPERTIES
  LABELS "styio_pipeline;shadow_gate;m7"
)

add_test(
  NAME parser_shadow_gate_m7_zero_internal_bridges
  COMMAND bash -c "tmp_dir=\$(mktemp -d) && trap 'rm -rf \"\$tmp_dir\"' EXIT && \"${CMAKE_SOURCE_DIR}/scripts/parser-shadow-suite-gate.sh\" --require-zero-fallback --require-zero-internal-bridges \"${STYIO_EXE}\" \"${STYIO_MS}/m7\" \"\$tmp_dir\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(parser_shadow_gate_m7_zero_internal_bridges PROPERTIES
  LABELS "styio_pipeline;shadow_gate;m7"
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

add_executable(styio_security_test
  security/styio_security_test.cpp
)
target_link_libraries(styio_security_test PRIVATE styio_core GTest::gtest_main)
# Keep LLVM headers off all tests by default; only security test needs AST/session headers.
target_include_directories(styio_security_test SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_security_test PRIVATE ${LLVM_DEFINITIONS_LIST})

add_executable(styio_soak_test
  soak/styio_soak_test.cpp
)
target_link_libraries(styio_soak_test PRIVATE styio_core GTest::gtest_main)
target_compile_definitions(styio_soak_test PRIVATE
  "STYIO_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\""
  "STYIO_COMPILER_EXE=\"$<TARGET_FILE:styio>\""
)
add_dependencies(styio_soak_test styio)

include(GoogleTest)
gtest_discover_tests(styio_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "styio_pipeline\\;unit"
)

gtest_discover_tests(styio_security_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "security\\;safety\\;lexer\\;ffi"
)

gtest_discover_tests(styio_soak_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "soak\\;soak_smoke\\;single_thread"
)

# Nightly deep soak variants (same test binary, larger loop counts).
add_test(
  NAME soak_deep_tokenizer
  COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.TokenizerIngestionLoop
)
set_tests_properties(soak_deep_tokenizer PROPERTIES
  LABELS "soak;soak_deep;single_thread"
  ENVIRONMENT "STYIO_SOAK_LEXER_ITERS=5000;STYIO_SOAK_INGEST_LINES=4096"
)

add_test(
  NAME soak_deep_file_lifecycle
  COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.FileHandleLifecycleLoop
)
set_tests_properties(soak_deep_file_lifecycle PROPERTIES
  LABELS "soak;soak_deep;single_thread"
  ENVIRONMENT "STYIO_SOAK_FILE_ITERS=12000;STYIO_SOAK_FILE_LINES=256"
)

add_test(
  NAME soak_deep_file_rss_guard
  COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.FileHandleMemoryGrowthBound
)
set_tests_properties(soak_deep_file_rss_guard PROPERTIES
  LABELS "soak;soak_deep;single_thread"
  ENVIRONMENT "STYIO_SOAK_MEM_ITERS=8000;STYIO_SOAK_MEM_FILE_LINES=128;STYIO_SOAK_RSS_GROWTH_LIMIT_KIB=98304"
)

add_test(
  NAME soak_deep_concat_rss_guard
  COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.ConcatMemoryGrowthBound
)
set_tests_properties(soak_deep_concat_rss_guard PROPERTIES
  LABELS "soak;soak_deep;single_thread"
  ENVIRONMENT "STYIO_SOAK_CONCAT_ITERS=6000;STYIO_SOAK_CONCAT_CHAIN=16;STYIO_SOAK_CONCAT_SEG_BYTES=128;STYIO_SOAK_CONCAT_RSS_GROWTH_LIMIT_KIB=98304"
)

add_test(
  NAME soak_deep_invalid_handle_diag
  COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.InvalidHandleDiagnosticsLoop
)
set_tests_properties(soak_deep_invalid_handle_diag PROPERTIES
  LABELS "soak;soak_deep;single_thread"
  ENVIRONMENT "STYIO_SOAK_INVALID_HANDLE_ITERS=120000"
)

add_test(
  NAME soak_deep_stream_program
  COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.StreamProgramLoop
)
set_tests_properties(soak_deep_stream_program PROPERTIES
  LABELS "soak;soak_deep;single_thread"
  ENVIRONMENT "STYIO_SOAK_STREAM_ITERS=1500"
)

add_test(
  NAME soak_deep_state_inline_program
  COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.StateInlineHelperProgramLoop
)
set_tests_properties(soak_deep_state_inline_program PROPERTIES
  LABELS "soak;soak_deep;single_thread"
  ENVIRONMENT "STYIO_SOAK_STATE_INLINE_ITERS=1500"
)

add_test(
  NAME soak_deep_state_inline_matchcases_program
  COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.StateInlineMatchCasesProgramLoop
)
set_tests_properties(soak_deep_state_inline_matchcases_program PROPERTIES
  LABELS "soak;soak_deep;single_thread"
  ENVIRONMENT "STYIO_SOAK_STATE_MATCH_ITERS=1500"
)

add_test(
  NAME soak_deep_state_inline_infinite_program
  COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.StateInlineInfiniteProgramLoop
)
set_tests_properties(soak_deep_state_inline_infinite_program PROPERTIES
  LABELS "soak;soak_deep;single_thread"
  ENVIRONMENT "STYIO_SOAK_STATE_INFINITE_ITERS=1500"
)

option(STYIO_ENABLE_FUZZ "Build libFuzzer targets" OFF)
if(STYIO_ENABLE_FUZZ)
  add_subdirectory(fuzz)
endif()
