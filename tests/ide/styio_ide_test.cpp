#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "StyioAST/AST.hpp"
#include "StyioIDE/CompilerBridge.hpp"
#include "StyioIDE/HIR.hpp"
#include "StyioIDE/Service.hpp"
#include "StyioIDE/Syntax.hpp"
#include "StyioLSP/Server.hpp"
#include "StyioException/Exception.hpp"
#include "StyioParser/Parser.hpp"
#include "StyioParser/Tokenizer.hpp"
#include "llvm/Support/FormatVariadic.h"

namespace {

std::string
make_temp_dir() {
  const auto path = std::filesystem::temp_directory_path() / std::filesystem::path("styio-ide-test");
  std::filesystem::create_directories(path);
  return path.string();
}

std::string
make_temp_project_dir(const std::string& name) {
  const auto path = std::filesystem::path(make_temp_dir()) / name;
  std::filesystem::remove_all(path);
  std::filesystem::create_directories(path);
  return path.string();
}

void
write_text_file(const std::string& path, const std::string& contents) {
  std::ofstream output(path);
  output << contents;
}

std::string
temp_uri(const std::string& name) {
  return styio::ide::uri_from_path((std::filesystem::path(make_temp_dir()) / name).string());
}

std::string
read_text_file(const std::string& path) {
  std::ifstream input(path);
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

std::size_t
nth_occurrence(const std::string& text, const std::string& needle, std::size_t occurrence) {
  std::size_t offset = 0;
  for (std::size_t i = 0; i <= occurrence; ++i) {
    offset = text.find(needle, offset);
    if (offset == std::string::npos) {
      return std::string::npos;
    }
    if (i == occurrence) {
      return offset;
    }
    offset += needle.size();
  }
  return std::string::npos;
}

llvm::json::Object
lsp_position(int line, int character) {
  return llvm::json::Object{{"line", line}, {"character", character}};
}

llvm::json::Object
lsp_range(int start_line, int start_character, int end_line, int end_character) {
  return llvm::json::Object{
    {"start", lsp_position(start_line, start_character)},
    {"end", lsp_position(end_line, end_character)}};
}

std::string
lsp_frame(const llvm::json::Object& object) {
  llvm::json::Object value = object;
  const std::string body = llvm::formatv("{0}", llvm::json::Value(std::move(value))).str();
  return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

std::string
lsp_messages_to_text(const std::vector<styio::lsp::OutboundMessage>& messages) {
  std::string output;
  for (const auto& message : messages) {
    llvm::json::Object value = message.payload;
    const std::string body = llvm::formatv("{0}", llvm::json::Value(std::move(value))).str();
    output += "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
    output += body;
  }
  return output;
}

bool
has_completion_label(const std::vector<styio::ide::CompletionItem>& items, const std::string& label) {
  return std::any_of(
    items.begin(),
    items.end(),
    [&](const styio::ide::CompletionItem& item)
    {
      return item.label == label;
    });
}

std::size_t
completion_index(const std::vector<styio::ide::CompletionItem>& items, const std::string& label) {
  const auto it = std::find_if(
    items.begin(),
    items.end(),
    [&](const styio::ide::CompletionItem& item)
    {
      return item.label == label;
    });
  return it == items.end() ? items.size() : static_cast<std::size_t>(std::distance(items.begin(), it));
}

bool
has_indexed_symbol(
  const std::vector<styio::ide::IndexedSymbol>& symbols,
  const std::string& name,
  const std::string& path
) {
  return std::any_of(
    symbols.begin(),
    symbols.end(),
    [&](const styio::ide::IndexedSymbol& symbol)
    {
      return symbol.name == name && symbol.path == path;
    });
}

bool
has_location(
  const std::vector<styio::ide::Location>& locations,
  const std::string& path,
  std::size_t start
) {
  return std::any_of(
    locations.begin(),
    locations.end(),
    [&](const styio::ide::Location& location)
    {
      return location.path == path && location.range.start == start;
    });
}

std::vector<std::size_t>
syntax_statement_starts(const styio::ide::SyntaxSnapshot& syntax) {
  std::vector<std::size_t> starts;
  bool at_stmt_start = true;
  std::size_t block_depth = 0;

  for (std::size_t i = 0; i < syntax.tokens.size(); ++i) {
    const auto& token = syntax.tokens[i];
    if (token.type == StyioTokenType::TOK_LCURBRAC) {
      block_depth += 1;
    } else if (token.type == StyioTokenType::TOK_RCURBRAC && block_depth > 0) {
      block_depth -= 1;
    }

    if (token.type == StyioTokenType::TOK_LF && block_depth == 0) {
      at_stmt_start = true;
      continue;
    }
    if (token.is_trivia() || token.type == StyioTokenType::TOK_EOF) {
      continue;
    }
    if (block_depth == 0 && at_stmt_start) {
      starts.push_back(token.range.start);
      at_stmt_start = false;
    }
  }

  return starts;
}

std::vector<std::string>
syntax_outline(const styio::ide::SyntaxSnapshot& syntax) {
  std::vector<std::string> outline;
  const auto starts = syntax_statement_starts(syntax);
  for (std::size_t start : starts) {
    const auto it = std::find_if(
      syntax.tokens.begin(),
      syntax.tokens.end(),
      [&](const styio::ide::SyntaxToken& token)
      {
        return token.range.start == start;
      });
    if (it == syntax.tokens.end()) {
      continue;
    }

    const std::size_t token_index = static_cast<std::size_t>(std::distance(syntax.tokens.begin(), it));
    const auto& token = syntax.tokens[token_index];
    if (token.type == StyioTokenType::TOK_HASH) {
      for (std::size_t i = token_index + 1; i < syntax.tokens.size(); ++i) {
        if (syntax.tokens[i].is_trivia()) {
          continue;
        }
        if (syntax.tokens[i].type == StyioTokenType::NAME) {
          outline.push_back("function:" + syntax.tokens[i].lexeme);
        }
        break;
      }
      continue;
    }

    if (token.type == StyioTokenType::NAME) {
      outline.push_back("binding:" + token.lexeme);
      continue;
    }

    if (token.type == StyioTokenType::TOK_LBOXBRAC) {
      outline.push_back("collection:[");
      continue;
    }

    outline.push_back("stmt:" + token.lexeme);
  }

  return outline;
}

std::vector<std::size_t>
syntax_block_starts(const styio::ide::SyntaxSnapshot& syntax) {
  std::vector<std::size_t> starts;
  for (std::size_t i = 0; i < syntax.tokens.size(); ++i) {
    if (syntax.tokens[i].type == StyioTokenType::TOK_LCURBRAC) {
      starts.push_back(syntax.tokens[i].range.start);
    }
  }
  return starts;
}

std::size_t
syntax_max_block_depth(const styio::ide::SyntaxSnapshot& syntax) {
  std::size_t depth = 0;
  std::size_t max_depth = 0;
  for (const auto& token : syntax.tokens) {
    if (token.type == StyioTokenType::TOK_LCURBRAC) {
      depth += 1;
      max_depth = std::max(max_depth, depth);
    } else if (token.type == StyioTokenType::TOK_RCURBRAC && depth > 0) {
      depth -= 1;
    }
  }
  return max_depth;
}

std::size_t
count_non_trivia_tokens(const styio::ide::SyntaxSnapshot& syntax) {
  return static_cast<std::size_t>(std::count_if(
    syntax.tokens.begin(),
    syntax.tokens.end(),
    [](const styio::ide::SyntaxToken& token)
    {
      return !token.is_trivia() && token.type != StyioTokenType::TOK_EOF;
    }));
}

bool
has_token_boundary(
  const styio::ide::SyntaxSnapshot& syntax,
  const std::string& lexeme,
  std::size_t start
) {
  return std::any_of(
    syntax.tokens.begin(),
    syntax.tokens.end(),
    [&](const styio::ide::SyntaxToken& token)
    {
      return token.lexeme == lexeme && token.range.start == start;
    });
}

std::vector<std::string>
nightly_outline(const std::string& path, const std::string& source, bool* used_recovery = nullptr) {
  std::vector<std::string> outline;
  std::vector<StyioToken*> tokens;
  StyioContext* context = nullptr;
  MainBlockAST* ast = nullptr;

  auto cleanup = [&]()
  {
    delete ast;
    delete context;
    for (auto* token : tokens) {
      delete token;
    }
    StyioAST::destroy_all_tracked_nodes();
  };

  try {
    tokens = StyioTokenizer::tokenize(source);
    styio::ide::TextBuffer buffer(source);
    context = StyioContext::Create(path, source, buffer.build_line_seps(), tokens, false);
    ast = parse_main_block_with_engine_latest(
      *context,
      StyioParserEngine::Nightly,
      nullptr,
      StyioParseMode::Recovery);
    if (used_recovery != nullptr) {
      *used_recovery = !context->parse_diagnostics().empty();
    }
    if (ast != nullptr) {
      for (auto* stmt : ast->getStmts()) {
        if (auto* fn = dynamic_cast<FunctionAST*>(stmt)) {
          outline.push_back("function:" + fn->getNameAsStr());
          continue;
        }
        if (auto* fn = dynamic_cast<SimpleFuncAST*>(stmt)) {
          if (fn->func_name != nullptr) {
            outline.push_back("function:" + fn->func_name->getAsStr());
          }
          continue;
        }
        if (auto* bind = dynamic_cast<FlexBindAST*>(stmt)) {
          outline.push_back("binding:" + bind->getNameAsStr());
          continue;
        }
        if (auto* bind = dynamic_cast<FinalBindAST*>(stmt)) {
          outline.push_back("binding:" + bind->getName());
          continue;
        }
        outline.push_back("stmt:other");
      }
    }
  } catch (...) {
    if (used_recovery != nullptr) {
      *used_recovery = false;
    }
  }

  cleanup();
  return outline;
}

std::uint64_t
measure_microseconds(const std::function<void()>& fn) {
  const auto start = std::chrono::steady_clock::now();
  fn();
  return static_cast<std::uint64_t>(
    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
}

std::uint64_t
percentile95(std::vector<std::uint64_t> samples) {
  if (samples.empty()) {
    return 0;
  }
  std::sort(samples.begin(), samples.end());
  const std::size_t index = (samples.size() * 95 + 99) / 100 - 1;
  return samples[std::min(index, samples.size() - 1)];
}

std::string
make_incremental_perf_source(std::size_t line_count) {
  std::ostringstream source;
  for (std::size_t i = 0; i < line_count; ++i) {
    source << "value_" << i << ": i32 := 1\n";
  }
  return source.str();
}

std::string
make_hot_query_perf_source(std::size_t function_count) {
  std::ostringstream source;
  for (std::size_t i = 0; i < function_count; ++i) {
    source << "# fn_" << i << " := (value: i32) => value + " << (i % 7) << "\n";
  }
  for (std::size_t i = 0; i + 2 < function_count; ++i) {
    source << "value_" << i << ": i32 := fn_" << i << "(1)\n";
  }
  source << "needle_value: i32 := fn_" << (function_count - 1) << "(1)\n";
  source << "hover_target: i32 := needle_value\n";
  source << "completion_target: i32 := fn_" << (function_count - 1) << "(need\n";
  return source.str();
}

styio::ide::HirModule
build_hir_for_source(
  const std::string& path,
  const std::string& source,
  styio::ide::FileId file_id,
  styio::ide::SnapshotId snapshot_id,
  styio::ide::HirIdentityStore& identity_store
) {
  styio::ide::DocumentSnapshot snapshot;
  snapshot.file_id = file_id;
  snapshot.snapshot_id = snapshot_id;
  snapshot.path = path;
  snapshot.version = static_cast<styio::ide::DocumentVersion>(snapshot_id);
  snapshot.buffer = styio::ide::TextBuffer{source};

  styio::ide::SyntaxParser parser;
  const auto syntax = parser.parse(snapshot);
  const auto semantic = styio::ide::analyze_document(path, source);
  return styio::ide::HirBuilder{}.build(syntax, semantic, identity_store);
}

const styio::ide::HirItem*
find_hir_item(
  const styio::ide::HirModule& module,
  const std::string& name,
  styio::ide::HirItemKind kind
) {
  auto it = std::find_if(
    module.items.begin(),
    module.items.end(),
    [&](const styio::ide::HirItem& item)
    {
      return item.name == name && item.kind == kind;
    });
  return it == module.items.end() ? nullptr : &(*it);
}

const styio::ide::HirSymbol*
find_hir_symbol(
  const styio::ide::HirModule& module,
  const std::string& name,
  styio::ide::SymbolKind kind
) {
  auto it = std::find_if(
    module.symbols.begin(),
    module.symbols.end(),
    [&](const styio::ide::HirSymbol& symbol)
    {
      return symbol.name == name && symbol.kind == kind;
    });
  return it == module.symbols.end() ? nullptr : &(*it);
}

}  // namespace

TEST(StyioVfs, AppliesSequentialTextEdits) {
  styio::ide::VirtualFileSystem vfs;
  const std::string path = make_temp_dir() + "/vfs_multi_edit.styio";
  vfs.open(path, "one two three", 1);

  styio::ide::DocumentDelta delta;
  delta.edits.push_back(styio::ide::TextEdit{styio::ide::TextRange{4, 7}, "TWO"});
  delta.edits.push_back(styio::ide::TextEdit{styio::ide::TextRange{8, 13}, "THREE"});
  delta.edits.push_back(styio::ide::TextEdit{styio::ide::TextRange{0, 3}, "ONE"});

  const auto result = vfs.update(path, delta, 2);

  ASSERT_NE(result.snapshot, nullptr);
  EXPECT_TRUE(result.applied_incremental);
  EXPECT_FALSE(result.needs_full_resync);
  EXPECT_EQ(result.snapshot->buffer.text(), "ONE TWO THREE");
  ASSERT_EQ(result.snapshot->applied_edits.size(), 3u);

  styio::ide::DocumentDelta invalid_delta;
  invalid_delta.edits.push_back(styio::ide::TextEdit{styio::ide::TextRange{99, 100}, "!"});
  const auto invalid_result = vfs.update(path, invalid_delta, 3);

  ASSERT_NE(invalid_result.snapshot, nullptr);
  EXPECT_TRUE(invalid_result.needs_full_resync);
  EXPECT_EQ(invalid_result.snapshot->buffer.text(), "ONE TWO THREE");
}

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

TEST(StyioSyntaxParser, ReusesIncrementalTreeAcrossMultiEditDelta) {
  styio::ide::SyntaxParser parser;

  styio::ide::DocumentSnapshot first_snapshot;
  first_snapshot.file_id = 10;
  first_snapshot.snapshot_id = 1;
  first_snapshot.path = make_temp_dir() + "/incremental_multi_edit_sample.styio";
  first_snapshot.version = 1;
  first_snapshot.buffer = styio::ide::TextBuffer{
    "# add := (a: i32, b: i32) => {\n"
    "  value: i32 := a + b\n"
    "  <| value\n"
    "}\n"};

  const auto first = parser.parse(first_snapshot);
  EXPECT_FALSE(first.reused_incremental_tree);

  std::string final_text = first_snapshot.buffer.text();
  std::vector<styio::ide::TextEdit> edits;
  std::size_t offset = final_text.find("add");
  ASSERT_NE(offset, std::string::npos);
  edits.push_back(styio::ide::TextEdit{styio::ide::TextRange{offset, offset + 3}, "sum"});
  final_text.replace(offset, 3, "sum");

  offset = final_text.find("value");
  ASSERT_NE(offset, std::string::npos);
  edits.push_back(styio::ide::TextEdit{styio::ide::TextRange{offset, offset + 5}, "total"});
  final_text.replace(offset, 5, "total");

  styio::ide::DocumentSnapshot second_snapshot = first_snapshot;
  second_snapshot.snapshot_id = 2;
  second_snapshot.version = 2;
  second_snapshot.buffer = styio::ide::TextBuffer{final_text};
  second_snapshot.applied_edits = edits;

  const auto second = parser.parse(second_snapshot);

#ifdef STYIO_HAS_TREE_SITTER
  EXPECT_TRUE(second.reused_incremental_tree);
#else
  EXPECT_FALSE(second.reused_incremental_tree);
#endif
  EXPECT_TRUE(second.diagnostics.empty());
}

TEST(StyioSyntaxParser, MultiEditIncrementalMatchesFullParse) {
  styio::ide::SyntaxParser incremental_parser;

  styio::ide::DocumentSnapshot first_snapshot;
  first_snapshot.file_id = 11;
  first_snapshot.snapshot_id = 1;
  first_snapshot.path = make_temp_dir() + "/incremental_equivalence_sample.styio";
  first_snapshot.version = 1;
  first_snapshot.buffer = styio::ide::TextBuffer{
    "# add := (a: i32, b: i32) => {\n"
    "  value: i32 := a + b\n"
    "  <| value\n"
    "}\n"};

  (void)incremental_parser.parse(first_snapshot);

  std::string final_text = first_snapshot.buffer.text();
  std::vector<styio::ide::TextEdit> edits;
  std::size_t offset = final_text.find("a + b");
  ASSERT_NE(offset, std::string::npos);
  edits.push_back(styio::ide::TextEdit{styio::ide::TextRange{offset, offset + 5}, "a + b + 1"});
  final_text.replace(offset, 5, "a + b + 1");

  offset = final_text.rfind("}\n");
  ASSERT_NE(offset, std::string::npos);
  edits.push_back(styio::ide::TextEdit{styio::ide::TextRange{offset, offset}, "  \n"});
  final_text.insert(offset, "  \n");

  styio::ide::DocumentSnapshot incremental_snapshot = first_snapshot;
  incremental_snapshot.snapshot_id = 2;
  incremental_snapshot.version = 2;
  incremental_snapshot.buffer = styio::ide::TextBuffer{final_text};
  incremental_snapshot.applied_edits = edits;

  const auto incremental = incremental_parser.parse(incremental_snapshot);

  styio::ide::SyntaxParser full_parser;
  styio::ide::DocumentSnapshot full_snapshot = incremental_snapshot;
  full_snapshot.applied_edits.clear();
  const auto full = full_parser.parse(full_snapshot);

  ASSERT_EQ(incremental.tokens.size(), full.tokens.size());
  for (std::size_t i = 0; i < incremental.tokens.size(); ++i) {
    EXPECT_EQ(incremental.tokens[i].type, full.tokens[i].type);
    EXPECT_EQ(incremental.tokens[i].range.start, full.tokens[i].range.start);
    EXPECT_EQ(incremental.tokens[i].range.end, full.tokens[i].range.end);
  }
  EXPECT_EQ(incremental.diagnostics.size(), full.diagnostics.size());
  ASSERT_FALSE(incremental.nodes.empty());
  ASSERT_FALSE(full.nodes.empty());
  EXPECT_EQ(incremental.nodes.front().range.start, full.nodes.front().range.start);
  EXPECT_EQ(incremental.nodes.front().range.end, full.nodes.front().range.end);
}

TEST(StyioHirBuilder, BuildsStableTopLevelItems) {
  styio::ide::HirIdentityStore identity_store;
  const std::string path = make_temp_dir() + "/hir_items.styio";
  const std::string source =
    "# add := (a: i32, b: i32) => a + b\n"
    "result: i32 := add(1, 2)\n";

  const auto semantic = styio::ide::analyze_document(path, source);
  ASSERT_TRUE(semantic.parse_success);
  EXPECT_TRUE(std::any_of(
    semantic.items.begin(),
    semantic.items.end(),
    [](const styio::ide::SemanticItemFact& item)
    {
      return item.kind == styio::ide::SemanticItemKind::Function && item.name == "add";
    }));

  const auto module = build_hir_for_source(path, source, 21, 1, identity_store);

  EXPECT_TRUE(module.used_semantic_facts);
  EXPECT_EQ(module.module_id, 21u);

  const auto* add = find_hir_item(module, "add", styio::ide::HirItemKind::Function);
  ASSERT_NE(add, nullptr);
  EXPECT_NE(add->id, 0u);
  EXPECT_TRUE(add->canonical);
  EXPECT_TRUE(add->scope_id.has_value());
  EXPECT_NE(add->detail.find("add"), std::string::npos);

  const auto* result = find_hir_item(module, "result", styio::ide::HirItemKind::GlobalBinding);
  ASSERT_NE(result, nullptr);
  EXPECT_NE(result->id, 0u);
  EXPECT_TRUE(result->canonical);
  EXPECT_NE(result->id, add->id);

  const auto* add_symbol = find_hir_symbol(module, "add", styio::ide::SymbolKind::Function);
  ASSERT_NE(add_symbol, nullptr);
  ASSERT_TRUE(add_symbol->item_id.has_value());
  EXPECT_EQ(*add_symbol->item_id, add->id);

  styio::ide::SyntaxSnapshot empty_syntax;
  empty_syntax.file_id = 24;
  empty_syntax.snapshot_id = 1;
  empty_syntax.path = path + ".facts";
  empty_syntax.buffer = styio::ide::TextBuffer{""};

  styio::ide::SemanticSummary semantic_facts;
  semantic_facts.items.push_back(styio::ide::SemanticItemFact{
    styio::ide::SemanticItemKind::Import,
    "std/io",
    {},
    "import",
    "",
    0,
    true});
  semantic_facts.items.push_back(styio::ide::SemanticItemFact{
    styio::ide::SemanticItemKind::Resource,
    "input.csv",
    {},
    "resource",
    "csv",
    0,
    true});

  styio::ide::HirIdentityStore facts_identity_store;
  const auto fact_module = styio::ide::HirBuilder{}.build(empty_syntax, semantic_facts, facts_identity_store);
  EXPECT_NE(find_hir_item(fact_module, "std/io", styio::ide::HirItemKind::Import), nullptr);
  EXPECT_NE(find_hir_item(fact_module, "input.csv", styio::ide::HirItemKind::Resource), nullptr);
}

TEST(StyioHirBuilder, CanonicalizesAtImportPaths) {
  styio::ide::HirIdentityStore identity_store;
  const std::string path = make_temp_dir() + "/hir_imports.styio";
  const std::string source =
    "@import { std.io; tools/helpers, core }\n";

  const auto semantic = styio::ide::analyze_document(path, source);
  ASSERT_TRUE(semantic.parse_success);
  EXPECT_TRUE(std::any_of(
    semantic.items.begin(),
    semantic.items.end(),
    [](const styio::ide::SemanticItemFact& item)
    {
      return item.kind == styio::ide::SemanticItemKind::Import && item.name == "std/io";
    }));
  EXPECT_TRUE(std::any_of(
    semantic.items.begin(),
    semantic.items.end(),
    [](const styio::ide::SemanticItemFact& item)
    {
      return item.kind == styio::ide::SemanticItemKind::Import && item.name == "tools/helpers";
    }));
  EXPECT_TRUE(std::any_of(
    semantic.items.begin(),
    semantic.items.end(),
    [](const styio::ide::SemanticItemFact& item)
    {
      return item.kind == styio::ide::SemanticItemKind::Import && item.name == "core";
    }));

  const auto module = build_hir_for_source(path, source, 25, 1, identity_store);
  EXPECT_NE(find_hir_item(module, "std/io", styio::ide::HirItemKind::Import), nullptr);
  EXPECT_NE(find_hir_item(module, "tools/helpers", styio::ide::HirItemKind::Import), nullptr);
  EXPECT_NE(find_hir_item(module, "core", styio::ide::HirItemKind::Import), nullptr);
}

TEST(StyioHirBuilder, RetainsUnaffectedItemIdentityAcrossEdits) {
  styio::ide::HirIdentityStore identity_store;
  const std::string path = make_temp_dir() + "/hir_identity.styio";
  const std::string first_source =
    "# add := (a: i32, b: i32) => a + b\n"
    "# mul := (a: i32, b: i32) => a * b\n";
  const std::string second_source =
    "# add := (a: i32, b: i32) => a + b + 1\n"
    "# mul := (a: i32, b: i32) => a * b\n";

  const auto first = build_hir_for_source(path, first_source, 22, 1, identity_store);
  const auto second = build_hir_for_source(path, second_source, 22, 2, identity_store);

  const auto* first_add = find_hir_item(first, "add", styio::ide::HirItemKind::Function);
  const auto* first_mul = find_hir_item(first, "mul", styio::ide::HirItemKind::Function);
  const auto* second_add = find_hir_item(second, "add", styio::ide::HirItemKind::Function);
  const auto* second_mul = find_hir_item(second, "mul", styio::ide::HirItemKind::Function);

  ASSERT_NE(first_add, nullptr);
  ASSERT_NE(first_mul, nullptr);
  ASSERT_NE(second_add, nullptr);
  ASSERT_NE(second_mul, nullptr);
  EXPECT_EQ(second_mul->id, first_mul->id);
  EXPECT_EQ(second_add->id, first_add->id);
  EXPECT_NE(second_add->fingerprint, first_add->fingerprint);
  EXPECT_EQ(second_mul->fingerprint, first_mul->fingerprint);
}

TEST(StyioHirBuilder, ModelsNestedScopesAndBindings) {
  styio::ide::HirIdentityStore identity_store;
  const std::string path = make_temp_dir() + "/hir_scopes.styio";
  const std::string source =
    "# add := (a: i32, b: i32) => {\n"
    "  value: i32 := a + b\n"
    "  {\n"
    "    inner: i32 := value\n"
    "  }\n"
    "  <| value\n"
    "}\n";

  const auto module = build_hir_for_source(path, source, 23, 1, identity_store);
  const auto* add = find_hir_item(module, "add", styio::ide::HirItemKind::Function);
  ASSERT_NE(add, nullptr);
  ASSERT_TRUE(add->scope_id.has_value());

  const auto* param_a = find_hir_symbol(module, "a", styio::ide::SymbolKind::Parameter);
  ASSERT_NE(param_a, nullptr);
  EXPECT_EQ(param_a->scope_id, *add->scope_id);
  ASSERT_TRUE(param_a->item_id.has_value());
  EXPECT_EQ(*param_a->item_id, add->id);

  const auto* local_value = find_hir_symbol(module, "value", styio::ide::SymbolKind::Variable);
  ASSERT_NE(local_value, nullptr);
  EXPECT_NE(local_value->scope_id, 0u);

  const bool has_nested_block = std::any_of(
    module.scopes.begin(),
    module.scopes.end(),
    [&](const styio::ide::HirScope& scope)
    {
      return scope.kind == styio::ide::HirScopeKind::Block
        && scope.parent.has_value()
        && *scope.parent == *add->scope_id;
    });
  EXPECT_TRUE(has_nested_block);
}

TEST(StyioIdeService, UsesHirBackedDocumentSymbolsAndDefinition) {
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(make_temp_dir()));

  const std::string uri = temp_uri("hir_service_sample.styio");
  const std::string source =
    "# add := (a: i32, b: i32) => a + b\n"
    "result: i32 := add(1, 2)\n";

  const auto diagnostics = service.did_open(uri, source, 1);
  EXPECT_TRUE(diagnostics.empty());

  const auto symbols = service.document_symbols(uri);
  ASSERT_GE(symbols.size(), 2u);
  EXPECT_EQ(symbols[0].name, "add");
  EXPECT_EQ(symbols[1].name, "result");

  const auto definitions = service.definition(uri, styio::ide::Position{1, 16});
  ASSERT_EQ(definitions.size(), 1u);
  EXPECT_EQ(definitions[0].range.start, 2u);
}

TEST(StyioNameResolver, LocalBindingsShadowImportsAndGlobals) {
  const std::string root = make_temp_project_dir("m14_shadow");
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string uri = styio::ide::uri_from_path((std::filesystem::path(root) / "shadow.styio").string());
  const std::string source =
    "value: i32 := 1\n"
    "# echo := (value: i32) => value\n"
    "other: i32 := value\n";
  const auto diagnostics = service.did_open(uri, source, 1);
  EXPECT_TRUE(diagnostics.empty());

  styio::ide::TextBuffer buffer(source);
  const std::size_t global_offset = source.find("value");
  const std::size_t param_marker_offset = source.find("(value");
  const std::size_t local_ref_marker_offset = source.find("=> value");
  const std::size_t global_ref_offset = source.rfind("value");
  ASSERT_NE(global_offset, std::string::npos);
  ASSERT_NE(param_marker_offset, std::string::npos);
  ASSERT_NE(local_ref_marker_offset, std::string::npos);
  ASSERT_NE(global_ref_offset, std::string::npos);
  const std::size_t param_offset = param_marker_offset + 1;
  const std::size_t local_ref_offset = local_ref_marker_offset + 3;

  const auto local_definitions = service.definition(uri, buffer.position_at(local_ref_offset));
  ASSERT_EQ(local_definitions.size(), 1u);
  EXPECT_EQ(local_definitions[0].range.start, param_offset);

  const auto global_definitions = service.definition(uri, buffer.position_at(global_ref_offset));
  ASSERT_EQ(global_definitions.size(), 1u);
  EXPECT_EQ(global_definitions[0].range.start, global_offset);
}

TEST(StyioNameResolver, ResolvesImportsAcrossFiles) {
  const std::string root = make_temp_project_dir("m14_imports");
  const std::filesystem::path imported_file = std::filesystem::path(root) / "pkg" / "math.styio";
  std::filesystem::create_directories(imported_file.parent_path());
  const std::string imported_path = imported_file.string();
  const std::string imported_source =
    "# add := (a: i32, b: i32) => a + b\n";
  write_text_file(imported_path, imported_source);

  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string main_uri = styio::ide::uri_from_path((std::filesystem::path(root) / "main.styio").string());
  const std::string source =
    "@import { pkg.math }\n"
    "result: i32 := add(1, 2)\n";
  service.did_open(main_uri, source, 1);

  styio::ide::TextBuffer buffer(source);
  const std::size_t add_ref_offset = source.find("add(1");
  ASSERT_NE(add_ref_offset, std::string::npos);

  const auto definitions = service.definition(main_uri, buffer.position_at(add_ref_offset));
  ASSERT_EQ(definitions.size(), 1u);
  EXPECT_EQ(definitions[0].path, imported_path);
  EXPECT_EQ(definitions[0].range.start, imported_source.find("add"));

  const std::string incomplete_source =
    "@import { pkg.math }\n"
    "result: i32 := ad\n";
  service.did_change(main_uri, incomplete_source, 2);
  styio::ide::TextBuffer incomplete_buffer(incomplete_source);
  const auto completion = service.completion(main_uri, incomplete_buffer.position_at(incomplete_source.size()));
  EXPECT_TRUE(has_completion_label(completion, "add"));

  const std::string missing_uri = styio::ide::uri_from_path((std::filesystem::path(root) / "missing_main.styio").string());
  const std::string missing_source =
    "@import { pkg.missing }\n"
    "result: i32 := add(1, 2)\n";
  service.did_open(missing_uri, missing_source, 1);
  styio::ide::TextBuffer missing_buffer(missing_source);
  const std::size_t missing_add_offset = missing_source.find("add(1");
  ASSERT_NE(missing_add_offset, std::string::npos);
  EXPECT_TRUE(service.definition(missing_uri, missing_buffer.position_at(missing_add_offset)).empty());
}

TEST(StyioIdeService, ReferencesUseScopeAwareResolution) {
  const std::string root = make_temp_project_dir("m14_references");
  const std::string imported_path = (std::filesystem::path(root) / "lib.styio").string();
  const std::string imported_source =
    "# shared := (x: i32) => x\n";
  write_text_file(imported_path, imported_source);

  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string main_uri = styio::ide::uri_from_path((std::filesystem::path(root) / "main.styio").string());
  const std::string source =
    "@import { lib }\n"
    "result: i32 := shared(1)\n"
    "# use := (shared: i32) => shared\n";
  service.did_open(main_uri, source, 1);

  const std::string imported_uri = styio::ide::uri_from_path(imported_path);
  styio::ide::TextBuffer imported_buffer(imported_source);
  const std::size_t imported_shared_offset = imported_source.find("shared");
  ASSERT_NE(imported_shared_offset, std::string::npos);

  const auto references = service.references(imported_uri, imported_buffer.position_at(imported_shared_offset));
  ASSERT_EQ(references.size(), 1u);
  EXPECT_EQ(references[0].path, styio::ide::path_from_uri(main_uri));
  EXPECT_EQ(references[0].range.start, source.find("shared(1"));
}

TEST(StyioIdeService, DefinitionAndHoverUseResolvedSymbols) {
  const std::string root = make_temp_project_dir("m14_hover_definition");
  const std::string imported_path = (std::filesystem::path(root) / "math.styio").string();
  const std::string imported_source =
    "# add := (a: i32, b: i32) => a + b\n";
  write_text_file(imported_path, imported_source);

  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string main_uri = styio::ide::uri_from_path((std::filesystem::path(root) / "main.styio").string());
  const std::string source =
    "@import { math }\n"
    "result: i32 := add(1, 2)\n";
  service.did_open(main_uri, source, 1);

  styio::ide::TextBuffer buffer(source);
  const std::size_t add_ref_offset = source.find("add(1");
  ASSERT_NE(add_ref_offset, std::string::npos);

  const auto definitions = service.definition(main_uri, buffer.position_at(add_ref_offset));
  ASSERT_EQ(definitions.size(), 1u);
  EXPECT_EQ(definitions[0].path, imported_path);
  EXPECT_EQ(definitions[0].range.start, imported_source.find("add"));

  const auto hover = service.hover(main_uri, buffer.position_at(add_ref_offset));
  ASSERT_TRUE(hover.has_value());
  EXPECT_NE(hover->contents.find("add("), std::string::npos);
  EXPECT_EQ(hover->range->start, imported_source.find("add"));
}

TEST(StyioTypeInference, SeparatesSignatureAndBodyQueries) {
  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  styio::ide::SemanticDB semdb(vfs, project);
  const std::string path = make_temp_dir() + "/type_signature_body.styio";
  const std::string source =
    "# add := (a: i32, b: i32) => a + b\n";

  vfs.open(path, source, 1);
  semdb.reset_query_stats();

  const std::size_t add_offset = source.find("add");
  const std::size_t body_offset = source.find("a + b");
  ASSERT_NE(add_offset, std::string::npos);
  ASSERT_NE(body_offset, std::string::npos);

  const auto signature = semdb.type_signature_at(path, add_offset);
  ASSERT_TRUE(signature.has_value());
  ASSERT_EQ(signature->params.size(), 2u);
  EXPECT_EQ(signature->params[0].name, "a");
  EXPECT_EQ(signature->params[0].type_name, "i32");

  const auto body = semdb.type_body_at(path, body_offset);
  ASSERT_TRUE(body.has_value());
  EXPECT_EQ(body->signature_fingerprint, signature->signature_fingerprint);
  EXPECT_EQ(body->signature_identity_key, signature->identity_key);

  const auto after_first = semdb.query_stats();
  EXPECT_EQ(after_first.type_signature.misses, 1u);
  EXPECT_EQ(after_first.type_body.misses, 1u);
  EXPECT_GE(after_first.type_signature.hits, 1u);

  const auto signature_again = semdb.type_signature_at(path, add_offset);
  const auto body_again = semdb.type_body_at(path, body_offset);
  ASSERT_TRUE(signature_again.has_value());
  ASSERT_TRUE(body_again.has_value());
  EXPECT_EQ(signature_again->identity_key, signature->identity_key);
  EXPECT_EQ(body_again->body_fingerprint, body->body_fingerprint);

  const auto after_second = semdb.query_stats();
  EXPECT_EQ(after_second.type_signature.misses, after_first.type_signature.misses);
  EXPECT_EQ(after_second.type_body.misses, after_first.type_body.misses);
  EXPECT_GT(after_second.type_signature.hits, after_first.type_signature.hits);
  EXPECT_GT(after_second.type_body.hits, after_first.type_body.hits);
}

TEST(StyioTypeInference, InvalidatesOnlyEditedFunctionBody) {
  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  styio::ide::SemanticDB semdb(vfs, project);
  const std::string path = make_temp_dir() + "/type_item_invalidation.styio";
  const std::string first_source =
    "# stable := (x: i32) => x\n"
    "# edited := (x: i32) => x\n";
  const std::string second_source =
    "# stable := (x: i32) => x\n"
    "# edited := (x: i32) => x + 1\n";

  vfs.open(path, first_source, 1);
  semdb.reset_query_stats();

  const std::size_t first_stable_marker = first_source.find("=> x");
  const std::size_t first_edited_marker = first_source.rfind("=> x");
  ASSERT_NE(first_stable_marker, std::string::npos);
  ASSERT_NE(first_edited_marker, std::string::npos);
  const std::size_t first_stable_offset = first_stable_marker + 3;
  const std::size_t first_edited_offset = first_edited_marker + 3;
  const auto first_stable = semdb.type_body_at(path, first_stable_offset);
  const auto first_edited = semdb.type_body_at(path, first_edited_offset);
  ASSERT_TRUE(first_stable.has_value());
  ASSERT_TRUE(first_edited.has_value());
  EXPECT_NE(first_stable->item_id, first_edited->item_id);

  const auto after_initial = semdb.query_stats();
  EXPECT_EQ(after_initial.type_body.misses, 2u);

  vfs.update(path, second_source, 2);
  const std::size_t second_stable_marker = second_source.find("=> x");
  const std::size_t second_edited_marker = second_source.rfind("=> x");
  ASSERT_NE(second_stable_marker, std::string::npos);
  ASSERT_NE(second_edited_marker, std::string::npos);
  const std::size_t second_stable_offset = second_stable_marker + 3;
  const std::size_t second_edited_offset = second_edited_marker + 3;
  const auto second_stable = semdb.type_body_at(path, second_stable_offset);
  const auto second_edited = semdb.type_body_at(path, second_edited_offset);
  ASSERT_TRUE(second_stable.has_value());
  ASSERT_TRUE(second_edited.has_value());

  EXPECT_EQ(second_stable->body_fingerprint, first_stable->body_fingerprint);
  EXPECT_NE(second_edited->body_fingerprint, first_edited->body_fingerprint);

  const auto after_edit = semdb.query_stats();
  EXPECT_EQ(after_edit.type_body.hits, after_initial.type_body.hits + 1);
  EXPECT_EQ(after_edit.type_body.misses, after_initial.type_body.misses + 1);
}

TEST(StyioIdeService, ExposesReceiverTypesForMembers) {
  const std::string root = make_temp_project_dir("m15_receiver_type");
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string uri = styio::ide::uri_from_path((std::filesystem::path(root) / "receiver.styio").string());
  const std::string source =
    "items: list[i32] := [1, 2]\n"
    "result: i32 := items.len\n";
  service.did_open(uri, source, 1);

  styio::ide::TextBuffer buffer(source);
  const std::size_t member_offset = source.find("len");
  ASSERT_NE(member_offset, std::string::npos);

  const auto context = service.completion_context(uri, buffer.position_at(member_offset));
  EXPECT_EQ(context.position_kind, styio::ide::PositionKind::MemberAccess);
  EXPECT_EQ(context.receiver_type_name, "list[i32]");
  EXPECT_NE(context.receiver_type_id, 0u);

  const auto hover = service.hover(uri, buffer.position_at(member_offset));
  ASSERT_TRUE(hover.has_value());
  EXPECT_NE(hover->contents.find("Receiver: `list[i32]`"), std::string::npos);
}

TEST(StyioIdeService, ExposesCallSiteExpectedTypes) {
  const std::string root = make_temp_project_dir("m15_expected_type");
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string uri = styio::ide::uri_from_path((std::filesystem::path(root) / "expected.styio").string());
  const std::string source =
    "# add := (a: i32, b: string) => a\n"
    "result: i32 := add(1, \"text\")\n";
  service.did_open(uri, source, 1);

  styio::ide::TextBuffer buffer(source);
  const std::size_t argument_offset = source.find("\"text\"");
  ASSERT_NE(argument_offset, std::string::npos);

  const auto context = service.completion_context(uri, buffer.position_at(argument_offset + 1));
  EXPECT_EQ(context.expected_type_name, "string");
  EXPECT_EQ(context.expected_param_name, "b");
  EXPECT_EQ(context.argument_index, 1u);
}

TEST(StyioCompletionEngine, FiltersTypePositionCandidates) {
  const std::string root = make_temp_project_dir("m16_type_position");
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string uri = styio::ide::uri_from_path((std::filesystem::path(root) / "types.styio").string());
  const std::string source =
    "value: i32 := 1\n"
    "answer: i";
  service.did_open(uri, source, 1);

  styio::ide::TextBuffer buffer(source);
  const auto completion = service.completion(uri, buffer.position_at(source.size()));

  EXPECT_TRUE(has_completion_label(completion, "i32"));
  EXPECT_TRUE(has_completion_label(completion, "i64"));
  EXPECT_FALSE(has_completion_label(completion, "value"));
  EXPECT_FALSE(has_completion_label(completion, "true"));
}

TEST(StyioCompletionEngine, RanksLocalsAboveImportsAndBuiltins) {
  const std::string root = make_temp_project_dir("m16_ranking");
  const std::string imported_path = (std::filesystem::path(root) / "lib.styio").string();
  write_text_file(imported_path, "# stable := (x: i32) => x\n");

  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string uri = styio::ide::uri_from_path((std::filesystem::path(root) / "main.styio").string());
  const std::string source =
    "@import { lib }\n"
    "status: i32 := 1\n"
    "# use := (stream: i32, shared: i32) => {\n"
    "  sh\n"
    "  st\n"
    "}\n";
  service.did_open(uri, source, 1);

  styio::ide::TextBuffer buffer(source);
  const std::size_t local_shadow_offset = source.find("sh\n");
  ASSERT_NE(local_shadow_offset, std::string::npos);
  const auto shadow_completion = service.completion(uri, buffer.position_at(local_shadow_offset + 2));
  ASSERT_FALSE(shadow_completion.empty());
  EXPECT_EQ(shadow_completion.front().label, "shared");
  EXPECT_EQ(shadow_completion.front().detail, "parameter");

  const std::size_t stream_offset = source.find("st\n");
  ASSERT_NE(stream_offset, std::string::npos);
  const auto ranked_completion = service.completion(uri, buffer.position_at(stream_offset + 2));
  ASSERT_FALSE(ranked_completion.empty());
  EXPECT_EQ(ranked_completion.front().label, "stream");
  EXPECT_LT(completion_index(ranked_completion, "stable"), completion_index(ranked_completion, "stdin"));
  EXPECT_LT(completion_index(ranked_completion, "status"), completion_index(ranked_completion, "stable"));
}

TEST(StyioCompletionEngine, FiltersMembersByReceiverType) {
  const std::string root = make_temp_project_dir("m16_member_filter");
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string uri = styio::ide::uri_from_path((std::filesystem::path(root) / "members.styio").string());
  const std::string source =
    "items: list[i32] := [1, 2]\n"
    "result: i32 := items.";
  service.did_open(uri, source, 1);

  styio::ide::TextBuffer buffer(source);
  const auto completion = service.completion(uri, buffer.position_at(source.size()));

  EXPECT_TRUE(has_completion_label(completion, "len"));
  EXPECT_TRUE(has_completion_label(completion, "first"));
  EXPECT_TRUE(has_completion_label(completion, "last"));
  EXPECT_FALSE(has_completion_label(completion, "keys"));
  EXPECT_FALSE(has_completion_label(completion, "values"));
}

TEST(StyioCompletionEngine, UsesExpectedTypesAtCallSites) {
  const std::string root = make_temp_project_dir("m16_expected_type_completion");
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string uri = styio::ide::uri_from_path((std::filesystem::path(root) / "expected_completion.styio").string());
  const std::string source =
    "word: string := \"hi\"\n"
    "count: i32 := 1\n"
    "# take := (value: string) => value\n"
    "result: string := take()";
  service.did_open(uri, source, 1);

  styio::ide::TextBuffer buffer(source);
  const std::size_t call_offset = source.find("take(");
  ASSERT_NE(call_offset, std::string::npos);
  const auto completion = service.completion(uri, buffer.position_at(call_offset + 5));

  EXPECT_LT(completion_index(completion, "word"), completion_index(completion, "count"));
  ASSERT_LT(completion_index(completion, "word"), completion.size());
  EXPECT_EQ(completion[completion_index(completion, "word")].type_name, "string");
}

TEST(StyioCompletionEngine, RecoversInBrokenSyntax) {
  const std::string root = make_temp_project_dir("m16_recovery");
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string uri = styio::ide::uri_from_path((std::filesystem::path(root) / "broken.styio").string());
  const std::string source =
    "# add := (a: i32, b: i32) => a + b\n"
    "# broken := (x: i32 => {\n"
    "result: i32 := ad";
  service.did_open(uri, source, 1);

  styio::ide::TextBuffer buffer(source);
  const auto completion = service.completion(uri, buffer.position_at(source.size()));

  EXPECT_TRUE(has_completion_label(completion, "add"));
}

TEST(StyioWorkspaceIndex, WorkspaceSymbolSearchIncludesBackgroundIndexedFiles) {
  const std::string root = make_temp_project_dir("m17_workspace_symbols");
  const std::string indexed_path = (std::filesystem::path(root) / "indexed.styio").string();
  write_text_file(indexed_path, "# background_only := (x: i32) => x\n");

  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const auto background_symbols = service.workspace_symbols("background_only");
  EXPECT_TRUE(has_indexed_symbol(background_symbols, "background_only", indexed_path));

  const std::string indexed_uri = styio::ide::uri_from_path(indexed_path);
  service.did_open(indexed_uri, "# fresh_only := (x: i32) => x\n", 2);

  const auto stale_symbols = service.workspace_symbols("background_only");
  EXPECT_FALSE(has_indexed_symbol(stale_symbols, "background_only", indexed_path));
  const auto fresh_symbols = service.workspace_symbols("fresh_only");
  EXPECT_TRUE(has_indexed_symbol(fresh_symbols, "fresh_only", indexed_path));
}

TEST(StyioIdeService, DefinitionUsesWorkspaceIndexAcrossFiles) {
  const std::string root = make_temp_project_dir("m17_index_definition");
  const std::string owner_path = (std::filesystem::path(root) / "owner.styio").string();
  const std::string owner_source = "# indexed_target := (x: i32) => x\n";
  write_text_file(owner_path, owner_source);

  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string main_uri = styio::ide::uri_from_path((std::filesystem::path(root) / "main.styio").string());
  const std::string main_source = "result: i32 := indexed_target(1)\n";
  service.did_open(main_uri, main_source, 1);

  styio::ide::TextBuffer main_buffer(main_source);
  const std::size_t target_offset = main_source.find("indexed_target");
  ASSERT_NE(target_offset, std::string::npos);
  const auto definitions = service.definition(main_uri, main_buffer.position_at(target_offset));
  ASSERT_EQ(definitions.size(), 1u);
  EXPECT_EQ(definitions[0].path, owner_path);
  EXPECT_EQ(definitions[0].range.start, owner_source.find("indexed_target"));

  const std::string owner_uri = styio::ide::uri_from_path(owner_path);
  const std::string edited_owner_source = "# edited_target := (x: i32) => x\n";
  service.did_open(owner_uri, edited_owner_source, 2);
  const std::string edited_main_source = "result: i32 := edited_target(1)\n";
  service.did_change(main_uri, edited_main_source, 2);

  styio::ide::TextBuffer edited_main_buffer(edited_main_source);
  const std::size_t edited_target_offset = edited_main_source.find("edited_target");
  ASSERT_NE(edited_target_offset, std::string::npos);
  const auto edited_definitions = service.definition(main_uri, edited_main_buffer.position_at(edited_target_offset));
  ASSERT_EQ(edited_definitions.size(), 1u);
  EXPECT_EQ(edited_definitions[0].path, owner_path);
  EXPECT_EQ(edited_definitions[0].range.start, edited_owner_source.find("edited_target"));
  EXPECT_FALSE(has_indexed_symbol(service.workspace_symbols("indexed_target"), "indexed_target", owner_path));
}

TEST(StyioIdeService, ReferencesMergeOpenFileAndBackgroundIndex) {
  const std::string root = make_temp_project_dir("m17_index_references");
  const std::string owner_path = (std::filesystem::path(root) / "owner.styio").string();
  const std::string disk_user_path = (std::filesystem::path(root) / "disk_user.styio").string();
  const std::string owner_source = "# indexed_target := (x: i32) => x\n";
  const std::string disk_user_source = "disk_result: i32 := indexed_target(1)\n";
  write_text_file(owner_path, owner_source);
  write_text_file(disk_user_path, disk_user_source);

  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string open_user_uri = styio::ide::uri_from_path((std::filesystem::path(root) / "open_user.styio").string());
  const std::string open_user_path = styio::ide::path_from_uri(open_user_uri);
  const std::string open_user_source = "open_result: i32 := indexed_target(2)\n";
  service.did_open(open_user_uri, open_user_source, 1);

  const std::string owner_uri = styio::ide::uri_from_path(owner_path);
  styio::ide::TextBuffer owner_buffer(owner_source);
  const std::size_t owner_offset = owner_source.find("indexed_target");
  ASSERT_NE(owner_offset, std::string::npos);
  const auto references = service.references(owner_uri, owner_buffer.position_at(owner_offset));

  ASSERT_EQ(references.size(), 2u);
  EXPECT_TRUE(has_location(references, disk_user_path, disk_user_source.find("indexed_target")));
  EXPECT_TRUE(has_location(references, open_user_path, open_user_source.find("indexed_target")));
}

TEST(StyioWorkspaceIndex, PersistentIndexClearsDeletedSymbolsOnNewSession) {
  const std::string root = make_temp_project_dir("m17_persistent_index");
  const std::string path = (std::filesystem::path(root) / "persisted.styio").string();
  const std::string source = "# persisted_symbol := (x: i32) => x\n";
  write_text_file(path, source);

  {
    styio::ide::IdeService service;
    service.initialize(styio::ide::uri_from_path(root));
    EXPECT_TRUE(has_indexed_symbol(service.workspace_symbols("persisted_symbol"), "persisted_symbol", path));
  }

  std::filesystem::remove(path);
  styio::ide::IdeService warmed_service;
  warmed_service.initialize(styio::ide::uri_from_path(root));
  EXPECT_FALSE(has_indexed_symbol(warmed_service.workspace_symbols("persisted_symbol"), "persisted_symbol", path));

  const std::string uri = styio::ide::uri_from_path(path);
  warmed_service.did_open(uri, "# live_symbol := (x: i32) => x\n", 2);
  EXPECT_TRUE(has_indexed_symbol(warmed_service.workspace_symbols("live_symbol"), "live_symbol", path));
}

TEST(StyioWorkspaceIndex, ClosedFileRefreshesFromDiskBeforeBackgroundIndexing) {
  const std::string root = make_temp_project_dir("m17_closed_file_refresh");
  const std::string path = (std::filesystem::path(root) / "refresh.styio").string();
  write_text_file(path, "# old_symbol := (x: i32) => x\n");

  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));
  ASSERT_TRUE(has_indexed_symbol(service.workspace_symbols("old_symbol"), "old_symbol", path));

