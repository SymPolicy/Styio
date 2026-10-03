add_executable(styio_nano main.cpp)
set_target_properties(styio_nano PROPERTIES OUTPUT_NAME "styio-nano")
styio_configure_binary_target(styio_nano)
target_compile_definitions(styio_nano PRIVATE
  ${STYIO_NANO_COMPILE_DEFINITIONS}
  "STYIO_PROJECT_VERSION=\"${PROJECT_VERSION}\""
  "STYIO_RELEASE_CHANNEL=\"nano\""
  "STYIO_EDITION_MAX=\"2026\""
  "STYIO_LLVM_DIR=\"${STYIO_DEFINE_LLVM_DIR}\""
  "STYIO_SOURCE_DIR=\"${STYIO_DEFINE_SOURCE_DIR}\""
  "STYIO_CMAKE_C_COMPILER=\"${STYIO_DEFINE_CMAKE_C_COMPILER}\""
  "STYIO_CMAKE_CXX_COMPILER=\"${STYIO_DEFINE_CMAKE_CXX_COMPILER}\""
  "STYIO_CMAKE_MAKE_PROGRAM=\"${STYIO_DEFINE_CMAKE_MAKE_PROGRAM}\""
)
target_link_libraries(styio_nano PRIVATE styio_nano_core)
add_custom_target(styio-nano DEPENDS styio_nano)
install(TARGETS styio_nano RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime)

if(STYIO_NANO_OPTIMIZE_FOR_SIZE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|AppleClang" AND NOT MSVC)
    target_compile_options(styio_nano PRIVATE -Os)
  elseif(MSVC)
    target_compile_options(styio_nano PRIVATE "$<$<NOT:$<CONFIG:Debug>>:/O1>")
  endif()
endif()
