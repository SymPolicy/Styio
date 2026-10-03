include("${CMAKE_CURRENT_LIST_DIR}/../StyioCoreSources.cmake")

add_library(styio_core STATIC ${STYIO_CORE_SOURCES})
styio_configure_library_target(styio_core)
target_link_libraries(styio_core PUBLIC styio_frontend_core styio_runtime_core ${LLVM_LIBS})
