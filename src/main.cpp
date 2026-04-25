/*                                   */
/*    __| __ __| \ \  / _ _|   _ \   */
/*  \__ \    |    \  /    |   (   |  */
/*  ____/   _|     _|   ___| \___/   */
/*                                   */

// [C++ STL]
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// [Styio]
#include "StyioAST/AST.hpp"
#include "StyioAnalyzer/ASTAnalyzer.hpp"   /* StyioASTAnalyzer */
#include "StyioCodeGen/CodeGenVisitor.hpp" /* StyioToLLVMIR Code Generator */
#include "StyioException/Exception.hpp"
#include "StyioExtern/ExternLib.hpp"
#include "StyioIR/StyioIR.hpp" /* StyioIR */
#include "StyioParser/Parser.hpp"
#include "StyioParser/Tokenizer.hpp"
#include "StyioConfig/NanoProfile.hpp"
#include "StyioRuntime/HandleTable.hpp"
#include "StyioSession/CompilationSession.hpp"
#include "StyioToString/ToStringVisitor.hpp" /* StyioRepr */
#include "StyioToken/Token.hpp"
#include "StyioUtil/Util.hpp"

// [LLVM]
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Error.h" /* ExitOnErr */
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/JSON.h"

// [Styio LLVM ORC JIT]
#include "StyioJIT/StyioJIT_ORC.hpp"

// [Others]
#include "include/cxxopts.hpp" /* https://github.com/jarro2783/cxxopts */

#ifndef STYIO_PROJECT_VERSION
#define STYIO_PROJECT_VERSION "0.0.0-dev"
#endif

#ifndef STYIO_RELEASE_CHANNEL
#define STYIO_RELEASE_CHANNEL "dev"
#endif

#ifndef STYIO_EDITION_MAX
#define STYIO_EDITION_MAX "2026"
#endif

#ifndef STYIO_LLVM_DIR
#define STYIO_LLVM_DIR ""
#endif

extern "C" void
hello_world() {
  std::cout << "hello, world!" << std::endl;
}

struct tmp_code_wrap
{
  bool ok = false;
  std::string error_message;
  std::string code_text;
  std::vector<std::pair<size_t, size_t>> line_seps;
};

void
show_cwd() {
  std::filesystem::path cwd = std::filesystem::current_path();
  std::cout << cwd.string() << std::endl;
}

/*
  linenum_map:

  O(n)
  foreach {
    (0, a1): 1,
    (a1 + 1, a2): 2,
    (a2 + 1, a3): 3,
    ...
  }

  O(logN)
  vector<pair<size_t, size_t>>
  [0, a1, a2, ..., an]

  l: total length of the code
  n: last line number
  p: current position

  p < (l / 2)
  then [0 ~ l/2]
  else [l/2 ~ l]

  what if two consecutive "\n"?
  that is: "\n\n"
*/

tmp_code_wrap
read_styio_file(
  std::string file_path
) {
  tmp_code_wrap result;

  const char* fpath_cstr = file_path.c_str();
  if (std::filesystem::exists(fpath_cstr)) {
    std::ifstream file(fpath_cstr);
    if (!file.is_open()) {
      result.ok = false;
      result.error_message = std::string("cannot open file: ") + fpath_cstr;
      return result;
    }

    size_t p = 0;
    std::string line;
    while (std::getline(file, line)) {
      result.code_text += line;
      result.line_seps.push_back(std::make_pair(p, line.size()));
      p += line.size();

      result.code_text.push_back('\n');
      p += 1;
    }
    result.ok = true;
  }
  else {
    result.ok = false;
    result.error_message = std::string("file not found: ") + fpath_cstr;
  }

  return result;
}

void
show_code_with_linenum(tmp_code_wrap c) {
  auto& code = c.code_text;
  auto& lineseps = c.line_seps;

  std::cout << "\033[1;32mCode\033[0m\n";
  for (size_t i = 0; i < lineseps.size(); i++) {
    std::string line = code.substr(lineseps.at(i).first, lineseps.at(i).second);

    std::regex newline_regex("\n");
    std::string replaced_text = std::regex_replace(line, newline_regex, "[NEWLINE]");

    std::cout
      << "|" << i << "|-[" << lineseps.at(i).first << ":" << (lineseps.at(i).first + lineseps.at(i).second) << "] "
      << line << std::endl;
  }
};

void
show_tokens(std::vector<StyioToken*> tokens) {
  std::cout
    << "\n"
    << "\033[1;32mTokens\033[0m"
    << std::endl;
  std::string sep = " ║ "; // ┃ ║
  for (auto tok : tokens) {
    if (tok->type == StyioTokenType::TOK_LF) {
      std::cout << sep + "\\n" + "\n";
    }
    else if (tok->type == StyioTokenType::TOK_SPACE) {
      std::cout << sep + " ";
    }
    else if (tok->type == StyioTokenType::NAME) {
      std::cout << sep + tok->original;
    }
    else if (tok->type == StyioTokenType::STRING) {
      std::cout << sep + tok->original;
    }
    else if (tok->type == StyioTokenType::INTEGER
             || tok->type == StyioTokenType::DECIMAL) {
      std::cout << sep + tok->original + ": " + StyioToken::getTokName(tok->type);
    }
    else {
      std::cout << sep + StyioToken::getTokName(tok->type);
    }
  }
  std::cout << "\n"
            << std::endl;
}

enum class StyioErrorCategory
{
  CliError,
  LexError,
  ParseError,
  TypeError,
  RuntimeError,
};

enum class StyioExitCode : int
{
  Success = 0,
  LexError = 2,
  ParseError = 3,
  TypeError = 4,
  RuntimeError = 5,
  CliError = 6,
};

static const char*
styio_category_name(StyioErrorCategory c) {
  switch (c) {
    case StyioErrorCategory::CliError:
      return "CliError";
    case StyioErrorCategory::LexError:
      return "LexError";
    case StyioErrorCategory::ParseError:
      return "ParseError";
    case StyioErrorCategory::TypeError:
      return "TypeError";
    case StyioErrorCategory::RuntimeError:
      return "RuntimeError";
  }
  return "RuntimeError";
}

static const char*
styio_category_code(StyioErrorCategory c) {
  switch (c) {
    case StyioErrorCategory::CliError:
      return "STYIO_CLI";
    case StyioErrorCategory::LexError:
      return "STYIO_LEX";
    case StyioErrorCategory::ParseError:
      return "STYIO_PARSE";
    case StyioErrorCategory::TypeError:
      return "STYIO_TYPE";
    case StyioErrorCategory::RuntimeError:
      return "STYIO_RUNTIME";
  }
  return "STYIO_RUNTIME";
}

static int
styio_exit_code(StyioErrorCategory c) {
  switch (c) {
    case StyioErrorCategory::CliError:
      return static_cast<int>(StyioExitCode::CliError);
    case StyioErrorCategory::LexError:
      return static_cast<int>(StyioExitCode::LexError);
    case StyioErrorCategory::ParseError:
      return static_cast<int>(StyioExitCode::ParseError);
    case StyioErrorCategory::TypeError:
      return static_cast<int>(StyioExitCode::TypeError);
    case StyioErrorCategory::RuntimeError:
      return static_cast<int>(StyioExitCode::RuntimeError);
  }
  return static_cast<int>(StyioExitCode::RuntimeError);
}

static std::string
styio_json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 16);
  for (char ch : s) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '\"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(ch);
        break;
    }
  }
  return out;
}

static bool
styio_error_jsonl_enabled(const std::string& fmt) {
  return fmt == "jsonl";
}

static bool
styio_arg_matches_latest(const char* raw, const char* long_name, const char* short_name = nullptr) {
  if (raw == nullptr || long_name == nullptr) {
    return false;
  }
  const std::string arg(raw);
  const std::string long_opt(long_name);
  if (arg == long_opt) {
    return true;
  }
  if (arg.rfind(long_opt + "=", 0) == 0) {
    return true;
  }
  if (short_name != nullptr && short_name[0] != '\0' && arg == short_name) {
    return true;
  }
  return false;
}

static std::string
styio_nano_disabled_option_latest(int argc, char* argv[]) {
#if STYIO_NANO_BUILD
  for (int i = 1; i < argc; ++i) {
    if (styio_arg_matches_latest(argv[i], "--compile-plan")) {
      return "compile-plan consumer is only available in the full styio compiler";
    }
    if (styio_arg_matches_latest(argv[i], "--nano-create")
        || styio_arg_matches_latest(argv[i], "--nano-publish")
        || styio_arg_matches_latest(argv[i], "--nano-package-config")
        || styio_arg_matches_latest(argv[i], "--nano-publish-config")
        || styio_arg_matches_latest(argv[i], "--nano-mode")
        || styio_arg_matches_latest(argv[i], "--nano-output")
        || styio_arg_matches_latest(argv[i], "--nano-name")
        || styio_arg_matches_latest(argv[i], "--nano-profile")
        || styio_arg_matches_latest(argv[i], "--nano-binary")
        || styio_arg_matches_latest(argv[i], "--nano-source-root")
        || styio_arg_matches_latest(argv[i], "--nano-package-dir")
        || styio_arg_matches_latest(argv[i], "--nano-channel")
        || styio_arg_matches_latest(argv[i], "--nano-manifest")
        || styio_arg_matches_latest(argv[i], "--nano-registry")
        || styio_arg_matches_latest(argv[i], "--nano-package")
        || styio_arg_matches_latest(argv[i], "--nano-version")) {
      return "styio-nano packaging commands are only available in the full styio compiler";
    }
  }
#if !STYIO_NANO_ENABLE_MACHINE_INFO
  for (int i = 1; i < argc; ++i) {
    if (styio_arg_matches_latest(argv[i], "--machine-info")) {
      return "--machine-info is disabled in this styio-nano profile";
    }
  }
#endif
#if !STYIO_NANO_ENABLE_DEBUG_CLI
  for (int i = 1; i < argc; ++i) {
    if (styio_arg_matches_latest(argv[i], "--debug")) {
      return "--debug is disabled in this styio-nano profile";
    }
  }
#endif
#if !STYIO_NANO_ENABLE_AST_DUMP_CLI
  for (int i = 1; i < argc; ++i) {
    if (styio_arg_matches_latest(argv[i], "--styio-ast")) {
      return "--styio-ast is disabled in this styio-nano profile";
    }
  }
#endif
#if !STYIO_NANO_ENABLE_STYIO_IR_DUMP_CLI
  for (int i = 1; i < argc; ++i) {
    if (styio_arg_matches_latest(argv[i], "--styio-ir")) {
      return "--styio-ir is disabled in this styio-nano profile";
    }
  }
#endif
#if !STYIO_NANO_ENABLE_LLVM_IR_DUMP_CLI
  for (int i = 1; i < argc; ++i) {
    if (styio_arg_matches_latest(argv[i], "--llvm-ir")) {
      return "--llvm-ir is disabled in this styio-nano profile";
    }
  }
#endif
#if !STYIO_NANO_ENABLE_LEGACY_PARSER
  for (int i = 1; i < argc; ++i) {
    if (styio_arg_matches_latest(argv[i], "--parser-engine")) {
      return "--parser-engine is disabled in this styio-nano profile";
    }
  }
#endif
#if !STYIO_NANO_ENABLE_PARSER_SHADOW_COMPARE
  for (int i = 1; i < argc; ++i) {
    if (styio_arg_matches_latest(argv[i], "--parser-shadow-compare")
        || styio_arg_matches_latest(argv[i], "--parser-shadow-artifact-dir")) {
      return "parser shadow compare options are disabled in this styio-nano profile";
    }
  }
#endif
#endif
  return "";
}

struct StyioProjectConfigLatest
{
  bool has_dict_impl = false;
  std::string dict_impl_raw;
  std::string loaded_from;
};

struct StyioDictImplSelectionLatest
{
  std::string impl_name;
  std::string source = "default";
  std::string config_path;
};

struct StyioNanoPackageConfigLatest
{
  bool has_mode = false;
  std::string mode_raw;

  bool has_output_dir = false;
  std::string output_dir_raw;

  bool has_package_name = false;
  std::string package_name;

  bool has_profile = false;
  std::string profile_raw;

  bool has_binary = false;
  std::string binary_raw;

  bool has_source_root = false;
  std::string source_root_raw;

  bool has_manifest = false;
  std::string manifest_raw;

  bool has_registry = false;
  std::string registry_raw;

  bool has_registry_package = false;
  std::string registry_package_raw;

  bool has_registry_version = false;
  std::string registry_version_raw;

  std::string loaded_from;
};

struct StyioNanoCreateSelectionLatest
{
  std::string mode;
  std::string output_dir;
  std::string package_name;
  std::string profile_path;
  std::string binary_path;
  std::string source_root;
  std::string manifest_ref;
  std::string registry_root;
  std::string registry_package;
  std::string registry_version;
  std::string config_path;
};

struct StyioNanoPublishConfigLatest
{
  bool has_package_dir = false;
  std::string package_dir_raw;

  bool has_registry = false;
  std::string registry_raw;

  bool has_registry_package = false;
  std::string registry_package_raw;

  bool has_registry_version = false;
  std::string registry_version_raw;

  bool has_channel = false;
  std::string channel_raw;

  std::string loaded_from;
};

struct StyioNanoPublishSelectionLatest
{
  std::string package_dir;
  std::string registry_root;
  std::string registry_package;
  std::string registry_version;
  std::string channel = "nano";
  std::string config_path;
};

struct StyioNanoPackageManifestLatest
{
  std::string package_name;
  std::string version;
  std::string channel = "nano";
  std::string binary_ref;
  std::string profile_ref;
  std::string loaded_from;
};

struct StyioNanoRepositoryEntryLatest
{
  std::string package_name;
  std::string version;
  std::string channel = "nano";
  std::string sha256;
  std::string blob_path;
  uint64_t size_bytes = 0;
  std::string published_at;
};

static std::string
styio_trim_copy_latest(const std::string& text) {
  size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

static std::string
styio_strip_inline_comment_latest(const std::string& text) {
  bool in_single = false;
  bool in_double = false;
  bool escaped = false;
  for (size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (ch == '\\' && (in_single || in_double)) {
      escaped = true;
      continue;
    }
    if (ch == '"' && !in_single) {
      in_double = !in_double;
      continue;
    }
    if (ch == '\'' && !in_double) {
      in_single = !in_single;
      continue;
    }
    if (ch == '#' && !in_single && !in_double) {
      return text.substr(0, i);
    }
  }
  return text;
}

static bool
styio_parse_config_scalar_latest(
  const std::string& raw_value,
  std::string& out_value,
  std::string& error_message
) {
  const std::string trimmed = styio_trim_copy_latest(raw_value);
  if (trimmed.empty()) {
    error_message = "empty config value";
    return false;
  }
  if ((trimmed.front() == '"' && trimmed.back() == '"')
      || (trimmed.front() == '\'' && trimmed.back() == '\'')) {
    if (trimmed.size() < 2) {
      error_message = "malformed quoted config value";
      return false;
    }
    out_value = trimmed.substr(1, trimmed.size() - 2);
    return true;
  }
  for (char ch : trimmed) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      error_message = "bare config values cannot contain whitespace";
      return false;
    }
  }
  out_value = trimmed;
  return true;
}

static bool
styio_parse_project_config_latest(
  const std::filesystem::path& config_path,
  StyioProjectConfigLatest& out_config,
  std::string& error_message
) {
  std::ifstream in(config_path);
  if (!in.is_open()) {
    error_message = "cannot open config file: " + config_path.string();
    return false;
  }

  std::string line;
  std::string section;
  size_t line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    const std::string stripped = styio_trim_copy_latest(styio_strip_inline_comment_latest(line));
    if (stripped.empty()) {
      continue;
    }
    if (stripped.front() == '[') {
      if (stripped.back() != ']') {
        error_message = "malformed section header in config file: " + config_path.string()
                        + ":" + std::to_string(line_no);
        return false;
      }
      section = styio_trim_copy_latest(stripped.substr(1, stripped.size() - 2));
      continue;
    }

    const size_t eq = stripped.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    const std::string key = styio_trim_copy_latest(stripped.substr(0, eq));
    const std::string raw_value = stripped.substr(eq + 1);

    bool interested = false;
    if (section.empty() || section == "runtime") {
      interested = key == "dict_impl" || key == "dictionary_impl";
    }
    else if (section == "dict" || section == "dictionary") {
      interested = key == "impl";
    }
    if (!interested) {
      continue;
    }

    std::string parsed_value;
    if (!styio_parse_config_scalar_latest(raw_value, parsed_value, error_message)) {
      error_message =
        "invalid dict_impl value in config file: " + config_path.string() + ":"
        + std::to_string(line_no) + " (" + error_message + ")";
      return false;
    }
    out_config.has_dict_impl = true;
    out_config.dict_impl_raw = parsed_value;
    out_config.loaded_from = config_path.string();
  }
  return true;
}

static bool
styio_find_project_config_latest(
  const std::string& file_path,
  std::filesystem::path& out_config_path
) {
  if (file_path.empty()) {
    return false;
  }

  std::error_code ec;
  std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(file_path), ec);
  if (ec) {
    dir = std::filesystem::path(file_path);
  }
  dir = dir.parent_path();
  while (!dir.empty()) {
    const std::filesystem::path candidates[2] = {dir / "styio.toml", dir / ".styio.toml"};
    for (const auto& candidate : candidates) {
      if (std::filesystem::exists(candidate, ec) && !ec) {
        out_config_path = candidate;
        return true;
      }
    }
    const std::filesystem::path parent = dir.parent_path();
    if (parent == dir) {
      break;
    }
    dir = parent;
  }
  return false;
}

static bool
styio_ref_is_http_url_latest(const std::string& ref) {
  return ref.rfind("http://", 0) == 0 || ref.rfind("https://", 0) == 0;
}

static bool
styio_ref_is_file_uri_latest(const std::string& ref) {
  return ref.rfind("file://", 0) == 0;
}

static std::string
styio_file_uri_to_path_latest(const std::string& ref) {
  if (!styio_ref_is_file_uri_latest(ref)) {
    return ref;
  }
  std::string path_text = ref.substr(std::string("file://").size());
  if (!path_text.empty() && path_text[0] != '/') {
    path_text = "/" + path_text;
  }
  return path_text;
}

static std::string
styio_shell_quote_latest(const std::string& text) {
  std::string quoted = "'";
  for (char ch : text) {
    if (ch == '\'') {
      quoted += "'\\''";
    }
    else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

static std::filesystem::path
styio_absolute_path_latest(const std::filesystem::path& raw_path) {
  std::error_code ec;
  std::filesystem::path abs = std::filesystem::absolute(raw_path, ec);
  if (ec) {
    return raw_path.lexically_normal();
  }
  return abs.lexically_normal();
}

static std::string
styio_resolve_path_from_config_latest(
  const std::filesystem::path& config_path,
  const std::string& raw_value
) {
  if (raw_value.empty() || styio_ref_is_http_url_latest(raw_value) || styio_ref_is_file_uri_latest(raw_value)) {
    return raw_value;
  }
  std::filesystem::path resolved(raw_value);
  if (resolved.is_relative()) {
    resolved = config_path.parent_path() / resolved;
  }
  return styio_absolute_path_latest(resolved).string();
}

static std::string
styio_resolve_cli_path_latest(const std::string& raw_value) {
  if (raw_value.empty() || styio_ref_is_http_url_latest(raw_value) || styio_ref_is_file_uri_latest(raw_value)) {
    return raw_value;
  }
  return styio_absolute_path_latest(std::filesystem::path(raw_value)).string();
}

static std::string
styio_toml_escape_string_latest(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char ch : s) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '\"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(ch);
        break;
    }
  }
  return out;
}

static std::string
styio_now_token_latest() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

static std::string
styio_now_utc_iso8601_latest() {
  const std::time_t now = std::time(nullptr);
  std::tm tm_buf {};
#ifdef _WIN32
  gmtime_s(&tm_buf, &now);
#else
  gmtime_r(&now, &tm_buf);
#endif
  char buffer[32];
  if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_buf) == 0) {
    return "1970-01-01T00:00:00Z";
  }
  return std::string(buffer);
}

