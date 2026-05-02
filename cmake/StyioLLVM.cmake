find_package(LLVM 18.1.0 REQUIRED CONFIG)

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

llvm_map_components_to_libnames(LLVM_LIBS support core irreader orcjit native)

function(styio_apply_llvm_compile_settings target_name)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|AppleClang")
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
