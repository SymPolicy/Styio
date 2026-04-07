/*                                   */
/*    __| __ __| \ \  / _ _|   _ \   */
/*  \__ \    |    \  /    |   (   |  */
/*  ____/   _|     _|   ___| \___/   */
/*                                   */

// [C++ STL]
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <regex>
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

// [Styio LLVM ORC JIT]
#include "StyioJIT/StyioJIT_ORC.hpp"

// [Others]
#include "include/cxxopts.hpp" /* https://github.com/jarro2783/cxxopts */

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

static void
styio_emit_diagnostic(
  const std::string& format,
  StyioErrorCategory category,
  const std::string& file_path,
  const std::string& message,
  const std::string& subcode = ""
) {
  if (styio_error_jsonl_enabled(format)) {
    std::cerr
      << "{\"category\":\"" << styio_category_name(category)
      << "\",\"code\":\"" << styio_category_code(category)
      << (subcode.empty() ? "" : "\",\"subcode\":\"")
      << (subcode.empty() ? "" : styio_json_escape(subcode))
      << "\",\"file\":\"" << styio_json_escape(file_path)
      << "\",\"message\":\"" << styio_json_escape(message)
      << "\"}\n";
    return;
  }

  std::cerr << "[" << styio_category_name(category) << "] " << message << std::endl;
}

static void
styio_free_tokens(std::vector<StyioToken*>& tokens) {
  for (auto* tok : tokens) {
    delete tok;
  }
  tokens.clear();
}

