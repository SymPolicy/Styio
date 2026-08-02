#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "StyioParser/Tokenizer.hpp"

#include "../src/StyioParser/Parser.cpp"

namespace {

namespace fs = std::filesystem;

std::string
native_path(std::string path) {
  return fs::path(std::move(path)).lexically_normal().string();
}

std::vector<std::pair<size_t, size_t>> build_line_seps(const std::string& src) {
  std::vector<std::pair<size_t, size_t>> seps;
  size_t line_start = 0;
  size_t line_len = 0;
  for (size_t i = 0; i < src.size(); ++i) {
    if (src[i] == '\n') {
      seps.push_back({line_start, line_len});
      line_start = i + 1;
      line_len = 0;
    }
    else {
      line_len += 1;
    }
  }
  if (!src.empty() && src.back() != '\n') {
    seps.push_back({line_start, line_len});
  }
  return seps;
}

std::string flat_add_expression(size_t operand_count) {
  std::ostringstream out;
  out << "1";
  for (size_t i = 1; i < operand_count; ++i) {
    out << " + 1";
  }
  return out.str();
}

std::string power_expression(size_t operand_count) {
  std::ostringstream out;
  out << "2";
  for (size_t i = 1; i < operand_count; ++i) {
    out << " ** 2";
  }
  return out.str();
}

void free_tokens(std::vector<StyioToken*>& tokens) {
  for (auto* token : tokens) {
    delete token;
  }
  tokens.clear();
}

std::string read_file_text(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void exercise_recovery_parser_for_source(const std::string& source, StyioParserEngine engine) {
  std::vector<StyioToken*> tokens;
  StyioContext* context = nullptr;
  MainBlockAST* ast = nullptr;
  try {
    tokens = StyioTokenizer::tokenize(source);
    context = StyioContext::Create(
      "<parser-corpus>",
      source,
      build_line_seps(source),
      tokens,
      false);
    ast = parse_main_block_with_engine_latest(context[0], engine, nullptr, StyioParseMode::Recovery);
  }
  catch (...) {
  }
  delete ast;
  delete context;
  free_tokens(tokens);
  StyioAST::destroy_all_tracked_nodes();
}

class DirectContext {
 public:
  explicit DirectContext(
      std::string src,
      bool omit_equal_tokens = false,
      std::string file_name = "<parser-internal>",
      bool debug_mode = false)
    : source_(std::move(src)),
      file_name_(std::move(file_name)),
      tokens_(StyioTokenizer::tokenize(source_)),
      context_(nullptr)
  {
    if (omit_equal_tokens) {
      std::vector<StyioToken*> filtered;
      filtered.reserve(tokens_.size());
      for (auto* token : tokens_) {
        if (token->type == StyioTokenType::TOK_EQUAL) {
          delete token;
        }
        else {
          filtered.push_back(token);
        }
      }
      tokens_ = std::move(filtered);
    }
    context_ = StyioContext::Create(
        file_name_,
        source_,
        build_line_seps(source_),
        tokens_,
        debug_mode);
  }

  ~DirectContext() {
    delete context_;
    free_tokens(tokens_);
    StyioAST::destroy_all_tracked_nodes();
  }

  StyioContext& get() {
    return *context_;
  }

  size_t token_count() const {
    return tokens_.size();
  }

  void align_token(StyioTokenType type, size_t occurrence = 0) {
    size_t seen = 0;
    for (size_t i = 0; i < tokens_.size(); ++i) {
      if (tokens_[i]->type != type) {
        continue;
      }
      if (seen == occurrence) {
        context_->restore_cursor({i, 0});
        return;
      }
      seen += 1;
    }
    throw std::runtime_error("parser internal token not found");
  }

 private:
  std::string source_;
  std::string file_name_;
  std::vector<StyioToken*> tokens_;
  StyioContext* context_ = nullptr;
};

}  // namespace

TEST(StyioParserInternal, CanonicalExpressionOperatorMetadataIsTheOnlyAuthority) {
  const std::vector<StyioExprOperatorInfo> expected{
    {StyioTokenType::YIELD_PIPE, 10, StyioExprAssociativity::Left, StyioExprOperatorKind::Apply, StyioOpType::Undefined},
    {StyioTokenType::TOK_PIPE, 20, StyioExprAssociativity::Left, StyioExprOperatorKind::Fallback, StyioOpType::Undefined},
    {StyioTokenType::LOGIC_OR, 30, StyioExprAssociativity::Left, StyioExprOperatorKind::Logic, StyioOpType::Logic_OR},
    {StyioTokenType::LOGIC_AND, 40, StyioExprAssociativity::Left, StyioExprOperatorKind::Logic, StyioOpType::Logic_AND},
    {StyioTokenType::BINOP_EQ, 50, StyioExprAssociativity::Left, StyioExprOperatorKind::Comparison, StyioOpType::Equal},
    {StyioTokenType::BINOP_NE, 50, StyioExprAssociativity::Left, StyioExprOperatorKind::Comparison, StyioOpType::Not_Equal},
    {StyioTokenType::BINOP_GT, 60, StyioExprAssociativity::Left, StyioExprOperatorKind::Comparison, StyioOpType::Greater_Than},
    {StyioTokenType::TOK_RANGBRAC, 60, StyioExprAssociativity::Left, StyioExprOperatorKind::Comparison, StyioOpType::Greater_Than},
    {StyioTokenType::BINOP_GE, 60, StyioExprAssociativity::Left, StyioExprOperatorKind::Comparison, StyioOpType::Greater_Than_Equal},
    {StyioTokenType::BINOP_LT, 60, StyioExprAssociativity::Left, StyioExprOperatorKind::Comparison, StyioOpType::Less_Than},
    {StyioTokenType::TOK_LANGBRAC, 60, StyioExprAssociativity::Left, StyioExprOperatorKind::Comparison, StyioOpType::Less_Than},
    {StyioTokenType::BINOP_LE, 60, StyioExprAssociativity::Left, StyioExprOperatorKind::Comparison, StyioOpType::Less_Than_Equal},
    {StyioTokenType::TOK_PLUS, 70, StyioExprAssociativity::Left, StyioExprOperatorKind::Arithmetic, StyioOpType::Binary_Add},
    {StyioTokenType::TOK_MINUS, 70, StyioExprAssociativity::Left, StyioExprOperatorKind::Arithmetic, StyioOpType::Binary_Sub},
    {StyioTokenType::TOK_STAR, 80, StyioExprAssociativity::Left, StyioExprOperatorKind::Arithmetic, StyioOpType::Binary_Mul},
    {StyioTokenType::TOK_SLASH, 80, StyioExprAssociativity::Left, StyioExprOperatorKind::Arithmetic, StyioOpType::Binary_Div},
    {StyioTokenType::TOK_PERCENT, 80, StyioExprAssociativity::Left, StyioExprOperatorKind::Arithmetic, StyioOpType::Binary_Mod},
    {StyioTokenType::BINOP_POW, 90, StyioExprAssociativity::Right, StyioExprOperatorKind::Arithmetic, StyioOpType::Binary_Pow},
  };
  ASSERT_EQ(std::size(kStyioExprOperators), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE(i);
    const auto* actual = styio_expr_operator_info(expected[i].token);
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(actual, &kStyioExprOperators[i]);
    EXPECT_EQ(actual->precedence, expected[i].precedence);
    EXPECT_EQ(actual->associativity, expected[i].associativity);
    EXPECT_EQ(actual->kind, expected[i].kind);
    EXPECT_EQ(actual->ast_op, expected[i].ast_op);
  }

  const auto* add = styio_expr_operator_info(StyioTokenType::TOK_PLUS);
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->precedence, 70);
  EXPECT_EQ(add->associativity, StyioExprAssociativity::Left);
  EXPECT_EQ(add->kind, StyioExprOperatorKind::Arithmetic);
  EXPECT_EQ(add->ast_op, StyioOpType::Binary_Add);

  const auto* multiply = styio_expr_operator_info(StyioTokenType::TOK_STAR);
  ASSERT_NE(multiply, nullptr);
  EXPECT_GT(multiply->precedence, add->precedence);
  EXPECT_EQ(multiply->associativity, StyioExprAssociativity::Left);
  EXPECT_EQ(multiply->ast_op, StyioOpType::Binary_Mul);

  const auto* power = styio_expr_operator_info(StyioTokenType::BINOP_POW);
  ASSERT_NE(power, nullptr);
  EXPECT_GT(power->precedence, multiply->precedence);
  EXPECT_EQ(power->associativity, StyioExprAssociativity::Right);
  EXPECT_EQ(power->ast_op, StyioOpType::Binary_Pow);

  const auto* logic_or = styio_expr_operator_info(StyioTokenType::LOGIC_OR);
  ASSERT_NE(logic_or, nullptr);
  EXPECT_EQ(logic_or->kind, StyioExprOperatorKind::Logic);
  EXPECT_LT(logic_or->precedence, add->precedence);

  const auto* fallback = styio_expr_operator_info(StyioTokenType::TOK_PIPE);
  ASSERT_NE(fallback, nullptr);
  EXPECT_EQ(fallback->kind, StyioExprOperatorKind::Fallback);
  EXPECT_LT(fallback->precedence, logic_or->precedence);

  EXPECT_EQ(styio_expr_operator_info(StyioTokenType::BINOP_BITAND), nullptr);
  EXPECT_EQ(styio_expr_operator_info(StyioTokenType::BINOP_BITOR), nullptr);
  EXPECT_EQ(styio_expr_operator_info(StyioTokenType::BINOP_BITXOR), nullptr);
  EXPECT_EQ(styio_expr_operator_info(StyioTokenType::LOGIC_XOR), nullptr);
  EXPECT_EQ(styio_expr_operator_info(StyioTokenType::TOK_EQUAL), nullptr);
  EXPECT_EQ(styio_expr_operator_info(StyioTokenType::ARROW_SINGLE_RIGHT), nullptr);
}