static bool
styio_write_text_file_latest(
  const std::filesystem::path& path,
  const std::string& text,
  std::string& error_message
) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    error_message = "cannot open file for writing: " + path.string();
    return false;
  }
  out << text;
  if (!out.good()) {
    error_message = "failed to write file: " + path.string();
    return false;
  }
  return true;
}

static void
styio_make_file_executable_latest(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::permissions(
    path,
    std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec
      | std::filesystem::perms::others_exec,
    std::filesystem::perm_options::add,
    ec);
}

static bool
styio_copy_file_with_exec_latest(
  const std::filesystem::path& source_path,
  const std::filesystem::path& dest_path,
  bool make_executable,
  std::string& error_message
) {
  std::error_code ec;
  std::filesystem::create_directories(dest_path.parent_path(), ec);
  ec.clear();
  std::filesystem::copy_file(
    source_path,
    dest_path,
    std::filesystem::copy_options::overwrite_existing,
    ec);
  if (ec) {
    error_message = "failed to copy " + source_path.string() + " -> " + dest_path.string()
                    + ": " + ec.message();
    return false;
  }
  if (make_executable) {
    styio_make_file_executable_latest(dest_path);
  }
  return true;
}

static bool
styio_fetch_ref_to_file_latest(
  const std::string& ref,
  const std::filesystem::path& base_dir,
  const std::filesystem::path& dest_path,
  bool make_executable,
  std::string& error_message
) {
  if (ref.empty()) {
    error_message = "empty artifact reference";
    return false;
  }

  if (styio_ref_is_http_url_latest(ref)) {
    std::error_code ec;
    std::filesystem::create_directories(dest_path.parent_path(), ec);
    const std::string cmd = "curl -fsSL " + styio_shell_quote_latest(ref)
                            + " -o " + styio_shell_quote_latest(dest_path.string());
    const int status = std::system(cmd.c_str());
    if (status != 0) {
      error_message = "failed to download artifact via curl: " + ref;
      return false;
    }
    if (make_executable) {
      styio_make_file_executable_latest(dest_path);
    }
    return true;
  }

  std::filesystem::path source_path;
  if (styio_ref_is_file_uri_latest(ref)) {
    source_path = std::filesystem::path(styio_file_uri_to_path_latest(ref));
  }
  else {
    source_path = std::filesystem::path(ref);
    if (source_path.is_relative()) {
      if (base_dir.empty()) {
        error_message = "relative artifact reference requires a local manifest or config base directory: " + ref;
        return false;
      }
      source_path = base_dir / source_path;
    }
  }

  source_path = styio_absolute_path_latest(source_path);
  if (!std::filesystem::exists(source_path)) {
    error_message = "artifact not found: " + source_path.string();
    return false;
  }
  return styio_copy_file_with_exec_latest(source_path, dest_path, make_executable, error_message);
}

static bool
styio_parse_nano_profile_name_latest(
  const std::filesystem::path& profile_path,
  std::string& out_profile_name,
  std::string& error_message
) {
  std::ifstream in(profile_path);
  if (!in.is_open()) {
    error_message = "cannot open styio-nano profile: " + profile_path.string();
    return false;
  }

  std::string line;
  std::string section;
  size_t line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    const std::string stripped = styio_trim_copy_latest(styio_strip_inline_comment_latest(line));
    if (stripped.empty()) {
      continue;
    }
    if (stripped.front() == '[') {
      if (stripped.back() != ']') {
        error_message = "malformed section header in styio-nano profile: " + profile_path.string()
                        + ":" + std::to_string(line_no);
        return false;
      }
      section = styio_trim_copy_latest(stripped.substr(1, stripped.size() - 2));
      continue;
    }

    if (section != "profile") {
      continue;
    }

    const size_t eq = stripped.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    const std::string key = styio_trim_copy_latest(stripped.substr(0, eq));
    if (key != "name") {
      continue;
    }

    std::string parsed_value;
    if (!styio_parse_config_scalar_latest(stripped.substr(eq + 1), parsed_value, error_message)) {
      error_message =
        "invalid profile.name in styio-nano profile: " + profile_path.string() + ":"
        + std::to_string(line_no) + " (" + error_message + ")";
      return false;
    }
    out_profile_name = parsed_value;
    return true;
  }

  out_profile_name = profile_path.stem().string();
  return true;
}

static bool
styio_parse_nano_package_config_latest(
  const std::filesystem::path& config_path,
  StyioNanoPackageConfigLatest& out_config,
  std::string& error_message
) {
  std::ifstream in(config_path);
  if (!in.is_open()) {
    error_message = "cannot open nano package config: " + config_path.string();
    return false;
  }

  std::string line;
  std::string section;
  size_t line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    const std::string stripped = styio_trim_copy_latest(styio_strip_inline_comment_latest(line));
    if (stripped.empty()) {
      continue;
    }
    if (stripped.front() == '[') {
      if (stripped.back() != ']') {
        error_message = "malformed section header in nano package config: " + config_path.string()
                        + ":" + std::to_string(line_no);
        return false;
      }
      section = styio_trim_copy_latest(stripped.substr(1, stripped.size() - 2));
      continue;
    }

    const size_t eq = stripped.find('=');
    if (eq == std::string::npos) {
      continue;
    }

    const std::string key = styio_trim_copy_latest(stripped.substr(0, eq));
    const std::string raw_value = stripped.substr(eq + 1);
    bool interested = false;

    enum class NanoConfigField
    {
      None,
      Mode,
      OutputDir,
      PackageName,
      Profile,
      Binary,
      SourceRoot,
      Manifest,
      Registry,
      RegistryPackage,
      RegistryVersion,
    };

    NanoConfigField field = NanoConfigField::None;
    if (section.empty() || section == "nano") {
      if (key == "mode") {
        interested = true;
        field = NanoConfigField::Mode;
      }
      else if (key == "output_dir" || key == "output") {
        interested = true;
        field = NanoConfigField::OutputDir;
      }
      else if (key == "name" || key == "package_name") {
        interested = true;
        field = NanoConfigField::PackageName;
      }
      else if (key == "profile") {
        interested = true;
        field = NanoConfigField::Profile;
      }
      else if (key == "binary") {
        interested = true;
        field = NanoConfigField::Binary;
      }
      else if (key == "source_root") {
        interested = true;
        field = NanoConfigField::SourceRoot;
      }
      else if (key == "manifest") {
        interested = true;
        field = NanoConfigField::Manifest;
      }
    }
    else if (section == "nano.local") {
      if (key == "profile") {
        interested = true;
        field = NanoConfigField::Profile;
      }
      else if (key == "binary") {
        interested = true;
        field = NanoConfigField::Binary;
      }
      else if (key == "source_root") {
        interested = true;
        field = NanoConfigField::SourceRoot;
      }
    }
    else if (section == "nano.cloud") {
      if (key == "manifest") {
        interested = true;
        field = NanoConfigField::Manifest;
      }
      else if (key == "registry") {
        interested = true;
        field = NanoConfigField::Registry;
      }
      else if (key == "package") {
        interested = true;
        field = NanoConfigField::RegistryPackage;
      }
      else if (key == "version") {
        interested = true;
        field = NanoConfigField::RegistryVersion;
      }
    }

    if (!interested) {
      continue;
    }

    std::string parsed_value;
    if (!styio_parse_config_scalar_latest(raw_value, parsed_value, error_message)) {
      error_message =
        "invalid nano package value in config file: " + config_path.string() + ":"
        + std::to_string(line_no) + " (" + error_message + ")";
      return false;
    }

    switch (field) {
      case NanoConfigField::Mode:
        out_config.has_mode = true;
        out_config.mode_raw = parsed_value;
        break;
      case NanoConfigField::OutputDir:
        out_config.has_output_dir = true;
        out_config.output_dir_raw = parsed_value;
        break;
      case NanoConfigField::PackageName:
        out_config.has_package_name = true;
        out_config.package_name = parsed_value;
        break;
      case NanoConfigField::Profile:
        out_config.has_profile = true;
        out_config.profile_raw = parsed_value;
        break;
      case NanoConfigField::Binary:
        out_config.has_binary = true;
        out_config.binary_raw = parsed_value;
        break;
      case NanoConfigField::SourceRoot:
        out_config.has_source_root = true;
        out_config.source_root_raw = parsed_value;
        break;
      case NanoConfigField::Manifest:
        out_config.has_manifest = true;
        out_config.manifest_raw = parsed_value;
        break;
      case NanoConfigField::Registry:
        out_config.has_registry = true;
        out_config.registry_raw = parsed_value;
        break;
      case NanoConfigField::RegistryPackage:
        out_config.has_registry_package = true;
        out_config.registry_package_raw = parsed_value;
        break;
      case NanoConfigField::RegistryVersion:
        out_config.has_registry_version = true;
        out_config.registry_version_raw = parsed_value;
        break;
      case NanoConfigField::None:
        break;
    }
  }

  out_config.loaded_from = config_path.string();
  return true;
}

static bool
styio_parse_nano_publish_config_latest(
  const std::filesystem::path& config_path,
  StyioNanoPublishConfigLatest& out_config,
  std::string& error_message
) {
  std::ifstream in(config_path);
  if (!in.is_open()) {
    error_message = "cannot open nano publish config: " + config_path.string();
    return false;
  }

  std::string line;
  std::string section;
  size_t line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    const std::string stripped = styio_trim_copy_latest(styio_strip_inline_comment_latest(line));
    if (stripped.empty()) {
      continue;
    }
    if (stripped.front() == '[') {
      if (stripped.back() != ']') {
        error_message = "malformed section header in nano publish config: " + config_path.string()
                        + ":" + std::to_string(line_no);
        return false;
      }
      section = styio_trim_copy_latest(stripped.substr(1, stripped.size() - 2));
      continue;
    }

    const size_t eq = stripped.find('=');
    if (eq == std::string::npos) {
      continue;
    }

    const std::string key = styio_trim_copy_latest(stripped.substr(0, eq));
    const std::string raw_value = stripped.substr(eq + 1);
    bool interested = false;

    enum class NanoPublishField
    {
      None,
      PackageDir,
      Registry,
      Package,
      Version,
      Channel,
    };

    NanoPublishField field = NanoPublishField::None;
    if (section == "nano.publish" || section == "publish") {
      if (key == "package_dir" || key == "package_root") {
        interested = true;
        field = NanoPublishField::PackageDir;
      }
      else if (key == "registry") {
        interested = true;
        field = NanoPublishField::Registry;
      }
      else if (key == "package") {
        interested = true;
        field = NanoPublishField::Package;
      }
      else if (key == "version") {
        interested = true;
        field = NanoPublishField::Version;
      }
      else if (key == "channel") {
        interested = true;
        field = NanoPublishField::Channel;
      }
    }

    if (!interested) {
      continue;
    }

    std::string parsed_value;
    if (!styio_parse_config_scalar_latest(raw_value, parsed_value, error_message)) {
      error_message =
        "invalid nano publish value in config file: " + config_path.string() + ":"
        + std::to_string(line_no) + " (" + error_message + ")";
      return false;
    }

    switch (field) {
      case NanoPublishField::PackageDir:
        out_config.has_package_dir = true;
        out_config.package_dir_raw = parsed_value;
        break;
      case NanoPublishField::Registry:
        out_config.has_registry = true;
        out_config.registry_raw = parsed_value;
        break;
      case NanoPublishField::Package:
        out_config.has_registry_package = true;
        out_config.registry_package_raw = parsed_value;
        break;
      case NanoPublishField::Version:
        out_config.has_registry_version = true;
        out_config.registry_version_raw = parsed_value;
        break;
      case NanoPublishField::Channel:
        out_config.has_channel = true;
        out_config.channel_raw = parsed_value;
        break;
      case NanoPublishField::None:
        break;
    }
  }

  out_config.loaded_from = config_path.string();
  return true;
}

static bool
styio_parse_nano_package_manifest_latest(
  const std::filesystem::path& manifest_path,
  StyioNanoPackageManifestLatest& out_manifest,
  std::string& error_message
) {
  std::ifstream in(manifest_path);
  if (!in.is_open()) {
    error_message = "cannot open nano package manifest: " + manifest_path.string();
    return false;
  }

  std::string line;
  std::string section;
  size_t line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    const std::string stripped = styio_trim_copy_latest(styio_strip_inline_comment_latest(line));
    if (stripped.empty()) {
      continue;
    }
    if (stripped.front() == '[') {
      if (stripped.back() != ']') {
        error_message = "malformed section header in nano package manifest: " + manifest_path.string()
                        + ":" + std::to_string(line_no);
        return false;
      }
      section = styio_trim_copy_latest(stripped.substr(1, stripped.size() - 2));
      continue;
    }

    const size_t eq = stripped.find('=');
    if (eq == std::string::npos) {
      continue;
    }

    const std::string key = styio_trim_copy_latest(stripped.substr(0, eq));
    const std::string raw_value = stripped.substr(eq + 1);
    bool interested = false;

    enum class NanoManifestField
    {
      None,
      PackageName,
      Version,
      Channel,
      Binary,
      Profile,
    };

    NanoManifestField field = NanoManifestField::None;
    if (section.empty() || section == "package") {
      if (key == "name") {
        interested = true;
        field = NanoManifestField::PackageName;
      }
      else if (key == "version") {
        interested = true;
        field = NanoManifestField::Version;
      }
      else if (key == "channel") {
        interested = true;
        field = NanoManifestField::Channel;
      }
      else if (key == "binary" || key == "binary_ref") {
        interested = true;
        field = NanoManifestField::Binary;
      }
      else if (key == "profile" || key == "profile_ref") {
        interested = true;
        field = NanoManifestField::Profile;
      }
    }
    else if (section == "artifact") {
      if (key == "binary" || key == "binary_url") {
        interested = true;
        field = NanoManifestField::Binary;
      }
      else if (key == "profile" || key == "profile_url") {
        interested = true;
        field = NanoManifestField::Profile;
      }
    }

    if (!interested) {
      continue;
    }

    std::string parsed_value;
    if (!styio_parse_config_scalar_latest(raw_value, parsed_value, error_message)) {
      error_message =
        "invalid nano package manifest value in file: " + manifest_path.string() + ":"
        + std::to_string(line_no) + " (" + error_message + ")";
      return false;
    }

    switch (field) {
      case NanoManifestField::PackageName:
        out_manifest.package_name = parsed_value;
        break;
      case NanoManifestField::Version:
        out_manifest.version = parsed_value;
        break;
      case NanoManifestField::Channel:
        out_manifest.channel = parsed_value;
        break;
      case NanoManifestField::Binary:
        out_manifest.binary_ref = parsed_value;
        break;
      case NanoManifestField::Profile:
        out_manifest.profile_ref = parsed_value;
        break;
      case NanoManifestField::None:
        break;
    }
  }

  out_manifest.loaded_from = manifest_path.string();
  if (out_manifest.package_name.empty()) {
    out_manifest.package_name = manifest_path.stem().string();
  }
  if (out_manifest.channel.empty()) {
    out_manifest.channel = "nano";
  }
  if (out_manifest.binary_ref.empty()) {
    error_message = "nano package manifest is missing artifact.binary";
    return false;
  }
  return true;
}

static std::string
styio_nano_binary_filename_latest() {
#ifdef _WIN32
  return "styio-nano.exe";
#else
  return "styio-nano";
#endif
}

static bool
styio_read_text_file_latest(
  const std::filesystem::path& path,
  std::string& out_text,
  std::string& error_message
) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    error_message = "cannot open file: " + path.string();
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  if (!in.good() && !in.eof()) {
    error_message = "failed to read file: " + path.string();
    return false;
  }
  out_text = buffer.str();
  return true;
}

static bool
styio_run_shell_command_latest(
  const std::string& command,
  const std::string& purpose,
  std::string& error_message
) {
  const int status = std::system(command.c_str());
  if (status != 0) {
    error_message = purpose + " failed";
    return false;
  }
  return true;
}

static bool
styio_read_first_command_token_latest(
  const std::string& command,
  std::string& out_token
) {
  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    return false;
  }

  std::string output;
  char buffer[4096];
  while (fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr) {
    output += buffer;
  }

  const int status = pclose(pipe);
  if (status != 0) {
    return false;
  }

  std::istringstream in(output);
  in >> out_token;
  return !out_token.empty();
}

static bool
styio_compute_file_sha256_latest(
  const std::filesystem::path& path,
  std::string& out_sha256,
  std::string& error_message
) {
  const std::string quoted_path = styio_shell_quote_latest(path.string());
  if (styio_read_first_command_token_latest("shasum -a 256 " + quoted_path + " 2>/dev/null", out_sha256)) {
    return true;
  }
  if (styio_read_first_command_token_latest("sha256sum " + quoted_path + " 2>/dev/null", out_sha256)) {
    return true;
  }
  error_message = "failed to compute sha256 for: " + path.string();
  return false;
}

static bool
styio_is_hex_digest_64_latest(const std::string& text) {
  if (text.size() != 64) {
    return false;
  }
  for (char ch : text) {
    if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) {
      return false;
    }
  }
  return true;
}

static std::string
styio_nano_repository_package_leaf_latest(const std::string& package_name) {
  const size_t slash = package_name.find_last_of('/');
  if (slash == std::string::npos || slash + 1 >= package_name.size()) {
    return package_name;
  }
  return package_name.substr(slash + 1);
}

static std::string
styio_normalize_nano_registry_root_latest(const std::string& raw_root) {
  std::string value = raw_root;
  while (!value.empty() && value.back() == '/') {
    value.pop_back();
  }
  if (value.empty()) {
    return value;
  }
  if (styio_ref_is_http_url_latest(value) || styio_ref_is_file_uri_latest(value)) {
    return value;
  }
  return styio_absolute_path_latest(std::filesystem::path(value)).string();
}

static std::string
styio_join_registry_ref_latest(
  const std::string& registry_root,
  const std::string& rel_path
) {
  if (styio_ref_is_http_url_latest(registry_root) || styio_ref_is_file_uri_latest(registry_root)) {
    return registry_root + "/" + rel_path;
  }
  return (std::filesystem::path(registry_root) / rel_path).string();
}

static std::string
styio_nano_repository_marker_relpath_latest() {
  return "styio-nano-repository.json";
}

static std::string
styio_nano_repository_entry_relpath_latest(
  const std::string& package_name,
  const std::string& version
) {
  return (std::filesystem::path("index") / std::filesystem::path(package_name) / (version + ".json"))
    .generic_string();
}

static std::string
styio_nano_repository_blob_relpath_latest(const std::string& sha256) {
  return (std::filesystem::path("blobs") / "sha256" / sha256.substr(0, 2) / sha256.substr(2, 2)
          / (sha256 + ".tar"))
    .generic_string();
}

static bool
styio_fetch_registry_text_latest(
  const std::string& registry_root,
  const std::string& rel_path,
  std::string& out_text,
  std::string& error_message
) {
  const std::string full_ref = styio_join_registry_ref_latest(registry_root, rel_path);
  if (styio_ref_is_http_url_latest(full_ref)) {
    const std::filesystem::path tmp_path =
      std::filesystem::temp_directory_path() / ("styio-nano-fetch-" + styio_now_token_latest() + ".tmp");
    if (!styio_fetch_ref_to_file_latest(full_ref, {}, tmp_path, false, error_message)) {
      return false;
    }
    const bool ok = styio_read_text_file_latest(tmp_path, out_text, error_message);
    std::error_code ec;
    std::filesystem::remove(tmp_path, ec);
    return ok;
  }

  std::filesystem::path local_path;
  if (styio_ref_is_file_uri_latest(full_ref)) {
    local_path = std::filesystem::path(styio_file_uri_to_path_latest(full_ref));
  }
  else {
    local_path = std::filesystem::path(full_ref);
  }
  return styio_read_text_file_latest(local_path, out_text, error_message);
}

