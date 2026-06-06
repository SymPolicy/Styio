#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
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
    std::unique_ptr<ResourceDeclAST> decl(parse_resource_decl_v2_after_at_latest(direct.get()));
    ASSERT_NE(decl, nullptr);
    ASSERT_EQ(decl->getSlots().size(), 1u);
    ASSERT_NE(decl->getDriver(), nullptr);
  }
  {
    DirectContext direct("a: i64, @b: f64");
    std::unique_ptr<ResourceDeclAST> decl(parse_resource_decl_v2_after_at_latest(direct.get()));
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
    EXPECT_THROW((void)parse_resource_decl_v2_after_at_latest(direct.get()), StyioSyntaxError);
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
