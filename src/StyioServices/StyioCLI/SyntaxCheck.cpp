#include "SyntaxCheck.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "StyioAST/AST.hpp"
#include "StyioException/Exception.hpp"
#include "StyioParser/Parser.hpp"
#include "StyioParser/Tokenizer.hpp"
#include "StyioServices/DiagnosticContract.hpp"
#include "StyioToken/Token.hpp"

#ifndef STYIO_PROJECT_VERSION
#define STYIO_PROJECT_VERSION "0.0.0-dev"
#endif

#ifndef STYIO_RELEASE_CHANNEL
#define STYIO_RELEASE_CHANNEL "dev"
#endif

#ifndef STYIO_EDITION_MAX
#define STYIO_EDITION_MAX "2026"
#endif

namespace styio::services {

namespace {

enum class SyntaxCheckExitCode : int
{
  Success = 0,
  LexError = 2,
  ParseError = 3,
  CliError = 6,
};

struct SourceText
{
  std::string text;
  std::vector<std::size_t> line_starts;
  std::vector<std::pair<std::size_t, std::size_t>> line_seps;
};

struct Diagnostic
{
  std::string phase;
  std::string code;
  std::string message;
  std::size_t offset = 0;
  std::size_t length = 0;
};

struct SourceContext
{
  std::string line_text;
  std::size_t range_start_column = 1;
  std::size_t range_end_column = 1;
  std::string caret;
};

std::string
json_escape(const std::string& input) {
  std::string out;
  out.reserve(input.size() + 16);
  for (char ch : input) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
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
        if (static_cast<unsigned char>(ch) < 0x20) {
          out += "\\u00";
          constexpr char kHex[] = "0123456789abcdef";
          out.push_back(kHex[(static_cast<unsigned char>(ch) >> 4) & 0x0f]);
          out.push_back(kHex[static_cast<unsigned char>(ch) & 0x0f]);
        }
        else {
          out.push_back(ch);
        }
        break;
    }
  }
  return out;
}

SourceText
make_source_text(std::string text) {
  SourceText source;
  source.text = std::move(text);
  source.line_starts.push_back(0);

  std::size_t line_start = 0;
  std::size_t line_len = 0;
  for (std::size_t i = 0; i < source.text.size(); ++i) {
    const char ch = source.text[i];
    if (ch == '\n') {
      source.line_seps.emplace_back(line_start, line_len);
      source.line_starts.push_back(i + 1);
      line_start = i + 1;
      line_len = 0;
      continue;
    }
    line_len += 1;
  }
  if (!source.text.empty() && source.text.back() != '\n') {
    source.line_seps.emplace_back(line_start, line_len);
  }

  return source;
}

std::pair<std::size_t, std::size_t>
position_at(const SourceText& source, std::size_t offset) {
  if (source.line_starts.empty()) {
    return {0, 0};
  }
  offset = std::min(offset, source.text.size());
  const auto it = std::upper_bound(source.line_starts.begin(), source.line_starts.end(), offset);
  const std::size_t line =
    it == source.line_starts.begin() ? 0 : static_cast<std::size_t>((it - source.line_starts.begin()) - 1);
  return {line + 1, offset - source.line_starts[line] + 1};
}