TEST(StyioParserInternal, UnifiedExpressionCorePreservesPrecedenceAndAssociativity) {
  {
    DirectContext direct("1 + 2 * 3");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* add = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->getOp(), StyioOpType::Binary_Add);
    auto* multiply = dynamic_cast<BinOpAST*>(add->getRHS());
    ASSERT_NE(multiply, nullptr);
    EXPECT_EQ(multiply->getOp(), StyioOpType::Binary_Mul);
  }
  {
    DirectContext direct("8 / 4 % 3");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* modulo = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(modulo, nullptr);
    EXPECT_EQ(modulo->getOp(), StyioOpType::Binary_Mod);
    auto* divide = dynamic_cast<BinOpAST*>(modulo->getLHS());
    ASSERT_NE(divide, nullptr);
    EXPECT_EQ(divide->getOp(), StyioOpType::Binary_Div);
  }
  {
    DirectContext direct("2 ** 3 ** 2");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* outer = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(outer, nullptr);
    EXPECT_EQ(outer->getOp(), StyioOpType::Binary_Pow);
    auto* inner = dynamic_cast<BinOpAST*>(outer->getRHS());
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->getOp(), StyioOpType::Binary_Pow);
  }
  {
    DirectContext direct("a == b && c != d || e");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* logical_or = dynamic_cast<CondAST*>(ast.get());
    ASSERT_NE(logical_or, nullptr);
    EXPECT_EQ(logical_or->getSign(), LogicType::OR);
    auto* logical_and = dynamic_cast<CondAST*>(logical_or->getLHS());
    ASSERT_NE(logical_and, nullptr);
    EXPECT_EQ(logical_and->getSign(), LogicType::AND);
  }
  {
    DirectContext direct("a | b | c");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* outer = dynamic_cast<FallbackAST*>(ast.get());
    ASSERT_NE(outer, nullptr);
    EXPECT_NE(dynamic_cast<FallbackAST*>(outer->getPrimary()), nullptr);
  }
}

TEST(StyioParserInternal, UnifiedExpressionCoreKeepsFlatWorkLinearAndDepthBounded) {
  {
    DirectContext direct(flat_add_expression(4096));
    StyioParserRouteStats stats;
    direct.get().set_parser_route_stats_latest(&stats);
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    direct.get().set_parser_route_stats_latest(nullptr);
    ASSERT_NE(ast, nullptr);
    EXPECT_LE(stats.expression_token_visits, 8 * direct.token_count() + 8);
    EXPECT_EQ(stats.expression_scratch_allocations, 0u);
    EXPECT_EQ(stats.expression_ast_nodes, 4095u);
    EXPECT_LT(stats.expression_max_depth, kStyioExprMaxDepth);
  }
  {
    DirectContext direct(power_expression(kStyioExprMaxDepth));
    EXPECT_NO_THROW({
      std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
      EXPECT_NE(ast, nullptr);
    });
  }
  {
    DirectContext direct(power_expression(kStyioExprMaxDepth + 1));
    try {
      std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
      FAIL() << "expected right-associative expression depth failure";
    }
    catch (const StyioParserResourceLimitError& ex) {
      EXPECT_NE(
        std::string(ex.what()).find("expression exceeds parser recursion limit of 128"),
        std::string::npos);
    }
  }
}

TEST(StyioParserInternal, UnifiedExpressionCoreOwnsFollowFatalAndDeclineRoutes) {
  {
    DirectContext direct("1 + 2 | fallback");
    auto attempt = try_parse_expr_subset_until_latest(
      direct.get(), {StyioTokenType::TOK_PIPE});
    ASSERT_EQ(attempt.status, ParseAttemptStatus::Parsed);
    std::unique_ptr<StyioAST> ast(attempt.node);
    auto* add = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->getOp(), StyioOpType::Binary_Add);
    EXPECT_EQ(direct.get().cur_tok_type(), StyioTokenType::TOK_PIPE);
  }
  {
    DirectContext direct(")");
    const auto saved = direct.get().save_cursor();
    auto attempt = try_parse_expr_subset_until_latest(direct.get(), {});
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Declined);
    EXPECT_EQ(attempt.node, nullptr);
    EXPECT_EQ(direct.get().save_cursor(), saved);
  }
  {
    DirectContext direct("1 +");
    const auto saved = direct.get().save_cursor();
    auto attempt = try_parse_expr_subset_until_latest(direct.get(), {});
    ASSERT_EQ(attempt.status, ParseAttemptStatus::Fatal);
    EXPECT_EQ(attempt.node, nullptr);
    EXPECT_EQ(direct.get().save_cursor(), saved);
    ASSERT_NE(attempt.error, nullptr);
    try {
      std::rethrow_exception(attempt.error);
      FAIL() << "expected preserved expression failure";
    }
    catch (const StyioSyntaxError& ex) {
      EXPECT_NE(std::string(ex.what()).find(
        "unexpected token in nightly parser expression subset"), std::string::npos);
    }
  }
  {
    DirectContext direct("base . 1");
    try {
      std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
      FAIL() << "expected missing-member failure";
    }
    catch (const StyioSyntaxError& ex) {
      const std::string payload = "expected name after '.' in nightly parser subset";
      const std::string message = ex.what();
      const size_t payload_start = message.find(payload);
      ASSERT_NE(payload_start, std::string::npos);
      EXPECT_EQ(message.substr(payload_start), payload);
    }
  }
}

TEST(StyioParserInternal, UnifiedResourceSuffixesAndResourceHelpersStayExplicit) {
  {
    DirectContext direct("\"prefix\" + \"payload\" >> @stdout");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* write = dynamic_cast<ResourceWriteAST*>(ast.get());
    ASSERT_NE(write, nullptr);
    auto* data = dynamic_cast<BinOpAST*>(write->getData());
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->getOp(), StyioOpType::Binary_Add);
  }
  {
    DirectContext direct("\"prefix\" + \"payload\" -> @stdout");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* redirect = dynamic_cast<ResourceRedirectAST*>(ast.get());
    ASSERT_NE(redirect, nullptr);
    auto* data = dynamic_cast<BinOpAST*>(redirect->getData());
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->getOp(), StyioOpType::Binary_Add);
  }
  {
    DirectContext direct("#(item: i64, raw)");
    auto params = parse_internal_resource_params_latest(direct.get());
    ASSERT_EQ(params.size(), 2u);
    EXPECT_EQ(params[0]->getNameAsStr(), "item");
    EXPECT_EQ(params[0]->getDType()->getTypeName(), "i64");
    EXPECT_EQ(params[1]->getNameAsStr(), "raw");
    EXPECT_FALSE(params[1]->isTyped());
  }
  {
    DirectContext direct("slot: i64 := { << 1 }");
    std::unique_ptr<ResourceDeclAST> decl(parse_resource_decl_after_at_latest(direct.get()));
    ASSERT_NE(decl, nullptr);
    ASSERT_EQ(decl->getSlots().size(), 1u);
    ASSERT_NE(decl->getDriver(), nullptr);
  }
  {
    DirectContext direct("a: i64, @b: f64");
    std::unique_ptr<ResourceDeclAST> decl(parse_resource_decl_after_at_latest(direct.get()));
    ASSERT_NE(decl, nullptr);
    ASSERT_EQ(decl->getSlots().size(), 2u);
    EXPECT_EQ(decl->getSlots()[0].name->getAsStr(), "a");
    EXPECT_EQ(decl->getSlots()[1].type->getTypeName(), "f64");
  }
  {
    DirectContext direct(">>(item) { << item }");
    direct.get().move_forward(1, "parser_internal_after_iterator");
    std::unique_ptr<StyioAST> ast(parse_iterator_tail(
      direct.get(),
      ListAST::Create({IntAST::Create("1")})));
    ASSERT_EQ(ast->getNodeType(), StyioNodeType::Iterator);
    auto* iter = static_cast<IteratorAST*>(ast.get());
    ASSERT_EQ(iter->params.size(), 1u);
    ASSERT_EQ(iter->following.size(), 1u);
    EXPECT_EQ(iter->following[0]->getNodeType(), StyioNodeType::Block);
  }
}

TEST(StyioParserInternal, ArgumentTypesAndUnifiedExpressionBranchesStayExplicit) {
  {
    DirectContext direct("with_default:i64=7", true);
    direct.get().restore_cursor({2, 0});
    std::unique_ptr<ParamAST> param(parse_argument(direct.get()));
    ASSERT_NE(param, nullptr);
    EXPECT_EQ(param->getNameAsStr(), "with_default");
    ASSERT_NE(param->getDType(), nullptr);
    EXPECT_EQ(param->getDType()->getTypeName(), "i64");
    ASSERT_NE(param->val_init, nullptr);
    EXPECT_EQ(param->val_init->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectContext direct("(i64, f64)");
    std::unique_ptr<TypeAST> type(parse_styio_type(direct.get()));
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->getTypeName(), "(i64,f64)");
  }
  {
    DirectContext direct("i64...");
    std::unique_ptr<TypeAST> type(parse_styio_type(direct.get()));
    ASSERT_NE(type, nullptr);
    EXPECT_NE(type->getTypeName().find("i64"), std::string::npos);
  }
  {
    DirectContext direct("1 + 2 * 3");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* bin = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->getOp(), StyioOpType::Binary_Add);
    auto* rhs = dynamic_cast<BinOpAST*>(bin->getRHS());
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getOp(), StyioOpType::Binary_Mul);
  }
  {
    DirectContext direct("@stdin");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::StdinResource);
  }
  {
    DirectContext direct("1 + 2 * 3");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* bin = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->getOp(), StyioOpType::Binary_Add);
    ASSERT_NE(dynamic_cast<BinOpAST*>(bin->getRHS()), nullptr);
  }
  {
    DirectContext direct("-name");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* bin = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->getOp(), StyioOpType::Binary_Sub);
  }
  {
    DirectContext direct("<| 7");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Return);
  }
  {
    DirectContext direct("|<| 8");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Return);
  }
  {
    DirectContext direct("dict{}");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_EQ(ast->getNodeType(), StyioNodeType::Dict);
    EXPECT_TRUE(static_cast<DictAST*>(ast.get())->getEntries().empty());
  }
  {
    DirectContext direct("dict");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Id);
  }
  {
    DirectContext direct("=>");
    EXPECT_THROW(
      (void)parse_infinite_conditional_loop_after_iterator(direct.get()),
      StyioSyntaxError);
  }
  {
    DirectContext direct("{1 + : 2}");
    EXPECT_THROW(
      (void)parse_dict_literal_for_resource_body_latest(direct.get()),
      StyioBaseException);
  }
  {
    DirectContext direct("{ _ 1 }");
    EXPECT_THROW((void)parse_cases_only_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct(">> > #");
    EXPECT_THROW(
      (void)parse_iterator_only_latest(direct.get(), ListAST::Create({IntAST::Create("1")})),
      StyioSyntaxError);
  }
  {
    DirectContext direct(">> > route");
    EXPECT_THROW(
      (void)parse_iterator_only_latest(direct.get(), ListAST::Create({IntAST::Create("1")})),
      StyioSyntaxError);
  }
}