  write_text_file(path, "# new_symbol := (x: i32) => x\n");
  service.schedule_background_index_refresh();
  ASSERT_GT(service.pending_background_task_count(), 0u);
  EXPECT_EQ(service.run_background_tasks(1), 1u);

  const auto symbols = service.workspace_symbols("new_symbol");
  EXPECT_FALSE(has_indexed_symbol(symbols, "old_symbol", path));
  EXPECT_TRUE(has_indexed_symbol(symbols, "new_symbol", path));
}

TEST(StyioSemanticDb, ReusesFileQueriesWithinSnapshot) {
  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  styio::ide::SemanticDB semdb(vfs, project);
  const std::string path = make_temp_dir() + "/semantic_file_cache.styio";
  const std::string source =
    "# add := (a: i32, b: i32) => a + b\n"
    "result: i32 := add(1, 2)\n";

  write_text_file(path, source);
  vfs.open(path, source, 1);
  semdb.reset_query_stats();

  const auto first_symbols = semdb.document_symbols(path);
  ASSERT_FALSE(first_symbols.empty());
  const auto after_first_symbols = semdb.query_stats();

  const auto second_symbols = semdb.document_symbols(path);
  EXPECT_EQ(second_symbols.size(), first_symbols.size());
  const auto after_second_symbols = semdb.query_stats();

  EXPECT_EQ(after_first_symbols.document_symbols.misses, 1u);
  EXPECT_EQ(after_second_symbols.document_symbols.hits, 1u);
  EXPECT_EQ(after_second_symbols.syntax_tree.misses, after_first_symbols.syntax_tree.misses);
  EXPECT_EQ(after_second_symbols.semantic_summary.misses, after_first_symbols.semantic_summary.misses);
  EXPECT_EQ(after_second_symbols.hir_module.misses, after_first_symbols.hir_module.misses);

  const auto first_tokens = semdb.semantic_tokens_for(path);
  ASSERT_FALSE(first_tokens.empty());
  const auto after_first_tokens = semdb.query_stats();

  const auto second_tokens = semdb.semantic_tokens_for(path);
  EXPECT_EQ(second_tokens, first_tokens);
  const auto after_second_tokens = semdb.query_stats();

  EXPECT_EQ(after_first_tokens.semantic_tokens.misses, 1u);
  EXPECT_EQ(after_second_tokens.semantic_tokens.hits, 1u);
  EXPECT_EQ(after_second_tokens.syntax_tree.misses, after_first_tokens.syntax_tree.misses);
  EXPECT_EQ(after_second_tokens.semantic_summary.misses, after_first_tokens.semantic_summary.misses);
  EXPECT_EQ(after_second_tokens.hir_module.misses, after_first_tokens.hir_module.misses);
}

