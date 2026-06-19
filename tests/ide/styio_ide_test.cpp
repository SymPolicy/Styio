#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "StyioAST/AST.hpp"
#include "StyioServices/StyioIDE/CompilerBridge.hpp"
#include "StyioServices/StyioIDE/HIR.hpp"
#include "StyioServices/StyioIDE/Index.hpp"
#include "StyioServices/StyioIDE/Project.hpp"
#include "StyioServices/StyioIDE/Syntax.hpp"
#include "StyioServices/StyioIDE/VFS.hpp"

#define private public
#include "StyioServices/StyioIDE/SemDB.hpp"
#undef private

#define private public
#include "StyioServices/StyioIDE/Service.hpp"
#undef private

#include "StyioServices/StyioLSP/Server.hpp"
#include "StyioException/Exception.hpp"
#include "StyioParser/Parser.hpp"
#include "StyioParser/Tokenizer.hpp"
#include "llvm/Support/FormatVariadic.h"

#define analyze_document analyze_document_internal_for_test
#include "../src/StyioServices/StyioIDE/CompilerBridge.cpp"
#undef analyze_document

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

class EnvVarGuard
{
public:
  explicit EnvVarGuard(std::string name) :
      name_(std::move(name)) {
    const char* value = std::getenv(name_.c_str());
    if (value != nullptr) {
      had_value_ = true;
      old_value_ = value;
    }
  }

  ~EnvVarGuard() {
    if (had_value_) {
      setenv(name_.c_str(), old_value_.c_str(), 1);
    } else {
      unsetenv(name_.c_str());
    }
  }

  void set(const std::string& value) {
    setenv(name_.c_str(), value.c_str(), 1);
  }

  void unset() {
    unsetenv(name_.c_str());
  }

private:
  std::string name_;
  bool had_value_ = false;
  std::string old_value_;
};

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

bool
has_token(
  const styio::ide::SyntaxSnapshot& syntax,
  StyioTokenType type,
  const std::string& lexeme
) {
  return std::any_of(
    syntax.tokens.begin(),
    syntax.tokens.end(),
    [&](const styio::ide::SyntaxToken& token)
    {
      return token.type == type && token.lexeme == lexeme;
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
      StyioParseMode::Strict);
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
  const auto empty_path_snapshot = vfs.open("", "", 0);
  ASSERT_NE(empty_path_snapshot, nullptr);
  EXPECT_EQ(empty_path_snapshot->path, "");

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

  vfs.close(path + ".missing");
}

TEST(StyioIDECommon, TextBufferUriAndEnumHelpersCoverEdgeCases) {
  styio::ide::TextBuffer empty_buffer;
  EXPECT_TRUE(empty_buffer.empty());
  EXPECT_EQ(empty_buffer.text(), "");
  EXPECT_EQ(empty_buffer.size(), 0U);
  EXPECT_EQ(empty_buffer.position_at(42).line, 0U);
  EXPECT_EQ(empty_buffer.position_at(42).character, 0U);
  EXPECT_EQ(empty_buffer.offset_at(styio::ide::Position{42, 42}), 0U);

  styio::ide::TextBuffer buffer("alpha\nbeta");
  EXPECT_FALSE(buffer.empty());
  EXPECT_EQ(buffer.position_at(999).line, 1U);
  EXPECT_EQ(buffer.position_at(999).character, 4U);
  EXPECT_EQ(buffer.offset_at(styio::ide::Position{99, 99}), buffer.text().size());
  EXPECT_EQ(buffer.offset_at(styio::ide::Position{0, 99}), 6U);

  const auto line_seps = buffer.build_line_seps();
  ASSERT_EQ(line_seps.size(), 2U);
  const auto first_line = std::make_pair<std::size_t, std::size_t>(0, 5);
  const auto second_line = std::make_pair<std::size_t, std::size_t>(6, 4);
  EXPECT_EQ(line_seps[0], first_line);
  EXPECT_EQ(line_seps[1], second_line);

  const styio::ide::TextRange range{2, 7};
  EXPECT_TRUE(range.contains(2));
  EXPECT_TRUE(range.contains(7));
  EXPECT_EQ(range.length(), 5U);
  EXPECT_EQ((styio::ide::TextRange{9, 4}).length(), 0U);

  EXPECT_EQ(styio::ide::path_from_uri("untitled:main.styio"), "untitled:main.styio");
  EXPECT_EQ(
    styio::ide::path_from_uri("file://tmp/space+name%2Fchild%41.styio"),
    "/tmp/space name/childA.styio");
  EXPECT_EQ(
    styio::ide::path_from_uri("file:///tmp/lower%2fhex%61.styio"),
    "/tmp/lower/hexa.styio");
  EXPECT_EQ(
    styio::ide::path_from_uri("file:///tmp/bad%ZZ+name.styio"),
    "/tmp/bad%ZZ name.styio");

  EXPECT_EQ(
    styio::ide::uri_from_path("relative dir/main file.styio"),
    "file:///relative%20dir/main%20file.styio");
  EXPECT_EQ(styio::ide::uri_from_path("/tmp/main#1.styio"), "file:///tmp/main%231.styio");

  EXPECT_EQ(styio::ide::to_string(styio::ide::PositionKind::TopLevel), "TopLevel");
  EXPECT_EQ(styio::ide::to_string(styio::ide::PositionKind::StmtStart), "StmtStart");
  EXPECT_EQ(styio::ide::to_string(styio::ide::PositionKind::Expr), "Expr");
  EXPECT_EQ(styio::ide::to_string(styio::ide::PositionKind::Type), "Type");
  EXPECT_EQ(styio::ide::to_string(styio::ide::PositionKind::Pattern), "Pattern");
  EXPECT_EQ(styio::ide::to_string(styio::ide::PositionKind::MemberAccess), "MemberAccess");
  EXPECT_EQ(styio::ide::to_string(styio::ide::PositionKind::ImportPath), "ImportPath");
  EXPECT_EQ(styio::ide::to_string(styio::ide::PositionKind::CallArg), "CallArg");
  EXPECT_EQ(styio::ide::to_string(styio::ide::PositionKind::AttrName), "AttrName");
  EXPECT_EQ(styio::ide::to_string(static_cast<styio::ide::PositionKind>(999)), "Expr");

  EXPECT_EQ(styio::ide::to_string(styio::ide::SymbolKind::Variable), "variable");
  EXPECT_EQ(styio::ide::to_string(styio::ide::SymbolKind::Function), "function");
  EXPECT_EQ(styio::ide::to_string(styio::ide::SymbolKind::Parameter), "parameter");
  EXPECT_EQ(styio::ide::to_string(styio::ide::SymbolKind::Builtin), "builtin");
  EXPECT_EQ(styio::ide::to_string(static_cast<styio::ide::SymbolKind>(999)), "variable");

  EXPECT_EQ(styio::ide::to_string(styio::ide::CompletionItemKind::Variable), "variable");
  EXPECT_EQ(styio::ide::to_string(styio::ide::CompletionItemKind::Function), "function");
  EXPECT_EQ(styio::ide::to_string(styio::ide::CompletionItemKind::Type), "type");
  EXPECT_EQ(styio::ide::to_string(styio::ide::CompletionItemKind::Keyword), "keyword");
  EXPECT_EQ(styio::ide::to_string(styio::ide::CompletionItemKind::Snippet), "snippet");
  EXPECT_EQ(styio::ide::to_string(styio::ide::CompletionItemKind::Property), "property");
  EXPECT_EQ(styio::ide::to_string(styio::ide::CompletionItemKind::Module), "module");
  EXPECT_EQ(styio::ide::to_string(static_cast<styio::ide::CompletionItemKind>(999)), "variable");

  EXPECT_EQ(styio::ide::to_string(styio::ide::CompletionSource::Local), "local");
  EXPECT_EQ(styio::ide::to_string(styio::ide::CompletionSource::Imported), "imported");
  EXPECT_EQ(styio::ide::to_string(styio::ide::CompletionSource::Builtin), "builtin");
  EXPECT_EQ(styio::ide::to_string(styio::ide::CompletionSource::Keyword), "keyword");
  EXPECT_EQ(styio::ide::to_string(styio::ide::CompletionSource::Snippet), "snippet");
  EXPECT_EQ(styio::ide::to_string(static_cast<styio::ide::CompletionSource>(999)), "local");
}

TEST(StyioSemanticBridge, InternalCompilerBridgeHelpersCoverEdgeFacts) {
  using namespace styio::ide;

  EXPECT_EQ(normalize_import_path("pkg.math.utils"), "pkg/math/utils");
  EXPECT_EQ(normalize_import_path("pkg/math/utils"), "pkg/math/utils");
  EXPECT_EQ(normalize_import_path(""), "");
  EXPECT_EQ(semantic_item_kind_key(static_cast<SemanticItemKind>(999)), "global");

  {
    std::unique_ptr<StringAST> path(StringAST::Create("plain.txt"));
    EXPECT_EQ(path_name_from_ast(path.get()), "plain.txt");
  }
  {
    std::unique_ptr<ResPathAST> path(ResPathAST::Create(StyioPathType::local_relevant_any, "rel/path"));
    EXPECT_EQ(path_name_from_ast(path.get()), "rel/path");
  }
  {
    std::unique_ptr<RemotePathAST> path(RemotePathAST::Create(StyioPathType::ipv4_addr, "127.0.0.1"));
    EXPECT_EQ(path_name_from_ast(path.get()), "127.0.0.1");
  }
  {
    std::unique_ptr<WebUrlAST> path(WebUrlAST::Create(StyioPathType::url_https, "https://example.test"));
    EXPECT_EQ(path_name_from_ast(path.get()), "https://example.test");
  }
  {
    std::unique_ptr<FileResourceAST> file(FileResourceAST::Create(StringAST::Create("nested.txt"), true));
    EXPECT_EQ(path_name_from_ast(file.get()), "nested.txt");
  }
  {
    std::unique_ptr<NameAST> name(NameAST::Create(""));
    EXPECT_EQ(path_name_from_ast(name.get()), "");
  }

  std::variant<TypeAST*, TypeTupleAST*> tuple_ret(static_cast<TypeTupleAST*>(nullptr));
  EXPECT_EQ(type_name_from_variant(tuple_ret), "undefined");
  EXPECT_EQ(signature_for_function(IntAST::Create("1")), "");
  EXPECT_EQ(type_name_from_var(nullptr), "");
  {
    std::unique_ptr<ParamAST> param(ParamAST::Create(NameAST::Create("p")));
    const auto params = params_from_ast({nullptr, param.get()});
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0].name, "p");
    EXPECT_EQ(params[0].type_name, "");
  }

  SemanticSummary summary;
  std::unordered_map<std::string, std::size_t> ordinals;
  append_item_fact(
    summary,
    SemanticItemFact{SemanticItemKind::Import, "", {}, "import", "", 0, true},
    ordinals);
  append_item_fact(
    summary,
    SemanticItemFact{SemanticItemKind::Import, "pkg.math", {}, "import", "", 0, true},
    ordinals);
  ASSERT_EQ(summary.items.size(), 2u);
  EXPECT_EQ(summary.items[0].name, "import");
  EXPECT_EQ(summary.items[1].name, "pkg/math");

  collect_semantic_items(summary, nullptr);
  summary.inferred_types["mut"] = "string";
  {
    std::unique_ptr<MainBlockAST> main_block(MainBlockAST::Create({
      nullptr,
      FunctionAST::Create(
        NameAST::Create("fn"),
        false,
        {ParamAST::Create(NameAST::Create("arg"), TypeAST::Create("i32"))},
        TypeAST::Create("i64"),
        BlockAST::Create({ReturnAST::Create(IntAST::Create("1"))})),
      SimpleFuncAST::Create(
        NameAST::Create("simple"),
        {ParamAST::Create(NameAST::Create("value"))},
        TypeAST::Create("string"),
        StringAST::Create("ok")),
      FlexBindAST::Create(
        VarAST::Create(NameAST::Create("mut"), TypeAST::Create("i64")),
        StringAST::Create("changed")),
      FinalBindAST::Create(
        VarAST::Create(NameAST::Create("fixed"), TypeAST::Create("bool")),
        BoolAST::Create(true)),
      new ExtPackAST({"tools.net", "already/path"}),
      ResourceAST::Create({
        {FinalBindAST::Create(
           VarAST::Create(NameAST::Create("handle")),
           FileResourceAST::Create(StringAST::Create("handle.txt"), false)), "file"},
        {StringAST::Create("plain.txt"), "text"},
        {FileResourceAST::Create(StringAST::Create("nested.txt"), true), ""},
        {NameAST::Create(""), ""},
      }),
    }));
    collect_semantic_items(summary, main_block.get());
  }
  const std::size_t before_malformed_items = summary.items.size();
  {
    std::unique_ptr<MainBlockAST> malformed(MainBlockAST::Create({
      SimpleFuncAST::Create(),
      FlexBindAST::Create(nullptr, IntAST::Create("1")),
      FinalBindAST::Create(nullptr, IntAST::Create("2")),
    }));
    collect_semantic_items(summary, malformed.get());
  }
  EXPECT_EQ(summary.items.size(), before_malformed_items);

  EXPECT_TRUE(std::any_of(
    summary.items.begin(),
    summary.items.end(),
    [](const SemanticItemFact& item)
    {
      return item.kind == SemanticItemKind::Function
        && item.name == "fn"
        && item.detail.find("fn(arg: i32)") != std::string::npos;
    }));
  EXPECT_TRUE(std::any_of(
    summary.items.begin(),
    summary.items.end(),
    [](const SemanticItemFact& item)
    {
      return item.kind == SemanticItemKind::GlobalBinding
        && item.name == "mut"
        && item.type_name == "string";
    }));
  EXPECT_TRUE(std::any_of(
    summary.items.begin(),
    summary.items.end(),
    [](const SemanticItemFact& item)
    {
      return item.kind == SemanticItemKind::Resource && item.name == "resource@0";
    }));
  EXPECT_TRUE(std::any_of(
    summary.items.begin(),
    summary.items.end(),
    [](const SemanticItemFact& item)
    {
      return item.kind == SemanticItemKind::Resource && item.name == "nested.txt";
    }));
}