SourceContext
source_context_at(const SourceText& source, std::size_t offset, std::size_t length) {
  SourceContext context;
  if (source.line_starts.empty()) {
    context.caret = "^";
    return context;
  }

  offset = std::min(offset, source.text.size());
  const auto it = std::upper_bound(source.line_starts.begin(), source.line_starts.end(), offset);
  const std::size_t line_index =
    it == source.line_starts.begin() ? 0 : static_cast<std::size_t>((it - source.line_starts.begin()) - 1);
  const std::size_t line_start = source.line_starts[line_index];
  std::size_t line_end =
    line_index + 1 < source.line_starts.size() ? source.line_starts[line_index + 1] : source.text.size();
  if (line_end > line_start && source.text[line_end - 1] == '\n') {
    line_end -= 1;
  }
  if (line_end > line_start && source.text[line_end - 1] == '\r') {
    line_end -= 1;
  }

  context.line_text = source.text.substr(line_start, line_end - line_start);
  context.range_start_column = offset - line_start + 1;
  const std::size_t line_length = line_end - line_start;
  const std::size_t start_offset_in_line = offset - line_start;
  const std::size_t max_width_on_line =
    line_length > start_offset_in_line ? line_length - start_offset_in_line : 1;
  const std::size_t requested_length = length == 0 ? 1 : length;
  const std::size_t effective_length = std::max<std::size_t>(1, std::min(requested_length, max_width_on_line));
  context.range_end_column = context.range_start_column + effective_length;
  const std::size_t caret_indent = context.range_start_column > 0 ? context.range_start_column - 1 : 0;
  context.caret.assign(caret_indent, ' ');
  context.caret.push_back('^');
  if (effective_length > 1) {
    context.caret.append(effective_length - 1, '~');
  }
  return context;
}

std::size_t
diagnostic_offset_from_message(const std::string& message, const SourceText& source) {
  const std::string marker = " at offset ";
  const std::size_t marker_pos = message.rfind(marker);
  if (marker_pos == std::string::npos) {
    return 0;
  }

  std::size_t cursor = marker_pos + marker.size();
  if (cursor >= message.size() || !std::isdigit(static_cast<unsigned char>(message[cursor]))) {
    return 0;
  }

  std::size_t value = 0;
  while (cursor < message.size() && std::isdigit(static_cast<unsigned char>(message[cursor]))) {
    value = (value * 10) + static_cast<std::size_t>(message[cursor] - '0');
    ++cursor;
  }
  return std::min(value, source.text.size());
}

std::vector<Diagnostic>
parse_diagnostics_from_context(const StyioContext& context, const SourceText& source) {
  std::vector<Diagnostic> diagnostics;
  for (const auto& diagnostic : context.parse_diagnostics()) {
    const std::size_t start = std::min(diagnostic.start, source.text.size());
    const std::size_t end = std::min(std::max(diagnostic.end, diagnostic.start), source.text.size());
    const std::size_t length = end > start ? end - start : (source.text.empty() ? 0u : 1u);
    diagnostics.push_back(Diagnostic{
      std::string(diagnostics::kPhaseParse),
      diagnostics::classify_parse_code(diagnostic.message),
      diagnostic.message,
      start,
      length});
  }
  return diagnostics;
}

bool
read_file(const std::string& path, std::string& out, std::string& error) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    error = std::filesystem::exists(path) ? "cannot open file" : "file not found";
    return false;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  out = buffer.str();
  if (!input.good() && !input.eof()) {
    error = "failed to read file";
    return false;
  }
  return true;
}

void
delete_tokens(std::vector<StyioToken*>& tokens) {
  for (auto* token : tokens) {
    delete token;
  }
  tokens.clear();
}

