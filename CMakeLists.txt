cmake_minimum_required(VERSION 3.14)

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

# [Styio] Version
project(styio VERSION 0.0.1)

option(STYIO_USE_ICU "Enable ICU-backed Unicode handling for StyioUnicode and CLI option text" OFF)
option(STYIO_BUILD_NANO "Build the profile-pruned styio-nano compiler target" ON)
option(STYIO_NANO_OPTIMIZE_FOR_SIZE "Compile styio-nano with size-oriented optimization flags when supported" ON)
option(STYIO_ENABLE_TREE_SITTER "Enable the generated Tree-sitter syntax backend for styio_ide_core" ON)
set(STYIO_NANO_PROFILE "${CMAKE_SOURCE_DIR}/configs/styio-nano-default.toml" CACHE FILEPATH
    "Path to the styio-nano profile TOML used to configure the styio-nano target")

if(STYIO_ENABLE_TREE_SITTER)
  if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
  endif()

  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/grammar/tree-sitter-styio/src/parser.c")
    message(FATAL_ERROR
      "Tree-sitter grammar sources are missing. Run `npx --yes tree-sitter-cli@0.26.8 generate` "
      "in grammar/tree-sitter-styio or configure with -DSTYIO_ENABLE_TREE_SITTER=OFF.")
  endif()

  FetchContent_Declare(
    tree_sitter_runtime
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
    GIT_TAG v0.26.8
    GIT_SHALLOW TRUE
  )
  FetchContent_GetProperties(tree_sitter_runtime)
  if(NOT tree_sitter_runtime_POPULATED)
    FetchContent_Populate(tree_sitter_runtime)
  endif()

  file(GLOB STYIO_TREE_SITTER_RUNTIME_SOURCES CONFIGURE_DEPENDS
    "${tree_sitter_runtime_SOURCE_DIR}/lib/src/*.c"
  )
  list(REMOVE_ITEM STYIO_TREE_SITTER_RUNTIME_SOURCES
    "${tree_sitter_runtime_SOURCE_DIR}/lib/src/lib.c"
  )

  add_library(styio_tree_sitter_runtime STATIC ${STYIO_TREE_SITTER_RUNTIME_SOURCES})
  target_include_directories(styio_tree_sitter_runtime
    PUBLIC "${tree_sitter_runtime_SOURCE_DIR}/lib/include"
    PRIVATE "${tree_sitter_runtime_SOURCE_DIR}/lib/src"
            "${tree_sitter_runtime_SOURCE_DIR}/lib/src/wasm"
  )
  target_compile_definitions(styio_tree_sitter_runtime PRIVATE
    _POSIX_C_SOURCE=200112L
    _DEFAULT_SOURCE
    _BSD_SOURCE
    _DARWIN_C_SOURCE
  )
  set_target_properties(styio_tree_sitter_runtime PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
  )

  set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/grammar/tree-sitter-styio/src/parser.c"
    PROPERTIES LANGUAGE C
  )
  add_library(styio_tree_sitter_styio STATIC
    "${CMAKE_SOURCE_DIR}/grammar/tree-sitter-styio/src/parser.c"
  )
  target_include_directories(styio_tree_sitter_styio
    PUBLIC "${CMAKE_SOURCE_DIR}/grammar/tree-sitter-styio/src"
  )
  target_link_libraries(styio_tree_sitter_styio PUBLIC styio_tree_sitter_runtime)
endif()

# [Styio] Module Path
# set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

# [Styio] Build Output Directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

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

set(STYIO_SHARED_CORE_SOURCES
    src/StyioToken/Token.cpp
    src/StyioUnicode/Unicode.cpp
    src/StyioParser/Parser.cpp
    src/StyioParser/ParserLookahead.cpp
    src/StyioParser/NewParserExpr.cpp
    src/StyioParser/Tokenizer.cpp
    src/StyioToString/ToString.cpp
    src/StyioAnalyzer/TypeInfer.cpp
    src/StyioAnalyzer/ToStyioIR.cpp
)