TEST(StyioSemanticDb, ReusesOffsetQueriesWithinSnapshot) {
  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  styio::ide::SemanticDB semdb(vfs, project);
  const std::string path = make_temp_dir() + "/semantic_offset_cache.styio";
  const std::string source =
    "# add := (a: i32, b: i32) => a + b\n"
    "result: i32 := ad\n";

  vfs.open(path, source, 1);
  semdb.reset_query_stats();

  const std::size_t hover_offset = source.find("add");
  ASSERT_NE(hover_offset, std::string::npos);
  const auto first_hover = semdb.hover_at(path, hover_offset);
  ASSERT_TRUE(first_hover.has_value());
  const auto after_first_hover = semdb.query_stats();

  const auto second_hover = semdb.hover_at(path, hover_offset);
  ASSERT_TRUE(second_hover.has_value());
  EXPECT_EQ(second_hover->contents, first_hover->contents);
  const auto after_second_hover = semdb.query_stats();

  EXPECT_EQ(after_first_hover.hover.misses, 1u);
  EXPECT_EQ(after_second_hover.hover.hits, 1u);
  EXPECT_EQ(after_second_hover.syntax_tree.misses, after_first_hover.syntax_tree.misses);
  EXPECT_EQ(after_second_hover.semantic_summary.misses, after_first_hover.semantic_summary.misses);
  EXPECT_EQ(after_second_hover.hir_module.misses, after_first_hover.hir_module.misses);

  const std::size_t completion_offset = source.rfind("ad");
  ASSERT_NE(completion_offset, std::string::npos);
  const auto first_completion = semdb.complete_at(path, completion_offset + 2);
  ASSERT_FALSE(first_completion.empty());
  const auto after_first_completion = semdb.query_stats();

  const auto second_completion = semdb.complete_at(path, completion_offset + 2);
  EXPECT_EQ(second_completion.size(), first_completion.size());
  const auto after_second_completion = semdb.query_stats();

  EXPECT_EQ(after_first_completion.completion.misses, 1u);
  EXPECT_EQ(after_second_completion.completion.hits, 1u);
  EXPECT_EQ(after_second_completion.syntax_tree.misses, after_first_completion.syntax_tree.misses);
  EXPECT_EQ(after_second_completion.semantic_summary.misses, after_first_completion.semantic_summary.misses);
  EXPECT_EQ(after_second_completion.hir_module.misses, after_first_completion.hir_module.misses);
}

