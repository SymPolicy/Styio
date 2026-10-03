include("${CMAKE_CURRENT_LIST_DIR}/../StyioCLIContractSources.cmake")

add_library(styio_cli_contract_core STATIC ${STYIO_CONTRACT_SOURCES})
styio_configure_library_target(styio_cli_contract_core)
target_compile_definitions(styio_cli_contract_core PRIVATE
  "STYIO_PROJECT_VERSION=\"${PROJECT_VERSION}\""
  "STYIO_RELEASE_CHANNEL=\"nightly\""
  "STYIO_EDITION_MAX=\"2026\""
)
target_link_libraries(styio_cli_contract_core PUBLIC styio_observable_core styio_frontend_core styio_symbol_core ${LLVM_LIBS})
