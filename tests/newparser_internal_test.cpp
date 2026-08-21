#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "EnvTestUtil.hpp"
#include "StyioParser/Tokenizer.hpp"

#include "../src/StyioParser/NewParserExpr.cpp"

namespace {

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

void free_tokens(std::vector<StyioToken*>& tokens) {
  for (auto* token : tokens) {
    delete token;
  }
  tokens.clear();
}

class DirectContext {
 public:
  explicit DirectContext(std::string src)
    : source_(std::move(src)),
      tokens_(StyioTokenizer::tokenize(source_)),
      context_(StyioContext::Create(
        "<newparser-internal>",
        source_,
        build_line_seps(source_),
        tokens_,
        false))
  {
  }

  ~DirectContext() {
    delete context_;
    free_tokens(tokens_);
    StyioAST::destroy_all_tracked_nodes();
  }

  StyioContext& get() {
    return *context_;
  }

  const std::vector<StyioToken*>& tokens() const {
    return tokens_;
  }

 private:
  std::string source_;
  std::vector<StyioToken*> tokens_;
  StyioContext* context_ = nullptr;
};

class ManualTokenContext {
 public:
  explicit ManualTokenContext(std::vector<std::pair<StyioTokenType, std::string>> specs)
    : tokens_()
  {
    tokens_.reserve(specs.size());
    for (const auto& spec : specs) {
      tokens_.push_back(StyioToken::Create(spec.first, spec.second));
    }
    context_ = StyioContext::Create(
      "<newparser-manual-tokens>",
      "",
      {},
      tokens_,
      false);
  }

  ~ManualTokenContext() {
    delete context_;
    free_tokens(tokens_);
    StyioAST::destroy_all_tracked_nodes();
  }

  StyioContext& get() {
    return *context_;
  }

 private:
  std::vector<StyioToken*> tokens_;
  StyioContext* context_ = nullptr;
};

}  // namespace

