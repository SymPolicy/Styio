function(styio_native_toolchain_has_clang root out_result)
  if(EXISTS "${root}/bin/clang" AND EXISTS "${root}/bin/clang++")
    set(${out_result} TRUE PARENT_SCOPE)
    return()
  endif()
  if(EXISTS "${root}/bin/clang-18" AND EXISTS "${root}/bin/clang++-18")
    set(${out_result} TRUE PARENT_SCOPE)
    return()
  endif()
  set(${out_result} FALSE PARENT_SCOPE)
endfunction()

function(styio_resolve_native_toolchain_bundle out_root)
  set(candidate_roots)
  if(STYIO_NATIVE_TOOLCHAIN_BUNDLE_ROOT)
    list(APPEND candidate_roots "${STYIO_NATIVE_TOOLCHAIN_BUNDLE_ROOT}")
  endif()
  if(STYIO_NATIVE_TOOLCHAIN_ROOT)
    list(APPEND candidate_roots "${STYIO_NATIVE_TOOLCHAIN_ROOT}")
  endif()
  if(DEFINED LLVM_TOOLS_BINARY_DIR AND NOT LLVM_TOOLS_BINARY_DIR STREQUAL "")
    get_filename_component(llvm_tools_root "${LLVM_TOOLS_BINARY_DIR}/.." ABSOLUTE)
    list(APPEND candidate_roots "${llvm_tools_root}")
  endif()

  foreach(candidate IN LISTS candidate_roots)
    if(NOT candidate)
      continue()
    endif()
    get_filename_component(candidate_abs "${candidate}" ABSOLUTE)
    styio_native_toolchain_has_clang("${candidate_abs}" has_clang)
    if(has_clang)
      set(${out_root} "${candidate_abs}" PARENT_SCOPE)
      return()
    endif()
  endforeach()

  set(${out_root} "" PARENT_SCOPE)
endfunction()

if(STYIO_INSTALL_NATIVE_TOOLCHAIN)
  styio_resolve_native_toolchain_bundle(STYIO_RESOLVED_NATIVE_TOOLCHAIN_BUNDLE_ROOT)
  if(STYIO_RESOLVED_NATIVE_TOOLCHAIN_BUNDLE_ROOT)
    message(STATUS "[Styio] Native @extern clang+LLVM bundle: ${STYIO_RESOLVED_NATIVE_TOOLCHAIN_BUNDLE_ROOT}")
    install(
      DIRECTORY "${STYIO_RESOLVED_NATIVE_TOOLCHAIN_BUNDLE_ROOT}/"
      DESTINATION "${CMAKE_INSTALL_BINDIR}/${STYIO_NATIVE_TOOLCHAIN_RELATIVE_DIR}"
      USE_SOURCE_PERMISSIONS
      COMPONENT NativeToolchain
    )
  else()
    message(WARNING
      "[Styio] STYIO_INSTALL_NATIVE_TOOLCHAIN is ON, but no clang+LLVM root with bin/clang and bin/clang++ was found. "
      "Installed styio packages will still fall back to system cc/c++ in native toolchain auto mode.")
  endif()
endif()
