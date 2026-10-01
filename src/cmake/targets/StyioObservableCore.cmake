include("${CMAKE_CURRENT_LIST_DIR}/../StyioObservableSources.cmake")

add_library(styio_observable_core STATIC ${STYIO_OBSERVABLE_PUBLIC_SOURCES})
target_include_directories(styio_observable_core PUBLIC
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_BINARY_DIR}/generated"
)
if(MSVC)
  target_compile_options(styio_observable_core PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>"
  )
endif()
