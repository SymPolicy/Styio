include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Milestone 3 (control flow): build `styio`, then `ctest -L m3` from `build/`
file(GLOB M3_STYIO_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/m3/t*.styio")
list(SORT M3_STYIO_TESTS)
foreach(M3FILE IN LISTS M3_STYIO_TESTS)
  get_filename_component(M3NAME ${M3FILE} NAME_WE)
  add_test(
    NAME "m3_${M3NAME}"
    COMMAND "${CMAKE_SOURCE_DIR}/styio" --file "${M3FILE}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
  set_tests_properties("m3_${M3NAME}" PROPERTIES LABELS "milestone;m3")
endforeach()

# Milestone 6 (state & pulse): `ctest -L m6` from `build/`
file(GLOB M6_STYIO_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/m6/t*.styio")
list(SORT M6_STYIO_TESTS)
foreach(M6FILE IN LISTS M6_STYIO_TESTS)
  get_filename_component(M6NAME ${M6FILE} NAME_WE)
  add_test(
    NAME "m6_${M6NAME}"
    COMMAND "${CMAKE_SOURCE_DIR}/styio" --file "${M6FILE}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
  set_tests_properties("m6_${M6NAME}" PROPERTIES LABELS "milestone;m6")
endforeach()

# Milestone 5 (resources & I/O): build `styio`, then `ctest -L m5` from `build/`
set(STYIO_EXE "${CMAKE_SOURCE_DIR}/styio")

add_test(
  NAME m5_t01_read_file
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${CMAKE_SOURCE_DIR}/tests/m5/t01_read_file.styio\" | cmp -s - \"${CMAKE_SOURCE_DIR}/tests/m5/expected/t01_read_file.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m5_t01_read_file PROPERTIES LABELS "milestone;m5")

add_test(
  NAME m5_t02_write_file
  COMMAND bash -c "rm -f /tmp/styio_out.txt && \"${STYIO_EXE}\" --file \"${CMAKE_SOURCE_DIR}/tests/m5/t02_write_file.styio\" && test \"$(cat /tmp/styio_out.txt)\" = \"Hello from Styio\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m5_t02_write_file PROPERTIES LABELS "milestone;m5")

add_test(
  NAME m5_t04_raii
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${CMAKE_SOURCE_DIR}/tests/m5/t04_raii.styio\" | cmp -s - \"${CMAKE_SOURCE_DIR}/tests/m5/expected/t04_raii.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m5_t04_raii PROPERTIES LABELS "milestone;m5")

add_test(
  NAME m5_t05_auto_detect
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${CMAKE_SOURCE_DIR}/tests/m5/t05_auto_detect.styio\" | cmp -s - \"${CMAKE_SOURCE_DIR}/tests/m5/expected/t05_auto_detect.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m5_t05_auto_detect PROPERTIES LABELS "milestone;m5")

add_test(
  NAME m5_t06_fail_fast
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${CMAKE_SOURCE_DIR}/tests/m5/t06_fail_fast.styio\"; test \$? -ne 0"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m5_t06_fail_fast PROPERTIES LABELS "milestone;m5")

add_test(
  NAME m5_t07_redirect
  COMMAND bash -c "rm -f /tmp/styio_val.txt && \"${STYIO_EXE}\" --file \"${CMAKE_SOURCE_DIR}/tests/m5/t07_redirect.styio\" && test \"$(cat /tmp/styio_val.txt)\" = \"42\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m5_t07_redirect PROPERTIES LABELS "milestone;m5")

add_test(
  NAME m5_t08_pipe_func
  COMMAND bash -c "\"${STYIO_EXE}\" --file \"${CMAKE_SOURCE_DIR}/tests/m5/t08_pipe_func.styio\" | cmp -s - \"${CMAKE_SOURCE_DIR}/tests/m5/expected/t08_pipe_func.out\""
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(m5_t08_pipe_func PROPERTIES LABELS "milestone;m5")

add_executable(styio_test
  styio_test.cpp
)
target_link_libraries(
  styio_test
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(styio_test)