set(STYIO_FRONTEND_SOURCES
    ${STYIO_SHARED_CORE_SOURCES}
)

set(STYIO_IDE_SOURCES
    src/StyioIDE/Common.cpp
    src/StyioIDE/VFS.cpp
    src/StyioIDE/CompilerBridge.cpp
    src/StyioIDE/Syntax.cpp
    src/StyioIDE/TreeSitterBackend.cpp
    src/StyioIDE/HIR.cpp
    src/StyioIDE/Project.cpp
    src/StyioIDE/Index.cpp
    src/StyioIDE/SemDB.cpp
    src/StyioIDE/Service.cpp
)

set(STYIO_CODEGEN_SOURCES
    src/StyioCodeGen/CodeGen.cpp
    src/StyioCodeGen/GetTypeG.cpp
    src/StyioCodeGen/CodeGenG.cpp
    src/StyioCodeGen/CodeGenPulse.cpp
    src/StyioCodeGen/GetTypeIO.cpp
    src/StyioCodeGen/CodeGenIO.cpp
    src/StyioExtern/ExternLib.cpp
)

set(STYIO_FULL_ONLY_SOURCES
    src/StyioTesting/PipelineCheck.cpp
)

set(STYIO_CORE_SOURCES
    ${STYIO_CODEGEN_SOURCES}
    ${STYIO_FULL_ONLY_SOURCES}
)