TEST(StyioSemanticDb, InvalidatesQueriesAcrossSnapshots) {
  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  styio::ide::SemanticDB semdb(vfs, project);
  const std::string path = make_temp_dir() + "/semantic_invalidation.styio";
  const std::string first_source =
    "# ad := (a: i32) => a\n"
    "result: i32 := a\n";
  const std::string second_source =
    "# ax := (a: i32) => a\n"
    "result: i32 := a\n";

  vfs.open(path, first_source, 1);
  semdb.reset_query_stats();

  const auto first_symbols = semdb.document_symbols(path);
  ASSERT_FALSE(first_symbols.empty());
  EXPECT_EQ(first_symbols.front().name, "ad");
  const std::size_t completion_offset = first_source.rfind("a");
  ASSERT_NE(completion_offset, std::string::npos);
  const auto first_completion = semdb.complete_at(path, completion_offset + 1);
  EXPECT_TRUE(has_completion_label(first_completion, "ad"));
  const auto after_first_queries = semdb.query_stats();

  vfs.update(path, second_source, 2);
  const auto second_symbols = semdb.document_symbols(path);
  ASSERT_FALSE(second_symbols.empty());
  EXPECT_EQ(second_symbols.front().name, "ax");
  const auto second_completion = semdb.complete_at(path, completion_offset + 1);
  EXPECT_TRUE(has_completion_label(second_completion, "ax"));
  EXPECT_FALSE(has_completion_label(second_completion, "ad"));
  const auto after_second_queries = semdb.query_stats();

  EXPECT_EQ(after_second_queries.document_symbols.misses, after_first_queries.document_symbols.misses + 1);
  EXPECT_EQ(after_second_queries.completion.misses, after_first_queries.completion.misses + 1);
  EXPECT_EQ(after_second_queries.document_symbols.hits, after_first_queries.document_symbols.hits);
  EXPECT_EQ(after_second_queries.completion.hits, after_first_queries.completion.hits);
}