static bool
styio_validate_nano_repository_marker_latest(
  const std::string& marker_text,
  std::string& error_message
) {
  llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(marker_text);
  if (!parsed) {
    error_message = "nano repository marker is not valid JSON: " + llvm::toString(parsed.takeError());
    return false;
  }
  const llvm::json::Object* obj = parsed->getAsObject();
  if (obj == nullptr) {
    error_message = "nano repository marker must be a JSON object";
    return false;
  }
  const auto kind = obj->getString("kind");
  const auto schema_version = obj->getInteger("schema_version");
  if (!kind.has_value() || *kind != "styio-nano-static" || !schema_version.has_value() || *schema_version != 1) {
    error_message = "nano repository marker does not match the supported styio-nano static repository contract";
    return false;
  }
  return true;
}

static bool
styio_parse_nano_repository_entry_latest(
  const std::string& entry_text,
  const std::string& expected_package,
  const std::string& expected_version,
  StyioNanoRepositoryEntryLatest& out_entry,
  std::string& error_message
) {
  llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(entry_text);
  if (!parsed) {
    error_message = "nano repository entry is not valid JSON: " + llvm::toString(parsed.takeError());
    return false;
  }
  const llvm::json::Object* obj = parsed->getAsObject();
  if (obj == nullptr) {
    error_message = "nano repository entry must be a JSON object";
    return false;
  }

  const auto schema_version = obj->getInteger("schema_version");
  const auto package_name = obj->getString("package");
  const auto version = obj->getString("version");
  const auto sha256 = obj->getString("sha256");
  const auto blob_path = obj->getString("blob_path");
  const auto size_bytes = obj->getInteger("size_bytes");
  const auto channel = obj->getString("channel");
  const auto published_at = obj->getString("published_at");

  if (!schema_version.has_value() || *schema_version != 1) {
    error_message = "nano repository entry has unsupported schema_version";
    return false;
  }
  if (!package_name.has_value() || std::string(*package_name) != expected_package) {
    error_message = "nano repository entry package mismatch";
    return false;
  }
  if (!version.has_value() || std::string(*version) != expected_version) {
    error_message = "nano repository entry version mismatch";
    return false;
  }
  if (!sha256.has_value() || !styio_is_hex_digest_64_latest(std::string(*sha256))) {
    error_message = "nano repository entry is missing a valid sha256 digest";
    return false;
  }
  if (!blob_path.has_value() || blob_path->empty()) {
    error_message = "nano repository entry is missing blob_path";
    return false;
  }
  if (!size_bytes.has_value() || *size_bytes < 0) {
    error_message = "nano repository entry is missing a valid size_bytes";
    return false;
  }

  out_entry.package_name = std::string(*package_name);
  out_entry.version = std::string(*version);
  out_entry.channel = channel.has_value() ? std::string(*channel) : "nano";
  out_entry.sha256 = std::string(*sha256);
  out_entry.blob_path = std::string(*blob_path);
  out_entry.size_bytes = static_cast<uint64_t>(*size_bytes);
  out_entry.published_at = published_at.has_value() ? std::string(*published_at) : "";
  return true;
}

static bool
styio_write_nano_repository_marker_latest(
  const std::filesystem::path& marker_path,
  std::string& error_message
) {
  const std::string marker_text =
    "{\n"
    "  \"kind\": \"styio-nano-static\",\n"
    "  \"schema_version\": 1\n"
    "}\n";
  return styio_write_text_file_latest(marker_path, marker_text, error_message);
}

static bool
styio_ensure_writable_nano_repository_latest(
  const std::filesystem::path& repo_root,
  std::string& error_message
) {
  std::error_code ec;
  std::filesystem::create_directories(repo_root, ec);
  if (ec) {
    error_message = "failed to create nano repository root: " + repo_root.string() + ": " + ec.message();
    return false;
  }

  const std::filesystem::path marker_path = repo_root / styio_nano_repository_marker_relpath_latest();
  if (!std::filesystem::exists(marker_path)) {
    return styio_write_nano_repository_marker_latest(marker_path, error_message);
  }

  std::string marker_text;
  if (!styio_read_text_file_latest(marker_path, marker_text, error_message)) {
    return false;
  }
  return styio_validate_nano_repository_marker_latest(marker_text, error_message);
}

static bool
styio_write_nano_repository_entry_latest(
  const std::filesystem::path& repo_root,
  const StyioNanoRepositoryEntryLatest& entry,
  std::string& error_message
) {
  const std::filesystem::path entry_path =
    repo_root / std::filesystem::path(styio_nano_repository_entry_relpath_latest(entry.package_name, entry.version));
  std::error_code ec;
  std::filesystem::create_directories(entry_path.parent_path(), ec);
  if (ec) {
    error_message = "failed to create nano repository index directory: " + entry_path.parent_path().string()
                    + ": " + ec.message();
    return false;
  }

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"package\": \"" << styio_json_escape(entry.package_name) << "\",\n";
  out << "  \"version\": \"" << styio_json_escape(entry.version) << "\",\n";
  out << "  \"channel\": \"" << styio_json_escape(entry.channel) << "\",\n";
  out << "  \"sha256\": \"" << styio_json_escape(entry.sha256) << "\",\n";
  out << "  \"size_bytes\": " << entry.size_bytes << ",\n";
  out << "  \"blob_path\": \"" << styio_json_escape(entry.blob_path) << "\",\n";
  out << "  \"published_at\": \"" << styio_json_escape(entry.published_at) << "\"\n";
  out << "}\n";
  return styio_write_text_file_latest(entry_path, out.str(), error_message);
}

static bool
styio_profile_cmake_includes_pipeline_check_latest(
  const std::filesystem::path& profile_cmake_path,
  bool& out_include_pipeline_check,
  std::string& error_message
) {
  std::string text;
  if (!styio_read_text_file_latest(profile_cmake_path, text, error_message)) {
    return false;
  }
  out_include_pipeline_check =
    text.find("set(STYIO_NANO_INCLUDE_PIPELINE_CHECK ON)") != std::string::npos;
  return true;
}

static std::vector<std::string>
styio_nano_source_roots_latest(bool include_pipeline_check) {
  std::vector<std::string> sources = {
    "src/main.cpp",
    "src/StyioToken/Token.cpp",
    "src/StyioUnicode/Unicode.cpp",
    "src/StyioParser/Parser.cpp",
    "src/StyioParser/ParserLookahead.cpp",
    "src/StyioParser/NewParserExpr.cpp",
    "src/StyioParser/Tokenizer.cpp",
    "src/StyioToString/ToString.cpp",
    "src/StyioAnalyzer/TypeInfer.cpp",
    "src/StyioAnalyzer/ToStyioIR.cpp",
    "src/StyioCodeGen/CodeGen.cpp",
    "src/StyioCodeGen/GetTypeG.cpp",
    "src/StyioCodeGen/CodeGenG.cpp",
    "src/StyioCodeGen/CodeGenPulse.cpp",
    "src/StyioCodeGen/GetTypeIO.cpp",
    "src/StyioCodeGen/CodeGenIO.cpp",
    "src/StyioExtern/ExternLib.cpp",
  };
  if (include_pipeline_check) {
    sources.push_back("src/StyioTesting/PipelineCheck.cpp");
  }
  return sources;
}

static bool
styio_resolve_local_include_relpath_latest(
  const std::filesystem::path& source_root,
  const std::string& current_relpath,
  const std::string& include_target,
  std::string& out_relpath
) {
  const std::filesystem::path current_path = source_root / current_relpath;
  const std::filesystem::path candidates[3] = {
    current_path.parent_path() / include_target,
    source_root / "src" / include_target,
    source_root / include_target,
  };

  for (const auto& candidate : candidates) {
    std::error_code ec;
    if (!std::filesystem::exists(candidate, ec) || ec) {
      continue;
    }
    std::filesystem::path normalized = candidate.lexically_normal();
    std::filesystem::path normalized_root = source_root.lexically_normal();
    const auto normalized_string = normalized.generic_string();
    const auto root_string = normalized_root.generic_string();
    if (normalized_string.rfind(root_string, 0) != 0) {
      continue;
    }
    std::filesystem::path rel = normalized.lexically_relative(source_root);
    if (!rel.empty()) {
      out_relpath = rel.generic_string();
      return true;
    }
  }
  return false;
}

