#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "StyioParser/Tokenizer.hpp"

#include "../src/StyioParser/Parser.cpp"

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
  explicit DirectContext(std::string src, bool omit_equal_tokens = false)
    : source_(std::move(src)),
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
        "<parser-internal>",
        source_,
        build_line_seps(source_),
        tokens_,
        false);
  }

  ~DirectContext() {
    delete context_;
    free_tokens(tokens_);
    StyioAST::destroy_all_tracked_nodes();
  }

  StyioContext& get() {
    return *context_;
  }

 private:
  std::string source_;
  std::vector<StyioToken*> tokens_;
  StyioContext* context_ = nullptr;
};

}  // namespace

TEST(StyioParserInternal, ReassociatesResourceSinksAndParsesResourceHelpers) {
  {
    std::unique_ptr<StyioAST> ast(reassociate_add_into_resource_sink_latest_draft(
      StyioOpType::Binary_Add,
      StringAST::Create("prefix"),
      ResourceWriteAST::Create(
        StringAST::Create("payload"),
        StdStreamAST::Create(StdStreamKind::Stdout))));
    ASSERT_EQ(ast->getNodeType(), StyioNodeType::ResourceWrite);
    auto* write = static_cast<ResourceWriteAST*>(ast.get());
    ASSERT_NE(dynamic_cast<BinOpAST*>(write->getData()), nullptr);
  }
  {
    std::unique_ptr<StyioAST> ast(reassociate_add_into_resource_sink_latest_draft(
      StyioOpType::Binary_Add,
      StringAST::Create("prefix"),
      ResourceRedirectAST::Create(
        StringAST::Create("payload"),
        FileResourceAST::Create(StringAST::Create("out"), false))));
    ASSERT_EQ(ast->getNodeType(), StyioNodeType::ResourceRedirect);
    auto* redirect = static_cast<ResourceRedirectAST*>(ast.get());
    ASSERT_NE(dynamic_cast<BinOpAST*>(redirect->getData()), nullptr);
  }
  {
    std::unique_ptr<StyioAST> ast(reassociate_add_into_resource_sink_latest_draft(
      StyioOpType::Binary_Sub,
      IntAST::Create("3"),
      IntAST::Create("1")));
    ASSERT_EQ(ast->getNodeType(), StyioNodeType::BinOp);
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

TEST(StyioParserInternal, LegacyArgumentTypesAndBinopBranchesStayExplicit) {
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
    DirectContext direct("2 * 3");
    std::unique_ptr<StyioAST> ast(parse_binop_rhs(
      direct.get(),
      IntAST::Create("1"),
      StyioOpType::Binary_Add));
    auto* bin = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->getOp(), StyioOpType::Binary_Add);
    ASSERT_NE(dynamic_cast<BinOpAST*>(bin->getRHS()), nullptr);
  }
  {
    DirectContext direct("@stdin");
    std::unique_ptr<StyioAST> ast(parse_binop_item(direct.get()));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::StdinResource);
  }
  {
    DirectContext direct("1 + 2 * 3");
    std::unique_ptr<StyioAST> ast(parse_arithmetic_expr(direct.get()));
    auto* bin = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->getOp(), StyioOpType::Binary_Add);
    ASSERT_NE(dynamic_cast<BinOpAST*>(bin->getRHS()), nullptr);
  }
  {
    DirectContext direct("-name");
    std::unique_ptr<StyioAST> ast(parse_arithmetic_expr(direct.get()));
    auto* bin = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->getOp(), StyioOpType::Binary_Sub);
  }
  {
    DirectContext direct("<| 7");
    std::unique_ptr<StyioAST> ast(parse_arithmetic_expr(direct.get()));
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Return);
  }
  {
    DirectContext direct("|<| 8");
    std::unique_ptr<StyioAST> ast(parse_arithmetic_expr(direct.get()));
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Return);
  }
  {
    DirectContext direct("dict{}");
    std::unique_ptr<StyioAST> ast(parse_arithmetic_expr(direct.get()));
    ASSERT_EQ(ast->getNodeType(), StyioNodeType::Dict);
    EXPECT_TRUE(static_cast<DictAST*>(ast.get())->getEntries().empty());
  }
  {
    DirectContext direct("dict");
    std::unique_ptr<StyioAST> ast(parse_binop_item(direct.get()));
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
}

TEST(StyioParserInternal, LegacyOperatorForwardAndCodpEdgesStayExplicit) {
  {
    DirectContext direct("base ** 2");
    std::unique_ptr<StyioAST> ast(parse_name_and_following_unsafe(direct.get()));
    auto* bin = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->getOp(), StyioOpType::Binary_Pow);
  }
  {
    DirectContext direct("base / 2");
    std::unique_ptr<StyioAST> ast(parse_name_and_following_unsafe(direct.get()));
    auto* bin = dynamic_cast<BinOpAST*>(ast.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->getOp(), StyioOpType::Binary_Div);
  }
  {
    SCOPED_TRACE("dot without member");
    DirectContext direct("base . 1");
    EXPECT_THROW((void)parse_name_and_following_unsafe(direct.get()), StyioSyntaxError);
  }
  {
    DirectContext direct("+ 5");
    std::unique_ptr<StyioAST> ast(parse_arithmetic_expr(direct.get()));
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
    EXPECT_THROW((void)parse_arithmetic_expr(direct.get()), StyioSyntaxError);
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
    DirectContext direct("@(123)");
    EXPECT_THROW((void)parse_read_file(direct.get(), NameAST::Create("input")), StyioSyntaxError);
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
}

TEST(StyioParserInternal, LegacyExpressionPostfixEdgesStayExplicit) {
  {
    std::unique_ptr<FuncCallAST> call(make_callable_apply_latest(
      AttrAST::Create(NameAST::Create("obj"), NameAST::Create("member")),
      IntAST::Create("1")));
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->getNodeType(), StyioNodeType::Call);
  }
  {
    DirectContext direct("1\n[2]");
    direct.get().move_forward(1, "after_int");
    std::unique_ptr<StyioAST> ast(parse_arithmetic_tail_from_atom(direct.get(), IntAST::Create("1")));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectContext direct("1\n(2)");
    direct.get().move_forward(1, "after_int");
    std::unique_ptr<StyioAST> ast(parse_arithmetic_tail_from_atom(direct.get(), IntAST::Create("1")));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Integer);
  }
  {
    DirectContext direct(". 1");
    EXPECT_THROW((void)parse_expr_postfix(direct.get(), NameAST::Create("base")), StyioSyntaxError);
  }
  {
    DirectContext direct("(1)");
    std::unique_ptr<StyioAST> ast(parse_expr_postfix(direct.get(), NameAST::Create("callable")));
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->getNodeType(), StyioNodeType::Call);
  }
}
