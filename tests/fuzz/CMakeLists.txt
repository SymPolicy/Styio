if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_custom_target(styio_fuzz_lexer)
  add_custom_target(styio_fuzz_parser)
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

add_executable(styio_fuzz_parser
  fuzz_parser.cpp
)
target_link_libraries(styio_fuzz_parser PRIVATE styio_core)
target_include_directories(styio_fuzz_parser SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_fuzz_parser PRIVATE ${LLVM_DEFINITIONS_LIST})
target_compile_options(styio_fuzz_parser PRIVATE ${STYIO_FUZZ_FLAGS})
target_link_options(styio_fuzz_parser PRIVATE ${STYIO_FUZZ_FLAGS})

add_test(
  NAME fuzz_lexer_smoke
  COMMAND
    bash ${CMAKE_CURRENT_LIST_DIR}/fuzz_smoke_runner.sh
    $<TARGET_FILE:styio_fuzz_lexer>
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
    $<TARGET_FILE:styio_fuzz_parser>
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
  NAME fuzz_regression_pack_smoke
  COMMAND bash ${CMAKE_CURRENT_LIST_DIR}/fuzz_regression_pack_smoke.sh
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(
  fuzz_regression_pack_smoke
  PROPERTIES
    LABELS "fuzz;fuzz_smoke;fuzz_regression")
