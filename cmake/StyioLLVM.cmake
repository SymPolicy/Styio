find_package(ZLIB QUIET)
find_package(zstd CONFIG QUIET)
find_package(LibXml2 QUIET)
find_package(LLVM 18.1.0 REQUIRED CONFIG)
if(LLVM_PACKAGE_VERSION VERSION_LESS "18.1.0"
   OR NOT LLVM_PACKAGE_VERSION VERSION_LESS "18.2.0")
  message(FATAL_ERROR
    "Styio requires LLVM 18.1.x; found ${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")
endif()

# CMake's LLVM dependency discovery can expose an Apple SDK's usr/include as
# an imported target include directory. Non-Apple Clang installations select
# their own SDK and resource headers internally; emitting a different SDK as
# -isystem puts C headers before Clang's wrappers and breaks libc++ include_next
# handling. SDK C headers are compiler-provided system headers, so mark every
# discovered SDK usr/include as implicit instead of forwarding it to targets.
if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  foreach(styio_sdk_include IN ITEMS
      "${ZLIB_INCLUDE_DIRS}"
      "${FFI_INCLUDE_DIRS}"
      "${LibEdit_INCLUDE_DIRS}")
    if(styio_sdk_include MATCHES "\\.sdk/usr/include$")
      list(APPEND CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES
        "${styio_sdk_include}")
      list(APPEND CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES
        "${styio_sdk_include}")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES)
  list(REMOVE_DUPLICATES CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
endif()

# LLVM headers ship a libc++abi-flavored cxxabi.h. On Debian + libstdc++,
# putting LLVM on the normal -I/-isystem search path lets internal libstdc++
# includes pick up the wrong cxxabi.h, which breaks GoogleTest and other
# host-side tooling. Keep LLVM reachable, but only after the standard library
# headers.
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

message(STATUS "[LLVM] Include Directory: ${LLVM_INCLUDE_DIRS}")
message(STATUS "[LLVM] Definitions: ${LLVM_DEFINITIONS_LIST}")
message(STATUS "[LLVM] Version: ${LLVM_PACKAGE_VERSION}")
message(STATUS "[LLVM] Using LLVMConfig.cmake in: ${LLVM_DIR}")

llvm_map_components_to_libnames(LLVM_LIBS support core irreader orcjit native ipo)

function(styio_apply_llvm_compile_settings target_name)
  if(MSVC)
    target_compile_options(${target_name} PRIVATE
      "$<$<COMPILE_LANGUAGE:C>:/utf-8>"
      "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>"
    )
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|AppleClang" AND NOT MSVC)
    foreach(llvm_include_dir IN LISTS LLVM_INCLUDE_DIRS)
      target_compile_options(${target_name} PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:SHELL:-idirafter ${llvm_include_dir}>"
        "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-idirafter ${llvm_include_dir}>"
      )
    endforeach()
  else()
    target_include_directories(${target_name} SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
  endif()

  target_compile_definitions(${target_name} PRIVATE ${LLVM_DEFINITIONS_LIST})
endfunction()
