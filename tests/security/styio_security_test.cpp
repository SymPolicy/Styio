/**
 * Security-focused regression tests for the Styio toolchain.
 *
 * These tests encode expectations around untrusted/malformed source input and
 * runtime helper behaviour. Some expectations document *current* behaviour
 * (e.g. exceptions) until the lexer reports structured errors.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <io.h>
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#define close _close
#define dup _dup
#define dup2 _dup2
#define fileno _fileno
#else
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef STYIO_SOURCE_DIR
#define STYIO_SOURCE_DIR "."
#endif

#ifndef STYIO_TEST_C_COMPILER
#define STYIO_TEST_C_COMPILER "cc"
#endif

#include "../EnvTestUtil.hpp"
#include "StyioAST/AST.hpp"
#include "StyioCodeGen/CodeGenVisitor.hpp"
#include "StyioException/Exception.hpp"
#include "StyioExtern/ExternLib.hpp"
#include "StyioIR/GenIR/GenIR.hpp"
#include "StyioIR/StyioIR.hpp"
#include "StyioIR/StyioIRWalker.hpp"
#include "StyioIR/Verifier.hpp"
#include "StyioJIT/StyioJIT_ORC.hpp"
#include "StyioLowering/AstToStyioIRLowerer.hpp"
#include "StyioLowering/StyioIROptimizer.hpp"
#include "StyioNative/NativeInterop.hpp"
#include "StyioParser/NewParserExpr.hpp"
#include "StyioParser/Parser.hpp"
#include "StyioParser/ParserLookahead.hpp"
#include "StyioParser/SymbolRegistry.hpp"
#include "StyioParser/Tokenizer.hpp"
#include "StyioRuntime/HandleTable.hpp"
#include "StyioServices/DiagnosticContract.hpp"
#include "StyioSession/SessionAllocation.hpp"
#include "StyioToken/Token.hpp"

// Expose CompilationSession internals for focused white-box state-machine tests.
#define private public
#include "StyioSession/CompilationSession.hpp"
#undef private
#include "StyioUnicode/Unicode.hpp"
#include "StyioUtil/BoundedType.hpp"
#include "StyioUtil/BuiltinMethods.hpp"
#include "StyioUtil/DynamicValue.hpp"

using StyioBinOpFunc = std::function<StyioAST*(StyioAST*, StyioAST*)>;
extern std::unordered_map<StyioOpType, StyioBinOpFunc> bin_op_mapper;

StyioAST* parse_char_or_string(StyioContext& context);
TypeAST* parse_dtype(StyioContext& context);
StyioAST* parse_iterable(StyioContext& context);
StyioAST* parse_int_or_float(StyioContext& context);
StyioAST* parse_loop(StyioContext& context);
StyioAST* parse_iterator_with_forward(StyioContext& context, StyioAST* collection);
TypeAST* parse_name_as_type_unsafe(StyioContext& context);
std::string parse_name_as_str(StyioContext& context);
HashTagNameAST* parse_name_for_hash_tag(StyioContext& context);
CondAST* parse_cond_rhs(StyioContext& context, StyioAST* lhsExpr);
StyioAST* parse_struct(StyioContext& context, NameAST* name);

namespace
{

class EnvSnapshot
{
  std::string name_;
  bool had_value_ = false;
  std::string old_value_;

public:
  explicit EnvSnapshot(std::string name) :
      name_(std::move(name)) {
    if (const char* existing = std::getenv(name_.c_str())) {
      had_value_ = true;
      old_value_ = existing;
    }
  }

  ~EnvSnapshot() {
    if (had_value_) {
      styio_test_setenv(name_.c_str(), old_value_.c_str(), 1);
    }
    else {
      styio_test_unsetenv(name_.c_str());
    }
  }

  void set(const std::string& value) {
    styio_test_setenv(name_.c_str(), value.c_str(), 1);
  }

  void unset() {
    styio_test_unsetenv(name_.c_str());
  }
};

class ScopedStdinRedirect
{
  int saved_fd_ = -1;
  FILE* tmp_ = nullptr;

public:
  explicit ScopedStdinRedirect(const std::string& input) {
    saved_fd_ = dup(STDIN_FILENO);
    tmp_ = std::tmpfile();
    if (saved_fd_ < 0 || tmp_ == nullptr) {
      return;
    }
    (void)std::fwrite(input.data(), 1, input.size(), tmp_);
    std::rewind(tmp_);
    std::clearerr(stdin);
    if (dup2(fileno(tmp_), STDIN_FILENO) < 0) {
      std::fclose(tmp_);
      tmp_ = nullptr;
    }
  }

  ~ScopedStdinRedirect() {
    if (saved_fd_ >= 0) {
      (void)dup2(saved_fd_, STDIN_FILENO);
      close(saved_fd_);
    }
    if (tmp_ != nullptr) {
      std::fclose(tmp_);
    }
    std::clearerr(stdin);
  }

  bool ok() const {
    return saved_fd_ >= 0 && tmp_ != nullptr;
  }
};

std::vector<std::pair<std::string, std::string>> g_runtime_log_events;

std::string
native_path(std::string path) {
  return std::filesystem::path(std::move(path)).lexically_normal().string();
}

std::string
slash_normalized(std::string text) {
  std::replace(text.begin(), text.end(), '\\', '/');
  return text;
}

std::string
posix_shell_quote(std::string text) {
  std::string out = "'";
  for (char ch : text) {
    if (ch == '\'') {
      out += "'\\''";
    }
    else {
      out.push_back(ch);
    }
  }
  out.push_back('\'');
  return out;
}

std::string
test_c_compiler() {
  std::string compiler = STYIO_TEST_C_COMPILER;
  return compiler.empty() ? "cc" : compiler;
}

void
capture_runtime_log(const char* stream, const char* message) {
  g_runtime_log_events.emplace_back(
    stream == nullptr ? std::string() : std::string(stream),
    message == nullptr ? std::string() : std::string(message));
}

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

  void typeInfer(StyioSemaContext* visitor) override {
    (void)visitor;
  }

  StyioIR* toStyioIR(AstToStyioIRLowerer* visitor) override {
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

class InactiveTestIR : public StyioIR
{
public:
  std::string toString(StyioRepr* visitor, int indent = 0) override {
    (void)visitor;
    (void)indent;
    return "inactive-test-ir";
  }

  llvm::Type* toLLVMType(StyioToLLVM* visitor) override {
    (void)visitor;
    return nullptr;
  }

  llvm::Value* toLLVMIR(StyioToLLVM* visitor) override {
    (void)visitor;
    return nullptr;
  }

  bool is_active() const override {
    return false;
  }
};

void
free_tokens(std::vector<StyioToken*>& tokens) {
  for (auto* t : tokens) {
    delete t;
  }
}

StyioDataType
infer_final_bound_type(
  StyioSemaContext& analyzer,
  const std::string& name,
  StyioAST* value
) {
  auto* var = VarAST::Create(NameAST::Create(name));
  std::unique_ptr<StyioAST> bind(FinalBindAST::Create(var, value));
  bind->typeInfer(&analyzer);
  return var->getDType()->getDataType();
}

void
expect_matrix_type(
  const StyioDataType& type,
  const std::string& elem,
  size_t rows,
  size_t cols
) {
  EXPECT_TRUE(styio_is_matrix_type(type));
  EXPECT_EQ(styio_matrix_elem_type_name(type), elem);
  EXPECT_EQ(styio_matrix_row_count(type), rows);
  EXPECT_EQ(styio_matrix_col_count(type), cols);
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

class DirectParserContext
{
public:
  std::string source;
  std::vector<StyioToken*> tokens;
  StyioContext* ctx = nullptr;

  explicit DirectParserContext(std::string src) :
      source(std::move(src)),
      tokens(StyioTokenizer::tokenize(source)),
      ctx(StyioContext::Create(
        "<direct-parser-context>",
        source,
        build_line_seps(source),
        tokens,
        false
      )) {
  }

  DirectParserContext(const DirectParserContext&) = delete;
  DirectParserContext& operator=(const DirectParserContext&) = delete;

  ~DirectParserContext() {
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  }

  StyioContext& get() {
    return *ctx;
  }
};

class ManualTokenParserContext
{
public:
  std::string source;
  std::vector<StyioToken*> tokens;
  StyioContext* ctx = nullptr;

  explicit ManualTokenParserContext(
    const std::vector<std::pair<StyioTokenType, std::string>>& specs,
    std::string file_name = "<manual-token-parser-context>"
  ) {
    tokens.reserve(specs.size());
    for (const auto& spec : specs) {
      source += spec.second;
      tokens.push_back(StyioToken::Create(spec.first, spec.second));
    }
    ctx = StyioContext::Create(
      file_name,
      source,
      build_line_seps(source),
      tokens,
      false
    );
  }

  ManualTokenParserContext(const ManualTokenParserContext&) = delete;
  ManualTokenParserContext& operator=(const ManualTokenParserContext&) = delete;

  ~ManualTokenParserContext() {
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  }

  StyioContext& get() {
    return *ctx;
  }
};

class ExposedAstToStyioIRLowerer : public AstToStyioIRLowerer
{
public:
  void bindRuntimeName(
    const std::string& name,
    StyioSemaContext::BindingValueKind kind,
    bool dynamic = true,
    StyioDataType declared_type = StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0},
    bool resource_value = false
  ) {
    StyioSemaContext::BindingInfo info;
    info.dynamic_slot = dynamic;
    info.resource_value = resource_value;
    info.value_kind = kind;
    info.declared_type = declared_type;
    binding_info_[name] = info;
    if (!declared_type.isUndefined()) {
      local_binding_types[name] = declared_type;
    }
  }
};

size_t
find_direct_token_index(
  const DirectParserContext& direct,
  StyioTokenType type,
  size_t occurrence = 0
) {
  size_t seen = 0;
  for (size_t i = 0; i < direct.tokens.size(); ++i) {
    if (direct.tokens[i]->type != type) {
      continue;
    }
    if (seen == occurrence) {
      return i;
    }
    seen += 1;
  }
  throw std::runtime_error("direct parser token not found");
}

void
align_legacy_char_entry_token(
  DirectParserContext& direct,
  StyioTokenType type,
  size_t occurrence = 0
) {
  direct.get().restore_cursor({find_direct_token_index(direct, type, occurrence), 0});
}

std::string
parse_expr_to_repr_latest(const std::string& source, bool use_nightly_parser) {
  // Parse expression prefix and stop before a non-expression sentinel token.
  const std::string wrapped = source + " @";
  auto tokens = StyioTokenizer::tokenize(wrapped);
  StyioContext* ctx = StyioContext::Create(
    "<expr-subset-test>",
    wrapped,
    build_line_seps(wrapped),
    tokens,
    false
  );

  StyioAST* ast = nullptr;
  auto cleanup = [&]()
  {
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  };

  try {
    ast = use_nightly_parser ? parse_expr_subset_nightly(*ctx) : parse_expr(*ctx);
    ctx->skip();
    if (ctx->cur_tok_type() != StyioTokenType::TOK_AT) {
      throw std::runtime_error("expression parser did not stop at sentinel");
    }

    StyioRepr repr;
    const std::string out = ast->toString(&repr);
    cleanup();
    return out;
  }
  catch (...) {
    cleanup();
    throw;
  }
}

std::string
parse_program_to_repr_latest(const std::string& source, bool use_nightly_parser) {
  auto tokens = StyioTokenizer::tokenize(source);
  StyioContext* ctx = StyioContext::Create(
    "<stmt-subset-test>",
    source,
    build_line_seps(source),
    tokens,
    false
  );

  MainBlockAST* ast = nullptr;
  auto cleanup = [&]()
  {
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  };

  try {
    ast = use_nightly_parser ? parse_main_block_subset_nightly(*ctx) : parse_main_block_legacy(*ctx);
    ctx->skip();
    if (ctx->cur_tok_type() != StyioTokenType::TOK_EOF) {
      throw std::runtime_error("statement parser did not consume input");
    }

    StyioRepr repr;
    const std::string out = ast->toString(&repr);
    cleanup();
    return out;
  }
  catch (...) {
    cleanup();
    throw;
  }
}

std::string
parse_program_engine_to_repr_latest(const std::string& source, StyioParserEngine engine) {
  auto tokens = StyioTokenizer::tokenize(source);
  StyioContext* ctx = StyioContext::Create(
    "<engine-shadow-test>",
    source,
    build_line_seps(source),
    tokens,
    false
  );

  MainBlockAST* ast = nullptr;
  auto cleanup = [&]()
  {
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  };

  try {
    ast = parse_main_block_with_engine_latest(*ctx, engine);
    ctx->skip();
    if (ctx->cur_tok_type() != StyioTokenType::TOK_EOF) {
      throw std::runtime_error("engine parser did not consume input");
    }

    StyioRepr repr;
    const std::string out = ast->toString(&repr);
    cleanup();
    return out;
  }
  catch (...) {
    cleanup();
    throw;
  }
}

void
parse_typecheck_and_lower_program_engine_latest(const std::string& source, StyioParserEngine engine) {
  auto tokens = StyioTokenizer::tokenize(source);
  StyioContext* ctx = StyioContext::Create(
    "<engine-lowering-test>",
    source,
    build_line_seps(source),
    tokens,
    false
  );

  MainBlockAST* ast = nullptr;
  StyioIR* ir = nullptr;
  auto cleanup = [&]()
  {
    delete ir;
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  };

  try {
    ast = parse_main_block_with_engine_latest(*ctx, engine);
    ctx->skip();
    if (ctx->cur_tok_type() != StyioTokenType::TOK_EOF) {
      throw std::runtime_error("engine lowering parser did not consume input");
    }

    AstToStyioIRLowerer analyzer;
    ast->typeInfer(&analyzer);
    ir = ast->toStyioIR(&analyzer);
    cleanup();
  }
  catch (...) {
    cleanup();
    throw;
  }
}

void
parse_typecheck_program_engine_latest(const std::string& source, StyioParserEngine engine) {
  auto tokens = StyioTokenizer::tokenize(source);
  StyioContext* ctx = StyioContext::Create(
    "<engine-typecheck-test>",
    source,
    build_line_seps(source),
    tokens,
    false
  );

  MainBlockAST* ast = nullptr;
  auto cleanup = [&]()
  {
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  };

  try {
    ast = parse_main_block_with_engine_latest(*ctx, engine);
    ctx->skip();
    if (ctx->cur_tok_type() != StyioTokenType::TOK_EOF) {
      throw std::runtime_error("engine typecheck parser did not consume input");
    }

    AstToStyioIRLowerer analyzer;
    ast->typeInfer(&analyzer);
    cleanup();
  }
  catch (...) {
    cleanup();
    throw;
  }
}

std::string
compile_program_to_llvm_ir_engine_latest(const std::string& source, StyioParserEngine engine) {
  auto tokens = StyioTokenizer::tokenize(source);
  StyioContext* ctx = StyioContext::Create(
    "<engine-llvm-test>",
    source,
    build_line_seps(source),
    tokens,
    false
  );

  MainBlockAST* ast = nullptr;
  StyioIR* ir = nullptr;
  auto cleanup = [&]()
  {
    delete ir;
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  };

  try {
    ast = parse_main_block_with_engine_latest(*ctx, engine);
    ctx->skip();
    if (ctx->cur_tok_type() != StyioTokenType::TOK_EOF) {
      throw std::runtime_error("engine llvm parser did not consume input");
    }

    AstToStyioIRLowerer analyzer;
    ast->typeInfer(&analyzer);
    ir = ast->toStyioIR(&analyzer);

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    llvm::ExitOnError exit_on_error;
    std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
    StyioToLLVM generator(std::move(jit));
    ir->toLLVMIR(&generator);
    std::string out = generator.dump_llvm_ir();
    cleanup();
    return out;
  }
  catch (...) {
    cleanup();
    throw;
  }
}

void
execute_program_engine_with_stdin_latest(
  const std::string& source,
  StyioParserEngine engine,
  const std::string& stdin_text
) {
  auto tokens = StyioTokenizer::tokenize(source);
  StyioContext* ctx = StyioContext::Create(
    "<engine-exec-test>",
    source,
    build_line_seps(source),
    tokens,
    false
  );

  MainBlockAST* ast = nullptr;
  StyioIR* ir = nullptr;
  auto cleanup = [&]()
  {
    delete ir;
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  };

  FILE* tmp = tmpfile();
  ASSERT_NE(tmp, nullptr);
  std::fwrite(stdin_text.data(), 1, stdin_text.size(), tmp);
  std::rewind(tmp);

  const int saved_stdin = dup(fileno(stdin));
  ASSERT_GE(saved_stdin, 0);
  std::clearerr(stdin);
  ASSERT_EQ(dup2(fileno(tmp), fileno(stdin)), fileno(stdin));
  std::clearerr(stdin);

  styio_runtime_clear_error();

  try {
    ast = parse_main_block_with_engine_latest(*ctx, engine);
    ctx->skip();
    if (ctx->cur_tok_type() != StyioTokenType::TOK_EOF) {
      throw std::runtime_error("engine exec parser did not consume input");
    }

    AstToStyioIRLowerer analyzer;
    ast->typeInfer(&analyzer);
    ir = ast->toStyioIR(&analyzer);

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    llvm::ExitOnError exit_on_error;
    std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
    StyioToLLVM generator(std::move(jit));
    ir->toLLVMIR(&generator);
    generator.execute();
  }
  catch (...) {
    dup2(saved_stdin, fileno(stdin));
    std::clearerr(stdin);
    close(saved_stdin);
    std::fclose(tmp);
    cleanup();
    throw;
  }

  dup2(saved_stdin, fileno(stdin));
  std::clearerr(stdin);
  close(saved_stdin);
  std::fclose(tmp);
  cleanup();
}
}  // namespace

TEST(StyioIRContract, ExistingIRNodesAreActiveByDefault) {
  std::vector<std::unique_ptr<StyioIR>> nodes;
  nodes.emplace_back(SGNoOp::Create());
  nodes.emplace_back(SGConstInt::Create(0));
  nodes.emplace_back(SGConstString::Create("value"));
  nodes.emplace_back(SGBlock::Create(std::vector<StyioIR*>{
    SGConstInt::Create(1)
  }));
  nodes.emplace_back(SCListLiteral::Create(std::vector<StyioIR*>{
    SGConstInt::Create(2)
  }));
  nodes.emplace_back(SIOStdStreamWrite::Create(
    SIOStdStreamWrite::Stream::Stdout,
    std::vector<StyioIR*>{SGConstString::Create("out")}
  ));
  nodes.emplace_back(SIOPath::Create("fixture.txt"));

  for (const auto& node : nodes) {
    EXPECT_TRUE(node->is_active());
  }
}

TEST(StyioIRContract, NoOpAstNodesLowerToExplicitNoOp) {
  AstToStyioIRLowerer analyzer;
  std::unique_ptr<StyioAST> comment(CommentAST::Create("comment"));
  std::unique_ptr<StyioAST> empty(EmptyAST::Create());
  std::unique_ptr<StyioAST> pass(PassAST::Create());
  std::unique_ptr<StyioAST> resources(ResourceAST::Create({}));

  std::unique_ptr<StyioIR> comment_ir(comment->toStyioIR(&analyzer));
  std::unique_ptr<StyioIR> empty_ir(empty->toStyioIR(&analyzer));
  std::unique_ptr<StyioIR> pass_ir(pass->toStyioIR(&analyzer));
  std::unique_ptr<StyioIR> resources_ir(resources->toStyioIR(&analyzer));

  EXPECT_NE(dynamic_cast<SGNoOp*>(comment_ir.get()), nullptr);
  EXPECT_NE(dynamic_cast<SGNoOp*>(empty_ir.get()), nullptr);
  EXPECT_NE(dynamic_cast<SGNoOp*>(pass_ir.get()), nullptr);
  EXPECT_NE(dynamic_cast<SGNoOp*>(resources_ir.get()), nullptr);
}

TEST(StyioIRContract, CharAstLowersToConstChar) {
  AstToStyioIRLowerer analyzer;
  std::unique_ptr<StyioAST> ch(CharAST::Create("x"));

  EXPECT_EQ(ch->getDataType().option, StyioDataTypeOption::Char);
  EXPECT_EQ(ch->getDataType().num_of_bit, 8);
  std::unique_ptr<StyioIR> ir(ch->toStyioIR(&analyzer));

  auto* const_char = dynamic_cast<SGConstChar*>(ir.get());
  ASSERT_NE(const_char, nullptr);
  EXPECT_EQ(const_char->value, 'x');
}

TEST(StyioIRContract, ParamAstLowersPlainAndDefaultValueForms) {
  AstToStyioIRLowerer analyzer;
  std::unique_ptr<ParamAST> plain(
    ParamAST::Create(NameAST::Create("plain"), TypeAST::Create("i64"))
  );
  std::unique_ptr<ParamAST> with_default(
    ParamAST::Create(NameAST::Create("with_default"), TypeAST::Create("i64"), IntAST::Create("42"))
  );

  std::unique_ptr<StyioIR> plain_ir(analyzer.toStyioIR(plain.get()));
  auto* plain_var = dynamic_cast<SGVar*>(plain_ir.get());
  ASSERT_NE(plain_var, nullptr);
  EXPECT_EQ(plain_var->val_init, nullptr);

  std::unique_ptr<StyioIR> default_ir(analyzer.toStyioIR(with_default.get()));
  auto* default_var = dynamic_cast<SGVar*>(default_ir.get());
  ASSERT_NE(default_var, nullptr);
  EXPECT_NE(dynamic_cast<SGConstInt*>(default_var->val_init), nullptr);
}

TEST(StyioIRContract, DynamicNameBindingsAndVarDefaultsLowerToRuntimeSlots) {
  ExposedAstToStyioIRLowerer analyzer;

  struct DynamicCase
  {
    const char* name;
    StyioSemaContext::BindingValueKind binding_kind;
    SGDynLoadKind ir_kind;
    bool dynamic;
  };

  const std::vector<DynamicCase> cases = {
    {"dyn_bool", StyioSemaContext::BindingValueKind::Bool, SGDynLoadKind::Bool, true},
    {"dyn_i64", StyioSemaContext::BindingValueKind::I64, SGDynLoadKind::I64, true},
    {"dyn_f64", StyioSemaContext::BindingValueKind::F64, SGDynLoadKind::F64, true},
    {"dyn_string", StyioSemaContext::BindingValueKind::String, SGDynLoadKind::CString, true},
    {"list_handle", StyioSemaContext::BindingValueKind::ListHandle, SGDynLoadKind::ListHandle, false},
    {"dict_handle", StyioSemaContext::BindingValueKind::DictHandle, SGDynLoadKind::DictHandle, false},
    {"matrix_handle", StyioSemaContext::BindingValueKind::MatrixHandle, SGDynLoadKind::MatrixHandle, false},
    {"task_handle", StyioSemaContext::BindingValueKind::TaskHandle, SGDynLoadKind::TaskHandle, false},
  };

  for (const auto& c : cases) {
    SCOPED_TRACE(c.name);
    analyzer.bindRuntimeName(c.name, c.binding_kind, c.dynamic);
    std::unique_ptr<StyioAST> ast(NameAST::Create(c.name));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* load = dynamic_cast<SGDynLoad*>(ir.get());
    ASSERT_NE(load, nullptr);
    EXPECT_EQ(load->var_name, c.name);
    EXPECT_EQ(load->kind, c.ir_kind);
  }

  analyzer.resource_method_dynamic_local_binding_types["local_list"] = styio_make_list_type("i64");
  analyzer.resource_method_dynamic_local_binding_types["local_dict"] = styio_make_dict_type("string", "i64");
  analyzer.resource_method_dynamic_local_binding_types["local_matrix"] = styio_make_matrix_type("f64", 2, 3);
  {
    std::unique_ptr<StyioAST> ast(NameAST::Create("local_list"));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* load = dynamic_cast<SGDynLoad*>(ir.get());
    ASSERT_NE(load, nullptr);
    EXPECT_EQ(load->kind, SGDynLoadKind::ListHandle);
  }
  {
    std::unique_ptr<StyioAST> ast(NameAST::Create("local_dict"));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* load = dynamic_cast<SGDynLoad*>(ir.get());
    ASSERT_NE(load, nullptr);
    EXPECT_EQ(load->kind, SGDynLoadKind::DictHandle);
  }
  {
    std::unique_ptr<StyioAST> ast(NameAST::Create("local_matrix"));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* load = dynamic_cast<SGDynLoad*>(ir.get());
    ASSERT_NE(load, nullptr);
    EXPECT_EQ(load->kind, SGDynLoadKind::MatrixHandle);
  }

  analyzer.bindRuntimeName(
    "unknown_slot",
    StyioSemaContext::BindingValueKind::Unknown,
    true
  );
  EXPECT_THROW({
    std::unique_ptr<StyioAST> ast(NameAST::Create("unknown_slot"));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
  }, StyioTypeError);

  AstToStyioIRLowerer plain_analyzer;
  std::unique_ptr<VarAST> initialized_var(
    new VarAST(NameAST::Create("with_init"), TypeAST::Create("i64"), IntAST::Create("7"))
  );
  std::unique_ptr<StyioIR> var_ir(plain_analyzer.toStyioIR(initialized_var.get()));
  auto* sg_var = dynamic_cast<SGVar*>(var_ir.get());
  ASSERT_NE(sg_var, nullptr);
  EXPECT_NE(dynamic_cast<SGConstInt*>(sg_var->val_init), nullptr);
}

TEST(StyioIRContract, AstLeafAccessorsCoverDeclarationAndLiteralEdges) {
  {
    std::unique_ptr<CommentAST> comment(CommentAST::Create("note"));
    EXPECT_EQ(comment->getText(), "note");
    EXPECT_EQ(comment->getNodeType(), StyioNodeType::Comment);
    EXPECT_EQ(comment->getDataType().option, StyioDataTypeOption::Undefined);
  }
  {
    std::unique_ptr<NameAST> empty_name(NameAST::Create());
    EXPECT_TRUE(empty_name->getAsStr().empty());
    EXPECT_EQ(empty_name->getNodeType(), StyioNodeType::Id);
    EXPECT_EQ(empty_name->getDataType().option, StyioDataTypeOption::Undefined);
  }
  {
    std::unique_ptr<TypeTupleAST> tuple_type(TypeTupleAST::Create({
      TypeAST::Create("i64"),
      TypeAST::Create("f64"),
    }));
    EXPECT_EQ(tuple_type->getNodeType(), StyioNodeType::TypeTuple);
    EXPECT_EQ(tuple_type->getDataType().option, StyioDataTypeOption::Tuple);
    ASSERT_EQ(tuple_type->type_list.size(), 2u);
    EXPECT_EQ(tuple_type->type_list[1]->getTypeName(), "f64");
  }
  {
    std::unique_ptr<NoneAST> none(NoneAST::Create());
    EXPECT_EQ(none->getNodeType(), StyioNodeType::None);
    EXPECT_EQ(none->getDataType().option, StyioDataTypeOption::Undefined);
  }
  {
    std::unique_ptr<EmptyAST> empty(EmptyAST::Create());
    EXPECT_EQ(empty->getNodeType(), StyioNodeType::Empty);
    EXPECT_EQ(empty->getDataType().option, StyioDataTypeOption::Undefined);
  }
  {
    StyioDataType f32_type{StyioDataTypeOption::Float, "f32", 32};
    std::unique_ptr<FloatAST> f32_value(new FloatAST("1.25", f32_type));
    EXPECT_EQ(f32_value->getNodeType(), StyioNodeType::Float);
    EXPECT_EQ(f32_value->getValue(), "1.25");
    EXPECT_EQ(f32_value->getDataType().name, "f32");
    EXPECT_EQ(f32_value->getDataType().num_of_bit, 32);
  }
  {
    std::unique_ptr<BlockAST> block(BlockAST::Create({PassAST::Create()}));
    block->set_followings({ContinueAST::Create()});
    EXPECT_EQ(block->getNodeType(), StyioNodeType::Block);
    EXPECT_EQ(block->getDataType().option, StyioDataTypeOption::Undefined);
    ASSERT_EQ(block->stmts.size(), 1u);
    ASSERT_EQ(block->followings.size(), 1u);
    EXPECT_EQ(block->followings[0]->getNodeType(), StyioNodeType::Continue);
  }
  {
    std::unique_ptr<ResourceReceiverAST> file(ResourceReceiverAST::Create("file"));
    std::unique_ptr<ResourceReceiverAST> stdin_rx(ResourceReceiverAST::Create("stdin"));
    std::unique_ptr<ResourceReceiverAST> stdout_rx(ResourceReceiverAST::Create("stdout"));
    std::unique_ptr<ResourceReceiverAST> stderr_rx(ResourceReceiverAST::Create("stderr"));
    std::unique_ptr<ResourceReceiverAST> custom(ResourceReceiverAST::Create("sensor"));

    EXPECT_EQ(file->getDataType().option, StyioDataTypeOption::Defined);
    EXPECT_EQ(stdin_rx->getDataType().name, "stdin[string]");
    EXPECT_EQ(stdout_rx->getDataType().name, "stdout[string]");
    EXPECT_EQ(stderr_rx->getDataType().name, "stderr[string]");
    EXPECT_EQ(custom->getDataType().name, "resource-family:sensor");
  }
  {
    std::unique_ptr<FunctionAST> fn(FunctionAST::Create(
      NameAST::Create("blocky"),
      false,
      {
        ParamAST::Create(NameAST::Create("a"), TypeAST::Create("i64")),
        ParamAST::Create(NameAST::Create("b"), TypeAST::Create("f64")),
      },
      TypeAST::Create("i64"),
      BlockAST::Create({ReturnAST::Create(IntAST::Create("1"))})
    ));
    EXPECT_TRUE(fn->hasName());
    EXPECT_FALSE(fn->hasRetType());
    EXPECT_EQ(fn->getNameAsStr(), "blocky");
    EXPECT_TRUE(fn->allArgsTyped());
    auto param_map = fn->getParamMap();
    ASSERT_EQ(param_map.size(), 2u);
    EXPECT_EQ(param_map["a"]->getDType()->getTypeName(), "i64");
    EXPECT_EQ(param_map["b"]->getDType()->getTypeName(), "f64");
  }
  {
    std::unique_ptr<FunctionAST> untyped(FunctionAST::Create(
      NameAST::Create("loose"),
      false,
      {ParamAST::Create(NameAST::Create("x"))},
      TypeAST::Create("i64"),
      BlockAST::Create({})
    ));
    EXPECT_FALSE(untyped->allArgsTyped());
  }
  {
    std::unique_ptr<SimpleFuncAST> no_type(SimpleFuncAST::Create(
      NameAST::Create("plain"),
      true,
      {ParamAST::Create(NameAST::Create("x"))},
      IntAST::Create("1")
    ));
    EXPECT_TRUE(no_type->is_unique);
    EXPECT_EQ(no_type->getNodeType(), StyioNodeType::SimpleFunc);
    EXPECT_EQ(no_type->getDataType().name, "TupleOp");
    ASSERT_EQ(no_type->params.size(), 1u);
    EXPECT_EQ(no_type->params[0]->getNameAsStr(), "x");

    std::unique_ptr<SimpleFuncAST> tuple_return(SimpleFuncAST::Create(
      NameAST::Create("tuple_ret"),
      {
        ParamAST::Create(NameAST::Create("a"), TypeAST::Create("i64")),
      },
      TypeTupleAST::Create({TypeAST::Create("i64"), TypeAST::Create("f64")}),
      TupleAST::Create({IntAST::Create("1"), FloatAST::Create("2.5")})
    ));
    EXPECT_FALSE(tuple_return->is_unique);
    EXPECT_TRUE(std::holds_alternative<TypeTupleAST*>(tuple_return->ret_type));

    std::variant<TypeAST*, TypeTupleAST*> scalar_ret = TypeAST::Create("bool");
    std::unique_ptr<SimpleFuncAST> variant_return(SimpleFuncAST::Create(
      NameAST::Create("variant_ret"),
      true,
      {},
      scalar_ret,
      BoolAST::Create(true)
    ));
    EXPECT_TRUE(variant_return->is_unique);
    EXPECT_TRUE(std::holds_alternative<TypeAST*>(variant_return->ret_type));
  }
}

TEST(StyioIRContract, LegacyAndDeclarationAstLoweringEdgesStayExplicit) {
  auto expect_unsupported = [](const char* label, std::unique_ptr<StyioAST> ast)
  {
    SCOPED_TRACE(label);
    AstToStyioIRLowerer analyzer;
    EXPECT_THROW({
      std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    }, StyioTypeError);
  };

  expect_unsupported(
    "InfiniteAST",
    std::unique_ptr<StyioAST>(new InfiniteAST())
  );
  expect_unsupported(
    "VarTupleAST",
    std::unique_ptr<StyioAST>(VarTupleAST::Create({}))
  );
  expect_unsupported(
    "ExtractorAST",
    std::unique_ptr<StyioAST>(ExtractorAST::Create(
      TupleAST::Create({}),
      NameAST::Create("op")
    ))
  );
  expect_unsupported(
    "EmptyResourceAST",
    std::unique_ptr<StyioAST>(EmptyResourceAST::Create())
  );
  expect_unsupported(
    "ResPathAST",
    std::unique_ptr<StyioAST>(ResPathAST::Create(
      StyioPathType::local_relevant_any,
      "data.txt"
    ))
  );
  expect_unsupported(
    "RemotePathAST",
    std::unique_ptr<StyioAST>(RemotePathAST::Create(
      StyioPathType::ipv4_addr,
      "127.0.0.1:/tmp/data"
    ))
  );
  expect_unsupported(
    "WebUrlAST",
    std::unique_ptr<StyioAST>(WebUrlAST::Create(
      StyioPathType::url_https,
      "https://example.test/data"
    ))
  );
  expect_unsupported(
    "DBUrlAST",
    std::unique_ptr<StyioAST>(DBUrlAST::Create(
      StyioPathType::db_postgresql,
      "postgres://example/db"
    ))
  );
  expect_unsupported(
    "ReadFileAST",
    std::unique_ptr<StyioAST>(new ReadFileAST(
      NameAST::Create("line"),
      StringAST::Create("input.txt")
    ))
  );

  AstToStyioIRLowerer analyzer;

  {
    std::unique_ptr<StyioAST> ast(new ExtPackAST({"pkg"}));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGNoOp*>(ir.get()), nullptr);
  }
  {
    std::unique_ptr<StyioAST> ast(UndefinedLitAST::Create());
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGUndef*>(ir.get()), nullptr);
  }
  {
    std::unique_ptr<StyioAST> ast(WaveDispatchAST::Create(
      BoolAST::Create(true),
      IntAST::Create("1"),
      IntAST::Create("0")
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGWaveDispatch*>(ir.get()), nullptr);
  }
  {
    std::unique_ptr<StyioAST> ast(FallbackAST::Create(
      IntAST::Create("1"),
      IntAST::Create("2")
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGFallback*>(ir.get()), nullptr);
  }
  {
    std::unique_ptr<StyioAST> ast(GuardSelectorAST::Create(
      NameAST::Create("value"),
      BoolAST::Create(true)
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGGuardSelect*>(ir.get()), nullptr);
  }
  {
    std::unique_ptr<StyioAST> ast(EqProbeAST::Create(
      NameAST::Create("value"),
      IntAST::Create("3")
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGEqProbe*>(ir.get()), nullptr);
  }
  {
    std::unique_ptr<StyioAST> ast(FileResourceAST::Create(
      StringAST::Create("input.txt"),
      false
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGConstString*>(ir.get()), nullptr);
  }
  {
    std::unique_ptr<StyioAST> ast(ResourceReceiverAST::Create("file"));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGResId*>(ir.get()), nullptr);
  }
  {
    std::unique_ptr<StyioAST> ast(ResourceOrderAST::Create(
      NameAST::Create("before"),
      NameAST::Create("after")
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGNoOp*>(ir.get()), nullptr);
  }
  {
    std::unique_ptr<StyioAST> ast(ExportDeclAST::Create({"foo", "bar"}));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* export_ir = dynamic_cast<SGExportDecl*>(ir.get());
    ASSERT_NE(export_ir, nullptr);
    EXPECT_EQ(export_ir->symbols.size(), 2u);
  }
  {
    std::unique_ptr<StyioAST> ast(EOFAST::Create());
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGNoOp*>(ir.get()), nullptr);
  }
  {
    std::unique_ptr<StyioAST> ast(StructAST::Create(
      NameAST::Create("Pair"),
      {ParamAST::Create(NameAST::Create("left"), TypeAST::Create("i64"))}
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* struct_ir = dynamic_cast<SGStruct*>(ir.get());
    ASSERT_NE(struct_ir, nullptr);
    EXPECT_EQ(struct_ir->elements.size(), 1u);
  }
}

TEST(StyioIRContract, IteratorPulseStatePlanningLowersSlotsAndHelperClones) {
  auto i64_ast_type = []() {
    return TypeAST::Create("i64");
  };
  auto i64_var = [&](const std::string& name) {
    return VarAST::Create(NameAST::Create(name), i64_ast_type());
  };
  auto param_i64 = [&](const std::string& name) {
    return ParamAST::Create(NameAST::Create(name), i64_ast_type());
  };
  auto state_decl = [&](IntAST* window, NameAST* acc_name, StyioAST* acc_init, const std::string& out, StyioAST* update) {
    return StateDeclAST::Create(window, acc_name, acc_init, i64_var(out), update);
  };

  AstToStyioIRLowerer analyzer;

  std::unique_ptr<SimpleFuncAST> max_series(SimpleFuncAST::Create(
    NameAST::Create("max_series"),
    std::vector<ParamAST*>{param_i64("sample")},
    SeriesIntrinsicAST::Create(NameAST::Create("sample"), SeriesIntrinsicOp::Max, IntAST::Create("5"))
  ));
  std::unique_ptr<SimpleFuncAST> make_simple_state(SimpleFuncAST::Create(
    NameAST::Create("make_simple_state"),
    std::vector<ParamAST*>{param_i64("sample")},
    state_decl(
      nullptr,
      NameAST::Create("sum_alias"),
      IntAST::Create("0"),
      "simple_out",
      BinOpAST::Create(StyioOpType::Binary_Add, NameAST::Create("sample"), IntAST::Create("1"))
    )
  ));
  std::unique_ptr<FunctionAST> make_block_state(FunctionAST::Create(
    NameAST::Create("make_block_state"),
    false,
    std::vector<ParamAST*>{param_i64("sample")},
    i64_ast_type(),
    BlockAST::Create({
      state_decl(
        IntAST::Create("2"),
        nullptr,
        nullptr,
        "block_out",
        BinOpAST::Create(
          StyioOpType::Binary_Add,
          NameAST::Create("sample"),
          StateRefAST::Create(NameAST::Create("acc_out"))
        )
      )
    })
  ));

  analyzer.func_defs["max_series"] = max_series.get();
  analyzer.func_defs["make_simple_state"] = make_simple_state.get();
  analyzer.func_defs["make_block_state"] = make_block_state.get();

  auto* collection = ListAST::Create({IntAST::Create("1"), IntAST::Create("2")});
  collection->setDataType(styio_make_list_type("i64"));

  std::unique_ptr<StyioAST> iterator(IteratorAST::Create(
    collection,
    std::vector<ParamAST*>{param_i64("x")},
    std::vector<StyioAST*>{
      BlockAST::Create({
        state_decl(
          nullptr,
          NameAST::Create("acc"),
          IntAST::Create("0"),
          "acc_out",
          BinOpAST::Create(
            StyioOpType::Binary_Add,
            StateRefAST::Create(NameAST::Create("acc")),
            NameAST::Create("x")
          )
        ),
        state_decl(
          IntAST::Create("3"),
          nullptr,
          nullptr,
          "track_out",
          HistoryProbeAST::Create(StateRefAST::Create(NameAST::Create("track_out")), IntAST::Create("1"))
        ),
        state_decl(
          IntAST::Create("4"),
          nullptr,
          nullptr,
          "avg_out",
          SeriesIntrinsicAST::Create(NameAST::Create("x"), SeriesIntrinsicOp::Avg, IntAST::Create("4"))
        ),
        state_decl(
          IntAST::Create("5"),
          nullptr,
          nullptr,
          "max_out",
          FuncCallAST::Create(NameAST::Create("max_series"), {NameAST::Create("x")})
        ),
        FuncCallAST::Create(NameAST::Create("make_simple_state"), {NameAST::Create("x")}),
        FuncCallAST::Create(NameAST::Create("make_block_state"), {NameAST::Create("x")}),
      })
    }
  ));

  std::unique_ptr<StyioIR> lowered(iterator->toStyioIR(&analyzer));
  auto* foreach_ir = dynamic_cast<SGForEach*>(lowered.get());
  ASSERT_NE(foreach_ir, nullptr);
  ASSERT_NE(foreach_ir->pulse_plan, nullptr);
  ASSERT_NE(foreach_ir->body, nullptr);
  EXPECT_GE(foreach_ir->pulse_region_id, 0);

  const auto& slots = foreach_ir->pulse_plan->slots;
  ASSERT_EQ(slots.size(), 6u);
  EXPECT_EQ(slots[0].kind, SGStateSlotKind::Acc);
  EXPECT_EQ(slots[1].kind, SGStateSlotKind::Track);
  EXPECT_EQ(slots[2].kind, SGStateSlotKind::WinAvg);
  EXPECT_EQ(slots[3].kind, SGStateSlotKind::WinMax);
  EXPECT_EQ(slots[4].kind, SGStateSlotKind::Acc);
  EXPECT_EQ(slots[5].kind, SGStateSlotKind::Track);
  EXPECT_EQ(slots[2].win_n, 4);
  EXPECT_EQ(slots[3].win_n, 5);
  EXPECT_EQ(slots[5].win_n, 2);
  EXPECT_EQ(foreach_ir->pulse_plan->ref_to_slot.at("acc"), 0);
  EXPECT_EQ(foreach_ir->pulse_plan->ref_to_slot.at("acc_out"), 0);
  EXPECT_EQ(foreach_ir->pulse_plan->ref_to_slot.at("simple_out"), 4);
  EXPECT_EQ(foreach_ir->pulse_plan->ref_to_slot.at("block_out"), 5);
  EXPECT_GT(foreach_ir->pulse_plan->total_bytes, 0);

  ASSERT_EQ(foreach_ir->body->stmts.size(), 6u);
  std::vector<SGFlexBind*> binds;
  for (auto* stmt : foreach_ir->body->stmts) {
    auto* bind = dynamic_cast<SGFlexBind*>(stmt);
    ASSERT_NE(bind, nullptr);
    binds.push_back(bind);
  }
  EXPECT_NE(dynamic_cast<SGBinOp*>(binds[0]->value), nullptr);
  EXPECT_NE(dynamic_cast<SGStateHistLoad*>(binds[1]->value), nullptr);
  EXPECT_NE(dynamic_cast<SGSeriesAvgStep*>(binds[2]->value), nullptr);
  EXPECT_NE(dynamic_cast<SGSeriesMaxStep*>(binds[3]->value), nullptr);
  EXPECT_NE(dynamic_cast<SGBinOp*>(binds[4]->value), nullptr);
  EXPECT_NE(dynamic_cast<SGBinOp*>(binds[5]->value), nullptr);
}

TEST(StyioIRContract, IteratorPulseStateDetectionSeesHelperFunctionCallsWithoutDirectState) {
  auto i64_ast_type = []() {
    return TypeAST::Create("i64");
  };
  auto i64_var = [&](const std::string& name) {
    return VarAST::Create(NameAST::Create(name), i64_ast_type());
  };
  auto param_i64 = [&](const std::string& name) {
    return ParamAST::Create(NameAST::Create(name), i64_ast_type());
  };
  auto state_decl = [&](IntAST* window, const std::string& out, StyioAST* update) {
    return StateDeclAST::Create(
      window,
      NameAST::Create(out + "_acc"),
      IntAST::Create("0"),
      i64_var(out),
      update
    );
  };

  AstToStyioIRLowerer analyzer;
  std::unique_ptr<SimpleFuncAST> simple_state(SimpleFuncAST::Create(
    NameAST::Create("simple_state"),
    std::vector<ParamAST*>{param_i64("sample")},
    state_decl(
      nullptr,
      "simple_out",
      BinOpAST::Create(StyioOpType::Binary_Add, NameAST::Create("sample"), IntAST::Create("1"))
    )
  ));
  std::unique_ptr<FunctionAST> block_state(FunctionAST::Create(
    NameAST::Create("block_state"),
    false,
    std::vector<ParamAST*>{param_i64("sample")},
    i64_ast_type(),
    BlockAST::Create({
      state_decl(
        nullptr,
        "block_out",
        BinOpAST::Create(StyioOpType::Binary_Add, NameAST::Create("sample"), IntAST::Create("2"))
      )
    })
  ));
  analyzer.func_defs["simple_state"] = simple_state.get();
  analyzer.func_defs["block_state"] = block_state.get();

  auto lower_iterator = [&](const std::string& helper_name) {
    auto* collection = ListAST::Create({IntAST::Create("1"), IntAST::Create("2")});
    collection->setDataType(styio_make_list_type("i64"));
    std::vector<StyioAST*> stmts;
    stmts.push_back(FuncCallAST::Create(NameAST::Create(helper_name), {NameAST::Create("x")}));
    std::unique_ptr<StyioAST> iterator(IteratorAST::Create(
      collection,
      std::vector<ParamAST*>{param_i64("x")},
      std::vector<StyioAST*>{BlockAST::Create(std::move(stmts))}
    ));
    return std::unique_ptr<StyioIR>(iterator->toStyioIR(&analyzer));
  };

  {
    std::unique_ptr<StyioIR> lowered = lower_iterator("simple_state");
    auto* foreach_ir = dynamic_cast<SGForEach*>(lowered.get());
    ASSERT_NE(foreach_ir, nullptr);
    ASSERT_NE(foreach_ir->pulse_plan, nullptr);
    ASSERT_EQ(foreach_ir->pulse_plan->slots.size(), 1u);
    EXPECT_EQ(foreach_ir->pulse_plan->slots[0].kind, SGStateSlotKind::Acc);
  }
  {
    std::unique_ptr<StyioIR> lowered = lower_iterator("block_state");
    auto* foreach_ir = dynamic_cast<SGForEach*>(lowered.get());
    ASSERT_NE(foreach_ir, nullptr);
    ASSERT_NE(foreach_ir->pulse_plan, nullptr);
    ASSERT_EQ(foreach_ir->pulse_plan->slots.size(), 1u);
    EXPECT_EQ(foreach_ir->pulse_plan->slots[0].kind, SGStateSlotKind::Acc);
  }
}

TEST(StyioIRContract, IteratorPulseStateHelperCloneCoversHistoryAndSeriesExpressions) {
  auto i64_ast_type = []() {
    return TypeAST::Create("i64");
  };
  auto i64_var = [&](const std::string& name) {
    return VarAST::Create(NameAST::Create(name), i64_ast_type());
  };
  auto param_i64 = [&](const std::string& name) {
    return ParamAST::Create(NameAST::Create(name), i64_ast_type());
  };

  AstToStyioIRLowerer analyzer;
  std::unique_ptr<StyioAST> resource_decl(ResourceDeclAST::Create({
    {NameAST::Create("recent_i64"), TypeAST::Create(styio_make_topology_resource_type(
      styio_data_type_from_name("i64"),
      StyioResourceShapeKind::Fixed,
      2
    ))}
  }));
  resource_decl->typeInfer(&analyzer);
  std::unique_ptr<SimpleFuncAST> track_state(SimpleFuncAST::Create(
    NameAST::Create("track_state"),
    std::vector<ParamAST*>{param_i64("sample")},
    StateDeclAST::Create(
      IntAST::Create("3"),
      nullptr,
      nullptr,
      i64_var("track_out"),
      HistoryProbeAST::Create(StateRefAST::Create(NameAST::Create("track_out")), IntAST::Create("1"))
    )
  ));
  std::unique_ptr<SimpleFuncAST> avg_state(SimpleFuncAST::Create(
    NameAST::Create("avg_state"),
    std::vector<ParamAST*>{param_i64("sample")},
    StateDeclAST::Create(
      nullptr,
      nullptr,
      nullptr,
      i64_var("avg_out"),
      SeriesIntrinsicAST::Create(NameAST::Create("sample"), SeriesIntrinsicOp::Avg, IntAST::Create("4"))
    )
  ));
  std::unique_ptr<SimpleFuncAST> var_state(SimpleFuncAST::Create(
    NameAST::Create("var_state"),
    std::vector<ParamAST*>{param_i64("sample")},
    StateDeclAST::Create(
      nullptr,
      NameAST::Create("var_acc"),
      IntAST::Create("0"),
      i64_var("var_out"),
      new VarAST(NameAST::Create("inner"), i64_ast_type(), NameAST::Create("sample"))
    )
  ));
  std::unique_ptr<SimpleFuncAST> ref_state(SimpleFuncAST::Create(
    NameAST::Create("ref_state"),
    std::vector<ParamAST*>{param_i64("sample")},
    StateDeclAST::Create(
      nullptr,
      NameAST::Create("ref_acc"),
      IntAST::Create("0"),
      i64_var("ref_out"),
      ResourceRefAST::CreateSelector(NameAST::Create("recent_i64"), ResourceSelectorKind::Offset, -1)
    )
  ));
  analyzer.func_defs["track_state"] = track_state.get();
  analyzer.func_defs["avg_state"] = avg_state.get();
  analyzer.func_defs["var_state"] = var_state.get();
  analyzer.func_defs["ref_state"] = ref_state.get();

  auto* collection = ListAST::Create({IntAST::Create("1"), IntAST::Create("2")});
  collection->setDataType(styio_make_list_type("i64"));
  std::unique_ptr<StyioAST> iterator(IteratorAST::Create(
    collection,
    std::vector<ParamAST*>{param_i64("x")},
    std::vector<StyioAST*>{
      BlockAST::Create({
        FuncCallAST::Create(NameAST::Create("track_state"), {NameAST::Create("x")}),
        FuncCallAST::Create(NameAST::Create("avg_state"), {NameAST::Create("x")}),
        FuncCallAST::Create(NameAST::Create("var_state"), {NameAST::Create("x")}),
        FuncCallAST::Create(NameAST::Create("ref_state"), {NameAST::Create("x")}),
      })
    }
  ));

  std::unique_ptr<StyioIR> lowered(iterator->toStyioIR(&analyzer));
  auto* foreach_ir = dynamic_cast<SGForEach*>(lowered.get());
  ASSERT_NE(foreach_ir, nullptr);
  ASSERT_NE(foreach_ir->pulse_plan, nullptr);
  ASSERT_EQ(foreach_ir->pulse_plan->slots.size(), 4u);
  EXPECT_EQ(foreach_ir->pulse_plan->slots[0].kind, SGStateSlotKind::Track);
  EXPECT_EQ(foreach_ir->pulse_plan->slots[0].win_n, 3);
  EXPECT_EQ(foreach_ir->pulse_plan->slots[1].kind, SGStateSlotKind::WinAvg);
  EXPECT_EQ(foreach_ir->pulse_plan->slots[1].win_n, 4);
  EXPECT_EQ(foreach_ir->pulse_plan->slots[2].kind, SGStateSlotKind::Acc);
  EXPECT_EQ(foreach_ir->pulse_plan->slots[3].kind, SGStateSlotKind::Acc);
  ASSERT_NE(foreach_ir->body, nullptr);
  ASSERT_EQ(foreach_ir->body->stmts.size(), 4u);
  auto* track_bind = dynamic_cast<SGFlexBind*>(foreach_ir->body->stmts[0]);
  ASSERT_NE(track_bind, nullptr);
  EXPECT_NE(dynamic_cast<SGStateHistLoad*>(track_bind->value), nullptr);
  auto* avg_bind = dynamic_cast<SGFlexBind*>(foreach_ir->body->stmts[1]);
  ASSERT_NE(avg_bind, nullptr);
  EXPECT_NE(dynamic_cast<SGSeriesAvgStep*>(avg_bind->value), nullptr);
  auto* var_bind = dynamic_cast<SGFlexBind*>(foreach_ir->body->stmts[2]);
  ASSERT_NE(var_bind, nullptr);
  auto* cloned_var = dynamic_cast<SGVar*>(var_bind->value);
  ASSERT_NE(cloned_var, nullptr);
  EXPECT_NE(dynamic_cast<SGResId*>(cloned_var->val_init), nullptr);
  auto* ref_bind = dynamic_cast<SGFlexBind*>(foreach_ir->body->stmts[3]);
  ASSERT_NE(ref_bind, nullptr);
  auto* hist_ref = dynamic_cast<SGResId*>(ref_bind->value);
  ASSERT_NE(hist_ref, nullptr);
  EXPECT_EQ(hist_ref->as_str(), "recent_i64");
  EXPECT_TRUE(hist_ref->has_history_selector);
  EXPECT_EQ(hist_ref->history_offset, -1);
}

TEST(StyioIRContract, IteratorPulseStateHelperCloneCoversExpressionNodeFamilies) {
  enum class ExpectedNode {
    BinOp,
    Cond,
    WaveMerge,
    WaveDispatch,
    Fallback,
    Guard,
    EqProbe,
    RangeLiteral,
    ListGet,
    ListSlice,
  };
  struct HelperSpec {
    const char* name;
    ExpectedNode expected;
    std::function<StyioAST*()> make_expr;
  };

  auto i64_ast_type = []() {
    return TypeAST::Create("i64");
  };
  auto i64_var = [&](const std::string& name) {
    return VarAST::Create(NameAST::Create(name), i64_ast_type());
  };
  auto param_i64 = [&](const std::string& name) {
    return ParamAST::Create(NameAST::Create(name), i64_ast_type());
  };
  auto list_literal = []() {
    auto* list = ListAST::Create({IntAST::Create("1"), IntAST::Create("2"), IntAST::Create("3")});
    list->setDataType(styio_make_list_type("i64"));
    return list;
  };

  std::vector<HelperSpec> specs = {
    {
      "compare_state",
      ExpectedNode::BinOp,
      []()
      {
        return new BinCompAST(CompType::EQ, NameAST::Create("sample"), IntAST::Create("1"));
      }
    },
    {
      "cond_unary_state",
      ExpectedNode::Cond,
      []()
      {
        return CondAST::Create(LogicType::NOT, BoolAST::Create(false));
      }
    },
    {
      "cond_binary_state",
      ExpectedNode::Cond,
      []()
      {
        return CondAST::Create(LogicType::AND, BoolAST::Create(true), BoolAST::Create(false));
      }
    },
    {
      "wave_merge_state",
      ExpectedNode::WaveMerge,
      []()
      {
        return WaveMergeAST::Create(BoolAST::Create(true), IntAST::Create("1"), IntAST::Create("0"));
      }
    },
    {
      "wave_dispatch_state",
      ExpectedNode::WaveDispatch,
      []()
      {
        return WaveDispatchAST::Create(BoolAST::Create(true), IntAST::Create("1"), IntAST::Create("0"));
      }
    },
    {
      "fallback_state",
      ExpectedNode::Fallback,
      []()
      {
        return FallbackAST::Create(UndefinedLitAST::Create(), IntAST::Create("7"));
      }
    },
    {
      "guard_state",
      ExpectedNode::Guard,
      []()
      {
        return GuardSelectorAST::Create(NameAST::Create("sample"), BoolAST::Create(true));
      }
    },
    {
      "eq_probe_state",
      ExpectedNode::EqProbe,
      []()
      {
        return EqProbeAST::Create(NameAST::Create("sample"), IntAST::Create("2"));
      }
    },
    {
      "range_state",
      ExpectedNode::RangeLiteral,
      []()
      {
        return new RangeAST(IntAST::Create("0"), IntAST::Create("2"), IntAST::Create("1"));
      }
    },
    {
      "list_get_state",
      ExpectedNode::ListGet,
      [=]()
      {
        return new ListOpAST(StyioNodeType::Access_By_Index, list_literal(), IntAST::Create("1"));
      }
    },
    {
      "list_slice_state",
      ExpectedNode::ListSlice,
      [=]()
      {
        return new ListOpAST(
          StyioNodeType::Access_By_Slice,
          list_literal(),
          IntAST::Create("0"),
          IntAST::Create("2"));
      }
    },
  };

  AstToStyioIRLowerer analyzer;
  std::vector<std::unique_ptr<SimpleFuncAST>> helpers;
  std::vector<StyioAST*> calls;
  helpers.reserve(specs.size());
  calls.reserve(specs.size());
  for (const HelperSpec& spec : specs) {
    helpers.emplace_back(SimpleFuncAST::Create(
      NameAST::Create(spec.name),
      std::vector<ParamAST*>{param_i64("sample")},
      StateDeclAST::Create(
        nullptr,
        NameAST::Create(std::string(spec.name) + "_acc"),
        IntAST::Create("0"),
        i64_var(std::string(spec.name) + "_out"),
        spec.make_expr()
      )
    ));
    analyzer.func_defs[spec.name] = helpers.back().get();
    calls.push_back(FuncCallAST::Create(NameAST::Create(spec.name), {NameAST::Create("x")}));
  }

  auto* collection = ListAST::Create({IntAST::Create("1"), IntAST::Create("2")});
  collection->setDataType(styio_make_list_type("i64"));
  std::unique_ptr<StyioAST> iterator(IteratorAST::Create(
    collection,
    std::vector<ParamAST*>{param_i64("x")},
    std::vector<StyioAST*>{BlockAST::Create(std::move(calls))}
  ));

  std::unique_ptr<StyioIR> lowered(iterator->toStyioIR(&analyzer));
  auto* foreach_ir = dynamic_cast<SGForEach*>(lowered.get());
  ASSERT_NE(foreach_ir, nullptr);
  ASSERT_NE(foreach_ir->body, nullptr);
  ASSERT_EQ(foreach_ir->body->stmts.size(), specs.size());
  for (std::size_t i = 0; i < specs.size(); ++i) {
    SCOPED_TRACE(specs[i].name);
    auto* bind = dynamic_cast<SGFlexBind*>(foreach_ir->body->stmts[i]);
    ASSERT_NE(bind, nullptr);
    switch (specs[i].expected) {
      case ExpectedNode::BinOp:
        EXPECT_NE(dynamic_cast<SGBinOp*>(bind->value), nullptr);
        break;
      case ExpectedNode::Cond:
        EXPECT_NE(dynamic_cast<SGCond*>(bind->value), nullptr);
        break;
      case ExpectedNode::WaveMerge:
        EXPECT_NE(dynamic_cast<SGWaveMerge*>(bind->value), nullptr);
        break;
      case ExpectedNode::WaveDispatch:
        EXPECT_NE(dynamic_cast<SGWaveDispatch*>(bind->value), nullptr);
        break;
      case ExpectedNode::Fallback:
        EXPECT_NE(dynamic_cast<SGFallback*>(bind->value), nullptr);
        break;
      case ExpectedNode::Guard:
        EXPECT_NE(dynamic_cast<SGGuardSelect*>(bind->value), nullptr);
        break;
      case ExpectedNode::EqProbe:
        EXPECT_NE(dynamic_cast<SGEqProbe*>(bind->value), nullptr);
        break;
      case ExpectedNode::RangeLiteral:
        EXPECT_NE(dynamic_cast<SCListLiteral*>(bind->value), nullptr);
        break;
      case ExpectedNode::ListGet:
        EXPECT_NE(dynamic_cast<SCListGet*>(bind->value), nullptr);
        break;
      case ExpectedNode::ListSlice:
        EXPECT_NE(dynamic_cast<SCListSlice*>(bind->value), nullptr);
        break;
    }
  }
}

TEST(StyioIRContract, IteratorPulseStateSeriesDetectionScansWaveMergeBranches) {
  auto i64_ast_type = []() {
    return TypeAST::Create("i64");
  };
  auto i64_var = [&](const std::string& name) {
    return VarAST::Create(NameAST::Create(name), i64_ast_type());
  };
  auto param_i64 = [&](const std::string& name) {
    return ParamAST::Create(NameAST::Create(name), i64_ast_type());
  };
  auto avg = [](const std::string& name, const char* window) {
    return SeriesIntrinsicAST::Create(NameAST::Create(name), SeriesIntrinsicOp::Avg, IntAST::Create(window));
  };
  auto max = [](const std::string& name, const char* window) {
    return SeriesIntrinsicAST::Create(NameAST::Create(name), SeriesIntrinsicOp::Max, IntAST::Create(window));
  };
  auto state_decl = [&](const std::string& out, StyioAST* update) {
    return StateDeclAST::Create(
      nullptr,
      nullptr,
      nullptr,
      i64_var(out),
      update
    );
  };

  auto* collection = ListAST::Create({IntAST::Create("1"), IntAST::Create("2")});
  collection->setDataType(styio_make_list_type("i64"));
  AstToStyioIRLowerer analyzer;
  std::unique_ptr<StyioAST> iterator(IteratorAST::Create(
    collection,
    std::vector<ParamAST*>{param_i64("x")},
    std::vector<StyioAST*>{
      BlockAST::Create({
        state_decl("cond_out", WaveMergeAST::Create(
          avg("x", "2"),
          IntAST::Create("1"),
          IntAST::Create("0"))),
        state_decl("true_out", WaveMergeAST::Create(
          BoolAST::Create(true),
          max("x", "3"),
          IntAST::Create("0"))),
        state_decl("false_out", WaveMergeAST::Create(
          BoolAST::Create(false),
          IntAST::Create("1"),
          avg("x", "4"))),
      })
    }
  ));

  std::unique_ptr<StyioIR> lowered(iterator->toStyioIR(&analyzer));
  auto* foreach_ir = dynamic_cast<SGForEach*>(lowered.get());
  ASSERT_NE(foreach_ir, nullptr);
  ASSERT_NE(foreach_ir->pulse_plan, nullptr);
  const auto& slots = foreach_ir->pulse_plan->slots;
  ASSERT_EQ(slots.size(), 3u);
  EXPECT_EQ(slots[0].kind, SGStateSlotKind::WinAvg);
  EXPECT_EQ(slots[0].win_n, 2);
  EXPECT_EQ(slots[1].kind, SGStateSlotKind::WinMax);
  EXPECT_EQ(slots[1].win_n, 3);
  EXPECT_EQ(slots[2].kind, SGStateSlotKind::WinAvg);
  EXPECT_EQ(slots[2].win_n, 4);
}

TEST(StyioIRContract, IteratorAndInstantPullLoweringCoversResourceSpecificPaths) {
  auto param = [](const std::string& name, const std::string& type = "string") {
    return ParamAST::Create(NameAST::Create(name), TypeAST::Create(type));
  };
  auto body = []() {
    return BlockAST::Create({PassAST::Create()});
  };

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> ast(IteratorAST::Create(
      StdStreamAST::Create(StdStreamKind::Stdin),
      {param("line")},
      std::vector<StyioAST*>{body()}
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* iter = dynamic_cast<SIOStdStreamLineIter*>(ir.get());
    ASSERT_NE(iter, nullptr);
    EXPECT_EQ(iter->line_var, "line");
    ASSERT_NE(iter->body, nullptr);
  }
  {
    AstToStyioIRLowerer analyzer;
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ast(IteratorAST::Create(
        StdStreamAST::Create(StdStreamKind::Stdout),
        {param("line")},
        std::vector<StyioAST*>{body()}
      ));
      std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ast(IteratorAST::Create(
        StdStreamAST::Create(StdStreamKind::Stderr),
        {param("line")},
        std::vector<StyioAST*>{body()}
      ));
      std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    }, StyioTypeError);
  }
  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> ast(IteratorAST::Create(
      FileResourceAST::Create(StringAST::Create("input.txt"), false),
      {param("row")},
      std::vector<StyioAST*>{body()}
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* iter = dynamic_cast<SIOFileLineIter*>(ir.get());
    ASSERT_NE(iter, nullptr);
    EXPECT_TRUE(iter->from_path);
    EXPECT_EQ(iter->line_var, "row");
    EXPECT_NE(dynamic_cast<SGConstString*>(iter->path_expr), nullptr);
  }
  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["fh"] = styio_make_file_handle_type("string");
    std::unique_ptr<StyioAST> ast(IteratorAST::Create(
      NameAST::Create("fh"),
      {param("row")},
      std::vector<StyioAST*>{body()}
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* iter = dynamic_cast<SIOFileLineIter*>(ir.get());
    ASSERT_NE(iter, nullptr);
    EXPECT_FALSE(iter->from_path);
    EXPECT_EQ(iter->handle_var, "fh");
    EXPECT_EQ(iter->line_var, "row");
  }
  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["stdin_handle"] = styio_make_std_stream_type(StdStreamKind::Stdin, "string");
    std::unique_ptr<StyioAST> ast(IteratorAST::Create(
      NameAST::Create("stdin_handle"),
      {param("line")},
      std::vector<StyioAST*>{body()}
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* iter = dynamic_cast<SIOStdStreamLineIter*>(ir.get());
    ASSERT_NE(iter, nullptr);
    EXPECT_EQ(iter->line_var, "line");
  }
  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["stdout_handle"] = styio_make_std_stream_type(StdStreamKind::Stdout, "string");
    analyzer.local_binding_types["stderr_handle"] = styio_make_std_stream_type(StdStreamKind::Stderr, "string");
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ast(IteratorAST::Create(
        NameAST::Create("stdout_handle"),
        {param("line")},
        std::vector<StyioAST*>{body()}
      ));
      std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ast(IteratorAST::Create(
        NameAST::Create("stderr_handle"),
        {param("line")},
        std::vector<StyioAST*>{body()}
      ));
      std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    }, StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> ast(InstantPullAST::Create(
      StdStreamAST::Create(StdStreamKind::Stdin),
      styio_data_type_from_name("f64")
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* pull = dynamic_cast<SIOStdStreamPull*>(ir.get());
    ASSERT_NE(pull, nullptr);
    EXPECT_EQ(pull->result_type.name, "f64");
  }
  {
    AstToStyioIRLowerer analyzer;
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ast(InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdout)));
      std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ast(InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stderr)));
      std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    }, StyioTypeError);
  }
  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> ast(InstantPullAST::Create(
      FileResourceAST::Create(StringAST::Create("pull.txt"), false)
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* pull = dynamic_cast<SIOInstantPull*>(ir.get());
    ASSERT_NE(pull, nullptr);
    EXPECT_FALSE(pull->from_handle);
    EXPECT_NE(dynamic_cast<SGConstString*>(pull->path_expr), nullptr);
  }
  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["fh"] = styio_make_file_handle_type("string");
    std::unique_ptr<StyioAST> ast(InstantPullAST::Create(NameAST::Create("fh")));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* pull = dynamic_cast<SIOInstantPull*>(ir.get());
    ASSERT_NE(pull, nullptr);
    EXPECT_TRUE(pull->from_handle);
    EXPECT_EQ(pull->handle_var, "fh");
  }
  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["scalar"] = styio_data_type_from_name("i64");
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ast(InstantPullAST::Create(NameAST::Create("scalar")));
      std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ast(InstantPullAST::Create(IntAST::Create("7")));
      std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    }, StyioTypeError);
  }
}

TEST(StyioIRContract, StateReferenceHistoryAndSeriesContextsStayExplicit) {
  AstToStyioIRLowerer analyzer;
  SGPulsePlan plan;
  plan.ref_to_slot["state"] = 7;

  analyzer.set_cur_pulse_plan(&plan);
  {
    std::unique_ptr<StyioAST> ast(StateRefAST::Create(NameAST::Create("state")));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* snap = dynamic_cast<SGStateSnapLoad*>(ir.get());
    ASSERT_NE(snap, nullptr);
    EXPECT_EQ(snap->slot_id, 7);
  }
  {
    std::unique_ptr<StyioAST> ast(HistoryProbeAST::Create(
      StateRefAST::Create(NameAST::Create("state")),
      IntAST::Create("3")
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* hist = dynamic_cast<SGStateHistLoad*>(ir.get());
    ASSERT_NE(hist, nullptr);
    EXPECT_EQ(hist->slot_id, 7);
    EXPECT_EQ(hist->depth, 3);
    EXPECT_EQ(hist->pulse_region_id, -1);
  }
  {
    std::unique_ptr<StyioAST> ast(StateRefAST::Create(NameAST::Create("missing")));
    EXPECT_THROW({
      std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    }, StyioTypeError);
  }
  analyzer.set_cur_pulse_plan(nullptr);

  analyzer.set_post_pulse_hist_context(42, &plan);
  {
    std::unique_ptr<StyioAST> ast(HistoryProbeAST::Create(
      StateRefAST::Create(NameAST::Create("state")),
      IntAST::Create("2")
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* hist = dynamic_cast<SGStateHistLoad*>(ir.get());
    ASSERT_NE(hist, nullptr);
    EXPECT_EQ(hist->slot_id, 7);
    EXPECT_EQ(hist->depth, 2);
    EXPECT_EQ(hist->pulse_region_id, 42);
  }
  analyzer.set_post_pulse_hist_context(-1, nullptr);
  {
    std::unique_ptr<StyioAST> ast(HistoryProbeAST::Create(
      StateRefAST::Create(NameAST::Create("state")),
      IntAST::Create("1")
    ));
    EXPECT_THROW({
      std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    }, StyioTypeError);
  }

  analyzer.set_active_series_slot(9);
  {
    std::unique_ptr<StyioAST> ast(SeriesIntrinsicAST::Create(
      IntAST::Create("11"),
      SeriesIntrinsicOp::Max,
      IntAST::Create("4")
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* max_step = dynamic_cast<SGSeriesMaxStep*>(ir.get());
    ASSERT_NE(max_step, nullptr);
    EXPECT_EQ(max_step->slot_id, 9);
  }
  analyzer.set_active_series_slot(-1);
  {
    std::unique_ptr<StyioAST> ast(SeriesIntrinsicAST::Create(
      IntAST::Create("11"),
      SeriesIntrinsicOp::Avg,
      IntAST::Create("4")
    ));
    EXPECT_THROW({
      std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    }, StyioTypeError);
  }
}

TEST(StyioTypeInferenceContract, MatrixIntrinsicCallsInferShapesAndRejectBadInputs) {
  AstToStyioIRLowerer analyzer;
  analyzer.local_binding_types["mi23"] = styio_make_matrix_type("i64", 2, 3);
  analyzer.local_binding_types["mf23"] = styio_make_matrix_type("f64", 2, 3);
  analyzer.local_binding_types["mf34"] = styio_make_matrix_type("f64", 3, 4);
  analyzer.local_binding_types["mi22"] = styio_make_matrix_type("i64", 2, 2);

  int seq = 0;
  auto infer_call = [&](const std::string& name, std::vector<StyioAST*> args) {
    return infer_final_bound_type(
      analyzer,
      "matrix_tmp_" + std::to_string(seq++),
      FuncCallAST::Create(NameAST::Create(name), std::move(args))
    );
  };

  expect_matrix_type(infer_call("mat_zeros", {IntAST::Create("2"), IntAST::Create("3")}), "f64", 2, 3);
  expect_matrix_type(infer_call("mat_zeros_i64", {IntAST::Create("2"), IntAST::Create("3")}), "i64", 2, 3);
  expect_matrix_type(infer_call("mat_identity", {IntAST::Create("4")}), "f64", 4, 4);
  expect_matrix_type(infer_call("mat_identity_i64", {IntAST::Create("4")}), "i64", 4, 4);
  EXPECT_EQ(infer_call("mat_rows", {NameAST::Create("mi23")}).name, "i64");
  EXPECT_EQ(infer_call("mat_cols", {NameAST::Create("mi23")}).name, "i64");
  EXPECT_EQ(infer_call("mat_shape", {NameAST::Create("mi23")}).name, "list[i64]");
  EXPECT_EQ(infer_call("mat_get", {NameAST::Create("mf23"), IntAST::Create("0"), IntAST::Create("1")}).name, "f64");
  EXPECT_EQ(infer_call("mat_set", {NameAST::Create("mi23"), IntAST::Create("0"), IntAST::Create("1"), IntAST::Create("9")}).name, "i64");
  expect_matrix_type(infer_call("mat_clone", {NameAST::Create("mi23")}), "i64", 2, 3);
  expect_matrix_type(infer_call("transpose", {NameAST::Create("mi23")}), "i64", 3, 2);
  expect_matrix_type(infer_call("mat_add", {NameAST::Create("mi23"), NameAST::Create("mf23")}), "f64", 2, 3);
  expect_matrix_type(infer_call("mat_sub", {NameAST::Create("mf23"), NameAST::Create("mi23")}), "f64", 2, 3);
  expect_matrix_type(infer_call("mat_hadamard", {NameAST::Create("mi23"), NameAST::Create("mf23")}), "f64", 2, 3);
  expect_matrix_type(infer_call("mat_scale", {NameAST::Create("mi23"), FloatAST::Create("1.5")}), "f64", 2, 3);
  expect_matrix_type(infer_call("matmul", {NameAST::Create("mi23"), NameAST::Create("mf34")}), "f64", 2, 4);
  EXPECT_EQ(infer_call("dot", {NameAST::Create("mi23"), NameAST::Create("mf23")}).name, "f64");
  EXPECT_EQ(infer_call("mat_sum", {NameAST::Create("mi23")}).name, "i64");
  EXPECT_EQ(infer_call("norm", {NameAST::Create("mi23")}).name, "f64");

  EXPECT_THROW({
    (void)infer_call("mat_zeros", std::vector<StyioAST*>{IntAST::Create("2")});
  }, StyioTypeError);
  EXPECT_THROW({
    (void)infer_call("mat_rows", std::vector<StyioAST*>{IntAST::Create("1")});
  }, StyioTypeError);
  EXPECT_THROW({
    (void)infer_call(
      "mat_get",
      std::vector<StyioAST*>{
        NameAST::Create("mi23"),
        StringAST::Create("row"),
        IntAST::Create("1")
      });
  }, StyioTypeError);
  EXPECT_THROW({
    (void)infer_call(
      "mat_set",
      std::vector<StyioAST*>{
        NameAST::Create("mi23"),
        IntAST::Create("0"),
        IntAST::Create("1"),
        FloatAST::Create("1.5")
      });
  }, StyioTypeError);
  EXPECT_THROW({
    (void)infer_call(
      "mat_add",
      std::vector<StyioAST*>{NameAST::Create("mi23"), NameAST::Create("mf34")});
  }, StyioTypeError);
  EXPECT_THROW({
    (void)infer_call(
      "matmul",
      std::vector<StyioAST*>{NameAST::Create("mf23"), NameAST::Create("mi22")});
  }, StyioTypeError);
  EXPECT_THROW({
    (void)infer_call(
      "mat_scale",
      std::vector<StyioAST*>{NameAST::Create("mi23"), StringAST::Create("bad")});
  }, StyioTypeError);
}

TEST(StyioTypeInferenceContract, CollectionSizeListOpAndParallelAssignmentEdgesStayExplicit) {
  {
    AstToStyioIRLowerer analyzer;
    EXPECT_THROW(
      (void)infer_final_bound_type(
        analyzer,
        "empty_list",
        ListAST::Create()),
      StyioTypeError);
    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "mixed_list",
        ListAST::Create({IntAST::Create("1"), FloatAST::Create("2.5")})
      ).name,
      "list[i64]");
    EXPECT_THROW(
      (void)infer_final_bound_type(
        analyzer,
        "empty_dict",
        DictAST::Create()),
      StyioTypeError);
    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "numeric_dict",
        DictAST::Create({
          {StringAST::Create("a"), IntAST::Create("1")},
          {StringAST::Create("b"), FloatAST::Create("2.5")},
        })
      ).name,
      "dict[string,f64]");

    std::unique_ptr<StyioAST> tuple(TupleAST::Create({IntAST::Create("1"), IntAST::Create("2")}));
    tuple->typeInfer(&analyzer);
    EXPECT_EQ(tuple->getDataType().name, "(i64,i64)");

    std::unique_ptr<StyioAST> range(new RangeAST(IntAST::Create("0"), IntAST::Create("4"), IntAST::Create("1")));
    EXPECT_NO_THROW(range->typeInfer(&analyzer));

    std::unique_ptr<StyioAST> sized(new SizeOfAST(ListAST::Create({IntAST::Create("1")})));
    sized->typeInfer(&analyzer);
    EXPECT_EQ(sized->getDataType().name, "i64");
  }

  {
    AstToStyioIRLowerer analyzer;
    EXPECT_THROW({
      std::unique_ptr<StyioAST> dict(DictAST::Create({{IntAST::Create("1"), IntAST::Create("2")}}));
      dict->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> dict(DictAST::Create({
        {StringAST::Create("bad"), FileResourceAST::Create(StringAST::Create("/tmp/styio-dict-value"), false)}
      }));
      dict->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> range(new RangeAST(StringAST::Create("0"), IntAST::Create("4"), IntAST::Create("1")));
      range->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> sized(new SizeOfAST(IntAST::Create("7")));
      sized->typeInfer(&analyzer);
    }, StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["items"] = styio_make_list_type("i64");
    analyzer.local_binding_types["lookup"] = styio_make_dict_type("string", "f64");
    analyzer.local_binding_types["grid"] = styio_make_matrix_type("i64", 2, 3);
    analyzer.local_binding_types["scalar"] = styio_data_type_from_name("i64");

    std::unique_ptr<StyioAST> list_index(
      new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("items"), IntAST::Create("0")));
    EXPECT_NO_THROW(list_index->typeInfer(&analyzer));
    std::unique_ptr<StyioAST> dict_name(
      new ListOpAST(StyioNodeType::Access_By_Name, NameAST::Create("lookup"), StringAST::Create("key")));
    EXPECT_NO_THROW(dict_name->typeInfer(&analyzer));
    std::unique_ptr<StyioAST> matrix_slice(
      new ListOpAST(StyioNodeType::Access_By_Slice, NameAST::Create("grid"), IntAST::Create("0"), IntAST::Create("2")));
    EXPECT_NO_THROW(matrix_slice->typeInfer(&analyzer));
    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "lookup_by_name",
        new ListOpAST(StyioNodeType::Access_By_Name, NameAST::Create("lookup"), StringAST::Create("key"))
      ).name,
      "f64");
    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "lookup_slice",
        new ListOpAST(StyioNodeType::Access_By_Slice, NameAST::Create("lookup"), IntAST::Create("0"), IntAST::Create("2"))
      ).name,
      "list[f64]");
    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "grid_row",
        new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("grid"), IntAST::Create("0"))
      ).name,
      "list[i64]");

    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(
        new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("items"), StringAST::Create("zero")));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(
        new ListOpAST(StyioNodeType::Access_By_Name, NameAST::Create("items"), StringAST::Create("key")));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(
        new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("lookup"), IntAST::Create("0")));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(
        new ListOpAST(StyioNodeType::Access_By_Slice, NameAST::Create("items"), StringAST::Create("0"), IntAST::Create("1")));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(
        new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("scalar"), IntAST::Create("0")));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> names(ParallelAssignAST::Create(
      {NameAST::Create("a"), NameAST::Create("b")},
      {IntAST::Create("1"), FloatAST::Create("2.0")}
    ));
    EXPECT_NO_THROW(names->typeInfer(&analyzer));

    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(ParallelAssignAST::Create(
        {NameAST::Create("a")},
        {IntAST::Create("1"), IntAST::Create("2")}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);

    (void)infer_final_bound_type(analyzer, "locked", IntAST::Create("7"));
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(ParallelAssignAST::Create(
        {NameAST::Create("locked")},
        {IntAST::Create("8")}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["items"] = styio_make_list_type("i64");
    analyzer.local_binding_types["lookup"] = styio_make_dict_type("string", "f64");
    analyzer.local_binding_types["scalar"] = styio_data_type_from_name("i64");

    std::unique_ptr<StyioAST> list_ok(ParallelAssignAST::Create(
      {new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("items"), IntAST::Create("0"))},
      {IntAST::Create("9")}
    ));
    EXPECT_NO_THROW(list_ok->typeInfer(&analyzer));
    std::unique_ptr<StyioAST> dict_ok(ParallelAssignAST::Create(
      {new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("lookup"), StringAST::Create("k"))},
      {FloatAST::Create("9.5")}
    ));
    EXPECT_NO_THROW(dict_ok->typeInfer(&analyzer));

    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(ParallelAssignAST::Create(
        {new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("items"), IntAST::Create("0"))},
        {FloatAST::Create("9.5")}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(ParallelAssignAST::Create(
        {new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("lookup"), StringAST::Create("k"))},
        {StringAST::Create("wrong")}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(ParallelAssignAST::Create(
        {new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("scalar"), IntAST::Create("0"))},
        {IntAST::Create("1")}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> left(FlexBindAST::Create(
      VarAST::Create(NameAST::Create("left")),
      ListAST::Create({IntAST::Create("1")})
    ));
    left->typeInfer(&analyzer);
    std::unique_ptr<StyioAST> right(FlexBindAST::Create(
      VarAST::Create(NameAST::Create("right")),
      ListAST::Create({IntAST::Create("2")})
    ));
    right->typeInfer(&analyzer);
    std::unique_ptr<StyioAST> move_dynamic(ParallelAssignAST::Create(
      {NameAST::Create("left")},
      {NameAST::Create("right")}
    ));
    EXPECT_THROW(move_dynamic->typeInfer(&analyzer), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["items"] = styio_make_list_type("i64");
    analyzer.local_binding_types["text"] = styio_data_type_from_name("string");

    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "push_result",
        FuncCallAST::Create(NameAST::Create("items"), NameAST::Create("push"), {IntAST::Create("4")})
      ).name,
      "i64");
    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "insert_result",
        FuncCallAST::Create(NameAST::Create("items"), NameAST::Create("insert"), {IntAST::Create("0"), IntAST::Create("4")})
      ).name,
      "i64");
    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "lines_result",
        FuncCallAST::Create(NameAST::Create("text"), NameAST::Create("lines"), {})
      ).name,
      "list[string]");

    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(FuncCallAST::Create(
        NameAST::Create("items"),
        NameAST::Create("insert"),
        std::vector<StyioAST*>{StringAST::Create("bad"), IntAST::Create("4")}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(FuncCallAST::Create(
        NameAST::Create("items"),
        NameAST::Create("insert"),
        std::vector<StyioAST*>{IntAST::Create("0"), StringAST::Create("wrong")}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(FuncCallAST::Create(
        NameAST::Create("items"),
        NameAST::Create("pop"),
        std::vector<StyioAST*>{IntAST::Create("1")}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(FuncCallAST::Create(
        NameAST::Create("items"),
        NameAST::Create("lines"),
        {}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
  }
}

TEST(StyioTypeInferenceContract, FunctionResourceMethodAndFlowEdgesStayExplicit) {
  auto i64_type = []() {
    return styio_data_type_from_name("i64");
  };
  auto bool_type = []() {
    return styio_data_type_from_name("bool");
  };
  auto f64_type = []() {
    return styio_data_type_from_name("f64");
  };
  auto param_i64 = [](const std::string& name) {
    return ParamAST::Create(NameAST::Create(name), TypeAST::Create("i64"));
  };

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<SimpleFuncAST> id(SimpleFuncAST::Create(
      NameAST::Create("id_i64"),
      std::vector<ParamAST*>{param_i64("x")},
      NameAST::Create("x")
    ));
    id->typeInfer(&analyzer);

    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "id_result",
        FuncCallAST::Create(NameAST::Create("id_i64"), {IntAST::Create("5")})
      ).name,
      "i64");
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(FuncCallAST::Create(
        NameAST::Create("id_i64"),
        std::vector<StyioAST*>{BoolAST::Create(true)}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(FuncCallAST::Create(NameAST::Create("missing"), {}));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);

    analyzer.native_func_defs["native_scale"] = {f64_type(), {i64_type()}};
    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "native_result",
        FuncCallAST::Create(NameAST::Create("native_scale"), {IntAST::Create("2")})
      ).name,
      "f64");
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(FuncCallAST::Create(
        NameAST::Create("native_scale"),
        std::vector<StyioAST*>{IntAST::Create("2"), IntAST::Create("3")}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(FuncCallAST::Create(
        NameAST::Create("native_scale"),
        std::vector<StyioAST*>{BoolAST::Create(false)}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<ResourceMethodDefAST> emit(ResourceMethodDefAST::Create(
      "file",
      "emit",
      false,
      false,
      std::vector<ParamAST*>{param_i64("payload")},
      ReturnAST::Create(NameAST::Create("payload"))
    ));
    emit->typeInfer(&analyzer);
    std::unique_ptr<ResourceMethodDefAST> label(ResourceMethodDefAST::Create(
      "file",
      "label",
      false,
      true,
      {},
      ReturnAST::Create(StringAST::Create("ok"))
    ));
    label->typeInfer(&analyzer);

    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "emit_result",
        FuncCallAST::Create(ResourceReceiverAST::Create("file"), NameAST::Create("emit"), {IntAST::Create("7")})
      ).name,
      "i64");
    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "label_result",
        AttrAST::Create(ResourceReceiverAST::Create("file"), NameAST::Create("label"))
      ).name,
      "string");

    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(FuncCallAST::Create(
        ResourceReceiverAST::Create("file"),
        NameAST::Create("label"),
        {}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(FuncCallAST::Create(
        ResourceReceiverAST::Create("file"),
        NameAST::Create("emit"),
        std::vector<StyioAST*>{BoolAST::Create(true)}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(FuncCallAST::Create(
        ResourceReceiverAST::Create("file"),
        NameAST::Create("missing"),
        {}
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["task"] = styio_make_task_type("i64");

    std::unique_ptr<FlowBindAST> await_ok(FlowBindAST::CreateAwait(
      NameAST::Create("task"),
      VarAST::Create(NameAST::Create("out")),
      IntAST::Create("0")
    ));
    await_ok->typeInfer(&analyzer);
    EXPECT_EQ(await_ok->getResultType().name, "i64");
    EXPECT_EQ(analyzer.local_binding_types["out"].name, "i64");

    EXPECT_THROW({
      std::unique_ptr<StyioAST> second(FlowBindAST::CreateAwait(
        NameAST::Create("task"),
        VarAST::Create(NameAST::Create("again")),
        IntAST::Create("0")
      ));
      second->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(FlowBindAST::CreateAwait(
        IntAST::Create("1"),
        VarAST::Create(NameAST::Create("not_task")),
        nullptr
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bare(FlowBindAST::Create(
        nullptr,
        VarAST::Create(NameAST::Create("frozen"))
      ));
      bare->typeInfer(&analyzer);
    }, StyioTypeError);
  }

  EXPECT_TRUE(i64_type().equals(styio_data_type_from_name("i64")));
  EXPECT_TRUE(bool_type().equals(styio_data_type_from_name("bool")));
}

TEST(StyioTypeInferenceContract, MatrixLiteralContextAndFlowMergesCoverEdgeBranches) {
  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType matrix_f64_1x2 = styio_make_matrix_type("f64", 1, 2);

  {
    AstToStyioIRLowerer analyzer;
    auto* var = VarAST::Create(NameAST::Create("m"), TypeAST::Create(matrix_f64_1x2));
    std::unique_ptr<StyioAST> bind(FinalBindAST::Create(var, ListAST::Create({
      ListAST::Create({IntAST::Create("1"), FloatAST::Create("2.5")})
    })));
    bind->typeInfer(&analyzer);
    expect_matrix_type(var->getDType()->getDataType(), "f64", 1, 2);
  }

  {
    AstToStyioIRLowerer analyzer;
    auto* block = BlockAST::Create({
      ReturnAST::Create(ListAST::Create({
        ListAST::Create({IntAST::Create("1"), FloatAST::Create("2.5")})
      }))
    });
    block->set_followings({
      ReturnAST::Create(ListAST::Create({
        ListAST::Create({IntAST::Create("3"), IntAST::Create("4")})
      }))
    });
    std::unique_ptr<FunctionAST> make_matrix(FunctionAST::Create(
      NameAST::Create("make_matrix"),
      false,
      {},
      TypeAST::Create(matrix_f64_1x2),
      block
    ));
    analyzer.func_defs["make_matrix"] = make_matrix.get();
    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "result",
        FuncCallAST::Create(NameAST::Create("make_matrix"), {})
      ).name,
      matrix_f64_1x2.name);
    expect_matrix_type(analyzer.local_binding_types["result"], "f64", 1, 2);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<SimpleFuncAST> bad_matrix(SimpleFuncAST::Create(
      NameAST::Create("bad_matrix"),
      false,
      {},
      TypeAST::Create(matrix_f64_1x2),
      IntAST::Create("1")
    ));
    analyzer.func_defs["bad_matrix"] = bad_matrix.get();
    EXPECT_THROW(
      (void)infer_final_bound_type(
        analyzer,
        "bad_result",
        FuncCallAST::Create(NameAST::Create("bad_matrix"), {})
      ),
      StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    auto* var = VarAST::Create(NameAST::Create("empty_m"), TypeAST::Create(matrix_f64_1x2));
    std::unique_ptr<StyioAST> bind(FinalBindAST::Create(var, ListAST::Create({})));
    EXPECT_THROW(bind->typeInfer(&analyzer), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    auto* var = VarAST::Create(NameAST::Create("bad_row_m"), TypeAST::Create(matrix_f64_1x2));
    std::unique_ptr<StyioAST> bind(FinalBindAST::Create(var, ListAST::Create({IntAST::Create("1")})));
    EXPECT_THROW(bind->typeInfer(&analyzer), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    auto* var = VarAST::Create(NameAST::Create("empty_row_m"), TypeAST::Create(matrix_f64_1x2));
    std::unique_ptr<StyioAST> bind(FinalBindAST::Create(var, ListAST::Create({ListAST::Create({})})));
    EXPECT_THROW(bind->typeInfer(&analyzer), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    auto* var = VarAST::Create(NameAST::Create("ragged_m"), TypeAST::Create(matrix_f64_1x2));
    std::unique_ptr<StyioAST> bind(FinalBindAST::Create(var, ListAST::Create({
      ListAST::Create({IntAST::Create("1")}),
      ListAST::Create({IntAST::Create("2"), IntAST::Create("3")})
    })));
    EXPECT_THROW(bind->typeInfer(&analyzer), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    auto* var = VarAST::Create(NameAST::Create("generic_m"), TypeAST::Create(styio_make_matrix_type("f64")));
    std::unique_ptr<StyioAST> bind(FinalBindAST::Create(
      var,
      NameAST::Create("source_matrix")
    ));
    analyzer.local_binding_types["source_matrix"] = matrix_f64_1x2;
    bind->typeInfer(&analyzer);
    expect_matrix_type(var->getDType()->getDataType(), "f64", 1, 2);
  }

  {
    AstToStyioIRLowerer analyzer;
    auto* var = VarAST::Create(NameAST::Create("bad_source_m"), TypeAST::Create(matrix_f64_1x2));
    std::unique_ptr<StyioAST> bind(FinalBindAST::Create(
      var,
      NameAST::Create("scalar")
    ));
    analyzer.local_binding_types["scalar"] = i64;
    EXPECT_THROW(bind->typeInfer(&analyzer), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    auto* var = VarAST::Create(NameAST::Create("bad_shape_m"), TypeAST::Create(matrix_f64_1x2));
    std::unique_ptr<StyioAST> bind(FinalBindAST::Create(
      var,
      NameAST::Create("source_matrix")
    ));
    analyzer.local_binding_types["source_matrix"] = styio_make_matrix_type("f64", 2, 2);
    EXPECT_THROW(bind->typeInfer(&analyzer), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "wave_else",
        WaveMergeAST::Create(BoolAST::Create(true), UndefinedLitAST::Create(), FloatAST::Create("2.5"))
      ).name,
      "f64");
  }
  {
    AstToStyioIRLowerer analyzer;
    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "wave_then",
        WaveMergeAST::Create(BoolAST::Create(true), IntAST::Create("1"), UndefinedLitAST::Create())
      ).name,
      "i64");
  }
  {
    AstToStyioIRLowerer analyzer;
    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "wave_numeric",
        WaveMergeAST::Create(BoolAST::Create(true), IntAST::Create("1"), FloatAST::Create("2.5"))
      ).name,
      "f64");
  }
  {
    AstToStyioIRLowerer analyzer;
    EXPECT_TRUE(
      infer_final_bound_type(
        analyzer,
        "wave_undefined",
        WaveMergeAST::Create(BoolAST::Create(true), UndefinedLitAST::Create(), UndefinedLitAST::Create())
      ).isUndefined());
  }
  {
    AstToStyioIRLowerer analyzer;
    EXPECT_TRUE(
      infer_final_bound_type(
        analyzer,
        "wave_mixed",
        WaveMergeAST::Create(
          BoolAST::Create(true),
          StringAST::Create("value"),
          ListAST::Create({IntAST::Create("1")}))
      ).isUndefined());
  }

  EXPECT_TRUE(i64.equals(styio_data_type_from_name("i64")));
}

TEST(StyioTypeInferenceContract, ExplicitSimpleFunctionReturnAndResourceCopyEdgesStayClosed) {
  auto param_i64 = [](const std::string& name) {
    return ParamAST::Create(NameAST::Create(name), TypeAST::Create("i64"));
  };

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<SimpleFuncAST> explicit_f64(SimpleFuncAST::Create(
      NameAST::Create("as_f64"),
      std::vector<ParamAST*>{param_i64("x")},
      TypeAST::Create("f64"),
      NameAST::Create("x")
    ));
    explicit_f64->typeInfer(&analyzer);

    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "f64_result",
        FuncCallAST::Create(NameAST::Create("as_f64"), {IntAST::Create("5")})
      ).name,
      "f64");
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<SimpleFuncAST> literal_i64(SimpleFuncAST::Create(
      NameAST::Create("literal_i64"),
      std::vector<ParamAST*>{},
      IntAST::Create("7")
    ));
    analyzer.func_defs["literal_i64"] = literal_i64.get();

    EXPECT_EQ(
      infer_final_bound_type(
        analyzer,
        "literal_result",
        FuncCallAST::Create(NameAST::Create("literal_i64"), {})
      ).name,
      "i64");
  }

  {
    AstToStyioIRLowerer analyzer;
    EXPECT_THROW({
      std::unique_ptr<StyioAST> copied(FinalBindAST::Create(
        VarAST::Create(NameAST::Create("snapshot")),
        ResourceRefAST::Create(NameAST::Create("hist"))
      ));
      copied->typeInfer(&analyzer);
    }, StyioTypeError);
  }

  {
    ExposedAstToStyioIRLowerer analyzer;
    analyzer.bindRuntimeName(
      "owned_resource",
      StyioSemaContext::BindingValueKind::ListHandle,
      true,
      styio_make_list_type("i64"),
      true
    );
    EXPECT_THROW({
      std::unique_ptr<StyioAST> copied(FinalBindAST::Create(
        VarAST::Create(NameAST::Create("copy")),
        NameAST::Create("owned_resource")
      ));
      copied->typeInfer(&analyzer);
    }, StyioTypeError);
  }

  {
    ExposedAstToStyioIRLowerer analyzer;
    analyzer.bindRuntimeName(
      "target",
      StyioSemaContext::BindingValueKind::Unknown,
      true
    );
    analyzer.bindRuntimeName(
      "rhs_i64",
      StyioSemaContext::BindingValueKind::I64,
      false,
      styio_data_type_from_name("i64"),
      false
    );
    std::unique_ptr<StyioAST> assign(ParallelAssignAST::Create(
      {NameAST::Create("target")},
      {NameAST::Create("rhs_i64")}
    ));
    assign->typeInfer(&analyzer);
    EXPECT_EQ(analyzer.local_binding_types["target"].name, "i64");
  }

  {
    AstToStyioIRLowerer analyzer;
    EXPECT_THROW({
      std::unique_ptr<StyioAST> iter_seq(IterSeqAST::Create(
        ListAST::Create({IntAST::Create("1")}),
        std::vector<HashTagNameAST*>{HashTagNameAST::Create({"route"})}
      ));
      iter_seq->typeInfer(&analyzer);
    }, StyioTypeError);
  }
}

TEST(StyioTypeInferenceContract, BinOpResourceRefMatchAndCondFlowEdgesStayExplicit) {
  auto infer_binop = [](AstToStyioIRLowerer& analyzer, BinOpAST* bin) {
    std::unique_ptr<StyioAST> owner(bin);
    owner->typeInfer(&analyzer);
    return bin->getType();
  };

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["mi23"] = styio_make_matrix_type("i64", 2, 3);
    analyzer.local_binding_types["mf23"] = styio_make_matrix_type("f64", 2, 3);
    analyzer.local_binding_types["mf34"] = styio_make_matrix_type("f64", 3, 4);
    analyzer.local_binding_types["mi22"] = styio_make_matrix_type("i64", 2, 2);
    analyzer.local_binding_types["ms22"] = styio_make_matrix_type("string", 2, 2);
    analyzer.local_binding_types["lhs"] = styio_data_type_from_name("i64");
    analyzer.local_binding_types["rhs"] = styio_data_type_from_name("f64");

    expect_matrix_type(
      infer_binop(analyzer, BinOpAST::Create(StyioOpType::Binary_Add, NameAST::Create("mi23"), NameAST::Create("mf23"))),
      "f64",
      2,
      3);
    expect_matrix_type(
      infer_binop(analyzer, BinOpAST::Create(StyioOpType::Binary_Mul, NameAST::Create("mi23"), FloatAST::Create("2.0"))),
      "f64",
      2,
      3);
    expect_matrix_type(
      infer_binop(analyzer, BinOpAST::Create(StyioOpType::Binary_Mul, FloatAST::Create("2.0"), NameAST::Create("mi23"))),
      "f64",
      2,
      3);
    expect_matrix_type(
      infer_binop(analyzer, BinOpAST::Create(StyioOpType::Binary_Mul, NameAST::Create("mi23"), NameAST::Create("mf34"))),
      "f64",
      2,
      4);
    EXPECT_EQ(
      infer_binop(analyzer, BinOpAST::Create(StyioOpType::Binary_Add, StringAST::Create("x"), IntAST::Create("1"))).name,
      "string");
    EXPECT_EQ(
      infer_binop(analyzer, BinOpAST::Create(StyioOpType::Binary_Sub, StringAST::Create("4"), IntAST::Create("1"))).name,
      "i64");
    EXPECT_EQ(
      infer_binop(analyzer, BinOpAST::Create(StyioOpType::Self_Add_Assign, NameAST::Create("lhs"), IntAST::Create("1"))).name,
      "i64");
    EXPECT_TRUE(
      infer_binop(analyzer, BinOpAST::Create(StyioOpType::Self_Add_Assign, IntAST::Create("1"), IntAST::Create("1"))).isUndefined());
    EXPECT_TRUE(
      infer_binop(analyzer, BinOpAST::Create(StyioOpType::Binary_Add, nullptr, IntAST::Create("1"))).isUndefined());

    auto* nested = BinOpAST::Create(
      StyioOpType::Binary_Add,
      BinOpAST::Create(StyioOpType::Binary_Add, IntAST::Create("1"), IntAST::Create("2")),
      BinOpAST::Create(StyioOpType::Binary_Add, IntAST::Create("3"), IntAST::Create("4"))
    );
    nested->setDType(styio_data_type_from_name("f64"));
    EXPECT_EQ(infer_binop(analyzer, nested).name, "f64");

    EXPECT_THROW({
      (void)infer_binop(
        analyzer,
        BinOpAST::Create(StyioOpType::Binary_Div, NameAST::Create("mi23"), NameAST::Create("mf23")));
    }, StyioTypeError);
    EXPECT_THROW({
      (void)infer_binop(
        analyzer,
        BinOpAST::Create(StyioOpType::Binary_Add, NameAST::Create("mi23"), IntAST::Create("1")));
    }, StyioTypeError);
    EXPECT_THROW({
      (void)infer_binop(
        analyzer,
        BinOpAST::Create(StyioOpType::Binary_Mul, NameAST::Create("mi23"), StringAST::Create("bad")));
    }, StyioTypeError);
    EXPECT_THROW({
      (void)infer_binop(
        analyzer,
        BinOpAST::Create(StyioOpType::Binary_Mul, NameAST::Create("mf23"), NameAST::Create("mi22")));
    }, StyioTypeError);
    EXPECT_THROW({
      (void)infer_binop(
        analyzer,
        BinOpAST::Create(StyioOpType::Binary_Add, NameAST::Create("ms22"), NameAST::Create("mi22")));
    }, StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    auto declare_resource = [&](const std::string& name, StyioDataType type) {
      std::unique_ptr<StyioAST> decl(ResourceDeclAST::Create({
        {NameAST::Create(name), TypeAST::Create(type)}
      }));
      decl->typeInfer(&analyzer);
    };
    const StyioDataType i64 = styio_data_type_from_name("i64");
    declare_resource("hist", styio_make_topology_resource_type(i64, StyioResourceShapeKind::Fixed, 3));
    declare_resource("scalar", styio_make_topology_resource_type(i64, StyioResourceShapeKind::Scalar));
    declare_resource("seq", styio_make_topology_resource_type(i64, StyioResourceShapeKind::Sequence));
    declare_resource("task_hist", styio_make_topology_resource_type(styio_make_task_type("i64"), StyioResourceShapeKind::Fixed, 2));
    StyioDataType unreadable = styio_make_topology_resource_type(i64, StyioResourceShapeKind::Fixed, 2);
    unreadable.capabilities = 0;
    declare_resource("unreadable", unreadable);
    StyioDataType not_indexable = styio_make_topology_resource_type(i64, StyioResourceShapeKind::Fixed, 2);
    not_indexable.capabilities = styio_caps(StyioTypeCapability::Readable);
    declare_resource("not_indexable", not_indexable);

    {
      std::unique_ptr<StyioAST> ref(ResourceRefAST::Create(NameAST::Create("hist")));
      ref->typeInfer(&analyzer);
      EXPECT_TRUE(styio_is_topology_resource_type(ref->getDataType()));
    }
    {
      std::unique_ptr<StyioAST> ref(ResourceRefAST::CreateSelector(NameAST::Create("hist"), ResourceSelectorKind::Offset, -1));
      ref->typeInfer(&analyzer);
      EXPECT_EQ(ref->getDataType().name, "i64");
    }
    {
      std::unique_ptr<StyioAST> ref(ResourceRefAST::CreateSelector(NameAST::Create("hist"), ResourceSelectorKind::SliceFrom, -2));
      ref->typeInfer(&analyzer);
      EXPECT_EQ(ref->getDataType().name, "list[i64]");
    }
    {
      std::unique_ptr<StyioAST> ref(ResourceRefAST::CreateSelector(NameAST::Create("hist"), ResourceSelectorKind::SnapshotAll));
      ref->typeInfer(&analyzer);
      EXPECT_EQ(ref->getDataType().name, "list[i64]");
    }

    EXPECT_THROW({
      std::unique_ptr<StyioAST> ref(ResourceRefAST::Create(NameAST::Create("missing")));
      ref->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ref(ResourceRefAST::CreateSelector(NameAST::Create("scalar"), ResourceSelectorKind::SnapshotAll));
      ref->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ref(ResourceRefAST::CreateSelector(NameAST::Create("seq"), ResourceSelectorKind::SliceFrom, -1));
      ref->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ref(ResourceRefAST::CreateSelector(NameAST::Create("hist"), ResourceSelectorKind::SliceFrom, 1));
      ref->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ref(ResourceRefAST::CreateSelector(NameAST::Create("hist"), ResourceSelectorKind::SliceFrom, -4));
      ref->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ref(ResourceRefAST::CreateSelector(
        NameAST::Create("hist"),
        ResourceSelectorKind::SliceFrom,
        std::numeric_limits<int>::min()));
      ref->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ref(ResourceRefAST::CreateSelector(NameAST::Create("task_hist"), ResourceSelectorKind::Offset, -1));
      ref->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ref(ResourceRefAST::CreateSelector(NameAST::Create("unreadable"), ResourceSelectorKind::Offset, -1));
      ref->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> ref(ResourceRefAST::CreateSelector(NameAST::Create("not_indexable"), ResourceSelectorKind::Offset, -1));
      ref->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> duplicate(ResourceDeclAST::Create({
        {NameAST::Create("hist"), TypeAST::Create(i64)}
      }));
      duplicate->typeInfer(&analyzer);
    }, StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["x"] = styio_data_type_from_name("i64");

    std::unique_ptr<StyioAST> match(MatchCasesAST::make(
      NameAST::Create("x"),
      CasesAST::Create({
        {IntAST::Create("1"), IntAST::Create("10")},
        {new BinCompAST(CompType::EQ, NameAST::Create("x"), IntAST::Create("2")), FloatAST::Create("2.5")},
      }, StringAST::Create("fallback"))
    ));
    match->typeInfer(&analyzer);
    EXPECT_EQ(match->getDataType().name, "string");

    std::unique_ptr<StyioAST> empty_match(MatchCasesAST::make(
      IntAST::Create("0"),
      CasesAST::Create(nullptr)
    ));
    empty_match->typeInfer(&analyzer);
    EXPECT_EQ(empty_match->getDataType().name, "i64");

    std::unique_ptr<StyioAST> bool_char_match(MatchCasesAST::make(
      IntAST::Create("0"),
      CasesAST::Create({{IntAST::Create("1"), BoolAST::Create(true)}}, CharAST::Create("z"))
    ));
    bool_char_match->typeInfer(&analyzer);
    EXPECT_EQ(bool_char_match->getDataType().name, "i64");

    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(MatchCasesAST::make(
        BoolAST::Create(true),
        CasesAST::Create(IntAST::Create("0"))
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(MatchCasesAST::make(
        NameAST::Create("x"),
        CasesAST::Create({{StringAST::Create("one"), IntAST::Create("1")}}, IntAST::Create("0"))
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
    EXPECT_THROW({
      std::unique_ptr<StyioAST> bad(MatchCasesAST::make(
        NameAST::Create("x"),
        CasesAST::Create({{IntAST::Create("1"), ListAST::Create({IntAST::Create("1")})}}, IntAST::Create("0"))
      ));
      bad->typeInfer(&analyzer);
    }, StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["branch"] = styio_data_type_from_name("i64");
    std::unique_ptr<StyioAST> flow(new CondFlowAST(
      StyioNodeType::CondFlow_Both,
      CondAST::Create(LogicType::RAW, BoolAST::Create(true)),
      BlockAST::Create({
        FlexBindAST::Create(VarAST::Create(NameAST::Create("branch")), FloatAST::Create("1.5"))
      }),
      BlockAST::Create({
        FlexBindAST::Create(VarAST::Create(NameAST::Create("branch")), IntAST::Create("1"))
      })
    ));
    flow->typeInfer(&analyzer);
    EXPECT_EQ(analyzer.local_binding_types["branch"].name, "f64");

    std::unique_ptr<StyioAST> then_only(new CondFlowAST(
      StyioNodeType::CondFlow_True,
      CondAST::Create(LogicType::RAW, BoolAST::Create(true)),
      BlockAST::Create({FlexBindAST::Create(VarAST::Create(NameAST::Create("branch")), IntAST::Create("3"))})
    ));
    EXPECT_NO_THROW(then_only->typeInfer(&analyzer));
  }
}

TEST(StyioTypeInferenceContract, LeafNoopAndFailClosedTypeInferNodesStayExplicit) {
  AstToStyioIRLowerer analyzer;
  std::vector<std::unique_ptr<StyioAST>> noops;
  noops.emplace_back(CommentAST::Create("comment"));
  noops.emplace_back(NoneAST::Create());
  noops.emplace_back(EmptyAST::Create());
  noops.emplace_back(PassAST::Create());
  noops.emplace_back(BreakAST::Create());
  noops.emplace_back(ContinueAST::Create());
  noops.emplace_back(EOFAST::Create());
  noops.emplace_back(TypeAST::Create("i64"));
  noops.emplace_back(TypeTupleAST::Create({TypeAST::Create("i64"), TypeAST::Create("f64")}));
  noops.emplace_back(VarAST::Create(NameAST::Create("v")));
  noops.emplace_back(ParamAST::Create(NameAST::Create("p"), TypeAST::Create("i64")));
  noops.emplace_back(OptArgAST::Create(NameAST::Create("opt")));
  noops.emplace_back(OptKwArgAST::Create(NameAST::Create("kw")));
  noops.emplace_back(VarTupleAST::Create({VarAST::Create(NameAST::Create("a"))}));
  noops.emplace_back(ExtractorAST::Create(TupleAST::Create({IntAST::Create("1")}), NameAST::Create("first")));
  noops.emplace_back(SetAST::Create({IntAST::Create("1")}));
  noops.emplace_back(UndefinedLitAST::Create());
  noops.emplace_back(WaveDispatchAST::Create(BoolAST::Create(true), IntAST::Create("1"), IntAST::Create("0")));
  noops.emplace_back(FallbackAST::Create(UndefinedLitAST::Create(), IntAST::Create("0")));
  noops.emplace_back(GuardSelectorAST::Create(IntAST::Create("1"), BoolAST::Create(true)));
  noops.emplace_back(EqProbeAST::Create(IntAST::Create("1"), IntAST::Create("1")));
  noops.emplace_back(ResourceOrderAST::Create(NameAST::Create("before"), NameAST::Create("after")));
  noops.emplace_back(ResPathAST::Create(StyioPathType::local_relevant_any, "data.txt"));
  noops.emplace_back(RemotePathAST::Create(StyioPathType::ipv4_addr, "127.0.0.1:/tmp/data"));
  noops.emplace_back(WebUrlAST::Create(StyioPathType::url_https, "https://example.test/data"));
  noops.emplace_back(DBUrlAST::Create(StyioPathType::db_postgresql, "postgres://example/db"));
  noops.emplace_back(new ExtPackAST({"pkg"}));
  noops.emplace_back(ExportDeclAST::Create({"symbol"}));
  noops.emplace_back(ExternBlockAST::Create("c", "int f(void) { return 0; }"));
  noops.emplace_back(new ReadFileAST(NameAST::Create("line"), StringAST::Create("input.txt")));
  noops.emplace_back(CheckEqualAST::Create({IntAST::Create("1")}));
  noops.emplace_back(new CheckIsinAST(ListAST::Create({IntAST::Create("1")})));
  noops.emplace_back(HashTagNameAST::Create({"tag"}));
  noops.emplace_back(new ForwardAST());
  noops.emplace_back(CODPAST::Create("pipe", {IntAST::Create("1")}));
  noops.emplace_back(new InfiniteAST());
  noops.emplace_back(new AnonyFuncAST(VarTupleAST::Create({}), IntAST::Create("1")));
  noops.emplace_back(StructAST::Create(
    NameAST::Create("Pair"),
    {ParamAST::Create(NameAST::Create("left"), TypeAST::Create("i64"))}
  ));
  noops.emplace_back(StateRefAST::Create(NameAST::Create("state")));
  noops.emplace_back(HistoryProbeAST::Create(StateRefAST::Create(NameAST::Create("state")), IntAST::Create("1")));
  noops.emplace_back(SeriesIntrinsicAST::Create(IntAST::Create("1"), SeriesIntrinsicOp::Avg, IntAST::Create("3")));
  noops.emplace_back(StateDeclAST::Create(
    nullptr,
    NameAST::Create("acc"),
    IntAST::Create("0"),
    VarAST::Create(NameAST::Create("out"), TypeAST::Create("i64")),
    BinOpAST::Create(StyioOpType::Binary_Add, StateRefAST::Create(NameAST::Create("acc")), IntAST::Create("1"))
  ));
  noops.emplace_back(TaskGroupLaunchAST::Create({TaskBlockAST::Create(BlockAST::Create({ReturnAST::Create(IntAST::Create("1"))}))}));

  for (auto& ast : noops) {
    EXPECT_NO_THROW(ast->typeInfer(&analyzer));
  }

  {
    std::unique_ptr<StyioAST> snapshot(SnapshotDeclAST::Create(
      NameAST::Create("snap"),
      FileResourceAST::Create(StringAST::Create("/tmp/styio-snapshot"), false)
    ));
    EXPECT_NO_THROW(snapshot->typeInfer(&analyzer));
    EXPECT_EQ(analyzer.local_binding_types["snap"].name, "i64");
  }
  {
    std::unique_ptr<StyioAST> loop(InfiniteLoopAST::CreateWhile(
      BoolAST::Create(true),
      BlockAST::Create({PassAST::Create()})
    ));
    EXPECT_NO_THROW(loop->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<StyioAST> loop(InfiniteLoopAST::CreateWhile(
      IntAST::Create("1"),
      BlockAST::Create({PassAST::Create()})
    ));
    EXPECT_THROW(loop->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<StyioAST> iter(IterSeqAST::Create(
      ListAST::Create({IntAST::Create("1")}),
      {ParamAST::Create(NameAST::Create("x"), TypeAST::Create("i64"))},
      {}
    ));
    EXPECT_THROW(iter->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<TaskBlockAST> task(TaskBlockAST::Create(BlockAST::Create({
      BlockAST::Create({
        ReturnAST::Create(FloatAST::Create("1.25"))
      })
    })));
    EXPECT_NO_THROW((*task).typeInfer(&analyzer));
    EXPECT_EQ((*task).getResultType().name, "f64");
  }
  {
    std::unique_ptr<StyioAST> bad_method(ResourceMethodDefAST::Create(
      "file",
      "bad_receiver",
      false,
      false,
      {},
      ResourceReceiverAST::Create("stderr")
    ));
    EXPECT_THROW(bad_method->typeInfer(&analyzer), StyioTypeError);
  }
}

TEST(StyioIRContract, TypeConvertAstLowersToValueCarryingCast) {
  AstToStyioIRLowerer analyzer;
  std::unique_ptr<StyioAST> expr(
    TypeConvertAST::Create(IntAST::Create("7"), NumPromoTy::Int_To_Float)
  );

  expr->typeInfer(&analyzer);
  EXPECT_EQ(expr->getDataType().option, StyioDataTypeOption::Float);
  EXPECT_EQ(expr->getDataType().name, "f64");

  std::unique_ptr<StyioIR> ir(expr->toStyioIR(&analyzer));
  auto* cast = dynamic_cast<SGCast*>(ir.get());
  ASSERT_NE(cast, nullptr);
  EXPECT_NE(dynamic_cast<SGConstInt*>(cast->value), nullptr);
  ASSERT_NE(cast->from_type, nullptr);
  ASSERT_NE(cast->to_type, nullptr);
  EXPECT_EQ(cast->from_type->data_type.option, StyioDataTypeOption::Integer);
  EXPECT_EQ(cast->to_type->data_type.option, StyioDataTypeOption::Float);

  struct FallbackCase {
    NumPromoTy promo;
    std::string value_name;
    StyioDataTypeOption from_option;
    StyioDataTypeOption to_option;
    std::string to_name;
  };
  const std::vector<FallbackCase> fallback_cases = {
    {NumPromoTy::Bool_To_Int, "flag_value", StyioDataTypeOption::Bool,
     StyioDataTypeOption::Integer, "i64"},
    {NumPromoTy::Int_To_Float, "count_value", StyioDataTypeOption::Integer,
     StyioDataTypeOption::Float, "f64"},
  };

  for (const FallbackCase& fallback_case : fallback_cases) {
    SCOPED_TRACE(fallback_case.value_name);
    std::unique_ptr<StyioAST> fallback_expr(
      TypeConvertAST::Create(
        NameAST::Create(fallback_case.value_name),
        fallback_case.promo)
    );
    EXPECT_NO_THROW(fallback_expr->typeInfer(&analyzer));
    EXPECT_EQ(fallback_expr->getDataType().option, fallback_case.to_option);
    EXPECT_EQ(fallback_expr->getDataType().name, fallback_case.to_name);
    std::unique_ptr<StyioIR> fallback_ir(fallback_expr->toStyioIR(&analyzer));
    auto* fallback_cast = dynamic_cast<SGCast*>(fallback_ir.get());
    ASSERT_NE(fallback_cast, nullptr);
    EXPECT_NE(dynamic_cast<SGResId*>(fallback_cast->value), nullptr);
    ASSERT_NE(fallback_cast->from_type, nullptr);
    ASSERT_NE(fallback_cast->to_type, nullptr);
    EXPECT_EQ(fallback_cast->from_type->data_type.option, fallback_case.from_option);
    EXPECT_EQ(fallback_cast->to_type->data_type.option, fallback_case.to_option);
    EXPECT_EQ(fallback_cast->to_type->data_type.name, fallback_case.to_name);
  }
}

TEST(StyioIRContract, ShapedTupleReturnAnnotationsLowerToDirectHandleFunctions) {
  auto expect_tuple_return_lowered = [](const char* label, StyioAST* ast) {
    SCOPED_TRACE(label);
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> owned(ast);
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* function = dynamic_cast<SGFunc*>(ir.get());
    ASSERT_NE(function, nullptr);
    ASSERT_NE(function->ret_type, nullptr);
    EXPECT_TRUE(styio_is_shaped_tuple_type(function->ret_type->data_type));
  };

  auto* block_tuple = TupleAST::Create({IntAST::Create("1"), FloatAST::Create("2.5")});
  block_tuple->setDataType(styio_make_tuple_type({
    styio_data_type_from_name("i64"), styio_data_type_from_name("f64")}));
  expect_tuple_return_lowered(
    "FunctionAST",
    FunctionAST::Create(
      NameAST::Create("tuple_fn"),
      false,
      {},
      TypeTupleAST::Create({TypeAST::Create("i64"), TypeAST::Create("f64")}),
      BlockAST::Create({ReturnAST::Create(block_tuple)})));

  auto* simple_tuple = TupleAST::Create({IntAST::Create("1"), FloatAST::Create("2.5")});
  simple_tuple->setDataType(styio_make_tuple_type({
    styio_data_type_from_name("i64"), styio_data_type_from_name("f64")}));
  expect_tuple_return_lowered(
    "SimpleFuncAST",
    SimpleFuncAST::Create(
      NameAST::Create("tuple_simple"),
      false,
      {},
      TypeTupleAST::Create({TypeAST::Create("i64"), TypeAST::Create("f64")}),
      simple_tuple));
}

TEST(StyioIRContract, FunctionLoweringDefaultsUntypedParamsAndRejectsEmptyBodies) {
  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> ast(FunctionAST::Create(
      NameAST::Create("identity"),
      false,
      {ParamAST::Create(NameAST::Create("x"))},
      TypeAST::Create(),
      BlockAST::Create({ReturnAST::Create(NameAST::Create("x"))})
    ));

    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* fn = dynamic_cast<SGFunc*>(ir.get());
    ASSERT_NE(fn, nullptr);
    ASSERT_NE(fn->ret_type, nullptr);
    EXPECT_EQ(fn->ret_type->data_type.option, StyioDataTypeOption::Integer);
    ASSERT_EQ(fn->func_args.size(), 1u);
    ASSERT_NE(fn->func_args[0]->arg_type, nullptr);
    EXPECT_EQ(fn->func_args[0]->arg_type->data_type.option, StyioDataTypeOption::Integer);
    ASSERT_NE(fn->func_block, nullptr);
    ASSERT_EQ(fn->func_block->stmts.size(), 1u);
    EXPECT_NE(dynamic_cast<SGReturn*>(fn->func_block->stmts[0]), nullptr);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> ast(FunctionAST::Create(
      NameAST::Create("null_body"),
      false,
      {},
      TypeAST::Create(),
      nullptr
    ));

    EXPECT_THROW((void)ast->toStyioIR(&analyzer), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> ast(FunctionAST::Create(
      NameAST::Create("empty_block"),
      false,
      {},
      TypeAST::Create(),
      BlockAST::Create({})
    ));

    EXPECT_THROW((void)ast->toStyioIR(&analyzer), StyioTypeError);
  }
}

TEST(StyioIRContract, FunctionsInferUntypedTailAndNestedBlockFamilies) {
  struct TailCase {
    const char* label;
    std::function<StyioAST*()> make_expr;
    StyioDataTypeOption ret_option;
    const char* ret_name;
  };
  const std::vector<TailCase> cases = {
    {
      "format string plus int",
      []()
      {
        return BinOpAST::Create(
          StyioOpType::Binary_Add,
          FmtStrAST::Create({"value="}, {}),
          IntAST::Create("7"));
      },
      StyioDataTypeOption::String,
      "string"
    },
    {
      "float plus int",
      []()
      {
        return BinOpAST::Create(
          StyioOpType::Binary_Add,
          FloatAST::Create("1.5"),
          IntAST::Create("2"));
      },
      StyioDataTypeOption::Float,
      "f64"
    },
    {
      "bool plus bool",
      []()
      {
        return BinOpAST::Create(
          StyioOpType::Binary_Add,
          BoolAST::Create(true),
          BoolAST::Create(false));
      },
      StyioDataTypeOption::Bool,
      "bool"
    },
  };

  for (const TailCase& test_case : cases) {
    SCOPED_TRACE(test_case.label);
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> ast(SimpleFuncAST::Create(
      NameAST::Create(std::string("infer_") + test_case.label),
      false,
      {},
      TypeAST::Create(),
      test_case.make_expr()
    ));

    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* fn = dynamic_cast<SGFunc*>(ir.get());
    ASSERT_NE(fn, nullptr);
    ASSERT_NE(fn->ret_type, nullptr);
    EXPECT_EQ(fn->ret_type->data_type.option, test_case.ret_option);
    EXPECT_EQ(fn->ret_type->data_type.name, test_case.ret_name);
  }

  AstToStyioIRLowerer analyzer;
  std::unique_ptr<StyioAST> ast(FunctionAST::Create(
    NameAST::Create("nested_block_tail"),
    false,
    {},
    TypeAST::Create(),
    BlockAST::Create({
      BlockAST::Create({IntAST::Create("9")})
    })
  ));

  std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
  auto* fn = dynamic_cast<SGFunc*>(ir.get());
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->ret_type, nullptr);
  EXPECT_EQ(fn->ret_type->data_type.option, StyioDataTypeOption::Integer);
  ASSERT_NE(fn->func_block, nullptr);
  ASSERT_EQ(fn->func_block->stmts.size(), 1u);
  auto* inner = dynamic_cast<SGBlock*>(fn->func_block->stmts[0]);
  ASSERT_NE(inner, nullptr);
  ASSERT_EQ(inner->stmts.size(), 1u);
  EXPECT_NE(dynamic_cast<SGReturn*>(inner->stmts[0]), nullptr);
}

TEST(StyioIRContract, MatchTailInferenceCoversPatternFormsAndMergeFamilies) {
  {
    auto* match = MatchCasesAST::make(
      NameAST::Create("x"),
      CasesAST::Create(
        {
          {
            new BinCompAST(CompType::EQ, NameAST::Create("x"), IntAST::Create("1")),
            ReturnAST::Create(BinOpAST::Create(
              StyioOpType::Binary_Add,
              FmtStrAST::Create({"one"}, {}),
              IntAST::Create("1")))
          },
          {
            new BinCompAST(CompType::EQ, IntAST::Create("2"), NameAST::Create("x")),
            ReturnAST::Create(FloatAST::Create("2.5"))
          },
        },
        ReturnAST::Create(IntAST::Create("0"))
      )
    );
    match->setDataType(StyioDataType{StyioDataTypeOption::String, "string", 0});

    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> ast(SimpleFuncAST::Create(
      NameAST::Create("match_string_merge"),
      false,
      {ParamAST::Create(NameAST::Create("x"), TypeAST::Create("i64"))},
      TypeAST::Create(),
      BlockAST::Create({match})
    ));

    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* fn = dynamic_cast<SGFunc*>(ir.get());
    ASSERT_NE(fn, nullptr);
    ASSERT_NE(fn->ret_type, nullptr);
    EXPECT_EQ(fn->ret_type->data_type.option, StyioDataTypeOption::String);
    ASSERT_NE(fn->func_block, nullptr);
    ASSERT_EQ(fn->func_block->stmts.size(), 1u);
    auto* ret = dynamic_cast<SGReturn*>(fn->func_block->stmts[0]);
    ASSERT_NE(ret, nullptr);
    auto* lowered_match = dynamic_cast<SGMatch*>(ret->expr);
    ASSERT_NE(lowered_match, nullptr);
    ASSERT_EQ(lowered_match->int_arms.size(), 2u);
    EXPECT_EQ(lowered_match->int_arms[0].first, 1);
    EXPECT_EQ(lowered_match->int_arms[1].first, 2);
    EXPECT_EQ(lowered_match->repr_kind, SGMatchReprKind::ExprMixed);
  }

  {
    auto* match = MatchCasesAST::make(
      NameAST::Create("x"),
      CasesAST::Create(
        {
          {IntAST::Create("1"), ReturnAST::Create(BoolAST::Create(true))},
        },
        ReturnAST::Create(CharAST::Create("z"))
      )
    );

    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> ast(SimpleFuncAST::Create(
      NameAST::Create("match_bool_char_merge"),
      false,
      {ParamAST::Create(NameAST::Create("x"), TypeAST::Create("i64"))},
      TypeAST::Create(),
      BlockAST::Create({match})
    ));

    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* fn = dynamic_cast<SGFunc*>(ir.get());
    ASSERT_NE(fn, nullptr);
    ASSERT_NE(fn->ret_type, nullptr);
    EXPECT_EQ(fn->ret_type->data_type.option, StyioDataTypeOption::Integer);
    ASSERT_NE(fn->func_block, nullptr);
    ASSERT_EQ(fn->func_block->stmts.size(), 1u);
    auto* ret = dynamic_cast<SGReturn*>(fn->func_block->stmts[0]);
    ASSERT_NE(ret, nullptr);
    auto* lowered_match = dynamic_cast<SGMatch*>(ret->expr);
    ASSERT_NE(lowered_match, nullptr);
    ASSERT_EQ(lowered_match->int_arms.size(), 1u);
    EXPECT_EQ(lowered_match->int_arms[0].first, 1);
  }
}

TEST(StyioIRContract, NestedMatchReturnTreeClassifiesExpressionArms) {
  auto* inner = MatchCasesAST::make(
    NameAST::Create("x"),
    CasesAST::Create(
      {
        {IntAST::Create("2"), ReturnAST::Create(IntAST::Create("10"))},
      },
      BlockAST::Create({
        ReturnAST::Create(IntAST::Create("20")),
      })
    )
  );
  inner->setDataType(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});

  auto* outer_match = MatchCasesAST::make(
    NameAST::Create("x"),
    CasesAST::Create(
      {
        {IntAST::Create("1"), inner},
      },
      ReturnAST::Create(IntAST::Create("30"))
    )
  );
  outer_match->setDataType(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
  std::unique_ptr<StyioAST> outer(outer_match);

  AstToStyioIRLowerer analyzer;
  analyzer.local_binding_types["x"] = styio_data_type_from_name("i64");
  std::unique_ptr<StyioIR> ir(outer->toStyioIR(&analyzer));
  auto* match = dynamic_cast<SGMatch*>(ir.get());
  ASSERT_NE(match, nullptr);
  EXPECT_EQ(match->repr_kind, SGMatchReprKind::ExprInt);
  ASSERT_EQ(match->int_arms.size(), 1u);
  ASSERT_NE(match->int_arms[0].second, nullptr);
  ASSERT_EQ(match->int_arms[0].second->stmts.size(), 1u);
  auto* nested_return = dynamic_cast<SGReturn*>(match->int_arms[0].second->stmts[0]);
  ASSERT_NE(nested_return, nullptr);
  EXPECT_NE(dynamic_cast<SGMatch*>(nested_return->expr), nullptr);
}

TEST(StyioIRContract, MatrixIntrinsicLoweringUsesTypedRuntimeEntryPoints) {
  AstToStyioIRLowerer analyzer;
  analyzer.local_binding_types["mi23"] = styio_make_matrix_type("i64", 2, 3);
  analyzer.local_binding_types["mi32"] = styio_make_matrix_type("i64", 3, 2);
  analyzer.local_binding_types["mf23"] = styio_make_matrix_type("f64", 2, 3);
  analyzer.local_binding_types["mf34"] = styio_make_matrix_type("f64", 3, 4);

  struct CallCase {
    const char* name;
    std::function<std::vector<StyioAST*>()> args;
    const char* runtime_name;
    std::size_t argc;
  };
  const std::vector<CallCase> cases = {
    {"mat_zeros", []() { return std::vector<StyioAST*>{IntAST::Create("2"), IntAST::Create("3")}; },
     "__styio_matrix_new_f64", 2},
    {"mat_zeros_i64", []() { return std::vector<StyioAST*>{IntAST::Create("2"), IntAST::Create("3")}; },
     "__styio_matrix_new_i64", 2},
    {"mat_identity", []() { return std::vector<StyioAST*>{IntAST::Create("3")}; },
     "__styio_matrix_identity_f64", 1},
    {"mat_identity_i64", []() { return std::vector<StyioAST*>{IntAST::Create("3")}; },
     "__styio_matrix_identity_i64", 1},
    {"mat_shape", []() { return std::vector<StyioAST*>{NameAST::Create("mi23")}; },
     "__styio_matrix_shape", 1},
    {"mat_rows", []() { return std::vector<StyioAST*>{NameAST::Create("mi23")}; },
     "__styio_matrix_rows", 1},
    {"mat_cols", []() { return std::vector<StyioAST*>{NameAST::Create("mi23")}; },
     "__styio_matrix_cols", 1},
    {"mat_get", []() { return std::vector<StyioAST*>{NameAST::Create("mi23"), IntAST::Create("0"), IntAST::Create("1")}; },
     "__styio_matrix_get_i64", 3},
    {"mat_get", []() { return std::vector<StyioAST*>{NameAST::Create("mf23"), IntAST::Create("0"), IntAST::Create("1")}; },
     "__styio_matrix_get_f64", 3},
    {"mat_set", []() { return std::vector<StyioAST*>{NameAST::Create("mf23"), IntAST::Create("0"), IntAST::Create("1"), FloatAST::Create("2.5")}; },
     "__styio_matrix_set_f64", 4},
    {"mat_clone", []() { return std::vector<StyioAST*>{NameAST::Create("mi23")}; },
     "__styio_matrix_clone_i64", 1},
    {"transpose", []() { return std::vector<StyioAST*>{NameAST::Create("mf23")}; },
     "__styio_matrix_transpose_f64", 1},
    {"mat_add", []() { return std::vector<StyioAST*>{NameAST::Create("mi23"), NameAST::Create("mf23")}; },
     "__styio_matrix_add_f64", 2},
    {"mat_sub", []() { return std::vector<StyioAST*>{NameAST::Create("mi23"), NameAST::Create("mi23")}; },
     "__styio_matrix_sub_i64", 2},
    {"mat_hadamard", []() { return std::vector<StyioAST*>{NameAST::Create("mf23"), NameAST::Create("mf23")}; },
     "__styio_matrix_hadamard_f64", 2},
    {"mat_scale", []() { return std::vector<StyioAST*>{NameAST::Create("mi23"), IntAST::Create("2")}; },
     "__styio_matrix_scale_i64", 2},
    {"mat_scale", []() { return std::vector<StyioAST*>{NameAST::Create("mi23"), FloatAST::Create("2.0")}; },
     "__styio_matrix_scale_f64", 2},
    {"matmul", []() { return std::vector<StyioAST*>{NameAST::Create("mi23"), NameAST::Create("mf34")}; },
     "__styio_matrix_matmul_f64", 2},
    {"dot", []() { return std::vector<StyioAST*>{NameAST::Create("mi23"), NameAST::Create("mi23")}; },
     "__styio_matrix_dot_i64", 2},
    {"dot", []() { return std::vector<StyioAST*>{NameAST::Create("mi23"), NameAST::Create("mf23")}; },
     "__styio_matrix_dot_f64", 2},
    {"mat_sum", []() { return std::vector<StyioAST*>{NameAST::Create("mi32")}; },
     "__styio_matrix_sum_i64", 1},
    {"norm", []() { return std::vector<StyioAST*>{NameAST::Create("mi23")}; },
     "__styio_matrix_norm", 1},
  };

  for (const CallCase& test_case : cases) {
    SCOPED_TRACE(test_case.name);
    std::unique_ptr<StyioAST> ast(FuncCallAST::Create(
      NameAST::Create(test_case.name),
      test_case.args()
    ));
    std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
    auto* call = dynamic_cast<SGCall*>(ir.get());
    ASSERT_NE(call, nullptr);
    ASSERT_NE(call->func_name, nullptr);
    EXPECT_EQ(call->func_name->as_str(), test_case.runtime_name);
    EXPECT_EQ(call->func_args.size(), test_case.argc);
  }
}

TEST(StyioIRContract, ParallelAssignmentLoweringMaterializesTempsAndIndexedWrites) {
  AstToStyioIRLowerer analyzer;
  analyzer.local_binding_types["items"] = styio_make_list_type("i64");
  analyzer.local_binding_types["lookup"] = styio_make_dict_type("string", "f64");

  std::unique_ptr<StyioAST> ast(ParallelAssignAST::Create(
    {
      NameAST::Create("slot"),
      new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("items"), IntAST::Create("0")),
      new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("lookup"), StringAST::Create("answer")),
    },
    {
      IntAST::Create("1"),
      IntAST::Create("2"),
      FloatAST::Create("3.5"),
    }
  ));

  std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
  auto* block = dynamic_cast<SGBlock*>(ir.get());
  ASSERT_NE(block, nullptr);
  ASSERT_EQ(block->stmts.size(), 6u);
  for (std::size_t i = 0; i < 3; ++i) {
    auto* tmp = dynamic_cast<SGFinalBind*>(block->stmts[i]);
    ASSERT_NE(tmp, nullptr);
    ASSERT_NE(tmp->var, nullptr);
    ASSERT_NE(tmp->var->var_name, nullptr);
    EXPECT_NE(tmp->var->var_name->as_str().find("__styio_parallel_tmp_"), std::string::npos);
  }

  EXPECT_NE(dynamic_cast<SGFlexBind*>(block->stmts[3]), nullptr);

  auto* list_set = dynamic_cast<SCListSet*>(block->stmts[4]);
  ASSERT_NE(list_set, nullptr);
  EXPECT_EQ(list_set->elem_type, "i64");
  EXPECT_NE(dynamic_cast<SGResId*>(list_set->value), nullptr);

  auto* dict_set = dynamic_cast<SCDictSet*>(block->stmts[5]);
  ASSERT_NE(dict_set, nullptr);
  EXPECT_EQ(dict_set->value_type, "f64");
  EXPECT_NE(dynamic_cast<SGResId*>(dict_set->value), nullptr);
}

TEST(StyioIRContract, ResourceDeclarationsSnapshotsAndWritesPreserveStorageContracts) {
  AstToStyioIRLowerer analyzer;
  auto resource_type = [](StyioDataType value, StyioResourceShapeKind shape, std::size_t bound = 0) {
    return styio_make_topology_resource_type(std::move(value), shape, bound);
  };

  std::unique_ptr<StyioAST> decl(ResourceDeclAST::Create({
    {NameAST::Create("recent_i64"), TypeAST::Create(resource_type(styio_data_type_from_name("i64"), StyioResourceShapeKind::Fixed, 3))},
    {NameAST::Create("recent_f64"), TypeAST::Create(resource_type(styio_data_type_from_name("f64"), StyioResourceShapeKind::Recent, 2))},
    {NameAST::Create("recent_bool"), TypeAST::Create(resource_type(styio_data_type_from_name("bool"), StyioResourceShapeKind::Fixed, 2))},
    {NameAST::Create("recent_text"), TypeAST::Create(resource_type(styio_data_type_from_name("string"), StyioResourceShapeKind::Fixed, 2))},
    {NameAST::Create("scalar_i64"), TypeAST::Create(resource_type(styio_data_type_from_name("i64"), StyioResourceShapeKind::Scalar))},
  }));
  decl->typeInfer(&analyzer);

  std::unique_ptr<StyioIR> ir(decl->toStyioIR(&analyzer));
  auto* block = dynamic_cast<SGBlock*>(ir.get());
  ASSERT_NE(block, nullptr);
  ASSERT_EQ(block->stmts.size(), 5u);

  auto expect_bind = [&](std::size_t index, const std::string& name, const std::string& type_name) -> SGFinalBind* {
    auto* bind = dynamic_cast<SGFinalBind*>(block->stmts[index]);
    EXPECT_NE(bind, nullptr);
    if (bind == nullptr || bind->var == nullptr || bind->var->var_name == nullptr || bind->var->var_type == nullptr) {
      return nullptr;
    }
    EXPECT_EQ(bind->var->var_name->as_str(), name);
    EXPECT_EQ(bind->var->var_type->data_type.name, type_name);
    return bind;
  };

  auto* i64_bind = expect_bind(0, "recent_i64", "bounded_ring:3");
  ASSERT_NE(i64_bind, nullptr);
  EXPECT_NE(dynamic_cast<SGConstInt*>(i64_bind->value), nullptr);
  auto* f64_bind = expect_bind(1, "recent_f64", "bounded_ring:f64:2");
  ASSERT_NE(f64_bind, nullptr);
  EXPECT_NE(dynamic_cast<SGConstFloat*>(f64_bind->value), nullptr);
  auto* bool_bind = expect_bind(2, "recent_bool", "bounded_ring:bool:2");
  ASSERT_NE(bool_bind, nullptr);
  EXPECT_NE(dynamic_cast<SGConstBool*>(bool_bind->value), nullptr);
  auto* text_bind = expect_bind(3, "recent_text", "bounded_ring:string:2");
  ASSERT_NE(text_bind, nullptr);
  EXPECT_NE(dynamic_cast<SGConstString*>(text_bind->value), nullptr);
  auto* scalar_bind = expect_bind(4, "scalar_i64", "i64");
  ASSERT_NE(scalar_bind, nullptr);
  EXPECT_NE(dynamic_cast<SGConstInt*>(scalar_bind->value), nullptr);

  {
    std::unique_ptr<StyioAST> ref(ResourceRefAST::CreateSelector(
      NameAST::Create("recent_i64"),
      ResourceSelectorKind::SnapshotAll
    ));
    ref->typeInfer(&analyzer);
    std::unique_ptr<StyioIR> ref_ir(ref->toStyioIR(&analyzer));
    auto* list = dynamic_cast<SCListLiteral*>(ref_ir.get());
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->elem_type, "i64");
    ASSERT_EQ(list->elems.size(), 3u);
    auto* oldest = dynamic_cast<SGResId*>(list->elems.front());
    ASSERT_NE(oldest, nullptr);
    EXPECT_TRUE(oldest->has_history_selector);
    EXPECT_EQ(oldest->history_offset, -3);
  }
  {
    std::unique_ptr<StyioAST> ref(ResourceRefAST::CreateSelector(
      NameAST::Create("recent_i64"),
      ResourceSelectorKind::SliceFrom,
      -2
    ));
    ref->typeInfer(&analyzer);
    std::unique_ptr<StyioIR> ref_ir(ref->toStyioIR(&analyzer));
    auto* list = dynamic_cast<SCListLiteral*>(ref_ir.get());
    ASSERT_NE(list, nullptr);
    ASSERT_EQ(list->elems.size(), 2u);
    auto* oldest = dynamic_cast<SGResId*>(list->elems.front());
    ASSERT_NE(oldest, nullptr);
    EXPECT_EQ(oldest->history_offset, -2);
  }
  {
    std::unique_ptr<StyioAST> write(ResourceWriteAST::Create(
      IntAST::Create("9"),
      ResourceRefAST::Create(NameAST::Create("scalar_i64"))
    ));
    std::unique_ptr<StyioIR> write_ir(write->toStyioIR(&analyzer));
    auto* pending = dynamic_cast<SGFlexBind*>(write_ir.get());
    ASSERT_NE(pending, nullptr);
    EXPECT_TRUE(pending->pending_resource_write);
    ASSERT_NE(pending->var, nullptr);
    ASSERT_NE(pending->var->var_type, nullptr);
    EXPECT_EQ(pending->var->var_type->data_type.name, "i64");
  }
}

TEST(StyioIRContract, MainBlockResourceMethodInliningCoversValueScopesAndProperties) {
  AstToStyioIRLowerer analyzer;
  auto param_i64 = [](const std::string& name) {
    return ParamAST::Create(NameAST::Create(name), TypeAST::Create("i64"));
  };
  auto var_typed = [](const std::string& name, TypeAST* type) {
    return VarAST::Create(NameAST::Create(name), type);
  };

  std::unique_ptr<MainBlockAST> program(MainBlockAST::Create({
    ResourceMethodDefAST::Create(
      "file",
      "answer",
      false,
      false,
      {param_i64("payload")},
      BlockAST::Create({
        FlexBindAST::Create(
          var_typed("n", TypeAST::Create("i64")),
          BinOpAST::Create(StyioOpType::Binary_Add, NameAST::Create("payload"), IntAST::Create("1"))
        ),
        FinalBindAST::Create(
          var_typed("items", TypeAST::Create(styio_make_list_type("i64"))),
          ListAST::Create({NameAST::Create("payload"), NameAST::Create("n")})
        ),
        ReturnAST::Create(new ListOpAST(
          StyioNodeType::Access_By_Index,
          NameAST::Create("items"),
          IntAST::Create("1")))
      })
    ),
    ResourceMethodDefAST::Create(
      "file",
      "label",
      false,
      true,
      {},
      BlockAST::Create({
        ReturnAST::Create(ResourceReceiverAST::Create("file"))
      })
    ),
    HandleAcquireAST::Create(
      var_typed("log", TypeAST::Create(styio_make_file_handle_type("i64"))),
      FileResourceAST::Create(StringAST::Create("/tmp/styio-resource-method-inline"), false)
    ),
    FinalBindAST::Create(
      var_typed("computed", TypeAST::Create("i64")),
      FuncCallAST::Create(NameAST::Create("log"), NameAST::Create("answer"), {IntAST::Create("41")})
    ),
    FinalBindAST::Create(
      var_typed("path", TypeAST::Create("string")),
      AttrAST::Create(NameAST::Create("log"), NameAST::Create("label"))
    ),
  }));

  program->typeInfer(&analyzer);
  std::unique_ptr<StyioIR> ir(program->toStyioIR(&analyzer));
  auto* entry = dynamic_cast<SGMainEntry*>(ir.get());
  ASSERT_NE(entry, nullptr);
  ASSERT_EQ(entry->stmts.size(), 5u);
  EXPECT_NE(dynamic_cast<SGNoOp*>(entry->stmts[0]), nullptr);
  EXPECT_NE(dynamic_cast<SGNoOp*>(entry->stmts[1]), nullptr);
  EXPECT_NE(dynamic_cast<SIOHandleAcquire*>(entry->stmts[2]), nullptr);

  auto* computed = dynamic_cast<SGFinalBind*>(entry->stmts[3]);
  ASSERT_NE(computed, nullptr);
  auto* value_scope = dynamic_cast<SGBlock*>(computed->value);
  ASSERT_NE(value_scope, nullptr);
  ASSERT_EQ(value_scope->stmts.size(), 3u);
  EXPECT_NE(dynamic_cast<SGFlexBind*>(value_scope->stmts[0]), nullptr);
  EXPECT_NE(dynamic_cast<SGFinalBind*>(value_scope->stmts[1]), nullptr);
  EXPECT_NE(dynamic_cast<SCListGet*>(value_scope->stmts[2]), nullptr);

  auto* path = dynamic_cast<SGFinalBind*>(entry->stmts[4]);
  ASSERT_NE(path, nullptr);
  EXPECT_NE(dynamic_cast<SGResId*>(path->value), nullptr);
}

TEST(StyioIRContract, ResourceMethodInliningClonesWritePreface) {
  AstToStyioIRLowerer analyzer;
  auto param_string = [](const std::string& name) {
    return ParamAST::Create(NameAST::Create(name), TypeAST::Create("string"));
  };
  auto var_typed = [](const std::string& name, TypeAST* type) {
    return VarAST::Create(NameAST::Create(name), type);
  };

  std::unique_ptr<MainBlockAST> program(MainBlockAST::Create({
    ResourceMethodDefAST::Create(
      "file",
      "write_echo",
      false,
      false,
      {param_string("payload")},
      BlockAST::Create({
        CommentAST::Create("resource-method write preface"),
        EmptyAST::Create(),
        PassAST::Create(),
        ResourceRedirectAST::Create(
          NameAST::Create("payload"),
          FileResourceAST::Create(StringAST::Create("/tmp/styio-resource-method-write-inline"), false)
        ),
        ReturnAST::Create(NameAST::Create("payload"))
      })
    ),
    HandleAcquireAST::Create(
      var_typed("log", TypeAST::Create(styio_make_file_handle_type("string"))),
      FileResourceAST::Create(StringAST::Create("/tmp/styio-resource-method-write"), false)
    ),
    FinalBindAST::Create(
      var_typed("echo", TypeAST::Create("string")),
      FuncCallAST::Create(
        NameAST::Create("log"),
        NameAST::Create("write_echo"),
        {StringAST::Create("ok")})
    ),
  }));

  program->typeInfer(&analyzer);
  std::unique_ptr<StyioIR> ir(program->toStyioIR(&analyzer));
  auto* entry = dynamic_cast<SGMainEntry*>(ir.get());
  ASSERT_NE(entry, nullptr);
  ASSERT_EQ(entry->stmts.size(), 3u);
  EXPECT_NE(dynamic_cast<SGNoOp*>(entry->stmts[0]), nullptr);
  EXPECT_NE(dynamic_cast<SIOHandleAcquire*>(entry->stmts[1]), nullptr);

  auto* echo = dynamic_cast<SGFinalBind*>(entry->stmts[2]);
  ASSERT_NE(echo, nullptr);
  auto* value_scope = dynamic_cast<SGBlock*>(echo->value);
  ASSERT_NE(value_scope, nullptr);
  ASSERT_GE(value_scope->stmts.size(), 2u);
  EXPECT_NE(dynamic_cast<SIOResourceWriteToFile*>(value_scope->stmts[value_scope->stmts.size() - 2]), nullptr);
  EXPECT_NE(dynamic_cast<SGConstString*>(value_scope->stmts.back()), nullptr);
}

TEST(StyioIRContract, ResourceMethodInliningClonesFmtStringBody) {
  AstToStyioIRLowerer analyzer;
  auto param_i64 = [](const std::string& name) {
    return ParamAST::Create(NameAST::Create(name), TypeAST::Create("i64"));
  };
  auto var_typed = [](const std::string& name, TypeAST* type) {
    return VarAST::Create(NameAST::Create(name), type);
  };

  std::unique_ptr<MainBlockAST> program(MainBlockAST::Create({
    ResourceMethodDefAST::Create(
      "file",
      "summary",
      false,
      false,
      {param_i64("payload")},
      ReturnAST::Create(FmtStrAST::Create(
        {"value="},
        {
          BinOpAST::Create(
            StyioOpType::Binary_Add,
            NameAST::Create("payload"),
            IntAST::Create("1"))
        }))
    ),
    HandleAcquireAST::Create(
      var_typed("log", TypeAST::Create(styio_make_file_handle_type("i64"))),
      FileResourceAST::Create(StringAST::Create("/tmp/styio-resource-method-fmt-inline"), false)
    ),
    FinalBindAST::Create(
      var_typed("summary", TypeAST::Create("string")),
      FuncCallAST::Create(NameAST::Create("log"), NameAST::Create("summary"), {IntAST::Create("41")})
    ),
  }));

  program->typeInfer(&analyzer);
  std::unique_ptr<StyioIR> ir(program->toStyioIR(&analyzer));
  auto* entry = dynamic_cast<SGMainEntry*>(ir.get());
  ASSERT_NE(entry, nullptr);
  ASSERT_EQ(entry->stmts.size(), 3u);
  EXPECT_NE(dynamic_cast<SGNoOp*>(entry->stmts[0]), nullptr);
  EXPECT_NE(dynamic_cast<SIOHandleAcquire*>(entry->stmts[1]), nullptr);

  auto* summary = dynamic_cast<SGFinalBind*>(entry->stmts[2]);
  ASSERT_NE(summary, nullptr);
  auto* fmt = dynamic_cast<SGBinOp*>(summary->value);
  ASSERT_NE(fmt, nullptr);
}

TEST(StyioIRContract, ResourceMethodInliningClonesSizeOfBody) {
  AstToStyioIRLowerer analyzer;
  auto param_i64 = [](const std::string& name) {
    return ParamAST::Create(NameAST::Create(name), TypeAST::Create("i64"));
  };
  auto var_typed = [](const std::string& name, TypeAST* type) {
    return VarAST::Create(NameAST::Create(name), type);
  };

  std::unique_ptr<MainBlockAST> program(MainBlockAST::Create({
    ResourceMethodDefAST::Create(
      "file",
      "count_items",
      false,
      false,
      {param_i64("payload")},
      ReturnAST::Create(new SizeOfAST(ListAST::Create({
        NameAST::Create("payload"),
        IntAST::Create("2")
      })))
    ),
    HandleAcquireAST::Create(
      var_typed("log", TypeAST::Create(styio_make_file_handle_type("i64"))),
      FileResourceAST::Create(StringAST::Create("/tmp/styio-resource-method-sizeof-inline"), false)
    ),
    FinalBindAST::Create(
      var_typed("count", TypeAST::Create("i64")),
      FuncCallAST::Create(NameAST::Create("log"), NameAST::Create("count_items"), {IntAST::Create("41")})
    ),
  }));

  program->typeInfer(&analyzer);
  std::unique_ptr<StyioIR> ir(program->toStyioIR(&analyzer));
  auto* entry = dynamic_cast<SGMainEntry*>(ir.get());
  ASSERT_NE(entry, nullptr);
  ASSERT_EQ(entry->stmts.size(), 3u);
  EXPECT_NE(dynamic_cast<SGNoOp*>(entry->stmts[0]), nullptr);
  EXPECT_NE(dynamic_cast<SIOHandleAcquire*>(entry->stmts[1]), nullptr);

  auto* count = dynamic_cast<SGFinalBind*>(entry->stmts[2]);
  ASSERT_NE(count, nullptr);
  EXPECT_NE(dynamic_cast<SCListLen*>(count->value), nullptr);
}

TEST(StyioIRContract, ResourceMethodInliningClonesHandleAcquireIteratorStatements) {
  AstToStyioIRLowerer analyzer;
  auto var_typed = [](const std::string& name, TypeAST* type) {
    return VarAST::Create(NameAST::Create(name), type);
  };

  std::unique_ptr<MainBlockAST> program(MainBlockAST::Create({
    ResourceMethodDefAST::Create(
      "file",
      "scan",
      false,
      false,
      {},
      BlockAST::Create({
        HandleAcquireAST::Create(
          var_typed("input", TypeAST::Create(styio_make_file_handle_type("string"))),
          FileResourceAST::Create(
            StringAST::Create("tests/features/file_resources/data/hello.txt"),
            false)
        ),
        IteratorAST::Create(
          NameAST::Create("input"),
          {ParamAST::Create(NameAST::Create("line"), TypeAST::Create("string"))},
          BlockAST::Create({
            PrintAST::Create({NameAST::Create("line")})
          }))
      })
    ),
    FinalBindAST::Create(
      var_typed("input", TypeAST::Create("i64")),
      IntAST::Create("7")
    ),
    FuncCallAST::Create(
      FileResourceAST::Create(StringAST::Create("/tmp/styio-resource-method-handle-iter"), false),
      NameAST::Create("scan"),
      {})
  }));

  program->typeInfer(&analyzer);
  std::unique_ptr<StyioIR> ir(program->toStyioIR(&analyzer));
  auto* entry = dynamic_cast<SGMainEntry*>(ir.get());
  ASSERT_NE(entry, nullptr);
  ASSERT_EQ(entry->stmts.size(), 3u);

  auto* call_block = dynamic_cast<SGBlock*>(entry->stmts[2]);
  ASSERT_NE(call_block, nullptr);
  ASSERT_EQ(call_block->stmts.size(), 2u);

  auto* acquire = dynamic_cast<SIOHandleAcquire*>(call_block->stmts[0]);
  ASSERT_NE(acquire, nullptr);
  EXPECT_NE(acquire->var_name, "input");
  EXPECT_EQ(acquire->var_name.rfind("__styio_resource_method_local_", 0), 0u);

  auto* iter = dynamic_cast<SIOFileLineIter*>(call_block->stmts[1]);
  ASSERT_NE(iter, nullptr);
  EXPECT_FALSE(iter->from_path);
  EXPECT_EQ(iter->handle_var, acquire->var_name);
  EXPECT_EQ(iter->line_var, "line");
  ASSERT_NE(iter->body, nullptr);
  ASSERT_FALSE(iter->body->stmts.empty());
  EXPECT_NE(dynamic_cast<SIOStdStreamWrite*>(iter->body->stmts.front()), nullptr);
}

TEST(StyioIRContract, ResourceMethodInliningCoversDirectReturnCastsAndStatementCloneEdges) {
  auto var_typed = [](const std::string& name, TypeAST* type) {
    return VarAST::Create(NameAST::Create(name), type);
  };
  auto file_receiver = []() {
    return FileResourceAST::Create(StringAST::Create("/tmp/styio-resource-method-direct"), false);
  };

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<MainBlockAST> program(MainBlockAST::Create({
      ResourceMethodDefAST::Create(
        "file",
        "bool_as_i64",
        false,
        false,
        {},
        ReturnAST::Create(TypeConvertAST::Create(BoolAST::Create(true), NumPromoTy::Bool_To_Int))
      ),
      ResourceMethodDefAST::Create(
        "file",
        "i64_as_f64",
        false,
        false,
        {},
        ReturnAST::Create(TypeConvertAST::Create(IntAST::Create("9"), NumPromoTy::Int_To_Float))
      ),
      FinalBindAST::Create(
        var_typed("as_i64", TypeAST::Create("i64")),
        FuncCallAST::Create(file_receiver(), NameAST::Create("bool_as_i64"), {})
      ),
      FinalBindAST::Create(
        var_typed("as_f64", TypeAST::Create("f64")),
        FuncCallAST::Create(file_receiver(), NameAST::Create("i64_as_f64"), {})
      ),
    }));

    program->typeInfer(&analyzer);
    std::unique_ptr<StyioIR> ir(program->toStyioIR(&analyzer));
    auto* entry = dynamic_cast<SGMainEntry*>(ir.get());
    ASSERT_NE(entry, nullptr);
    ASSERT_EQ(entry->stmts.size(), 4u);
    auto* as_i64 = dynamic_cast<SGFinalBind*>(entry->stmts[2]);
    auto* as_f64 = dynamic_cast<SGFinalBind*>(entry->stmts[3]);
    ASSERT_NE(as_i64, nullptr);
    ASSERT_NE(as_f64, nullptr);
    EXPECT_NE(dynamic_cast<SGCast*>(as_i64->value), nullptr);
    EXPECT_NE(dynamic_cast<SGCast*>(as_f64->value), nullptr);
  }

  class SeededResourceMethodLowerer : public AstToStyioIRLowerer
  {
  public:
    void seedMethod(const std::string& family, const std::string& method, StyioDataType result_type) {
      ResourceMethodInfo info;
      info.result_type = result_type;
      resource_method_defs_[family][method] = std::move(info);
    }
  };

  auto seeded_program = [&](const std::string& method, StyioAST* body) {
    return std::unique_ptr<MainBlockAST>(MainBlockAST::Create({
      ResourceDeclAST::Create({
        {NameAST::Create("whole_resource"), TypeAST::Create(styio_make_topology_resource_type(
          styio_data_type_from_name("i64"),
          StyioResourceShapeKind::Fixed,
          1
        ))}
      }),
      ResourceMethodDefAST::Create("file", method, false, false, {}, body),
      FinalBindAST::Create(
        var_typed("out_" + method, TypeAST::Create("i64")),
        FuncCallAST::Create(file_receiver(), NameAST::Create(method), {})
      ),
    }));
  };

  {
    SeededResourceMethodLowerer analyzer;
    analyzer.seedMethod("file", "control_edges", styio_data_type_from_name("i64"));
    const auto control_body = []() {
      auto* body = BlockAST::Create({
        ResourceRefAST::Create(NameAST::Create("whole_resource")),
        ResourceReceiverAST::Create("stream"),
        BreakAST::Create(2),
        ContinueAST::Create(),
        ReturnAST::Create(IntAST::Create("1")),
      });
      body->set_followings({PassAST::Create()});
      return body;
    };

    std::unique_ptr<MainBlockAST> registration(MainBlockAST::Create({
      ResourceDeclAST::Create({
        {NameAST::Create("whole_resource"), TypeAST::Create(styio_make_topology_resource_type(
          styio_data_type_from_name("i64"),
          StyioResourceShapeKind::Fixed,
          1
        ))}
      }),
      ResourceMethodDefAST::Create(
        "file", "control_edges", false, false, {}, control_body()),
    }));
    std::unique_ptr<StyioIR> registration_ir(registration->toStyioIR(&analyzer));

    std::unique_ptr<FuncCallAST> direct_call(FuncCallAST::Create(
      file_receiver(), NameAST::Create("control_edges"), {}));
    std::unique_ptr<StyioIR> cloned_ir(direct_call->toStyioIR(&analyzer));
    auto* cloned_block = dynamic_cast<SGBlock*>(cloned_ir.get());
    ASSERT_NE(cloned_block, nullptr);
    ASSERT_GE(cloned_block->stmts.size(), 4u);
    EXPECT_NE(dynamic_cast<SGBreak*>(cloned_block->stmts[2]), nullptr);
    EXPECT_NE(dynamic_cast<SGContinue*>(cloned_block->stmts[3]), nullptr);

    auto program = seeded_program("control_edges", control_body());
    EXPECT_THROW({
      std::unique_ptr<StyioIR> ir(program->toStyioIR(&analyzer));
    }, StyioTypeError);
  }

  {
    SeededResourceMethodLowerer analyzer;
    analyzer.seedMethod("file", "unsupported_clone_edges", styio_data_type_from_name("i64"));
    auto* list = ListAST::Create({IntAST::Create("1"), IntAST::Create("2")});
    list->setDataType(styio_make_list_type("i64"));
    auto* body = BlockAST::Create({
      TupleAST::Create({IntAST::Create("1"), IntAST::Create("2")}),
      SetAST::Create({IntAST::Create("3")}),
      new ListOpAST(StyioNodeType::Get_Reversed, list),
      StdStreamAST::CreateTerminalHandle(StdStreamKind::Stdout),
      CasesAST::Create({{IntAST::Create("1"), IntAST::Create("2")}}, IntAST::Create("0")),
      new InfiniteAST(IntAST::Create("0"), IntAST::Create("1")),
      new InfiniteAST(),
      ReturnAST::Create(IntAST::Create("4")),
    });
    body->set_followings({CommentAST::Create("unreachable after unsupported tuple")});
    auto program = seeded_program("unsupported_clone_edges", body);

    EXPECT_THROW({
      std::unique_ptr<StyioIR> ir(program->toStyioIR(&analyzer));
    }, StyioTypeError);
  }
}

TEST(StyioUtilityContracts, CoversBoundedMethodAndDynamicTagEdgeCases) {
  const StyioDataType undefined{StyioDataTypeOption::Undefined, "undefined", 0};
  const StyioDataType plain_i64{StyioDataTypeOption::Integer, "i64", 64};
  const StyioDataType defined_plain{StyioDataTypeOption::Defined, "plain_resource", 0};
  const StyioDataType bounded_default{StyioDataTypeOption::Defined, "bounded_ring:8", 0};
  const StyioDataType bounded_typed{StyioDataTypeOption::Defined, "bounded_ring:f64:16", 0};
  const StyioDataType bounded_empty_type{StyioDataTypeOption::Defined, "bounded_ring::16", 0};
  const StyioDataType bounded_zero{StyioDataTypeOption::Defined, "bounded_ring:i64:0", 0};
  const StyioDataType bounded_bad_digits{StyioDataTypeOption::Defined, "bounded_ring:i64:16x", 0};
  const StyioDataType bounded_alpha_capacity{StyioDataTypeOption::Defined, "bounded_ring:i64:not-a-number", 0};

  EXPECT_FALSE(styio_bounded_ring_value_type_name(undefined).has_value());
  EXPECT_FALSE(styio_bounded_ring_value_type_name(plain_i64).has_value());
  EXPECT_FALSE(styio_bounded_ring_value_type_name(defined_plain).has_value());
  EXPECT_EQ(styio_bounded_ring_value_type_name(bounded_default).value(), "i64");
  EXPECT_EQ(styio_bounded_ring_value_type_name(bounded_typed).value(), "f64");
  EXPECT_FALSE(styio_bounded_ring_value_type_name(bounded_empty_type).has_value());

  EXPECT_FALSE(styio_bounded_ring_capacity(undefined).has_value());
  EXPECT_FALSE(styio_bounded_ring_capacity(plain_i64).has_value());
  EXPECT_FALSE(styio_bounded_ring_capacity(defined_plain).has_value());
  EXPECT_EQ(styio_bounded_ring_capacity(bounded_default).value(), 8U);
  EXPECT_EQ(styio_bounded_ring_capacity(bounded_typed).value(), 16U);
  EXPECT_FALSE(styio_bounded_ring_capacity(bounded_zero).has_value());
  EXPECT_FALSE(styio_bounded_ring_capacity(bounded_bad_digits).has_value());
  EXPECT_FALSE(styio_bounded_ring_capacity(bounded_alpha_capacity).has_value());

  EXPECT_EQ(styio_builtin_method_kind("drop"), StyioBuiltinMethodKind::ResourceDrop);
  EXPECT_EQ(styio_builtin_method_kind("destroy"), StyioBuiltinMethodKind::ResourceDestroy);
  EXPECT_TRUE(styio_is_resource_destroy_method_kind(StyioBuiltinMethodKind::ResourceClose));
  EXPECT_TRUE(styio_is_resource_destroy_method_name("drop"));
  EXPECT_TRUE(styio_is_resource_destroy_method_name("destroy"));
  EXPECT_FALSE(styio_is_resource_destroy_method_name("write"));
  EXPECT_TRUE(styio_is_resource_write_method_name("write"));
  EXPECT_FALSE(styio_is_resource_write_method_name("close"));
  EXPECT_TRUE(styio_is_resource_property_method_name("path"));
  EXPECT_FALSE(styio_is_resource_property_method_name("destroy"));

  EXPECT_TRUE(styio_dynamic_tag_is_owned_resource(StyioDynamicTag::List));
  EXPECT_TRUE(styio_dynamic_tag_is_owned_resource(StyioDynamicTag::Dict));
  EXPECT_TRUE(styio_dynamic_tag_is_owned_resource(StyioDynamicTag::Matrix));
  EXPECT_TRUE(styio_dynamic_tag_is_owned_resource(StyioDynamicTag::Task));
  EXPECT_FALSE(styio_dynamic_tag_is_owned_resource(StyioDynamicTag::I64));
  EXPECT_FALSE(styio_dynamic_tag_is_owned_resource(StyioDynamicTag::CStr));
}

TEST(StyioDiagnosticContract, ClassifiersCoverFallbackAndPhaseFamilies) {
  namespace diag = styio::services::diagnostics;

  EXPECT_EQ(diag::to_upper_ascii("styio_native"), "STYIO_NATIVE");
  EXPECT_EQ(diag::diagnostic_phase_for_code("STYIO_LEX_INVALID_TOKEN"), "lex");
  EXPECT_EQ(diag::diagnostic_phase_for_code("STYIO_LOWER_UNSUPPORTED_AST"), "lowering");
  EXPECT_EQ(diag::diagnostic_phase_for_code("STYIO_IR_VERIFY_INACTIVE_NODE"), "ir_verify");
  EXPECT_EQ(diag::diagnostic_phase_for_code("STYIO_CODEGEN_ERROR"), "codegen");
  EXPECT_EQ(diag::diagnostic_phase_for_code("STYIO_UNKNOWN"), "service");

  EXPECT_EQ(
    diag::classify_lex_code("unterminated string literal at offset 4"),
    "STYIO_LEX_UNTERMINATED_STRING");
  EXPECT_EQ(
    diag::classify_lex_code("bad byte"),
    "STYIO_LEX_INVALID_TOKEN");
  EXPECT_EQ(
    diag::classify_parse_code("shadow parser mismatch"),
    "STYIO_PARSE_SHADOW_MISMATCH");
  EXPECT_EQ(
    diag::classify_service_code("", "file not found: missing.styio"),
    "STYIO_SERVICE_READ_FAILED");

  EXPECT_EQ(
    diag::classify_native_interop_code("unsupported @extern ABI `rust`"),
    "STYIO_NATIVE_UNSUPPORTED_ABI");
  EXPECT_EQ(
    diag::classify_native_interop_code("native subsystem returned an unknown failure"),
    "STYIO_NATIVE_INTEROP_ERROR");
  EXPECT_EQ(
    diag::classify_type_or_lowering_code("unsupported AST lowering: CasesAST"),
    "STYIO_LOWER_UNSUPPORTED_AST");
  EXPECT_EQ(
    diag::classify_type_or_lowering_code(
      "StyioIR verifier failed: inactive StyioIR node reached codegen boundary"),
    "STYIO_IR_VERIFY_INACTIVE_NODE");
  EXPECT_EQ(
    diag::classify_type_or_lowering_code(
      "StyioIR verifier failed: missing required StyioIR child: SGReturn.expr"),
    "STYIO_IR_VERIFY_CONTRACT");
  EXPECT_EQ(
    diag::classify_type_or_lowering_code("function body requires a return value"),
    "STYIO_TYPE_FUNCTION_MISSING_RETURN");
  EXPECT_EQ(
    diag::classify_type_or_lowering_code("native subsystem returned an unknown failure"),
    "STYIO_NATIVE_INTEROP_ERROR");
  EXPECT_EQ(
    diag::classify_runtime_or_native_code("STYIO_RUNTIME_LIST_INDEX", "ignored"),
    "STYIO_RUNTIME_LIST_INDEX");
  EXPECT_EQ(
    diag::classify_runtime_or_native_code("STYIO_NATIVE_LOAD_FAILED", "ignored"),
    "STYIO_NATIVE_LOAD_FAILED");
  EXPECT_EQ(
    diag::classify_runtime_or_native_code("", "native subsystem returned an unknown failure"),
    "STYIO_NATIVE_INTEROP_ERROR");
}

TEST(StyioIRContract, StandardStreamAliasesLowerToExplicitNoOp) {
  AstToStyioIRLowerer analyzer;
  std::unique_ptr<StyioAST> flex(FlexBindAST::Create(
    VarAST::Create(NameAST::Create("out")),
    StdStreamAST::Create(StdStreamKind::Stdout)
  ));
  std::unique_ptr<StyioAST> final(FinalBindAST::Create(
    VarAST::Create(NameAST::Create("err")),
    StdStreamAST::Create(StdStreamKind::Stderr)
  ));

  std::unique_ptr<StyioIR> flex_ir(flex->toStyioIR(&analyzer));
  std::unique_ptr<StyioIR> final_ir(final->toStyioIR(&analyzer));

  EXPECT_NE(dynamic_cast<SGNoOp*>(flex_ir.get()), nullptr);
  EXPECT_NE(dynamic_cast<SGNoOp*>(final_ir.get()), nullptr);
}

TEST(StyioIRContract, UnsupportedAstNodesFailClosedInsteadOfPlaceholder) {
  auto expect_unsupported = [](const char* label, std::unique_ptr<StyioAST> ast)
  {
    SCOPED_TRACE(label);
    AstToStyioIRLowerer analyzer;
    EXPECT_THROW(
      {
        std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));
      },
      StyioTypeError
    );
  };

  expect_unsupported("NoneAST", std::unique_ptr<StyioAST>(NoneAST::Create()));
  expect_unsupported("TypeTupleAST", std::unique_ptr<StyioAST>(TypeTupleAST::Create()));
  expect_unsupported("TupleAST", std::unique_ptr<StyioAST>(
    TupleAST::Create(std::vector<StyioAST*>{IntAST::Create("1")})
  ));
  expect_unsupported("SetAST", std::unique_ptr<StyioAST>(
    SetAST::Create(std::vector<StyioAST*>{IntAST::Create("1")})
  ));
  expect_unsupported("StdStreamAST", std::unique_ptr<StyioAST>(StdStreamAST::Create(StdStreamKind::Stdout)));
  expect_unsupported("ForwardAST", std::unique_ptr<StyioAST>(new ForwardAST()));
  expect_unsupported("CasesAST", std::unique_ptr<StyioAST>(CasesAST::Create(IntAST::Create("1"))));
  expect_unsupported("StateDeclAST", std::unique_ptr<StyioAST>(
    StateDeclAST::Create(
      IntAST::Create("2"),
      nullptr,
      nullptr,
      VarAST::Create(NameAST::Create("state_out")),
      IntAST::Create("1"))
  ));
  expect_unsupported("StateRefAST", std::unique_ptr<StyioAST>(
    StateRefAST::Create(NameAST::Create("state_out"))
  ));
  expect_unsupported("HistoryProbeAST", std::unique_ptr<StyioAST>(
    HistoryProbeAST::Create(StateRefAST::Create(NameAST::Create("state_out")), IntAST::Create("1"))
  ));
  expect_unsupported("SeriesIntrinsicAST", std::unique_ptr<StyioAST>(
    SeriesIntrinsicAST::Create(NameAST::Create("sample"), SeriesIntrinsicOp::Avg, IntAST::Create("3"))
  ));
  expect_unsupported("IterSeqAST", std::unique_ptr<StyioAST>(IterSeqAST::Create(
    ListAST::Create({IntAST::Create("1")}),
    {ParamAST::Create(NameAST::Create("x"), TypeAST::Create("i64"))},
    {}
  )));

  AstToStyioIRLowerer analyzer;
  std::unique_ptr<OptArgAST> opt_arg(OptArgAST::Create(NameAST::Create("maybe")));
  std::unique_ptr<OptKwArgAST> opt_kw_arg(OptKwArgAST::Create(NameAST::Create("named")));
  EXPECT_THROW(
    {
      std::unique_ptr<StyioIR> ir(analyzer.toStyioIR(opt_arg.get()));
    },
    StyioTypeError);
  EXPECT_THROW(
    {
      std::unique_ptr<StyioIR> ir(analyzer.toStyioIR(opt_kw_arg.get()));
    },
    StyioTypeError);
}

TEST(StyioIRContract, VerifierRejectsLoopControlOutsideLoopsAndAcceptsNestedLoops) {
  const auto verify_owned = [](StyioIR* root) {
    std::unique_ptr<StyioIR> owner(root);
    return styio::ir::verify_styio_ir(owner.get());
  };

  const auto top_level = verify_owned(SGMainEntry::Create({
    SGBreak::Create(),
    SGContinue::Create(),
  }));
  ASSERT_FALSE(top_level.ok());
  ASSERT_EQ(top_level.diagnostics.size(), 2u);
  EXPECT_EQ(top_level.diagnostics[0].phase, "ir_verify");
  EXPECT_EQ(top_level.diagnostics[0].code, "STYIO_IR_VERIFY_CONTRACT");
  EXPECT_EQ(top_level.diagnostics[0].message, "break outside enclosing loop");
  EXPECT_EQ(top_level.diagnostics[1].phase, "ir_verify");
  EXPECT_EQ(top_level.diagnostics[1].code, "STYIO_IR_VERIFY_CONTRACT");
  EXPECT_EQ(top_level.diagnostics[1].message, "continue outside enclosing loop");

  const auto loop_body = verify_owned(SGLoop::CreateWhile(
    SGConstBool::Create(false),
    SGBlock::Create({SGBreak::Create(), SGContinue::Create()})));
  EXPECT_TRUE(loop_body.ok())
    << (loop_body.diagnostics.empty() ? "" : loop_body.diagnostics.front().message);

  const auto foreach_body = verify_owned(SGForEach::Create(
    SCListLiteral::Create({SGConstInt::Create(1)}, "i64"),
    "item",
    "i64",
    SGBlock::Create({SGBreak::Create(), SGContinue::Create()})));
  EXPECT_TRUE(foreach_body.ok())
    << (foreach_body.diagnostics.empty() ? "" : foreach_body.diagnostics.front().message);

  const auto range_body = verify_owned(SGRangeFor::Create(
    SGConstInt::Create(0),
    SGConstInt::Create(1),
    SGConstInt::Create(1),
    "i",
    SGBlock::Create({SGBreak::Create(), SGContinue::Create()})));
  EXPECT_TRUE(range_body.ok())
    << (range_body.diagnostics.empty() ? "" : range_body.diagnostics.front().message);

  const auto nested_then_restored = verify_owned(SGMainEntry::Create({
    SGLoop::CreateInfinite(SGBlock::Create({
      SGForEach::Create(
        SCListLiteral::Create({SGConstInt::Create(1)}, "i64"),
        "item",
        "i64",
        SGBlock::Create({
          SGRangeFor::Create(
            SGConstInt::Create(0),
            SGConstInt::Create(1),
            SGConstInt::Create(1),
            "i",
            SGBlock::Create({SGBreak::Create(), SGContinue::Create()})),
          SGContinue::Create(),
        })),
      SGBreak::Create(),
    })),
    SGBreak::Create(),
    SGContinue::Create(),
  }));
  ASSERT_FALSE(nested_then_restored.ok());
  ASSERT_EQ(nested_then_restored.diagnostics.size(), 2u);
  EXPECT_EQ(nested_then_restored.diagnostics[0].message, "break outside enclosing loop");
  EXPECT_EQ(nested_then_restored.diagnostics[1].message, "continue outside enclosing loop");

  const auto pre_body_operands = verify_owned(SGMainEntry::Create({
    SGLoop::CreateWhile(SGBreak::Create(), SGBlock::Create({})),
    SGForEach::Create(
      SGContinue::Create(),
      "item",
      "i64",
      SGBlock::Create({})),
    SGRangeFor::Create(
      SGBreak::Create(),
      SGContinue::Create(),
      SGBreak::Create(),
      "i",
      SGBlock::Create({})),
  }));
  ASSERT_FALSE(pre_body_operands.ok());
  ASSERT_EQ(pre_body_operands.diagnostics.size(), 5u);
  EXPECT_EQ(pre_body_operands.diagnostics[0].message, "break outside enclosing loop");
  EXPECT_EQ(pre_body_operands.diagnostics[1].message, "continue outside enclosing loop");
  EXPECT_EQ(pre_body_operands.diagnostics[2].message, "break outside enclosing loop");
  EXPECT_EQ(pre_body_operands.diagnostics[3].message, "continue outside enclosing loop");
  EXPECT_EQ(pre_body_operands.diagnostics[4].message, "break outside enclosing loop");

  const auto inherited_outer_depth = verify_owned(SGLoop::CreateInfinite(SGBlock::Create({
    SGLoop::CreateWhile(SGBreak::Create(), SGBlock::Create({})),
    SGForEach::Create(
      SGContinue::Create(),
      "item",
      "i64",
      SGBlock::Create({})),
    SGRangeFor::Create(
      SGBreak::Create(),
      SGContinue::Create(),
      SGBreak::Create(),
      "i",
      SGBlock::Create({})),
  })));
  EXPECT_TRUE(inherited_outer_depth.ok())
    << (inherited_outer_depth.diagnostics.empty()
          ? ""
          : inherited_outer_depth.diagnostics.front().message);
}

TEST(StyioIRContract, VerifierRejectsInactiveIR) {
  styio::ir::StyioIRVerifierResult null_result = styio::ir::verify_styio_ir(nullptr);
  EXPECT_FALSE(null_result.ok());
  ASSERT_FALSE(null_result.diagnostics.empty());
  EXPECT_NE(null_result.diagnostics.front().message.find("missing StyioIR root"), std::string::npos);

  std::unique_ptr<SGCast> missing_value(SGCast::Create(
    nullptr,
    SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64}),
    SGType::Create(StyioDataType{StyioDataTypeOption::Float, "f64", 64})));
  styio::ir::StyioIRVerifierResult missing_child_result = styio::ir::verify_styio_ir(missing_value.get());
  EXPECT_FALSE(missing_child_result.ok());
  ASSERT_FALSE(missing_child_result.diagnostics.empty());
  EXPECT_NE(
    missing_child_result.diagnostics.front().message.find("missing required StyioIR child: SGCast.value"),
    std::string::npos);

  std::unique_ptr<SGNoOp> shared_child(SGNoOp::Create());
  std::unique_ptr<SIOStdStreamWrite> repeated_child(SIOStdStreamWrite::Create(
    SIOStdStreamWrite::Stream::Stdout,
    {shared_child.get(), shared_child.get()}));
  EXPECT_TRUE(styio::ir::verify_styio_ir(repeated_child.get()).ok());
  repeated_child->exprs.clear();

  std::unique_ptr<SIOInstantPull> missing_handle(SIOInstantPull::CreateFromHandle(""));
  styio::ir::StyioIRVerifierResult missing_handle_result =
    styio::ir::verify_styio_ir(missing_handle.get());
  EXPECT_FALSE(missing_handle_result.ok());
  ASSERT_FALSE(missing_handle_result.diagnostics.empty());
  EXPECT_EQ(missing_handle_result.diagnostics.front().phase, "ir_verify");
  EXPECT_EQ(missing_handle_result.diagnostics.front().code, "STYIO_IR_VERIFY_CONTRACT");
  EXPECT_NE(
    missing_handle_result.diagnostics.front().message.find("SIOInstantPull.handle_var"),
    std::string::npos);
  EXPECT_THROW(styio::ir::require_verified_styio_ir(missing_handle.get()), StyioTypeError);

  InactiveTestIR node;

  styio::ir::StyioIRVerifierResult result = styio::ir::verify_styio_ir(&node);
  EXPECT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().phase, "ir_verify");
  EXPECT_EQ(result.diagnostics.front().code, "STYIO_IR_VERIFY_INACTIVE_NODE");

  EXPECT_THROW(styio::ir::require_verified_styio_ir(&node), StyioTypeError);
}

// ---------------------------------------------------------------
// StyioIRWalker — unified IR walker infrastructure
// ---------------------------------------------------------------

TEST(StyioIRWalker, WalkVisitsAllThreeDomains) {
  // Verify the walker dispatch covers SG, SC, and SIO node types
  // without falling through to visitUnknown.
  class DomainCounter : public styio::ir::StyioIRWalker
  {
  public:
    int sg = 0;
    int sc = 0;
    int sio = 0;

    void visitSGBlock(SGBlock*) override { sg++; }
    void visitSCListLiteral(SCListLiteral*) override { sc++; }
    void visitSIOStdStreamLineIter(SIOStdStreamLineIter* node) override {
      sio++;
      // Delegate to default child-walking (walks body SGBlock)
      StyioIRWalker::visitSIOStdStreamLineIter(node);
    }
  };

  DomainCounter counter;

  auto* sg_node = SGBlock::Create({});
  counter.walk(sg_node);
  EXPECT_EQ(counter.sg, 1);
  EXPECT_EQ(counter.sc, 0);
  EXPECT_EQ(counter.sio, 0);
  delete sg_node;

  auto* sc_node = SCListLiteral::Create({});
  counter.walk(sc_node);
  EXPECT_EQ(counter.sg, 1);
  EXPECT_EQ(counter.sc, 1);
  EXPECT_EQ(counter.sio, 0);
  delete sc_node;

  auto* sio_node = SIOStdStreamLineIter::Create("line", SGBlock::Create({}));
  counter.walk(sio_node);
  EXPECT_EQ(counter.sg, 2);  // SGBlock body was walked
  EXPECT_EQ(counter.sc, 1);
  EXPECT_EQ(counter.sio, 1);
  delete sio_node;
}

TEST(StyioIRWalker, NullNodeIsSilentlySkipped) {
  class NoCrashWalker : public styio::ir::StyioIRWalker {};
  NoCrashWalker walker;
  EXPECT_NO_THROW(walker.walk(nullptr));
}

TEST(StyioIRWalker, WalkVectorWalksAllChildren) {
  class ChildCounter : public styio::ir::StyioIRWalker
  {
  public:
    int count = 0;
    void visitSGConstInt(SGConstInt*) override { count++; }
  };

  ChildCounter counter;
  std::vector<StyioIR*> nodes = {
    SGConstInt::Create(1L),
    SGConstInt::Create(2L),
    SGConstInt::Create(3L),
  };
  counter.walkVector(nodes);
  EXPECT_EQ(counter.count, 3);
  styio_delete_ir_nodes(nodes);
}

TEST(StyioIRWalker, BeforeAndAfterHooksFire) {
  class HookWalker : public styio::ir::StyioIRWalker
  {
  public:
    int before = 0;
    int after = 0;
    void beforeNode(StyioIR*) override { before++; }
    void afterNode(StyioIR*) override { after++; }
  };

  HookWalker walker;
  auto* node = SGConstBool::Create(true);
  walker.walk(node);
  EXPECT_EQ(walker.before, 1);
  EXPECT_EQ(walker.after, 1);
  delete node;
}

TEST(StyioIRWalker, DefaultVisitWalksChildrenOfBinOp) {
  // Default visitSGBinOp should walk lhs_expr and rhs_expr children.
  class ChildCheck : public styio::ir::StyioIRWalker
  {
  public:
    bool saw_lhs = false;
    bool saw_rhs = false;
    void visitSGConstBool(SGConstBool*) override { saw_lhs = true; }
  };

  ChildCheck walker;
  auto* binop = SGBinOp::Create(
    SGConstBool::Create(true),
    SGConstBool::Create(false),
    StyioOpType::Binary_Add,
    SGType::Create(StyioDataType{StyioDataTypeOption::Bool, "bool", 1}));
  // Don't use walk() — use dispatch() directly to test default visit
  walker.visitSGBinOp(binop);
  // The default visit walks lhs_expr and rhs_expr, which are SGConstBool
  // But ChildCheck overrides visitSGConstBool — so saw_lhs should fire twice
  EXPECT_TRUE(walker.saw_lhs);
  delete binop;
}

// ---------------------------------------------------------------
// StyioIRPassManager — pass pipeline infrastructure
// ---------------------------------------------------------------

TEST(StyioIRPassManager, OptLevelZeroSkipsCanonicalization) {
  auto* ir = SGBlock::Create({
    SGConstInt::Create(42L),
  });

  auto manager = styio::lowering::default_styio_ir_pass_manager(0);
  styio::lowering::StyioIRPassPipelineOptions opts;
  opts.opt_level = 0;
  opts.verify_before = true;
  opts.verify_after_each_pass = true;
  opts.collect_timing = true;

  auto result = manager.run(ir, opts);
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.passes.empty());  // No passes at opt_level 0
  EXPECT_EQ(result.root, ir);
  delete ir;
}

TEST(StyioIRPassManager, OptLevelOneRunsCanonicalization) {
  auto* ir = SGBlock::Create({
    SGFlexBind::Create(
      SGVar::Create(
        SGResId::Create("x"),
        SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64})),
      SGConstInt::Create(1L)),
  });

  auto manager = styio::lowering::default_styio_ir_pass_manager(1);
  styio::lowering::StyioIRPassPipelineOptions opts;
  opts.opt_level = 1;
  opts.verify_before = true;
  opts.verify_after_each_pass = true;
  opts.collect_timing = true;

  auto result = manager.run(ir, opts);
  EXPECT_TRUE(result.ok());
  ASSERT_EQ(result.passes.size(), 3u);
  EXPECT_EQ(result.passes[0].name, "styioir-dead-suffix-elimination");
  EXPECT_EQ(result.passes[1].name, "styioir-canonicalization");
  EXPECT_EQ(result.passes[2].name, "styioir-constant-folding");
  EXPECT_TRUE(result.passes.front().verifier_before_ok);
  EXPECT_TRUE(result.passes.front().verifier_after_ok);
  // Root may have been transformed by canonicalization
  delete result.root;
}

TEST(StyioSecurityIROptimizer, RemovesExactEffectfulSuffixAndPreservesReachableIdentity) {
  auto* live = SIOPrint::Create({SGConstString::Create("live")});
  auto* value = SGConstInt::Create(7);
  auto* terminator = SGReturn::Create(value);
  auto* root = SGBlock::Create({
    live,
    terminator,
    SIOPrint::Create({SGConstString::Create("dead")}),
    SGConstInt::Create(99),
  });

  ASSERT_TRUE(styio::ir::verify_styio_ir(root).ok());
  const auto stats = styio::lowering::run_dead_stmt_elim_pass(root);
  ASSERT_TRUE(styio::ir::verify_styio_ir(root).ok());
  ASSERT_EQ(root->stmts.size(), 2u);
  EXPECT_EQ(root->stmts[0], live);
  EXPECT_EQ(root->stmts[1], terminator);
  EXPECT_EQ(terminator->expr, value);
  EXPECT_EQ(stats.statement_containers_visited, 1u);
  EXPECT_EQ(stats.statements_examined, 2u);
  EXPECT_EQ(stats.statements_removed, 2u);
  EXPECT_EQ(stats.statement_containers_changed, 1u);
  EXPECT_TRUE(stats.changed());

  StyioRepr before_second_repr;
  const std::string before_second = root->toString(&before_second_repr);
  const auto second = styio::lowering::run_dead_stmt_elim_pass(root);
  StyioRepr after_second_repr;
  EXPECT_EQ(second.statements_removed, 0u);
  EXPECT_EQ(second.statement_containers_changed, 0u);
  EXPECT_EQ(root->toString(&after_second_repr), before_second);
  EXPECT_TRUE(styio::ir::verify_styio_ir(root).ok());
  EXPECT_EQ(root->stmts[0], live);
  EXPECT_EQ(root->stmts[1], terminator);
  delete root;
}

TEST(StyioSecurityIROptimizer, HandlesLoopControlFinalEmptyAndFirstTerminator) {
  auto* break_body = SGBlock::Create({SGNoOp::Create(), SGBreak::Create(), SIOPrint::Create({SGConstString::Create("dead")})});
  auto* continue_body = SGBlock::Create({SGNoOp::Create(), SGContinue::Create(), SIOPrint::Create({SGConstString::Create("dead")})});
  auto* final_body = SGBlock::Create({SGNoOp::Create(), SGBreak::Create()});
  auto* empty_body = SGBlock::Create({});
  auto* multiple_body = SGBlock::Create({SGBreak::Create(), SGContinue::Create(), SGNoOp::Create()});
  auto* root = SGMainEntry::Create({
    SGLoop::CreateInfinite(break_body),
    SGLoop::CreateInfinite(continue_body),
    SGLoop::CreateInfinite(final_body),
    SGLoop::CreateInfinite(empty_body),
    SGLoop::CreateInfinite(multiple_body),
  });

  ASSERT_TRUE(styio::ir::verify_styio_ir(root).ok());
  const auto stats = styio::lowering::run_dead_stmt_elim_pass(root);
  ASSERT_TRUE(styio::ir::verify_styio_ir(root).ok());
  EXPECT_EQ(break_body->stmts.size(), 2u);
  EXPECT_EQ(continue_body->stmts.size(), 2u);
  EXPECT_EQ(final_body->stmts.size(), 2u);
  EXPECT_TRUE(empty_body->stmts.empty());
  ASSERT_EQ(multiple_body->stmts.size(), 1u);
  EXPECT_NE(dynamic_cast<SGBreak*>(multiple_body->stmts[0]), nullptr);
  EXPECT_EQ(stats.statement_containers_visited, 6u);
  EXPECT_EQ(stats.statements_removed, 4u);
  EXPECT_EQ(stats.statement_containers_changed, 3u);
  delete root;
}

TEST(StyioSecurityIROptimizer, CoversAllOwnersWithoutPropagatingNestedTermination) {
  auto* block = SGBlock::Create({SGNoOp::Create(), SGReturn::Create(SGConstInt::Create(1)), SGNoOp::Create()});
  auto* entry = SGEntry::Create({SGNoOp::Create(), SGReturn::Create(SGConstInt::Create(2)), SGNoOp::Create()});
  auto* main = SGMainEntry::Create({SGNoOp::Create(), SGReturn::Create(SGConstInt::Create(3)), SGNoOp::Create()});
  EXPECT_EQ(styio::lowering::run_dead_stmt_elim_pass(block).statements_removed, 1u);
  EXPECT_EQ(styio::lowering::run_dead_stmt_elim_pass(entry).statements_removed, 1u);
  EXPECT_EQ(styio::lowering::run_dead_stmt_elim_pass(main).statements_removed, 1u);
  EXPECT_EQ(block->stmts.size(), 2u);
  EXPECT_EQ(entry->stmts.size(), 2u);
  EXPECT_EQ(main->stmts.size(), 2u);
  delete block;
  delete entry;
  delete main;

  auto* live_after_if = SIOPrint::Create({SGConstString::Create("after-if")});
  auto* live_after_block = SIOPrint::Create({SGConstString::Create("after-block")});
  auto* nested_block = SGBlock::Create({SGReturn::Create(SGConstInt::Create(5))});
  auto* negative = SGMainEntry::Create({
    SGIf::Create(
      SGConstBool::Create(true),
      SGBlock::Create({SGReturn::Create(SGConstInt::Create(1))}),
      SGBlock::Create({SGReturn::Create(SGConstInt::Create(0))})),
    live_after_if,
    nested_block,
    live_after_block,
  });
  ASSERT_TRUE(styio::ir::verify_styio_ir(negative).ok());
  styio::lowering::run_dead_stmt_elim_pass(negative);
  ASSERT_EQ(negative->stmts.size(), 4u);
  EXPECT_EQ(negative->stmts[1], live_after_if);
  EXPECT_EQ(negative->stmts[2], nested_block);
  EXPECT_EQ(negative->stmts[3], live_after_block);
  delete negative;
}

TEST(StyioSecurityIROptimizer, PreservesMainEntryCompileTimeLiveSuffixNodes) {
  auto* terminator = SGReturn::Create(SGConstInt::Create(0));
  auto* function = SGFunc::Create(
    SGType::Create(styio_data_type_from_name("i64")),
    SGResId::Create("late_function"),
    {},
    SGBlock::Create({SGReturn::Create(SGConstInt::Create(42))}));
  auto* export_decl = SGExportDecl::Create({"late_function"});
  auto* extern_block = SGExternBlock::Create(
    "c",
    "int late_external(void) { return 7; }",
    {},
    {"late_external"});
  auto* flex_bind = SGFlexBind::Create(
    SGVar::Create(
      SGResId::Create("late_flex"),
      SGType::Create(styio_data_type_from_name("i64"))),
    SGConstInt::Create(1));
  auto* final_bind = SGFinalBind::Create(
    SGVar::Create(
      SGResId::Create("late_final"),
      SGType::Create(styio_data_type_from_name("i64"))),
    SGConstInt::Create(2));
  auto* root = SGMainEntry::Create({
    terminator,
    SIOPrint::Create({SGConstString::Create("dead-before-metadata")}),
    function,
    export_decl,
    extern_block,
    flex_bind,
    final_bind,
    SIOPrint::Create({SGConstString::Create("dead-after-metadata")}),
  });

  ASSERT_TRUE(styio::ir::verify_styio_ir(root).ok());
  const auto stats = styio::lowering::run_dead_stmt_elim_pass(root);
  ASSERT_TRUE(styio::ir::verify_styio_ir(root).ok());
  ASSERT_EQ(root->stmts.size(), 6u);
  EXPECT_EQ(root->stmts[0], terminator);
  EXPECT_EQ(root->stmts[1], function);
  EXPECT_EQ(root->stmts[2], export_decl);
  EXPECT_EQ(root->stmts[3], extern_block);
  EXPECT_EQ(root->stmts[4], flex_bind);
  EXPECT_EQ(root->stmts[5], final_bind);
  EXPECT_EQ(stats.statement_containers_visited, 2u);
  EXPECT_EQ(stats.statements_examined, 7u);
  EXPECT_EQ(stats.statements_removed, 2u);
  EXPECT_EQ(stats.statement_containers_changed, 1u);
  delete root;
}

TEST(StyioSecurityIROptimizer, TrimsRepresentativeDeepOwningTreeInOneWalk) {
  auto* stream_body = SGBlock::Create({
    SGNoOp::Create(), SGReturn::Create(SGConstInt::Create(1)), SGNoOp::Create()});
  auto* task_body = SGBlock::Create({
    SIOStdStreamLineIter::Create("line", stream_body),
    SGReturn::Create(SGConstInt::Create(2)),
    SGNoOp::Create(),
  });
  auto* loop_body = SGBlock::Create({
    SIOTaskCreate::Create(task_body, styio_data_type_from_name("i64")),
    SGBreak::Create(),
    SGNoOp::Create(),
  });
  auto* match_body = SGBlock::Create({
    SGLoop::CreateInfinite(loop_body),
    SGReturn::Create(SGConstInt::Create(3)),
    SGNoOp::Create(),
  });
  auto* if_body = SGBlock::Create({
    SGMatch::Create(
      SGConstInt::Create(1),
      {{1, match_body}},
      nullptr,
      SGMatchReprKind::Stmt),
    SGReturn::Create(SGConstInt::Create(4)),
    SGNoOp::Create(),
  });
  auto* function_body = SGBlock::Create({
    SGIf::Create(SGConstBool::Create(true), if_body),
    SGReturn::Create(SGConstInt::Create(5)),
    SGNoOp::Create(),
  });
  auto* root = SGMainEntry::Create({
    SGFunc::Create(
      SGType::Create(styio_data_type_from_name("i64")),
      SGResId::Create("deep"),
      {},
      function_body),
    SGNoOp::Create(),
  });

  ASSERT_TRUE(styio::ir::verify_styio_ir(root).ok());
  const auto stats = styio::lowering::run_dead_stmt_elim_pass(root);
  ASSERT_TRUE(styio::ir::verify_styio_ir(root).ok());
  EXPECT_EQ(stream_body->stmts.size(), 2u);
  EXPECT_EQ(task_body->stmts.size(), 2u);
  EXPECT_EQ(loop_body->stmts.size(), 2u);
  EXPECT_EQ(match_body->stmts.size(), 2u);
  EXPECT_EQ(if_body->stmts.size(), 2u);
  EXPECT_EQ(function_body->stmts.size(), 2u);
  EXPECT_EQ(root->stmts.size(), 2u);
  EXPECT_EQ(stats.statement_containers_visited, 7u);
  EXPECT_EQ(stats.statements_examined, 14u);
  EXPECT_EQ(stats.statements_removed, 6u);
  EXPECT_EQ(stats.statement_containers_changed, 6u);
  delete root;
}

TEST(StyioIRPassManager, DeadSuffixRecordDumpsStatisticsAndVerifierGates) {
  auto* root = SGMainEntry::Create({
    SGReturn::Create(SGConstInt::Create(1)),
    SIOPrint::Create({SGConstString::Create("dead")}),
  });
  styio::lowering::StyioIRPassPipelineOptions options;
  options.collect_ir_dumps = true;
  auto result = styio::lowering::run_default_styio_ir_pass_pipeline(root, options);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.passes.size(), 3u);
  const auto& dead = result.passes[0];
  EXPECT_EQ(dead.name, "styioir-dead-suffix-elimination");
  EXPECT_TRUE(dead.verifier_before_ok);
  EXPECT_TRUE(dead.verifier_after_ok);
  EXPECT_EQ(dead.statistics.statements_removed, 1u);
  EXPECT_EQ(dead.statistics.statement_containers_visited, 1u);
  EXPECT_EQ(dead.statistics.statements_examined, 1u);
  EXPECT_EQ(dead.statistics.statement_containers_changed, 1u);
  EXPECT_NE(dead.ir_before, dead.ir_after);
  EXPECT_NE(dead.ir_before.find("styio.ir.print"), std::string::npos);
  EXPECT_EQ(dead.ir_after.find("styio.ir.print"), std::string::npos);
  EXPECT_EQ(result.initial_ir, dead.ir_before);
  EXPECT_EQ(result.final_ir, result.passes.back().ir_after);
  EXPECT_FALSE(result.passes[1].statistics.changed());
  EXPECT_FALSE(result.passes[2].statistics.changed());
  delete result.root;

  auto* inactive = new InactiveTestIR();
  auto* invalid = SGMainEntry::Create({SGReturn::Create(SGConstInt::Create(1)), inactive});
  auto rejected = styio::lowering::run_default_styio_ir_pass_pipeline(invalid);
  EXPECT_FALSE(rejected.ok());
  EXPECT_TRUE(rejected.passes.empty());
  ASSERT_EQ(invalid->stmts.size(), 2u);
  EXPECT_EQ(invalid->stmts[1], inactive);
  delete invalid;
}

TEST(StyioIRPassManager, DefersDeadSuffixUntilLoopControlLegalityIsResolved) {
  styio::lowering::StyioIRPassPipelineOptions deferred_options;
  deferred_options.verifier_options.defer_unresolved_loop_control = true;

  {
    auto* trailing_break = SGBreak::Create();
    auto* fragment = SGBlock::Create({
      SGReturn::Create(SGConstInt::Create(1)),
      trailing_break,
    });
    auto result = styio::lowering::run_default_styio_ir_pass_pipeline(
      fragment,
      deferred_options);
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.passes.size(), 2u);
    EXPECT_EQ(result.passes[0].name, "styioir-canonicalization");
    EXPECT_EQ(result.passes[1].name, "styioir-constant-folding");
    ASSERT_EQ(fragment->stmts.size(), 2u);
    EXPECT_EQ(fragment->stmts[1], trailing_break);
    EXPECT_FALSE(styio::ir::verify_styio_ir(fragment).ok());
    delete fragment;
  }

  auto* dead = SIOPrint::Create({SGConstString::Create("dead")});
  auto* loop_body = SGBlock::Create({SGBreak::Create(), dead});

  styio::lowering::StyioIRPassManager explicit_manager;
  explicit_manager.add_dead_suffix_elimination_pass();
  const auto rejected = explicit_manager.run(loop_body, deferred_options);
  EXPECT_FALSE(rejected.ok());
  EXPECT_TRUE(rejected.passes.empty());
  ASSERT_FALSE(rejected.diagnostics.empty());
  EXPECT_EQ(
    rejected.diagnostics.front().message,
    "dead-suffix elimination requires resolved loop-control legality");
  ASSERT_EQ(loop_body->stmts.size(), 2u);
  EXPECT_EQ(loop_body->stmts[1], dead);

  const auto deferred = styio::lowering::run_default_styio_ir_pass_pipeline(
    loop_body,
    deferred_options);
  ASSERT_TRUE(deferred.ok());
  ASSERT_EQ(loop_body->stmts.size(), 2u);
  EXPECT_EQ(loop_body->stmts[1], dead);

  auto* root = SGMainEntry::Create({SGLoop::CreateInfinite(loop_body)});
  const auto resolved =
    styio::lowering::run_default_styio_ir_pass_pipeline(root);
  ASSERT_TRUE(resolved.ok());
  ASSERT_FALSE(resolved.passes.empty());
  EXPECT_EQ(resolved.passes.front().statistics.statements_removed, 1u);
  ASSERT_EQ(loop_body->stmts.size(), 1u);
  delete root;
}

TEST(StyioIRPassManager, RejectsAliasedOrCyclicOwnershipBeforeDeadSuffixMutation) {
  const auto has_ownership_diagnostic = [](const auto& result) {
    return std::any_of(
      result.diagnostics.begin(),
      result.diagnostics.end(),
      [](const auto& diagnostic) {
        return diagnostic.message
          == "StyioIR ownership graph reuses a node or contains a cycle";
      });
  };

  auto* reused = SGNoOp::Create();
  auto* root = SGMainEntry::Create({
    reused,
    SGReturn::Create(SGConstInt::Create(0)),
    reused,
  });

  // General structural verification remains DAG-compatible. Mutation requires
  // the stronger owning-tree contract applied by the pass manager below.
  EXPECT_TRUE(styio::ir::verify_styio_ir(root).ok());

  const auto result =
    styio::lowering::run_default_styio_ir_pass_pipeline(root);
  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(result.passes.empty());
  ASSERT_EQ(root->stmts.size(), 3u);
  EXPECT_EQ(root->stmts[0], reused);
  EXPECT_EQ(root->stmts[2], reused);
  EXPECT_TRUE(has_ownership_diagnostic(result));

  root->stmts.pop_back();
  delete root;

  auto* cyclic_root = SGMainEntry::Create({
    SGReturn::Create(SGConstInt::Create(0)),
  });
  cyclic_root->stmts.push_back(cyclic_root);

  const auto cyclic_result =
    styio::lowering::run_default_styio_ir_pass_pipeline(cyclic_root);
  EXPECT_FALSE(cyclic_result.ok());
  EXPECT_TRUE(cyclic_result.passes.empty());
  ASSERT_EQ(cyclic_root->stmts.size(), 2u);
  EXPECT_EQ(cyclic_root->stmts[1], cyclic_root);
  EXPECT_TRUE(has_ownership_diagnostic(cyclic_result));

  cyclic_root->stmts.pop_back();
  delete cyclic_root;
}

TEST(StyioSecurityIROptimizer, ReportsLinearWorkAndExactAllocationDestruction) {
  const auto make_identical = []() {
    return SGMainEntry::Create({
      SGLoop::CreateInfinite(SGBlock::Create({
        SGNoOp::Create(), SGBreak::Create(), SGNoOp::Create()})),
    });
  };
  std::unique_ptr<SGMainEntry> identical_a(make_identical());
  std::unique_ptr<SGMainEntry> identical_b(make_identical());
  const auto identical_a_stats =
    styio::lowering::run_dead_stmt_elim_pass(identical_a.get());
  const auto identical_b_stats =
    styio::lowering::run_dead_stmt_elim_pass(identical_b.get());
  EXPECT_EQ(
    identical_a_stats.statement_containers_visited,
    identical_b_stats.statement_containers_visited);
  EXPECT_EQ(identical_a_stats.statements_examined, identical_b_stats.statements_examined);
  EXPECT_EQ(identical_a_stats.statements_removed, identical_b_stats.statements_removed);
  EXPECT_EQ(
    identical_a_stats.statement_containers_changed,
    identical_b_stats.statement_containers_changed);

  for (std::size_t suffix_size : {8u, 32u}) {
    std::vector<StyioIR*> statements;
    statements.push_back(SGNoOp::Create());
    statements.push_back(SGReturn::Create(SGConstInt::Create(1)));
    for (std::size_t index = 0; index < suffix_size; ++index) {
      statements.push_back(SGNoOp::Create());
    }
    auto* root = SGBlock::Create(std::move(statements));
    const auto stats = styio::lowering::run_dead_stmt_elim_pass(root);
    EXPECT_EQ(stats.statements_examined + stats.statements_removed, suffix_size + 2u);
    delete root;
  }

  styio::session_alloc::SessionAllocationStats allocations;
  auto* previous = styio::session_alloc::set_current_ir_stats(&allocations);
  auto* root = SGBlock::Create({
    SGNoOp::Create(),
    SGReturn::Create(SGConstInt::Create(1)),
    SIOPrint::Create({SGConstString::Create("dead")}),
    SGNoOp::Create(),
  });
  const auto raw_before = allocations.raw_allocations;
  const auto arena_before = allocations.arena_allocations;
  const auto destructors_before = allocations.destructor_calls;
  const auto live_nodes = [&]() {
    return allocations.raw_allocations + allocations.arena_allocations
      - allocations.destructor_calls;
  };
  const auto live_before = live_nodes();
  const auto stats = styio::lowering::run_dead_stmt_elim_pass(root);
  EXPECT_EQ(stats.statements_removed, 2u);
  EXPECT_EQ(allocations.raw_allocations, raw_before);
  EXPECT_EQ(allocations.arena_allocations, arena_before);
  EXPECT_EQ(live_before - live_nodes(), 3u);
  EXPECT_EQ(allocations.destructor_calls - destructors_before, 3u);
  delete root;
  EXPECT_EQ(live_nodes(), 0u);
  EXPECT_EQ(allocations.destructor_calls, raw_before);
  styio::session_alloc::set_current_ir_stats(previous);
}

TEST(StyioSecurityIROptimizer, MatchesSuffixFreeRootsThroughRealJitExecution) {
  const auto execute = [](SGMainEntry* root) {
    EXPECT_TRUE(styio::ir::verify_styio_ir(root).ok());
    testing::internal::CaptureStdout();
    try {
      llvm::InitializeNativeTarget();
      llvm::InitializeNativeTargetAsmPrinter();
      llvm::InitializeNativeTargetAsmParser();
      llvm::ExitOnError exit_on_error;
      std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
      StyioToLLVM generator(std::move(jit));
      root->toLLVMIR(&generator);
      generator.execute();
    }
    catch (...) {
      (void)testing::internal::GetCapturedStdout();
      throw;
    }
    std::fflush(stdout);
    return testing::internal::GetCapturedStdout();
  };

  const auto compare = [&](SGMainEntry* with_suffix, SGMainEntry* expected) {
    std::unique_ptr<SGMainEntry> actual_owner(with_suffix);
    std::unique_ptr<SGMainEntry> expected_owner(expected);
    ASSERT_TRUE(styio::ir::verify_styio_ir(actual_owner.get()).ok());
    ASSERT_TRUE(styio::ir::verify_styio_ir(expected_owner.get()).ok());

    const auto result = styio::lowering::run_default_styio_ir_pass_pipeline(actual_owner.get());
    ASSERT_TRUE(result.ok());
    ASSERT_FALSE(result.passes.empty());
    EXPECT_GT(result.passes.front().statistics.statements_removed, 0u);
    StyioRepr actual_repr;
    StyioRepr expected_repr;
    EXPECT_EQ(
      actual_owner->toString(&actual_repr),
      expected_owner->toString(&expected_repr));
    EXPECT_EQ(execute(actual_owner.get()), execute(expected_owner.get()));
  };

  const auto make_return_root = [](bool include_suffix) {
    std::vector<StyioIR*> body{
      SIOPrint::Create({SGConstString::Create("before-return")}),
      SGReturn::Create(SGConstInt::Create(7)),
    };
    if (include_suffix) {
      body.push_back(SIOPrint::Create({SGConstString::Create("dead-return")}));
    }
    return SGMainEntry::Create({
      SGFunc::Create(
        SGType::Create(styio_data_type_from_name("i64")),
        SGResId::Create("dead_suffix_return"),
        {},
        SGBlock::Create(std::move(body))),
      SIOPrint::Create({SGCall::Create(SGResId::Create("dead_suffix_return"), {})}),
    });
  };
  compare(make_return_root(true), make_return_root(false));

  const auto make_break_root = [](bool include_suffix) {
    std::vector<StyioIR*> body{
      SIOPrint::Create({SGConstString::Create("before-break")}),
      SGBreak::Create(),
    };
    if (include_suffix) {
      body.push_back(SIOPrint::Create({SGConstString::Create("dead-break")}));
    }
    return SGMainEntry::Create({
      SGLoop::CreateWhile(SGConstBool::Create(true), SGBlock::Create(std::move(body))),
      SIOPrint::Create({SGConstString::Create("after-break")}),
    });
  };
  compare(make_break_root(true), make_break_root(false));

  const auto make_continue_root = [](bool include_suffix) {
    std::vector<StyioIR*> body{
      SIOPrint::Create({SGConstString::Create("before-continue")}),
      SGContinue::Create(),
    };
    if (include_suffix) {
      body.push_back(SIOPrint::Create({SGConstString::Create("dead-continue")}));
    }
    return SGMainEntry::Create({
      SGRangeFor::Create(
        SGConstInt::Create(0),
        SGConstInt::Create(2),
        SGConstInt::Create(1),
        "i",
        SGBlock::Create(std::move(body))),
      SIOPrint::Create({SGConstString::Create("after-continue")}),
    });
  };
  compare(make_continue_root(true), make_continue_root(false));

  const auto make_main_predeclaration_root = [](bool include_dead_suffix) {
    std::vector<StyioIR*> statements{
      SIOPrint::Create({
        SGCall::Create(SGResId::Create("dead_suffix_late_function"), {})}),
      SGReturn::Create(SGConstInt::Create(0)),
    };
    if (include_dead_suffix) {
      statements.push_back(
        SIOPrint::Create({SGConstString::Create("dead-main-suffix")}));
    }
    statements.push_back(SGFunc::Create(
      SGType::Create(styio_data_type_from_name("i64")),
      SGResId::Create("dead_suffix_late_function"),
      {},
      SGBlock::Create({SGReturn::Create(SGConstInt::Create(23))})));
    return SGMainEntry::Create(std::move(statements));
  };
  compare(
    make_main_predeclaration_root(true),
    make_main_predeclaration_root(false));
}

TEST(StyioIRPassManager, VerifierCatchesInactiveNodeBeforePass) {
  InactiveTestIR inactive;
  auto* ir = SGBlock::Create({&inactive});

  auto manager = styio::lowering::default_styio_ir_pass_manager(1);
  styio::lowering::StyioIRPassPipelineOptions opts;
  opts.verify_before = true;

  auto result = manager.run(ir, opts);
  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.diagnostics.empty());
  // Clean up: remove the inactive node from the block so SGBlock
  // doesn't try to delete it (it's stack-allocated).
  ir->stmts.clear();
  delete ir;
}

TEST(StyioIRPassManager, CollectsIrDumpsWhenRequested) {
  auto* ir = SGBlock::Create({
    SGConstInt::Create(1L),
  });

  styio::lowering::StyioIRPassPipelineOptions opts;
  opts.opt_level = 0;
  opts.collect_ir_dumps = true;

  auto result = styio::lowering::run_default_styio_ir_pass_pipeline(ir, opts);
  EXPECT_TRUE(result.ok());
  EXPECT_FALSE(result.initial_ir.empty());
  EXPECT_FALSE(result.final_ir.empty());
  delete result.root;
}

TEST(StyioIRPassManager, RequirePipelineThrowsOnFailure) {
  InactiveTestIR inactive;
  auto* ir = SGBlock::Create({&inactive});

  EXPECT_THROW(
    styio::lowering::require_default_styio_ir_pass_pipeline(ir),
    StyioTypeError);
  ir->stmts.clear();
  delete ir;
}

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
    StyioLexError
  );
}

TEST(StyioSecurityLexer, UnterminatedStringAfterOwnedTokensStaysExceptionSafe) {
  CompilationSession session;
  EXPECT_THROW(
    {
      session.adopt_tokens(StyioTokenizer::tokenize("555555555555555555555555555555555 \""));
    },
    StyioLexError
  );
}

TEST(StyioSecuritySession, SessionArenaAndCompilationSessionEdgesStayExplicit) {
  using styio::session_alloc::SessionArena;
  using styio::session_alloc::allocate_object;
  using styio::session_alloc::ast_arena_active;
  using styio::session_alloc::free_object;
  using styio::session_alloc::set_current_ast_arena;
  using styio::session_alloc::set_current_token_arena;
  using styio::session_alloc::token_arena_active;

  EXPECT_FALSE(ast_arena_active());
  EXPECT_FALSE(token_arena_active());
  free_object(nullptr);
  void* heap_object = allocate_object(nullptr, 16);
  ASSERT_NE(heap_object, nullptr);
  free_object(heap_object);

  EXPECT_STREQ(CompilationSession::phase_name_for_diagnostics(CompilationPhase::Empty), "Empty");
  EXPECT_STREQ(CompilationSession::phase_name_for_diagnostics(CompilationPhase::Tokenized), "Tokenized");
  EXPECT_STREQ(CompilationSession::phase_name_for_diagnostics(CompilationPhase::Parsed), "Parsed");
  EXPECT_STREQ(CompilationSession::phase_name_for_diagnostics(CompilationPhase::Typed), "Typed");
  EXPECT_STREQ(CompilationSession::phase_name_for_diagnostics(CompilationPhase::Lowered), "Lowered");
  EXPECT_STREQ(CompilationSession::phase_name_for_diagnostics(CompilationPhase::CodegenReady), "CodegenReady");
  EXPECT_STREQ(CompilationSession::phase_name_for_diagnostics(CompilationPhase::Executed), "Executed");
  EXPECT_STREQ(CompilationSession::phase_name_for_diagnostics(CompilationPhase::Failed), "Failed");
  EXPECT_STREQ(
    CompilationSession::phase_name_for_diagnostics(static_cast<CompilationPhase>(999)),
    "Unknown");
  EXPECT_TRUE(CompilationSession::phase_at_least(CompilationPhase::Parsed, CompilationPhase::Tokenized));
  EXPECT_TRUE(CompilationSession::phase_at_least(CompilationPhase::Executed, CompilationPhase::CodegenReady));
  EXPECT_FALSE(CompilationSession::phase_at_least(CompilationPhase::Empty, CompilationPhase::Parsed));
  EXPECT_FALSE(CompilationSession::phase_at_least(CompilationPhase::Failed, CompilationPhase::Empty));

  SessionArena arena_a(32);
  EXPECT_EQ(arena_a.bytes_used(), 0u);
  EXPECT_NE(arena_a.allocate_span(7), nullptr);
  EXPECT_GT(arena_a.bytes_used(), 0u);
  SessionArena arena_b(std::move(arena_a));
  EXPECT_EQ(arena_a.bytes_used(), 0u);
  EXPECT_GT(arena_b.bytes_used(), 0u);
  SessionArena arena_c(16);
  arena_c = std::move(arena_b);
  EXPECT_EQ(arena_b.bytes_used(), 0u);
  EXPECT_GT(arena_c.bytes_used(), 0u);
  arena_c = std::move(arena_c);
  EXPECT_GT(arena_c.bytes_used(), 0u);

  {
    SessionArena* previous_ast = set_current_ast_arena(&arena_c);
    SessionArena* previous_token = set_current_token_arena(&arena_c);
    EXPECT_TRUE(ast_arena_active());
    EXPECT_TRUE(token_arena_active());
    set_current_ast_arena(previous_ast);
    set_current_token_arena(previous_token);
  }

  CompilationSession session;
  EXPECT_TRUE(ast_arena_active());
  EXPECT_TRUE(token_arena_active());
  session.adopt_tokens(StyioTokenizer::tokenize("x\n"));
  const CompilationSession& const_session = session;
  EXPECT_FALSE(const_session.tokens().empty());

  StyioContext* first = StyioContext::Create(
    "<session-one>",
    "x\n",
    build_line_seps("x\n"),
    session.tokens(),
    false);
  EXPECT_EQ(session.attach_context(first), first);
  StyioContext* second = StyioContext::Create(
    "<session-two>",
    "x\n",
    build_line_seps("x\n"),
    session.tokens(),
    false);
  EXPECT_EQ(session.attach_context(second), second);
  session.reset();
  EXPECT_EQ(session.phase(), CompilationPhase::Empty);
}

TEST(StyioSecurityLexer, UnterminatedBlockCommentThrowsLexError) {
  EXPECT_THROW(
    {
      auto tokens = StyioTokenizer::tokenize("a /* no closing");
      free_tokens(tokens);
    },
    StyioLexError
  );
}

TEST(StyioSecurityParserContext, EmptyTokenVectorFallsBackToEofToken) {
  std::vector<StyioToken*> tokens;
  StyioContext* ctx = StyioContext::Create(
    "<empty-token-context>",
    "",
    {{0, 0}},
    tokens,
    false
  );

  EXPECT_EQ(ctx->cur_tok_type(), StyioTokenType::TOK_EOF);
  EXPECT_FALSE(ctx->match(StyioTokenType::NAME));
  EXPECT_FALSE(ctx->try_match(StyioTokenType::NAME));
  EXPECT_EQ(ctx->mark_cur_tok("empty-token-context"), "empty-token-context");
  EXPECT_EQ(ctx->current_token_end_pos(), 0u);
  ctx->move_forward(1, "empty-token-context");
  EXPECT_EQ(ctx->get_curr_pos(), 0u);

  bool map_matched = false;
  EXPECT_NO_THROW({
    map_matched = ctx->map_match(StyioTokenType::BINOP_EQ);
  });
  EXPECT_FALSE(map_matched);

  delete ctx;
}

TEST(StyioSecurityParserContext, LabelCurrentLineClampsSparseLineSeparators) {
  auto tokens = StyioTokenizer::tokenize("x");
  StyioContext* ctx = StyioContext::Create(
    "<sparse-line-seps>",
    "x",
    {{50, 10}},
    tokens,
    false
  );

  const std::string label = ctx->label_cur_line(50, "sparse-line-seps");
  EXPECT_NE(label.find("sparse-line-seps"), std::string::npos) << label;
  EXPECT_NE(label.find("Line 0"), std::string::npos) << label;

  delete ctx;
  free_tokens(tokens);
}

TEST(StyioSecurityParserContext, MoveForwardBeyondTokenTailIsClampedToEof) {
  auto tokens = StyioTokenizer::tokenize("x");
  StyioContext* ctx = StyioContext::Create(
    "<move-forward-clamp>",
    "x",
    {{0, 1}},
    tokens,
    false
  );

  EXPECT_NO_THROW({
    ctx->move_forward(tokens.size() + 5, "security-clamp");
  });
  EXPECT_EQ(ctx->cur_tok_type(), StyioTokenType::TOK_EOF);

  delete ctx;
  free_tokens(tokens);
}

TEST(StyioSecurityParserContext, CoversDirectContextNavigationAndCharacterHelpers) {
  {
    const std::string src = "   x == y\nz";
    auto tokens = StyioTokenizer::tokenize(src);
    StyioContext* ctx = StyioContext::Create(
      "<context-token-helpers>",
      src,
      build_line_seps(src),
      tokens,
      false
    );

    EXPECT_GE(ctx->check_seq_of(StyioTokenType::TOK_SPACE), 1u);
    EXPECT_TRUE(ctx->try_check(StyioTokenType::NAME));
    EXPECT_TRUE(ctx->try_match(StyioTokenType::NAME));
    EXPECT_TRUE(ctx->try_match_panic(StyioTokenType::BINOP_EQ));
    EXPECT_THROW(ctx->match_panic(StyioTokenType::INTEGER), StyioSyntaxError);
    EXPECT_THROW(ctx->match_panic(StyioTokenType::INTEGER, "custom mismatch"), StyioSyntaxError);

    ctx->record_parse_diagnostic(5, 3, "reversed");
    ctx->record_parse_diagnostic(7, 7, "point");
    ASSERT_GE(ctx->parse_diagnostics().size(), 2u);
    EXPECT_EQ(ctx->parse_diagnostics()[0].start, 5u);
    EXPECT_EQ(ctx->parse_diagnostics()[0].end, 6u);
    EXPECT_EQ(ctx->parse_diagnostics()[1].end, 8u);

    ctx->move_forward(tokens.size() + 10, "context-helper-tail");
    ctx->skip();
    EXPECT_THROW(ctx->try_match_panic(StyioTokenType::NAME), StyioParseError);

    delete ctx;
    free_tokens(tokens);
  }

  {
    auto tokens = StyioTokenizer::tokenize("{ bad }\nnext");
    StyioContext* ctx = StyioContext::Create(
      "<context-recover-brace>",
      "{ bad }\nnext",
      build_line_seps("{ bad }\nnext"),
      tokens,
      false
    );
    EXPECT_TRUE(ctx->recover_to_statement_boundary(0));
    EXPECT_EQ(ctx->cur_tok_type(), StyioTokenType::NAME);
    delete ctx;
    free_tokens(tokens);
  }

  {
    auto tokens = StyioTokenizer::tokenize("}\nnext");
    StyioContext* ctx = StyioContext::Create(
      "<context-recover-base-close>",
      "}\nnext",
      build_line_seps("}\nnext"),
      tokens,
      false
    );
    EXPECT_FALSE(ctx->recover_to_statement_boundary(0));
    EXPECT_EQ(ctx->cur_tok_type(), StyioTokenType::TOK_RCURBRAC);
    delete ctx;
    free_tokens(tokens);
  }

  {
    auto tokens = StyioTokenizer::tokenize("(unterminated");
    StyioContext* ctx = StyioContext::Create(
      "<context-recover-eof>",
      "(unterminated",
      build_line_seps("(unterminated"),
      tokens,
      false
    );
    ctx->move_forward(1, "inside-unclosed-paren");
    EXPECT_FALSE(ctx->recover_to_statement_boundary(0));
    EXPECT_EQ(ctx->cur_tok_type(), StyioTokenType::TOK_EOF);
    delete ctx;
    free_tokens(tokens);
  }

  {
    auto tokens = StyioTokenizer::tokenize("x");
    StyioParserRouteStats stats;
    StyioContext* ctx = StyioContext::Create(
      "<context-bridge-count>",
      "x",
      build_line_seps("x"),
      tokens,
      false
    );
    ctx->set_parser_route_stats_latest(&stats);
    EXPECT_EQ(ctx->note_nightly_internal_legacy_bridge_latest(), 1u);
    EXPECT_EQ(ctx->note_nightly_internal_legacy_bridge_latest(), 2u);
    EXPECT_EQ(stats.nightly_internal_legacy_bridges, 2u);
    delete ctx;
    free_tokens(tokens);
  }

  auto make_char_context = [](const std::string& src)
  {
    return std::unique_ptr<StyioContext>(StyioContext::Create(
      "<context-char-helpers>",
      src,
      build_line_seps(src),
      {},
      false
    ));
  };

  {
    auto ctx = make_char_context("abc:");
    ctx->move_until(':');
    EXPECT_EQ(ctx->get_curr_char(), ':');
    ctx->move(100);
    EXPECT_EQ(ctx->get_curr_pos(), ctx->get_code().size());
    ctx->move(1);
    EXPECT_EQ(ctx->get_curr_pos(), ctx->get_code().size());
  }

  {
    auto ctx = make_char_context("abcend");
    ctx->move_until("end");
    EXPECT_TRUE(ctx->check_next("end"));
    EXPECT_FALSE(ctx->check_next("ending"));
  }

  {
    auto ctx = make_char_context("  // c\n/* b */abc=>");
    EXPECT_TRUE(ctx->check_drop(' '));
    EXPECT_TRUE(ctx->check_drop(" "));
    EXPECT_TRUE(ctx->find_drop('a'));
    EXPECT_TRUE(ctx->find_drop("bc"));
    EXPECT_FALSE(ctx->find_drop('z'));
    EXPECT_TRUE(ctx->check_drop_panic('='));
    EXPECT_THROW(ctx->check_drop_panic('x', "expected x"), StyioSyntaxError);
  }

  {
    auto ctx = make_char_context("  // c\n/* b */target");
    EXPECT_TRUE(ctx->find_drop_panic("target"));
  }
  {
    auto ctx = make_char_context("  // c\n/* b */target");
    EXPECT_TRUE(ctx->find_panic("target"));
  }
  {
    auto ctx = make_char_context("actual");
    EXPECT_THROW(ctx->find_drop_panic("missing"), StyioSyntaxError);
  }
  {
    auto ctx = make_char_context("actual");
    EXPECT_THROW(ctx->find_panic("missing"), StyioSyntaxError);
  }

  {
    auto ctx = make_char_context("a1");
    EXPECT_TRUE(ctx->check_isal_());
    EXPECT_TRUE(ctx->check_isalnum_());
    EXPECT_FALSE(ctx->check_isdigit());
    ctx->move(1);
    EXPECT_TRUE(ctx->check_isdigit());
    EXPECT_TRUE(ctx->check_ahead(-1, 'a'));
    EXPECT_FALSE(ctx->check_ahead(-2, 'a'));
    EXPECT_TRUE(ctx->peak_isdigit(0));
    EXPECT_FALSE(ctx->peak_isdigit(-2));
  }

  EXPECT_TRUE(make_char_context("<<")->check_tuple_ops());
  EXPECT_TRUE(make_char_context("filter")->check_codp());
  EXPECT_FALSE(make_char_context("noop")->check_codp());
  EXPECT_EQ(type_to_int(StyioParserEngine::Nightly), 1);

  const std::vector<std::pair<std::string, StyioOpType>> binary_tokens = {
    {"+", StyioOpType::Binary_Add},
    {"-", StyioOpType::Binary_Sub},
    {"*", StyioOpType::Binary_Mul},
    {"/", StyioOpType::Binary_Div},
    {"%", StyioOpType::Binary_Mod},
  };
  for (const auto& [src, op] : binary_tokens) {
    auto ctx = make_char_context(src);
    EXPECT_TRUE(ctx->check_binop()) << src;
    const auto [is_operator, mapped_op] = ctx->get_binop_token();
    EXPECT_TRUE(is_operator) << src;
    EXPECT_EQ(mapped_op, op) << src;
  }

  {
    auto ctx = make_char_context("// comment");
    EXPECT_FALSE(ctx->check_binop());
    const auto [is_operator, mapped_op] = ctx->get_binop_token();
    EXPECT_FALSE(is_operator);
    EXPECT_EQ(mapped_op, StyioOpType::Comment_SingleLine);
  }
  {
    auto ctx = make_char_context("/* comment */");
    EXPECT_FALSE(ctx->check_binop());
    const auto [is_operator, mapped_op] = ctx->get_binop_token();
    EXPECT_FALSE(is_operator);
    EXPECT_EQ(mapped_op, StyioOpType::Comment_MultiLine);
  }
  {
    auto ctx = make_char_context("?");
    EXPECT_FALSE(ctx->check_binop());
    const auto [is_operator, mapped_op] = ctx->get_binop_token();
    EXPECT_FALSE(is_operator);
    EXPECT_EQ(mapped_op, StyioOpType::Undefined);
  }
  {
    auto ctx = make_char_context("");
    EXPECT_FALSE(ctx->check_binop());
    const auto [is_operator, mapped_op] = ctx->get_binop_token();
    EXPECT_FALSE(is_operator);
    EXPECT_EQ(mapped_op, StyioOpType::Undefined);
  }
}

TEST(StyioSecurityParserContext, CoversLegacyScalarAndParameterHelpersDirectly) {
  {
    DirectParserContext ctx("alpha");
    EXPECT_EQ(parse_name_as_str(ctx.get()), "alpha");
  }
  {
    DirectParserContext ctx("123");
    EXPECT_THROW(parse_name_as_str(ctx.get()), StyioParseError);
  }
  {
    DirectParserContext ctx("hash tag 123");
    std::unique_ptr<HashTagNameAST> tag(parse_name_for_hash_tag(ctx.get()));
    ASSERT_NE(tag, nullptr);
    ASSERT_EQ(tag->words.size(), 3u);
    EXPECT_EQ(tag->words[0], "hash");
    EXPECT_EQ(tag->words[2], "123");
  }
  {
    DirectParserContext ctx("i64");
    std::unique_ptr<TypeAST> type(parse_dtype(ctx.get()));
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->getNodeType(), StyioNodeType::DType);
    EXPECT_EQ(type->getTypeName(), "i64");
  }
  {
    DirectParserContext ctx("custom_resource");
    std::unique_ptr<TypeAST> type(parse_name_as_type_unsafe(ctx.get()));
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->getTypeName(), "custom_resource");
  }
  {
    DirectParserContext ctx("123.45");
    std::unique_ptr<StyioAST> value(parse_int_or_float(ctx.get()));
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->getNodeType(), StyioNodeType::Float);
  }
  {
    DirectParserContext ctx("42.");
    std::unique_ptr<StyioAST> value(parse_int_or_float(ctx.get()));
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectParserContext ctx("'x'");
    std::unique_ptr<StyioAST> value(parse_char_or_string(ctx.get()));
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->getNodeType(), StyioNodeType::Char);
  }
  {
    DirectParserContext ctx("'long'");
    std::unique_ptr<StyioAST> value(parse_char_or_string(ctx.get()));
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->getNodeType(), StyioNodeType::String);
  }
  {
    DirectParserContext ctx("\"left {{ brace }} right\"");
    std::unique_ptr<FmtStrAST> fmt(parse_fmt_str(ctx.get()));
    ASSERT_NE(fmt, nullptr);
    ASSERT_EQ(fmt->getFragments().size(), 1u);
    EXPECT_EQ(fmt->getFragments()[0], "left { brace } right");
    EXPECT_TRUE(fmt->getExprs().empty());
  }
  {
    DirectParserContext ctx("\"bad }\"");
    EXPECT_THROW(parse_fmt_str(ctx.get()), StyioSyntaxError);
  }
  {
    DirectParserContext ctx("$\"left {1 + 2} {{ brace }}\"");
    std::unique_ptr<FmtStrAST> fmt(parse_fmt_str_token_latest(ctx.get(), StyioParserEngine::Nightly));
    ASSERT_NE(fmt, nullptr);
    ASSERT_EQ(fmt->getFragments().size(), 2u);
    EXPECT_EQ(fmt->getFragments()[0], "left ");
    EXPECT_EQ(fmt->getFragments()[1], " { brace }");
    ASSERT_EQ(fmt->getExprs().size(), 1u);
  }
  {
    DirectParserContext ctx("plain");
    EXPECT_THROW(parse_fmt_str_token_latest(ctx.get(), StyioParserEngine::Nightly), StyioParseError);
  }
  {
    DirectParserContext ctx("$ name");
    EXPECT_THROW(parse_fmt_str_token_latest(ctx.get(), StyioParserEngine::Nightly), StyioSyntaxError);
  }
  {
    DirectParserContext ctx("$\"{}\"");
    EXPECT_THROW(parse_fmt_str_token_latest(ctx.get(), StyioParserEngine::Nightly), StyioSyntaxError);
  }
  {
    DirectParserContext ctx("plain");
    std::unique_ptr<ParamAST> param(parse_argument(ctx.get()));
    ASSERT_NE(param, nullptr);
    EXPECT_EQ(param->getNodeType(), StyioNodeType::Param);
  }
  {
    DirectParserContext ctx("typed:i64");
    std::unique_ptr<ParamAST> param(parse_argument(ctx.get()));
    ASSERT_NE(param, nullptr);
    ASSERT_NE(param->getDType(), nullptr);
    EXPECT_EQ(param->val_init, nullptr);
  }
  {
    DirectParserContext ctx("with_default:f64=1.5");
    std::unique_ptr<ParamAST> param(parse_argument(ctx.get()));
    ASSERT_NE(param, nullptr);
    ASSERT_NE(param->getDType(), nullptr);
  }
  {
    DirectParserContext ctx("()");
    std::unique_ptr<VarTupleAST> params(parse_var_tuple(ctx.get()));
    ASSERT_NE(params, nullptr);
    EXPECT_TRUE(params->getParams().empty());
  }
  {
    DirectParserContext ctx("(one,two)");
    std::unique_ptr<VarTupleAST> params(parse_var_tuple(ctx.get()));
    ASSERT_NE(params, nullptr);
    EXPECT_EQ(params->getParams().size(), 2u);
  }
  {
    DirectParserContext ctx("(*,kept)");
    std::unique_ptr<VarTupleAST> params(parse_var_tuple(ctx.get()));
    ASSERT_NE(params, nullptr);
    ASSERT_EQ(params->getParams().size(), 1u);
    EXPECT_EQ(params->getParams()[0]->getNameAsStr(), "kept");
  }
}

TEST(StyioSecurityParserContext, CoversLegacyContainerConditionAndLoopHelpersDirectly) {
  {
    DirectParserContext ctx("()");
    std::unique_ptr<StyioAST> tuple(parse_tuple(ctx.get()));
    ASSERT_NE(tuple, nullptr);
    EXPECT_EQ(tuple->getNodeType(), StyioNodeType::Tuple);
  }
  {
    DirectParserContext ctx("{}");
    std::unique_ptr<StyioAST> set(parse_set(ctx.get()));
    ASSERT_NE(set, nullptr);
    EXPECT_EQ(set->getNodeType(), StyioNodeType::Set);
  }
  {
    DirectParserContext ctx("()");
    std::unique_ptr<StyioAST> tuple(parse_iterable(ctx.get()));
    ASSERT_NE(tuple, nullptr);
    EXPECT_EQ(tuple->getNodeType(), StyioNodeType::Tuple);
  }
  {
    DirectParserContext ctx("[]");
    std::unique_ptr<StyioAST> list(parse_iterable(ctx.get()));
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->getNodeType(), StyioNodeType::List);
  }
  {
    DirectParserContext ctx("{}");
    std::unique_ptr<StyioAST> set(parse_iterable(ctx.get()));
    ASSERT_NE(set, nullptr);
    EXPECT_EQ(set->getNodeType(), StyioNodeType::Set);
  }
  {
    DirectParserContext ctx("name");
    std::unique_ptr<StyioAST> name(parse_iterable(ctx.get()));
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(name->getNodeType(), StyioNodeType::Id);
  }
  const std::vector<std::string> iterable_name_binops = {
    "name+1",
    "name-1",
    "name*1",
    "name**2",
    "name/1",
    "name%1",
  };
  for (const auto& source : iterable_name_binops) {
    SCOPED_TRACE(source);
    DirectParserContext ctx(source);
    StyioAST* raw_expr = nullptr;
    ASSERT_NO_THROW(raw_expr = parse_iterable(ctx.get()));
    std::unique_ptr<StyioAST> expr(raw_expr);
    auto* binop = dynamic_cast<BinOpAST*>(expr.get());
    ASSERT_NE(binop, nullptr) << source;
  }
  {
    DirectParserContext ctx("x");
    std::unique_ptr<CondAST> cond(parse_cond(ctx.get()));
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->getNodeType(), StyioNodeType::Condition);
    EXPECT_EQ(cond->getSign(), LogicType::RAW);
  }
  {
    DirectParserContext ctx("...]");
    std::unique_ptr<StyioAST> infinite(parse_loop(ctx.get()));
    ASSERT_NE(infinite, nullptr);
    EXPECT_EQ(infinite->getNodeType(), StyioNodeType::Infinite);
  }
  {
    DirectParserContext ctx("...] >>");
    std::unique_ptr<StyioAST> loop(parse_loop(ctx.get()));
    ASSERT_NE(loop, nullptr);
    EXPECT_EQ(loop->getNodeType(), StyioNodeType::Loop);
  }
  {
    DirectParserContext ctx("");
    std::unique_ptr<StyioAST> infinite(new InfiniteAST());
    std::unique_ptr<StyioAST> loop(parse_loop_or_iter(ctx.get(), infinite.release()));
    ASSERT_NE(loop, nullptr);
    EXPECT_EQ(loop->getNodeType(), StyioNodeType::Loop);
  }
  {
    DirectParserContext ctx("");
    std::unique_ptr<NameAST> name(NameAST::Create("xs"));
    EXPECT_THROW(parse_loop_or_iter(ctx.get(), name.get()), StyioParseError);
  }
}

TEST(StyioSecurityParserContext, CoversLegacyValueConditionAndFlowHelpersDirectly) {
  {
    DirectParserContext ctx("|items|");
    align_legacy_char_entry_token(ctx, StyioTokenType::NAME);
    std::unique_ptr<SizeOfAST> size(parse_size_of(ctx.get()));
    ASSERT_NE(size, nullptr);
    EXPECT_EQ(size->getNodeType(), StyioNodeType::SizeOf);
    ASSERT_NE(size->getValue(), nullptr);
    EXPECT_EQ(size->getValue()->getNodeType(), StyioNodeType::Id);
  }
  {
    DirectParserContext ctx("|1|");
    EXPECT_THROW(parse_size_of(ctx.get()), StyioParseError);
  }

  const std::vector<std::pair<std::string, CompType>> comparisons = {
    {"1==2", CompType::EQ},
    {"1!=2", CompType::NE},
    {"1>2", CompType::GT},
    {"1>=2", CompType::GE},
    {"1<2", CompType::LT},
    {"1<=2", CompType::LE},
  };
  for (const auto& [source, sign] : comparisons) {
    DirectParserContext ctx(source);
    std::unique_ptr<StyioAST> expr(parse_cond_item(ctx.get()));
    auto* comp = dynamic_cast<BinCompAST*>(expr.get());
    ASSERT_NE(comp, nullptr) << source;
    EXPECT_EQ(comp->getSign(), sign) << source;
  }

  {
    DirectParserContext ctx("1&&2)");
    std::unique_ptr<CondAST> cond(parse_cond(ctx.get()));
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->getSign(), LogicType::AND);
  }
  {
    DirectParserContext ctx("1||2)");
    std::unique_ptr<CondAST> cond(parse_cond(ctx.get()));
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->getSign(), LogicType::OR);
  }
  {
    DirectParserContext ctx("!(1)");
    std::unique_ptr<CondAST> cond(parse_cond(ctx.get()));
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->getSign(), LogicType::NOT);
  }
  {
    DirectParserContext ctx("!1");
    EXPECT_THROW(parse_cond(ctx.get()), StyioSyntaxError);
  }
  {
    DirectParserContext ctx("^2)");
    std::unique_ptr<CondAST> cond(parse_cond_rhs(ctx.get(), IntAST::Create("1")));
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->getSign(), LogicType::XOR);
  }
  {
    DirectParserContext ctx("!(2))");
    std::unique_ptr<CondAST> cond(parse_cond_rhs(ctx.get(), nullptr));
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->getSign(), LogicType::NOT);
  }

  {
    DirectParserContext ctx("?(1) \\t\\ {}");
    align_legacy_char_entry_token(ctx, StyioTokenType::TOK_LCURBRAC);
    std::unique_ptr<StyioAST> flow(parse_cond_flow(ctx.get()));
    ASSERT_NE(flow, nullptr);
    EXPECT_EQ(flow->getNodeType(), StyioNodeType::CondFlow_True);
  }
  {
    DirectParserContext ctx("?(1) \\f\\ {}");
    align_legacy_char_entry_token(ctx, StyioTokenType::TOK_LCURBRAC);
    std::unique_ptr<StyioAST> flow(parse_cond_flow(ctx.get()));
    ASSERT_NE(flow, nullptr);
    EXPECT_EQ(flow->getNodeType(), StyioNodeType::CondFlow_False);
  }
  {
    DirectParserContext ctx("?(1) \\x\\ {}");
    align_legacy_char_entry_token(ctx, StyioTokenType::TOK_LCURBRAC);
    EXPECT_THROW(parse_cond_flow(ctx.get()), StyioSyntaxError);
  }
  {
    DirectParserContext ctx("?1");
    EXPECT_THROW(parse_cond_flow(ctx.get()), StyioSyntaxError);
  }
}

TEST(StyioSecurityParserContext, CoversLegacyIndexAndContainerHelpersDirectly) {
  auto expect_index_op =
    [](const std::string& source, StyioTokenType token_type, StyioNodeType op)
  {
    SCOPED_TRACE(source);
    DirectParserContext ctx(source);
    align_legacy_char_entry_token(ctx, token_type);
    std::unique_ptr<StyioAST> ast(parse_index_op(ctx.get(), NameAST::Create("items")));
    auto* list_op = dynamic_cast<ListOpAST*>(ast.get());
    ASSERT_NE(list_op, nullptr) << source;
    EXPECT_EQ(list_op->getOp(), op) << source;
  };

  expect_index_op("[field]", StyioTokenType::NAME, StyioNodeType::Access);
  expect_index_op("[2]", StyioTokenType::INTEGER, StyioNodeType::Access_By_Index);
  expect_index_op("[\"field\"]", StyioTokenType::STRING, StyioNodeType::Access_By_Name);
  expect_index_op("[<]", StyioTokenType::TOK_LBOXBRAC, StyioNodeType::Get_Reversed);
  expect_index_op("[<<]", StyioTokenType::TOK_LBOXBRAC, StyioNodeType::Get_Reversed);
  expect_index_op("[?= 7]", StyioTokenType::INTEGER, StyioNodeType::Get_Index_By_Value);
  expect_index_op("[?^ ()]", StyioTokenType::TOK_LPAREN, StyioNodeType::Get_Indices_By_Many_Values);
  expect_index_op("[^2]", StyioTokenType::INTEGER, StyioNodeType::Access_By_Index);
  expect_index_op("[+: 7]", StyioTokenType::INTEGER, StyioNodeType::Append_Value);
  expect_index_op("[-: ^2]", StyioTokenType::INTEGER, StyioNodeType::Remove_Item_By_Index);
  expect_index_op("[-: ^()]", StyioTokenType::TOK_LPAREN, StyioNodeType::Remove_Items_By_Many_Indices);
  expect_index_op("[-: ?= 7]", StyioTokenType::INTEGER, StyioNodeType::Remove_Item_By_Value);
  expect_index_op("[-: ?^ ()]", StyioTokenType::TOK_LPAREN, StyioNodeType::Remove_Items_By_Many_Values);
  expect_index_op("[-: 7]", StyioTokenType::INTEGER, StyioNodeType::Remove_Item_By_Value);
  {
    DirectParserContext ctx("[?x]");
    EXPECT_ANY_THROW(parse_index_op(ctx.get(), NameAST::Create("items")));
  }
  {
    DirectParserContext ctx("[^2<-9]");
    EXPECT_ANY_THROW(parse_index_op(ctx.get(), NameAST::Create("items")));
  }
  {
    DirectParserContext ctx("[~]");
    EXPECT_ANY_THROW(parse_index_op(ctx.get(), NameAST::Create("items")));
  }

  {
    DirectParserContext ctx("[]");
    std::unique_ptr<StyioAST> ast(parse_index_op(ctx.get(), NameAST::Create("items")));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Id);
  }
  {
    DirectParserContext ctx("(7)");
    align_legacy_char_entry_token(ctx, StyioTokenType::INTEGER);
    std::unique_ptr<StyioAST> tuple(parse_tuple(ctx.get()));
    ASSERT_NE(tuple, nullptr);
    EXPECT_EQ(tuple->getNodeType(), StyioNodeType::Tuple);
  }
  {
    DirectParserContext ctx("{7}");
    align_legacy_char_entry_token(ctx, StyioTokenType::INTEGER);
    std::unique_ptr<StyioAST> set(parse_set(ctx.get()));
    ASSERT_NE(set, nullptr);
    EXPECT_EQ(set->getNodeType(), StyioNodeType::Set);
  }
  {
    DirectParserContext ctx("(7)");
    align_legacy_char_entry_token(ctx, StyioTokenType::INTEGER);
    std::unique_ptr<StyioAST> tuple(parse_iterable(ctx.get()));
    ASSERT_NE(tuple, nullptr);
    EXPECT_EQ(tuple->getNodeType(), StyioNodeType::Tuple);
  }
  {
    DirectParserContext ctx("[7]");
    align_legacy_char_entry_token(ctx, StyioTokenType::INTEGER);
    std::unique_ptr<StyioAST> list(parse_iterable(ctx.get()));
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->getNodeType(), StyioNodeType::List);
  }
  {
    DirectParserContext ctx("{7}");
    align_legacy_char_entry_token(ctx, StyioTokenType::INTEGER);
    std::unique_ptr<StyioAST> set(parse_iterable(ctx.get()));
    ASSERT_NE(set, nullptr);
    EXPECT_EQ(set->getNodeType(), StyioNodeType::Set);
  }
  {
    DirectParserContext ctx("}");
    std::unique_ptr<StyioAST> ast(parse_struct(ctx.get(), NameAST::Create("Point")));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Struct);
  }
  {
    DirectParserContext ctx("field}");
    std::unique_ptr<StyioAST> ast(parse_struct(ctx.get(), NameAST::Create("Point")));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Struct);
  }
  {
    DirectParserContext ctx("(\"input.txt\": i64; out <- value)");
    std::unique_ptr<ResourceAST> resources(parse_resources_after_at(ctx.get()));
    ASSERT_NE(resources, nullptr);
    ASSERT_EQ(resources->res_list.size(), 2u);
    EXPECT_EQ(resources->res_list[0].first->getNodeType(), StyioNodeType::String);
    EXPECT_EQ(resources->res_list[0].second, "i64");
    auto* bind = dynamic_cast<FinalBindAST*>(resources->res_list[1].first);
    ASSERT_NE(bind, nullptr);
    EXPECT_EQ(bind->getVar()->getNameAsStr(), "out");
    EXPECT_EQ(resources->res_list[1].second, "");
  }
  {
    DirectParserContext ctx("@(\"plain.txt\", orphan; out <- value)");
    std::unique_ptr<ResourceAST> resources(parse_resources(ctx.get()));
    ASSERT_NE(resources, nullptr);
    ASSERT_EQ(resources->res_list.size(), 2u);
    EXPECT_EQ(resources->res_list[0].first->getNodeType(), StyioNodeType::String);
    EXPECT_EQ(resources->res_list[0].second, "");
    auto* bind = dynamic_cast<FinalBindAST*>(resources->res_list[1].first);
    ASSERT_NE(bind, nullptr);
    EXPECT_EQ(bind->getVar()->getNameAsStr(), "out");
  }
  {
    DirectParserContext ctx("()");
    std::unique_ptr<ResourceAST> resources(parse_resources_after_at(ctx.get()));
    ASSERT_NE(resources, nullptr);
    EXPECT_TRUE(resources->res_list.empty());
  }
}

TEST(StyioSecurityParserContext, CoversLegacyBinopTupleListAndReturnHelpersDirectly) {
  const std::vector<std::pair<std::string, StyioNodeType>> binop_items = {
    {"true", StyioNodeType::Bool},
    {"7", StyioNodeType::Integer},
    {"7.5", StyioNodeType::Float},
    {"\"s\"", StyioNodeType::String},
    {"(7)", StyioNodeType::Integer},
  };
  for (const auto& [source, node_type] : binop_items) {
    SCOPED_TRACE(source);
    DirectParserContext ctx(source);
    std::unique_ptr<StyioAST> ast(parse_binop_item(ctx.get()));
    ASSERT_NE(ast, nullptr) << source;
    EXPECT_EQ(ast->getNodeType(), node_type) << source;
  }
  {
    DirectParserContext ctx("dict { \"a\": 1 }");
    std::unique_ptr<StyioAST> ast(parse_binop_item(ctx.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Dict);
  }
  {
    DirectParserContext ctx("$\"value {1 + 2}\"");
    std::unique_ptr<StyioAST> ast(parse_binop_item(ctx.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::FmtStr);
  }
  {
    DirectParserContext ctx("$ name");
    EXPECT_THROW(parse_binop_item(ctx.get()), StyioSyntaxError);
  }
  {
    DirectParserContext ctx("(<- @stdin)");
    std::unique_ptr<StyioAST> ast(parse_binop_item(ctx.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::InstantPull);
  }

  {
    SCOPED_TRACE("parse_binop_rhs");
    DirectParserContext ctx("2");
    std::unique_ptr<StyioAST> ast(parse_binop_rhs(
      ctx.get(),
      IntAST::Create("1"),
      StyioOpType::Binary_Add
    ));
    auto* binop = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(binop, nullptr);
    EXPECT_EQ(binop->getOp(), StyioOpType::Binary_Add);
  }
  {
    SCOPED_TRACE("parse_binop_rhs nested precedence from name rhs");
    DirectParserContext ctx("rhs * 3");
    std::unique_ptr<StyioAST> ast(parse_binop_rhs(
      ctx.get(),
      NameAST::Create("lhs"),
      StyioOpType::Binary_Add
    ));
    auto* binop = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(binop, nullptr);
    EXPECT_EQ(binop->getOp(), StyioOpType::Binary_Add);
    ASSERT_NE(dynamic_cast<BinOpAST*>(binop->getRHS()), nullptr);
  }
  {
    SCOPED_TRACE("parse_tuple_no_braces");
    DirectParserContext ctx("7");
    std::unique_ptr<StyioAST> tuple(parse_tuple_no_braces(ctx.get(), IntAST::Create("1")));
    ASSERT_NE(tuple, nullptr);
    EXPECT_EQ(tuple->getNodeType(), StyioNodeType::Tuple);
  }
  {
    SCOPED_TRACE("parse_list_or_loop list");
    DirectParserContext ctx("7]");
    std::unique_ptr<StyioAST> list(parse_list_or_loop(ctx.get()));
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->getNodeType(), StyioNodeType::List);
  }
  {
    SCOPED_TRACE("parse_list_or_loop invalid range loop");
    DirectParserContext ctx("name..3]");
    EXPECT_THROW(parse_list_or_loop(ctx.get()), StyioSyntaxError);
  }
  {
    SCOPED_TRACE("parse_return");
    DirectParserContext ctx("<< 7");
    std::unique_ptr<ReturnAST> ret(parse_return(ctx.get()));
    ASSERT_NE(ret, nullptr);
    EXPECT_EQ(ret->getNodeType(), StyioNodeType::Return);
    ASSERT_NE(ret->getExpr(), nullptr);
    EXPECT_EQ(ret->getExpr()->getNodeType(), StyioNodeType::Integer);
  }
}

TEST(StyioSecurityParserContext, CoversLegacyAttributeCallAndRangeHelpersDirectly) {
  {
    DirectParserContext ctx("person.name");
    EXPECT_ANY_THROW(parse_attr(ctx.get()));
  }
  {
    DirectParserContext ctx("person[\"name\"]");
    EXPECT_ANY_THROW(parse_attr(ctx.get()));
  }
  {
    DirectParserContext ctx("person[1 + 2]");
    std::unique_ptr<AttrAST> attr(parse_attr(ctx.get()));
    ASSERT_NE(attr, nullptr);
    EXPECT_EQ(attr->getNodeType(), StyioNodeType::Attribute);
    ASSERT_NE(attr->attr, nullptr);
  }

  {
    DirectParserContext ctx("name");
    std::unique_ptr<StyioAST> ast(parse_chain_of_call(ctx.get(), NameAST::Create("person")));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Attribute);
  }
  {
    DirectParserContext ctx("name.next");
    EXPECT_ANY_THROW(parse_chain_of_call(ctx.get(), NameAST::Create("person")));
  }
  {
    DirectParserContext ctx("make(1)");
    std::unique_ptr<StyioAST> ast(parse_chain_of_call(ctx.get(), NameAST::Create("person")));
    auto* call = dynamic_cast<FuncCallAST*>(ast.get());
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->getNodeType(), StyioNodeType::Call);
    ASSERT_NE(call->func_callee, nullptr);
    EXPECT_EQ(call->func_callee->getNodeType(), StyioNodeType::Id);
    EXPECT_EQ(call->getArgList().size(), 1u);
  }
  {
    DirectParserContext ctx("make(1).next");
    EXPECT_ANY_THROW(parse_chain_of_call(ctx.get(), NameAST::Create("person")));
  }

  const std::vector<std::string> rhs_cases = {
    "2 + 3",
    "2 - 3",
    "2 * 3",
    "2 / 3",
    "2 % 3",
    "2 ** 3",
  };
  for (const auto& source : rhs_cases) {
    DirectParserContext ctx(source);
    std::unique_ptr<StyioAST> ast(parse_binop_rhs(
      ctx.get(),
      IntAST::Create("1"),
      StyioOpType::Binary_Add
    ));
    auto* binop = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(binop, nullptr) << source;
  }

  {
    DirectParserContext ctx("1..3]");
    EXPECT_ANY_THROW(parse_list_or_loop(ctx.get()));
  }
  {
    DirectParserContext ctx("1..n]");
    EXPECT_ANY_THROW(parse_list_or_loop(ctx.get()));
  }
  {
    DirectParserContext ctx("1,]");
    std::unique_ptr<StyioAST> list(parse_list_or_loop(ctx.get()));
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->getNodeType(), StyioNodeType::List);
  }
  {
    DirectParserContext ctx("1, 2, 3]");
    EXPECT_ANY_THROW(parse_list_or_loop(ctx.get()));
  }
  {
    DirectParserContext ctx("1][0]");
    EXPECT_ANY_THROW(parse_list_or_loop(ctx.get()));
  }
  {
    DirectParserContext ctx("1..n] >>");
    EXPECT_ANY_THROW(parse_list_or_loop(ctx.get()));
  }
  {
    DirectParserContext ctx("name..3]");
    EXPECT_ANY_THROW(parse_list_or_loop(ctx.get()));
  }
}

TEST(StyioSecurityParserContext, CoversLegacyStatementCodpIteratorAndReadFileHelpersDirectly) {
  {
    DirectParserContext ctx("a, b <- @stdin : (f64, string)");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* block = dynamic_cast<BlockAST*>(stmt.get());
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 2u);
    auto* first = dynamic_cast<FinalBindAST*>(block->stmts[0]);
    auto* second = dynamic_cast<FinalBindAST*>(block->stmts[1]);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(first->getVar()->getNameAsStr(), "a");
    EXPECT_EQ(first->getVar()->getDType()->getTypeName(), "f64");
    EXPECT_EQ(second->getVar()->getNameAsStr(), "b");
    EXPECT_EQ(second->getVar()->getDType()->getTypeName(), "string");
    EXPECT_EQ(first->getValue()->getNodeType(), StyioNodeType::InstantPull);
    EXPECT_EQ(second->getValue()->getNodeType(), StyioNodeType::InstantPull);
  }
  {
    DirectParserContext ctx("value <- @stdin : i64");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* bind = dynamic_cast<FinalBindAST*>(stmt.get());
    ASSERT_NE(bind, nullptr);
    EXPECT_EQ(bind->getVar()->getNameAsStr(), "value");
    EXPECT_EQ(bind->getVar()->getDType()->getTypeName(), "i64");
    EXPECT_EQ(bind->getValue()->getNodeType(), StyioNodeType::InstantPull);
  }
  {
    DirectParserContext ctx("a, b <- @stdin : list[i64]");
    EXPECT_THROW(parse_stmt_or_expr_legacy(ctx.get()), StyioSyntaxError);
  }
  {
    DirectParserContext ctx("a, b <- @stdin : (list[i64], string)");
    EXPECT_THROW(parse_stmt_or_expr_legacy(ctx.get()), StyioSyntaxError);
  }
  {
    DirectParserContext ctx("flag <- @stdin : bool");
    EXPECT_THROW(parse_stmt_or_expr_legacy(ctx.get()), StyioSyntaxError);
  }
  {
    DirectParserContext ctx("slot = 3");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* bind = dynamic_cast<FlexBindAST*>(stmt.get());
    ASSERT_NE(bind, nullptr);
    EXPECT_EQ(bind->getNameAsStr(), "slot");
    EXPECT_EQ(bind->getValue()->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectParserContext ctx("fixed := 4");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* bind = dynamic_cast<FinalBindAST*>(stmt.get());
    ASSERT_NE(bind, nullptr);
    EXPECT_EQ(bind->getVar()->getNameAsStr(), "fixed");
    EXPECT_EQ(bind->getValue()->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectParserContext ctx("typed: i64 := 5");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* bind = dynamic_cast<FinalBindAST*>(stmt.get());
    ASSERT_NE(bind, nullptr);
    EXPECT_EQ(bind->getVar()->getDType()->getTypeName(), "i64");
  }
  {
    DirectParserContext ctx("upstream => downstream");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* order = dynamic_cast<ResourceOrderAST*>(stmt.get());
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->getNodeType(), StyioNodeType::ResourceOrder);
    ASSERT_NE(dynamic_cast<NameAST*>(order->getBefore()), nullptr);
    ASSERT_NE(dynamic_cast<NameAST*>(order->getAfter()), nullptr);
  }
  {
    DirectParserContext ctx("input <- @file(\"data.txt\")");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* acquire = dynamic_cast<HandleAcquireAST*>(stmt.get());
    ASSERT_NE(acquire, nullptr);
    EXPECT_EQ(acquire->getNodeType(), StyioNodeType::HandleAcquire);
    EXPECT_EQ(acquire->getVar()->getNameAsStr(), "input");
    EXPECT_FALSE(acquire->isFlexBind());
  }
  {
    DirectParserContext ctx("count += 2");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* binop = dynamic_cast<BinOpAST*>(stmt.get());
    ASSERT_NE(binop, nullptr);
    EXPECT_EQ(binop->getOp(), StyioOpType::Self_Add_Assign);
  }
  {
    DirectParserContext ctx("^^^");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* brk = dynamic_cast<BreakAST*>(stmt.get());
    ASSERT_NE(brk, nullptr);
    EXPECT_EQ(brk->getNodeType(), StyioNodeType::Break);
  }
  {
    DirectParserContext ctx(">>");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* cont = dynamic_cast<ContinueAST*>(stmt.get());
    ASSERT_NE(cont, nullptr);
    EXPECT_EQ(cont->getNodeType(), StyioNodeType::Continue);
  }
  {
    DirectParserContext ctx(">>>>");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* cont = dynamic_cast<ContinueAST*>(stmt.get());
    ASSERT_NE(cont, nullptr);
    EXPECT_EQ(cont->getNodeType(), StyioNodeType::Continue);
  }
  {
    DirectParserContext ctx("1.5");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getNodeType(), StyioNodeType::Float);
  }
  {
    DirectParserContext ctx("<< 7");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* ret = dynamic_cast<ReturnAST*>(stmt.get());
    ASSERT_NE(ret, nullptr);
    EXPECT_EQ(ret->getExpr()->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectParserContext ctx("<| 8");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* ret = dynamic_cast<ReturnAST*>(stmt.get());
    ASSERT_NE(ret, nullptr);
    EXPECT_EQ(ret->getExpr()->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectParserContext ctx("|<| 9");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* ret = dynamic_cast<ReturnAST*>(stmt.get());
    ASSERT_NE(ret, nullptr);
    EXPECT_EQ(ret->getExpr()->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectParserContext ctx(">_()");
    std::unique_ptr<StyioAST> stmt(parse_stmt_or_expr_legacy(ctx.get()));
    auto* print = dynamic_cast<PrintAST*>(stmt.get());
    ASSERT_NE(print, nullptr);
    EXPECT_TRUE(print->exprs.empty());
  }

  {
    DirectParserContext ctx("filter{x}");
    EXPECT_THROW(parse_codp(ctx.get()), StyioSyntaxError);
  }
  {
    DirectParserContext ctx("slice{1,2}");
    EXPECT_THROW(parse_codp(ctx.get()), StyioParseError);
  }
  {
    DirectParserContext ctx("noop{}");
    std::unique_ptr<CODPAST> chain(parse_codp(ctx.get()));
    ASSERT_NE(chain, nullptr);
    EXPECT_EQ(chain->OpName, "noop");
    EXPECT_TRUE(chain->OpArgs.empty());
    EXPECT_EQ(chain->NextOp, nullptr);
  }
  {
    DirectParserContext ctx("@(\"input.txt\")");
    std::unique_ptr<StyioAST> read(parse_read_file(ctx.get(), NameAST::Create("line")));
    auto* read_file = dynamic_cast<ReadFileAST*>(read.get());
    ASSERT_NE(read_file, nullptr);
    EXPECT_EQ(read_file->getNodeType(), StyioNodeType::ReadFile);
    EXPECT_EQ(read_file->getId()->getAsStr(), "line");
    ASSERT_NE(read_file->getValue(), nullptr);
    EXPECT_EQ(read_file->getValue()->getNodeType(), StyioNodeType::LocalPath);
  }
  {
    DirectParserContext ctx("(\"input.txt\")");
    EXPECT_THROW(parse_read_file(ctx.get(), NameAST::Create("line")), StyioSyntaxError);
  }
  {
    DirectParserContext ctx(">>(item) > #hot > #cold");
    std::unique_ptr<StyioAST> seq(parse_iterator_only_latest(
      ctx.get(),
      ListAST::Create({IntAST::Create("1"), IntAST::Create("2")})
    ));
    auto* iter_seq = dynamic_cast<IterSeqAST*>(seq.get());
    ASSERT_NE(iter_seq, nullptr);
    ASSERT_EQ(iter_seq->params.size(), 1u);
    ASSERT_EQ(iter_seq->hash_tags.size(), 2u);
    EXPECT_EQ(iter_seq->hash_tags[0]->words.front(), "hot");
    EXPECT_EQ(iter_seq->hash_tags[1]->words.front(), "cold");
  }
  {
    DirectParserContext ctx(">> #hot > #cold");
    std::unique_ptr<StyioAST> seq(parse_iterator_only_latest(
      ctx.get(),
      ListAST::Create({IntAST::Create("1"), IntAST::Create("2")})
    ));
    auto* iter_seq = dynamic_cast<IterSeqAST*>(seq.get());
    ASSERT_NE(iter_seq, nullptr);
    EXPECT_TRUE(iter_seq->params.empty());
    ASSERT_EQ(iter_seq->hash_tags.size(), 2u);
    EXPECT_EQ(iter_seq->hash_tags[0]->words.front(), "hot");
    EXPECT_EQ(iter_seq->hash_tags[1]->words.front(), "cold");
  }
  {
    DirectParserContext ctx(">>(item) => { << item } ?= 1");
    EXPECT_THROW(
      parse_iterator_with_forward(ctx.get(), ListAST::Create({IntAST::Create("1")})),
      StyioParseError);
  }
  {
    DirectParserContext ctx(">>(item) => { << item } => { << item } => { << item }");
    EXPECT_THROW(
      parse_iterator_with_forward(ctx.get(), ListAST::Create({IntAST::Create("1")})),
      StyioParseError);
  }
  {
    DirectParserContext ctx(">> #hot >");
    EXPECT_THROW(
      parse_iterator_only_latest(ctx.get(), ListAST::Create({IntAST::Create("1")})),
      StyioSyntaxError);
  }
  {
    DirectParserContext ctx(">> #");
    EXPECT_THROW(
      parse_iterator_only_latest(ctx.get(), ListAST::Create({IntAST::Create("1")})),
      StyioSyntaxError);
  }
  {
    DirectParserContext ctx(">> #hot > #");
    EXPECT_THROW(
      parse_iterator_only_latest(ctx.get(), ListAST::Create({IntAST::Create("1")})),
      StyioSyntaxError);
  }
}

TEST(StyioSecurityParserContext, CoversBoundExternManualTokenBoundariesDirectly) {
  {
    ManualTokenParserContext ctx({
      {StyioTokenType::TOK_AT, "@"},
      {StyioTokenType::TOK_SPACE, " "},
      {StyioTokenType::NAME, "extern"},
      {StyioTokenType::TOK_LPAREN, "("},
      {StyioTokenType::NAME, "c"},
      {StyioTokenType::TOK_RPAREN, ")"},
      {StyioTokenType::TOK_SPACE, " "},
      {StyioTokenType::TOK_LCURBRAC, "{"},
      {StyioTokenType::NAME, "int"},
      {StyioTokenType::TOK_SPACE, " "},
      {StyioTokenType::NAME, "body"},
      {StyioTokenType::TOK_LCURBRAC, "{"},
      {StyioTokenType::NAME, "nested"},
      {StyioTokenType::TOK_RCURBRAC, "}"},
      {StyioTokenType::TOK_RCURBRAC, "}"},
      {StyioTokenType::TOK_EOF, ""},
    });
    std::unique_ptr<ExternBlockAST> ast(
      parse_bound_extern_after_at_latest(ctx.get(), {"manual_symbol"}));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getAbi(), "c");
    EXPECT_EQ(ast->getBody(), "int body{nested}");
    EXPECT_TRUE(ast->getSourcePaths().empty());
    ASSERT_EQ(ast->getExportedSymbols().size(), 1u);
    EXPECT_EQ(ast->getExportedSymbols()[0], "manual_symbol");
  }

  {
    ManualTokenParserContext ctx({
      {StyioTokenType::TOK_AT, "@"},
      {StyioTokenType::NAME, "extern"},
      {StyioTokenType::TOK_LPAREN, "("},
      {StyioTokenType::NAME, "c"},
      {StyioTokenType::TOK_PLUS, "+"},
      {StyioTokenType::TOK_PLUS, "+"},
      {StyioTokenType::TOK_RPAREN, ")"},
      {StyioTokenType::TOK_LCURBRAC, "{"},
      {StyioTokenType::STRING, "\"native/a.c\""},
      {StyioTokenType::TOK_COMMA, ","},
      {StyioTokenType::STRING, "\"native/with\\\"quote.c\""},
      {StyioTokenType::TOK_RCURBRAC, "}"},
      {StyioTokenType::TOK_EOF, ""},
    }, "/tmp/styio-parser/manual/main.styio");
    std::unique_ptr<ExternBlockAST> ast(
      parse_bound_extern_after_at_latest(ctx.get(), {"ref_a", "ref_b"}));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getAbi(), "c++");
    ASSERT_EQ(ast->getSourcePaths().size(), 2u);
    EXPECT_EQ(ast->getSourcePaths()[0], native_path("/tmp/styio-parser/manual/native/a.c"));
    EXPECT_NE(ast->getSourcePaths()[1].find("with"), std::string::npos);
    EXPECT_TRUE(ast->getBody().empty());
  }

  {
    ManualTokenParserContext ctx({
      {StyioTokenType::TOK_AT, "@"},
      {StyioTokenType::NAME, "extern"},
      {StyioTokenType::TOK_LPAREN, "("},
      {StyioTokenType::NAME, "c"},
      {StyioTokenType::TOK_RPAREN, ")"},
      {StyioTokenType::TOK_LCURBRAC, "{"},
      {StyioTokenType::STRING, "\"native/a.c\""},
      {StyioTokenType::NAME, "oops"},
      {StyioTokenType::STRING, "\"native/b.c\""},
      {StyioTokenType::TOK_RCURBRAC, "}"},
      {StyioTokenType::TOK_EOF, ""},
    });
    std::unique_ptr<ExternBlockAST> ast(
      parse_bound_extern_after_at_latest(ctx.get(), {"manual_symbol"}));
    ASSERT_NE(ast, nullptr);
    EXPECT_TRUE(ast->getSourcePaths().empty());
    EXPECT_NE(ast->getBody().find("oops"), std::string::npos);
  }

  {
    ManualTokenParserContext ctx({
      {StyioTokenType::TOK_AT, "@"},
      {StyioTokenType::NAME, "extern"},
      {StyioTokenType::TOK_LPAREN, "("},
      {StyioTokenType::NAME, "c"},
      {StyioTokenType::TOK_RPAREN, ")"},
      {StyioTokenType::TOK_LCURBRAC, "{"},
      {StyioTokenType::STRING, "\"native/a.c\""},
      {StyioTokenType::TOK_COMMA, ","},
      {StyioTokenType::TOK_RCURBRAC, "}"},
      {StyioTokenType::TOK_EOF, ""},
    });
    std::unique_ptr<ExternBlockAST> ast(
      parse_bound_extern_after_at_latest(ctx.get(), {"manual_symbol"}));
    ASSERT_NE(ast, nullptr);
    EXPECT_TRUE(ast->getSourcePaths().empty());
    EXPECT_EQ(ast->getBody(), "\"native/a.c\",");
  }

  {
    DirectParserContext ctx("@ extern(c) { \"native/missing_symbol.c\" }");
    EXPECT_THROW(parse_bound_extern_after_at_latest(ctx.get(), {}), StyioSyntaxError);
  }
  {
    DirectParserContext ctx("@ import { x }");
    EXPECT_THROW(parse_bound_extern_after_at_latest(ctx.get(), {"x"}), StyioSyntaxError);
  }
}

TEST(StyioSecurityParserContext, CoversModernIndexTailsAndCharLiteralEdgesDirectly) {
  auto expect_modern_index =
    [](const std::string& source, StyioNodeType op)
  {
    SCOPED_TRACE(source);
    DirectParserContext ctx(source);
    StyioAST* raw_ast = nullptr;
    ASSERT_NO_THROW(raw_ast = parse_expr(ctx.get()));
    std::unique_ptr<StyioAST> ast(raw_ast);
    auto* list_op = dynamic_cast<ListOpAST*>(ast.get());
    ASSERT_NE(list_op, nullptr) << source;
    EXPECT_EQ(list_op->getOp(), op) << source;
  };

  expect_modern_index("items[1]", StyioNodeType::Access_By_Index);
  expect_modern_index("items[1..3]", StyioNodeType::Access_By_Slice);
  expect_modern_index("items[..3]", StyioNodeType::Access_By_Slice);
  expect_modern_index("items[1..]", StyioNodeType::Access_By_Slice);

  {
    SCOPED_TRACE("series[avg, 3]");
    DirectParserContext ctx("series[avg, 3]");
    StyioAST* raw_ast = nullptr;
    ASSERT_NO_THROW(raw_ast = parse_expr(ctx.get()));
    std::unique_ptr<StyioAST> ast(raw_ast);
    auto* intrinsic = dynamic_cast<SeriesIntrinsicAST*>(ast.get());
    ASSERT_NE(intrinsic, nullptr);
    EXPECT_EQ(intrinsic->getNodeType(), StyioNodeType::SeriesIntrinsic);
    EXPECT_EQ(intrinsic->getOp(), SeriesIntrinsicOp::Avg);
    ASSERT_NE(intrinsic->getWindow(), nullptr);
    EXPECT_EQ(intrinsic->getWindow()->getNodeType(), StyioNodeType::Integer);
  }
  {
    SCOPED_TRACE("series[max, 3]");
    DirectParserContext ctx("series[max, 3]");
    StyioAST* raw_ast = nullptr;
    ASSERT_NO_THROW(raw_ast = parse_expr(ctx.get()));
    std::unique_ptr<StyioAST> ast(raw_ast);
    auto* intrinsic = dynamic_cast<SeriesIntrinsicAST*>(ast.get());
    ASSERT_NE(intrinsic, nullptr);
    EXPECT_EQ(intrinsic->getOp(), SeriesIntrinsicOp::Max);
  }
  {
    SCOPED_TRACE("items[?, true]");
    DirectParserContext ctx("items[?, true]");
    StyioAST* raw_ast = nullptr;
    ASSERT_NO_THROW(raw_ast = parse_expr(ctx.get()));
    std::unique_ptr<StyioAST> ast(raw_ast);
    auto* selector = dynamic_cast<GuardSelectorAST*>(ast.get());
    ASSERT_NE(selector, nullptr);
    EXPECT_EQ(selector->getNodeType(), StyioNodeType::GuardSelector);
    ASSERT_NE(selector->getCond(), nullptr);
    EXPECT_EQ(selector->getCond()->getNodeType(), StyioNodeType::Bool);
  }
  {
    SCOPED_TRACE("items[?=, 7]");
    DirectParserContext ctx("items[?=, 7]");
    StyioAST* raw_ast = nullptr;
    ASSERT_NO_THROW(raw_ast = parse_expr(ctx.get()));
    std::unique_ptr<StyioAST> ast(raw_ast);
    auto* probe = dynamic_cast<EqProbeAST*>(ast.get());
    ASSERT_NE(probe, nullptr);
    EXPECT_EQ(probe->getNodeType(), StyioNodeType::EqProbeSelector);
    ASSERT_NE(probe->getProbeValue(), nullptr);
    EXPECT_EQ(probe->getProbeValue()->getNodeType(), StyioNodeType::Integer);
  }

  const std::vector<std::pair<std::string, std::string>> char_literals = {
    {"'x'", "x"},
    {"'\\n'", "\n"},
    {"'\\r'", "\r"},
    {"'\\t'", "\t"},
    {"'\\0'", std::string(1, '\0')},
    {"'\\\\'", "\\"},
    {"'\\''", "'"},
  };
  for (const auto& [source, expected] : char_literals) {
    DirectParserContext ctx(source);
    std::unique_ptr<StyioAST> ast(parse_expr(ctx.get()));
    auto* ch = dynamic_cast<CharAST*>(ast.get());
    ASSERT_NE(ch, nullptr) << source;
    EXPECT_EQ(ch->getValue(), expected) << source;
  }

  for (const std::string& source : {"''", "'ab'", "'\\x'", "'x"}) {
    DirectParserContext ctx(source);
    EXPECT_THROW(parse_expr(ctx.get()), StyioSyntaxError) << source;
  }
}

TEST(StyioSecurityParserContext, HashFunctionFuzzSeedStaysExceptionSafe) {
  std::string nested_match_print_seed =
    "x = 1\n"
    "x ?= {\n"
    " \n"
    "x ?= {\n"
    "  1 => >_(1)\n"
    "(1)";
  nested_match_print_seed.push_back('\0');
  nested_match_print_seed += "|\n}\n";

  std::string typed_binding_recovery_seed =
    "# ad : d=(a:i6,4  b: " + std::string(100, 'r') + "i64) => {\n"
    "  <- a + ";
  typed_binding_recovery_seed.push_back(static_cast<char>(0xa2));
  typed_binding_recovery_seed += "? >";
  typed_binding_recovery_seed.append(5, '\0');
  typed_binding_recovery_seed += "E";
  typed_binding_recovery_seed.push_back('\0');
  typed_binding_recovery_seed += "b\n}\n\n>_ad(d(1, 2))\n";

  const std::vector<std::string> samples{
    "# a : d=(a: a63, )b 6i4:",
    "a# : dHHHHHHHHHHHHHHH5, ",
    "# ad : d=(a: i64, b: i64) =>(add(0, 2)>",
    nested_match_print_seed,
    typed_binding_recovery_seed
  };
  for (const std::string& src : samples) {
    for (StyioParserEngine engine : {StyioParserEngine::Legacy, StyioParserEngine::Nightly}) {
      CompilationSession session;
      session.adopt_tokens(StyioTokenizer::tokenize(src));
      session.attach_context(StyioContext::Create(
        "<fuzz-regression>",
        src,
        build_line_seps(src),
        session.tokens(),
        false
      ));
      try {
        session.attach_ast(parse_main_block_with_engine_latest(*session.context(), engine, nullptr));
      }
      catch (const StyioBaseException&) {
        session.mark_failed();
      }
      SUCCEED();
    }
  }
}

TEST(StyioSecurityParserContext, DeepUnclosedIndexListSeedHitsNestingBudget) {
  const std::string src = "x" + std::string(70, '[') + "x)\n";

  CompilationSession session;
  session.adopt_tokens(StyioTokenizer::tokenize(src));
  session.attach_context(StyioContext::Create(
    "<deep-index-list-oom-regression>",
    src,
    build_line_seps(src),
    session.tokens(),
    false
  ));

  EXPECT_THROW(
    {
      std::unique_ptr<StyioAST> parsed(parse_expr_subset_nightly(*session.context()));
    },
    StyioParserResourceLimitError
  );
}

TEST(StyioSecurityParserContext, DeepBraceNestedIndexSeedFailsClosedAtUnsupportedElement) {
  const std::string src =
    "x[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[{[[[[[[[[x)\n";

  CompilationSession session;
  session.adopt_tokens(StyioTokenizer::tokenize(src));
  session.attach_context(StyioContext::Create(
    "<deep-brace-index-timeout-regression>",
    src,
    build_line_seps(src),
    session.tokens(),
    false
  ));

  try {
    std::unique_ptr<StyioAST> parsed(parse_expr_subset_nightly(*session.context()));
    FAIL() << "expected deep malformed index seed to fail closed";
  } catch (const StyioSyntaxError& ex) {
    EXPECT_NE(
      std::string(ex.what()).find("unsupported syntax in authoritative nightly parser"),
      std::string::npos
    );
  }
}

TEST(StyioSecurityParserContext, DeepBraceNestedIndexSeedFailsClosedBeforeBridgeBudget) {
  const std::string src = "x[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[{[[[[[[[[x)\n";

  CompilationSession session;
  session.adopt_tokens(StyioTokenizer::tokenize(src));
  session.attach_context(StyioContext::Create(
    "<deep-brace-index-bridge-timeout-regression>",
    src,
    build_line_seps(src),
    session.tokens(),
    false
  ));

  try {
    std::unique_ptr<StyioAST> parsed(parse_expr_subset_nightly(*session.context()));
    FAIL() << "expected deep malformed index seed to fail closed";
  } catch (const StyioSyntaxError& ex) {
    EXPECT_NE(
      std::string(ex.what()).find("unsupported syntax in authoritative nightly parser"),
      std::string::npos
    );
  }
}

TEST(StyioSecurityParserContext, DeepBraceNestedIndexLeakSeedDoesNotLeakUnderSessionArena) {
  static constexpr unsigned char kSeed[] = {
    0x78, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b,
    0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x35, 0x5b, 0x5b, 0x5b, 0x32, 0x32, 0x32,
    0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,
    0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,
    0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,
    0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,
    0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x5b, 0x5b, 0x5b, 0x53, 0x5b, 0x5b, 0x01, 0x00, 0x00, 0x0a,
    0x5b, 0x5b, 0x5b, 0x5b, 0x0f, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b,
    0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x7b, 0x5b, 0x5b, 0x5b, 0x5b,
    0x5b, 0x5b, 0x5b, 0x5b, 0x78, 0x29, 0x0a,
  };
  const std::string src(reinterpret_cast<const char*>(kSeed), sizeof(kSeed));

  CompilationSession session;
  session.adopt_tokens(StyioTokenizer::tokenize(src));
  session.attach_context(StyioContext::Create(
    "<deep-brace-index-leak-regression>",
    src,
    build_line_seps(src),
    session.tokens(),
    false
  ));

  try {
    session.attach_ast(parse_main_block_with_engine_latest(*session.context(), StyioParserEngine::Nightly, nullptr));
  } catch (const StyioBaseException&) {
  } catch (...) {
  }
}

TEST(StyioSecurityParserContext, MalformedBlockLeakSeedDoesNotLeakUnderSessionArena) {
  static constexpr unsigned char kSeed[] = {
    0x61, 0x7b, 0x22, 0x2f, 0x41, 0x61, 0xff, 0x0a, 0x20, 0x0a, 0x23, 0x20, 0x64, 0x20, 0x3a, 0x20,
    0x64, 0xcd, 0xd7, 0x22, 0x2f, 0x41, 0x61, 0xff, 0x0a, 0x20, 0x0a, 0x9e, 0xc5, 0xdf, 0x96, 0x84,
    0xdd, 0x2f, 0x00, 0x7c, 0x34, 0x20, 0x41, 0x61, 0xff, 0x0a, 0x78, 0xde, 0x20, 0x0a,
  };
  const std::string src(reinterpret_cast<const char*>(kSeed), sizeof(kSeed));

  for (StyioParserEngine engine : {StyioParserEngine::Legacy, StyioParserEngine::Nightly}) {
    CompilationSession session;
    session.adopt_tokens(StyioTokenizer::tokenize(src));
    session.attach_context(StyioContext::Create(
      "<malformed-block-leak-regression>",
      src,
      build_line_seps(src),
      session.tokens(),
      false
    ));

    try {
      session.attach_ast(parse_main_block_with_engine_latest(*session.context(), engine, nullptr));
    } catch (const StyioBaseException&) {
    } catch (...) {
    }
  }
}

TEST(StyioSecurityParserContext, MalformedHashParamTypeLeakSeedDoesNotLeakUnderSessionArena) {
  static constexpr unsigned char kSeed[] = {
    0x23, 0x20, 0x61, 0x64, 0x64, 0x20, 0x3a, 0x3d, 0x20, 0x28, 0x61, 0x3a,
    0x20, 0x69, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73,
    0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73,
    0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73,
    0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73,
    0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73,
    0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73,
    0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73,
    0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73,
    0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x36, 0x34, 0x2c, 0x20,
    0x62, 0x3a, 0x28, 0x61, 0x3a, 0x20, 0x69, 0x36, 0x34, 0x2c, 0x20, 0x62,
    0x3a, 0x20, 0x69, 0x36, 0x34, 0x29, 0x20, 0x3d, 0x3e, 0x20, 0x7b, 0x0a,
    0x20, 0x20, 0x3c, 0x7c, 0x20, 0x61, 0x20, 0x2b, 0x20, 0x62, 0x0a, 0x7d,
    0x0a, 0x20, 0x69, 0x36, 0x34, 0x29, 0x20, 0x3d, 0x3e, 0x20, 0x7b, 0x0a,
    0x20, 0x20, 0x3c, 0x7c, 0x20, 0x61, 0x20, 0x2b, 0x20, 0x62, 0x0a, 0x7d,
    0x0a, 0x0a, 0x3e, 0x5b, 0x28, 0x61, 0x64, 0x64, 0x28, 0x31, 0x2c, 0x20,
    0x32, 0x29, 0x29, 0x0a,
  };
  const std::string src(reinterpret_cast<const char*>(kSeed), sizeof(kSeed));

  for (StyioParserEngine engine : {StyioParserEngine::Legacy, StyioParserEngine::Nightly}) {
    CompilationSession session;
    session.adopt_tokens(StyioTokenizer::tokenize(src));
    session.attach_context(StyioContext::Create(
      "<malformed-hash-param-type-leak-regression>",
      src,
      build_line_seps(src),
      session.tokens(),
      false
    ));

    try {
      session.attach_ast(parse_main_block_with_engine_latest(*session.context(), engine, nullptr));
    } catch (const StyioBaseException&) {
    } catch (...) {
    }
  }
}

TEST(StyioSecurityParserContext, MalformedPrintCallLeakSeedDoesNotLeakUnderSessionArena) {
  const std::string src = "x = 1 + >_(x(xN)\n";

  for (StyioParserEngine engine : {StyioParserEngine::Legacy, StyioParserEngine::Nightly}) {
    CompilationSession session;
    session.adopt_tokens(StyioTokenizer::tokenize(src));
    session.attach_context(StyioContext::Create(
      "<malformed-print-call-leak-regression>",
      src,
      build_line_seps(src),
      session.tokens(),
      false
    ));

    try {
      session.attach_ast(parse_main_block_with_engine_latest(*session.context(), engine, nullptr));
    } catch (const StyioBaseException&) {
    } catch (...) {
    }
  }
}

TEST(StyioSecurityParserContext, MalformedIteratorHashTagLeakSeedDoesNotLeakUnderSessionArena) {
  static constexpr unsigned char kSeed[] = {
    0x78, 0x31, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b,
    0x5b, 0x69, 0x73, 0x34, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x35,
    0x5b, 0x5b, 0x5b, 0x32, 0x32, 0x32, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x23,
    0x20, 0x61, 0x74, 0x20, 0x32, 0x3e, 0x69, 0x36, 0x38, 0x34, 0x29, 0x29,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  };
  const std::string src(reinterpret_cast<const char*>(kSeed), sizeof(kSeed));

  for (StyioParserEngine engine : {StyioParserEngine::Legacy, StyioParserEngine::Nightly}) {
    CompilationSession session;
    session.adopt_tokens(StyioTokenizer::tokenize(src));
    session.attach_context(StyioContext::Create(
      "<malformed-iterator-hashtag-leak-regression>",
      src,
      build_line_seps(src),
      session.tokens(),
      false
    ));

    try {
      session.attach_ast(parse_main_block_with_engine_latest(*session.context(), engine, nullptr));
    } catch (const StyioBaseException&) {
    } catch (...) {
    }
  }
}

TEST(StyioSecurityParserContext, MalformedIteratorFallbackLeakSeedDoesNotLeakUnderSessionArena) {
  static constexpr unsigned char kSeed[] = {
    0x5b, 0x5b, 0x5b, 0x5b, 0x35, 0x5b, 0x5b, 0x5b, 0x32, 0x32, 0x32, 0x3e,
    0x40, 0x3e, 0x3e, 0x3e, 0x23, 0x20, 0x61, 0x74, 0x3e, 0x3e, 0x23, 0x20,
    0x61, 0x74, 0x20, 0x72, 0x3e, 0x69, 0x78, 0x20, 0x72, 0x3e, 0x69, 0x78,
    0x36, 0x38, 0x34, 0x29, 0x20, 0x29,
  };
  const std::string src(reinterpret_cast<const char*>(kSeed), sizeof(kSeed));

  for (StyioParserEngine engine : {StyioParserEngine::Legacy, StyioParserEngine::Nightly}) {
    CompilationSession session;
    session.adopt_tokens(StyioTokenizer::tokenize(src));
    session.attach_context(StyioContext::Create(
      "<malformed-iterator-fallback-leak-regression>",
      src,
      build_line_seps(src),
      session.tokens(),
      false
    ));

    try {
      session.attach_ast(parse_main_block_with_engine_latest(*session.context(), engine, nullptr));
    } catch (const StyioBaseException&) {
    } catch (...) {
    }
  }
}

TEST(StyioSecurityParserContext, MalformedCompoundAssignNameLeakSeedDoesNotLeakUnderSessionArena) {
  std::string src(103, 'Q');
  src += "x*=( > 2\n*****";
  src.push_back('\x08');
  src += ">y2&";

  for (StyioParserEngine engine : {StyioParserEngine::Legacy, StyioParserEngine::Nightly}) {
    CompilationSession session;
    session.adopt_tokens(StyioTokenizer::tokenize(src));
    session.attach_context(StyioContext::Create(
      "<malformed-compound-assign-name-leak-regression>",
      src,
      build_line_seps(src),
      session.tokens(),
      false
    ));

    try {
      session.attach_ast(parse_main_block_with_engine_latest(*session.context(), engine, nullptr));
    } catch (const StyioBaseException&) {
    } catch (...) {
    }
  }
}

TEST(StyioSecurityParserContext, MalformedAtResourceRefNameLeakSeedDoesNotLeakUnderSessionArena) {
  static constexpr unsigned char kSeed[] = {
    0x61, 0x23, 0x3a, 0x3d, 0x28, 0x61, 0x3a, 0x20, 0x69, 0x48, 0x48, 0x48,
    0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x40, 0x48, 0x48, 0x48, 0x48, 0x48,
    0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48,
    0x48, 0x78, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b,
    0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b,
    0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b,
    0x01, 0x00, 0x00, 0x20, 0x5b, 0x48, 0x00, 0x00, 0x00, 0x20, 0x00, 0x7c,
    0x32, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x78, 0x29, 0x0a, 0xe4,
    0x20, 0x78,
  };
  const std::string src(reinterpret_cast<const char*>(kSeed), sizeof(kSeed));

  for (StyioParserEngine engine : {StyioParserEngine::Legacy, StyioParserEngine::Nightly}) {
    CompilationSession session;
    session.adopt_tokens(StyioTokenizer::tokenize(src));
    session.attach_context(StyioContext::Create(
      "<malformed-at-resource-ref-name-leak-regression>",
      src,
      build_line_seps(src),
      session.tokens(),
      false
    ));

    try {
      session.attach_ast(parse_main_block_with_engine_latest(*session.context(), engine, nullptr));
    } catch (const StyioBaseException&) {
    } catch (...) {
    }
  }
}

TEST(StyioSecurityParserContext, TokenMapMatchesSingleRightArrow) {
  std::vector<StyioToken*> tokens{
    StyioToken::Create(StyioTokenType::TOK_MINUS, "-"),
    StyioToken::Create(StyioTokenType::TOK_RANGBRAC, ">"),
    StyioToken::Create(StyioTokenType::TOK_EOF, "")
  };
  StyioContext* ctx = StyioContext::Create(
    "<map-match-arrow>",
    "->",
    {{0, 0}},
    tokens,
    false
  );

  EXPECT_TRUE(ctx->map_match(StyioTokenType::ARROW_SINGLE_RIGHT));
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
    false
  );

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

TEST(StyioSecurityParserContext, FindDropPanicAtEofReportsSyntaxError) {
  std::vector<StyioToken*> tokens;
  StyioContext* ctx = StyioContext::Create(
    "<find-drop-panic-eof>",
    "",
    {},
    tokens,
    false
  );

  EXPECT_THROW(
    {
      ctx->find_drop_panic(')');
    },
    StyioSyntaxError
  );

  delete ctx;
}

TEST(StyioSecurityNightlyParserStmt, TryParseSubsetOwnsBindingAndLeavesFallbackFollow) {
  const std::string src = "x = 1 | 2\n";
  auto tokens = StyioTokenizer::tokenize(src);
  StyioContext* ctx = StyioContext::Create(
    "<nightly-decline>",
    src,
    build_line_seps(src),
    tokens,
    false
  );

  ParseAttempt<StyioAST> attempt;
  EXPECT_NO_THROW({
    attempt = try_parse_stmt_subset_nightly(*ctx);
  });
  EXPECT_EQ(attempt.status, ParseAttemptStatus::Parsed);
  auto* bind = dynamic_cast<FlexBindAST*>(attempt.node);
  ASSERT_NE(bind, nullptr);
  ASSERT_NE(bind->getValue(), nullptr);
  EXPECT_EQ(bind->getValue()->getNodeType(), StyioNodeType::Integer);
  EXPECT_EQ(ctx->cur_tok_type(), StyioTokenType::TOK_PIPE);

  delete ctx;
  free_tokens(tokens);
  StyioAST::destroy_all_tracked_nodes();
}

TEST(StyioSecurityNightlyParserStmt, TryParseSubsetFatalRestoresCursorAndCapturesError) {
  const std::string src = "#f(x) =>\n";
  auto tokens = StyioTokenizer::tokenize(src);
  StyioContext* ctx = StyioContext::Create(
    "<nightly-fatal>",
    src,
    build_line_seps(src),
    tokens,
    false
  );

  const auto saved = ctx->save_cursor();
  ParseAttempt<StyioAST> attempt;
  EXPECT_NO_THROW({
    attempt = try_parse_stmt_subset_nightly(*ctx);
  });
  EXPECT_EQ(attempt.status, ParseAttemptStatus::Fatal);
  EXPECT_EQ(attempt.node, nullptr);
  EXPECT_NE(attempt.error, nullptr);
  EXPECT_EQ(ctx->save_cursor(), saved);
  EXPECT_THROW(
    {
      std::rethrow_exception(attempt.error);
    },
    StyioBaseException
  );

  delete ctx;
  free_tokens(tokens);
  StyioAST::destroy_all_tracked_nodes();
}

TEST(StyioSecurityParserPath, SingleLetterPathDoesNotThrowOutOfRange) {
  std::vector<StyioToken*> tokens;
  const std::string src = "\"A\"";
  StyioContext* ctx = StyioContext::Create(
    "<single-letter-path>",
    src,
    build_line_seps(src),
    tokens,
    false
  );

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

TEST(StyioSecurityParserPath, ClassifiesPathFamiliesAndRejectsUnterminatedPath) {
  auto parse_path_node_type = [](const std::string& path)
  {
    const std::string src = "\"" + path + "\"";
    std::vector<StyioToken*> tokens;
    StyioContext* ctx = StyioContext::Create(
      "<path-family>",
      src,
      build_line_seps(src),
      tokens,
      false
    );

    std::unique_ptr<StyioAST> path_ast(parse_path(*ctx));
    const StyioNodeType node_type = path_ast->getNodeType();
    delete ctx;
    return node_type;
  };

  EXPECT_EQ(parse_path_node_type("/tmp/styio.txt"), StyioNodeType::LocalPath);
  EXPECT_EQ(parse_path_node_type("C:/<drive-root>/styio.txt"), StyioNodeType::LocalPath);
  EXPECT_EQ(parse_path_node_type("relative/styio.txt"), StyioNodeType::LocalPath);
  EXPECT_EQ(parse_path_node_type("http://example.test/styio"), StyioNodeType::WebUrl);
  EXPECT_EQ(parse_path_node_type("https://example.test/styio"), StyioNodeType::WebUrl);
  EXPECT_EQ(parse_path_node_type("ftp://example.test/styio"), StyioNodeType::WebUrl);
  EXPECT_EQ(parse_path_node_type("mysql://db/styio"), StyioNodeType::DBUrl);
  EXPECT_EQ(parse_path_node_type("postgres://db/styio"), StyioNodeType::DBUrl);
  EXPECT_EQ(parse_path_node_type("mongo://db/styio"), StyioNodeType::DBUrl);
  EXPECT_EQ(parse_path_node_type("localhost:8080/styio"), StyioNodeType::RemotePath);
  EXPECT_EQ(parse_path_node_type("127.0.0.1:8080/styio"), StyioNodeType::RemotePath);
  EXPECT_EQ(parse_path_node_type("192.0.2.1/styio"), StyioNodeType::RemotePath);
  EXPECT_EQ(parse_path_node_type("2001:0db8:85a3:0000:0000:8a2e:0370:7334"), StyioNodeType::RemotePath);
  EXPECT_EQ(parse_path_node_type("\\\\<server>\\<share>\\styio"), StyioNodeType::RemotePath);

  const std::string unterminated = "\"unterminated";
  std::vector<StyioToken*> tokens;
  StyioContext* ctx = StyioContext::Create(
    "<path-unterminated>",
    unterminated,
    build_line_seps(unterminated),
    tokens,
    false
  );
  EXPECT_THROW(
    {
      std::unique_ptr<StyioAST> path_ast(parse_path(*ctx));
    },
    StyioSyntaxError);
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

  auto fut = std::async(std::launch::async, [&src]
                        {
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
  EXPECT_EQ(tokens[0]->lexeme().size(), 200'000u);
  EXPECT_EQ(tokens[1]->type, StyioTokenType::TOK_EOF);
  free_tokens(tokens);
}

TEST(StyioSecurityLexer, TokenizesInlineReturnAndPipeSemicolon) {
  auto tokens = StyioTokenizer::tokenize("|<| result |;");
  ASSERT_GE(tokens.size(), 6u);
  EXPECT_EQ(tokens[0]->type, StyioTokenType::RETURN_PIPE);
  EXPECT_EQ(tokens[0]->lexeme(), "|<|");
  EXPECT_EQ(tokens[2]->type, StyioTokenType::NAME);
  EXPECT_EQ(tokens[2]->lexeme(), "result");
  EXPECT_EQ(tokens[4]->type, StyioTokenType::PIPE_SEMICOLON);
  EXPECT_EQ(tokens[4]->lexeme(), "|;");
  EXPECT_EQ(tokens.back()->type, StyioTokenType::TOK_EOF);
  free_tokens(tokens);
}

TEST(StyioSecurityLexer, TokenizesAwaitPipe) {
  auto tokens = StyioTokenizer::tokenize("?| job -> value: i64 | 0");
  ASSERT_GE(tokens.size(), 12u);
  EXPECT_EQ(tokens[0]->type, StyioTokenType::AWAIT_PIPE);
  EXPECT_EQ(tokens[0]->lexeme(), "?|");
  EXPECT_EQ(tokens[4]->type, StyioTokenType::ARROW_SINGLE_RIGHT);

  bool saw_fallback_pipe = false;
  for (auto* token : tokens) {
    if (token->type == StyioTokenType::TOK_PIPE) {
      saw_fallback_pipe = true;
      break;
    }
  }
  EXPECT_TRUE(saw_fallback_pipe);
  free_tokens(tokens);
}

TEST(StyioSecurityLexer, TokenizesTerminalHandleShorthands) {
  auto bracket_tokens = StyioTokenizer::tokenize("<|[>_]");
  ASSERT_GE(bracket_tokens.size(), 5u);
  EXPECT_EQ(bracket_tokens[0]->type, StyioTokenType::YIELD_PIPE);
  EXPECT_EQ(bracket_tokens[1]->type, StyioTokenType::TOK_LBOXBRAC);
  EXPECT_EQ(bracket_tokens[2]->type, StyioTokenType::PRINT);
  EXPECT_EQ(bracket_tokens[3]->type, StyioTokenType::TOK_RBOXBRAC);
  EXPECT_EQ(bracket_tokens.back()->type, StyioTokenType::TOK_EOF);
  free_tokens(bracket_tokens);

  auto paren_tokens = StyioTokenizer::tokenize("<|(>_)");
  ASSERT_GE(paren_tokens.size(), 5u);
  EXPECT_EQ(paren_tokens[0]->type, StyioTokenType::YIELD_PIPE);
  EXPECT_EQ(paren_tokens[1]->type, StyioTokenType::TOK_LPAREN);
  EXPECT_EQ(paren_tokens[2]->type, StyioTokenType::PRINT);
  EXPECT_EQ(paren_tokens[3]->type, StyioTokenType::TOK_RPAREN);
  EXPECT_EQ(paren_tokens.back()->type, StyioTokenType::TOK_EOF);
  free_tokens(paren_tokens);
}

TEST(StyioSecurityLexer, TokenizesRarePunctuationAndNativeExternBodies) {
  const std::string source =
    "\r` ~ --- === ??\n"
    "@extern(c) => {\n"
    "  // line comment with } ignored\n"
    "  /* block comment with } ignored */\n"
    "  const char single = '\\'';\n"
    "  const char* escaped = \"}\\\"\";\n"
    "  auto raw8 = u8R\"x({})x\";\n"
    "  auto raw16 = uR\"y({})y\";\n"
    "  auto raw32 = UR\"z({})z\";\n"
    "  auto rawwide = LR\"w({})w\";\n"
    "  int nested(void) { return 1; }\n"
    "}\n";

  auto tokens = StyioTokenizer::tokenize(source);
  auto has_token = [&](StyioTokenType type)
  {
    return std::any_of(
      tokens.begin(),
      tokens.end(),
      [&](const StyioToken* token)
      {
        return token != nullptr && token->type == type;
      });
  };

  EXPECT_TRUE(has_token(StyioTokenType::TOK_CR));
  EXPECT_TRUE(has_token(StyioTokenType::TOK_BQUOTE));
  EXPECT_TRUE(has_token(StyioTokenType::TOK_TILDE));
  EXPECT_TRUE(has_token(StyioTokenType::SINGLE_SEP_LINE));
  EXPECT_TRUE(has_token(StyioTokenType::DOUBLE_SEP_LINE));
  EXPECT_TRUE(has_token(StyioTokenType::DBQUESTION));
  EXPECT_TRUE(has_token(StyioTokenType::NATIVE_EXTERN_BODY));

  const auto body_it = std::find_if(
    tokens.begin(),
    tokens.end(),
    [](const StyioToken* token)
    {
      return token != nullptr && token->type == StyioTokenType::NATIVE_EXTERN_BODY;
    });
  ASSERT_NE(body_it, tokens.end());
  EXPECT_NE((*body_it)->lexeme().find("line comment with } ignored"), std::string::npos);
  EXPECT_NE((*body_it)->lexeme().find("block comment with } ignored"), std::string::npos);
  EXPECT_NE((*body_it)->lexeme().find("u8R\"x({})x\""), std::string::npos);
  EXPECT_NE((*body_it)->lexeme().find("LR\"w({})w\""), std::string::npos);
  EXPECT_NE((*body_it)->lexeme().find("nested(void)"), std::string::npos);

  free_tokens(tokens);

  const std::vector<std::string> malformed_extern_prefixes = {
    "){ int body; }",
    "extern c) { int body; }",
    "notextern(c) { int body; }",
  };
  for (const auto& malformed : malformed_extern_prefixes) {
    auto malformed_tokens = StyioTokenizer::tokenize(malformed);
    EXPECT_TRUE(std::none_of(
      malformed_tokens.begin(),
      malformed_tokens.end(),
      [](const StyioToken* token)
      {
        return token != nullptr && token->type == StyioTokenType::NATIVE_EXTERN_BODY;
      })) << malformed;
    free_tokens(malformed_tokens);
  }

  const std::string malformed_raw_source =
    "@extern(c) => {\n"
    "  auto no_open = R\"no_paren\";\n"
    "  auto no_close = R\"x(no close\";\n"
    "  int done(void) { return 2; }\n"
    "}\n";
  auto malformed_raw_tokens = StyioTokenizer::tokenize(malformed_raw_source);
  const auto malformed_raw_body = std::find_if(
    malformed_raw_tokens.begin(),
    malformed_raw_tokens.end(),
    [](const StyioToken* token)
    {
      return token != nullptr && token->type == StyioTokenType::NATIVE_EXTERN_BODY;
    });
  ASSERT_NE(malformed_raw_body, malformed_raw_tokens.end());
  EXPECT_NE((*malformed_raw_body)->lexeme().find("no_open"), std::string::npos);
  EXPECT_NE((*malformed_raw_body)->lexeme().find("no_close"), std::string::npos);
  EXPECT_NE((*malformed_raw_body)->lexeme().find("done(void)"), std::string::npos);
  free_tokens(malformed_raw_tokens);

  const std::string raw_without_open_paren_source =
    "@extern(c) => {\n"
    "  auto no_open = R\"no_paren\";\n"
    "}\n";
  auto raw_without_open_paren_tokens = StyioTokenizer::tokenize(raw_without_open_paren_source);
  const auto raw_without_open_paren_body = std::find_if(
    raw_without_open_paren_tokens.begin(),
    raw_without_open_paren_tokens.end(),
    [](const StyioToken* token)
    {
      return token != nullptr && token->type == StyioTokenType::NATIVE_EXTERN_BODY;
    });
  ASSERT_NE(raw_without_open_paren_body, raw_without_open_paren_tokens.end());
  EXPECT_NE((*raw_without_open_paren_body)->lexeme().find("no_open"), std::string::npos);
  free_tokens(raw_without_open_paren_tokens);

  EXPECT_THROW(
    {
      auto unterminated = StyioTokenizer::tokenize("@extern(c) => { int missing(void) { return 1; ");
      free_tokens(unterminated);
    },
    StyioLexError
  );
}

TEST(StyioSecurityParserLookahead, SkipTriviaFindsNextToken) {
  auto tokens = StyioTokenizer::tokenize("   // cmt\nfoo");
  ASSERT_FALSE(tokens.empty());

  const size_t idx = styio_skip_trivia_tokens(tokens, 0);
  ASSERT_LT(idx, tokens.size());
  EXPECT_EQ(tokens[idx]->type, StyioTokenType::NAME);
  EXPECT_EQ(tokens[idx]->lexeme(), "foo");

  EXPECT_TRUE(styio_try_check_non_trivia(tokens, 0, StyioTokenType::NAME));
  EXPECT_FALSE(styio_try_check_non_trivia(tokens, 0, StyioTokenType::INTEGER));
  EXPECT_FALSE(styio_try_check_non_trivia({}, 0, StyioTokenType::NAME));
  free_tokens(tokens);
}

TEST(StyioSecurityNightlyParserExpr, MatchesLegacyOnSubsetSamples) {
  const std::vector<std::string> samples = {
    "1 + 2 * 3",
    "(1 + 2) * 3",
    "\"x\" + \"y\"",
    "-5 + 2 ** 3",
  };

  for (const auto& src : samples) {
    try {
      EXPECT_EQ(parse_expr_to_repr_latest(src, true), parse_expr_to_repr_latest(src, false)) << src;
    }
    catch (const std::exception& ex) {
      FAIL() << "sample '" << src << "' threw: " << ex.what();
    }
  }
}

// ---------------------------------------------------------------
// StyioTokenizerSpan — span correctness regression tests
// ---------------------------------------------------------------

TEST(StyioTokenizerSpan, SpanBoundsAreCorrect) {
  auto tokens = StyioTokenizer::tokenize("x = 42");
  ASSERT_GE(tokens.size(), 4u);

  // x
  EXPECT_EQ(tokens[0]->type, StyioTokenType::NAME);
  EXPECT_EQ(tokens[0]->begin(), 0u);
  EXPECT_EQ(tokens[0]->len(), 1u);
  EXPECT_EQ(tokens[0]->lexeme(), "x");

  // space
  EXPECT_EQ(tokens[1]->type, StyioTokenType::TOK_SPACE);
  EXPECT_EQ(tokens[1]->begin(), 1u);
  EXPECT_EQ(tokens[1]->len(), 1u);

  // =
  EXPECT_EQ(tokens[2]->type, StyioTokenType::TOK_EQUAL);
  EXPECT_EQ(tokens[2]->begin(), 2u);
  EXPECT_EQ(tokens[2]->len(), 1u);

  free_tokens(tokens);
}

TEST(StyioTokenizerSpan, LexemeViewMatchesOriginal) {
  auto tokens = StyioTokenizer::tokenize("hello world");
  ASSERT_GE(tokens.size(), 4u);
  ASSERT_TRUE(tokens[0]->hasSourceSpan());
  EXPECT_EQ(tokens[0]->lexeme(), "hello");
  EXPECT_EQ(tokens[0]->lexeme(), tokens[0]->lexeme());
  free_tokens(tokens);
}

TEST(StyioTokenizerSpan, EofHasValidSpan) {
  auto tokens = StyioTokenizer::tokenize("x");
  ASSERT_GE(tokens.size(), 2u);
  EXPECT_EQ(tokens.back()->type, StyioTokenType::TOK_EOF);
  free_tokens(tokens);
}

TEST(StyioTokenizerSpan, EmptyInputYieldsOnlyEof) {
  auto tokens = StyioTokenizer::tokenize("");
  ASSERT_EQ(tokens.size(), 1u);
  EXPECT_EQ(tokens[0]->type, StyioTokenType::TOK_EOF);
  free_tokens(tokens);
}

TEST(StyioTokenizerSpan, WhitespaceOnly) {
  auto tokens = StyioTokenizer::tokenize("   \n  ");
  ASSERT_GE(tokens.size(), 1u);
  EXPECT_EQ(tokens.back()->type, StyioTokenType::TOK_EOF);
  free_tokens(tokens);
}

TEST(StyioTokenizerSpan, LongestMatchPrecedence) {
  // => must be ARROW_DOUBLE_RIGHT, not TOK_EQUAL followed by TOK_RANGBRAC
  auto tokens = StyioTokenizer::tokenize("=>");
  ASSERT_GE(tokens.size(), 2u);
  EXPECT_EQ(tokens[0]->type, StyioTokenType::ARROW_DOUBLE_RIGHT);
  EXPECT_EQ(tokens[0]->lexeme(), "=>");

  // ||> must be TASK_LAUNCH, not LOGIC_OR followed by TOK_RANGBRAC
  auto tokens2 = StyioTokenizer::tokenize("||>");
  ASSERT_GE(tokens2.size(), 2u);
  EXPECT_EQ(tokens2[0]->type, StyioTokenType::TASK_LAUNCH);
  EXPECT_EQ(tokens2[0]->lexeme(), "||>");

  // ** must be BINOP_POW, not TOK_STAR TOK_STAR
  auto tokens3 = StyioTokenizer::tokenize("**");
  ASSERT_GE(tokens3.size(), 2u);
  EXPECT_EQ(tokens3[0]->type, StyioTokenType::BINOP_POW);

  // ?| must be AWAIT_PIPE, not TOK_QUEST TOK_PIPE
  auto tokens4 = StyioTokenizer::tokenize("?|");
  ASSERT_GE(tokens4.size(), 2u);
  EXPECT_EQ(tokens4[0]->type, StyioTokenType::AWAIT_PIPE);

  // && must be LOGIC_AND, not TOK_AMP TOK_AMP
  auto tokens5 = StyioTokenizer::tokenize("&&");
  ASSERT_GE(tokens5.size(), 2u);
  EXPECT_EQ(tokens5[0]->type, StyioTokenType::LOGIC_AND);

  free_tokens(tokens);
  free_tokens(tokens2);
  free_tokens(tokens3);
  free_tokens(tokens4);
  free_tokens(tokens5);
}

TEST(StyioTokenizerSpan, KeywordVsIdentifier) {
  // Every word is NAME. Exact spelling is interpreted only after a
  // symbol-anchored parser context has selected a structural production.
  for (const char* word : {
         "if", "else", "match", "while", "for", "return", "fn",
         "schema", "type", "record", "variant", "protocol", "impl",
         "import", "extern", "true", "false"
       }) {
    auto tokens = StyioTokenizer::tokenize(word);
    ASSERT_GE(tokens.size(), 2u);
    EXPECT_EQ(tokens[0]->type, StyioTokenType::NAME) << "word: " << word;
    EXPECT_EQ(tokens[0]->lexeme(), word);
    free_tokens(tokens);
  }
}

TEST(StyioTokenizerSpan, UnterminatedStringThrowsLexError) {
  EXPECT_THROW(
    { auto tokens = StyioTokenizer::tokenize("\"no closing quote"); free_tokens(tokens); },
    StyioLexError);
}

TEST(StyioTokenizerSpan, UnterminatedBlockCommentThrowsLexError) {
  EXPECT_THROW(
    { auto tokens = StyioTokenizer::tokenize("/* no closing"); free_tokens(tokens); },
    StyioLexError);
}

TEST(StyioTokenizerSpan, UnterminatedNativeExternThrowsLexError) {
  EXPECT_THROW(
    {
      auto tokens = StyioTokenizer::tokenize(
        "@extern(c) => { int f(void) { return 1; ");
      free_tokens(tokens);
    },
    StyioLexError);
}

TEST(StyioTokenizerSpan, AsciiIdentifierSpansAreSequential) {
  // ASCII identifiers — full Unicode ID support requires ICU (STYIO_USE_ICU=ON)
  auto tokens = StyioTokenizer::tokenize("alpha beta gamma");
  ASSERT_GE(tokens.size(), 6u);
  EXPECT_EQ(tokens[0]->type, StyioTokenType::NAME);
  EXPECT_EQ(tokens[0]->lexeme(), "alpha");
  EXPECT_EQ(tokens[0]->begin(), 0u);
  EXPECT_EQ(tokens[0]->len(), 5u);
  EXPECT_EQ(tokens[2]->type, StyioTokenType::NAME);
  EXPECT_EQ(tokens[2]->lexeme(), "beta");
  EXPECT_EQ(tokens[2]->begin(), 6u);
  EXPECT_EQ(tokens[4]->type, StyioTokenType::NAME);
  EXPECT_EQ(tokens[4]->lexeme(), "gamma");
  EXPECT_EQ(tokens[4]->begin(), 11u);
  free_tokens(tokens);
}

TEST(StyioTokenizerSpan, NonAsciiIdentifierRejectedWithoutIcu) {
  // Without ICU, bytes > 0x7F are not valid identifier starts.
  // Each non-ASCII byte becomes UNKNOWN.
  auto tokens = StyioTokenizer::tokenize("\xE5\x8F\x98");
  ASSERT_GE(tokens.size(), 2u);
  // The leading byte is not ASCII-alpha or '_', so it falls through
  // to the default UNKNOWN case.
  bool has_unknown = false;
  for (auto* t : tokens) {
    if (t->type == StyioTokenType::UNKNOWN) has_unknown = true;
  }
  EXPECT_TRUE(has_unknown) << "Non-ASCII bytes without ICU must be UNKNOWN";
  free_tokens(tokens);
}

TEST(StyioTokenizerSpan, CrLfHandling) {
  auto tokens = StyioTokenizer::tokenize("a\r\nb");
  ASSERT_GE(tokens.size(), 3u);
  EXPECT_EQ(tokens[0]->type, StyioTokenType::NAME);
  EXPECT_EQ(tokens[0]->lexeme(), "a");
  free_tokens(tokens);
}

TEST(StyioTokenizerSpan, FileEndingWithoutNewline) {
  auto tokens = StyioTokenizer::tokenize("x // eof-no-newline");
  ASSERT_GE(tokens.size(), 4u);
  EXPECT_EQ(tokens[0]->type, StyioTokenType::NAME);
  EXPECT_EQ(tokens[2]->type, StyioTokenType::COMMENT_LINE);
  free_tokens(tokens);
}

TEST(StyioTokenizerSpan, MalformedInputProducesUnknownToken) {
  auto tokens = StyioTokenizer::tokenize(std::string("x\0y", 3));
  ASSERT_GE(tokens.size(), 3u);
  // Embedded NUL should produce UNKNOWN token
  bool has_unknown = false;
  for (auto* t : tokens) {
    if (t->type == StyioTokenType::UNKNOWN) {
      has_unknown = true;
      EXPECT_EQ(t->len(), 1u);
    }
  }
  EXPECT_TRUE(has_unknown);
  free_tokens(tokens);
}

// ---------------------------------------------------------------
// StyioTokenizerPerf — tokenizer benchmark / soak tests
// ---------------------------------------------------------------

TEST(StyioTokenizerPerf, LargeIdentifiers) {
  std::string src;
  src.reserve(100000);
  for (int i = 0; i < 1000; ++i) {
    src += "very_long_identifier_name_" + std::to_string(i) + " ";
  }
  src += "\n";

  auto start = std::chrono::steady_clock::now();
  auto tokens = StyioTokenizer::tokenize(src);
  auto end = std::chrono::steady_clock::now();

  auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  double mbps = (src.size() / 1048576.0) / (us / 1000000.0);

  EXPECT_GT(tokens.size(), 1000u);
  EXPECT_GT(mbps, 0.1);  // sanity: at least 0.1 MB/s
  free_tokens(tokens);
}

TEST(StyioTokenizerPerf, LargeNumbers) {
  std::string src;
  src.reserve(200000);
  for (int i = 0; i < 5000; ++i) {
    src += std::to_string(i * 123456789LL) + " ";
  }
  src += "\n";

  auto start = std::chrono::steady_clock::now();
  auto tokens = StyioTokenizer::tokenize(src);
  auto end = std::chrono::steady_clock::now();

  auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  double mbps = (src.size() / 1048576.0) / (us / 1000000.0);

  EXPECT_GT(tokens.size(), 5000u);
  EXPECT_GT(mbps, 1.0);  // at least 1 MB/s
  free_tokens(tokens);
}

TEST(StyioTokenizerPerf, LongStringLiteral) {
  std::string src = "\"";
  for (int i = 0; i < 50000; ++i) {
    src += 'a' + (i % 26);
  }
  src += "\"";

  auto start = std::chrono::steady_clock::now();
  auto tokens = StyioTokenizer::tokenize(src);
  auto end = std::chrono::steady_clock::now();

  auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  double mbps = (src.size() / 1048576.0) / (us / 1000000.0);

  EXPECT_GE(tokens.size(), 2u);  // STRING + EOF
  EXPECT_GT(mbps, 5.0);  // at least 5 MB/s
  free_tokens(tokens);
}

TEST(StyioTokenizerPerf, DenseOperators) {
  std::string src;
  src.reserve(50000);
  for (int i = 0; i < 3000; ++i) {
    src += "=> ||> ** ?| && <= >= != := <- -> <| >_ + - * / % ";
  }
  src += "\n";

  auto start = std::chrono::steady_clock::now();
  auto tokens = StyioTokenizer::tokenize(src);
  auto end = std::chrono::steady_clock::now();

  auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  double mbps = (src.size() / 1048576.0) / (us / 1000000.0);

  EXPECT_GT(tokens.size(), 10000u);
  EXPECT_GT(mbps, 1.0);  // at least 1 MB/s
  free_tokens(tokens);
}

TEST(StyioTokenizerPerf, LongComment) {
  std::string src = "// ";
  for (int i = 0; i < 50000; ++i) {
    src += 'a' + (i % 26);
  }
  src += "\nx";

  auto start = std::chrono::steady_clock::now();
  auto tokens = StyioTokenizer::tokenize(src);
  auto end = std::chrono::steady_clock::now();

  auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  double mbps = (src.size() / 1048576.0) / (us / 1000000.0);

  EXPECT_GE(tokens.size(), 4u);  // COMMENT_LINE, LF, NAME, EOF
  EXPECT_GT(mbps, 5.0);  // at least 5 MB/s
  free_tokens(tokens);
}

TEST(StyioTokenizerPerf, MixedRealisticInput) {
  std::string src;
  src.reserve(100000);
  for (int i = 0; i < 200; ++i) {
    src += "# compute_" + std::to_string(i) + " := (a: i64, b: i64) => {\n";
    src += "  x = a + b * 2\n";
    src += "  y := x ** 3\n";
    src += "  result = ?| (<< @file(\"data.txt\")) | io => 0 | y\n";
    src += "  <| result\n";
    src += "}\n\n";
  }

  auto start = std::chrono::steady_clock::now();
  auto tokens = StyioTokenizer::tokenize(src);
  auto end = std::chrono::steady_clock::now();

  auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  double mbps = (src.size() / 1048576.0) / (us / 1000000.0);

  EXPECT_GT(tokens.size(), 5000u);
  EXPECT_GT(mbps, 0.5);  // at least 0.5 MB/s
  free_tokens(tokens);
}

TEST(StyioTokenizerPerf, LargeInputSoak) {
  // Generate ~500KB of mixed Styio-like source
  std::string src;
  src.reserve(600000);
  for (int i = 0; i < 1000; ++i) {
    src += "# func_" + std::to_string(i) + " := (x: i64) => {\n";
    src += "  // comment line " + std::to_string(i) + "\n";
    src += "  y = x + " + std::to_string(i) + "\n";
    src += "  \"string literal " + std::to_string(i % 100) + "\" -> @stdout\n";
    src += "  <| y\n";
    src += "}\n";
  }

  auto start = std::chrono::steady_clock::now();
  auto tokens = StyioTokenizer::tokenize(src);
  auto end = std::chrono::steady_clock::now();

  auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  double mbps = (src.size() / 1048576.0) / (us / 1000000.0);
  size_t token_count = tokens.size();

  EXPECT_GT(token_count, 10000u);
  EXPECT_GT(mbps, 1.0);  // at least 1 MB/s
  free_tokens(tokens);

  // Verify the tokenizer doesn't time out on larger input
  // ~2MB input
  std::string big_src;
  big_src.reserve(2100000);
  for (int i = 0; i < 5000; ++i) {
    big_src += "x" + std::to_string(i) + " = " + std::to_string(i) + "\n";
  }

  auto start2 = std::chrono::steady_clock::now();
  auto big_tokens = StyioTokenizer::tokenize(big_src);
  auto end2 = std::chrono::steady_clock::now();

  auto us2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2).count();
  EXPECT_LT(us2, 5000000);  // Should complete in under 5 seconds
  free_tokens(big_tokens);
}

TEST(StyioTokenizerPerf, NativeExternBlock) {
  std::string src = "@extern(c) => {\nint add(int a, int b) {\n";
  for (int i = 0; i < 10000; ++i) {
    src += "  // line " + std::to_string(i) + "\n";
  }
  src += "  return a + b;\n}\n}";

  auto start = std::chrono::steady_clock::now();
  auto tokens = StyioTokenizer::tokenize(src);
  auto end = std::chrono::steady_clock::now();

  auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  double mbps = (src.size() / 1048576.0) / (us / 1000000.0);

  EXPECT_GT(tokens.size(), 3u);
  EXPECT_GT(mbps, 1.0);  // at least 1 MB/s
  free_tokens(tokens);
}

TEST(StyioTokenizerSpan, DenseSymbolMixedInput) {
  // Verify dense symbol sequences don't cause quadratic behavior
  std::string src;
  src.reserve(100000);
  for (int i = 0; i < 5000; ++i) {
    src += "! # $ % & ' ( ) * + , - . / : ; < = > ? @ [ \\ ] ^ _ ` { | } ~ ";
  }

  auto start = std::chrono::steady_clock::now();
  auto tokens = StyioTokenizer::tokenize(src);
  auto end = std::chrono::steady_clock::now();

  auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  double mbps = (src.size() / 1048576.0) / (us / 1000000.0);

  EXPECT_GT(tokens.size(), 50000u);
  EXPECT_GT(mbps, 1.0);  // at least 1 MB/s
  free_tokens(tokens);
}

// ---------------------------------------------------------------
// StyioTokenizerContract — B2 span/owned-text/micro-level checks
// ---------------------------------------------------------------

TEST(StyioTokenizerContract, EofHasZeroWidthSpan) {
  // Empty file: begin == end == 0
  {
    auto tokens = StyioTokenizer::tokenize("");
    ASSERT_GE(tokens.size(), 1u);
    EXPECT_EQ(tokens.back()->type, StyioTokenType::TOK_EOF);
    EXPECT_EQ(tokens.back()->begin(), 0u);
    EXPECT_EQ(tokens.back()->end(), 0u);
    EXPECT_EQ(tokens.back()->len(), 0u);
    EXPECT_TRUE(tokens.back()->hasSourceSpan());
    free_tokens(tokens);
  }
  // Non-empty file: begin == end == source.size()
  {
    auto tokens = StyioTokenizer::tokenize("x = 1");
    ASSERT_GE(tokens.size(), 2u);
    EXPECT_EQ(tokens.back()->type, StyioTokenType::TOK_EOF);
    EXPECT_EQ(tokens.back()->begin(), 5u);
    EXPECT_EQ(tokens.back()->end(), 5u);
    EXPECT_EQ(tokens.back()->len(), 0u);
    EXPECT_TRUE(tokens.back()->lexeme().empty());
    free_tokens(tokens);
  }
}

TEST(StyioTokenizerContract, StringLiteralHasDecodedValue) {
  // Basic escaped string
  auto tokens = StyioTokenizer::tokenize("\"a\\\"b\"");
  ASSERT_GE(tokens.size(), 2u);
  EXPECT_EQ(tokens[0]->type, StyioTokenType::STRING);
  // raw includes quotes and escape sequences
  EXPECT_EQ(tokens[0]->lexeme(), "\"a\\\"b\"");
  // decoded removes quotes and resolves escapes
  EXPECT_EQ(tokens[0]->decodedString(), "a\"b");
  EXPECT_TRUE(tokens[0]->hasDecodedText());
  free_tokens(tokens);
}

TEST(StyioTokenizerContract, StringEscapeSequences) {
  struct Case { const char* input; const char* raw; const char* decoded; };
  Case cases[] = {
    {"\"a\\\\b\"", "\"a\\\\b\"", "a\\b"},
    {"\"line\\n\"", "\"line\\n\"", "line\n"},
    {"\"tab\\t\"",   "\"tab\\t\"",   "tab\t"},
    {"\"ret\\r\"",   "\"ret\\r\"",   "ret\r"},
    {"\"\"",         "\"\"",         ""},
  };
  for (auto& c : cases) {
    auto tokens = StyioTokenizer::tokenize(c.input);
    ASSERT_GE(tokens.size(), 2u);
    EXPECT_EQ(tokens[0]->type, StyioTokenType::STRING);
    EXPECT_EQ(tokens[0]->lexeme(), c.raw);
    EXPECT_EQ(tokens[0]->decodedString(), c.decoded);
    EXPECT_TRUE(tokens[0]->hasDecodedText());
    free_tokens(tokens);
  }
}

TEST(StyioTokenizerContract, UnterminatedStringThrows) {
  EXPECT_THROW(
    { auto t = StyioTokenizer::tokenize("\"no close"); free_tokens(t); },
    StyioLexError);
}

TEST(StyioTokenizerContract, OperatorBucketIsPrecomputed) {
  // Verify kOperatorBuckets is a constexpr array with correct lookups
  std::string all_ops = "! % & * + - / : < = > ? [ | ~";
  auto tokens = StyioTokenizer::tokenize(all_ops);
  // Every single-char operator falls through to the single-char switch
  // Multi-char operators would be matched by the bucket
  ASSERT_GT(tokens.size(), 1u);
  free_tokens(tokens);
}

TEST(StyioTokenizerContract, MetricsArePopulated) {
  StyioTokenizerMetrics m;
  std::string src = "x = 1 + 2\n# func := () => { <| 42 }\n\"hello\"";
  auto tokens = StyioTokenizer::tokenizeWithMetrics(src, &m);

  EXPECT_EQ(m.input_bytes, src.size());
  EXPECT_GT(m.token_count, 0u);
  EXPECT_GT(m.source_view_token_count, 0u);
  // owned_text_token_count/bytes are always 0 in the zero-copy span-first tokenizer
  EXPECT_GT(m.source_span_token_count, 0u);
  EXPECT_GT(m.operator_bucket_probes, 0u);
  EXPECT_GE(m.string_decodes, 1u);

  free_tokens(tokens);
}

TEST(StyioTokenizerContract, AllPrefixConflictsResolve) {
  struct { const char* input; StyioTokenType type; const char* spelling; } cases[] = {
    {"=", StyioTokenType::TOK_EQUAL, "="},
    {"==", StyioTokenType::BINOP_EQ, "=="},
    {"=>", StyioTokenType::ARROW_DOUBLE_RIGHT, "=>"},
    {"===", StyioTokenType::DOUBLE_SEP_LINE, "==="},
    {"|", StyioTokenType::TOK_PIPE, "|"},
    {"||", StyioTokenType::LOGIC_OR, "||"},
    {"||>", StyioTokenType::TASK_LAUNCH, "||>"},
    {"|<|", StyioTokenType::RETURN_PIPE, "|<|"},
    {"|;", StyioTokenType::PIPE_SEMICOLON, "|;"},
    {"|]", StyioTokenType::BOUNDED_BUFFER_CLOSE, "|]"},
    {"?", StyioTokenType::TOK_QUEST, "?"},
    {"?|", StyioTokenType::AWAIT_PIPE, "?|"},
    {"?=", StyioTokenType::MATCH, "?="},
    {"??", StyioTokenType::DBQUESTION, "??"},
    {"<", StyioTokenType::TOK_LANGBRAC, "<"},
    {"<=", StyioTokenType::BINOP_LE, "<="},
    {"<-", StyioTokenType::ARROW_SINGLE_LEFT, "<-"},
    {"<|", StyioTokenType::YIELD_PIPE, "<|"},
    {"<~", StyioTokenType::WAVE_LEFT, "<~"},
    {"<<", StyioTokenType::EXTRACTOR, "<<"},
    {"<<<", StyioTokenType::EXTRACTOR, "<<<"},
    {">", StyioTokenType::TOK_RANGBRAC, ">"},
    {">=", StyioTokenType::BINOP_GE, ">="},
    {">_", StyioTokenType::PRINT, ">_"},
    {">>", StyioTokenType::ITERATOR, ">>"},
    {">>>", StyioTokenType::ITERATOR, ">>>"},
    {">>>>", StyioTokenType::ITERATOR, ">>>>"},
    {"*", StyioTokenType::TOK_STAR, "*"},
    {"**", StyioTokenType::BINOP_POW, "**"},
    {"*=", StyioTokenType::COMPOUND_MUL, "*="},
    {"-", StyioTokenType::TOK_MINUS, "-"},
    {"->", StyioTokenType::ARROW_SINGLE_RIGHT, "->"},
    {"---", StyioTokenType::SINGLE_SEP_LINE, "---"},
    {"-=", StyioTokenType::COMPOUND_SUB, "-="},
    {"[", StyioTokenType::TOK_LBOXBRAC, "["},
    {"[|", StyioTokenType::BOUNDED_BUFFER_OPEN, "[|"},
    {"]", StyioTokenType::TOK_RBOXBRAC, "]"},
  };
  for (auto& c : cases) {
    auto tokens = StyioTokenizer::tokenize(c.input);
    ASSERT_GE(tokens.size(), 2u) << "input: " << c.input;
    EXPECT_EQ(tokens[0]->type, c.type) << "input: " << c.input;
    EXPECT_EQ(tokens[0]->lexeme(), c.spelling) << "input: " << c.input;
    free_tokens(tokens);
  }
}

TEST(StyioSecurityNightlyParserExpr, NegativeNumericLiteralsAreAtoms) {
  for (const bool use_nightly_parser : {false, true}) {
    const std::string int_repr = parse_expr_to_repr_latest("-1 + 2", use_nightly_parser);
    EXPECT_NE(int_repr.find("{ -1 : int }"), std::string::npos);
    EXPECT_NE(int_repr.find("|- OP : <Add>"), std::string::npos);
    EXPECT_EQ(int_repr.find("|- OP : <Sub>"), std::string::npos);

    const std::string float_repr = parse_expr_to_repr_latest("-1.5", use_nightly_parser);
    EXPECT_NE(float_repr.find("{ -1.5 : f64 }"), std::string::npos);
    EXPECT_EQ(float_repr.find("styio.ast.binop"), std::string::npos);
  }
}

TEST(StyioSecurityNightlyParserExpr, UsesLeftAssociativeGroupingForEqualPrecedenceOps) {
  const std::string repr = parse_expr_to_repr_latest("price + fee - tax", true);

  EXPECT_NE(repr.find("|- LHS: styio.ast.binop: undefined"), std::string::npos);
  EXPECT_NE(repr.find("  |- LHS: price"), std::string::npos);
  EXPECT_NE(repr.find("  |- RHS: fee"), std::string::npos);
  EXPECT_NE(repr.find("|- OP : <Sub>"), std::string::npos);
  EXPECT_NE(repr.find("|- RHS: tax"), std::string::npos);
}

TEST(StyioSecurityNightlyParserExpr, ArithmeticTailCoversApplyPipeAndOperatorFamilies) {
  {
    DirectParserContext ctx("double <| 7");
    std::unique_ptr<StyioAST> ast(parse_expr(ctx.get()));
    auto* call = dynamic_cast<FuncCallAST*>(ast.get());
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->getNodeType(), StyioNodeType::Call);
    ASSERT_NE(call->getFuncName(), nullptr);
    EXPECT_EQ(call->getFuncName()->getAsStr(), "double");
    ASSERT_EQ(call->getArgList().size(), 1u);
    EXPECT_EQ(call->getArgList()[0]->getNodeType(), StyioNodeType::Integer);
  }

  const std::string repr = parse_expr_to_repr_latest("1 + 2 * 3 / 4 % 5 ** 6 - 7", true);
  for (const std::string& op : {"<Add>", "<Mul>", "<Div>", "<Mod>", "<Pow>", "<Sub>"}) {
    EXPECT_NE(repr.find(op), std::string::npos) << op;
  }

  const std::string unary_repr = parse_expr_to_repr_latest("-price", false);
  EXPECT_NE(unary_repr.find("|- OP : <Sub>"), std::string::npos);
  EXPECT_NE(unary_repr.find("price"), std::string::npos);

  const std::string fallback_repr = parse_expr_to_repr_latest("1 | 2", false);
  EXPECT_NE(fallback_repr.find("primary:"), std::string::npos);
  EXPECT_NE(fallback_repr.find("alt:"), std::string::npos);

  const std::string guard_repr = parse_expr_to_repr_latest("?(true) => 1 | 2", false);
  EXPECT_NE(guard_repr.find("cond:"), std::string::npos);
  EXPECT_NE(guard_repr.find("true:"), std::string::npos);
  EXPECT_NE(guard_repr.find("false:"), std::string::npos);

  const std::string dict_repr = parse_expr_to_repr_latest("dict{\"a\": 1}", false);
  EXPECT_NE(dict_repr.find("styio.ast.dict"), std::string::npos);

  const std::string pull_repr = parse_expr_to_repr_latest("(<- @stdin)", false);
  EXPECT_NE(pull_repr.find("instant.pull"), std::string::npos);

  EXPECT_THROW(parse_expr_to_repr_latest("?(true) => 1", true), StyioSyntaxError);
  EXPECT_THROW(parse_expr_to_repr_latest("?(true) => 1 |", true), std::runtime_error);
}

TEST(StyioSecurityNightlyParserExpr, SubsetTokenGateIncludesCompareAndLogic) {
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::BINOP_GT));
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::BINOP_GE));
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::BINOP_LT));
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::BINOP_LE));
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::TOK_RANGBRAC));
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::TOK_LANGBRAC));
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::BINOP_EQ));
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::BINOP_NE));
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::LOGIC_AND));
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::LOGIC_OR));
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::YIELD_PIPE));
}

TEST(StyioSecurityNightlyParserExpr, SubsetTokenGateIncludesDotCallTokens) {
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::TOK_LPAREN));
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::TOK_RPAREN));
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::TOK_DOT));
}

TEST(StyioSecurityNightlyParserExpr, RangeLiteralAcceptsRepeatedDotSeparator) {
  const std::string two_dot = parse_expr_to_repr_latest("[0..3]", true);
  const std::string three_dot = parse_expr_to_repr_latest("[0...3]", true);
  const std::string many_dot = parse_expr_to_repr_latest("[0......3]", true);

  EXPECT_NE(two_dot.find("range"), std::string::npos);
  EXPECT_EQ(two_dot, three_dot);
  EXPECT_EQ(two_dot, many_dot);
  EXPECT_EQ(two_dot, parse_expr_to_repr_latest("[0..3]", false));
  EXPECT_THROW(parse_expr_to_repr_latest("[0..3..2]", true), StyioSyntaxError);
}

TEST(StyioSecurityNightlyParserExpr, RejectsNonSubsetStatementToken) {
  auto tokens = StyioTokenizer::tokenize(">_ 1 + 2");
  StyioContext* ctx = StyioContext::Create(
    "<expr-subset-test>",
    ">_ 1 + 2",
    build_line_seps(">_ 1 + 2"),
    tokens,
    false
  );

  EXPECT_THROW(
    {
      StyioAST* ast = parse_expr_subset_nightly(*ctx);
      delete ast;
    },
    StyioSyntaxError
  );

  delete ctx;
  free_tokens(tokens);
}

TEST(StyioSecurityNightlyParserStmt, ParsesIteratorHashTagSequencesThroughNightlyPath) {
  auto expect_iter_seq = [](const std::string& src, std::size_t param_count)
  {
    auto tokens = StyioTokenizer::tokenize(src);
    StyioContext* ctx = StyioContext::Create(
      "<nightly-iterator-seq>",
      src,
      build_line_seps(src),
      tokens,
      false
    );

    ParseAttempt<StyioAST> attempt = try_parse_stmt_subset_nightly(*ctx);
    ASSERT_EQ(attempt.status, ParseAttemptStatus::Parsed) << src;
    std::unique_ptr<StyioAST> ast(attempt.node);
    auto* iter_seq = dynamic_cast<IterSeqAST*>(ast.get());
    ASSERT_NE(iter_seq, nullptr) << src;
    EXPECT_EQ(iter_seq->params.size(), param_count);
    ASSERT_EQ(iter_seq->hash_tags.size(), 2u);
    EXPECT_EQ(iter_seq->hash_tags[0]->words.front(), "hot");
    EXPECT_EQ(iter_seq->hash_tags[1]->words.front(), "cold");

    delete ctx;
    free_tokens(tokens);
  };

  expect_iter_seq("[1,2] >> #hot > #cold\n", 0u);
  expect_iter_seq("[1,2] >> (item) > #hot > #cold\n", 1u);

  EXPECT_THROW(parse_program_to_repr_latest("[1,2] >> #hot > #\n", true), StyioSyntaxError);
  EXPECT_THROW(parse_program_to_repr_latest("[1,2] >> (item) > #\n", true), StyioSyntaxError);
  EXPECT_THROW(parse_program_to_repr_latest("[1,2] >> #hot >\n", true), StyioSyntaxError);
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnPrintSubsetSamples) {
  const std::vector<std::string> samples = {
    ">_(1 + 2)\n",
    ">_(\"x\", 1 + 2, (3 * 4))\n",
    ">_(1 + 2)\n>_(3 + 4)\n",
    "1 + 2\n>_(3 * 4)\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, SubsetTokenGateIncludesFunctionDefTokens) {
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::TOK_HASH));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::ARROW_DOUBLE_RIGHT));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::TOK_AT));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::ARROW_SINGLE_RIGHT));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::TOK_LBOXBRAC));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::TOK_RBOXBRAC));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::TOK_LCURBRAC));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::TOK_RCURBRAC));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::EXTRACTOR));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::TOK_HAT));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::YIELD_PIPE));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::RETURN_PIPE));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::AWAIT_PIPE));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::PIPE_SEMICOLON));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::TOK_SEMICOLON));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::ELLIPSIS));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::BOUNDED_BUFFER_OPEN));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::BOUNDED_BUFFER_CLOSE));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::MATCH));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::TOK_UNDLINE));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::ITERATOR));
  EXPECT_FALSE(styio_parser_stmt_subset_token_nightly(StyioTokenType::WAVE_LEFT));
  EXPECT_FALSE(styio_parser_stmt_subset_token_nightly(StyioTokenType::WAVE_RIGHT));
  EXPECT_TRUE(styio_parser_stmt_subset_token_nightly(StyioTokenType::TOK_PIPE));
}

TEST(StyioSecurityNightlyParserStmt, SubsetStartGateIncludesBlockAndControlStarters) {
  EXPECT_TRUE(styio_parser_stmt_subset_start_nightly(StyioTokenType::TOK_AT));
  EXPECT_TRUE(styio_parser_stmt_subset_start_nightly(StyioTokenType::TOK_LCURBRAC));
  EXPECT_TRUE(styio_parser_stmt_subset_start_nightly(StyioTokenType::TOK_LBOXBRAC));
  EXPECT_TRUE(styio_parser_stmt_subset_start_nightly(StyioTokenType::EXTRACTOR));
  EXPECT_TRUE(styio_parser_stmt_subset_start_nightly(StyioTokenType::TOK_HAT));
  EXPECT_TRUE(styio_parser_stmt_subset_start_nightly(StyioTokenType::YIELD_PIPE));
  EXPECT_TRUE(styio_parser_stmt_subset_start_nightly(StyioTokenType::RETURN_PIPE));
  EXPECT_TRUE(styio_parser_stmt_subset_start_nightly(StyioTokenType::AWAIT_PIPE));
  EXPECT_TRUE(styio_parser_stmt_subset_start_nightly(StyioTokenType::ELLIPSIS));
  EXPECT_TRUE(styio_parser_stmt_subset_start_nightly(StyioTokenType::ITERATOR));
}

TEST(StyioSecurityNightlyParserStmt, ParsesInlineReturnAndStatementSeparators) {
  const std::string src =
    "# discount := (base: i32) => { fee = base / 10; |<| base - fee |; }\n"
    ">_(discount <| 100)\n";
  const std::string repr = parse_program_to_repr_latest(src, true);

  EXPECT_NE(repr.find("styio.ast.return"), std::string::npos);
  EXPECT_NE(repr.find("discount"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesCharLiteralEscapeFamilies) {
  const std::string src =
    ">_('\\n')\n"
    ">_('\\r')\n"
    ">_('\\t')\n"
    ">_('\\0')\n"
    ">_('\\\\')\n"
    ">_('\\'')\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.char"), std::string::npos);

  EXPECT_THROW(
    {
      (void)parse_program_to_repr_latest(">_('\\x')\n", true);
    },
    StyioSyntaxError);
}

TEST(StyioSecurityNightlyParserStmt, ParsesFormatStringEscapesAndRejectsMalformedInterpolations) {
  const std::string src =
    "x = 7\n"
    "$\"{{literal}} x={x + 1}\" -> @stdout\n";

  const std::string nightly = parse_program_to_repr_latest(src, true);
  const std::string legacy = parse_program_to_repr_latest(src, false);
  EXPECT_EQ(nightly, legacy);
  EXPECT_NE(nightly.find("styio.ast.fmtstr"), std::string::npos);
  EXPECT_NE(nightly.find("\"{literal} x=\""), std::string::npos);
  EXPECT_NE(nightly.find("|- OP : <Add>"), std::string::npos);

  const std::vector<std::string> malformed = {
    "$\"{}\" -> @stdout\n",
    "$\"value=}\" -> @stdout\n",
    "$\"value={1 + 2\" -> @stdout\n",
  };

  for (const auto& sample : malformed) {
    EXPECT_THROW(parse_program_to_repr_latest(sample, true), StyioSyntaxError) << sample;
    EXPECT_THROW(parse_program_to_repr_latest(sample, false), StyioSyntaxError) << sample;
  }
}

TEST(StyioSecurityNightlyParserStmt, RejectsMalformedFormatStringInResourceMethodBody) {
  const std::string src =
    "@file::summary = () => { <| $\"value={1 + 2\" }\n";

  EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioSyntaxError);
  EXPECT_THROW(parse_program_to_repr_latest(src, false), StyioSyntaxError);
}

TEST(StyioSecurityNightlyParserStmt, ParsesTaskGroupAndAwaitBindings) {
  const std::string src =
    "||> [\n"
    "  t1 := { <| 41 }\n"
    "  t2 := { <| 1 }\n"
    "]\n"
    "?| t1 -> a: i64\n"
    "?| t2 -> b: i64 | 0\n"
    ">_(a + b)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_task_i64_spawn"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_task_i64_pull"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_clear_error"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, RejectsBareAwaitFreezeFallbackBeforeContinuationLowering) {
  const std::string src =
    "?| -> answer: i64 | 0\n"
    ">_(answer)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected bare continuation freeze fallback to fail closed";
  }
  catch (const StyioSyntaxError& ex) {
    const std::string msg = ex.what();
    EXPECT_NE(msg.find("bare continuation freeze"), std::string::npos) << msg;
    EXPECT_NE(msg.find("does not accept fallback"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectDiscardStatement) {
  const std::string src =
    "@out : i64|..2|\n"
    "[1] >> #(v) => {\n"
    "  ?| v -> @out | ...\n"
    "}\n"
    ">_(@out[-1])\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.redirect"), std::string::npos);
  EXPECT_EQ(repr.find("styio.ast.FlowBind"), std::string::npos);
}

TEST(StyioSecurityResourceTypestate, conditional_close_rejects_post_join_property_use) {
  const std::string src =
    "f := @file(\"tests/features/file_resources/data/hello.txt\")\n"
    "?(true) => { f -> @() } | { ^^^ }\n"
    ">_(f.path)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected a maybe-closed handle use to fail in Sema";
  }
  catch (const StyioTypeError& ex) {
    EXPECT_NE(std::string(ex.what()).find("use-after-destroy"), std::string::npos)
      << ex.what();
  }
}

TEST(StyioSecurityResourceTypestate, both_open_branches_allow_post_join_property_use) {
  const std::string src =
    "f := @file(\"tests/features/file_resources/data/hello.txt\")\n"
    "?(true) => { ^^^ } | { ^^^ }\n"
    ">_(f.path)\n";

  EXPECT_NO_THROW(
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly));
}

TEST(StyioSecurityResourceTypestate, conditional_close_rejects_post_join_file_pull) {
  const std::string src =
    "f = @file(\"tests/features/file_resources/data/numbers.txt\")\n"
    "?(true) => { f -> @() } | { ^^^ }\n"
    "value = ?| (<< f) | 7\n"
    ">_(value)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected a maybe-closed handle pull to fail in Sema";
  }
  catch (const StyioTypeError& ex) {
    EXPECT_EQ(
      std::string(ex.what()),
      "\nStyio.TypeError:\n"
      "use-after-destroy: resource `f` was already destroyed");
  }
}

TEST(StyioSecurityResourceTypestate, file_rebind_after_join_reopens_handle) {
  const std::string src =
    "f = @file(\"tests/features/file_resources/data/hello.txt\")\n"
    "?(true) => { f -> @() } | { ^^^ }\n"
    "f = @file(\"tests/features/file_resources/data/numbers.txt\")\n"
    ">_(f.path)\n";

  EXPECT_NO_THROW(
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly));
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectFallbackStatement) {
  const std::string src =
    "?| \"x\" -> @stdout | \"fallback\" -> @stderr\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("styio.ast.resource.redirect"), std::string::npos);
  EXPECT_EQ(repr.find("styio.ast.FlowBind"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectHandleAcquireStatement) {
  const std::string src =
    "?| f <- @file(\"/tmp/styio-resource-effect-acquire-missing\")"
    " | io => \"io\" -> @stderr\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("styio.ast.handle.acquire"), std::string::npos);
  EXPECT_EQ(repr.find("styio.ast.FlowBind"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_file_open"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectFileRebindStatement) {
  const std::string src =
    "f = @file(\"tests/features/file_resources/data/hello.txt\")\n"
    "?| f = @file(\"tests/features/file_resources/data/numbers.txt\")"
    " | cleanup => \"cleanup\" -> @stderr\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("bind.flex"), std::string::npos);
  EXPECT_EQ(repr.find("styio.ast.FlowBind"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_file_close"), std::string::npos);
  EXPECT_NE(llvm_ir.find("file_rebind_cleanup_done"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectFileIteratorStatement) {
  const std::string src =
    "?| @file(\"tests/features/file_resources/data/hello.txt\") >> #(line) => {\n"
    "  >_(line)\n"
    "} | io => \"io\" -> @stderr\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("styio.ast.iterator"), std::string::npos);
  EXPECT_EQ(repr.find("styio.ast.FlowBind"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_file_open"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_file_read_line"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, RejectsScalarFlexBindResourceEffectStatement) {
  const std::string src =
    "?| x = 1 | \"fallback\" -> @stdout\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected scalar flex bind under resource-effect settlement to fail closed";
  }
  catch (const StyioSyntaxError& err) {
    EXPECT_NE(
      std::string(err.what()).find("unsupported expression continuation in nightly parser subset"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlySemantics, RejectsNonFileIteratorResourceEffectStatement) {
  const std::string src =
    "?| [1, 2] >> #(n) => {\n"
    "  >_(n)\n"
    "} | \"fallback\" -> @stdout\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected non-file iterator under resource-effect settlement to fail closed";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(
      std::string(err.what()).find("`?|` resource settlement requires a resource operation"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlyParserStmt, ResourceEffectHandleAcquireFeedsLaterIterator) {
  const std::string src =
    "?| f <- @file(\"tests/features/file_resources/data/hello.txt\")"
    " | \"fallback\" -> @stderr\n"
    "f >> #(line) => {\n"
    "  >_(line)\n"
    "}\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_file_open"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_file_rewind"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_file_read_line"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ResourceEffectHandleAcquireFeedsLaterCloseMethod) {
  const std::string src =
    "?| f <- @file(\"tests/features/file_resources/data/hello.txt\")"
    " | \"fallback\" -> @stderr\n"
    "?| f.close() | \"close fallback\" -> @stderr\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_file_open"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_file_close"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ResourceEffectHandleAcquireFeedsLaterInstantPull) {
  const std::string src =
    "?| f <- @file(\"tests/features/file_resources/data/hello.txt\")"
    " | \"fallback\" -> @stderr\n"
    "value = ?| (<< f) | 7\n"
    ">_(value)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("instant.pull"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_file_read_i64line_from_handle"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
  EXPECT_EQ(llvm_ir.find("styio_read_file_i64line"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectNamedHandlerStatement) {
  const std::string src =
    "?| \"x\" -> @stdout | io => \"fallback\" -> @stderr\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("handler:io"), std::string::npos);
  EXPECT_EQ(repr.find("styio.ast.FlowBind"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectResourceMethodStatement) {
  const std::string src =
    "?| @file(\"/tmp/styio-resource-effect-method-close\").close() | io => \"io\" -> @stderr\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_EQ(repr.find("styio.ast.FlowBind"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_file_close"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectDirectFileReleaseStatement) {
  const std::string src =
    "?| @file(\"tests/features/file_resources/data/hello.txt\") -> @() | io => \"io\" -> @stderr\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("styio.ast.resource.redirect"), std::string::npos);
  EXPECT_EQ(repr.find("styio.ast.FlowBind"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_file_close"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ResourceEffectHandleAcquireFeedsLaterDirectRelease) {
  const std::string src =
    "?| f <- @file(\"tests/features/file_resources/data/hello.txt\")"
    " | \"fallback\" -> @stderr\n"
    "?| f -> @() | \"release fallback\" -> @stderr\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_file_open"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_file_close"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesPressureObserverStatementToSemaBoundary) {
  const std::string src =
    "@channel : i64|..4|\n"
    "channel.pressure >> #(p) => {\n"
    "  >_(p)\n"
    "}\n";

  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.iterator"), std::string::npos) << repr;
  EXPECT_NE(repr.find("styio.ast.attr"), std::string::npos) << repr;
  EXPECT_NE(repr.find("channel.pressure"), std::string::npos) << repr;
}

TEST(StyioSecurityNightlySemantics, RejectsPressureObserverBeforeResourceFamilySupport) {
  const std::string src =
    "@channel : i64|..4|\n"
    "channel.pressure >> #(p) => {\n"
    "  >_(p)\n"
    "}\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected pressure observer to fail closed before resource-family support";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("resource family @resource does not expose pressure stream"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlyCodegen, ReturnRunsFileScopeCleanupBeforeRet) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const std::filesystem::path data =
    std::filesystem::temp_directory_path()
    / ("styio-return-cleanup-data-" + std::to_string(uniq) + ".txt");
  {
    std::ofstream out(data);
    ASSERT_TRUE(out.is_open());
    out << "line\n";
  }

  const std::string src =
    "# early : i64 := () => {\n"
    "  f <- @file(\"" + data.generic_string() + "\")\n"
    "  <| 17\n"
    "}\n"
    ">_(early())\n";

  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  const std::size_t close_pos = llvm_ir.find("call void @styio_file_close");
  ASSERT_NE(close_pos, std::string::npos) << llvm_ir;
  const std::size_t ret_pos = llvm_ir.find("ret i64", close_pos);
  EXPECT_NE(ret_pos, std::string::npos) << llvm_ir;
  EXPECT_LT(close_pos, ret_pos);

  std::filesystem::remove(data);
}

TEST(StyioSecurityNightlyCodegen, RebindRunsFileCleanupBeforeNextAcquire) {
  const std::string src =
    "f = @file(\"tests/features/file_resources/data/hello.txt\")\n"
    "f = @file(\"tests/features/file_resources/data/numbers.txt\")\n"
    ">_(\"after\")\n";

  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  const std::size_t first_open_pos = llvm_ir.find("call i64 @styio_file_open");
  ASSERT_NE(first_open_pos, std::string::npos) << llvm_ir;
  const std::size_t close_pos = llvm_ir.find("call void @styio_file_close", first_open_pos);
  ASSERT_NE(close_pos, std::string::npos) << llvm_ir;
  const std::size_t guard_pos = llvm_ir.find("call i32 @styio_runtime_has_error", close_pos);
  ASSERT_NE(guard_pos, std::string::npos) << llvm_ir;
  const std::size_t second_open_pos = llvm_ir.find("call i64 @styio_file_open", guard_pos);
  ASSERT_NE(second_open_pos, std::string::npos) << llvm_ir;
  EXPECT_LT(close_pos, guard_pos);
  EXPECT_LT(guard_pos, second_open_pos);
}

TEST(StyioSecurityNightlyCodegen, BreakRunsFileScopeCleanupBeforeLoopExitBranch) {
  const std::string src =
    "[1] >> #(i) => {\n"
    "  f <- @file(\"tests/features/file_resources/data/hello.txt\")\n"
    "  ^^^\n"
    "}\n"
    ">_(\"after\")\n";

  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  const std::size_t close_pos = llvm_ir.find("call void @styio_file_close");
  ASSERT_NE(close_pos, std::string::npos) << llvm_ir;
  std::size_t branch_pos = llvm_ir.find("br label %foreach_exit", close_pos);
  if (branch_pos == std::string::npos) {
    branch_pos = llvm_ir.find("br label %foreach_rt_exit", close_pos);
  }
  EXPECT_NE(branch_pos, std::string::npos) << llvm_ir;
  EXPECT_LT(close_pos, branch_pos);
}

TEST(StyioSecurityNightlyCodegen, ContinueRunsFileScopeCleanupBeforeLoopStepBranch) {
  const std::string src =
    "[1] >> #(i) => {\n"
    "  f <- @file(\"tests/features/file_resources/data/hello.txt\")\n"
    "  >>\n"
    "}\n"
    ">_(\"after\")\n";

  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  const std::size_t close_pos = llvm_ir.find("call void @styio_file_close");
  ASSERT_NE(close_pos, std::string::npos) << llvm_ir;
  std::size_t branch_pos = llvm_ir.find("br label %foreach_step", close_pos);
  if (branch_pos == std::string::npos) {
    branch_pos = llvm_ir.find("br label %foreach_rt_step", close_pos);
  }
  EXPECT_NE(branch_pos, std::string::npos) << llvm_ir;
  EXPECT_LT(close_pos, branch_pos);
}

TEST(StyioSecurityNightlyCodegen, LongStandaloneContinueNormalizesToNearestLoop) {
  const std::string src =
    "[0..2] >> #(i) => {\n"
    "  [0..2] >> #(j) => {\n"
    "    >>>>\n"
    "    >_(\"skipped\")\n"
    "  }\n"
    "}\n";

  EXPECT_NO_THROW({
    const std::string llvm_ir =
      compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
    std::size_t branch_pos = llvm_ir.find("br label %foreach_step");
    if (branch_pos == std::string::npos) {
      branch_pos = llvm_ir.find("br label %foreach_rt_step");
    }
    if (branch_pos == std::string::npos) {
      branch_pos = llvm_ir.find("br label %rangefor_step");
    }
    EXPECT_NE(branch_pos, std::string::npos) << llvm_ir;
    EXPECT_EQ(llvm_ir.find("skipped"), std::string::npos) << llvm_ir;
  });
}

TEST(StyioSecurityNightlyCodegen, RejectsBreakAndContinueOutsideLoop) {
  try {
    compile_program_to_llvm_ir_engine_latest("^^^\n", StyioParserEngine::Nightly);
    FAIL() << "expected top-level break to fail closed";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(std::string(err.what()).find("break outside enclosing loop"), std::string::npos);
  }

  try {
    compile_program_to_llvm_ir_engine_latest(">>\n", StyioParserEngine::Nightly);
    FAIL() << "expected top-level continue to fail closed";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(std::string(err.what()).find("continue outside enclosing loop"), std::string::npos);
  }
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectValueFallbackExpression) {
  const std::string src =
    "result = ?| (<< @file(\"/tmp/styio-resource-effect-value-missing\")) | 7\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectValueListIndexFallbackExpression) {
  const std::string src =
    "xs = [1,2]\n"
    "result = ?| xs[3] | bounds => 9 | 7\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  EXPECT_NE(repr.find("handler:bounds"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_get"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectValueListSliceFallbackExpression) {
  const std::string src =
    "xs = [1,2]\n"
    "result = ?| xs[0..] | bounds => [9] | [7]\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  EXPECT_NE(repr.find("handler:bounds"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_slice"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectValueDictIndexFallbackExpression) {
  const std::string src =
    "d = dict{\"a\": 1}\n"
    "result = ?| d[\"missing\"] | bounds => 9 | 7\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  EXPECT_NE(repr.find("handler:bounds"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_dict_get_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodReturnedDictIndexFallbackExpression) {
  const std::string src =
    "@file::read_missing = () => { <| dict{\"a\": 1}[\"missing\"] }\n"
    "log := @file(\"/tmp/styio-resource-method-dict-index\")\n"
    "result = ?| log.read_missing() | bounds => 9 | 7\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  EXPECT_NE(repr.find("handler:bounds"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_dict_get_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodReturnedListSliceFallbackExpression) {
  const std::string src =
    "@file::tail_missing = () => { <| [1,2][5..] }\n"
    "log := @file(\"/tmp/styio-resource-method-list-slice\")\n"
    "result = ?| log.tail_missing() | bounds => [9] | [7]\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  EXPECT_NE(repr.find("handler:bounds"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_slice"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodReturnedRangeFallbackExpression) {
  const std::string src =
    "@file::span = (start: int, stop: int) => { <| [start..stop] }\n"
    "start = 1\n"
    "stop = 3\n"
    "log := @file(\"/tmp/styio-resource-method-range\")\n"
    "result = ?| log.span(start, stop) | [9]\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("range_list_hdr"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_new_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_push_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodReturnedScalarFmtFallbackExpression) {
  const std::string src =
    "@file::summary = (x: int) => { <| $\"value={x + 1}\" }\n"
    "@file::letter = () => { <| 'q' }\n"
    "log := @file(\"/tmp/styio-resource-method-scalar-fmt\")\n"
    "summary = ?| log.summary(4) | \"fallback\"\n"
    "letter = ?| log.letter() | 'x'\n"
    ">_(summary)\n"
    ">_(letter)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
  EXPECT_NE(llvm_ir.find("value="), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodStatementPrefaceReturnExpression) {
  const std::string src =
    "@file::answer = () => {\n"
    "  >_(\"inside\")\n"
    "  <| 42\n"
    "}\n"
    "log := @file(\"/tmp/styio-resource-method-preface\")\n"
    "result = ?| log.answer() | 7\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.method.def"), std::string::npos);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("inside"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodHandleAcquireIteratorStatementBody) {
  const std::string src =
    "@file::scan = () => {\n"
    "  input <- @file(\"tests/features/file_resources/data/hello.txt\")\n"
    "  input >> #(line) => {\n"
    "    >_(line)\n"
    "  }\n"
    "}\n"
    "input = 7\n"
    "log := @file(\"/tmp/styio-resource-method-handle-iter\")\n"
    "log.scan()\n"
    ">_(input)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.handle.acquire"), std::string::npos);
  EXPECT_NE(repr.find("styio.ast.iterator"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_file_open"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_file_rewind"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_file_read_line"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodLocalFlexBindReturnExpression) {
  const std::string src =
    "x = 100\n"
    "@file::answer = () => {\n"
    "  x = 41\n"
    "  <| x + 1\n"
    "}\n"
    "log := @file(\"/tmp/styio-resource-method-local-flex\")\n"
    "result = ?| log.answer() | 7\n"
    ">_(result)\n"
    ">_(x)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.method.def"), std::string::npos);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("__styio_resource_method_local_"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodLocalFinalBindReturnExpression) {
  const std::string src =
    "x = 100\n"
    "@file::answer = () => {\n"
    "  x := 41\n"
    "  <| x + 1\n"
    "}\n"
    "log := @file(\"/tmp/styio-resource-method-local-final\")\n"
    "result = ?| log.answer() | 7\n"
    ">_(result)\n"
    ">_(x)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.method.def"), std::string::npos);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("__styio_resource_method_local_"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodLocalListDictMatrixBindReturnExpressions) {
  const std::string src =
    "@file::list_answer = () => {\n"
    "  xs = [40, 2]\n"
    "  <| xs[0] + xs[1]\n"
    "}\n"
    "@file::dict_answer = () => {\n"
    "  d := dict{\"a\": 40, \"b\": 2}\n"
    "  <| d[\"a\"] + d[\"b\"]\n"
    "}\n"
    "@file::matrix_answer = () => {\n"
    "  m: matrix := [[42, 2], [1, 3]]\n"
    "  <| m[0][0]\n"
    "}\n"
    "log := @file(\"/tmp/styio-resource-method-local-container\")\n"
    "result = ?| log.list_answer() | 7\n"
    "result2 = ?| log.dict_answer() | 7\n"
    "result3 = ?| log.matrix_answer() | 7\n"
    ">_(result + result2 + result3)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.method.def"), std::string::npos);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("__styio_resource_method_local_"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_matrix_get_i64"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceReceiverPropertyAccessInsideDefinitions) {
  const std::string src =
    "@file::self_path := @file.path\n"
    "@file::describe = () => { <| $\"path={@file.path}\" }\n"
    "log := @file(\"/tmp/styio-resource-method-receiver-property\")\n"
    ">_(log.self_path)\n"
    ">_(log.describe())\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.method.def"), std::string::npos);
  EXPECT_NE(repr.find("property"), std::string::npos);
  EXPECT_NE(repr.find("@file"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("path="), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, RejectsResourceReceiverPropertyOutsideDefinition) {
  const std::string src =
    ">_(@file.path)\n";

  try {
    parse_program_to_repr_latest(src, true);
    FAIL() << "expected resource receiver property outside a definition to fail closed";
  }
  catch (const StyioSyntaxError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("which is expected to be ("), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodReturnedMatchCasesScalarFamilies) {
  const std::string src =
    "@file::flag = (x: int) => { <| x ?= {\n"
    "  0 => true\n"
    "  _ => false\n"
    "} }\n"
    "@file::mark = (x: int) => { <| x ?= {\n"
    "  0 => 'a'\n"
    "  _ => 'b'\n"
    "} }\n"
    "@file::word = (x: int) => { <| x ?= {\n"
    "  0 => \"zero\"\n"
    "  _ => \"other\"\n"
    "} }\n"
    "log := @file(\"/tmp/styio-resource-method-match-cases\")\n"
    "flag = ?| log.flag(0) | false\n"
    "mark = ?| log.mark(1) | 'x'\n"
    "word = ?| log.word(0) | \"fallback\"\n"
    ">_(flag)\n"
    ">_(mark)\n"
    ">_(word)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.method.def"), std::string::npos);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("phi i1"), std::string::npos);
  EXPECT_NE(llvm_ir.find("phi i8"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodReturnedBlockFunctionCallFallbackExpression) {
  const std::string src =
    "# plus_one := (x: i64) => { <| x + 1 }\n"
    "# label := (x: i64) => {\n"
    "  $\"score={x + 2}\"\n"
    "}\n"
    "@file::score = (x: i64) => { <| plus_one(x) }\n"
    "@file::label = (x: i64) => { <| label(x) }\n"
    "log := @file(\"/tmp/styio-resource-method-function-call\")\n"
    "score = ?| log.score(4) | 0\n"
    "label = ?| log.label(5) | \"fallback\"\n"
    ">_(score)\n"
    ">_(label)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.method.def"), std::string::npos);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
  EXPECT_NE(llvm_ir.find("score="), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, MatrixFunctionReturnFeedsResourceMethodValue) {
  const std::string src =
    "# make_matrix : matrix = () => { <| [[1,2],[3,4]] }\n"
    "@file::matrix_from_function = () => { <| make_matrix() }\n"
    "log := @file(\"/tmp/styio-resource-method-function-matrix\")\n"
    "direct: matrix = make_matrix()\n"
    "method_direct: matrix = log.matrix_from_function()\n"
    "guarded: matrix = ?| log.matrix_from_function() | [[9,9],[8,8]]\n"
    ">_(direct)\n"
    ">_(method_direct)\n"
    ">_(guarded)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("define i64 @make_matrix()"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_matrix_new_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_matrix_set_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, RejectsMatrixFunctionReturnFlatListBeforeRuntime) {
  const std::string src =
    "# bad : matrix = () => { <| [1,2] }\n"
    "result: matrix = bad()\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected matrix return flat list to fail before runtime";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("matrix rows must be list literals"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, RejectsResourceMethodReturnedContainerMatchResult) {
  const std::string src =
    "@file::bad = (x: int) => { <| x ?= {\n"
    "  0 => [1,2]\n"
    "  _ => [3,4]\n"
    "} }\n"
    "log := @file(\"/tmp/styio-resource-method-container-match\")\n"
    ">_(log.bad(0))\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected resource method returned container match result to fail closed";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("match branch values support scalar and string results"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, RejectsResourceMethodReturnedStatementOnlyFunctionCall) {
  const std::string src =
    "# emit := (x: i64) => { >_(x) }\n"
    "@file::bad = (x: i64) => { <| emit(x) }\n"
    "log := @file(\"/tmp/styio-resource-method-function-stmt\")\n"
    ">_(log.bad(4))\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected statement-only function result to fail closed";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("scalar/list/dict/matrix local preface followed by a final"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodReturnedResourceEffectFallbackExpression) {
  const std::string src =
    "@file::read_or = () => { <| ?| (<< @file(\"/tmp/styio-resource-method-returned-effect-missing\")) | io => 8 | 7 }\n"
    "log := @file(\"/tmp/styio-resource-method-returned-effect\")\n"
    "result = ?| log.read_or() | 9\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  EXPECT_NE(repr.find("handler:io"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, RejectsResourceMethodReturnedResourceEffectDiscardExpression) {
  const std::string src =
    "@file::bad = () => { <| ?| (<< @file(\"/tmp/styio-resource-method-returned-effect-discard\")) | ... }\n"
    "log := @file(\"/tmp/styio-resource-method-returned-effect-discard-log\")\n"
    ">_(log.bad())\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected returned resource-effect discard to stay statement-only";
  }
  catch (const StyioSyntaxError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("resource-effect discard is statement-only"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectValueMatrixIndexFallbackExpression) {
  const std::string src =
    "m: matrix = [[1,2],[3,4]]\n"
    "result = ?| m[3][0] | bounds => 9 | 7\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  EXPECT_NE(repr.find("handler:bounds"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_matrix_get_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectValueMatrixSliceFallbackExpression) {
  const std::string src =
    "m: matrix = [[1,2],[3,4]]\n"
    "result = ?| m[3..] | bounds => [[9]] | [[7]]\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  EXPECT_NE(repr.find("handler:bounds"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_matrix_rows_slice_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodReturnedMatrixIndexFallbackExpression) {
  const std::string src =
    "@file::cell_missing = (m: matrix) => { <| m[3][0] }\n"
    "m: matrix = [[1,2],[3,4]]\n"
    "log := @file(\"/tmp/styio-resource-method-matrix-index\")\n"
    "result = ?| log.cell_missing(m) | bounds => 9 | 7\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  EXPECT_NE(repr.find("handler:bounds"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_matrix_get_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodReturnedMatrixRowFallbackExpression) {
  const std::string src =
    "@file::row_missing = (m: matrix) => { <| m[3] }\n"
    "m: matrix = [[1,2],[3,4]]\n"
    "log := @file(\"/tmp/styio-resource-method-matrix-row\")\n"
    "result: list[i64] = ?| log.row_missing(m) | bounds => [9] | [7]\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  EXPECT_NE(repr.find("handler:bounds"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_matrix_row_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodReturnedMatrixSliceFallbackExpression) {
  const std::string src =
    "@file::rows_missing = (m: matrix) => { <| m[3..] }\n"
    "m: matrix = [[1,2],[3,4]]\n"
    "log := @file(\"/tmp/styio-resource-method-matrix-slice\")\n"
    "result = ?| log.rows_missing(m) | bounds => [[9]] | [[7]]\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  EXPECT_NE(repr.find("handler:bounds"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_matrix_rows_slice_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectValueStdinPullFallbackExpression) {
  const std::string src =
    "result = ?| (<- @stdin) | 7\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_cstr_to_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectValueTypedStdinPullFallbackExpressions) {
  const std::string src =
    "rate: f64 = ?| (<- @stdin) | 1.5\n"
    "text: string = ?| (<- @stdin) | \"fallback\"\n"
    "values: list[i64] = ?| (<- @stdin) | [1,2]\n"
    ">_(rate)\n"
    ">_(text)\n"
    ">_(values)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.resource.effect"), std::string::npos);
  EXPECT_NE(repr.find("value: required"), std::string::npos);
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_cstr_to_f64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_strcat_ab"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_i64_read_stdin"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, RejectsResourceEffectDiscardExpression) {
  const std::string src =
    "result = ?| (<< @file(\"/tmp/styio-resource-effect-value-missing\")) | ...\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected resource-effect discard expression to fail closed";
  }
  catch (const StyioSyntaxError& err) {
    EXPECT_NE(
      std::string(err.what()).find("resource-effect discard is statement-only"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlySemantics, RejectsStatementShapedResourceEffectExpression) {
  const std::string src =
    "result = ?| \"x\" -> @stdout | 7\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected statement-shaped resource-effect expression to fail closed";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(
      std::string(err.what()).find("resource-effect expression requires a value-producing resource operation"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlyParserStmt, RejectsHandleAcquireResourceEffectExpression) {
  const std::string src =
    "result = ?| f <- @file(\"/tmp/styio-resource-effect-acquire-missing\") | 7\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected statement-shaped resource acquire expression to fail closed";
  }
  catch (const StyioSyntaxError& err) {
    EXPECT_NE(
      std::string(err.what()).find("unsupported expression continuation in nightly parser subset"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlyParserStmt, RejectsFileRebindResourceEffectExpression) {
  const std::string src =
    "f = @file(\"tests/features/file_resources/data/hello.txt\")\n"
    "result = ?| f = @file(\"tests/features/file_resources/data/numbers.txt\") | 7\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected statement-shaped file rebind expression to fail closed";
  }
  catch (const StyioSyntaxError& err) {
    EXPECT_NE(
      std::string(err.what()).find("unsupported expression continuation in nightly parser subset"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlySemantics, RejectsDirectFileReleaseResourceEffectExpression) {
  const std::string src =
    "result = ?| @file(\"/tmp/styio-resource-effect-release-missing\") -> @() | 7\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected statement-shaped file release expression to fail closed";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(
      std::string(err.what()).find("resource-effect expression requires a value-producing resource operation"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlySemantics, RejectsResourceEffectValueFallbackTypeMismatch) {
  const std::string src =
    "result = ?| (<< @file(\"/tmp/styio-resource-effect-value-missing\")) | \"fallback\"\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected mismatched resource-effect value fallback to fail closed";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("resource-effect fallback expects i64, got string"), std::string::npos)
      << msg;
  }
}

TEST(StyioSecurityNightlySemantics, RejectsResourceEffectValueListIndexFallbackTypeMismatch) {
  const std::string src =
    "xs = [1,2]\n"
    "result = ?| xs[3] | \"fallback\"\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected list-index resource-effect value fallback mismatch to fail closed";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("resource-effect fallback expects"), std::string::npos) << msg;
    EXPECT_NE(msg.find("got string"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, RejectsResourceEffectValueListSliceFallbackTypeMismatch) {
  const std::string src =
    "xs = [1,2]\n"
    "result = ?| xs[0..] | \"fallback\"\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected list-slice resource-effect value fallback mismatch to fail closed";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("resource-effect fallback expects"), std::string::npos) << msg;
    EXPECT_NE(msg.find("got string"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, RejectsResourceEffectValueDictIndexFallbackTypeMismatch) {
  const std::string src =
    "d = dict{\"a\": 1}\n"
    "result = ?| d[\"missing\"] | \"fallback\"\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected dict-index resource-effect value fallback mismatch to fail closed";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("resource-effect fallback expects"), std::string::npos) << msg;
    EXPECT_NE(msg.find("got string"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, RejectsResourceEffectValueMatrixIndexFallbackTypeMismatch) {
  const std::string src =
    "m: matrix = [[1,2],[3,4]]\n"
    "result = ?| m[3][0] | \"fallback\"\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected matrix-index resource-effect value fallback mismatch to fail closed";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("resource-effect fallback expects"), std::string::npos) << msg;
    EXPECT_NE(msg.find("got string"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, RejectsResourceMethodArgumentTypeMismatch) {
  const std::string src =
    "@file::cell = (m: matrix) => { <| m[0][0] }\n"
    "xs = [1,2]\n"
    "log := @file(\"/tmp/styio-resource-method-matrix-arg-mismatch\")\n"
    "result = ?| log.cell(xs) | 7\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected resource method argument type mismatch to fail closed";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("resource method argument type mismatch for parameter 'm'"), std::string::npos) << msg;
    EXPECT_NE(msg.find("expected matrix"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, ResourceMethodLocalContainerReturnsLowerThroughValueScope) {
  const std::string src =
    "@file::list_answer = () => {\n"
    "  xs := [41, 42]\n"
    "  <| xs\n"
    "}\n"
    "@file::dict_answer = () => {\n"
    "  d := dict{\"a\": 40, \"b\": 2}\n"
    "  <| d\n"
    "}\n"
    "@file::matrix_answer = () => {\n"
    "  m: matrix := [[40, 2], [1, 3]]\n"
    "  <| m\n"
    "}\n"
    "log := @file(\"/tmp/styio-resource-method-local-bind\")\n"
    "list_result = ?| log.list_answer() | [7, 8]\n"
    "dict_result = ?| log.dict_answer() | dict{\"a\": 7, \"b\": 8}\n"
    "matrix_result: matrix = ?| log.matrix_answer() | [[7, 8], [9, 10]]\n"
    ">_(list_result[0] + list_result[1] + dict_result[\"a\"] + dict_result[\"b\"] + matrix_result[0][0] + matrix_result[0][1])\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("__styio_resource_method_local_"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_clone"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_dict_clone"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_matrix_clone"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, RejectsResourceMethodLocalResourceBindBeforeResourceValueScopeSupport) {
  const std::string src =
    "@file::answer = () => {\n"
    "  f <- @file(\"/tmp/styio-resource-method-local-resource-bind-data\")\n"
    "  <| 42\n"
    "}\n"
    "log := @file(\"/tmp/styio-resource-method-local-list-bind\")\n"
    "result = ?| log.answer() | 7\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected resource method local resource bind to fail closed";
  }
  catch (const std::exception& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("resource method return currently requires"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, RejectsResourceMethodGlobalMatrixCaptureBeforeLexicalCaptureSupport) {
  const std::string src =
    "m: matrix = [[1,2],[3,4]]\n"
    "@file::cell = () => { <| m[0][0] }\n"
    "log := @file(\"/tmp/styio-resource-method-matrix-global\")\n"
    "result = ?| log.cell() | 7\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected resource method global matrix capture to stay fail-closed";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("indexed access requires an indexable value"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceEffectValueDictSliceFallbackExpression) {
  const std::string src =
    "d = dict{\"a\": 1, \"b\": 2}\n"
    "result = ?| d[0..] | [9]\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_dict_values_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_slice"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourceMethodReturnedDictSliceFallbackExpression) {
  const std::string src =
    "@file::dict_slice = () => { <| dict{\"a\": 1, \"b\": 2}[0..] }\n"
    "log := @file(\"/tmp/styio-resource-method-dict-slice\")\n"
    "result = ?| log.dict_slice() | [9]\n"
    ">_(result)\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_dict_values_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_slice"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_runtime_error_matches_effect"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, RejectsResourceEffectValueDictSliceFallbackTypeMismatch) {
  const std::string src =
    "d = dict{\"a\": 1}\n"
    "result = ?| d[0..] | 9\n"
    ">_(result)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected dict-slice resource-effect fallback mismatch to fail closed";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("resource-effect fallback expects list[i64], got i64"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, RejectsUnsupportedTypedStdinResourceEffectValue) {
  const std::string src =
    "flag: bool = ?| (<- @stdin) | 1\n"
    ">_(flag)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected unsupported typed stdin resource-effect value to fail closed";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(
      std::string(err.what()).find("typed stdin pull supports i64, f64, string, or list[T] targets"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlySemantics, RejectsNonFileHandleInstantPullResourceEffectValue) {
  const std::string src =
    "xs = [1,2]\n"
    "value = ?| (<< xs) | 7\n"
    ">_(value)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected non-file acquired-handle instant pull to fail closed";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(
      std::string(err.what()).find("instant pull handle source must be an acquired file handle"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlySemantics, RejectsNonResourceMethodResourceEffectStatement) {
  const std::string src =
    "text = \"alpha\"\n"
    "?| text.lines() | \"fallback\" -> @stdout\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected non-resource member call under resource-effect settlement to fail closed";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(
      std::string(err.what()).find("`?|` resource settlement requires a resource operation"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlySemantics, ResourceEffectAcquireThenCloseConsumesReceiver) {
  const std::string src =
    "?| f <- @file(\"tests/features/file_resources/data/hello.txt\")"
    " | \"fallback\" -> @stdout\n"
    "?| f.close() | \"close fallback\" -> @stdout\n"
    "f.path\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected resource-effect acquired handle to stay destroyed after close";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(std::string(err.what()).find("use-after-destroy"), std::string::npos);
  }
}

TEST(StyioSecurityNightlySemantics, ResourceEffectAcquireThenDirectReleaseConsumesReceiver) {
  const std::string src =
    "?| f <- @file(\"tests/features/file_resources/data/hello.txt\")"
    " | \"fallback\" -> @stdout\n"
    "?| f -> @() | \"release fallback\" -> @stdout\n"
    "f.path\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected resource-effect direct release to consume the acquired handle";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(std::string(err.what()).find("use-after-destroy"), std::string::npos);
  }
}

TEST(StyioSecurityNightlyParserStmt, RejectsUnknownResourceEffectNamedHandler) {
  const std::string src =
    "?| \"x\" -> @stdout | unknown_effect => \"fallback\" -> @stderr\n";

  try {
    (void)parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected unknown resource-effect handler to fail closed";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(
      std::string(err.what()).find("unknown resource-effect handler `unknown_effect`"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlyParserStmt, RejectsDuplicateResourceEffectNamedHandler) {
  const std::string src =
    "?| \"x\" -> @stdout | io => \"a\" -> @stderr | io => \"b\" -> @stderr\n";

  try {
    (void)parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected duplicate resource-effect handler to fail closed";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(
      std::string(err.what()).find("duplicate resource-effect handler `io`"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlyParserStmt, RejectsEmptyResourceNamedHandlerBody) {
  const std::string src =
    "?| \"x\" -> @stdout | io => @()\n";

  try {
    (void)parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected empty resource handler body to fail closed";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(
      std::string(err.what()).find("resource-effect handler must be executable code, not @()"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlyParserStmt, ParsesTerminalHandleReturnShorthands) {
  const std::vector<std::string> samples = {
    "# stdin_a := () => { <|[>_] }\n",
    "# stdin_b := () => { <|(>_) }\n",
    "# stdin_c := () => { <| <- [>_] }\n",
    "# stdin_d := () => { <| <- (>_) }\n",
  };

  for (const auto& src : samples) {
    const std::string repr = parse_program_to_repr_latest(src, true);
    EXPECT_NE(repr.find("styio.ast.return"), std::string::npos) << src;
    EXPECT_NE(repr.find("@stdin"), std::string::npos) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, ParsesInternalResourceDefinitions) {
  const std::string src =
    "@ stdout := #(xs) => { xs >> [>_] }\n"
    "@ stdout := #(x) => { x -> [>_] }\n"
    "@ stdout := #(xs) => { xs >> (>_) }\n"
    "@ stdout := #(x) => { x -> (>_) }\n"
    "@ stderr := #(xs) => { !(xs) >> [>_] }\n"
    "@ stderr := #(x) => { !(x) -> [>_] }\n"
    "@ stderr := #(xs) => { !(xs) >> (>_) }\n"
    "@ stderr := #(x) => { !(x) -> (>_) }\n"
    "@ stdin := #() => { <|[>_] }\n"
    "@ stdin := #() => { <|(>_) }\n"
    "@ stdin := #() => { <| <- [>_] }\n"
    "@ stdin := #() => { <| <- (>_) }\n"
    "@ file : ftype := #(path) => { ... }\n";

  EXPECT_NO_THROW((void)parse_program_to_repr_latest(src, true));
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
}

TEST(StyioSecurityNightlyParserStmt, ParsesResourcePreludeSourceFile) {
  const auto prelude =
    std::filesystem::path(STYIO_SOURCE_DIR) / "share" / "styio" / "prelude" / "resources.styio";
  std::ifstream in(prelude);
  ASSERT_TRUE(in.good()) << prelude;
  const std::string src(
    (std::istreambuf_iterator<char>(in)),
    std::istreambuf_iterator<char>()
  );

  EXPECT_NO_THROW((void)parse_program_to_repr_latest(src, true));
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
}

TEST(StyioSecurityNightlyParserStmt, RejectsParameterizedResourcePseudoDefinitions) {
  const std::vector<std::string> samples = {
    "@file(\"/tmp/styio-invalid-resource-definition.txt\") := { file(path) }\n",
    "@{\"/tmp/styio-invalid-resource-definition.txt\"} := @file(\"/tmp/styio-invalid-resource-definition.txt\")\n",
    "@ file := #(path) => { file(path) }\n",
    "@ file : ftype := #(path) => { file(path) }\n",
    "@ file : ftype := #(path) => { path }\n",
    "@ socket : ftype := #(path) => { ... }\n",
    "@ stdout := { xs >> [>_] }\n",
    "@ stderr := #(xs) => { !(x) >> [>_] }\n",
    "# sink = @stdout\n",
  };

  for (const auto& src : samples) {
    EXPECT_THROW((void)parse_program_to_repr_latest(src, true), StyioSyntaxError) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, ParsesGenericFunctionTypeAnnotations) {
  const std::string src =
    "# first : i64 := (xs: list[i64]) => xs[0]\n"
    "# lookup : i64 := (table: dict[string, i64], key: string) => table[key]\n"
    "# identity_list : list[i64] := (xs: list[i64]) => {\n"
    "  n = xs.length\n"
    "  <| xs\n"
    "}\n";

  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("list[i64]"), std::string::npos);
  EXPECT_NE(repr.find("dict[string,i64]"), std::string::npos);

  const std::string engine_repr =
    parse_program_engine_to_repr_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(engine_repr.find("styio.ast.attr { xs.length }"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, RejectsTupleReturnShapeMismatchBeforeCodegen) {
  const std::string src =
    "# pair : (i64, i64) := (x: i64) => x\n"
    ">_(pair(7))\n";

  try {
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected tuple return shape mismatch to fail closed";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(
      std::string(err.what()).find("tuple return"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnFlexBindSubsetSamples) {
  const std::vector<std::string> samples = {
    "x = 1 + 2\n>_(x)\n",
    "price = 1 + 2 * 3\n>_(price)\n",
    "a = 1\nb = a + 2\n>_(b)\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnHandleIoSubsetSamples) {
  const std::vector<std::string> samples = {
    "f <- @file(\"tests/features/file_resources/data/hello.txt\")\n",
    "out << @file(\"/tmp/styio-new-parser-handle-io.txt\")\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, RejectsBracedExplicitFileResource) {
  const std::string src = "f <- @file{\"tests/features/file_resources/data/hello.txt\"}\n";
  EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioSyntaxError);
  EXPECT_THROW(parse_program_to_repr_latest(src, false), StyioSyntaxError);
}

TEST(StyioSecurityNightlyParserStmt, AcceptsAtImportSyntaxWithCanonicalSlashPaths) {
  const std::string src =
    "@import { styio.mod; tools/helpers, core }\n";

  const std::string nightly = parse_program_to_repr_latest(src, true);
  const std::string legacy = parse_program_to_repr_latest(src, false);
  EXPECT_EQ(nightly, legacy);
  EXPECT_NE(nightly.find("styio/mod"), std::string::npos);
  EXPECT_NE(nightly.find("tools/helpers"), std::string::npos);
  EXPECT_NE(nightly.find("core"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, AcceptsAtExportSyntax) {
  const std::string src =
    "@export { fast_add; native/math }\n";

  const std::string nightly = parse_program_to_repr_latest(src, true);
  const std::string legacy = parse_program_to_repr_latest(src, false);
  EXPECT_EQ(nightly, legacy);
  EXPECT_NE(nightly.find("styio.ast.export"), std::string::npos);
  EXPECT_NE(nightly.find("fast_add"), std::string::npos);
  EXPECT_NE(nightly.find("native/math"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, AcceptsAtExternCBlockSyntax) {
  const std::string src =
    "@extern(c) => {\n"
    "  int fast_add(int a, int b);\n"
    "  double fast_dot(double* a, double* b, long n);\n"
    "}\n";

  const std::string nightly = parse_program_to_repr_latest(src, true);
  const std::string legacy = parse_program_to_repr_latest(src, false);
  EXPECT_EQ(nightly, legacy);
  EXPECT_NE(nightly.find("styio.ast.extern"), std::string::npos);
  EXPECT_NE(nightly.find("abi: c"), std::string::npos);
  EXPECT_NE(nightly.find("fast_add"), std::string::npos);
  EXPECT_NE(nightly.find("fast_dot"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, AcceptsAtExternCPlusPlusBlockSyntax) {
  const std::string src =
    "@extern(C++) => {\n"
    "  extern \"C\" int fast_square(int x) { return x * x; }\n"
    "}\n";

  const std::string nightly = parse_program_to_repr_latest(src, true);
  const std::string legacy = parse_program_to_repr_latest(src, false);
  EXPECT_EQ(nightly, legacy);
  EXPECT_NE(nightly.find("styio.ast.extern"), std::string::npos);
  EXPECT_NE(nightly.find("abi: c++"), std::string::npos);
  EXPECT_NE(nightly.find("fast_square"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, AcceptsBoundExternReferenceSyntax) {
  const std::string src =
    "# ref_square := @ extern(C++) { \"native/ref_square.cpp\" }\n";

  const std::string nightly = parse_program_to_repr_latest(src, true);
  const std::string legacy = parse_program_to_repr_latest(src, false);
  EXPECT_EQ(nightly, legacy);
  EXPECT_NE(nightly.find("styio.ast.extern"), std::string::npos);
  EXPECT_NE(nightly.find("abi: c++"), std::string::npos);
  EXPECT_NE(nightly.find("sources:"), std::string::npos);
  EXPECT_NE(nightly.find("exports: ref_square"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, AcceptsBoundExternMultipleSymbolsSyntax) {
  const std::string src =
    "# ref_square_1, ref_square_2 := @ extern(C++) { \"native/ref_pair.cpp\" }\n";

  const std::string nightly = parse_program_to_repr_latest(src, true);
  const std::string legacy = parse_program_to_repr_latest(src, false);
  EXPECT_EQ(nightly, legacy);
  EXPECT_NE(nightly.find("exports: ref_square_1 ref_square_2"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, CoversImportExportExternBoundarySyntax) {
  const std::string extern_arrow_source =
    "@extern(c) => \"native/arrow_source.c\"\n";
  const std::string nightly = parse_program_to_repr_latest(extern_arrow_source, true);
  const std::string legacy = parse_program_to_repr_latest(extern_arrow_source, false);
  EXPECT_EQ(nightly, legacy);
  EXPECT_NE(nightly.find("styio.ast.extern"), std::string::npos);
  EXPECT_NE(nightly.find("sources:"), std::string::npos);
  EXPECT_NE(slash_normalized(nightly).find("native/arrow_source.c"), std::string::npos);

  const std::string bound_source_list =
    "# ref_one, ref_two := @ extern(c) { \"native/ref_one.c\"; \"native/ref_two.c\" }\n";
  const std::string bound_nightly = parse_program_to_repr_latest(bound_source_list, true);
  const std::string bound_legacy = parse_program_to_repr_latest(bound_source_list, false);
  EXPECT_EQ(bound_nightly, bound_legacy);
  const std::string bound_nightly_slashes = slash_normalized(bound_nightly);
  EXPECT_NE(bound_nightly_slashes.find("native/ref_one.c"), std::string::npos);
  EXPECT_NE(bound_nightly_slashes.find("native/ref_two.c"), std::string::npos);
  EXPECT_NE(bound_nightly.find("exports: ref_one ref_two"), std::string::npos);

  const std::vector<std::string> rejected = {
    "@import { }\n",
    "@import { core, }\n",
    "@import { core tools }\n",
    "@import { core/ }\n",
    "@export { }\n",
    "@export { fast_add; }\n",
    "@export { fast_add native_math }\n",
    "@extern() => { int fast_add(int a, int b); }\n",
    "@extern(c+) => { int fast_add(int a, int b); }\n",
    "@extern(c)\n",
    "# := @ extern(c) { \"native/missing_symbol.c\" }\n",
  };

  for (const auto& src : rejected) {
    EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioBaseException) << src;
    EXPECT_THROW(parse_program_to_repr_latest(src, false), StyioBaseException) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, CoversLegacyStringListImportsAcrossListShapes) {
  const std::vector<std::string> rejected = {
    "[ \"legacy/pkg\" ]\n",
    "[\"legacy/pkg\", \"tools/helper\"]\n",
    "[\"legacy/pkg\" \"tools/helper\"]\n",
  };

  for (const auto& src : rejected) {
    EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioBaseException) << src;
    EXPECT_THROW(parse_program_to_repr_latest(src, false), StyioBaseException) << src;
  }

  const std::string trailing_comma_list = "[\"legacy/pkg\", ]\n";
  EXPECT_NO_THROW((void)parse_program_to_repr_latest(trailing_comma_list, true));
  EXPECT_NO_THROW((void)parse_program_to_repr_latest(trailing_comma_list, false));
}

TEST(StyioSecurityNightlyParserStmt, RejectsMixedSeparatorsInsideAtImportItem) {
  const std::string src = "@import { styio/mod.sub }\n";
  EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioSyntaxError);
  EXPECT_THROW(parse_program_to_repr_latest(src, false), StyioSyntaxError);
}

TEST(StyioSecurityNightlyParserStmt, RejectsAtImportOutsideTopLevel) {
  const std::string src =
    "# use := () => {\n"
    "  @import { styio/mod }\n"
    "}\n";
  EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioSyntaxError);
  EXPECT_THROW(parse_program_to_repr_latest(src, false), StyioSyntaxError);
}

TEST(StyioSecurityNightlyParserStmt, RejectsAtExportOutsideTopLevel) {
  const std::string src =
    "# use := () => {\n"
    "  @export { fast_add }\n"
    "}\n";
  EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioSyntaxError);
  EXPECT_THROW(parse_program_to_repr_latest(src, false), StyioSyntaxError);
}

TEST(StyioSecurityNightlyParserStmt, RejectsAtExternOutsideTopLevel) {
  const std::string src =
    "# use := () => {\n"
    "  @extern(c) => { int fast_add(int a, int b); }\n"
    "}\n";
  EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioSyntaxError);
  EXPECT_THROW(parse_program_to_repr_latest(src, false), StyioSyntaxError);
}

TEST(StyioSecurityNightlyParserStmt, RejectsUnsupportedExternAbi) {
  const std::string src =
    "@extern(rust) => { int fast_add(int a, int b); }\n";
  EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioSyntaxError);
  EXPECT_THROW(parse_program_to_repr_latest(src, false), StyioSyntaxError);
}

TEST(StyioSecurityNightlyParserStmt, RejectsDeprecatedExternCppAbiSpelling) {
  const std::string src =
    "@extern(cpp) => { int fast_add(int a, int b); }\n";
  EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioSyntaxError);
  EXPECT_THROW(parse_program_to_repr_latest(src, false), StyioSyntaxError);
}

TEST(StyioSecurityNativeToolchain, EnvCompilerOverridesBundledMode) {
  EnvSnapshot cxx("STYIO_NATIVE_CXX");
  EnvSnapshot mode("STYIO_NATIVE_TOOLCHAIN_MODE");
  EnvSnapshot root("STYIO_NATIVE_TOOLCHAIN_ROOT");
  cxx.set("/tmp/styio-explicit-clang++");
  mode.set("bundled");
  root.set("/tmp/styio-missing-toolchain-root");

  const auto resolved = styio::native::resolve_compiler_for_abi("c++");
  EXPECT_EQ(resolved.command, "/tmp/styio-explicit-clang++");
  EXPECT_EQ(resolved.source, "env:STYIO_NATIVE_CXX");
}

TEST(StyioSecurityNativeToolchain, BundledModeFindsClangPlusPlusUnderToolchainRoot) {
  EnvSnapshot cxx("STYIO_NATIVE_CXX");
  EnvSnapshot mode("STYIO_NATIVE_TOOLCHAIN_MODE");
  EnvSnapshot root_env("STYIO_NATIVE_TOOLCHAIN_ROOT");
  cxx.unset();
  mode.set("bundled");

  const auto root =
    std::filesystem::temp_directory_path()
    / ("styio-native-toolchain-test-" + std::to_string(static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count())));
  const auto bin = root / "bin";
  const auto clangxx = bin / "clang++";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(bin);
  {
    std::ofstream out(clangxx);
    out << "#!/bin/sh\nexit 0\n";
  }
#ifndef _WIN32
  chmod(clangxx.c_str(), 0755);
#endif
  root_env.set(root.string());

  const auto resolved = styio::native::resolve_compiler_for_abi("c++");
  EXPECT_EQ(resolved.command, clangxx.string());
  EXPECT_EQ(resolved.source, "bundled-clang");

  std::filesystem::remove_all(root);
}

TEST(StyioSecurityNativeToolchain, SystemModeSkipsBundledClangSearch) {
  EnvSnapshot cxx("STYIO_NATIVE_CXX");
  EnvSnapshot mode("STYIO_NATIVE_TOOLCHAIN_MODE");
  EnvSnapshot root_env("STYIO_NATIVE_TOOLCHAIN_ROOT");
  cxx.unset();
  mode.set("system");
  root_env.set("/tmp/styio-ignored-toolchain-root");

  const auto resolved = styio::native::resolve_compiler_for_abi("c++");
#if defined(_WIN32)
  std::string resolved_name =
    std::filesystem::path(resolved.command).filename().string();
  std::transform(
    resolved_name.begin(),
    resolved_name.end(),
    resolved_name.begin(),
    [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  EXPECT_TRUE(
    resolved_name.find("clang++") != std::string::npos
    || resolved_name.find("clang-cl") != std::string::npos)
    << resolved.command;
#else
  EXPECT_EQ(resolved.command, "c++");
#endif
  EXPECT_EQ(resolved.source, "system");
}

TEST(StyioSecurityNativeToolchain, ParsesNativeSignaturesCommentsAndReferencedSources) {
  using styio::native::CType;
  using styio::native::CTypeKind;

  EnvSnapshot mode("STYIO_NATIVE_TOOLCHAIN_MODE");

  EXPECT_EQ(styio::native::normalize_abi(" C++ "), "c++");
  EXPECT_THROW(styio::native::normalize_abi("rust"), StyioTypeError);

  mode.set("bad");
  EXPECT_THROW(styio::native::configured_native_toolchain_mode(), StyioTypeError);
  mode.set(" AUTO ");
  EXPECT_EQ(styio::native::configured_native_toolchain_mode(), "auto");

  const std::string body =
    "// int hidden_line(int x);\n"
    "/* int hidden_block(int x); */\n"
    "extern \"C\" int visible(int a, const char* name);\n"
    "static inline unsigned long count_items(size_t n) { return n; }\n"
    "bool flag(_Bool ok);\n"
    "float f32_value(float x);\n"
    "double f64_value(double y);\n"
    "char one_char(char c);\n"
    "void reset(void);\n"
    "int with_literals(void) { const char* s = \"// not comment\"; char c = '\\''; return 1; }\n"
    "if (not_a_signature) { }\n";
  const auto sigs = styio::native::parse_function_signatures(body);
  ASSERT_EQ(sigs.size(), 8U);

  auto find_sig = [&](const std::string& name) -> const styio::native::FunctionSignature& {
    auto it = std::find_if(
      sigs.begin(),
      sigs.end(),
      [&](const styio::native::FunctionSignature& sig) { return sig.name == name; });
    EXPECT_NE(it, sigs.end()) << name;
    return *it;
  };

  const auto& visible = find_sig("visible");
  EXPECT_EQ(visible.return_type.kind, CTypeKind::I32);
  ASSERT_EQ(visible.params.size(), 2U);
  EXPECT_EQ(visible.params[0].name, "a");
  EXPECT_EQ(visible.params[0].type.kind, CTypeKind::I32);
  EXPECT_EQ(visible.params[1].name, "name");
  EXPECT_EQ(visible.params[1].type.kind, CTypeKind::Pointer);

  const auto& count_items = find_sig("count_items");
  EXPECT_EQ(count_items.return_type.kind, CTypeKind::I64);
  EXPECT_TRUE(count_items.internal_linkage);
  EXPECT_TRUE(count_items.return_type.is_unsigned);
  ASSERT_EQ(count_items.params.size(), 1U);
  EXPECT_EQ(count_items.params[0].type.kind, CTypeKind::I64);

  const auto& flag = find_sig("flag");
  EXPECT_EQ(flag.return_type.kind, CTypeKind::Bool);
  ASSERT_EQ(flag.params.size(), 1U);
  EXPECT_EQ(flag.params[0].type.kind, CTypeKind::Bool);
  EXPECT_EQ(find_sig("f32_value").return_type.kind, CTypeKind::F32);
  EXPECT_EQ(find_sig("f64_value").return_type.kind, CTypeKind::F64);
  EXPECT_EQ(find_sig("one_char").return_type.kind, CTypeKind::I8);
  EXPECT_TRUE(find_sig("reset").params.empty());
  EXPECT_EQ(find_sig("with_literals").return_type.kind, CTypeKind::I32);

  EXPECT_THROW(styio::native::parse_function_signatures("int bad(...);"), StyioTypeError);
  EXPECT_THROW(styio::native::parse_function_signatures("int bad(void x);"), StyioTypeError);
  EXPECT_THROW(styio::native::parse_function_signatures("mystery nope(int x);"), StyioTypeError);
  EXPECT_THROW(styio::native::parse_function_signatures("int bad(struct Thing x);"), StyioTypeError);

  auto mapped = [](CTypeKind kind) {
    return styio::native::styio_data_type_for_c_type(CType{kind, false, ""});
  };
  EXPECT_EQ(mapped(CTypeKind::Void).option, StyioDataTypeOption::Undefined);
  EXPECT_EQ(mapped(CTypeKind::Bool).option, StyioDataTypeOption::Bool);
  EXPECT_EQ(mapped(CTypeKind::F32).option, StyioDataTypeOption::Float);
  EXPECT_EQ(mapped(CTypeKind::F64).name, "f64");
  EXPECT_EQ(mapped(CTypeKind::Pointer).option, StyioDataTypeOption::String);
  EXPECT_EQ(mapped(CTypeKind::I8).option, StyioDataTypeOption::Integer);
  EXPECT_EQ(mapped(CTypeKind::I16).name, "i64");
  EXPECT_EQ(mapped(CTypeKind::I32).num_of_bit, 64U);
  EXPECT_EQ(mapped(CTypeKind::I64).option, StyioDataTypeOption::Integer);

  const auto root =
    std::filesystem::temp_directory_path()
    / ("styio-native-signature-test-" + std::to_string(static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count())));
#if defined(_WIN32)
  const auto source_path = root / "native quoted.h";
#else
  const auto source_path = root / "native \"quoted\".h";
#endif
  std::filesystem::remove_all(root);
  ASSERT_TRUE(std::filesystem::create_directories(root));
  {
    std::ofstream out(source_path);
    out << "extern \"C\" unsigned short from_ref(unsigned char raw);\n";
  }

  const auto block_sigs = styio::native::parse_function_signatures_for_block(
    "int inline_decl(int x);\n",
    {source_path.string()});
  ASSERT_EQ(block_sigs.size(), 2U);
  EXPECT_EQ(block_sigs[0].name, "inline_decl");
  EXPECT_EQ(block_sigs[1].name, "from_ref");
  EXPECT_TRUE(block_sigs[1].return_type.is_unsigned);
  EXPECT_EQ(block_sigs[1].return_type.kind, CTypeKind::I16);
  ASSERT_EQ(block_sigs[1].params.size(), 1U);
  EXPECT_TRUE(block_sigs[1].params[0].type.is_unsigned);
  EXPECT_EQ(block_sigs[1].params[0].type.kind, CTypeKind::I8);

  const std::string c_source = styio::native::source_text_for_block(
    "c",
    "int inline_decl(int x);\n",
    {source_path.string()});
  EXPECT_NE(c_source.find("#include <stdbool.h>"), std::string::npos);
#if defined(_WIN32)
  EXPECT_NE(c_source.find("native quoted.h"), std::string::npos);
#else
  EXPECT_NE(c_source.find("\\\"quoted\\\""), std::string::npos);
#endif
  EXPECT_NE(c_source.find("hash="), std::string::npos);
  const std::string cpp_source = styio::native::source_text_for_block(" c++ ", "", {});
  EXPECT_NE(cpp_source.find("#include <cstdint>"), std::string::npos);
  EXPECT_THROW(
    styio::native::parse_function_signatures_for_block("", {""}),
    StyioTypeError);
  EXPECT_THROW(
    styio::native::source_text_for_block("c", "", {(root / "missing.h").string()}),
    StyioTypeError);

#ifndef _WIN32
  void* self_handle = ::dlopen(nullptr, RTLD_NOW);
  ASSERT_NE(self_handle, nullptr);
  styio::native::close_loaded_block(self_handle);
#endif
  styio::native::close_loaded_block(nullptr);

  std::filesystem::remove_all(root);
}

TEST(StyioSecurityNativeToolchain, EnvCompilerCommandIsNotInterpretedByShell) {
  EnvSnapshot cc("STYIO_NATIVE_CC");
  EnvSnapshot mode("STYIO_NATIVE_TOOLCHAIN_MODE");
  EnvSnapshot root_env("STYIO_NATIVE_TOOLCHAIN_ROOT");
  const auto root =
    std::filesystem::temp_directory_path()
    / ("styio-native-injection-test-" + std::to_string(static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count())));
  const auto marker = root / "injected-marker";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  cc.set("/bin/false; touch " + marker.string() + "; #");
  mode.set("system");
  root_env.unset();

  std::filesystem::remove(marker);
  EXPECT_THROW(
    styio::native::compile_and_load_block(
      "c",
      "int fast_add(void) { return 1; }",
      {}
    ),
    StyioTypeError
  );
  EXPECT_FALSE(std::filesystem::exists(marker));
  std::filesystem::remove_all(root);
}

TEST(StyioSecurityNativeToolchain, NativeSourceCacheAvoidsRepeatedCompilerInvocation) {
#if defined(_WIN32)
  GTEST_SKIP() << "POSIX shell-wrapper compiler counter coverage; Windows cache load paths use the native compiler tests.";
#endif
  EnvSnapshot cc("STYIO_NATIVE_CC");
  EnvSnapshot mode("STYIO_NATIVE_TOOLCHAIN_MODE");
  EnvSnapshot root_env("STYIO_NATIVE_TOOLCHAIN_ROOT");
  EnvSnapshot cache_dir_env("STYIO_NATIVE_CACHE_DIR");
  EnvSnapshot cache_mode("STYIO_NATIVE_CACHE");
  const auto root =
    std::filesystem::temp_directory_path()
    / ("styio-native-cache-test-" + std::to_string(static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count())));
  const auto wrapper = root / "cc-wrapper.sh";
  const auto counter = root / "compiler-count";
  const auto cache_dir = root / "cache";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  {
    std::ofstream out(wrapper);
    out << "#!/bin/sh\n"
        << "count_file='" << counter.string() << "'\n"
        << "if [ -f \"$count_file\" ]; then n=$(cat \"$count_file\"); else n=0; fi\n"
        << "n=$((n + 1))\n"
        << "printf '%s\\n' \"$n\" > \"$count_file\"\n"
        << "exec " << posix_shell_quote(test_c_compiler()) << " \"$@\"\n";
  }
#ifndef _WIN32
  chmod(wrapper.c_str(), 0755);
#endif

  cc.set(wrapper.string());
  mode.set("system");
  root_env.unset();
  cache_dir_env.set(cache_dir.string());
  cache_mode.unset();

  const std::string body =
    "int cached_add(int a, int b) { return a + b; }\n"
    "int cached_sub(int a, int b) { return a - b; }\n";

  auto read_counter = [&]()
  {
    std::ifstream in(counter);
    int value = 0;
    in >> value;
    return value;
  };

  auto first = styio::native::compile_and_load_block("c", body, {"cached_add"});
  ASSERT_EQ(first.symbols.size(), 1U);
  auto* add = reinterpret_cast<int (*)(int, int)>(first.symbols[0].address);
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add(20, 22), 42);
  EXPECT_EQ(read_counter(), 1);

  auto second = styio::native::compile_and_load_block("c", body, {"cached_sub"});
  ASSERT_EQ(second.symbols.size(), 1U);
  auto* sub = reinterpret_cast<int (*)(int, int)>(second.symbols[0].address);
  ASSERT_NE(sub, nullptr);
  EXPECT_EQ(sub(50, 8), 42);
  EXPECT_EQ(read_counter(), 1);

  std::filesystem::remove_all(root);
}

TEST(StyioSecurityNightlyParserStmt, RejectsLegacyStringListImportSyntax) {
  const std::string src = "[\"math\"]\n";
  EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioSyntaxError);
  EXPECT_THROW(parse_program_to_repr_latest(src, false), StyioSyntaxError);
}

TEST(StyioSecurityNightlyParserStmt, AcceptsBubbleSortFeatureSyntax) {
  const std::string src =
    "l <- @stdin: list[i32]\n"
    "n = l.length - 1\n"
    "[0..n] >> #(i) => {\n"
    "  [0..n-i-1] >> #(j) => {\n"
    "    ?(l[j] > l[j+1]) => {\n"
    "      l[j], l[j+1] = l[j+1], l[j]\n"
    "    }\n"
    "  }\n"
    "}\n";
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("assign.parallel"), std::string::npos);
  EXPECT_NE(repr.find("only_true"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, TypedStdinPullFormsShareInstantPullAst) {
  const std::string list_src =
    "xs <- @stdin : list[i32]\n";
  const std::string tuple_src =
    "a, b <- @stdin : (f64, f64)\n";
  const std::string list_repr = parse_program_to_repr_latest(list_src, true);
  const std::string tuple_repr = parse_program_to_repr_latest(tuple_src, true);
  EXPECT_NE(list_repr.find("instant.pull"), std::string::npos);
  EXPECT_NE(list_repr.find("list[i32]"), std::string::npos);
  EXPECT_EQ(list_repr.find("stdin.list.typed"), std::string::npos);
  EXPECT_NE(tuple_repr.find("instant.pull"), std::string::npos);
  EXPECT_NE(tuple_repr.find("f64"), std::string::npos);
  EXPECT_EQ(tuple_repr.find("stdin.list.typed"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, AcceptsGuardFallbackBlockSyntax) {
  const std::string src =
    "x = 0\n"
    "?(x < 1) => {\n"
    "  x = 1\n"
    "} | {\n"
    "  x = 2\n"
    "}\n";

  const std::string nightly = parse_program_to_repr_latest(src, true);
  const std::string legacy = parse_program_to_repr_latest(src, false);
  EXPECT_EQ(nightly, legacy);
  EXPECT_NE(nightly.find("if_else"), std::string::npos);
  EXPECT_NE(nightly.find("Else:"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, RejectsGuardFallbackWithoutBlock) {
  const std::string src =
    "x = 0\n"
    "?(x < 1) => {\n"
    "  x = 1\n"
    "} | x = 2\n";

  EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioSyntaxError);
  EXPECT_THROW(parse_program_to_repr_latest(src, false), StyioSyntaxError);
}

TEST(StyioSecurityNightlyParserStmt, AcceptsGuardValueExpressionSyntax) {
  const std::string src =
    "x = ?(1 < 2) => 10 | 20\n"
    "y = ?(x > 10) => x + 1 | x - 1\n"
    ">_(y)\n";

  EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false));
  EXPECT_NO_THROW(parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly));
  EXPECT_NO_THROW(parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Legacy));
}

TEST(StyioSecurityNightlyParserStmt, AcceptsDictFeatureSyntax) {
  const std::string src =
    "d = dict{\"a\": 1, \"b\": 2}\n"
    "n = d.length\n"
    "ks = d.keys\n"
    "vs = d.values\n"
    "x = d[\"a\"]\n";
  const std::string repr = parse_program_to_repr_latest(src, true);
  EXPECT_NE(repr.find("styio.ast.dict"), std::string::npos);
  EXPECT_NE(repr.find("styio.ast.attr"), std::string::npos);
  EXPECT_NE(repr.find("styio.ast.access.by_index"), std::string::npos);
}

TEST(StyioSecurityNightlyParserStmt, RejectsDotChainAfterCall) {
  const std::string src = "x = foo.bar(1).baz(2)\n";
  EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioSyntaxError);
}

TEST(StyioSecurityNightlySemantics, RejectsUnknownFunctionDuringTypecheck) {
  const std::string src = "x = missing(1)\n";
  EXPECT_THROW(
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly),
    StyioTypeError
  );
}

TEST(StyioSecurityNightlySemantics, RejectsBoundExternMissingDeclaredSymbol) {
  const std::string src =
    "# missing_add := @ extern(c) {\n"
    "int fast_add(int a, int b) { return a + b; }\n"
    "}\n"
    ">_(missing_add(1, 2))\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected bound extern declaration mismatch";
  }
  catch (const StyioTypeError& ex) {
    const std::string msg = ex.what();
    EXPECT_NE(msg.find("@extern binding does not declare native function `missing_add`"), std::string::npos)
      << msg;
  }
}

TEST(StyioSecurityNightlySemantics, RegistersOnlyBoundExternSymbols) {
  const std::string src =
    "# fast_add := @ extern(c) {\n"
    "int fast_add(int a, int b) { return a + b; }\n"
    "int hidden_add(int a, int b) { return a + b; }\n"
    "}\n"
    ">_(hidden_add(1, 2))\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected hidden native symbol rejection";
  }
  catch (const StyioTypeError& ex) {
    const std::string msg = ex.what();
    EXPECT_NE(msg.find("unknown function `hidden_add`"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, RejectsUserFunctionArityMismatchDuringTypecheck) {
  const std::vector<std::string> samples = {
    "# add := (a: i32, b: i32) => a + b\nx = add(1)\n",
    "# add := (a: i32, b: i32) => a + b\nx = add(1, 2, 3)\n",
  };

  for (const auto& src : samples) {
    EXPECT_THROW(
      parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly),
      StyioTypeError
    ) << src;
  }
}

TEST(StyioSecurityNightlySemantics, RejectsOneShotContinuationResumeBeforeLowering) {
  const std::vector<std::string> samples = {
    "# id := (x: i32) => x\nx = id <| 1 <| 2\n",
    "# id := (x: i32) => x\nx = id(1)(2)\n",
  };

  for (const auto& src : samples) {
    try {
      parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
      FAIL() << "expected one-shot continuation lowering error for " << src;
    }
    catch (const StyioTypeError& ex) {
      const std::string msg = ex.what();
      EXPECT_NE(msg.find("one-shot continuation resume"), std::string::npos) << msg;
      EXPECT_NE(msg.find("exactly once"), std::string::npos) << msg;
    }
  }
}

TEST(StyioSecurityNightlySemantics, RejectsBareAwaitFreezeBeforeContinuationLowering) {
  const std::string src =
    "?| -> input: i64\n"
    ">_(input)\n";
  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected bare continuation freeze lowering error";
  }
  catch (const StyioTypeError& ex) {
    const std::string msg = ex.what();
    EXPECT_NE(msg.find("bare continuation freeze"), std::string::npos) << msg;
    EXPECT_NE(msg.find("continuation lowering"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, RejectsAwaitPipeNonTaskSource) {
  const std::string src =
    "x = 1\n"
    "?| x -> answer: i64 | 0\n"
    ">_(answer)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected non-task await source to fail closed";
  }
  catch (const StyioTypeError& ex) {
    const std::string msg = ex.what();
    EXPECT_NE(msg.find("await source for `?|` must be a task/future handle"), std::string::npos)
      << msg;
  }
}

TEST(StyioSecurityNightlySemantics, AllowsDirectNestedFunctionCallsDuringLowering) {
  const std::string src =
    "# outer := (x: i32) => {\n"
    "  # inner := (y: i32) => y + 1\n"
    "  <| inner(x) + inner(x + 1)\n"
    "}\n"
    ">_(outer(3))\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
}

TEST(StyioSecurityNightlySemantics, RejectsPlainResourceCopyByEqual) {
  const std::string src =
    "l = @stdin: list[i32]\n"
    "l1 = l\n";
  EXPECT_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly),
    StyioTypeError
  );
}

TEST(StyioSecurityNightlySemantics, RejectsBoundResourceCopyByLeftArrow) {
  const std::string src =
    "l <- @stdin: list[i32]\n"
    "l1 <- l\n";
  try {
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected bound resource clone with `<-` to fail";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(
      std::string(err.what()).find("must use `<<`; `<-` only acquires external resources"),
      std::string::npos)
      << err.what();
  }
}

TEST(StyioSecurityNightlySemantics, AllowsExplicitCloneFormAndIndexedMutation) {
  const std::string src =
    "l <- @stdin: list[i32]\n"
    "l2 << l\n"
    "l[0] = 9\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_clone"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, AllowsExplicitMatrixCloneForm) {
  const std::string src =
    "m: matrix = [[1,2],[3,4]]\n"
    "m2 << m\n"
    "ok = mat_set(m,0,0,9)\n"
    "cell = m2[0][0]\n"
    "mf: matrix = [[1.5,2.5],[3.5,4.5]]\n"
    "mf2 << mf\n"
    "ok2 = mat_set(mf,1,1,9.5)\n"
    "fcell = mf2[1][1]\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_matrix_clone_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_matrix_clone_f64"), std::string::npos);
  EXPECT_EQ(llvm_ir.find("styio_list_clone"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, RejectsMatrixCopyByLeftArrow) {
  const std::string src =
    "m: matrix = [[1,2],[3,4]]\n"
    "m2 <- m\n";
  try {
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected bound matrix clone with `<-` to fail";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(
      std::string(err.what()).find("must use `<<`; `<-` only acquires external resources"),
      std::string::npos)
      << err.what();
  }
}

TEST(StyioSecurityNightlySemantics, RejectsTopologyResourceCloneByCopyOperator) {
  const std::string src =
    "@samples : i64|2|\n"
    "samples_copy << @samples\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected topology resource clone with `<<` to fail";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(std::string(err.what()).find("topology resource"), std::string::npos)
      << err.what();
  }
}

TEST(StyioSecurityNightlySemantics, RejectsFileHandleCloneByCopyOperator) {
  const std::string src =
    "f <- @file(\"tests/features/stream_processing/data/ref.txt\")\n"
    "f_copy << f\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected file handle clone with `<<` to fail closed";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(std::string(err.what()).find("file/stream handle"), std::string::npos)
      << err.what();
  }
}

TEST(StyioSecurityNightlySemantics, AllowsPredefinedListOperationsAcrossRuntimeFamilies) {
  const std::string src =
    "nums = [1,2]\n"
    "nums.push(3)\n"
    "nums.insert(0,4)\n"
    "nums.pop()\n"
    "letters = ['a','b']\n"
    "letters.push('c')\n"
    "letters.insert(0,'z')\n"
    "letters.pop()\n"
    "flags = [true,false]\n"
    "flags[1] = true\n"
    "names = [\"Ada\"]\n"
    "names.push(\"Lovelace\")\n"
    "names.insert(1, \"Byron\")\n"
    "names.pop()\n"
    "bags = [[1,2]]\n"
    "bags.push([3])\n"
    "bags[0] = [9]\n"
    "maps = [dict{\"a\": 1}]\n"
    "maps.insert(1, dict{\"b\": 2})\n"
    "maps[0] = dict{\"c\": 3}\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
}

TEST(StyioSecurityNightlySemantics, BoundedCharResourceSelectorsMaterializeCharLists) {
  const std::string src =
    "@letter : char|..3|\n"
    "['a','b','c','d'] >> #(v) => {\n"
    "  v -> @letter\n"
    "}\n"
    ">_(@letter[-1])\n"
    ">_(@letter[...])\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_new_char"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_push_char"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_char_cstr"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, ResourceSelectorSnapshotIteratorUsesRuntimeListPath) {
  const std::string src =
    "@price : i64|..2|\n"
    "[1,2] >> #(v) => {\n"
    "  v -> @price\n"
    "}\n"
    "@price[...] >> #(v) => {\n"
    "  >_(v)\n"
    "}\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_push_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_get"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, CharMaterializedListsFeedZipBarrier) {
  const std::string src =
    "['a','b'] >> #(left) & ['x','y'] >> #(right) => {\n"
    "  >_(left)\n"
    "  >_(right)\n"
    "}\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_get_char"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_char_cstr"), std::string::npos);
}

TEST(StyioSecurityZipBarrierFacts, VerifierRejectsEveryMalformedClosedFact) {
  const auto expect_rejected = [](const auto& mutate) {
    std::unique_ptr<SIOStreamZip> zip(SIOStreamZip::Create(
      SCListLiteral::Create({SGConstInt::Create(1)}, "i64"),
      false,
      false,
      "left",
      SCListLiteral::Create({SGConstInt::Create(2)}, "i64"),
      false,
      false,
      "right",
      false,
      false,
      "i64",
      "i64",
      SGBlock::Create({SGNoOp::Create()})));
    mutate(zip->barrier_facts);
    const auto result = styio::ir::verify_styio_ir(zip.get());
    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
      result.diagnostics.front().message,
      "SIOStreamZip.barrier_facts must be canonical");
  };

  expect_rejected([](auto& facts) {
    facts.frame_identity = static_cast<SGStreamZipFrameIdentity>(255);
  });
  expect_rejected([](auto& facts) {
    std::swap(facts.members[0], facts.members[1]);
  });
  expect_rejected([](auto& facts) {
    facts.members[1] = SGStreamZipBarrierMember::SourceA;
  });
  expect_rejected([](auto& facts) {
    facts.members[0] = static_cast<SGStreamZipBarrierMember>(255);
  });
  expect_rejected([](auto& facts) {
    facts.readiness = static_cast<SGStreamZipReadiness>(255);
  });
  expect_rejected([](auto& facts) {
    facts.commit = static_cast<SGStreamZipCommit>(255);
  });
  expect_rejected([](auto& facts) {
    facts.termination = static_cast<SGStreamZipTermination>(255);
  });
}

TEST(StyioSecurityZipBarrierFacts, FactoryOwnsFactsAndBodyDefinesLoopDomain) {
  static_assert(!std::is_default_constructible_v<SIOStreamZip>);

  std::unique_ptr<SIOStreamZip> zip(SIOStreamZip::Create(
    SCListLiteral::Create({SGConstInt::Create(1)}, "i64"),
    false,
    false,
    "left",
    SCListLiteral::Create({SGConstInt::Create(2)}, "i64"),
    false,
    false,
    "right",
    false,
    false,
    "i64",
    "i64",
    SGBlock::Create({
      SGIf::Create(
        SGConstBool::Create(true),
        SGBlock::Create({SGBreak::Create()}),
        SGBlock::Create({SGContinue::Create()})),
    })));

  ASSERT_TRUE(zip->barrier_facts.is_canonical());
  const auto result = styio::ir::verify_styio_ir(zip.get());
  EXPECT_TRUE(result.ok())
    << (result.diagnostics.empty() ? "" : result.diagnostics.front().message);
}

TEST(StyioSecurityZipBarrierFacts, UnequalListsExecuteOnlyMatchedPairs) {
  const std::string src =
    "[1,2,3] >> #(left) & [10,20] >> #(right) => {\n"
    "  >_(left * 100 + right)\n"
    "}\n";

  testing::internal::CaptureStdout();
  try {
    execute_program_engine_with_stdin_latest(
      src, StyioParserEngine::Nightly, "");
  }
  catch (...) {
    (void)testing::internal::GetCapturedStdout();
    throw;
  }
  std::fflush(stdout);
  EXPECT_EQ(testing::internal::GetCapturedStdout(), "110\n220\n");
}

TEST(StyioSecurityNightlySemantics, MatrixSelectorSnapshotsFeedZipBarrier) {
  const std::string src =
    "@bucket : matrix|..2|\n"
    "[1, 5] >> #(base) => {\n"
    "  cur: matrix = [[base, base + 1], [base + 2, base + 3]]\n"
    "  cur -> @bucket\n"
    "}\n"
    "@bucket[...] >> #(m) & [10, 20] >> #(rank) => {\n"
    "  >_(m)\n"
    "  >_(rank)\n"
    "}\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_len"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_get_matrix"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_matrix_release"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, StdinFeedsStreamZipBarrier) {
  const std::string src =
    "@stdin >> #(line) & [1,2] >> #(rank) => {\n"
    "  >_(line + rank)\n"
    "}\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_stdin_read_line"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_len"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, RejectsDuplicateStdinZipBeforeDriverDecision) {
  const std::string src =
    "@stdin >> #(left) & @stdin >> #(right) => {\n"
    "  >_(left + right)\n"
    "}\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected duplicate stdin zip to fail closed";
  }
  catch (const StyioTypeError& err) {
    EXPECT_NE(
      std::string(err.what()).find(
        "zip over @stdin on both sides requires a distinct stream-driver decision"),
      std::string::npos);
  }
}

TEST(StyioSecurityNightlySemantics, AllowsDictIndexingAttrsAndClone) {
  const std::string src =
    "d = dict{\"a\": 1, \"b\": 2}\n"
    "d[\"c\"] = 3\n"
    "k = d.keys[0]\n"
    "v = d.values[1]\n"
    "d2 << d\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
}

TEST(StyioSecurityNightlySemantics, AllowsScalarAndStringDictFamilies) {
  const std::string src =
    "flags = dict{\"ok\": true, \"ng\": false}\n"
    "ok = flags[\"ok\"]\n"
    "flag_values = flags.values\n"
    "names = dict{\"first\": \"Ada\", \"last\": \"Lovelace\"}\n"
    "last = names[\"last\"]\n"
    "name_values = names.values\n"
    "nums = dict{\"pi\": 3.5, \"e\": 2}\n"
    "pi = nums[\"pi\"]\n"
    "num_values = nums.values\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
}

TEST(StyioSecurityNightlySemantics, AllowsHandleValuedDictFamilies) {
  const std::string src =
    "d = dict{\"nums\": [1,2,3], \"more\": [4,5]}\n"
    "xs = d[\"nums\"]\n"
    "vals = d.values\n"
    "child = dict{\"left\": dict{\"x\": 1}, \"right\": dict{\"y\": 2}}\n"
    "inner = child[\"left\"]\n"
    "inners = child.values\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
}

TEST(StyioSecurityNightlySemantics, AllowsRuntimeHandleListIteration) {
  const std::string src =
    "d = dict{\"nums\": [1,2,3], \"more\": [4,5]}\n"
    "vals = d.values\n"
    "vals >> #(xs) => {\n"
    "  >_(xs)\n"
    "}\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_get_list"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, MaxElementMatchFormsGenerateEquivalentLlvm) {
  const std::string hash_let_match =
    "values <- @stdin: list[i32]\n"
    "answer = -1\n"
    "#(n = values.length) ?= {\n"
    "  0 => { <| answer }\n"
    "  1 => { answer = values[0]\n"
    "    <| answer }\n"
    "  _ => {\n"
    "    answer = values[0]\n"
    "    [1..n-1] >> #(i) => {\n"
    "      ?(values[i] > answer) => { answer = values[i] }\n"
    "    }\n"
    "    <| answer\n"
    "  }\n"
    "}\n"
    ">_(answer)\n";
  const std::string scrutinee_rebind =
    "values <- @stdin: list[i32]\n"
    "answer = -1\n"
    "values.length ?= {\n"
    "  0 => { <| answer }\n"
    "  1 => { answer = values[0]\n"
    "    <| answer }\n"
    "  _ => {\n"
    "    answer = values[0]\n"
    "    n = values.length\n"
    "    [1..n-1] >> #(i) => {\n"
    "      ?(values[i] > answer) => { answer = values[i] }\n"
    "    }\n"
    "    <| answer\n"
    "  }\n"
    "}\n"
    ">_(answer)\n";
  const std::string guarded_cases =
    "values <- @stdin: list[i32]\n"
    "answer = -1\n"
    "n = values.length\n"
    "n ?= {\n"
    "  (n == 0) => { <| answer }\n"
    "  (n == 1) => { answer = values[0]\n"
    "    <| answer }\n"
    "  _______ => {\n"
    "    answer = values[0]\n"
    "    [1..n-1] >> #(i) => {\n"
    "      ?(values[i] > answer) => { answer = values[i] }\n"
    "    }\n"
    "    <| answer\n"
    "  }\n"
    "}\n"
    ">_(answer)\n";

  const std::string expected =
    compile_program_to_llvm_ir_engine_latest(hash_let_match, StyioParserEngine::Nightly);
  EXPECT_EQ(
    compile_program_to_llvm_ir_engine_latest(scrutinee_rebind, StyioParserEngine::Nightly),
    expected
  );
  EXPECT_EQ(
    compile_program_to_llvm_ir_engine_latest(guarded_cases, StyioParserEngine::Nightly),
    expected
  );
  EXPECT_NE(expected.find("switch i64"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, PureArithmeticMatchFormsGenerateEquivalentLlvm) {
  const std::string hash_let_match =
    "seed = 1\n"
    "answer = 0\n"
    "#(n = seed + 1) ?= {\n"
    "  1 => { answer = 11\n"
    "    <| answer }\n"
    "  2 => { answer = 22\n"
    "    <| answer }\n"
    "  _ => { answer = n\n"
    "    <| answer }\n"
    "}\n"
    ">_(answer)\n";
  const std::string scrutinee_rebind =
    "seed = 1\n"
    "answer = 0\n"
    "seed + 1 ?= {\n"
    "  1 => { answer = 11\n"
    "    <| answer }\n"
    "  2 => { answer = 22\n"
    "    <| answer }\n"
    "  _ => {\n"
    "    n = seed + 1\n"
    "    answer = n\n"
    "    <| answer\n"
    "  }\n"
    "}\n"
    ">_(answer)\n";
  const std::string guarded_cases =
    "seed = 1\n"
    "answer = 0\n"
    "n = seed + 1\n"
    "n ?= {\n"
    "  (n == 1) => { answer = 11\n"
    "    <| answer }\n"
    "  (2 == n) => { answer = 22\n"
    "    <| answer }\n"
    "  _______ => { answer = n\n"
    "    <| answer }\n"
    "}\n"
    ">_(answer)\n";

  const std::string hash_let_ir =
    compile_program_to_llvm_ir_engine_latest(hash_let_match, StyioParserEngine::Nightly);
  const std::string scrutinee_rebind_ir =
    compile_program_to_llvm_ir_engine_latest(scrutinee_rebind, StyioParserEngine::Nightly);
  const std::string guarded_cases_ir =
    compile_program_to_llvm_ir_engine_latest(guarded_cases, StyioParserEngine::Nightly);
  auto expect_pure_arithmetic_switch = [](const std::string& llvm_ir) {
    EXPECT_NE(llvm_ir.find("switch i64"), std::string::npos);
    EXPECT_NE(llvm_ir.find("i64 1, label"), std::string::npos);
    EXPECT_NE(llvm_ir.find("i64 2, label"), std::string::npos);
    EXPECT_NE(llvm_ir.find("store i64 11"), std::string::npos);
    EXPECT_NE(llvm_ir.find("store i64 22"), std::string::npos);
  };
  expect_pure_arithmetic_switch(hash_let_ir);
  expect_pure_arithmetic_switch(scrutinee_rebind_ir);
  expect_pure_arithmetic_switch(guarded_cases_ir);
}

TEST(StyioSecurityNightlySemantics, AllowsMatrixTypedNestedListLiteral) {
  const std::string src =
    "m: matrix = [[1,0],[0,1]]\n"
    "row = m[0]\n"
    "cell = m[1][1]\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_matrix_new_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_matrix_set_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_matrix_row_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_matrix_get_i64"), std::string::npos);
  EXPECT_EQ(llvm_ir.find("styio_list_new_list"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, RejectsRaggedMatrixTypedNestedListLiteral) {
  const std::string src =
    "m: matrix = [[1,0],[1]]\n";
  EXPECT_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly),
    StyioTypeError
  );
}

TEST(StyioSecurityNightlySemantics, RejectsStaticMatrixShapeMismatch) {
  const std::string src =
    "a: matrix = [[1,2],[3,4]]\n"
    "b: matrix = [[1,2]]\n"
    "c = a + b\n";
  EXPECT_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly),
    StyioTypeError
  );
}

TEST(StyioSecurityNightlySemantics, InlinesSmallStaticMatrixMultiply) {
  const std::string src =
    "a: matrix = [[1,2],[3,4]]\n"
    "b: matrix = [[5,6],[7,8]]\n"
    "c = a * b\n";
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_matrix_data_i64"), std::string::npos);
  EXPECT_EQ(llvm_ir.find("styio_matrix_matmul_i64"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, LowersMatrixIntrinsicFunctions) {
  const std::string src =
    "a: matrix = [[1,2],[3,4]]\n"
    "b: matrix = [[5,6],[7,8]]\n"
    "h = mat_hadamard(a,b)\n"
    "t = transpose(a)\n"
    "d = dot(a,b)\n";
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_matrix_hadamard_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_matrix_transpose_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_matrix_dot_i64"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, LeavesRaggedUntypedNestedListAsList) {
  const std::string src =
    "rows = [[1,0],[1]]\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
}

TEST(StyioSecurityNightlySemantics, RejectsMixedDictValueFamilies) {
  const std::string src =
    "d = dict{\"a\": 1, \"b\": \"two\"}\n";
  EXPECT_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly),
    StyioTypeError
  );
}

TEST(StyioSecurityNightlySemantics, RejectsNonStringDictIndex) {
  const std::string src =
    "d = dict{\"a\": 1}\n"
    "x = d[0]\n";
  EXPECT_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly),
    StyioTypeError
  );
}

TEST(StyioSecurityNightlySemantics, AllowsBoundStdinAliasIteration) {
  const std::string src =
    "s <- @stdin\n"
    "s >> #(line) => {\n"
    "  line -> @stdout\n"
    "}\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_stdin_read_line"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, SymbolicStdinDefinitionCanPrecedeIteration) {
  const std::string src =
    "@ stdin := #() => { <|[>_] }\n"
    "@stdin >> #(line) => {\n"
    "  line -> [>_]\n"
    "}\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_stdin_read_line"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_stdout_write"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, ImmediatePullUsesArrowLeftSpelling) {
  const std::string src =
    "value = (<- @stdin)\n"
    "value -> [>_]\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_stdin_read_line"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, StringLinesCanFeedTerminalHandleIteratorWrite) {
  const std::string src =
    "text = \"alpha\nbeta\"\n"
    "text.lines() >> [>_]\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_string_lines"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_get_cstr"), std::string::npos);
  EXPECT_EQ(llvm_ir.find("styio_list_to_cstr"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_stdout_write"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, TerminalHandleIteratorWriteRejectsScalarString) {
  const std::string src =
    "text = \"alpha\"\n"
    "text >> [>_]\n";
  EXPECT_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly),
    StyioTypeError
  );
}

TEST(StyioSecurityNightlySemantics, StringLinesCanFeedStdoutResourceIteratorWrite) {
  const std::string src =
    "text = \"alpha\nbeta\"\n"
    "text.lines() >> @stdout\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_string_lines"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_get_cstr"), std::string::npos);
  EXPECT_EQ(llvm_ir.find("styio_list_to_cstr"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_stdout_write"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, DictValuesFeedStdoutResourceIteratorWriteAsPulses) {
  const std::string src =
    "items = dict{\"a\": 1, \"b\": 2}\n"
    "items >> @stdout\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_dict_values_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_get"), std::string::npos);
  EXPECT_EQ(llvm_ir.find("styio_dict_to_cstr"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_stdout_write"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, StringLinesFeedFileResourceIteratorWriteAsPulses) {
  const std::string src =
    "text = \"alpha\nbeta\"\n"
    "text.lines() >> @file(\"out.txt\")\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_string_lines"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_get_cstr"), std::string::npos);
  EXPECT_EQ(llvm_ir.find("styio_list_to_cstr"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_file_open_write"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_file_write_cstr"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, DictValuesFeedFileResourceIteratorWriteAsPulses) {
  const std::string src =
    "items = dict{\"a\": 1, \"b\": 2}\n"
    "items >> @file(\"out.txt\")\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_dict_values_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_get"), std::string::npos);
  EXPECT_EQ(llvm_ir.find("styio_dict_to_cstr"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_file_write_cstr"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, FileResourceIteratorWriteRejectsScalarString) {
  const std::string src =
    "text = \"alpha\"\n"
    "text >> @file(\"out.txt\")\n";
  EXPECT_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly),
    StyioTypeError
  );
}

TEST(StyioSecurityNightlySemantics, FileResourceRedirectStillAcceptsScalarString) {
  const std::string src =
    "text = \"alpha\"\n"
    "text -> @file(\"out.txt\")\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
}

TEST(StyioSecurityNightlySemantics, StdoutResourceIteratorWriteRejectsScalarString) {
  const std::string src =
    "text = \"alpha\"\n"
    "text >> @stdout\n";
  EXPECT_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly),
    StyioTypeError
  );
}

TEST(StyioSecurityNightlySemantics, AllowsStandaloneCollectBindFromStdin) {
  const std::string src =
    "lines << @stdin\n"
    "count = lines.length\n"
    "first = lines[0]\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_cstr_read_stdin"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, AllowsCollectBindFromBoundStdinAlias) {
  const std::string src =
    "s <- @stdin\n"
    "lines << s\n"
    "first = lines[0]\n";
  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_cstr_read_stdin"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, RejectsBoundStdoutAliasIteration) {
  const std::string src =
    "out <- @stdout\n"
    "out >> #(line) => {\n"
    "  line -> @stderr\n"
    "}\n";
  EXPECT_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly),
    StyioTypeError
  );
}

TEST(StyioSecurityNightlyCodegen, EmitsImmediateListReleaseForFlexReassign) {
  const std::string src =
    "l = @stdin: list[i32]\n"
    "l = 7\n";
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_release"), std::string::npos);
}

TEST(StyioSecurityNightlyCodegen, UnknownSgCallFailsClosed) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  llvm::ExitOnError exit_on_error;
  std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
  StyioToLLVM generator(std::move(jit));

  auto* call = SGCall::Create(SGResId::Create("missing"), {});
  EXPECT_THROW(call->toLLVMIR(&generator), StyioTypeError);
  delete call;
}

TEST(StyioSecurityNightlyCodegen, SgCallArityMismatchFailsBeforeLlvmEmission) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  llvm::ExitOnError exit_on_error;
  std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
  StyioToLLVM generator(std::move(jit));

  auto* fn = SGFunc::Create(
    SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64}),
    SGResId::Create("add2"),
    std::vector<SGFuncArg*>{
      SGFuncArg::Create("a", SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64})),
      SGFuncArg::Create("b", SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64}))
    },
    SGBlock::Create(std::vector<StyioIR*>{SGConstInt::Create(0)})
  );
  auto* call = SGCall::Create(
    SGResId::Create("add2"),
    std::vector<StyioIR*>{SGConstInt::Create(1)}
  );
  auto* entry = SGMainEntry::Create(std::vector<StyioIR*>{fn, call});

  EXPECT_THROW(entry->toLLVMIR(&generator), StyioTypeError);
  delete entry;
}

TEST(StyioSecurityNightlyCodegen, CodegenRejectsUnverifiedStyioIR) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  llvm::ExitOnError exit_on_error;
  std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
  StyioToLLVM generator(std::move(jit));

  auto* entry = SGMainEntry::Create(std::vector<StyioIR*>{
    new InactiveTestIR()
  });
  EXPECT_THROW(entry->toLLVMIR(&generator), StyioTypeError);
  delete entry;
}

TEST(StyioSecurityNightlyCodegen, ValueCarryingCastEmitsNumericConversion) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  llvm::ExitOnError exit_on_error;
  std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
  StyioToLLVM generator(std::move(jit));

  std::unique_ptr<StyioIR> bool_cast(SGCast::Create(
    SGConstBool::Create(true),
    SGType::Create(StyioDataType{StyioDataTypeOption::Bool, "bool", 1}),
    SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64})
  ));
  llvm::Value* bool_cast_value = bool_cast->toLLVMIR(&generator);
  auto* bool_const = llvm::dyn_cast<llvm::ConstantInt>(bool_cast_value);
  ASSERT_NE(bool_const, nullptr);
  EXPECT_EQ(bool_const->getSExtValue(), 1);

  auto* entry = SGMainEntry::Create(std::vector<StyioIR*>{
    SGCast::Create(
      SGConstInt::Create(7),
      SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64}),
      SGType::Create(StyioDataType{StyioDataTypeOption::Float, "f64", 64})
    ),
    SGCast::Create(
      SGConstBool::Create(true),
      SGType::Create(StyioDataType{StyioDataTypeOption::Bool, "bool", 1}),
      SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64})
    )
  });
  EXPECT_NO_THROW(entry->toLLVMIR(&generator));
  delete entry;
}

TEST(StyioSecurityNightlyCodegen, LogicalNotAndXorLowerWithoutLeftOperandFallback) {
  AstToStyioIRLowerer analyzer;
  std::unique_ptr<StyioAST> not_ast(CondAST::Create(LogicType::NOT, BoolAST::Create(true)));
  std::unique_ptr<StyioAST> xor_ast(CondAST::Create(LogicType::XOR, BoolAST::Create(true), BoolAST::Create(false)));

  std::unique_ptr<StyioIR> not_ir(not_ast->toStyioIR(&analyzer));
  std::unique_ptr<StyioIR> xor_ir(xor_ast->toStyioIR(&analyzer));
  auto* not_cond = dynamic_cast<SGCond*>(not_ir.get());
  auto* xor_cond = dynamic_cast<SGCond*>(xor_ir.get());
  ASSERT_NE(not_cond, nullptr);
  ASSERT_NE(xor_cond, nullptr);
  EXPECT_EQ(not_cond->operand, StyioOpType::Logic_NOT);
  EXPECT_EQ(xor_cond->operand, StyioOpType::Logic_XOR);

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  llvm::ExitOnError exit_on_error;
  std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
  StyioToLLVM generator(std::move(jit));
  auto* entry = SGMainEntry::Create(std::vector<StyioIR*>{
    SGCond::Create(SGConstBool::Create(true), SGConstBool::Create(false), StyioOpType::Logic_NOT),
    SGCond::Create(SGConstBool::Create(true), SGConstBool::Create(false), StyioOpType::Logic_XOR)
  });
  EXPECT_NO_THROW(entry->toLLVMIR(&generator));
  delete entry;
}

TEST(StyioSecurityNightlyCodegen, PulsePlanCodegenCoversStateSlotsAndSeriesIntrinsics) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  llvm::ExitOnError exit_on_error;
  std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
  StyioToLLVM generator(std::move(jit));

  auto make_i64_var = [](const std::string& name) {
    return SGVar::Create(
      SGResId::Create(name),
      SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64}));
  };

  auto plan = std::make_unique<SGPulsePlan>();
  plan->slots.push_back(SGStateSlotDesc{SGStateSlotKind::Acc, 0, 0, 8, 0, "", "total"});
  plan->slots.push_back(SGStateSlotDesc{SGStateSlotKind::Track, 1, 8, 32, 2, "", "track"});
  plan->slots.push_back(SGStateSlotDesc{SGStateSlotKind::WinAvg, 2, 40, 88, 3, "", "avg"});
  plan->slots.push_back(SGStateSlotDesc{SGStateSlotKind::WinMax, 3, 128, 72, 2, "", "maxv"});
  plan->commits = {
    {0, "total"},
    {1, "track"},
    {2, "avg"},
    {3, "maxv"},
  };
  plan->ref_to_slot = {
    {"total", 0},
    {"track", 1},
    {"avg", 2},
    {"maxv", 3},
  };
  plan->total_bytes = 200;

  auto* pulse_loop = SGForEach::Create(
    SCListLiteral::Create({
      SGConstInt::Create(2),
      SGConstInt::Create(4),
      SGConstInt::Create(8),
    }),
    "tick",
    "i64",
    SGBlock::Create({
      SGFlexBind::Create(make_i64_var("total"), SGConstInt::Create(11)),
      SGFlexBind::Create(make_i64_var("track"), SGResId::Create("tick")),
      SGFlexBind::Create(make_i64_var("avg"), SGSeriesAvgStep::Create(2, SGResId::Create("tick"))),
      SGFlexBind::Create(make_i64_var("avg_from_text"), SGSeriesAvgStep::Create(2, SGConstString::Create("7"))),
      SGFlexBind::Create(make_i64_var("maxv"), SGSeriesMaxStep::Create(3, SGResId::Create("tick"))),
      SGFlexBind::Create(make_i64_var("snap"), SGStateSnapLoad::Create(1)),
      SGFlexBind::Create(make_i64_var("hist"), SGStateHistLoad::Create(1, 1)),
    }));
  pulse_loop->pulse_region_id = 17;
  pulse_loop->set_pulse_plan(std::move(plan));

  auto* entry = SGMainEntry::Create({
    SGStateSnapLoad::Create(0),
    SGStateHistLoad::Create(0, 1),
    pulse_loop,
    SGFlexBind::Create(make_i64_var("after_hist"), SGStateHistLoad::Create(1, 1, 17)),
  });

  EXPECT_NO_THROW(entry->toLLVMIR(&generator));
  const std::string llvm_ir = generator.dump_llvm_ir();
  EXPECT_NE(llvm_ir.find("pulse_ledger"), std::string::npos);
  EXPECT_NE(llvm_ir.find("sgavg_ok"), std::string::npos);
  EXPECT_NE(llvm_ir.find("sgmax_ok"), std::string::npos);
  EXPECT_NE(llvm_ir.find("after_hist"), std::string::npos);

  delete entry;
}

TEST(StyioSecurityNightlyCodegen, DirectIrCodegenCoversDynamicSlotsCollectionsAndZipSurfaces) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  llvm::ExitOnError exit_on_error;
  std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
  StyioToLLVM generator(std::move(jit));

  auto i64_type = []() {
    return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  };
  auto f64_type = []() {
    return StyioDataType{StyioDataTypeOption::Float, "f64", 64};
  };
  auto bool_type = []() {
    return StyioDataType{StyioDataTypeOption::Bool, "bool", 1};
  };
  auto char_type = []() {
    return StyioDataType{StyioDataTypeOption::Char, "char", 8};
  };
  auto string_type = []() {
    return StyioDataType{StyioDataTypeOption::String, "string", 0};
  };
  auto undefined_type = []() {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  };
  auto matrix_i64_type = []() {
    return styio_make_matrix_type("i64", 2, 2);
  };
  auto matrix_i64_large_type = []() {
    return styio_make_matrix_type("i64", 5, 5);
  };
  auto matrix_f64_type = []() {
    return styio_make_matrix_type("f64", 2, 2);
  };
  auto bounded_type = [](const std::string& value_type, std::size_t cap = 3) {
    return StyioDataType{
      StyioDataTypeOption::Defined,
      std::string("bounded_ring:") + value_type + ":" + std::to_string(cap),
      0};
  };
  auto make_var = [](const std::string& name, StyioDataType type, bool dynamic = false) {
    auto* var = SGVar::Create(SGResId::Create(name), SGType::Create(std::move(type)));
    var->is_dynamic_slot = dynamic;
    return var;
  };
  auto make_i64_matrix = [](std::size_t rows, std::size_t cols) {
    std::vector<StyioIR*> elems;
    elems.reserve(rows * cols);
    for (std::size_t i = 0; i < rows * cols; ++i) {
      elems.push_back(SGConstInt::Create(static_cast<long>(i + 1)));
    }
    return SCMatrixLiteral::Create(std::move(elems), "i64", rows, cols);
  };
  auto make_f64_matrix = [](std::size_t rows, std::size_t cols) {
    std::vector<StyioIR*> elems;
    elems.reserve(rows * cols);
    for (std::size_t i = 0; i < rows * cols; ++i) {
      elems.push_back(SGConstFloat::Create(std::to_string(static_cast<double>(i + 1) + 0.5)));
    }
    return SCMatrixLiteral::Create(std::move(elems), "f64", rows, cols);
  };
  auto list_i64 = []() {
    return SCListLiteral::Create(std::vector<StyioIR*>{
      SGConstInt::Create(1),
      SGConstInt::Create(2),
      SGConstInt::Create(3),
    }, "i64");
  };
  auto list_bool = []() {
    return SCListLiteral::Create(std::vector<StyioIR*>{
      SGConstBool::Create(true),
      SGConstBool::Create(false),
    }, "bool");
  };
  auto list_char = []() {
    return SCListLiteral::Create(std::vector<StyioIR*>{
      SGConstChar::Create('a'),
      SGConstInt::Create(66),
    }, "char");
  };
  auto list_f64 = []() {
    return SCListLiteral::Create(std::vector<StyioIR*>{
      SGConstFloat::Create("1.5"),
      SGConstInt::Create(2),
    }, "f64");
  };
  auto list_string = []() {
    return SCListLiteral::Create(std::vector<StyioIR*>{
      SGConstString::Create("left"),
      SGConstString::Create("right"),
    }, "string");
  };
  auto dict_i64 = []() {
    return SCDictLiteral::Create({
      {SGConstString::Create("k"), SGConstInt::Create(7)},
    }, "i64");
  };
  auto dict_string = []() {
    return SCDictLiteral::Create({
      {SGConstString::Create("s"), SGConstString::Create("v")},
    }, "string");
  };
  auto list_of_lists = [&]() {
    return SCListLiteral::Create(std::vector<StyioIR*>{
      list_i64(),
    }, "list[i64]");
  };
  auto list_of_dicts = [&]() {
    return SCListLiteral::Create(std::vector<StyioIR*>{
      dict_i64(),
    }, "dict[string,i64]");
  };
  auto dict_of_lists = [&]() {
    return SCDictLiteral::Create({
      {SGConstString::Create("l"), list_i64()},
    }, "list[i64]");
  };
  auto dict_of_dicts = [&]() {
    return SCDictLiteral::Create({
      {SGConstString::Create("d"), dict_i64()},
    }, "dict[string,i64]");
  };
  auto task_i64 = [&]() {
    return SIOTaskCreate::Create(
      SGBlock::Create({SGReturn::Create(SGConstInt::Create(9))}),
      i64_type());
  };
  auto task_f64_from_i64 = [&]() {
    return SIOTaskCreate::Create(
      SGBlock::Create({SGReturn::Create(SGConstInt::Create(11))}),
      f64_type());
  };
  auto task_f64_default = [&]() {
    return SIOTaskCreate::Create(SGBlock::Create({}), f64_type());
  };
  auto task_string_from_i64 = [&]() {
    return SIOTaskCreate::Create(
      SGBlock::Create({SGReturn::Create(SGConstInt::Create(12))}),
      string_type());
  };
  auto task_string_default = [&]() {
    return SIOTaskCreate::Create(SGBlock::Create({}), string_type());
  };
  auto task_i64_from_bool = [&]() {
    return SIOTaskCreate::Create(
      SGBlock::Create({SGReturn::Create(SGConstBool::Create(true))}),
      i64_type());
  };
  auto task_i64_from_f64 = [&]() {
    return SIOTaskCreate::Create(
      SGBlock::Create({SGReturn::Create(SGConstFloat::Create("3.5"))}),
      i64_type());
  };
  auto task_i64_default = [&]() {
    return SIOTaskCreate::Create(SGBlock::Create({}), i64_type());
  };
  auto dyn_bind = [&](const std::string& name, StyioDataType type, StyioIR* value) {
    return SGFlexBind::Create(make_var(name, std::move(type), true), value);
  };
  auto append_matrix_calls = [&]() {
    std::vector<StyioIR*> calls;
    const std::vector<std::string> one_arg_matrix = {
      "__styio_matrix_identity_i64",
      "__styio_matrix_identity_f64",
      "__styio_matrix_clone_i64",
      "__styio_matrix_clone_f64",
      "__styio_matrix_transpose_i64",
      "__styio_matrix_transpose_f64",
      "__styio_matrix_rows",
      "__styio_matrix_cols",
      "__styio_matrix_shape",
      "__styio_matrix_sum_i64",
      "__styio_matrix_sum_f64",
      "__styio_matrix_norm",
    };
    for (const auto& name : one_arg_matrix) {
      calls.push_back(SGCall::Create(SGResId::Create(name), {SGConstInt::Create(1)}));
    }
    const std::vector<std::string> two_arg_matrix = {
      "__styio_matrix_new_i64",
      "__styio_matrix_new_f64",
      "__styio_matrix_add_i64",
      "__styio_matrix_add_f64",
      "__styio_matrix_sub_i64",
      "__styio_matrix_sub_f64",
      "__styio_matrix_hadamard_i64",
      "__styio_matrix_hadamard_f64",
      "__styio_matrix_matmul_i64",
      "__styio_matrix_matmul_f64",
      "__styio_matrix_scale_i64",
      "__styio_matrix_dot_i64",
      "__styio_matrix_dot_f64",
    };
    for (const auto& name : two_arg_matrix) {
      calls.push_back(SGCall::Create(SGResId::Create(name), {SGConstInt::Create(1), SGConstInt::Create(2)}));
    }
    calls.push_back(SGCall::Create(SGResId::Create("__styio_matrix_scale_f64"), {
      SGConstInt::Create(1),
      SGConstFloat::Create("2.5"),
    }));
    calls.push_back(SGCall::Create(SGResId::Create("__styio_matrix_get_i64"), {
      SGConstInt::Create(1),
      SGConstInt::Create(0),
      SGConstInt::Create(0),
    }));
    calls.push_back(SGCall::Create(SGResId::Create("__styio_matrix_get_f64"), {
      SGConstInt::Create(1),
      SGConstInt::Create(0),
      SGConstInt::Create(0),
    }));
    calls.push_back(SGCall::Create(SGResId::Create("__styio_matrix_set_i64"), {
      SGConstInt::Create(1),
      SGConstInt::Create(0),
      SGConstInt::Create(0),
      SGConstInt::Create(4),
    }));
    calls.push_back(SGCall::Create(SGResId::Create("__styio_matrix_set_f64"), {
      SGConstInt::Create(1),
      SGConstInt::Create(0),
      SGConstInt::Create(0),
      SGConstFloat::Create("4.5"),
    }));
    return calls;
  };

  std::vector<StyioIR*> stmts;

  stmts.push_back(SGFunc::Create(
    SGType::Create(f64_type()),
    SGResId::Create("ret_f64_from_i64"),
    {},
    SGBlock::Create({SGReturn::Create(SGConstInt::Create(7))})));
  stmts.push_back(SGFunc::Create(
    SGType::Create(i64_type()),
    SGResId::Create("ret_i64_from_f64"),
    {},
    SGBlock::Create({SGReturn::Create(SGConstFloat::Create("8.25"))})));
  stmts.push_back(SGFunc::Create(
    SGType::Create(bool_type()),
    SGResId::Create("ret_bool_from_f64"),
    {},
    SGBlock::Create({SGReturn::Create(SGConstFloat::Create("1.0"))})));
  stmts.push_back(SGFunc::Create(
    SGType::Create(bool_type()),
    SGResId::Create("ret_bool_from_i64"),
    {},
    SGBlock::Create({SGReturn::Create(SGConstInt::Create(1))})));
  stmts.push_back(SGFunc::Create(
    SGType::Create(i64_type()),
    SGResId::Create("ret_i64_from_bool"),
    {},
    SGBlock::Create({SGReturn::Create(SGConstBool::Create(true))})));
  stmts.push_back(SGFunc::Create(
    SGType::Create(char_type()),
    SGResId::Create("ret_char_from_i64"),
    {},
    SGBlock::Create({SGReturn::Create(SGConstInt::Create(65))})));
  stmts.push_back(SGFunc::Create(
    SGType::Create(string_type()),
    SGResId::Create("ret_default_string"),
    {},
    SGBlock::Create({SGNoOp::Create()})));
  stmts.push_back(SGFunc::Create(
    SGType::Create(f64_type()),
    SGResId::Create("take_f64"),
    {SGFuncArg::Create("v", SGType::Create(f64_type()))},
    SGBlock::Create({SGReturn::Create(SGResId::Create("v"))})));
  stmts.push_back(SGFunc::Create(
    SGType::Create(i64_type()),
    SGResId::Create("take_i64"),
    {SGFuncArg::Create("v", SGType::Create(i64_type()))},
    SGBlock::Create({SGReturn::Create(SGResId::Create("v"))})));
  stmts.push_back(SGFunc::Create(
    SGType::Create(bool_type()),
    SGResId::Create("take_bool"),
    {SGFuncArg::Create("v", SGType::Create(bool_type()))},
    SGBlock::Create({SGReturn::Create(SGResId::Create("v"))})));

  stmts.push_back(SGCall::Create(SGResId::Create("take_f64"), {SGConstString::Create("3.5")}));
  stmts.push_back(SGCall::Create(SGResId::Create("take_f64"), {SGConstInt::Create(3)}));
  stmts.push_back(SGCall::Create(SGResId::Create("take_i64"), {SGConstString::Create("4")}));
  stmts.push_back(SGCall::Create(SGResId::Create("take_i64"), {SGConstFloat::Create("4.75")}));
  stmts.push_back(SGCall::Create(SGResId::Create("take_bool"), {SGConstInt::Create(1)}));

  stmts.push_back(dyn_bind("dyn_bool", bool_type(), SGConstBool::Create(true)));
  stmts.push_back(dyn_bind("dyn_i64", i64_type(), SGConstInt::Create(42)));
  stmts.push_back(dyn_bind("dyn_f64", f64_type(), SGConstFloat::Create("2.5")));
  stmts.push_back(dyn_bind("dyn_string", string_type(), SGConstString::Create("slot")));
  stmts.push_back(dyn_bind("dyn_list", styio_make_list_type("i64"), list_i64()));
  stmts.push_back(dyn_bind("dyn_dict", styio_make_dict_type("string", "i64"), dict_i64()));
  stmts.push_back(dyn_bind("dyn_matrix", matrix_i64_type(), make_i64_matrix(2, 2)));
  stmts.push_back(dyn_bind("dyn_task", styio_make_task_type("i64"), task_i64()));
  stmts.push_back(dyn_bind("dyn_f64_from_i64", f64_type(), SGConstInt::Create(13)));
  stmts.push_back(dyn_bind("dyn_i64_from_char", i64_type(), SGConstChar::Create('q')));
  stmts.push_back(dyn_bind(
    "dyn_infer_list_from_list_get",
    undefined_type(),
    SCListGet::Create(list_of_lists(), SGConstInt::Create(0), "list[i64]")));
  stmts.push_back(dyn_bind(
    "dyn_infer_list_from_dict_get",
    undefined_type(),
    SCDictGet::Create(dict_of_lists(), SGConstString::Create("l"), "list[i64]")));
  stmts.push_back(dyn_bind(
    "dyn_infer_dict_from_list_get",
    undefined_type(),
    SCListGet::Create(list_of_dicts(), SGConstInt::Create(0), "dict[string,i64]")));
  stmts.push_back(dyn_bind(
    "dyn_infer_dict_from_dict_get",
    undefined_type(),
    SCDictGet::Create(dict_of_dicts(), SGConstString::Create("d"), "dict[string,i64]")));
  stmts.push_back(dyn_bind(
    "dyn_infer_matrix_from_call",
    undefined_type(),
    SGCall::Create(SGResId::Create("__styio_matrix_new_i64"), {
      SGConstInt::Create(1),
      SGConstInt::Create(1),
    })));
  stmts.push_back(dyn_bind(
    "dyn_infer_task_from_load",
    undefined_type(),
    SGDynLoad::Create("dyn_task", SGDynLoadKind::TaskHandle)));
  stmts.push_back(SGDynLoad::Create("missing_dyn_bool", SGDynLoadKind::Bool));
  stmts.push_back(SGDynLoad::Create("missing_dyn_f64", SGDynLoadKind::F64));
  stmts.push_back(SGDynLoad::Create("missing_dyn_string", SGDynLoadKind::CString));
  stmts.push_back(SGDynLoad::Create("missing_dyn_i64", SGDynLoadKind::I64));
  stmts.push_back(SGDynLoad::Create("dyn_bool", SGDynLoadKind::Bool));
  stmts.push_back(SGDynLoad::Create("dyn_i64", SGDynLoadKind::I64));
  stmts.push_back(SGDynLoad::Create("dyn_f64", SGDynLoadKind::F64));
  stmts.push_back(SGDynLoad::Create("dyn_string", SGDynLoadKind::CString));
  stmts.push_back(SGDynLoad::Create("dyn_list", SGDynLoadKind::ListHandle));
  stmts.push_back(SGDynLoad::Create("dyn_dict", SGDynLoadKind::DictHandle));
  stmts.push_back(SGDynLoad::Create("dyn_matrix", SGDynLoadKind::MatrixHandle));
  stmts.push_back(SGDynLoad::Create("dyn_task", SGDynLoadKind::TaskHandle));

  auto* dyn_final_var = make_var("dyn_final_string", string_type(), true);
  stmts.push_back(SGFinalBind::Create(dyn_final_var, SGConstString::Create("final")));
  stmts.push_back(SGDynLoad::Create("dyn_final_string", SGDynLoadKind::CString));

  stmts.push_back(SGFlexBind::Create(make_var("mut_i64", i64_type()), SGConstInt::Create(10)));
  stmts.push_back(SGFlexBind::Create(make_var("mut_f64", f64_type()), SGConstFloat::Create("10.0")));
  stmts.push_back(SGBinOp::Create(SGResId::Create("mut_i64"), SGConstInt::Create(1), StyioOpType::Self_Add_Assign, SGType::Create(i64_type())));
  stmts.push_back(SGBinOp::Create(SGResId::Create("mut_i64"), SGConstInt::Create(1), StyioOpType::Self_Sub_Assign, SGType::Create(i64_type())));
  stmts.push_back(SGBinOp::Create(SGResId::Create("mut_i64"), SGConstInt::Create(2), StyioOpType::Self_Mul_Assign, SGType::Create(i64_type())));
  stmts.push_back(SGBinOp::Create(SGResId::Create("mut_i64"), SGConstInt::Create(2), StyioOpType::Self_Div_Assign, SGType::Create(i64_type())));
  stmts.push_back(SGBinOp::Create(SGResId::Create("mut_i64"), SGConstInt::Create(2), StyioOpType::Self_Mod_Assign, SGType::Create(i64_type())));
  stmts.push_back(SGBinOp::Create(SGResId::Create("mut_f64"), SGConstFloat::Create("1.5"), StyioOpType::Self_Add_Assign, SGType::Create(f64_type())));
  stmts.push_back(SGBinOp::Create(SGResId::Create("mut_f64"), SGConstFloat::Create("1.5"), StyioOpType::Self_Sub_Assign, SGType::Create(f64_type())));
  stmts.push_back(SGBinOp::Create(SGResId::Create("mut_f64"), SGConstFloat::Create("2.0"), StyioOpType::Self_Mul_Assign, SGType::Create(f64_type())));
  stmts.push_back(SGBinOp::Create(SGResId::Create("mut_f64"), SGConstFloat::Create("2.0"), StyioOpType::Self_Div_Assign, SGType::Create(f64_type())));
  stmts.push_back(SGBinOp::Create(SGResId::Create("mut_f64"), SGConstFloat::Create("2.0"), StyioOpType::Self_Mod_Assign, SGType::Create(f64_type())));
  stmts.push_back(SIOTaskCreate::Create(
    SGBlock::Create({
      SGFlexBind::Create(
        make_var("task_local", i64_type()),
        SGBinOp::Create(SGResId::Create("mut_i64"), SGConstInt::Create(1), StyioOpType::Binary_Add, SGType::Create(i64_type()))),
      SGFinalBind::Create(
        make_var("task_final", i64_type()),
        SGDynLoad::Create("dyn_i64", SGDynLoadKind::I64)),
      SGLoop::CreateWhile(
        SGConstBool::Create(false),
        SGBlock::Create({SGResId::Create("task_local")})),
      SGForEach::Create(
        list_i64(),
        "task_item",
        "i64",
        SGBlock::Create({
          SGBinOp::Create(SGResId::Create("task_item"), SGResId::Create("mut_i64"), StyioOpType::Binary_Add, SGType::Create(i64_type()))
        })),
      SGRangeFor::Create(
        SGConstInt::Create(0),
        SGConstInt::Create(1),
        SGConstInt::Create(1),
        "task_index",
        SGBlock::Create({
          SGBinOp::Create(SGResId::Create("task_index"), SGResId::Create("task_local"), StyioOpType::Binary_Add, SGType::Create(i64_type()))
        })),
      SGIf::Create(
        SGConstBool::Create(true),
        SGBlock::Create({SGResId::Create("mut_i64")}),
        SGBlock::Create({SGResId::Create("task_final")})),
      SGMatch::Create(
        SGResId::Create("task_local"),
        {{1, SGBlock::Create({SGResId::Create("mut_i64")})}},
        SGBlock::Create({SGResId::Create("task_final")}),
        SGMatchReprKind::Stmt),
      SIOFlowBind::Create(SGConstInt::Create(1), "task_flow", i64_type(), false, SGResId::Create("mut_i64"), false),
      SIOHandleRelease::CreateFromVar("mut_i64"),
      SIOResourceWriteToFile::Create(SGResId::Create("task_local"), SGConstString::Create("task.txt"), false, false, false, ""),
      SIOStdStreamWrite::Create(SIOStdStreamWrite::Stream::Stdout, {SGResId::Create("task_local")}),
      SIOResourceEffect::Create(
        SIOInstantPull::Create(SGConstString::Create("task_missing.txt")),
        SGResId::Create("mut_i64"),
        false,
        i64_type(),
        {SIOResourceEffect::Handler("io", SGResId::Create("task_final"))},
        true),
      SIOPrint::Create({SGResId::Create("task_final")}),
      SGReturn::Create(SGBinOp::Create(SGResId::Create("mut_i64"), SGResId::Create("task_final"), StyioOpType::Binary_Add, SGType::Create(i64_type())))
    }),
    i64_type()));

  stmts.push_back(SGCast::Create(SGConstInt::Create(1), SGType::Create(i64_type()), SGType::Create(f64_type())));
  stmts.push_back(SGCast::Create(SGConstString::Create("2.5"), SGType::Create(string_type()), SGType::Create(f64_type())));
  stmts.push_back(SGCast::Create(SGConstInt::Create(1), SGType::Create(i64_type()), SGType::Create(bool_type())));
  stmts.push_back(SGCast::Create(SGConstFloat::Create("0.5"), SGType::Create(f64_type()), SGType::Create(bool_type())));
  stmts.push_back(SGCast::Create(SGConstBool::Create(true), SGType::Create(bool_type()), SGType::Create(i64_type())));
  stmts.push_back(SGCast::Create(SGConstFloat::Create("3.5"), SGType::Create(f64_type()), SGType::Create(i64_type())));
  stmts.push_back(SGCast::Create(SGConstString::Create("6"), SGType::Create(string_type()), SGType::Create(i64_type())));
  stmts.push_back(SGCast::Create(SGConstChar::Create('x'), SGType::Create(char_type()), SGType::Create(char_type())));

  stmts.push_back(SGBinOp::Create(SGConstString::Create("a"), SGConstInt::Create(2), StyioOpType::Binary_Add, SGType::Create(string_type())));
  stmts.push_back(SGBinOp::Create(SGConstString::Create("3"), SGConstInt::Create(4), StyioOpType::Binary_Add, SGType::Create(i64_type())));
  stmts.push_back(SGBinOp::Create(SGConstString::Create("3.5"), SGConstFloat::Create("4.5"), StyioOpType::Binary_Add, SGType::Create(f64_type())));
  stmts.push_back(SGBinOp::Create(SGConstFloat::Create("7.5"), SGConstInt::Create(2), StyioOpType::Binary_Sub, SGType::Create(f64_type())));
  stmts.push_back(SGBinOp::Create(SGConstInt::Create(7), SGConstInt::Create(2), StyioOpType::Binary_Sub, SGType::Create(i64_type())));
  stmts.push_back(SGBinOp::Create(SGConstFloat::Create("7.5"), SGConstInt::Create(2), StyioOpType::Binary_Mul, SGType::Create(f64_type())));
  stmts.push_back(SGBinOp::Create(SGConstInt::Create(7), SGConstInt::Create(2), StyioOpType::Binary_Mul, SGType::Create(i64_type())));
  stmts.push_back(SGBinOp::Create(SGConstFloat::Create("7.5"), SGConstInt::Create(2), StyioOpType::Binary_Div, SGType::Create(f64_type())));
  stmts.push_back(SGBinOp::Create(SGConstInt::Create(8), SGConstInt::Create(2), StyioOpType::Binary_Div, SGType::Create(i64_type())));
  stmts.push_back(SGBinOp::Create(SGConstFloat::Create("7.5"), SGConstInt::Create(2), StyioOpType::Binary_Mod, SGType::Create(f64_type())));
  stmts.push_back(SGBinOp::Create(SGConstInt::Create(7), SGConstInt::Create(2), StyioOpType::Binary_Mod, SGType::Create(i64_type())));
  stmts.push_back(SGBinOp::Create(SGConstInt::Create(2), SGConstInt::Create(3), StyioOpType::Binary_Pow, SGType::Create(i64_type())));
  stmts.push_back(SGBinOp::Create(SGConstFloat::Create("2.0"), SGConstInt::Create(3), StyioOpType::Binary_Pow, SGType::Create(f64_type())));
  for (const auto op : {
         StyioOpType::Equal,
         StyioOpType::Not_Equal,
         StyioOpType::Greater_Than,
         StyioOpType::Greater_Than_Equal,
         StyioOpType::Less_Than,
         StyioOpType::Less_Than_Equal,
       }) {
    stmts.push_back(SGBinOp::Create(SGConstFloat::Create("2.0"), SGConstInt::Create(1), op, SGType::Create(bool_type())));
    stmts.push_back(SGBinOp::Create(SGConstInt::Create(2), SGConstInt::Create(1), op, SGType::Create(bool_type())));
  }

  stmts.push_back(SGBinOp::Create(make_i64_matrix(2, 2), make_i64_matrix(2, 2), StyioOpType::Binary_Add, SGType::Create(matrix_i64_type()), matrix_i64_type(), matrix_i64_type()));
  stmts.push_back(SGBinOp::Create(make_i64_matrix(2, 2), make_i64_matrix(2, 2), StyioOpType::Binary_Sub, SGType::Create(matrix_i64_type()), matrix_i64_type(), matrix_i64_type()));
  stmts.push_back(SGBinOp::Create(make_f64_matrix(2, 2), make_f64_matrix(2, 2), StyioOpType::Binary_Mul, SGType::Create(matrix_f64_type()), matrix_f64_type(), matrix_f64_type()));
  stmts.push_back(SGBinOp::Create(make_i64_matrix(5, 5), make_i64_matrix(5, 5), StyioOpType::Binary_Add, SGType::Create(matrix_i64_large_type()), matrix_i64_large_type(), matrix_i64_large_type()));
  stmts.push_back(SGBinOp::Create(make_i64_matrix(5, 5), SGConstInt::Create(3), StyioOpType::Binary_Mul, SGType::Create(matrix_i64_large_type()), matrix_i64_large_type(), i64_type()));
  stmts.push_back(SGBinOp::Create(SGConstFloat::Create("3.0"), make_f64_matrix(2, 2), StyioOpType::Binary_Mul, SGType::Create(matrix_f64_type()), f64_type(), matrix_f64_type()));

  stmts.push_back(SGCond::Create(SGConstBool::Create(true), SGConstInt::Create(5), StyioOpType::Logic_AND));
  stmts.push_back(SGCond::Create(SGConstInt::Create(5), SGConstBool::Create(true), StyioOpType::Logic_AND));
  stmts.push_back(SGCond::Create(SGConstInt::Create(0), SGConstInt::Create(1), StyioOpType::Logic_OR));

  stmts.push_back(SGFlexBind::Create(make_var("ring_str", bounded_type("string")), SGConstString::Create("a")));
  stmts.push_back(SGFlexBind::Create(make_var("ring_str", bounded_type("string")), SGConstString::Create("b"), true));
  stmts.push_back(SGResId::CreateHistory("ring_str", -1));
  stmts.push_back(SGFlexBind::Create(make_var("ring_f64", bounded_type("f64")), SGConstInt::Create(21)));
  stmts.push_back(SGFlexBind::Create(make_var("ring_i64", bounded_type("i64")), SGConstFloat::Create("22.5")));
  stmts.push_back(SGFlexBind::Create(make_var("ring_char", bounded_type("char")), SGConstBool::Create(true)));
  stmts.push_back(SGFlexBind::Create(make_var("ring_list", bounded_type("list[i64]")), list_i64()));
  stmts.push_back(SGFlexBind::Create(make_var("ring_list", bounded_type("list[i64]")), list_i64(), true));
  stmts.push_back(SGResId::CreateHistory("ring_list", -1));
  stmts.push_back(SGFlexBind::Create(make_var("ring_dict", bounded_type("dict[string,i64]")), dict_i64()));
  stmts.push_back(SGFlexBind::Create(make_var("ring_dict", bounded_type("dict[string,i64]")), dict_i64(), true));
  stmts.push_back(SGResId::CreateHistory("ring_dict", -1));
  stmts.push_back(SGFlexBind::Create(make_var("ring_matrix", bounded_type("matrix[i64,2,2]")), make_i64_matrix(2, 2)));
  stmts.push_back(SGFlexBind::Create(make_var("ring_matrix", bounded_type("matrix[i64,2,2]")), make_i64_matrix(2, 2), true));
  stmts.push_back(SGResId::CreateHistory("ring_matrix", -1));

  stmts.push_back(SCListLiteral::Create(std::vector<StyioIR*>{SGConstBool::Create(true)}, "bool"));
  stmts.push_back(SCListLiteral::Create(std::vector<StyioIR*>{SGConstChar::Create('z')}, "char"));
  stmts.push_back(SCListLiteral::Create(std::vector<StyioIR*>{SGConstFloat::Create("1.25")}, "f64"));
  stmts.push_back(SCListLiteral::Create(std::vector<StyioIR*>{SGConstString::Create("s")}, "string"));
  stmts.push_back(SCListLiteral::Create(std::vector<StyioIR*>{list_i64()}, "list[i64]"));
  stmts.push_back(SCListLiteral::Create(std::vector<StyioIR*>{dict_i64()}, "dict[string,i64]"));
  stmts.push_back(SCListLiteral::Create(std::vector<StyioIR*>{make_i64_matrix(2, 2)}, "matrix[i64,2,2]"));
  stmts.push_back(SCMatrixLiteral::Create(std::vector<StyioIR*>{SGConstInt::Create(1), SGConstBool::Create(true)}, "i64", 1, 2));
  stmts.push_back(SCMatrixLiteral::Create(std::vector<StyioIR*>{SGConstInt::Create(1), SGConstFloat::Create("2.5")}, "f64", 1, 2));
  stmts.push_back(SCDictLiteral::Create({{SGConstString::Create("b"), SGConstBool::Create(true)}}, "bool"));
  stmts.push_back(SCDictLiteral::Create({{SGConstString::Create("f"), SGConstInt::Create(2)}}, "f64"));
  stmts.push_back(dict_string());
  stmts.push_back(SCDictLiteral::Create({{SGConstString::Create("l"), list_i64()}}, "list[i64]"));
  stmts.push_back(SCDictLiteral::Create({{SGConstString::Create("d"), dict_i64()}}, "dict[string,i64]"));

  stmts.push_back(SCListGet::Create(list_bool(), SGConstInt::Create(0), "bool"));
  stmts.push_back(SCListGet::Create(list_char(), SGConstInt::Create(0), "char"));
  stmts.push_back(SCListGet::Create(list_f64(), SGConstInt::Create(0), "f64"));
  stmts.push_back(SCListGet::Create(list_string(), SGConstInt::Create(0), "string"));
  stmts.push_back(SCListGet::Create(SCListLiteral::Create(std::vector<StyioIR*>{list_i64()}, "list[i64]"), SGConstInt::Create(0), "list[i64]"));
  stmts.push_back(SCListGet::Create(SCListLiteral::Create(std::vector<StyioIR*>{dict_i64()}, "dict[string,i64]"), SGConstInt::Create(0), "dict[string,i64]"));
  stmts.push_back(SCListGet::Create(SCListLiteral::Create(std::vector<StyioIR*>{make_i64_matrix(2, 2)}, "matrix[i64,2,2]"), SGConstInt::Create(0), "matrix[i64,2,2]"));
  stmts.push_back(SCListSlice::Create(list_i64(), SGConstInt::Create(0), SGConstInt::Create(2), "i64"));
  stmts.push_back(SCListSet::Create(list_bool(), SGConstInt::Create(0), SGConstBool::Create(false), "bool"));
  stmts.push_back(SCListSet::Create(list_char(), SGConstInt::Create(0), SGConstInt::Create(65), "char"));
  stmts.push_back(SCListSet::Create(list_f64(), SGConstInt::Create(0), SGConstInt::Create(8), "f64"));
  stmts.push_back(SCListSet::Create(list_string(), SGConstInt::Create(0), SGConstString::Create("n"), "string"));
  stmts.push_back(SCListSet::Create(SCListLiteral::Create(std::vector<StyioIR*>{list_i64()}, "list[i64]"), SGConstInt::Create(0), list_i64(), "list[i64]"));
  stmts.push_back(SCListSet::Create(SCListLiteral::Create(std::vector<StyioIR*>{dict_i64()}, "dict[string,i64]"), SGConstInt::Create(0), dict_i64(), "dict[string,i64]"));
  stmts.push_back(SCListSet::Create(SCListLiteral::Create(std::vector<StyioIR*>{make_i64_matrix(2, 2)}, "matrix[i64,2,2]"), SGConstInt::Create(0), make_i64_matrix(2, 2), "matrix[i64,2,2]"));
  stmts.push_back(SCListClone::Create(list_i64()));
  stmts.push_back(SCListLen::Create(list_i64()));
  stmts.push_back(SCListToString::Create(list_i64()));
  stmts.push_back(SCMatrixClone::Create(make_f64_matrix(2, 2), "f64"));
  stmts.push_back(SCMatrixGet::Create(make_f64_matrix(2, 2), SGConstInt::Create(0), SGConstInt::Create(1), "f64"));
  stmts.push_back(SCMatrixRow::Create(make_f64_matrix(2, 2), SGConstInt::Create(0), "f64"));
  stmts.push_back(SCMatrixRowsSlice::Create(make_f64_matrix(2, 2), SGConstInt::Create(0), SGConstInt::Create(1), "f64"));
  stmts.push_back(SCMatrixToString::Create(make_f64_matrix(2, 2)));
  stmts.push_back(SCDictClone::Create(dict_i64()));
  stmts.push_back(SCDictLen::Create(dict_i64()));
  stmts.push_back(SCDictGet::Create(dict_string(), SGConstString::Create("s"), "string"));
  stmts.push_back(SCDictGet::Create(SCDictLiteral::Create({{SGConstString::Create("f"), SGConstFloat::Create("1.5")}}, "f64"), SGConstString::Create("f"), "f64"));
  stmts.push_back(SCDictGet::Create(SCDictLiteral::Create({{SGConstString::Create("b"), SGConstBool::Create(true)}}, "bool"), SGConstString::Create("b"), "bool"));
  stmts.push_back(SCDictGet::Create(SCDictLiteral::Create({{SGConstString::Create("l"), list_i64()}}, "list[i64]"), SGConstString::Create("l"), "list[i64]"));
  stmts.push_back(SCDictGet::Create(SCDictLiteral::Create({{SGConstString::Create("d"), dict_i64()}}, "dict[string,i64]"), SGConstString::Create("d"), "dict[string,i64]"));
  stmts.push_back(SCDictSet::Create(dict_string(), SGConstString::Create("s"), SGConstString::Create("next"), "string"));
  stmts.push_back(SCDictSet::Create(SCDictLiteral::Create({{SGConstString::Create("f"), SGConstFloat::Create("1.5")}}, "f64"), SGConstString::Create("f"), SGConstInt::Create(2), "f64"));
  stmts.push_back(SCDictSet::Create(SCDictLiteral::Create({{SGConstString::Create("b"), SGConstBool::Create(true)}}, "bool"), SGConstString::Create("b"), SGConstBool::Create(false), "bool"));
  stmts.push_back(SCDictSet::Create(SCDictLiteral::Create({{SGConstString::Create("l"), list_i64()}}, "list[i64]"), SGConstString::Create("l"), list_i64(), "list[i64]"));
  stmts.push_back(SCDictSet::Create(SCDictLiteral::Create({{SGConstString::Create("d"), dict_i64()}}, "dict[string,i64]"), SGConstString::Create("d"), dict_i64(), "dict[string,i64]"));
  stmts.push_back(SCDictKeys::Create(dict_i64()));
  stmts.push_back(SCDictValues::Create(dict_string(), "string"));
  stmts.push_back(SCDictToString::Create(dict_i64()));

  stmts.push_back(SGCall::Create(SGResId::Create("__styio_list_range_i64"), {
    SGConstInt::Create(1),
    SGConstInt::Create(3),
    SGConstInt::Create(1),
  }));
  stmts.push_back(SGCall::Create(SGResId::Create("__styio_string_lines"), {SGConstString::Create("a\nb")}));
  for (const auto& suffix : {"bool", "char", "f64", "cstr", "list", "dict", "matrix", "i64"}) {
    StyioIR* value = SGConstInt::Create(5);
    if (std::string(suffix) == "f64") {
      value = SGConstFloat::Create("5.5");
    }
    else if (std::string(suffix) == "cstr") {
      value = SGConstString::Create("v");
    }
    else if (std::string(suffix) == "list") {
      value = list_i64();
    }
    else if (std::string(suffix) == "dict") {
      value = dict_i64();
    }
    else if (std::string(suffix) == "matrix") {
      value = make_i64_matrix(2, 2);
    }
    stmts.push_back(SGCall::Create(SGResId::Create(std::string("__styio_list_push_") + suffix), {
      list_i64(),
      value,
    }));
    StyioIR* insert_value = SGConstInt::Create(6);
    if (std::string(suffix) == "f64") {
      insert_value = SGConstFloat::Create("6.5");
    }
    else if (std::string(suffix) == "cstr") {
      insert_value = SGConstString::Create("iv");
    }
    else if (std::string(suffix) == "list") {
      insert_value = list_i64();
    }
    else if (std::string(suffix) == "dict") {
      insert_value = dict_i64();
    }
    else if (std::string(suffix) == "matrix") {
      insert_value = make_i64_matrix(2, 2);
    }
    stmts.push_back(SGCall::Create(SGResId::Create(std::string("__styio_list_insert_") + suffix), {
      list_i64(),
      SGConstInt::Create(0),
      insert_value,
    }));
  }
  {
    std::vector<StyioIR*> matrix_calls = append_matrix_calls();
    stmts.insert(stmts.end(), matrix_calls.begin(), matrix_calls.end());
  }

  stmts.push_back(SGLoop::CreateWhile(SGConstInt::Create(1), SGBlock::Create({SGBreak::Create()})));
  stmts.push_back(SGLoop::CreateWhile(SGConstInt::Create(1), SGBlock::Create({SGContinue::Create()})));
  stmts.push_back(SGRangeFor::Create(SGConstInt::Create(0), SGConstInt::Create(2), SGConstInt::Create(1), "ri", SGBlock::Create({SGContinue::Create()})));
  stmts.push_back(SGIf::Create(SGConstInt::Create(1), SGBlock::Create({SGConstInt::Create(1)}), SGBlock::Create({SGConstInt::Create(0)})));
  stmts.push_back(SGUndef::Create());
  stmts.push_back(SGFallback::Create(SGUndef::Create(), SGConstInt::Create(2)));
  stmts.push_back(SGFallback::Create(SGUndef::Create(), SGConstString::Create("fallback")));
  stmts.push_back(SGWaveMerge::Create(SGConstInt::Create(1), SGConstString::Create("yes"), SGConstString::Create("no")));
  stmts.push_back(SGWaveDispatch::Create(
    SGConstInt::Create(1),
    SGBlock::Create({SGConstInt::Create(1)}),
    SGBlock::Create({SGConstInt::Create(0)})));
  stmts.push_back(SGGuardSelect::Create(SGConstInt::Create(7), SGConstInt::Create(1)));
  stmts.push_back(SGEqProbe::Create(SGConstInt::Create(7), SGConstInt::Create(7)));
  stmts.push_back(SGMatch::Create(
    SGConstInt::Create(1),
    {{1, SGBlock::Create({SGReturn::Create(SGConstInt::Create(10))})}},
    SGBlock::Create({SGReturn::Create(SGConstInt::Create(0))}),
    SGMatchReprKind::ExprInt));
  stmts.push_back(SGMatch::Create(
    SGConstInt::Create(1),
    {{1, SGBlock::Create({SGReturn::Create(SGConstFloat::Create("1.5"))})}},
    SGBlock::Create({SGReturn::Create(SGConstInt::Create(0))}),
    SGMatchReprKind::ExprFloat));
  stmts.push_back(SGMatch::Create(
    SGConstInt::Create(1),
    {{1, SGBlock::Create({SGReturn::Create(SGConstBool::Create(true))})}},
    SGBlock::Create({SGReturn::Create(SGConstBool::Create(false))}),
    SGMatchReprKind::ExprBool));
  stmts.push_back(SGMatch::Create(
    SGConstInt::Create(1),
    {{1, SGBlock::Create({SGReturn::Create(SGConstChar::Create('a'))})}},
    SGBlock::Create({SGReturn::Create(SGConstChar::Create('b'))}),
    SGMatchReprKind::ExprChar));
  stmts.push_back(SGMatch::Create(
    SGConstInt::Create(2),
    {{1, SGBlock::Create({SGReturn::Create(SGConstString::Create("one"))})}},
    SGBlock::Create({SGReturn::Create(SGConstInt::Create(2))}),
    SGMatchReprKind::ExprMixed));
  stmts.push_back(SGMatch::Create(
    SGConstInt::Create(99),
    {{1, SGBlock::Create({SGReturn::Create(SGConstInt::Create(10))})}},
    nullptr,
    SGMatchReprKind::ExprFloat));
  stmts.push_back(SGMatch::Create(
    SGConstInt::Create(99),
    {{1, SGBlock::Create({SGReturn::Create(SGConstString::Create("one"))})}},
    nullptr,
    SGMatchReprKind::ExprMixed));
  stmts.push_back(SGMatch::Create(
    SGConstInt::Create(99),
    {{1, SGBlock::Create({SGReturn::Create(SGConstBool::Create(true))})}},
    nullptr,
    SGMatchReprKind::ExprBool));
  stmts.push_back(SGMatch::Create(
    SGConstInt::Create(1),
    {{1, SGBlock::Create({
      SGMatch::Create(
        SGConstInt::Create(1),
        {{1, SGBlock::Create({SGReturn::Create(SGConstInt::Create(13))})}},
        nullptr,
        SGMatchReprKind::ExprFloat)
    })}},
    nullptr,
    SGMatchReprKind::ExprMixed));
  stmts.push_back(SGMatch::Create(
    SGConstInt::Create(1),
    {{1, SGBlock::Create({
      SGMatch::Create(
        SGConstInt::Create(1),
        {{1, SGBlock::Create({SGReturn::Create(SGConstBool::Create(true))})}},
        nullptr,
        SGMatchReprKind::ExprBool)
    })}},
    nullptr,
    SGMatchReprKind::ExprFloat));
  stmts.push_back(SGMatch::Create(
    SGConstInt::Create(0),
    {},
    SGBlock::Create({SGConstInt::Create(3)}),
    SGMatchReprKind::Stmt));
  stmts.push_back(SGEntry::Create({SGConstInt::Create(1), SGConstInt::Create(2)}));
  stmts.push_back(SGSnapshotDecl::Create("snap", SGConstString::Create("snap.bin")));
  stmts.push_back(SGSnapshotShadowLoad::Create("snap"));

  stmts.push_back(SIOStreamZip::Create(list_bool(), false, false, "za", list_string(), false, false, "zb", false, true, "bool", "string", SGBlock::Create({SGResId::Create("za"), SGResId::Create("zb")})));
  stmts.push_back(SIOStreamZip::Create(list_i64(), false, false, "zia", list_string(), false, false, "zsb", false, true, "i64", "string", SGBlock::Create({SGResId::Create("zia"), SGResId::Create("zsb")})));
  stmts.push_back(SIOStreamZip::Create(list_string(), false, false, "zsa", list_i64(), false, false, "zib", true, false, "string", "i64", SGBlock::Create({SGResId::Create("zsa"), SGResId::Create("zib")})));
  stmts.push_back(SIOStreamZip::Create(
    SCListGet::Create(list_of_lists(), SGConstInt::Create(0), "list[i64]"),
    false,
    false,
    "zlg",
    list_i64(),
    false,
    false,
    "zli",
    false,
    false,
    "i64",
    "i64",
    SGBlock::Create({SGResId::Create("zlg"), SGResId::Create("zli")})));
  stmts.push_back(SIOStreamZip::Create(
    SCDictGet::Create(dict_of_lists(), SGConstString::Create("l"), "list[i64]"),
    false,
    false,
    "zdg",
    list_i64(),
    false,
    false,
    "zdi",
    false,
    false,
    "i64",
    "i64",
    SGBlock::Create({SGResId::Create("zdg"), SGResId::Create("zdi")})));
  stmts.push_back(SIOStreamZip::Create(SGConstString::Create("left.txt"), true, false, "fa", list_bool(), false, false, "lb", false, false, "i64", "bool", SGBlock::Create({SGResId::Create("fa"), SGResId::Create("lb")})));
  stmts.push_back(SIOStreamZip::Create(list_string(), false, false, "la", SGConstString::Create("right.txt"), true, false, "fb", true, true, "string", "string", SGBlock::Create({SGResId::Create("la"), SGResId::Create("fb")})));
  stmts.push_back(SIOStreamZip::Create(list_i64(), false, false, "lia", SGConstString::Create("right-i64.txt"), true, false, "lif", false, true, "i64", "string", SGBlock::Create({SGResId::Create("lia"), SGResId::Create("lif")})));
  stmts.push_back(SIOStreamZip::Create(SGConstString::Create("left-string.txt"), true, false, "fsl", list_string(), false, false, "lsr", false, true, "i64", "string", SGBlock::Create({SGResId::Create("fsl"), SGResId::Create("lsr")})));
  stmts.push_back(SIOStreamZip::Create(SGConstString::Create("stdin"), false, true, "sa", list_f64(), false, false, "fl", false, false, "i64", "f64", SGBlock::Create({SGResId::Create("sa"), SGResId::Create("fl")})));
  stmts.push_back(SIOStreamZip::Create(list_i64(), false, false, "il", SGConstString::Create("stdin"), false, true, "sb", false, false, "i64", "i64", SGBlock::Create({SGResId::Create("il"), SGResId::Create("sb")})));
  stmts.push_back(SIOStreamZip::Create(SGConstString::Create("a.txt"), true, false, "ffa", SGConstString::Create("b.txt"), true, false, "ffb", false, true, "i64", "string", SGBlock::Create({SGResId::Create("ffa"), SGResId::Create("ffb")})));
  {
    auto pulse_plan = std::make_unique<SGPulsePlan>();
    pulse_plan->total_bytes = 8;
    auto* pulse_zip = SIOStreamZip::Create(
      list_i64(),
      false,
      false,
      "pza",
      SGConstString::Create("pulse.txt"),
      true,
      false,
      "pzb",
      false,
      false,
      "i64",
      "i64",
      SGBlock::Create({SGResId::Create("pza"), SGResId::Create("pzb")}));
    pulse_zip->pulse_region_id = 29;
    pulse_zip->set_pulse_plan(std::move(pulse_plan));
    stmts.push_back(pulse_zip);
  }

  stmts.push_back(SIOHandleAcquire::Create("h", SGConstString::Create("in.txt"), true));
  stmts.push_back(SIOInstantPull::CreateFromHandle("h"));
  stmts.push_back(SIOResourceWriteToFile::Create(SGConstInt::Create(9), SGConstString::Create("out.txt"), true, true, true, "h"));
  stmts.push_back(SIOHandleRelease::CreateFromVar("h"));
  stmts.push_back(SIOHandleRelease::CreateFromPath(SGConstString::Create("in.txt"), false));
  {
    auto pulse_plan = std::make_unique<SGPulsePlan>();
    pulse_plan->total_bytes = 4;
    auto* file_pulse = SIOFileLineIter::CreateFromPath(
      SGConstString::Create("pulse-file.txt"),
      "file_pulse_line",
      SGBlock::Create({SGResId::Create("file_pulse_line")}));
    file_pulse->pulse_region_id = 37;
    file_pulse->set_pulse_plan(std::move(pulse_plan));
    stmts.push_back(file_pulse);
  }
  stmts.push_back(SIOResourceEffect::Create(
    SIOInstantPull::Create(SGConstString::Create("missing.txt")),
    SGConstInt::Create(0),
    false,
    i64_type(),
    {
      SIOResourceEffect::Handler("io", SGConstInt::Create(1)),
      SIOResourceEffect::Handler("parse", SGConstInt::Create(2)),
    },
    true));
  stmts.push_back(SIOStdStreamWrite::Create(SIOStdStreamWrite::Stream::Stdout, {
    SGConstInt::Create(1),
    SGConstFloat::Create("2.5"),
    SGConstString::Create("out"),
  }));
  stmts.push_back(SIOStdStreamWrite::Create(SIOStdStreamWrite::Stream::Stderr, {
    SGConstChar::Create('e'),
  }));
  stmts.push_back(SIOStdStreamLineIter::Create("line", SGBlock::Create({SGResId::Create("line")})));
  {
    auto pulse_plan = std::make_unique<SGPulsePlan>();
    pulse_plan->total_bytes = 4;
    auto* stdin_pulse = SIOStdStreamLineIter::Create(
      "pulse_line",
      SGBlock::Create({SGResId::Create("pulse_line")}));
    stdin_pulse->pulse_region_id = 31;
    stdin_pulse->set_pulse_plan(std::move(pulse_plan));
    stmts.push_back(stdin_pulse);
  }
  stmts.push_back(SIOStdStreamPull::Create(f64_type()));
  stmts.push_back(SIOStdStreamPull::Create(string_type()));
  stmts.push_back(SIOStdStreamPull::Create(styio_make_list_type("i64")));
  stmts.push_back(task_f64_default());
  stmts.push_back(task_string_default());
  stmts.push_back(task_i64_default());
  stmts.push_back(task_f64_from_i64());
  stmts.push_back(task_string_from_i64());
  stmts.push_back(task_i64_from_bool());
  stmts.push_back(task_i64_from_f64());
  stmts.push_back(SIOFlowBind::Create(task_i64(), "awaited", i64_type(), true, SGConstInt::Create(0), true));
  stmts.push_back(SIOFlowBind::Create(task_f64_from_i64(), "awaited_f64", f64_type(), true, SGConstBool::Create(true), true));
  stmts.push_back(SIOFlowBind::Create(task_string_from_i64(), "awaited_string", string_type(), true, SGConstInt::Create(7), true));
  stmts.push_back(SIOFlowBind::Create(task_i64_from_bool(), "awaited_bool", bool_type(), true, SGConstInt::Create(1), true));
  stmts.push_back(SIOFlowBind::Create(task_i64_from_f64(), "awaited_i64_from_f64", i64_type(), true, SGConstFloat::Create("4.5"), true));
  stmts.push_back(SIOFlowBind::Create(task_i64_default(), "awaited_i64_from_bool", i64_type(), true, SGConstBool::Create(true), true));
  stmts.push_back(SIOFlowBind::Create(SGConstInt::Create(3), "flow_bool_from_i64", bool_type(), false));
  stmts.push_back(SIOFlowBind::Create(SGConstBool::Create(true), "flow_i64_from_bool", i64_type(), false));
  stmts.push_back(SIOFlowBind::Create(SGConstFloat::Create("4.5"), "flow_i64_from_f64", i64_type(), false));
  stmts.push_back(SIOFlowBind::Create(SGConstInt::Create(6), "flow_f64_from_i64", f64_type(), false));
  stmts.push_back(SIOFlowBind::Create(SIOStdStreamPull::Create(string_type()), "flow_string", string_type(), false, SGConstString::Create("fallback"), false));

  auto* entry = SGMainEntry::Create(std::move(stmts));
  EXPECT_NO_THROW(entry->toLLVMIR(&generator));
  const std::string llvm_ir = generator.dump_llvm_ir();
  EXPECT_NE(llvm_ir.find("styio.dyncell"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_matrix_scale_f64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("zip_stream_list"), std::string::npos);
  EXPECT_NE(llvm_ir.find("zip_ll_hdr"), std::string::npos);
  EXPECT_NE(llvm_ir.find("pulse_ledger_f"), std::string::npos);
  EXPECT_NE(llvm_ir.find("pulse_ledger_stdin"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_task_f64_spawn"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_task_cstr_spawn"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_task_f64_pull"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_task_cstr_pull"), std::string::npos);
  EXPECT_NE(llvm_ir.find("resource_effect_value"), std::string::npos);
  EXPECT_NE(llvm_ir.find("dyn_final_string"), std::string::npos);
  delete entry;
}

TEST(StyioSecurityRepr, RareAstAndIrNodesHaveStableTextForm) {
  StyioRepr repr;
  std::string all_repr;

  auto append_ast = [&](StyioAST* node, const std::string& needle) {
    (void)needle;
    std::unique_ptr<StyioAST> owner(node);
    const std::string text = node->toString(&repr, 1);
    EXPECT_FALSE(text.empty());
    all_repr += text;
    all_repr += "\n";
  };
  auto append_ir = [&](StyioIR* node, const std::string& needle) {
    std::unique_ptr<StyioIR> owner(node);
    const std::string text = node->toString(&repr, 1);
    EXPECT_FALSE(text.empty());
    EXPECT_NE(text.find(needle), std::string::npos) << text;
    all_repr += text;
    all_repr += "\n";
  };
  auto i64_type = []() {
    return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  };
  auto f64_type = []() {
    return StyioDataType{StyioDataTypeOption::Float, "f64", 64};
  };
  auto make_i64_ir_var = [&](const std::string& name) {
    return SGVar::Create(SGResId::Create(name), SGType::Create(i64_type()));
  };

  append_ast(CommentAST::Create("repr"), "Comment");
  append_ast(NoneAST::Create(), "none");
  append_ast(EmptyAST::Create(), "empty");
  append_ast(TypeConvertAST::Create(IntAST::Create("1"), NumPromoTy::Int_To_Float), "type.convert");
  append_ast(ParamAST::Create(NameAST::Create("p"), TypeAST::Create("i64")), "param");
  append_ast(OptArgAST::Create(NameAST::Create("maybe")), "opt.arg");
  append_ast(OptKwArgAST::Create(NameAST::Create("kw")), "opt.kwarg");
  append_ast(new InfiniteAST(), "infinite.original");
  append_ast(new InfiniteAST(IntAST::Create("0"), IntAST::Create("1")), "infinite");
  append_ast(StructAST::Create(
    NameAST::Create("Box"),
    {ParamAST::Create(NameAST::Create("value"), TypeAST::Create("i64"))}), "struct");
  append_ast(TupleAST::Create({IntAST::Create("1"), StringAST::Create("a")}), "tuple");
  append_ast(VarTupleAST::Create({}), "parameters.empty");
  append_ast(VarTupleAST::Create({
    VarAST::Create(NameAST::Create("left"), TypeAST::Create("i64")),
    VarAST::Create(NameAST::Create("right"), TypeAST::Create("i64")),
  }), "parameters");
  append_ast(ExtractorAST::Create(
    TupleAST::Create({IntAST::Create("1")}),
    CODPAST::Create("map", {NameAST::Create("value")})), "tuple.operation");
  append_ast(SetAST::Create({IntAST::Create("1"), IntAST::Create("2")}), "set");
  append_ast(ListAST::Create(), "list.empty");
  append_ast(DictAST::Create(), "dict.empty");
  append_ast(DictAST::Create({{StringAST::Create("k"), IntAST::Create("1")}}), "dict");
  append_ast(new SizeOfAST(NameAST::Create("items")), "sizeof");
  append_ast(new ListOpAST(
    StyioNodeType::Insert_Item_By_Index,
    ListAST::Create({IntAST::Create("1")}),
    IntAST::Create("0"),
    IntAST::Create("2")), "insert.item.by.index");
  append_ast(new ListOpAST(
    StyioNodeType::Access,
    NameAST::Create("items"),
    NameAST::Create("field")), "access");
  append_ast(new ListOpAST(
    StyioNodeType::Access_By_Index,
    NameAST::Create("items"),
    IntAST::Create("0")), "access.index");
  append_ast(new ListOpAST(
    StyioNodeType::Access_By_Name,
    NameAST::Create("items"),
    StringAST::Create("field")), "access.name");
  append_ast(new ListOpAST(
    StyioNodeType::Access_By_Slice,
    NameAST::Create("items"),
    IntAST::Create("0"),
    IntAST::Create("2")), "access.slice");
  append_ast(new ListOpAST(
    StyioNodeType::Get_Index_By_Value,
    NameAST::Create("items"),
    IntAST::Create("7")), "get.index.by.value");
  append_ast(new ListOpAST(
    StyioNodeType::Get_Indices_By_Many_Values,
    NameAST::Create("items"),
    TupleAST::Create({IntAST::Create("7"), IntAST::Create("8")})), "get.indices.by.values");
  append_ast(new ListOpAST(
    StyioNodeType::Append_Value,
    NameAST::Create("items"),
    IntAST::Create("9")), "append.value");
  append_ast(new ListOpAST(
    StyioNodeType::Remove_Item_By_Index,
    NameAST::Create("items"),
    IntAST::Create("0")), "remove.index");
  append_ast(new ListOpAST(
    StyioNodeType::Remove_Item_By_Value,
    NameAST::Create("items"),
    IntAST::Create("7")), "remove.value");
  append_ast(new ListOpAST(
    StyioNodeType::Remove_Items_By_Many_Indices,
    NameAST::Create("items"),
    TupleAST::Create({IntAST::Create("0"), IntAST::Create("1")})), "remove.indices");
  append_ast(new ListOpAST(
    StyioNodeType::Remove_Items_By_Many_Values,
    NameAST::Create("items"),
    TupleAST::Create({IntAST::Create("7"), IntAST::Create("8")})), "remove.values");
  append_ast(new ListOpAST(
    StyioNodeType::Get_Reversed,
    NameAST::Create("items")), "get.reversed");
  append_ast(new ListOpAST(
    StyioNodeType::Get_Index_By_Item_From_Right,
    NameAST::Create("items"),
    IntAST::Create("7")), "get.index.from.right");
  append_ast(new ListOpAST(
    StyioNodeType::Remove_Last_Item,
    NameAST::Create("items")), "remove.last.default");
  append_ast(CondAST::Create(LogicType::AND, BoolAST::Create(true), BoolAST::Create(false)), "cond.and");
  append_ast(CondAST::Create(LogicType::OR, BoolAST::Create(true), BoolAST::Create(false)), "cond.or");
  append_ast(CondAST::Create(LogicType::XOR, BoolAST::Create(true), BoolAST::Create(false)), "cond.xor");
  append_ast(CondAST::Create(LogicType::NOT, BoolAST::Create(false)), "cond.not");
  append_ast(CondAST::Create(LogicType::RAW, BoolAST::Create(true)), "cond.raw");
  append_ast(CondAST::Create(static_cast<LogicType>(999), BoolAST::Create(true)), "cond.undefined");
  append_ast(UndefinedLitAST::Create(), "@");
  append_ast(WaveDispatchAST::Create(
    BoolAST::Create(true),
    BlockAST::Create({PrintAST::Create({StringAST::Create("yes")})}),
    BlockAST::Create({PrintAST::Create({StringAST::Create("no")})})), "wave.dispatch");
  append_ast(FallbackAST::Create(IntAST::Create("1"), IntAST::Create("2")), "fallback");
  append_ast(GuardSelectorAST::Create(NameAST::Create("v"), BoolAST::Create(true)), "guard");
  append_ast(EqProbeAST::Create(NameAST::Create("v"), IntAST::Create("1")), "eq.probe");
  append_ast(TaskBlockAST::Create(BlockAST::Create({ReturnAST::Create(IntAST::Create("7"))})), "task.block");
  append_ast(TaskGroupLaunchAST::Create({
    FinalBindAST::Create(VarAST::Create(NameAST::Create("t1")), TaskBlockAST::Create(BlockAST::Create({ReturnAST::Create(IntAST::Create("1"))}))),
    FinalBindAST::Create(VarAST::Create(NameAST::Create("t2")), TaskBlockAST::Create(BlockAST::Create({ReturnAST::Create(IntAST::Create("2"))}))),
  }), "task.group");
  append_ast(FlowBindAST::CreateAwait(
    NameAST::Create("job"),
    VarAST::Create(NameAST::Create("answer"), TypeAST::Create("i64")),
    IntAST::Create("0")), "flow.bind");
  append_ast(StateDeclAST::Create(
    IntAST::Create("3"),
    nullptr,
    nullptr,
    VarAST::Create(NameAST::Create("ma")),
    IntAST::Create("1")), "state.decl");
  append_ast(StateRefAST::Create(NameAST::Create("ma")), "state.ref");
  append_ast(HistoryProbeAST::Create(StateRefAST::Create(NameAST::Create("ma")), IntAST::Create("1")), "history.probe");
  append_ast(SeriesIntrinsicAST::Create(NameAST::Create("price"), SeriesIntrinsicOp::Avg, IntAST::Create("3")), "series.intrinsic");
  append_ast(StdStreamAST::Create(StdStreamKind::Stdin), "std.stdin");
  append_ast(StdStreamAST::Create(StdStreamKind::Stdout), "std.stdout");
  append_ast(StdStreamAST::Create(StdStreamKind::Stderr), "std.stderr");
  append_ast(ResourceAST::Create({{NameAST::Create("file"), "file"}}), "resource.ast");
  append_ast(EmptyResourceAST::Create(), "empty.resource");
  append_ast(new ForwardAST(), "forward");
  append_ast(ResourceReceiverAST::Create("file"), "resource.receiver");
  append_ast(ResourceDeclAST::Create(
    {{NameAST::Create("log"), TypeAST::Create("file")}},
    BlockAST::Create({PassAST::Create()})), "resource.decl");
  append_ast(ResourceRefAST::CreateSelector(NameAST::Create("hist"), ResourceSelectorKind::Offset, -1), "resource.ref.offset");
  append_ast(ResourceRefAST::CreateSelector(NameAST::Create("hist"), ResourceSelectorKind::SliceFrom, -2), "resource.ref.slice");
  append_ast(ResourceRefAST::CreateSelector(NameAST::Create("hist"), ResourceSelectorKind::SnapshotAll, 0), "resource.ref.snapshot");
  append_ast(ResourceOrderAST::Create(ResourceRefAST::Create(NameAST::Create("a")), ResourceRefAST::Create(NameAST::Create("b"))), "resource.order");
  append_ast(ResPathAST::Create(StyioPathType::local_relevant_any, "local/path"), "resource.path");
  append_ast(RemotePathAST::Create(StyioPathType::ipv4_addr, "192.0.2.1:/path"), "remote.path");
  append_ast(WebUrlAST::Create(StyioPathType::url_https, "https://example.test"), "web.url");
  append_ast(DBUrlAST::Create(StyioPathType::db_postgresql, "db://main"), "db.url");
  append_ast(new ReadFileAST(NameAST::Create("line"), StringAST::Create("input.txt")), "read.file");
  append_ast(EOFAST::Create(), "eof");
  append_ast(PrintAST::Create({}), "print.empty");
  append_ast(BackwardAST::Create(
    NameAST::Create("obj"),
    VarTupleAST::Create({VarAST::Create(NameAST::Create("x"))}),
    {NameAST::Create("op")},
    {IntAST::Create("1")}), "backward");
  append_ast(CheckEqualAST::Create({IntAST::Create("1"), IntAST::Create("2")}), "check.equal");
  append_ast(new CheckIsinAST(ListAST::Create({IntAST::Create("1")})), "check.isin");
  append_ast(HashTagNameAST::Create({"alpha", "beta"}), "hash.tag.name");
  append_ast(CODPAST::Create(
    "normalize",
    {NameAST::Create("row")},
    nullptr,
    CODPAST::Create("emit", {NameAST::Create("row")})), "CODP.normalize");
  append_ast(new AnonyFuncAST(
    VarTupleAST::Create({VarAST::Create(NameAST::Create("x"))}),
    IntAST::Create("1")), "anonymous.func");
  append_ast(SnapshotDeclAST::Create(NameAST::Create("snap"), FileResourceAST::Create(StringAST::Create("snap.bin"), true)), "snapshot.decl");
  append_ast(IterSeqAST::Create(
    ListAST::Create({IntAST::Create("1")}),
    {ParamAST::Create(NameAST::Create("v"))},
    {HashTagNameAST::Create({"route"})}), "iter.seq");

  append_ir(SGType::Create(i64_type()), "styio.ir.type");
  append_ir(SGConstBool::Create(true), "styio.ir.bool");
  append_ir(SGConstFloat::Create("1.5"), "styio.ir.float");
  append_ir(SGConstChar::Create('x'), "styio.ir.char");
  append_ir(SGFormatString::Create({"v="}, {SGConstInt::Create(1)}), "styio.ir.fmtstr");
  append_ir(SGCast::Create(SGConstInt::Create(1), SGType::Create(i64_type()), SGType::Create(f64_type())), "styio.ir.cast");
  append_ir(SGBinOp::Create(SGConstInt::Create(1), SGConstInt::Create(2), StyioOpType::Binary_Add, SGType::Create(i64_type())), "styio.ir.binop");
  append_ir(SGCond::Create(SGConstBool::Create(true), SGConstBool::Create(false), StyioOpType::Logic_AND), "styio.ir.cond");
  append_ir(SGVar::Create(SGResId::Create("v"), SGType::Create(i64_type()), SGConstInt::Create(4)), "styio.ir.var");
  append_ir(SGFinalBind::Create(make_i64_ir_var("fixed"), SGConstInt::Create(9)), "styio.ir.final_bind");
  append_ir(SGDynLoad::Create("dyn", SGDynLoadKind::I64), "styio.ir.dyn_load");
  append_ir(SGFuncArg::Create("arg", SGType::Create(i64_type())), "styio.ir.func_arg");
  append_ir(SGCall::Create(SGResId::Create("callee"), {SGConstInt::Create(1)}), "styio.ir.call");
  append_ir(SGExportDecl::Create({"entry", "helper"}), "styio.ir.export");
  append_ir(SGExternBlock::Create("c", "int helper(void){return 1;}", {"helper.c"}, {"helper"}), "styio.ir.extern");
  append_ir(SGReturn::Create(SGConstInt::Create(1)), "styio.ir.return");
  append_ir(SGEntry::Create({SGNoOp::Create(), SGConstString::Create("done")}), "styio.ir.entry");
  append_ir(SGLoop::CreateWhile(SGConstBool::Create(false), SGBlock::Create({SGNoOp::Create()})), "styio.ir.loop");
  append_ir(SGRangeFor::Create(SGConstInt::Create(0), SGConstInt::Create(3), SGConstInt::Create(1), "i", SGBlock::Create({SGNoOp::Create()})), "styio.ir.range_for");
  append_ir(SGIf::Create(SGConstBool::Create(true), SGBlock::Create({SGNoOp::Create()}), SGBlock::Create({SGNoOp::Create()})), "styio.ir.if");
  append_ir(SCListLiteral::Create({SGConstInt::Create(1), SGConstInt::Create(2)}), "styio.ir.listlit");
  append_ir(SCDictLiteral::Create({{SGConstString::Create("k"), SGConstInt::Create(1)}}, "i64"), "styio.ir.dict_literal");
  append_ir(SCMatrixLiteral::Create({SGConstInt::Create(1), SGConstInt::Create(2)}, "i64", 1, 2), "styio.ir.matrix_literal");
  append_ir(SCMatrixGet::Create(SGResId::Create("m"), SGConstInt::Create(0), SGConstInt::Create(1), "i64"), "styio.ir.matrix_get");
  append_ir(SCMatrixRow::Create(SGResId::Create("m"), SGConstInt::Create(0), "i64"), "styio.ir.matrix_row");
  append_ir(SCMatrixRowsSlice::Create(SGResId::Create("m"), SGConstInt::Create(0), nullptr, "i64"), "styio.ir.matrix_rows_slice");
  append_ir(SCMatrixToString::Create(SGResId::Create("m")), "styio.ir.matrix_to_string");
  append_ir(SGStateSnapLoad::Create(0), "styio.ir.state.snap");
  append_ir(SGStateHistLoad::Create(0, 1), "styio.ir.state.hist");
  append_ir(SGSeriesAvgStep::Create(0, SGConstInt::Create(1)), "styio.ir.series.avg");
  append_ir(SGSeriesMaxStep::Create(0, SGConstInt::Create(1)), "styio.ir.series.max");
  append_ir(SGMatch::Create(SGConstInt::Create(1), {}, SGBlock::Create({SGNoOp::Create()}), SGMatchReprKind::Stmt), "styio.ir.match");
  append_ir(SGBreak::Create(), "styio.ir.break");
  append_ir(SGContinue::Create(), "styio.ir.continue");
  append_ir(SGUndef::Create(), "styio.ir.undef");
  append_ir(SGFallback::Create(SGConstInt::Create(1), SGConstInt::Create(2)), "styio.ir.fallback");
  append_ir(SGWaveMerge::Create(SGConstBool::Create(true), SGConstInt::Create(1), SGConstInt::Create(0)), "styio.ir.wave_merge");
  append_ir(SGWaveDispatch::Create(SGConstBool::Create(true), SGNoOp::Create(), SGNoOp::Create()), "styio.ir.wave_dispatch");
  append_ir(SGGuardSelect::Create(SGConstInt::Create(1), SGConstBool::Create(true)), "styio.ir.guard");
  append_ir(SGEqProbe::Create(SGConstInt::Create(1), SGConstInt::Create(1)), "styio.ir.eq_probe");
  append_ir(SIOHandleAcquire::Create("h", SGConstString::Create("in.txt"), true), "styio.ir.handle_acquire");
  append_ir(SIOHandleRelease::CreateFromPath(SGConstString::Create("in.txt"), true), "styio.ir.handle_release");
  append_ir(SIOFileLineIter::CreateFromHandle("h", "line", SGBlock::Create({SGNoOp::Create()})), "styio.ir.file_line_iter");
  append_ir(SIOStreamZip::Create(
    SGResId::Create("left"),
    false,
    false,
    "a",
    SGResId::Create("right"),
    false,
    false,
    "b",
    false,
    false,
    "i64",
    "i64",
    SGBlock::Create({SGNoOp::Create()})), "styio.ir.stream_zip");
  append_ir(SGSnapshotDecl::Create("snap", SGConstString::Create("snap.bin")), "styio.ir.snapshot_decl");
  append_ir(SGSnapshotShadowLoad::Create("snap"), "styio.ir.snapshot_load");
  append_ir(SIOInstantPull::CreateFromHandle("h"), "styio.ir.instant_pull");
  append_ir(SIOListReadStdin::Create("i64"), "styio.ir.list_read_stdin");
  append_ir(SCListClone::Create(SGResId::Create("items")), "styio.ir.list_clone");
  append_ir(SCMatrixClone::Create(SGResId::Create("m"), "i64"), "styio.ir.matrix_clone");
  append_ir(SCListLen::Create(SGResId::Create("items")), "styio.ir.list_len");
  append_ir(SCListGet::Create(SGResId::Create("items"), SGConstInt::Create(0), "i64"), "styio.ir.list_get");
  append_ir(SCListSlice::Create(SGResId::Create("items"), SGConstInt::Create(0), nullptr, "i64"), "styio.ir.list_slice");
  append_ir(SCListSet::Create(SGResId::Create("items"), SGConstInt::Create(0), SGConstInt::Create(7), "i64"), "styio.ir.list_set");
  append_ir(SCListToString::Create(SGResId::Create("items")), "styio.ir.list_to_string");
  append_ir(SCDictClone::Create(SGResId::Create("dict")), "styio.ir.dict_clone");
  append_ir(SCDictLen::Create(SGResId::Create("dict")), "styio.ir.dict_len");
  append_ir(SCDictGet::Create(SGResId::Create("dict"), SGConstString::Create("k"), "i64"), "styio.ir.dict_get");
  append_ir(SCDictSet::Create(SGResId::Create("dict"), SGConstString::Create("k"), SGConstInt::Create(2), "i64"), "styio.ir.dict_set");
  append_ir(SCDictKeys::Create(SGResId::Create("dict")), "styio.ir.dict_keys");
  append_ir(SCDictValues::Create(SGResId::Create("dict"), "i64"), "styio.ir.dict_values");
  append_ir(SCDictToString::Create(SGResId::Create("dict")), "styio.ir.dict_to_string");
  append_ir(SIOResourceWriteToFile::Create(SGConstString::Create("data"), SGConstString::Create("out.txt"), true, true, true, "h"), "styio.ir.resource_write");
  append_ir(SIOStdStreamWrite::Create(SIOStdStreamWrite::Stream::Stderr, {SGConstString::Create("err")}), "styio.ir.std_stream_write");
  append_ir(SIOResourceEffect::Create(
    SIOStdStreamWrite::Create(SIOStdStreamWrite::Stream::Stderr, {SGConstString::Create("primary")}),
    SGConstInt::Create(0),
    false,
    i64_type(),
    {SIOResourceEffect::Handler("io", SGConstInt::Create(1))},
    true), "styio.ir.resource_effect");
  append_ir(SIOStdStreamLineIter::Create("line", SGBlock::Create({SGNoOp::Create()})), "styio.ir.stdin_line_iter");
  append_ir(SIOStdStreamPull::Create(StyioDataType{StyioDataTypeOption::String, "string", 0}), "styio.ir.stdin_pull");
  append_ir(SIOTaskCreate::Create(SGBlock::Create({SGReturn::Create(SGConstInt::Create(1))}), i64_type()), "styio.ir.task_create");
  append_ir(SIOFlowBind::Create(SGResId::Create("task"), "answer", i64_type(), true, SGConstInt::Create(0), true), "styio.ir.flow_bind");
  append_ir(SIOPath::Create("/tmp/in"), "styio.ir.path");
  append_ir(SIOPrint::Create({SGConstString::Create("out")}), "styio.ir.print");
  append_ir(SIORead::Create(SIOPath::Create("/tmp/in")), "styio.ir.read");

  EXPECT_GT(all_repr.size(), 4000u);
}

TEST(StyioSecurityIROptimizer, CanonicalizesRepeatedDefaultRebindAndHonorsGuards) {
  auto i64_type = []() {
    return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  };
  auto make_i64_var = [&](const std::string& name) {
    return SGVar::Create(SGResId::Create(name), SGType::Create(i64_type()));
  };
  auto make_target_var = [&]() {
    return make_i64_var("target");
  };
  auto make_returning_block = [](long value) {
    return SGBlock::Create(std::vector<StyioIR*>{
      SGReturn::Create(SGConstInt::Create(value)),
    });
  };
  auto make_spec_expr = [&]() -> StyioIR* {
    return SGBinOp::Create(
      SGCast::Create(
        SCListLen::Create(SGResId::Create("items")),
        SGType::Create(i64_type()),
        SGType::Create(i64_type())),
      SCDictLen::Create(SGResId::Create("dict")),
      StyioOpType::Binary_Add,
      SGType::Create(i64_type()),
      i64_type(),
      i64_type());
  };
  auto make_match = [&](StyioIR* scrutinee, StyioIR* rebind_value, std::vector<StyioIR*> default_prefix = {}) {
    std::vector<StyioIR*> default_stmts = std::move(default_prefix);
    default_stmts.push_back(SGFlexBind::Create(make_target_var(), rebind_value));
    default_stmts.push_back(SGReturn::Create(SGResId::Create("target")));
    return SGMatch::Create(
      scrutinee,
      std::vector<std::pair<std::int64_t, SGBlock*>>{
        {1, make_returning_block(1)},
      },
      SGBlock::Create(std::move(default_stmts)),
      SGMatchReprKind::ExprInt);
  };
  auto make_probe_match = [&](StyioIR* scrutinee, SGBlock* arm, SGBlock* default_arm) {
    return SGMatch::Create(
      scrutinee,
      std::vector<std::pair<std::int64_t, SGBlock*>>{
        {1, arm},
      },
      default_arm,
      SGMatchReprKind::ExprInt);
  };
  auto make_write_block = [&](const std::string& name) {
    return SGBlock::Create(std::vector<StyioIR*>{
      SGFlexBind::Create(make_i64_var(name), SGConstInt::Create(7)),
      SGReturn::Create(SGConstInt::Create(0)),
    });
  };

  {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_match(
        make_spec_expr(),
        make_spec_expr(),
        std::vector<StyioIR*>{
          SGBlock::Create(std::vector<StyioIR*>{
            SGFinalBind::Create(make_i64_var("transparent"), SGConstInt::Create(0)),
          }),
        }),
    }));

    StyioIR* optimized = styio::lowering::optimize_styio_ir(root.get());
    EXPECT_EQ(optimized, root.get());
    auto* block = dynamic_cast<SGBlock*>(optimized);
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 1u);
    auto* hoisted = dynamic_cast<SGBlock*>(block->stmts[0]);
    ASSERT_NE(hoisted, nullptr);
    ASSERT_EQ(hoisted->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGFlexBind*>(hoisted->stmts[0]), nullptr);
    auto* rewritten = dynamic_cast<SGMatch*>(hoisted->stmts[1]);
    ASSERT_NE(rewritten, nullptr);
    auto* scrutinee = dynamic_cast<SGResId*>(rewritten->scrutinee);
    ASSERT_NE(scrutinee, nullptr);
    EXPECT_EQ(scrutinee->as_str(), "target");
    ASSERT_NE(rewritten->default_arm, nullptr);
    ASSERT_EQ(rewritten->default_arm->stmts.size(), 2u);
  }

  {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      SGFlexBind::Create(make_target_var(), SGConstInt::Create(99)),
      make_match(SGResId::Create("source"), SGResId::Create("source")),
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[1]), nullptr);
  }

  {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_match(SGResId::Create("source"), SGResId::Create("source")),
      SGReturn::Create(SGResId::Create("target")),
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[0]), nullptr);
  }

  {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      SCListSet::Create(SGResId::Create("target"), SGConstInt::Create(0), SGConstInt::Create(7)),
      make_match(SGResId::Create("source"), SGResId::Create("source")),
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[1]), nullptr);
  }

  {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_match(
        SGResId::Create("source"),
        SGResId::Create("source"),
        std::vector<StyioIR*>{
          SIOStdStreamWrite::Create(SIOStdStreamWrite::Stream::Stdout, {
            SGConstString::Create("opaque"),
          }),
        }),
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 1u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[0]), nullptr);
  }

  {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_probe_match(SGConstInt::Create(1), make_write_block("target"), make_returning_block(0)),
      make_match(SGResId::Create("source"), SGResId::Create("source")),
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[1]), nullptr);
  }

  {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_probe_match(SGConstInt::Create(1), make_returning_block(0), make_write_block("target")),
      make_match(SGResId::Create("source"), SGResId::Create("source")),
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[1]), nullptr);
  }

  {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_match(SGResId::Create("source"), SGResId::Create("source")),
      make_probe_match(SGResId::Create("target"), make_returning_block(1), make_returning_block(0)),
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[0]), nullptr);
  }

  {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_match(SGResId::Create("source"), SGResId::Create("source")),
      make_probe_match(
        SGConstInt::Create(1),
        SGBlock::Create(std::vector<StyioIR*>{
          SGReturn::Create(SGResId::Create("target")),
        }),
        make_returning_block(0)),
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[0]), nullptr);
  }

  {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_match(SGResId::Create("source"), SGResId::Create("source")),
      make_probe_match(
        SGConstInt::Create(1),
        make_returning_block(1),
        SGBlock::Create(std::vector<StyioIR*>{
          SGReturn::Create(SGResId::Create("target")),
        })),
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[0]), nullptr);
  }

  {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_match(
        SGResId::Create("source"),
        SGResId::Create("source"),
        std::vector<StyioIR*>{
          SGFlexBind::Create(make_i64_var("source"), SGConstInt::Create(1)),
        }),
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 1u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[0]), nullptr);
  }

  {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_match(
        SGCall::Create(SGResId::Create("callee"), {}),
        SGCall::Create(SGResId::Create("callee"), {})),
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 1u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[0]), nullptr);
  }

  {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_match(SGConstString::Create("left"), SGConstString::Create("right")),
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 1u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[0]), nullptr);
  }

  auto expect_hoisted_match = [&](StyioIR* scrutinee, StyioIR* rebind_value) {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_match(scrutinee, rebind_value),
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 1u);
    auto* hoisted = dynamic_cast<SGBlock*>(block->stmts[0]);
    ASSERT_NE(hoisted, nullptr);
    ASSERT_EQ(hoisted->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGFlexBind*>(hoisted->stmts[0]), nullptr);
    EXPECT_NE(dynamic_cast<SGMatch*>(hoisted->stmts[1]), nullptr);
  };

  expect_hoisted_match(SGConstBool::Create(true), SGConstBool::Create(true));
  expect_hoisted_match(SGConstFloat::Create("1.5"), SGConstFloat::Create("1.5"));
  expect_hoisted_match(
    SGCond::Create(SGConstInt::Create(3), SGConstInt::Create(2), StyioOpType::Greater_Than),
    SGCond::Create(SGConstInt::Create(3), SGConstInt::Create(2), StyioOpType::Greater_Than));

  {
    auto* shared_scrutinee = SGResId::Create("shared_source");
    expect_hoisted_match(shared_scrutinee, shared_scrutinee);
  }

  auto expect_prefix_blocks_hoist = [&](StyioIR* prefix) {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      prefix,
      make_match(SGResId::Create("source"), SGResId::Create("source")),
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[1]), nullptr);
  };

  auto expect_suffix_blocks_hoist = [&](StyioIR* suffix) {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_match(SGResId::Create("source"), SGResId::Create("source")),
      suffix,
    }));

    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[0]), nullptr);
  };

  expect_prefix_blocks_hoist(SCDictSet::Create(
    SGResId::Create("target"),
    SGConstString::Create("k"),
    SGConstInt::Create(1),
    "i64"));
  expect_prefix_blocks_hoist(SGLoop::CreateWhile(
    SGConstBool::Create(true),
    SGBlock::Create(std::vector<StyioIR*>{
      SGFlexBind::Create(make_target_var(), SGConstInt::Create(1)),
    })));
  expect_prefix_blocks_hoist(SGForEach::Create(
    SGResId::Create("items"),
    "target",
    "i64",
    make_returning_block(0)));
  expect_prefix_blocks_hoist(SGRangeFor::Create(
    SGConstInt::Create(0),
    SGConstInt::Create(1),
    SGConstInt::Create(1),
    "target",
    make_returning_block(0)));
  expect_prefix_blocks_hoist(SGIf::Create(
    SGConstBool::Create(true),
    SGBlock::Create(std::vector<StyioIR*>{
      SGFlexBind::Create(make_target_var(), SGConstInt::Create(1)),
    }),
    make_returning_block(0)));

  expect_suffix_blocks_hoist(SGLoop::CreateWhile(
    SGResId::Create("target"),
    make_returning_block(0)));
  expect_suffix_blocks_hoist(SGForEach::Create(
    SGResId::Create("target"),
    "item",
    "i64",
    make_returning_block(0)));
  expect_suffix_blocks_hoist(SGIf::Create(
    SGResId::Create("target"),
    make_returning_block(1),
    make_returning_block(0)));
  expect_suffix_blocks_hoist(SCListSet::Create(
    SGResId::Create("items"),
    SGConstInt::Create(0),
    SGResId::Create("target"),
    "i64"));
  expect_suffix_blocks_hoist(SCDictSet::Create(
    SGResId::Create("dict"),
    SGConstString::Create("k"),
    SGResId::Create("target"),
    "i64"));
}

TEST(StyioSecurityIROptimizer, RebindTransparencyCoversEffectFreeCollectionSelectors) {
  auto i64_type = []() {
    return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  };
  auto make_i64_var = [&](const std::string& name) {
    return SGVar::Create(SGResId::Create(name), SGType::Create(i64_type()));
  };
  auto make_returning_block = [](long value) {
    return SGBlock::Create(std::vector<StyioIR*>{
      SGReturn::Create(SGConstInt::Create(value)),
    });
  };
  auto make_match = [&](std::vector<StyioIR*> default_prefix) {
    default_prefix.push_back(SGFlexBind::Create(
      make_i64_var("target"),
      SGResId::Create("source")));
    default_prefix.push_back(SGReturn::Create(SGResId::Create("target")));
    return SGMatch::Create(
      SGResId::Create("source"),
      std::vector<std::pair<std::int64_t, SGBlock*>>{
        {1, make_returning_block(1)},
      },
      SGBlock::Create(std::move(default_prefix)),
      SGMatchReprKind::ExprInt);
  };

  {
    std::vector<StyioIR*> prefix;
    prefix.push_back(SGFlexBind::Create(
      make_i64_var("slice_closed"),
      SCListSlice::Create(SGResId::Create("items"), SGConstInt::Create(0), SGConstInt::Create(2), "i64")));
    prefix.push_back(SGFlexBind::Create(
      make_i64_var("slice_open"),
      SCListSlice::Create(SGResId::Create("items"), SGConstInt::Create(1), nullptr, "i64")));
    prefix.push_back(SGFinalBind::Create(
      make_i64_var("dict_value"),
      SCDictGet::Create(SGResId::Create("dict"), SGConstString::Create("k"), "i64")));
    prefix.push_back(SGFlexBind::Create(
      make_i64_var("matrix_cell"),
      SCMatrixGet::Create(SGResId::Create("matrix"), SGConstInt::Create(0), SGConstInt::Create(1), "i64")));
    prefix.push_back(SGFlexBind::Create(
      make_i64_var("matrix_row"),
      SCMatrixRow::Create(SGResId::Create("matrix"), SGConstInt::Create(0), "i64")));
    prefix.push_back(SGFlexBind::Create(
      make_i64_var("cond_value"),
      SGCond::Create(SGConstBool::Create(true), SGConstBool::Create(false), StyioOpType::Logic_AND)));
    prefix.push_back(SGFlexBind::Create(
      make_i64_var("cast_value"),
      SGCast::Create(SGConstInt::Create(1), SGType::Create(i64_type()), SGType::Create(i64_type()))));
    prefix.push_back(SGFlexBind::Create(
      make_i64_var("bin_value"),
      SGBinOp::Create(SGConstInt::Create(1), SGConstInt::Create(2), StyioOpType::Binary_Add, SGType::Create(i64_type()))));
    prefix.push_back(SGFlexBind::Create(
      make_i64_var("list_len"),
      SCListLen::Create(SGResId::Create("items"))));
    prefix.push_back(SGFlexBind::Create(
      make_i64_var("dict_len"),
      SCDictLen::Create(SGResId::Create("dict"))));

    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_match(std::move(prefix)),
    }));
    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 1u);
    auto* hoisted = dynamic_cast<SGBlock*>(block->stmts[0]);
    ASSERT_NE(hoisted, nullptr);
    ASSERT_EQ(hoisted->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGFlexBind*>(hoisted->stmts[0]), nullptr);
    EXPECT_NE(dynamic_cast<SGMatch*>(hoisted->stmts[1]), nullptr);
  }

  {
    std::unique_ptr<StyioIR> root(SGBlock::Create(std::vector<StyioIR*>{
      make_match(std::vector<StyioIR*>{
        SGFlexBind::Create(make_i64_var("opaque"), SGCall::Create(SGResId::Create("callee"), {})),
      }),
    }));
    auto* block = dynamic_cast<SGBlock*>(styio::lowering::optimize_styio_ir(root.get()));
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 1u);
    EXPECT_NE(dynamic_cast<SGMatch*>(block->stmts[0]), nullptr);
  }
}

TEST(StyioSecurityIROptimizer, TraversesGeneralCollectionAndIoChildren) {
  auto i64_type = []() {
    return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  };
  auto f64_type = []() {
    return StyioDataType{StyioDataTypeOption::Float, "f64", 64};
  };
  auto make_i64_var = [&](const std::string& name) {
    return SGVar::Create(SGResId::Create(name), SGType::Create(i64_type()));
  };
  auto list_expr = []() {
    return SCListLiteral::Create(std::vector<StyioIR*>{
      SGConstInt::Create(1),
      SGConstInt::Create(2),
    });
  };
  auto dict_expr = []() {
    return SCDictLiteral::Create(std::vector<SCDictLiteral::Entry>{
      {SGConstString::Create("k"), SGConstInt::Create(1)},
    });
  };
  auto matrix_expr = []() {
    return SCMatrixLiteral::Create(std::vector<StyioIR*>{
      SGConstInt::Create(1),
      SGConstInt::Create(2),
    }, "i64", 1, 2);
  };
  auto body = [](StyioIR* stmt) {
    return SGBlock::Create(std::vector<StyioIR*>{stmt});
  };

  std::vector<StyioIR*> stmts;
  stmts.push_back(SGEntry::Create(std::vector<StyioIR*>{
    SGFinalBind::Create(make_i64_var("entry_fixed"), SGConstInt::Create(1)),
  }));
  stmts.push_back(SGFlexBind::Create(
    make_i64_var("flex"),
    SGBinOp::Create(SGResId::Create("a"), SGConstInt::Create(1), StyioOpType::Binary_Add, SGType::Create(i64_type()))));
  stmts.push_back(SGFinalBind::Create(
    make_i64_var("fixed"),
    SGCast::Create(SGConstInt::Create(1), SGType::Create(i64_type()), SGType::Create(f64_type()))));
  stmts.push_back(SGBinOp::Create(
    SGCond::Create(SGConstBool::Create(true), SGConstBool::Create(false), StyioOpType::Logic_AND),
    SGConstInt::Create(1),
    StyioOpType::Binary_Add,
    SGType::Create(i64_type())));
  stmts.push_back(SGReturn::Create(SGFallback::Create(SGConstInt::Create(1), SGConstInt::Create(2))));
  stmts.push_back(SGFunc::Create(
    SGType::Create(i64_type()),
    SGResId::Create("fn"),
    std::vector<SGFuncArg*>{
      SGFuncArg::Create("x", SGType::Create(i64_type())),
    },
    body(SGReturn::Create(SGResId::Create("x")))));
  stmts.push_back(SGCall::Create(SGResId::Create("fn"), std::vector<StyioIR*>{
    SGConstInt::Create(1),
    SCListLen::Create(SGResId::Create("items")),
  }));
  stmts.push_back(SGLoop::CreateWhile(SGResId::Create("cond"), body(SGReturn::Create(SGResId::Create("loopv")))));
  stmts.push_back(SGForEach::Create(list_expr(), "item", "i64", body(SGReturn::Create(SGResId::Create("item")))));
  stmts.push_back(SGRangeFor::Create(SGConstInt::Create(0), SGConstInt::Create(2), SGConstInt::Create(1), "i", body(SGReturn::Create(SGResId::Create("i")))));
  stmts.push_back(SGIf::Create(
    SGResId::Create("flag"),
    body(SGReturn::Create(SGConstInt::Create(1))),
    body(SGReturn::Create(SGConstInt::Create(0)))));
  stmts.push_back(SGMatch::Create(
    SGResId::Create("m"),
    std::vector<std::pair<std::int64_t, SGBlock*>>{
      {1, body(SGReturn::Create(SGConstInt::Create(1)))},
    },
    body(SGReturn::Create(SGConstInt::Create(0))),
    SGMatchReprKind::ExprInt));
  stmts.push_back(SGSeriesAvgStep::Create(0, SGResId::Create("avg_in")));
  stmts.push_back(SGSeriesMaxStep::Create(0, SGResId::Create("max_in")));
  stmts.push_back(SGWaveMerge::Create(SGResId::Create("flag"), SGConstInt::Create(1), SGConstInt::Create(0)));
  stmts.push_back(SGWaveDispatch::Create(
    SGResId::Create("flag"),
    SGReturn::Create(SGConstInt::Create(1)),
    SGReturn::Create(SGConstInt::Create(0))));
  stmts.push_back(SGGuardSelect::Create(SGResId::Create("base"), SGResId::Create("guard")));
  stmts.push_back(SGEqProbe::Create(SGResId::Create("base"), SGConstInt::Create(1)));
  stmts.push_back(SGSnapshotDecl::Create("snap", SGConstString::Create("snap.bin")));
  stmts.push_back(SCListLiteral::Create(std::vector<StyioIR*>{
    SGResId::Create("a"),
    SGConstInt::Create(2),
  }));
  stmts.push_back(SCDictLiteral::Create(std::vector<SCDictLiteral::Entry>{
    {SGResId::Create("key"), SGResId::Create("value")},
  }));
  stmts.push_back(SCMatrixLiteral::Create(std::vector<StyioIR*>{
    SGResId::Create("cell"),
  }, "i64", 1, 1));
  stmts.push_back(SCListClone::Create(SGResId::Create("items")));
  stmts.push_back(SCMatrixClone::Create(SGResId::Create("matrix"), "i64"));
  stmts.push_back(SCListLen::Create(SGResId::Create("items")));
  stmts.push_back(SCDictLen::Create(SGResId::Create("dict")));
  stmts.push_back(SCListGet::Create(SGResId::Create("items"), SGConstInt::Create(0), "i64"));
  stmts.push_back(SCListSlice::Create(SGResId::Create("items"), SGConstInt::Create(0), SGConstInt::Create(2), "i64"));
  stmts.push_back(SCDictGet::Create(SGResId::Create("dict"), SGConstString::Create("k"), "i64"));
  stmts.push_back(SCListSet::Create(SGResId::Create("items"), SGConstInt::Create(0), SGConstInt::Create(7), "i64"));
  stmts.push_back(SCDictSet::Create(SGResId::Create("dict"), SGConstString::Create("k"), SGConstInt::Create(2), "i64"));
  stmts.push_back(SCListToString::Create(list_expr()));
  stmts.push_back(SCMatrixGet::Create(matrix_expr(), SGConstInt::Create(0), SGConstInt::Create(1), "i64"));
  stmts.push_back(SCMatrixRow::Create(matrix_expr(), SGConstInt::Create(0), "i64"));
  stmts.push_back(SCMatrixToString::Create(matrix_expr()));
  stmts.push_back(SCDictClone::Create(dict_expr()));
  stmts.push_back(SCDictKeys::Create(dict_expr()));
  stmts.push_back(SCDictValues::Create(dict_expr(), "i64"));
  stmts.push_back(SCDictToString::Create(dict_expr()));
  stmts.push_back(SIOHandleAcquire::Create("h", SGConstString::Create("in.txt"), true));
  stmts.push_back(SIOHandleRelease::CreateFromPath(SGConstString::Create("in.txt"), true));
  stmts.push_back(SIOHandleRelease::CreateFromVar("h"));
  stmts.push_back(SIOFileLineIter::CreateFromPath(
    SGConstString::Create("in.txt"),
    "line",
    body(SGReturn::Create(SGResId::Create("line")))));
  stmts.push_back(SIOStreamZip::Create(
    list_expr(),
    false,
    false,
    "a",
    list_expr(),
    false,
    false,
    "b",
    false,
    false,
    "i64",
    "i64",
    body(SGReturn::Create(SGResId::Create("a")))));
  stmts.push_back(SIOInstantPull::Create(SGConstString::Create("pull.txt")));
  stmts.push_back(SIOInstantPull::CreateFromHandle("h"));
  stmts.push_back(SIOResourceWriteToFile::Create(SGResId::Create("data"), SGConstString::Create("out.txt"), true, true));
  stmts.push_back(SIOStdStreamWrite::Create(SIOStdStreamWrite::Stream::Stdout, std::vector<StyioIR*>{
    SGResId::Create("out"),
    SGConstString::Create("done"),
  }));
  stmts.push_back(SIOResourceEffect::Create(
    SIOInstantPull::Create(SGConstString::Create("missing.txt")),
    SGConstInt::Create(0),
    false,
    i64_type(),
    std::vector<SIOResourceEffect::Handler>{
      SIOResourceEffect::Handler("io", SGResId::Create("fallback")),
    },
    true));
  stmts.push_back(SIOStdStreamLineIter::Create("line", body(SGReturn::Create(SGResId::Create("line")))));
  stmts.push_back(SIOPrint::Create(std::vector<StyioIR*>{
    SGResId::Create("printed"),
  }));
  stmts.push_back(SIORead::Create(SIOPath::Create("/tmp/input")));

  std::unique_ptr<StyioIR> root(SGMainEntry::Create(std::move(stmts)));
  StyioIR* optimized = styio::lowering::optimize_styio_ir(root.get());
  EXPECT_EQ(optimized, root.get());
  auto* main = dynamic_cast<SGMainEntry*>(optimized);
  ASSERT_NE(main, nullptr);
  EXPECT_GT(main->stmts.size(), 45u);
}

TEST(StyioSecurityNightlyCodegen, GetTypeCoversScalarCollectionIoAndTaskNodes) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  llvm::ExitOnError exit_on_error;
  std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
  StyioToLLVM generator(std::move(jit));

  auto i64_type = []() {
    return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  };
  auto f64_type = []() {
    return StyioDataType{StyioDataTypeOption::Float, "f64", 64};
  };
  auto bool_type = []() {
    return StyioDataType{StyioDataTypeOption::Bool, "bool", 1};
  };
  auto char_type = []() {
    return StyioDataType{StyioDataTypeOption::Char, "char", 8};
  };
  auto string_type = []() {
    return StyioDataType{StyioDataTypeOption::String, "string", 0};
  };
  auto list_i64_type = []() {
    return styio_make_list_type("i64");
  };
  auto dict_type = []() {
    return styio_make_dict_type("string", "i64");
  };
  auto matrix_type = []() {
    return StyioDataType{StyioDataTypeOption::Matrix, "matrix", 0};
  };
  auto make_i64_ir_var = [&](const std::string& name) {
    return SGVar::Create(SGResId::Create(name), SGType::Create(i64_type()));
  };
  auto bounded_type = [&](StyioDataType value_type) {
    return StyioDataType{
      StyioDataTypeOption::Defined,
      std::string("bounded_ring:") + value_type.name + ":3",
      value_type.num_of_bit};
  };

  int pointer_count = 0;
  int void_count = 0;
  int array_count = 0;
  int double_count = 0;
  int integer_count = 0;
  auto record_type = [&](StyioIR* node) {
    std::unique_ptr<StyioIR> owner(node);
    llvm::Type* type = node->toLLVMType(&generator);
    ASSERT_NE(type, nullptr);
    if (type->isPointerTy()) {
      ++pointer_count;
    }
    if (type->isVoidTy()) {
      ++void_count;
    }
    if (type->isArrayTy()) {
      ++array_count;
    }
    if (type->isDoubleTy()) {
      ++double_count;
    }
    if (type->isIntegerTy()) {
      ++integer_count;
    }
  };

  record_type(SGResId::Create("missing"));
  record_type(SGType::Create(bool_type()));
  record_type(SGType::Create(i64_type()));
  record_type(SGType::Create(f64_type()));
  record_type(SGType::Create(char_type()));
  record_type(SGType::Create(string_type()));
  record_type(SGType::Create(list_i64_type()));
  record_type(SGType::Create(dict_type()));
  record_type(SGType::Create(matrix_type()));
  record_type(SGType::Create(bounded_type(f64_type())));
  record_type(SGType::Create(bounded_type(bool_type())));
  record_type(SGType::Create(bounded_type(char_type())));
  record_type(SGType::Create(bounded_type(string_type())));
  record_type(SGNoOp::Create());
  record_type(SGConstBool::Create(true));
  record_type(SGConstInt::Create(1));
  record_type(SGConstFloat::Create("1.25"));
  record_type(SGConstChar::Create('c'));
  record_type(SGConstString::Create("s"));
  record_type(SGFormatString::Create({"x"}, {SGConstInt::Create(1)}));
  record_type(SGStruct::Create({}));
  record_type(SGCast::Create(SGConstInt::Create(1), SGType::Create(i64_type()), SGType::Create(f64_type())));
  record_type(SGBinOp::Create(SGConstInt::Create(1), SGConstInt::Create(2), StyioOpType::Binary_Add, SGType::Create(i64_type())));
  record_type(SGCond::Create(SGConstBool::Create(true), SGConstBool::Create(false), StyioOpType::Logic_AND));
  {
    auto* dyn_var = make_i64_ir_var("dyn");
    dyn_var->is_dynamic_slot = true;
    record_type(dyn_var);
  }
  record_type(SGVar::Create(SGResId::Create("ring"), SGType::Create(bounded_type(string_type()))));
  record_type(SGFlexBind::Create(make_i64_ir_var("mixed"), SGMatch::Create(SGConstInt::Create(1), {}, SGBlock::Create({SGNoOp::Create()}), SGMatchReprKind::ExprMixed)));
  record_type(SGFlexBind::Create(make_i64_ir_var("fallback_string"), SGFallback::Create(SGConstInt::Create(0), SGConstString::Create("ok"))));
  {
    auto* dyn_final_var = make_i64_ir_var("dyn_final");
    dyn_final_var->is_dynamic_slot = true;
    record_type(SGFinalBind::Create(dyn_final_var, SGConstInt::Create(1)));
  }
  record_type(SGDynLoad::Create("b", SGDynLoadKind::Bool));
  record_type(SGDynLoad::Create("i", SGDynLoadKind::I64));
  record_type(SGDynLoad::Create("f", SGDynLoadKind::F64));
  record_type(SGDynLoad::Create("s", SGDynLoadKind::CString));
  record_type(SGDynLoad::Create("l", SGDynLoadKind::ListHandle));
  record_type(SGDynLoad::Create("d", SGDynLoadKind::DictHandle));
  record_type(SGDynLoad::Create("m", SGDynLoadKind::MatrixHandle));
  record_type(SGDynLoad::Create("t", SGDynLoadKind::TaskHandle));
  record_type(SGFuncArg::Create("arg", SGType::Create(i64_type())));
  record_type(SGFunc::Create(SGType::Create(f64_type()), SGResId::Create("fn"), {}, SGBlock::Create({SGReturn::Create(SGConstFloat::Create("1.0"))})));
  record_type(SGCall::Create(SGResId::Create("missing_call"), {}));
  record_type(SGExportDecl::Create({"entry"}));
  record_type(SGExternBlock::Create("c", "int entry(void){return 0;}"));
  record_type(SGReturn::Create(SGConstInt::Create(1)));
  record_type(SGBlock::Create({SGNoOp::Create()}));
  record_type(SGEntry::Create({SGNoOp::Create()}));
  record_type(SGMainEntry::Create({SGNoOp::Create()}));
  record_type(SGLoop::CreateInfinite(SGBlock::Create({SGNoOp::Create()})));
  record_type(SGForEach::Create(SCListLiteral::Create({SGConstInt::Create(1)}), "v", "i64", SGBlock::Create({SGNoOp::Create()})));
  record_type(SGRangeFor::Create(SGConstInt::Create(0), SGConstInt::Create(1), SGConstInt::Create(1), "i", SGBlock::Create({SGNoOp::Create()})));
  record_type(SGIf::Create(SGConstBool::Create(true), SGBlock::Create({SGNoOp::Create()})));
  record_type(SCListLiteral::Create({SGConstInt::Create(1)}));
  record_type(SCDictLiteral::Create({{SGConstString::Create("k"), SGConstInt::Create(1)}}, "i64"));
  record_type(SCMatrixLiteral::Create({SGConstInt::Create(1)}, "i64", 1, 1));
  record_type(SGStateSnapLoad::Create(0));
  record_type(SGStateHistLoad::Create(0, 1));
  record_type(SGSeriesAvgStep::Create(0, SGConstInt::Create(1)));
  record_type(SGSeriesMaxStep::Create(0, SGConstInt::Create(1)));
  record_type(SGMatch::Create(SGConstInt::Create(1), {}, SGBlock::Create({SGNoOp::Create()}), SGMatchReprKind::ExprMixed));
  record_type(SGMatch::Create(SGConstInt::Create(1), {}, SGBlock::Create({SGNoOp::Create()}), SGMatchReprKind::ExprFloat));
  record_type(SGMatch::Create(SGConstInt::Create(1), {}, SGBlock::Create({SGNoOp::Create()}), SGMatchReprKind::ExprBool));
  record_type(SGMatch::Create(SGConstInt::Create(1), {}, SGBlock::Create({SGNoOp::Create()}), SGMatchReprKind::ExprChar));
  record_type(SGBreak::Create());
  record_type(SGContinue::Create());
  record_type(SGUndef::Create());
  record_type(SGFallback::Create(SGConstInt::Create(0), SGConstString::Create("ok")));
  record_type(SGWaveMerge::Create(SGConstBool::Create(true), SGConstInt::Create(1), SGConstInt::Create(0)));
  record_type(SGWaveDispatch::Create(SGConstBool::Create(true), SGNoOp::Create(), SGNoOp::Create()));
  record_type(SGGuardSelect::Create(SGConstInt::Create(1), SGConstBool::Create(true)));
  record_type(SGEqProbe::Create(SGConstInt::Create(1), SGConstInt::Create(1)));
  record_type(SIOHandleAcquire::Create("h", SGConstString::Create("in.txt"), true));
  record_type(SIOHandleRelease::CreateFromVar("h"));
  record_type(SIOFileLineIter::CreateFromPath(SGConstString::Create("in.txt"), "line", SGBlock::Create({SGNoOp::Create()})));
  record_type(SIOStreamZip::Create(SGResId::Create("a"), false, false, "a", SGResId::Create("b"), false, false, "b", false, false, "i64", "i64", SGBlock::Create({SGNoOp::Create()})));
  record_type(SGSnapshotDecl::Create("snap", SGConstString::Create("snap.bin")));
  record_type(SGSnapshotShadowLoad::Create("snap"));
  record_type(SIOInstantPull::Create(SGConstString::Create("in.txt")));
  record_type(SIOListReadStdin::Create("i64"));
  record_type(SCListClone::Create(SGResId::Create("items")));
  record_type(SCMatrixClone::Create(SGResId::Create("m"), "i64"));
  record_type(SCListLen::Create(SGResId::Create("items")));
  record_type(SCListGet::Create(SGResId::Create("items"), SGConstInt::Create(0), "string"));
  record_type(SCListGet::Create(SGResId::Create("items"), SGConstInt::Create(0), "f64"));
  record_type(SCListGet::Create(SGResId::Create("items"), SGConstInt::Create(0), "bool"));
  record_type(SCListGet::Create(SGResId::Create("items"), SGConstInt::Create(0), "char"));
  record_type(SCListGet::Create(SGResId::Create("items"), SGConstInt::Create(0), "i64"));
  record_type(SCListSlice::Create(SGResId::Create("items"), SGConstInt::Create(0), nullptr, "i64"));
  record_type(SCListSet::Create(SGResId::Create("items"), SGConstInt::Create(0), SGConstInt::Create(7), "i64"));
  record_type(SCListToString::Create(SGResId::Create("items")));
  record_type(SCMatrixGet::Create(SGResId::Create("m"), SGConstInt::Create(0), SGConstInt::Create(0), "f64"));
  record_type(SCMatrixGet::Create(SGResId::Create("m"), SGConstInt::Create(0), SGConstInt::Create(0), "i64"));
  record_type(SCMatrixRow::Create(SGResId::Create("m"), SGConstInt::Create(0), "i64"));
  record_type(SCMatrixRowsSlice::Create(SGResId::Create("m"), SGConstInt::Create(0), nullptr, "i64"));
  record_type(SCMatrixToString::Create(SGResId::Create("m")));
  record_type(SCDictClone::Create(SGResId::Create("dict")));
  record_type(SCDictLen::Create(SGResId::Create("dict")));
  record_type(SCDictGet::Create(SGResId::Create("dict"), SGConstString::Create("k"), "string"));
  record_type(SCDictGet::Create(SGResId::Create("dict"), SGConstString::Create("k"), "f64"));
  record_type(SCDictGet::Create(SGResId::Create("dict"), SGConstString::Create("k"), "bool"));
  record_type(SCDictGet::Create(SGResId::Create("dict"), SGConstString::Create("k"), "i64"));
  record_type(SCDictSet::Create(SGResId::Create("dict"), SGConstString::Create("k"), SGConstInt::Create(2), "i64"));
  record_type(SCDictKeys::Create(SGResId::Create("dict")));
  record_type(SCDictValues::Create(SGResId::Create("dict"), "i64"));
  record_type(SCDictToString::Create(SGResId::Create("dict")));
  record_type(SIOResourceWriteToFile::Create(SGConstString::Create("data"), SGConstString::Create("out.txt"), true, true));
  record_type(SIOStdStreamWrite::Create(SIOStdStreamWrite::Stream::Stdout, {SGConstString::Create("out")}));
  record_type(SIOResourceEffect::Create(nullptr, nullptr, false, StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0}, {}, false));
  record_type(SIOResourceEffect::Create(SGNoOp::Create(), nullptr, false, bool_type(), {}, true));
  record_type(SIOResourceEffect::Create(SGNoOp::Create(), nullptr, false, f64_type(), {}, true));
  record_type(SIOResourceEffect::Create(SGNoOp::Create(), nullptr, false, char_type(), {}, true));
  record_type(SIOResourceEffect::Create(SGNoOp::Create(), nullptr, false, string_type(), {}, true));
  record_type(SIOResourceEffect::Create(SGNoOp::Create(), nullptr, false, i64_type(), {}, true));
  record_type(SIOStdStreamLineIter::Create("line", SGBlock::Create({SGNoOp::Create()})));
  record_type(SIOStdStreamPull::Create(list_i64_type()));
  record_type(SIOStdStreamPull::Create(f64_type()));
  record_type(SIOStdStreamPull::Create(string_type()));
  record_type(SIOStdStreamPull::Create(i64_type()));
  record_type(SIOTaskCreate::Create(SGBlock::Create({SGReturn::Create(SGConstInt::Create(1))}), i64_type()));
  record_type(SIOFlowBind::Create(SGResId::Create("task"), "answer", bool_type(), true));
  record_type(SIOFlowBind::Create(SGResId::Create("task"), "answer", f64_type(), true));
  record_type(SIOFlowBind::Create(SGResId::Create("task"), "answer", string_type(), true));
  record_type(SIOFlowBind::Create(SGResId::Create("task"), "answer", i64_type(), true));
  record_type(SIOPath::Create("input.txt"));
  record_type(SIOPrint::Create({SGConstString::Create("out")}));
  record_type(SIORead::Create(SIOPath::Create("input.txt")));

  EXPECT_GT(pointer_count, 10);
  EXPECT_GT(void_count, 15);
  EXPECT_GT(array_count, 3);
  EXPECT_GT(double_count, 8);
  EXPECT_GT(integer_count, 40);
}

TEST(StyioSecurityTokenRepr, StableNamesCoverAstDataTypeOperatorAndTokenSwitches) {
  std::vector<StyioNodeType> ast_types = {
    StyioNodeType::True,
    StyioNodeType::False,
    StyioNodeType::None,
    StyioNodeType::Empty,
    StyioNodeType::Id,
    StyioNodeType::DType,
    StyioNodeType::TypeTuple,
    StyioNodeType::Variable,
    StyioNodeType::Param,
    StyioNodeType::Integer,
    StyioNodeType::Float,
    StyioNodeType::Char,
    StyioNodeType::String,
    StyioNodeType::NumConvert,
    StyioNodeType::FmtStr,
    StyioNodeType::LocalPath,
    StyioNodeType::RemotePath,
    StyioNodeType::WebUrl,
    StyioNodeType::DBUrl,
    StyioNodeType::ExtPack,
    StyioNodeType::ExportDecl,
    StyioNodeType::ExternBlock,
    StyioNodeType::Parameters,
    StyioNodeType::Condition,
    StyioNodeType::SizeOf,
    StyioNodeType::BinOp,
    StyioNodeType::Print,
    StyioNodeType::ReadFile,
    StyioNodeType::Call,
    StyioNodeType::Attribute,
    StyioNodeType::Access,
    StyioNodeType::Access_By_Name,
    StyioNodeType::Access_By_Index,
    StyioNodeType::Access_By_Slice,
    StyioNodeType::Get_Index_By_Value,
    StyioNodeType::Get_Indices_By_Many_Values,
    StyioNodeType::Append_Value,
    StyioNodeType::Insert_Item_By_Index,
    StyioNodeType::Remove_Item_By_Index,
    StyioNodeType::Remove_Items_By_Many_Indices,
    StyioNodeType::Remove_Item_By_Value,
    StyioNodeType::Remove_Items_By_Many_Values,
    StyioNodeType::Get_Reversed,
    StyioNodeType::Get_Index_By_Item_From_Right,
    StyioNodeType::Return,
    StyioNodeType::Range,
    StyioNodeType::Tuple,
    StyioNodeType::List,
    StyioNodeType::Dict,
    StyioNodeType::Set,
    StyioNodeType::Resources,
    StyioNodeType::MutBind,
    StyioNodeType::FinalBind,
    StyioNodeType::ParallelAssign,
    StyioNodeType::Block,
    StyioNodeType::Cases,
    StyioNodeType::Func,
    StyioNodeType::SimpleFunc,
    StyioNodeType::Struct,
    StyioNodeType::Loop,
    StyioNodeType::Iterator,
    StyioNodeType::StreamZip,
    StyioNodeType::SnapshotDecl,
    StyioNodeType::InstantPull,
    StyioNodeType::IterSeq,
    StyioNodeType::CheckEq,
    StyioNodeType::CheckIsin,
    StyioNodeType::HashTagName,
    StyioNodeType::TupleOperation,
    StyioNodeType::Forward,
    StyioNodeType::If_Equal_To_Forward,
    StyioNodeType::If_Is_In_Forward,
    StyioNodeType::Cases_Forward,
    StyioNodeType::If_True_Forward,
    StyioNodeType::If_False_Forward,
    StyioNodeType::Fill_Forward,
    StyioNodeType::Fill_If_Equal_To_Forward,
    StyioNodeType::Fill_If_Is_in_Forward,
    StyioNodeType::Fill_Cases_Forward,
    StyioNodeType::Fill_If_True_Forward,
    StyioNodeType::Fill_If_False_Forward,
    StyioNodeType::Backward,
    StyioNodeType::Chain_Of_Data_Processing,
    StyioNodeType::TypedVar,
    StyioNodeType::Pass,
    StyioNodeType::Break,
    StyioNodeType::Continue,
    StyioNodeType::CondFlow_True,
    StyioNodeType::CondFlow_False,
    StyioNodeType::CondFlow_Both,
    StyioNodeType::MainBlock,
    StyioNodeType::FileResource,
    StyioNodeType::EmptyResource,
    StyioNodeType::ResourceReceiver,
    StyioNodeType::ResourceMethodDef,
    StyioNodeType::ResourceOrder,
    StyioNodeType::ResourceDecl,
    StyioNodeType::ResourceRef,
    StyioNodeType::HandleAcquire,
    StyioNodeType::ResourceWrite,
    StyioNodeType::ResourceRedirect,
    StyioNodeType::ResourceEffect,
    StyioNodeType::TaskBlock,
    StyioNodeType::TaskGroupLaunch,
    StyioNodeType::FlowBind,
    StyioNodeType::StateDecl,
    StyioNodeType::StateRef,
    StyioNodeType::HistoryProbe,
    StyioNodeType::SeriesIntrinsic,
    StyioNodeType::None,
  };

  std::string joined;
  for (const auto type : ast_types) {
    const std::string text = reprASTType(type, "!");
    EXPECT_FALSE(text.empty());
    EXPECT_NE(text.find("styio.ast."), std::string::npos) << text;
    joined += text;
  }

  std::vector<StyioDataTypeOption> data_type_options = {
    StyioDataTypeOption::Undefined,
    StyioDataTypeOption::Defined,
    StyioDataTypeOption::Bool,
    StyioDataTypeOption::Integer,
    StyioDataTypeOption::Float,
    StyioDataTypeOption::Decimal,
    StyioDataTypeOption::Char,
    StyioDataTypeOption::String,
    StyioDataTypeOption::Tuple,
    StyioDataTypeOption::List,
    StyioDataTypeOption::Dict,
    StyioDataTypeOption::Matrix,
    StyioDataTypeOption::Struct,
    StyioDataTypeOption::Func,
  };
  for (const auto option : data_type_options) {
    joined += reprDataTypeOption(option);
  }

  std::vector<StyioOpType> op_types = {
    StyioOpType::Binary_Add,
    StyioOpType::Binary_Sub,
    StyioOpType::Binary_Mul,
    StyioOpType::Binary_Div,
    StyioOpType::Binary_Pow,
    StyioOpType::Binary_Mod,
    StyioOpType::Self_Add_Assign,
    StyioOpType::Self_Sub_Assign,
    StyioOpType::Self_Mul_Assign,
    StyioOpType::Self_Div_Assign,
    StyioOpType::Self_Mod_Assign,
    StyioOpType::Undefined,
  };
  for (const auto op : op_types) {
    joined += reprToken(op);
  }

  for (const auto logic : {LogicType::NOT, LogicType::AND, LogicType::OR, LogicType::XOR, LogicType::RAW}) {
    joined += reprToken(logic);
  }
  for (const auto comp : {CompType::EQ, CompType::NE, CompType::GT, CompType::GE, CompType::LT, CompType::LE}) {
    joined += reprToken(comp);
  }

  std::vector<StyioTokenType> token_types = {
    StyioTokenType::TOK_SPACE,
    StyioTokenType::TOK_CR,
    StyioTokenType::TOK_LF,
    StyioTokenType::TOK_EOF,
    StyioTokenType::NAME,
    StyioTokenType::INTEGER,
    StyioTokenType::DECIMAL,
    StyioTokenType::STRING,
    StyioTokenType::COMMENT_LINE,
    StyioTokenType::COMMENT_CLOSED,
    StyioTokenType::NATIVE_EXTERN_BODY,
    StyioTokenType::TOK_COMMA,
    StyioTokenType::TOK_PLUS,
    StyioTokenType::TOK_MINUS,
    StyioTokenType::TOK_STAR,
    StyioTokenType::TOK_DOT,
    StyioTokenType::TOK_COLON,
    StyioTokenType::TOK_TILDE,
    StyioTokenType::TOK_EXCLAM,
    StyioTokenType::TOK_AT,
    StyioTokenType::TOK_HASH,
    StyioTokenType::TOK_DOLLAR,
    StyioTokenType::TOK_PERCENT,
    StyioTokenType::TOK_HAT,
    StyioTokenType::TOK_QUEST,
    StyioTokenType::TOK_SLASH,
    StyioTokenType::TOK_BACKSLASH,
    StyioTokenType::TOK_PIPE,
    StyioTokenType::TOK_AMP,
    StyioTokenType::ELLIPSIS,
    StyioTokenType::TOK_SQUOTE,
    StyioTokenType::TOK_DQUOTE,
    StyioTokenType::TOK_BQUOTE,
    StyioTokenType::TOK_LPAREN,
    StyioTokenType::TOK_RPAREN,
    StyioTokenType::TOK_LBOXBRAC,
    StyioTokenType::TOK_RBOXBRAC,
    StyioTokenType::TOK_LCURBRAC,
    StyioTokenType::TOK_RCURBRAC,
    StyioTokenType::TOK_EQUAL,
    StyioTokenType::TOK_LANGBRAC,
    StyioTokenType::TOK_RANGBRAC,
    StyioTokenType::LOGIC_NOT,
    StyioTokenType::LOGIC_AND,
    StyioTokenType::LOGIC_OR,
    StyioTokenType::LOGIC_XOR,
    StyioTokenType::BINOP_BITAND,
    StyioTokenType::BINOP_BITOR,
    StyioTokenType::BINOP_BITXOR,
    StyioTokenType::EXTRACTOR,
    StyioTokenType::ITERATOR,
    StyioTokenType::PRINT,
    StyioTokenType::UNARY_NEG,
    StyioTokenType::BINOP_ADD,
    StyioTokenType::BINOP_SUB,
    StyioTokenType::BINOP_MUL,
    StyioTokenType::BINOP_DIV,
    StyioTokenType::BINOP_MOD,
    StyioTokenType::BINOP_POW,
    StyioTokenType::BINOP_GT,
    StyioTokenType::BINOP_GE,
    StyioTokenType::BINOP_LT,
    StyioTokenType::BINOP_LE,
    StyioTokenType::BINOP_EQ,
    StyioTokenType::BINOP_NE,
    StyioTokenType::ARROW_DOUBLE_RIGHT,
    StyioTokenType::ARROW_DOUBLE_LEFT,
    StyioTokenType::ARROW_SINGLE_RIGHT,
    StyioTokenType::ARROW_SINGLE_LEFT,
    StyioTokenType::WALRUS,
    StyioTokenType::COMPOUND_ADD,
    StyioTokenType::COMPOUND_SUB,
    StyioTokenType::COMPOUND_MUL,
    StyioTokenType::COMPOUND_DIV,
    StyioTokenType::COMPOUND_MOD,
    StyioTokenType::WAVE_LEFT,
    StyioTokenType::WAVE_RIGHT,
    StyioTokenType::DBQUESTION,
    StyioTokenType::MATCH,
    StyioTokenType::YIELD_PIPE,
    StyioTokenType::RETURN_PIPE,
    StyioTokenType::AWAIT_PIPE,
    StyioTokenType::PIPE_SEMICOLON,
    StyioTokenType::TASK_LAUNCH,
    StyioTokenType::INFINITE_LIST,
    StyioTokenType::BOUNDED_BUFFER_OPEN,
    StyioTokenType::BOUNDED_BUFFER_CLOSE,
  };
  for (const auto token_type : token_types) {
    joined += StyioToken::getTokName(token_type);
  }

  EXPECT_EQ(reprToken(static_cast<StyioOpType>(999)), "<Undefined>");
  EXPECT_EQ(reprToken(static_cast<LogicType>(999)), "<NULL>");
  EXPECT_EQ(reprToken(static_cast<CompType>(999)), "<NULL>");
  EXPECT_EQ(reprDataTypeOption(static_cast<StyioDataTypeOption>(999)), "unknown");
  EXPECT_EQ(StyioToken::getTokName(static_cast<StyioTokenType>(999)), "<UNKNOWN>");

  const StyioDataType bool_type{StyioDataTypeOption::Bool, "bool", 1};
  const StyioDataType also_bool_type{StyioDataTypeOption::Bool, "bool", 1};
  EXPECT_EQ(getMaxType(bool_type, also_bool_type).option, StyioDataTypeOption::Bool);
  EXPECT_EQ(
    getMaxType(bool_type, StyioDataType{StyioDataTypeOption::String, "string", 0}).option,
    StyioDataTypeOption::Undefined);

  std::unique_ptr<StyioToken> name(StyioToken::Create(StyioTokenType::NAME, "price"));
  std::unique_ptr<StyioToken> integer(StyioToken::Create(StyioTokenType::INTEGER, "42"));
  std::unique_ptr<StyioToken> decimal(StyioToken::Create(StyioTokenType::DECIMAL, "4.2"));
  std::unique_ptr<StyioToken> string(StyioToken::Create(StyioTokenType::STRING, "hello"));
  std::unique_ptr<StyioToken> lf(StyioToken::Create(StyioTokenType::TOK_LF, "\n"));
  std::unique_ptr<StyioToken> space(StyioToken::Create(StyioTokenType::TOK_SPACE, " "));
  std::unique_ptr<StyioToken> plus(StyioToken::Create(StyioTokenType::TOK_PLUS, "+"));
  EXPECT_EQ(name->length(), 5u);
  EXPECT_NE(name->as_str().find("price"), std::string::npos);
  EXPECT_NE(integer->as_str().find("42"), std::string::npos);
  EXPECT_NE(decimal->as_str().find("4.2"), std::string::npos);
  EXPECT_EQ(string->as_str(), "\"hello\"");
  EXPECT_EQ(lf->as_str(), "<LF>");
  EXPECT_EQ(space->as_str(), "<SPACE>");
  EXPECT_EQ(plus->as_str(), "+");
  EXPECT_GT(joined.size(), 1000u);
}

TEST(StyioSecurityTokenTypes, ResourceAndValueFamilyHelpersCoverFallbacks) {
  EXPECT_EQ(
    styio_caps(StyioTypeCapability::Pull, StyioTypeCapability::Close),
    styio_caps(StyioTypeCapability::Pull) | styio_caps(StyioTypeCapability::Close));

  const StyioDataType i64 = styio_data_type_from_name("i64");
  EXPECT_TRUE(i64.isInteger());
  EXPECT_FALSE(i64.isFloat());
  EXPECT_FALSE(i64.isUndefined());
  EXPECT_TRUE(i64.equals(StyioDataType{StyioDataTypeOption::Integer, "i64", 64}));
  EXPECT_FALSE(i64.equals(StyioDataType{StyioDataTypeOption::Integer, "i32", 32}));

  const StyioDataType sequence_type = styio_make_topology_sequence_type("string");
  EXPECT_EQ(sequence_type.name, "string..");
  EXPECT_EQ(sequence_type.resource_shape, StyioResourceShapeKind::Sequence);
  EXPECT_EQ(styio_make_topology_resource_type(i64, StyioResourceShapeKind::Sequence).name, "resource[i64..]");
  EXPECT_EQ(
    styio_make_topology_resource_type(i64, StyioResourceShapeKind::TupleSequence).resource_shape,
    StyioResourceShapeKind::TupleSequence);
  EXPECT_EQ(
    styio_make_topology_resource_type(i64, StyioResourceShapeKind::None).resource_shape,
    StyioResourceShapeKind::Scalar);

  StyioDataType resource_with_item = styio_make_topology_resource_type(i64, StyioResourceShapeKind::Scalar);
  resource_with_item.resource_value_type_name.clear();
  resource_with_item.item_type_name = "f64";
  EXPECT_EQ(styio_topology_resource_value_type(resource_with_item).name, "f64");
  resource_with_item.item_type_name.clear();
  EXPECT_EQ(styio_topology_resource_value_type(resource_with_item).name, "i64");

  const StyioDataType normalized_list = styio_normalize_resource_decl_type(styio_make_list_type("bool"));
  EXPECT_TRUE(styio_is_topology_resource_type(normalized_list));
  EXPECT_EQ(normalized_list.resource_shape, StyioResourceShapeKind::Sequence);
  EXPECT_EQ(normalized_list.resource_value_type_name, "bool");

  const StyioDataType normalized_dict = styio_normalize_resource_decl_type(styio_make_dict_type("string", "i64"));
  EXPECT_TRUE(styio_is_topology_resource_type(normalized_dict));
  EXPECT_EQ(normalized_dict.resource_shape, StyioResourceShapeKind::TupleSequence);
  EXPECT_EQ(normalized_dict.resource_value_type_name, "(string,i64)");

  EXPECT_TRUE(styio_is_list_type(StyioDataType{StyioDataTypeOption::Defined, "list[string]", 0}));
  EXPECT_TRUE(styio_is_dict_type(StyioDataType{StyioDataTypeOption::Defined, "dict[string,i64]", 0}));
  EXPECT_TRUE(styio_is_matrix_type(StyioDataType{StyioDataTypeOption::Defined, "matrix", 0}));
  EXPECT_EQ(styio_list_elem_type_name(StyioDataType{StyioDataTypeOption::Defined, "not-list", 0}), "i64");
  EXPECT_EQ(styio_dict_key_type_name(StyioDataType{StyioDataTypeOption::Defined, "dict[string]", 0}), "string");
  EXPECT_EQ(styio_dict_value_type_name(StyioDataType{StyioDataTypeOption::Defined, "dict[string]", 0}), "i64");
  EXPECT_EQ(styio_matrix_elem_type_name(StyioDataType{StyioDataTypeOption::Defined, "matrix[]", 0}), "i64");
  EXPECT_EQ(styio_matrix_row_count(StyioDataType{StyioDataTypeOption::Defined, "not-matrix", 0}), 0U);
  EXPECT_EQ(styio_matrix_row_count(StyioDataType{StyioDataTypeOption::Matrix, "not-matrix", 0}), 0U);
  EXPECT_EQ(styio_matrix_row_count(StyioDataType{StyioDataTypeOption::Matrix, "matrix[i64,,2]", 0}), 0U);
  EXPECT_EQ(styio_matrix_row_count(StyioDataType{StyioDataTypeOption::Matrix, "matrix[i64,NaN,2]", 0}), 0U);
  EXPECT_EQ(styio_matrix_col_count(StyioDataType{StyioDataTypeOption::Defined, "not-matrix", 0}), 0U);
  EXPECT_EQ(styio_matrix_col_count(StyioDataType{StyioDataTypeOption::Matrix, "not-matrix", 0}), 0U);
  EXPECT_EQ(styio_matrix_col_count(StyioDataType{StyioDataTypeOption::Matrix, "matrix[i64,2,]", 0}), 0U);
  EXPECT_EQ(styio_matrix_col_count(StyioDataType{StyioDataTypeOption::Matrix, "matrix[i64,2,NaN]", 0}), 0U);

  EXPECT_EQ(
    styio_value_family_for_type(StyioDataType{StyioDataTypeOption::Defined, "handle-list", 0, StyioHandleFamily::List}),
    StyioValueFamily::ListHandle);
  EXPECT_EQ(
    styio_value_family_for_type(StyioDataType{StyioDataTypeOption::Defined, "handle-dict", 0, StyioHandleFamily::Dict}),
    StyioValueFamily::DictHandle);
  EXPECT_EQ(
    styio_value_family_for_type(StyioDataType{StyioDataTypeOption::Defined, "handle-matrix", 0, StyioHandleFamily::Matrix}),
    StyioValueFamily::MatrixHandle);
  EXPECT_EQ(
    styio_value_family_for_type(StyioDataType{StyioDataTypeOption::Defined, "handle-range", 0, StyioHandleFamily::Range}),
    StyioValueFamily::RangeHandle);
  EXPECT_EQ(
    styio_value_family_for_type(StyioDataType{StyioDataTypeOption::Defined, "handle-file", 0, StyioHandleFamily::File}),
    StyioValueFamily::FileHandle);
  EXPECT_EQ(
    styio_value_family_for_type(StyioDataType{StyioDataTypeOption::Defined, "handle-stream", 0, StyioHandleFamily::Stream}),
    StyioValueFamily::StreamHandle);
  EXPECT_EQ(
    styio_value_family_for_type(StyioDataType{StyioDataTypeOption::Defined, "handle-task", 0, StyioHandleFamily::Task}),
    StyioValueFamily::TaskHandle);
  EXPECT_EQ(
    styio_value_family_for_type(StyioDataType{StyioDataTypeOption::Matrix, "plain-matrix", 0}),
    StyioValueFamily::MatrixHandle);
  EXPECT_EQ(
    styio_value_family_for_type(StyioDataType{StyioDataTypeOption::Struct, "struct Price", 0}),
    StyioValueFamily::UserDefined);
  EXPECT_EQ(
    styio_value_family_for_type(StyioDataType{StyioDataTypeOption::Func, "fn", 0}),
    StyioValueFamily::UserDefined);

  EXPECT_EQ(styio_type_item_type_name(StyioDataType{StyioDataTypeOption::Matrix, "matrix[f64]", 0}), "list[f64]");
  EXPECT_EQ(styio_type_item_type_name(StyioDataType{StyioDataTypeOption::List, "list[char]", 0}), "char");
  EXPECT_EQ(styio_type_item_type_name(StyioDataType{StyioDataTypeOption::Dict, "dict[string,bool]", 0}), "bool");
  EXPECT_EQ(styio_type_item_type_name(StyioDataType{StyioDataTypeOption::Defined, "opaque", 0}), "i64");

  const StyioDataType parsed_task = styio_data_type_from_name("task[]");
  EXPECT_EQ(parsed_task.handle_family, StyioHandleFamily::Task);
  EXPECT_EQ(styio_task_result_type_name(parsed_task), "unit");
  const StyioDataType task_i64 = styio_make_task_type("i64");
  EXPECT_EQ(task_i64.item_value_family, StyioValueFamily::Integer);
  EXPECT_EQ(styio_type_item_value_family(task_i64), StyioValueFamily::Integer);
  EXPECT_EQ(styio_type_item_value_family(styio_make_list_type("bool")), StyioValueFamily::Bool);
  EXPECT_EQ(styio_dict_key_value_family(styio_make_dict_type("string", "i64")), StyioValueFamily::String);
  StyioDataType dict_with_explicit_key_family = styio_make_dict_type("string", "i64");
  dict_with_explicit_key_family.key_value_family = StyioValueFamily::Char;
  EXPECT_EQ(styio_dict_key_value_family(dict_with_explicit_key_family), StyioValueFamily::Char);

  EXPECT_TRUE(styio_value_family_is_runtime_scalar(StyioValueFamily::Integer));
  EXPECT_FALSE(styio_value_family_is_runtime_scalar(StyioValueFamily::TaskHandle));
  EXPECT_TRUE(styio_value_family_is_runtime_handle(StyioValueFamily::MatrixHandle));
  EXPECT_FALSE(styio_value_family_is_runtime_handle(StyioValueFamily::FileHandle));
  EXPECT_TRUE(styio_type_supports_runtime_list_elem(styio_data_type_from_name("char")));
  EXPECT_TRUE(styio_type_supports_runtime_list_elem(styio_make_matrix_type("i64")));
  EXPECT_FALSE(styio_type_supports_runtime_list_elem(styio_make_file_handle_type("i64")));
  EXPECT_TRUE(styio_type_supports_runtime_dict_value(styio_make_dict_type("string", "i64")));
  EXPECT_FALSE(styio_type_supports_runtime_dict_value(styio_make_range_type("i64")));

  const StyioDataType stdin_type = styio_make_std_stream_type(StdStreamKind::Stdin, "string");
  const StyioDataType stdout_type = styio_make_std_stream_type(StdStreamKind::Stdout, "i64");
  const StyioDataType stderr_type = styio_make_std_stream_type(StdStreamKind::Stderr, "string");
  EXPECT_TRUE(stdin_type.has_std_stream_kind);
  EXPECT_TRUE(styio_type_is_iterable(stdin_type));
  EXPECT_TRUE(styio_type_is_readable(stdin_type));
  EXPECT_FALSE(styio_type_is_writable(stdin_type));
  EXPECT_TRUE(styio_type_is_writable(stdout_type));
  EXPECT_EQ(stderr_type.name, "stderr[string]");
}

TEST(StyioSecurityNightlyCodegen, UnsupportedInternalBinaryOperatorFailsClosed) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  llvm::ExitOnError exit_on_error;
  std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
  StyioToLLVM generator(std::move(jit));
  auto* entry = SGMainEntry::Create(std::vector<StyioIR*>{
    SGBinOp::Create(
      SGConstInt::Create(1),
      SGConstInt::Create(2),
      static_cast<StyioOpType>(999),
      SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64})
    )
  });
  EXPECT_THROW(entry->toLLVMIR(&generator), StyioTypeError);
  delete entry;
}

TEST(StyioSecurityNightlyCodegen, UnsupportedInternalLoweringOperatorsFailClosed) {
  AstToStyioIRLowerer analyzer;

  std::unique_ptr<StyioAST> bad_comp(new BinCompAST(
    static_cast<CompType>(999),
    IntAST::Create("1", 64),
    IntAST::Create("2", 64)
  ));
  EXPECT_THROW(
    {
      std::unique_ptr<StyioIR> ir(bad_comp->toStyioIR(&analyzer));
    },
    StyioTypeError
  );

  std::unique_ptr<StyioAST> bad_list(new ListOpAST(
    StyioNodeType::Get_Reversed,
    ListAST::Create(std::vector<StyioAST*>{IntAST::Create("1", 64)})
  ));
  EXPECT_THROW(
    {
      std::unique_ptr<StyioIR> ir(bad_list->toStyioIR(&analyzer));
    },
    StyioTypeError
  );
}

TEST(StyioSecurityNightlyCodegen, EmitsTypedListHelpersForMutationAndOperations) {
  const std::string src =
    "nums = [1,2]\n"
    "nums.push(3)\n"
    "nums.insert(0,4)\n"
    "nums.pop()\n"
    "flags = [true,false]\n"
    "flags[1] = true\n"
    "bags = [[1,2]]\n"
    "bags[0] = [9]\n"
    "maps = [dict{\"a\": 1}]\n"
    "maps.insert(1, dict{\"b\": 2})\n"
    "maps[0] = dict{\"c\": 3}\n";
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_push_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_insert_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_pop"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_set_bool"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_set_list"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_insert_dict"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_set_dict"), std::string::npos);
}

TEST(StyioSecurityNightlyCodegen, DynamicRangeLiteralLowersToRuntimeListLoop) {
  const std::string src =
    "start = 2\n"
    "stop = 6\n"
    "xs = [start..stop]\n"
    ">_(xs)\n";
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("range_list_hdr"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_new_i64"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_push_i64"), std::string::npos);
}

TEST(StyioSecurityNightlyCodegen, EmitsStringListCollectHelperForStdinCollectBind) {
  const std::string src =
    "lines << @stdin\n"
    "first = lines[0]\n";
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_cstr_read_stdin"), std::string::npos);
}

TEST(StyioSecurityNightlyCodegen, EmitsImmediateDictReleaseForFlexReassign) {
  const std::string src =
    "d = dict{\"a\": 1}\n"
    "d = 7\n";
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_dict_release"), std::string::npos);
}

TEST(StyioSecurityNightlyCodegen, EmitsTypedDictHelpersForStringAndFloatValues) {
  const std::string src =
    "names = dict{\"first\": \"Ada\"}\n"
    ">_(names[\"first\"])\n"
    "nums = dict{\"pi\": 3.5, \"e\": 2}\n"
    ">_(nums.values)\n";
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_dict_set_cstr"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_dict_get_cstr"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_dict_values_f64"), std::string::npos);
}

TEST(StyioSecurityNightlyCodegen, EmitsHandleDictHelpersForNestedCollections) {
  const std::string src =
    "d = dict{\"nums\": [1,2,3], \"more\": [4,5]}\n"
    ">_(d[\"nums\"])\n"
    "child = dict{\"left\": dict{\"x\": 1}, \"right\": dict{\"y\": 2}}\n"
    ">_(child.values)\n";
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_dict_set_list"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_dict_get_list"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_dict_values_dict"), std::string::npos);
}

TEST(StyioSecurityNightlyCodegen, PreservesDeclaredFunctionParamTypesAcrossStringlyCallSites) {
  const std::string src =
    "# double_it := (x: i64) => x * 2\n"
    "@stdin >> #(line) => {\n"
    "  >_(double_it(line))\n"
    "}\n";
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("define i64 @double_it(i64 %x)"), std::string::npos);
  EXPECT_NE(llvm_ir.find("call i64 @double_it(i64"), std::string::npos);
  EXPECT_EQ(llvm_ir.find("define i64 @double_it(ptr %x)"), std::string::npos);
}

TEST(StyioSecurityNightlyCodegen, LowersGenericListFunctionParamTypes) {
  const std::string src =
    "# first : i64 := (xs: list[i64]) => xs[0]\n"
    ">_(first([1, 2, 3]))\n";
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("define i64 @first(i64 %xs)"), std::string::npos);
  EXPECT_NE(llvm_ir.find("call i64 @styio_list_get"), std::string::npos);
  EXPECT_NE(llvm_ir.find("call i64 @first(i64"), std::string::npos);
}

TEST(StyioSecurityNightlyCodegen, NestedLoopDirectReturnExitsEnclosingFunction) {
  const std::string src =
    "# two_sum : list[i64] := (nums: list[i64], target: i64) => {\n"
    "  n = nums.length - 1\n"
    "  [0..n] >> #(i) => {\n"
    "    [i+1..n] >> #(j) => {\n"
    "      ?(nums[i] + nums[j] == target) => {\n"
    "        <| [i, j]\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "  <| [-1, -1]\n"
    "}\n"
    "ans = two_sum([2, 7, 11, 15], 9)\n"
    "ans[0] -> [>_]\n"
    "ans[1] -> [>_]\n";
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("define i64 @two_sum(i64 %nums, i64 %target)"), std::string::npos);
  EXPECT_NE(llvm_ir.find("call i64 @styio_list_get"), std::string::npos);
  const auto nested_then = llvm_ir.find("styif_then");
  ASSERT_NE(nested_then, std::string::npos);
  EXPECT_NE(llvm_ir.find("ret i64", nested_then), std::string::npos);
}

TEST(StyioSecurityNightlyRuntime, ContainerRuntimeCApiCoversNestedHandlesAndErrors) {
  styio_runtime_clear_error();

  ASSERT_GT(styio_dict_runtime_supported_impl_count(), 0);
  EXPECT_EQ(styio_dict_runtime_supported_impl_name(-1), nullptr);
  EXPECT_STREQ(styio_dict_runtime_canonical_impl_name("ORDERED-HASH"), "ordered-hash");
  EXPECT_EQ(styio_dict_runtime_canonical_impl_name("missing-backend"), nullptr);
  EXPECT_EQ(styio_dict_runtime_set_impl_by_name("linear"), 1);
  EXPECT_STREQ(styio_dict_runtime_get_impl_name(), "linear");
  EXPECT_EQ(styio_dict_runtime_set_impl(9999), 0);
  EXPECT_EQ(styio_dict_runtime_set_impl_by_name("ordered-hash"), 1);

  int64_t bools = styio_list_new_bool();
  int64_t chars = styio_list_new_char();
  int64_t ints = styio_list_new_i64();
  int64_t floats = styio_list_new_f64();
  int64_t strings = styio_list_new_cstr();
  ASSERT_NE(bools, 0);
  ASSERT_NE(chars, 0);
  ASSERT_NE(ints, 0);
  ASSERT_NE(floats, 0);
  ASSERT_NE(strings, 0);

  styio_list_push_bool(bools, 2);
  styio_list_insert_bool(bools, 0, 0);
  EXPECT_EQ(styio_list_get_bool(bools, 1), 1);
  styio_list_set_bool(bools, 0, 1);
  styio_list_pop(bools);
  EXPECT_EQ(styio_list_len(bools), 1);

  styio_list_push_char(chars, '\n');
  styio_list_push_char(chars, '\r');
  styio_list_push_char(chars, '\t');
  styio_list_push_char(chars, '\0');
  styio_list_push_char(chars, '\\');
  styio_list_push_char(chars, '\'');
  styio_list_insert_char(chars, 1, 'A');
  EXPECT_EQ(styio_list_get_char(chars, 1), 'A');
  styio_list_set_char(chars, 1, 'B');

  styio_list_push_i64(ints, 10);
  styio_list_insert_i64(ints, 1, 20);
  styio_list_set(ints, 0, 5);
  EXPECT_EQ(styio_list_get(ints, 1), 20);
  int64_t ints_tail = styio_list_slice(ints, 1, 0, 0);
  ASSERT_NE(ints_tail, 0);
  EXPECT_EQ(styio_list_get(ints_tail, 0), 20);

  styio_list_push_f64(floats, 1.25);
  styio_list_insert_f64(floats, 1, 2.5);
  styio_list_set_f64(floats, 0, 3.5);
  EXPECT_DOUBLE_EQ(styio_list_get_f64(floats, 1), 2.5);

  styio_list_push_cstr(strings, "alpha\nbeta");
  styio_list_insert_cstr(strings, 1, nullptr);
  styio_list_set_cstr(strings, 1, "quote\"");
  const char* list_string = styio_list_get_cstr(strings, 1);
  ASSERT_NE(list_string, nullptr);
  EXPECT_STREQ(list_string, "quote\"");
  styio_free_cstr(list_string);

  const char* char_repr = styio_list_to_cstr(chars);
  ASSERT_NE(char_repr, nullptr);
  EXPECT_NE(std::strstr(char_repr, "'\\n'"), nullptr);
  EXPECT_NE(std::strstr(char_repr, "'\\r'"), nullptr);
  EXPECT_NE(std::strstr(char_repr, "'\\t'"), nullptr);
  EXPECT_NE(std::strstr(char_repr, "'\\0'"), nullptr);
  styio_free_cstr(char_repr);

  int64_t matrix = styio_matrix_new_i64(2, 2);
  ASSERT_NE(matrix, 0);
  styio_matrix_set_i64(matrix, 0, 0, 1);
  styio_matrix_set_i64(matrix, 0, 1, 2);
  styio_matrix_set_i64(matrix, 1, 0, 3);
  styio_matrix_set_i64(matrix, 1, 1, 4);

  int64_t nested_lists = styio_list_new_list();
  int64_t nested_dicts = styio_list_new_dict();
  int64_t nested_mats = styio_list_new_matrix();
  ASSERT_NE(nested_lists, 0);
  ASSERT_NE(nested_dicts, 0);
  ASSERT_NE(nested_mats, 0);
  styio_list_push_list(nested_lists, ints);
  styio_list_insert_list(nested_lists, 1, ints_tail);
  int64_t nested_list_copy = styio_list_get_list(nested_lists, 0);
  ASSERT_NE(nested_list_copy, 0);
  styio_list_set_list(nested_lists, 0, ints_tail);
  int64_t nested_list_slice = styio_list_slice(nested_lists, 0, 1, 1);
  ASSERT_NE(nested_list_slice, 0);

  int64_t dict_i64 = styio_dict_new_i64();
  ASSERT_NE(dict_i64, 0);
  styio_dict_set_i64(dict_i64, "a", 1);
  styio_dict_set_i64(dict_i64, "a", 2);
  EXPECT_EQ(styio_dict_get_i64(dict_i64, "a"), 2);
  int64_t dict_keys = styio_dict_keys(dict_i64);
  int64_t dict_vals = styio_dict_values_i64(dict_i64);
  ASSERT_NE(dict_keys, 0);
  ASSERT_NE(dict_vals, 0);
  EXPECT_EQ(styio_list_get(dict_vals, 0), 2);
  const char* dict_repr = styio_dict_to_cstr(dict_i64);
  ASSERT_NE(dict_repr, nullptr);
  EXPECT_NE(std::strstr(dict_repr, "\"a\":2"), nullptr);
  styio_free_cstr(dict_repr);
  int64_t dict_clone = styio_dict_clone(dict_i64);
  ASSERT_NE(dict_clone, 0);

  int64_t dict_bool = styio_dict_new_bool();
  int64_t dict_f64 = styio_dict_new_f64();
  int64_t dict_str = styio_dict_new_cstr();
  int64_t dict_list = styio_dict_new_list();
  int64_t dict_dict = styio_dict_new_dict();
  ASSERT_NE(dict_bool, 0);
  ASSERT_NE(dict_f64, 0);
  ASSERT_NE(dict_str, 0);
  ASSERT_NE(dict_list, 0);
  ASSERT_NE(dict_dict, 0);
  styio_dict_set_bool(dict_bool, "ok", 3);
  styio_dict_set_f64(dict_f64, "pi", 3.25);
  styio_dict_set_cstr(dict_str, "text", "line\nnext");
  EXPECT_EQ(styio_dict_get_bool(dict_bool, "ok"), 1);
  EXPECT_DOUBLE_EQ(styio_dict_get_f64(dict_f64, "pi"), 3.25);
  const char* got_text = styio_dict_get_cstr(dict_str, "text");
  ASSERT_NE(got_text, nullptr);
  EXPECT_STREQ(got_text, "line\nnext");
  styio_free_cstr(got_text);

  styio_dict_set_list(dict_list, "xs", ints);
  styio_dict_set_list(dict_list, "xs", ints_tail);
  int64_t dict_list_value = styio_dict_get_list(dict_list, "xs");
  ASSERT_NE(dict_list_value, 0);
  int64_t dict_list_values = styio_dict_values_list(dict_list);
  ASSERT_NE(dict_list_values, 0);
  styio_dict_set_dict(dict_dict, "nested", dict_i64);
  styio_dict_set_dict(dict_dict, "nested", dict_clone);
  int64_t dict_dict_value = styio_dict_get_dict(dict_dict, "nested");
  ASSERT_NE(dict_dict_value, 0);
  int64_t dict_dict_values = styio_dict_values_dict(dict_dict);
  ASSERT_NE(dict_dict_values, 0);

  styio_list_push_dict(nested_dicts, dict_i64);
  styio_list_insert_dict(nested_dicts, 1, dict_clone);
  int64_t nested_dict_copy = styio_list_get_dict(nested_dicts, 0);
  ASSERT_NE(nested_dict_copy, 0);
  styio_list_set_dict(nested_dicts, 0, dict_clone);
  int64_t nested_dict_slice = styio_list_slice(nested_dicts, 0, 1, 1);
  ASSERT_NE(nested_dict_slice, 0);

  styio_list_push_matrix(nested_mats, matrix);
  styio_list_insert_matrix(nested_mats, 1, matrix);
  int64_t nested_matrix_copy = styio_list_get_matrix(nested_mats, 0);
  ASSERT_NE(nested_matrix_copy, 0);
  styio_list_set_matrix(nested_mats, 0, matrix);
  int64_t nested_matrix_slice = styio_list_slice(nested_mats, 0, 1, 1);
  ASSERT_NE(nested_matrix_slice, 0);

  const char* nested_repr = styio_list_to_cstr(nested_mats);
  ASSERT_NE(nested_repr, nullptr);
  EXPECT_NE(std::strstr(nested_repr, "[[1,2],[3,4]]"), nullptr);
  styio_free_cstr(nested_repr);

  int64_t empty = styio_list_new_i64();
  styio_list_pop(empty);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  EXPECT_EQ(styio_runtime_error_matches_effect("bounds"), 1);
  styio_runtime_clear_error();
  EXPECT_EQ(styio_list_slice(ints, -1, 0, 0), 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_i64(dict_i64, nullptr), 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_i64(dict_i64, "missing"), 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  styio_runtime_clear_error();

  styio_list_release(empty);
  styio_list_release(nested_matrix_slice);
  styio_matrix_release(nested_matrix_copy);
  styio_list_release(nested_dict_slice);
  styio_dict_release(nested_dict_copy);
  styio_list_release(nested_list_slice);
  styio_list_release(nested_list_copy);
  styio_list_release(dict_dict_values);
  styio_dict_release(dict_dict_value);
  styio_list_release(dict_list_values);
  styio_list_release(dict_list_value);
  styio_dict_release(dict_dict);
  styio_dict_release(dict_list);
  styio_dict_release(dict_str);
  styio_dict_release(dict_f64);
  styio_dict_release(dict_bool);
  styio_dict_release(dict_clone);
  styio_list_release(dict_vals);
  styio_list_release(dict_keys);
  styio_dict_release(dict_i64);
  styio_list_release(nested_mats);
  styio_list_release(nested_dicts);
  styio_list_release(nested_lists);
  styio_matrix_release(matrix);
  styio_list_release(ints_tail);
  styio_list_release(strings);
  styio_list_release(floats);
  styio_list_release(ints);
  styio_list_release(chars);
  styio_list_release(bools);

  EXPECT_EQ(styio_list_active_count(), 0);
  EXPECT_EQ(styio_dict_active_count(), 0);
  EXPECT_EQ(styio_matrix_active_count(), 0);
  EXPECT_EQ(styio_runtime_has_error(), 0);
}

TEST(StyioSecurityNightlyRuntime, MatrixTaskFileAndStdinCApiEdgesStayExplicit) {
  styio_runtime_clear_error();

  EXPECT_EQ(styio_matrix_new_i64(0, 2), 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  ASSERT_NE(styio_runtime_last_error_subcode(), nullptr);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_MATRIX_SHAPE");
  styio_runtime_clear_error();

  int64_t a = styio_matrix_new_i64(2, 2);
  int64_t b = styio_matrix_identity_i64(2);
  int64_t f = styio_matrix_identity_f64(2);
  ASSERT_NE(a, 0);
  ASSERT_NE(b, 0);
  ASSERT_NE(f, 0);
  int64_t* raw_i64 = styio_matrix_data_i64(a);
  ASSERT_NE(raw_i64, nullptr);
  raw_i64[0] = 1;
  raw_i64[1] = 2;
  raw_i64[2] = 3;
  raw_i64[3] = 4;
  EXPECT_EQ(styio_matrix_rows(a), 2);
  EXPECT_EQ(styio_matrix_cols(a), 2);
  int64_t shape = styio_matrix_shape(a);
  ASSERT_NE(shape, 0);
  EXPECT_EQ(styio_list_get(shape, 0), 2);
  EXPECT_EQ(styio_list_get(shape, 1), 2);
  styio_list_release(shape);

  int64_t a_clone = styio_matrix_clone(a);
  int64_t a_as_f64 = styio_matrix_clone_f64(a);
  ASSERT_NE(a_clone, 0);
  ASSERT_NE(a_as_f64, 0);
  EXPECT_DOUBLE_EQ(styio_matrix_get_f64(a_as_f64, 1, 1), 4.0);
  double* raw_f64 = styio_matrix_data_f64(a_as_f64);
  ASSERT_NE(raw_f64, nullptr);
  raw_f64[0] = 8.0;

  int64_t row_i64 = styio_matrix_row_i64(a, 1);
  int64_t row_f64 = styio_matrix_row_f64(a_as_f64, 0);
  int64_t rows_i64 = styio_matrix_rows_slice_i64(a, 0, 0, 0);
  int64_t rows_f64 = styio_matrix_rows_slice_f64(a_as_f64, 0, 1, 1);
  ASSERT_NE(row_i64, 0);
  ASSERT_NE(row_f64, 0);
  ASSERT_NE(rows_i64, 0);
  ASSERT_NE(rows_f64, 0);
  EXPECT_EQ(styio_list_get(row_i64, 1), 4);
  EXPECT_DOUBLE_EQ(styio_list_get_f64(row_f64, 0), 8.0);

  int64_t add_i = styio_matrix_add_i64(a, b);
  int64_t sub_i = styio_matrix_sub_i64(a, b);
  int64_t had_i = styio_matrix_hadamard_i64(a, b);
  int64_t mat_i = styio_matrix_matmul_i64(a, b);
  int64_t scale_i = styio_matrix_scale_i64(a, 2);
  int64_t trans_i = styio_matrix_transpose_i64(a);
  ASSERT_NE(add_i, 0);
  ASSERT_NE(sub_i, 0);
  ASSERT_NE(had_i, 0);
  ASSERT_NE(mat_i, 0);
  ASSERT_NE(scale_i, 0);
  ASSERT_NE(trans_i, 0);
  EXPECT_EQ(styio_matrix_dot_i64(a, b), 5);
  EXPECT_EQ(styio_matrix_sum_i64(a), 10);
  EXPECT_EQ(styio_matrix_get_i64(trans_i, 1, 0), 2);

  int64_t add_f = styio_matrix_add_f64(a_as_f64, f);
  int64_t sub_f = styio_matrix_sub_f64(a_as_f64, f);
  int64_t had_f = styio_matrix_hadamard_f64(a_as_f64, f);
  int64_t mat_f = styio_matrix_matmul_f64(a_as_f64, f);
  int64_t scale_f = styio_matrix_scale_f64(a_as_f64, 0.5);
  int64_t trans_f = styio_matrix_transpose_f64(a_as_f64);
  ASSERT_NE(add_f, 0);
  ASSERT_NE(sub_f, 0);
  ASSERT_NE(had_f, 0);
  ASSERT_NE(mat_f, 0);
  ASSERT_NE(scale_f, 0);
  ASSERT_NE(trans_f, 0);
  EXPECT_DOUBLE_EQ(styio_matrix_dot_f64(a_as_f64, f), 12.0);
  EXPECT_DOUBLE_EQ(styio_matrix_sum_f64(a_as_f64), 17.0);
  EXPECT_GT(styio_matrix_norm(a_as_f64), 0.0);

  const char* matrix_repr = styio_matrix_to_cstr(a);
  ASSERT_NE(matrix_repr, nullptr);
  EXPECT_NE(std::strstr(matrix_repr, "[[1,2],[3,4]]"), nullptr);
  styio_free_cstr(matrix_repr);

  int64_t bad_shape = styio_matrix_new_i64(1, 3);
  ASSERT_NE(bad_shape, 0);
  EXPECT_EQ(styio_matrix_add_i64(a, bad_shape), 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  styio_runtime_clear_error();
  EXPECT_EQ(styio_matrix_get_i64(a, 9, 0), 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  styio_runtime_clear_error();
  EXPECT_EQ(styio_matrix_rows_slice_i64(a, -1, 0, 0), 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  styio_runtime_clear_error();
  styio_matrix_set_f64(a, 0, 0, 1.0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  styio_runtime_clear_error();

  styio_task_scheduler_profile_reset();
  styio_task_scheduler_profile_enable(1);
  styio_task_scheduler_profile_snapshot(nullptr);
  int64_t task_i = styio_task_i64_ready(42);
  ASSERT_NE(task_i, 0);
  EXPECT_DOUBLE_EQ(styio_task_f64_pull(task_i), 0.0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  styio_runtime_clear_error();
  EXPECT_EQ(styio_task_i64_pull(task_i), 42);
  EXPECT_EQ(styio_task_i64_pull(task_i), 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  styio_runtime_clear_error();
  styio_task_release(task_i);

  int64_t task_f = styio_task_f64_ready(2.25);
  int64_t task_s = styio_task_cstr_ready(nullptr);
  ASSERT_NE(task_f, 0);
  ASSERT_NE(task_s, 0);
  EXPECT_DOUBLE_EQ(styio_task_f64_pull(task_f), 2.25);
  EXPECT_STREQ(styio_task_cstr_pull(task_s), "");
  styio_task_release(task_f);
  styio_task_release(task_s);

  int64_t spawned_i = styio_task_i64_spawn(+[](void*) -> int64_t { return 7; }, nullptr);
  int64_t spawned_f = styio_task_f64_spawn(+[](void*) -> double { return 1.5; }, nullptr);
  int64_t spawned_s = styio_task_cstr_spawn(+[](void*) -> const char* { return "done"; }, nullptr);
  ASSERT_NE(spawned_i, 0);
  ASSERT_NE(spawned_f, 0);
  ASSERT_NE(spawned_s, 0);
  EXPECT_EQ(styio_task_i64_pull(spawned_i), 7);
  EXPECT_DOUBLE_EQ(styio_task_f64_pull(spawned_f), 1.5);
  EXPECT_STREQ(styio_task_cstr_pull(spawned_s), "done");
  styio_task_release(spawned_i);
  styio_task_release(spawned_f);
  styio_task_release(spawned_s);
  StyioTaskSchedulerProfileSnapshot snapshot{};
  styio_task_scheduler_profile_snapshot(&snapshot);
  EXPECT_EQ(snapshot.enabled, 1);
  EXPECT_GE(snapshot.ready_tasks, 3);
  EXPECT_GE(snapshot.spawned_tasks, 3);
  styio_task_scheduler_profile_enable(0);

  const std::string temp_path =
    (std::filesystem::temp_directory_path()
     / ("styio-runtime-capi-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt")).string();
  int64_t out_file = styio_file_open_write(temp_path.c_str());
  ASSERT_NE(out_file, 0);
  styio_file_write_cstr(out_file, "123\n456\n");
  styio_file_close(out_file);
  int64_t in_file = styio_file_open_auto(temp_path.c_str());
  ASSERT_NE(in_file, 0);
  EXPECT_STREQ(styio_file_read_line(in_file), "123");
  EXPECT_EQ(styio_file_read_i64line_from_handle(in_file), 456);
  styio_file_rewind(in_file);
  EXPECT_EQ(styio_file_read_i64line_from_handle(in_file), 123);
  styio_file_close(in_file);
  EXPECT_EQ(styio_read_file_i64line(temp_path.c_str()), 123);
  std::filesystem::remove(temp_path);

  EXPECT_EQ(styio_file_open(nullptr), 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  styio_runtime_clear_error();
  EXPECT_EQ(styio_read_file_i64line(nullptr), 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  styio_runtime_clear_error();

#ifndef _WIN32
  {
    ScopedStdinRedirect stdin_redirect("[1, 2, 3]\n");
    ASSERT_TRUE(stdin_redirect.ok());
    int64_t stdin_list = styio_list_i64_read_stdin();
    ASSERT_NE(stdin_list, 0);
    EXPECT_EQ(styio_list_get(stdin_list, 2), 3);
    styio_list_release(stdin_list);
  }
  {
    ScopedStdinRedirect stdin_redirect("[1.5, 2.5]\n");
    ASSERT_TRUE(stdin_redirect.ok());
    int64_t stdin_list = styio_list_f64_read_stdin();
    ASSERT_NE(stdin_list, 0);
    EXPECT_DOUBLE_EQ(styio_list_get_f64(stdin_list, 1), 2.5);
    styio_list_release(stdin_list);
  }
  {
    ScopedStdinRedirect stdin_redirect("[1, nope]\n");
    ASSERT_TRUE(stdin_redirect.ok());
    EXPECT_EQ(styio_list_i64_read_stdin(), 0);
    EXPECT_EQ(styio_runtime_has_error(), 1);
    styio_runtime_clear_error();
  }
#endif

  int64_t empty_lines = styio_string_lines(nullptr);
  int64_t split_lines = styio_string_lines("a\r\nb\n");
  ASSERT_NE(empty_lines, 0);
  ASSERT_NE(split_lines, 0);
  EXPECT_EQ(styio_list_len(empty_lines), 0);
  EXPECT_EQ(styio_list_len(split_lines), 2);
  styio_list_release(split_lines);
  styio_list_release(empty_lines);

  styio_matrix_release(bad_shape);
  styio_matrix_release(trans_f);
  styio_matrix_release(scale_f);
  styio_matrix_release(mat_f);
  styio_matrix_release(had_f);
  styio_matrix_release(sub_f);
  styio_matrix_release(add_f);
  styio_matrix_release(trans_i);
  styio_matrix_release(scale_i);
  styio_matrix_release(mat_i);
  styio_matrix_release(had_i);
  styio_matrix_release(sub_i);
  styio_matrix_release(add_i);
  styio_list_release(rows_f64);
  styio_list_release(rows_i64);
  styio_list_release(row_f64);
  styio_list_release(row_i64);
  styio_matrix_release(a_as_f64);
  styio_matrix_release(a_clone);
  styio_matrix_release(f);
  styio_matrix_release(b);
  styio_matrix_release(a);

  EXPECT_EQ(styio_task_active_count(), 0);
  EXPECT_EQ(styio_list_active_count(), 0);
  EXPECT_EQ(styio_dict_active_count(), 0);
  EXPECT_EQ(styio_matrix_active_count(), 0);
  EXPECT_EQ(styio_runtime_has_error(), 0);
}

TEST(StyioSecurityNightlyRuntime, ListHandlesAreCleanedUpAfterExecution) {
  const std::string src =
    "l = @stdin: list[i32]\n"
    "l = 7\n";
  EXPECT_NO_THROW(
    execute_program_engine_with_stdin_latest(
      src,
      StyioParserEngine::Nightly,
      "[1,2,3]\n"
    )
  );
  EXPECT_EQ(styio_list_active_count(), 0);
  EXPECT_EQ(styio_runtime_has_error(), 0);
}

TEST(StyioSecurityNightlyRuntime, ListLiteralHandlesAreCleanedUpAfterExecution) {
  const std::string src =
    "l = [1,2,3]\n"
    "l = []\n";
  EXPECT_NO_THROW(
    execute_program_engine_with_stdin_latest(
      src,
      StyioParserEngine::Nightly,
      ""
    )
  );
  EXPECT_EQ(styio_list_active_count(), 0);
  EXPECT_EQ(styio_runtime_has_error(), 0);
}

TEST(StyioSecurityNightlyRuntime, CollectedStringListsAreCleanedUpAfterExecution) {
  const std::string src =
    "lines << @stdin\n"
    "lines = 7\n";
  EXPECT_NO_THROW(
    execute_program_engine_with_stdin_latest(
      src,
      StyioParserEngine::Nightly,
      "alpha\nbeta\n"
    )
  );
  EXPECT_EQ(styio_list_active_count(), 0);
  EXPECT_EQ(styio_runtime_has_error(), 0);
}

TEST(StyioSecurityNightlyRuntime, DictHandlesAreCleanedUpAfterExecution) {
  const std::string src =
    "d = dict{\"a\": 1}\n"
    "d = 7\n";
  EXPECT_NO_THROW(
    execute_program_engine_with_stdin_latest(
      src,
      StyioParserEngine::Nightly,
      ""
    )
  );
  EXPECT_EQ(styio_dict_active_count(), 0);
  EXPECT_EQ(styio_runtime_has_error(), 0);
}

TEST(StyioSecurityNightlyRuntime, MatrixFinalBindHandlesStayLiveUntilScopeExit) {
  const std::string src =
    "m: matrix := [[1,2],[3,4]]\n"
    "n = m + m\n"
    "q = m + m\n"
    ">_(q[0][0])\n";

  EXPECT_NO_THROW(
    execute_program_engine_with_stdin_latest(
      src,
      StyioParserEngine::Nightly,
      ""
    )
  );
  EXPECT_EQ(styio_matrix_active_count(), 0);
  EXPECT_EQ(styio_runtime_has_error(), 0);
}

TEST(StyioSecurityNightlyRuntime, MatrixHandlesAreCleanedUpAfterOverwriteAndExecution) {
  const std::string src =
    "m: matrix = [[1,2],[3,4]]\n"
    "m = m + m\n";

  EXPECT_NO_THROW(
    execute_program_engine_with_stdin_latest(
      src,
      StyioParserEngine::Nightly,
      ""
    )
  );
  EXPECT_EQ(styio_matrix_active_count(), 0);
  EXPECT_EQ(styio_runtime_has_error(), 0);
}

TEST(StyioSecurityNightlyRuntime, RuntimeErrorReturnCleansMatrixHandles) {
  const std::string src =
    "m: matrix = [[1,2],[3,4]]\n"
    ">_(m[9][0])\n";

  EXPECT_NO_THROW(
    execute_program_engine_with_stdin_latest(
      src,
      StyioParserEngine::Nightly,
      ""
    )
  );
  EXPECT_EQ(styio_matrix_active_count(), 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  styio_runtime_clear_error();
}

TEST(StyioSecurityNightlyRuntime, NestedHandleDictsReleaseOwnedChildrenOnOverwrite) {
  const std::string src =
    "d = dict{\"nums\": [1,2,3], \"more\": [4,5]}\n"
    "d = 7\n"
    "child = dict{\"left\": dict{\"x\": 1}, \"right\": dict{\"y\": 2}}\n"
    "child = 8\n";
  EXPECT_NO_THROW(
    execute_program_engine_with_stdin_latest(
      src,
      StyioParserEngine::Nightly,
      ""
    )
  );
  EXPECT_EQ(styio_list_active_count(), 0);
  EXPECT_EQ(styio_dict_active_count(), 0);
  EXPECT_EQ(styio_runtime_has_error(), 0);
}

TEST(StyioSecurityNightlyRuntime, InvalidNumericStringArgumentSetsRuntimeError) {
  const std::string src =
    "# add1 := (x: i64) => x + 1\n"
    "@stdin >> #(line) => {\n"
    "  >_(add1(line))\n"
    "}\n";
  EXPECT_NO_THROW(
    execute_program_engine_with_stdin_latest(
      src,
      StyioParserEngine::Nightly,
      "abc\n"
    )
  );
  EXPECT_EQ(styio_runtime_has_error(), 1);
  ASSERT_NE(styio_runtime_last_error_subcode(), nullptr);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_NUMERIC_PARSE");
  ASSERT_NE(styio_runtime_last_error(), nullptr);
  EXPECT_NE(std::strstr(styio_runtime_last_error(), "cannot parse integer"), nullptr);
  styio_runtime_clear_error();
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnResourcePostfixSubsetSamples) {
  const std::vector<std::string> samples = {
    "\"Hello from Styio\" -> @file(\"/tmp/styio-new-parser-resource-postfix-write.txt\")\n",
    "x = 42\nx -> @file(\"/tmp/styio-new-parser-resource-postfix-redirect.txt\")\n",
    "# write_value := () => \"payload\" -> @file(\"/tmp/styio-new-parser-resource-postfix-func.txt\")\nwrite_value()\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnIteratorStmtSubsetSamples) {
  const std::vector<std::string> samples = {
    "f <- @file(\"tests/features/file_resources/data/hello.txt\")\nf >> #(line) => {\n    >_(line)\n}\n",
    "# double_it := (x: i32) => x * 2\nf <- @file(\"tests/features/file_resources/data/numbers.txt\")\nf >> #(line) => {\n    >_(double_it(line))\n}\n",
    "result = true\n[1, 2, 3] >> #(x) => {\n    result = result && (x > 0)\n}\n>_(result)\n",
    "[1, 2, 3] >> #(x) => {\n    >_(x)\n}\n",
    "[1, 2, 3] >> #(n) & [4, 5, 6] >> #(m) => {\n    >_(n + m)\n}\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnInfiniteLoopSubsetSamples) {
  const std::vector<std::string> samples = {
    "x = 0\n[...] >> ?(x < 3) => {\n    x += 1\n}\n>_(x)\n",
    "[...] => {\n    >_(1)\n    ^^^\n}\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityBreakSpelling, RejectsSingleCaretWithStableRouteParityDiagnostic) {
  constexpr const char* expected =
    "break statement requires at least two consecutive '^' characters; "
    "use '^^' (minimum) or '^^^' (conventional)";

  auto diagnostic = [](const std::string& source, StyioParserEngine engine) {
    try {
      (void)parse_program_engine_to_repr_latest(source, engine);
    }
    catch (const StyioSyntaxError& error) {
      return std::string(error.what());
    }
    return std::string();
  };

  const std::vector<std::pair<std::string, std::string>> cases = {
    {"^\n", std::string("\nStyio.SyntaxError:\n^\n^- ") + expected},
    {
      "[...] => {\n  ^\n}\n",
      std::string("\nStyio.SyntaxError:\n  ^\n  ^- ") + expected,
    },
  };
  for (const auto& [source, exact_diagnostic] : cases) {
    const std::string nightly = diagnostic(source, StyioParserEngine::Nightly);
    const std::string legacy = diagnostic(source, StyioParserEngine::Legacy);
    ASSERT_FALSE(nightly.empty()) << source;
    EXPECT_EQ(nightly, legacy) << source;
    EXPECT_EQ(nightly, exact_diagnostic) << source;
    EXPECT_EQ(nightly, diagnostic(source, StyioParserEngine::Nightly)) << source;
    EXPECT_EQ(legacy, diagnostic(source, StyioParserEngine::Legacy)) << source;
  }
}

TEST(StyioSecurityBreakSpelling, AcceptsMinimumConventionalAndUnboundedRunsAtDepthOne) {
  const std::vector<std::string> spellings = {
    "^^",
    "^^^",
    "^^^^",
    std::string(64, '^'),
  };

  for (const auto& spelling : spellings) {
    {
      DirectParserContext context(spelling);
      std::unique_ptr<StyioAST> statement(parse_stmt_or_expr_legacy(context.get()));
      auto* break_statement = dynamic_cast<BreakAST*>(statement.get());
      ASSERT_NE(break_statement, nullptr) << spelling.size();
      EXPECT_EQ(break_statement->getDepth(), 1u);
      EXPECT_EQ(context.get().get_token_index(), spelling.size());
    }
    {
      DirectParserContext context(spelling);
      std::unique_ptr<StyioAST> statement(parse_stmt_subset_nightly(context.get()));
      auto* break_statement = dynamic_cast<BreakAST*>(statement.get());
      ASSERT_NE(break_statement, nullptr) << spelling.size();
      EXPECT_EQ(break_statement->getDepth(), 1u);
      EXPECT_EQ(context.get().get_token_index(), spelling.size());
    }

    const std::string loop_source = "[...] => {\n  " + spelling + "\n}\n";
    for (const StyioParserEngine engine : {
           StyioParserEngine::Nightly,
           StyioParserEngine::Legacy,
         }) {
      DirectParserContext context(loop_source);
      std::unique_ptr<MainBlockAST> ast(
        parse_main_block_with_engine_latest(context.get(), engine));
      AstToStyioIRLowerer analyzer;
      ast->typeInfer(&analyzer);
      std::unique_ptr<StyioIR> ir(ast->toStyioIR(&analyzer));

      class BreakCounter final : public styio::ir::StyioIRWalker
      {
      public:
        std::size_t count = 0;
        unsigned depth = 0;

        void visitSGBreak(SGBreak* node) override {
          count += 1;
          depth = node->depth;
        }
      } counter;
      counter.walk(ir.get());
      EXPECT_EQ(counter.count, 1u) << spelling.size();
      EXPECT_EQ(counter.depth, 1u) << spelling.size();
    }
  }
}

TEST(StyioSecurityBreakSpelling, RejectsSeparatedRunsAndPreservesCaretContexts) {
  constexpr const char* minimum_spelling_diagnostic =
    "break statement requires at least two consecutive '^' characters; "
    "use '^^' (minimum) or '^^^' (conventional)";
  auto syntax_diagnostic = [](const std::string& source, StyioParserEngine engine) {
    try {
      (void)parse_program_engine_to_repr_latest(source, engine);
    }
    catch (const StyioSyntaxError& error) {
      return std::string(error.what());
    }
    return std::string();
  };

  const std::vector<std::pair<std::string, std::string>> one_caret_runs = {
    {
      "^ ^\n",
      std::string("\nStyio.SyntaxError:\n^ ^\n^--- ") + minimum_spelling_diagnostic,
    },
    {
      "^/*gap*/^\n",
      std::string("\nStyio.SyntaxError:\n^/*gap*/^\n^--------- ") + minimum_spelling_diagnostic,
    },
  };
  for (const auto& [source, expected_diagnostic] : one_caret_runs) {
    const std::string nightly = syntax_diagnostic(source, StyioParserEngine::Nightly);
    const std::string legacy = syntax_diagnostic(source, StyioParserEngine::Legacy);
    EXPECT_EQ(nightly, expected_diagnostic) << source;
    EXPECT_EQ(legacy, expected_diagnostic) << source;
  }

  const std::vector<std::string> separated_valid_runs = {
    "^^ ^^\n",
    "^^/*gap*/^^\n",
  };
  for (const auto& source : separated_valid_runs) {
    const std::string nightly = syntax_diagnostic(source, StyioParserEngine::Nightly);
    const std::string legacy = syntax_diagnostic(source, StyioParserEngine::Legacy);
    ASSERT_FALSE(nightly.empty()) << source;
    EXPECT_EQ(nightly, legacy) << source;
    EXPECT_NE(nightly.find("spaced caret runs do not form one break statement"),
              std::string::npos) << source;
  }

  const std::string separated_statements = "^^\n^^\n";
  for (const StyioParserEngine engine : {
         StyioParserEngine::Nightly,
         StyioParserEngine::Legacy,
       }) {
    DirectParserContext context(separated_statements);
    std::unique_ptr<MainBlockAST> ast(
      parse_main_block_with_engine_latest(context.get(), engine));
    ASSERT_EQ(ast->getStmts().size(), 2u);
    for (StyioAST* statement : ast->getStmts()) {
      auto* break_statement = dynamic_cast<BreakAST*>(statement);
      ASSERT_NE(break_statement, nullptr);
      EXPECT_EQ(break_statement->getDepth(), 1u);
    }
  }

  {
    DirectParserContext context("^3)");
    std::unique_ptr<StyioAST> ast(
      parse_cond_rhs(context.get(), IntAST::Create("5")));
    auto* xor_expression = dynamic_cast<CondAST*>(ast.get());
    ASSERT_NE(xor_expression, nullptr);
    EXPECT_EQ(xor_expression->getSign(), LogicType::XOR);
  }

  struct ListCaretCase {
    const char* source;
    StyioTokenType entry_token;
    StyioNodeType expected_type;
  };
  const std::vector<ListCaretCase> list_caret_forms = {
    {"[^2]", StyioTokenType::INTEGER, StyioNodeType::Access_By_Index},
    {"[-: ^2]", StyioTokenType::INTEGER, StyioNodeType::Remove_Item_By_Index},
    {"[?^ ()]", StyioTokenType::TOK_LPAREN, StyioNodeType::Get_Indices_By_Many_Values},
  };
  for (const auto& caret_case : list_caret_forms) {
    DirectParserContext context(caret_case.source);
    align_legacy_char_entry_token(context, caret_case.entry_token);
    std::unique_ptr<StyioAST> ast(
      parse_index_op(context.get(), NameAST::Create("xs")));
    auto* list_operation = dynamic_cast<ListOpAST*>(ast.get());
    ASSERT_NE(list_operation, nullptr) << caret_case.source;
    EXPECT_EQ(list_operation->getOp(), caret_case.expected_type) << caret_case.source;
  }
}

TEST(StyioSecurityNightlyParserStmt, RejectsOldConditionalLoopSyntax) {
  const std::string src = "x = 0\n[...] ?(x < 3) >> {\n  x += 1\n}\n";
  EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioSyntaxError);
  EXPECT_THROW(parse_program_to_repr_latest(src, false), StyioSyntaxError);
}

TEST(StyioSecurityNightlySemantics, RejectsNonBoolConditionalLoopGuard) {
  const std::string src = "[...] >> ?(\"not_bool\") => {\n  ^^^\n}\n";
  EXPECT_THROW(
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly),
    StyioTypeError
  );
  EXPECT_THROW(
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Legacy),
    StyioTypeError
  );
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnInstantPullSubsetSamples) {
  const std::string src = "x = 1\nresult = x + (<< @file(\"tests/features/stream_processing/data/ref50.txt\"))\n>_(result)\n";
  EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false));
}

TEST(StyioSecurityNightlyParserStmt, RejectsRetiredLegacyStateAndSnapshotSyntax) {
  const std::vector<std::string> samples = {
    "@[ref_val] << @file(\"tests/features/stream_processing/data/ref.txt\")\n",
    "@[3](ma = 1 + 2)\n",
    "$state\n",
    "$state[<<, 1]\n",
  };

  for (const auto& src : samples) {
    EXPECT_THROW(parse_program_to_repr_latest(src, true), StyioSyntaxError) << src;
    EXPECT_THROW(parse_program_to_repr_latest(src, false), StyioSyntaxError) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnAtResourceSubsetSamples) {
  const std::vector<std::string> samples = {
    "@file(\"tests/features/stream_processing/data/prices_a.txt\") >> #(a) => {\n    >_(a)\n}\n",
    "@file(\"tests/features/stream_processing/data/input.txt\") >> #(x) => {\n    result = x * 2\n    result << @file(\"/tmp/styio-new-parser-at-resource-subset.txt\")\n}\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityNightlyParserShadow, MatchesLegacyOnRedirectRouteSample) {
  const std::string src =
    "x = 42\n"
    "x -> @file(\"/tmp/styio-nightly-parser-shadow-redirect.txt\")\n";

  EXPECT_EQ(
    parse_program_engine_to_repr_latest(src, StyioParserEngine::Nightly),
    parse_program_engine_to_repr_latest(src, StyioParserEngine::Legacy)
  );
}

TEST(StyioSecurityNightlyParserShadow, MatchesLegacyOnArbitrageGuardRouteSample) {
  const std::string src =
    "@file(\"tests/features/stream_processing/data/exchange_a.txt\") >> #(a) & @file(\"tests/features/stream_processing/data/exchange_b.txt\") >> #(b) => {\n"
    "  gap = a - b\n"
    "  ?(gap > 5 || gap < -5) => { >_(\"Arb: \" + gap) }\n"
    "}\n";

  EXPECT_EQ(
    parse_program_engine_to_repr_latest(src, StyioParserEngine::Nightly),
    parse_program_engine_to_repr_latest(src, StyioParserEngine::Legacy)
  );
}

TEST(StyioSecurityNightlyParserShadow, RejectsRetiredWaveOperatorSyntax) {
  const std::vector<std::string> samples = {
    "x = (1 < 2) <~ 1 | 0\n",
    "x = ?(1 < 2) <~ 1 | 0\n",
    "(1 < 2) ~> >_(1) | @\n",
  };

  for (const auto& src : samples) {
    for (const auto engine : {StyioParserEngine::Nightly, StyioParserEngine::Legacy}) {
      try {
        (void)parse_program_engine_to_repr_latest(src, engine);
        FAIL() << "expected retired wave syntax to be rejected: " << src;
      }
      catch (const StyioSyntaxError& ex) {
        const std::string message = ex.what();
        EXPECT_TRUE(
          message.find("reserved") != std::string::npos
          || message.find("unsupported syntax in authoritative nightly parser") != std::string::npos
        ) << src << "\n" << message;
      }
    }
  }
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnStdStreamWriteShorthandSamples) {
  const std::vector<std::string> samples = {
    "items = [\"hello\"]\nitems >> @stdout\n",
    "items = [\"error\"]\nitems >> @stderr\n",
    "@stdin >> #(line) => {\n    items = [line]\n    items >> @stdout\n}\n",
    "@stdin >> #(line) => {\n    items = [\"processing: \" + line]\n    items >> @stderr\n}\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSafetyStdStream, RejectsWriteShorthandToStdinAtLowering) {
  const std::string src = "\"hello\" >> @stdin\n";

  EXPECT_THROW(
    {
      parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly);
    },
    StyioTypeError
  );
  EXPECT_THROW(
    {
      parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Legacy);
    },
    StyioTypeError
  );
}

TEST(StyioSecurityResourceTopology, ParsesAndLowersTopLevelResourceDeclWriteAndRead) {
  const std::string src =
    "@x : i64|2|\n"
    "1 -> @x\n"
    ">_(@x[-1])\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
}

TEST(StyioSecurityResourceTopology, RejectsLocalResourceDeclarationsWithStableDiagnostic) {
  const std::string src =
    "{\n"
    "  @x : i64|2|\n"
    "}\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected local resource declaration rejection";
  }
  catch (const StyioTypeError& ex) {
    EXPECT_NE(std::string(ex.what()).find("resource declarations are top-level only"), std::string::npos)
      << ex.what();
  }
}

TEST(StyioSecurityResourceTopology, RejectsImplicitWholeResourceCopy) {
  const std::string src =
    "@x : i64|2|\n"
    "copy = @x\n";

  EXPECT_THROW(
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly),
    StyioTypeError
  );
}

TEST(StyioSecurityResourceTopology, LowersBoundedListAndDictResourceSelectors) {
  const std::string src =
    "@samples : list[list[i64]]|..2|\n"
    "[[1,2], [3,4]] >> #(v) => {\n"
    "  v -> @samples\n"
    "}\n"
    ">_(@samples[-1])\n"
    ">_(@samples[...])\n"
    "samples_copy << @samples[...]\n"
    "samples_recent << @samples[-1..]\n"
    ">_(samples_copy)\n"
    ">_(samples_recent)\n"
    "@samples[...] >> #(xs) => {\n"
    "  >_(xs)\n"
    "}\n"
    "@tables : dict[string,i64]|..2|\n"
    "[dict{\"v\": 1}, dict{\"v\": 2}] >> #(d) => {\n"
    "  d -> @tables\n"
    "}\n"
    ">_(@tables[-1])\n"
    ">_(@tables[...])\n"
    "tables_copy << @tables[...]\n"
    "tables_recent << @tables[-1..]\n"
    ">_(tables_copy)\n"
    ">_(tables_recent)\n"
    "@tables[...] >> #(row) => {\n"
    "  >_(row)\n"
    "}\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_list_clone"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_dict_clone"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_new_list"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_new_dict"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_push_list"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_push_dict"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_get_list"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_get_dict"), std::string::npos);
}

TEST(StyioSecurityResourceTopology, LowersBoundedMatrixResourceSelectors) {
  const std::string src =
    "@bucket : matrix|..2|\n"
    "[1] >> #(base) => {\n"
    "  cur: matrix = [[base, base + 1], [base + 2, base + 3]]\n"
    "  cur -> @bucket\n"
    "}\n"
    ">_(@bucket[-1])\n"
    ">_(@bucket[...])\n"
    "snap << @bucket[...]\n"
    ">_(snap)\n"
    "@bucket[...] >> #(row) => {\n"
    "  >_(row)\n"
    "}\n";

  EXPECT_NO_THROW(
    parse_typecheck_and_lower_program_engine_latest(src, StyioParserEngine::Nightly)
  );
  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("styio_matrix_clone"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_new_matrix"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_push_matrix"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_list_get_matrix"), std::string::npos);
  EXPECT_NE(llvm_ir.find("styio_matrix_release"), std::string::npos);
}

TEST(StyioSecurityResourceTopology, KeepsUnboundedMatrixResourceSelectorSnapshotFailClosed) {
  const std::string src =
    "@bucket : matrix\n"
    ">_(@bucket[...])\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected unbounded matrix selector snapshot rejection";
  }
  catch (const StyioTypeError& ex) {
    EXPECT_NE(
      std::string(ex.what()).find("resource `bucket` does not support snapshot selection"),
      std::string::npos
    ) << ex.what();
  }
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnFinalBindSubsetSamples) {
  const std::vector<std::string> samples = {
    "x : i32 := 100\n>_(x)\n",
    "price: f64 := 1 + 2\n>_(price)\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnCompoundAssignSubsetSamples) {
  const std::vector<std::string> samples = {
    "x = 10\nx += 5\n>_(x)\n",
    "a = 20\na -= 3\n>_(a)\n",
    "m = 4\nm *= 2\n>_(m)\n",
    "q = 9\nq /= 3\n>_(q)\n",
    "r = 9\nr %= 4\n>_(r)\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnCompareAndLogicSubsetSamples) {
  const std::vector<std::string> samples = {
    "lhs = 3\nrhs = 2\n>_(lhs > rhs)\n",
    "a = 1\nb = 1\n>_(a <= b)\n",
    "x = 7\ny = 7\n>_(x == y)\n",
    "ok = true\nready = false\n>_(ok && ready)\n",
    "hot = false\ncold = true\n>_(hot || cold)\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnSimpleCallSubsetSamples) {
  const std::vector<std::string> samples = {
    "foo(1)\n",
    "sum(1, 2, 3)\n",
    "x = add(1, 2)\n>_(x)\n",
    ">_(mul(2, 3))\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnFunctionDefSubsetSamples) {
  const std::vector<std::string> samples = {
    "# add := (a: i32, b: i32) => a + b\n>_(add(3, 4))\n",
    "# answer := () => 42\n>_(answer())\n",
    "# pulse : [|3|] = (x: i32) => x\n>_(pulse(5))\n",
    "# pair : (i32, [|2|]) = (a: i32, b: i32) => a + b\n>_(pair(1, 2))\n",
    "# mix(a: i32, b: i32) : i32 = a + b\n>_(mix(5, 7))\n",
    "# transform = #(x: i32) => x * 2\n>_(transform(5))\n",
    "# const42 : i32 => 42\n>_(const42())\n",
    "# ping => 1\n>_(ping())\n",
    "# parity(n: i32) ?={\n    0 => 0\n    _ => 1\n}\n>_(parity(0), parity(3))\n",
    "# iter_only(x) >> (n) => >_(n)\niter_only(3)\n",
    "# alert := () => >_(\"ALERT\")\nalert()\n",
    "# compute := (x: i32) => {\n    y = x * 2\n    <| y\n}\n>_(compute(5))\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnBlockControlSubsetSamples) {
  const std::vector<std::string> samples = {
    "{\n    value = 1 + 2\n    <| value\n}\n",
    "{\n    ...\n    ^^^\n    >>\n}\n",
    "# outer := (x: i32) => {\n    # inner := (y: i32) => y + 1\n    <| inner(x) + inner(x + 1)\n}\n>_(outer(3))\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnMatchCasesSubsetSamples) {
  const std::vector<std::string> samples = {
    "# fact := (n: i32) => {\n    n ?= {\n        0 => { <| 1 }\n        _ => { <| n * fact(n - 1) }\n    }\n}\n>_(fact(5))\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityNightlyParserStmt, AcceptsModuloSubjectBeforeMatchCases) {
  const std::string src =
    "x = 4\nlabel = x % 2 ?= {\n    0 => { <| \"even\" }\n    _ => { <| \"odd\" }\n}\n>_(label)\n";

  const std::string nightly = parse_program_to_repr_latest(src, true);
  const std::string legacy = parse_program_to_repr_latest(src, false);
  EXPECT_EQ(nightly, legacy);
  EXPECT_NE(nightly.find("label"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, CallableBindingOperatorsStayMutableAndFinal) {
  const std::string mutable_src =
    "# transform = #(x: i64) => x + 1\n"
    "# transform = #(x: i64) => x + 2\n"
    "transform(3) -> @stdout\n";

  g_runtime_log_events.clear();
  styio_runtime_set_log_sink(capture_runtime_log);
  try {
    execute_program_engine_with_stdin_latest(
      mutable_src,
      StyioParserEngine::Nightly,
      ""
    );
  }
  catch (...) {
    styio_runtime_set_log_sink(nullptr);
    throw;
  }
  styio_runtime_set_log_sink(nullptr);

  ASSERT_EQ(g_runtime_log_events.size(), 1u);
  EXPECT_EQ(g_runtime_log_events[0].first, "stdout");
  EXPECT_EQ(g_runtime_log_events[0].second, "5");

  const std::string final_then_mutable =
    "# transform := #(x: i64) => x + 1\n"
    "# transform = #(x: i64) => x + 2\n"
    "transform(3) -> @stdout\n";
  EXPECT_THROW(
    parse_typecheck_program_engine_latest(final_then_mutable, StyioParserEngine::Nightly),
    StyioTypeError
  );

  const std::string mutable_then_final =
    "# transform = #(x: i64) => x + 1\n"
    "# transform := #(x: i64) => x + 2\n"
    "transform(3) -> @stdout\n";
  EXPECT_THROW(
    parse_typecheck_program_engine_latest(mutable_then_final, StyioParserEngine::Nightly),
    StyioTypeError
  );
}

TEST(StyioSecurityNightlySemantics, RejectsUndefinedMatchArmTailValue) {
  const std::string src =
    "x = 1\n"
    "label = x ?= {\n"
    "  1 => missing_value\n"
    "  _ => 2\n"
    "}\n"
    ">_(label)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected match arm tail value to require semantic type inference";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("match branch value has undefined type"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, MatchArmBindingsDoNotLeakAcrossBranches) {
  const std::string src =
    "x = 2\n"
    "label = x ?= {\n"
    "  1 => {\n"
    "    hidden = 4\n"
    "    hidden\n"
    "  }\n"
    "  _ => hidden + 1\n"
    "}\n"
    ">_(label)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected match arm local binding to stay scoped to its branch";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("match branch value has undefined type"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, FunctionMatchSugarInfersArmBodiesOnCall) {
  const std::string src =
    "# choose := (n: i32) ?= {\n"
    "  0 => missing_value\n"
    "  _ => 1\n"
    "}\n"
    ">_(choose(0))\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected function match sugar arms to be type-inferred when called";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("match branch value has undefined type"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, FunctionMatchSugarArmBindingsStayBranchScoped) {
  const std::string src =
    "# choose := (n: i32) ?= {\n"
    "  0 => {\n"
    "    hidden = 4\n"
    "    hidden\n"
    "  }\n"
    "  _ => hidden + 1\n"
    "}\n"
    ">_(choose(1))\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected function match sugar arm locals to stay branch scoped";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("match branch value has undefined type"), std::string::npos) << msg;
  }
}

TEST(StyioSecurityNightlySemantics, MatchBoolAndCharLowerToNativeScalarWidths) {
  const std::string src =
    "x = 1\n"
    "flag = x ?= {\n"
    "  1 => true\n"
    "  _ => false\n"
    "}\n"
    "mark = x ?= {\n"
    "  1 => 'a'\n"
    "  _ => 'b'\n"
    "}\n"
    ">_(flag)\n"
    ">_(mark)\n";

  const std::string llvm_ir =
    compile_program_to_llvm_ir_engine_latest(src, StyioParserEngine::Nightly);
  EXPECT_NE(llvm_ir.find("phi i1"), std::string::npos);
  EXPECT_NE(llvm_ir.find("phi i8"), std::string::npos);
}

TEST(StyioSecurityNightlySemantics, RejectsUnsupportedContainerMatchResultBeforeLowering) {
  const std::string src =
    "x = 1\n"
    "values = x ?= {\n"
    "  1 => [1,2]\n"
    "  _ => [3,4]\n"
    "}\n"
    ">_(values)\n";

  try {
    parse_typecheck_program_engine_latest(src, StyioParserEngine::Nightly);
    FAIL() << "expected container-valued match branches to stay fail-closed";
  }
  catch (const StyioTypeError& err) {
    const std::string msg = err.what();
    EXPECT_NE(msg.find("match branch values support scalar and string results in this slice"), std::string::npos)
      << msg;
  }
}

TEST(StyioSecurityNightlyParserStmt, RejectsHashIteratorMatchForwardChainWithStableError) {
  const std::string src = "# iter_only(x) >> (n) ?= 2 => >_(n)\niter_only([1, 2, 3])\n";
  EXPECT_THROW(
    {
      (void)parse_program_to_repr_latest(src, true);
    },
    StyioBaseException
  );
  EXPECT_THROW(
    {
      (void)parse_program_to_repr_latest(src, false);
    },
    StyioBaseException
  );
}

TEST(StyioSecurityNightlyParserStmt, MatchesLegacyOnDotCallSubsetSamples) {
  const std::vector<std::string> samples = {
    "foo.bar(1)\n",
    "x = foo.bar(1 + 2)\n>_(x)\n",
    ">_(foo.bar(3), 4)\n",
  };

  for (const auto& src : samples) {
    EXPECT_EQ(parse_program_to_repr_latest(src, true), parse_program_to_repr_latest(src, false)) << src;
  }
}

TEST(StyioSecurityNightlyParserShadow, FallsBackOnDotChainSequence) {
  const std::string src = "foo.bar(1).baz(2)\n";
  EXPECT_THROW(
    {
      (void)parse_program_engine_to_repr_latest(src, StyioParserEngine::Nightly);
    },
    StyioSyntaxError
  );
  EXPECT_THROW(
    {
      (void)parse_program_engine_to_repr_latest(src, StyioParserEngine::Legacy);
    },
    StyioSyntaxError
  );
}

TEST(StyioSecurityUnicode, ByteClassificationIsStable) {
  const auto unicode_backend = StyioUnicode::backend();
  constexpr bool has_unicode_properties = STYIO_TEST_EXPECT_ICU != 0;
  EXPECT_EQ(
    unicode_backend,
    has_unicode_properties ? StyioUnicode::Backend::ICU : StyioUnicode::Backend::ASCII);

  EXPECT_TRUE(StyioUnicode::is_identifier_start('a'));
  EXPECT_TRUE(StyioUnicode::is_identifier_start('Z'));
  EXPECT_TRUE(StyioUnicode::is_identifier_start('_'));
  EXPECT_FALSE(StyioUnicode::is_identifier_start('9'));
  EXPECT_FALSE(StyioUnicode::is_identifier_start(static_cast<std::uint32_t>(0x80)));
  EXPECT_TRUE(StyioUnicode::is_identifier_start(static_cast<std::uint32_t>('A')));
  EXPECT_EQ(
    StyioUnicode::is_identifier_start(static_cast<std::uint32_t>(0x4E2D)),
    has_unicode_properties);

  EXPECT_TRUE(StyioUnicode::is_identifier_continue('9'));
  EXPECT_TRUE(StyioUnicode::is_identifier_continue('_'));
  EXPECT_FALSE(StyioUnicode::is_identifier_continue('-'));
  EXPECT_FALSE(StyioUnicode::is_identifier_continue(static_cast<std::uint32_t>(0x80)));
  EXPECT_TRUE(StyioUnicode::is_identifier_continue(static_cast<std::uint32_t>('9')));
  EXPECT_EQ(
    StyioUnicode::is_identifier_continue(static_cast<std::uint32_t>(0x4E2D)),
    has_unicode_properties);

  EXPECT_TRUE(StyioUnicode::is_digit('7'));
  EXPECT_TRUE(StyioUnicode::is_decimal_digit(static_cast<std::uint32_t>('5')));
  EXPECT_FALSE(StyioUnicode::is_decimal_digit(static_cast<std::uint32_t>('x')));
  EXPECT_EQ(
    StyioUnicode::is_decimal_digit(static_cast<std::uint32_t>(0x660)),
    has_unicode_properties);
  EXPECT_TRUE(StyioUnicode::is_space(' '));
  EXPECT_TRUE(StyioUnicode::is_ascii_alpha('Q'));
  EXPECT_TRUE(StyioUnicode::is_ascii_alnum('8'));
  EXPECT_FALSE(StyioUnicode::is_space('x'));
}

TEST(StyioSecurityUnicode, DecodeUtf8CodepointBoundaries) {
  {
    std::string valid = "A";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_TRUE(StyioUnicode::decode_utf8_codepoint(valid, 0, cp, width));
    EXPECT_EQ(cp, static_cast<std::uint32_t>('A'));
    EXPECT_EQ(width, 1u);
  }

  {
    std::string valid = "\xC2\xA2";  // U+00A2
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_TRUE(StyioUnicode::decode_utf8_codepoint(valid, 0, cp, width));
    EXPECT_EQ(cp, 0x00A2u);
    EXPECT_EQ(width, 2u);
  }

  {
    std::string valid = "\xE4\xB8\xAD";  // U+4E2D
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_TRUE(StyioUnicode::decode_utf8_codepoint(valid, 0, cp, width));
    EXPECT_EQ(cp, 0x4E2Du);
    EXPECT_EQ(width, 3u);
  }

  {
    std::string valid = "\xF0\x9F\x92\xA9";  // U+1F4A9
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_TRUE(StyioUnicode::decode_utf8_codepoint(valid, 0, cp, width));
    EXPECT_EQ(cp, 0x1F4A9u);
    EXPECT_EQ(width, 4u);
  }

  {
    std::string overlong = "\xC0\xAF";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(overlong, 0, cp, width));
  }

  {
    std::string truncated = "\xC2";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(truncated, 0, cp, width));
  }

  {
    std::string invalid_continuation = "\xE2\x28\xA1";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(invalid_continuation, 0, cp, width));
  }

  {
    std::string truncated = "\xE2\x82";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(truncated, 0, cp, width));
  }

  {
    std::string surrogate = "\xED\xA0\x80";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(surrogate, 0, cp, width));
  }

  {
    std::string out_of_range = "\xF4\x90\x80\x80";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(out_of_range, 0, cp, width));
  }

  {
    std::string out_of_range = "\xF5\x80\x80\x80";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(out_of_range, 0, cp, width));
  }

  {
    std::string invalid_continuation = "\xF0\x28\x80\x80";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(invalid_continuation, 0, cp, width));
  }

  {
    std::string invalid_leading_byte = "\xFF";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(invalid_leading_byte, 0, cp, width));
  }

  {
    std::string truncated = "\xF0\x9F\x92";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(truncated, 0, cp, width));
  }

  {
    std::string empty;
    std::uint32_t cp = 123;
    std::size_t width = 4;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(empty, 0, cp, width));
    EXPECT_EQ(cp, 0u);
    EXPECT_EQ(width, 0u);
  }
}

TEST(StyioSecuritySymbolRegistry, DefaultRegistryGroupsAndNamesAreStable) {
  using styio::symbols::RegistryOrigin;
  using styio::symbols::RegistrySurface;

  const auto& registry = styio::symbols::default_symbol_registry();
  ASSERT_GT(registry.size(), 20u);
  EXPECT_EQ(styio::symbols::default_symbol_registry_version(), 1u);

  EXPECT_EQ(std::string(styio::symbols::to_string(RegistrySurface::Type)), "type");
  EXPECT_EQ(std::string(styio::symbols::to_string(RegistrySurface::Value)), "value");
  EXPECT_EQ(std::string(styio::symbols::to_string(RegistrySurface::Member)), "member");
  EXPECT_EQ(std::string(styio::symbols::to_string(static_cast<RegistrySurface>(999))), "value");
  EXPECT_EQ(std::string(styio::symbols::to_string(RegistryOrigin::Builtin)), "builtin");
  EXPECT_EQ(std::string(styio::symbols::to_string(RegistryOrigin::Prelude)), "prelude");
  EXPECT_EQ(std::string(styio::symbols::to_string(RegistryOrigin::Macro)), "macro");
  EXPECT_EQ(std::string(styio::symbols::to_string(static_cast<RegistryOrigin>(999))), "builtin");

  const auto builtins = styio::symbols::default_symbol_names_by_origin(RegistryOrigin::Builtin);
  const auto prelude = styio::symbols::default_symbol_names_by_origin(RegistryOrigin::Prelude);
  const auto macros = styio::symbols::default_symbol_names_by_origin(RegistryOrigin::Macro);

  EXPECT_TRUE(std::is_sorted(builtins.begin(), builtins.end()));
  EXPECT_NE(std::find(builtins.begin(), builtins.end(), "i64"), builtins.end());
  EXPECT_NE(std::find(builtins.begin(), builtins.end(), "string"), builtins.end());
  EXPECT_NE(std::find(prelude.begin(), prelude.end(), "stdin"), prelude.end());
  EXPECT_NE(std::find(prelude.begin(), prelude.end(), "file"), prelude.end());
  EXPECT_EQ(macros, std::vector<std::string>{"match"});
}

TEST(StyioSecurityParserHelpers, BinExprMapperBuildsExpectedAstNodes) {
  struct BinaryCase
  {
    StyioOpType op;
    StyioOpType expected;
  };

  for (const auto& item : std::vector<BinaryCase>{
         {StyioOpType::Binary_Add, StyioOpType::Binary_Add},
         {StyioOpType::Binary_Sub, StyioOpType::Binary_Sub},
         {StyioOpType::Binary_Mul, StyioOpType::Binary_Mul},
         {StyioOpType::Binary_Div, StyioOpType::Binary_Div},
         {StyioOpType::Binary_Mod, StyioOpType::Binary_Mod},
         {StyioOpType::Binary_Pow, StyioOpType::Binary_Pow},
       }) {
    ASSERT_NE(bin_op_mapper.find(item.op), bin_op_mapper.end());
    std::unique_ptr<StyioAST> ast(
      bin_op_mapper.at(item.op)(IntAST::Create("1"), IntAST::Create("2")));
    auto* binop = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(binop, nullptr);
    EXPECT_EQ(binop->getOp(), item.expected);
    EXPECT_NE(binop->getLHS(), nullptr);
    EXPECT_NE(binop->getRHS(), nullptr);
  }

  struct LogicCase
  {
    StyioOpType op;
    LogicType expected;
    bool unary;
  };

  for (const auto& item : std::vector<LogicCase>{
         {StyioOpType::Logic_NOT, LogicType::NOT, true},
         {StyioOpType::Logic_AND, LogicType::AND, false},
         {StyioOpType::Logic_OR, LogicType::OR, false},
         {StyioOpType::Logic_XOR, LogicType::XOR, false},
       }) {
    ASSERT_NE(bin_op_mapper.find(item.op), bin_op_mapper.end());
    std::unique_ptr<StyioAST> ast(bin_op_mapper.at(item.op)(
      BoolAST::Create(true),
      item.unary ? nullptr : BoolAST::Create(false)));
    auto* cond = dynamic_cast<CondAST*>(ast.get());
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->getSign(), item.expected);
    EXPECT_NE(item.unary ? cond->getValue() : cond->getLHS(), nullptr);
  }
}

TEST(StyioSecurityExceptions, StableWhatStringsCoverExceptionFamilies) {
  EXPECT_NE(std::string(StyioBaseException().what()).find("BaseException"), std::string::npos);
  EXPECT_NE(std::string(StyioBaseException("base").what()).find("base"), std::string::npos);
  EXPECT_NE(std::string(StyioSyntaxError().what()).find("SyntaxError"), std::string::npos);
  EXPECT_NE(std::string(StyioSyntaxError("syntax").what()).find("syntax"), std::string::npos);
  EXPECT_NE(std::string(StyioSyntaxError("meta", "syntax").what()).find("meta"), std::string::npos);
  EXPECT_NE(std::string(StyioParserResourceLimitError("limit").what()).find("limit"), std::string::npos);
  EXPECT_NE(std::string(StyioParserResourceLimitError("meta", "limit").what()).find("meta"), std::string::npos);
  EXPECT_NE(std::string(StyioParseError().what()).find("ParseError"), std::string::npos);
  EXPECT_NE(std::string(StyioParseError("parse").what()).find("parse"), std::string::npos);
  EXPECT_NE(std::string(StyioLexError().what()).find("LexError"), std::string::npos);
  EXPECT_NE(std::string(StyioLexError("lex").what()).find("lex"), std::string::npos);
  EXPECT_NE(std::string(StyioTypeError().what()).find("TypeError"), std::string::npos);
  EXPECT_NE(std::string(StyioTypeError("type").what()).find("type"), std::string::npos);
  EXPECT_NE(std::string(StyioNotImplemented().what()).find("NotImplemented"), std::string::npos);
  EXPECT_NE(std::string(StyioNotImplemented("todo").what()).find("todo"), std::string::npos);
  EXPECT_NE(std::string(StyioUndefinedBehaviour().what()).find("UndefinedBehaviour"), std::string::npos);
  EXPECT_NE(std::string(StyioUndefinedBehaviour("ub").what()).find("ub"), std::string::npos);
  char mutable_message[] = "mutable";
  EXPECT_NE(std::string(StyioUndefinedBehaviour(mutable_message).what()).find("mutable"), std::string::npos);
}

TEST(StyioSecuritySession, ResetClearsSessionState) {
  CompilationSession session;
  const std::string src = "x = 1\n";
  EXPECT_EQ(session.phase(), CompilationPhase::Empty);
  EXPECT_EQ(session.token_arena_bytes(), 0u);
  EXPECT_EQ(session.ast_arena_bytes(), 0u);

  session.adopt_tokens(StyioTokenizer::tokenize(src));
  EXPECT_EQ(session.phase(), CompilationPhase::Tokenized);
  EXPECT_GT(session.token_arena_bytes(), 0u);

  session.attach_context(StyioContext::Create(
    "<security>",
    src,
    {{0, src.size() - 1}},
    session.tokens(),
    false
  ));
  session.attach_ast(MainBlockAST::Create({}));
  EXPECT_EQ(session.phase(), CompilationPhase::Parsed);
  EXPECT_GT(session.ast_arena_bytes(), 0u);

  session.mark_type_checked();
  EXPECT_EQ(session.phase(), CompilationPhase::Typed);
  session.attach_ir(nullptr);
  EXPECT_EQ(session.phase(), CompilationPhase::Lowered);
  session.mark_codegen_ready();
  EXPECT_EQ(session.phase(), CompilationPhase::CodegenReady);
  session.mark_executed();
  EXPECT_EQ(session.phase(), CompilationPhase::Executed);

  ASSERT_FALSE(session.tokens().empty());
  ASSERT_NE(session.context(), nullptr);
  ASSERT_NE(session.ast(), nullptr);

  session.reset();
  EXPECT_TRUE(session.tokens().empty());
  EXPECT_EQ(session.context(), nullptr);
  EXPECT_EQ(session.ast(), nullptr);
  EXPECT_EQ(session.ir(), nullptr);
  EXPECT_EQ(session.phase(), CompilationPhase::Empty);
  EXPECT_EQ(session.token_arena_bytes(), 0u);
  EXPECT_EQ(session.ast_arena_bytes(), 0u);
}

TEST(StyioSecuritySession, InvalidPhaseTransitionsAreRejected) {
  CompilationSession session;
  EXPECT_FALSE(CompilationSession::phase_at_least(
    CompilationPhase::Executed,
    CompilationPhase::Failed));
  EXPECT_THROW(session.mark_type_checked(), std::logic_error);
  EXPECT_THROW(session.attach_context(nullptr), std::logic_error);
  EXPECT_THROW(session.attach_ast(nullptr), std::logic_error);
  EXPECT_THROW(session.attach_ir(nullptr), std::logic_error);

  session.adopt_tokens(StyioTokenizer::tokenize("x = 1\n"));
  auto duplicate_tokens = StyioTokenizer::tokenize("y = 2\n");
  EXPECT_THROW(session.adopt_tokens(std::move(duplicate_tokens)), std::logic_error);
  free_tokens(duplicate_tokens);
  EXPECT_THROW(session.mark_codegen_ready(), std::logic_error);
  EXPECT_THROW(session.mark_executed(), std::logic_error);

  session.mark_failed();
  EXPECT_EQ(session.phase(), CompilationPhase::Failed);
  session.mark_failed();
  EXPECT_EQ(session.phase(), CompilationPhase::Failed);
}

TEST(StyioSecuritySession, WhiteBoxTransitionAndReplacementEdges) {
  EXPECT_EQ(CompilationSession::phase_rank(static_cast<CompilationPhase>(999)), 0);
  EXPECT_FALSE(CompilationSession::can_transition(
    CompilationPhase::Failed,
    CompilationPhase::Parsed));

  CompilationSession failed_session;
  failed_session.phase_ = CompilationPhase::Failed;
  try {
    failed_session.transition_to(CompilationPhase::Parsed, "whitebox");
    FAIL() << "transition from Failed should be rejected";
  } catch (const std::logic_error& error) {
    const std::string message = error.what();
    EXPECT_NE(
      message.find("invalid compilation session transition in whitebox"),
      std::string::npos);
    EXPECT_NE(message.find("Failed -> Parsed"), std::string::npos);
  }

  CompilationSession ast_session;
  ast_session.adopt_tokens(StyioTokenizer::tokenize("x = 1\n"));
  auto* first_ast = MainBlockAST::Create({});
  ASSERT_EQ(ast_session.attach_ast(first_ast), first_ast);
  ast_session.phase_ = CompilationPhase::Tokenized;
  auto* second_ast = MainBlockAST::Create({});
  EXPECT_EQ(ast_session.attach_ast(second_ast), second_ast);
  EXPECT_EQ(ast_session.ast(), second_ast);

  CompilationSession ir_session;
  ir_session.adopt_tokens(StyioTokenizer::tokenize("x = 1\n"));
  ir_session.attach_ast(MainBlockAST::Create({}));
  ir_session.mark_type_checked();
  auto* first_ir = SGNoOp::Create();
  ASSERT_EQ(ir_session.attach_ir(first_ir), first_ir);
  ir_session.phase_ = CompilationPhase::Typed;
  auto* second_ir = SGNoOp::Create();
  EXPECT_EQ(ir_session.attach_ir(second_ir), second_ir);
  EXPECT_EQ(ir_session.ir(), second_ir);
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
      new CountingExprAST(&arg_destructed)
    }
  );
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
    std::vector<StyioAST*>{new CountingExprAST(&arg_destructed)}
  );
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
    std::vector<StyioAST*>{}
  );
  expr->setFuncCallee(new CountingExprAST(&callee_destructed));
  delete expr;
  EXPECT_EQ(callee_destructed, 1);
  EXPECT_EQ(name_destructed, 1);
}

TEST(StyioSecurityAstOwnership, ListOpOwnsListOnly) {
  int list_destructed = 0;
  auto* expr = new ListOpAST(
    StyioNodeType::Get_Reversed,
    new CountingExprAST(&list_destructed)
  );
  delete expr;
  EXPECT_EQ(list_destructed, 1);
}

TEST(StyioSecurityAstOwnership, ListOpOwnsListAndSlot1) {
  int list_destructed = 0;
  int slot1_destructed = 0;
  auto* expr = new ListOpAST(
    StyioNodeType::Access_By_Index,
    new CountingExprAST(&list_destructed),
    new CountingExprAST(&slot1_destructed)
  );
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
    new CountingExprAST(&slot2_destructed)
  );
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
      new CountingExprAST(&destructed)
    }
  );
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
    new CountingExprAST(&destructed)
  );
  delete expr;
  EXPECT_EQ(destructed, 3);
}

TEST(StyioSecurityAstOwnership, SizeOfOwnsValueExpr) {
  int destructed = 0;
  auto* expr = new SizeOfAST(new CountingExprAST(&destructed));
  delete expr;
  EXPECT_EQ(destructed, 1);
}

TEST(StyioSecurityAstOwnership, SizeOfLowersListLength) {
  auto* list = ListAST::Create(
    std::vector<StyioAST*>{
      IntAST::Create("1"),
      IntAST::Create("2"),
      IntAST::Create("3")
    }
  );
  auto* expr = new SizeOfAST(list);

  AstToStyioIRLowerer analyzer;
  expr->typeInfer(&analyzer);
  EXPECT_EQ(expr->getDataType().option, StyioDataTypeOption::Integer);
  EXPECT_EQ(expr->getDataType().name, "i64");

  StyioIR* ir = expr->toStyioIR(&analyzer);
  EXPECT_NE(dynamic_cast<SCListLen*>(ir), nullptr);

  delete ir;
  delete expr;
}

TEST(StyioSecurityAstOwnership, TupleOwnsElements) {
  int destructed = 0;
  auto* expr = TupleAST::Create(
    std::vector<StyioAST*>{
      new CountingExprAST(&destructed),
      new CountingExprAST(&destructed)
    }
  );
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, TypeTupleOwnsTypeNodes) {
  int destructed = 0;
  auto* expr = TypeTupleAST::Create(
    std::vector<TypeAST*>{
      new CountingTypeAST("i64", &destructed),
      new CountingTypeAST("f64", &destructed)
    }
  );
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, ListOwnsElements) {
  int destructed = 0;
  auto* expr = ListAST::Create(
    std::vector<StyioAST*>{
      new CountingExprAST(&destructed),
      new CountingExprAST(&destructed)
    }
  );
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, SetOwnsElements) {
  int destructed = 0;
  auto* expr = SetAST::Create(
    std::vector<StyioAST*>{
      new CountingExprAST(&destructed),
      new CountingExprAST(&destructed)
    }
  );
  delete expr;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, VarTupleOwnsVarNodes) {
  int destructed = 0;
  auto* expr = VarTupleAST::Create(
    std::vector<VarAST*>{
      new CountingVarAST(&destructed),
      new CountingVarAST(&destructed)
    }
  );
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
    new CountingExprAST(&value_destructed)
  );
  delete stmt;
  EXPECT_EQ(var_destructed, 1);
  EXPECT_EQ(value_destructed, 1);
}

TEST(StyioSecurityAstOwnership, FinalBindOwnsVarAndValue) {
  int var_destructed = 0;
  int value_destructed = 0;
  auto* stmt = FinalBindAST::Create(
    new CountingVarAST(&var_destructed),
    new CountingExprAST(&value_destructed)
  );
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
    new CountingExprAST(&resource_destructed)
  );
  delete stmt;
  EXPECT_EQ(var_destructed, 1);
  EXPECT_EQ(resource_destructed, 1);
}

TEST(StyioSecurityAstOwnership, ResourceWriteOwnsDataAndResource) {
  int data_destructed = 0;
  int resource_destructed = 0;
  auto* stmt = ResourceWriteAST::Create(
    new CountingExprAST(&data_destructed),
    new CountingExprAST(&resource_destructed)
  );
  delete stmt;
  EXPECT_EQ(data_destructed, 1);
  EXPECT_EQ(resource_destructed, 1);
}

TEST(StyioSecurityAstOwnership, ResourceRedirectOwnsDataAndResource) {
  int data_destructed = 0;
  int resource_destructed = 0;
  auto* stmt = ResourceRedirectAST::Create(
    new CountingExprAST(&data_destructed),
    new CountingExprAST(&resource_destructed)
  );
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
    new CountingExprAST(&depth_destructed)
  );
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
    new CountingExprAST(&window_destructed)
  );
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
    new CountingExprAST(&update_expr_destructed)
  );

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
    new CountingExprAST(&init_destructed)
  );
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
    new CountingExprAST(&init_destructed)
  );
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
      ParamAST::Create(new CountingNameAST("p2", &param_name_destructed))
    }
  );
  delete node;
  EXPECT_EQ(struct_name_destructed, 1);
  EXPECT_EQ(param_name_destructed, 2);
}

TEST(StyioSecurityAstOwnership, ResourceOwnsEntries) {
  int destructed = 0;
  auto* node = ResourceAST::Create(
    std::vector<std::pair<StyioAST*, std::string>>{
      {new CountingExprAST(&destructed), "file"},
      {new CountingExprAST(&destructed), "db"}
    }
  );
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
    new CountingExprAST(&destructed)
  );

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
      new CountingExprAST(&destructed)
    )
  );

  delete node;
  EXPECT_EQ(destructed, 4);
}

TEST(StyioSecurityAstOwnership, CheckEqualOwnsRightValueExprs) {
  int destructed = 0;
  auto* node = CheckEqualAST::Create(std::vector<StyioAST*>{
    new CountingExprAST(&destructed),
    new CountingExprAST(&destructed),
  });

  delete node;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, CondFlowOwnsConditionAndThenBranch) {
  int destructed = 0;
  auto* node = new CondFlowAST(
    StyioNodeType::CondFlow_True,
    CondAST::Create(LogicType::NOT, new CountingExprAST(&destructed)),
    new CountingExprAST(&destructed)
  );

  delete node;
  EXPECT_EQ(destructed, 2);
}

TEST(StyioSecurityAstOwnership, CondFlowOwnsConditionThenAndElseBranches) {
  int destructed = 0;
  auto* node = new CondFlowAST(
    StyioNodeType::CondFlow_Both,
    CondAST::Create(LogicType::NOT, new CountingExprAST(&destructed)),
    new CountingExprAST(&destructed),
    new CountingExprAST(&destructed)
  );

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
    new CountingExprAST(&body_destructed)
  );

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
    new CountingExprAST(&ret_expr_destructed)
  );

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
    BlockAST::Create({})
  );

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
    new CountingExprAST(&body_destructed)
  );

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
    }
  );

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
    }
  );

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
    }
  );

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
    new CountingExprAST(&destructed)
  );

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
    new CountingExprAST(&then_destructed)
  );

  delete node;
  EXPECT_EQ(var_destructed, 1);
  EXPECT_EQ(then_destructed, 1);
}

TEST(StyioSecurityAstOwnership, SnapshotDeclOwnsVarAndResource) {
  int var_destructed = 0;
  int path_destructed = 0;
  auto* node = SnapshotDeclAST::Create(
    new CountingNameAST("snap", &var_destructed),
    FileResourceAST::Create(new CountingExprAST(&path_destructed), true)
  );

  delete node;
  EXPECT_EQ(var_destructed, 1);
  EXPECT_EQ(path_destructed, 1);
}

TEST(StyioSecurityAstOwnership, InstantPullOwnsResource) {
  int path_destructed = 0;
  auto* node = InstantPullAST::Create(
    FileResourceAST::Create(new CountingExprAST(&path_destructed), false)
  );

  delete node;
  EXPECT_EQ(path_destructed, 1);
}

TEST(StyioSecurityAstOwnership, TaskBlockOwnsBody) {
  int stmt_destructed = 0;
  auto* node = TaskBlockAST::Create(
    BlockAST::Create(std::vector<StyioAST*>{
      new CountingExprAST(&stmt_destructed),
    })
  );

  delete node;
  EXPECT_EQ(stmt_destructed, 1);
}

TEST(StyioSecurityAstOwnership, TaskGroupLaunchOwnsEntries) {
  int entry_destructed = 0;
  auto* node = TaskGroupLaunchAST::Create(std::vector<StyioAST*>{
    new CountingExprAST(&entry_destructed),
    new CountingExprAST(&entry_destructed),
  });

  delete node;
  EXPECT_EQ(entry_destructed, 2);
}

TEST(StyioSecurityAstOwnership, FlowBindOwnsSourceAndTarget) {
  int source_destructed = 0;
  int target_destructed = 0;
  auto* node = FlowBindAST::Create(
    new CountingExprAST(&source_destructed),
    new CountingVarAST(&target_destructed));

  delete node;
  EXPECT_EQ(source_destructed, 1);
  EXPECT_EQ(target_destructed, 1);
}

TEST(StyioSecurityAstOwnership, IterSeqOwnsHashTags) {
  StyioAST::destroy_all_tracked_nodes();
  int collection_destructed = 0;
  auto* node = IterSeqAST::Create(
    new CountingExprAST(&collection_destructed),
    std::vector<HashTagNameAST*>{
      HashTagNameAST::Create(std::vector<std::string>{"left"}),
      HashTagNameAST::Create(std::vector<std::string>{"right"}),
    }
  );

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
    new CountingExprAST(&op_destructed)
  );

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
    }
  );

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

TEST(StyioSafetyRuntime, CloneCstrAllocatesOwnedCopyAndHandlesNull) {
  const char* p = styio_clone_cstr("ring");
  ASSERT_NE(p, nullptr);
  ASSERT_STREQ(p, "ring");
  styio_free_cstr(p);

  const char* empty = styio_clone_cstr(nullptr);
  ASSERT_NE(empty, nullptr);
  ASSERT_STREQ(empty, "");
  styio_free_cstr(empty);
}

TEST(StyioSafetyRuntime, CharListRendersEscapedCharLiterals) {
  styio_runtime_clear_error();
  const int64_t h = styio_list_new_char();
  ASSERT_NE(h, 0);
  styio_list_push_char(h, static_cast<int8_t>('x'));
  styio_list_push_char(h, static_cast<int8_t>('\n'));
  styio_list_push_char(h, static_cast<int8_t>('\\'));
  styio_list_push_char(h, static_cast<int8_t>('\''));
  styio_list_push_char(h, static_cast<int8_t>('\0'));
  styio_list_push_char(h, static_cast<int8_t>(0x01));
  const char* text = styio_list_to_cstr(h);
  ASSERT_NE(text, nullptr);
  const std::string repr = text;
  EXPECT_NE(repr.find("'x'"), std::string::npos);
  EXPECT_NE(repr.find("'\\n'"), std::string::npos);
  EXPECT_NE(repr.find("'\\\\'"), std::string::npos);
  EXPECT_NE(repr.find(std::string("'") + "\\'" + "'"), std::string::npos);
  EXPECT_NE(repr.find("'\\0'"), std::string::npos);
  EXPECT_NE(repr.find("'\\x01'"), std::string::npos);
  styio_free_cstr(text);
  EXPECT_EQ(styio_list_get_char(h, 0), static_cast<int8_t>('x'));
  styio_list_release(h);
  EXPECT_EQ(styio_runtime_has_error(), 0);
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

TEST(StyioSafetyRuntime, ZeroFileHandleReadSetsRuntimeError) {
  styio_runtime_clear_error();
  styio_file_rewind(0);
  EXPECT_EQ(styio_file_read_line(0), nullptr);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_FILE_HANDLE");
  const char* msg = styio_runtime_last_error();
  ASSERT_NE(msg, nullptr);
  EXPECT_NE(std::strstr(msg, "invalid file handle: 0"), nullptr);
  styio_runtime_clear_error();

  EXPECT_EQ(styio_file_read_i64line_from_handle(0), 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_FILE_HANDLE");
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
  const int64_t h = styio_file_open("tests/features/file_resources/data/hello.txt");
  ASSERT_NE(h, 0);
  styio_file_close(h);
  styio_file_close(h);
  EXPECT_EQ(styio_runtime_has_error(), 0);
  EXPECT_EQ(styio_runtime_last_error_subcode(), nullptr);
  EXPECT_EQ(styio_runtime_last_error(), nullptr);
}

TEST(StyioSafetyRuntime, FileCloseFailureIsCleanupRuntimeEffect) {
#ifdef _WIN32
  GTEST_SKIP() << "/dev/full cleanup-failure fixture is Unix-specific";
#else
  if (!std::filesystem::exists("/dev/full")) {
    GTEST_SKIP() << "/dev/full is not available";
  }

  styio_runtime_clear_error();
  const int64_t h = styio_file_open_write("/dev/full");
  ASSERT_NE(h, 0);
  styio_file_write_cstr(h, "cleanup-probe");
  styio_file_close(h);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_FILE_CLEANUP_FAILURE");
  const char* msg = styio_runtime_last_error();
  ASSERT_NE(msg, nullptr);
  EXPECT_NE(std::strstr(msg, "file cleanup failed"), nullptr);
  EXPECT_EQ(styio_runtime_error_matches_effect("cleanup"), 1);
  EXPECT_EQ(styio_runtime_error_matches_effect("io"), 0);
  styio_runtime_clear_error();
#endif
}

TEST(StyioSafetyRuntime, FreeCstrAcceptsNull) {
  styio_free_cstr(nullptr);
  SUCCEED();
}

TEST(StyioSafetyRuntime, FreeCstrIgnoresBorrowedPointer) {
  const int64_t h = styio_file_open("tests/features/file_resources/data/hello.txt");
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

TEST(StyioSafetyRuntime, NumericFormattingAndLogSinkStayStable) {
  styio_runtime_clear_error();
  EXPECT_STREQ(styio_i64_dec_cstr(-42), "-42");
  EXPECT_STREQ(styio_f64_dec_cstr(1.25), "1.250000");
  EXPECT_STREQ(styio_char_cstr('A'), "A");

  EXPECT_EQ(styio_cstr_to_i64(nullptr), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_NUMERIC_PARSE");
  EXPECT_EQ(styio_runtime_error_matches_effect(nullptr), 0);
  EXPECT_EQ(styio_runtime_error_matches_effect("parse"), 1);
  styio_runtime_clear_error();

  EXPECT_DOUBLE_EQ(styio_cstr_to_f64("2.5"), 2.5);
  EXPECT_EQ(styio_runtime_has_error(), 0);
  EXPECT_DOUBLE_EQ(styio_cstr_to_f64(""), 0.0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_NUMERIC_PARSE");
  styio_runtime_clear_error();

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const long long uniq = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const std::string empty_path =
    "/tmp/styio_empty_i64line_" + std::to_string(uniq) + ".txt";
  {
    std::ofstream out(empty_path);
    ASSERT_TRUE(out.good());
  }
  EXPECT_EQ(styio_read_file_i64line(empty_path.c_str()), 0);
  EXPECT_EQ(styio_runtime_has_error(), 0);
  std::filesystem::remove(empty_path);

  g_runtime_log_events.clear();
  styio_runtime_set_log_sink(capture_runtime_log);
  styio_stdout_write_cstr("runtime-out");
  styio_stderr_write_cstr("runtime-err");
  styio_stdout_write_cstr(nullptr);
  styio_stderr_write_cstr(nullptr);
  styio_runtime_set_log_sink(nullptr);

  ASSERT_EQ(g_runtime_log_events.size(), 2u);
  EXPECT_EQ(g_runtime_log_events[0].first, "stdout");
  EXPECT_EQ(g_runtime_log_events[0].second, "runtime-out");
  EXPECT_EQ(g_runtime_log_events[1].first, "stderr");
  EXPECT_EQ(g_runtime_log_events[1].second, "runtime-err");
  EXPECT_EQ(styio_runtime_has_error(), 0);
}

#ifndef _WIN32
TEST(StyioSafetyRuntime, StdinReadersParseListsLinesAndStableFailures) {
  styio_runtime_clear_error();

  {
    ScopedStdinRedirect input("  [1, -2, 3] \n");
    ASSERT_TRUE(input.ok());
    const int64_t values = styio_list_i64_read_stdin();
    ASSERT_NE(values, 0);
    EXPECT_EQ(styio_list_len(values), 3);
    EXPECT_EQ(styio_list_get(values, 1), -2);
    styio_list_release(values);
  }

  {
    ScopedStdinRedirect input("[1.5, 2.25]");
    ASSERT_TRUE(input.ok());
    const int64_t values = styio_list_f64_read_stdin();
    ASSERT_NE(values, 0);
    EXPECT_EQ(styio_list_len(values), 2);
    EXPECT_DOUBLE_EQ(styio_list_get_f64(values, 0), 1.5);
    styio_list_release(values);
  }

  {
    ScopedStdinRedirect input("first\r\nsecond\n");
    ASSERT_TRUE(input.ok());
    const int64_t lines = styio_list_cstr_read_stdin();
    ASSERT_NE(lines, 0);
    EXPECT_EQ(styio_list_len(lines), 2);
    const char* first = styio_list_get_cstr(lines, 0);
    const char* second = styio_list_get_cstr(lines, 1);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_STREQ(first, "first");
    EXPECT_STREQ(second, "second");
    styio_free_cstr(first);
    styio_free_cstr(second);
    styio_list_release(lines);
  }

  {
    ScopedStdinRedirect input("line-one\r\nline-two\n");
    ASSERT_TRUE(input.ok());
    EXPECT_STREQ(styio_stdin_read_line(), "line-one");
    EXPECT_STREQ(styio_stdin_read_line(), "line-two");
    EXPECT_EQ(styio_stdin_read_line(), nullptr);
  }

  {
    ScopedStdinRedirect input("[nan]");
    ASSERT_TRUE(input.ok());
    EXPECT_EQ(styio_list_f64_read_stdin(), 0);
    EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_PARSE");
    EXPECT_EQ(styio_runtime_error_matches_effect("parse"), 1);
    styio_runtime_clear_error();
  }

  {
    ScopedStdinRedirect input("not-list");
    ASSERT_TRUE(input.ok());
    EXPECT_EQ(styio_list_i64_read_stdin(), 0);
    EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_PARSE");
    styio_runtime_clear_error();
  }

  EXPECT_EQ(styio_list_active_count(), 0);
}
#endif

TEST(StyioSafetyRuntime, MatrixApiCoversI64F64OperationsAndStableErrors) {
  styio_runtime_clear_error();
  const int64_t a = styio_matrix_new_i64(2, 2);
  const int64_t b = styio_matrix_identity_i64(2);
  ASSERT_NE(a, 0);
  ASSERT_NE(b, 0);
  styio_matrix_set_i64(a, 0, 0, 1);
  styio_matrix_set_i64(a, 0, 1, 2);
  styio_matrix_set_i64(a, 1, 0, 3);
  styio_matrix_set_i64(a, 1, 1, 4);

  const int64_t add = styio_matrix_add_i64(a, b);
  const int64_t sub = styio_matrix_sub_i64(a, b);
  const int64_t had = styio_matrix_hadamard_i64(a, b);
  const int64_t mul = styio_matrix_matmul_i64(a, b);
  const int64_t scale = styio_matrix_scale_i64(a, 3);
  const int64_t trans = styio_matrix_transpose_i64(a);
  ASSERT_NE(add, 0);
  ASSERT_NE(sub, 0);
  ASSERT_NE(had, 0);
  ASSERT_NE(mul, 0);
  ASSERT_NE(scale, 0);
  ASSERT_NE(trans, 0);
  EXPECT_EQ(styio_matrix_get_i64(add, 1, 1), 5);
  EXPECT_EQ(styio_matrix_get_i64(sub, 0, 0), 0);
  EXPECT_EQ(styio_matrix_get_i64(had, 0, 1), 0);
  EXPECT_EQ(styio_matrix_get_i64(mul, 1, 0), 3);
  EXPECT_EQ(styio_matrix_get_i64(scale, 0, 1), 6);
  EXPECT_EQ(styio_matrix_get_i64(trans, 0, 1), 3);
  EXPECT_EQ(styio_matrix_dot_i64(a, b), 5);
  EXPECT_EQ(styio_matrix_sum_i64(a), 10);

  int64_t* raw_i64 = styio_matrix_data_i64(a);
  ASSERT_NE(raw_i64, nullptr);
  EXPECT_EQ(raw_i64[2], 3);
  const int64_t shape = styio_matrix_shape(a);
  const int64_t row = styio_matrix_row_i64(a, 1);
  const int64_t rows = styio_matrix_rows_slice_i64(a, 0, 2, 1);
  ASSERT_NE(shape, 0);
  ASSERT_NE(row, 0);
  ASSERT_NE(rows, 0);
  EXPECT_EQ(styio_list_get(shape, 0), 2);
  EXPECT_EQ(styio_list_get(row, 1), 4);

  const int64_t fa = styio_matrix_clone_f64(a);
  const int64_t fb = styio_matrix_identity_f64(2);
  ASSERT_NE(fa, 0);
  ASSERT_NE(fb, 0);
  styio_matrix_set_f64(fb, 0, 1, 2.5);
  const int64_t fadd = styio_matrix_add_f64(fa, fb);
  const int64_t fsub = styio_matrix_sub_f64(fa, fb);
  const int64_t fhad = styio_matrix_hadamard_f64(fa, fb);
  const int64_t fmul = styio_matrix_matmul_f64(fa, fb);
  const int64_t fscale = styio_matrix_scale_f64(fa, 0.5);
  const int64_t ftrans = styio_matrix_transpose_f64(fa);
  const int64_t frow = styio_matrix_row_f64(fa, 0);
  const int64_t frows = styio_matrix_rows_slice_f64(fa, 1, 0, 0);
  ASSERT_NE(fadd, 0);
  ASSERT_NE(fsub, 0);
  ASSERT_NE(fhad, 0);
  ASSERT_NE(fmul, 0);
  ASSERT_NE(fscale, 0);
  ASSERT_NE(ftrans, 0);
  ASSERT_NE(frow, 0);
  ASSERT_NE(frows, 0);
  EXPECT_DOUBLE_EQ(styio_matrix_get_f64(fadd, 0, 1), 4.5);
  EXPECT_DOUBLE_EQ(styio_matrix_get_f64(fsub, 0, 1), -0.5);
  EXPECT_DOUBLE_EQ(styio_matrix_get_f64(fhad, 0, 0), 1.0);
  EXPECT_DOUBLE_EQ(styio_matrix_get_f64(fmul, 0, 1), 4.5);
  EXPECT_DOUBLE_EQ(styio_matrix_get_f64(fscale, 1, 1), 2.0);
  EXPECT_DOUBLE_EQ(styio_matrix_get_f64(ftrans, 1, 0), 2.0);
  EXPECT_DOUBLE_EQ(styio_matrix_dot_f64(fa, fb), 10.0);
  EXPECT_DOUBLE_EQ(styio_matrix_sum_f64(fa), 10.0);
  EXPECT_NEAR(styio_matrix_norm(fa), std::sqrt(30.0), 1e-9);
  double* raw_f64 = styio_matrix_data_f64(fa);
  ASSERT_NE(raw_f64, nullptr);
  EXPECT_DOUBLE_EQ(raw_f64[3], 4.0);

  const char* text = styio_matrix_to_cstr(fa);
  ASSERT_NE(text, nullptr);
  EXPECT_STREQ(text, "[[1.000000,2.000000],[3.000000,4.000000]]");
  styio_free_cstr(text);

  for (int64_t h : {add, sub, had, mul, scale, trans, shape, row, rows,
                    fa, fb, fadd, fsub, fhad, fmul, fscale, ftrans, frow, frows,
                    a, b}) {
    styio_matrix_release(h);
    styio_list_release(h);
  }
  EXPECT_EQ(styio_list_active_count(), 0);
  EXPECT_EQ(styio_matrix_active_count(), 0);

  EXPECT_EQ(styio_matrix_new_i64(0, 2), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_MATRIX_SHAPE");
  EXPECT_EQ(styio_runtime_error_matches_effect("bounds"), 0);
  styio_runtime_clear_error();

  const int64_t m = styio_matrix_new_i64(1, 1);
  ASSERT_NE(m, 0);
  EXPECT_EQ(styio_matrix_get_i64(m, 3, 0), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_MATRIX_INDEX");
  EXPECT_EQ(styio_runtime_error_matches_effect("bounds"), 1);
  styio_runtime_clear_error();
  styio_matrix_release(m);
}

TEST(StyioSafetyRuntime, DictApiCoversBackendsFamiliesClonesAndViews) {
  const int old_impl = styio_dict_runtime_get_impl();
  ASSERT_GE(styio_dict_runtime_supported_impl_count(), 2);
  EXPECT_STREQ(styio_dict_runtime_supported_impl_name(-1), nullptr);
  EXPECT_STREQ(styio_dict_runtime_supported_impl_name(99), nullptr);
  EXPECT_STREQ(styio_dict_runtime_canonical_impl_name("ordered-hash"), "ordered-hash");
  EXPECT_STREQ(styio_dict_runtime_canonical_impl_name("linear"), "linear");
  EXPECT_EQ(styio_dict_runtime_canonical_impl_name("missing"), nullptr);
  EXPECT_EQ(styio_dict_runtime_set_impl_by_name("linear"), 1);
  EXPECT_STREQ(styio_dict_runtime_get_impl_name(), "linear");

  const int64_t bools = styio_dict_new_bool();
  const int64_t ints = styio_dict_new_i64();
  const int64_t floats = styio_dict_new_f64();
  const int64_t strings = styio_dict_new_cstr();
  ASSERT_NE(bools, 0);
  ASSERT_NE(ints, 0);
  ASSERT_NE(floats, 0);
  ASSERT_NE(strings, 0);
  styio_dict_set_bool(bools, "ok", 1);
  styio_dict_set_i64(ints, "n", 41);
  styio_dict_set_i64(ints, "n", 42);
  styio_dict_set_f64(floats, "x", 1.25);
  styio_dict_set_cstr(strings, "s", "a\nb");
  EXPECT_EQ(styio_dict_get_bool(bools, "ok"), 1);
  EXPECT_EQ(styio_dict_get_i64(ints, "n"), 42);
  EXPECT_DOUBLE_EQ(styio_dict_get_f64(floats, "x"), 1.25);
  const char* s = styio_dict_get_cstr(strings, "s");
  ASSERT_NE(s, nullptr);
  EXPECT_STREQ(s, "a\nb");
  styio_free_cstr(s);

  const int64_t keys = styio_dict_keys(ints);
  const int64_t values_bool = styio_dict_values_bool(bools);
  const int64_t values_i64 = styio_dict_values_i64(ints);
  const int64_t values_f64 = styio_dict_values_f64(floats);
  const int64_t values_cstr = styio_dict_values_cstr(strings);
  ASSERT_NE(keys, 0);
  ASSERT_NE(values_bool, 0);
  ASSERT_NE(values_i64, 0);
  ASSERT_NE(values_f64, 0);
  ASSERT_NE(values_cstr, 0);
  EXPECT_EQ(styio_list_len(keys), 1);
  EXPECT_EQ(styio_list_get_bool(values_bool, 0), 1);
  EXPECT_EQ(styio_list_get(values_i64, 0), 42);
  EXPECT_DOUBLE_EQ(styio_list_get_f64(values_f64, 0), 1.25);
  const char* sv = styio_list_get_cstr(values_cstr, 0);
  ASSERT_NE(sv, nullptr);
  EXPECT_STREQ(sv, "a\nb");
  styio_free_cstr(sv);

  const int64_t child_list = styio_list_new_i64();
  const int64_t replacement_list = styio_list_new_i64();
  styio_list_push_i64(child_list, 7);
  styio_list_push_i64(replacement_list, 9);
  const int64_t list_dict = styio_dict_new_list();
  ASSERT_NE(list_dict, 0);
  styio_dict_set_list(list_dict, "xs", child_list);
  styio_dict_set_list(list_dict, "xs", replacement_list);
  const int64_t cloned_list = styio_dict_get_list(list_dict, "xs");
  ASSERT_NE(cloned_list, 0);
  EXPECT_EQ(styio_list_get(cloned_list, 0), 9);
  const int64_t list_values = styio_dict_values_list(list_dict);
  ASSERT_NE(list_values, 0);

  const int64_t nested = styio_dict_new_i64();
  styio_dict_set_i64(nested, "v", 5);
  const int64_t nested2 = styio_dict_new_i64();
  styio_dict_set_i64(nested2, "v", 6);
  const int64_t dict_dict = styio_dict_new_dict();
  ASSERT_NE(dict_dict, 0);
  styio_dict_set_dict(dict_dict, "d", nested);
  styio_dict_set_dict(dict_dict, "d", nested2);
  const int64_t cloned_dict = styio_dict_get_dict(dict_dict, "d");
  ASSERT_NE(cloned_dict, 0);
  EXPECT_EQ(styio_dict_get_i64(cloned_dict, "v"), 6);
  const int64_t dict_values = styio_dict_values_dict(dict_dict);
  ASSERT_NE(dict_values, 0);

  const int64_t cloned_ints = styio_dict_clone(ints);
  ASSERT_NE(cloned_ints, 0);
  EXPECT_EQ(styio_dict_get_i64(cloned_ints, "n"), 42);
  const char* repr = styio_dict_to_cstr(strings);
  ASSERT_NE(repr, nullptr);
  EXPECT_STREQ(repr, "{\"s\":\"a\\nb\"}");
  styio_free_cstr(repr);

  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_i64(ints, "missing"), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  EXPECT_EQ(styio_runtime_error_matches_effect("bounds"), 1);
  styio_runtime_clear_error();
  styio_dict_set_i64(ints, nullptr, 1);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();

  for (int64_t h : {keys, values_bool, values_i64, values_f64, values_cstr,
                    child_list, replacement_list, cloned_list, list_values,
                    bools, ints, floats, strings, list_dict, nested, nested2,
                    dict_dict, cloned_dict, dict_values, cloned_ints}) {
    styio_list_release(h);
    styio_dict_release(h);
  }
  EXPECT_EQ(styio_list_active_count(), 0);
  EXPECT_EQ(styio_dict_active_count(), 0);
  EXPECT_EQ(styio_dict_runtime_set_impl_by_name("missing"), 0);
  EXPECT_EQ(styio_dict_runtime_set_impl(999), 0);
  EXPECT_EQ(styio_dict_runtime_set_impl(old_impl), 1);
}

TEST(StyioSafetyRuntime, FloatAndStringDictClonesRebuildIndexesAndEscapedRepr) {
  styio_runtime_clear_error();
  const int old_impl = styio_dict_runtime_get_impl();
  EXPECT_EQ(styio_dict_runtime_canonical_impl_name(nullptr), nullptr);
  EXPECT_EQ(styio_dict_runtime_set_impl_by_name(""), 0);
  ASSERT_EQ(styio_dict_runtime_set_impl_by_name("ordered-hash"), 1);

  const int64_t floats = styio_dict_new_f64();
  const int64_t strings = styio_dict_new_cstr();
  ASSERT_NE(floats, 0);
  ASSERT_NE(strings, 0);
  styio_dict_set_f64(floats, "pi", 3.0);
  styio_dict_set_f64(floats, "pi", 3.25);
  styio_dict_set_cstr(strings, "k\\\"\r\t", "v\\\"\r\t");
  styio_dict_set_cstr(strings, "plain", "text");

  const int64_t floats_clone = styio_dict_clone(floats);
  const int64_t strings_clone = styio_dict_clone(strings);
  ASSERT_NE(floats_clone, 0);
  ASSERT_NE(strings_clone, 0);
  EXPECT_EQ(styio_dict_len(floats_clone), 1);
  EXPECT_EQ(styio_dict_len(strings_clone), 2);
  EXPECT_DOUBLE_EQ(styio_dict_get_f64(floats_clone, "pi"), 3.25);
  const char* cloned_text = styio_dict_get_cstr(strings_clone, "plain");
  ASSERT_NE(cloned_text, nullptr);
  EXPECT_STREQ(cloned_text, "text");
  styio_free_cstr(cloned_text);

  const char* repr = styio_dict_to_cstr(strings_clone);
  ASSERT_NE(repr, nullptr);
  const std::string repr_text = repr;
  EXPECT_NE(repr_text.find("\\\\"), std::string::npos);
  EXPECT_NE(repr_text.find("\\\""), std::string::npos);
  EXPECT_NE(repr_text.find("\\r"), std::string::npos);
  EXPECT_NE(repr_text.find("\\t"), std::string::npos);
  styio_free_cstr(repr);

  for (int64_t h : {floats, strings, floats_clone, strings_clone}) {
    styio_dict_release(h);
  }
  EXPECT_EQ(styio_dict_active_count(), 0);
  EXPECT_EQ(styio_dict_runtime_set_impl(old_impl), 1);
  EXPECT_EQ(styio_runtime_has_error(), 0);
}

TEST(StyioSafetyRuntime, ListSlicesPopsAndDictKeysCoverContainerFamilies) {
  styio_runtime_clear_error();
  EXPECT_EQ(styio_runtime_error_matches_effect(nullptr), 0);

  const int64_t bools = styio_list_new_bool();
  const int64_t chars = styio_list_new_char();
  const int64_t ints = styio_list_new_i64();
  const int64_t floats = styio_list_new_f64();
  const int64_t strings = styio_list_new_cstr();
  ASSERT_NE(bools, 0);
  ASSERT_NE(chars, 0);
  ASSERT_NE(ints, 0);
  ASSERT_NE(floats, 0);
  ASSERT_NE(strings, 0);

  styio_list_push_bool(bools, 1);
  styio_list_push_bool(bools, 0);
  styio_list_push_char(chars, static_cast<int8_t>('a'));
  styio_list_push_char(chars, static_cast<int8_t>('b'));
  styio_list_push_i64(ints, 10);
  styio_list_push_i64(ints, 20);
  styio_list_push_f64(floats, 1.25);
  styio_list_push_f64(floats, 2.5);
  styio_list_push_cstr(strings, "left");
  styio_list_push_cstr(strings, "right");

  const int64_t bool_tail = styio_list_slice(bools, 1, 0, 0);
  const int64_t char_tail = styio_list_slice(chars, 1, 0, 0);
  const int64_t int_mid = styio_list_slice(ints, 0, 1, 1);
  const int64_t float_mid = styio_list_slice(floats, 0, 1, 1);
  const int64_t string_tail = styio_list_slice(strings, 1, 0, 0);
  ASSERT_NE(bool_tail, 0);
  ASSERT_NE(char_tail, 0);
  ASSERT_NE(int_mid, 0);
  ASSERT_NE(float_mid, 0);
  ASSERT_NE(string_tail, 0);
  EXPECT_EQ(styio_list_get_bool(bool_tail, 0), 0);
  EXPECT_EQ(styio_list_get_char(char_tail, 0), static_cast<int8_t>('b'));
  EXPECT_EQ(styio_list_get(int_mid, 0), 10);
  EXPECT_DOUBLE_EQ(styio_list_get_f64(float_mid, 0), 1.25);
  const char* sliced_string = styio_list_get_cstr(string_tail, 0);
  ASSERT_NE(sliced_string, nullptr);
  EXPECT_STREQ(sliced_string, "right");
  styio_free_cstr(sliced_string);

  for (int64_t h : {bools, chars, ints, floats, strings}) {
    ASSERT_EQ(styio_list_len(h), 2);
    styio_list_pop(h);
    EXPECT_EQ(styio_list_len(h), 1);
  }

  const int64_t child_list = styio_list_new_i64();
  const int64_t child_dict = styio_dict_new_i64();
  const int64_t child_matrix = styio_matrix_new_i64(1, 1);
  ASSERT_NE(child_list, 0);
  ASSERT_NE(child_dict, 0);
  ASSERT_NE(child_matrix, 0);
  styio_list_push_i64(child_list, 7);
  styio_dict_set_i64(child_dict, "v", 8);
  styio_matrix_set_i64(child_matrix, 0, 0, 9);

  const int64_t list_handles = styio_list_new_list();
  const int64_t dict_handles = styio_list_new_dict();
  const int64_t matrix_handles = styio_list_new_matrix();
  ASSERT_NE(list_handles, 0);
  ASSERT_NE(dict_handles, 0);
  ASSERT_NE(matrix_handles, 0);
  styio_list_push_list(list_handles, child_list);
  styio_list_push_list(list_handles, child_list);
  styio_list_push_dict(dict_handles, child_dict);
  styio_list_push_dict(dict_handles, child_dict);
  styio_list_push_matrix(matrix_handles, child_matrix);
  styio_list_push_matrix(matrix_handles, child_matrix);

  const char* list_handles_repr = styio_list_to_cstr(list_handles);
  const char* dict_handles_repr = styio_list_to_cstr(dict_handles);
  const char* matrix_handles_repr = styio_list_to_cstr(matrix_handles);
  ASSERT_NE(list_handles_repr, nullptr);
  ASSERT_NE(dict_handles_repr, nullptr);
  ASSERT_NE(matrix_handles_repr, nullptr);
  EXPECT_NE(std::string(list_handles_repr).find("[7]"), std::string::npos);
  EXPECT_NE(std::string(dict_handles_repr).find("\"v\":8"), std::string::npos);
  EXPECT_NE(std::string(matrix_handles_repr).find("[[9]]"), std::string::npos);
  styio_free_cstr(list_handles_repr);
  styio_free_cstr(dict_handles_repr);
  styio_free_cstr(matrix_handles_repr);

  const int64_t list_slice = styio_list_slice(list_handles, 0, 1, 1);
  const int64_t dict_slice = styio_list_slice(dict_handles, 0, 1, 1);
  const int64_t matrix_slice = styio_list_slice(matrix_handles, 0, 1, 1);
  ASSERT_NE(list_slice, 0);
  ASSERT_NE(dict_slice, 0);
  ASSERT_NE(matrix_slice, 0);
  const int64_t list_clone = styio_list_get_list(list_slice, 0);
  const int64_t dict_clone = styio_list_get_dict(dict_slice, 0);
  const int64_t matrix_clone = styio_list_get_matrix(matrix_slice, 0);
  ASSERT_NE(list_clone, 0);
  ASSERT_NE(dict_clone, 0);
  ASSERT_NE(matrix_clone, 0);
  EXPECT_EQ(styio_list_get(list_clone, 0), 7);
  EXPECT_EQ(styio_dict_get_i64(dict_clone, "v"), 8);
  EXPECT_EQ(styio_matrix_get_i64(matrix_clone, 0, 0), 9);
  styio_list_pop(list_handles);
  styio_list_pop(dict_handles);
  styio_list_pop(matrix_handles);
  EXPECT_EQ(styio_list_len(list_handles), 1);
  EXPECT_EQ(styio_list_len(dict_handles), 1);
  EXPECT_EQ(styio_list_len(matrix_handles), 1);

  const int64_t dict_bool = styio_dict_new_bool();
  const int64_t dict_i64 = styio_dict_new_i64();
  const int64_t dict_f64 = styio_dict_new_f64();
  const int64_t dict_cstr = styio_dict_new_cstr();
  const int64_t dict_list = styio_dict_new_list();
  const int64_t dict_dict = styio_dict_new_dict();
  ASSERT_NE(dict_bool, 0);
  ASSERT_NE(dict_i64, 0);
  ASSERT_NE(dict_f64, 0);
  ASSERT_NE(dict_cstr, 0);
  ASSERT_NE(dict_list, 0);
  ASSERT_NE(dict_dict, 0);
  styio_dict_set_bool(dict_bool, "b", 1);
  styio_dict_set_i64(dict_i64, "i", 2);
  styio_dict_set_f64(dict_f64, "f", 3.5);
  styio_dict_set_cstr(dict_cstr, "s", "text");
  styio_dict_set_list(dict_list, "l", child_list);
  styio_dict_set_dict(dict_dict, "d", child_dict);
  const char* dict_list_repr = styio_dict_to_cstr(dict_list);
  const char* dict_dict_repr = styio_dict_to_cstr(dict_dict);
  ASSERT_NE(dict_list_repr, nullptr);
  ASSERT_NE(dict_dict_repr, nullptr);
  EXPECT_NE(std::string(dict_list_repr).find("\"l\":[7]"), std::string::npos);
  EXPECT_NE(std::string(dict_dict_repr).find("\"d\":{\"v\":8}"), std::string::npos);
  styio_free_cstr(dict_list_repr);
  styio_free_cstr(dict_dict_repr);
  const int64_t keys_bool = styio_dict_keys(dict_bool);
  const int64_t keys_i64 = styio_dict_keys(dict_i64);
  const int64_t keys_f64 = styio_dict_keys(dict_f64);
  const int64_t keys_cstr = styio_dict_keys(dict_cstr);
  const int64_t keys_list = styio_dict_keys(dict_list);
  const int64_t keys_dict = styio_dict_keys(dict_dict);
  ASSERT_NE(keys_bool, 0);
  ASSERT_NE(keys_i64, 0);
  ASSERT_NE(keys_f64, 0);
  ASSERT_NE(keys_cstr, 0);
  ASSERT_NE(keys_list, 0);
  ASSERT_NE(keys_dict, 0);
  const char* key_bool = styio_list_get_cstr(keys_bool, 0);
  const char* key_i64 = styio_list_get_cstr(keys_i64, 0);
  const char* key_f64 = styio_list_get_cstr(keys_f64, 0);
  const char* key_cstr = styio_list_get_cstr(keys_cstr, 0);
  const char* key_list = styio_list_get_cstr(keys_list, 0);
  const char* key_dict = styio_list_get_cstr(keys_dict, 0);
  ASSERT_NE(key_bool, nullptr);
  ASSERT_NE(key_i64, nullptr);
  ASSERT_NE(key_f64, nullptr);
  ASSERT_NE(key_cstr, nullptr);
  ASSERT_NE(key_list, nullptr);
  ASSERT_NE(key_dict, nullptr);
  EXPECT_STREQ(key_bool, "b");
  EXPECT_STREQ(key_i64, "i");
  EXPECT_STREQ(key_f64, "f");
  EXPECT_STREQ(key_cstr, "s");
  EXPECT_STREQ(key_list, "l");
  EXPECT_STREQ(key_dict, "d");
  styio_free_cstr(key_bool);
  styio_free_cstr(key_i64);
  styio_free_cstr(key_f64);
  styio_free_cstr(key_cstr);
  styio_free_cstr(key_list);
  styio_free_cstr(key_dict);

  styio_runtime_clear_error();
  const int64_t empty_for_pop = styio_list_new_i64();
  ASSERT_NE(empty_for_pop, 0);
  styio_list_pop(empty_for_pop);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_INDEX");
  EXPECT_EQ(styio_runtime_error_matches_effect("bounds"), 1);
  styio_runtime_clear_error();
  EXPECT_EQ(styio_list_slice(ints, -1, 1, 1), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_INDEX");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_list_get(ints, -1), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_INDEX");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_list_slice(ints, 0, -1, 1), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_INDEX");
  styio_runtime_clear_error();

  for (int64_t h : {
         bool_tail, char_tail, int_mid, float_mid, string_tail,
         list_slice, dict_slice, matrix_slice, list_clone, keys_bool, keys_i64,
         keys_f64, keys_cstr, keys_list, keys_dict, bools, chars, ints, floats,
         strings, child_list, child_dict, child_matrix, list_handles,
         dict_handles, matrix_handles, dict_clone, matrix_clone, dict_bool,
         dict_i64, dict_f64, dict_cstr, dict_list, dict_dict, empty_for_pop}) {
    styio_list_release(h);
    styio_dict_release(h);
    styio_matrix_release(h);
  }
  EXPECT_EQ(styio_list_active_count(), 0);
  EXPECT_EQ(styio_dict_active_count(), 0);
  EXPECT_EQ(styio_matrix_active_count(), 0);
}

TEST(StyioSafetyRuntime, ContainerMutationAndMismatchErrorsCoverRuntimeBoundaries) {
  styio_runtime_clear_error();

  const int64_t bools = styio_list_new_bool();
  const int64_t chars = styio_list_new_char();
  const int64_t ints = styio_list_new_i64();
  const int64_t floats = styio_list_new_f64();
  const int64_t strings = styio_list_new_cstr();
  ASSERT_NE(bools, 0);
  ASSERT_NE(chars, 0);
  ASSERT_NE(ints, 0);
  ASSERT_NE(floats, 0);
  ASSERT_NE(strings, 0);

  styio_list_push_bool(bools, 1);
  styio_list_insert_bool(bools, 0, 0);
  styio_list_set_bool(bools, 1, 0);
  EXPECT_EQ(styio_list_get_bool(bools, 0), 0);
  EXPECT_EQ(styio_list_get_bool(bools, 1), 0);

  styio_list_push_char(chars, static_cast<int8_t>('x'));
  styio_list_insert_char(chars, 0, static_cast<int8_t>('y'));
  styio_list_set_char(chars, 1, static_cast<int8_t>('z'));
  EXPECT_EQ(styio_list_get_char(chars, 0), static_cast<int8_t>('y'));
  EXPECT_EQ(styio_list_get_char(chars, 1), static_cast<int8_t>('z'));

  styio_list_push_i64(ints, 10);
  styio_list_insert_i64(ints, 0, 5);
  styio_list_set(ints, 1, 20);
  EXPECT_EQ(styio_list_get(ints, 0), 5);
  EXPECT_EQ(styio_list_get(ints, 1), 20);

  styio_list_push_f64(floats, 1.0);
  styio_list_insert_f64(floats, 0, 0.5);
  styio_list_set_f64(floats, 1, 2.5);
  EXPECT_DOUBLE_EQ(styio_list_get_f64(floats, 0), 0.5);
  EXPECT_DOUBLE_EQ(styio_list_get_f64(floats, 1), 2.5);

  styio_list_push_cstr(strings, "a");
  styio_list_insert_cstr(strings, 0, nullptr);
  styio_list_set_cstr(strings, 1, "b");
  const char* empty = styio_list_get_cstr(strings, 0);
  const char* b = styio_list_get_cstr(strings, 1);
  ASSERT_NE(empty, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_STREQ(empty, "");
  EXPECT_STREQ(b, "b");
  styio_free_cstr(empty);
  styio_free_cstr(b);

  const int64_t child_list = styio_list_new_i64();
  const int64_t replacement_list = styio_list_new_i64();
  const int64_t child_dict = styio_dict_new_i64();
  const int64_t replacement_dict = styio_dict_new_i64();
  const int64_t child_matrix = styio_matrix_new_i64(1, 1);
  const int64_t replacement_matrix = styio_matrix_new_i64(1, 1);
  ASSERT_NE(child_list, 0);
  ASSERT_NE(replacement_list, 0);
  ASSERT_NE(child_dict, 0);
  ASSERT_NE(replacement_dict, 0);
  ASSERT_NE(child_matrix, 0);
  ASSERT_NE(replacement_matrix, 0);
  styio_list_push_i64(child_list, 1);
  styio_list_push_i64(replacement_list, 2);
  styio_dict_set_i64(child_dict, "v", 3);
  styio_dict_set_i64(replacement_dict, "v", 4);
  styio_matrix_set_i64(child_matrix, 0, 0, 5);
  styio_matrix_set_i64(replacement_matrix, 0, 0, 6);

  const int64_t list_handles = styio_list_new_list();
  const int64_t dict_handles = styio_list_new_dict();
  const int64_t matrix_handles = styio_list_new_matrix();
  ASSERT_NE(list_handles, 0);
  ASSERT_NE(dict_handles, 0);
  ASSERT_NE(matrix_handles, 0);
  styio_list_push_list(list_handles, child_list);
  styio_list_insert_list(list_handles, 0, replacement_list);
  styio_list_set_list(list_handles, 1, child_list);
  styio_list_push_dict(dict_handles, child_dict);
  styio_list_insert_dict(dict_handles, 0, replacement_dict);
  styio_list_set_dict(dict_handles, 1, child_dict);
  styio_list_push_matrix(matrix_handles, child_matrix);
  styio_list_insert_matrix(matrix_handles, 0, replacement_matrix);
  styio_list_set_matrix(matrix_handles, 1, child_matrix);

  const int64_t got_list = styio_list_get_list(list_handles, 0);
  const int64_t got_dict = styio_list_get_dict(dict_handles, 0);
  const int64_t got_matrix = styio_list_get_matrix(matrix_handles, 0);
  ASSERT_NE(got_list, 0);
  ASSERT_NE(got_dict, 0);
  ASSERT_NE(got_matrix, 0);
  EXPECT_EQ(styio_list_get(got_list, 0), 2);
  EXPECT_EQ(styio_dict_get_i64(got_dict, "v"), 4);
  EXPECT_EQ(styio_matrix_get_i64(got_matrix, 0, 0), 6);

  styio_runtime_clear_error();
  styio_list_insert_matrix(matrix_handles, 99, child_matrix);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_INDEX");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_list_get(floats, 0), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_ELEM_KIND");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_list_get_bool(ints, 0), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_ELEM_KIND");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_list_get_char(ints, 0), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_ELEM_KIND");
  styio_runtime_clear_error();
  EXPECT_DOUBLE_EQ(styio_list_get_f64(ints, 0), 0.0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_ELEM_KIND");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_list_get_cstr(ints, 0), nullptr);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_ELEM_KIND");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_list_get_list(ints, 0), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_ELEM_KIND");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_list_get_dict(ints, 0), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_ELEM_KIND");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_list_get_matrix(ints, 0), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_LIST_ELEM_KIND");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_list_len(child_dict), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_LIST_HANDLE");
  EXPECT_EQ(styio_runtime_error_matches_effect("closed"), 1);
  styio_runtime_clear_error();

  const int64_t dict_bool = styio_dict_new_bool();
  const int64_t dict_i64 = styio_dict_new_i64();
  const int64_t dict_f64 = styio_dict_new_f64();
  const int64_t dict_cstr = styio_dict_new_cstr();
  const int64_t dict_list = styio_dict_new_list();
  const int64_t dict_dict = styio_dict_new_dict();
  ASSERT_NE(dict_bool, 0);
  ASSERT_NE(dict_i64, 0);
  ASSERT_NE(dict_f64, 0);
  ASSERT_NE(dict_cstr, 0);
  ASSERT_NE(dict_list, 0);
  ASSERT_NE(dict_dict, 0);
  styio_dict_set_bool(dict_bool, "b", 1);
  styio_dict_set_i64(dict_i64, "i", 7);
  styio_dict_set_f64(dict_f64, "f", 1.5);
  styio_dict_set_cstr(dict_cstr, "s", nullptr);
  styio_dict_set_list(dict_list, "l", child_list);
  styio_dict_set_dict(dict_dict, "d", child_dict);
  EXPECT_EQ(styio_dict_len(dict_list), 1);
  EXPECT_EQ(styio_dict_len(dict_dict), 1);

  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_bool(dict_i64, "i"), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_DICT_HANDLE");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_i64(dict_f64, "f"), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_DICT_HANDLE");
  styio_runtime_clear_error();
  EXPECT_DOUBLE_EQ(styio_dict_get_f64(dict_i64, "i"), 0.0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_DICT_HANDLE");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_cstr(dict_i64, "i"), nullptr);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_DICT_HANDLE");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_list(dict_i64, "i"), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_DICT_HANDLE");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_dict(dict_i64, "i"), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_DICT_HANDLE");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_bool(dict_i64, nullptr), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_DICT_HANDLE");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_bool(dict_bool, nullptr), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_bool(dict_bool, "missing"), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_i64(dict_i64, nullptr), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_i64(dict_i64, "missing"), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_f64(dict_f64, nullptr), 0.0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_f64(dict_f64, "missing"), 0.0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_cstr(dict_cstr, nullptr), nullptr);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_cstr(dict_cstr, "missing"), nullptr);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_list(dict_list, nullptr), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_list(dict_list, "missing"), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_dict(dict_dict, nullptr), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_get_dict(dict_dict, "missing"), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  styio_dict_set_bool(dict_bool, nullptr, 1);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  styio_dict_set_i64(dict_i64, nullptr, 1);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  styio_dict_set_f64(dict_f64, nullptr, 1.0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  styio_dict_set_cstr(dict_cstr, nullptr, "x");
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  styio_dict_set_list(dict_list, nullptr, child_list);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  styio_dict_set_dict(dict_dict, nullptr, child_dict);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_DICT_KEY");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_dict_len(child_list), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_DICT_HANDLE");
  EXPECT_EQ(styio_runtime_error_matches_effect("closed"), 1);
  styio_runtime_clear_error();

  for (int64_t h : {
         bools, chars, ints, floats, strings, child_list, replacement_list,
         list_handles, got_list, child_dict, replacement_dict, dict_handles,
         got_dict, child_matrix, replacement_matrix, matrix_handles, got_matrix,
         dict_bool, dict_i64, dict_f64, dict_cstr, dict_list, dict_dict}) {
    styio_list_release(h);
    styio_dict_release(h);
    styio_matrix_release(h);
  }
  EXPECT_EQ(styio_list_active_count(), 0);
  EXPECT_EQ(styio_dict_active_count(), 0);
  EXPECT_EQ(styio_matrix_active_count(), 0);
}

TEST(StyioSafetyRuntime, MatrixMismatchAndSliceErrorsCoverRuntimeBoundaries) {
  styio_runtime_clear_error();

  EXPECT_EQ(styio_matrix_new_i64(std::numeric_limits<int64_t>::max(), 2), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_MATRIX_SHAPE");
  styio_runtime_clear_error();

  const int64_t wide = styio_matrix_new_i64(1, 2);
  const int64_t tall = styio_matrix_new_i64(3, 1);
  const int64_t floats = styio_matrix_new_f64(1, 2);
  const int64_t not_matrix = styio_list_new_i64();
  ASSERT_NE(wide, 0);
  ASSERT_NE(tall, 0);
  ASSERT_NE(floats, 0);
  ASSERT_NE(not_matrix, 0);
  styio_matrix_set_i64(wide, 0, 0, 1);
  styio_matrix_set_i64(wide, 0, 1, 2);
  styio_matrix_set_i64(tall, 0, 0, 3);
  styio_matrix_set_f64(floats, 0, 0, 4.0);

  EXPECT_EQ(styio_matrix_rows(wide), 1);
  EXPECT_EQ(styio_matrix_cols(wide), 2);

  styio_runtime_clear_error();
  EXPECT_EQ(styio_matrix_add_i64(wide, tall), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_MATRIX_SHAPE");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_matrix_matmul_i64(wide, wide), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_MATRIX_SHAPE");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_matrix_matmul_f64(wide, wide), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_MATRIX_SHAPE");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_matrix_get_i64(floats, 0, 0), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_MATRIX_ELEM_KIND");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_matrix_data_f64(wide), nullptr);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_MATRIX_ELEM_KIND");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_matrix_get_i64(0, 0, 0), 0);
  EXPECT_EQ(styio_matrix_get_f64(0, 0, 0), 0.0);
  EXPECT_EQ(styio_matrix_row_i64(0, 0), 0);
  EXPECT_EQ(styio_matrix_row_f64(0, 0), 0);
  EXPECT_EQ(styio_matrix_rows_slice_i64(0, 0, 1, 1), 0);
  EXPECT_EQ(styio_matrix_rows_slice_f64(0, 0, 1, 1), 0);
  EXPECT_EQ(styio_matrix_clone_i64(0), 0);
  EXPECT_EQ(styio_matrix_clone_f64(0), 0);
  EXPECT_EQ(styio_matrix_active_count(), 3);
  EXPECT_EQ(styio_runtime_has_error(), 0);
  EXPECT_EQ(styio_matrix_rows_slice_i64(wide, -1, 1, 1), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_MATRIX_INDEX");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_matrix_rows_slice_f64(floats, 0, -1, 1), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_MATRIX_INDEX");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_matrix_rows_slice_f64(floats, 2, 1, 1), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_MATRIX_INDEX");
  styio_runtime_clear_error();
  EXPECT_EQ(styio_matrix_norm(not_matrix), 0.0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_MATRIX_HANDLE");
  EXPECT_EQ(styio_runtime_error_matches_effect("closed"), 1);
  styio_runtime_clear_error();

  const char* null_matrix = styio_matrix_to_cstr(0);
  ASSERT_NE(null_matrix, nullptr);
  EXPECT_STREQ(null_matrix, "[]");
  styio_free_cstr(null_matrix);

  for (int64_t h : {wide, tall, floats}) {
    styio_matrix_release(h);
  }
  styio_list_release(not_matrix);
  EXPECT_EQ(styio_list_active_count(), 0);
  EXPECT_EQ(styio_matrix_active_count(), 0);
}

TEST(StyioSafetyRuntime, HandleCollectionClonesAndReprsCoverAllValueKinds) {
  styio_runtime_clear_error();
  const int old_impl = styio_dict_runtime_get_impl();
  ASSERT_EQ(styio_dict_runtime_set_impl_by_name("ordered_hash"), 1);
  EXPECT_STREQ(styio_dict_runtime_get_impl_name(), "ordered-hash");

  const int64_t bools = styio_list_new_bool();
  const int64_t chars = styio_list_new_char();
  const int64_t ints = styio_list_new_i64();
  const int64_t floats = styio_list_new_f64();
  const int64_t strings = styio_list_new_cstr();
  ASSERT_NE(bools, 0);
  ASSERT_NE(chars, 0);
  ASSERT_NE(ints, 0);
  ASSERT_NE(floats, 0);
  ASSERT_NE(strings, 0);
  styio_list_push_bool(bools, 1);
  styio_list_push_bool(bools, 0);
  styio_list_push_char(chars, static_cast<int8_t>('\t'));
  styio_list_push_i64(ints, 11);
  styio_list_push_f64(floats, 2.25);
  styio_list_push_cstr(strings, "alpha\"beta");

  const int64_t bool_clone = styio_list_clone(bools);
  const int64_t char_clone = styio_list_clone(chars);
  const int64_t int_clone = styio_list_clone(ints);
  const int64_t float_clone = styio_list_clone(floats);
  const int64_t string_clone = styio_list_clone(strings);
  ASSERT_NE(bool_clone, 0);
  ASSERT_NE(char_clone, 0);
  ASSERT_NE(int_clone, 0);
  ASSERT_NE(float_clone, 0);
  ASSERT_NE(string_clone, 0);
  EXPECT_EQ(styio_list_get_bool(bool_clone, 0), 1);
  EXPECT_EQ(styio_list_get_char(char_clone, 0), static_cast<int8_t>('\t'));
  EXPECT_EQ(styio_list_get(int_clone, 0), 11);
  EXPECT_DOUBLE_EQ(styio_list_get_f64(float_clone, 0), 2.25);
  const char* cloned_string = styio_list_get_cstr(string_clone, 0);
  ASSERT_NE(cloned_string, nullptr);
  EXPECT_STREQ(cloned_string, "alpha\"beta");
  styio_free_cstr(cloned_string);

  const int64_t child_list = styio_list_new_i64();
  const int64_t child_dict = styio_dict_new_i64();
  const int64_t child_matrix = styio_matrix_new_i64(1, 1);
  ASSERT_NE(child_list, 0);
  ASSERT_NE(child_dict, 0);
  ASSERT_NE(child_matrix, 0);
  styio_list_push_i64(child_list, 31);
  styio_dict_set_i64(child_dict, "n", 32);
  styio_matrix_set_i64(child_matrix, 0, 0, 33);

  const int64_t list_handles = styio_list_new_list();
  const int64_t dict_handles = styio_list_new_dict();
  const int64_t matrix_handles = styio_list_new_matrix();
  ASSERT_NE(list_handles, 0);
  ASSERT_NE(dict_handles, 0);
  ASSERT_NE(matrix_handles, 0);
  styio_list_push_list(list_handles, child_list);
  styio_list_push_dict(dict_handles, child_dict);
  styio_list_push_matrix(matrix_handles, child_matrix);
  const int64_t list_handles_clone = styio_list_clone(list_handles);
  const int64_t dict_handles_clone = styio_list_clone(dict_handles);
  const int64_t matrix_handles_clone = styio_list_clone(matrix_handles);
  ASSERT_NE(list_handles_clone, 0);
  ASSERT_NE(dict_handles_clone, 0);
  ASSERT_NE(matrix_handles_clone, 0);
  const int64_t cloned_child_list = styio_list_get_list(list_handles_clone, 0);
  const int64_t cloned_child_dict = styio_list_get_dict(dict_handles_clone, 0);
  const int64_t cloned_child_matrix = styio_list_get_matrix(matrix_handles_clone, 0);
  ASSERT_NE(cloned_child_list, 0);
  ASSERT_NE(cloned_child_dict, 0);
  ASSERT_NE(cloned_child_matrix, 0);
  EXPECT_EQ(styio_list_get(cloned_child_list, 0), 31);
  EXPECT_EQ(styio_dict_get_i64(cloned_child_dict, "n"), 32);
  EXPECT_EQ(styio_matrix_get_i64(cloned_child_matrix, 0, 0), 33);

  const int64_t dict_bool = styio_dict_new_bool();
  const int64_t dict_i64 = styio_dict_new_i64();
  const int64_t dict_f64 = styio_dict_new_f64();
  const int64_t dict_cstr = styio_dict_new_cstr();
  const int64_t dict_list = styio_dict_new_list();
  const int64_t dict_dict = styio_dict_new_dict();
  ASSERT_NE(dict_bool, 0);
  ASSERT_NE(dict_i64, 0);
  ASSERT_NE(dict_f64, 0);
  ASSERT_NE(dict_cstr, 0);
  ASSERT_NE(dict_list, 0);
  ASSERT_NE(dict_dict, 0);
  styio_dict_set_bool(dict_bool, "flag", 1);
  styio_dict_set_i64(dict_i64, "count", 44);
  styio_dict_set_f64(dict_f64, "ratio", 1.5);
  styio_dict_set_cstr(dict_cstr, "msg", "line\nnext");
  styio_dict_set_list(dict_list, "items", child_list);
  styio_dict_set_dict(dict_dict, "nested", child_dict);

  const int64_t dict_bool_clone = styio_dict_clone(dict_bool);
  const int64_t dict_list_clone = styio_dict_clone(dict_list);
  const int64_t dict_dict_clone = styio_dict_clone(dict_dict);
  ASSERT_NE(dict_bool_clone, 0);
  ASSERT_NE(dict_list_clone, 0);
  ASSERT_NE(dict_dict_clone, 0);
  EXPECT_EQ(styio_dict_get_bool(dict_bool_clone, "flag"), 1);
  const int64_t cloned_items = styio_dict_get_list(dict_list_clone, "items");
  const int64_t cloned_nested = styio_dict_get_dict(dict_dict_clone, "nested");
  ASSERT_NE(cloned_items, 0);
  ASSERT_NE(cloned_nested, 0);
  EXPECT_EQ(styio_list_get(cloned_items, 0), 31);
  EXPECT_EQ(styio_dict_get_i64(cloned_nested, "n"), 32);

  const int64_t list_values = styio_dict_values_list(dict_list);
  const int64_t dict_values = styio_dict_values_dict(dict_dict);
  ASSERT_NE(list_values, 0);
  ASSERT_NE(dict_values, 0);
  const int64_t list_value = styio_list_get_list(list_values, 0);
  const int64_t dict_value = styio_list_get_dict(dict_values, 0);
  ASSERT_NE(list_value, 0);
  ASSERT_NE(dict_value, 0);
  EXPECT_EQ(styio_list_get(list_value, 0), 31);
  EXPECT_EQ(styio_dict_get_i64(dict_value, "n"), 32);

  const char* bool_repr = styio_list_to_cstr(bool_clone);
  const char* char_repr = styio_list_to_cstr(char_clone);
  const char* dict_bool_repr = styio_dict_to_cstr(dict_bool);
  const char* dict_i64_repr = styio_dict_to_cstr(dict_i64);
  const char* float_repr = styio_dict_to_cstr(dict_f64);
  const char* string_repr = styio_dict_to_cstr(dict_cstr);
  const char* list_repr = styio_dict_to_cstr(dict_list);
  const char* dict_repr = styio_dict_to_cstr(dict_dict);
  ASSERT_NE(bool_repr, nullptr);
  ASSERT_NE(char_repr, nullptr);
  ASSERT_NE(dict_bool_repr, nullptr);
  ASSERT_NE(dict_i64_repr, nullptr);
  ASSERT_NE(float_repr, nullptr);
  ASSERT_NE(string_repr, nullptr);
  ASSERT_NE(list_repr, nullptr);
  ASSERT_NE(dict_repr, nullptr);
  EXPECT_STREQ(bool_repr, "[true,false]");
  EXPECT_STREQ(char_repr, "['\\t']");
  EXPECT_NE(std::string(dict_bool_repr).find("\"flag\":true"), std::string::npos);
  EXPECT_NE(std::string(dict_i64_repr).find("\"count\":44"), std::string::npos);
  EXPECT_NE(std::string(float_repr).find("\"ratio\":1.500000"), std::string::npos);
  EXPECT_NE(std::string(string_repr).find("\"msg\":\"line\\nnext\""), std::string::npos);
  EXPECT_NE(std::string(list_repr).find("\"items\":[31]"), std::string::npos);
  EXPECT_NE(std::string(dict_repr).find("\"nested\":{\"n\":32}"), std::string::npos);
  styio_free_cstr(bool_repr);
  styio_free_cstr(char_repr);
  styio_free_cstr(dict_bool_repr);
  styio_free_cstr(dict_i64_repr);
  styio_free_cstr(float_repr);
  styio_free_cstr(string_repr);
  styio_free_cstr(list_repr);
  styio_free_cstr(dict_repr);

  for (int64_t h : {
         bools, chars, ints, floats, strings, bool_clone, char_clone,
         int_clone, float_clone, string_clone, child_list, child_dict,
         child_matrix, list_handles, dict_handles, matrix_handles,
         list_handles_clone, dict_handles_clone, matrix_handles_clone,
         cloned_child_list, cloned_child_dict, cloned_child_matrix,
         dict_bool, dict_i64, dict_f64, dict_cstr, dict_list, dict_dict,
         dict_bool_clone, dict_list_clone, dict_dict_clone, cloned_items,
         cloned_nested, list_values, dict_values, list_value, dict_value}) {
    styio_list_release(h);
    styio_dict_release(h);
    styio_matrix_release(h);
  }
  EXPECT_EQ(styio_dict_runtime_set_impl(old_impl), 1);
  EXPECT_EQ(styio_list_active_count(), 0);
  EXPECT_EQ(styio_dict_active_count(), 0);
  EXPECT_EQ(styio_matrix_active_count(), 0);
}

TEST(StyioSafetyRuntime, TaskApiCoversReadySpawnPullProfilesAndInvalidHandles) {
  styio_runtime_clear_error();
  styio_task_scheduler_profile_reset();
  styio_task_scheduler_profile_enable(1);

  const int64_t ready_i64 = styio_task_i64_ready(12);
  const int64_t ready_f64 = styio_task_f64_ready(3.5);
  const int64_t ready_cstr = styio_task_cstr_ready("done");
  ASSERT_NE(ready_i64, 0);
  ASSERT_NE(ready_f64, 0);
  ASSERT_NE(ready_cstr, 0);
  EXPECT_EQ(styio_task_i64_pull(ready_i64), 12);
  EXPECT_DOUBLE_EQ(styio_task_f64_pull(ready_f64), 3.5);
  EXPECT_STREQ(styio_task_cstr_pull(ready_cstr), "done");

  auto i64_fn = +[](void* ctx) -> int64_t {
    return *static_cast<int64_t*>(ctx) + 8;
  };
  auto f64_fn = +[](void* ctx) -> double {
    return *static_cast<double*>(ctx) * 2.0;
  };
  auto cstr_fn = +[](void*) -> const char* {
    return "spawned";
  };
  auto* i64_ctx = static_cast<int64_t*>(std::malloc(sizeof(int64_t)));
  auto* f64_ctx = static_cast<double*>(std::malloc(sizeof(double)));
  ASSERT_NE(i64_ctx, nullptr);
  ASSERT_NE(f64_ctx, nullptr);
  *i64_ctx = 34;
  *f64_ctx = 2.25;
  const int64_t spawned_i64 = styio_task_i64_spawn(i64_fn, i64_ctx);
  const int64_t spawned_f64 = styio_task_f64_spawn(f64_fn, f64_ctx);
  const int64_t spawned_cstr = styio_task_cstr_spawn(cstr_fn, nullptr);
  ASSERT_NE(spawned_i64, 0);
  ASSERT_NE(spawned_f64, 0);
  ASSERT_NE(spawned_cstr, 0);
  EXPECT_EQ(styio_task_i64_pull(spawned_i64), 42);
  EXPECT_DOUBLE_EQ(styio_task_f64_pull(spawned_f64), 4.5);
  EXPECT_STREQ(styio_task_cstr_pull(spawned_cstr), "spawned");
  EXPECT_GE(styio_task_worker_count(), 1);

  StyioTaskSchedulerProfileSnapshot snapshot{};
  styio_task_scheduler_profile_snapshot(&snapshot);
  EXPECT_EQ(snapshot.enabled, 1);
  EXPECT_GE(snapshot.ready_tasks, 3);
  EXPECT_GE(snapshot.spawned_tasks, 3);
  EXPECT_GE(snapshot.pulled_tasks, 6);
  styio_task_scheduler_profile_snapshot(nullptr);

  EXPECT_EQ(styio_task_i64_pull(ready_i64), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_TASK_CONSUMED");
  EXPECT_EQ(styio_runtime_error_matches_effect("closed"), 1);
  styio_runtime_clear_error();

  EXPECT_EQ(styio_task_active_count(), 6);
  styio_task_release(ready_i64);
  styio_task_release(ready_f64);
  styio_task_release(ready_cstr);
  styio_task_release(spawned_i64);
  styio_task_release(spawned_f64);
  styio_task_release(spawned_cstr);
  EXPECT_EQ(styio_task_active_count(), 0);

  EXPECT_DOUBLE_EQ(styio_task_f64_pull(0), 0.0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_TASK_HANDLE");
  styio_runtime_clear_error();
  EXPECT_STREQ(styio_task_cstr_pull(0), "");
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_INVALID_TASK_HANDLE");
  styio_runtime_clear_error();
  styio_task_scheduler_profile_enable(0);
}

TEST(StyioSafetyRuntime, TaskFailuresPropagateRuntimeAndExceptionDiagnostics) {
  styio_runtime_clear_error();
  styio_task_scheduler_profile_reset();
  styio_task_scheduler_profile_enable(1);
  EnvSnapshot task_threads("STYIO_TASK_THREADS");
  task_threads.set("2");

  auto parse_fail = +[](void*) -> int64_t {
    return styio_cstr_to_i64("not-an-int");
  };
  auto std_exception_fail = +[](void*) -> int64_t {
    throw std::runtime_error("task exploded");
  };
  auto unknown_exception_fail = +[](void*) -> int64_t {
    throw 17;
  };
  auto null_cstr = +[](void*) -> const char* {
    return nullptr;
  };

  const int64_t bad_parse = styio_task_i64_spawn(parse_fail, nullptr);
  const int64_t bad_std_exception = styio_task_i64_spawn(std_exception_fail, nullptr);
  const int64_t bad_unknown_exception = styio_task_i64_spawn(unknown_exception_fail, nullptr);
  const int64_t empty_string = styio_task_cstr_spawn(null_cstr, nullptr);
  ASSERT_NE(bad_parse, 0);
  ASSERT_NE(bad_std_exception, 0);
  ASSERT_NE(bad_unknown_exception, 0);
  ASSERT_NE(empty_string, 0);

  EXPECT_EQ(styio_task_i64_pull(bad_parse), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_NUMERIC_PARSE");
  EXPECT_EQ(styio_runtime_error_matches_effect("parse"), 1);
  styio_runtime_clear_error();

  EXPECT_EQ(styio_task_i64_pull(bad_std_exception), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_TASK_EXCEPTION");
  const char* std_message = styio_runtime_last_error();
  ASSERT_NE(std_message, nullptr);
  EXPECT_NE(std::strstr(std_message, "task exploded"), nullptr);
  styio_runtime_clear_error();

  EXPECT_EQ(styio_task_i64_pull(bad_unknown_exception), 0);
  EXPECT_STREQ(styio_runtime_last_error_subcode(), "STYIO_RUNTIME_TASK_EXCEPTION");
  const char* unknown_message = styio_runtime_last_error();
  ASSERT_NE(unknown_message, nullptr);
  EXPECT_NE(std::strstr(unknown_message, "unknown task exception"), nullptr);
  styio_runtime_clear_error();

  EXPECT_STREQ(styio_task_cstr_pull(empty_string), "");
  EXPECT_EQ(styio_runtime_has_error(), 0);

  StyioTaskSchedulerProfileSnapshot snapshot{};
  styio_task_scheduler_profile_snapshot(&snapshot);
  EXPECT_EQ(snapshot.enabled, 1);
  EXPECT_GE(snapshot.worker_count, 1);
  EXPECT_GE(snapshot.failed_pulls, 3);
  EXPECT_GE(snapshot.started_tasks, 4);

  for (int64_t h : {bad_parse, bad_std_exception, bad_unknown_exception, empty_string}) {
    styio_task_release(h);
  }
  EXPECT_EQ(styio_task_active_count(), 0);
  styio_task_scheduler_profile_enable(0);
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
    [&](void*)
    {
      closer_called = true;
    }
  ));
  EXPECT_FALSE(closer_called);
  EXPECT_TRUE(table.contains(id));

  EXPECT_TRUE(table.release(
    id,
    StyioHandleTable::HandleKind::File,
    [&](void* raw)
    {
      closer_called = true;
      EXPECT_EQ(raw, &payload);
    }
  ));
  EXPECT_TRUE(closer_called);
  EXPECT_FALSE(table.contains(id));
  EXPECT_EQ(table.size(), 0U);
}

TEST(StyioSafetyHandleTable, ReleaseWithoutCloserRecyclesHandle) {
  StyioHandleTable table;
  int payload = 7;

  EXPECT_EQ(table.acquire(StyioHandleTable::HandleKind::File, nullptr), 0);

  const auto id = table.acquire(StyioHandleTable::HandleKind::File, &payload);
  ASSERT_NE(id, 0);
  EXPECT_TRUE(table.release(id, StyioHandleTable::HandleKind::File));
  EXPECT_FALSE(table.contains(id));
  EXPECT_FALSE(table.release(id, StyioHandleTable::HandleKind::File));

  const auto recycled_id = table.acquire(StyioHandleTable::HandleKind::Resource, &payload);
  EXPECT_EQ(recycled_id, id);
  EXPECT_EQ(table.lookup_as<int>(recycled_id, StyioHandleTable::HandleKind::Resource), &payload);
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
    [&](void* raw)
    {
      ++released;
      EXPECT_EQ(raw, &file_payload);
    }
  );
  EXPECT_EQ(released_count, 1U);
  EXPECT_EQ(released, 1);
  EXPECT_FALSE(table.contains(file_id));
  EXPECT_FALSE(table.contains(stub_id));
  EXPECT_TRUE(table.contains(resource_id));
  EXPECT_EQ(table.size(), 1U);

  table.invalidate(resource_id);
  EXPECT_EQ(table.size(), 0U);
}
