add_executable(styio main.cpp)
styio_configure_binary_target(styio)
target_compile_definitions(styio PRIVATE
  "STYIO_PROJECT_VERSION=\"${PROJECT_VERSION}\""
  "STYIO_RELEASE_CHANNEL=\"nightly\""
  "STYIO_EDITION_MAX=\"2026\""
  "STYIO_LLVM_DIR=\"${STYIO_DEFINE_LLVM_DIR}\""
  "STYIO_SOURCE_DIR=\"${STYIO_DEFINE_SOURCE_DIR}\""
  "STYIO_CMAKE_C_COMPILER=\"${STYIO_DEFINE_CMAKE_C_COMPILER}\""
  "STYIO_CMAKE_CXX_COMPILER=\"${STYIO_DEFINE_CMAKE_CXX_COMPILER}\""
  "STYIO_CMAKE_MAKE_PROGRAM=\"${STYIO_DEFINE_CMAKE_MAKE_PROGRAM}\""
)
target_link_libraries(styio PRIVATE styio_core styio_cli_contract_core)
install(TARGETS styio RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime)
