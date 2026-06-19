#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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

}  // namespace

TEST(StyioNewParserInternal, DefaultValuesRecoveryAndTokenProbesStayExplicit) {
  {
    std::unique_ptr<StyioAST> attr(AttrAST::Create(NameAST::Create("cpu"), NameAST::Create("pressure")));
    EXPECT_TRUE(is_pressure_observer_attr_latest(attr.get()));
    std::unique_ptr<StyioAST> other(AttrAST::Create(NameAST::Create("cpu"), NameAST::Create("load")));
    EXPECT_FALSE(is_pressure_observer_attr_latest(other.get()));
    EXPECT_FALSE(is_pressure_observer_attr_latest(IntAST::Create("1")));
  }
  EXPECT_TRUE(expr_is_comp(StyioTokenType::BINOP_GE));
  EXPECT_EQ(expr_map_comp(StyioTokenType::BINOP_GE), CompType::GE);
  EXPECT_EQ(expr_map_comp(StyioTokenType::TOK_LANGBRAC), CompType::LT);
  EXPECT_EQ(expr_map_comp(StyioTokenType::BINOP_NE), CompType::NE);
  EXPECT_EQ(expr_map_comp(StyioTokenType::TOK_SPACE), CompType::EQ);
  EXPECT_TRUE(expr_is_logic(StyioTokenType::LOGIC_AND));
  EXPECT_TRUE(expr_is_logic(StyioTokenType::LOGIC_OR));
  EXPECT_FALSE(expr_is_logic(StyioTokenType::TOK_PIPE));
  EXPECT_EQ(expr_map_logic(StyioTokenType::LOGIC_AND), LogicType::AND);
  EXPECT_EQ(expr_map_logic(StyioTokenType::LOGIC_OR), LogicType::OR);
  EXPECT_EQ(expr_map_logic(StyioTokenType::TOK_PIPE), LogicType::RAW);

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
    EXPECT_FALSE(expr_subset_route_supported_until_latest(direct.get(), {StyioTokenType::TOK_EOF}));
  }
  {
    DirectContext direct(")");
    EXPECT_FALSE(stmt_subset_route_supported_latest(direct.get()));
    EXPECT_FALSE(expr_subset_route_supported_until_latest(direct.get(), {StyioTokenType::TOK_EOF}));
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

  EXPECT_EQ(expr_prec_of(StyioTokenType::BINOP_GT), 40);
  EXPECT_EQ(expr_prec_of(StyioTokenType::TOK_RANGBRAC), 40);
  EXPECT_EQ(expr_prec_of(StyioTokenType::TOK_PLUS), 50);
  EXPECT_EQ(expr_prec_of(StyioTokenType::TOK_STAR), 60);
  EXPECT_EQ(expr_prec_of(StyioTokenType::BINOP_POW), 70);
  EXPECT_EQ(expr_prec_of(StyioTokenType::TOK_HASH), -1);
  EXPECT_TRUE(expr_is_right_assoc(StyioTokenType::BINOP_POW));
  EXPECT_FALSE(expr_is_right_assoc(StyioTokenType::TOK_STAR));
  EXPECT_EQ(expr_map_binop(StyioTokenType::TOK_PLUS), StyioOpType::Binary_Add);
  EXPECT_EQ(expr_map_binop(StyioTokenType::TOK_MINUS), StyioOpType::Binary_Sub);
  EXPECT_EQ(expr_map_binop(StyioTokenType::TOK_STAR), StyioOpType::Binary_Mul);
  EXPECT_EQ(expr_map_binop(StyioTokenType::TOK_SLASH), StyioOpType::Binary_Div);
  EXPECT_EQ(expr_map_binop(StyioTokenType::TOK_PERCENT), StyioOpType::Binary_Mod);
  EXPECT_EQ(expr_map_binop(StyioTokenType::BINOP_POW), StyioOpType::Binary_Pow);
  EXPECT_EQ(expr_map_binop(StyioTokenType::TOK_HASH), StyioOpType::Undefined);
  EXPECT_TRUE(expr_is_comp(StyioTokenType::BINOP_GT));
  EXPECT_TRUE(expr_is_comp(StyioTokenType::BINOP_LE));
  EXPECT_EQ(expr_map_comp(StyioTokenType::BINOP_GT), CompType::GT);
  EXPECT_EQ(expr_map_comp(StyioTokenType::BINOP_LE), CompType::LE);
  EXPECT_EQ(expr_map_comp(StyioTokenType::BINOP_EQ), CompType::EQ);
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
    DirectContext direct("a := 1");
    auto attempt = try_parse_block_only_subset_nightly(direct.get());
    EXPECT_EQ(attempt.status, ParseAttemptStatus::Declined);
  }
  {
    DirectContext direct(")");
    EXPECT_THROW((void)parse_stmt_subset_with_legacy_fallback_latest_draft(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct(")");
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
    std::unique_ptr<StyioAST> ast(parse_list_expr_or_iterator_nightly_draft(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::List);
  }
  {
    DirectContext direct("[1, 2] >>(item) => { << item }");
    std::unique_ptr<StyioAST> ast(parse_list_expr_or_iterator_nightly_draft(direct.get()));
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
    DirectContext direct("=> { << 1 }");
    std::vector<StyioAST*> followings = parse_forward_as_list_nightly_draft(direct.get());
    ASSERT_EQ(followings.size(), 1u);
    std::unique_ptr<StyioAST> owner(followings[0]);
    EXPECT_EQ(owner->getNodeType(), StyioNodeType::Block);
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