TEST(StyioSemanticDb, DropsOpenFileQueryStateOnClose) {
  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  styio::ide::SemanticDB semdb(vfs, project);
  const std::string path = make_temp_dir() + "/semantic_close_cache.styio";
  const std::string source =
    "# add := (a: i32, b: i32) => a + b\n"
    "result: i32 := add(1, 2)\n";

  write_text_file(path, source);
  vfs.open(path, source, 1);
  semdb.reset_query_stats();

  const auto first_symbols = semdb.document_symbols(path);
  ASSERT_FALSE(first_symbols.empty());
  const auto second_symbols = semdb.document_symbols(path);
  EXPECT_EQ(second_symbols.size(), first_symbols.size());
  const auto after_cached_query = semdb.query_stats();
  EXPECT_EQ(after_cached_query.document_symbols.misses, 1u);
  EXPECT_EQ(after_cached_query.document_symbols.hits, 1u);

  vfs.close(path);
  semdb.drop_open_file(path);
  const auto closed_symbols = semdb.document_symbols(path);
  EXPECT_EQ(closed_symbols.size(), first_symbols.size());
  const auto after_close_query = semdb.query_stats();
  EXPECT_EQ(after_close_query.document_symbols.misses, after_cached_query.document_symbols.misses + 1);
  EXPECT_EQ(after_close_query.document_symbols.hits, after_cached_query.document_symbols.hits);

  vfs.open(path, source, 2);
  const auto reopened_symbols = semdb.document_symbols(path);
  EXPECT_EQ(reopened_symbols.size(), first_symbols.size());
  const auto after_reopen_query = semdb.query_stats();
  EXPECT_EQ(after_reopen_query.document_symbols.misses, after_close_query.document_symbols.misses + 1);
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

TEST(StyioLspServer, SkipsMalformedFramesAndHandlesLargeStringIds) {
  styio::lsp::Server server;
  const std::string root_uri = styio::ide::uri_from_path(make_temp_dir());
  const std::string uri = temp_uri("server_malformed_frame.styio");

  const llvm::json::Object initialize_request{
    {"jsonrpc", "2.0"},
    {"id", "9999999999999999999999999999999999999999"},
    {"method", "initialize"},
    {"params", llvm::json::Object{{"rootUri", root_uri}}}};

  std::string input_text = "Content-Length: not-a-number\r\n\r\n";
  input_text += lsp_frame(initialize_request);

  std::istringstream input(input_text);
  std::ostringstream output;
  server.run(input, output);

  EXPECT_NE(output.str().find("\"capabilities\""), std::string::npos);

  auto open_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didOpen"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{
          {"uri", uri},
          {"version", 1},
          {"text", "# add := (a: i32, b: i32) => a + b\nresult: i32 := ad\n"}}}}}});
  ASSERT_EQ(open_messages.size(), 1u);
  EXPECT_EQ(open_messages[0].payload.getString("method").value_or(""), "textDocument/publishDiagnostics");
}