static bool
styio_collect_nano_closure_files_latest(
  const std::filesystem::path& source_root,
  bool include_pipeline_check,
  std::set<std::string>& out_files,
  std::string& error_message
) {
  const std::regex include_regex(R"styio(^\s*#\s*include\s*"([^"]+)")styio");
  std::vector<std::string> worklist = styio_nano_source_roots_latest(include_pipeline_check);

  for (size_t idx = 0; idx < worklist.size(); ++idx) {
    const std::string relpath = worklist[idx];
    if (!out_files.insert(relpath).second) {
      continue;
    }

    const std::filesystem::path full_path = source_root / relpath;
    if (!std::filesystem::exists(full_path)) {
      error_message = "nano source closure is missing required file: " + full_path.string();
      return false;
    }

    std::ifstream in(full_path);
    if (!in.is_open()) {
      error_message = "cannot read nano closure source: " + full_path.string();
      return false;
    }

    std::string line;
    while (std::getline(in, line)) {
      std::smatch match;
      if (!std::regex_search(line, match, include_regex)) {
        continue;
      }
      const std::string include_target = match[1].str();
      std::string include_relpath;
      if (!styio_resolve_local_include_relpath_latest(source_root, relpath, include_target, include_relpath)) {
        if (include_target.rfind("llvm/", 0) == 0) {
          continue;
        }
        error_message = "failed to resolve local include '" + include_target + "' from " + relpath;
        return false;
      }
      if (out_files.find(include_relpath) == out_files.end()) {
        worklist.push_back(include_relpath);
      }
    }
  }

  return true;
}

static bool
styio_copy_closure_files_latest(
  const std::filesystem::path& source_root,
  const std::filesystem::path& output_dir,
  const std::set<std::string>& closure_files,
  std::string& error_message
) {
  for (const std::string& relpath : closure_files) {
    const std::filesystem::path source_path = source_root / relpath;
    const std::filesystem::path dest_path = output_dir / relpath;
    if (!styio_copy_file_with_exec_latest(source_path, dest_path, false, error_message)) {
      return false;
    }
  }
  return true;
}

static bool
styio_write_nano_closure_manifest_latest(
  const std::filesystem::path& output_dir,
  const std::set<std::string>& closure_files,
  std::string& error_message
) {
  std::ostringstream out;
  for (const std::string& relpath : closure_files) {
    out << relpath << '\n';
  }
  return styio_write_text_file_latest(output_dir / "source-closure-manifest.txt", out.str(), error_message);
}

static std::vector<std::string>
styio_nano_cpp_sources_from_closure_latest(const std::set<std::string>& closure_files) {
  std::vector<std::string> cpp_sources;
  cpp_sources.reserve(closure_files.size());
  for (const std::string& relpath : closure_files) {
    if (std::filesystem::path(relpath).extension() == ".cpp") {
      cpp_sources.push_back(relpath);
    }
  }

  std::sort(cpp_sources.begin(), cpp_sources.end());
  auto main_it = std::find(cpp_sources.begin(), cpp_sources.end(), std::string("src/main.cpp"));
  if (main_it != cpp_sources.end() && main_it != cpp_sources.begin()) {
    std::rotate(cpp_sources.begin(), main_it, main_it + 1);
  }
  return cpp_sources;
}

static bool
styio_generate_profile_cmake_latest(
  const std::filesystem::path& source_root,
  const std::filesystem::path& input_profile,
  const std::filesystem::path& output_profile_cmake,
  std::string& error_message
) {
  const std::filesystem::path script_path = source_root / "scripts" / "gen-styio-nano-profile.py";
  if (!std::filesystem::exists(script_path)) {
    error_message = "styio-nano profile generator not found: " + script_path.string();
    return false;
  }
  const std::string command =
    "python3 "
    + styio_shell_quote_latest(script_path.string())
    + " --input " + styio_shell_quote_latest(input_profile.string())
    + " --cmake-out " + styio_shell_quote_latest(output_profile_cmake.string());
  return styio_run_shell_command_latest(command, "styio-nano profile generation", error_message);
}

static bool
styio_write_nano_package_cmakelists_latest(
  const std::filesystem::path& output_dir,
  const std::set<std::string>& closure_files,
  std::string& error_message
) {
  const std::vector<std::string> cpp_sources = styio_nano_cpp_sources_from_closure_latest(closure_files);
  if (cpp_sources.empty()) {
    error_message = "nano source closure does not contain any C++ sources";
    return false;
  }

  std::ostringstream cmake;
  cmake << "cmake_minimum_required(VERSION 3.14)\n";
  cmake << "project(styio_nano_subset VERSION " << STYIO_PROJECT_VERSION << ")\n\n";
  cmake << "set(CMAKE_CXX_STANDARD 20)\n";
  cmake << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n";
  cmake << "set(CMAKE_CXX_EXTENSIONS OFF)\n";
  cmake << "option(STYIO_USE_ICU \"Enable ICU-backed Unicode handling for StyioUnicode and CLI option text\" OFF)\n";
  cmake << "set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY \"${CMAKE_BINARY_DIR}/lib\")\n";
  cmake << "set(CMAKE_LIBRARY_OUTPUT_DIRECTORY \"${CMAKE_BINARY_DIR}/lib\")\n";
  cmake << "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY \"${CMAKE_BINARY_DIR}/bin\")\n\n";
  cmake << "if(NOT DEFINED LLVM_DIR AND NOT \"$ENV{LLVM_DIR}\" STREQUAL \"\")\n";
  cmake << "  set(LLVM_DIR \"$ENV{LLVM_DIR}\")\n";
  cmake << "endif()\n";
  cmake << "if(NOT DEFINED LLVM_DIR OR LLVM_DIR STREQUAL \"\")\n";
  cmake << "  set(LLVM_DIR \"" << STYIO_LLVM_DIR << "\")\n";
  cmake << "endif()\n";
  cmake << "find_package(LLVM 18.1.0 REQUIRED CONFIG)\n";
  cmake << "separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})\n";
  cmake << "find_library(STYIO_NANO_LLVM_MONOLITHIC NAMES LLVM libLLVM HINTS ${LLVM_LIBRARY_DIRS})\n";
  cmake << "if(STYIO_NANO_LLVM_MONOLITHIC)\n";
  cmake << "  set(STYIO_NANO_LLVM_LINK_LIBS ${STYIO_NANO_LLVM_MONOLITHIC})\n";
  cmake << "else()\n";
  cmake << "  llvm_map_components_to_libnames(STYIO_NANO_LLVM_LINK_LIBS support core irreader orcjit native)\n";
  cmake << "  list(REMOVE_DUPLICATES STYIO_NANO_LLVM_LINK_LIBS)\n";
  cmake << "endif()\n";
  cmake << "include(\"${CMAKE_SOURCE_DIR}/styio_nano_profile.cmake\")\n\n";
  cmake << "set(STYIO_NANO_CPP_SOURCES\n";
  for (const std::string& source : cpp_sources) {
    cmake << "  " << source << '\n';
  }
  cmake << ")\n\n";
  cmake << "add_executable(styio_nano ${STYIO_NANO_CPP_SOURCES})\n";
  cmake << "set_target_properties(styio_nano PROPERTIES OUTPUT_NAME \"" << styio_nano_binary_filename_latest() << "\")\n";
  cmake << "target_include_directories(styio_nano PUBLIC \"${CMAKE_SOURCE_DIR}/src\")\n";
  cmake << "target_include_directories(styio_nano SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})\n";
  cmake << "target_compile_definitions(styio_nano PRIVATE ${LLVM_DEFINITIONS_LIST} ${STYIO_NANO_COMPILE_DEFINITIONS} ";
  cmake << "\"STYIO_PROJECT_VERSION=\\\"" << STYIO_PROJECT_VERSION << "\\\"\" ";
  cmake << "\"STYIO_RELEASE_CHANNEL=\\\"nano\\\"\" ";
  cmake << "\"STYIO_EDITION_MAX=\\\"" << STYIO_EDITION_MAX << "\\\"\")\n";
  cmake << "target_link_libraries(styio_nano PRIVATE ${STYIO_NANO_LLVM_LINK_LIBS})\n\n";
  cmake << "if(CMAKE_CXX_COMPILER_ID MATCHES \"Clang|GNU|AppleClang\")\n";
  cmake << "  target_compile_options(styio_nano PRIVATE -Os)\n";
  cmake << "elseif(MSVC)\n";
  cmake << "  target_compile_options(styio_nano PRIVATE /O1)\n";
  cmake << "endif()\n\n";
  cmake << "if(STYIO_USE_ICU)\n";
  cmake << "  find_package(ICU COMPONENTS uc i18n REQUIRED)\n";
  cmake << "  target_link_libraries(styio_nano PRIVATE ICU::uc ICU::i18n)\n";
  cmake << "  target_compile_definitions(styio_nano PRIVATE STYIO_USE_ICU)\n";
  cmake << "  target_compile_definitions(styio_nano PRIVATE CXXOPTS_USE_UNICODE)\n";
  cmake << "endif()\n";

  return styio_write_text_file_latest(output_dir / "CMakeLists.txt", cmake.str(), error_message);
}

static bool
styio_build_nano_package_latest(
  const std::filesystem::path& output_dir,
  std::string& error_message
) {
  const std::filesystem::path build_dir = output_dir / ".nano-build";
  const std::string configure_cmd =
    "cmake -S " + styio_shell_quote_latest(output_dir.string())
    + " -B " + styio_shell_quote_latest(build_dir.string());
  if (!styio_run_shell_command_latest(configure_cmd, "styio-nano package configure", error_message)) {
    return false;
  }

  const std::string build_cmd =
    "cmake --build " + styio_shell_quote_latest(build_dir.string()) + " --parallel --target styio_nano";
  if (!styio_run_shell_command_latest(build_cmd, "styio-nano package build", error_message)) {
    return false;
  }

  const std::filesystem::path built_binary = build_dir / "bin" / styio_nano_binary_filename_latest();
  if (!std::filesystem::exists(built_binary)) {
    error_message = "styio-nano package build did not produce " + built_binary.string();
    return false;
  }
  return styio_copy_file_with_exec_latest(
    built_binary,
    output_dir / "bin" / styio_nano_binary_filename_latest(),
    true,
    error_message);
}

static bool
styio_copy_directory_contents_latest(
  const std::filesystem::path& source_dir,
  const std::filesystem::path& dest_dir,
  std::string& error_message
) {
  std::error_code ec;
  std::filesystem::create_directories(dest_dir, ec);
  if (ec) {
    error_message = "failed to create directory: " + dest_dir.string() + ": " + ec.message();
    return false;
  }

  for (const auto& entry : std::filesystem::directory_iterator(source_dir)) {
    const std::filesystem::path dest_path = dest_dir / entry.path().filename();
    std::error_code copy_ec;
    std::filesystem::copy(
      entry.path(),
      dest_path,
      std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
      copy_ec);
    if (copy_ec) {
      error_message = "failed to copy package content into " + dest_dir.string() + ": " + copy_ec.message();
      return false;
    }
  }
  return true;
}

static bool
styio_resolve_extracted_nano_package_root_latest(
  const std::filesystem::path& extract_root,
  std::filesystem::path& out_package_root,
  std::string& error_message
) {
  const std::filesystem::path direct_binary = extract_root / "bin" / styio_nano_binary_filename_latest();
  if (std::filesystem::exists(direct_binary)) {
    out_package_root = extract_root;
    return true;
  }

  std::vector<std::filesystem::path> child_dirs;
  for (const auto& entry : std::filesystem::directory_iterator(extract_root)) {
    if (entry.is_directory()) {
      child_dirs.push_back(entry.path());
    }
  }
  if (child_dirs.size() == 1U
      && std::filesystem::exists(child_dirs.front() / "bin" / styio_nano_binary_filename_latest())) {
    out_package_root = child_dirs.front();
    return true;
  }

  error_message = "extracted nano package does not contain bin/" + styio_nano_binary_filename_latest();
  return false;
}

static std::filesystem::path
styio_guess_sibling_nano_binary_latest(const char* argv0) {
  if (argv0 == nullptr || argv0[0] == '\0') {
    return {};
  }
  std::filesystem::path self = styio_absolute_path_latest(std::filesystem::path(argv0));
  std::filesystem::path sibling = self.parent_path() / "styio-nano";
  if (!self.extension().empty()) {
    sibling = self.parent_path() / ("styio-nano" + self.extension().string());
  }
  return sibling;
}

static bool
styio_cli_mentions_any_nano_packaging_args_latest(const cxxopts::ParseResult& cmlopts) {
  return cmlopts["nano-create"].as<bool>()
         || cmlopts["nano-publish"].as<bool>()
         || cmlopts.count("nano-package-config")
         || cmlopts.count("nano-publish-config")
         || cmlopts.count("nano-mode")
         || cmlopts.count("nano-output")
         || cmlopts.count("nano-profile")
         || cmlopts.count("nano-binary")
         || cmlopts.count("nano-source-root")
         || cmlopts.count("nano-package-dir")
         || cmlopts.count("nano-channel")
         || cmlopts.count("nano-manifest")
         || cmlopts.count("nano-registry")
         || cmlopts.count("nano-package")
         || cmlopts.count("nano-version")
         || cmlopts.count("nano-name");
}

static bool
styio_resolve_nano_create_selection_latest(
  const cxxopts::ParseResult& cmlopts,
  const char* argv0,
  StyioNanoCreateSelectionLatest& out_selection,
  std::string& error_message
) {
  StyioNanoPackageConfigLatest config_values;
  std::filesystem::path config_path;

  if (cmlopts.count("nano-package-config")) {
    config_path = cmlopts["nano-package-config"].as<std::string>();
    if (config_path.empty()) {
      error_message = "--nano-package-config requires a non-empty path";
      return false;
    }
    config_path = styio_absolute_path_latest(config_path);
    if (!styio_parse_nano_package_config_latest(config_path, config_values, error_message)) {
      return false;
    }
    out_selection.config_path = config_path.string();
  }

  if (config_values.has_mode) {
    out_selection.mode = styio_trim_copy_latest(config_values.mode_raw);
  }
  if (config_values.has_output_dir) {
    out_selection.output_dir =
      styio_resolve_path_from_config_latest(config_path, config_values.output_dir_raw);
  }
  if (config_values.has_package_name) {
    out_selection.package_name = config_values.package_name;
  }
  if (config_values.has_profile) {
    out_selection.profile_path =
      styio_resolve_path_from_config_latest(config_path, config_values.profile_raw);
  }
  if (config_values.has_binary) {
    out_selection.binary_path =
      styio_resolve_path_from_config_latest(config_path, config_values.binary_raw);
  }
  if (config_values.has_source_root) {
    out_selection.source_root =
      styio_resolve_path_from_config_latest(config_path, config_values.source_root_raw);
  }
  if (config_values.has_manifest) {
    out_selection.manifest_ref =
      styio_resolve_path_from_config_latest(config_path, config_values.manifest_raw);
  }
  if (config_values.has_registry) {
    out_selection.registry_root =
      styio_resolve_path_from_config_latest(config_path, config_values.registry_raw);
  }
  if (config_values.has_registry_package) {
    out_selection.registry_package = styio_trim_copy_latest(config_values.registry_package_raw);
  }
  if (config_values.has_registry_version) {
    out_selection.registry_version = styio_trim_copy_latest(config_values.registry_version_raw);
  }

  if (cmlopts.count("nano-mode")) {
    out_selection.mode = styio_trim_copy_latest(cmlopts["nano-mode"].as<std::string>());
  }
  if (cmlopts.count("nano-output")) {
    out_selection.output_dir = styio_resolve_cli_path_latest(cmlopts["nano-output"].as<std::string>());
  }
  if (cmlopts.count("nano-name")) {
    out_selection.package_name = styio_trim_copy_latest(cmlopts["nano-name"].as<std::string>());
  }
  if (cmlopts.count("nano-profile")) {
    out_selection.profile_path = styio_resolve_cli_path_latest(cmlopts["nano-profile"].as<std::string>());
  }
  if (cmlopts.count("nano-binary")) {
    out_selection.binary_path = styio_resolve_cli_path_latest(cmlopts["nano-binary"].as<std::string>());
  }
  if (cmlopts.count("nano-source-root")) {
    out_selection.source_root =
      styio_resolve_cli_path_latest(cmlopts["nano-source-root"].as<std::string>());
  }
  if (cmlopts.count("nano-manifest")) {
    out_selection.manifest_ref = styio_resolve_cli_path_latest(cmlopts["nano-manifest"].as<std::string>());
  }
  if (cmlopts.count("nano-registry")) {
    out_selection.registry_root = styio_resolve_cli_path_latest(cmlopts["nano-registry"].as<std::string>());
  }
  if (cmlopts.count("nano-package")) {
    out_selection.registry_package = styio_trim_copy_latest(cmlopts["nano-package"].as<std::string>());
  }
  if (cmlopts.count("nano-version")) {
    out_selection.registry_version = styio_trim_copy_latest(cmlopts["nano-version"].as<std::string>());
  }

  out_selection.mode = styio_trim_copy_latest(out_selection.mode);
  if (out_selection.mode.empty()) {
    error_message = "styio-nano creation requires --nano-mode or nano.mode in the package config";
    return false;
  }
  if (!(out_selection.mode == "local-subset" || out_selection.mode == "cloud")) {
    error_message = "unsupported --nano-mode: " + out_selection.mode;
    return false;
  }

  if (out_selection.output_dir.empty()) {
    error_message = "styio-nano creation requires --nano-output or nano.output_dir";
    return false;
  }
  out_selection.output_dir = styio_absolute_path_latest(out_selection.output_dir).string();

  if (out_selection.mode == "local-subset") {
    if (out_selection.profile_path.empty()) {
      error_message = "local-subset styio-nano creation requires --nano-profile or nano.local.profile";
      return false;
    }
    if (out_selection.source_root.empty()) {
      out_selection.source_root = styio_absolute_path_latest(std::filesystem::current_path()).string();
    }
  }
  else {
    const bool has_manifest = !out_selection.manifest_ref.empty();
    const bool has_registry =
      !out_selection.registry_root.empty()
      || !out_selection.registry_package.empty()
      || !out_selection.registry_version.empty();
    if (has_manifest && has_registry) {
      error_message = "cloud styio-nano creation accepts either nano.cloud.manifest or nano.cloud.registry/package/version, not both";
      return false;
    }
    if (!has_manifest) {
      if (out_selection.registry_root.empty()
          || out_selection.registry_package.empty()
          || out_selection.registry_version.empty()) {
        error_message =
          "cloud styio-nano creation requires either --nano-manifest or the full --nano-registry/--nano-package/--nano-version triple";
        return false;
      }
      out_selection.registry_root = styio_normalize_nano_registry_root_latest(out_selection.registry_root);
    }
  }

  if (out_selection.package_name.empty() && !out_selection.profile_path.empty()) {
    std::string profile_name;
    std::string profile_error;
    if (styio_parse_nano_profile_name_latest(out_selection.profile_path, profile_name, profile_error)) {
      out_selection.package_name = profile_name;
    }
  }
  if (out_selection.package_name.empty() && !out_selection.registry_package.empty()) {
    out_selection.package_name = styio_nano_repository_package_leaf_latest(out_selection.registry_package);
  }
  if (out_selection.package_name.empty()) {
    out_selection.package_name = "styio-nano";
  }
  return true;
}

static bool
styio_resolve_nano_publish_selection_latest(
  const cxxopts::ParseResult& cmlopts,
  StyioNanoPublishSelectionLatest& out_selection,
  std::string& error_message
) {
  StyioNanoPublishConfigLatest config_values;
  std::filesystem::path config_path;

  if (cmlopts.count("nano-publish-config")) {
    config_path = cmlopts["nano-publish-config"].as<std::string>();
    if (config_path.empty()) {
      error_message = "--nano-publish-config requires a non-empty path";
      return false;
    }
    config_path = styio_absolute_path_latest(config_path);
    if (!styio_parse_nano_publish_config_latest(config_path, config_values, error_message)) {
      return false;
    }
    out_selection.config_path = config_path.string();
  }

  if (config_values.has_package_dir) {
    out_selection.package_dir =
      styio_resolve_path_from_config_latest(config_path, config_values.package_dir_raw);
  }
  if (config_values.has_registry) {
    out_selection.registry_root =
      styio_resolve_path_from_config_latest(config_path, config_values.registry_raw);
  }
  if (config_values.has_registry_package) {
    out_selection.registry_package = styio_trim_copy_latest(config_values.registry_package_raw);
  }
  if (config_values.has_registry_version) {
    out_selection.registry_version = styio_trim_copy_latest(config_values.registry_version_raw);
  }
  if (config_values.has_channel) {
    out_selection.channel = styio_trim_copy_latest(config_values.channel_raw);
  }

  if (cmlopts.count("nano-package-dir")) {
    out_selection.package_dir = styio_resolve_cli_path_latest(cmlopts["nano-package-dir"].as<std::string>());
  }
  if (cmlopts.count("nano-registry")) {
    out_selection.registry_root = styio_resolve_cli_path_latest(cmlopts["nano-registry"].as<std::string>());
  }
  if (cmlopts.count("nano-package")) {
    out_selection.registry_package = styio_trim_copy_latest(cmlopts["nano-package"].as<std::string>());
  }
  if (cmlopts.count("nano-version")) {
    out_selection.registry_version = styio_trim_copy_latest(cmlopts["nano-version"].as<std::string>());
  }
  if (cmlopts.count("nano-channel")) {
    out_selection.channel = styio_trim_copy_latest(cmlopts["nano-channel"].as<std::string>());
  }

  if (out_selection.package_dir.empty()) {
    error_message = "styio-nano publish requires --nano-package-dir or nano.publish.package_dir";
    return false;
  }
  out_selection.package_dir = styio_absolute_path_latest(out_selection.package_dir).string();
  if (!std::filesystem::exists(out_selection.package_dir)) {
    error_message = "styio-nano package directory not found: " + out_selection.package_dir;
    return false;
  }
  if (!std::filesystem::exists(std::filesystem::path(out_selection.package_dir) / "bin"
                               / styio_nano_binary_filename_latest())) {
    error_message = "styio-nano package directory is missing bin/" + styio_nano_binary_filename_latest();
    return false;
  }

  if (out_selection.registry_root.empty()) {
    error_message = "styio-nano publish requires --nano-registry or nano.publish.registry";
    return false;
  }
  out_selection.registry_root = styio_normalize_nano_registry_root_latest(out_selection.registry_root);
  if (styio_ref_is_http_url_latest(out_selection.registry_root)) {
    error_message = "styio-nano publish only supports local repository roots or file:// URIs";
    return false;
  }

  const std::filesystem::path receipt_path =
    std::filesystem::path(out_selection.package_dir) / "styio-nano-package.toml";
  StyioNanoPackageManifestLatest receipt;
  if (std::filesystem::exists(receipt_path)
      && !styio_parse_nano_package_manifest_latest(receipt_path, receipt, error_message)) {
    return false;
  }

  if (out_selection.registry_package.empty()) {
    out_selection.registry_package =
      receipt.package_name.empty()
        ? std::filesystem::path(out_selection.package_dir).filename().string()
        : receipt.package_name;
  }
  if (out_selection.registry_version.empty()) {
    out_selection.registry_version = receipt.version;
  }
  if (out_selection.channel.empty()) {
    out_selection.channel = receipt.channel.empty() ? "nano" : receipt.channel;
  }

  if (out_selection.registry_package.empty()) {
    error_message = "styio-nano publish requires --nano-package or nano.publish.package";
    return false;
  }
  if (out_selection.registry_version.empty()) {
    error_message =
      "styio-nano publish requires --nano-version or nano.publish.version when the package receipt has no version";
    return false;
  }
  if (out_selection.channel.empty()) {
    out_selection.channel = "nano";
  }
  return true;
}

static bool
styio_materialize_local_nano_package_latest(
  const StyioNanoCreateSelectionLatest& selection,
  const char* argv0,
  std::string& error_message
) {
  const std::filesystem::path output_dir(selection.output_dir);
  const std::filesystem::path bin_dir = output_dir / "bin";
  const std::filesystem::path profile_dest = output_dir / "styio-nano.profile.toml";
  const std::filesystem::path profile_cmake_dest = output_dir / "styio_nano_profile.cmake";
  const std::filesystem::path receipt_dest = output_dir / "styio-nano-package.toml";
  const std::filesystem::path helper_dest = output_dir / "build-styio-nano.sh";

  std::error_code ec;
  std::filesystem::create_directories(bin_dir, ec);
  if (ec) {
    error_message = "failed to create output directory: " + output_dir.string() + ": " + ec.message();
    return false;
  }

  const std::filesystem::path source_profile = styio_absolute_path_latest(selection.profile_path);
  const std::filesystem::path source_root = styio_absolute_path_latest(selection.source_root);

  if (!std::filesystem::exists(source_profile)) {
    error_message = "styio-nano profile not found: " + source_profile.string();
    return false;
  }
  if (!std::filesystem::exists(source_root / "CMakeLists.txt")) {
    error_message = "styio source root does not look like a styio repository: " + source_root.string();
    return false;
  }

  if (!styio_copy_file_with_exec_latest(source_profile, profile_dest, false, error_message)) {
    return false;
  }
  if (!styio_generate_profile_cmake_latest(source_root, source_profile, profile_cmake_dest, error_message)) {
    return false;
  }

  bool include_pipeline_check = false;
  if (!styio_profile_cmake_includes_pipeline_check_latest(
        profile_cmake_dest,
        include_pipeline_check,
        error_message)) {
    return false;
  }

  std::filesystem::remove_all(output_dir / "src", ec);
  std::filesystem::remove_all(output_dir / ".nano-build", ec);

  std::set<std::string> closure_files;
  if (!styio_collect_nano_closure_files_latest(
        source_root,
        include_pipeline_check,
        closure_files,
        error_message)) {
    return false;
  }
  if (!styio_copy_closure_files_latest(source_root, output_dir, closure_files, error_message)) {
    return false;
  }
  if (!styio_write_nano_closure_manifest_latest(output_dir, closure_files, error_message)) {
    return false;
  }
  if (!styio_write_nano_package_cmakelists_latest(output_dir, closure_files, error_message)) {
    return false;
  }

  const std::string helper_script =
    "#!/usr/bin/env bash\n"
    "set -euo pipefail\n"
    "script_dir=\"$(cd \"$(dirname \"$0\")\" && pwd)\"\n"
    "build_dir=\"${1:-$script_dir/build}\"\n"
    "cmake -S \"$script_dir\" -B \"$build_dir\"\n"
    "cmake --build \"$build_dir\" --parallel --target styio_nano\n"
    "cp \"$build_dir/bin/" + styio_nano_binary_filename_latest() + "\" \"$script_dir/bin/" + styio_nano_binary_filename_latest() + "\"\n";
  if (!styio_write_text_file_latest(helper_dest, helper_script, error_message)) {
    return false;
  }
  styio_make_file_executable_latest(helper_dest);
  if (!styio_build_nano_package_latest(output_dir, error_message)) {
    return false;
  }

  const std::filesystem::path source_compiler =
    argv0 == nullptr || argv0[0] == '\0' ? std::filesystem::path() : styio_absolute_path_latest(argv0);
  const std::string receipt =
    "[package]\n"
    "name = \"" + styio_toml_escape_string_latest(selection.package_name) + "\"\n"
    "channel = \"nano\"\n"
    "mode = \"local-subset\"\n"
    "binary = \"bin/" + styio_toml_escape_string_latest(styio_nano_binary_filename_latest()) + "\"\n"
    "profile = \"styio-nano.profile.toml\"\n"
    "profile_cmake = \"styio_nano_profile.cmake\"\n"
    "closure_manifest = \"source-closure-manifest.txt\"\n"
    "version = \"" + styio_toml_escape_string_latest(STYIO_PROJECT_VERSION) + "\"\n"
    "\n"
    "[source]\n"
    "compiler = \"" + styio_toml_escape_string_latest(source_compiler.string()) + "\"\n"
    "source_root = \"" + styio_toml_escape_string_latest(source_root.string()) + "\"\n"
    "profile = \"" + styio_toml_escape_string_latest(source_profile.string()) + "\"\n";
  if (!styio_write_text_file_latest(receipt_dest, receipt, error_message)) {
    return false;
  }
  return true;
}

static bool
styio_materialize_cloud_nano_package_latest(
  const StyioNanoCreateSelectionLatest& selection,
  std::string& error_message
) {
  const std::filesystem::path output_dir(selection.output_dir);
  const std::filesystem::path bin_dir = output_dir / "bin";
  const std::filesystem::path binary_dest = bin_dir / styio_nano_binary_filename_latest();
  const std::filesystem::path profile_dest = output_dir / "styio-nano.profile.toml";
  const std::filesystem::path receipt_dest = output_dir / "styio-nano-package.toml";

  std::error_code ec;
  std::filesystem::create_directories(bin_dir, ec);
  if (ec) {
    error_message = "failed to create output directory: " + output_dir.string() + ": " + ec.message();
    return false;
  }

  if (!selection.registry_root.empty()) {
    std::string marker_text;
    if (!styio_fetch_registry_text_latest(
          selection.registry_root,
          styio_nano_repository_marker_relpath_latest(),
          marker_text,
          error_message)) {
      return false;
    }
    if (!styio_validate_nano_repository_marker_latest(marker_text, error_message)) {
      return false;
    }

    std::string entry_text;
    if (!styio_fetch_registry_text_latest(
          selection.registry_root,
          styio_nano_repository_entry_relpath_latest(selection.registry_package, selection.registry_version),
          entry_text,
          error_message)) {
      return false;
    }

    StyioNanoRepositoryEntryLatest entry;
    if (!styio_parse_nano_repository_entry_latest(
          entry_text,
          selection.registry_package,
          selection.registry_version,
          entry,
          error_message)) {
      return false;
    }

    const std::filesystem::path blob_path =
      std::filesystem::temp_directory_path() / ("styio-nano-blob-" + styio_now_token_latest() + ".tar");
    if (!styio_fetch_ref_to_file_latest(
          styio_join_registry_ref_latest(selection.registry_root, entry.blob_path),
          {},
          blob_path,
          false,
          error_message)) {
      return false;
    }

    std::error_code cleanup_ec;
    if (std::filesystem::file_size(blob_path, cleanup_ec) != entry.size_bytes) {
      std::filesystem::remove(blob_path, cleanup_ec);
      error_message = "nano repository blob size mismatch for " + selection.registry_package + "@"
                      + selection.registry_version;
      return false;
    }

    std::string blob_sha256;
    if (!styio_compute_file_sha256_latest(blob_path, blob_sha256, error_message)) {
      std::filesystem::remove(blob_path, cleanup_ec);
      return false;
    }
    if (blob_sha256 != entry.sha256) {
      std::filesystem::remove(blob_path, cleanup_ec);
      error_message = "nano repository blob sha256 mismatch for " + selection.registry_package + "@"
                      + selection.registry_version;
      return false;
    }

    const std::filesystem::path extract_root =
      std::filesystem::temp_directory_path() / ("styio-nano-extract-" + styio_now_token_latest());
    std::filesystem::create_directories(extract_root, cleanup_ec);
    const std::string extract_cmd =
      "tar -xf " + styio_shell_quote_latest(blob_path.string())
      + " -C " + styio_shell_quote_latest(extract_root.string());
    if (!styio_run_shell_command_latest(extract_cmd, "styio-nano blob extraction", error_message)) {
      std::filesystem::remove(blob_path, cleanup_ec);
      std::filesystem::remove_all(extract_root, cleanup_ec);
      return false;
    }

    std::filesystem::path package_root;
    if (!styio_resolve_extracted_nano_package_root_latest(extract_root, package_root, error_message)) {
      std::filesystem::remove(blob_path, cleanup_ec);
      std::filesystem::remove_all(extract_root, cleanup_ec);
      return false;
    }
    if (!styio_copy_directory_contents_latest(package_root, output_dir, error_message)) {
      std::filesystem::remove(blob_path, cleanup_ec);
      std::filesystem::remove_all(extract_root, cleanup_ec);
      return false;
    }
    styio_make_file_executable_latest(binary_dest);

    const std::string package_name =
      selection.package_name.empty()
        ? styio_nano_repository_package_leaf_latest(selection.registry_package)
        : selection.package_name;
    const std::string receipt =
      "[package]\n"
      "name = \"" + styio_toml_escape_string_latest(package_name) + "\"\n"
      "channel = \"" + styio_toml_escape_string_latest(entry.channel) + "\"\n"
      "mode = \"cloud\"\n"
      "binary = \"bin/" + styio_toml_escape_string_latest(styio_nano_binary_filename_latest()) + "\"\n"
      + (std::filesystem::exists(profile_dest)
           ? std::string("profile = \"styio-nano.profile.toml\"\n")
           : std::string())
      + "version = \"" + styio_toml_escape_string_latest(entry.version) + "\"\n"
      + "\n[source]\n"
      + "registry = \"" + styio_toml_escape_string_latest(selection.registry_root) + "\"\n"
      + "package = \"" + styio_toml_escape_string_latest(selection.registry_package) + "\"\n"
      + "version = \"" + styio_toml_escape_string_latest(selection.registry_version) + "\"\n"
      + "sha256 = \"" + styio_toml_escape_string_latest(entry.sha256) + "\"\n"
      + "blob_path = \"" + styio_toml_escape_string_latest(entry.blob_path) + "\"\n"
      + (entry.published_at.empty()
           ? std::string()
           : "published_at = \"" + styio_toml_escape_string_latest(entry.published_at) + "\"\n");
    const bool wrote_receipt = styio_write_text_file_latest(receipt_dest, receipt, error_message);
    std::filesystem::remove(blob_path, cleanup_ec);
    std::filesystem::remove_all(extract_root, cleanup_ec);
    return wrote_receipt;
  }

  const std::filesystem::path manifest_copy =
    std::filesystem::temp_directory_path() / ("styio-nano-manifest-" + styio_now_token_latest() + ".toml");
  std::filesystem::path manifest_base_dir;
  if (!styio_fetch_ref_to_file_latest(selection.manifest_ref, {}, manifest_copy, false, error_message)) {
    if (!styio_ref_is_http_url_latest(selection.manifest_ref) && !styio_ref_is_file_uri_latest(selection.manifest_ref)) {
      manifest_base_dir = std::filesystem::path(selection.manifest_ref).parent_path();
    }
    return false;
  }

  if (!styio_ref_is_http_url_latest(selection.manifest_ref)) {
    std::filesystem::path manifest_source_path =
      styio_ref_is_file_uri_latest(selection.manifest_ref)
        ? std::filesystem::path(styio_file_uri_to_path_latest(selection.manifest_ref))
        : std::filesystem::path(selection.manifest_ref);
    if (manifest_source_path.is_relative()) {
      manifest_source_path = styio_absolute_path_latest(manifest_source_path);
    }
    manifest_base_dir = manifest_source_path.parent_path();
  }

  StyioNanoPackageManifestLatest manifest;
  if (!styio_parse_nano_package_manifest_latest(manifest_copy, manifest, error_message)) {
    std::filesystem::remove(manifest_copy, ec);
    return false;
  }

  if (!styio_fetch_ref_to_file_latest(manifest.binary_ref, manifest_base_dir, binary_dest, true, error_message)) {
    std::filesystem::remove(manifest_copy, ec);
    return false;
  }
  if (!manifest.profile_ref.empty()) {
    if (!styio_fetch_ref_to_file_latest(manifest.profile_ref, manifest_base_dir, profile_dest, false, error_message)) {
      std::filesystem::remove(manifest_copy, ec);
      return false;
    }
  }

  const std::string package_name =
    selection.package_name.empty() ? manifest.package_name : selection.package_name;
  const std::string receipt =
    "[package]\n"
    "name = \"" + styio_toml_escape_string_latest(package_name) + "\"\n"
    "channel = \"" + styio_toml_escape_string_latest(manifest.channel) + "\"\n"
    "mode = \"cloud\"\n"
    "binary = \"bin/" + styio_toml_escape_string_latest(styio_nano_binary_filename_latest()) + "\"\n"
    + (manifest.profile_ref.empty() ? std::string()
                                    : "profile = \"styio-nano.profile.toml\"\n")
    + (manifest.version.empty() ? std::string()
                                : "version = \"" + styio_toml_escape_string_latest(manifest.version) + "\"\n")
    + "\n[source]\n"
    + "manifest = \"" + styio_toml_escape_string_latest(selection.manifest_ref) + "\"\n"
    + "artifact_binary = \"" + styio_toml_escape_string_latest(manifest.binary_ref) + "\"\n"
    + (manifest.profile_ref.empty() ? std::string()
                                    : "artifact_profile = \"" + styio_toml_escape_string_latest(manifest.profile_ref) + "\"\n");
  if (!styio_write_text_file_latest(receipt_dest, receipt, error_message)) {
    std::filesystem::remove(manifest_copy, ec);
    return false;
  }

  std::filesystem::remove(manifest_copy, ec);
  return true;
}

static bool
styio_publish_nano_package_latest(
  const StyioNanoPublishSelectionLatest& selection,
  std::string& error_message
) {
  std::filesystem::path repo_root =
    styio_ref_is_file_uri_latest(selection.registry_root)
      ? std::filesystem::path(styio_file_uri_to_path_latest(selection.registry_root))
      : std::filesystem::path(selection.registry_root);
  repo_root = styio_absolute_path_latest(repo_root);
  if (!styio_ensure_writable_nano_repository_latest(repo_root, error_message)) {
    return false;
  }

  const std::filesystem::path package_dir = styio_absolute_path_latest(selection.package_dir);
  const std::filesystem::path tar_path =
    std::filesystem::temp_directory_path() / ("styio-nano-publish-" + styio_now_token_latest() + ".tar");
  const std::string tar_cmd =
    "tar -cf " + styio_shell_quote_latest(tar_path.string())
    + " --exclude=.nano-build --exclude=./.nano-build"
    + " -C " + styio_shell_quote_latest(package_dir.string()) + " .";
  if (!styio_run_shell_command_latest(tar_cmd, "styio-nano package archive", error_message)) {
    return false;
  }

  std::string sha256;
  if (!styio_compute_file_sha256_latest(tar_path, sha256, error_message)) {
    std::error_code cleanup_ec;
    std::filesystem::remove(tar_path, cleanup_ec);
    return false;
  }

  std::error_code size_ec;
  const uint64_t size_bytes = std::filesystem::file_size(tar_path, size_ec);
  if (size_ec) {
    std::filesystem::remove(tar_path, size_ec);
    error_message = "failed to read styio-nano archive size: " + tar_path.string() + ": " + size_ec.message();
    return false;
  }

  const std::string blob_relpath = styio_nano_repository_blob_relpath_latest(sha256);
  const std::filesystem::path blob_dest = repo_root / std::filesystem::path(blob_relpath);
  if (!styio_copy_file_with_exec_latest(tar_path, blob_dest, false, error_message)) {
    std::error_code cleanup_ec;
    std::filesystem::remove(tar_path, cleanup_ec);
    return false;
  }

  StyioNanoRepositoryEntryLatest entry;
  entry.package_name = selection.registry_package;
  entry.version = selection.registry_version;
  entry.channel = selection.channel;
  entry.sha256 = sha256;
  entry.blob_path = blob_relpath;
  entry.size_bytes = size_bytes;
  entry.published_at = styio_now_utc_iso8601_latest();
  const bool ok = styio_write_nano_repository_entry_latest(repo_root, entry, error_message);
  std::error_code cleanup_ec;
  std::filesystem::remove(tar_path, cleanup_ec);
  return ok;
}

static bool
styio_resolve_dict_impl_selection_latest(
  const cxxopts::ParseResult& cmlopts,
  const std::string& file_path,
  StyioDictImplSelectionLatest& out_selection,
  std::string& error_message
) {
  if (const char* current_name = styio_dict_runtime_get_impl_name()) {
    out_selection.impl_name = current_name;
  }

  StyioProjectConfigLatest project_config;
  std::filesystem::path project_config_path;

  if (cmlopts.count("config")) {
    project_config_path = cmlopts["config"].as<std::string>();
    if (project_config_path.empty()) {
      error_message = "--config requires a non-empty path";
      return false;
    }
    if (!styio_parse_project_config_latest(project_config_path, project_config, error_message)) {
      return false;
    }
  }
  else if (styio_find_project_config_latest(file_path, project_config_path)) {
    if (!styio_parse_project_config_latest(project_config_path, project_config, error_message)) {
      return false;
    }
  }

  if (project_config.has_dict_impl) {
    const char* canonical =
      styio_dict_runtime_canonical_impl_name(project_config.dict_impl_raw.c_str());
    if (canonical == nullptr) {
      error_message =
        "unsupported dict_impl in config file " + project_config.loaded_from + ": "
        + project_config.dict_impl_raw;
      return false;
    }
    out_selection.impl_name = canonical;
    out_selection.source = "project-config";
    out_selection.config_path = project_config.loaded_from;
  }

  if (cmlopts.count("dict-impl")) {
    const std::string cli_raw = cmlopts["dict-impl"].as<std::string>();
    const char* canonical = styio_dict_runtime_canonical_impl_name(cli_raw.c_str());
    if (canonical == nullptr) {
      error_message = "unsupported --dict-impl: " + cli_raw;
      return false;
    }
    out_selection.impl_name = canonical;
    out_selection.source = "cli";
  }

  if (out_selection.impl_name.empty()) {
    out_selection.impl_name = "ordered-hash";
  }
  return true;
}

static void
styio_emit_machine_info_json(const StyioDictImplSelectionLatest& dict_impl_selection) {
  const char* active_integration_phase =
#if STYIO_NANO_BUILD
    "bootstrap-single-file";
#else
    "compile-plan-live";
#endif
  std::cout
    << "{\"tool\":\"styio\""
    << ",\"compiler_version\":\"" << styio_json_escape(STYIO_PROJECT_VERSION) << "\""
    << ",\"channel\":\"" << styio_json_escape(STYIO_RELEASE_CHANNEL) << "\""
    << ",\"variant\":\"" << (STYIO_NANO_BUILD ? "nano" : "full") << "\""
    << ",\"active_integration_phase\":\"" << active_integration_phase << "\""
    << ",\"supported_contracts\":{\"machine_info\":[1],\"jsonl_diagnostics\":[1],\"compile_plan\":"
#if STYIO_NANO_BUILD
    << "[]"
#else
    << "[1]"
#endif
    << ",\"runtime_events\":"
#if STYIO_NANO_BUILD
    << "[]"
#else
    << "[1]"
#endif
    << "}"
    << ",\"supported_contract_versions\":{\"machine_info\":[1],\"jsonl_diagnostics\":[1],\"compile_plan\":"
#if STYIO_NANO_BUILD
    << "[]"
#else
    << "[1]"
#endif
    << ",\"runtime_events\":"
#if STYIO_NANO_BUILD
    << "[]"
#else
    << "[1]"
#endif
    << "}"
    << ",\"supported_adapter_modes\":[\"cli\"]"
    << ",\"feature_flags\":{\"single_file_entry\":true"
    << ",\"jsonl_diagnostics\":true"
    << ",\"compile_plan_consumer\":"
#if STYIO_NANO_BUILD
    << "false"
#else
    << "true"
#endif
    << ",\"project_execution_via_compile_plan\":"
#if STYIO_NANO_BUILD
    << "false"
#else
    << "true"
#endif
    << ",\"runtime_event_stream\":"
#if STYIO_NANO_BUILD
    << "false"
#else
    << "true"
#endif
    << "}"
    << ",\"capabilities\":["
    << "\"machine_info_json\","
    << "\"single_file_entry\","
    << "\"jsonl_diagnostics\"";
#if !STYIO_NANO_BUILD
  std::cout << ",\"nano_package_materialize\"";
  std::cout << ",\"nano_package_local_subset_closure_v1\"";
  std::cout << ",\"nano_package_registry_consume_v1\"";
  std::cout << ",\"nano_package_registry_publish_v1\"";
#endif
  std::cout
    << "]"
    << ",\"dict_impl\":{\"selected\":\""
    << styio_json_escape(dict_impl_selection.impl_name)
    << "\",\"source\":\"" << styio_json_escape(dict_impl_selection.source)
    << "\",\"supported\":[";
  const int supported_count = styio_dict_runtime_supported_impl_count();
  for (int i = 0; i < supported_count; ++i) {
    if (i > 0) {
      std::cout << ",";
    }
    const char* supported_name = styio_dict_runtime_supported_impl_name(i);
    std::cout << "\"" << styio_json_escape(supported_name != nullptr ? supported_name : "") << "\"";
  }
  std::cout << "]";
  if (!dict_impl_selection.config_path.empty()) {
    std::cout << ",\"config_file\":\"" << styio_json_escape(dict_impl_selection.config_path) << "\"";
  }
  std::cout
    << "}";
#if STYIO_NANO_BUILD
  std::cout
    << ",\"nano_profile\":{\"name\":\"" << styio_json_escape(STYIO_NANO_PROFILE_NAME)
    << "\",\"source\":\"" << styio_json_escape(STYIO_NANO_PROFILE_SOURCE)
    << "\",\"compiler\":{\"machine_info\":" << (STYIO_NANO_ENABLE_MACHINE_INFO ? "true" : "false")
    << ",\"debug_cli\":" << (STYIO_NANO_ENABLE_DEBUG_CLI ? "true" : "false")
    << ",\"ast_dump_cli\":" << (STYIO_NANO_ENABLE_AST_DUMP_CLI ? "true" : "false")
    << ",\"styio_ir_dump_cli\":" << (STYIO_NANO_ENABLE_STYIO_IR_DUMP_CLI ? "true" : "false")
    << ",\"llvm_ir_dump_cli\":" << (STYIO_NANO_ENABLE_LLVM_IR_DUMP_CLI ? "true" : "false")
    << ",\"legacy_parser\":" << (STYIO_NANO_ENABLE_LEGACY_PARSER ? "true" : "false")
    << ",\"parser_shadow_compare\":" << (STYIO_NANO_ENABLE_PARSER_SHADOW_COMPARE ? "true" : "false")
    << "},\"build\":{\"pipeline_check\":" << (STYIO_NANO_INCLUDE_PIPELINE_CHECK ? "true" : "false")
    << "}}";
#endif
  std::cout
    << ",\"edition_max\":\"" << styio_json_escape(STYIO_EDITION_MAX) << "\""
    << "}\n";
}

struct StyioDiagnosticSinkLatest
{
  bool enabled = false;
  std::filesystem::path path;
};

static StyioDiagnosticSinkLatest g_styio_diagnostic_sink_latest;

struct StyioRuntimeEventSinkLatest
{
  bool enabled = false;
  std::filesystem::path path;
  std::string session_id;
  std::uint64_t next_sequence = 1;
};

static StyioRuntimeEventSinkLatest g_styio_runtime_event_sink_latest;
static std::atomic<unsigned long long> g_styio_runtime_event_session_counter_latest{1};

static std::string
styio_render_diagnostic_jsonl_latest(
  StyioErrorCategory category,
  const std::string& file_path,
  const std::string& message,
  const std::string& subcode
) {
  std::ostringstream out;
  out << "{\"severity\":\"error\""
      << ",\"category\":\"" << styio_category_name(category)
      << "\",\"code\":\"" << styio_category_code(category) << "\"";
  if (!subcode.empty()) {
    out << ",\"subcode\":\"" << styio_json_escape(subcode) << "\"";
  }
  out << ",\"file\":\"" << styio_json_escape(file_path)
      << "\",\"message\":\"" << styio_json_escape(message)
      << "\"}";
  return out.str();
}

static void
styio_clear_diagnostic_sink_latest() {
  g_styio_diagnostic_sink_latest = StyioDiagnosticSinkLatest{};
}

static std::string
styio_make_runtime_session_id_latest() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto micros =
    std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const auto ordinal = g_styio_runtime_event_session_counter_latest.fetch_add(1);
  return "styio-session-" + std::to_string(micros) + "-" + std::to_string(ordinal);
}

static void
styio_clear_runtime_event_sink_latest() {
  g_styio_runtime_event_sink_latest = StyioRuntimeEventSinkLatest{};
}

static bool
styio_append_text_file_latest(
  const std::filesystem::path& path,
  const std::string& text,
  std::string& error_message
);

static void
styio_set_runtime_event_sink_latest(const std::filesystem::path& build_root) {
  g_styio_runtime_event_sink_latest.enabled = !build_root.empty();
  g_styio_runtime_event_sink_latest.path = build_root / "runtime-events.jsonl";
  g_styio_runtime_event_sink_latest.session_id = styio_make_runtime_session_id_latest();
  g_styio_runtime_event_sink_latest.next_sequence = 1;
}

static void
styio_emit_runtime_event_latest(
  const std::string& event_kind,
  const std::string& origin,
  const std::string& payload_json = "{}"
) {
  if (!g_styio_runtime_event_sink_latest.enabled) {
    return;
  }

  std::ostringstream out;
  out << "{\"schema_version\":1"
      << ",\"session_id\":\"" << styio_json_escape(g_styio_runtime_event_sink_latest.session_id) << "\""
      << ",\"sequence\":" << g_styio_runtime_event_sink_latest.next_sequence++
      << ",\"timestamp\":\"" << styio_json_escape(styio_now_utc_iso8601_latest()) << "\""
      << ",\"eventKind\":\"" << styio_json_escape(event_kind) << "\""
      << ",\"origin\":\"" << styio_json_escape(origin) << "\""
      << ",\"payload\":" << payload_json
      << "}";

  const std::string event_line = out.str() + "\n";
  std::string sink_error;
  (void)styio_append_text_file_latest(
    g_styio_runtime_event_sink_latest.path,
    event_line,
    sink_error);
}

static void
styio_runtime_log_sink_latest(const char* stream, const char* message) {
  if (!g_styio_runtime_event_sink_latest.enabled || stream == nullptr || message == nullptr) {
    return;
  }

  std::ostringstream payload;
  payload << "{\"stream\":\"" << styio_json_escape(stream)
          << "\",\"message\":\"" << styio_json_escape(message)
          << "\"}";
  styio_emit_runtime_event_latest(
    "log.emitted",
    "styio.runtime",
    payload.str());
}

static const char*
styio_runtime_phase_name_latest(CompilationPhase phase) {
  switch (phase) {
    case CompilationPhase::Empty:
      return "empty";
    case CompilationPhase::Tokenized:
      return "tokenized";
    case CompilationPhase::Parsed:
      return "parsed";
    case CompilationPhase::Typed:
      return "typed";
    case CompilationPhase::Lowered:
      return "lowered";
    case CompilationPhase::CodegenReady:
      return "codegen_ready";
    case CompilationPhase::Executed:
      return "executed";
    case CompilationPhase::Failed:
      return "failed";
  }
  return "unknown";
}

static void
styio_emit_runtime_phase_transition_latest(
  CompilationPhase from_phase,
  CompilationPhase to_phase,
  const char* operation,
  const std::string& file_path,
  const std::string* intent
) {
  if (!g_styio_runtime_event_sink_latest.enabled || from_phase == to_phase) {
    return;
  }

  std::ostringstream transition_payload;
  transition_payload << "{\"from\":\""
                     << styio_json_escape(styio_runtime_phase_name_latest(from_phase))
                     << "\",\"to\":\""
                     << styio_json_escape(styio_runtime_phase_name_latest(to_phase))
                     << "\",\"operation\":\""
                     << styio_json_escape(operation != nullptr ? operation : "")
                     << "\",\"file\":\""
                     << styio_json_escape(file_path)
                     << "\"";
  if (intent != nullptr) {
    transition_payload << ",\"intent\":\"" << styio_json_escape(*intent) << "\"";
  }
  transition_payload << "}";
  styio_emit_runtime_event_latest(
    "transition.fired",
    "styio.session",
    transition_payload.str());

  std::ostringstream state_payload;
  state_payload << "{\"phase\":\""
                << styio_json_escape(styio_runtime_phase_name_latest(to_phase))
                << "\",\"file\":\""
                << styio_json_escape(file_path)
                << "\"";
  if (intent != nullptr) {
    state_payload << ",\"intent\":\"" << styio_json_escape(*intent) << "\"";
  }
  state_payload << "}";
  styio_emit_runtime_event_latest(
    "state.changed",
    "styio.session",
    state_payload.str());
}

static void
styio_set_diagnostic_sink_latest(const std::filesystem::path& diag_dir) {
  g_styio_diagnostic_sink_latest.enabled = !diag_dir.empty();
  g_styio_diagnostic_sink_latest.path = diag_dir / "diagnostics.jsonl";
}

static bool
styio_append_text_file_latest(
  const std::filesystem::path& path,
  const std::string& text,
  std::string& error_message
) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    error_message = "cannot create file parent directory: " + path.parent_path().string();
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out.is_open()) {
    error_message = "cannot open file for appending: " + path.string();
    return false;
  }
  out << text;
  if (!out.good()) {
    error_message = "failed to append file: " + path.string();
    return false;
  }
  return true;
}

