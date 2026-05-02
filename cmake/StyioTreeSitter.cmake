if(NOT STYIO_ENABLE_TREE_SITTER)
  return()
endif()

if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_TREE_SITTER_RUNTIME OR
   NOT EXISTS "${FETCHCONTENT_SOURCE_DIR_TREE_SITTER_RUNTIME}/lib/include")
  set(_STYIO_TREE_SITTER_SOURCE_CANDIDATES
    "${CMAKE_BINARY_DIR}/_deps/tree_sitter_runtime-src"
    "${CMAKE_SOURCE_DIR}/build/default/_deps/tree_sitter_runtime-src"
    "${CMAKE_SOURCE_DIR}/build/ide-perf/_deps/tree_sitter_runtime-src"
    "${CMAKE_SOURCE_DIR}/build/fuzz/_deps/tree_sitter_runtime-src"
  )
  foreach(_candidate IN LISTS _STYIO_TREE_SITTER_SOURCE_CANDIDATES)
    if(EXISTS "${_candidate}/lib/include")
      set(FETCHCONTENT_SOURCE_DIR_TREE_SITTER_RUNTIME "${_candidate}" CACHE PATH
        "Local tree-sitter runtime source directory reused for offline configure" FORCE)
      break()
    endif()
  endforeach()
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
