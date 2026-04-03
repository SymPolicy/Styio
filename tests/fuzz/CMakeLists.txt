if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(WARNING "STYIO_ENABLE_FUZZ is ON, but compiler is not Clang. Skipping fuzz targets.")
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
target_compile_options(styio_fuzz_lexer PRIVATE ${STYIO_FUZZ_FLAGS})
target_link_options(styio_fuzz_lexer PRIVATE ${STYIO_FUZZ_FLAGS})

add_executable(styio_fuzz_parser
  fuzz_parser.cpp
)
target_link_libraries(styio_fuzz_parser PRIVATE styio_core)
target_compile_options(styio_fuzz_parser PRIVATE ${STYIO_FUZZ_FLAGS})
target_link_options(styio_fuzz_parser PRIVATE ${STYIO_FUZZ_FLAGS})

add_test(
  NAME fuzz_lexer_smoke
  COMMAND styio_fuzz_lexer -runs=500 ${CMAKE_CURRENT_LIST_DIR}/corpus/lexer
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(fuzz_lexer_smoke PROPERTIES LABELS "fuzz;fuzz_smoke")

add_test(
  NAME fuzz_parser_smoke
  COMMAND styio_fuzz_parser -runs=500 ${CMAKE_CURRENT_LIST_DIR}/corpus/parser
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(fuzz_parser_smoke PROPERTIES LABELS "fuzz;fuzz_smoke")