static bool
styio_ensure_text_file_exists_latest(
  const std::filesystem::path& path,
  std::string& error_message
) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    error_message = "cannot create file parent directory: " + path.parent_path().string();
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out.is_open()) {
    error_message = "cannot open file for writing: " + path.string();
    return false;
  }
  return true;
}

static void
styio_append_diagnostic_to_sink_latest(const std::string& jsonl_line) {
  if (!g_styio_diagnostic_sink_latest.enabled) {
    return;
  }

  std::string sink_error;
  (void)styio_append_text_file_latest(
    g_styio_diagnostic_sink_latest.path,
    jsonl_line + "\n",
    sink_error);
}

static bool
styio_read_text_file_latest(
  const std::filesystem::path& path,
  std::string& out_text,
  std::string& error_message
);

static void
styio_emit_diagnostic(
  const std::string& format,
  StyioErrorCategory category,
  const std::string& file_path,
  const std::string& message,
  const std::string& subcode
);

static bool
styio_probe_compile_plan_diag_dir_latest(
  const std::filesystem::path& plan_path,
  std::filesystem::path& out_diag_dir
) {
  std::string plan_text;
  std::string error_message;
  if (!styio_read_text_file_latest(plan_path, plan_text, error_message)) {
    return false;
  }

  llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(plan_text);
  if (!parsed) {
    return false;
  }
  const llvm::json::Object* root = parsed->getAsObject();
  if (root == nullptr) {
    return false;
  }

  const llvm::json::Object* outputs = root->getObject("outputs");
  if (outputs == nullptr) {
    return false;
  }

  const auto diag_dir = outputs->getString("diag_dir");
  if (!diag_dir.has_value() || diag_dir->empty()) {
    return false;
  }

  const std::filesystem::path candidate{std::string(*diag_dir)};
  if (!candidate.is_absolute()) {
    return false;
  }

  out_diag_dir = candidate;
  return true;
}