void
emit_result(
  const std::string& file,
  const std::string& status,
  bool ok,
  const std::string& phase,
  StyioParserEngine engine,
  const SourceText& source,
  const std::vector<Diagnostic>& diagnostics
) {
  std::cout
    << "{\"schema_version\":1"
    << ",\"contract\":\"syntax-check\""
    << ",\"tool\":\"styio\""
    << ",\"compiler_version\":\"" << json_escape(STYIO_PROJECT_VERSION) << "\""
    << ",\"channel\":\"" << json_escape(STYIO_RELEASE_CHANNEL) << "\""
    << ",\"grammar_version\":\"" << json_escape(STYIO_EDITION_MAX) << "\""
    << ",\"file\":\"" << json_escape(file) << "\""
    << ",\"status\":\"" << json_escape(status) << "\""
    << ",\"ok\":" << (ok ? "true" : "false")
    << ",\"phase\":\"" << json_escape(phase) << "\""
    << ",\"parser_engine\":\"" << styio_parser_engine_name_latest(engine) << "\""
    << ",\"diagnostics\":[";

  for (std::size_t i = 0; i < diagnostics.size(); ++i) {
    const auto& diagnostic = diagnostics[i];
    const auto [line, column] = position_at(source, diagnostic.offset);
    if (i > 0) {
      std::cout << ",";
    }
    std::cout
      << "{\"phase\":\"" << json_escape(diagnostic.phase) << "\""
      << ",\"code\":\"" << json_escape(diagnostic.code) << "\""
      << ",\"severity\":\"error\""
      << ",\"message\":\"" << json_escape(diagnostic.message) << "\""
      << ",\"line\":" << line
      << ",\"column\":" << column
      << ",\"offset\":" << diagnostic.offset
      << ",\"length\":" << diagnostic.length
      << ",\"source_context\":";
    const SourceContext context = source_context_at(source, diagnostic.offset, diagnostic.length);
    std::cout
      << "{\"line_text\":\"" << json_escape(context.line_text) << "\""
      << ",\"range_start_column\":" << context.range_start_column
      << ",\"range_end_column\":" << context.range_end_column
      << ",\"caret\":\"" << json_escape(context.caret) << "\"}"
      << ",\"notes\":[]"
      << "}";
  }

  std::cout << "]}\n";
}

int
emit_cli_error(const std::string& message, const std::string& file = "") {
  SourceText empty = make_source_text("");
  emit_result(
    file,
    "cli_error",
    false,
    std::string(diagnostics::kPhaseService),
    StyioParserEngine::Nightly,
    empty,
    {Diagnostic{
      std::string(diagnostics::kPhaseService),
      diagnostics::classify_service_code("", message),
      message,
      0,
      0}});
  return static_cast<int>(SyntaxCheckExitCode::CliError);
}

void
print_help() {
  std::cout
    << "Usage: styio check --syntax --json --file <path> [--parser-engine nightly]\n"
    << "\n"
    << "Runs lexing, authoritative nightly parsing, and AST construction only. It does not type-check,\n"
    << "lower, codegen, execute, or access runtime resources.\n";
}

}  // namespace