TEST(StyioIdeProject, EnvironmentFallbacksAndWorkspaceSkipsStayExplicit) {
  EnvVarGuard xdg_cache_home("XDG_CACHE_HOME");
  EnvVarGuard home("HOME");

  const std::filesystem::path root = make_temp_project_dir("ide-project-env");
  write_text_file((root / "main.styio").string(), "value: i32 := 1\n");
  std::filesystem::create_directories(root / ".git");
  std::filesystem::create_directories(root / "build");
  std::filesystem::create_directories(root / "build-codex");
  write_text_file((root / ".git" / "hidden.styio").string(), "hidden: i32 := 1\n");
  write_text_file((root / "build" / "hidden.styio").string(), "hidden: i32 := 1\n");
  write_text_file((root / "build-codex" / "hidden.styio").string(), "hidden: i32 := 1\n");

  const std::filesystem::path cache = root / "cache-home";
  xdg_cache_home.set(cache.string());
  home.set((root / "home").string());
  styio::ide::Project project;
  project.set_root(root.string());
  EXPECT_EQ(project.project_id(), 1u);
  EXPECT_NE(project.cache_root().find((cache / "styio" / "ide").string()), std::string::npos);
  ASSERT_EQ(project.workspace_files().size(), 1u);
  EXPECT_EQ(std::filesystem::path(project.workspace_files()[0]).filename(), "main.styio");

  xdg_cache_home.unset();
  home.unset();
  styio::ide::Project fallback;
  fallback.set_root("");
  EXPECT_TRUE(fallback.workspace_files().empty());
  EXPECT_NE(fallback.cache_root().find("styio-ide-cache"), std::string::npos);
}

TEST(StyioIdeService, RuntimeSchedulingEdgesStayExplicit) {
  const std::filesystem::path root = make_temp_project_dir("ide-service-runtime");
  const std::filesystem::path lib_path = root / "lib.styio";
  const std::filesystem::path other_path = root / "other.styio";
  write_text_file(lib_path.string(), "# value := () => 1\n");
  write_text_file(other_path.string(), "other: i32 := 2\n");

  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root.string()));

  const std::string lib_uri = styio::ide::uri_from_path(lib_path.string());
  service.did_open(lib_uri, "# value := () => 1\nvalue_result: i32 := value()\n", 1);
  service.schedule_background_index_refresh();
  EXPECT_EQ(service.pending_background_task_count(), 1u);
  EXPECT_EQ(service.runtime_counters().background_tasks_enqueued, 1u);
  EXPECT_EQ(service.run_background_tasks(10), 1u);
  EXPECT_EQ(service.runtime_counters().background_tasks_completed, 1u);

  const auto ticket = service.begin_foreground_request(
    lib_uri,
    styio::ide::RuntimeRequestKind::Completion,
    9001);
  service.did_close(lib_uri);
  EXPECT_TRUE(service.completion(ticket, styio::ide::Position{0, 0}).empty());
  EXPECT_EQ(service.runtime_counters().stale_request_drops, 1u);
}

TEST(StyioIdeService, WhiteBoxSemanticQueueStateEdgesStayExplicit) {
  const std::filesystem::path root = make_temp_project_dir("ide-service-runtime-private");
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root.string()));

  const std::string debounce_path = (root / "debounce.styio").string();
  auto debounce_snapshot = service.vfs_.open(debounce_path, "value: i32 := 1\n", 1);
  service.record_visible_snapshot(debounce_snapshot);
  auto& debounce_state = service.runtime_state_for(debounce_path);
  debounce_state.has_pending_semantic = true;
  service.pending_semantic_paths_.clear();
  service.pending_semantic_path_set_.clear();
  service.reset_runtime_counters();

  service.schedule_semantic_diagnostics(debounce_snapshot);
  EXPECT_EQ(service.runtime_counters().semantic_diagnostic_debounces, 1u);
  ASSERT_EQ(service.pending_semantic_paths_.size(), 1u);
  EXPECT_EQ(service.pending_semantic_paths_.front(), debounce_path);

  service.pending_semantic_paths_.clear();
  service.pending_semantic_path_set_.clear();
  service.pending_semantic_paths_.push_back((root / "missing.styio").string());
  service.reset_runtime_counters();
  EXPECT_TRUE(service.drain_semantic_diagnostics(1).empty());
  EXPECT_EQ(service.runtime_counters().stale_request_drops, 0u);

  const std::string stale_state_path = (root / "stale-state.styio").string();
  auto stale_state_snapshot = service.vfs_.open(stale_state_path, "value: i32 := 1\n", 1);
  service.record_visible_snapshot(stale_state_snapshot);
  auto& stale_state = service.runtime_state_for(stale_state_path);
  stale_state.has_pending_semantic = true;
  stale_state.pending_semantic_snapshot_id = stale_state.snapshot_id;
  stale_state.pending_semantic_version = stale_state.version;
  stale_state.pending_semantic_generation = stale_state.generation + 1;
  service.pending_semantic_paths_.clear();
  service.pending_semantic_path_set_.clear();
  service.pending_semantic_paths_.push_back(stale_state_path);
  service.pending_semantic_path_set_.insert(stale_state_path);
  service.reset_runtime_counters();

  EXPECT_TRUE(service.drain_semantic_diagnostics(1).empty());
  EXPECT_EQ(service.runtime_counters().stale_request_drops, 1u);

  const std::string stale_snapshot_path = (root / "stale-snapshot.styio").string();
  auto stale_snapshot = service.vfs_.open(stale_snapshot_path, "value: i32 := 1\n", 1);
  service.record_visible_snapshot(stale_snapshot);
  auto& stale_snapshot_state = service.runtime_state_for(stale_snapshot_path);
  stale_snapshot_state.has_pending_semantic = true;
  stale_snapshot_state.pending_semantic_snapshot_id = stale_snapshot_state.snapshot_id;
  stale_snapshot_state.pending_semantic_version = stale_snapshot_state.version;
  stale_snapshot_state.pending_semantic_generation = stale_snapshot_state.generation;
  service.vfs_.update(stale_snapshot_path, "value: i32 := 2\n", 2);
  service.pending_semantic_paths_.clear();
  service.pending_semantic_path_set_.clear();
  service.pending_semantic_paths_.push_back(stale_snapshot_path);
  service.pending_semantic_path_set_.insert(stale_snapshot_path);
  service.reset_runtime_counters();

  EXPECT_TRUE(service.drain_semantic_diagnostics(1).empty());
  EXPECT_EQ(service.runtime_counters().stale_request_drops, 1u);
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

TEST(StyioSyntaxParser, TolerantTokenizerCoversWidePunctuationAndQueries) {
  styio::ide::SyntaxParser parser;
  styio::ide::DocumentSnapshot snapshot;
  snapshot.file_id = 12;
  snapshot.snapshot_id = 1;
  snapshot.path = "memory://wide_syntax_tokens.styio";
  snapshot.version = 1;
  snapshot.buffer = styio::ide::TextBuffer{
    "// comment\r\n"
    "/* closed */\n"
    "123 45.67 ... |<| -> <- << >> >= <= == != && || ** += -= *= /= %= [| |] |; <~ ~> ?? ?|\n"
    "$ = ? < > | ! ^ ~ @ _ \"unterminated\n"
    "{ [ ( name.member, other: i32 ?= _ ) ] }\n"};

  const auto syntax = parser.parse(snapshot);

  EXPECT_TRUE(has_token(syntax, StyioTokenType::COMMENT_LINE, "// comment"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::TOK_CR, "\r"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::COMMENT_CLOSED, "/* closed */"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::INTEGER, "123"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::DECIMAL, "45.67"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::ELLIPSIS, "..."));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::RETURN_PIPE, "|<|"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::ARROW_SINGLE_RIGHT, "->"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::ARROW_SINGLE_LEFT, "<-"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::EXTRACTOR, "<<"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::ITERATOR, ">>"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::BINOP_GE, ">="));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::BINOP_LE, "<="));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::BINOP_EQ, "=="));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::BINOP_NE, "!="));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::LOGIC_AND, "&&"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::LOGIC_OR, "||"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::BINOP_POW, "**"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::COMPOUND_ADD, "+="));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::COMPOUND_SUB, "-="));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::COMPOUND_MUL, "*="));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::COMPOUND_DIV, "/="));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::COMPOUND_MOD, "%="));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::BOUNDED_BUFFER_OPEN, "[|"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::BOUNDED_BUFFER_CLOSE, "|]"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::PIPE_SEMICOLON, "|;"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::WAVE_LEFT, "<~"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::WAVE_RIGHT, "~>"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::DBQUESTION, "??"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::AWAIT_PIPE, "?|"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::TOK_DOLLAR, "$"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::TOK_EQUAL, "="));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::TOK_QUEST, "?"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::TOK_LANGBRAC, "<"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::TOK_RANGBRAC, ">"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::TOK_PIPE, "|"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::TOK_EXCLAM, "!"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::TOK_HAT, "^"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::TOK_TILDE, "~"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::TOK_UNDLINE, "_"));

  EXPECT_NE(
    std::find_if(
      syntax.diagnostics.begin(),
      syntax.diagnostics.end(),
      [](const styio::ide::Diagnostic& diagnostic)
      {
        return diagnostic.message == "unterminated string literal";
      }),
    syntax.diagnostics.end());

  EXPECT_EQ(syntax.position_kind_at(0), styio::ide::PositionKind::TopLevel);
  EXPECT_EQ(
    syntax.expected_tokens_at(0),
    std::vector<std::string>({"NAME", "#", "@", "["}));
  EXPECT_EQ(
    syntax.expected_categories_at(0),
    std::vector<std::string>({"value", "function", "keyword", "snippet"}));
  const std::size_t member_offset = snapshot.buffer.text().find("member");
  ASSERT_NE(member_offset, std::string::npos);
  EXPECT_EQ(syntax.position_kind_at(member_offset), styio::ide::PositionKind::MemberAccess);
  EXPECT_EQ(syntax.expected_categories_at(member_offset), std::vector<std::string>({"member"}));
  const std::size_t type_offset = snapshot.buffer.text().find("i32");
  ASSERT_NE(type_offset, std::string::npos);
  EXPECT_EQ(syntax.position_kind_at(type_offset), styio::ide::PositionKind::Type);
  EXPECT_EQ(syntax.expected_tokens_at(type_offset), std::vector<std::string>({"NAME"}));
  EXPECT_EQ(syntax.expected_categories_at(type_offset), std::vector<std::string>({"type"}));
  const std::size_t pattern_offset = snapshot.buffer.text().find("_ )");
  ASSERT_NE(pattern_offset, std::string::npos);
  EXPECT_EQ(syntax.position_kind_at(pattern_offset), styio::ide::PositionKind::Pattern);
  EXPECT_EQ(
    syntax.expected_tokens_at(pattern_offset),
    std::vector<std::string>({"NAME", "INTEGER", "STRING", "_"}));
  EXPECT_EQ(
    syntax.expected_categories_at(pattern_offset),
    std::vector<std::string>({"pattern", "literal"}));
  const std::size_t attr_offset = snapshot.buffer.text().find("@ _");
  ASSERT_NE(attr_offset, std::string::npos);
  EXPECT_EQ(syntax.position_kind_at(attr_offset + 2), styio::ide::PositionKind::AttrName);
  EXPECT_EQ(syntax.expected_tokens_at(attr_offset + 2), std::vector<std::string>({"NAME"}));
  EXPECT_EQ(
    syntax.expected_categories_at(attr_offset + 2),
    std::vector<std::string>({"resource", "attribute"}));

  EXPECT_EQ(syntax.prefix_at(snapshot.buffer.size() + 100), "");
  EXPECT_FALSE(syntax.node_path_at(0).empty());
  EXPECT_NE(syntax.node_at_offset(0), nullptr);
  EXPECT_EQ(syntax.node_at_offset(snapshot.buffer.size() + 100), nullptr);
  EXPECT_FALSE(syntax.token_index_at(snapshot.buffer.size() + 100).has_value());

  styio::ide::SyntaxSnapshot equal_span_nodes;
  equal_span_nodes.nodes = {
    styio::ide::SyntaxNode{styio::ide::SyntaxNodeKind::Group, "same-a", styio::ide::TextRange{0, 2}, {}},
    styio::ide::SyntaxNode{styio::ide::SyntaxNodeKind::Group, "same-b", styio::ide::TextRange{0, 2}, {}},
    styio::ide::SyntaxNode{styio::ide::SyntaxNodeKind::Group, "later", styio::ide::TextRange{1, 3}, {}},
  };
  EXPECT_EQ(
    equal_span_nodes.node_path_at(1),
    std::vector<std::size_t>({0, 1, 2}));
}