static int
styio_emit_compile_plan_cli_error_latest(
  const std::filesystem::path& plan_path,
  const std::string& message,
  const std::string& subcode = "compile_plan_invalid"
) {
  styio_clear_diagnostic_sink_latest();

  std::filesystem::path diag_dir;
  if (styio_probe_compile_plan_diag_dir_latest(plan_path, diag_dir)) {
    styio_set_diagnostic_sink_latest(diag_dir);
  }

  styio_emit_diagnostic("jsonl", StyioErrorCategory::CliError, plan_path.string(), message, subcode);
  styio_clear_diagnostic_sink_latest();
  return static_cast<int>(StyioExitCode::CliError);
}

static void
styio_emit_diagnostic(
  const std::string& format,
  StyioErrorCategory category,
  const std::string& file_path,
  const std::string& message,
  const std::string& subcode = ""
) {
  const std::string jsonl_line =
    styio_render_diagnostic_jsonl_latest(category, file_path, message, subcode);
  styio_append_diagnostic_to_sink_latest(jsonl_line);
  styio_emit_runtime_event_latest(
    "diagnostic.emitted",
    "styio.diagnostics",
    jsonl_line);

  if (styio_error_jsonl_enabled(format)) {
    std::cerr << jsonl_line << "\n";
    return;
  }

  std::cerr << "[" << styio_category_name(category) << "] " << message << std::endl;
}

struct StyioCompilePlanRequestLatest
{
  std::filesystem::path plan_path;
  int plan_version = 0;
  std::string intent;
  std::filesystem::path workspace_root;
  std::string entry_package_id;
  std::string entry_target_kind;
  std::string entry_target_name;
  std::filesystem::path entry_file;
  std::filesystem::path build_root;
  std::filesystem::path artifact_dir;
  std::filesystem::path diag_dir;
  std::string error_format = "text";
  bool emit_ast = false;
  bool emit_styio_ir = false;
  bool emit_llvm_ir = false;
};

static std::string
styio_compile_plan_unit_id_latest(const StyioCompilePlanRequestLatest& request) {
  std::string unit_id = request.entry_package_id;
  if (!request.entry_target_kind.empty()) {
    if (!unit_id.empty()) {
      unit_id += "::";
    }
    unit_id += request.entry_target_kind;
  }
  if (!request.entry_target_name.empty()) {
    if (!unit_id.empty()) {
      unit_id += ":";
    }
    unit_id += request.entry_target_name;
  }
  if (unit_id.empty()) {
    unit_id = request.entry_file.string();
  }
  return unit_id;
}

static std::string
styio_render_compile_plan_unit_payload_latest(
  const StyioCompilePlanRequestLatest& request,
  const std::string& intent,
  const bool* success = nullptr,
  const bool* executed = nullptr,
  const CompilationPhase* final_phase = nullptr
) {
  std::ostringstream payload;
  payload << "{\"unit_id\":\""
          << styio_json_escape(styio_compile_plan_unit_id_latest(request))
          << "\",\"package_id\":\""
          << styio_json_escape(request.entry_package_id)
          << "\",\"target_kind\":\""
          << styio_json_escape(request.entry_target_kind)
          << "\",\"target_name\":\""
          << styio_json_escape(request.entry_target_name)
          << "\",\"intent\":\""
          << styio_json_escape(intent)
          << "\",\"file\":\""
          << styio_json_escape(request.entry_file.string())
          << "\"";
  if (request.entry_target_kind == "test" || intent == "test") {
    payload << ",\"test_name\":\""
            << styio_json_escape(request.entry_target_name)
            << "\"";
  }
  if (success != nullptr) {
    payload << ",\"success\":" << (*success ? "true" : "false");
  }
  if (executed != nullptr) {
    payload << ",\"executed\":" << (*executed ? "true" : "false");
  }
  if (final_phase != nullptr) {
    payload << ",\"final_phase\":\""
            << styio_json_escape(styio_runtime_phase_name_latest(*final_phase))
            << "\"";
  }
  payload << "}";
  return payload.str();
}

static bool
styio_read_text_file_latest(
  const std::filesystem::path& path,
  std::string& out_text,
  std::string& error_message
);

static bool
styio_json_require_string_latest(
  const llvm::json::Object& obj,
  const char* key,
  std::string& out_value,
  std::string& error_message
) {
  const auto raw = obj.getString(key);
  if (!raw.has_value() || raw->empty()) {
    error_message = std::string("compile-plan is missing required string field: ") + key;
    return false;
  }
  out_value = std::string(*raw);
  return true;
}

static bool
styio_json_require_integer_latest(
  const llvm::json::Object& obj,
  const char* key,
  std::int64_t& out_value,
  std::string& error_message
) {
  const auto raw = obj.getInteger(key);
  if (!raw.has_value()) {
    error_message = std::string("compile-plan is missing required integer field: ") + key;
    return false;
  }
  out_value = *raw;
  return true;
}

static bool
styio_json_require_bool_latest(
  const llvm::json::Object& obj,
  const char* key,
  bool& out_value,
  std::string& error_message
) {
  auto raw = obj.getBoolean(key);
  if (!raw.has_value()) {
    error_message = std::string("compile-plan is missing required boolean field: ") + key;
    return false;
  }
  out_value = *raw;
  return true;
}

static bool
styio_json_require_object_latest(
  const llvm::json::Object& obj,
  const char* key,
  const llvm::json::Object*& out_value,
  std::string& error_message
) {
  out_value = obj.getObject(key);
  if (out_value == nullptr) {
    error_message = std::string("compile-plan is missing required object field: ") + key;
    return false;
  }
  return true;
}

static bool
styio_json_require_array_latest(
  const llvm::json::Object& obj,
  const char* key,
  const llvm::json::Array*& out_value,
  std::string& error_message
) {
  out_value = obj.getArray(key);
  if (out_value == nullptr) {
    error_message = std::string("compile-plan is missing required array field: ") + key;
    return false;
  }
  return true;
}

static bool
styio_compile_plan_require_absolute_path_latest(
  const llvm::json::Object& obj,
  const char* key,
  std::filesystem::path& out_value,
  std::string& error_message
) {
  std::string raw_value;
  if (!styio_json_require_string_latest(obj, key, raw_value, error_message)) {
    return false;
  }
  out_value = std::filesystem::path(raw_value);
  if (!out_value.is_absolute()) {
    error_message = std::string("compile-plan path must be absolute: ") + key;
    return false;
  }
  return true;
}

static std::string
styio_compile_plan_artifact_stem_latest(const StyioCompilePlanRequestLatest& request) {
  std::string stem = request.entry_target_name.empty()
                       ? request.entry_file.stem().string()
                       : request.entry_target_name;
  if (stem.empty()) {
    stem = "entry";
  }
  for (char& ch : stem) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (!(std::isalnum(uch) || ch == '-' || ch == '_' || ch == '.')) {
      ch = '_';
    }
  }
  return stem;
}

static bool
styio_write_compile_plan_artifact_latest(
  const std::filesystem::path& path,
  const std::string& text,
  std::string& error_message
) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    error_message = "cannot create artifact directory: " + path.parent_path().string();
    return false;
  }
  return styio_write_text_file_latest(path, text, error_message);
}

static bool
styio_write_compile_plan_receipt_latest(
  const StyioCompilePlanRequestLatest& request,
  const std::vector<std::filesystem::path>& artifacts,
  const std::string& dict_impl_name,
  const std::string& session_id,
  bool executed,
  long long wall_time_ms,
  std::string& error_message
) {
  std::ostringstream receipt;
  receipt << "{"
          << "\"schema_version\":1"
          << ",\"tool\":\"styio\""
          << ",\"compiler_version\":\"" << styio_json_escape(STYIO_PROJECT_VERSION) << "\""
          << ",\"channel\":\"" << styio_json_escape(STYIO_RELEASE_CHANNEL) << "\""
          << ",\"plan_version\":" << request.plan_version
          << ",\"intent\":\"" << styio_json_escape(request.intent) << "\""
          << ",\"session_id\":\"" << styio_json_escape(session_id) << "\""
          << ",\"executed\":" << (executed ? "true" : "false")
          << ",\"wall_time_ms\":" << wall_time_ms
          << ",\"generated_at\":\"" << styio_json_escape(styio_now_utc_iso8601_latest()) << "\""
          << ",\"dict_impl\":{\"selected\":\"" << styio_json_escape(dict_impl_name) << "\"}"
          << ",\"entry\":{\"package_id\":\"" << styio_json_escape(request.entry_package_id)
          << "\",\"target_kind\":\"" << styio_json_escape(request.entry_target_kind)
          << "\",\"target_name\":\"" << styio_json_escape(request.entry_target_name)
          << "\",\"file\":\"" << styio_json_escape(request.entry_file.string()) << "\"}"
          << ",\"outputs\":{\"build_root\":\"" << styio_json_escape(request.build_root.string())
          << "\",\"artifact_dir\":\"" << styio_json_escape(request.artifact_dir.string())
          << "\",\"diag_dir\":\"" << styio_json_escape(request.diag_dir.string()) << "\""
          << ",\"runtime_events_path\":\""
          << styio_json_escape((request.build_root / "runtime-events.jsonl").string())
          << "\"}"
          << ",\"artifacts\":[";
  for (size_t i = 0; i < artifacts.size(); ++i) {
    if (i > 0) {
      receipt << ",";
    }
    receipt << "\"" << styio_json_escape(artifacts[i].string()) << "\"";
  }
  receipt << "]}\n";

  return styio_write_compile_plan_artifact_latest(
    request.build_root / "receipt.json",
    receipt.str(),
    error_message);
}

static bool
styio_parse_compile_plan_latest(
  const std::filesystem::path& plan_path,
  StyioCompilePlanRequestLatest& out_request,
  std::string& error_message
) {
  std::string plan_text;
  if (!styio_read_text_file_latest(plan_path, plan_text, error_message)) {
    return false;
  }

  llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(plan_text);
  if (!parsed) {
    error_message = "compile-plan is not valid JSON: " + llvm::toString(parsed.takeError());
    return false;
  }
  const llvm::json::Object* root = parsed->getAsObject();
  if (root == nullptr) {
    error_message = "compile-plan must be a JSON object";
    return false;
  }

  const llvm::json::Object* generated_by = nullptr;
  const llvm::json::Object* entry = nullptr;
  const llvm::json::Object* toolchain = nullptr;
  const llvm::json::Object* profile = nullptr;
  const llvm::json::Object* resolution = nullptr;
  const llvm::json::Object* outputs = nullptr;
  const llvm::json::Object* emit = nullptr;
  const llvm::json::Array* packages = nullptr;
  std::int64_t plan_version = 0;

  if (!styio_json_require_integer_latest(*root, "plan_version", plan_version, error_message)
      || !styio_json_require_object_latest(*root, "generated_by", generated_by, error_message)
      || !styio_json_require_string_latest(*root, "intent", out_request.intent, error_message)
      || !styio_compile_plan_require_absolute_path_latest(*root, "workspace_root", out_request.workspace_root, error_message)
      || !styio_json_require_object_latest(*root, "entry", entry, error_message)
      || !styio_json_require_object_latest(*root, "toolchain", toolchain, error_message)
      || !styio_json_require_object_latest(*root, "profile", profile, error_message)
      || !styio_json_require_array_latest(*root, "packages", packages, error_message)
      || !styio_json_require_object_latest(*root, "resolution", resolution, error_message)
      || !styio_json_require_object_latest(*root, "outputs", outputs, error_message)
      || !styio_json_require_object_latest(*root, "emit", emit, error_message)) {
    return false;
  }

  (void)toolchain;
  (void)profile;
  (void)resolution;

  if (plan_version != 1) {
    error_message = "unsupported compile-plan version: " + std::to_string(plan_version);
    return false;
  }
  out_request.plan_version = static_cast<int>(plan_version);
  out_request.plan_path = plan_path;

  std::string generated_by_tool;
  std::string generated_by_version;
  if (!styio_json_require_string_latest(*generated_by, "tool", generated_by_tool, error_message)
      || !styio_json_require_string_latest(*generated_by, "version", generated_by_version, error_message)) {
    return false;
  }
  (void)generated_by_version;
  if (generated_by_tool != "spio") {
    error_message = "compile-plan generated_by.tool must equal \"spio\"";
    return false;
  }

  if (!(out_request.intent == "build"
        || out_request.intent == "check"
        || out_request.intent == "run"
        || out_request.intent == "test")) {
    error_message = "unsupported compile-plan intent: " + out_request.intent;
    return false;
  }
  if (packages->empty()) {
    error_message = "compile-plan packages array must not be empty";
    return false;
  }

  if (!styio_json_require_string_latest(*entry, "package_id", out_request.entry_package_id, error_message)
      || !styio_json_require_string_latest(*entry, "target_kind", out_request.entry_target_kind, error_message)
      || !styio_json_require_string_latest(*entry, "target_name", out_request.entry_target_name, error_message)
      || !styio_compile_plan_require_absolute_path_latest(*entry, "file", out_request.entry_file, error_message)) {
    return false;
  }
  if (!(out_request.entry_target_kind == "lib"
        || out_request.entry_target_kind == "bin"
        || out_request.entry_target_kind == "test")) {
    error_message = "unsupported compile-plan entry.target_kind: " + out_request.entry_target_kind;
    return false;
  }

  if (!styio_compile_plan_require_absolute_path_latest(*outputs, "build_root", out_request.build_root, error_message)
      || !styio_compile_plan_require_absolute_path_latest(*outputs, "artifact_dir", out_request.artifact_dir, error_message)
      || !styio_compile_plan_require_absolute_path_latest(*outputs, "diag_dir", out_request.diag_dir, error_message)) {
    return false;
  }

  if (!styio_json_require_string_latest(*emit, "error_format", out_request.error_format, error_message)
      || !styio_json_require_bool_latest(*emit, "ast", out_request.emit_ast, error_message)
      || !styio_json_require_bool_latest(*emit, "styio_ir", out_request.emit_styio_ir, error_message)
      || !styio_json_require_bool_latest(*emit, "llvm_ir", out_request.emit_llvm_ir, error_message)) {
    return false;
  }
  if (!(out_request.error_format == "text" || out_request.error_format == "jsonl")) {
    error_message = "unsupported compile-plan emit.error_format: " + out_request.error_format;
    return false;
  }

  return true;
}

static void
styio_free_tokens(std::vector<StyioToken*>& tokens) {
  for (auto* tok : tokens) {
    delete tok;
  }
  tokens.clear();
}

static bool
styio_parse_engine_to_repr_latest(
  const std::string& file_path,
  const std::string& code_text,
  const std::vector<std::pair<size_t, size_t>>& line_seps,
  bool debug_mode,
  StyioParserEngine engine,
  std::string& out_repr,
  std::string& out_error,
  std::string* out_detail = nullptr
) {
  std::vector<StyioToken*> tokens;
  StyioContext* ctx = nullptr;
  MainBlockAST* ast = nullptr;
  try {
    tokens = StyioTokenizer::tokenize(code_text);
    ctx = StyioContext::Create(file_path, code_text, line_seps, tokens, debug_mode);
    StyioParserRouteStats route_stats;
    StyioParserRouteStats* route_stats_ptr =
      engine == StyioParserEngine::Nightly ? &route_stats : nullptr;
    ast = parse_main_block_with_engine_latest(*ctx, engine, route_stats_ptr);
    StyioRepr repr;
    out_repr = ast->toString(&repr);
    if (out_detail != nullptr) {
      if (route_stats_ptr != nullptr) {
        *out_detail = "nightly_subset_statements=" + std::to_string(route_stats.nightly_subset_statements)
                      + ",legacy_fallback_statements="
                      + std::to_string(route_stats.legacy_fallback_statements)
                      + ",nightly_internal_legacy_bridges="
                      + std::to_string(route_stats.nightly_internal_legacy_bridges)
                      + ",nightly_declined_statements="
                      + std::to_string(route_stats.nightly_declined_statements);
      }
      else {
        out_detail->clear();
      }
    }
    delete ast;
    delete ctx;
    styio_free_tokens(tokens);
    return true;
  } catch (const std::exception& ex) {
    out_error = ex.what();
  }

  delete ast;
  delete ctx;
  styio_free_tokens(tokens);
  return false;
}

static std::string
styio_hash_hex(const std::string& text) {
  std::ostringstream oss;
  oss << std::hex << std::hash<std::string> {}(text);
  return oss.str();
}

