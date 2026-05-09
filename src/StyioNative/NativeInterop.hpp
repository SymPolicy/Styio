#pragma once
#ifndef STYIO_NATIVE_INTEROP_H_
#define STYIO_NATIVE_INTEROP_H_

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "../StyioToken/Token.hpp"

namespace styio::native {

enum class CTypeKind {
  Void,
  Bool,
  I8,
  I16,
  I32,
  I64,
  F32,
  F64,
  Pointer,
};

struct CType {
  CTypeKind kind = CTypeKind::I64;
  bool is_unsigned = false;
  std::string spelling;
};

struct FunctionParam {
  std::string name;
  CType type;
};

struct FunctionSignature {
  std::string name;
  CType return_type;
  std::vector<FunctionParam> params;
  bool variadic = false;
};

struct LoadedSymbol {
  std::string name;
  void* address = nullptr;
};

struct LoadedBlock {
  void* handle = nullptr;
  std::vector<FunctionSignature> functions;
  std::vector<LoadedSymbol> symbols;
};

struct CompilerResolution {
  std::string command;
  std::string source;
};

std::string normalize_abi(std::string abi);
std::string configured_native_toolchain_mode();
CompilerResolution resolve_compiler_for_abi(const std::string& abi);
std::vector<FunctionSignature> parse_function_signatures(const std::string& body);
StyioDataType styio_data_type_for_c_type(const CType& type);

LoadedBlock compile_and_load_block(
  const std::string& abi,
  const std::string& body,
  const std::vector<std::string>& export_symbols);

void close_loaded_block(void* handle);

}  // namespace styio::native

#endif  // STYIO_NATIVE_INTEROP_H_