TEST(StyioLspServer, AppliesMultipleIncrementalChangesInOrder) {
  styio::lsp::Server server;
  const std::string root_uri = styio::ide::uri_from_path(make_temp_dir());
  const std::string uri = temp_uri("server_multi_edit_sample.styio");

  auto init_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 1},
    {"method", "initialize"},
    {"params", llvm::json::Object{{"rootUri", root_uri}}}});
  ASSERT_EQ(init_messages.size(), 1u);
  const auto* init_result = init_messages[0].payload.getObject("result");
  ASSERT_NE(init_result, nullptr);
  const auto* capabilities = init_result->getObject("capabilities");
  ASSERT_NE(capabilities, nullptr);
  const auto* sync = capabilities->getObject("textDocumentSync");
  ASSERT_NE(sync, nullptr);
  EXPECT_EQ(sync->getInteger("change").value_or(0), 2);

  auto open_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didOpen"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{
          {"uri", uri},
          {"version", 1},
          {"text", "# ad := (a: i32, b: i32) => a + b\nresult: i32 := ad\n"}}}}}});
  ASSERT_EQ(open_messages.size(), 1u);

  llvm::json::Array changes;
  changes.push_back(llvm::json::Object{
    {"range", lsp_range(0, 4, 0, 4)},
    {"text", "d"}});
  changes.push_back(llvm::json::Object{
    {"range", lsp_range(1, 17, 1, 17)},
    {"text", "d"}});

  auto change_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didChange"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}, {"version", 2}}},
       {"contentChanges", std::move(changes)}}}});
  ASSERT_EQ(change_messages.size(), 1u);
  EXPECT_EQ(change_messages[0].payload.getString("method").value_or(""), "textDocument/publishDiagnostics");

  auto symbol_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 2},
    {"method", "textDocument/documentSymbol"},
    {"params", llvm::json::Object{{"textDocument", llvm::json::Object{{"uri", uri}}}}}});
  ASSERT_EQ(symbol_messages.size(), 1u);
  const auto* symbols = symbol_messages[0].payload.getArray("result");
  ASSERT_NE(symbols, nullptr);
  ASSERT_FALSE(symbols->empty());
  const auto* first_symbol = (*symbols)[0].getAsObject();
  ASSERT_NE(first_symbol, nullptr);
  EXPECT_EQ(first_symbol->getString("name").value_or(""), "add");
}

TEST(StyioLspRuntime, DropsStaleCompletionResponses) {
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(make_temp_dir()));

  const std::string uri = temp_uri("runtime_stale_completion.styio");
  service.did_open(
    uri,
    "# add := (a: i32, b: i32) => a + b\n"
    "result: i32 := ad\n",
    1);
  service.drain_semantic_diagnostics();
  service.reset_runtime_counters();

  const auto stale_ticket = service.begin_foreground_request(uri, styio::ide::RuntimeRequestKind::Completion, 41);
  service.did_change(
    uri,
    "# sum := (a: i32, b: i32) => a + b\n"
    "result: i32 := su\n",
    2);

  const auto stale_completion = service.completion(stale_ticket, styio::ide::Position{1, 16});
  EXPECT_TRUE(stale_completion.empty());

  const auto fresh_completion = service.completion(uri, styio::ide::Position{1, 16});
  EXPECT_TRUE(has_completion_label(fresh_completion, "sum"));
  EXPECT_EQ(service.runtime_counters().stale_request_drops, 1u);
}

TEST(StyioLspRuntime, DebouncesSemanticDiagnostics) {
  styio::lsp::Server server;
  const std::string root_uri = styio::ide::uri_from_path(make_temp_dir());
  const std::string uri = temp_uri("runtime_debounce.styio");

  server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 1},
    {"method", "initialize"},
    {"params", llvm::json::Object{{"rootUri", root_uri}}}});

  auto open_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didOpen"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{
          {"uri", uri},
          {"version", 1},
          {"text", "# add := (a: i32, b: i32) => a + b\nresult: i32 := add(1, 2)\n"}}}}}});
  ASSERT_EQ(open_messages.size(), 1u);

  llvm::json::Array invalid_changes;
  invalid_changes.push_back(llvm::json::Object{
    {"text", "# add := (a: i32, b: i32) => {\nresult: i32 := add(1, 2)\n"}});
  auto invalid_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didChange"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}, {"version", 2}}},
       {"contentChanges", std::move(invalid_changes)}}}});
  ASSERT_EQ(invalid_messages.size(), 1u);

  llvm::json::Array invalid_changes_2;
  invalid_changes_2.push_back(llvm::json::Object{
    {"text", "# add := (a: i32, b: i32) => a +\nresult: i32 := add(1, 2)\n"}});
  auto invalid_messages_2 = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didChange"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}, {"version", 3}}},
       {"contentChanges", std::move(invalid_changes_2)}}}});
  ASSERT_EQ(invalid_messages_2.size(), 1u);

  llvm::json::Array final_changes;
  final_changes.push_back(llvm::json::Object{
    {"text", "# sum := (a: i32, b: i32) => a + b\nresult: i32 := sum(1, 2)\n"}});
  auto final_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didChange"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}, {"version", 4}}},
       {"contentChanges", std::move(final_changes)}}}});
  ASSERT_EQ(final_messages.size(), 1u);

  const auto runtime_messages = server.drain_runtime();
  ASSERT_EQ(runtime_messages.size(), 1u);
  EXPECT_EQ(runtime_messages[0].payload.getString("method").value_or(""), "textDocument/publishDiagnostics");
  const auto* params = runtime_messages[0].payload.getObject("params");
  ASSERT_NE(params, nullptr);
  const auto* diagnostics = params->getArray("diagnostics");
  ASSERT_NE(diagnostics, nullptr);
  EXPECT_TRUE(diagnostics->empty());
  EXPECT_EQ(server.runtime_counters().semantic_diagnostic_runs, 1u);
  EXPECT_EQ(server.runtime_counters().semantic_diagnostic_debounces, 3u);
}

