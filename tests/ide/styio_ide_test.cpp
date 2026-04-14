#include <algorithm>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "StyioIDE/CompilerBridge.hpp"
#include "StyioIDE/Service.hpp"
#include "StyioIDE/Syntax.hpp"
#include "StyioLSP/Server.hpp"

namespace {

std::string
make_temp_dir() {
  const auto path = std::filesystem::temp_directory_path() / std::filesystem::path("styio-ide-test");
  std::filesystem::create_directories(path);
  return path.string();
}

std::string
temp_uri(const std::string& name) {
  return styio::ide::uri_from_path((std::filesystem::path(make_temp_dir()) / name).string());
}

}  // namespace

TEST(StyioIdeService, DocumentSymbolsHoverDefinitionAndCompletion) {
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(make_temp_dir()));

  const std::string uri = temp_uri("service_sample.styio");
  const std::string source =
    "# add := (a: i32, b: i32) => a + b\n"
    "result: i32 := add(1, 2)\n";

  const auto diagnostics = service.did_open(uri, source, 1);
  EXPECT_TRUE(diagnostics.empty());

  const auto symbols = service.document_symbols(uri);
  ASSERT_GE(symbols.size(), 2u);
  EXPECT_EQ(symbols[0].name, "add");

  const auto hover = service.hover(uri, styio::ide::Position{1, 16});
  ASSERT_TRUE(hover.has_value());
  EXPECT_NE(hover->contents.find("add"), std::string::npos);

  const auto definitions = service.definition(uri, styio::ide::Position{1, 16});
  ASSERT_EQ(definitions.size(), 1u);
  EXPECT_EQ(definitions[0].range.start, 2u);

  const std::string incomplete =
    "# add := (a: i32, b: i32) => a + b\n"
    "result: i32 := ad\n";
  service.did_change(uri, incomplete, 2);
  const auto completion = service.completion(uri, styio::ide::Position{1, 16});
  const auto it = std::find_if(
    completion.begin(),
    completion.end(),
    [](const styio::ide::CompletionItem& item)
    {
      return item.label == "add";
    });
  EXPECT_NE(it, completion.end());
}

TEST(StyioSyntaxParser, UsesTreeSitterBackendWhenAvailable) {
  styio::ide::SyntaxParser parser;
  styio::ide::DocumentSnapshot snapshot;
  snapshot.file_id = 1;
  snapshot.snapshot_id = 1;
  snapshot.path = "memory://syntax_sample.styio";
  snapshot.version = 1;
  snapshot.buffer = styio::ide::TextBuffer{
    "# add := (a: i32, b: i32) => {\n"
    "  value: i32 := a + b\n"
    "  <| value\n"
    "}\n"};

  const auto syntax = parser.parse(snapshot);

#ifdef STYIO_HAS_TREE_SITTER
  EXPECT_EQ(syntax.backend, styio::ide::SyntaxBackendKind::TreeSitter);
  ASSERT_FALSE(syntax.nodes.empty());
  EXPECT_EQ(syntax.nodes.front().label, "source_file");
  EXPECT_TRUE(std::any_of(
    syntax.nodes.begin(),
    syntax.nodes.end(),
    [](const styio::ide::SyntaxNode& node)
    {
      return node.label == "function_decl";
    }));
#else
  EXPECT_EQ(syntax.backend, styio::ide::SyntaxBackendKind::Tolerant);
#endif

  EXPECT_FALSE(syntax.folding_ranges.empty());
  EXPECT_TRUE(syntax.diagnostics.empty());
}

TEST(StyioSyntaxParser, ReusesIncrementalTreeForSubsequentParses) {
  styio::ide::SyntaxParser parser;

  styio::ide::DocumentSnapshot first_snapshot;
  first_snapshot.file_id = 9;
  first_snapshot.snapshot_id = 1;
  first_snapshot.path = make_temp_dir() + "/incremental_sample.styio";
  first_snapshot.version = 1;
  first_snapshot.buffer = styio::ide::TextBuffer{
    "# add := (a: i32, b: i32) => {\n"
    "  value: i32 := a + b\n"
    "  <| value\n"
    "}\n"};

  const auto first = parser.parse(first_snapshot);
  EXPECT_FALSE(first.reused_incremental_tree);

  styio::ide::DocumentSnapshot second_snapshot = first_snapshot;
  second_snapshot.snapshot_id = 2;
  second_snapshot.version = 2;
  second_snapshot.buffer = styio::ide::TextBuffer{
    "# add := (a: i32, b: i32) => {\n"
    "  value: i32 := a + b + 1\n"
    "  <| value\n"
    "}\n"};

  const auto second = parser.parse(second_snapshot);

#ifdef STYIO_HAS_TREE_SITTER
  EXPECT_TRUE(second.reused_incremental_tree);
#else
  EXPECT_FALSE(second.reused_incremental_tree);
#endif
}

TEST(StyioSemanticBridge, RecoversNightlyParseForLaterStatements) {
  const std::string source =
    "# broken := (a: i32, b: i32) => {\n"
    "  value: i32 := a +\n"
    "}\n"
    "# stable := (x: i32, y: i32) => x + y\n"
    "result: i32 := stable(1, 2)\n";

  const auto summary = styio::ide::analyze_document("memory://recovery_sample.styio", source);
  EXPECT_TRUE(summary.parse_success);
  EXPECT_TRUE(summary.used_recovery);
  EXPECT_FALSE(summary.diagnostics.empty());

  const auto it = summary.function_signatures.find("stable");
  ASSERT_NE(it, summary.function_signatures.end());
  EXPECT_NE(it->second.find("stable"), std::string::npos);
}

TEST(StyioLspServer, HandlesInitializeOpenAndCompletion) {
  styio::lsp::Server server;
  const std::string root_uri = styio::ide::uri_from_path(make_temp_dir());
  const std::string uri = temp_uri("server_sample.styio");

  auto init_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 1},
    {"method", "initialize"},
    {"params", llvm::json::Object{{"rootUri", root_uri}}}});
  ASSERT_EQ(init_messages.size(), 1u);
  EXPECT_NE(init_messages[0].payload.getObject("result"), nullptr);

  llvm::json::Object open_text_document{
    {"uri", uri},
    {"version", 1},
    {"text", "# add := (a: i32, b: i32) => a + b\nresult: i32 := ad\n"}};
  llvm::json::Object open_params{
    {"textDocument", std::move(open_text_document)}};
  auto open_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didOpen"},
    {"params", std::move(open_params)}});
  ASSERT_EQ(open_messages.size(), 1u);
  EXPECT_EQ(open_messages[0].payload.getString("method").value_or(""), "textDocument/publishDiagnostics");

  auto completion_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 2},
    {"method", "textDocument/completion"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}}},
       {"position", llvm::json::Object{{"line", 1}, {"character", 16}}}}}});
  ASSERT_EQ(completion_messages.size(), 1u);
  const auto* result = completion_messages[0].payload.getArray("result");
  ASSERT_NE(result, nullptr);

  bool found_add = false;
  for (const auto& item : *result) {
    const auto* object = item.getAsObject();
    if (object != nullptr && object->getString("label").value_or("") == "add") {
      found_add = true;
      break;
    }
  }
  EXPECT_TRUE(found_add);
}
