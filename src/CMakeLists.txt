include("${CMAKE_CURRENT_LIST_DIR}/cmake/StyioTargetHelpers.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/StyioFrontendSources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/StyioBackendSources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/StyioIDESources.cmake")

add_library(styio_symbol_core STATIC ${STYIO_SYMBOL_SOURCES})
styio_configure_library_target(styio_symbol_core)

add_library(styio_frontend_core STATIC ${STYIO_FRONTEND_SOURCES})
styio_configure_library_target(styio_frontend_core)
target_link_libraries(styio_frontend_core PUBLIC styio_symbol_core ${LLVM_LIBS} ${CMAKE_DL_LIBS})

add_library(styio_runtime_core STATIC ${STYIO_RUNTIME_SUPPORT_SOURCES})
styio_configure_library_target(styio_runtime_core)
target_link_libraries(styio_runtime_core PUBLIC ${LLVM_LIBS})

add_library(styio_core STATIC ${STYIO_CORE_SOURCES})
styio_configure_library_target(styio_core)
target_link_libraries(styio_core PUBLIC styio_frontend_core styio_runtime_core ${LLVM_LIBS})

add_library(styio_cli_contract_core STATIC ${STYIO_CONTRACT_SOURCES})
styio_configure_library_target(styio_cli_contract_core)
target_link_libraries(styio_cli_contract_core PUBLIC styio_symbol_core ${LLVM_LIBS})

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

add_executable(styio main.cpp)
styio_configure_binary_target(styio)
target_compile_definitions(styio PRIVATE
  "STYIO_PROJECT_VERSION=\"${PROJECT_VERSION}\""
  "STYIO_RELEASE_CHANNEL=\"stable\""
  "STYIO_EDITION_MAX=\"2026\""
  "STYIO_LLVM_DIR=\"${LLVM_DIR}\""
)
target_link_libraries(styio PRIVATE styio_core styio_cli_contract_core)
install(TARGETS styio RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime)

add_executable(styio_lspd ${STYIO_LSP_SOURCES})
styio_configure_binary_target(styio_lspd)
target_link_libraries(styio_lspd PRIVATE styio_ide_core ${LLVM_LIBS})
install(TARGETS styio_lspd RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime)

if(STYIO_BUILD_NANO)
  set(STYIO_NANO_CORE_SOURCES
    ${STYIO_FRONTEND_SOURCES}
    ${STYIO_BACKEND_SOURCES}
    ${STYIO_RUNTIME_SUPPORT_SOURCES}
  )
  if(STYIO_NANO_INCLUDE_PIPELINE_CHECK)
    list(APPEND STYIO_NANO_CORE_SOURCES ${STYIO_TESTING_SUPPORT_SOURCES})
  endif()

  add_library(styio_nano_core STATIC ${STYIO_NANO_CORE_SOURCES})
  styio_configure_library_target(styio_nano_core)
  target_compile_definitions(styio_nano_core PRIVATE
    ${STYIO_NANO_COMPILE_DEFINITIONS}
  )
  target_link_libraries(styio_nano_core PUBLIC styio_symbol_core ${LLVM_LIBS})

  add_executable(styio_nano main.cpp)
  set_target_properties(styio_nano PROPERTIES OUTPUT_NAME "styio-nano")
  styio_configure_binary_target(styio_nano)
  target_compile_definitions(styio_nano PRIVATE
    ${STYIO_NANO_COMPILE_DEFINITIONS}
    "STYIO_PROJECT_VERSION=\"${PROJECT_VERSION}\""
    "STYIO_RELEASE_CHANNEL=\"nano\""
    "STYIO_EDITION_MAX=\"2026\""
    "STYIO_LLVM_DIR=\"${LLVM_DIR}\""
  )
  target_link_libraries(styio_nano PRIVATE styio_nano_core)
  install(TARGETS styio_nano RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime)

  if(STYIO_NANO_OPTIMIZE_FOR_SIZE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|AppleClang")
      target_compile_options(styio_nano_core PRIVATE -Os)
      target_compile_options(styio_nano PRIVATE -Os)
    elseif(MSVC)
      target_compile_options(styio_nano_core PRIVATE /O1)
      target_compile_options(styio_nano PRIVATE /O1)
    endif()
  endif()
endif()
