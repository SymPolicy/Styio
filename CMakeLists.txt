cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 20)

project(styio VERSION 0.0.1)

message(STATUS "[Styio] Source Directory: " ${PROJECT_SOURCE_DIR})
message(STATUS "[Styio] Binary Directory: " ${PROJECT_BINARY_DIR})

message(STATUS "[LLVM] Version: ${LLVM_PACKAGE_VERSION}")
message(STATUS "[LLVM] Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_executable(styio src/main.cpp)