set(STYIO_BENCHMARK_ROOT "" CACHE STRING
    "Explicit path to the external styio-benchmark checkout")
option(STYIO_REQUIRE_EXTERNAL_BENCHMARK
       "Fail configure when STYIO_BENCHMARK_ROOT is unset or incomplete"
       OFF)

if(NOT STYIO_BENCHMARK_ROOT)
  if(STYIO_REQUIRE_EXTERNAL_BENCHMARK)
    message(FATAL_ERROR
      "STYIO_REQUIRE_EXTERNAL_BENCHMARK=ON requires an explicit "
      "-DSTYIO_BENCHMARK_ROOT=/path/to/styio-benchmark")
  endif()
  message(STATUS
    "[Styio] STYIO_BENCHMARK_ROOT is unset; benchmark targets are not registered.")
  return()
endif()

get_filename_component(STYIO_BENCHMARK_ROOT_RESOLVED
  "${STYIO_BENCHMARK_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
set(_styio_benchmark_required_files
  "workloads/core/manifest.json"
  "workloads/core/run-core.py"
  "styio-probes/bench_utils.hpp"
  "styio-probes/core_bench.cpp"
  "styio-probes/styio_soak_test.cpp"
  "styio-probes/styio_task_scheduler_perf_test.cpp"
  "tests/core_bench_json_smoke.py"
  "tests/core_benchmark_evidence_test.cpp")
set(_styio_benchmark_missing_files)
foreach(_styio_benchmark_file IN LISTS _styio_benchmark_required_files)
  if(NOT EXISTS "${STYIO_BENCHMARK_ROOT_RESOLVED}/${_styio_benchmark_file}")
    list(APPEND _styio_benchmark_missing_files "${_styio_benchmark_file}")
  endif()
endforeach()
if(_styio_benchmark_missing_files)
  list(JOIN _styio_benchmark_missing_files ", " _styio_benchmark_missing_text)
  if(STYIO_REQUIRE_EXTERNAL_BENCHMARK)
    message(FATAL_ERROR
      "STYIO_BENCHMARK_ROOT is incomplete; missing: ${_styio_benchmark_missing_text}")
  endif()
  message(STATUS
    "[Styio] External benchmark integration disabled; missing: ${_styio_benchmark_missing_text}")
  return()
endif()

set(STYIO_BENCHMARK_ROOT "${STYIO_BENCHMARK_ROOT_RESOLVED}" CACHE STRING
    "Explicit path to the external styio-benchmark checkout" FORCE)

include(GoogleTest)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(STYIO_BENCHMARK_RUNTIME_PATH_MODS)
macro(styio_add_benchmark_runtime_path path)
  if(NOT "${path}" STREQUAL "")
    list(APPEND STYIO_BENCHMARK_RUNTIME_PATH_MODS "PATH=path_list_prepend:${path}")
  endif()
endmacro()

styio_add_benchmark_runtime_path("$<TARGET_FILE_DIR:styio>")
if(DEFINED LLVM_TOOLS_BINARY_DIR)
  styio_add_benchmark_runtime_path("${LLVM_TOOLS_BINARY_DIR}")
endif()
if(DEFINED LLVM_BINARY_DIR)
  styio_add_benchmark_runtime_path("${LLVM_BINARY_DIR}")
endif()
if(DEFINED LLVM_DIR)
  get_filename_component(_styio_bench_llvm_cmake_dir "${LLVM_DIR}" ABSOLUTE)
  get_filename_component(_styio_bench_llvm_lib_dir "${_styio_bench_llvm_cmake_dir}/../.." ABSOLUTE)
  get_filename_component(_styio_bench_llvm_prefix "${_styio_bench_llvm_lib_dir}/.." ABSOLUTE)
  styio_add_benchmark_runtime_path("${_styio_bench_llvm_prefix}/bin")
endif()
list(JOIN STYIO_BENCHMARK_RUNTIME_PATH_MODS "\\;" STYIO_BENCHMARK_RUNTIME_PATH_MODS_ESCAPED)

set(STYIO_BENCHMARK_PROBE_DIR "${STYIO_BENCHMARK_ROOT}/styio-probes")
set(STYIO_BENCHMARK_CORE_DIR "${STYIO_BENCHMARK_ROOT}/workloads/core")
message(STATUS "[Styio] Benchmark sources: ${STYIO_BENCHMARK_ROOT}")

add_executable(styio_core_bench
  "${STYIO_BENCHMARK_PROBE_DIR}/core_bench.cpp")
target_link_libraries(styio_core_bench PRIVATE styio_core)
target_include_directories(styio_core_bench PRIVATE
  "${STYIO_BENCHMARK_PROBE_DIR}"
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_BINARY_DIR}/generated")
styio_apply_llvm_compile_settings(styio_core_bench)
if(NOT CMAKE_BUILD_TYPE)
  set_target_properties(styio_core_bench PROPERTIES COMPILE_FLAGS "-O2 -DNDEBUG")
endif()

add_test(NAME styio_core_benchmark_internal
  COMMAND $<TARGET_FILE:styio_core_bench> --stdout)
set_tests_properties(styio_core_benchmark_internal PROPERTIES
  LABELS "benchmark;performance;core_internal"
  ENVIRONMENT "STYIO_BENCH_WARMUP=1;STYIO_BENCH_MEASURED=3")

set(STYIO_CORE_BENCHMARK_RESULT
    "${CMAKE_BINARY_DIR}/benchmark/core/core-benchmark-result.json")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/benchmark/core")
add_test(NAME styio_core_benchmark_json_output
  COMMAND $<TARGET_FILE:styio_core_bench> --output "${STYIO_CORE_BENCHMARK_RESULT}")
set_tests_properties(styio_core_benchmark_json_output PROPERTIES
  LABELS "benchmark;performance;core_internal"
  ENVIRONMENT "STYIO_BENCH_WARMUP=1;STYIO_BENCH_MEASURED=3")

add_test(NAME styio_core_benchmark_json_contract
  COMMAND "${Python3_EXECUTABLE}"
          "${STYIO_BENCHMARK_ROOT}/tests/core_bench_json_smoke.py"
          --binary "$<TARGET_FILE:styio_core_bench>"
          --output "${CMAKE_BINARY_DIR}/benchmark/core/core-benchmark-contract.json"
          --warmup 1 --measured 1)
set_tests_properties(styio_core_benchmark_json_contract PROPERTIES
  LABELS "benchmark;performance;core_internal")

add_executable(styio_core_benchmark_evidence_test
  "${STYIO_BENCHMARK_ROOT}/tests/core_benchmark_evidence_test.cpp")
target_link_libraries(styio_core_benchmark_evidence_test PRIVATE
  styio_core GTest::gtest_main)
target_include_directories(styio_core_benchmark_evidence_test PRIVATE
  "${STYIO_BENCHMARK_PROBE_DIR}"
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_BINARY_DIR}/generated")
styio_apply_llvm_compile_settings(styio_core_benchmark_evidence_test)
styio_gtest_discover_tests(styio_core_benchmark_evidence_test
  PROPERTIES LABELS "benchmark\\;performance\\;core_internal")

add_test(NAME styio_core_benchmark_smoke
  COMMAND "${Python3_EXECUTABLE}"
          "${STYIO_BENCHMARK_CORE_DIR}/run-core.py"
          --repo-root "${STYIO_BENCHMARK_ROOT}"
          --manifest "${STYIO_BENCHMARK_CORE_DIR}/manifest.json"
          --styio "$<TARGET_FILE:styio>"
          --iterations 1
          --output "${CMAKE_BINARY_DIR}/benchmark/core/core-workload-smoke.json")
set_tests_properties(styio_core_benchmark_smoke PROPERTIES
  LABELS "benchmark;performance;core_smoke"
  ENVIRONMENT_MODIFICATION "${STYIO_BENCHMARK_RUNTIME_PATH_MODS}")

if(WIN32)
  set_tests_properties(
    styio_core_benchmark_internal
    styio_core_benchmark_json_output
    styio_core_benchmark_json_contract
    PROPERTIES DISABLED TRUE)
endif()

if(STYIO_BENCHMARK_ROOT_RESOLVED)
  add_executable(styio_soak_test
    "${STYIO_BENCHMARK_PROBE_DIR}/styio_soak_test.cpp"
  )
  target_link_libraries(styio_soak_test PRIVATE styio_core GTest::gtest_main)
  styio_apply_llvm_compile_settings(styio_soak_test)
  target_compile_definitions(styio_soak_test PRIVATE
    "STYIO_SOURCE_DIR=\"${STYIO_DEFINE_SOURCE_DIR}\""
    "STYIO_COMPILER_EXE=\"$<TARGET_FILE:styio>\""
  )
  add_dependencies(styio_soak_test styio)
  styio_gtest_discover_tests(styio_soak_test
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PROPERTIES LABELS "benchmark\\;soak\\;soak_smoke\\;single_thread"
  )

  add_executable(styio_task_scheduler_perf_test
    "${STYIO_BENCHMARK_PROBE_DIR}/styio_task_scheduler_perf_test.cpp"
  )
  target_link_libraries(styio_task_scheduler_perf_test PRIVATE styio_core GTest::gtest_main)
  styio_apply_llvm_compile_settings(styio_task_scheduler_perf_test)
  styio_gtest_discover_tests(styio_task_scheduler_perf_test
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PROPERTIES LABELS "benchmark\\;task_runtime\\;performance"
  )

  # Nightly deep soak variants (same test binary, larger loop counts).
  add_test(
    NAME soak_deep_tokenizer
    COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.TokenizerIngestionLoop
  )
  set_tests_properties(soak_deep_tokenizer PROPERTIES
    LABELS "benchmark;soak;soak_deep;single_thread"
    ENVIRONMENT "STYIO_SOAK_LEXER_ITERS=5000;STYIO_SOAK_INGEST_LINES=4096"
  )

  add_test(
    NAME soak_deep_file_lifecycle
    COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.FileHandleLifecycleLoop
  )
  set_tests_properties(soak_deep_file_lifecycle PROPERTIES
    LABELS "benchmark;soak;soak_deep;single_thread"
    ENVIRONMENT "STYIO_SOAK_FILE_ITERS=12000;STYIO_SOAK_FILE_LINES=256"
  )

  add_test(
    NAME soak_deep_file_rss_guard
    COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.FileHandleMemoryGrowthBound
  )
  set_tests_properties(soak_deep_file_rss_guard PROPERTIES
    LABELS "benchmark;soak;soak_deep;single_thread"
    ENVIRONMENT "STYIO_SOAK_MEM_ITERS=8000;STYIO_SOAK_MEM_FILE_LINES=128;STYIO_SOAK_RSS_GROWTH_LIMIT_KIB=98304"
  )

  add_test(
    NAME soak_deep_concat_rss_guard
    COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.ConcatMemoryGrowthBound
  )
  set_tests_properties(soak_deep_concat_rss_guard PROPERTIES
    LABELS "benchmark;soak;soak_deep;single_thread"
    ENVIRONMENT "STYIO_SOAK_CONCAT_ITERS=6000;STYIO_SOAK_CONCAT_CHAIN=16;STYIO_SOAK_CONCAT_SEG_BYTES=128;STYIO_SOAK_CONCAT_RSS_GROWTH_LIMIT_KIB=98304"
  )

  add_test(
    NAME soak_deep_invalid_handle_diag
    COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.InvalidHandleDiagnosticsLoop
  )
  set_tests_properties(soak_deep_invalid_handle_diag PROPERTIES
    LABELS "benchmark;soak;soak_deep;single_thread"
    ENVIRONMENT "STYIO_SOAK_INVALID_HANDLE_ITERS=120000"
  )

  add_test(
    NAME soak_deep_stream_program
    COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.StreamProgramLoop
  )
  set_tests_properties(soak_deep_stream_program PROPERTIES
    LABELS "benchmark;soak;soak_deep;single_thread"
    ENVIRONMENT "STYIO_SOAK_STREAM_ITERS=1500"
  )

  add_test(
    NAME soak_deep_state_inline_program
    COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.StateInlineHelperProgramLoop
  )
  set_tests_properties(soak_deep_state_inline_program PROPERTIES
    LABELS "benchmark;soak;soak_deep;single_thread"
    ENVIRONMENT "STYIO_SOAK_STATE_INLINE_ITERS=1500"
  )

  add_test(
    NAME soak_deep_state_inline_matchcases_program
    COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.StateInlineMatchCasesProgramLoop
  )
  set_tests_properties(soak_deep_state_inline_matchcases_program PROPERTIES
    LABELS "benchmark;soak;soak_deep;single_thread"
    ENVIRONMENT "STYIO_SOAK_STATE_MATCH_ITERS=1500"
  )

  add_test(
    NAME soak_deep_state_inline_infinite_program
    COMMAND $<TARGET_FILE:styio_soak_test> --gtest_filter=StyioSoakSingleThread.StateInlineInfiniteProgramLoop
  )
  set_tests_properties(soak_deep_state_inline_infinite_program PROPERTIES
    LABELS "benchmark;soak;soak_deep;single_thread"
    ENVIRONMENT "STYIO_SOAK_STATE_INFINITE_ITERS=1500"
  )
endif()
