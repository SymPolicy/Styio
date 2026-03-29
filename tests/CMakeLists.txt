include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# All milestone .styio sources live under tests/milestones/{m1,m2,...}
set(STYIO_MS "${CMAKE_CURRENT_LIST_DIR}/milestones")
set(STYIO_EXE "${CMAKE_SOURCE_DIR}/styio")

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
  "STYIO_COMPILER_EXE=\"${CMAKE_SOURCE_DIR}/styio\""
)
add_dependencies(styio_test styio)

include(GoogleTest)
gtest_discover_tests(styio_test
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PROPERTIES LABELS "styio_pipeline;unit"
)