TEST(StyioNewParserInternal, DefaultValuesRecoveryAndTokenProbesStayExplicit) {
  {
    std::unique_ptr<StyioAST> attr(AttrAST::Create(NameAST::Create("cpu"), NameAST::Create("pressure")));
    EXPECT_TRUE(is_pressure_observer_attr_latest(attr.get()));
    std::unique_ptr<StyioAST> other(AttrAST::Create(NameAST::Create("cpu"), NameAST::Create("load")));
    EXPECT_FALSE(is_pressure_observer_attr_latest(other.get()));
    EXPECT_FALSE(is_pressure_observer_attr_latest(IntAST::Create("1")));
  }
  const auto* greater_equal = styio_expr_operator_info(StyioTokenType::BINOP_GE);
  ASSERT_NE(greater_equal, nullptr);
  EXPECT_EQ(greater_equal->kind, StyioExprOperatorKind::Comparison);
  EXPECT_EQ(greater_equal->ast_op, StyioOpType::Greater_Than_Equal);
  const auto* less_alias = styio_expr_operator_info(StyioTokenType::TOK_LANGBRAC);
  ASSERT_NE(less_alias, nullptr);
  EXPECT_EQ(less_alias->kind, StyioExprOperatorKind::Comparison);
  EXPECT_EQ(less_alias->ast_op, StyioOpType::Less_Than);
  const auto* not_equal = styio_expr_operator_info(StyioTokenType::BINOP_NE);
  ASSERT_NE(not_equal, nullptr);
  EXPECT_EQ(not_equal->kind, StyioExprOperatorKind::Comparison);
  EXPECT_EQ(not_equal->ast_op, StyioOpType::Not_Equal);
  EXPECT_EQ(styio_expr_operator_info(StyioTokenType::TOK_SPACE), nullptr);
  const auto* logic_and = styio_expr_operator_info(StyioTokenType::LOGIC_AND);
  ASSERT_NE(logic_and, nullptr);
  EXPECT_EQ(logic_and->kind, StyioExprOperatorKind::Logic);
  EXPECT_EQ(logic_and->ast_op, StyioOpType::Logic_AND);
  const auto* logic_or = styio_expr_operator_info(StyioTokenType::LOGIC_OR);
  ASSERT_NE(logic_or, nullptr);
  EXPECT_EQ(logic_or->kind, StyioExprOperatorKind::Logic);
  EXPECT_EQ(logic_or->ast_op, StyioOpType::Logic_OR);
  const auto* fallback = styio_expr_operator_info(StyioTokenType::TOK_PIPE);
  ASSERT_NE(fallback, nullptr);
  EXPECT_EQ(fallback->kind, StyioExprOperatorKind::Fallback);

  auto allow_any = [](StyioTokenType) { return true; };
  auto stop_at_eof = [](StyioTokenType type, int, int, int, bool) {
    return type == StyioTokenType::TOK_EOF;
  };
  auto stop_never = [](StyioTokenType, int, int, int, bool) {
    return false;
  };
  {
    DirectContext direct("]");
    EXPECT_FALSE(scan_subset_route_tokens_latest(direct.tokens(), 0, allow_any, stop_never));
  }
  {
    DirectContext direct("|");
    EXPECT_FALSE(scan_subset_route_tokens_latest(direct.tokens(), 0, allow_any, stop_at_eof));
    EXPECT_TRUE(scan_subset_route_tokens_latest(direct.tokens(), 0, allow_any, stop_at_eof, true));
  }
  {
    DirectContext direct("[|");
    EXPECT_FALSE(scan_subset_route_tokens_latest(direct.tokens(), 0, allow_any, stop_never));
  }
  {
    DirectContext direct("{");
    EXPECT_FALSE(scan_subset_route_tokens_latest(direct.tokens(), 0, allow_any, stop_never));
  }
  {
    DirectContext direct(")");
    EXPECT_FALSE(scan_subset_route_tokens_latest(direct.tokens(), 0, allow_any, stop_never));
  }
  {
    DirectContext direct("await ?| x | y");
    auto allow_name_await_pipe = [](StyioTokenType type) {
      return type == StyioTokenType::NAME
        || type == StyioTokenType::TOK_SPACE
        || type == StyioTokenType::AWAIT_PIPE
        || type == StyioTokenType::TOK_PIPE;
    };
    EXPECT_TRUE(scan_subset_route_tokens_latest(direct.tokens(), 0, allow_name_await_pipe, stop_at_eof));
  }
  {
    DirectContext direct("name @");
    auto allow_names_only = [](StyioTokenType type) {
      return type == StyioTokenType::NAME;
    };
    EXPECT_FALSE(scan_subset_route_tokens_latest(direct.tokens(), 0, allow_names_only, stop_at_eof));
  }

  {
    std::unique_ptr<StyioAST> value(make_default_value_for_decl_latest(styio_data_type_from_name("bool")));
    EXPECT_EQ(value->getNodeType(), StyioNodeType::Bool);
  }
  {
    std::unique_ptr<StyioAST> value(make_default_value_for_decl_latest(styio_data_type_from_name("f64")));
    EXPECT_EQ(value->getNodeType(), StyioNodeType::Float);
  }
  {
    std::unique_ptr<StyioAST> value(make_default_value_for_decl_latest(styio_data_type_from_name("string")));
    EXPECT_EQ(value->getNodeType(), StyioNodeType::String);
  }
  {
    std::unique_ptr<StyioAST> value(make_default_value_for_decl_latest(styio_data_type_from_name("i64")));
    EXPECT_EQ(value->getNodeType(), StyioNodeType::Integer);
  }

  try {
    throw std::runtime_error("std failure");
  }
  catch (...) {
    EXPECT_EQ(nightly_recovery_message_latest(), "std failure");
  }
  try {
    throw 7;
  }
  catch (...) {
    EXPECT_EQ(nightly_recovery_message_latest(), "unknown nightly parser failure");
  }

  {
    DirectContext direct("bad token\nnext := 1");
    auto start = direct.get().save_cursor();
    EXPECT_FALSE(nightly_handle_recovery_latest(direct.get(), start, "ignored"));
    direct.get().set_parse_mode(StyioParseMode::Recovery);
    direct.get().move_forward(1);
    EXPECT_TRUE(nightly_handle_recovery_latest(direct.get(), start, "recovered"));
    ASSERT_EQ(direct.get().parse_diagnostics().size(), 1u);
    EXPECT_EQ(direct.get().parse_diagnostics()[0].message, "recovered");
  }

  {
    DirectContext direct("[\"a\", \"b\"]");
    EXPECT_TRUE(matches_legacy_string_list_import_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("name");
    EXPECT_FALSE(matches_legacy_string_list_import_nightly_latest(direct.get()));
    TokenProbeLatest probe(direct.tokens(), direct.tokens().size());
    EXPECT_EQ(probe.type(), StyioTokenType::TOK_EOF);
    EXPECT_FALSE(probe.take(StyioTokenType::NAME));
    EXPECT_FALSE(probe.advance());
  }
  {
    DirectContext direct("[\"a\" 7]");
    EXPECT_FALSE(matches_legacy_string_list_import_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("[\"a\", 7]");
    EXPECT_FALSE(matches_legacy_string_list_import_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("[\"a\"");
    EXPECT_FALSE(matches_legacy_string_list_import_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("[\"a\",");
    EXPECT_FALSE(matches_legacy_string_list_import_nightly_latest(direct.get()));
  }
  {
    ManualTokenContext direct({
      {StyioTokenType::TOK_LBOXBRAC, "["},
      {StyioTokenType::STRING, "\"a\""},
    });
    EXPECT_FALSE(matches_legacy_string_list_import_nightly_latest(direct.get()));
  }
  {
    ManualTokenContext direct({
      {StyioTokenType::TOK_LBOXBRAC, "["},
      {StyioTokenType::STRING, "\"a\""},
      {StyioTokenType::TOK_COMMA, ","},
      {StyioTokenType::STRING, "\"b\""},
    });
    EXPECT_FALSE(matches_legacy_string_list_import_nightly_latest(direct.get()));
  }
  {
    ManualTokenContext direct({});
    EXPECT_FALSE(stmt_subset_route_supported_latest(direct.get()));
    const auto saved = direct.get().save_cursor();
    auto attempt = try_parse_expr_subset_until_latest(direct.get(), {});
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Declined);
    EXPECT_EQ(attempt.node, nullptr);
    EXPECT_EQ(direct.get().save_cursor(), saved);
  }
  {
    ManualTokenContext direct({
      {StyioTokenType::AWAIT_PIPE, "?|"},
      {StyioTokenType::TOK_SPACE, " "},
    });
    EXPECT_FALSE(looks_like_await_bind_stmt_nightly(direct.get()));
  }
  {
    ManualTokenContext direct({
      {StyioTokenType::AWAIT_PIPE, "?|"},
      {StyioTokenType::TOK_SPACE, " "},
      {StyioTokenType::NAME, "source"},
    });
    EXPECT_FALSE(looks_like_await_bind_stmt_nightly(direct.get()));
  }
  {
    DirectContext direct("name");
    EXPECT_FALSE(can_route_hash_let_match_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("?| source");
    EXPECT_FALSE(looks_like_await_bind_stmt_nightly(direct.get()));
  }
  {
    DirectContext direct("name");
    TokenProbeLatest probe(direct.get());
    EXPECT_FALSE(consume_balanced_group_latest(
      probe,
      StyioTokenType::TOK_LPAREN,
      StyioTokenType::TOK_RPAREN));
  }
  {
    DirectContext direct("(");
    TokenProbeLatest probe(direct.get());
    EXPECT_FALSE(consume_balanced_group_latest(
      probe,
      StyioTokenType::TOK_LPAREN,
      StyioTokenType::TOK_RPAREN));
  }
  {
    DirectContext direct("(outer(inner))");
    TokenProbeLatest probe(direct.get());
    EXPECT_TRUE(consume_balanced_group_latest(
      probe,
      StyioTokenType::TOK_LPAREN,
      StyioTokenType::TOK_RPAREN));
  }
  {
    DirectContext direct("list[i64]");
    TokenProbeLatest probe(direct.get());
    EXPECT_TRUE(consume_hash_return_type_latest(probe));
  }
  {
    DirectContext direct("[| i64 |]");
    TokenProbeLatest probe(direct.get());
    EXPECT_TRUE(consume_hash_return_type_latest(probe));
  }
  {
    DirectContext direct("7");
    TokenProbeLatest probe(direct.get());
    EXPECT_FALSE(consume_hash_return_type_latest(probe));
  }
  {
    DirectContext direct("");
    EXPECT_FALSE(stmt_subset_route_supported_latest(direct.get()));
    const auto saved = direct.get().save_cursor();
    auto attempt = try_parse_expr_subset_until_latest(
      direct.get(), {StyioTokenType::TOK_EOF});
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Declined);
    EXPECT_EQ(attempt.node, nullptr);
    EXPECT_EQ(direct.get().save_cursor(), saved);
  }
  {
    DirectContext direct(")");
    EXPECT_FALSE(stmt_subset_route_supported_latest(direct.get()));
    const auto saved = direct.get().save_cursor();
    auto attempt = try_parse_expr_subset_until_latest(
      direct.get(), {StyioTokenType::TOK_EOF});
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Declined);
    EXPECT_EQ(attempt.node, nullptr);
    EXPECT_EQ(direct.get().save_cursor(), saved);
  }
  {
    DirectContext direct("}");
    auto allow_any = [](StyioTokenType) { return true; };
    auto stop_never = [](StyioTokenType, int, int, int, bool) {
      return false;
    };
    EXPECT_FALSE(scan_subset_route_tokens_latest(direct.tokens(), 0, allow_any, stop_never));
  }
  EXPECT_TRUE(styio_parser_expr_subset_token_nightly(StyioTokenType::TOK_SPACE));
  EXPECT_FALSE(styio_parser_expr_subset_token_nightly(StyioTokenType::TOK_HASH));
  {
    DirectContext direct("{ _ 1 }");
    EXPECT_THROW((void)parse_cases_only_nightly_draft(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("...");
    EXPECT_THROW((void)parse_iterator_collection_rhs_nightly_draft(direct.get()), StyioSyntaxError);
  }

  const std::vector<StyioExprOperatorInfo> expected_operators{
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
  ASSERT_EQ(expected_operators.size(), std::size(kStyioExprOperators));
  for (size_t i = 0; i < expected_operators.size(); ++i) {
    SCOPED_TRACE(i);
    const auto* actual = styio_expr_operator_info(expected_operators[i].token);
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(actual, &kStyioExprOperators[i]);
    EXPECT_EQ(actual->precedence, expected_operators[i].precedence);
    EXPECT_EQ(actual->associativity, expected_operators[i].associativity);
    EXPECT_EQ(actual->kind, expected_operators[i].kind);
    EXPECT_EQ(actual->ast_op, expected_operators[i].ast_op);
  }
  EXPECT_EQ(styio_expr_operator_info(StyioTokenType::BINOP_BITAND), nullptr);
  EXPECT_EQ(styio_expr_operator_info(StyioTokenType::BINOP_BITOR), nullptr);
  EXPECT_EQ(styio_expr_operator_info(StyioTokenType::BINOP_BITXOR), nullptr);
  EXPECT_EQ(styio_expr_operator_info(StyioTokenType::LOGIC_XOR), nullptr);
  EXPECT_EQ(styio_expr_operator_info(StyioTokenType::TOK_EQUAL), nullptr);
  EXPECT_EQ(styio_expr_operator_info(StyioTokenType::ARROW_SINGLE_RIGHT), nullptr);
}

TEST(StyioNewParserInternal, AwaitResourceEffectAndTaskLookaheadStayExplicit) {
  {
    DirectContext direct("?| task -> out: i64 | 0");
    EXPECT_TRUE(looks_like_await_bind_stmt_nightly(direct.get()));
    std::unique_ptr<StyioAST> ast(parse_await_bind_stmt_nightly(direct.get()));
    auto* bind = dynamic_cast<FlowBindAST*>(ast.get());
    ASSERT_NE(bind, nullptr);
    EXPECT_EQ(bind->getTarget()->getNameAsStr(), "out");
  }
  {
    DirectContext direct("name");
    EXPECT_FALSE(looks_like_await_bind_stmt_nightly(direct.get()));
  }
  {
    DirectContext direct("?|");
    EXPECT_FALSE(looks_like_await_bind_stmt_nightly(direct.get()));
  }
  {
    DirectContext direct("?| -> out: i64 | 0");
    EXPECT_THROW((void)parse_await_bind_stmt_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("?| task -> 1: i64");
    EXPECT_FALSE(looks_like_await_bind_stmt_nightly(direct.get()));
    EXPECT_THROW((void)parse_await_bind_stmt_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("?| task -> out");
    EXPECT_FALSE(looks_like_await_bind_stmt_nightly(direct.get()));
    EXPECT_THROW((void)parse_await_bind_stmt_nightly(direct.get()), StyioSyntaxError);
  }

  {
    DirectContext direct("handle <- @file(\"in.txt\")");
    EXPECT_TRUE(looks_like_resource_effect_handle_acquire_latest(direct.get()));
  }
  {
    DirectContext direct("handle = @file(\"in.txt\")");
    EXPECT_TRUE(looks_like_resource_effect_file_rebind_latest(direct.get()));
    std::unique_ptr<StyioAST> ast(parse_resource_effect_file_rebind_latest(direct.get()));
    ASSERT_NE(dynamic_cast<FlexBindAST*>(ast.get()), nullptr);
  }
  {
    DirectContext direct("repair => 1");
    EXPECT_TRUE(looks_like_resource_effect_named_handler_latest(direct.get()));
    ResourceEffectAST::Handler handler = parse_resource_effect_named_handler_latest(direct.get());
    EXPECT_EQ(handler.effect_name, "repair");
    EXPECT_NE(handler.body, nullptr);
  }
  {
    DirectContext direct("repair => ...");
    EXPECT_THROW((void)parse_resource_effect_named_handler_latest(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("?| 123 | 0");
    EXPECT_THROW((void)parse_resource_effect_expr_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("?| @stdout | ...");
    EXPECT_THROW((void)parse_resource_effect_expr_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("?| @stdout | first => 1 | ...");
    EXPECT_THROW((void)parse_resource_effect_stmt_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("?| (<< @file(\"missing\")) | io => 1 | ...");
    EXPECT_THROW((void)parse_resource_effect_stmt_nightly(direct.get()), StyioSyntaxError);
  }

  {
    DirectContext direct("||> [ ; task = { << 1 }, final := { << 2 } ]");
    std::unique_ptr<StyioAST> ast(parse_task_group_launch_nightly(direct.get()));
    ASSERT_NE(dynamic_cast<TaskGroupLaunchAST*>(ast.get()), nullptr);
  }
  {
    DirectContext direct("||> []");
    EXPECT_THROW((void)parse_task_group_launch_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("||> [ 1 = { << 1 } ]");
    EXPECT_THROW((void)parse_task_group_launch_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("||> [ task { << 1 } ]");
    EXPECT_THROW((void)parse_task_group_launch_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("||> [ task = 1 ]");
    EXPECT_THROW((void)parse_task_group_launch_nightly(direct.get()), StyioSyntaxError);
  }
}

TEST(StyioNewParserInternal, HashLetMatchAndSubsetDeclinesStayExplicit) {
  {
    DirectContext direct(
      "#(x = 1) ?= {\n"
      "  1 => { << \"one\" }\n"
      "  _ => { << \"other\" }\n"
      "}");
    auto attempt = try_parse_hash_let_match_nightly_latest(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Parsed);
    std::unique_ptr<StyioAST> ast(attempt.node);
    ASSERT_NE(dynamic_cast<BlockAST*>(ast.get()), nullptr);
  }
  {
    DirectContext direct("name");
    auto attempt = try_parse_hash_let_match_nightly_latest(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Declined);
  }
  {
    DirectContext direct("# 1");
    auto attempt = try_parse_hash_let_match_nightly_latest(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Declined);
  }
  {
    DirectContext direct("#(1 = 2) ?= { _ => 0 }");
    auto attempt = try_parse_hash_let_match_nightly_latest(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Fatal);
  }
  {
    DirectContext direct("#(x = 2) { _ => 0 }");
    auto attempt = try_parse_hash_let_match_nightly_latest(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Fatal);
  }
  {
    DirectContext direct("#(x = 2) ?= 1");
    auto attempt = try_parse_hash_let_match_nightly_latest(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Fatal);
  }

  {
    DirectContext direct("{ a := 1 }");
    auto attempt = try_parse_block_only_subset_nightly(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Parsed);
    std::unique_ptr<BlockAST> block(attempt.node);
    ASSERT_NE(block, nullptr);
  }
  {
    DirectContext direct("{ ?| -> out: i64 | 0 }");
    auto attempt = try_parse_block_only_subset_nightly(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Fatal);
  }
  {
    DirectContext direct("{ $state }");
    auto attempt = try_parse_block_only_subset_nightly(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Fatal);
  }
  {
    DirectContext direct("a := 1");
    auto attempt = try_parse_block_only_subset_nightly(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Declined);
  }
  {
    DirectContext direct("{ @extern }");
    auto attempt = try_parse_block_only_subset_nightly(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Fatal);
  }
  {
    ManualTokenContext direct({
      {StyioTokenType::TOK_LCURBRAC, "{"},
    });
    auto attempt = try_parse_block_only_subset_nightly(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Declined);
  }
  {
    DirectContext direct(")");
    EXPECT_THROW((void)parse_stmt_subset_with_legacy_fallback_latest_draft(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("a, 1 = 2");
    auto attempt = try_parse_stmt_subset_nightly(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Fatal);
    EXPECT_THROW((void)parse_stmt_subset_with_legacy_fallback_latest_draft(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct(")");
    EXPECT_THROW((void)parse_block_only_subset_with_legacy_fallback_latest_draft(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("{ ?| -> out: i64 | 0 }");
    EXPECT_THROW((void)parse_block_only_subset_with_legacy_fallback_latest_draft(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("name");
    auto attempt = try_parse_hash_stmt_nightly_latest(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Declined);
  }
}

TEST(StyioNewParserInternal, ForwardIteratorAndContinuationEdgesStayExplicit) {
  {
    DirectContext direct("?= { 1 => { << 1 } _ => { << 2 } }");
    std::vector<StyioAST*> followings = parse_forward_as_list_nightly_draft(direct.get());
    ASSERT_EQ(followings.size(), 1u);
    std::unique_ptr<StyioAST> owner(followings[0]);
    EXPECT_NE(dynamic_cast<CasesAST*>(owner.get()), nullptr);
  }
  {
    DirectContext direct("?= 1, 2");
    std::vector<StyioAST*> followings = parse_forward_as_list_nightly_draft(direct.get());
    ASSERT_EQ(followings.size(), 1u);
    std::unique_ptr<StyioAST> owner(followings[0]);
    EXPECT_NE(dynamic_cast<CheckEqualAST*>(owner.get()), nullptr);
  }
  {
    DirectContext direct("?= )");
    EXPECT_THROW((void)parse_forward_as_list_nightly_draft(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("?");
    EXPECT_THROW((void)parse_forward_as_list_nightly_draft(direct.get()), StyioParseError);
  }
  {
    DirectContext direct(">>next(1)");
    auto followings = parse_forward_as_list_nightly_draft(direct.get());
    EXPECT_TRUE(followings.empty());
    EXPECT_EQ(direct.get().cur_tok_type(), StyioTokenType::ITERATOR);
  }

  {
    DirectContext direct(">>(item) { << item }");
    std::unique_ptr<StyioAST> ast(parse_iterator_only_nightly_draft(
      direct.get(),
      ListAST::Create({IntAST::Create("1"), IntAST::Create("2")})));
    auto* iter = dynamic_cast<IteratorAST*>(ast.get());
    ASSERT_NE(iter, nullptr);
    ASSERT_EQ(iter->params.size(), 1u);
  }
  {
    DirectContext direct("?(1) => { << 1 }");
    std::unique_ptr<InfiniteLoopAST> loop(parse_infinite_after_double_right_nightly_latest(direct.get()));
    ASSERT_NE(loop, nullptr);
  }
  {
    DirectContext direct("before => after");
    std::unique_ptr<StyioAST> ast(parse_stmt_subset_nightly(direct.get()));
    ASSERT_NE(dynamic_cast<ResourceOrderAST*>(ast.get()), nullptr);
  }
  {
    DirectContext direct("name <~ 0");
    EXPECT_THROW(
      (void)parse_expr_core_allowing_follow_latest(direct.get(), {StyioTokenType::TOK_EOF}),
      StyioSyntaxError);
  }
  {
    DirectContext direct("name [0]");
    std::unique_ptr<StyioAST> ast(parse_expr_core_allowing_follow_latest(direct.get(), {}));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Access_By_Index);
  }
  {
    DirectContext direct("items[]");
    EXPECT_THROW((void)parse_expr_subset_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("value ?= 1");
    EXPECT_THROW((void)parse_expr_subset_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("42");
    std::unique_ptr<StyioAST> ast(parse_expr_subset_nightly(direct.get()));
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectContext direct("4.25");
    std::unique_ptr<StyioAST> ast(parse_expr_subset_nightly(direct.get()));
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Float);
  }
  {
    DirectContext direct("\"text\"");
    std::unique_ptr<StyioAST> ast(parse_expr_subset_nightly(direct.get()));
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::String);
  }
  {
    DirectContext direct("'x'");
    std::unique_ptr<StyioAST> ast(parse_expr_subset_nightly(direct.get()));
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Char);
  }
}

TEST(StyioNewParserInternal, RouteDictIteratorAndStatementHelpersStayExplicit) {
  {
    DirectContext direct("#(x = (1 + (2))) ?= { _ => { << 0 } }");
    EXPECT_TRUE(can_route_hash_let_match_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("#(x = 1");
    EXPECT_FALSE(can_route_hash_let_match_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("#(x = 1) ?= { _ => { << 0 } } | tail");
    EXPECT_FALSE(can_route_hash_let_match_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("#sum(a: i64): i64 => { << a }");
    EXPECT_TRUE(can_route_hash_stmt_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("#sum(a: i64): => { << a }");
    EXPECT_FALSE(can_route_hash_stmt_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("not_hash");
    EXPECT_FALSE(can_route_hash_stmt_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("#(1 = 1) match { _ => { << 0 } }");
    EXPECT_FALSE(can_route_hash_let_match_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("#(x = 1) { _ => { << 0 } }");
    EXPECT_FALSE(can_route_hash_let_match_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("#(x = 1) ?= 1");
    EXPECT_FALSE(can_route_hash_let_match_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("#sum(a: i64");
    EXPECT_FALSE(can_route_hash_stmt_nightly_latest(direct.get()));
  }
  {
    DirectContext direct("#sum: => { << 0 }");
    EXPECT_FALSE(can_route_hash_stmt_nightly_latest(direct.get()));
  }

  {
    DirectContext direct("{ \"a\": 1, \"b\": 2 }");
    std::unique_ptr<DictAST> dict(parse_dict_literal_nightly_draft(direct.get()));
    ASSERT_NE(dict, nullptr);
    EXPECT_EQ(dict->getNodeType(), StyioNodeType::Dict);
  }
  {
    DirectContext direct("{}");
    std::unique_ptr<DictAST> dict(parse_dict_literal_nightly_draft(direct.get()));
    ASSERT_NE(dict, nullptr);
    EXPECT_EQ(dict->getNodeType(), StyioNodeType::Dict);
  }
  {
    DirectContext direct("{ 1: ) }");
    EXPECT_THROW((void)parse_dict_literal_nightly_draft(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("[1, 2]");
    std::unique_ptr<StyioAST> ast(
      parse_list_expr_or_iterator_nightly_draft(direct.get(), false));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::List);
  }
  {
    DirectContext direct("[0..n]");
    std::unique_ptr<StyioAST> ast(
      parse_list_expr_or_iterator_nightly_draft(direct.get(), false));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Range);
  }
  {
    DirectContext direct("[0..n..2]");
    EXPECT_THROW(
      (void)parse_list_expr_or_iterator_nightly_draft(direct.get(), false),
      StyioSyntaxError);
  }
  {
    DirectContext direct("[1, 2] >>(item) => { << item }");
    std::unique_ptr<StyioAST> ast(
      parse_list_expr_or_iterator_nightly_draft(direct.get(), true));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Iterator);
  }

  {
    DirectContext direct("[1, 2] >>");
    std::unique_ptr<StyioAST> ast(parse_iterator_collection_rhs_nightly_draft(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::List);
  }
  {
    DirectContext direct("@stdin >>");
    std::unique_ptr<StyioAST> ast(parse_iterator_collection_rhs_nightly_draft(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::StdinResource);
  }
  {
    DirectContext direct("name >>");
    std::unique_ptr<StyioAST> ast(parse_iterator_collection_rhs_nightly_draft(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Id);
  }
  {
    DirectContext direct("name + 1");
    std::unique_ptr<StyioAST> ast(parse_iterator_collection_rhs_nightly_draft(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::BinOp);
  }
  {
    DirectContext direct("1 + 2");
    std::unique_ptr<StyioAST> ast(parse_iterator_collection_rhs_nightly_draft(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::BinOp);
  }
  {
    DirectContext direct("(");
    EXPECT_THROW((void)parse_iterator_collection_rhs_nightly_draft(direct.get()), StyioSyntaxError);
  }

  {
    DirectContext direct("<< 1");
    std::unique_ptr<BlockAST> block(parse_infinite_loop_body_nightly_draft(direct.get()));
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(block->getNodeType(), StyioNodeType::Block);
  }
  {
    DirectContext direct("{ _ => { << 0 } }");
    std::unique_ptr<CasesAST> cases(parse_cases_only_nightly_draft(direct.get()));
    ASSERT_NE(cases, nullptr);
    EXPECT_EQ(cases->getNodeType(), StyioNodeType::Cases);
  }
  {
    DirectContext direct("{ ) => { << 0 } }");
    EXPECT_THROW((void)parse_cases_only_nightly_draft(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("=> { << 1 }");
    std::vector<StyioAST*> followings = parse_forward_as_list_nightly_draft(direct.get());
    ASSERT_EQ(followings.size(), 1u);
    std::unique_ptr<StyioAST> owner(followings[0]);
    EXPECT_EQ(owner->getNodeType(), StyioNodeType::Block);
  }
  {
    DirectContext direct("?= (");
    EXPECT_THROW((void)parse_forward_as_list_nightly_draft(direct.get()), StyioSyntaxError);
  }

  {
    DirectContext direct("#first > #second");
    std::unique_ptr<StyioAST> ast(parse_iterator_tail_nightly_draft(
      direct.get(),
      ListAST::Create({IntAST::Create("1")})));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::IterSeq);
  }
  {
    DirectContext direct(">>(a) & [1] >>(b) => { << a }");
    std::unique_ptr<StyioAST> ast(parse_iterator_only_nightly_draft(
      direct.get(),
      ListAST::Create({IntAST::Create("1")})));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::StreamZip);
  }
  {
    DirectContext direct(">>(a) & [1] (b) => { << a }");
    EXPECT_THROW(
      (void)parse_iterator_only_nightly_draft(direct.get(), ListAST::Create({IntAST::Create("1")})),
      StyioSyntaxError);
  }
  {
    DirectContext direct(">>(a) & [1] >>(b) { << a }");
    EXPECT_THROW(
      (void)parse_iterator_only_nightly_draft(direct.get(), ListAST::Create({IntAST::Create("1")})),
      StyioSyntaxError);
  }
  {
    DirectContext direct(">>(a) > #tag");
    std::unique_ptr<StyioAST> ast(parse_iterator_only_nightly_draft(
      direct.get(),
      ListAST::Create({IntAST::Create("1")})));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::IterSeq);
  }
  {
    DirectContext direct(">>(a)");
    std::unique_ptr<StyioAST> ast(parse_iterator_only_nightly_draft(
      direct.get(),
      ListAST::Create({IntAST::Create("1")})));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Iterator);
  }

  {
    DirectContext direct("'a'");
    std::unique_ptr<StyioAST> ast(parse_expr_subset_nightly(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Char);
  }
  {
    DirectContext direct("'\\n'");
    std::unique_ptr<StyioAST> ast(parse_expr_subset_nightly(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Char);
  }
  {
    DirectContext direct("+1");
    std::unique_ptr<StyioAST> ast(parse_expr_subset_nightly(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectContext direct("'ab'");
    EXPECT_THROW((void)parse_expr_subset_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("'");
    EXPECT_THROW((void)parse_expr_subset_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("'a");
    EXPECT_THROW((void)parse_expr_subset_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("?(true) => 1 | 0");
    std::unique_ptr<StyioAST> ast(parse_expr_subset_nightly(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::WaveMerge);
  }
  {
    DirectContext direct("name [0]");
    std::unique_ptr<StyioAST> ast(parse_expr_subset_nightly(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Access_By_Index);
  }
  {
    DirectContext direct("name[...]");
    std::unique_ptr<StyioAST> ast(parse_expr_subset_nightly(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Access_By_Slice);
  }
  {
    DirectContext direct("name[1...]");
    std::unique_ptr<StyioAST> ast(parse_expr_subset_nightly(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Access_By_Slice);
  }
  {
    DirectContext direct("name[]");
    EXPECT_THROW((void)parse_expr_subset_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("name\n[0]");
    std::unique_ptr<StyioAST> ast(parse_expr_subset_nightly(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Id);
  }
  {
    DirectContext direct("'\\q'");
    EXPECT_THROW((void)parse_expr_subset_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("f(1, 2)");
    std::unique_ptr<StyioAST> ast(parse_expr_subset_nightly(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Call);
  }
  {
    DirectContext direct("||> 1");
    EXPECT_THROW((void)parse_expr_subset_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("||> 1");
    EXPECT_THROW((void)parse_stmt_subset_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("(1");
    EXPECT_THROW((void)parse_expr_subset_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct(">_(1, 2)");
    std::unique_ptr<PrintAST> ast(parse_print_nightly(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Print);
  }
  {
    DirectContext direct(">_()");
    std::unique_ptr<PrintAST> ast(parse_print_nightly(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Print);
  }
  {
    DirectContext direct("<- @stdin");
    std::unique_ptr<StyioAST> ast(parse_return_value_nightly(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::InstantPull);
  }
  {
    DirectContext direct("<- [>_]");
    std::unique_ptr<StyioAST> ast(parse_return_value_nightly(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::StdinResource);
  }
  {
    DirectContext direct("<- @file(\"x\")");
    std::unique_ptr<StyioAST> ast(parse_return_value_nightly(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::InstantPull);
  }
  {
    DirectContext direct("#)");
    EXPECT_THROW(
      (void)parse_iterator_tail_nightly_draft(direct.get(), ListAST::Create({IntAST::Create("1")})),
      StyioSyntaxError);
  }
  {
    DirectContext direct(">>(a) > item");
    EXPECT_THROW(
      (void)parse_iterator_only_nightly_draft(direct.get(), ListAST::Create({IntAST::Create("1")})),
      StyioSyntaxError);
  }
  {
    DirectContext direct("1");
    EXPECT_THROW(
      (void)parse_infinite_conditional_loop_after_iterator_nightly_draft(
        direct.get(),
        [&]() -> StyioAST* { return IntAST::Create("1"); }),
      StyioSyntaxError);
  }
  {
    DirectContext direct("?| @stdout | recover => 1 | ...");
    EXPECT_THROW((void)parse_resource_effect_expr_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("?| @file(\"missing\") | io => 1 | ...");
    EXPECT_THROW((void)parse_resource_effect_expr_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("?| @stdout | repair => 1 | ...");
    EXPECT_THROW((void)parse_resource_effect_stmt_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("?| fn([1], { x := 1 }) -> out: i64 | 0");
    EXPECT_TRUE(looks_like_await_bind_stmt_nightly(direct.get()));
  }
  {
    DirectContext direct("?| task | 0");
    EXPECT_FALSE(looks_like_await_bind_stmt_nightly(direct.get()));
  }
  {
    DirectContext direct("a, 1 = 2");
    EXPECT_THROW((void)parse_stmt_subset_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("a[0], b <- @stdin : (i64, i64)");
    EXPECT_THROW((void)parse_stmt_subset_nightly(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("a, b <- @file(\"x\")");
    EXPECT_THROW((void)parse_stmt_subset_nightly(direct.get()), StyioSyntaxError);
  }
}

TEST(StyioNewParserInternal, RouteCacheCountersStayAccurate) {
  // --- 1. Cache hit on repeated query at same start position ---
  {
    DirectContext direct("x := 1");
    StyioContext& ctx = direct.get();
    EXPECT_EQ(ctx.route_scan_count(), 0u);
    EXPECT_EQ(ctx.route_cache_hit_count(), 0u);
    EXPECT_EQ(ctx.route_cache_miss_count(), 0u);

    // First query: cache miss, triggers scan.
    EXPECT_TRUE(stmt_subset_route_supported_latest(ctx));
    EXPECT_GE(ctx.route_scan_count(), 1u);
    EXPECT_EQ(ctx.route_cache_hit_count(), 0u);
    EXPECT_EQ(ctx.route_cache_miss_count(), 1u);

    // Second query at same position: cache hit, no new scan.
    const size_t scans_before = ctx.route_scan_count();
    EXPECT_TRUE(stmt_subset_route_supported_latest(ctx));
    EXPECT_EQ(ctx.route_scan_count(), scans_before);
    EXPECT_GE(ctx.route_cache_hit_count(), 1u);
    EXPECT_EQ(ctx.route_cache_miss_count(), 1u);
  }

  // --- 2. Different start positions produce separate cache entries ---
  {
    DirectContext direct("x := 1\ny := 2\nz := 3");
    StyioContext& ctx = direct.get();
    ctx.clear_route_cache();

    // First stmt at position 0: miss.
    EXPECT_TRUE(stmt_subset_route_supported_latest(ctx));
    EXPECT_EQ(ctx.route_cache_miss_count(), 1u);

    // Advance past first stmt.
    ctx.move_forward(3);  // skip "x", ":=", "1"
    ctx.skip();
    EXPECT_TRUE(stmt_subset_route_supported_latest(ctx));
    // Second stmt at different start: should also be a miss (new position).
    EXPECT_EQ(ctx.route_cache_miss_count(), 2u);
  }

  // --- 3. Cache disabled via env var ---
  {
    // Set env to disable cache.
    styio_test_setenv("STYIO_PARSER_ROUTE_CACHE", "0", 1);

    DirectContext direct("x := 1");
    StyioContext& ctx = direct.get();
    ctx.clear_route_cache();  // clears counters

    // First call: disabled, so misses and scans.
    EXPECT_TRUE(stmt_subset_route_supported_latest(ctx));
    EXPECT_GE(ctx.route_scan_count(), 1u);
    EXPECT_EQ(ctx.route_cache_disabled_count(), 1u);

    // Second call: still disabled, should miss and scan again.
    const size_t scans_before = ctx.route_scan_count();
    EXPECT_TRUE(stmt_subset_route_supported_latest(ctx));
    EXPECT_GT(ctx.route_scan_count(), scans_before);
    EXPECT_GE(ctx.route_cache_disabled_count(), 2u);

    // Restore env (unset).
    styio_test_unsetenv("STYIO_PARSER_ROUTE_CACHE");
  }

  // --- 4. clear_route_cache resets counters ---
  {
    DirectContext direct("x := 1");
    StyioContext& ctx = direct.get();
    EXPECT_TRUE(stmt_subset_route_supported_latest(ctx));
    EXPECT_GT(ctx.route_scan_count(), 0u);

    ctx.clear_route_cache();
    EXPECT_EQ(ctx.route_scan_count(), 0u);
    EXPECT_EQ(ctx.route_cache_hit_count(), 0u);
    EXPECT_EQ(ctx.route_cache_miss_count(), 0u);
    EXPECT_EQ(ctx.route_cache_disabled_count(), 0u);

    // After reset, first query is a miss again.
    EXPECT_TRUE(stmt_subset_route_supported_latest(ctx));
    EXPECT_EQ(ctx.route_cache_miss_count(), 1u);
  }

  // --- 5. Canonical expression parsing reports useful work without a route pre-scan ---
  {
    DirectContext direct("1 + 2 * 3");
    StyioContext& ctx = direct.get();
    ctx.clear_route_cache();
    StyioParserRouteStats stats;
    ctx.set_parser_route_stats_latest(&stats);

    std::unique_ptr<StyioAST> ast(parse_expr(ctx));
    ctx.set_parser_route_stats_latest(nullptr);
    auto* add = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->getOp(), StyioOpType::Binary_Add);
    auto* multiply = dynamic_cast<BinOpAST*>(add->getRHS());
    ASSERT_NE(multiply, nullptr);
    EXPECT_EQ(multiply->getOp(), StyioOpType::Binary_Mul);
    EXPECT_EQ(ctx.route_scan_count(), 0u);
    EXPECT_GT(stats.expression_token_visits, 0u);
    EXPECT_GT(stats.expression_operator_probes, 0u);
    EXPECT_EQ(stats.expression_ast_nodes, 2u);
    EXPECT_EQ(stats.expression_scratch_allocations, 0u);
    EXPECT_GT(stats.expression_max_depth, 0u);
    EXPECT_LT(stats.expression_max_depth, kStyioExprMaxDepth);
    EXPECT_EQ(stats.legacy_fallback_statements, 0u);
    EXPECT_EQ(stats.nightly_internal_legacy_bridges, 0u);
  }
}

TEST(StyioNewParserInternal, CanonicalTryParsePreservesCursorAndFatalOwnership) {
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
}