TEST(StyioSyntaxParser, TolerantDiagnosticsAndContextQueriesCoverEdges) {
  styio::ide::SyntaxParser parser;
  styio::ide::DocumentSnapshot snapshot;
  snapshot.file_id = 13;
  snapshot.snapshot_id = 1;
  snapshot.path = make_temp_dir() + "/syntax_edge_queries.styio";
  snapshot.version = 1;
  snapshot.buffer = styio::ide::TextBuffer{
    "{\n"
    "  fn(a, b) -\n"
    "  list[0\n"
    "}\n"
    ")\n"
    "/* unterminated"};

  const auto syntax = parser.parse(snapshot);

  EXPECT_TRUE(has_token(syntax, StyioTokenType::COMMENT_CLOSED, "/* unterminated"));
  EXPECT_TRUE(has_token(syntax, StyioTokenType::TOK_MINUS, "-"));
  EXPECT_NE(
    std::find_if(
      syntax.diagnostics.begin(),
      syntax.diagnostics.end(),
      [](const styio::ide::Diagnostic& diagnostic)
      {
        return diagnostic.message == "unterminated block comment";
      }),
    syntax.diagnostics.end());
  EXPECT_NE(
    std::find_if(
      syntax.diagnostics.begin(),
      syntax.diagnostics.end(),
      [](const styio::ide::Diagnostic& diagnostic)
      {
        return diagnostic.message.find("unmatched closing token") != std::string::npos;
      }),
    syntax.diagnostics.end());
  EXPECT_NE(
    std::find_if(
      syntax.diagnostics.begin(),
      syntax.diagnostics.end(),
      [](const styio::ide::Diagnostic& diagnostic)
      {
        return diagnostic.message.find("unclosed opening token") != std::string::npos;
      }),
    syntax.diagnostics.end());

  const std::size_t inner_offset = snapshot.buffer.text().find("fn");
  ASSERT_NE(inner_offset, std::string::npos);
  EXPECT_EQ(syntax.position_kind_at(inner_offset), styio::ide::PositionKind::StmtStart);
  EXPECT_EQ(syntax.expected_categories_at(inner_offset), std::vector<std::string>({"value", "function", "keyword", "snippet"}));

  const std::size_t arg_offset = snapshot.buffer.text().find("b)");
  ASSERT_NE(arg_offset, std::string::npos);
  EXPECT_EQ(syntax.position_kind_at(arg_offset), styio::ide::PositionKind::CallArg);
  EXPECT_EQ(
    syntax.expected_tokens_at(arg_offset),
    std::vector<std::string>({"NAME", "INTEGER", "STRING", "(", "[", "@"}));

  const auto next = syntax.next_non_trivia_index(0);
  ASSERT_TRUE(next.has_value());
  EXPECT_EQ(syntax.tokens[*next].type, StyioTokenType::TOK_LCURBRAC);

  parser.drop_cached_file(snapshot.path);
  styio::ide::DocumentSnapshot second = snapshot;
  second.snapshot_id = 2;
  second.version = 2;
  second.buffer = styio::ide::TextBuffer{"value: i32 := 1\n"};
  const auto reparsed = parser.parse(second);
  EXPECT_FALSE(reparsed.tokens.empty());
  EXPECT_TRUE(reparsed.diagnostics.empty());
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

TEST(StyioIDEIndex, OpenBackgroundAndPersistentIndexCoverEdgeBranches) {
  styio::ide::HirModule module;
  module.symbols.push_back(styio::ide::HirSymbol{
    1,
    "alpha",
    styio::ide::SymbolKind::Function,
    0,
    std::optional<styio::ide::ItemId>{1},
    styio::ide::TextRange{0, 5},
    styio::ide::TextRange{0, 20},
    "alpha(x: i32) -> i64",
    "",
    "alpha-key",
    true});
  module.symbols.push_back(styio::ide::HirSymbol{
    2,
    "beta",
    styio::ide::SymbolKind::Variable,
    0,
    std::nullopt,
    styio::ide::TextRange{21, 25},
    styio::ide::TextRange{21, 32},
    "",
    "string",
    "beta-key",
    true});
  module.symbols.push_back(styio::ide::HirSymbol{
    3,
    "local_only",
    styio::ide::SymbolKind::Variable,
    7,
    std::nullopt,
    styio::ide::TextRange{33, 43},
    styio::ide::TextRange{33, 50},
    "",
    "i64",
    "local-key",
    true});
  module.references.push_back(styio::ide::HirReference{
    "alpha",
    0,
    styio::ide::TextRange{60, 65},
    std::optional<styio::ide::SymbolId>{1}});

  const std::string path = make_temp_dir() + "/indexed.styio";
  styio::ide::OpenFileIndex open_index;
  open_index.update(path, module);
  EXPECT_EQ(open_index.query_symbols("").size(), 2u);
  EXPECT_EQ(open_index.query_symbols("alp").size(), 1u);
  EXPECT_EQ(open_index.query_symbols_exact("beta").size(), 1u);
  EXPECT_EQ(open_index.query_references("alpha").size(), 1u);
  EXPECT_EQ(open_index.indexed_paths(), std::vector<std::string>({path}));
  open_index.erase(path);
  EXPECT_TRUE(open_index.query_symbols("").empty());
  EXPECT_TRUE(open_index.query_references("alpha").empty());

  styio::ide::BackgroundIndex background_index;
  background_index.update(path, module);
  EXPECT_EQ(background_index.query_symbols_exact("alpha").size(), 1u);
  EXPECT_EQ(background_index.query_references("alpha").size(), 1u);
  EXPECT_EQ(background_index.indexed_paths(), std::vector<std::string>({path}));
  background_index.erase(path);
  EXPECT_TRUE(background_index.query_symbols("").empty());

  styio::ide::PersistentIndex empty_persistent;
  empty_persistent.save_symbols({});
  EXPECT_TRUE(empty_persistent.load_symbols().empty());

  const std::filesystem::path cache_root = make_temp_project_dir("ide-index");
  styio::ide::PersistentIndex persistent(cache_root.string());
  EXPECT_TRUE(persistent.load_symbols().empty());
  persistent.save_symbols({
    styio::ide::IndexedSymbol{
      path,
      "alpha",
      styio::ide::SymbolKind::Function,
      styio::ide::TextRange{0, 20},
      styio::ide::TextRange{0, 5},
      "alpha(x: i32) -> i64"},
    styio::ide::IndexedSymbol{
      path,
      "param",
      styio::ide::SymbolKind::Parameter,
      styio::ide::TextRange{6, 11},
      styio::ide::TextRange{6, 11},
      "i32"},
    styio::ide::IndexedSymbol{
      path,
      "builtin",
      styio::ide::SymbolKind::Builtin,
      styio::ide::TextRange{12, 19},
      styio::ide::TextRange{12, 19},
      "builtin detail"},
  });
  const auto loaded = persistent.load_symbols();
  ASSERT_EQ(loaded.size(), 3u);
  EXPECT_EQ(loaded[0].kind, styio::ide::SymbolKind::Function);
  EXPECT_EQ(loaded[1].kind, styio::ide::SymbolKind::Parameter);
  EXPECT_EQ(loaded[2].kind, styio::ide::SymbolKind::Builtin);

  write_text_file((cache_root / "symbols.json").string(), "{not-json");
  EXPECT_TRUE(persistent.load_symbols().empty());
  write_text_file((cache_root / "symbols.json").string(), "{\"symbols\":[]}");
  EXPECT_TRUE(persistent.load_symbols().empty());
  write_text_file(
    (cache_root / "symbols.json").string(),
    "[null,{\"path\":\"p\",\"name\":\"mystery\",\"kind\":\"unknown\","
    "\"range_start\":2,\"range_end\":5,\"selection_start\":3,"
    "\"selection_end\":4,\"detail\":\"d\"}]");
  const auto unknown_kind = persistent.load_symbols();
  ASSERT_EQ(unknown_kind.size(), 1u);
  EXPECT_EQ(unknown_kind[0].kind, styio::ide::SymbolKind::Variable);
  EXPECT_EQ(unknown_kind[0].range.start, 2u);
  EXPECT_EQ(unknown_kind[0].selection_range.end, 4u);

  persistent.set_cache_root("");
  persistent.save_symbols(unknown_kind);
  EXPECT_TRUE(persistent.load_symbols().empty());
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

TEST(StyioSyntaxParser, InvalidIncrementalEditsFallBackToFreshTreeSitterParse) {
  const std::string original_text =
    "# add := (a: i32, b: i32) => {\n"
    "  value: i32 := a + b\n"
    "  <| value\n"
    "}\n";

  styio::ide::DocumentSnapshot first_snapshot;
  first_snapshot.file_id = 21;
  first_snapshot.snapshot_id = 1;
  first_snapshot.path = make_temp_dir() + "/invalid_incremental_edit_sample.styio";
  first_snapshot.version = 1;
  first_snapshot.buffer = styio::ide::TextBuffer{original_text};

  styio::ide::SyntaxParser invalid_range_parser;
  const auto first = invalid_range_parser.parse(first_snapshot);

  styio::ide::DocumentSnapshot invalid_range_snapshot = first_snapshot;
  invalid_range_snapshot.snapshot_id = 2;
  invalid_range_snapshot.version = 2;
  invalid_range_snapshot.applied_edits.push_back(
    styio::ide::TextEdit{styio::ide::TextRange{8, 4}, "sum"});
  const auto invalid_range = invalid_range_parser.parse(invalid_range_snapshot);

  styio::ide::SyntaxParser mismatch_parser;
  (void)mismatch_parser.parse(first_snapshot);
  styio::ide::DocumentSnapshot mismatch_snapshot = first_snapshot;
  mismatch_snapshot.snapshot_id = 3;
  mismatch_snapshot.version = 3;
  mismatch_snapshot.applied_edits.push_back(
    styio::ide::TextEdit{styio::ide::TextRange{0, 0}, "// inserted\n"});
  const auto mismatch = mismatch_parser.parse(mismatch_snapshot);

#ifdef STYIO_HAS_TREE_SITTER
  EXPECT_EQ(first.backend, styio::ide::SyntaxBackendKind::TreeSitter);
  EXPECT_EQ(invalid_range.backend, styio::ide::SyntaxBackendKind::TreeSitter);
  EXPECT_EQ(mismatch.backend, styio::ide::SyntaxBackendKind::TreeSitter);
  EXPECT_FALSE(invalid_range.reused_incremental_tree);
  EXPECT_FALSE(mismatch.reused_incremental_tree);
#else
  EXPECT_EQ(first.backend, styio::ide::SyntaxBackendKind::Tolerant);
  EXPECT_EQ(invalid_range.backend, styio::ide::SyntaxBackendKind::Tolerant);
  EXPECT_EQ(mismatch.backend, styio::ide::SyntaxBackendKind::Tolerant);
#endif
  EXPECT_TRUE(invalid_range.diagnostics.empty());
  EXPECT_TRUE(mismatch.diagnostics.empty());
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
  EXPECT_EQ(module.item_by_id(add->id), add);
  EXPECT_EQ(module.item_by_name("add"), add);
  EXPECT_EQ(module.item_by_id(999999), nullptr);
  EXPECT_EQ(module.item_by_name("missing"), nullptr);

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

TEST(StyioHirBuilder, ModelsLegacyFunctionAssignmentScopes) {
  styio::ide::HirIdentityStore identity_store;
  const std::string path = make_temp_dir() + "/hir_legacy_function.styio";
  const std::string source =
    "legacy(a: i32, b: i32) := {\n"
    "  total: i32 := a + b\n"
    "  <| total\n"
    "}\n"
    "out: i32 := legacy(1, 2)\n";

  const auto module = build_hir_for_source(path, source, 26, 1, identity_store);
  const auto* legacy = find_hir_item(module, "legacy", styio::ide::HirItemKind::Function);
  ASSERT_NE(legacy, nullptr);
  ASSERT_TRUE(legacy->scope_id.has_value());
  EXPECT_NE(legacy->fingerprint, 0u);
  EXPECT_NE(legacy->signature_fingerprint, legacy->body_fingerprint);

  const auto* param_a = find_hir_symbol(module, "a", styio::ide::SymbolKind::Parameter);
  const auto* param_b = find_hir_symbol(module, "b", styio::ide::SymbolKind::Parameter);
  ASSERT_NE(param_a, nullptr);
  ASSERT_NE(param_b, nullptr);
  EXPECT_EQ(param_a->scope_id, *legacy->scope_id);
  EXPECT_EQ(param_b->scope_id, *legacy->scope_id);
  ASSERT_TRUE(param_a->item_id.has_value());
  EXPECT_EQ(*param_a->item_id, legacy->id);

  const auto* total = find_hir_symbol(module, "total", styio::ide::SymbolKind::Variable);
  ASSERT_NE(total, nullptr);
  EXPECT_EQ(total->scope_id, *legacy->scope_id);

  const auto* out = find_hir_item(module, "out", styio::ide::HirItemKind::GlobalBinding);
  ASSERT_NE(out, nullptr);

  EXPECT_NE(module.reference_at(source.find("legacy(1")), nullptr);
}

TEST(StyioHirBuilder, ModelsHashLambdaBlockScopes) {
  styio::ide::HirIdentityStore identity_store;
  const std::string path = make_temp_dir() + "/hir_hash_lambda.styio";
  const std::string source =
    "#(item: i32) => {\n"
    "  local: i32 := item\n"
    "}\n";

  const auto module = build_hir_for_source(path, source, 27, 1, identity_store);
  const auto* item = find_hir_symbol(module, "item", styio::ide::SymbolKind::Parameter);
  ASSERT_NE(item, nullptr);
  EXPECT_NE(item->scope_id, 0u);
  EXPECT_FALSE(item->item_id.has_value());

  const auto* local = find_hir_symbol(module, "local", styio::ide::SymbolKind::Variable);
  ASSERT_NE(local, nullptr);
  EXPECT_EQ(local->scope_id, item->scope_id);

  const bool has_lambda_block = std::any_of(
    module.scopes.begin(),
    module.scopes.end(),
    [&](const styio::ide::HirScope& scope)
    {
      return scope.kind == styio::ide::HirScopeKind::Block
        && scope.id == item->scope_id;
    });
  EXPECT_TRUE(has_lambda_block);
  EXPECT_NE(module.reference_at(source.rfind("item")), nullptr);
}

TEST(StyioHirBuilder, CoversFallbackAndMalformedSyntaxEdges) {
  const std::string root = make_temp_dir();

  auto parse_syntax = [](const std::string& path,
                         const std::string& source,
                         styio::ide::FileId file_id,
                         styio::ide::SnapshotId snapshot_id)
  {
    styio::ide::DocumentSnapshot snapshot;
    snapshot.file_id = file_id;
    snapshot.snapshot_id = snapshot_id;
    snapshot.path = path;
    snapshot.version = static_cast<styio::ide::DocumentVersion>(snapshot_id);
    snapshot.buffer = styio::ide::TextBuffer{source};

    styio::ide::SyntaxParser parser;
    return parser.parse(snapshot);
  };

  styio::ide::SemanticSummary empty_semantic;

  const auto empty_syntax = parse_syntax(root + "/hir_empty.styio", "", 31, 1);
  const auto no_store_module = styio::ide::HirBuilder{}.build(empty_syntax, empty_semantic);
  EXPECT_EQ(no_store_module.file_id, 31u);
  EXPECT_TRUE(no_store_module.items.empty());
  ASSERT_EQ(no_store_module.scopes.size(), 1u);
  EXPECT_EQ(no_store_module.scopes.front().kind, styio::ide::HirScopeKind::Module);

  styio::ide::HirIdentityStore identity_store;
  const auto orphan_syntax = parse_syntax(root + "/hir_orphan.styio", "# orphan\n", 32, 1);
  const auto orphan_module = styio::ide::HirBuilder{}.build(orphan_syntax, empty_semantic, identity_store);
  const auto* orphan = find_hir_item(orphan_module, "orphan", styio::ide::HirItemKind::Function);
  ASSERT_NE(orphan, nullptr);
  EXPECT_EQ(orphan->signature_fingerprint, orphan->fingerprint);
  EXPECT_EQ(orphan->body_fingerprint, orphan->fingerprint);

  const auto odd_param_syntax = parse_syntax(
    root + "/hir_odd_param.styio",
    "# odd := (1, value: i32) => value\n",
    33,
    1);
  const auto odd_module = styio::ide::HirBuilder{}.build(odd_param_syntax, empty_semantic, identity_store);
  EXPECT_NE(find_hir_item(odd_module, "odd", styio::ide::HirItemKind::Function), nullptr);
  EXPECT_NE(find_hir_symbol(odd_module, "value", styio::ide::SymbolKind::Parameter), nullptr);

  const auto blank_param_syntax = parse_syntax(
    root + "/hir_blank_param.styio",
    "# blank := (   ) => 1\n",
    40,
    1);
  const auto blank_param_module =
    styio::ide::HirBuilder{}.build(blank_param_syntax, empty_semantic, identity_store);
  const auto* blank = find_hir_item(blank_param_module, "blank", styio::ide::HirItemKind::Function);
  ASSERT_NE(blank, nullptr);
  EXPECT_TRUE(blank->params.empty());

  const auto lambda_expr_syntax = parse_syntax(
    root + "/hir_lambda_expr.styio",
    "#(item: i32) => item\n",
    34,
    1);
  const auto lambda_expr_module = styio::ide::HirBuilder{}.build(lambda_expr_syntax, empty_semantic, identity_store);
  EXPECT_NE(find_hir_symbol(lambda_expr_module, "item", styio::ide::SymbolKind::Parameter), nullptr);
  EXPECT_NE(lambda_expr_module.reference_at(lambda_expr_syntax.buffer.text().rfind("item")), nullptr);

  const auto legacy_expr_syntax = parse_syntax(
    root + "/hir_legacy_expr.styio",
    "legacy(a: i32) := a\n",
    35,
    1);
  const auto legacy_expr_module = styio::ide::HirBuilder{}.build(legacy_expr_syntax, empty_semantic, identity_store);
  EXPECT_NE(find_hir_item(legacy_expr_module, "legacy", styio::ide::HirItemKind::Function), nullptr);
  EXPECT_NE(legacy_expr_module.reference_at(legacy_expr_syntax.buffer.text().rfind("a")), nullptr);

  const auto unmatched_block_syntax = parse_syntax(root + "/hir_unmatched_block.styio", "{\nmissing\n", 36, 1);
  const auto unmatched_block_module = styio::ide::HirBuilder{}.build(unmatched_block_syntax, empty_semantic, identity_store);
  EXPECT_NE(unmatched_block_module.reference_at(unmatched_block_syntax.buffer.text().find("missing")), nullptr);

  styio::ide::SyntaxSnapshot bad_range_syntax;
  bad_range_syntax.file_id = 37;
  bad_range_syntax.snapshot_id = 1;
  bad_range_syntax.path = root + "/hir_bad_range.styio";
  bad_range_syntax.buffer = styio::ide::TextBuffer{""};
  bad_range_syntax.tokens.push_back(styio::ide::SyntaxToken{
    StyioTokenType::TOK_HASH,
    "#",
    styio::ide::TextRange{3, 4}});
  bad_range_syntax.tokens.push_back(styio::ide::SyntaxToken{
    StyioTokenType::NAME,
    "bad_range",
    styio::ide::TextRange{5, 14}});
  const auto bad_range_module = styio::ide::HirBuilder{}.build(bad_range_syntax, empty_semantic, identity_store);
  const auto* bad_range = find_hir_item(bad_range_module, "bad_range", styio::ide::HirItemKind::Function);
  ASSERT_NE(bad_range, nullptr);
  EXPECT_EQ(bad_range->fingerprint, 0u);

  styio::ide::SemanticSummary invalid_semantic;
  invalid_semantic.items.push_back(styio::ide::SemanticItemFact{
    static_cast<styio::ide::SemanticItemKind>(999),
    "fallback",
    {},
    "binding",
    "i32",
    0,
    true});
  const auto invalid_fact_module = styio::ide::HirBuilder{}.build(empty_syntax, invalid_semantic, identity_store);
  EXPECT_NE(find_hir_item(invalid_fact_module, "fallback", styio::ide::HirItemKind::GlobalBinding), nullptr);

  const auto mismatch_syntax = parse_syntax(root + "/hir_semantic_mismatch.styio", "# target := () => 1\n", 38, 1);
  styio::ide::SemanticSummary mismatch_semantic;
  mismatch_semantic.items.push_back(styio::ide::SemanticItemFact{
    styio::ide::SemanticItemKind::Function,
    "other",
    {},
    "other() -> i32",
    "",
    0,
    true});
  mismatch_semantic.items.push_back(styio::ide::SemanticItemFact{
    styio::ide::SemanticItemKind::Function,
    "target",
    {},
    "target() -> i32",
    "",
    0,
    true});
  const auto mismatch_module = styio::ide::HirBuilder{}.build(mismatch_syntax, mismatch_semantic, identity_store);
  const auto* target = find_hir_item(mismatch_module, "target", styio::ide::HirItemKind::Function);
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->detail, "target() -> i32");

  const auto missing_match_syntax = parse_syntax(
    root + "/hir_missing_matching_paren.styio",
    "# missing_match := (value: i32 => value\n",
    39,
    1);
  const auto missing_match_module =
    styio::ide::HirBuilder{}.build(missing_match_syntax, empty_semantic, identity_store);
  EXPECT_NE(find_hir_item(missing_match_module, "missing_match", styio::ide::HirItemKind::Function), nullptr);
  EXPECT_NE(missing_match_module.reference_at(missing_match_syntax.buffer.text().rfind("value")), nullptr);
}

TEST(StyioHirBuilder, ResolvesDuplicateLocalSymbolsToNearestDeclaration) {
  styio::ide::HirIdentityStore identity_store;
  const std::string path = make_temp_dir() + "/hir_duplicate_locals.styio";
  const std::string source =
    "# choose := () => {\n"
    "  value: i32 := 1\n"
    "  value: i32 := 2\n"
    "  result: i32 := value\n"
    "  <| result\n"
    "}\n";

  const auto module = build_hir_for_source(path, source, 40, 1, identity_store);
  const std::size_t value_ref_offset = source.rfind("value");
  ASSERT_NE(value_ref_offset, std::string::npos);
  const auto* reference = module.reference_at(value_ref_offset);
  ASSERT_NE(reference, nullptr);
  ASSERT_TRUE(reference->target_symbol.has_value());
  const auto* target = module.symbol_by_id(*reference->target_symbol);
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->name_range.start, source.find("value: i32 := 2"));
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

TEST(StyioIdeService, FullResyncStatePublishesSyntaxAndSemanticDiagnostics) {
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(make_temp_dir()));

  const std::string uri = temp_uri("full_resync_sample.styio");
  const std::string source = "value: i32 := 1\n";
  EXPECT_TRUE(service.did_open(uri, source, 1).empty());
  service.drain_semantic_diagnostics();

  styio::ide::DocumentDelta delta;
  delta.requires_full_resync = true;
  delta.resync_reason = "malformed content change";

  const auto syntax_diagnostics = service.did_change(uri, delta, 2);
  ASSERT_FALSE(syntax_diagnostics.empty());
  EXPECT_EQ(syntax_diagnostics.back().code, "STYIO_SERVICE_LSP_RESYNC_REQUIRED");
  EXPECT_NE(syntax_diagnostics.back().message.find("malformed content change"), std::string::npos);

  const auto publications = service.drain_semantic_diagnostics();
  ASSERT_EQ(publications.size(), 1u);
  ASSERT_FALSE(publications[0].diagnostics.empty());
  const auto& diagnostic = publications[0].diagnostics.back();
  EXPECT_EQ(diagnostic.code, "STYIO_SERVICE_LSP_RESYNC_REQUIRED");
  EXPECT_EQ(diagnostic.phase, "service");
}

TEST(StyioNameResolver, LocalBindingsShadowImportsAndGlobals) {
  const std::string root = make_temp_project_dir("ide_shadow");
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
  const std::string root = make_temp_project_dir("ide_imports");
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
  const std::string root = make_temp_project_dir("ide_references");
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
  const std::string root = make_temp_project_dir("ide_hover_definition");
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
  const std::string root = make_temp_project_dir("ide_receiver_type");
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
  const std::string root = make_temp_project_dir("ide_expected_type");
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

TEST(StyioSemanticDb, ExposesReceiverAndExpectedTypeQueriesDirectly) {
  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  styio::ide::SemanticDB semdb(vfs, project);
  const std::string path = make_temp_dir() + "/semantic_type_contexts.styio";
  const std::string source =
    "items: list[i32] := [1, 2]\n"
    "# take := (value: string) => value\n"
    "member_result: i32 := items.len\n"
    "call_result: string := take(\"text\")\n";
  vfs.open(path, source, 1);
  semdb.reset_query_stats();

  const std::size_t member_offset = source.find("len");
  ASSERT_NE(member_offset, std::string::npos);
  const auto receiver = semdb.receiver_type_at(path, member_offset);
  ASSERT_TRUE(receiver.has_value());
  EXPECT_TRUE(receiver->known);
  EXPECT_EQ(receiver->receiver_name, "items");
  EXPECT_EQ(receiver->type_name, "list[i32]");

  const auto receiver_again = semdb.receiver_type_at(path, member_offset);
  ASSERT_TRUE(receiver_again.has_value());
  EXPECT_EQ(receiver_again->type_name, receiver->type_name);

  const std::size_t argument_offset = source.find("\"text\"");
  ASSERT_NE(argument_offset, std::string::npos);
  const auto expected = semdb.expected_type_at(path, argument_offset + 1);
  ASSERT_TRUE(expected.has_value());
  EXPECT_TRUE(expected->known);
  EXPECT_EQ(expected->callee_name, "take");
  EXPECT_EQ(expected->param_name, "value");
  EXPECT_EQ(expected->type_name, "string");
  EXPECT_EQ(expected->argument_index, 0u);

  const auto expected_again = semdb.expected_type_at(path, argument_offset + 1);
  ASSERT_TRUE(expected_again.has_value());
  EXPECT_EQ(expected_again->type_name, expected->type_name);

  const auto stats = semdb.query_stats();
  EXPECT_EQ(stats.receiver_type.misses, 1u);
  EXPECT_EQ(stats.receiver_type.hits, 1u);
  EXPECT_EQ(stats.expected_type.misses, 1u);
  EXPECT_EQ(stats.expected_type.hits, 1u);
}

TEST(StyioSemanticDb, CompletionScoringCoversExactPrefixesAndTypeAliases) {
  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  styio::ide::SemanticDB semdb(vfs, project);
  const std::string path = make_temp_dir() + "/semantic_completion_scores.styio";
  const std::string source =
    "# make_str : str := () => \"hi\"\n"
    "# make_int : int := () => 1\n"
    "# make_long : long := () => 1\n"
    "# make_float : float := () => 1.0\n"
    "# make_double : double := () => 1.0\n"
    "# take_string := (value: string) => value\n"
    "# take_i32 := (value: i32) => value\n"
    "# take_i64 := (value: i64) => value\n"
    "# take_f32 := (value: f32) => value\n"
    "# take_f64 := (value: f64) => value\n"
    "# take_pair := (first: i32, second: string) => second\n"
    "exact: i32 := 1\n"
    "exact_result: i32 := exact\n"
    "str_result: string := take_string()\n"
    "int_result: i32 := take_i32()\n"
    "long_result: i64 := take_i64()\n"
    "float_result: f32 := take_f32()\n"
    "double_result: f64 := take_f64()\n"
    "pair_result: string := take_pair((1), \"x\")\n";

  vfs.open(path, source, 1);

  const std::size_t exact_offset = source.find("exact\n");
  ASSERT_NE(exact_offset, std::string::npos);
  const auto exact_completion = semdb.complete_at(path, exact_offset + std::string("exact").size());
  ASSERT_FALSE(exact_completion.empty());
  EXPECT_EQ(exact_completion.front().label, "exact");

  auto expect_alias_completion = [&](const std::string& call, const std::string& expected_type, const std::string& label)
  {
    const std::size_t call_offset = source.rfind(call);
    ASSERT_NE(call_offset, std::string::npos) << call;
    const std::size_t arg_offset = call_offset + call.size();
    const auto context = semdb.completion_context_at(path, arg_offset);
    EXPECT_EQ(context.expected_type_name, expected_type);

    const auto completion = semdb.complete_at(path, arg_offset);
    EXPECT_LT(completion_index(completion, label), completion.size()) << label;
  };

  expect_alias_completion("take_string(", "string", "make_str");
  expect_alias_completion("take_i32(", "i32", "make_int");
  expect_alias_completion("take_i64(", "i64", "make_long");
  expect_alias_completion("take_f32(", "f32", "make_float");
  expect_alias_completion("take_f64(", "f64", "make_double");

  const std::size_t pair_arg_offset = source.find("\"x\"");
  ASSERT_NE(pair_arg_offset, std::string::npos);
  const auto pair_expected = semdb.expected_type_at(path, pair_arg_offset + 1);
  ASSERT_TRUE(pair_expected.has_value());
  EXPECT_TRUE(pair_expected->known);
  EXPECT_EQ(pair_expected->argument_index, 1u);
  EXPECT_EQ(pair_expected->param_name, "second");
}

TEST(StyioSemanticDb, CompletionSeesParentScopeSymbolsFromNestedBlocks) {
  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  styio::ide::SemanticDB semdb(vfs, project);
  const std::string path = make_temp_dir() + "/semantic_parent_scope_completion.styio";
  const std::string source =
    "# outer := (needle: i32) => {\n"
    "  {\n"
    "    nee\n"
    "  }\n"
    "}\n";

  vfs.open(path, source, 1);

  const std::size_t completion_offset = source.find("nee\n");
  ASSERT_NE(completion_offset, std::string::npos);
  const auto completion = semdb.complete_at(path, completion_offset + 3);

  EXPECT_TRUE(has_completion_label(completion, "needle"));
}

TEST(StyioSemanticDb, EmptyFilesReturnEmptyTypeQueryResults) {
  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  styio::ide::SemanticDB semdb(vfs, project);

  const std::string empty_path = make_temp_dir() + "/semantic_empty_queries.styio";
  vfs.open(empty_path, "\n", 1);
  EXPECT_FALSE(semdb.type_signature_at(empty_path, 0).has_value());
  EXPECT_FALSE(semdb.type_body_at(empty_path, 0).has_value());
}

TEST(StyioSemanticDb, CoversTypeContextFallbackEdges) {
  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  styio::ide::SemanticDB semdb(vfs, project);
  const std::string path = make_temp_dir() + "/semantic_type_context_edges.styio";
  const std::string source =
    "# orphan\n"
    "# recurse := (n: i32) => recurse(n)\n"
    "# one := (value: i32) => value\n"
    "items: list[i32] := [1, 2]\n"
    "member_result: i32 := items.len\n"
    "grouped: i32 := (1)\n"
    "unknown_result: i32 := missing(1)\n"
    "extra_result: i32 := one(1, 2)\n";

  vfs.open(path, source, 1);

  const std::size_t orphan_offset = source.find("orphan");
  ASSERT_NE(orphan_offset, std::string::npos);
  const auto orphan_signature = semdb.type_signature_at(path, orphan_offset);
  ASSERT_TRUE(orphan_signature.has_value());
  EXPECT_EQ(orphan_signature->return_type_name, "unknown");

  const std::size_t recurse_call_offset = source.rfind("recurse(n)");
  ASSERT_NE(recurse_call_offset, std::string::npos);
  const auto recurse_body = semdb.type_body_at(path, recurse_call_offset);
  ASSERT_TRUE(recurse_body.has_value());
  EXPECT_FALSE(recurse_body->dependency_identity_keys.empty());

  const std::size_t grouped_arg_offset = source.find("(1)");
  ASSERT_NE(grouped_arg_offset, std::string::npos);
  EXPECT_FALSE(semdb.expected_type_at(path, grouped_arg_offset + 1).has_value());

  const std::size_t missing_arg_offset = source.find("missing(1)");
  ASSERT_NE(missing_arg_offset, std::string::npos);
  EXPECT_FALSE(semdb.expected_type_at(path, missing_arg_offset + std::string("missing(").size()).has_value());

  const std::size_t extra_arg_offset = source.find("one(1, 2)");
  ASSERT_NE(extra_arg_offset, std::string::npos);
  EXPECT_FALSE(semdb.expected_type_at(path, extra_arg_offset + std::string("one(1, ").size()).has_value());

  const std::string receiver_path = make_temp_dir() + "/semantic_receiver_context_edges.styio";
  const std::string receiver_source =
    "items: list[i32] := [1, 2]\n"
    "member_result: i32 := items.len\n";
  vfs.open(receiver_path, receiver_source, 1);
  const std::size_t member_dot_offset = receiver_source.find(".len");
  ASSERT_NE(member_dot_offset, std::string::npos);
  const auto receiver_after_dot = semdb.receiver_type_at(receiver_path, member_dot_offset + 1);
  ASSERT_TRUE(receiver_after_dot.has_value());
  EXPECT_EQ(receiver_after_dot->type_name, "list[i32]");

  const auto receiver_inside_member = semdb.receiver_type_at(receiver_path, member_dot_offset + 2);
  ASSERT_TRUE(receiver_inside_member.has_value());
  EXPECT_EQ(receiver_inside_member->type_name, "list[i32]");

  const std::string dangling_path = make_temp_dir() + "/semantic_dangling_receiver_context.styio";
  const std::string dangling_source =
    "items: list[i32] := [1]\n"
    "member_result := items.";
  vfs.open(dangling_path, dangling_source, 1);
  const auto dangling_receiver = semdb.receiver_type_at(dangling_path, dangling_source.size() + 4);
  ASSERT_TRUE(dangling_receiver.has_value());
  EXPECT_EQ(dangling_receiver->type_name, "list[i32]");

  EXPECT_FALSE(semdb.receiver_type_at(receiver_path, 0).has_value());
}

TEST(StyioSemanticDb, WhiteBoxImportCandidatesAndSymbolItemsCoverEdges) {
  const std::string root = make_temp_project_dir("semantic_private_edges");
  const std::filesystem::path importer_dir = std::filesystem::path(root) / "src";
  std::filesystem::create_directories(importer_dir);

  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  project.set_root(root);
  styio::ide::SemanticDB semdb(vfs, project);

  styio::ide::DocumentSnapshot document;
  document.path = (importer_dir / "main.styio").string();
  document.buffer = styio::ide::TextBuffer{""};

  EXPECT_TRUE(semdb.import_candidate_paths(document, "").empty());

  const auto absolute_spec = (std::filesystem::path(root) / "absolute_mod").lexically_normal();
  const auto absolute_candidates = semdb.import_candidate_paths(document, absolute_spec.string());
  ASSERT_EQ(absolute_candidates.size(), 2u);
  EXPECT_EQ(absolute_candidates[0], absolute_spec.string());
  EXPECT_EQ(absolute_candidates[1], (absolute_spec.string() + ".styio"));

  const auto relative_candidates = semdb.import_candidate_paths(document, "./local_mod");
  ASSERT_EQ(relative_candidates.size(), 2u);
  EXPECT_EQ(relative_candidates[0], (importer_dir / "local_mod").lexically_normal().string());
  EXPECT_EQ(relative_candidates[1], (importer_dir / "local_mod.styio").lexically_normal().string());

  const auto dotted_candidates = semdb.import_candidate_paths(document, "pkg.math");
  ASSERT_EQ(dotted_candidates.size(), 4u);
  EXPECT_EQ(dotted_candidates[0], (std::filesystem::path(root) / "pkg/math").lexically_normal().string());
  EXPECT_EQ(dotted_candidates[1], (std::filesystem::path(root) / "pkg/math.styio").lexically_normal().string());
  EXPECT_EQ(dotted_candidates[2], (importer_dir / "pkg/math").lexically_normal().string());
  EXPECT_EQ(dotted_candidates[3], (importer_dir / "pkg/math.styio").lexically_normal().string());

  styio::ide::HirModule hir;
  hir.items.push_back(styio::ide::HirItem{
    7,
    styio::ide::HirItemKind::GlobalBinding,
    "target",
    styio::ide::TextRange{0, 6},
    styio::ide::TextRange{0, 18}});

  styio::ide::HirSymbol unbound_symbol;
  EXPECT_EQ(semdb.item_for_symbol(hir, unbound_symbol), nullptr);

  styio::ide::HirSymbol bound_symbol;
  bound_symbol.item_id = 7;
  const auto* bound_item = semdb.item_for_symbol(hir, bound_symbol);
  ASSERT_NE(bound_item, nullptr);
  EXPECT_EQ(bound_item->name, "target");

  const std::string source = "value: i32 := 1\n";
  auto opened = vfs.open(document.path, source, 1);
  styio::ide::SemanticDB::ResolvedName resolved;
  resolved.kind = styio::ide::SemanticDB::ResolvedNameKind::Symbol;
  resolved.name = "mismatched";
  resolved.path = document.path;
  resolved.selection_range = styio::ide::TextRange{1, 2};
  resolved.has_location = true;

  const auto* fallback_item = semdb.item_for_resolved_name(*opened, resolved);
  ASSERT_NE(fallback_item, nullptr);
  EXPECT_EQ(fallback_item->name, "value");

  styio::ide::CompletionContext incomplete_list_context;
  incomplete_list_context.position_kind = styio::ide::PositionKind::MemberAccess;
  incomplete_list_context.receiver_type_name = "list[]";
  incomplete_list_context.prefix = "f";
  const auto incomplete_list_members = semdb.builtin_items(incomplete_list_context);
  const auto first_member = std::find_if(
    incomplete_list_members.begin(),
    incomplete_list_members.end(),
    [](const styio::ide::CompletionItem& item)
    {
      return item.label == "first";
    });
  ASSERT_NE(first_member, incomplete_list_members.end());
  EXPECT_TRUE(first_member->type_name.empty());

  styio::ide::DocumentSnapshot manual_document;
  manual_document.file_id = 701;
  manual_document.snapshot_id = 1;
  manual_document.path = (std::filesystem::path(root) / "manual_signature.styio").string();
  manual_document.buffer = styio::ide::TextBuffer{""};
  styio::ide::HirModule manual_hir;
  styio::ide::HirItem manual_item;
  manual_item.id = 17;
  manual_item.name = "manual";
  manual_item.detail = "manual() -> double   ";
  const auto& manual_signature = semdb.type_signature_query(manual_document, manual_hir, manual_item);
  EXPECT_EQ(manual_signature.return_type_name, "double");

  const std::filesystem::path cache_root = std::filesystem::path(root) / "persistent-cache";
  styio::ide::PersistentIndex persistent(cache_root.string());
  const std::string persistent_path = (std::filesystem::path(root) / "persistent-only.styio").string();
  persistent.save_symbols({
    styio::ide::IndexedSymbol{
      persistent_path,
      "persisted_same",
      styio::ide::SymbolKind::Variable,
      styio::ide::TextRange{20, 30},
      styio::ide::TextRange{20, 30},
      "late"},
    styio::ide::IndexedSymbol{
      persistent_path,
      "persisted_same",
      styio::ide::SymbolKind::Variable,
      styio::ide::TextRange{5, 15},
      styio::ide::TextRange{5, 15},
      "early"},
  });
  semdb.configure_cache_root(cache_root.string());
  const auto persistent_symbols = semdb.workspace_symbols("persisted_same");
  ASSERT_EQ(persistent_symbols.size(), 2u);
  EXPECT_EQ(persistent_symbols[0].selection_range.start, 5u);
  EXPECT_EQ(persistent_symbols[1].selection_range.start, 20u);
}

TEST(StyioSemanticDb, BuiltinQueriesUseLocationlessTargets) {
  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  styio::ide::SemanticDB semdb(vfs, project);
  const std::string path = make_temp_dir() + "/semantic_builtin_targets.styio";
  const std::string source =
    "first := stdin\n"
    "second := stdin\n"
    "call := stdin()\n";

  vfs.open(path, source, 1);
  const std::size_t first_stdin = source.find("stdin");
  const std::size_t call_stdin = source.find("stdin()");
  ASSERT_NE(first_stdin, std::string::npos);
  ASSERT_NE(call_stdin, std::string::npos);

  EXPECT_TRUE(semdb.definition_at(path, first_stdin).empty());
  const auto references = semdb.references_of(path, first_stdin);
  EXPECT_GE(references.size(), 2u);

  const auto expected = semdb.expected_type_at(path, call_stdin + std::string("stdin(").size());
  EXPECT_FALSE(expected.has_value());
}

TEST(StyioCompletionEngine, FiltersTypePositionCandidates) {
  const std::string root = make_temp_project_dir("ide_type_position");
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
  const std::string root = make_temp_project_dir("ide_ranking");
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

TEST(StyioCompletionEngine, ReplacesOuterCompletionWithCloserDuplicate) {
  const std::string root = make_temp_project_dir("ide_duplicate_completion");
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(root));

  const std::string uri = styio::ide::uri_from_path((std::filesystem::path(root) / "duplicates.styio").string());
  const std::string source =
    "shadow: i32 := 1\n"
    "# use := (shadow: string) => {\n"
    "  sha\n"
    "}\n";
  service.did_open(uri, source, 1);

  styio::ide::TextBuffer buffer(source);
  const std::size_t completion_offset = source.find("sha\n");
  ASSERT_NE(completion_offset, std::string::npos);
  const auto completion = service.completion(uri, buffer.position_at(completion_offset + 3));
  const std::size_t shadow_index = completion_index(completion, "shadow");
  ASSERT_LT(shadow_index, completion.size());
  EXPECT_EQ(completion[shadow_index].detail, "parameter");
}

TEST(StyioCompletionEngine, FiltersMembersByReceiverType) {
  const std::string root = make_temp_project_dir("ide_member_filter");
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

  const std::string dict_uri = styio::ide::uri_from_path((std::filesystem::path(root) / "dict_members.styio").string());
  const std::string dict_source =
    "lookup: dict[string,i32] := dict{\"a\": 1}\n"
    "result := lookup.";
  service.did_open(dict_uri, dict_source, 1);

  styio::ide::TextBuffer dict_buffer(dict_source);
  const auto dict_completion = service.completion(dict_uri, dict_buffer.position_at(dict_source.size()));

  EXPECT_TRUE(has_completion_label(dict_completion, "len"));
  EXPECT_TRUE(has_completion_label(dict_completion, "keys"));
  EXPECT_TRUE(has_completion_label(dict_completion, "values"));
  EXPECT_FALSE(has_completion_label(dict_completion, "first"));
  EXPECT_FALSE(has_completion_label(dict_completion, "last"));

  const std::size_t keys_index = completion_index(dict_completion, "keys");
  const std::size_t values_index = completion_index(dict_completion, "values");
  ASSERT_LT(keys_index, dict_completion.size());
  ASSERT_LT(values_index, dict_completion.size());
  EXPECT_EQ(dict_completion[keys_index].type_name, "list[string]");
  EXPECT_EQ(dict_completion[values_index].type_name, "list[unknown]");

  const std::string string_uri = styio::ide::uri_from_path((std::filesystem::path(root) / "string_members.styio").string());
  const std::string string_source =
    "word: string := \"hi\"\n"
    "result := word.";
  service.did_open(string_uri, string_source, 1);

  styio::ide::TextBuffer string_buffer(string_source);
  const auto string_completion = service.completion(string_uri, string_buffer.position_at(string_source.size()));

  EXPECT_TRUE(has_completion_label(string_completion, "len"));
  EXPECT_FALSE(has_completion_label(string_completion, "first"));
  EXPECT_FALSE(has_completion_label(string_completion, "keys"));

  const std::string scalar_uri = styio::ide::uri_from_path((std::filesystem::path(root) / "scalar_members.styio").string());
  const std::string scalar_source =
    "count: i32 := 1\n"
    "result := count.";
  service.did_open(scalar_uri, scalar_source, 1);

  styio::ide::TextBuffer scalar_buffer(scalar_source);
  const auto scalar_completion = service.completion(scalar_uri, scalar_buffer.position_at(scalar_source.size()));

  EXPECT_TRUE(has_completion_label(scalar_completion, "len"));
  EXPECT_FALSE(has_completion_label(scalar_completion, "first"));
  EXPECT_FALSE(has_completion_label(scalar_completion, "keys"));
}

TEST(StyioCompletionEngine, UsesExpectedTypesAtCallSites) {
  const std::string root = make_temp_project_dir("ide_expected_type_completion");
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

  const std::string applied_source =
    "word: string := \"hi\"\n"
    "count: i32 := 1\n"
    "# take := (value: string) => value\n"
    "result: string := take(word)";
  service.did_change(uri, applied_source, 2);
  styio::ide::TextBuffer applied_buffer(applied_source);
  const std::size_t word_arg_offset = applied_source.rfind("word");
  ASSERT_NE(word_arg_offset, std::string::npos);

  const auto hover = service.hover(uri, applied_buffer.position_at(word_arg_offset));
  ASSERT_TRUE(hover.has_value());
  EXPECT_NE(hover->contents.find("Type: `string`"), std::string::npos);
  EXPECT_NE(hover->contents.find("Expected: `string`"), std::string::npos);
}

TEST(StyioCompletionEngine, RecoversInBrokenSyntax) {
  const std::string root = make_temp_project_dir("ide_recovery");
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
  const std::string root = make_temp_project_dir("ide_workspace_symbols");
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
  const std::string root = make_temp_project_dir("ide_index_definition");
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
  const std::string root = make_temp_project_dir("ide_index_references");
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
  const std::string root = make_temp_project_dir("ide_persistent_index");
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
  const std::string root = make_temp_project_dir("ide_closed_file_refresh");
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

TEST(StyioWorkspaceIndex, DirectFileIndexingErasesDeletedBackgroundSymbols) {
  const std::string root = make_temp_project_dir("ide_direct_deleted_index");
  const std::string path = (std::filesystem::path(root) / "deleted.styio").string();
  write_text_file(path, "# vanishing_symbol := (x: i32) => x\n");

  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  project.set_root(root);
  styio::ide::SemanticDB semdb(vfs, project);

  semdb.index_workspace_file(path);
  EXPECT_TRUE(has_indexed_symbol(semdb.workspace_symbols("vanishing_symbol"), "vanishing_symbol", path));

  std::filesystem::remove(path);
  semdb.index_workspace_file(path);
  EXPECT_FALSE(has_indexed_symbol(semdb.workspace_symbols("vanishing_symbol"), "vanishing_symbol", path));
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

TEST(StyioSemanticDb, ReusesDefinitionReferenceAndCompletionContextQueriesWithinSnapshot) {
  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  styio::ide::SemanticDB semdb(vfs, project);
  const std::string path = make_temp_dir() + "/semantic_navigation_cache.styio";
  const std::string source =
    "# add := (a: i32, b: i32) => a + b\n"
    "result: i32 := add(1, 2)\n";

  vfs.open(path, source, 1);
  semdb.reset_query_stats();

  const std::size_t definition_offset = source.find("add");
  const std::size_t call_offset = source.rfind("add(");
  const std::size_t number_offset = source.find("1, 2");
  ASSERT_NE(definition_offset, std::string::npos);
  ASSERT_NE(call_offset, std::string::npos);
  ASSERT_NE(number_offset, std::string::npos);

  const auto first_definition = semdb.definition_at(path, call_offset);
  ASSERT_EQ(first_definition.size(), 1u);
  EXPECT_EQ(first_definition[0].path, path);
  EXPECT_EQ(first_definition[0].range.start, definition_offset);
  const auto after_first_definition = semdb.query_stats();

  const auto second_definition = semdb.definition_at(path, call_offset);
  EXPECT_EQ(second_definition.size(), first_definition.size());
  const auto after_second_definition = semdb.query_stats();

  EXPECT_EQ(after_first_definition.definition.misses, 1u);
  EXPECT_EQ(after_second_definition.definition.hits, 1u);
  EXPECT_EQ(after_second_definition.definition.misses, after_first_definition.definition.misses);

  const auto first_references = semdb.references_of(path, definition_offset);
  EXPECT_TRUE(has_location(first_references, path, call_offset));
  const auto after_first_references = semdb.query_stats();

  const auto second_references = semdb.references_of(path, definition_offset);
  EXPECT_EQ(second_references.size(), first_references.size());
  const auto after_second_references = semdb.query_stats();

  EXPECT_EQ(after_first_references.references.misses, 1u);
  EXPECT_EQ(after_second_references.references.hits, 1u);
  EXPECT_EQ(after_second_references.references.misses, after_first_references.references.misses);

  const auto first_empty_references = semdb.references_of(path, number_offset);
  EXPECT_TRUE(first_empty_references.empty());
  const auto after_first_empty_references = semdb.query_stats();

  const auto second_empty_references = semdb.references_of(path, number_offset);
  EXPECT_TRUE(second_empty_references.empty());
  const auto after_second_empty_references = semdb.query_stats();

  EXPECT_EQ(after_first_empty_references.references.misses, after_second_references.references.misses + 1);
  EXPECT_EQ(after_second_empty_references.references.hits, after_first_empty_references.references.hits + 1);

  const auto first_context = semdb.completion_context_at(path, call_offset + 2);
  EXPECT_EQ(first_context.prefix, "ad");
  const auto after_first_context = semdb.query_stats();

  const auto second_context = semdb.completion_context_at(path, call_offset + 2);
  EXPECT_EQ(second_context.prefix, first_context.prefix);
  const auto after_second_context = semdb.query_stats();

  EXPECT_EQ(after_first_context.completion_context.misses, 1u);
  EXPECT_EQ(after_second_context.completion_context.hits, 1u);
  EXPECT_EQ(after_second_context.completion_context.misses, after_first_context.completion_context.misses);
}

TEST(StyioSemanticDb, MemberCompletionInfersReceiverTypeFromImportedFunctionSignature) {
  const std::string root = make_temp_project_dir("semantic_imported_receiver_signature");
  const std::filesystem::path lib_path = std::filesystem::path(root) / "math.styio";
  const std::string lib_source =
    "# numbers : list[i32] := () => [1, 2]\n";
  write_text_file(lib_path.string(), lib_source);

  styio::ide::VirtualFileSystem vfs;
  styio::ide::Project project;
  project.set_root(root);
  styio::ide::SemanticDB semdb(vfs, project);

  const std::string main_path = (std::filesystem::path(root) / "main.styio").string();
  const std::string source =
    "@import { math }\n"
    "value: i32 := numbers.len\n";
  vfs.open(main_path, source, 1);

  const std::size_t member_offset = source.find("len");
  ASSERT_NE(member_offset, std::string::npos);
  const auto receiver = semdb.receiver_type_at(main_path, member_offset);
  ASSERT_TRUE(receiver.has_value());
  EXPECT_TRUE(receiver->known);
  EXPECT_EQ(receiver->receiver_name, "numbers");
  EXPECT_EQ(receiver->type_name, "list[i32]");

  const auto completion = semdb.complete_at(main_path, member_offset);
  EXPECT_TRUE(has_completion_label(completion, "len"));
  EXPECT_TRUE(has_completion_label(completion, "first"));
  EXPECT_TRUE(has_completion_label(completion, "last"));
  EXPECT_FALSE(has_completion_label(completion, "keys"));
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

TEST(StyioSemanticBridge, RejectsMalformedInputWithoutRecovery) {
  const std::string source =
    "# broken := (a: i32, b: i32) => {\n"
    "  value: i32 := a +\n"
    "}\n"
    "# stable := (x: i32, y: i32) => x + y\n"
    "result: i32 := stable(1, 2)\n";

  const auto summary = styio::ide::analyze_document("memory://recovery_sample.styio", source);
  EXPECT_FALSE(summary.parse_success);
  EXPECT_FALSE(summary.used_recovery);
  EXPECT_FALSE(summary.diagnostics.empty());
  EXPECT_EQ(summary.diagnostics.front().source, "styio-compiler");
  EXPECT_EQ(summary.diagnostics.front().code, "STYIO_PARSE_UNEXPECTED_TOKEN");
  EXPECT_EQ(summary.diagnostics.front().phase, "parse");

  const auto it = summary.function_signatures.find("stable");
  EXPECT_EQ(it, summary.function_signatures.end());
}

TEST(StyioSemanticBridge, CoversCompilerBridgeLexTupleAndResourceFacts) {
  const auto lex_summary = styio::ide::analyze_document(
    "memory://compiler_bridge_lex.styio",
    "/* unterminated\n");
  EXPECT_FALSE(lex_summary.parse_success);
  ASSERT_FALSE(lex_summary.diagnostics.empty());
  EXPECT_EQ(lex_summary.diagnostics.front().source, "styio-compiler");
  EXPECT_EQ(lex_summary.diagnostics.front().phase, "lex");
  EXPECT_NE(lex_summary.diagnostics.front().code.find("LEX"), std::string::npos);

  const auto tuple_summary = styio::ide::analyze_document(
    "memory://compiler_bridge_tuple.styio",
    "# pair : (i64, f64) := (x: i64, y: f64) => x\n");
  EXPECT_TRUE(tuple_summary.parse_success);
  ASSERT_FALSE(tuple_summary.diagnostics.empty());
  EXPECT_EQ(tuple_summary.diagnostics.front().phase, "type");
  const auto signature = tuple_summary.function_signatures.find("pair");
  ASSERT_NE(signature, tuple_summary.function_signatures.end());
  EXPECT_NE(signature->second.find("pair(x: i64, y: f64) -> (i64, f64)"), std::string::npos);

  const auto resource_summary = styio::ide::analyze_document(
    "memory://compiler_bridge_resource.styio",
    "@(slot <- seed)\n");
  EXPECT_TRUE(resource_summary.parse_success);
  const bool has_resource = std::any_of(
    resource_summary.items.begin(),
    resource_summary.items.end(),
    [](const styio::ide::SemanticItemFact& item)
    {
      return item.kind == styio::ide::SemanticItemKind::Resource
        && item.name == "slot"
        && item.detail == "resource";
    });
  EXPECT_TRUE(has_resource);
}

TEST(StyioLspServer, PublishDiagnosticsCarriesStyioCodeAndPhase) {
  styio::lsp::Server server;
  const std::string root_uri = styio::ide::uri_from_path(make_temp_dir());
  const std::string uri = temp_uri("server_diagnostic_code_sample.styio");

  auto init_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 1},
    {"method", "initialize"},
    {"params", llvm::json::Object{{"rootUri", root_uri}}}});
  ASSERT_EQ(init_messages.size(), 1u);

  auto open_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didOpen"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{
          {"uri", uri},
          {"version", 1},
          {"text", "/* unterminated\n"}}}}}});
  ASSERT_EQ(open_messages.size(), 1u);
  EXPECT_EQ(open_messages[0].payload.getString("method").value_or(""), "textDocument/publishDiagnostics");

  const auto* params = open_messages[0].payload.getObject("params");
  ASSERT_NE(params, nullptr);
  const auto* diagnostics = params->getArray("diagnostics");
  ASSERT_NE(diagnostics, nullptr);
  ASSERT_FALSE(diagnostics->empty());
  const auto* diagnostic = (*diagnostics)[0].getAsObject();
  ASSERT_NE(diagnostic, nullptr);
  EXPECT_EQ(diagnostic->getString("source").value_or(""), "styio-editor");
  EXPECT_EQ(diagnostic->getString("code").value_or(""), "STYIO_SERVICE_EDITOR_SYNTAX");
  const auto* data = diagnostic->getObject("data");
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(data->getString("phase").value_or(""), "service");
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