int
run_syntax_check_cli(int argc, char* argv[]) {
  bool syntax_only = false;
  bool json = false;
  std::string file;
  std::string parser_engine_raw = "nightly";

  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i] == nullptr ? std::string() : std::string(argv[i]);
    if (arg == "--help" || arg == "-h") {
      print_help();
      return static_cast<int>(SyntaxCheckExitCode::Success);
    }
    if (arg == "--syntax") {
      syntax_only = true;
      continue;
    }
    if (arg == "--json") {
      json = true;
      continue;
    }
    if (arg == "--file" || arg == "-f") {
      if (i + 1 >= argc || argv[i + 1] == nullptr) {
        return emit_cli_error("--file requires a path");
      }
      file = argv[++i];
      continue;
    }
    if (arg.rfind("--file=", 0) == 0) {
      file = arg.substr(std::string("--file=").size());
      continue;
    }
    if (arg == "--parser-engine") {
      if (i + 1 >= argc || argv[i + 1] == nullptr) {
        return emit_cli_error("--parser-engine requires nightly");
      }
      parser_engine_raw = argv[++i];
      continue;
    }
    if (arg.rfind("--parser-engine=", 0) == 0) {
      parser_engine_raw = arg.substr(std::string("--parser-engine=").size());
      continue;
    }
    return emit_cli_error("unsupported styio check option: " + arg, file);
  }

  if (!syntax_only) {
    return emit_cli_error("styio check currently requires --syntax", file);
  }
  if (!json) {
    return emit_cli_error("styio check --syntax currently requires --json", file);
  }
  if (file.empty()) {
    return emit_cli_error("styio check --syntax requires --file <path>");
  }

  StyioParserEngine parser_engine = StyioParserEngine::Nightly;
  if (parser_engine_raw != "nightly") {
    return emit_cli_error("styio check --syntax only accepts the authoritative nightly parser", file);
  }

  std::string text;
  std::string read_error;
  if (!read_file(file, text, read_error)) {
    return emit_cli_error(read_error + ": " + file, file);
  }

  SourceText source = make_source_text(std::move(text));
  std::vector<StyioToken*> tokens;
  StyioContext* context = nullptr;
  MainBlockAST* ast = nullptr;

  auto cleanup = [&]()
  {
    delete ast;
    delete context;
    delete_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  };

  try {
    tokens = StyioTokenizer::tokenize(source.text);
  } catch (const StyioLexError& ex) {
    const std::size_t offset = diagnostic_offset_from_message(ex.what(), source);
    cleanup();
    emit_result(
      file,
      "lexical_error",
      false,
      std::string(diagnostics::kPhaseLex),
      parser_engine,
      source,
      {Diagnostic{
        std::string(diagnostics::kPhaseLex),
        diagnostics::classify_lex_code(ex.what()),
        ex.what(),
        offset,
        source.text.empty() ? 0u : 1u}});
    return static_cast<int>(SyntaxCheckExitCode::LexError);
  } catch (const std::exception& ex) {
    const std::size_t offset = diagnostic_offset_from_message(ex.what(), source);
    cleanup();
    emit_result(
      file,
      "lexical_error",
      false,
      std::string(diagnostics::kPhaseLex),
      parser_engine,
      source,
      {Diagnostic{
        std::string(diagnostics::kPhaseLex),
        diagnostics::classify_lex_code(ex.what()),
        ex.what(),
        offset,
        source.text.empty() ? 0u : 1u}});
    return static_cast<int>(SyntaxCheckExitCode::LexError);
  }

  try {
    context = StyioContext::Create(file, source.text, source.line_seps, tokens, false);
    ast = parse_main_block_with_engine_latest(*context, parser_engine, nullptr, StyioParseMode::Recovery);
  } catch (const StyioBaseException& ex) {
    const std::size_t offset =
      context == nullptr ? 0 : std::min(context->current_token_end_pos(), source.text.size());
    cleanup();
    emit_result(
      file,
      "syntax_error",
      false,
      std::string(diagnostics::kPhaseParse),
      parser_engine,
      source,
      {Diagnostic{
        std::string(diagnostics::kPhaseParse),
        diagnostics::classify_parse_code(ex.what()),
        ex.what(),
        offset,
        source.text.empty() ? 0u : 1u}});
    return static_cast<int>(SyntaxCheckExitCode::ParseError);
  } catch (const std::exception& ex) {
    const std::size_t offset =
      context == nullptr ? 0 : std::min(context->current_token_end_pos(), source.text.size());
    cleanup();
    emit_result(
      file,
      "syntax_error",
      false,
      std::string(diagnostics::kPhaseParse),
      parser_engine,
      source,
      {Diagnostic{
        std::string(diagnostics::kPhaseParse),
        diagnostics::classify_parse_code(ex.what()),
        ex.what(),
        offset,
        source.text.empty() ? 0u : 1u}});
    return static_cast<int>(SyntaxCheckExitCode::ParseError);
  }

  std::vector<Diagnostic> parse_diagnostics =
    context == nullptr ? std::vector<Diagnostic>{} : parse_diagnostics_from_context(*context, source);
  if (!parse_diagnostics.empty()) {
    cleanup();
    emit_result(
      file,
      "syntax_error",
      false,
      "parse",
      parser_engine,
      source,
      parse_diagnostics);
    return static_cast<int>(SyntaxCheckExitCode::ParseError);
  }

  cleanup();
  emit_result(file, "ok", true, std::string(diagnostics::kPhaseParse), parser_engine, source, {});
  return static_cast<int>(SyntaxCheckExitCode::Success);
}

}  // namespace styio::services
