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

# In-repo core benchmark binary (TASK-00).
add_subdirectory(internal)

set(STYIO_BENCHMARK_ROOT "" CACHE PATH
    "Path to the external styio-benchmark checkout that owns migrated benchmark probe sources")
option(STYIO_REQUIRE_EXTERNAL_BENCHMARK
       "Fail CMake configure when the external styio-benchmark checkout is unavailable"
       OFF)

set(STYIO_CORE_BENCHMARK_RESULT
    "${CMAKE_BINARY_DIR}/benchmark/core/core-benchmark-smoke.json")
add_test(
  NAME styio_core_benchmark_smoke
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/benchmark/core/run-core.py"
          --repo-root "${CMAKE_SOURCE_DIR}"
          --manifest "${CMAKE_SOURCE_DIR}/benchmark/core/manifest.json"
          --styio "$<TARGET_FILE:styio>"
          --iterations 1
          --output "${STYIO_CORE_BENCHMARK_RESULT}"
)
set_tests_properties(styio_core_benchmark_smoke PROPERTIES
  LABELS "benchmark;performance;core_smoke"
  ENVIRONMENT_MODIFICATION "${STYIO_BENCHMARK_RUNTIME_PATH_MODS}"
)

function(styio_resolve_benchmark_root out_var)
  set(candidates)
  if(STYIO_BENCHMARK_ROOT)
    list(APPEND candidates "${STYIO_BENCHMARK_ROOT}")
  endif()
  if(DEFINED ENV{STYIO_BENCHMARK_ROOT})
    list(APPEND candidates "$ENV{STYIO_BENCHMARK_ROOT}")
  endif()
  list(APPEND candidates
    "${CMAKE_SOURCE_DIR}/../styio-benchmark"
    "${CMAKE_SOURCE_DIR}/../../eBioRing/styio-benchmark"
    "$ENV{HOME}/DevSpace/eBioRing/styio-benchmark"
    "$ENV{HOME}/eBioRing/styio-benchmark"
  )

  foreach(candidate IN LISTS candidates)
    if(candidate STREQUAL "")
      continue()
    endif()
    get_filename_component(candidate_abs "${candidate}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    if(EXISTS "${candidate_abs}/styio-probes/styio_soak_test.cpp"
       AND EXISTS "${candidate_abs}/styio-probes/styio_task_scheduler_perf_test.cpp")
      set(${out_var} "${candidate_abs}" PARENT_SCOPE)
      return()
    endif()
  endforeach()

  set(${out_var} "" PARENT_SCOPE)
endfunction()

styio_resolve_benchmark_root(STYIO_BENCHMARK_ROOT_RESOLVED)
if(NOT STYIO_BENCHMARK_ROOT_RESOLVED)
  if(STYIO_REQUIRE_EXTERNAL_BENCHMARK)
    message(FATAL_ERROR
      "Styio benchmark probe sources have moved to styio-benchmark/styio-probes. "
      "Set -DSTYIO_BENCHMARK_ROOT=/path/to/styio-benchmark or export STYIO_BENCHMARK_ROOT.")
  endif()
  message(STATUS
    "[Styio] External styio-benchmark probes not found; only the in-repo core benchmark smoke will be registered.")
endif()

if(STYIO_BENCHMARK_ROOT_RESOLVED)
  set(STYIO_BENCHMARK_ROOT "${STYIO_BENCHMARK_ROOT_RESOLVED}" CACHE PATH
      "Path to the external styio-benchmark checkout that owns migrated benchmark probe sources" FORCE)
  set(STYIO_BENCHMARK_PROBE_DIR "${STYIO_BENCHMARK_ROOT}/styio-probes")
  message(STATUS "[Styio] Benchmark probes: ${STYIO_BENCHMARK_PROBE_DIR}")

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
  gtest_discover_tests(styio_soak_test
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PROPERTIES LABELS "benchmark\\;soak\\;soak_smoke\\;single_thread"
  )

  add_executable(styio_task_scheduler_perf_test
    "${STYIO_BENCHMARK_PROBE_DIR}/styio_task_scheduler_perf_test.cpp"
  )
  target_link_libraries(styio_task_scheduler_perf_test PRIVATE styio_core GTest::gtest_main)
  styio_apply_llvm_compile_settings(styio_task_scheduler_perf_test)
  gtest_discover_tests(styio_task_scheduler_perf_test
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