TEST(StyioLspServer, MemberAccessCompletionUsesPropertyKind) {
  styio::lsp::Server server;
  const std::string root_uri = styio::ide::uri_from_path(make_temp_dir());
  const std::string uri = temp_uri("server_member_completion_sample.styio");

  ASSERT_EQ(
    server.handle(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", 1},
      {"method", "initialize"},
      {"params", llvm::json::Object{{"rootUri", root_uri}}}})
      .size(),
    1u);

  auto open_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didOpen"},
    {"params", llvm::json::Object{
      {"textDocument", llvm::json::Object{
        {"uri", uri},
        {"version", 1},
        {"text", "items := [1, 2]\nmember := items.\n"}}}}}});
  ASSERT_EQ(open_messages.size(), 1u);

  auto completion_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 2},
    {"method", "textDocument/completion"},
    {"params", llvm::json::Object{
      {"textDocument", llvm::json::Object{{"uri", uri}}},
      {"position", lsp_position(1, 16)}}}});
  ASSERT_EQ(completion_messages.size(), 1u);
  const auto* result = completion_messages[0].payload.getArray("result");
  ASSERT_NE(result, nullptr);

  const bool found_len_property = std::any_of(
    result->begin(),
    result->end(),
    [](const llvm::json::Value& item)
    {
      const auto* object = item.getAsObject();
      return object != nullptr
        && object->getString("label").value_or("") == "len"
        && object->getInteger("kind").value_or(0) == 10;
    });
  EXPECT_TRUE(found_len_property);
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

