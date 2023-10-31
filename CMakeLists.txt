cmake_minimum_required(VERSION 3.14)

# [C++ Standard]: C++ 20
set(CMAKE_CXX_STANDARD 20)

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
find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

message(STATUS "[LLVM] Version: ${LLVM_PACKAGE_VERSION}")
message(STATUS "[LLVM] Using LLVMConfig.cmake in: ${LLVM_DIR}")

# llvm-config-${LLVM_VERSION} --components
llvm_map_components_to_libnames(LLVM_LIBS support core irreader)

# [ICU]
# ICU Components Reference:
# https://cmake.org/cmake/help/latest/module/FindICU.html
# ICU components = data, i18n, io, le, lx, test, tu and uc.
message(STATUS "[ICU] Using FindICU.cmake in: ${PROJECT_SOURCE_DIR}")
find_package(ICU COMPONENTS uc i18n REQUIRED)

# [Styio] Executable
add_executable(styio 
    src/main.cpp
    # Utilities
    src/include/StyioUtil/Util.cpp
    # Lexer (Token)
    src/include/StyioToken/Token.cpp
    # Parser (AST)
    src/include/StyioAST/ASTToString.cpp
    src/include/StyioParser/Parser.cpp
    # Visitor (AST)
    src/include/StyioVisitors/TypeChecker.cpp
    src/include/StyioVisitors/LLVMGenerator.cpp
    )

# [Styio - ICU4C] Link
target_link_libraries(
    styio 
    ICU::uc ICU::i18n)

# [Styio ~ LLVM] Link
target_link_libraries(
    styio 
    ${LLVM_LIBS})

# [CTest: Sub-Directory]
add_subdirectory(test)