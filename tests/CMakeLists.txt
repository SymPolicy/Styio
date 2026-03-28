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

add_executable(styio_test
  styio_test.cpp
)
target_link_libraries(
  styio_test
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(styio_test)