static void
styio_write_shadow_artifact_latest(
  const std::string& artifact_dir,
  const std::string& file_path,
  const std::string& code_text,
  StyioParserEngine primary_engine,
  StyioParserEngine shadow_engine,
  const std::string& status,
  const std::string& primary_repr,
  const std::string& shadow_repr,
  const std::string& shadow_error,
  const std::string& detail
) {
  if (artifact_dir.empty()) {
    return;
  }

  try {
    std::filesystem::create_directories(artifact_dir);
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const long long micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

    std::filesystem::path base = std::filesystem::path(artifact_dir) / ("shadow-" + std::to_string(micros));
    int suffix = 1;
    while (std::filesystem::exists(base.string() + ".jsonl")) {
      base = std::filesystem::path(artifact_dir)
             / ("shadow-" + std::to_string(micros) + "-" + std::to_string(suffix));
      ++suffix;
    }

    std::ofstream meta(base.string() + ".jsonl", std::ios::out | std::ios::trunc);
    if (!meta.is_open()) {
      return;
    }
    meta << "{\"status\":\"" << styio_json_escape(status)
         << "\",\"file\":\"" << styio_json_escape(file_path)
         << "\",\"primary_engine\":\"" << styio_parser_engine_name_latest(primary_engine)
         << "\",\"shadow_engine\":\"" << styio_parser_engine_name_latest(shadow_engine)
         << "\",\"primary_repr_hash\":\"" << styio_hash_hex(primary_repr)
         << "\",\"shadow_repr_hash\":\"" << styio_hash_hex(shadow_repr)
         << "\",\"primary_repr_size\":" << primary_repr.size()
         << ",\"shadow_repr_size\":" << shadow_repr.size()
         << ",\"shadow_error\":\"" << styio_json_escape(shadow_error)
         << "\",\"detail\":\"" << styio_json_escape(detail)
         << "\"}\n";
    meta.close();

    if (status == "match") {
      return;
    }

    std::ofstream source_payload(base.string() + ".styio", std::ios::out | std::ios::trunc);
    if (source_payload.is_open()) {
      source_payload << code_text;
    }

    std::ofstream primary_payload(base.string() + ".primary.ast.txt", std::ios::out | std::ios::trunc);
    if (primary_payload.is_open()) {
      primary_payload << primary_repr;
    }

    if (!shadow_repr.empty()) {
      std::ofstream shadow_payload(base.string() + ".shadow.ast.txt", std::ios::out | std::ios::trunc);
      if (shadow_payload.is_open()) {
        shadow_payload << shadow_repr;
      }
    }

    if (!shadow_error.empty()) {
      std::ofstream error_payload(base.string() + ".shadow.err.txt", std::ios::out | std::ios::trunc);
      if (error_payload.is_open()) {
        error_payload << shadow_error;
      }
    }
  } catch (const std::exception&) {
  }
}