TEST(StyioParserInternal, ContextHelpersCoverNavigationDiagnosticsAndDisplayEdges) {
  {
    DirectContext direct("[item]\nnext");
    EXPECT_TRUE(direct.get().recover_to_statement_boundary(0));
    EXPECT_EQ(direct.get().cur_tok_type(), StyioTokenType::NAME);
  }
  {
    DirectContext direct("[|4|]\nnext");
    EXPECT_TRUE(direct.get().recover_to_statement_boundary(0));
    EXPECT_EQ(direct.get().cur_tok_type(), StyioTokenType::NAME);
  }
  {
    DirectContext direct("=<");
    EXPECT_FALSE(direct.get().map_match(StyioTokenType::ARROW_DOUBLE_RIGHT));
  }
  {
    DirectContext direct("line");
    std::string label = direct.get().label_cur_line(0);
    EXPECT_NE(label.find("^---"), std::string::npos);
  }
  {
    DirectContext direct("+");
    EXPECT_TRUE(direct.get().check_binop());
  }
  {
    DirectContext direct("-");
    EXPECT_TRUE(direct.get().check_binop());
  }
  {
    DirectContext direct("*");
    EXPECT_TRUE(direct.get().check_binop());
  }
  {
    DirectContext direct("%");
    EXPECT_TRUE(direct.get().check_binop());
  }
  {
    DirectContext direct("/");
    EXPECT_TRUE(direct.get().check_binop());
  }
  {
    DirectContext direct("// comment");
    EXPECT_FALSE(direct.get().check_binop());
  }
  {
    DirectContext direct("/* comment */");
    EXPECT_FALSE(direct.get().check_binop());
  }
  {
    DirectContext direct("name");
    EXPECT_FALSE(direct.get().check_binop());
  }
  {
    DirectContext direct("");
    EXPECT_FALSE(direct.get().check_binop());
  }
  {
    DirectContext direct("one\ntwo");
    testing::internal::CaptureStdout();
    direct.get().show_code_with_linenum();
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("|0|-"), std::string::npos);
    EXPECT_NE(output.find("one"), std::string::npos);
  }
  {
    DirectContext direct("ast");
    testing::internal::CaptureStdout();
    direct.get().show_ast(IntAST::Create("7"));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(output.empty());
  }
}

TEST(StyioParserInternal, ContextHelpersCoverAdditionalTokenEdges) {
  EXPECT_TRUE(is_all_underscore_identifier_latest("__"));
  EXPECT_FALSE(is_all_underscore_identifier_latest("_x"));
  {
    std::vector<StyioToken*> tokens{
      StyioToken::Create(StyioTokenType::NAME, "__"),
    };
    {
      StyioContext context("<parser-internal>", "__", build_line_seps("__"), tokens, false);
      EXPECT_TRUE(is_default_case_wildcard_latest(context));
    }
    free_tokens(tokens);
  }
  {
    DirectContext direct(";; name");
    consume_statement_separators_latest(direct.get());
    EXPECT_EQ(direct.get().cur_tok_type(), StyioTokenType::NAME);
  }
  {
    EXPECT_TRUE(is_statement_separator_latest(StyioTokenType::TOK_SEMICOLON));
    EXPECT_TRUE(is_statement_separator_latest(StyioTokenType::PIPE_SEMICOLON));
    EXPECT_FALSE(is_statement_separator_latest(StyioTokenType::NAME));
    EXPECT_TRUE(is_import_list_separator_latest(StyioTokenType::TOK_COMMA));
    EXPECT_TRUE(is_import_list_separator_latest(StyioTokenType::TOK_SEMICOLON));
    EXPECT_FALSE(is_import_list_separator_latest(StyioTokenType::NAME));
    EXPECT_TRUE(is_import_path_separator_latest(StyioTokenType::TOK_SLASH));
    EXPECT_TRUE(is_import_path_separator_latest(StyioTokenType::TOK_DOT));
    EXPECT_FALSE(is_import_path_separator_latest(StyioTokenType::NAME));
  }
  {
    DirectContext direct("left\n.right");
    direct.get().move_forward(2, "linebreak_dot");
    EXPECT_TRUE(has_linebreak_before_current_token_latest(direct.get()));
  }
  {
    DirectContext direct("left\n   .right");
    direct.get().move_forward(3, "linebreak_space_dot");
    EXPECT_TRUE(has_linebreak_before_current_token_latest(direct.get()));
  }
  {
    DirectContext direct("left.right");
    direct.get().move_forward(1, "same_line_dot");
    EXPECT_FALSE(has_linebreak_before_current_token_latest(direct.get()));
  }
  {
    DirectContext direct("[\"core\", \"util\"]");
    EXPECT_TRUE(matches_legacy_string_list_import_latest(direct.get()));
  }
  {
    DirectContext direct("[\"core\",]");
    EXPECT_FALSE(matches_legacy_string_list_import_latest(direct.get()));
  }
  {
    DirectContext direct("[");
    EXPECT_FALSE(matches_legacy_string_list_import_latest(direct.get()));
  }
  {
    std::vector<StyioToken*> tokens{
      StyioToken::Create(StyioTokenType::TOK_LBOXBRAC, "["),
      StyioToken::Create(StyioTokenType::STRING, "pkg"),
    };
    {
      StyioContext context(
        "<parser-internal>",
        "[\"pkg\"",
        build_line_seps("[\"pkg\""),
        tokens,
        false);
      EXPECT_FALSE(matches_legacy_string_list_import_latest(context));
    }
    free_tokens(tokens);
  }
  {
    std::vector<StyioToken*> tokens{
      StyioToken::Create(StyioTokenType::TOK_LBOXBRAC, "["),
      StyioToken::Create(StyioTokenType::TOK_SPACE, " "),
      StyioToken::Create(StyioTokenType::STRING, "\"core\""),
      StyioToken::Create(StyioTokenType::TOK_COMMA, ","),
      StyioToken::Create(StyioTokenType::TOK_SPACE, " "),
      StyioToken::Create(StyioTokenType::STRING, "\"util\""),
      StyioToken::Create(StyioTokenType::TOK_SPACE, " "),
      StyioToken::Create(StyioTokenType::TOK_RBOXBRAC, "]"),
    };
    {
      StyioContext context(
        "<parser-internal>",
        "[ \"core\", \"util\" ]",
        build_line_seps("[ \"core\", \"util\" ]"),
        tokens,
        false);
      EXPECT_TRUE(matches_legacy_string_list_import_latest(context));
    }
    free_tokens(tokens);
  }
  {
    DirectContext direct("name");
    EXPECT_FALSE(matches_legacy_string_list_import_latest(direct.get()));
  }
  {
    DirectContext direct(" name");
    EXPECT_TRUE(direct.get().try_check(StyioTokenType::NAME));
    EXPECT_TRUE(direct.get().try_match(StyioTokenType::NAME));
    EXPECT_EQ(direct.get().cur_tok_type(), StyioTokenType::TOK_EOF);
  }
  {
    DirectContext direct("");
    EXPECT_FALSE(direct.get().try_check(StyioTokenType::NAME));
    EXPECT_FALSE(direct.get().try_match(StyioTokenType::NAME));
    EXPECT_THROW(direct.get().try_match_panic(StyioTokenType::NAME), StyioSyntaxError);
    EXPECT_EQ(direct.get().current_token_end_pos(), 0u);
  }
  {
    StyioContext direct("<parser-internal>", "", {}, {}, false);
    EXPECT_EQ(direct.current_token_end_pos(), 0u);
    direct.move_forward(1, "empty_context");
    EXPECT_EQ(direct.get_token_index(), 0u);
    direct.skip();
    direct.skip_spaces_no_linebreak();
    EXPECT_EQ(direct.find_line_index(), 0u);
    EXPECT_NE(direct.label_cur_line().find("<empty>"), std::string::npos);
    EXPECT_EQ(direct.mark_cur_tok(), "Unknown token location");
    EXPECT_FALSE(direct.find_drop('x'));
    EXPECT_THROW(direct.try_match_panic(StyioTokenType::NAME), StyioParseError);
  }
  {
    StyioContext no_lines("<parser-internal>", "abc", {}, {}, false);
    EXPECT_NE(no_lines.label_cur_line(99, "tail").find("abc"), std::string::npos);
    no_lines.restore_cursor({99, 0});
    EXPECT_EQ(no_lines.mark_cur_tok("custom location"), "custom location");
  }
  {
    DirectContext direct("name");
    EXPECT_THROW(direct.get().map_match(StyioTokenType::NAME), StyioSyntaxError);
    EXPECT_EQ(direct.get().mark_cur_tok("name location"), "name location");
  }
  {
    DirectContext direct("1");
    EXPECT_THROW(direct.get().try_match_panic(StyioTokenType::NAME, "custom panic"), StyioSyntaxError);
  }
  {
    DirectContext direct("one\ntwo");
    EXPECT_EQ(direct.get().find_line_index(), 0u);
  }
  {
    DirectContext direct("// skip\nvalue");
    EXPECT_TRUE(direct.get().find_drop("value"));
  }
  {
    DirectContext direct(" \n/* skip */x");
    EXPECT_TRUE(direct.get().find_drop_panic('x'));
  }
  {
    DirectContext direct("// skip\nx");
    EXPECT_TRUE(direct.get().find_drop_panic('x'));
  }
  {
    DirectContext direct("^^^");
    EXPECT_EQ(direct.get().check_seq_of(StyioTokenType::TOK_HAT), 3u);
  }
  {
    DirectContext direct("12");
    EXPECT_EQ(direct.get().current_token_end_pos(), 2u);
    EXPECT_FALSE(direct.get().is_recovery_mode());
    direct.get().set_parse_mode(StyioParseMode::Recovery);
    EXPECT_TRUE(direct.get().is_recovery_mode());
    direct.get().record_parse_diagnostic(5, 3, "clamped");
    ASSERT_EQ(direct.get().parse_diagnostics().size(), 1u);
    EXPECT_EQ(direct.get().parse_diagnostics()[0].start, 5u);
    EXPECT_EQ(direct.get().parse_diagnostics()[0].end, 6u);
    direct.get().clear_parse_diagnostics();
    EXPECT_TRUE(direct.get().parse_diagnostics().empty());

    StyioParserRouteStats stats;
    direct.get().set_parser_route_stats_latest(&stats);
    EXPECT_EQ(direct.get().note_nightly_internal_legacy_bridge_latest(), 1u);
    EXPECT_EQ(stats.nightly_internal_legacy_bridges, 1u);
    EXPECT_EQ(direct.get().note_nightly_internal_legacy_bridge_latest(), 2u);
    EXPECT_EQ(stats.nightly_internal_legacy_bridges, 2u);
    direct.get().set_parser_route_stats_latest(nullptr);
    EXPECT_EQ(direct.get().note_nightly_internal_legacy_bridge_latest(), 3u);

    direct.get().move(1);
    EXPECT_TRUE(direct.get().check_next("2"));
    EXPECT_TRUE(direct.get().check_next(""));
    EXPECT_FALSE(direct.get().check_next("234"));
    EXPECT_TRUE(direct.get().check_ahead(-1, '1'));
    EXPECT_FALSE(direct.get().check_ahead(-9, '1'));
    EXPECT_TRUE(direct.get().peak_isdigit(-1));
    EXPECT_FALSE(direct.get().peak_isdigit(-9));
  }
}

