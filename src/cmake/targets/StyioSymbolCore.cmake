include("${CMAKE_CURRENT_LIST_DIR}/../StyioSymbolSources.cmake")

add_library(styio_symbol_core STATIC ${STYIO_SYMBOL_SOURCES})
styio_configure_library_target(styio_symbol_core)
