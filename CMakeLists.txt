cmake_minimum_required(VERSION 3.20)

# [Build Type]
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# [CMake]
set(DOWNLOAD_EXTRACT_TIMESTAMP true)
include(FetchContent)
include(GNUInstallDirs)

# [Styio] Version
project(styio VERSION 0.0.1)

list(APPEND CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

option(STYIO_USE_ICU "Enable ICU-backed Unicode handling for StyioUnicode and CLI option text" OFF)
option(STYIO_BUILD_NANO "Build the profile-pruned styio-nano compiler target" ON)
option(STYIO_NANO_OPTIMIZE_FOR_SIZE "Compile styio-nano with size-oriented optimization flags when supported" ON)
option(STYIO_ENABLE_TREE_SITTER "Enable the generated Tree-sitter syntax backend for styio_ide_core" ON)
option(STYIO_INSTALL_NATIVE_TOOLCHAIN
       "Install a clang+LLVM bundle next to styio so @extern(c/c++) works without user compiler configuration"
       ON)
set(STYIO_NATIVE_TOOLCHAIN_MODE "auto" CACHE STRING
    "Native @extern compiler resolution: auto, bundled, or system")
set_property(CACHE STYIO_NATIVE_TOOLCHAIN_MODE PROPERTY STRINGS auto bundled system)
set(STYIO_NATIVE_TOOLCHAIN_ROOT "" CACHE PATH
    "Optional clang+LLVM toolchain root used by native @extern blocks; must contain bin/clang and bin/clang++")
set(STYIO_NATIVE_TOOLCHAIN_BUNDLE_ROOT "" CACHE PATH
    "Optional clang+LLVM root copied into installed styio packages; empty means auto-detect from LLVM_TOOLS_BINARY_DIR")
set(STYIO_NATIVE_TOOLCHAIN_RELATIVE_DIR "native-toolchain" CACHE STRING
    "Relative clang+LLVM bundle directory searched next to the styio executable")
set(STYIO_NANO_PROFILE "${CMAKE_SOURCE_DIR}/configs/styio-nano-default.toml" CACHE FILEPATH
    "Path to the styio-nano profile TOML used to configure the styio-nano target")

string(TOLOWER "${STYIO_NATIVE_TOOLCHAIN_MODE}" STYIO_NATIVE_TOOLCHAIN_MODE_NORMALIZED)
if(NOT STYIO_NATIVE_TOOLCHAIN_MODE_NORMALIZED MATCHES "^(auto|bundled|system)$")
  message(FATAL_ERROR "STYIO_NATIVE_TOOLCHAIN_MODE must be one of: auto, bundled, system")
endif()
set(STYIO_NATIVE_TOOLCHAIN_MODE "${STYIO_NATIVE_TOOLCHAIN_MODE_NORMALIZED}" CACHE STRING
    "Native @extern compiler resolution: auto, bundled, or system" FORCE)

configure_file(
  "${CMAKE_SOURCE_DIR}/src/StyioNative/NativeToolchainConfig.hpp.in"
  "${CMAKE_BINARY_DIR}/generated/StyioNative/NativeToolchainConfig.hpp"
  @ONLY
)

# [Styio] Build Output Directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

message(STATUS "[Styio] Source: " ${PROJECT_SOURCE_DIR})
message(STATUS "[Styio] Binary: " ${PROJECT_BINARY_DIR})
message(STATUS "[Styio] Archive: " ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
message(STATUS "[Styio] Library: " ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
message(STATUS "[Styio] Runtime: " ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

include(StyioLLVM)
include(StyioNativeToolchain)
include(StyioTreeSitter)
include(StyioNanoProfile)

add_subdirectory(src)

include(StyioICU)

enable_testing()
include(StyioGoogleTest)

add_subdirectory(tests)
add_subdirectory(benchmark)