TEST(StyioParserInternal, HashFunctionCommonEdgesStayExplicit) {
  StyioHashFunctionParserOps ops{
    false,
    "expected function name after #",
    "hash_ret_tuple_open",
    "hash_ret_colon",
    "hash_walrus",
    "hash_equal",
    "hash_arrow",
    "hash_match",
    "hash_assign_match",
    parse_expr,
    parse_stmt_or_expr_legacy,
    [](StyioContext& inner) -> StyioAST* { return parse_block_only(inner); },
    parse_cases_only_latest,
    parse_iterator_tail,
  };

  {
    DirectContext direct("#foo { << 1 }");
    std::unique_ptr<StyioAST> ast(parse_hash_function_common_latest(direct.get(), ops));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Func);
  }
  {
    DirectContext direct("#foo ?= 1, 2");
    std::unique_ptr<StyioAST> ast(parse_hash_function_common_latest(direct.get(), ops));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Func);
  }
  {
    DirectContext direct("#foo = #(x) => x");
    std::unique_ptr<StyioAST> ast(parse_hash_function_common_latest(direct.get(), ops));
    auto* simple = dynamic_cast<SimpleFuncAST*>(ast.get());
    ASSERT_NE(simple, nullptr);
    EXPECT_FALSE(simple->is_unique);
    ASSERT_EQ(simple->params.size(), 1u);
    EXPECT_EQ(simple->params[0]->getNameAsStr(), "x");
  }
  {
    DirectContext direct("#foo := #(x) => x");
    std::unique_ptr<StyioAST> ast(parse_hash_function_common_latest(direct.get(), ops));
    auto* simple = dynamic_cast<SimpleFuncAST*>(ast.get());
    ASSERT_NE(simple, nullptr);
    EXPECT_TRUE(simple->is_unique);
    ASSERT_EQ(simple->params.size(), 1u);
    EXPECT_EQ(simple->params[0]->getNameAsStr(), "x");
  }
  {
    DirectContext direct("#sink = @stdout");
    EXPECT_THROW((void)parse_hash_function_common_latest(direct.get(), ops), StyioSyntaxError);
  }
  {
    DirectContext direct("#foo(a, b) >> (item) => item");
    EXPECT_THROW((void)parse_hash_function_common_latest(direct.get(), ops), StyioSyntaxError);
  }
  {
    DirectContext direct("#foo := ?= { _ => 1 }");
    EXPECT_THROW((void)parse_hash_function_common_latest(direct.get(), ops), StyioSyntaxError);
  }
  {
    DirectContext direct("#foo(x) := ?= 1");
    EXPECT_THROW((void)parse_hash_function_common_latest(direct.get(), ops), StyioSyntaxError);
  }
  {
    DirectContext direct("#foo, := @extern(\"c\", \"int foo(void) { return 1; }\")");
    StyioAST* out = nullptr;
    EXPECT_FALSE(try_parse_hash_extern_binding_common_latest(direct.get(), out));
    EXPECT_EQ(out, nullptr);
    EXPECT_EQ(direct.get().cur_tok_type(), StyioTokenType::TOK_HASH);
  }
  {
    DirectContext direct("foo := @extern(\"c\", \"int foo(void) { return 1; }\")");
    StyioAST* out = nullptr;
    EXPECT_FALSE(try_parse_hash_extern_binding_common_latest(direct.get(), out));
    EXPECT_EQ(out, nullptr);
    EXPECT_EQ(direct.get().cur_tok_type(), StyioTokenType::NAME);
  }
  {
    DirectContext direct("#foo := @not_extern(\"c\", \"int foo(void) { return 1; }\")");
    StyioAST* out = nullptr;
    EXPECT_FALSE(try_parse_hash_extern_binding_common_latest(direct.get(), out));
    EXPECT_EQ(out, nullptr);
    EXPECT_EQ(direct.get().cur_tok_type(), StyioTokenType::TOK_HASH);
  }
}

