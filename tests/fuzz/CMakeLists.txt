if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_custom_target(styio_fuzz_lexer)
  add_custom_target(styio_fuzz_parser)
  add_custom_target(styio_fuzz_ide_syntax)
  add_custom_target(styio_fuzz_ide_completion)
  add_custom_target(styio_fuzz_ide_lsp_sync)
  add_custom_target(styio_fuzz_suite)
  message(WARNING "STYIO_ENABLE_FUZZ is ON, but compiler is not Clang. Skipping fuzz targets.")
  return()
endif()

include(CheckCXXSourceCompiles)
set(_OLD_REQ_FLAGS "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -fsanitize=fuzzer,address,undefined")
check_cxx_source_compiles(
  "int main() { return 0; }"
  STYIO_HAVE_LIBFUZZER_RUNTIME)
set(CMAKE_REQUIRED_FLAGS "${_OLD_REQ_FLAGS}")

if(NOT STYIO_HAVE_LIBFUZZER_RUNTIME)
  add_custom_target(styio_fuzz_lexer)
  add_custom_target(styio_fuzz_parser)
  add_custom_target(styio_fuzz_ide_syntax)
  add_custom_target(styio_fuzz_ide_completion)
  add_custom_target(styio_fuzz_ide_lsp_sync)
  add_custom_target(styio_fuzz_suite)
  message(WARNING "STYIO_ENABLE_FUZZ is ON, but libFuzzer runtime is unavailable with this toolchain. Skipping fuzz targets.")
  return()
endif()

set(STYIO_FUZZ_FLAGS
  -fsanitize=fuzzer,address,undefined
  -fno-omit-frame-pointer
)

add_executable(styio_fuzz_lexer
  fuzz_lexer.cpp
)
target_link_libraries(styio_fuzz_lexer PRIVATE styio_core)
target_include_directories(styio_fuzz_lexer SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_fuzz_lexer PRIVATE ${LLVM_DEFINITIONS_LIST})
target_compile_options(styio_fuzz_lexer PRIVATE ${STYIO_FUZZ_FLAGS})
target_link_options(styio_fuzz_lexer PRIVATE ${STYIO_FUZZ_FLAGS})

add_executable(styio_fuzz_lexer_smoke
  fuzz_smoke_main.cpp
  fuzz_lexer.cpp
)
target_link_libraries(styio_fuzz_lexer_smoke PRIVATE styio_core)
target_include_directories(styio_fuzz_lexer_smoke SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_fuzz_lexer_smoke PRIVATE ${LLVM_DEFINITIONS_LIST})

add_executable(styio_fuzz_parser
  fuzz_parser.cpp
)
target_link_libraries(styio_fuzz_parser PRIVATE styio_core)
target_include_directories(styio_fuzz_parser SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_fuzz_parser PRIVATE ${LLVM_DEFINITIONS_LIST})
target_compile_options(styio_fuzz_parser PRIVATE ${STYIO_FUZZ_FLAGS})
target_link_options(styio_fuzz_parser PRIVATE ${STYIO_FUZZ_FLAGS})

add_executable(styio_fuzz_parser_smoke
  fuzz_smoke_main.cpp
  fuzz_parser.cpp
)
target_link_libraries(styio_fuzz_parser_smoke PRIVATE styio_core)
target_include_directories(styio_fuzz_parser_smoke SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_fuzz_parser_smoke PRIVATE ${LLVM_DEFINITIONS_LIST})

add_executable(styio_fuzz_ide_syntax
  fuzz_ide_syntax.cpp
)
target_link_libraries(styio_fuzz_ide_syntax PRIVATE styio_ide_core ${LLVM_LIBS})
target_include_directories(styio_fuzz_ide_syntax SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_fuzz_ide_syntax PRIVATE ${LLVM_DEFINITIONS_LIST})
target_compile_options(styio_fuzz_ide_syntax PRIVATE ${STYIO_FUZZ_FLAGS})
target_link_options(styio_fuzz_ide_syntax PRIVATE ${STYIO_FUZZ_FLAGS})

add_executable(styio_fuzz_ide_syntax_smoke
  fuzz_smoke_main.cpp
  fuzz_ide_syntax.cpp
)
target_link_libraries(styio_fuzz_ide_syntax_smoke PRIVATE styio_ide_core ${LLVM_LIBS})
target_include_directories(styio_fuzz_ide_syntax_smoke SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_fuzz_ide_syntax_smoke PRIVATE ${LLVM_DEFINITIONS_LIST})

add_executable(styio_fuzz_ide_completion
  fuzz_ide_completion.cpp
)
target_link_libraries(styio_fuzz_ide_completion PRIVATE styio_ide_core ${LLVM_LIBS})
target_include_directories(styio_fuzz_ide_completion SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_fuzz_ide_completion PRIVATE ${LLVM_DEFINITIONS_LIST})
target_compile_options(styio_fuzz_ide_completion PRIVATE ${STYIO_FUZZ_FLAGS})
target_link_options(styio_fuzz_ide_completion PRIVATE ${STYIO_FUZZ_FLAGS})