TEST(StyioLspServer, Utf16CrLfAndIncrementalPositionEdgesStayExplicit) {
  styio::lsp::Server server;
  const std::string root_uri = styio::ide::uri_from_path(make_temp_dir());
  const std::string uri = temp_uri("server_utf16_crlf_sample.styio");
  const std::string source =
    "\xC3\xA9"
    "\xE4\xB8\xAD"
    "\xF0\x9D\x84\x9E"
    "x\r\n"
    "# fn := (p: i32) => p\n"
    "value: i32 := fn(1)\n";

  ASSERT_EQ(
    server.handle(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", 1},
      {"method", "initialize"},
      {"params", llvm::json::Object{{"rootUri", root_uri}}}})
      .size(),
    1u);

  auto open_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didOpen"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{
          {"uri", uri},
          {"version", 1},
          {"text", source}}}}}});
  ASSERT_EQ(open_messages.size(), 1u);

  auto completion_after_wide_chars = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 2},
    {"method", "textDocument/completion"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}}},
       {"position", lsp_position(0, 2)}}}});
  ASSERT_EQ(completion_after_wide_chars.size(), 1u);
  EXPECT_NE(completion_after_wide_chars[0].payload.getArray("result"), nullptr);

  auto invalid_surrogate_position = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 3},
    {"method", "textDocument/hover"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}}},
       {"position", lsp_position(0, 3)}}}});
  ASSERT_EQ(invalid_surrogate_position.size(), 1u);
  EXPECT_NE(invalid_surrogate_position[0].payload.get("result"), nullptr);

  llvm::json::Array changes;
  changes.push_back(llvm::json::Object{
    {"range", lsp_range(0, 4, 0, 5)},
    {"text", "y"}});
  auto change_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didChange"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}, {"version", 2}}},
       {"contentChanges", std::move(changes)}}}});
  ASSERT_EQ(change_messages.size(), 1u);
  EXPECT_EQ(change_messages[0].payload.getString("method").value_or(""), "textDocument/publishDiagnostics");

  auto symbols = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 4},
    {"method", "textDocument/documentSymbol"},
    {"params", llvm::json::Object{{"textDocument", llvm::json::Object{{"uri", uri}}}}}});
  ASSERT_EQ(symbols.size(), 1u);
  EXPECT_NE(symbols[0].payload.getArray("result"), nullptr);
}

