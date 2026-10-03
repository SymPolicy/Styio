include("${CMAKE_CURRENT_LIST_DIR}/../StyioFrontendSources.cmake")

add_library(styio_frontend_core STATIC ${STYIO_FRONTEND_SOURCES})
styio_configure_library_target(styio_frontend_core)
target_link_libraries(styio_frontend_core PUBLIC styio_symbol_core ${LLVM_LIBS} ${CMAKE_DL_LIBS})
