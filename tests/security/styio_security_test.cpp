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

  StyioAST* ast = use_new_parser ? parse_expr_new_subset(*ctx) : parse_expr(*ctx);
  ctx->skip();
  if (ctx->cur_tok_type() != StyioTokenType::TOK_AT) {
    delete ast;
    delete ctx;
    free_tokens(tokens);
    throw std::runtime_error("expression parser did not stop at sentinel");
  }

  StyioRepr repr;
  const std::string out = ast->toString(&repr);
  delete ast;
  delete ctx;
  free_tokens(tokens);
  return out;
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

  MainBlockAST* ast = use_new_parser ? parse_main_block_new_subset(*ctx) : parse_main_block(*ctx);
  ctx->skip();
  if (ctx->cur_tok_type() != StyioTokenType::TOK_EOF) {
    delete ast;
    delete ctx;
    free_tokens(tokens);
    throw std::runtime_error("statement parser did not consume input");
  }

  StyioRepr repr;
  const std::string out = ast->toString(&repr);
  delete ast;
  delete ctx;
  free_tokens(tokens);
  return out;
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

  MainBlockAST* ast = parse_main_block_with_engine(*ctx, engine);
  ctx->skip();
  if (ctx->cur_tok_type() != StyioTokenType::TOK_EOF) {
    delete ast;
    delete ctx;
    free_tokens(tokens);
    throw std::runtime_error("engine parser did not consume input");
  }

  StyioRepr repr;
  const std::string out = ast->toString(&repr);
  delete ast;
  delete ctx;
  free_tokens(tokens);
  return out;
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

TEST(StyioSecurityRuntime, StrcatAbAllocatesWithoutPairingFree) {
  // Document: each successful call returns malloc-backed memory; generated IR
  // does not emit free today — repeated use leaks (verify with ASan).
  const char* p = styio_strcat_ab("a", "b");
  ASSERT_NE(p, nullptr);
  ASSERT_STREQ(p, "ab");
  std::free(const_cast<char*>(p)); // Test harness cleanup only; JIT programs lack this.

  const char* chain = styio_strcat_ab("x", "y");
  const char* chain2 = styio_strcat_ab(chain, "z");
  std::free(const_cast<char*>(chain));
  std::free(const_cast<char*>(chain2));
}

TEST(StyioSecurityRuntime, StrcatAbHugeInputDoesNotCrash) {
  std::string a(40000, 'x');
  std::string b(40000, 'y');
  const char* p = styio_strcat_ab(a.c_str(), b.c_str());
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(strlen(p), 80000u);
  std::free(const_cast<char*>(p));
}

TEST(StyioSecurityRuntime, MissingFileOpenReturnsZeroHandle) {
  const int64_t h = styio_file_open("/tmp/styio_missing_9b8fe8e2_7dfe_42ed_9ce2_4f9e587f7f6d.txt");
  EXPECT_EQ(h, 0);
}

TEST(StyioSecurityRuntime, InvalidHandleOperationsAreSafe) {
  styio_file_close(123456789);
  styio_file_rewind(123456789);
  EXPECT_EQ(styio_file_read_line(123456789), nullptr);
}

TEST(StyioSecurityRuntime, FreeCstrAcceptsNull) {
  styio_free_cstr(nullptr);
  SUCCEED();
}
