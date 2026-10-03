include("${CMAKE_CURRENT_LIST_DIR}/../StyioLSPSources.cmake")

add_executable(styio_lspd ${STYIO_LSP_SOURCES})
styio_configure_binary_target(styio_lspd)
target_link_libraries(styio_lspd PRIVATE styio_ide_core ${LLVM_LIBS})
install(TARGETS styio_lspd RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime)