TEST(StyioLspRuntime, RuntimeDrainCanBeBudgetedForScheduling) {
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(make_temp_dir()));

  const std::string uri_a = temp_uri("runtime_schedule_a.styio");
  const std::string uri_b = temp_uri("runtime_schedule_b.styio");
  service.did_open(
    uri_a,
    "# add := (a: i32, b: i32) => a + b\n"
    "result: i32 := add(1, 2)\n",
    1);
  service.did_open(
    uri_b,
    "# other := (x: i32) => x\n"
    "result: i32 := other(1)\n",
    1);

  auto first_batch = service.drain_semantic_diagnostics(1);
  ASSERT_EQ(first_batch.size(), 1u);
  EXPECT_EQ(first_batch.front().snapshot->path, styio::ide::path_from_uri(uri_a));

  auto second_batch = service.drain_semantic_diagnostics(1);
  ASSERT_EQ(second_batch.size(), 1u);
  EXPECT_EQ(second_batch.front().snapshot->path, styio::ide::path_from_uri(uri_b));

  EXPECT_EQ(service.drain_semantic_diagnostics(1).size(), 0u);

  service.did_change(
    uri_a,
    "# add := (a: i32, b: i32) => a +\n"
    "result: i32 := add(1, 2)\n",
    2);
  service.did_change(
    uri_a,
    "# sum := (a: i32, b: i32) => a + b\n"
    "result: i32 := sum(1, 2)\n",
    3);

  auto refreshed = service.drain_semantic_diagnostics(1);
  ASSERT_EQ(refreshed.size(), 1u);
  EXPECT_EQ(refreshed.front().snapshot->version, 3u);
  EXPECT_EQ(service.runtime_counters().stale_request_drops, 0u);
}

TEST(StyioLspServer, RunDrainsRuntimeDiagnostics) {
  styio::lsp::Server reference_server;
  styio::lsp::Server run_server;
  const std::string root_uri = styio::ide::uri_from_path(make_temp_dir());
  const std::string uri = temp_uri("runtime_run_loop.styio");

  const std::vector<llvm::json::Object> requests = {
    llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", 1},
      {"method", "initialize"},
      {"params", llvm::json::Object{{"rootUri", root_uri}}}},
    llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", "textDocument/didOpen"},
      {"params", llvm::json::Object{
         {"textDocument", llvm::json::Object{
           {"uri", uri},
           {"version", 1},
           {"text", "# add := (a: i32, b: i32) => a + b\nresult: i32 := add(1, 2)\n"}}}}}},
    llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", "textDocument/didChange"},
      {"params", llvm::json::Object{
         {"textDocument", llvm::json::Object{{"uri", uri}, {"version", 2}}},
         {"contentChanges", llvm::json::Array{
           llvm::json::Object{{"text", "# add := (a: i32, b: i32) => {\nresult: i32 := add(1, 2)\n"}}}}}}},
    llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", "textDocument/didChange"},
      {"params", llvm::json::Object{
         {"textDocument", llvm::json::Object{{"uri", uri}, {"version", 3}}},
         {"contentChanges", llvm::json::Array{
           llvm::json::Object{{"text", "# add := (a: i32, b: i32) => a +\nresult: i32 := add(1, 2)\n"}}}}}}},
    llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", "textDocument/didChange"},
      {"params", llvm::json::Object{
         {"textDocument", llvm::json::Object{{"uri", uri}, {"version", 4}}},
         {"contentChanges", llvm::json::Array{
           llvm::json::Object{{"text", "# sum := (a: i32, b: i32) => a + b\nresult: i32 := sum(1, 2)\n"}}}}}}}
  };

  std::vector<styio::lsp::OutboundMessage> expected_messages;
  for (const auto& request : requests) {
    for (const auto& message : reference_server.handle(llvm::json::Object(request))) {
      expected_messages.push_back(message);
    }
    for (const auto& message : reference_server.drain_runtime()) {
      expected_messages.push_back(message);
    }
  }

  std::string transport_input;
  for (const auto& request : requests) {
    transport_input += lsp_frame(request);
  }

  std::istringstream input(transport_input);
  std::ostringstream output;
  run_server.run(input, output);

  EXPECT_EQ(output.str(), lsp_messages_to_text(expected_messages));
}

TEST(StyioLspRuntime, RunAdvancesBackgroundWorkAsRequestDrivenFallback) {
  styio::lsp::Server server;
  const std::string root = make_temp_project_dir("m18_request_driven_background");
  write_text_file((std::filesystem::path(root) / "lib_bg.styio").string(), "# lib_bg := (x: i32) => x\n");
  write_text_file((std::filesystem::path(root) / "other_bg.styio").string(), "# other_bg := (x: i32) => x\n");

  const std::string uri = temp_uri("runtime_request_driven_fallback.styio");
  const std::vector<llvm::json::Object> requests = {
    llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", 1},
      {"method", "initialize"},
      {"params", llvm::json::Object{{"rootUri", styio::ide::uri_from_path(root)}}}},
    llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", "textDocument/didOpen"},
      {"params", llvm::json::Object{
         {"textDocument", llvm::json::Object{
           {"uri", uri},
           {"version", 1},
           {"text", "# local := (x: i32) => x\nresult: i32 := lo\n"}}}}}},
    llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", "workspace/didChangeWatchedFiles"},
      {"params", llvm::json::Object{{"changes", llvm::json::Array{}}}}},
    llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", 2},
      {"method", "textDocument/completion"},
      {"params", llvm::json::Object{
         {"textDocument", llvm::json::Object{{"uri", uri}}},
         {"position", llvm::json::Object{{"line", 1}, {"character", 16}}}}}}
  };

  std::string transport_input;
  for (const auto& request : requests) {
    transport_input += lsp_frame(request);
  }

  std::istringstream input(transport_input);
  std::ostringstream output;
  server.run(input, output);

  EXPECT_NE(output.str().find("\"id\":2"), std::string::npos);
  EXPECT_NE(output.str().find("local"), std::string::npos);
  EXPECT_EQ(server.runtime_counters().foreground_yield_events, 1u);
  EXPECT_EQ(server.runtime_counters().background_tasks_completed, 2u);
}

TEST(StyioLspRuntime, BackgroundIndexYieldsToForegroundRequests) {
  const std::string root = make_temp_project_dir("m18_background");
  write_text_file((std::filesystem::path(root) / "lib.styio").string(), "# lib_add := (a: i32, b: i32) => a + b\n");
  write_text_file((std::filesystem::path(root) / "other.styio").string(), "# other := (x: i32) => x\n");

  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string uri = styio::ide::uri_from_path((std::filesystem::path(root) / "main.styio").string());
  service.did_open(
    uri,
    "# add := (a: i32, b: i32) => a + b\n"
    "result: i32 := ad\n",
    1);
  service.drain_semantic_diagnostics();
  service.reset_runtime_counters();

  service.schedule_background_index_refresh();
  const std::size_t pending_before = service.pending_background_task_count();
  ASSERT_GT(pending_before, 0u);

  const auto completion = service.completion(uri, styio::ide::Position{1, 16});
  EXPECT_TRUE(has_completion_label(completion, "add"));
  EXPECT_EQ(service.pending_background_task_count(), pending_before);
  EXPECT_EQ(service.runtime_counters().foreground_yield_events, 1u);

  EXPECT_EQ(service.run_background_tasks(1), 1u);
  EXPECT_EQ(service.runtime_counters().background_tasks_completed, 1u);
}

TEST(StyioLspRuntime, IdleSliceDrainsSemanticBeforeBackgroundWork) {
  const std::string root = make_temp_project_dir("m18_idle_slice");
  const auto background_path = std::filesystem::path(root) / "background.styio";
  write_text_file(background_path.string(), "# background := (x: i32) => x\n");

  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string uri = styio::ide::uri_from_path((std::filesystem::path(root) / "main.styio").string());
  service.did_open(
    uri,
    "# add := (a: i32, b: i32) => a + b\n"
    "result: i32 := add(1, 2)\n",
    1);
  service.schedule_background_index_refresh();

  ASSERT_EQ(service.pending_semantic_diagnostic_count(), 1u);
  ASSERT_GT(service.pending_background_task_count(), 0u);

  const auto completion = service.completion(uri, styio::ide::Position{1, 16});
  EXPECT_TRUE(has_completion_label(completion, "add"));
  EXPECT_GT(service.pending_background_task_count(), 0u);
  EXPECT_EQ(service.runtime_counters().background_tasks_completed, 0u);

  const auto idle = service.run_idle_tasks(1);
  ASSERT_EQ(idle.semantic_publications.size(), 1u);
  EXPECT_EQ(idle.semantic_publications[0].snapshot->version, 1);
  EXPECT_EQ(idle.background_tasks_completed, 1u);
  EXPECT_EQ(service.pending_semantic_diagnostic_count(), 0u);
  EXPECT_EQ(service.runtime_counters().semantic_diagnostic_runs, 1u);
  EXPECT_EQ(service.runtime_counters().background_tasks_completed, 1u);
}

TEST(StyioLspRuntime, CancellationPropagatesThroughSemanticQueries) {
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(make_temp_dir()));

  const std::string uri = temp_uri("runtime_cancellation.styio");
  service.did_open(
    uri,
    "# add := (a: i32, b: i32) => a + b\n"
    "result: i32 := add(1, 2)\n",
    1);
  service.drain_semantic_diagnostics();
  service.reset_runtime_counters();

  const auto canceled_ticket = service.begin_foreground_request(uri, styio::ide::RuntimeRequestKind::Hover, 77);
  service.cancel_request(77);
  const auto hover = service.hover(canceled_ticket, styio::ide::Position{1, 16});
  EXPECT_FALSE(hover.has_value());

  const auto stale_ticket = service.begin_foreground_request(uri, styio::ide::RuntimeRequestKind::Definition, 78);
  service.did_change(
    uri,
    "# sum := (a: i32, b: i32) => a + b\n"
    "result: i32 := sum(1, 2)\n",
    2);
  const auto definitions = service.definition(stale_ticket, styio::ide::Position{1, 16});
  EXPECT_TRUE(definitions.empty());

  const auto publications = service.drain_semantic_diagnostics();
  ASSERT_EQ(publications.size(), 1u);
  EXPECT_EQ(publications[0].snapshot->version, 2);
  EXPECT_EQ(service.runtime_counters().canceled_requests, 1u);
  EXPECT_EQ(service.runtime_counters().stale_request_drops, 1u);
}

TEST(StyioCompletionEngine, SurvivesMalformedBinaryOperandsFromFuzzRegression) {
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(make_temp_dir()));

  const std::string uri = temp_uri("m19_completion_fuzz_regression.styio");
  const std::string source = read_text_file(
    (std::filesystem::path("tests") / "fuzz" / "corpus" / "ide_completion" / "seed-malformed-binop.styio").string());

  service.did_open(uri, source, 1);

  const auto completion = service.completion(uri, styio::ide::Position{1, 0});
  (void)completion;

  const auto publications = service.drain_semantic_diagnostics();
  ASSERT_EQ(publications.size(), 1u);
}

