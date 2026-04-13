add_executable(styio_soak_test
  styio_soak_test.cpp
)
target_link_libraries(styio_soak_test PRIVATE styio_core GTest::gtest_main)
target_include_directories(styio_soak_test SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_soak_test PRIVATE ${LLVM_DEFINITIONS_LIST})
target_compile_definitions(styio_soak_test PRIVATE
  "STYIO_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\""
  "STYIO_COMPILER_EXE=\"$<TARGET_FILE:styio>\""
)
add_dependencies(styio_soak_test styio)

include(GoogleTest)
gtest_discover_tests(styio_soak_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "benchmark\\;soak\\;soak_smoke\\;single_thread"
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
