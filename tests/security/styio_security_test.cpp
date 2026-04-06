/**
 * Security-focused regression tests for the Styio toolchain.
 *
 * These tests encode expectations around untrusted/malformed source input and
 * runtime helper behaviour. Some expectations document *current* behaviour
 * (e.g. exceptions) until the lexer reports structured errors.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <cstdlib>
#include <future>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "StyioException/Exception.hpp"
#include "StyioExtern/ExternLib.hpp"
#include "StyioParser/NewParserExpr.hpp"
#include "StyioParser/ParserLookahead.hpp"
#include "StyioParser/Parser.hpp"
#include "StyioParser/Tokenizer.hpp"
#include "StyioRuntime/HandleTable.hpp"
#include "StyioSession/CompilationSession.hpp"
#include "StyioUnicode/Unicode.hpp"

namespace {

class CountingExprAST : public StyioAST
{
  int* dtor_count_ = nullptr;

public:
  explicit CountingExprAST(int* dtor_count) :
      dtor_count_(dtor_count) {
  }

  ~CountingExprAST() override {
    if (dtor_count_ != nullptr) {
      *dtor_count_ += 1;
    }
  }

  const StyioNodeType getNodeType() const override {
    return StyioNodeType::None;
  }

  const StyioDataType getDataType() const override {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }

  std::string toString(StyioRepr* visitor, int indent = 0) override {
    (void)visitor;
    (void)indent;
    return "counting-expr";
  }

  void typeInfer(StyioAnalyzer* visitor) override {
    (void)visitor;
  }

  StyioIR* toStyioIR(StyioAnalyzer* visitor) override {
    (void)visitor;
    return nullptr;
  }
};

class CountingNameAST : public NameAST
{
  int* dtor_count_ = nullptr;

public:
  CountingNameAST(const std::string& name, int* dtor_count) :
      NameAST(name), dtor_count_(dtor_count) {
  }

  ~CountingNameAST() override {
    if (dtor_count_ != nullptr) {
      *dtor_count_ += 1;
    }
  }
};

class CountingVarAST : public VarAST
{
  int* dtor_count_ = nullptr;

public:
  explicit CountingVarAST(int* dtor_count) :
      VarAST(NameAST::Create("v")), dtor_count_(dtor_count) {
  }

  ~CountingVarAST() override {
    if (dtor_count_ != nullptr) {
      *dtor_count_ += 1;
    }
  }
};

class CountingTypeAST : public TypeAST
{
  int* dtor_count_ = nullptr;

public:
  CountingTypeAST(const std::string& type_name, int* dtor_count) :
      TypeAST(type_name), dtor_count_(dtor_count) {
  }

  ~CountingTypeAST() override {
    if (dtor_count_ != nullptr) {
      *dtor_count_ += 1;
    }
  }
};

void
free_tokens(std::vector<StyioToken*>& tokens) {
  for (auto* t : tokens) {
    delete t;
  }
}

std::vector<std::pair<size_t, size_t>>
build_line_seps(const std::string& src) {
  std::vector<std::pair<size_t, size_t>> seps;
  size_t line_start = 0;
  size_t line_len = 0;
  for (size_t i = 0; i < src.size(); ++i) {
    if (src[i] == '\n') {
      seps.push_back(std::make_pair(line_start, line_len));
      line_start = i + 1;
      line_len = 0;
    }
    else {
      line_len += 1;
    }
  }
  if (!src.empty() && src.back() != '\n') {
    seps.push_back(std::make_pair(line_start, line_len));
  }
  return seps;
}

std::string
parse_expr_to_repr(const std::string& source, bool use_new_parser) {
  // Parse expression prefix and stop before a non-expression sentinel token.
  const std::string wrapped = source + " @";
  auto tokens = StyioTokenizer::tokenize(wrapped);
  StyioContext* ctx = StyioContext::Create(
    "<expr-subset-test>",
    wrapped,
    build_line_seps(wrapped),
    tokens,
    false);

  StyioAST* ast = nullptr;
  auto cleanup = [&]() {
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  };

  try {
    ast = use_new_parser ? parse_expr_new_subset(*ctx) : parse_expr(*ctx);
    ctx->skip();
    if (ctx->cur_tok_type() != StyioTokenType::TOK_AT) {
      throw std::runtime_error("expression parser did not stop at sentinel");
    }

    StyioRepr repr;
    const std::string out = ast->toString(&repr);
    cleanup();
    return out;
  } catch (...) {
    cleanup();
    throw;
  }
}

std::string
parse_program_to_repr(const std::string& source, bool use_new_parser) {
  auto tokens = StyioTokenizer::tokenize(source);
  StyioContext* ctx = StyioContext::Create(
    "<stmt-subset-test>",
    source,
    build_line_seps(source),
    tokens,
    false);

  MainBlockAST* ast = nullptr;
  auto cleanup = [&]() {
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  };

  try {
    ast = use_new_parser ? parse_main_block_new_subset(*ctx) : parse_main_block(*ctx);
    ctx->skip();
    if (ctx->cur_tok_type() != StyioTokenType::TOK_EOF) {
      throw std::runtime_error("statement parser did not consume input");
    }

    StyioRepr repr;
    const std::string out = ast->toString(&repr);
    cleanup();
    return out;
  } catch (...) {
    cleanup();
    throw;
  }
}

std::string
parse_program_engine_to_repr(const std::string& source, StyioParserEngine engine) {
  auto tokens = StyioTokenizer::tokenize(source);
  StyioContext* ctx = StyioContext::Create(
    "<engine-shadow-test>",
    source,
    build_line_seps(source),
    tokens,
    false);

  MainBlockAST* ast = nullptr;
  auto cleanup = [&]() {
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  };

  try {
    ast = parse_main_block_with_engine(*ctx, engine);
    ctx->skip();
    if (ctx->cur_tok_type() != StyioTokenType::TOK_EOF) {
      throw std::runtime_error("engine parser did not consume input");
    }

    StyioRepr repr;
    const std::string out = ast->toString(&repr);
    cleanup();
    return out;
  } catch (...) {
    cleanup();
    throw;
  }
}
} // namespace

TEST(StyioSecurityLexer, EmptySourceProducesEof) {
  auto tokens = StyioTokenizer::tokenize("");
  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens.back()->type, StyioTokenType::TOK_EOF);
  free_tokens(tokens);
}

TEST(StyioSecurityLexer, UnterminatedStringThrowsLexError) {
  // Unterminated string must produce a structured lexer error, not UB/crash.
  EXPECT_THROW(
    {
      auto tokens = StyioTokenizer::tokenize(R"(print "unterminated)");
      free_tokens(tokens);
    },
    StyioLexError);
}

TEST(StyioSecurityLexer, UnterminatedBlockCommentThrowsLexError) {
  EXPECT_THROW(
    {
      auto tokens = StyioTokenizer::tokenize("a /* no closing");
      free_tokens(tokens);
    },
    StyioLexError);
}

TEST(StyioSecurityParserContext, EmptyTokenVectorFallsBackToEofToken) {
  std::vector<StyioToken*> tokens;
  StyioContext* ctx = StyioContext::Create(
    "<empty-token-context>",
    "",
    {{0, 0}},
    tokens,
    false);

  EXPECT_EQ(ctx->cur_tok_type(), StyioTokenType::TOK_EOF);
  EXPECT_FALSE(ctx->match(StyioTokenType::NAME));
  EXPECT_FALSE(ctx->try_match(StyioTokenType::NAME));
  EXPECT_EQ(ctx->mark_cur_tok("empty-token-context"), "empty-token-context");

  bool map_matched = false;
  EXPECT_NO_THROW({
    map_matched = ctx->map_match(StyioTokenType::BINOP_EQ);
  });
  EXPECT_FALSE(map_matched);

  delete ctx;
}

TEST(StyioSecurityParserContext, MoveForwardBeyondTokenTailIsClampedToEof) {
  auto tokens = StyioTokenizer::tokenize("x");
  StyioContext* ctx = StyioContext::Create(
    "<move-forward-clamp>",
    "x",
    {{0, 1}},
    tokens,
    false);

  EXPECT_NO_THROW({
    ctx->move_forward(tokens.size() + 5, "security-clamp");
  });
  EXPECT_EQ(ctx->cur_tok_type(), StyioTokenType::TOK_EOF);

  delete ctx;
  free_tokens(tokens);
}

TEST(StyioSecurityParserContext, CharApiAtEofReturnsSafeDefaults) {
  std::vector<StyioToken*> tokens;
  StyioContext* ctx = StyioContext::Create(
    "<char-api-eof>",
    "",
    {},
    tokens,
    false);

  EXPECT_FALSE(ctx->check_next('x'));
  EXPECT_FALSE(ctx->check_next("//"));
  EXPECT_FALSE(ctx->check_ahead(1, 'x'));
  EXPECT_FALSE(ctx->peak_isdigit(0));
  EXPECT_FALSE(ctx->check_isal_());
  EXPECT_FALSE(ctx->check_isalnum_());
  EXPECT_FALSE(ctx->check_isdigit());

  EXPECT_NO_THROW({
    ctx->drop_all_spaces();
    ctx->drop_all_spaces_comments();
    ctx->move_until('x');
    ctx->move_until("abc");
  });
  EXPECT_EQ(ctx->cur_tok_type(), StyioTokenType::TOK_EOF);

  delete ctx;
}

TEST(StyioSecurityParserContext, PeakOperatorAtEofReturnsEofWithoutThrow) {
  std::vector<StyioToken*> tokens;
  StyioContext* ctx = StyioContext::Create(
    "<peak-operator-eof>",
    "",
    {},
    tokens,
    false);

  std::string op;
  EXPECT_NO_THROW({
    op = ctx->peak_operator();
  });
  EXPECT_EQ(op, "EOF");

  delete ctx;
}

TEST(StyioSecurityParserContext, FindDropPanicAtEofReportsSyntaxError) {
  std::vector<StyioToken*> tokens;
  StyioContext* ctx = StyioContext::Create(
    "<find-drop-panic-eof>",
    "",
    {},
    tokens,
    false);

  EXPECT_THROW(
    {
      ctx->find_drop_panic(')');
    },
    StyioSyntaxError);

  delete ctx;
}

TEST(StyioSecurityParserPath, SingleLetterPathDoesNotThrowOutOfRange) {
  std::vector<StyioToken*> tokens;
  const std::string src = "\"A\"";
  StyioContext* ctx = StyioContext::Create(
    "<single-letter-path>",
    src,
    build_line_seps(src),
    tokens,
    false);

  StyioAST* path_ast = nullptr;
  EXPECT_NO_THROW({
    path_ast = parse_path(*ctx);
  });
  ASSERT_NE(path_ast, nullptr);
  EXPECT_EQ(path_ast->getNodeType(), StyioNodeType::LocalPath);
  auto* local_path = dynamic_cast<ResPathAST*>(path_ast);
  ASSERT_NE(local_path, nullptr);
  EXPECT_EQ(local_path->getPath(), "A");

  delete path_ast;
  delete ctx;
}

TEST(StyioSecurityLexer, LineCommentAtEofWithoutNewlineDoesNotThrow) {
  auto tokens = StyioTokenizer::tokenize("x // eof-no-newline");
  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens.back()->type, StyioTokenType::TOK_EOF);
  free_tokens(tokens);
}

TEST(StyioSecurityLexer, EmbeddedNulByteDoesNotHang) {
  // If the inner switch hits `default: break` without advancing `loc`, tokenization
  // never terminates (denial-of-service). Expect completion within a short budget.
  std::string src = "a";
  src.push_back('\0');
  src += 'b';

  auto fut = std::async(std::launch::async, [&src] {
    return StyioTokenizer::tokenize(src);
  });
  const auto deadline = std::chrono::milliseconds(800);
  ASSERT_EQ(fut.wait_for(deadline), std::future_status::ready)
    << "Lexer should finish; hung input likely stuck on embedded NUL (loc not advanced).";

  auto tokens = fut.get();
  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens.back()->type, StyioTokenType::TOK_EOF);
  free_tokens(tokens);
}

TEST(StyioSecurityLexer, VeryLongIdentifierCompletes) {
  std::string id(200'000, 'a');
  auto tokens = StyioTokenizer::tokenize(id);
  ASSERT_EQ(tokens.size(), 2u);
  EXPECT_EQ(tokens[0]->type, StyioTokenType::NAME);
  EXPECT_EQ(tokens[0]->original.size(), 200'000u);
  EXPECT_EQ(tokens[1]->type, StyioTokenType::TOK_EOF);
  free_tokens(tokens);
}

TEST(StyioSecurityParserLookahead, SkipTriviaFindsNextToken) {
  auto tokens = StyioTokenizer::tokenize("   // cmt\nfoo");
  ASSERT_FALSE(tokens.empty());

  const size_t idx = styio_skip_trivia_tokens(tokens, 0);
  ASSERT_LT(idx, tokens.size());
  EXPECT_EQ(tokens[idx]->type, StyioTokenType::NAME);
  EXPECT_EQ(tokens[idx]->original, "foo");

  EXPECT_TRUE(styio_try_check_non_trivia(tokens, 0, StyioTokenType::NAME));
  EXPECT_FALSE(styio_try_check_non_trivia(tokens, 0, StyioTokenType::INTEGER));
  free_tokens(tokens);
}

TEST(StyioSecurityNewParserExpr, MatchesLegacyOnSubsetSamples) {
  const std::vector<std::string> samples = {
    "1 + 2 * 3",
    "(1 + 2) * 3",
    "price + fee - tax",
    "\"x\" + \"y\"",
    "-5 + 2 ** 3",
  };

  for (const auto& src : samples) {
    try {
      EXPECT_EQ(parse_expr_to_repr(src, true), parse_expr_to_repr(src, false)) << src;
    } catch (const std::exception& ex) {
      FAIL() << "sample '" << src << "' threw: " << ex.what();
    }
  }
}

TEST(StyioSecurityNewParserExpr, SubsetTokenGateIncludesCompareAndLogic) {
  EXPECT_TRUE(styio_new_parser_is_expr_subset_token(StyioTokenType::BINOP_GT));
  EXPECT_TRUE(styio_new_parser_is_expr_subset_token(StyioTokenType::BINOP_GE));
  EXPECT_TRUE(styio_new_parser_is_expr_subset_token(StyioTokenType::BINOP_LT));
  EXPECT_TRUE(styio_new_parser_is_expr_subset_token(StyioTokenType::BINOP_LE));
  EXPECT_TRUE(styio_new_parser_is_expr_subset_token(StyioTokenType::TOK_RANGBRAC));
  EXPECT_TRUE(styio_new_parser_is_expr_subset_token(StyioTokenType::TOK_LANGBRAC));
  EXPECT_TRUE(styio_new_parser_is_expr_subset_token(StyioTokenType::BINOP_EQ));
  EXPECT_TRUE(styio_new_parser_is_expr_subset_token(StyioTokenType::BINOP_NE));
  EXPECT_TRUE(styio_new_parser_is_expr_subset_token(StyioTokenType::LOGIC_AND));
  EXPECT_TRUE(styio_new_parser_is_expr_subset_token(StyioTokenType::LOGIC_OR));
}

TEST(StyioSecurityNewParserExpr, SubsetTokenGateIncludesDotCallTokens) {
  EXPECT_TRUE(styio_new_parser_is_expr_subset_token(StyioTokenType::TOK_LPAREN));
  EXPECT_TRUE(styio_new_parser_is_expr_subset_token(StyioTokenType::TOK_RPAREN));
  EXPECT_TRUE(styio_new_parser_is_expr_subset_token(StyioTokenType::TOK_DOT));
}

TEST(StyioSecurityNewParserExpr, RejectsNonSubsetStatementToken) {
  auto tokens = StyioTokenizer::tokenize(">_ 1 + 2");
  StyioContext* ctx = StyioContext::Create(
    "<expr-subset-test>",
    ">_ 1 + 2",
    build_line_seps(">_ 1 + 2"),
    tokens,
    false);

  EXPECT_THROW(
    {
      StyioAST* ast = parse_expr_new_subset(*ctx);
      delete ast;
    },
    StyioSyntaxError);

  delete ctx;
  free_tokens(tokens);
}

TEST(StyioSecurityNewParserStmt, MatchesLegacyOnPrintSubsetSamples) {
  const std::vector<std::string> samples = {
    ">_(1 + 2)\n",
    ">_(\"x\", 1 + 2, (3 * 4))\n",
    ">_(1 + 2)\n>_(3 + 4)\n",
    "1 + 2\n>_(3 * 4)\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr(src, true), parse_program_to_repr(src, false)) << src;
  }
}

TEST(StyioSecurityNewParserStmt, SubsetTokenGateIncludesFunctionDefTokens) {
  EXPECT_TRUE(styio_new_parser_is_stmt_subset_token(StyioTokenType::TOK_HASH));
  EXPECT_TRUE(styio_new_parser_is_stmt_subset_token(StyioTokenType::ARROW_DOUBLE_RIGHT));
  EXPECT_TRUE(styio_new_parser_is_stmt_subset_token(StyioTokenType::TOK_LCURBRAC));
  EXPECT_TRUE(styio_new_parser_is_stmt_subset_token(StyioTokenType::TOK_RCURBRAC));
  EXPECT_TRUE(styio_new_parser_is_stmt_subset_token(StyioTokenType::EXTRACTOR));
  EXPECT_TRUE(styio_new_parser_is_stmt_subset_token(StyioTokenType::BOUNDED_BUFFER_OPEN));
  EXPECT_TRUE(styio_new_parser_is_stmt_subset_token(StyioTokenType::BOUNDED_BUFFER_CLOSE));
  EXPECT_TRUE(styio_new_parser_is_stmt_subset_token(StyioTokenType::MATCH));
  EXPECT_TRUE(styio_new_parser_is_stmt_subset_token(StyioTokenType::TOK_UNDLINE));
  EXPECT_TRUE(styio_new_parser_is_stmt_subset_token(StyioTokenType::ITERATOR));
}

TEST(StyioSecurityNewParserStmt, MatchesLegacyOnFlexBindSubsetSamples) {
  const std::vector<std::string> samples = {
    "x = 1 + 2\n>_(x)\n",
    "price = 1 + 2 * 3\n>_(price)\n",
    "a = 1\nb = a + 2\n>_(b)\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr(src, true), parse_program_to_repr(src, false)) << src;
  }
}

TEST(StyioSecurityNewParserStmt, MatchesLegacyOnFinalBindSubsetSamples) {
  const std::vector<std::string> samples = {
    "x : i32 := 100\n>_(x)\n",
    "price: f64 := 1 + 2\n>_(price)\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr(src, true), parse_program_to_repr(src, false)) << src;
  }
}

TEST(StyioSecurityNewParserStmt, MatchesLegacyOnCompoundAssignSubsetSamples) {
  const std::vector<std::string> samples = {
    "x = 10\nx += 5\n>_(x)\n",
    "a = 20\na -= 3\n>_(a)\n",
    "m = 4\nm *= 2\n>_(m)\n",
    "q = 9\nq /= 3\n>_(q)\n",
    "r = 9\nr %= 4\n>_(r)\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr(src, true), parse_program_to_repr(src, false)) << src;
  }
}

TEST(StyioSecurityNewParserStmt, MatchesLegacyOnCompareAndLogicSubsetSamples) {
  const std::vector<std::string> samples = {
    "lhs = 3\nrhs = 2\n>_(lhs > rhs)\n",
    "a = 1\nb = 1\n>_(a <= b)\n",
    "x = 7\ny = 7\n>_(x == y)\n",
    "ok = true\nready = false\n>_(ok && ready)\n",
    "hot = false\ncold = true\n>_(hot || cold)\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr(src, true), parse_program_to_repr(src, false)) << src;
  }
}

TEST(StyioSecurityNewParserStmt, MatchesLegacyOnSimpleCallSubsetSamples) {
  const std::vector<std::string> samples = {
    "foo(1)\n",
    "sum(1, 2, 3)\n",
    "x = add(1, 2)\n>_(x)\n",
    ">_(mul(2, 3))\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr(src, true), parse_program_to_repr(src, false)) << src;
  }
}

TEST(StyioSecurityNewParserStmt, MatchesLegacyOnFunctionDefSubsetSamples) {
  const std::vector<std::string> samples = {
    "# add := (a: i32, b: i32) => a + b\n>_(add(3, 4))\n",
    "# answer := () => 42\n>_(answer())\n",
    "# pulse : [|3|] = (x: i32) => x\n>_(pulse(5))\n",
    "# pair : (i32, [|2|]) = (a: i32, b: i32) => a + b\n>_(pair(1, 2))\n",
    "# mix(a: i32, b: i32) : i32 = a + b\n>_(mix(5, 7))\n",
    "# const42 : i32 => 42\n>_(const42())\n",
    "# ping => 1\n>_(ping())\n",
    "# parity(n: i32) ?={\n    0 => 0\n    _ => 1\n}\n>_(parity(0), parity(3))\n",
    "# iter_only(x) >> (n) => >_(n)\niter_only(3)\n",
    "# alert := () => >_(\"ALERT\")\nalert()\n",
    "# compute := (x: i32) => {\n    y = x * 2\n    <| y\n}\n>_(compute(5))\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr(src, true), parse_program_to_repr(src, false)) << src;
  }
}

TEST(StyioSecurityNewParserStmt, RejectsHashIteratorMatchForwardChainWithStableError) {
  const std::string src = "# iter_only(x) >> (n) ?= 2 => >_(n)\niter_only([1, 2, 3])\n";
  EXPECT_THROW(
    {
      (void)parse_program_to_repr(src, true);
    },
    StyioBaseException);
  EXPECT_THROW(
    {
      (void)parse_program_to_repr(src, false);
    },
    StyioBaseException);
}

TEST(StyioSecurityNewParserStmt, MatchesLegacyOnDotCallSubsetSamples) {
  const std::vector<std::string> samples = {
    "foo.bar(1)\n",
    "x = foo.bar(1 + 2)\n>_(x)\n",
    ">_(foo.bar(3), 4)\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr(src, true), parse_program_to_repr(src, false)) << src;
  }
}

TEST(StyioSecurityNewParserShadow, FallsBackOnDotChainSequence) {
  const std::string src = "foo.bar(1).baz(2)\n";
  EXPECT_THROW(
    {
      (void)parse_program_engine_to_repr(src, StyioParserEngine::New);
    },
    StyioSyntaxError);
  EXPECT_THROW(
    {
      (void)parse_program_engine_to_repr(src, StyioParserEngine::Legacy);
    },
    StyioSyntaxError);
}

TEST(StyioSecurityUnicode, ByteClassificationIsStable) {
  EXPECT_TRUE(StyioUnicode::is_identifier_start('a'));
  EXPECT_TRUE(StyioUnicode::is_identifier_start('_'));
  EXPECT_FALSE(StyioUnicode::is_identifier_start('9'));

  EXPECT_TRUE(StyioUnicode::is_identifier_continue('9'));
  EXPECT_FALSE(StyioUnicode::is_identifier_continue('-'));

  EXPECT_TRUE(StyioUnicode::is_digit('7'));
  EXPECT_TRUE(StyioUnicode::is_space(' '));
  EXPECT_FALSE(StyioUnicode::is_space('x'));
}

TEST(StyioSecurityUnicode, DecodeUtf8CodepointBoundaries) {
  {
    std::string valid = "\xE4\xB8\xAD"; // U+4E2D
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_TRUE(StyioUnicode::decode_utf8_codepoint(valid, 0, cp, width));
    EXPECT_EQ(cp, 0x4E2Du);
    EXPECT_EQ(width, 3u);
  }

  {
    std::string overlong = "\xC0\xAF";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(overlong, 0, cp, width));
  }

  {
    std::string truncated = "\xF0\x9F\x92";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(truncated, 0, cp, width));
  }
}

TEST(StyioSecuritySession, ResetClearsSessionState) {
  CompilationSession session;
  const std::string src = "x = 1\n";
  session.adopt_tokens(StyioTokenizer::tokenize(src));
  session.attach_context(StyioContext::Create(
    "<security>",
    src,
    {{0, src.size() - 1}},
    session.tokens(),
    false));
  session.attach_ast(MainBlockAST::Create({}));
  ASSERT_FALSE(session.tokens().empty());
  ASSERT_NE(session.context(), nullptr);
  ASSERT_NE(session.ast(), nullptr);

  session.reset();
  EXPECT_TRUE(session.tokens().empty());
  EXPECT_EQ(session.context(), nullptr);
  EXPECT_EQ(session.ast(), nullptr);
  EXPECT_EQ(session.ir(), nullptr);
}

TEST(StyioSecurityAstOwnership, BinOpOwnsChildExprs) {
  int destructed = 0;
  auto* lhs = new CountingExprAST(&destructed);
  auto* rhs = new CountingExprAST(&destructed);
  auto* expr = BinOpAST::Create(StyioOpType::Binary_Add, lhs, rhs);
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, BinCompOwnsChildExprs) {
  int destructed = 0;
  auto* lhs = new CountingExprAST(&destructed);
  auto* rhs = new CountingExprAST(&destructed);
  auto* expr = new BinCompAST(CompType::EQ, lhs, rhs);
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, CondOwnsUnaryChildExpr) {
  int destructed = 0;
  auto* value = new CountingExprAST(&destructed);
  auto* expr = CondAST::Create(LogicType::NOT, value);
  delete expr;
  EXPECT_EQ(destructed, 1);
}

TEST(StyioSecurityAstOwnership, CondOwnsBinaryChildExprs) {
  int destructed = 0;
  auto* lhs = new CountingExprAST(&destructed);
  auto* rhs = new CountingExprAST(&destructed);
  auto* expr = CondAST::Create(LogicType::AND, lhs, rhs);
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, WaveMergeOwnsAllChildExprs) {
  int destructed = 0;
  auto* cond = new CountingExprAST(&destructed);
  auto* on_true = new CountingExprAST(&destructed);
  auto* on_false = new CountingExprAST(&destructed);
  auto* expr = WaveMergeAST::Create(cond, on_true, on_false);
  delete expr;
  EXPECT_EQ(destructed, 3);
}

TEST(StyioSecurityAstOwnership, WaveDispatchOwnsAllChildExprs) {
  int destructed = 0;
  auto* cond = new CountingExprAST(&destructed);
  auto* on_true = new CountingExprAST(&destructed);
  auto* on_false = new CountingExprAST(&destructed);
  auto* expr = WaveDispatchAST::Create(cond, on_true, on_false);
  delete expr;
  EXPECT_EQ(destructed, 3);
}

TEST(StyioSecurityAstOwnership, FallbackOwnsChildExprs) {
  int destructed = 0;
  auto* primary = new CountingExprAST(&destructed);
  auto* alternate = new CountingExprAST(&destructed);
  auto* expr = FallbackAST::Create(primary, alternate);
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, GuardSelectorOwnsChildExprs) {
  int destructed = 0;
  auto* base = new CountingExprAST(&destructed);
  auto* cond = new CountingExprAST(&destructed);
  auto* expr = GuardSelectorAST::Create(base, cond);
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, EqProbeOwnsChildExprs) {
  int destructed = 0;
  auto* base = new CountingExprAST(&destructed);
  auto* probe = new CountingExprAST(&destructed);
  auto* expr = EqProbeAST::Create(base, probe);
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, FuncCallOwnsNameAndArgs) {
  int name_destructed = 0;
  int arg_destructed = 0;
  auto* expr = FuncCallAST::Create(
    new CountingNameAST("f", &name_destructed),
    std::vector<StyioAST*>{
      new CountingExprAST(&arg_destructed),
      new CountingExprAST(&arg_destructed)});
  delete expr;
  EXPECT_EQ(name_destructed, 1);
  EXPECT_EQ(arg_destructed, 2);
}

TEST(StyioSecurityAstOwnership, FuncCallOwnsCalleeNameAndArgs) {
  int callee_destructed = 0;
  int name_destructed = 0;
  int arg_destructed = 0;
  auto* expr = FuncCallAST::Create(
    new CountingExprAST(&callee_destructed),
    new CountingNameAST("f", &name_destructed),
    std::vector<StyioAST*>{new CountingExprAST(&arg_destructed)});
  delete expr;
  EXPECT_EQ(callee_destructed, 1);
  EXPECT_EQ(name_destructed, 1);
  EXPECT_EQ(arg_destructed, 1);
}

TEST(StyioSecurityAstOwnership, FuncCallSetCalleeTakesOwnership) {
  int callee_destructed = 0;
  int name_destructed = 0;
  auto* expr = FuncCallAST::Create(
    new CountingNameAST("f", &name_destructed),
    std::vector<StyioAST*>{});
  expr->setFuncCallee(new CountingExprAST(&callee_destructed));
  delete expr;
  EXPECT_EQ(callee_destructed, 1);
  EXPECT_EQ(name_destructed, 1);
}

TEST(StyioSecurityAstOwnership, ListOpOwnsListOnly) {
  int list_destructed = 0;
  auto* expr = new ListOpAST(
    StyioNodeType::Get_Reversed,
    new CountingExprAST(&list_destructed));
  delete expr;
  EXPECT_EQ(list_destructed, 1);
}

TEST(StyioSecurityAstOwnership, ListOpOwnsListAndSlot1) {
  int list_destructed = 0;
  int slot1_destructed = 0;
  auto* expr = new ListOpAST(
    StyioNodeType::Access_By_Index,
    new CountingExprAST(&list_destructed),
    new CountingExprAST(&slot1_destructed));
  delete expr;
  EXPECT_EQ(list_destructed, 1);
  EXPECT_EQ(slot1_destructed, 1);
}

TEST(StyioSecurityAstOwnership, ListOpOwnsListSlot1AndSlot2) {
  int list_destructed = 0;
  int slot1_destructed = 0;
  int slot2_destructed = 0;
  auto* expr = new ListOpAST(
    StyioNodeType::Insert_Item_By_Index,
    new CountingExprAST(&list_destructed),
    new CountingExprAST(&slot1_destructed),
    new CountingExprAST(&slot2_destructed));
  delete expr;
  EXPECT_EQ(list_destructed, 1);
  EXPECT_EQ(slot1_destructed, 1);
  EXPECT_EQ(slot2_destructed, 1);
}

TEST(StyioSecurityAstOwnership, AttrOwnsBodyAndAttr) {
  int body_destructed = 0;
  int attr_destructed = 0;
  auto* expr =
    AttrAST::Create(new CountingExprAST(&body_destructed), new CountingExprAST(&attr_destructed));
  delete expr;
  EXPECT_EQ(body_destructed, 1);
  EXPECT_EQ(attr_destructed, 1);
}

TEST(StyioSecurityAstOwnership, FmtStrOwnsEmbeddedExprs) {
  int destructed = 0;
  auto* expr = FmtStrAST::Create(
    {"x=", ", y="},
    std::vector<StyioAST*>{
      new CountingExprAST(&destructed),
      new CountingExprAST(&destructed)});
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, TypeConvertOwnsValue) {
  int destructed = 0;
  auto* expr = TypeConvertAST::Create(new CountingExprAST(&destructed), NumPromoTy::Int_To_Float);
  delete expr;
  EXPECT_EQ(destructed, 1);
}

TEST(StyioSecurityAstOwnership, RangeOwnsAllBoundExprs) {
  int destructed = 0;
  auto* expr = new RangeAST(
    new CountingExprAST(&destructed),
    new CountingExprAST(&destructed),
    new CountingExprAST(&destructed));
  delete expr;
  EXPECT_EQ(destructed, 3);
}

TEST(StyioSecurityAstOwnership, SizeOfOwnsValueExpr) {
  int destructed = 0;
  auto* expr = new SizeOfAST(new CountingExprAST(&destructed));
  delete expr;
  EXPECT_EQ(destructed, 1);
}

TEST(StyioSecurityAstOwnership, TupleOwnsElements) {
  int destructed = 0;
  auto* expr = TupleAST::Create(
    std::vector<StyioAST*>{
      new CountingExprAST(&destructed),
      new CountingExprAST(&destructed)});
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, ListOwnsElements) {
  int destructed = 0;
  auto* expr = ListAST::Create(
    std::vector<StyioAST*>{
      new CountingExprAST(&destructed),
      new CountingExprAST(&destructed)});
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, SetOwnsElements) {
  int destructed = 0;
  auto* expr = SetAST::Create(
    std::vector<StyioAST*>{
      new CountingExprAST(&destructed),
      new CountingExprAST(&destructed)});
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, VarTupleOwnsVarNodes) {
  int destructed = 0;
  auto* expr = VarTupleAST::Create(
    std::vector<VarAST*>{
      new CountingVarAST(&destructed),
      new CountingVarAST(&destructed)});
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, ReturnOwnsExpr) {
  int destructed = 0;
  auto* stmt = ReturnAST::Create(new CountingExprAST(&destructed));
  delete stmt;
  EXPECT_EQ(destructed, 1);
}

TEST(StyioSecurityAstOwnership, FlexBindOwnsVarAndValue) {
  int var_destructed = 0;
  int value_destructed = 0;
  auto* stmt = FlexBindAST::Create(
    new CountingVarAST(&var_destructed),
    new CountingExprAST(&value_destructed));
  delete stmt;
  EXPECT_EQ(var_destructed, 1);
  EXPECT_EQ(value_destructed, 1);
}

TEST(StyioSecurityAstOwnership, FinalBindOwnsVarAndValue) {
  int var_destructed = 0;
  int value_destructed = 0;
  auto* stmt = FinalBindAST::Create(
    new CountingVarAST(&var_destructed),
    new CountingExprAST(&value_destructed));
  delete stmt;
  EXPECT_EQ(var_destructed, 1);
  EXPECT_EQ(value_destructed, 1);
}

TEST(StyioSecurityAstOwnership, ReadFileOwnsIdAndValue) {
  int id_destructed = 0;
  int value_destructed = 0;
  auto* stmt =
    new ReadFileAST(new CountingNameAST("x", &id_destructed), new CountingExprAST(&value_destructed));
  delete stmt;
  EXPECT_EQ(id_destructed, 1);
  EXPECT_EQ(value_destructed, 1);
}

TEST(StyioSecurityAstOwnership, FileResourceOwnsPathExpr) {
  int path_destructed = 0;
  auto* stmt = FileResourceAST::Create(new CountingExprAST(&path_destructed), true);
  delete stmt;
  EXPECT_EQ(path_destructed, 1);
}

TEST(StyioSecurityAstOwnership, HandleAcquireOwnsVarAndResource) {
  int var_destructed = 0;
  int resource_destructed = 0;
  auto* stmt = HandleAcquireAST::Create(
    new CountingVarAST(&var_destructed),
    new CountingExprAST(&resource_destructed));
  delete stmt;
  EXPECT_EQ(var_destructed, 1);
  EXPECT_EQ(resource_destructed, 1);
}

TEST(StyioSecurityAstOwnership, ResourceWriteOwnsDataAndResource) {
  int data_destructed = 0;
  int resource_destructed = 0;
  auto* stmt = ResourceWriteAST::Create(
    new CountingExprAST(&data_destructed),
    new CountingExprAST(&resource_destructed));
  delete stmt;
  EXPECT_EQ(data_destructed, 1);
  EXPECT_EQ(resource_destructed, 1);
}

TEST(StyioSecurityAstOwnership, ResourceRedirectOwnsDataAndResource) {
  int data_destructed = 0;
  int resource_destructed = 0;
  auto* stmt = ResourceRedirectAST::Create(
    new CountingExprAST(&data_destructed),
    new CountingExprAST(&resource_destructed));
  delete stmt;
  EXPECT_EQ(data_destructed, 1);
  EXPECT_EQ(resource_destructed, 1);
}

TEST(StyioSecurityAstOwnership, PrintOwnsExpressionList) {
  int expr_destructed = 0;
  auto* node = PrintAST::Create(std::vector<StyioAST*>{
    new CountingExprAST(&expr_destructed),
    new CountingExprAST(&expr_destructed),
  });
  delete node;
  EXPECT_EQ(expr_destructed, 2);
}

TEST(StyioSecurityAstOwnership, StateRefOwnsNameNode) {
  int name_destructed = 0;
  auto* node = StateRefAST::Create(new CountingNameAST("state", &name_destructed));
  delete node;
  EXPECT_EQ(name_destructed, 1);
}

TEST(StyioSecurityAstOwnership, HistoryProbeOwnsTargetAndDepth) {
  int name_destructed = 0;
  int depth_destructed = 0;
  auto* node = HistoryProbeAST::Create(
    StateRefAST::Create(new CountingNameAST("state", &name_destructed)),
    new CountingExprAST(&depth_destructed));
  delete node;
  EXPECT_EQ(name_destructed, 1);
  EXPECT_EQ(depth_destructed, 1);
}

TEST(StyioSecurityAstOwnership, SeriesIntrinsicOwnsBaseAndWindow) {
  int base_destructed = 0;
  int window_destructed = 0;
  auto* node = SeriesIntrinsicAST::Create(
    new CountingExprAST(&base_destructed),
    SeriesIntrinsicOp::Avg,
    new CountingExprAST(&window_destructed));
  delete node;
  EXPECT_EQ(base_destructed, 1);
  EXPECT_EQ(window_destructed, 1);
}

TEST(StyioSecurityAstOwnership, StateDeclOwnsAccInitExportVarAndUpdateExpr) {
  int acc_name_destructed = 0;
  int acc_init_destructed = 0;
  int export_var_destructed = 0;
  int update_expr_destructed = 0;

  auto* node = StateDeclAST::Create(
    IntAST::Create("3"),
    new CountingNameAST("acc", &acc_name_destructed),
    new CountingExprAST(&acc_init_destructed),
    new CountingVarAST(&export_var_destructed),
    new CountingExprAST(&update_expr_destructed));

  delete node;
  EXPECT_EQ(acc_name_destructed, 1);
  EXPECT_EQ(acc_init_destructed, 1);
  EXPECT_EQ(export_var_destructed, 1);
  EXPECT_EQ(update_expr_destructed, 1);
}

TEST(StyioSecurityAstOwnership, VarOwnsNameTypeAndInit) {
  int name_destructed = 0;
  int type_destructed = 0;
  int init_destructed = 0;
  auto* var = new VarAST(
    new CountingNameAST("x", &name_destructed),
    new CountingTypeAST("i64", &type_destructed),
    new CountingExprAST(&init_destructed));
  delete var;
  EXPECT_EQ(name_destructed, 1);
  EXPECT_EQ(type_destructed, 1);
  EXPECT_EQ(init_destructed, 1);
}

TEST(StyioSecurityAstOwnership, ParamOwnsNameTypeAndInit) {
  int name_destructed = 0;
  int type_destructed = 0;
  int init_destructed = 0;
  auto* param = ParamAST::Create(
    new CountingNameAST("p", &name_destructed),
    new CountingTypeAST("i64", &type_destructed),
    new CountingExprAST(&init_destructed));
  delete param;
  EXPECT_EQ(name_destructed, 1);
  EXPECT_EQ(type_destructed, 1);
  EXPECT_EQ(init_destructed, 1);
}

TEST(StyioSecurityAstOwnership, OptArgOwnsName) {
  int name_destructed = 0;
  auto* node = OptArgAST::Create(new CountingNameAST("oa", &name_destructed));
  delete node;
  EXPECT_EQ(name_destructed, 1);
}

TEST(StyioSecurityAstOwnership, OptKwArgOwnsName) {
  int name_destructed = 0;
  auto* node = OptKwArgAST::Create(new CountingNameAST("okw", &name_destructed));
  delete node;
  EXPECT_EQ(name_destructed, 1);
}

TEST(StyioSecurityAstOwnership, StructOwnsNameAndParams) {
  int struct_name_destructed = 0;
  int param_name_destructed = 0;
  auto* node = StructAST::Create(
    new CountingNameAST("S", &struct_name_destructed),
    std::vector<ParamAST*>{
      ParamAST::Create(new CountingNameAST("p1", &param_name_destructed)),
      ParamAST::Create(new CountingNameAST("p2", &param_name_destructed))});
  delete node;
  EXPECT_EQ(struct_name_destructed, 1);
  EXPECT_EQ(param_name_destructed, 2);
}

TEST(StyioSecurityAstOwnership, ResourceOwnsEntries) {
  int destructed = 0;
  auto* node = ResourceAST::Create(
    std::vector<std::pair<StyioAST*, std::string>>{
      {new CountingExprAST(&destructed), "file"},
      {new CountingExprAST(&destructed), "db"}});
  delete node;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, CasesOwnsCasePairsAndDefault) {
  int destructed = 0;
  auto* node = CasesAST::Create(
    std::vector<std::pair<StyioAST*, StyioAST*>>{
      {new CountingExprAST(&destructed), new CountingExprAST(&destructed)},
      {new CountingExprAST(&destructed), new CountingExprAST(&destructed)},
    },
    new CountingExprAST(&destructed));

  delete node;
  EXPECT_EQ(destructed, 5);
}

TEST(StyioSecurityAstOwnership, MatchCasesOwnsScrutineeAndCases) {
  int destructed = 0;
  auto* node = MatchCasesAST::make(
    new CountingExprAST(&destructed),
    CasesAST::Create(
      std::vector<std::pair<StyioAST*, StyioAST*>>{
        {new CountingExprAST(&destructed), new CountingExprAST(&destructed)},
      },
      new CountingExprAST(&destructed)));

  delete node;
  EXPECT_EQ(destructed, 4);
}

TEST(StyioSecurityAstOwnership, CondFlowOwnsConditionAndThenBranch) {
  int destructed = 0;
  auto* node = new CondFlowAST(
    StyioNodeType::CondFlow_True,
    CondAST::Create(LogicType::NOT, new CountingExprAST(&destructed)),
    new CountingExprAST(&destructed));

  delete node;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, CondFlowOwnsConditionThenAndElseBranches) {
  int destructed = 0;
  auto* node = new CondFlowAST(
    StyioNodeType::CondFlow_Both,
    CondAST::Create(LogicType::NOT, new CountingExprAST(&destructed)),
    new CountingExprAST(&destructed),
    new CountingExprAST(&destructed));

  delete node;
  EXPECT_EQ(destructed, 3);
}

TEST(StyioSecurityAstOwnership, FunctionOwnsNameParamsRetTypeAndBody) {
  int func_name_destructed = 0;
  int param_name_destructed = 0;
  int ret_type_destructed = 0;
  int body_destructed = 0;
  auto* node = FunctionAST::Create(
    new CountingNameAST("f", &func_name_destructed),
    false,
    std::vector<ParamAST*>{
      ParamAST::Create(new CountingNameAST("p1", &param_name_destructed)),
      ParamAST::Create(new CountingNameAST("p2", &param_name_destructed)),
    },
    new CountingTypeAST("i64", &ret_type_destructed),
    new CountingExprAST(&body_destructed));

  delete node;
  EXPECT_EQ(func_name_destructed, 1);
  EXPECT_EQ(param_name_destructed, 2);
  EXPECT_EQ(ret_type_destructed, 1);
  EXPECT_EQ(body_destructed, 1);
}

TEST(StyioSecurityAstOwnership, SimpleFuncOwnsNameParamsRetTypeAndReturnExpr) {
  int func_name_destructed = 0;
  int param_name_destructed = 0;
  int ret_type_destructed = 0;
  int ret_expr_destructed = 0;
  auto* node = SimpleFuncAST::Create(
    new CountingNameAST("f", &func_name_destructed),
    false,
    std::vector<ParamAST*>{
      ParamAST::Create(new CountingNameAST("p", &param_name_destructed)),
    },
    new CountingTypeAST("i64", &ret_type_destructed),
    new CountingExprAST(&ret_expr_destructed));

  delete node;
  EXPECT_EQ(func_name_destructed, 1);
  EXPECT_EQ(param_name_destructed, 1);
  EXPECT_EQ(ret_type_destructed, 1);
  EXPECT_EQ(ret_expr_destructed, 1);
}

TEST(StyioSecurityAstOwnership, InfiniteLoopOwnsWhileCondAndBodyNode) {
  StyioAST::destroy_all_tracked_nodes();

  int cond_destructed = 0;
  auto* node = InfiniteLoopAST::CreateWhile(
    new CountingExprAST(&cond_destructed),
    BlockAST::Create({}));

  delete node;
  EXPECT_EQ(cond_destructed, 1);
  EXPECT_EQ(StyioAST::tracked_node_count(), 0u);

  StyioAST::destroy_all_tracked_nodes();
}

TEST(StyioSecurityAstOwnership, StreamZipOwnsCollectionsParamsAndBody) {
  int collection_destructed = 0;
  int param_name_destructed = 0;
  int body_destructed = 0;
  auto* node = StreamZipAST::Create(
    new CountingExprAST(&collection_destructed),
    std::vector<ParamAST*>{
      ParamAST::Create(new CountingNameAST("a", &param_name_destructed)),
    },
    new CountingExprAST(&collection_destructed),
    std::vector<ParamAST*>{
      ParamAST::Create(new CountingNameAST("b", &param_name_destructed)),
    },
    new CountingExprAST(&body_destructed));

  delete node;
  EXPECT_EQ(collection_destructed, 2);
  EXPECT_EQ(param_name_destructed, 2);
  EXPECT_EQ(body_destructed, 1);
}

TEST(StyioSecurityAstOwnership, IteratorOwnsCollectionParamsAndFollowing) {
  int collection_destructed = 0;
  int param_name_destructed = 0;
  int following_destructed = 0;
  auto* node = IteratorAST::Create(
    new CountingExprAST(&collection_destructed),
    std::vector<ParamAST*>{
      ParamAST::Create(new CountingNameAST("it", &param_name_destructed)),
    },
    std::vector<StyioAST*>{
      new CountingExprAST(&following_destructed),
    });

  delete node;
  EXPECT_EQ(collection_destructed, 1);
  EXPECT_EQ(param_name_destructed, 1);
  EXPECT_EQ(following_destructed, 1);
}

TEST(StyioSecurityAstOwnership, BlockOwnsStmtList) {
  int stmt_destructed = 0;
  auto* node = BlockAST::Create(
    std::vector<StyioAST*>{
      new CountingExprAST(&stmt_destructed),
      new CountingExprAST(&stmt_destructed),
    });

  delete node;
  EXPECT_EQ(stmt_destructed, 2);
}

TEST(StyioSecurityAstOwnership, MainBlockOwnsResourceAndStmtList) {
  int destructed = 0;
  auto* node = new MainBlockAST(
    new CountingExprAST(&destructed),
    std::vector<StyioAST*>{
      new CountingExprAST(&destructed),
      new CountingExprAST(&destructed),
    });

  delete node;
  EXPECT_EQ(destructed, 3);
}

TEST(StyioSecurityAstOwnership, CheckIsinOwnsIterableExpr) {
  int destructed = 0;
  auto* node = new CheckIsinAST(new CountingExprAST(&destructed));

  delete node;
  EXPECT_EQ(destructed, 1);
}

TEST(StyioSecurityAstOwnership, InfiniteOwnsStartAndIncrementExprs) {
  int destructed = 0;
  auto* node = new InfiniteAST(
    new CountingExprAST(&destructed),
    new CountingExprAST(&destructed));

  delete node;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, AnonyFuncOwnsArgsAndThenExpr) {
  int var_destructed = 0;
  int then_destructed = 0;
  auto* node = new AnonyFuncAST(
    VarTupleAST::Create(std::vector<VarAST*>{
      new CountingVarAST(&var_destructed),
    }),
    new CountingExprAST(&then_destructed));

  delete node;
  EXPECT_EQ(var_destructed, 1);
  EXPECT_EQ(then_destructed, 1);
}

TEST(StyioSecurityAstOwnership, SnapshotDeclOwnsVarAndResource) {
  int var_destructed = 0;
  int path_destructed = 0;
  auto* node = SnapshotDeclAST::Create(
    new CountingNameAST("snap", &var_destructed),
    FileResourceAST::Create(new CountingExprAST(&path_destructed), true));

  delete node;
  EXPECT_EQ(var_destructed, 1);
  EXPECT_EQ(path_destructed, 1);
}

TEST(StyioSecurityAstOwnership, InstantPullOwnsResource) {
  int path_destructed = 0;
  auto* node = InstantPullAST::Create(
    FileResourceAST::Create(new CountingExprAST(&path_destructed), false));

  delete node;
  EXPECT_EQ(path_destructed, 1);
}

TEST(StyioSecurityAstOwnership, IterSeqOwnsHashTags) {
  StyioAST::destroy_all_tracked_nodes();
  int collection_destructed = 0;
  auto* node = IterSeqAST::Create(
    new CountingExprAST(&collection_destructed),
    std::vector<HashTagNameAST*>{
      HashTagNameAST::Create(std::vector<std::string>{"left"}),
      HashTagNameAST::Create(std::vector<std::string>{"right"}),
    });

  delete node;
  EXPECT_EQ(collection_destructed, 1);
  EXPECT_EQ(StyioAST::tracked_node_count(), 0u);
  StyioAST::destroy_all_tracked_nodes();
}

TEST(StyioSecurityAstOwnership, ExtractorOwnsTupleAndOperation) {
  int tuple_destructed = 0;
  int op_destructed = 0;
  auto* node = ExtractorAST::Create(
    new CountingExprAST(&tuple_destructed),
    new CountingExprAST(&op_destructed));

  delete node;
  EXPECT_EQ(tuple_destructed, 1);
  EXPECT_EQ(op_destructed, 1);
}

TEST(StyioSecurityAstOwnership, BackwardOwnsObjectParamsOperationsAndReturns) {
  int expr_destructed = 0;
  int var_destructed = 0;
  auto* node = BackwardAST::Create(
    new CountingExprAST(&expr_destructed),
    VarTupleAST::Create(std::vector<VarAST*>{
      new CountingVarAST(&var_destructed),
    }),
    std::vector<StyioAST*>{
      new CountingExprAST(&expr_destructed),
      new CountingExprAST(&expr_destructed),
    },
    std::vector<StyioAST*>{
      new CountingExprAST(&expr_destructed),
    });

  delete node;
  EXPECT_EQ(expr_destructed, 4);
  EXPECT_EQ(var_destructed, 1);
}

TEST(StyioSecurityAstOwnership, CodpOwnsArgsAndNextChain) {
  StyioAST::destroy_all_tracked_nodes();
  int expr_destructed = 0;
  auto* next = CODPAST::Create("map", std::vector<StyioAST*>{
                                      new CountingExprAST(&expr_destructed),
                                    });
  auto* node = CODPAST::Create("filter", std::vector<StyioAST*>{
                                             new CountingExprAST(&expr_destructed),
                                             new CountingExprAST(&expr_destructed),
                                           });
  node->setNextOp(next);

  delete node;
  EXPECT_EQ(expr_destructed, 3);
  EXPECT_EQ(StyioAST::tracked_node_count(), 0u);
  StyioAST::destroy_all_tracked_nodes();
}

TEST(StyioSecurityAstOwnership, ForwardSetRetExprTakesOwnership) {
  StyioAST::destroy_all_tracked_nodes();
  int ret_expr_destructed = 0;
  auto* node = new ForwardAST();
  node->setRetExpr(new CountingExprAST(&ret_expr_destructed));

  delete node;
  EXPECT_EQ(ret_expr_destructed, 1);
  EXPECT_EQ(StyioAST::tracked_node_count(), 0u);
  StyioAST::destroy_all_tracked_nodes();
}

TEST(StyioSafetyRuntime, StrcatAbAllocatesAndSupportsPairingFree) {
  const char* p = styio_strcat_ab("a", "b");
  ASSERT_NE(p, nullptr);
  ASSERT_STREQ(p, "ab");
  styio_free_cstr(p);

  const char* chain = styio_strcat_ab("x", "y");
  const char* chain2 = styio_strcat_ab(chain, "z");
  styio_free_cstr(chain);
  styio_free_cstr(chain2);
}

TEST(StyioSafetyRuntime, StrcatAbHugeInputDoesNotCrash) {
  std::string a(40000, 'x');
  std::string b(40000, 'y');
  const char* p = styio_strcat_ab(a.c_str(), b.c_str());
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(strlen(p), 80000u);
  styio_free_cstr(p);
}

TEST(StyioSafetyRuntime, MissingFileOpenReturnsZeroHandle) {
  styio_runtime_clear_error();
  const int64_t h = styio_file_open("/tmp/styio_missing_9b8fe8e2_7dfe_42ed_9ce2_4f9e587f7f6d.txt");
  EXPECT_EQ(h, 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_FILE_OPEN_READ");
  const char* msg = styio_runtime_last_error();
  ASSERT_NE(msg, nullptr);
  EXPECT_NE(std::strstr(msg, "cannot open file for read"), nullptr);
  styio_runtime_clear_error();
}

TEST(StyioSafetyRuntime, NullReadPathSetsStableSubcode) {
  styio_runtime_clear_error();
  const int64_t h = styio_file_open(nullptr);
  EXPECT_EQ(h, 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_FILE_PATH_NULL");
  const char* msg = styio_runtime_last_error();
  ASSERT_NE(msg, nullptr);
  EXPECT_NE(std::strstr(msg, "file path is null"), nullptr);
  styio_runtime_clear_error();
}

TEST(StyioSafetyRuntime, NullWritePathSetsStableSubcode) {
  styio_runtime_clear_error();
  const int64_t h = styio_file_open_write(nullptr);
  EXPECT_EQ(h, 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_FILE_PATH_NULL");
  const char* msg = styio_runtime_last_error();
  ASSERT_NE(msg, nullptr);
  EXPECT_NE(std::strstr(msg, "file path is null"), nullptr);
  styio_runtime_clear_error();
}

TEST(StyioSafetyRuntime, MissingWritePathSetsStableSubcode) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const std::string path =
    "/tmp/styio_missing_write_dir_" + std::to_string(uniq) + "/out.txt";

  styio_runtime_clear_error();
  const int64_t h = styio_file_open_write(path.c_str());
  EXPECT_EQ(h, 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_FILE_OPEN_WRITE");
  const char* msg = styio_runtime_last_error();
  ASSERT_NE(msg, nullptr);
  EXPECT_NE(std::strstr(msg, "cannot open file for write"), nullptr);
  styio_runtime_clear_error();
}

TEST(StyioSafetyRuntime, InvalidHandleOperationsAreSafe) {
  styio_runtime_clear_error();
  styio_file_close(123456789);
  styio_file_rewind(123456789);
  EXPECT_EQ(styio_file_read_line(123456789), nullptr);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_FILE_HANDLE");
  const char* msg = styio_runtime_last_error();
  ASSERT_NE(msg, nullptr);
  EXPECT_NE(std::strstr(msg, "invalid file handle"), nullptr);
  styio_runtime_clear_error();
}

TEST(StyioSafetyRuntime, FirstErrorWinsAcrossMultipleRuntimeFailures) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const std::string path =
    "/tmp/styio_missing_runtime_first_error_" + std::to_string(uniq) + ".txt";

  styio_runtime_clear_error();
  EXPECT_EQ(styio_file_open(path.c_str()), 0);
  ASSERT_EQ(styio_runtime_has_error(), 1);
  ASSERT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_FILE_OPEN_READ");
  const char* first_msg = styio_runtime_last_error();
  ASSERT_NE(first_msg, nullptr);

  // Second error should not overwrite first-error diagnostics within one run.
  styio_file_rewind(987654321);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_FILE_OPEN_READ");
  const char* msg = styio_runtime_last_error();
  ASSERT_NE(msg, nullptr);
  EXPECT_STREQ(msg, first_msg);
  styio_runtime_clear_error();
}

TEST(StyioSafetyRuntime, InvalidWriteHandleSetsRuntimeError) {
  styio_runtime_clear_error();
  styio_file_write_cstr(345678901, "x");
  EXPECT_EQ(styio_runtime_has_error(), 1);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_FILE_HANDLE");
  const char* msg = styio_runtime_last_error();
  ASSERT_NE(msg, nullptr);
  EXPECT_NE(std::strstr(msg, "invalid file handle"), nullptr);
  styio_runtime_clear_error();
}

TEST(StyioSafetyRuntime, CloseIsIdempotentAndKeepsErrorClear) {
  styio_runtime_clear_error();
  const int64_t h = styio_file_open("tests/m5/data/hello.txt");
  ASSERT_NE(h, 0);
  styio_file_close(h);
  styio_file_close(h);
  EXPECT_EQ(styio_runtime_has_error(), 0);
  EXPECT_EQ(styio_runtime_last_error_subcode(), nullptr);
  EXPECT_EQ(styio_runtime_last_error(), nullptr);
}

TEST(StyioSafetyRuntime, FreeCstrAcceptsNull) {
  styio_free_cstr(nullptr);
  SUCCEED();
}

TEST(StyioSafetyRuntime, FreeCstrIgnoresBorrowedPointer) {
  const int64_t h = styio_file_open("tests/m5/data/hello.txt");
  ASSERT_NE(h, 0);
  const char* line = styio_file_read_line(h);
  ASSERT_NE(line, nullptr);
  styio_free_cstr(line);
  styio_file_close(h);
  SUCCEED();
}

TEST(StyioSafetyRuntime, ClearErrorAlsoClearsLastErrorMessage) {
  styio_runtime_clear_error();
  (void)styio_file_open("/tmp/styio_missing_0a2f8bd8_9a47_4e2d_b258_1de50d7f8f08.txt");
  ASSERT_EQ(styio_runtime_has_error(), 1);
  ASSERT_NE(styio_runtime_last_error_subcode(), nullptr);
  ASSERT_NE(styio_runtime_last_error(), nullptr);
  styio_runtime_clear_error();
  EXPECT_EQ(styio_runtime_has_error(), 0);
  EXPECT_EQ(styio_runtime_last_error_subcode(), nullptr);
  EXPECT_EQ(styio_runtime_last_error(), nullptr);
}

TEST(StyioSafetyHandleTable, AcquireLookupAndReleaseHonorsKind) {
  StyioHandleTable table;
  int payload = 42;
  const auto id = table.acquire(StyioHandleTable::HandleKind::File, &payload);
  ASSERT_NE(id, 0);
  ASSERT_TRUE(table.contains(id));
  EXPECT_EQ(table.lookup_as<int>(id, StyioHandleTable::HandleKind::File), &payload);
  EXPECT_EQ(table.lookup_as<int>(id, StyioHandleTable::HandleKind::Resource), nullptr);

  bool closer_called = false;
  EXPECT_FALSE(table.release(
    id,
    StyioHandleTable::HandleKind::Resource,
    [&](void*) { closer_called = true; }));
  EXPECT_FALSE(closer_called);
  EXPECT_TRUE(table.contains(id));

  EXPECT_TRUE(table.release(
    id,
    StyioHandleTable::HandleKind::File,
    [&](void* raw) {
      closer_called = true;
      EXPECT_EQ(raw, &payload);
    }));
  EXPECT_TRUE(closer_called);
  EXPECT_FALSE(table.contains(id));
  EXPECT_EQ(table.size(), 0U);
}

TEST(StyioSafetyHandleTable, ReleaseAllSkipsKindMismatchesAndDropsStubs) {
  StyioHandleTable table;
  int file_payload = 1;
  int resource_payload = 2;

  const auto file_id = table.acquire(StyioHandleTable::HandleKind::File, &file_payload);
  const auto resource_id = table.acquire(StyioHandleTable::HandleKind::Resource, &resource_payload);
  const auto stub_id = table.reserve_stub(StyioHandleTable::HandleKind::File);
  ASSERT_NE(file_id, 0);
  ASSERT_NE(resource_id, 0);
  ASSERT_NE(stub_id, 0);
  EXPECT_EQ(table.size(), 3U);

  int released = 0;
  const size_t released_count = table.release_all(
    StyioHandleTable::HandleKind::File,
    [&](void* raw) {
      ++released;
      EXPECT_EQ(raw, &file_payload);
    });
  EXPECT_EQ(released_count, 1U);
  EXPECT_EQ(released, 1);
  EXPECT_FALSE(table.contains(file_id));
  EXPECT_FALSE(table.contains(stub_id));
  EXPECT_TRUE(table.contains(resource_id));
  EXPECT_EQ(table.size(), 1U);

  table.invalidate(resource_id);
  EXPECT_EQ(table.size(), 0U);
}