add_executable(styio_fuzz_ide_completion_smoke
  fuzz_smoke_main.cpp
  fuzz_ide_completion.cpp
)
target_link_libraries(styio_fuzz_ide_completion_smoke PRIVATE styio_ide_core ${LLVM_LIBS})
target_include_directories(styio_fuzz_ide_completion_smoke SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_fuzz_ide_completion_smoke PRIVATE ${LLVM_DEFINITIONS_LIST})

add_executable(styio_fuzz_ide_lsp_sync
  fuzz_ide_lsp_sync.cpp
  "${CMAKE_SOURCE_DIR}/src/StyioLSP/Server.cpp"
)
target_link_libraries(styio_fuzz_ide_lsp_sync PRIVATE styio_ide_core ${LLVM_LIBS})
target_include_directories(styio_fuzz_ide_lsp_sync SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_fuzz_ide_lsp_sync PRIVATE ${LLVM_DEFINITIONS_LIST})
target_compile_options(styio_fuzz_ide_lsp_sync PRIVATE ${STYIO_FUZZ_FLAGS})
target_link_options(styio_fuzz_ide_lsp_sync PRIVATE ${STYIO_FUZZ_FLAGS})

add_executable(styio_fuzz_ide_lsp_sync_smoke
  fuzz_smoke_main.cpp
  fuzz_ide_lsp_sync.cpp
  "${CMAKE_SOURCE_DIR}/src/StyioLSP/Server.cpp"
)
target_link_libraries(styio_fuzz_ide_lsp_sync_smoke PRIVATE styio_ide_core ${LLVM_LIBS})
target_include_directories(styio_fuzz_ide_lsp_sync_smoke SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_fuzz_ide_lsp_sync_smoke PRIVATE ${LLVM_DEFINITIONS_LIST})

add_custom_target(styio_fuzz_suite
  DEPENDS
    styio_fuzz_lexer
    styio_fuzz_lexer_smoke
    styio_fuzz_parser
    styio_fuzz_parser_smoke
    styio_fuzz_ide_syntax
    styio_fuzz_ide_syntax_smoke
    styio_fuzz_ide_completion
    styio_fuzz_ide_completion_smoke
    styio_fuzz_ide_lsp_sync
    styio_fuzz_ide_lsp_sync_smoke
)

add_test(
  NAME fuzz_lexer_smoke
  COMMAND
    bash ${CMAKE_CURRENT_LIST_DIR}/fuzz_smoke_runner.sh
    $<TARGET_FILE:styio_fuzz_lexer_smoke>
    ${CMAKE_CURRENT_LIST_DIR}/corpus/lexer
    500
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(
  fuzz_lexer_smoke
  PROPERTIES
    LABELS "fuzz;fuzz_smoke"
    ENVIRONMENT "ASAN_OPTIONS=detect_container_overflow=0")

add_test(
  NAME fuzz_parser_smoke
  COMMAND
    bash ${CMAKE_CURRENT_LIST_DIR}/fuzz_smoke_runner.sh
    $<TARGET_FILE:styio_fuzz_parser_smoke>
    ${CMAKE_CURRENT_LIST_DIR}/corpus/parser
    500
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(
  fuzz_parser_smoke
  PROPERTIES
    LABELS "fuzz;fuzz_smoke"
    ENVIRONMENT "ASAN_OPTIONS=detect_container_overflow=0")

add_test(
  NAME StyioFuzzTargets.SyntaxCompletionAndLspSyncRemainStable
  COMMAND
    bash ${CMAKE_CURRENT_LIST_DIR}/fuzz_ide_targets_smoke.sh
    $<TARGET_FILE:styio_fuzz_ide_syntax_smoke>
    ${CMAKE_CURRENT_LIST_DIR}/corpus/ide_syntax
    $<TARGET_FILE:styio_fuzz_ide_completion_smoke>
    ${CMAKE_CURRENT_LIST_DIR}/corpus/ide_completion
    $<TARGET_FILE:styio_fuzz_ide_lsp_sync_smoke>
    ${CMAKE_CURRENT_LIST_DIR}/corpus/ide_lsp_sync
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(
  StyioFuzzTargets.SyntaxCompletionAndLspSyncRemainStable
  PROPERTIES
    LABELS "fuzz;fuzz_smoke;ide"
    ENVIRONMENT "ASAN_OPTIONS=detect_container_overflow=0")

add_test(
  NAME fuzz_regression_pack_smoke
  COMMAND bash ${CMAKE_CURRENT_LIST_DIR}/fuzz_regression_pack_smoke.sh
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(
  fuzz_regression_pack_smoke
  PROPERTIES
    LABELS "fuzz;fuzz_smoke;fuzz_regression")