add_library(styio_frontend_core STATIC ${STYIO_FRONTEND_SOURCES})
target_include_directories(styio_frontend_core PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_include_directories(styio_frontend_core SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_frontend_core PRIVATE ${LLVM_DEFINITIONS_LIST})
target_link_libraries(styio_frontend_core PUBLIC ${LLVM_LIBS})

add_library(styio_core STATIC ${STYIO_CORE_SOURCES})
target_include_directories(styio_core PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_include_directories(styio_core SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_core PRIVATE ${LLVM_DEFINITIONS_LIST})
target_link_libraries(styio_core PUBLIC styio_frontend_core ${LLVM_LIBS})

add_library(styio_ide_core STATIC ${STYIO_IDE_SOURCES})
target_include_directories(styio_ide_core PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_include_directories(styio_ide_core SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_ide_core PRIVATE ${LLVM_DEFINITIONS_LIST})
target_link_libraries(styio_ide_core PUBLIC styio_frontend_core ${LLVM_LIBS})
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

add_executable(styio src/main.cpp)
target_include_directories(styio SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio PRIVATE ${LLVM_DEFINITIONS_LIST})
target_compile_definitions(styio PRIVATE
  "STYIO_PROJECT_VERSION=\"${PROJECT_VERSION}\""
  "STYIO_RELEASE_CHANNEL=\"stable\""
  "STYIO_EDITION_MAX=\"2026\""
  "STYIO_LLVM_DIR=\"${LLVM_DIR}\""
)
target_link_libraries(styio PRIVATE styio_core)

add_executable(styio_lspd
  src/StyioLSP/Server.cpp
  src/StyioLSP/main.cpp
)
target_include_directories(styio_lspd SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(styio_lspd PRIVATE ${LLVM_DEFINITIONS_LIST})
target_link_libraries(styio_lspd PRIVATE styio_ide_core ${LLVM_LIBS})

if(STYIO_BUILD_NANO)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(STYIO_NANO_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
  set(STYIO_NANO_PROFILE_CMAKE "${STYIO_NANO_GENERATED_DIR}/styio_nano_profile.cmake")
  file(MAKE_DIRECTORY "${STYIO_NANO_GENERATED_DIR}")
  execute_process(
    COMMAND
      "${Python3_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/scripts/gen-styio-nano-profile.py"
      --input "${STYIO_NANO_PROFILE}"
      --cmake-out "${STYIO_NANO_PROFILE_CMAKE}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE STYIO_NANO_PROFILE_RESULT
    OUTPUT_VARIABLE STYIO_NANO_PROFILE_STDOUT
    ERROR_VARIABLE STYIO_NANO_PROFILE_STDERR
  )
  if(NOT STYIO_NANO_PROFILE_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to generate styio-nano profile from ${STYIO_NANO_PROFILE}: ${STYIO_NANO_PROFILE_STDERR}")
  endif()
  include("${STYIO_NANO_PROFILE_CMAKE}")

  set(STYIO_NANO_CORE_SOURCES ${STYIO_FRONTEND_SOURCES} ${STYIO_CODEGEN_SOURCES})
  if(STYIO_NANO_INCLUDE_PIPELINE_CHECK)
    list(APPEND STYIO_NANO_CORE_SOURCES ${STYIO_FULL_ONLY_SOURCES})
  endif()

  add_library(styio_nano_core STATIC ${STYIO_NANO_CORE_SOURCES})
  target_include_directories(styio_nano_core PUBLIC "${CMAKE_SOURCE_DIR}/src")
  target_include_directories(styio_nano_core SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
  target_compile_definitions(styio_nano_core PRIVATE
    ${LLVM_DEFINITIONS_LIST}
    ${STYIO_NANO_COMPILE_DEFINITIONS}
  )
  target_link_libraries(styio_nano_core PUBLIC ${LLVM_LIBS})

  add_executable(styio_nano src/main.cpp)
  set_target_properties(styio_nano PROPERTIES OUTPUT_NAME "styio-nano")
  target_include_directories(styio_nano SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
  target_compile_definitions(styio_nano PRIVATE
    ${LLVM_DEFINITIONS_LIST}
    ${STYIO_NANO_COMPILE_DEFINITIONS}
    "STYIO_PROJECT_VERSION=\"${PROJECT_VERSION}\""
    "STYIO_RELEASE_CHANNEL=\"nano\""
    "STYIO_EDITION_MAX=\"2026\""
    "STYIO_LLVM_DIR=\"${LLVM_DIR}\""
  )
  target_link_libraries(styio_nano PRIVATE styio_nano_core)

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

if(STYIO_USE_ICU)
  # ICU Components Reference:
  # https://cmake.org/cmake/help/latest/module/FindICU.html
  # ICU components = data, i18n, io, le, lx, test, tu and uc.
  message(STATUS "[ICU] Enabled via STYIO_USE_ICU=ON; using FindICU.cmake in: ${PROJECT_SOURCE_DIR}")
  find_package(ICU COMPONENTS uc i18n REQUIRED)
  target_link_libraries(styio_frontend_core PUBLIC ICU::uc ICU::i18n)
  target_compile_definitions(styio_frontend_core PRIVATE STYIO_USE_ICU)
  target_link_libraries(styio_core PUBLIC ICU::uc ICU::i18n)
  target_compile_definitions(styio_core PRIVATE STYIO_USE_ICU)
  target_link_libraries(styio_ide_core PUBLIC ICU::uc ICU::i18n)
  target_compile_definitions(styio_ide_core PRIVATE STYIO_USE_ICU)
  target_compile_definitions(styio PRIVATE CXXOPTS_USE_UNICODE)
  if(STYIO_BUILD_NANO)
    target_link_libraries(styio_nano_core PUBLIC ICU::uc ICU::i18n)
    target_compile_definitions(styio_nano_core PRIVATE STYIO_USE_ICU)
    target_compile_definitions(styio_nano PRIVATE CXXOPTS_USE_UNICODE)
  endif()
else()
  message(STATUS "[ICU] Disabled (STYIO_USE_ICU=OFF). Building without ICU linkage.")
endif()

# [CTest: Sub-Directory]
enable_testing()
add_subdirectory(tests)
add_subdirectory(benchmark)
