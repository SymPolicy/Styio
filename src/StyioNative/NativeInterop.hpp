#pragma once
#ifndef STYIO_NATIVE_INTEROP_H_
#define STYIO_NATIVE_INTEROP_H_

#include <cstddef>
#include <filesystem>
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
  bool internal_linkage = false;
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

struct NativeCommandResult {
  int exit_code = -1;
  std::string launch_error;

  bool ok() const {
    return exit_code == 0 && launch_error.empty();
  }
};

std::string normalize_abi(std::string abi);
std::string configured_native_toolchain_mode();
CompilerResolution resolve_compiler_for_abi(const std::string& abi);
std::vector<std::string> native_shared_compile_argv(
  const CompilerResolution& compiler,
  const std::filesystem::path& source_path,
  const std::filesystem::path& shared_path);
std::vector<std::string> native_object_compile_argv(
  const CompilerResolution& compiler,
  const std::string& abi,
  const std::filesystem::path& source_path,
  const std::filesystem::path& object_path);
std::string native_command_display(const std::vector<std::string>& argv);
NativeCommandResult run_native_command_to_log(
  const std::vector<std::string>& argv,
  const std::filesystem::path& log_path,
  bool capture_stdout);
NativeCommandResult run_native_command_to_logs(
  const std::vector<std::string>& argv,
  const std::filesystem::path& stdout_log_path,
  const std::filesystem::path& stderr_log_path);
std::vector<FunctionSignature> parse_function_signatures(const std::string& body);
std::vector<FunctionSignature> parse_function_signatures_for_block(
  const std::string& body,
  const std::vector<std::string>& source_paths);
StyioDataType styio_data_type_for_c_type(const CType& type);
std::string source_text_for_block(
  const std::string& abi,
  const std::string& body,
  const std::vector<std::string>& source_paths);

LoadedBlock compile_and_load_block(
  const std::string& abi,
  const std::string& body,
  const std::vector<std::string>& export_symbols);

LoadedBlock compile_and_load_block(
  const std::string& abi,
  const std::string& body,
  const std::vector<std::string>& source_paths,
  const std::vector<std::string>& export_symbols);

void close_loaded_block(void* handle);

}  // namespace styio::native

#endif  // STYIO_NATIVE_INTEROP_H_
