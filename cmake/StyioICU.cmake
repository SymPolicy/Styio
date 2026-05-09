if(STYIO_USE_ICU)
  message(STATUS "[ICU] Enabled via STYIO_USE_ICU=ON; using FindICU.cmake in: ${PROJECT_SOURCE_DIR}")
  find_package(ICU COMPONENTS uc i18n REQUIRED)

  foreach(_target IN ITEMS styio_frontend_core styio_core styio_ide_core)
    target_link_libraries(${_target} PUBLIC ICU::uc ICU::i18n)
    target_compile_definitions(${_target} PRIVATE STYIO_USE_ICU)
  endforeach()

  target_compile_definitions(styio PRIVATE CXXOPTS_USE_UNICODE)

  if(TARGET styio_nano_core)
    target_link_libraries(styio_nano_core PUBLIC ICU::uc ICU::i18n)
    target_compile_definitions(styio_nano_core PRIVATE STYIO_USE_ICU)
    target_compile_definitions(styio_nano PRIVATE CXXOPTS_USE_UNICODE)
  endif()
else()
  message(STATUS "[ICU] Disabled (STYIO_USE_ICU=OFF). Building without ICU linkage.")
endif()