TEST(StyioLspServer, HandlesNavigationWorkspaceTokensAndEdgeRequests) {
  styio::lsp::Server server;
  const std::string root_uri = styio::ide::uri_from_path(make_temp_dir());
  const std::string uri = temp_uri("server_navigation_sample.styio");

  auto init_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", -7},
    {"method", "initialize"},
    {"params", llvm::json::Object{{"rootUri", root_uri}}}});
  ASSERT_EQ(init_messages.size(), 1u);

  const std::string source =
    "# add := (a: i32, b: i32) => a + b\n"
    "emoji_text := \"\xF0\x9F\x98\x80\"\n"
    "result: i32 := add(1, 2)\n"
    "typed_value: ";
  auto open_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didOpen"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{
          {"uri", uri},
          {"version", 1},
          {"text", source}}}}}});
  ASSERT_EQ(open_messages.size(), 1u);

  auto type_completion_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 2},
    {"method", "textDocument/completion"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}}},
       {"position", lsp_position(3, 13)}}}});
  ASSERT_EQ(type_completion_messages.size(), 1u);
  const auto* type_completion = type_completion_messages[0].payload.getArray("result");
  ASSERT_NE(type_completion, nullptr);
  const bool found_i32_type = std::any_of(
    type_completion->begin(),
    type_completion->end(),
    [](const llvm::json::Value& item)
    {
      const auto* object = item.getAsObject();
      return object != nullptr
        && object->getString("label").value_or("") == "i32"
        && object->getInteger("kind").value_or(0) == 7;
    });
  EXPECT_TRUE(found_i32_type);

  auto hover_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", "3"},
    {"method", "textDocument/hover"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}}},
       {"position", lsp_position(2, 16)}}}});
  ASSERT_EQ(hover_messages.size(), 1u);
  const auto* hover = hover_messages[0].payload.getObject("result");
  ASSERT_NE(hover, nullptr);
  EXPECT_NE(hover->getString("contents").value_or("").find("add"), std::string::npos);
  EXPECT_NE(hover->getObject("range"), nullptr);

  auto emoji_hover_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 4},
    {"method", "textDocument/hover"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}}},
       {"position", lsp_position(1, 17)}}}});
  ASSERT_EQ(emoji_hover_messages.size(), 1u);
  EXPECT_NE(emoji_hover_messages[0].payload.get("result"), nullptr);

  auto missing_line_hover = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 5},
    {"method", "textDocument/hover"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}}},
       {"position", lsp_position(99, 0)}}}});
  ASSERT_EQ(missing_line_hover.size(), 1u);
  EXPECT_NE(missing_line_hover[0].payload.get("result"), nullptr);

  auto definition_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 6},
    {"method", "textDocument/definition"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}}},
       {"position", lsp_position(2, 16)}}}});
  ASSERT_EQ(definition_messages.size(), 1u);
  const auto* definitions = definition_messages[0].payload.getArray("result");
  ASSERT_NE(definitions, nullptr);
  ASSERT_FALSE(definitions->empty());

  auto reference_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 7},
    {"method", "textDocument/references"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}}},
       {"position", lsp_position(2, 16)}}}});
  ASSERT_EQ(reference_messages.size(), 1u);
  const auto* references = reference_messages[0].payload.getArray("result");
  ASSERT_NE(references, nullptr);

  auto workspace_symbols = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 8},
    {"method", "workspace/symbol"},
    {"params", llvm::json::Object{{"query", "add"}}}});
  ASSERT_EQ(workspace_symbols.size(), 1u);
  const auto* workspace_result = workspace_symbols[0].payload.getArray("result");
  ASSERT_NE(workspace_result, nullptr);
  EXPECT_FALSE(workspace_result->empty());

  auto semantic_tokens = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 9},
    {"method", "textDocument/semanticTokens/full"},
    {"params", llvm::json::Object{{"textDocument", llvm::json::Object{{"uri", uri}}}}}});
  ASSERT_EQ(semantic_tokens.size(), 1u);
  const auto* semantic_result = semantic_tokens[0].payload.getObject("result");
  ASSERT_NE(semantic_result, nullptr);
  EXPECT_NE(semantic_result->getArray("data"), nullptr);

  auto cancel_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "$/cancelRequest"},
    {"params", llvm::json::Object{{"id", "123"}}}});
  EXPECT_TRUE(cancel_messages.empty());

  auto null_response = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 10},
    {"method", "styio/unknown"}});
  ASSERT_EQ(null_response.size(), 1u);
  EXPECT_NE(null_response[0].payload.get("result"), nullptr);

  llvm::json::Array bad_changes;
  bad_changes.push_back(llvm::json::Object{
    {"range", lsp_range(99, 0, 99, 1)},
    {"text", "x"}});
  auto bad_change_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didChange"},
    {"params", llvm::json::Object{
       {"textDocument", llvm::json::Object{{"uri", uri}, {"version", 2}}},
       {"contentChanges", std::move(bad_changes)}}}});
  ASSERT_EQ(bad_change_messages.size(), 1u);
  EXPECT_EQ(
    bad_change_messages[0].payload.getString("method").value_or(""),
    "textDocument/publishDiagnostics");

  auto close_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didClose"},
    {"params", llvm::json::Object{{"textDocument", llvm::json::Object{{"uri", uri}}}}}});
  EXPECT_TRUE(close_messages.empty());
}

