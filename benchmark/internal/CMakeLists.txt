# styio_core_bench - in-repo C++ benchmark binary.
# Uses only std::chrono and the compiler itself; no third-party deps.

add_executable(styio_core_bench
  core_bench.cpp
)

target_link_libraries(styio_core_bench PRIVATE styio_core)

target_include_directories(styio_core_bench PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_BINARY_DIR}/generated"
)

styio_apply_llvm_compile_settings(styio_core_bench)

# Default: Release for consistent benchmarking. Override with -DCMAKE_BUILD_TYPE.
if(NOT CMAKE_BUILD_TYPE)
  set_target_properties(styio_core_bench PROPERTIES
    COMPILE_FLAGS "-O2 -DNDEBUG"
  )
endif()

add_test(
  NAME styio_core_benchmark_internal
  COMMAND $<TARGET_FILE:styio_core_bench> --stdout
)
set_tests_properties(styio_core_benchmark_internal PROPERTIES
  LABELS "benchmark;performance;core_internal"
  ENVIRONMENT "STYIO_BENCH_WARMUP=1;STYIO_BENCH_MEASURED=3"
)

add_test(
  NAME styio_core_benchmark_json_output
  COMMAND $<TARGET_FILE:styio_core_bench>
          --output "${CMAKE_BINARY_DIR}/benchmark/core/core-benchmark-result.json"
)
set_tests_properties(styio_core_benchmark_json_output PROPERTIES
  LABELS "benchmark;performance;core_internal"
  ENVIRONMENT "STYIO_BENCH_WARMUP=1;STYIO_BENCH_MEASURED=3"
)

if(WIN32)
  # The Python-driven core benchmark smoke remains enabled on Windows. These
  # in-process performance-only probes are disabled until the benchmark binary
  # has a Windows-stable teardown path.
  set_tests_properties(
    styio_core_benchmark_internal
    styio_core_benchmark_json_output
    PROPERTIES DISABLED TRUE)
endif()