TEST(StyioSyntaxDrift, CorpusMatchesApprovedEnvelope) {
  struct DriftCase
  {
    std::string path;
    std::vector<std::string> syntax_expected;
    std::vector<std::string> nightly_expected;
    std::vector<std::size_t> statement_starts;
    std::vector<std::size_t> block_starts;
    std::vector<std::pair<std::string, std::size_t>> critical_tokens;
    std::size_t expected_block_depth = 0;
    std::size_t expected_non_trivia_tokens = 0;
    bool expected_recovery = false;
    std::string approved_exception;
  };

  const std::string corpus_root = (std::filesystem::path("tests") / "ide" / "corpus" / "m19").string();
  std::vector<DriftCase> cases;

  {
    const std::string path = (std::filesystem::path(corpus_root) / "simple-function-and-binding.styio").string();
    const std::string source = read_text_file(path);
    cases.push_back(DriftCase{
      path,
      {"function:add", "binding:result"},
      {"function:add", "binding:result"},
      {0, source.find("result")},
      {},
      {
        {"#", nth_occurrence(source, "#", 0)},
        {":=", nth_occurrence(source, ":=", 0)},
        {"=>", nth_occurrence(source, "=>", 0)},
        {":=", nth_occurrence(source, ":=", 1)},
      },
      0,
      26,
      false,
      ""});
  }

  {
    const std::string path = (std::filesystem::path(corpus_root) / "nested-blocks-and-match.styio").string();
    const std::string source = read_text_file(path);
    cases.push_back(DriftCase{
      path,
      {"function:classify", "binding:label"},
      {"function:classify", "binding:label"},
      {0, source.find("label")},
      {
        nth_occurrence(source, "{", 0),
        nth_occurrence(source, "{", 1),
        nth_occurrence(source, "{", 2),
        nth_occurrence(source, "{", 3),
      },
      {
        {"#", nth_occurrence(source, "#", 0)},
        {"=>", nth_occurrence(source, "=>", 0)},
        {"?=", nth_occurrence(source, "?=", 0)},
        {"{", nth_occurrence(source, "{", 0)},
        {"{", nth_occurrence(source, "{", 1)},
        {"{", nth_occurrence(source, "{", 2)},
        {"{", nth_occurrence(source, "{", 3)},
      },
      3,
      37,
      false,
      ""});
  }

  {
    const std::string path = (std::filesystem::path(corpus_root) / "member-and-types.styio").string();
    const std::string source = read_text_file(path);
    cases.push_back(DriftCase{
      path,
      {"binding:items", "binding:count"},
      {"binding:items"},
      {0, source.find("count")},
      {},
      {
        {":", nth_occurrence(source, ":", 0)},
        {"[", nth_occurrence(source, "[", 0)},
        {":=", nth_occurrence(source, ":=", 0)},
        {".", nth_occurrence(source, ".", 0)},
        {"len", nth_occurrence(source, "len", 0)},
      },
      0,
      21,
      true,
      "nightly recovery currently keeps the first typed binding but may drop the later member-access binding in this typed list case"});
  }

  {
    const std::string path = (std::filesystem::path(corpus_root) / "nested-function-capture.styio").string();
    const std::string source = read_text_file(path);
    cases.push_back(DriftCase{
      path,
      {"function:outer", "binding:value"},
      {"function:outer", "binding:value"},
      {0, source.find("value")},
      {nth_occurrence(source, "{", 0)},
      {
        {"#", nth_occurrence(source, "#", 0)},
        {"#", nth_occurrence(source, "#", 1)},
        {"{", nth_occurrence(source, "{", 0)},
        {"=>", nth_occurrence(source, "=>", 0)},
        {"=>", nth_occurrence(source, "=>", 1)},
      },
      1,
      35,
      false,
      ""});
  }

  {
    const std::string path = (std::filesystem::path(corpus_root) / "recovery-later-statements.styio").string();
    const std::string source = read_text_file(path);
    cases.push_back(DriftCase{
      path,
      {"function:broken", "function:stable", "binding:result"},
      {"function:broken", "function:stable", "binding:result"},
      {0, source.find("# stable"), source.find("result")},
      {nth_occurrence(source, "{", 0)},
      {
        {"#", nth_occurrence(source, "#", 0)},
        {"=>", nth_occurrence(source, "=>", 0)},
        {"{", nth_occurrence(source, "{", 0)},
        {"#", nth_occurrence(source, "#", 1)},
        {"=>", nth_occurrence(source, "=>", 1)},
      },
      1,
      47,
      true,
      ""});
  }

  styio::ide::VirtualFileSystem vfs;
  styio::ide::SyntaxParser parser;
  for (const auto& drift_case : cases) {
    SCOPED_TRACE(drift_case.path);
    const std::string source = read_text_file(drift_case.path);
    const auto snapshot = vfs.open(drift_case.path, source, 1);
    const auto syntax = parser.parse(*snapshot);
    bool used_recovery = false;
    const auto nightly = nightly_outline(drift_case.path, source, &used_recovery);

    EXPECT_EQ(syntax_outline(syntax), drift_case.syntax_expected);
    EXPECT_EQ(nightly, drift_case.nightly_expected);
    EXPECT_EQ(syntax_statement_starts(syntax), drift_case.statement_starts);
    EXPECT_EQ(syntax_block_starts(syntax), drift_case.block_starts);
    EXPECT_EQ(syntax_max_block_depth(syntax), drift_case.expected_block_depth);
    EXPECT_EQ(count_non_trivia_tokens(syntax), drift_case.expected_non_trivia_tokens);
    EXPECT_EQ(used_recovery, drift_case.expected_recovery);

    for (const auto& critical : drift_case.critical_tokens) {
      EXPECT_TRUE(has_token_boundary(syntax, critical.first, critical.second))
        << "missing token `" << critical.first << "` at " << critical.second;
    }

    if (drift_case.approved_exception.empty()) {
      EXPECT_EQ(syntax_outline(syntax), nightly);
    } else {
      EXPECT_FALSE(nightly.empty()) << drift_case.approved_exception;
    }
  }
}

TEST(StyioIdePerf, EnforcesFrozenLatencyBudgets) {
#ifndef NDEBUG
  GTEST_SKIP() << "Frozen latency budgets are enforced in the release perf harness.";
#endif

  constexpr std::size_t kLineCount = 5000;
  constexpr std::size_t kFunctionCount = 2500;
  constexpr std::size_t kHotIterations = 20;

  styio::ide::VirtualFileSystem vfs;
  styio::ide::SyntaxParser parser;
  const std::string parse_path = make_temp_dir() + "/m19_perf_incremental.styio";
  const std::string parse_source = make_incremental_perf_source(kLineCount);
  auto parse_snapshot = vfs.open(parse_path, parse_source, 1);
  (void)parser.parse(*parse_snapshot);

  const std::size_t toggle_offset = parse_source.rfind("1\n");
  ASSERT_NE(toggle_offset, std::string::npos);
  std::vector<std::uint64_t> parse_samples;
  for (std::size_t i = 0; i < kHotIterations; ++i) {
    styio::ide::DocumentDelta delta;
    delta.edits.push_back(styio::ide::TextEdit{
      styio::ide::TextRange{toggle_offset, toggle_offset + 1},
      (i % 2 == 0) ? "2" : "1"});
    const auto update = vfs.update(parse_path, delta, static_cast<styio::ide::DocumentVersion>(i + 2));
    ASSERT_NE(update.snapshot, nullptr);
    parse_samples.push_back(measure_microseconds([&]() { (void)parser.parse(*update.snapshot); }));
  }

  const std::string perf_source = make_hot_query_perf_source(kFunctionCount);
  const std::string root = make_temp_project_dir("m19_perf_workspace_hot");
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));
  const std::string uri = styio::ide::uri_from_path((std::filesystem::path(root) / "hot.styio").string());
  service.did_open(uri, perf_source, 1);
  service.drain_semantic_diagnostics();

  styio::ide::TextBuffer perf_buffer(perf_source);
  const std::size_t completion_offset = perf_source.rfind("need");
  const std::size_t hover_offset = perf_source.rfind("needle_value");
  ASSERT_NE(completion_offset, std::string::npos);
  ASSERT_NE(hover_offset, std::string::npos);

  const styio::ide::Position completion_pos = perf_buffer.position_at(completion_offset + 4);
  const styio::ide::Position hover_pos = perf_buffer.position_at(hover_offset);

  ASSERT_TRUE(has_completion_label(service.completion(uri, completion_pos), "needle_value"));
  ASSERT_TRUE(service.hover(uri, hover_pos).has_value());
  ASSERT_EQ(service.definition(uri, hover_pos).size(), 1u);

  service.reset_runtime_counters();
  std::vector<std::uint64_t> completion_samples;
  std::vector<std::uint64_t> hover_samples;
  std::vector<std::uint64_t> definition_samples;
  for (std::size_t i = 0; i < kHotIterations; ++i) {
    completion_samples.push_back(measure_microseconds([&]() { (void)service.completion(uri, completion_pos); }));
    hover_samples.push_back(measure_microseconds([&]() { (void)service.hover(uri, hover_pos); }));
    definition_samples.push_back(measure_microseconds([&]() { (void)service.definition(uri, hover_pos); }));
  }

  std::vector<std::uint64_t> startup_samples;
  for (std::size_t i = 0; i < 3; ++i) {
    const std::string startup_root = make_temp_project_dir("m19_perf_startup_" + std::to_string(i));
    for (std::size_t file_index = 0; file_index < 100; ++file_index) {
      std::ostringstream file_source;
      for (std::size_t line = 0; line < 50; ++line) {
        file_source << "# perf_fn_" << file_index << "_" << line << " := (value: i32) => value + " << (line % 3) << "\n";
      }
      write_text_file(
        (std::filesystem::path(startup_root) / ("file_" + std::to_string(file_index) + ".styio")).string(),
        file_source.str());
    }

    startup_samples.push_back(measure_microseconds([&]()
    {
      styio::ide::IdeService startup_service;
      startup_service.initialize(styio::ide::uri_from_path(startup_root));
    }));
  }

  const std::uint64_t parse_p95 = percentile95(parse_samples);
  const std::uint64_t completion_p95 = percentile95(completion_samples);
  const std::uint64_t hover_p95 = percentile95(hover_samples);
  const std::uint64_t definition_p95 = percentile95(definition_samples);
  const std::uint64_t startup_p95 = percentile95(startup_samples);

  EXPECT_LE(parse_p95, 10'000u) << "hot incremental syntax parse p95(us)=" << parse_p95;
  EXPECT_LE(completion_p95, 50'000u) << "hot completion p95(us)=" << completion_p95;
  EXPECT_LE(std::max(hover_p95, definition_p95), 80'000u)
    << "hot hover/definition p95(us)=" << std::max(hover_p95, definition_p95);
  EXPECT_LE(startup_p95, 5'000'000u) << "background index startup p95(us)=" << startup_p95;

  const auto& runtime = service.runtime_counters();
  EXPECT_EQ(runtime.completion_latency.count, kHotIterations);
  EXPECT_EQ(runtime.hover_latency.count, kHotIterations);
  EXPECT_EQ(runtime.definition_latency.count, kHotIterations);
}