TEST(StyioLspServer, WorkspaceSymbolMapsPersistentParameterAndBuiltinKinds) {
  EnvVarGuard xdg_cache_home("XDG_CACHE_HOME");
  EnvVarGuard home("HOME");

  const std::filesystem::path root = make_temp_project_dir("lsp_persistent_symbol_kinds");
  const std::filesystem::path cache_home = root / "cache-home";
  xdg_cache_home.set(cache_home.string());
  home.set((root / "home").string());

  styio::lsp::Server server;
  ASSERT_EQ(
    server.handle(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", 1},
      {"method", "initialize"},
      {"params", llvm::json::Object{{"rootUri", styio::ide::uri_from_path(root.string())}}}})
      .size(),
    1u);

  const std::filesystem::path target_dir = std::filesystem::path(make_temp_dir()) / "lsp-persistent-targets";
  std::filesystem::create_directories(target_dir);
  const std::filesystem::path target_path = target_dir / "persistent_kinds_target.styio";
  const std::string target_text =
    "persistent_builtin_lsp\n"
    "persistent_param_lsp\n";
  write_text_file(target_path.string(), target_text);

  const std::size_t builtin_start = target_text.find("persistent_builtin_lsp");
  const std::size_t param_start = target_text.find("persistent_param_lsp");
  ASSERT_NE(builtin_start, std::string::npos);
  ASSERT_NE(param_start, std::string::npos);

  const std::filesystem::path cache_root =
    cache_home / "styio" / "ide" / std::to_string(std::hash<std::string>{}(root.lexically_normal().string()));
  styio::ide::PersistentIndex(cache_root.string()).save_symbols({
    styio::ide::IndexedSymbol{
      target_path.string(),
      "persistent_builtin_lsp",
      styio::ide::SymbolKind::Builtin,
      styio::ide::TextRange{builtin_start, builtin_start + std::string("persistent_builtin_lsp").size()},
      styio::ide::TextRange{builtin_start, builtin_start + std::string("persistent_builtin_lsp").size()},
      "builtin detail"},
    styio::ide::IndexedSymbol{
      target_path.string(),
      "persistent_param_lsp",
      styio::ide::SymbolKind::Parameter,
      styio::ide::TextRange{param_start, param_start + std::string("persistent_param_lsp").size()},
      styio::ide::TextRange{param_start, param_start + std::string("persistent_param_lsp").size()},
      "parameter detail"},
  });

  auto workspace_symbols = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 2},
    {"method", "workspace/symbol"},
    {"params", llvm::json::Object{{"query", "persistent_"}}}});
  ASSERT_EQ(workspace_symbols.size(), 1u);
  const auto* result = workspace_symbols[0].payload.getArray("result");
  ASSERT_NE(result, nullptr);

  bool found_builtin = false;
  bool found_parameter = false;
  for (const auto& item : *result) {
    const auto* object = item.getAsObject();
    ASSERT_NE(object, nullptr);
    if (object->getString("name").value_or("") == "persistent_builtin_lsp") {
      found_builtin = object->getInteger("kind").value_or(0) == 14;
    }
    if (object->getString("name").value_or("") == "persistent_param_lsp") {
      found_parameter = object->getInteger("kind").value_or(0) == 13;
    }
  }
  EXPECT_TRUE(found_builtin);
  EXPECT_TRUE(found_parameter);
}

