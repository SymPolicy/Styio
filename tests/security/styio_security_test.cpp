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
#include <vector>

#include "StyioException/Exception.hpp"
#include "StyioExtern/ExternLib.hpp"
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

void
free_tokens(std::vector<StyioToken*>& tokens) {
  for (auto* t : tokens) {
    delete t;
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
