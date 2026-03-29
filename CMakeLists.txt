cmake_minimum_required(VERSION 3.14)

# [clang]
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_C_COMPILER "/usr/bin/clang")
# set(CMAKE_C_FLAGS "-Wl,-export-dynamic")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

set(CMAKE_LINKER "/usr/bin/lld")
set(CMAKE_OBJDUMP "/usr/bin/llvm-objdump")

# set(CMAKE_EXE_LINKER_FLAGS "-Wl,-export-dynamic")

# [CMake]
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

# [Styio] Version
project(styio VERSION 0.0.1)

# [Styio] Module Path
# set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

# [Styio] Build Output Directory
set(PROJECT_BINARY_DIR "${PROJECT_SOURCE_DIR}/build")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}")

message(STATUS "[Styio] Source: " ${PROJECT_SOURCE_DIR})
message(STATUS "[Styio] Binary: " ${PROJECT_BINARY_DIR})
message(STATUS "[Styio] Archive: " ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
message(STATUS "[Styio] Library: " ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
message(STATUS "[Styio] Runtime: " ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# [LLVM] Configuration
find_package(LLVM 18.1.0 REQUIRED CONFIG)
# LLVM headers pollute the translation unit (e.g. cxxabi); keep them on styio_core/styio only
# so GoogleTest and other tools can compile without LLVM on the include path.
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

message(STATUS "[LLVM] Include Directory: ${LLVM_INCLUDE_DIRS}")
message(STATUS "[LLVM] Definitions: ${LLVM_DEFINITIONS_LIST}")

message(STATUS "[LLVM] Version: ${LLVM_PACKAGE_VERSION}")
message(STATUS "[LLVM] Using LLVMConfig.cmake in: ${LLVM_DIR}")

# llvm-config-${LLVM_VERSION} --components
# llvm-config-18 --components
llvm_map_components_to_libnames(LLVM_LIBS support core irreader orcjit native)

# [ICU]
# ICU Components Reference:
# https://cmake.org/cmake/help/latest/module/FindICU.html
# ICU components = data, i18n, io, le, lx, test, tu and uc.
message(STATUS "[ICU] Using FindICU.cmake in: ${PROJECT_SOURCE_DIR}")
find_package(ICU COMPONENTS uc i18n REQUIRED)

set(STYIO_CORE_SOURCES
    src/StyioToken/Token.cpp
    src/StyioParser/Parser.cpp
    src/StyioParser/Tokenizer.cpp
    src/StyioToString/ToString.cpp
    src/StyioAnalyzer/TypeInfer.cpp
    src/StyioAnalyzer/ToStyioIR.cpp
    src/StyioCodeGen/CodeGen.cpp
    src/StyioCodeGen/GetTypeG.cpp
    src/StyioCodeGen/CodeGenG.cpp
    src/StyioCodeGen/CodeGenPulse.cpp
    src/StyioCodeGen/GetTypeIO.cpp
    src/StyioCodeGen/CodeGenIO.cpp
    src/StyioExtern/ExternLib.cpp
    src/StyioTesting/PipelineCheck.cpp
)

add_library(styio_core STATIC ${STYIO_CORE_SOURCES})
target_include_directories(styio_core PUBLIC "${CMAKE_SOURCE_DIR}/src")
# Do not propagate LLVM headers to dependents (breaks GoogleTest / libc++abi mix).
target_include_directories(styio_core SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_core PRIVATE ${LLVM_DEFINITIONS_LIST})
target_link_libraries(styio_core PUBLIC ICU::uc ICU::i18n ${LLVM_LIBS})

add_executable(styio src/main.cpp)
target_include_directories(styio SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio PRIVATE ${LLVM_DEFINITIONS_LIST})
target_link_libraries(styio PRIVATE styio_core)

# [CTest: Sub-Directory]
enable_testing()
add_subdirectory(tests)