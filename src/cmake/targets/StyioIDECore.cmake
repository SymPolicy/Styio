include("${CMAKE_CURRENT_LIST_DIR}/../StyioIDESources.cmake")

add_library(styio_ide_core STATIC ${STYIO_IDE_SOURCES})
styio_configure_library_target(styio_ide_core)
target_link_libraries(styio_ide_core PUBLIC styio_frontend_core styio_symbol_core ${LLVM_LIBS})

if(STYIO_ENABLE_TREE_SITTER)
  target_include_directories(styio_ide_core PRIVATE
    "${tree_sitter_runtime_SOURCE_DIR}/lib/include"
    "${CMAKE_SOURCE_DIR}/grammar/tree-sitter-styio/src"
  )
  target_compile_definitions(styio_ide_core PRIVATE STYIO_HAS_TREE_SITTER)
  target_link_libraries(styio_ide_core PUBLIC
    styio_tree_sitter_runtime
    styio_tree_sitter_styio
  )
endif()