TEST(StyioParserInternal, UnifiedOperatorForwardAndCodpEdgesStayExplicit) {
  {
    DirectContext direct("base ** 2");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* bin = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->getOp(), StyioOpType::Binary_Pow);
  }
  {
    DirectContext direct("base / 2");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* bin = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->getOp(), StyioOpType::Binary_Div);
  }
  {
    SCOPED_TRACE("dot without member");
    DirectContext direct("base . 1");
    EXPECT_THROW((void)parse_expr(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("+ 5");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectContext direct("1 != 2");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* comp = dynamic_cast<BinCompAST*>(ast.get());
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->getSign(), CompType::NE);
  }
  {
    DirectContext direct("1 >= 2");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* comp = dynamic_cast<BinCompAST*>(ast.get());
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->getSign(), CompType::GE);
  }
  {
    DirectContext direct("? 1");
    EXPECT_THROW((void)parse_expr(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("(1, 2)");
    std::unique_ptr<StyioAST> ast(parse_tuple_exprs(direct.get()));
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Tuple);
  }
  {
    SCOPED_TRACE("empty tuple");
    DirectContext direct("()");
    std::unique_ptr<StyioAST> ast(parse_tuple_exprs(direct.get()));
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Tuple);
  }
  {
    SCOPED_TRACE("tuple iterator");
    DirectContext direct("(1, 2) >>(item)");
    std::unique_ptr<StyioAST> ast(parse_tuple_exprs(direct.get()));
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Iterator);
  }
  {
    DirectContext direct("{ _ => 1 }");
    std::unique_ptr<CasesAST> cases(parse_cases_only_latest(direct.get()));
    ASSERT_NE(cases, nullptr);
    EXPECT_EQ(cases->getNodeType(), StyioNodeType::Cases);
  }
  {
    DirectContext direct("?= { _ => 1 }");
    auto followings = parse_forward_as_list(direct.get());
    ASSERT_EQ(followings.size(), 1u);
    std::unique_ptr<StyioAST> following(followings[0]);
    EXPECT_EQ(following->getNodeType(), StyioNodeType::Cases);
  }
  {
    DirectContext direct("?");
    EXPECT_THROW((void)parse_forward_as_list(direct.get()), StyioParseError);
  }
  {
    DirectContext direct(">>(item) => { << item }");
    std::unique_ptr<StyioAST> ast(parse_iterator_with_forward(
      direct.get(),
      ListAST::Create({IntAST::Create("1")})));
    auto* iter = dynamic_cast<IteratorAST*>(ast.get());
    ASSERT_NE(iter, nullptr);
    ASSERT_EQ(iter->following.size(), 1u);
    EXPECT_EQ(iter->following[0]->getNodeType(), StyioNodeType::Block);
  }
  {
    DirectContext direct(">>(item) > #tag => { << item }");
    std::unique_ptr<StyioAST> ast(parse_iterator_with_forward(
      direct.get(),
      ListAST::Create({IntAST::Create("1")})));
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::IterSeq);
  }
  {
    DirectContext direct(">>(item) > #tag => { << item } => 1");
    EXPECT_THROW(
      (void)parse_iterator_with_forward(direct.get(), ListAST::Create({IntAST::Create("1")})),
      StyioParseError);
  }
  {
    SCOPED_TRACE("iterator check-equal forward rejected");
    DirectContext direct(">>(item) ?= 1");
    EXPECT_THROW(
      (void)parse_iterator_with_forward(direct.get(), ListAST::Create({IntAST::Create("1")})),
      StyioParseError);
  }
  {
    DirectContext direct("map{item.name}");
    EXPECT_THROW((void)parse_codp(direct.get(), nullptr), StyioParseError);
  }
  {
    DirectContext direct("noop{}");
    CODPAST* previous = CODPAST::Create("filter", {}, nullptr);
    std::unique_ptr<CODPAST> pipeline(parse_codp(direct.get(), previous));
    ASSERT_NE(pipeline, nullptr);
  }
  {
    DirectContext direct("left, <- @stdin");
    EXPECT_THROW((void)parse_stmt_or_expr_legacy(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("left, right <- @stdin");
    EXPECT_THROW((void)parse_stmt_or_expr_legacy(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("left, right");
    std::unique_ptr<StyioAST> ast(parse_stmt_or_expr_legacy(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Id);
  }
  {
    DirectContext direct("{ 1");
    EXPECT_THROW((void)parse_block_only(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("@(123)");
    EXPECT_THROW((void)parse_read_file(direct.get(), NameAST::Create("input")), StyioSyntaxError);
  }
  {
    DirectContext direct("1");
    std::unique_ptr<BlockAST> body(parse_loop_body_clause(direct.get()));
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->stmts.size(), 1u);
    EXPECT_EQ(body->stmts[0]->getNodeType(), StyioNodeType::Integer);
  }
}

TEST(StyioParserInternal, ParserHelperFailureEdgesStayExplicit) {
  {
    DirectContext direct("name");
    StyioParserRouteStats outer;
    StyioParserRouteStats inner;
    direct.get().set_parser_route_stats_latest(&outer);
    {
      ParserRouteStatsScopeLatestDraft scope(direct.get(), &inner);
      EXPECT_EQ(direct.get().parser_route_stats_latest(), &inner);
    }
    EXPECT_EQ(direct.get().parser_route_stats_latest(), &outer);
  }
  {
    DirectContext direct("1");
    StyioParserRouteStats stats;
    stats.nightly_subset_statements = 3;
    std::unique_ptr<MainBlockAST> ast(parse_main_block_with_engine_latest(
      direct.get(),
      StyioParserEngine::Legacy,
      &stats));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(stats.nightly_subset_statements, 0u);
  }
  {
    EXPECT_STREQ(
      styio_parser_engine_name_latest(static_cast<StyioParserEngine>(255)),
      "invalid");
    DirectContext direct("2");
    StyioParserRouteStats stats;
    stats.nightly_declined_statements = 4;
    EXPECT_THROW(
      (void)parse_main_block_with_engine_latest(
        direct.get(),
        static_cast<StyioParserEngine>(255),
        &stats),
      StyioParseError);
    EXPECT_EQ(stats.nightly_declined_statements, 0u);
  }

  {
    try {
      throw StyioSyntaxError("syntax detail");
    }
    catch (...) {
      EXPECT_NE(parser_recovery_message_latest().find("syntax detail"), std::string::npos);
    }
    try {
      throw std::runtime_error("runtime detail");
    }
    catch (...) {
      EXPECT_NE(parser_recovery_message_latest().find("runtime detail"), std::string::npos);
    }
    try {
      throw 7;
    }
    catch (...) {
      EXPECT_EQ(parser_recovery_message_latest(), "unknown parser failure");
    }
  }

  {
    std::string nested(kMaxParserDelimiterNestingLatest, '(');
    nested += "x";
    DirectContext direct(nested);
    direct.get().move_forward(kMaxParserDelimiterNestingLatest);
    EXPECT_THROW(enforce_parser_delimiter_budget_latest(direct.get(), "test construct"), StyioParserResourceLimitError);
  }

  {
    DirectContext direct("name");
    EXPECT_THROW(reject_authoritative_nightly_gap_latest(direct.get(), "internal helper"), StyioSyntaxError);
  }

  {
    DirectContext direct("123");
    EXPECT_THROW((void)parse_name(direct.get()), StyioParseError);
  }
  {
    DirectContext direct("abc");
    EXPECT_THROW((void)parse_float(direct.get()), StyioParseError);
  }
  {
    DirectContext direct("|name");
    EXPECT_THROW((void)parse_size_of(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("|1|");
    EXPECT_THROW((void)parse_size_of(direct.get()), StyioParseError);
  }
  {
    DirectContext direct("[|name|]");
    EXPECT_THROW((void)parse_styio_type(direct.get()), StyioSyntaxError);
  }

  {
    auto seps = build_line_seps_for_embedded_expr_latest("");
    ASSERT_EQ(seps.size(), 1u);
    EXPECT_EQ(seps[0], (std::pair<size_t, size_t>{0, 0}));
    seps = build_line_seps_for_embedded_expr_latest("a\nbc");
    ASSERT_EQ(seps.size(), 2u);
    EXPECT_EQ(seps[0], (std::pair<size_t, size_t>{0, 1}));
    EXPECT_EQ(seps[1], (std::pair<size_t, size_t>{2, 2}));
    EXPECT_EQ(strip_string_token_quotes_latest("\"raw\""), "raw");
    EXPECT_EQ(strip_string_token_quotes_latest("raw"), "raw");
    EXPECT_TRUE(format_expr_text_is_empty_latest(" \n\t"));
    EXPECT_FALSE(format_expr_text_is_empty_latest(" value "));
    EXPECT_EQ(find_format_expr_end_latest("outer { nested } }", 0), 17u);
    EXPECT_THROW((void)find_format_expr_end_latest("unterminated {", 0), StyioSyntaxError);
    EXPECT_THROW((void)parse_format_expr_text_latest("   ", StyioParserEngine::Nightly), StyioSyntaxError);
    EXPECT_THROW((void)parse_format_expr_text_latest("1 2", StyioParserEngine::Nightly), StyioSyntaxError);
    EXPECT_THROW(
      (void)parse_format_expr_text_latest("1", static_cast<StyioParserEngine>(255)),
      StyioParseError);
  }

  {
    DirectContext direct("value");
    EXPECT_THROW((void)parse_fmt_str_token_latest(direct.get(), StyioParserEngine::Nightly), StyioParseError);
  }
  {
    DirectContext direct("$ name");
    EXPECT_THROW((void)parse_fmt_str_token_latest(direct.get(), StyioParserEngine::Nightly), StyioSyntaxError);
  }
  {
    DirectContext direct("\"left {{ brace }} right\"");
    std::unique_ptr<FmtStrAST> fmt(parse_fmt_str(direct.get()));
    ASSERT_NE(fmt, nullptr);
    ASSERT_EQ(fmt->getFragments().size(), 1u);
    EXPECT_EQ(fmt->getFragments()[0], "left { brace } right");
    EXPECT_TRUE(fmt->getExprs().empty());
  }
  {
    DirectContext direct("\"bad }\"");
    EXPECT_THROW((void)parse_fmt_str(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("$\"left {1 + 2} {{ brace }}\"");
    std::unique_ptr<FmtStrAST> fmt(parse_fmt_str_token_latest(direct.get(), StyioParserEngine::Nightly));
    ASSERT_NE(fmt, nullptr);
    ASSERT_EQ(fmt->getFragments().size(), 2u);
    EXPECT_EQ(fmt->getFragments()[0], "left ");
    EXPECT_EQ(fmt->getFragments()[1], " { brace }");
    ASSERT_EQ(fmt->getExprs().size(), 1u);
  }
  {
    DirectContext direct("$\"bad }\"");
    EXPECT_THROW((void)parse_fmt_str_token_latest(direct.get(), StyioParserEngine::Nightly), StyioSyntaxError);
  }
  {
    DirectContext direct("$\"{}\"");
    EXPECT_THROW((void)parse_fmt_str_token_latest(direct.get(), StyioParserEngine::Nightly), StyioSyntaxError);
  }
  {
    DirectContext direct("$\"{1}\"");
    EXPECT_THROW(
      (void)parse_fmt_str_token_latest(direct.get(), static_cast<StyioParserEngine>(255)),
      StyioParseError);
  }

  {
    DirectContext direct("{name}");
    EXPECT_THROW((void)parse_braced_string_path(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("(name)");
    EXPECT_THROW((void)parse_parenthesized_string_path(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("@()");
    EXPECT_THROW(
      (void)parse_resource_operand_after_at_latest(
        direct.get(),
        ResourceOperandPurposeLatest::InstantPullSource,
        "expected readable resource",
        false),
      StyioSyntaxError);
  }
  {
    DirectContext direct("<-(@stdin");
    EXPECT_THROW(
      (void)parse_parenthesized_instant_pull_latest(
        direct.get(),
        StyioTokenType::ARROW_SINGLE_LEFT,
        "expected readable resource",
        "expected ) after instant pull"),
      StyioSyntaxError);
  }
  {
    DirectContext direct("[\"pkg\"]\n");
    StyioParserRouteStats stats;
    EXPECT_THROW(
      (void)parse_main_block_with_engine_latest(direct.get(), StyioParserEngine::Nightly, &stats),
      StyioSyntaxError);
  }
}

TEST(StyioParserInternal, ImportExportAndRawBodyEdgesStayExplicit) {
  {
    DirectContext direct("import { pkg.core, util/io }");
    std::unique_ptr<ExtPackAST> ast(parse_import_decl_after_at_latest(direct.get()));
    ASSERT_NE(ast, nullptr);
    ASSERT_EQ(ast->getPaths().size(), 2u);
    EXPECT_EQ(ast->getPaths()[0], "pkg/core");
    EXPECT_EQ(ast->getPaths()[1], "util/io");
  }
  {
    DirectContext direct("export { main; mod.symbol }");
    std::unique_ptr<ExportDeclAST> ast(parse_export_decl_after_at_latest(direct.get()));
    ASSERT_NE(ast, nullptr);
    ASSERT_EQ(ast->getSymbols().size(), 2u);
    EXPECT_EQ(ast->getSymbols()[0], "main");
    EXPECT_EQ(ast->getSymbols()[1], "mod/symbol");
  }
  {
    DirectContext direct("(export { main })");
    direct.get().move_forward(1, "nested_export");
    EXPECT_THROW((void)parse_export_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("(import { pkg })");
    direct.get().move_forward(1, "nested_import");
    EXPECT_THROW((void)parse_import_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("module { pkg }");
    EXPECT_THROW((void)parse_import_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("import { }");
    EXPECT_THROW((void)parse_import_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("import { pkg.core/io }");
    EXPECT_THROW((void)parse_import_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("import { pkg other }");
    EXPECT_THROW((void)parse_import_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("import { pkg, }");
    EXPECT_THROW((void)parse_import_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("export { }");
    EXPECT_THROW((void)parse_export_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("export { symbol, }");
    EXPECT_THROW((void)parse_export_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("export { symbol helper }");
    EXPECT_THROW((void)parse_export_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("module { symbol }");
    EXPECT_THROW((void)parse_export_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("{ alpha { beta } gamma }");
    EXPECT_NE(parse_raw_braced_body_latest(direct.get(), "test").find("beta"), std::string::npos);
  }
  {
    DirectContext direct("{ alpha { beta }");
    EXPECT_THROW((void)parse_raw_braced_body_latest(direct.get(), "test"), StyioSyntaxError);
  }
  {
    DirectContext direct("@extern(c) { int native_body(void) { return 1; } }");
    std::unique_ptr<StyioAST> ast(parse_at_stmt_or_expr_latest(direct.get()));
    auto* extern_block = dynamic_cast<ExternBlockAST*>(ast.get());
    ASSERT_NE(extern_block, nullptr);
    EXPECT_EQ(extern_block->getAbi(), "c");
    EXPECT_NE(extern_block->getBody().find("native_body"), std::string::npos);
    EXPECT_TRUE(extern_block->getSourcePaths().empty());
  }
  {
    DirectContext direct("extern(c) => \"native/helper.h\"", false, "/tmp/styio-parser/project/main.styio");
    std::unique_ptr<ExternBlockAST> ast(parse_extern_decl_after_at_latest(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getAbi(), "c");
    ASSERT_EQ(ast->getSourcePaths().size(), 1u);
    EXPECT_EQ(ast->getSourcePaths()[0], native_path("/tmp/styio-parser/project/native/helper.h"));
    EXPECT_TRUE(ast->getBody().empty());
  }
  {
    DirectContext direct(
      "extern(c++) { \"native/a.h\", \"native/b.h\" }",
      false,
      "/tmp/styio-parser/project/main.styio");
    std::unique_ptr<ExternBlockAST> ast(parse_extern_decl_after_at_latest(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getAbi(), "c++");
    ASSERT_EQ(ast->getSourcePaths().size(), 2u);
    EXPECT_EQ(ast->getSourcePaths()[0], native_path("/tmp/styio-parser/project/native/a.h"));
    EXPECT_EQ(ast->getSourcePaths()[1], native_path("/tmp/styio-parser/project/native/b.h"));
    EXPECT_TRUE(ast->getBody().empty());
  }
  {
    DirectContext direct("", false, "/tmp/styio-parser/project/main.styio");
    auto source_paths = try_parse_extern_source_paths_from_body_latest(
      direct.get(),
      "\"native/a.h\", \"native/with\\\\escape.h\"");
    ASSERT_TRUE(source_paths.has_value());
    ASSERT_EQ(source_paths->size(), 2u);
    EXPECT_EQ((*source_paths)[0], native_path("/tmp/styio-parser/project/native/a.h"));
    EXPECT_NE((*source_paths)[1].find("with"), std::string::npos);
  }
  {
    DirectContext direct("extern(c) { int body { nested } }");
    std::unique_ptr<ExternBlockAST> ast(parse_extern_decl_after_at_latest(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getAbi(), "c");
    EXPECT_NE(ast->getBody().find("nested"), std::string::npos);
    EXPECT_TRUE(ast->getSourcePaths().empty());
  }
  {
    DirectContext direct("@extern(c) { int exported(void) { return 2; } }");
    std::unique_ptr<ExternBlockAST> ast(
      parse_bound_extern_after_at_latest(direct.get(), {"exported"}));
    ASSERT_NE(ast, nullptr);
    ASSERT_EQ(ast->getExportedSymbols().size(), 1u);
    EXPECT_EQ(ast->getExportedSymbols()[0], "exported");
  }
  {
    DirectContext direct("(@extern(c) { int nested(void) { return 0; } })");
    direct.get().move_forward(1, "nested_extern");
    EXPECT_THROW((void)parse_at_stmt_or_expr_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("import(c) {}");
    EXPECT_THROW((void)parse_extern_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("@extern(123) {}");
    direct.get().move_forward(1, "extern_bad_abi");
    EXPECT_THROW((void)parse_extern_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("@extern(rust) {}");
    direct.get().move_forward(1, "extern_unsupported_abi");
    EXPECT_THROW((void)parse_extern_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("@extern(c+) {}");
    direct.get().move_forward(1, "extern_bad_cpp_abi");
    EXPECT_THROW((void)parse_extern_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("@extern(c) => 123");
    direct.get().move_forward(1, "extern_bad_arrow_target");
    EXPECT_THROW((void)parse_extern_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
}

TEST(StyioParserInternal, ValueGuardIteratorAndPostfixEdgesStayExplicit) {
  {
    DirectContext direct("i64|999999999999999999999999999999999999999|");
    EXPECT_THROW((void)parse_styio_type(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("#(123)");
    EXPECT_THROW((void)parse_internal_resource_params_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("123 := #() => { ... }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("123");
    EXPECT_THROW((void)parse_resource_method_def_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("item");
    std::unique_ptr<StyioAST> ast(parse_var_name_or_value_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Id);
  }
  {
    SCOPED_TRACE("var index guard");
    DirectContext direct("item[0]");
    EXPECT_THROW((void)parse_var_name_or_value_expr(direct.get()), StyioParseError);
  }
  {
    SCOPED_TRACE("var call guard");
    DirectContext direct("item(0)");
    EXPECT_THROW((void)parse_var_name_or_value_expr(direct.get()), StyioParseError);
  }
  {
    DirectContext direct("42");
    std::unique_ptr<StyioAST> ast(parse_var_name_or_value_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectContext direct("3.5");
    std::unique_ptr<StyioAST> ast(parse_var_name_or_value_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Float);
  }
  {
    DirectContext direct("\"name\"");
    std::unique_ptr<StyioAST> ast(parse_var_name_or_value_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::String);
  }
  {
    SCOPED_TRACE("insert by index");
    DirectContext direct("[^2 <- 9]");
    direct.align_token(StyioTokenType::INTEGER);
    EXPECT_THROW((void)parse_index_op(direct.get(), NameAST::Create("items")), StyioSyntaxError);
  }
  {
    DirectContext direct("|value|");
    EXPECT_THROW((void)parse_value_expr(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("@");
    EXPECT_THROW((void)parse_value_expr(direct.get()), StyioParseError);
  }
  {
    DirectContext direct("'\n'");
    EXPECT_THROW((void)parse_expr(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("?(true) => 1 | 2");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::WaveMerge);
  }
  {
    DirectContext direct("?(true) => 1 2");
    EXPECT_THROW((void)parse_expr(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("1 ?= 2");
    EXPECT_THROW((void)parse_expr(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("base . 1");
    EXPECT_THROW((void)parse_expr(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("base.member.next");
    EXPECT_THROW((void)parse_expr(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("#tag > #next");
    std::unique_ptr<StyioAST> ast(parse_iterator_tail(
      direct.get(),
      ListAST::Create({IntAST::Create("1")})));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::IterSeq);
  }
  {
    DirectContext direct("# >");
    EXPECT_THROW(
      (void)parse_iterator_tail(direct.get(), ListAST::Create({IntAST::Create("1")})),
      StyioSyntaxError);
  }
  {
    DirectContext direct("(a) & other >> (b) => b");
    std::unique_ptr<StyioAST> ast(parse_iterator_tail(
      direct.get(),
      ListAST::Create({IntAST::Create("1")})));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::StreamZip);
  }
  {
    DirectContext direct("(a) & other => b");
    EXPECT_THROW(
      (void)parse_iterator_tail(direct.get(), ListAST::Create({IntAST::Create("1")})),
      StyioSyntaxError);
  }
  {
    DirectContext direct("(a) & other >> (b) b");
    EXPECT_THROW(
      (void)parse_iterator_tail(direct.get(), ListAST::Create({IntAST::Create("1")})),
      StyioSyntaxError);
  }
}

TEST(StyioParserInternal, InternalResourceAndMethodGuardsStayExplicit) {
  {
    DirectContext direct("stdin := #() => { <|[>_] }");
    std::unique_ptr<ResourceAST> ast(parse_internal_resource_decl_after_at_latest(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Resources);
  }
  {
    DirectContext direct("stdin := #(x) => { <|[>_] }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("stdout := #() => { x >> [>_] }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("stdout := #(x) => { 1 >> [>_] }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("stdout := #(x) => { y >> [>_] }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("stdout := #(x) => { x + [>_] }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("stdout := #(x) => { x >> sink }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("stderr := #() => { !(x) >> [>_] }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("stderr := #(x) => { !(1) >> [>_] }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("stderr := #(x) => { !(y) >> [>_] }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("stderr := #(x) => { !(x) + [>_] }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("file := #() => { ... }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("file: i64 := #() => { file(\"x\") }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("file: i64 := #() => { << 1 }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("custom := #() => { ... }");
    EXPECT_THROW((void)parse_internal_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("slot: i64 := 1");
    EXPECT_THROW((void)parse_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("name");
    EXPECT_FALSE(parse_at_name_colon_routes_to_internal_decl_latest(direct.get()));
    EXPECT_FALSE(parse_resource_method_route_after_at_latest(direct.get()));
  }
  {
    DirectContext direct("name: i64 := #()");
    EXPECT_TRUE(parse_at_name_colon_routes_to_internal_decl_latest(direct.get()));
  }
  {
    DirectContext direct("file::read = () => 1");
    EXPECT_TRUE(parse_resource_method_route_after_at_latest(direct.get()));
    std::unique_ptr<ResourceMethodDefAST> method(parse_resource_method_def_after_at_latest(direct.get()));
    ASSERT_NE(method, nullptr);
    EXPECT_FALSE(method->isProperty());
  }
  {
    DirectContext direct("file read = 1");
    EXPECT_THROW((void)parse_resource_method_def_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("file. = 1");
    EXPECT_THROW((void)parse_resource_method_def_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("file.path := \"x\"");
    std::unique_ptr<ResourceMethodDefAST> method(parse_resource_method_def_after_at_latest(direct.get()));
    ASSERT_NE(method, nullptr);
    EXPECT_TRUE(method->isFinalBinding());
    EXPECT_TRUE(method->isProperty());
  }
  {
    DirectContext direct("@[-1]");
    EXPECT_THROW((void)parse_resource_ref_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("price[1]");
    EXPECT_THROW((void)parse_resource_ref_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("price[-999999999999999999999]");
    EXPECT_THROW((void)parse_resource_ref_after_at_latest(direct.get()), StyioSyntaxError);
  }
}

TEST(StyioParserInternal, LegacyScannerRouteAndReceiverEdgesStayExplicit) {
  {
    DirectContext direct("{ value");
    EXPECT_FALSE(braced_region_contains_token_latest(direct.get(), StyioTokenType::AWAIT_PIPE));
    EXPECT_FALSE(braced_region_contains_token_outside_box_latest(direct.get(), StyioTokenType::MATCH));
    EXPECT_FALSE(file_resource_decl_body_calls_file_path_latest(direct.get()));
  }
  {
    DirectContext direct("prefix { [ ?= ]");
    EXPECT_FALSE(braced_region_contains_token_outside_box_latest(direct.get(), StyioTokenType::MATCH));
  }
  {
    DirectContext direct("{ file (\"path\")");
    EXPECT_TRUE(file_resource_decl_body_calls_file_path_latest(direct.get()));
  }
  {
    DirectContext direct("{ { } value");
    EXPECT_FALSE(file_resource_decl_body_calls_file_path_latest(direct.get()));
  }
  {
    DirectContext direct("123: i64");
    EXPECT_THROW((void)parse_resource_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("123: i64 := #()");
    EXPECT_FALSE(parse_at_name_colon_routes_to_internal_decl_latest(direct.get()));
  }
  {
    DirectContext direct("name: [|id|] := #()");
    EXPECT_THROW((void)parse_at_name_colon_routes_to_internal_decl_latest(direct.get()), StyioSyntaxError);
    EXPECT_EQ(direct.get().cur_tok_type(), StyioTokenType::NAME);
  }
  {
    DirectContext direct("123");
    EXPECT_FALSE(parse_resource_method_route_after_at_latest(direct.get()));
  }
  {
    DirectContext direct("file:: = 1");
    EXPECT_FALSE(parse_resource_method_route_after_at_latest(direct.get()));
    EXPECT_EQ(direct.get().cur_tok_type(), StyioTokenType::NAME);
  }
  {
    ResourceMethodReceiverScopeLatest scope("file");
    DirectContext direct("file(path)");
    EXPECT_EQ(try_parse_resource_receiver_expr_after_at_latest(direct.get()), nullptr);
    EXPECT_EQ(direct.get().cur_tok_type(), StyioTokenType::NAME);
  }
  {
    ResourceMethodReceiverScopeLatest scope("file");
    DirectContext direct("@file");
    std::unique_ptr<StyioAST> receiver(parse_at_expr_atom_latest(direct.get()));
    ASSERT_NE(receiver, nullptr);
    EXPECT_EQ(receiver->getNodeType(), StyioNodeType::ResourceReceiver);
  }
  {
    ResourceMethodReceiverScopeLatest scope("file");
    DirectContext direct("@file.name");
    std::unique_ptr<StyioAST> ast(parse_at_stmt_or_expr_latest(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Attribute);
  }
  {
    DirectContext direct("<- @stdin");
    EXPECT_THROW(
      (void)parse_parenthesized_instant_pull_latest(
        direct.get(),
        StyioTokenType::ARROW_SINGLE_LEFT,
        "expected readable resource",
        "expected ) after instant pull"),
      StyioSyntaxError);
  }
}

TEST(StyioParserInternal, ContextCharacterHelpersCoverBoundaryEdges) {
  {
    DirectContext direct("");
    EXPECT_NE(direct.get().label_cur_line(12, "empty-source").find("<empty>"), std::string::npos);
    direct.get().move_forward(1000, "past_eof");
    EXPECT_EQ(direct.get().mark_cur_tok("past-eof"), "past-eof");
    EXPECT_FALSE(direct.get().try_check(StyioTokenType::NAME));
    direct.get().skip();
    direct.get().skip_spaces_no_linebreak();
    EXPECT_EQ(direct.get().current_token_end_pos(), 0u);
  }
  {
    DirectContext direct("  // hidden\n/*block*/ ; tail");
    EXPECT_TRUE(direct.get().find_drop(';'));
    EXPECT_FALSE(direct.get().find_drop(';'));
  }
  {
    DirectContext direct("  /*block*/ => tail");
    EXPECT_TRUE(direct.get().find_drop(std::string("=>")));
    EXPECT_FALSE(direct.get().find_drop(std::string("=>")));
  }
  {
    DirectContext direct("abc");
    direct.get().restore_cursor({direct.get().get_token_index(), 4});
    EXPECT_FALSE(direct.get().check_next("a"));
    direct.get().restore_cursor({direct.get().get_token_index(), 3});
    EXPECT_TRUE(direct.get().check_next(std::string()));
    EXPECT_FALSE(direct.get().check_next("ab"));
    direct.get().move(99);
    EXPECT_EQ(direct.get().get_curr_pos(), 3u);
  }
  {
    DirectContext direct("+ * / /* comment */ // line\nx");
    EXPECT_TRUE(direct.get().check_binop());
    auto [ok_add, add_op] = direct.get().get_binop_token();
    EXPECT_TRUE(ok_add);
    EXPECT_EQ(add_op, StyioOpType::Binary_Add);

    direct.get().move(1);
    direct.get().drop_all_spaces();
    EXPECT_TRUE(direct.get().check_binop());
    auto [ok_mul, mul_op] = direct.get().get_binop_token();
    EXPECT_TRUE(ok_mul);
    EXPECT_EQ(mul_op, StyioOpType::Binary_Mul);

    direct.get().move(1);
    direct.get().drop_all_spaces();
    EXPECT_TRUE(direct.get().check_binop());
    auto [ok_div, div_op] = direct.get().get_binop_token();
    EXPECT_TRUE(ok_div);
    EXPECT_EQ(div_op, StyioOpType::Binary_Div);

    direct.get().move(1);
    direct.get().drop_all_spaces();
    EXPECT_FALSE(direct.get().check_binop());
    auto [ok_comment, comment_op] = direct.get().get_binop_token();
    EXPECT_FALSE(ok_comment);
    EXPECT_EQ(comment_op, StyioOpType::Comment_MultiLine);

    direct.get().drop_all_spaces_comments();
    EXPECT_EQ(direct.get().get_curr_char(), 'x');
    EXPECT_FALSE(direct.get().check_binop());
  }
  {
    DirectContext direct("// comment\n/");
    EXPECT_FALSE(direct.get().check_binop());
    auto [ok_comment, comment_op] = direct.get().get_binop_token();
    EXPECT_FALSE(ok_comment);
    EXPECT_EQ(comment_op, StyioOpType::Comment_SingleLine);
  }
  {
    DirectContext direct("name");
    EXPECT_THROW((void)direct.get().map_match(StyioTokenType::NAME), StyioSyntaxError);
    EXPECT_THROW((void)direct.get().try_match_panic(StyioTokenType::TOK_RPAREN), StyioSyntaxError);
    EXPECT_THROW((void)direct.get().check_drop_panic('!'), StyioSyntaxError);
  }
  {
    DirectContext direct("x\n", false, "<parser-debug>", true);
    testing::internal::CaptureStdout();
    const std::string label = direct.get().label_cur_line(0, "debug-label");
    const std::string debug_log = testing::internal::GetCapturedStdout();
    EXPECT_NE(debug_log.find("find_line_index(), starts with position: 0"), std::string::npos);
    EXPECT_NE(label.find("File \"<parser-debug>\""), std::string::npos);
    EXPECT_NE(label.find("debug-label"), std::string::npos);
  }
}

TEST(StyioParserInternal, ParserStaticHelpersCoverAdditionalFalseEdges) {
  {
    DirectContext direct("name");
    EXPECT_FALSE(has_linebreak_before_current_token_latest(direct.get()));
  }
  {
    DirectContext direct("[\"pkg\", ]");
    EXPECT_FALSE(matches_legacy_string_list_import_latest(direct.get()));
  }
  {
    DirectContext direct("[\"pkg\"");
    EXPECT_FALSE(matches_legacy_string_list_import_latest(direct.get()));
  }
  {
    DirectContext direct("[\"pkg\";]");
    EXPECT_FALSE(matches_legacy_string_list_import_latest(direct.get()));
  }
  {
    DirectContext direct("[\"pkg\",");
    EXPECT_FALSE(matches_legacy_string_list_import_latest(direct.get()));
  }
  {
    DirectContext direct(".pkg");
    EXPECT_THROW((void)parse_import_path_item_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("import { pkg. }");
    EXPECT_THROW((void)parse_import_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("module { symbol }");
    EXPECT_THROW((void)parse_export_decl_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("extern (c) { body }");
    EXPECT_EQ(strip_extern_source_string_latest("plain.styio"), "plain.styio");
    EXPECT_FALSE(try_parse_extern_source_paths_from_body_latest(direct.get(), "plain.styio").has_value());
    EXPECT_FALSE(try_parse_extern_source_paths_from_body_latest(direct.get(), "   ").has_value());
    EXPECT_FALSE(try_parse_extern_source_paths_from_body_latest(direct.get(), "\"unterminated").has_value());
    EXPECT_FALSE(try_parse_extern_source_paths_from_body_latest(direct.get(), "\"a.styio\",").has_value());
    auto paths = try_parse_extern_source_paths_from_body_latest(direct.get(), "\"a.styio\"; \"b.styio\"");
    ASSERT_TRUE(paths.has_value());
    ASSERT_EQ(paths->size(), 2u);
    EXPECT_NE((*paths)[0].find("a.styio"), std::string::npos);
    EXPECT_NE((*paths)[1].find("b.styio"), std::string::npos);
  }
  {
    DirectContext direct("'\\n'");
    std::unique_ptr<CharAST> ast(parse_char_literal_token_latest(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Char);
  }
  {
    DirectContext direct("123");
    std::unique_ptr<StyioAST> ast(parse_negative_numeric_literal_latest(direct.get()));
    auto* value = dynamic_cast<IntAST*>(ast.get());
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->getValue(), "-123");
  }
  {
    DirectContext direct("1.5");
    std::unique_ptr<StyioAST> ast(parse_negative_numeric_literal_latest(direct.get()));
    auto* value = dynamic_cast<FloatAST*>(ast.get());
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->getValue(), "-1.5");
  }
  {
    DirectContext direct("name");
    EXPECT_EQ(parse_negative_numeric_literal_latest(direct.get()), nullptr);
  }
  {
    auto expect_char = [](const std::string& source, const std::string& expected)
    {
      DirectContext direct(source);
      std::unique_ptr<CharAST> ast(parse_char_literal_token_latest(direct.get()));
      ASSERT_NE(ast, nullptr);
      EXPECT_EQ(ast->getValue(), expected);
    };
    expect_char("'\\r'", std::string(1, '\r'));
    expect_char("'\\t'", std::string(1, '\t'));
    expect_char("'\\0'", std::string(1, '\0'));
    expect_char("'\\\\'", "\\");
    expect_char("'\\''", "'");
    expect_char("'x'", "x");
  }
  {
    DirectContext direct("'\\q'");
    EXPECT_THROW((void)parse_char_literal_token_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("'ab'");
    EXPECT_THROW((void)parse_char_literal_token_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("'x");
    EXPECT_THROW((void)parse_char_literal_token_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("[|8|]");
    std::unique_ptr<TypeAST> type(parse_styio_type(direct.get()));
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->getTypeName(), "bounded_ring:8");
  }
  {
    DirectContext direct("list[i64]");
    std::unique_ptr<TypeAST> type(parse_styio_type(direct.get()));
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->getTypeName(), "list[i64]");
  }
  {
    DirectContext direct("dict[string, i64]");
    std::unique_ptr<TypeAST> type(parse_styio_type(direct.get()));
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->getTypeName(), "dict[string,i64]");
  }
  {
    DirectContext direct(
      "#(i64, #(string): bool): #(bool): f64");
    std::unique_ptr<TypeAST> type(parse_styio_type(direct.get()));
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(
      type->getTypeName(),
      "#(i64,#(string):bool):#(bool):f64");
    EXPECT_TRUE(styio_is_callable_type(type->getDataType()));
  }
  {
    DirectContext direct("#(i64): i64..");
    std::unique_ptr<TypeAST> type(parse_styio_type(direct.get()));
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->getTypeName(), "#(i64):i64..");
    EXPECT_EQ(
      styio_callable_result_type(type->getDataType()).name,
      "i64..");
  }
  {
    EXPECT_EQ(resource_suffix_value_type_latest(styio_make_list_type("i64")).name, "i64");
    EXPECT_EQ(resource_suffix_value_type_latest(styio_data_type_from_name("f64")).name, "f64");
  }
  {
    DirectContext direct("(\"typed\": i64, \"plain\", sink <- value)");
    std::unique_ptr<ResourceAST> resources(parse_resources_after_at(direct.get()));
    ASSERT_NE(resources, nullptr);
    ASSERT_EQ(resources->res_list.size(), 3u);
    EXPECT_EQ(resources->res_list[0].second, "i64");
    EXPECT_EQ(resources->res_list[1].second, "");
    EXPECT_EQ(resources->res_list[2].first->getNodeType(), StyioNodeType::FinalBind);
  }
  {
    DirectContext direct("()");
    std::unique_ptr<ResourceAST> resources(parse_resources_after_at(direct.get()));
    ASSERT_NE(resources, nullptr);
    EXPECT_TRUE(resources->res_list.empty());
  }
  {
    DirectContext direct("{\"auto.txt\"}");
    std::unique_ptr<StyioAST> path(parse_braced_string_path(direct.get()));
    ASSERT_NE(path, nullptr);
    EXPECT_EQ(path->getNodeType(), StyioNodeType::String);
  }
  {
    DirectContext direct("(\"manual.txt\")");
    std::unique_ptr<StyioAST> path(parse_parenthesized_string_path(direct.get()));
    ASSERT_NE(path, nullptr);
    EXPECT_EQ(path->getNodeType(), StyioNodeType::String);
  }
  {
    DirectContext direct("file(\"input.txt\")");
    std::unique_ptr<StyioAST> resource(parse_after_at_common(direct.get(), true));
    auto* file = dynamic_cast<FileResourceAST*>(resource.get());
    ASSERT_NE(file, nullptr);
    EXPECT_FALSE(file->isAutoDetect());
  }
  {
    DirectContext direct("{\"auto.txt\"}");
    std::unique_ptr<StyioAST> resource(parse_after_at_common(direct.get(), true));
    auto* file = dynamic_cast<FileResourceAST*>(resource.get());
    ASSERT_NE(file, nullptr);
    EXPECT_TRUE(file->isAutoDetect());
  }
  {
    EXPECT_TRUE(resource_operand_accepts_latest(
      StyioNodeType::FileResource,
      ResourceOperandPurposeLatest::FileAtom));
    EXPECT_TRUE(resource_operand_accepts_latest(
      StyioNodeType::InstantPull,
      ResourceOperandPurposeLatest::FileAtom));
    EXPECT_TRUE(resource_operand_accepts_latest(
      StyioNodeType::StdoutResource,
      ResourceOperandPurposeLatest::InstantPullSource));
    EXPECT_FALSE(resource_operand_accepts_latest(
      StyioNodeType::InstantPull,
      ResourceOperandPurposeLatest::InstantPullSource));
    EXPECT_TRUE(resource_operand_accepts_latest(
      StyioNodeType::EmptyResource,
      ResourceOperandPurposeLatest::SinkTarget));
    EXPECT_TRUE(resource_operand_accepts_latest(
      StyioNodeType::InstantPull,
      ResourceOperandPurposeLatest::SinkTarget));
    EXPECT_FALSE(resource_operand_accepts_latest(
      StyioNodeType::Integer,
      ResourceOperandPurposeLatest::SinkTarget));
  }
  {
    DirectContext direct("history");
    std::unique_ptr<ResourceRefAST> ref(parse_resource_ref_after_at_latest(direct.get()));
    ASSERT_NE(ref, nullptr);
    EXPECT_TRUE(ref->isWholeResource());
  }
  {
    DirectContext direct("history[...]");
    std::unique_ptr<ResourceRefAST> ref(parse_resource_ref_after_at_latest(direct.get()));
    ASSERT_NE(ref, nullptr);
    EXPECT_EQ(ref->getSelectorKind(), ResourceSelectorKind::SnapshotAll);
  }
  {
    DirectContext direct("history[-2...]");
    std::unique_ptr<ResourceRefAST> ref(parse_resource_ref_after_at_latest(direct.get()));
    ASSERT_NE(ref, nullptr);
    EXPECT_EQ(ref->getSelectorKind(), ResourceSelectorKind::SliceFrom);
    EXPECT_EQ(ref->getSelectorOffset(), -2);
  }
  {
    DirectContext direct("history[-3]");
    std::unique_ptr<ResourceRefAST> ref(parse_resource_ref_after_at_latest(direct.get()));
    ASSERT_NE(ref, nullptr);
    EXPECT_EQ(ref->getSelectorKind(), ResourceSelectorKind::Offset);
    EXPECT_EQ(ref->getSelectorOffset(), -3);
  }
  {
    DirectContext direct("history[name]");
    EXPECT_THROW((void)parse_resource_ref_after_at_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("@file(\"input.txt\")");
    std::unique_ptr<StyioAST> resource(parse_resource_file_atom_latest(direct.get()));
    ASSERT_NE(resource, nullptr);
    EXPECT_EQ(resource->getNodeType(), StyioNodeType::FileResource);
  }
  {
    DirectContext direct("@history[-1]");
    std::unique_ptr<StyioAST> resource(parse_resource_zip_collection_atom_latest(direct.get()));
    auto* ref = dynamic_cast<ResourceRefAST*>(resource.get());
    ASSERT_NE(ref, nullptr);
    EXPECT_EQ(ref->getSelectorKind(), ResourceSelectorKind::Offset);
  }
  {
    DirectContext direct("@stdout");
    std::unique_ptr<StyioAST> resource(
      parse_instant_pull_resource_atom_latest(direct.get(), "expected readable resource"));
    ASSERT_NE(resource, nullptr);
    EXPECT_EQ(resource->getNodeType(), StyioNodeType::StdoutResource);
  }
}

TEST(StyioParserInternal, UnifiedExpressionPostfixEdgesStayExplicit) {
  {
    std::unique_ptr<FuncCallAST> call(make_callable_apply_latest(
      AttrAST::Create(NameAST::Create("obj"), NameAST::Create("member")),
      IntAST::Create("1")));
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->getNodeType(), StyioNodeType::Call);
  }
  {
    DirectContext direct("fn <| 2");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Call);
  }
  {
    DirectContext direct("fn\n<| 2");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Id);
  }
  {
    DirectContext direct("fn(1)");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Call);
  }
  {
    DirectContext direct("items[0]");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Access_By_Index);
  }
  {
    DirectContext direct("series[avg, 3]");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* intrinsic = dynamic_cast<SeriesIntrinsicAST*>(ast.get());
    ASSERT_NE(intrinsic, nullptr);
    EXPECT_EQ(intrinsic->getOp(), SeriesIntrinsicOp::Avg);
  }
  {
    DirectContext direct("items[?, true]");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    EXPECT_NE(dynamic_cast<GuardSelectorAST*>(ast.get()), nullptr);
  }
  {
    DirectContext direct("items[?=, 7]");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    EXPECT_NE(dynamic_cast<EqProbeAST*>(ast.get()), nullptr);
  }
  {
    DirectContext direct("1[2]");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectContext direct("1 - 2");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* bin = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->getOp(), StyioOpType::Binary_Sub);
  }
  {
    DirectContext direct("1 ** 2");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* bin = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->getOp(), StyioOpType::Binary_Pow);
  }
  {
    DirectContext direct("1 / 2");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* bin = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->getOp(), StyioOpType::Binary_Div);
  }
  {
    DirectContext direct("1 % 2");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    auto* bin = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->getOp(), StyioOpType::Binary_Mod);
  }
  {
    DirectContext direct("1 <~");
    EXPECT_THROW((void)parse_expr(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("3.5");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Float);
  }
  {
    DirectContext direct("-1");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectContext direct("$\"value {1}\"");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::FmtStr);
  }
  {
    DirectContext direct("$ name");
    EXPECT_THROW((void)parse_expr(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("1\n[2]");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectContext direct("1\n(2)");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectContext direct("base . 1");
    EXPECT_THROW((void)parse_expr(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("callable(1)");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Call);
  }
  {
    DirectContext direct("f(x)(y)[0]");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Access_By_Index);
  }
  {
    DirectContext direct("xs[0...2]");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Access_By_Slice);
  }
  {
    DirectContext direct("fn\n(1)");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Id);
  }
  {
    DirectContext direct("xs\n[0]");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Id);
  }
  {
    DirectContext direct("@stdout\n(1)");
    std::unique_ptr<StyioAST> ast(parse_expr(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::StdoutResource);
  }
}

TEST(StyioParserInternal, ParserFuzzCorpusExercisesRecoveryRoutes) {
  std::vector<fs::path> corpus_paths;
  for (const auto& entry : fs::directory_iterator("tests/fuzz/corpus/parser")) {
    if (entry.is_regular_file()
        && entry.path().filename().string().rfind("seed-", 0) == 0) {
      corpus_paths.push_back(entry.path());
    }
  }
  std::sort(corpus_paths.begin(), corpus_paths.end());
  ASSERT_GT(corpus_paths.size(), 5u);

  for (const auto& path : corpus_paths) {
    const std::string source = read_file_text(path);
    SCOPED_TRACE(path.string());
    exercise_recovery_parser_for_source(source, StyioParserEngine::Legacy);
    exercise_recovery_parser_for_source(source, StyioParserEngine::Nightly);
  }
}

TEST(StyioParserInternal, LegacyCharacterParserBranchesStayExplicit) {
  {
    DirectContext direct("(1 == 1)");
    std::unique_ptr<CondAST> cond(parse_cond(direct.get()));
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->getNodeType(), StyioNodeType::Condition);
  }
  {
    DirectContext direct("(1 == 1 && 2 == 2 || 3 == 3)");
    std::unique_ptr<CondAST> cond(parse_cond(direct.get()));
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->getNodeType(), StyioNodeType::Condition);
  }
}
