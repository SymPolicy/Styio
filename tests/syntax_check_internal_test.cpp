#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "StyioServices/StyioCLI/SyntaxCheck.cpp"

namespace fs = std::filesystem;

namespace {

class TempDir {
 public:
  explicit TempDir(const std::string& name)
    : path_(fs::temp_directory_path() / (name + "-" + std::to_string(
      std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count())))
  {
    std::error_code ec;
    fs::create_directories(path_, ec);
    EXPECT_FALSE(ec) << ec.message();
  }

  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }

  const fs::path& path() const {
    return path_;
  }

 private:
  fs::path path_;
};

void WriteText(const fs::path& path, const std::string& text) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  ASSERT_FALSE(ec) << ec.message();
  std::ofstream out(path, std::ios::binary);
  ASSERT_TRUE(out.is_open());
  out << text;
}

int RunSyntaxCheck(const fs::path& path, std::string& output) {
  std::vector<std::string> args = {
    "styio",
    "check",
    "--syntax",
    "--json",
    "--file",
    path.string(),
  };
  std::vector<char*> argv;
  argv.reserve(args.size());
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }

  testing::internal::CaptureStdout();
  const int exit_code =
    styio::services::run_syntax_check_cli(static_cast<int>(argv.size()), argv.data());
  output = testing::internal::GetCapturedStdout();
  return exit_code;
}

} // namespace

TEST(StyioSyntaxCheckInternal, SourceContextAndOffsetHelpersCoverEmptyFallbacks) {
  using namespace styio::services;

  SourceText empty;
  empty.line_starts.clear();
  const std::pair<std::size_t, std::size_t> empty_position{0, 0};
  EXPECT_EQ(position_at(empty, 42), empty_position);
  const SourceContext empty_context = source_context_at(empty, 42, 0);
  EXPECT_EQ(empty_context.caret, "^");
  EXPECT_TRUE(empty_context.line_text.empty());

  const SourceText source = make_source_text("alpha\nbeta");
  EXPECT_EQ(position_at(source, 0), (std::pair<std::size_t, std::size_t>{1, 1}));
  EXPECT_EQ(diagnostic_offset_from_message("no marker here", source), 0u);
  EXPECT_EQ(diagnostic_offset_from_message("bad at offset end", source), 0u);
  EXPECT_EQ(diagnostic_offset_from_message("bad at offset 999", source), source.text.size());

  const SourceContext first_context = source_context_at(source, 0, 0);
  EXPECT_EQ(first_context.caret, "^");
  EXPECT_EQ(first_context.range_start_column, 1u);
  EXPECT_EQ(first_context.range_end_column, 2u);

  const SourceContext context = source_context_at(source, 6, 8);
  EXPECT_EQ(context.line_text, "beta");
  EXPECT_EQ(context.range_start_column, 1u);
  EXPECT_EQ(context.range_end_column, 5u);
  EXPECT_EQ(context.caret, "^~~~");

  const SourceContext eof_context = source_context_at(source, source.text.size(), 4);
  EXPECT_EQ(eof_context.caret, "    ^");
  EXPECT_EQ(eof_context.range_end_column, 6u);

  const SourceText crlf_source = make_source_text("alpha\r\nbeta");
  const SourceContext crlf_context = source_context_at(crlf_source, 0, 5);
  EXPECT_EQ(crlf_context.line_text, "alpha");
}

TEST(StyioSyntaxCheckInternal, JsonEscapeCoversControlBytes) {
  using namespace styio::services;

  const std::string escaped =
    json_escape(std::string("quote\" slash\\ tab\t cr\r lf\n ctrl") + static_cast<char>(1));
  EXPECT_NE(escaped.find("\\\""), std::string::npos);
  EXPECT_NE(escaped.find("\\\\"), std::string::npos);
  EXPECT_NE(escaped.find("\\t"), std::string::npos);
  EXPECT_NE(escaped.find("\\r"), std::string::npos);
  EXPECT_NE(escaped.find("\\n"), std::string::npos);
  EXPECT_NE(escaped.find("\\u0001"), std::string::npos);
}

