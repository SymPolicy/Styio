include("${CMAKE_CURRENT_LIST_DIR}/../StyioRuntimeSources.cmake")

add_library(styio_runtime_core STATIC ${STYIO_RUNTIME_SUPPORT_SOURCES})
styio_configure_library_target(styio_runtime_core)
target_link_libraries(styio_runtime_core PUBLIC ${LLVM_LIBS})
target_link_libraries(styio_runtime_core PUBLIC styio_observable_core)