int
main(
  int argc,
  char* argv[]
) {
  cxxopts::Options options("styio", "Styio Compiler");

  // styio example.styio
  options.allow_unrecognised_options();

  options.add_options()(
    "f,file", "Take the given source file.", cxxopts::value<std::string>()
  )(
    "h,help", "Show All Command-Line Options"
  );

  options.add_options()(
    "a", "Reserved (no effect).", cxxopts::value<bool>()->default_value("false")
  );
#if STYIO_NANO_ENABLE_AST_DUMP_CLI
  options.add_options()(
    "styio-ast", "Print AST before and after type inference (plain headers when stdout is not a tty).",
    cxxopts::value<bool>()->default_value("false")
  );
#endif
#if STYIO_NANO_ENABLE_STYIO_IR_DUMP_CLI
  options.add_options()(
    "styio-ir", "Print lowered Styio IR before LLVM codegen.",
    cxxopts::value<bool>()->default_value("false")
  );
#endif
#if STYIO_NANO_ENABLE_LLVM_IR_DUMP_CLI
  options.add_options()(
    "llvm-ir",
    "Print LLVM IR for the module (before running main).",
    cxxopts::value<bool>()->default_value("false")
  );
#endif

  options.add_options()(
    "config", "Read project configuration from the given file. When omitted, styio.toml or .styio.toml is auto-discovered upward from --file.",
    cxxopts::value<std::string>()
  )(
    "dict-impl", "Dictionary backend selector (ordered-hash|linear). Accepts aliases v2 and v1.",
    cxxopts::value<std::string>()
  )(
    "error-format", "Diagnostic output format: text|jsonl",
    cxxopts::value<std::string>()->default_value("text")
  );

#if !STYIO_NANO_BUILD
  options.add_options()(
    "compile-plan",
    "Read a versioned compile-plan JSON and treat it as the full compiler request envelope.",
    cxxopts::value<std::string>()
  )(
    "nano-create",
    "Materialize a styio-nano package using a local-subset profile or a cloud repository/package source.",
    cxxopts::value<bool>()->default_value("false")
  )(
    "nano-publish",
    "Publish a materialized styio-nano package into a local static repository layout.",
    cxxopts::value<bool>()->default_value("false")
  )(
    "nano-package-config",
    "Read styio-nano package configuration from the given TOML file.",
    cxxopts::value<std::string>()
  )(
    "nano-publish-config",
    "Read styio-nano publish configuration from the given TOML file.",
    cxxopts::value<std::string>()
  )(
    "nano-mode",
    "styio-nano package creation mode: local-subset|cloud.",
    cxxopts::value<std::string>()
  )(
    "nano-output",
    "Output directory for the materialized styio-nano package.",
    cxxopts::value<std::string>()
  )(
    "nano-name",
    "Optional package name override for the materialized styio-nano package.",
    cxxopts::value<std::string>()
  )(
    "nano-profile",
    "styio-nano profile TOML used for local-subset package creation.",
    cxxopts::value<std::string>()
  )(
    "nano-binary",
    "Existing styio-nano executable to package for local-subset mode. When omitted, a sibling styio-nano next to the current compiler is probed.",
    cxxopts::value<std::string>()
  )(
    "nano-source-root",
    "Source tree recorded in the local-subset package rebuild helper.",
    cxxopts::value<std::string>()
  )(
    "nano-package-dir",
    "Materialized styio-nano package directory to publish into a static repository.",
    cxxopts::value<std::string>()
  )(
    "nano-channel",
    "Optional channel override when publishing a styio-nano package.",
    cxxopts::value<std::string>()
  )(
    "nano-manifest",
    "Legacy cloud package manifest path or URL for styio-nano package materialization.",
    cxxopts::value<std::string>()
  )(
    "nano-registry",
    "styio-nano static repository root (path, file://, http://, or https://) for cloud package materialization.",
    cxxopts::value<std::string>()
  )(
    "nano-package",
    "Package id to resolve inside the styio-nano static repository.",
    cxxopts::value<std::string>()
  )(
    "nano-version",
    "Package version to resolve inside the styio-nano static repository.",
    cxxopts::value<std::string>()
  );
#endif

#if STYIO_NANO_ENABLE_DEBUG_CLI
  options.add_options()(
    "debug", "Debug Mode", cxxopts::value<bool>()->default_value("false")
  );
#endif
#if STYIO_NANO_ENABLE_MACHINE_INFO
  options.add_options()(
    "machine-info", "Emit machine-readable compiler metadata. Supported format: json",
    cxxopts::value<std::string>()
  );
#endif
#if STYIO_NANO_ENABLE_LEGACY_PARSER
  options.add_options()(
    "parser-engine", "Internal parser selector (legacy|nightly). Accepts deprecated alias: new.",
    cxxopts::value<std::string>()->default_value("nightly")
  );
#endif
#if STYIO_NANO_ENABLE_PARSER_SHADOW_COMPARE
  options.add_options()(
    "parser-shadow-compare",
    "When enabled, parse once with the selected parser and once with the alternate parser, then compare AST repr.",
    cxxopts::value<bool>()->default_value("false")
  )(
    "parser-shadow-artifact-dir",
    "Directory to write shadow-compare artifacts (JSONL record, and payload files on mismatch/error).",
    cxxopts::value<std::string>()->default_value("")
  );
#endif

  options.parse_positional({"file"});

  cxxopts::ParseResult cmlopts;
  try {
    cmlopts = options.parse(argc, argv);
  } catch (const std::exception& ex) {
    std::cerr << "[CliError] " << ex.what() << std::endl;
    return static_cast<int>(StyioExitCode::CliError);
  }

  if (cmlopts.count("help")) {
    std::cout << options.help() << std::endl;
    return static_cast<int>(StyioExitCode::Success);
  }

  if (const std::string disabled = styio_nano_disabled_option_latest(argc, argv); !disabled.empty()) {
    std::cerr << "[CliError] " << disabled << std::endl;
    return static_cast<int>(StyioExitCode::CliError);
  }

#if !STYIO_NANO_BUILD
  const bool wants_nano_create = cmlopts["nano-create"].as<bool>();
  const bool wants_nano_publish = cmlopts["nano-publish"].as<bool>();
  const bool mentions_nano_package_args = styio_cli_mentions_any_nano_packaging_args_latest(cmlopts);
  if (wants_nano_create && wants_nano_publish) {
    std::cerr << "[CliError] --nano-create and --nano-publish are mutually exclusive" << std::endl;
    return static_cast<int>(StyioExitCode::CliError);
  }
  if (mentions_nano_package_args && !wants_nano_create && !wants_nano_publish) {
    std::cerr << "[CliError] styio-nano packaging arguments require --nano-create or --nano-publish" << std::endl;
    return static_cast<int>(StyioExitCode::CliError);
  }
  if (wants_nano_create
      && (cmlopts.count("nano-publish-config")
          || cmlopts.count("nano-package-dir")
          || cmlopts.count("nano-channel"))) {
    std::cerr << "[CliError] --nano-create does not accept publish-only options" << std::endl;
    return static_cast<int>(StyioExitCode::CliError);
  }
  if (wants_nano_publish
      && (cmlopts.count("nano-package-config")
          || cmlopts.count("nano-mode")
          || cmlopts.count("nano-output")
          || cmlopts.count("nano-name")
          || cmlopts.count("nano-profile")
          || cmlopts.count("nano-binary")
          || cmlopts.count("nano-source-root")
          || cmlopts.count("nano-manifest"))) {
    std::cerr << "[CliError] --nano-publish does not accept create-only options" << std::endl;
    return static_cast<int>(StyioExitCode::CliError);
  }
  if (wants_nano_create) {
    StyioNanoCreateSelectionLatest nano_selection;
    std::string nano_error;
    if (!styio_resolve_nano_create_selection_latest(cmlopts, argv[0], nano_selection, nano_error)) {
      std::cerr << "[CliError] " << nano_error << std::endl;
      return static_cast<int>(StyioExitCode::CliError);
    }

    const bool ok =
      nano_selection.mode == "local-subset"
        ? styio_materialize_local_nano_package_latest(nano_selection, argv[0], nano_error)
        : styio_materialize_cloud_nano_package_latest(nano_selection, nano_error);
    if (!ok) {
      std::cerr << "[CliError] " << nano_error << std::endl;
      return static_cast<int>(StyioExitCode::CliError);
    }

    std::cout << "[styio-nano] materialized " << nano_selection.mode
              << " package at " << nano_selection.output_dir << std::endl;
    return static_cast<int>(StyioExitCode::Success);
  }
  if (wants_nano_publish) {
    StyioNanoPublishSelectionLatest nano_publish_selection;
    std::string nano_error;
    if (!styio_resolve_nano_publish_selection_latest(cmlopts, nano_publish_selection, nano_error)) {
      std::cerr << "[CliError] " << nano_error << std::endl;
      return static_cast<int>(StyioExitCode::CliError);
    }
    if (!styio_publish_nano_package_latest(nano_publish_selection, nano_error)) {
      std::cerr << "[CliError] " << nano_error << std::endl;
      return static_cast<int>(StyioExitCode::CliError);
    }

    std::cout << "[styio-nano] published " << nano_publish_selection.registry_package
              << "@" << nano_publish_selection.registry_version
              << " to " << nano_publish_selection.registry_root << std::endl;
    return static_cast<int>(StyioExitCode::Success);
  }
#endif

#if !STYIO_NANO_BUILD
  std::optional<StyioCompilePlanRequestLatest> compile_plan_request;
  if (cmlopts.count("compile-plan")) {
    const std::filesystem::path compile_plan_path(cmlopts["compile-plan"].as<std::string>());
    if (cmlopts.count("file")) {
      return styio_emit_compile_plan_cli_error_latest(
        compile_plan_path,
        "--compile-plan and --file are mutually exclusive",
        "compile_plan_cli_conflict");
    }
    StyioCompilePlanRequestLatest parsed_request;
    std::string compile_plan_error;
    if (!styio_parse_compile_plan_latest(
          compile_plan_path,
          parsed_request,
          compile_plan_error)) {
      return styio_emit_compile_plan_cli_error_latest(compile_plan_path, compile_plan_error);
    }
    compile_plan_request = std::move(parsed_request);
  }
#else
  std::optional<StyioCompilePlanRequestLatest> compile_plan_request;
#endif

  styio_clear_diagnostic_sink_latest();

  std::string fpath; /* File Path */
  if (compile_plan_request.has_value()) {
    fpath = compile_plan_request->entry_file.string();
  }
  else if (cmlopts.count("file")) {
    fpath = cmlopts["file"].as<std::string>();
  }

  StyioDictImplSelectionLatest dict_impl_selection;
  std::string dict_impl_error;
  if (!styio_resolve_dict_impl_selection_latest(cmlopts, fpath, dict_impl_selection, dict_impl_error)) {
    std::cerr << "[CliError] " << dict_impl_error << std::endl;
    return static_cast<int>(StyioExitCode::CliError);
  }
  if (!styio_dict_runtime_set_impl_by_name(dict_impl_selection.impl_name.c_str())) {
    std::cerr << "[CliError] failed to apply dict implementation selector" << std::endl;
    return static_cast<int>(StyioExitCode::CliError);
  }

#if STYIO_NANO_ENABLE_MACHINE_INFO
  if (cmlopts.count("machine-info")) {
    const std::string machine_info_format = cmlopts["machine-info"].as<std::string>();
    if (machine_info_format != "json") {
      std::cerr << "[CliError] unsupported --machine-info format: " << machine_info_format << std::endl;
      return static_cast<int>(StyioExitCode::CliError);
    }
    styio_emit_machine_info_json(dict_impl_selection);
    return static_cast<int>(StyioExitCode::Success);
  }
#endif

#if STYIO_NANO_ENABLE_AST_DUMP_CLI
  bool show_styio_ast = cmlopts["styio-ast"].as<bool>();
#else
  bool show_styio_ast = false;
#endif
#if STYIO_NANO_ENABLE_STYIO_IR_DUMP_CLI
  bool show_styio_ir = cmlopts["styio-ir"].as<bool>();
#else
  bool show_styio_ir = false;
#endif
#if STYIO_NANO_ENABLE_LLVM_IR_DUMP_CLI
  bool show_llvm_ir = cmlopts["llvm-ir"].as<bool>();
#else
  bool show_llvm_ir = false;
#endif

#if STYIO_NANO_ENABLE_DEBUG_CLI
  bool is_debug_mode = cmlopts["debug"].as<bool>();
#else
  bool is_debug_mode = false;
#endif
  std::string error_format = cmlopts["error-format"].as<std::string>();
  StyioParserEngine parser_engine = StyioParserEngine::Nightly;
  if (!(error_format == "text" || error_format == "jsonl")) {
    std::cerr << "[CliError] unsupported --error-format: " << error_format << std::endl;
    return static_cast<int>(StyioExitCode::CliError);
  }

#if STYIO_NANO_ENABLE_LEGACY_PARSER
  std::string parser_engine_raw = cmlopts["parser-engine"].as<std::string>();
  if (!styio_parse_parser_engine_latest(parser_engine_raw, parser_engine)) {
    std::cerr << "[CliError] unsupported --parser-engine: " << parser_engine_raw << std::endl;
    return static_cast<int>(StyioExitCode::CliError);
  }
#endif

#if STYIO_NANO_ENABLE_PARSER_SHADOW_COMPARE
  bool parser_shadow_compare = cmlopts["parser-shadow-compare"].as<bool>();
  std::string parser_shadow_artifact_dir = cmlopts["parser-shadow-artifact-dir"].as<std::string>();
  if (!parser_shadow_artifact_dir.empty() && !parser_shadow_compare) {
    std::cerr << "[CliError] --parser-shadow-artifact-dir requires --parser-shadow-compare" << std::endl;
    return static_cast<int>(StyioExitCode::CliError);
  }
#else
  bool parser_shadow_compare = false;
  std::string parser_shadow_artifact_dir;
#endif

  if (compile_plan_request.has_value()) {
    show_styio_ast = show_styio_ast || compile_plan_request->emit_ast;
    show_styio_ir = show_styio_ir || compile_plan_request->emit_styio_ir;
    show_llvm_ir = show_llvm_ir || compile_plan_request->emit_llvm_ir;
    error_format = compile_plan_request->error_format;
  }

  if (fpath.empty()) {
    return static_cast<int>(StyioExitCode::Success);
  }

  if (compile_plan_request.has_value()) {
    std::error_code ec;
    std::filesystem::create_directories(compile_plan_request->diag_dir, ec);
    if (ec) {
      styio_emit_diagnostic(
        error_format,
        StyioErrorCategory::RuntimeError,
        fpath,
        "cannot create compile-plan diag_dir: " + compile_plan_request->diag_dir.string());
      return styio_exit_code(StyioErrorCategory::RuntimeError);
    }
    styio_set_diagnostic_sink_latest(compile_plan_request->diag_dir);
    std::string sink_error;
    if (!styio_ensure_text_file_exists_latest(
          compile_plan_request->diag_dir / "diagnostics.jsonl",
          sink_error)) {
      styio_emit_diagnostic(
        error_format,
        StyioErrorCategory::RuntimeError,
        fpath,
        sink_error);
      return styio_exit_code(StyioErrorCategory::RuntimeError);
    }

    std::filesystem::create_directories(compile_plan_request->build_root, ec);
    if (ec) {
      styio_emit_diagnostic(
        error_format,
        StyioErrorCategory::RuntimeError,
        fpath,
        "cannot create compile-plan build_root: " + compile_plan_request->build_root.string());
      return styio_exit_code(StyioErrorCategory::RuntimeError);
    }
    std::filesystem::create_directories(compile_plan_request->artifact_dir, ec);
    if (ec) {
      styio_emit_diagnostic(
        error_format,
        StyioErrorCategory::RuntimeError,
        fpath,
        "cannot create compile-plan artifact_dir: " + compile_plan_request->artifact_dir.string());
      return styio_exit_code(StyioErrorCategory::RuntimeError);
    }
    styio_set_runtime_event_sink_latest(compile_plan_request->build_root);
    if (!styio_ensure_text_file_exists_latest(
          compile_plan_request->build_root / "runtime-events.jsonl",
          sink_error)) {
      styio_emit_diagnostic(
        error_format,
        StyioErrorCategory::RuntimeError,
        fpath,
        sink_error);
      return styio_exit_code(StyioErrorCategory::RuntimeError);
    }
    {
      std::ostringstream payload;
      payload << "{\"intent\":\"" << styio_json_escape(compile_plan_request->intent)
              << "\",\"file\":\"" << styio_json_escape(fpath) << "\"}";
      styio_emit_runtime_event_latest(
        "compile.started",
        "styio.compile-plan",
        payload.str());
      styio_emit_runtime_event_latest(
        "unit.entered",
        "styio.compile-plan",
        styio_render_compile_plan_unit_payload_latest(
          *compile_plan_request,
          compile_plan_request->intent));
    }
  }

  bool compile_plan_runtime_success = false;
  bool compile_plan_runtime_executed = false;
  bool compile_plan_test_runtime_started = false;
  struct StyioCompilePlanRuntimeEventScopeLatest
  {
    bool active = false;
    const StyioCompilePlanRequestLatest* request = nullptr;
    const std::string* file_path = nullptr;
    const std::string* intent = nullptr;
    bool* executed = nullptr;
    bool* success = nullptr;
    const CompilationPhase* final_phase = nullptr;

    ~StyioCompilePlanRuntimeEventScopeLatest() {
      if (!active || !g_styio_runtime_event_sink_latest.enabled) {
        return;
      }

      if (request != nullptr && intent != nullptr) {
        styio_emit_runtime_event_latest(
          "unit.exited",
          "styio.compile-plan",
          styio_render_compile_plan_unit_payload_latest(
            *request,
            *intent,
            success,
            executed,
            final_phase));
      }

      std::ostringstream payload;
      payload << "{\"intent\":\""
              << styio_json_escape(intent != nullptr ? *intent : "")
              << "\",\"file\":\""
              << styio_json_escape(file_path != nullptr ? *file_path : "")
              << "\",\"executed\":"
              << ((executed != nullptr && *executed) ? "true" : "false");
      if (final_phase != nullptr) {
        payload << ",\"final_phase\":\""
                << styio_json_escape(styio_runtime_phase_name_latest(*final_phase))
                << "\"";
      }
      payload << "}";
      styio_emit_runtime_event_latest(
        success != nullptr && *success ? "compile.finished" : "compile.failed",
        "styio.compile-plan",
        payload.str());
      styio_clear_runtime_event_sink_latest();
    }
  } compile_plan_runtime_scope;
  CompilationPhase compile_plan_final_phase = CompilationPhase::Empty;
  compile_plan_runtime_scope.active = compile_plan_request.has_value();
  compile_plan_runtime_scope.request =
    compile_plan_request.has_value() ? &*compile_plan_request : nullptr;
  compile_plan_runtime_scope.file_path = &fpath;
  compile_plan_runtime_scope.intent =
    compile_plan_request.has_value() ? &compile_plan_request->intent : nullptr;
  compile_plan_runtime_scope.executed = &compile_plan_runtime_executed;
  compile_plan_runtime_scope.success = &compile_plan_runtime_success;
  compile_plan_runtime_scope.final_phase = &compile_plan_final_phase;

  auto styio_code = read_styio_file(fpath);
  if (!styio_code.ok) {
    if (compile_plan_request.has_value()) {
      compile_plan_final_phase = CompilationPhase::Failed;
      styio_emit_runtime_phase_transition_latest(
        CompilationPhase::Empty,
        compile_plan_final_phase,
        "read_source",
        fpath,
        &compile_plan_request->intent);
    }
    styio_emit_diagnostic(
      error_format,
      StyioErrorCategory::RuntimeError,
      fpath,
      styio_code.error_message);
    return styio_exit_code(StyioErrorCategory::RuntimeError);
  }

  if (is_debug_mode) {
    show_code_with_linenum(styio_code);
  }

  // C.1 shell: handle table exists before runtime migration.
  StyioHandleTable handle_table;
  (void)handle_table;

  CompilationSession session;
  compile_plan_final_phase = session.phase();
  const auto compile_started_at = std::chrono::steady_clock::now();
  std::vector<std::filesystem::path> compile_plan_artifacts;
  const std::string compile_plan_artifact_stem =
    compile_plan_request.has_value()
      ? styio_compile_plan_artifact_stem_latest(*compile_plan_request)
      : std::string();
  const auto emit_compile_plan_session_transition =
    [&](CompilationPhase previous_phase, const char* operation) {
      compile_plan_final_phase = session.phase();
      if (!compile_plan_request.has_value()) {
        return;
      }
      styio_emit_runtime_phase_transition_latest(
        previous_phase,
        compile_plan_final_phase,
        operation,
        fpath,
        &compile_plan_request->intent);
    };
  const auto mark_failed_with_runtime_transition = [&](const char* operation) {
    const CompilationPhase previous_phase = session.phase();
    session.mark_failed();
    emit_compile_plan_session_transition(previous_phase, operation);
  };

  try {
    const CompilationPhase previous_phase = session.phase();
    session.adopt_tokens(StyioTokenizer::tokenize(styio_code.code_text));
    emit_compile_plan_session_transition(previous_phase, "adopt_tokens");
  } catch (const StyioLexError& ex) {
    styio_emit_diagnostic(error_format, StyioErrorCategory::LexError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::LexError);
  } catch (const std::exception& ex) {
    styio_emit_diagnostic(error_format, StyioErrorCategory::LexError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::LexError);
  }

  session.attach_context(StyioContext::Create(
    fpath,
    styio_code.code_text,
    styio_code.line_seps,
    session.tokens(),
    is_debug_mode /* is debug mode */
  ));

  if (is_debug_mode) {
    show_tokens(session.tokens());
  }

  StyioRepr styio_repr = StyioRepr();
  std::string primary_route_detail;
  StyioParserRouteStats primary_route_stats;
  StyioParserRouteStats* primary_route_stats_ptr =
    parser_engine == StyioParserEngine::Nightly ? &primary_route_stats : nullptr;

  try {
    const CompilationPhase previous_phase = session.phase();
    session.attach_ast(parse_main_block_with_engine_latest(*session.context(), parser_engine, primary_route_stats_ptr));
    emit_compile_plan_session_transition(previous_phase, "attach_ast");
    if (primary_route_stats_ptr != nullptr) {
      primary_route_detail =
        "nightly_subset_statements=" + std::to_string(primary_route_stats.nightly_subset_statements)
        + ",legacy_fallback_statements="
        + std::to_string(primary_route_stats.legacy_fallback_statements)
        + ",nightly_internal_legacy_bridges="
        + std::to_string(primary_route_stats.nightly_internal_legacy_bridges)
        + ",nightly_declined_statements="
        + std::to_string(primary_route_stats.nightly_declined_statements);
    }
  } catch (const StyioBaseException& ex) {
    styio_emit_diagnostic(error_format, StyioErrorCategory::ParseError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::ParseError);
  } catch (const std::exception& ex) {
    styio_emit_diagnostic(error_format, StyioErrorCategory::ParseError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::ParseError);
  }

  if (parser_shadow_compare) {
    const StyioParserEngine shadow_engine =
      parser_engine == StyioParserEngine::Legacy ? StyioParserEngine::Nightly : StyioParserEngine::Legacy;
    StyioRepr repr;
    const std::string primary_repr = session.ast()->toString(&repr);

    std::string shadow_repr;
    std::string shadow_err;
    std::string shadow_route_detail;
    if (!styio_parse_engine_to_repr_latest(
          fpath,
          styio_code.code_text,
          styio_code.line_seps,
          is_debug_mode,
          shadow_engine,
          shadow_repr,
          shadow_err,
          &shadow_route_detail)) {
      std::ostringstream detail;
      detail << "shadow parser failed under --parser-shadow-compare";
      if (!primary_route_detail.empty()) {
        detail << "; primary_route=" << primary_route_detail;
      }
      if (!shadow_route_detail.empty()) {
        detail << "; shadow_route=" << shadow_route_detail;
      }
      styio_emit_diagnostic(
        error_format,
        StyioErrorCategory::ParseError,
        fpath,
        std::string("shadow parser failed under --parser-shadow-compare: ") + shadow_err);
      styio_write_shadow_artifact_latest(
        parser_shadow_artifact_dir,
        fpath,
        styio_code.code_text,
        parser_engine,
        shadow_engine,
        "shadow_error",
        primary_repr,
        shadow_repr,
        shadow_err,
        detail.str());
      return styio_exit_code(StyioErrorCategory::ParseError);
    }

    if (primary_repr != shadow_repr) {
      std::ostringstream oss;
      oss << "shadow parser mismatch: primary=" << styio_parser_engine_name_latest(parser_engine)
          << ", shadow=" << styio_parser_engine_name_latest(shadow_engine);
      if (!primary_route_detail.empty()) {
        oss << "; primary_route=" << primary_route_detail;
      }
      if (!shadow_route_detail.empty()) {
        oss << "; shadow_route=" << shadow_route_detail;
      }
      styio_emit_diagnostic(error_format, StyioErrorCategory::ParseError, fpath, oss.str());
      styio_write_shadow_artifact_latest(
        parser_shadow_artifact_dir,
        fpath,
        styio_code.code_text,
        parser_engine,
        shadow_engine,
        "mismatch",
        primary_repr,
        shadow_repr,
        "",
        oss.str());
      return styio_exit_code(StyioErrorCategory::ParseError);
    }

    std::ostringstream detail;
    detail << "shadow parser match";
    if (!primary_route_detail.empty()) {
      detail << "; primary_route=" << primary_route_detail;
    }
    if (!shadow_route_detail.empty()) {
      detail << "; shadow_route=" << shadow_route_detail;
    }
    styio_write_shadow_artifact_latest(
      parser_shadow_artifact_dir,
      fpath,
      styio_code.code_text,
      parser_engine,
      shadow_engine,
      "match",
      primary_repr,
      shadow_repr,
      "",
      detail.str());
  }

  if (show_styio_ast) {
    if (styio_stdout_plain()) {
      std::cout << "AST -Original\n";
    }
    else {
      std::cout << "\033[1;32mAST\033[0m \033[31m-Original\033[0m\n";
    }
    std::cout << session.ast()->toString(&styio_repr) << "\n\n";
  }
  if (compile_plan_request.has_value() && compile_plan_request->emit_ast) {
    std::string artifact_error;
    const std::string original_ast = session.ast()->toString(&styio_repr);
    const std::filesystem::path ast_path =
      compile_plan_request->artifact_dir / (compile_plan_artifact_stem + ".original.ast.txt");
    if (!styio_write_compile_plan_artifact_latest(ast_path, original_ast + "\n", artifact_error)) {
      styio_emit_diagnostic(error_format, StyioErrorCategory::RuntimeError, fpath, artifact_error);
      return styio_exit_code(StyioErrorCategory::RuntimeError);
    }
    compile_plan_artifacts.push_back(ast_path);
  }

  StyioAnalyzer analyzer = StyioAnalyzer();
  try {
    analyzer.typeInfer(session.ast());
    const CompilationPhase previous_phase = session.phase();
    session.mark_type_checked();
    emit_compile_plan_session_transition(previous_phase, "mark_type_checked");
  } catch (const StyioBaseException& ex) {
    mark_failed_with_runtime_transition("mark_failed");
    styio_emit_diagnostic(error_format, StyioErrorCategory::TypeError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::TypeError);
  } catch (const std::exception& ex) {
    mark_failed_with_runtime_transition("mark_failed");
    styio_emit_diagnostic(error_format, StyioErrorCategory::TypeError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::TypeError);
  }

  if (show_styio_ast) {
    if (styio_stdout_plain()) {
      std::cout << "AST -Type-Checking\n";
    }
    else {
      std::cout << "\033[1;32mAST\033[0m \033[1;33m-Type-Checking\033[0m\n";
    }
    std::cout << session.ast()->toString(&styio_repr) << "\n\n";
  }
  if (compile_plan_request.has_value() && compile_plan_request->emit_ast) {
    std::string artifact_error;
    const std::string typed_ast = session.ast()->toString(&styio_repr);
    const std::filesystem::path ast_path =
      compile_plan_request->artifact_dir / (compile_plan_artifact_stem + ".typed.ast.txt");
    if (!styio_write_compile_plan_artifact_latest(ast_path, typed_ast + "\n", artifact_error)) {
      styio_emit_diagnostic(error_format, StyioErrorCategory::RuntimeError, fpath, artifact_error);
      return styio_exit_code(StyioErrorCategory::RuntimeError);
    }
    compile_plan_artifacts.push_back(ast_path);
  }

  try {
    const CompilationPhase previous_phase = session.phase();
    session.attach_ir(analyzer.toStyioIR(session.ast()));
    emit_compile_plan_session_transition(previous_phase, "attach_ir");
  } catch (const StyioBaseException& ex) {
    mark_failed_with_runtime_transition("mark_failed");
    styio_emit_diagnostic(error_format, StyioErrorCategory::TypeError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::TypeError);
  } catch (const std::exception& ex) {
    mark_failed_with_runtime_transition("mark_failed");
    styio_emit_diagnostic(error_format, StyioErrorCategory::TypeError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::TypeError);
  }

  if (show_styio_ir) {
    if (styio_stdout_plain()) {
      std::cout << "Styio IR\n";
    }
    else {
      std::cout << "\033[1;32mStyio IR\033[0m \033[1;33m\033[0m\n";
    }
    std::cout << session.ir()->toString(&styio_repr) << "\n\n";
  }
  if (compile_plan_request.has_value() && compile_plan_request->emit_styio_ir) {
    std::string artifact_error;
    const std::string styio_ir = session.ir()->toString(&styio_repr);
    const std::filesystem::path styio_ir_path =
      compile_plan_request->artifact_dir / (compile_plan_artifact_stem + ".styio.ir.txt");
    if (!styio_write_compile_plan_artifact_latest(styio_ir_path, styio_ir + "\n", artifact_error)) {
      styio_emit_diagnostic(error_format, StyioErrorCategory::RuntimeError, fpath, artifact_error);
      return styio_exit_code(StyioErrorCategory::RuntimeError);
    }
    compile_plan_artifacts.push_back(styio_ir_path);
  }

  try {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    auto jit_or_err = StyioJIT_ORC::Create();
    if (!jit_or_err) {
      std::string emsg;
      llvm::handleAllErrors(
        jit_or_err.takeError(),
        [&](const llvm::ErrorInfoBase& e) { emsg = e.message(); });
      styio_emit_diagnostic(error_format, StyioErrorCategory::RuntimeError, fpath, emsg);
      return styio_exit_code(StyioErrorCategory::RuntimeError);
    }

    StyioToLLVM generator = StyioToLLVM(std::move(*jit_or_err));
    session.ir()->toLLVMIR(&generator);
    const CompilationPhase previous_phase = session.phase();
    session.mark_codegen_ready();
    emit_compile_plan_session_transition(previous_phase, "mark_codegen_ready");
    const std::string llvm_ir_text = generator.dump_llvm_ir();

    if (compile_plan_request.has_value()) {
      std::string artifact_error;
      const std::filesystem::path llvm_ir_path =
        compile_plan_request->artifact_dir / (compile_plan_artifact_stem + ".llvm.ir");
      if (!styio_write_compile_plan_artifact_latest(llvm_ir_path, llvm_ir_text + "\n", artifact_error)) {
        styio_emit_diagnostic(error_format, StyioErrorCategory::RuntimeError, fpath, artifact_error);
        return styio_exit_code(StyioErrorCategory::RuntimeError);
      }
      compile_plan_artifacts.push_back(llvm_ir_path);
    }

    if (show_llvm_ir) {
      generator.print_llvm_ir();
    }
    const bool should_execute =
      !compile_plan_request.has_value()
      || (compile_plan_request->intent != "build" && compile_plan_request->intent != "check");
    if (should_execute) {
      if (compile_plan_request.has_value()) {
        if (compile_plan_request->intent == "test") {
          styio_emit_runtime_event_latest(
            "unit.test.started",
            "styio.tests",
            styio_render_compile_plan_unit_payload_latest(
              *compile_plan_request,
              compile_plan_request->intent));
          compile_plan_test_runtime_started = true;
        }
        std::ostringstream payload;
        payload << "{\"intent\":\"" << styio_json_escape(compile_plan_request->intent)
                << "\",\"file\":\"" << styio_json_escape(fpath) << "\"}";
        styio_emit_runtime_event_latest(
          "run.started",
          "styio.runtime",
          payload.str());
        std::ostringstream thread_payload;
        thread_payload << "{\"thread_id\":\"main\""
                       << ",\"role\":\"entry\""
                       << ",\"intent\":\"" << styio_json_escape(compile_plan_request->intent)
                       << "\",\"file\":\"" << styio_json_escape(fpath)
                       << "\"}";
        styio_emit_runtime_event_latest(
          "thread.spawned",
          "styio.runtime",
          thread_payload.str());
      }
      styio_runtime_clear_error();
      styio_runtime_set_log_sink(styio_runtime_log_sink_latest);
      generator.execute();
      styio_runtime_set_log_sink(nullptr);
      compile_plan_runtime_executed = true;
      if (compile_plan_request.has_value()) {
        const bool execution_success = !styio_runtime_has_error();
        std::ostringstream thread_payload;
        thread_payload << "{\"thread_id\":\"main\""
                       << ",\"role\":\"entry\""
                       << ",\"success\":"
                       << (execution_success ? "true" : "false")
                       << "}";
        styio_emit_runtime_event_latest(
          "thread.exited",
          "styio.runtime",
          thread_payload.str());
        if (compile_plan_request->intent == "test") {
          styio_emit_runtime_event_latest(
            "unit.test.finished",
            "styio.tests",
            styio_render_compile_plan_unit_payload_latest(
              *compile_plan_request,
              compile_plan_request->intent,
              &execution_success,
              &compile_plan_runtime_executed));
          compile_plan_test_runtime_started = false;
        }
        std::ostringstream payload;
        payload << "{\"intent\":\"" << styio_json_escape(compile_plan_request->intent)
                << "\",\"file\":\"" << styio_json_escape(fpath) << "\""
                << ",\"success\":"
                << (execution_success ? "true" : "false")
                << "}";
        styio_emit_runtime_event_latest(
          "run.finished",
          "styio.runtime",
          payload.str());
      }
      if (styio_runtime_has_error()) {
        const char* runtime_err = styio_runtime_last_error();
        const char* runtime_subcode = styio_runtime_last_error_subcode();
        styio_emit_diagnostic(
          error_format,
          StyioErrorCategory::RuntimeError,
          fpath,
          runtime_err ? runtime_err : "runtime helper reported error",
          runtime_subcode ? runtime_subcode : "");
        mark_failed_with_runtime_transition("mark_failed");
        return styio_exit_code(StyioErrorCategory::RuntimeError);
      }
      const CompilationPhase previous_phase = session.phase();
      session.mark_executed();
      emit_compile_plan_session_transition(previous_phase, "mark_executed");
    }
  } catch (const StyioTypeError& ex) {
    styio_runtime_set_log_sink(nullptr);
    if (compile_plan_test_runtime_started && compile_plan_request.has_value()) {
      const bool test_success = false;
        styio_emit_runtime_event_latest(
          "unit.test.finished",
          "styio.tests",
          styio_render_compile_plan_unit_payload_latest(
            *compile_plan_request,
            compile_plan_request->intent,
            &test_success,
            &compile_plan_runtime_executed));
      compile_plan_test_runtime_started = false;
    }
    mark_failed_with_runtime_transition("mark_failed");
    styio_emit_diagnostic(error_format, StyioErrorCategory::TypeError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::TypeError);
  } catch (const StyioBaseException& ex) {
    styio_runtime_set_log_sink(nullptr);
    if (compile_plan_test_runtime_started && compile_plan_request.has_value()) {
      const bool test_success = false;
        styio_emit_runtime_event_latest(
          "unit.test.finished",
          "styio.tests",
          styio_render_compile_plan_unit_payload_latest(
            *compile_plan_request,
            compile_plan_request->intent,
            &test_success,
            &compile_plan_runtime_executed));
      compile_plan_test_runtime_started = false;
    }
    mark_failed_with_runtime_transition("mark_failed");
    styio_emit_diagnostic(error_format, StyioErrorCategory::RuntimeError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::RuntimeError);
  } catch (const std::exception& ex) {
    styio_runtime_set_log_sink(nullptr);
    if (compile_plan_test_runtime_started && compile_plan_request.has_value()) {
      const bool test_success = false;
        styio_emit_runtime_event_latest(
          "unit.test.finished",
          "styio.tests",
          styio_render_compile_plan_unit_payload_latest(
            *compile_plan_request,
            compile_plan_request->intent,
            &test_success,
            &compile_plan_runtime_executed));
      compile_plan_test_runtime_started = false;
    }
    mark_failed_with_runtime_transition("mark_failed");
    styio_emit_diagnostic(error_format, StyioErrorCategory::RuntimeError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::RuntimeError);
  }

  if (compile_plan_request.has_value()) {
    std::string artifact_error;
    const long long wall_time_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - compile_started_at)
        .count();
    if (!styio_write_compile_plan_receipt_latest(
          *compile_plan_request,
          compile_plan_artifacts,
          dict_impl_selection.impl_name,
          g_styio_runtime_event_sink_latest.session_id,
          session.phase() == CompilationPhase::Executed,
          wall_time_ms,
          artifact_error)) {
      styio_emit_diagnostic(error_format, StyioErrorCategory::RuntimeError, fpath, artifact_error);
      return styio_exit_code(StyioErrorCategory::RuntimeError);
    }
    compile_plan_runtime_success = true;
  }

  return static_cast<int>(StyioExitCode::Success);
}