static bool
styio_parse_engine_to_repr(
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
      engine == StyioParserEngine::New ? &route_stats : nullptr;
    ast = parse_main_block_with_engine(*ctx, engine, route_stats_ptr);
    StyioRepr repr;
    out_repr = ast->toString(&repr);
    if (out_detail != nullptr) {
      if (route_stats_ptr != nullptr) {
        *out_detail = "new_subset_statements=" + std::to_string(route_stats.new_subset_statements)
                      + ",legacy_fallback_statements="
                      + std::to_string(route_stats.legacy_fallback_statements);
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
styio_write_shadow_artifact(
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
         << "\",\"primary_engine\":\"" << styio_parser_engine_name(primary_engine)
         << "\",\"shadow_engine\":\"" << styio_parser_engine_name(shadow_engine)
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
  )(
    "styio-ast", "Print AST before and after type inference (plain headers when stdout is not a tty).",
    cxxopts::value<bool>()->default_value("false")
  )(
    "styio-ir", "Print lowered Styio IR before LLVM codegen.",
    cxxopts::value<bool>()->default_value("false")
  )(
    "llvm-ir",
    "Print LLVM IR for the module (before running main).",
    cxxopts::value<bool>()->default_value("false")
  );

  options.add_options()(
    "debug", "Debug Mode", cxxopts::value<bool>()->default_value("false")
  )(
    "error-format", "Diagnostic output format: text|jsonl",
    cxxopts::value<std::string>()->default_value("text")
  )(
    "parser-engine", "Internal parser selector (legacy|new).",
    cxxopts::value<std::string>()->default_value("new")
  )(
    "parser-shadow-compare",
    "When enabled, parse once with the selected parser and once with the alternate parser, then compare AST repr.",
    cxxopts::value<bool>()->default_value("false")
  )(
    "parser-shadow-artifact-dir",
    "Directory to write shadow-compare artifacts (JSONL record, and payload files on mismatch/error).",
    cxxopts::value<std::string>()->default_value("")
  );

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

  bool show_styio_ast = cmlopts["styio-ast"].as<bool>();
  bool show_styio_ir = cmlopts["styio-ir"].as<bool>();
  bool show_llvm_ir = cmlopts["llvm-ir"].as<bool>();

  bool is_debug_mode = cmlopts["debug"].as<bool>();
  std::string error_format = cmlopts["error-format"].as<std::string>();
  std::string parser_engine_raw = cmlopts["parser-engine"].as<std::string>();
  bool parser_shadow_compare = cmlopts["parser-shadow-compare"].as<bool>();
  std::string parser_shadow_artifact_dir = cmlopts["parser-shadow-artifact-dir"].as<std::string>();
  StyioParserEngine parser_engine = StyioParserEngine::New;

  if (!(error_format == "text" || error_format == "jsonl")) {
    std::cerr << "[CliError] unsupported --error-format: " << error_format << std::endl;
    return static_cast<int>(StyioExitCode::CliError);
  }

  if (!styio_parse_parser_engine(parser_engine_raw, parser_engine)) {
    std::cerr << "[CliError] unsupported --parser-engine: " << parser_engine_raw << std::endl;
    return static_cast<int>(StyioExitCode::CliError);
  }

  if (!parser_shadow_artifact_dir.empty() && !parser_shadow_compare) {
    std::cerr << "[CliError] --parser-shadow-artifact-dir requires --parser-shadow-compare" << std::endl;
    return static_cast<int>(StyioExitCode::CliError);
  }

  std::string fpath; /* File Path */
  if (!cmlopts.count("file")) {
    return static_cast<int>(StyioExitCode::Success);
  }
  fpath = cmlopts["file"].as<std::string>();

  auto styio_code = read_styio_file(fpath);
  if (!styio_code.ok) {
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

  try {
    session.adopt_tokens(StyioTokenizer::tokenize(styio_code.code_text));
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
    parser_engine == StyioParserEngine::New ? &primary_route_stats : nullptr;

  try {
    session.attach_ast(parse_main_block_with_engine(*session.context(), parser_engine, primary_route_stats_ptr));
    if (primary_route_stats_ptr != nullptr) {
      primary_route_detail =
        "new_subset_statements=" + std::to_string(primary_route_stats.new_subset_statements)
        + ",legacy_fallback_statements="
        + std::to_string(primary_route_stats.legacy_fallback_statements);
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
      parser_engine == StyioParserEngine::Legacy ? StyioParserEngine::New : StyioParserEngine::Legacy;
    StyioRepr repr;
    const std::string primary_repr = session.ast()->toString(&repr);

    std::string shadow_repr;
    std::string shadow_err;
    std::string shadow_route_detail;
    if (!styio_parse_engine_to_repr(
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
      styio_write_shadow_artifact(
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
      oss << "shadow parser mismatch: primary=" << styio_parser_engine_name(parser_engine)
          << ", shadow=" << styio_parser_engine_name(shadow_engine);
      if (!primary_route_detail.empty()) {
        oss << "; primary_route=" << primary_route_detail;
      }
      if (!shadow_route_detail.empty()) {
        oss << "; shadow_route=" << shadow_route_detail;
      }
      styio_emit_diagnostic(error_format, StyioErrorCategory::ParseError, fpath, oss.str());
      styio_write_shadow_artifact(
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
    styio_write_shadow_artifact(
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

  StyioAnalyzer analyzer = StyioAnalyzer();
  try {
    analyzer.typeInfer(session.ast());
  } catch (const StyioBaseException& ex) {
    styio_emit_diagnostic(error_format, StyioErrorCategory::TypeError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::TypeError);
  } catch (const std::exception& ex) {
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

  try {
    session.attach_ir(analyzer.toStyioIR(session.ast()));
  } catch (const StyioBaseException& ex) {
    styio_emit_diagnostic(error_format, StyioErrorCategory::TypeError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::TypeError);
  } catch (const std::exception& ex) {
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

    if (show_llvm_ir) {
      generator.print_llvm_ir();
    }
    styio_runtime_clear_error();
    generator.execute();
    if (styio_runtime_has_error()) {
      const char* runtime_err = styio_runtime_last_error();
      const char* runtime_subcode = styio_runtime_last_error_subcode();
      styio_emit_diagnostic(
        error_format,
        StyioErrorCategory::RuntimeError,
        fpath,
        runtime_err ? runtime_err : "runtime helper reported error",
        runtime_subcode ? runtime_subcode : "");
      return styio_exit_code(StyioErrorCategory::RuntimeError);
    }
  } catch (const StyioTypeError& ex) {
    styio_emit_diagnostic(error_format, StyioErrorCategory::TypeError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::TypeError);
  } catch (const StyioBaseException& ex) {
    styio_emit_diagnostic(error_format, StyioErrorCategory::RuntimeError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::RuntimeError);
  } catch (const std::exception& ex) {
    styio_emit_diagnostic(error_format, StyioErrorCategory::RuntimeError, fpath, ex.what());
    return styio_exit_code(StyioErrorCategory::RuntimeError);
  }

  return static_cast<int>(StyioExitCode::Success);
}
