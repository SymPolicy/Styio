if(TARGET GTest::gtest_main)
  return()
endif()

if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_GOOGLETEST OR
   NOT EXISTS "${FETCHCONTENT_SOURCE_DIR_GOOGLETEST}/CMakeLists.txt")
  set(_STYIO_GOOGLETEST_SOURCE_CANDIDATES
    "${CMAKE_BINARY_DIR}/_deps/googletest-src"
    "${CMAKE_SOURCE_DIR}/build/default/_deps/googletest-src"
    "${CMAKE_SOURCE_DIR}/build/ide-perf/_deps/googletest-src"
    "${CMAKE_SOURCE_DIR}/build/fuzz/_deps/googletest-src"
  )
  foreach(_candidate IN LISTS _STYIO_GOOGLETEST_SOURCE_CANDIDATES)
    if(EXISTS "${_candidate}/CMakeLists.txt")
      set(FETCHCONTENT_SOURCE_DIR_GOOGLETEST "${_candidate}" CACHE PATH
        "Local googletest source directory reused for offline configure" FORCE)
      break()
    endif()
  endforeach()
endif()

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
  DOWNLOAD_EXTRACT_TIMESTAMP true
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)
