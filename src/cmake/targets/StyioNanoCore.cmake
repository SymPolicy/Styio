include("${CMAKE_CURRENT_LIST_DIR}/../StyioNanoCoreSources.cmake")

add_library(styio_nano_core STATIC ${STYIO_NANO_CORE_SOURCES})
styio_configure_library_target(styio_nano_core)
target_compile_definitions(styio_nano_core PRIVATE
  ${STYIO_NANO_COMPILE_DEFINITIONS}
)
target_link_libraries(styio_nano_core PUBLIC styio_symbol_core styio_observable_core ${LLVM_LIBS})

if(STYIO_NANO_OPTIMIZE_FOR_SIZE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|AppleClang" AND NOT MSVC)
    target_compile_options(styio_nano_core PRIVATE -Os)
  elseif(MSVC)
    target_compile_options(styio_nano_core PRIVATE "$<$<NOT:$<CONFIG:Debug>>:/O1>")
  endif()
endif()