TEST(StyioLspServer, CoversTransportReaderAndMalformedRequestEdges) {
  {
    styio::lsp::Server server;
    std::istringstream input("Content-Length: 16777217\r\n\r\n");
    std::ostringstream output;
    server.run(input, output);
    EXPECT_TRUE(output.str().empty());
  }

  {
    styio::lsp::Server server;
    const std::string root_uri = styio::ide::uri_from_path(make_temp_dir());
    constexpr std::size_t kOversizedContentLength = 16u * 1024u * 1024u + 1u;
    const std::string oversized_body(kOversizedContentLength, 'x');
    std::string transport_input = "Content-Length: " + std::to_string(kOversizedContentLength) + "\r\n";
    transport_input += oversized_body;
    transport_input += lsp_frame(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", 1},
      {"method", "initialize"},
      {"params", llvm::json::Object{{"rootUri", root_uri}}}});

    std::istringstream input(transport_input);
    std::ostringstream output;
    server.run(input, output);
    EXPECT_NE(output.str().find("\"capabilities\""), std::string::npos);
  }

  {
    styio::lsp::Server server;
    std::istringstream input("Content-Length: 999999999999999999999999999999\r\n\r\n");
    std::ostringstream output;
    server.run(input, output);
    EXPECT_TRUE(output.str().empty());
  }

  {
    styio::lsp::Server server;
    std::istringstream input("Content-Length: 8\r\n\r\n{}");
    std::ostringstream output;
    server.run(input, output);
    EXPECT_TRUE(output.str().empty());
  }

  {
    styio::lsp::Server server;
    std::string transport_input = "Content-Length: 7\r\n\r\nnotjson";
    transport_input += "Content-Length: 2\r\n\r\n[]";
    transport_input += lsp_frame(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", 1},
      {"method", "initialize"},
      {"params", llvm::json::Object{{"rootUri", styio::ide::uri_from_path(make_temp_dir())}}}});

    std::istringstream input(transport_input);
    std::ostringstream output;
    server.run(input, output);
    EXPECT_NE(output.str().find("\"capabilities\""), std::string::npos);
  }

  styio::lsp::Server server;
  const std::string root_uri = styio::ide::uri_from_path(make_temp_dir());
  const std::string uri = temp_uri("server_malformed_request_edges.styio");
  ASSERT_EQ(
    server.handle(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"id", 2},
      {"method", "initialize"},
      {"params", llvm::json::Object{{"rootUri", root_uri}}}})
      .size(),
    1u);
  ASSERT_EQ(
    server.handle(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", "textDocument/didOpen"},
      {"params", llvm::json::Object{
        {"textDocument", llvm::json::Object{
          {"uri", uri},
          {"version", 1},
          {"text",
           "# add := (a: i32, b: i32) => a + b\n"
           "value := add(1, 2)\n"}}}}}})
      .size(),
    1u);

  auto completion_notification = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/completion"},
    {"params", llvm::json::Object{
      {"textDocument", llvm::json::Object{{"uri", uri}}},
      {"position", lsp_position(1, 9)}}}});
  EXPECT_TRUE(completion_notification.empty());

  auto missing_completion_target = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 3},
    {"method", "textDocument/completion"},
    {"params", llvm::json::Object{}}});
  ASSERT_EQ(missing_completion_target.size(), 1u);
  ASSERT_NE(missing_completion_target[0].payload.getArray("result"), nullptr);
  EXPECT_TRUE(missing_completion_target[0].payload.getArray("result")->empty());

  auto completion_object_id = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", llvm::json::Object{{"opaque", true}}},
    {"method", "textDocument/completion"},
    {"params", llvm::json::Object{
      {"textDocument", llvm::json::Object{{"uri", uri}}},
      {"position", lsp_position(1, 9)}}}});
  ASSERT_EQ(completion_object_id.size(), 1u);
  ASSERT_NE(completion_object_id[0].payload.getArray("result"), nullptr);

  auto hover_non_numeric_string_id = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", "not-digits"},
    {"method", "textDocument/hover"},
    {"params", llvm::json::Object{
      {"textDocument", llvm::json::Object{{"uri", uri}}},
      {"position", lsp_position(1, 9)}}}});
  ASSERT_EQ(hover_non_numeric_string_id.size(), 1u);
  EXPECT_NE(hover_non_numeric_string_id[0].payload.get("result"), nullptr);

  auto hover_missing_position = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 4},
    {"method", "textDocument/hover"},
    {"params", llvm::json::Object{{"textDocument", llvm::json::Object{{"uri", uri}}}}}});
  ASSERT_EQ(hover_missing_position.size(), 1u);
  EXPECT_NE(hover_missing_position[0].payload.get("result"), nullptr);

  auto hover_past_line_end = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 40},
    {"method", "textDocument/hover"},
    {"params", llvm::json::Object{
      {"textDocument", llvm::json::Object{{"uri", uri}}},
      {"position", lsp_position(1, 999)}}}});
  ASSERT_EQ(hover_past_line_end.size(), 1u);
  EXPECT_NE(hover_past_line_end[0].payload.get("result"), nullptr);

  auto definition_notification = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/definition"},
    {"params", llvm::json::Object{
      {"textDocument", llvm::json::Object{{"uri", uri}}},
      {"position", lsp_position(1, 9)}}}});
  EXPECT_TRUE(definition_notification.empty());

  auto document_symbol_missing_target = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 5},
    {"method", "textDocument/documentSymbol"},
    {"params", llvm::json::Object{}}});
  ASSERT_EQ(document_symbol_missing_target.size(), 1u);
  ASSERT_NE(document_symbol_missing_target[0].payload.getArray("result"), nullptr);
  EXPECT_TRUE(document_symbol_missing_target[0].payload.getArray("result")->empty());

  auto semantic_tokens_missing_target = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 6},
    {"method", "textDocument/semanticTokens/full"},
    {"params", llvm::json::Object{}}});
  ASSERT_EQ(semantic_tokens_missing_target.size(), 1u);
  const auto* semantic_tokens = semantic_tokens_missing_target[0].payload.getObject("result");
  ASSERT_NE(semantic_tokens, nullptr);
  ASSERT_NE(semantic_tokens->getArray("data"), nullptr);
  EXPECT_TRUE(semantic_tokens->getArray("data")->empty());

  auto fallback_unknown = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"id", 7},
    {"method", "styio/unknown"},
    {"params", llvm::json::Object{}}});
  ASSERT_EQ(fallback_unknown.size(), 1u);
  EXPECT_NE(fallback_unknown[0].payload.get("result"), nullptr);

  llvm::json::Array malformed_changes;
  malformed_changes.push_back("not-an-object");
  auto malformed_change_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didChange"},
    {"params", llvm::json::Object{
      {"textDocument", llvm::json::Object{{"uri", uri}, {"version", 2}}},
      {"contentChanges", std::move(malformed_changes)}}}});
  ASSERT_EQ(malformed_change_messages.size(), 1u);
  EXPECT_EQ(
    malformed_change_messages[0].payload.getString("method").value_or(""),
    "textDocument/publishDiagnostics");

  llvm::json::Array missing_end_range_changes;
  missing_end_range_changes.push_back(llvm::json::Object{
    {"range", llvm::json::Object{{"start", lsp_position(0, 0)}}},
    {"text", "x"}});
  auto missing_end_range_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didChange"},
    {"params", llvm::json::Object{
      {"textDocument", llvm::json::Object{{"uri", uri}, {"version", 3}}},
      {"contentChanges", std::move(missing_end_range_changes)}}}});
  ASSERT_EQ(missing_end_range_messages.size(), 1u);
  EXPECT_EQ(
    missing_end_range_messages[0].payload.getString("method").value_or(""),
    "textDocument/publishDiagnostics");

  llvm::json::Array reversed_range_changes;
  reversed_range_changes.push_back(llvm::json::Object{
    {"range", lsp_range(0, 8, 0, 4)},
    {"text", "x"}});
  auto reversed_range_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didChange"},
    {"params", llvm::json::Object{
      {"textDocument", llvm::json::Object{{"uri", uri}, {"version", 4}}},
      {"contentChanges", std::move(reversed_range_changes)}}}});
  ASSERT_EQ(reversed_range_messages.size(), 1u);
  EXPECT_EQ(
    reversed_range_messages[0].payload.getString("method").value_or(""),
    "textDocument/publishDiagnostics");

  llvm::json::Array mixed_changes;
  mixed_changes.push_back(llvm::json::Object{{"text", "value := 1\n"}});
  mixed_changes.push_back(llvm::json::Object{
    {"range", lsp_range(0, 8, 0, 9)},
    {"text", "2"}});
  auto mixed_change_messages = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "textDocument/didChange"},
    {"params", llvm::json::Object{
      {"textDocument", llvm::json::Object{{"uri", uri}, {"version", 5}}},
      {"contentChanges", std::move(mixed_changes)}}}});
  ASSERT_EQ(mixed_change_messages.size(), 1u);
  EXPECT_EQ(
    mixed_change_messages[0].payload.getString("method").value_or(""),
    "textDocument/publishDiagnostics");

  auto cancel_without_params = server.handle(llvm::json::Object{
    {"jsonrpc", "2.0"},
    {"method", "$/cancelRequest"}});
  EXPECT_TRUE(cancel_without_params.empty());
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
  const std::string root = make_temp_project_dir("ide_request_driven_background");
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
  const std::string root = make_temp_project_dir("ide_background");
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
  const std::string root = make_temp_project_dir("ide_idle_slice");
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

  const auto canceled_references_ticket =
    service.begin_foreground_request(uri, styio::ide::RuntimeRequestKind::References, 79);
  service.cancel_request(79);
  const auto canceled_references =
    service.references(canceled_references_ticket, styio::ide::Position{1, 16});
  EXPECT_TRUE(canceled_references.empty());

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
  EXPECT_EQ(service.runtime_counters().canceled_requests, 2u);
  EXPECT_EQ(service.runtime_counters().stale_request_drops, 1u);
}

TEST(StyioCompletionEngine, SurvivesMalformedBinaryOperandsFromFuzzRegression) {
  styio::ide::IdeService service;
  service.initialize(styio::ide::uri_from_path(make_temp_dir()));

  const std::string uri = temp_uri("ide_completion_fuzz_regression.styio");
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

  const std::string corpus_root =
    (std::filesystem::path("tests") / "ide" / "corpus" / "completion_regression").string();
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
      {"binding:items", "binding:count"},
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
      false,
      ""});
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
      {},
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
      false,
      "editor token snapshots are not grammar authority for malformed source"});
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
      EXPECT_FALSE(drift_case.approved_exception.empty());
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
  const std::string parse_path = make_temp_dir() + "/ide_perf_incremental.styio";
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
  const std::string root = make_temp_project_dir("ide_perf_workspace_hot");
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
    const std::string startup_root = make_temp_project_dir("ide_perf_startup_" + std::to_string(i));
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