TEST(StyioSyntaxCheckInternal, ResultEmissionAndFileFailureHelpersStayExplicit) {
  using namespace styio::services;

  std::string output;
  std::string error;
  EXPECT_FALSE(read_file("/definitely/not/a/styio/source/file.styio", output, error));
  EXPECT_EQ(error, "file not found");

  const SourceText source = make_source_text("first\nsecond\n");
  testing::internal::CaptureStdout();
  emit_result(
    "quoted\"file.styio",
    "custom",
    true,
    std::string(diagnostics::kPhaseParse),
    StyioParserEngine::Nightly,
    source,
    {
      Diagnostic{std::string(diagnostics::kPhaseParse), "P1", "one", 0, 1},
      Diagnostic{std::string(diagnostics::kPhaseParse), "P2", "two", 6, 0},
    });
  output = testing::internal::GetCapturedStdout();
  EXPECT_NE(output.find("\"ok\":true"), std::string::npos) << output;
  EXPECT_NE(output.find("},{\"phase\":\"parse\""), std::string::npos) << output;
  EXPECT_NE(output.find("quoted\\\"file.styio"), std::string::npos) << output;

  std::vector<StyioToken*> tokens = StyioTokenizer::tokenize("");
  StyioContext* context = StyioContext::Create("<empty>", "", {}, tokens, false);
  context->record_parse_diagnostic(0, 0, "empty recovery");
  const SourceText empty_source = make_source_text("");
  const std::vector<Diagnostic> diagnostics = parse_diagnostics_from_context(*context, empty_source);
  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_EQ(diagnostics[0].length, 0u);
  delete context;
  delete_tokens(tokens);
  StyioAST::destroy_all_tracked_nodes();
}

TEST(StyioSyntaxCheckInternal, CliErrorAndHelpHelpersEmitJsonAndUsage) {
  using namespace styio::services;

  testing::internal::CaptureStdout();
  EXPECT_EQ(emit_cli_error("bad option", "input.styio"), 6);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_NE(output.find("\"status\":\"cli_error\""), std::string::npos) << output;
  EXPECT_NE(output.find("input.styio"), std::string::npos) << output;

  testing::internal::CaptureStdout();
  print_help();
  output = testing::internal::GetCapturedStdout();
  EXPECT_NE(output.find("Usage: styio check --syntax"), std::string::npos) << output;

  char styio[] = "styio";
  char check[] = "check";
  char syntax[] = "--syntax";
  char json[] = "--json";
  char parser_engine[] = "--parser-engine";
  char* null_arg = nullptr;
  {
    char* argv[] = {styio, check, null_arg};
    testing::internal::CaptureStdout();
    EXPECT_EQ(run_syntax_check_cli(3, argv), 6);
    output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("unsupported styio check option"), std::string::npos) << output;
  }
  {
    char* argv[] = {styio, check, syntax, json, parser_engine, null_arg};
    testing::internal::CaptureStdout();
    EXPECT_EQ(run_syntax_check_cli(6, argv), 6);
    output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("--parser-engine requires nightly"), std::string::npos) << output;
  }
}

TEST(StyioSyntaxCheckInternal, RunCliCoversLexAndParseDiagnostics) {
  TempDir temp("styio-syntax-internal");

  const fs::path lex_source = temp.path() / "lex.styio";
  WriteText(lex_source, "/* unterminated");
  std::string output;
  EXPECT_EQ(RunSyntaxCheck(lex_source, output), 2);
  EXPECT_NE(output.find("\"status\":\"lexical_error\""), std::string::npos) << output;
  EXPECT_NE(output.find("\"phase\":\"lex\""), std::string::npos) << output;

  const fs::path parse_source = temp.path() / "parse.styio";
  WriteText(parse_source, "foo.bar(1).baz(2)\n");
  EXPECT_EQ(RunSyntaxCheck(parse_source, output), 3);
  EXPECT_NE(output.find("\"status\":\"syntax_error\""), std::string::npos) << output;
  EXPECT_NE(output.find("\"phase\":\"parse\""), std::string::npos) << output;
}
