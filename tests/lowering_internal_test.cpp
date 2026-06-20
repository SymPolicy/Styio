#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "StyioLowering/AstToStyioIRLowerer.hpp"
#include "StyioLowering/StyioIROptimizer.hpp"

#include "../src/StyioLowering/AstToStyioIR.cpp"
#define optimize_styio_ir optimize_styio_ir_internal_for_test
#include "../src/StyioLowering/StyioIROptimizer.cpp"
#undef optimize_styio_ir

namespace {

StyioDataType undefined_type() {
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

StyioDataType bool_type() {
  return StyioDataType{StyioDataTypeOption::Bool, "bool", 1};
}

StyioDataType i64_type() {
  return styio_data_type_from_name("i64");
}

SGVar* sg_i64_var(const std::string& name) {
  return SGVar::Create(SGResId::Create(name), SGType::Create(i64_type()));
}

void seed_builtin_resource_methods(AstToStyioIRLowerer& analyzer) {
  std::unique_ptr<MainBlockAST> seed(MainBlockAST::Create({PassAST::Create()}));
  seed->typeInfer(&analyzer);
}

class LowererProbe : public AstToStyioIRLowerer
{
public:
  using StyioSemaContext::binding_info_;
  using StyioSemaContext::resource_binding_types_;
  using StyioSemaContext::set_post_pulse_hist_context;
  using StyioSemaContext::snapshot_var_names_;
};

void exercise_to_ir(StyioAST* node) {
  AstToStyioIRLowerer analyzer;
  std::unique_ptr<StyioAST> owner(node);
  try {
    std::unique_ptr<StyioIR> ir(owner->toStyioIR(&analyzer));
  }
  catch (...) {
  }
}

void exercise_type_infer(StyioAST* node) {
  AstToStyioIRLowerer analyzer;
  std::unique_ptr<StyioAST> owner(node);
  try {
    owner->typeInfer(&analyzer);
  }
  catch (...) {
  }
}

void expect_cloned_node(StyioAST* node, StyioNodeType expected) {
  std::unique_ptr<StyioAST> owner(node);
  std::unique_ptr<StyioAST> cloned(clone_state_expr(owner.get()));
  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->getNodeType(), expected);
}

TEST(StyioLoweringInternal, FunctionTailAndMatchHelpersStayExplicit) {
  AstToStyioIRLowerer analyzer;

  std::unique_ptr<FunctionAST> regular(FunctionAST::Create(
    NameAST::Create("regular"),
    false,
    {ParamAST::Create(NameAST::Create("x"), TypeAST::Create("i64"))},
    TypeAST::Create("i64"),
    BlockAST::Create({ReturnAST::Create(NameAST::Create("x"))})
  ));
  std::unique_ptr<SimpleFuncAST> simple(SimpleFuncAST::Create(
    NameAST::Create("simple"),
    {ParamAST::Create(NameAST::Create("s"))},
    TypeAST::Create("string"),
    StringAST::Create("ok")
  ));
  std::unique_ptr<IntAST> plain(IntAST::Create("1"));
  EXPECT_EQ(params_of_func_def(regular.get()).size(), 1u);
  EXPECT_EQ(params_of_func_def(simple.get()).size(), 1u);
  EXPECT_TRUE(params_of_func_def(plain.get()).empty());

  {
    std::variant<TypeAST*, TypeTupleAST*> ret(static_cast<TypeAST*>(nullptr));
    std::unique_ptr<SGType> lowered(func_ret_to_sgtype(ret, &analyzer));
    EXPECT_EQ(lowered->data_type.name, "i64");
    EXPECT_TRUE(func_ret_is_unspecified(ret));
  }
  {
    std::variant<TypeAST*, TypeTupleAST*> ret(TypeAST::Create("f64"));
    std::unique_ptr<SGType> lowered(func_ret_to_sgtype(ret, &analyzer));
    EXPECT_EQ(lowered->data_type.name, "f64");
    delete std::get<TypeAST*>(ret);
  }
  {
    std::variant<TypeAST*, TypeTupleAST*> ret(TypeTupleAST::Create({TypeAST::Create("i64")}));
    EXPECT_THROW((void)func_ret_to_sgtype(ret, &analyzer), StyioTypeError);
    EXPECT_FALSE(func_ret_is_unspecified(ret));
    delete std::get<TypeTupleAST*>(ret);
  }

  EXPECT_EQ(param_data_type(nullptr).name, "i64");
  {
    std::unique_ptr<ParamAST> p(ParamAST::Create(NameAST::Create("p")));
    EXPECT_EQ(param_data_type(p.get()).name, "i64");
    std::unique_ptr<SGFuncArg> arg(param_to_sgarg(p.get(), &analyzer));
    EXPECT_EQ(arg->id, "p");
    EXPECT_EQ(arg->arg_type->data_type.name, "i64");
  }
  {
    std::unique_ptr<ParamAST> p(ParamAST::Create(NameAST::Create("q"), TypeAST::Create("bool")));
    EXPECT_EQ(param_data_type(p.get()).name, "bool");
    std::unique_ptr<SGFuncArg> arg(param_to_sgarg(p.get(), &analyzer));
    EXPECT_EQ(arg->arg_type->data_type.name, "bool");
  }

  EXPECT_TRUE(ast_is_statement_only_tail(nullptr));
  EXPECT_TRUE(ast_is_statement_only_tail(PassAST::Create()));
  EXPECT_FALSE(ast_is_statement_only_tail(IntAST::Create("7")));
  EXPECT_FALSE(ast_can_be_implicit_tail_value(ReturnAST::Create(IntAST::Create("1"))));
  EXPECT_TRUE(ast_can_be_implicit_tail_value(IntAST::Create("1")));
  EXPECT_TRUE(ast_has_tail_value(ReturnAST::Create(IntAST::Create("1"))));
  EXPECT_FALSE(ast_has_tail_value(PassAST::Create()));

  std::unique_ptr<BlockAST> block(BlockAST::Create({PassAST::Create()}));
  block->set_followings({PassAST::Create()});
  std::unique_ptr<SGBlock> lowered_block(lower_func_body(&analyzer, block.get(), true));
  ASSERT_EQ(lowered_block->stmts.size(), 2u);
  std::unique_ptr<SGBlock> one(lower_func_body(&analyzer, IntAST::Create("9"), true));
  ASSERT_EQ(one->stmts.size(), 1u);
  std::unique_ptr<StyioIR> tail_null(lower_tail_stmt(&analyzer, nullptr));
  ASSERT_NE(dynamic_cast<SGConstInt*>(tail_null.get()), nullptr);

  std::unique_ptr<BlockAST> defs(BlockAST::Create({
    FunctionAST::Create(
      NameAST::Create("inner_regular"),
      false,
      {},
      TypeAST::Create("i64"),
      BlockAST::Create({ReturnAST::Create(IntAST::Create("1"))})),
    SimpleFuncAST::Create(NameAST::Create("inner_simple"), {}, IntAST::Create("2"))
  }));
  register_direct_local_function_defs(&analyzer, defs.get());
  EXPECT_NE(analyzer.func_defs.find("inner_regular"), analyzer.func_defs.end());
  EXPECT_NE(analyzer.func_defs.find("inner_simple"), analyzer.func_defs.end());

  EXPECT_FALSE(stmt_has_return_tree(nullptr));
  EXPECT_TRUE(stmt_has_return_tree(ReturnAST::Create(IntAST::Create("1"))));
  {
    auto* cases = CasesAST::Create(
      {{IntAST::Create("1"), ReturnAST::Create(IntAST::Create("10"))}},
      PassAST::Create());
    std::unique_ptr<MatchCasesAST> match(MatchCasesAST::make(NameAST::Create("x"), cases));
    EXPECT_TRUE(stmt_has_return_tree(match.get()));
  }

  std::int64_t parsed = 0;
  EXPECT_TRUE(try_parse_int_literal_value(IntAST::Create("-8"), parsed));
  EXPECT_EQ(parsed, -8);
  EXPECT_FALSE(try_parse_int_literal_value(StringAST::Create("bad"), parsed));
  EXPECT_FALSE(try_parse_int_literal_value(IntAST::Create("999999999999999999999999"), parsed));
  EXPECT_TRUE(is_name_ast(NameAST::Create("x"), "x"));
  EXPECT_FALSE(is_name_ast(StringAST::Create("x"), "x"));
  EXPECT_EQ(match_case_pattern_value(IntAST::Create("3"), NameAST::Create("value")).value(), 3);
  EXPECT_EQ(
    match_case_pattern_value(
      new BinCompAST(CompType::EQ, NameAST::Create("value"), IntAST::Create("4")),
      NameAST::Create("value")).value(),
    4);
  EXPECT_FALSE(match_case_pattern_value(StringAST::Create("nope"), IntAST::Create("0")).has_value());

  const StyioDataType undefined = undefined_type();
  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType f64 = styio_data_type_from_name("f64");
  const StyioDataType string_type = styio_data_type_from_name("string");
  const StyioDataType char_type = styio_data_type_from_name("char");
  EXPECT_EQ(merge_tail_value_type(undefined, string_type).name, "string");
  EXPECT_EQ(merge_tail_value_type(string_type, undefined).name, "string");
  EXPECT_EQ(merge_tail_value_type(string_type, i64).name, "string");
  EXPECT_EQ(merge_tail_value_type(i64, f64).name, "f64");
  EXPECT_EQ(merge_tail_value_type(bool_type(), bool_type()).name, "bool");
  EXPECT_EQ(merge_tail_value_type(char_type, char_type).name, "char");
  EXPECT_EQ(merge_tail_value_type(bool_type(), char_type).name, "i64");

  EXPECT_TRUE(infer_tail_value_type(&analyzer, nullptr).isUndefined());
  EXPECT_EQ(
    infer_tail_value_type(&analyzer, ReturnAST::Create(FmtStrAST::Create({"tail"}, {}))).name,
    "string");
  EXPECT_TRUE(infer_tail_value_type(&analyzer, BlockAST::Create()).isUndefined());
  EXPECT_EQ(infer_tail_value_type(&analyzer, BlockAST::Create({FloatAST::Create("1.5")})).name, "f64");
  {
    std::unique_ptr<CasesAST> cases(CasesAST::Create(
      {
        {IntAST::Create("1"), ReturnAST::Create(IntAST::Create("7"))},
        {IntAST::Create("2"), ReturnAST::Create(FloatAST::Create("2.5"))},
      },
      ReturnAST::Create(IntAST::Create("0"))));
    EXPECT_EQ(infer_tail_value_type(&analyzer, cases.get()).name, "f64");
    EXPECT_EQ(classify_cases(cases.get()), SGMatchReprKind::ExprFloat);
  }
  {
    std::unique_ptr<MatchCasesAST> match(MatchCasesAST::make(
      NameAST::Create("x"),
      CasesAST::Create(
        {{IntAST::Create("1"), ReturnAST::Create(BoolAST::Create(true))}},
        ReturnAST::Create(BoolAST::Create(false)))));
    EXPECT_EQ(infer_tail_value_type(&analyzer, match.get()).name, "bool");
  }
  {
    std::unique_ptr<MatchCasesAST> nested_return(MatchCasesAST::make(
      NameAST::Create("x"),
      CasesAST::Create(
        {{IntAST::Create("1"), BlockAST::Create({ReturnAST::Create(IntAST::Create("1")), PassAST::Create()})}},
        PassAST::Create())));
    EXPECT_TRUE(stmt_has_return_tree(nested_return.get()));
  }

  bool has_string = false;
  bool has_int = false;
  bool has_float = false;
  scan_returns_for_value_kinds(IntAST::Create("3"), has_string, has_int, has_float);
  EXPECT_FALSE(has_string);
  EXPECT_TRUE(has_int);
  EXPECT_FALSE(has_float);
  has_string = has_int = has_float = false;
  scan_returns_for_value_kinds(
    BlockAST::Create({PassAST::Create(), FloatAST::Create("4.5")}),
    has_string,
    has_int,
    has_float);
  EXPECT_FALSE(has_string);
  EXPECT_FALSE(has_int);
  EXPECT_TRUE(has_float);

  {
    std::unique_ptr<CasesAST> stmt_cases(CasesAST::Create(
      {{IntAST::Create("1"), PassAST::Create()}},
      PassAST::Create()));
    EXPECT_EQ(classify_cases(stmt_cases.get()), SGMatchReprKind::Stmt);
  }
  {
    const StyioDataType bool_data_type = bool_type();
    std::unique_ptr<CasesAST> typed_cases(CasesAST::Create(
      {{IntAST::Create("1"), ReturnAST::Create(IntAST::Create("1"))}},
      ReturnAST::Create(IntAST::Create("0"))));
    EXPECT_EQ(classify_cases(typed_cases.get(), &string_type), SGMatchReprKind::ExprMixed);
    EXPECT_EQ(classify_cases(typed_cases.get(), &f64), SGMatchReprKind::ExprFloat);
    EXPECT_EQ(classify_cases(typed_cases.get(), &bool_data_type), SGMatchReprKind::ExprBool);
    EXPECT_EQ(classify_cases(typed_cases.get(), &char_type), SGMatchReprKind::ExprChar);
    EXPECT_EQ(classify_cases(typed_cases.get(), &i64), SGMatchReprKind::ExprInt);
    EXPECT_FALSE(match_repr_kind_for_type(styio_make_list_type("i64")).has_value());
  }
  {
    std::unique_ptr<CasesAST> string_cases(CasesAST::Create(
      {{IntAST::Create("1"), ReturnAST::Create(StringAST::Create("one"))}},
      ReturnAST::Create(StringAST::Create("default"))));
    EXPECT_EQ(classify_cases(string_cases.get()), SGMatchReprKind::ExprMixed);
  }
  {
    std::unique_ptr<CasesAST> mixed_cases(CasesAST::Create(
      {{IntAST::Create("1"), ReturnAST::Create(StringAST::Create("one"))}},
      ReturnAST::Create(IntAST::Create("0"))));
    EXPECT_EQ(classify_cases(mixed_cases.get()), SGMatchReprKind::ExprMixed);
  }
  {
    std::unique_ptr<CasesAST> bad_pattern(CasesAST::Create(
      {{StringAST::Create("bad"), ReturnAST::Create(IntAST::Create("1"))}},
      ReturnAST::Create(IntAST::Create("0"))));
    EXPECT_THROW(
      (void)lower_cases_with_scrutinee(&analyzer, bad_pattern.get(), SGConstInt::Create(0), nullptr),
      StyioTypeError);
  }
  {
    AstToStyioIRLowerer function_analyzer;
    std::unique_ptr<FunctionAST> bad_match_sugar(FunctionAST::Create(
      NameAST::Create("bad_match_sugar"),
      false,
      {},
      TypeAST::Create("i64"),
      CasesAST::Create(
        {{IntAST::Create("1"), ReturnAST::Create(IntAST::Create("1"))}},
        ReturnAST::Create(IntAST::Create("0")))));
    EXPECT_THROW((void)bad_match_sugar->toStyioIR(&function_analyzer), StyioTypeError);
  }
  {
    AstToStyioIRLowerer function_analyzer;
    std::unique_ptr<FunctionAST> throwing_body(FunctionAST::Create(
      NameAST::Create("throwing_body"),
      false,
      {ParamAST::Create(NameAST::Create("x"))},
      TypeAST::Create("i64"),
      BlockAST::Create({
        AttrAST::Create(NameAST::Create("x"), IntAST::Create("1"))
      })));
    EXPECT_THROW((void)throwing_body->toStyioIR(&function_analyzer), StyioTypeError);
  }
  {
    AstToStyioIRLowerer function_analyzer;
    std::unique_ptr<SimpleFuncAST> string_match(SimpleFuncAST::Create(
      NameAST::Create("string_match"),
      {ParamAST::Create(NameAST::Create("x"))},
      BlockAST::Create({
        MatchCasesAST::make(
          NameAST::Create("x"),
          CasesAST::Create(
            {{IntAST::Create("1"), ReturnAST::Create(StringAST::Create("one"))}},
            ReturnAST::Create(StringAST::Create("default"))))
      })));
    std::unique_ptr<StyioIR> ir(string_match->toStyioIR(&function_analyzer));
    ASSERT_NE(ir, nullptr);
  }
  {
    AstToStyioIRLowerer function_analyzer;
    std::unique_ptr<SimpleFuncAST> throwing_body(SimpleFuncAST::Create(
      NameAST::Create("throwing_simple"),
      {},
      AttrAST::Create(NameAST::Create("x"), IntAST::Create("1"))));
    EXPECT_THROW((void)throwing_body->toStyioIR(&function_analyzer), StyioTypeError);
  }
}

TEST(StyioLoweringInternal, OptimizerCanonicalizesRebindAndVisitsIrFamilies) {
  auto make_rebind_match = []() {
    return SGMatch::Create(
      SGResId::Create("source"),
      {},
      SGBlock::Create({
        SGFinalBind::Create(sg_i64_var("before"), SGConstInt::Create(1)),
        SGFlexBind::Create(sg_i64_var("memo"), SGResId::Create("source")),
        SGReturn::Create(SGResId::Create("memo")),
      }),
      SGMatchReprKind::ExprInt);
  };

  {
    std::unique_ptr<SGMainEntry> root(SGMainEntry::Create({make_rebind_match()}));
    EXPECT_EQ(styio::lowering::optimize_styio_ir(root.get()), root.get());
    auto* wrapper = dynamic_cast<SGBlock*>(root->stmts[0]);
    ASSERT_NE(wrapper, nullptr);
    ASSERT_EQ(wrapper->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGFlexBind*>(wrapper->stmts[0]), nullptr);
    auto* match = dynamic_cast<SGMatch*>(wrapper->stmts[1]);
    ASSERT_NE(match, nullptr);
    auto* scrutinee = dynamic_cast<SGResId*>(match->scrutinee);
    ASSERT_NE(scrutinee, nullptr);
    EXPECT_EQ(scrutinee->as_str(), "memo");
  }

  {
    std::unique_ptr<SGMainEntry> root(SGMainEntry::Create({
      SGFlexBind::Create(sg_i64_var("memo"), SGConstInt::Create(0)),
      make_rebind_match(),
    }));
    (void)styio::lowering::optimize_styio_ir(root.get());
    EXPECT_NE(dynamic_cast<SGMatch*>(root->stmts[1]), nullptr);
  }
  {
    std::unique_ptr<SGMainEntry> root(SGMainEntry::Create({
      make_rebind_match(),
      SGReturn::Create(SGResId::Create("memo")),
    }));
    (void)styio::lowering::optimize_styio_ir(root.get());
    EXPECT_NE(dynamic_cast<SGMatch*>(root->stmts[0]), nullptr);
  }
  {
    std::unique_ptr<SGMainEntry> root(SGMainEntry::Create({
      SGMatch::Create(
        SGResId::Create("source"),
        {},
        SGBlock::Create({
          SGFinalBind::Create(sg_i64_var("memo"), SGConstInt::Create(1)),
          SGFlexBind::Create(sg_i64_var("memo"), SGResId::Create("source")),
          SGReturn::Create(SGResId::Create("memo")),
        }),
        SGMatchReprKind::ExprInt),
    }));
    (void)styio::lowering::optimize_styio_ir(root.get());
    EXPECT_NE(dynamic_cast<SGMatch*>(root->stmts[0]), nullptr);
  }
  {
    std::unique_ptr<SGMainEntry> root(SGMainEntry::Create({
      SGMatch::Create(
        SGResId::Create("source"),
        {},
        SGBlock::Create({
          SGFlexBind::Create(sg_i64_var("memo"), SGResId::Create("source")),
        }),
        SGMatchReprKind::ExprInt),
    }));
    (void)styio::lowering::optimize_styio_ir(root.get());
    EXPECT_NE(dynamic_cast<SGMatch*>(root->stmts[0]), nullptr);
  }

  std::unique_ptr<SGMainEntry> families(SGMainEntry::Create({
    SGFlexBind::Create(
      sg_i64_var("sum"),
      SGBinOp::Create(
        SGCast::Create(SGConstBool::Create(true), SGType::Create(bool_type()), SGType::Create(i64_type())),
        SGConstInt::Create(2),
        StyioOpType::Binary_Add,
        SGType::Create(i64_type()),
        i64_type(),
        i64_type())),
    SGFinalBind::Create(sg_i64_var("cond"), SGCond::Create(SGConstInt::Create(1), SGConstInt::Create(2), StyioOpType::Less_Than)),
    SGFunc::Create(
      SGType::Create(i64_type()),
      SGResId::Create("nested"),
      {SGFuncArg::Create("x", SGType::Create(i64_type()))},
      SGBlock::Create({SGReturn::Create(SGResId::Create("x"))})),
    SGCall::Create(SGResId::Create("nested"), {SGConstInt::Create(3)}),
    SGLoop::CreateWhile(SGConstBool::Create(false), SGBlock::Create({SGContinue::Create()})),
    SGForEach::Create(
      SCListLiteral::Create({SGConstInt::Create(1), SGConstInt::Create(2)}, "i64"),
      "item",
      "i64",
      SGBlock::Create({SGReturn::Create(SGResId::Create("item"))})),
    SGRangeFor::Create(
      SGConstInt::Create(0),
      SGConstInt::Create(3),
      SGConstInt::Create(1),
      "i",
      SGBlock::Create({SGBreak::Create()})),
    SGIf::Create(
      SGConstBool::Create(true),
      SGBlock::Create({SGReturn::Create(SGConstInt::Create(1))}),
      SGBlock::Create({SGReturn::Create(SGConstInt::Create(0))})),
    SGMatch::Create(
      SGConstInt::Create(1),
      {{1, SGBlock::Create({SGReturn::Create(SGConstInt::Create(10))})}},
      SGBlock::Create({SGReturn::Create(SGConstInt::Create(0))}),
      SGMatchReprKind::ExprInt),
    SGSeriesAvgStep::Create(1, SGConstInt::Create(5)),
    SGSeriesMaxStep::Create(2, SGConstInt::Create(6)),
    SGFallback::Create(SGUndef::Create(), SGConstInt::Create(7)),
    SGWaveMerge::Create(SGConstBool::Create(true), SGConstInt::Create(8), SGConstInt::Create(9)),
    SGWaveDispatch::Create(SGConstBool::Create(false), SGReturn::Create(SGConstInt::Create(1)), SGReturn::Create(SGConstInt::Create(0))),
    SGGuardSelect::Create(SGResId::Create("stream"), SGConstBool::Create(true)),
    SGEqProbe::Create(SGResId::Create("stream"), SGConstInt::Create(4)),
    SGSnapshotDecl::Create("snap", SGConstString::Create("path")),
    SCListClone::Create(SGResId::Create("xs")),
    SCMatrixClone::Create(SGResId::Create("mat"), "i64"),
    SCListLen::Create(SGResId::Create("xs")),
    SCDictLen::Create(SGResId::Create("dict")),
    SCListGet::Create(SGResId::Create("xs"), SGConstInt::Create(0), "i64"),
    SCListSlice::Create(SGResId::Create("xs"), SGConstInt::Create(0), SGConstInt::Create(1), "i64"),
    SCDictGet::Create(SGResId::Create("dict"), SGConstString::Create("k"), "i64"),
    SCListSet::Create(SGResId::Create("xs"), SGConstInt::Create(0), SGConstInt::Create(1), "i64"),
    SCDictSet::Create(SGResId::Create("dict"), SGConstString::Create("k"), SGConstInt::Create(1), "i64"),
    SCListToString::Create(SGResId::Create("xs")),
    SCMatrixGet::Create(SGResId::Create("mat"), SGConstInt::Create(0), SGConstInt::Create(1), "i64"),
    SCMatrixRow::Create(SGResId::Create("mat"), SGConstInt::Create(0), "i64"),
    SCMatrixToString::Create(SGResId::Create("mat")),
    SCDictClone::Create(SGResId::Create("dict")),
    SCDictKeys::Create(SGResId::Create("dict")),
    SCDictValues::Create(SGResId::Create("dict"), "i64"),
    SCDictToString::Create(SGResId::Create("dict")),
    SIOHandleAcquire::Create("fh", SGConstString::Create("file.txt"), true),
    SIOHandleRelease::CreateFromPath(SGConstString::Create("file.txt"), true),
    SIOFileLineIter::CreateFromPath(SGConstString::Create("file.txt"), "line", SGBlock::Create({SIOPrint::Create({SGResId::Create("line")})})),
    SIOStreamZip::Create(
      SCListLiteral::Create({SGConstInt::Create(1)}, "i64"),
      false,
      false,
      "a",
      SCListLiteral::Create({SGConstInt::Create(2)}, "i64"),
      false,
      false,
      "b",
      false,
      false,
      "i64",
      "i64",
      SGBlock::Create({SGReturn::Create(SGResId::Create("a"))})),
    SIOInstantPull::Create(SGConstString::Create("file.txt")),
    SIOInstantPull::CreateFromHandle("fh"),
    SIOResourceWriteToFile::Create(SGConstString::Create("data"), SGConstString::Create("out.txt"), true, true),
    SIOStdStreamWrite::Create(SIOStdStreamWrite::Stream::Stdout, {SGConstString::Create("out")}),
    SIOResourceEffect::Create(
      SIOResourceWriteToFile::Create(SGConstString::Create("data"), SGConstString::Create("out.txt"), true, true),
      SGConstString::Create("fallback"),
      false,
      styio_data_type_from_name("string"),
      {SIOResourceEffect::Handler("io", SGConstString::Create("handled"))},
      true),
    SIOStdStreamLineIter::Create("line", SGBlock::Create({SIOPrint::Create({SGResId::Create("line")})})),
    SIOPrint::Create({SGConstString::Create("print")}),
    SIORead::Create(SIOPath::Create("in.txt")),
  }));
  EXPECT_EQ(styio::lowering::optimize_styio_ir(families.get()), families.get());
  ASSERT_FALSE(families->stmts.empty());
}

TEST(StyioLoweringInternal, OptimizerPrivateReadWriteAndEquivalenceEdgesStayExplicit) {
  using namespace styio::lowering;

  EXPECT_TRUE(is_speculatable_op(StyioOpType::Unary_Positive));
  EXPECT_TRUE(is_speculatable_op(StyioOpType::Unary_Negative));
  EXPECT_FALSE(ir_expr_is_speculatable(nullptr));
  EXPECT_TRUE(ir_expr_has_no_runtime_effects(nullptr));
  EXPECT_TRUE(stmt_is_rebind_hoist_transparent(nullptr));
  EXPECT_FALSE(stmt_writes_name(nullptr, "missing"));
  EXPECT_FALSE(stmt_reads_name(nullptr, "missing"));

  {
    std::unique_ptr<StyioIR> rhs(SGConstInt::Create(1));
    EXPECT_TRUE(ir_expr_equiv(nullptr, nullptr));
    EXPECT_FALSE(ir_expr_equiv(nullptr, rhs.get()));
    EXPECT_FALSE(ir_expr_equiv(rhs.get(), nullptr));
  }
  {
    std::unique_ptr<StyioIR> lhs(SGResId::Create("name"));
    std::unique_ptr<StyioIR> rhs(SGResId::Create("name"));
    std::unique_ptr<StyioIR> other(SGResId::Create("other"));
    EXPECT_TRUE(ir_expr_equiv(lhs.get(), rhs.get()));
    EXPECT_FALSE(ir_expr_equiv(lhs.get(), other.get()));
  }
  {
    std::unique_ptr<StyioIR> lhs(SGDynLoad::Create("slot", SGDynLoadKind::I64));
    std::unique_ptr<StyioIR> rhs(SGDynLoad::Create("slot", SGDynLoadKind::I64));
    std::unique_ptr<StyioIR> other(SGDynLoad::Create("slot", SGDynLoadKind::F64));
    EXPECT_TRUE(ir_expr_equiv(lhs.get(), rhs.get()));
    EXPECT_FALSE(ir_expr_equiv(lhs.get(), other.get()));
  }
  {
    std::unique_ptr<StyioIR> lhs(SGConstBool::Create(true));
    std::unique_ptr<StyioIR> rhs(SGConstBool::Create(true));
    std::unique_ptr<StyioIR> other(SGConstBool::Create(false));
    EXPECT_TRUE(ir_expr_equiv(lhs.get(), rhs.get()));
    EXPECT_FALSE(ir_expr_equiv(lhs.get(), other.get()));
  }
  {
    std::unique_ptr<StyioIR> lhs(SGConstInt::Create("7", 32));
    std::unique_ptr<StyioIR> rhs(SGConstInt::Create("7", 32));
    std::unique_ptr<StyioIR> other(SGConstInt::Create("7", 64));
    EXPECT_TRUE(ir_expr_equiv(lhs.get(), rhs.get()));
    EXPECT_FALSE(ir_expr_equiv(lhs.get(), other.get()));
  }
  {
    std::unique_ptr<StyioIR> lhs(SGConstFloat::Create("1.5"));
    std::unique_ptr<StyioIR> rhs(SGConstFloat::Create("1.5"));
    std::unique_ptr<StyioIR> other(SGConstFloat::Create("2.5"));
    EXPECT_TRUE(ir_expr_equiv(lhs.get(), rhs.get()));
    EXPECT_FALSE(ir_expr_equiv(lhs.get(), other.get()));
  }
  {
    std::unique_ptr<StyioIR> lhs(SGCast::Create(
      SGConstInt::Create(1),
      SGType::Create(i64_type()),
      SGType::Create(i64_type())));
    std::unique_ptr<StyioIR> rhs(SGCast::Create(
      SGConstInt::Create(1),
      SGType::Create(i64_type()),
      SGType::Create(i64_type())));
    EXPECT_TRUE(ir_expr_equiv(lhs.get(), rhs.get()));
    EXPECT_TRUE(ir_expr_is_speculatable(lhs.get()));
  }
  {
    std::unique_ptr<StyioIR> lhs(SGBinOp::Create(
      SGConstInt::Create(1),
      SGConstInt::Create(2),
      StyioOpType::Binary_Add,
      SGType::Create(i64_type()),
      i64_type(),
      i64_type()));
    std::unique_ptr<StyioIR> rhs(SGBinOp::Create(
      SGConstInt::Create(1),
      SGConstInt::Create(2),
      StyioOpType::Binary_Add,
      SGType::Create(i64_type()),
      i64_type(),
      i64_type()));
    EXPECT_TRUE(ir_expr_equiv(lhs.get(), rhs.get()));
    EXPECT_TRUE(ir_expr_is_speculatable(lhs.get()));
  }
  {
    std::unique_ptr<StyioIR> div(SGBinOp::Create(
      SGConstInt::Create(4),
      SGConstInt::Create(2),
      StyioOpType::Binary_Div,
      SGType::Create(i64_type()),
      i64_type(),
      i64_type()));
    EXPECT_FALSE(ir_expr_is_speculatable(div.get()));
    EXPECT_TRUE(ir_expr_has_no_runtime_effects(div.get()));
  }
  {
    std::unique_ptr<StyioIR> lhs(SGCond::Create(
      SGConstInt::Create(1),
      SGConstInt::Create(2),
      StyioOpType::Less_Than));
    std::unique_ptr<StyioIR> rhs(SGCond::Create(
      SGConstInt::Create(1),
      SGConstInt::Create(2),
      StyioOpType::Less_Than));
    EXPECT_TRUE(ir_expr_equiv(lhs.get(), rhs.get()));
    EXPECT_TRUE(ir_expr_is_speculatable(lhs.get()));
  }
  {
    std::unique_ptr<StyioIR> logic(SGCond::Create(
      SGConstBool::Create(true),
      SGConstBool::Create(false),
      StyioOpType::Logic_AND));
    EXPECT_FALSE(ir_expr_is_speculatable(logic.get()));
    EXPECT_TRUE(ir_expr_has_no_runtime_effects(logic.get()));
  }
  {
    std::unique_ptr<StyioIR> lhs(SCListLen::Create(SGResId::Create("xs")));
    std::unique_ptr<StyioIR> rhs(SCListLen::Create(SGResId::Create("xs")));
    EXPECT_TRUE(ir_expr_equiv(lhs.get(), rhs.get()));
    EXPECT_TRUE(ir_expr_is_speculatable(lhs.get()));
  }
  {
    std::unique_ptr<StyioIR> lhs(SCDictLen::Create(SGResId::Create("dict")));
    std::unique_ptr<StyioIR> rhs(SCDictLen::Create(SGResId::Create("dict")));
    EXPECT_TRUE(ir_expr_equiv(lhs.get(), rhs.get()));
    EXPECT_TRUE(ir_expr_is_speculatable(lhs.get()));
  }
  {
    std::unique_ptr<StyioIR> set(SCListSet::Create(
      SGResId::Create("xs"),
      SGResId::Create("idx"),
      SGResId::Create("value")));
    EXPECT_TRUE(stmt_writes_name(set.get(), "xs"));
    EXPECT_TRUE(stmt_reads_name(set.get(), "idx"));
    EXPECT_TRUE(stmt_reads_name(set.get(), "value"));
    EXPECT_FALSE(stmt_writes_name(set.get(), "value"));
  }
  {
    std::unique_ptr<StyioIR> set(SCDictSet::Create(
      SGDynLoad::Create("dict", SGDynLoadKind::DictHandle),
      SGResId::Create("key"),
      SGResId::Create("value")));
    EXPECT_TRUE(stmt_writes_name(set.get(), "dict"));
    EXPECT_TRUE(stmt_reads_name(set.get(), "key"));
    EXPECT_TRUE(stmt_reads_name(set.get(), "value"));
  }
  {
    std::unique_ptr<StyioIR> match(SGMatch::Create(
      SGResId::Create("scr"),
      {{1, SGBlock::Create({SGFlexBind::Create(sg_i64_var("arm"), SGResId::Create("scr"))})}},
      SGBlock::Create({SGReturn::Create(SGResId::Create("fallback"))}),
      SGMatchReprKind::Stmt));
    EXPECT_TRUE(stmt_writes_name(match.get(), "arm"));
    EXPECT_TRUE(stmt_reads_name(match.get(), "scr"));
    EXPECT_TRUE(stmt_reads_name(match.get(), "fallback"));
  }
  {
    std::unique_ptr<StyioIR> loop(SGLoop::CreateWhile(
      SGResId::Create("keep"),
      SGBlock::Create({SGFinalBind::Create(sg_i64_var("inside"), SGConstInt::Create(1))})));
    EXPECT_TRUE(stmt_reads_name(loop.get(), "keep"));
    EXPECT_TRUE(stmt_writes_name(loop.get(), "inside"));
  }
  {
    std::unique_ptr<StyioIR> each(SGForEach::Create(
      SGResId::Create("items"),
      "item",
      "i64",
      SGBlock::Create({SGReturn::Create(SGResId::Create("item"))})));
    EXPECT_TRUE(stmt_writes_name(each.get(), "item"));
    EXPECT_TRUE(stmt_reads_name(each.get(), "items"));
  }
  {
    std::unique_ptr<StyioIR> range(SGRangeFor::Create(
      SGResId::Create("start"),
      SGResId::Create("end"),
      SGResId::Create("step"),
      "i",
      SGBlock::Create({SGReturn::Create(SGResId::Create("i"))})));
    EXPECT_TRUE(stmt_writes_name(range.get(), "i"));
    EXPECT_TRUE(stmt_reads_name(range.get(), "start"));
    EXPECT_TRUE(stmt_reads_name(range.get(), "end"));
    EXPECT_TRUE(stmt_reads_name(range.get(), "step"));
  }
  {
    std::unique_ptr<StyioIR> iff(SGIf::Create(
      SGResId::Create("cond"),
      SGBlock::Create({SGFinalBind::Create(sg_i64_var("then_name"), SGConstInt::Create(1))}),
      SGBlock::Create({SGFinalBind::Create(sg_i64_var("else_name"), SGResId::Create("cond"))})));
    EXPECT_TRUE(stmt_reads_name(iff.get(), "cond"));
    EXPECT_TRUE(stmt_writes_name(iff.get(), "then_name"));
    EXPECT_TRUE(stmt_writes_name(iff.get(), "else_name"));
  }
  {
    std::unique_ptr<StyioIR> ret(SGReturn::Create(SGResId::Create("result")));
    EXPECT_TRUE(stmt_reads_name(ret.get(), "result"));
  }
}

TEST(StyioLoweringInternal, MatrixCollectionAndResourceHelpersStayExplicit) {
  AstToStyioIRLowerer analyzer;
  analyzer.local_binding_types["list_s"] = styio_make_list_type("string");
  analyzer.local_binding_types["list_i"] = styio_make_list_type("i64");
  analyzer.local_binding_types["dict_s"] = styio_make_dict_type("string", "string");
  analyzer.local_binding_types["matrix_i"] = styio_make_matrix_type("i64", 2, 2);
  analyzer.local_binding_types["matrix_f"] = styio_make_matrix_type("f64", 2, 2);

  EXPECT_FALSE(std_stream_kind_of(styio_data_type_from_name("i64")).has_value());
  EXPECT_EQ(std_stream_kind_of(styio_make_std_stream_type(StdStreamKind::Stdout)).value(), StdStreamKind::Stdout);

  auto generic_stream = styio_make_std_stream_type(StdStreamKind::Stdout);
  generic_stream.has_std_stream_kind = false;
  EXPECT_EQ(resource_family_for_lowering_type(styio_make_file_handle_type("i64")), "file");
  EXPECT_EQ(resource_family_for_lowering_type(generic_stream), "stream");
  EXPECT_EQ(resource_family_for_lowering_type(styio_make_std_stream_type(StdStreamKind::Stderr)), "stderr");
  EXPECT_EQ(
    resource_family_for_lowering_type(
      styio_make_topology_resource_type(styio_data_type_from_name("i64"), StyioResourceShapeKind::Scalar)),
    "resource");
  EXPECT_EQ(resource_family_for_lowering_type(undefined_type()), "");

  EXPECT_EQ(resource_family_for_lowering_expr(&analyzer, nullptr), "");
  EXPECT_EQ(resource_family_for_lowering_expr(&analyzer, FileResourceAST::Create(StringAST::Create("p"), false)), "file");
  EXPECT_EQ(resource_family_for_lowering_expr(&analyzer, StdStreamAST::Create(StdStreamKind::Stdout)), "stdout");
  EXPECT_EQ(resource_family_for_lowering_expr(&analyzer, ResourceReceiverAST::Create("bucket")), "bucket");
  EXPECT_EQ(resource_family_for_lowering_expr(&analyzer, ResourceRefAST::Create(NameAST::Create("r"))), "resource");

  EXPECT_EQ(predefined_list_operation_runtime_name("pop", styio_make_list_type("i64")), "__styio_list_pop");
  EXPECT_EQ(predefined_list_operation_runtime_name("append", styio_make_list_type("bool")), "__styio_list_append_bool");
  EXPECT_EQ(predefined_list_operation_runtime_name("append", styio_make_list_type("char")), "__styio_list_append_char");
  EXPECT_EQ(predefined_list_operation_runtime_name("append", styio_make_list_type("f64")), "__styio_list_append_f64");
  EXPECT_EQ(predefined_list_operation_runtime_name("append", styio_make_list_type("string")), "__styio_list_append_cstr");
  EXPECT_EQ(predefined_list_operation_runtime_name("append", styio_make_list_type("list[i64]")), "__styio_list_append_list");
  EXPECT_EQ(predefined_list_operation_runtime_name("append", styio_make_list_type("dict[string,i64]")), "__styio_list_append_dict");
  EXPECT_EQ(predefined_list_operation_runtime_name("append", styio_make_list_type("matrix")), "__styio_list_append_matrix");

  EXPECT_TRUE(collection_elem_is_string(&analyzer, NameAST::Create("list_s")));
  EXPECT_FALSE(collection_elem_is_string(&analyzer, NameAST::Create("list_i")));
  EXPECT_TRUE(collection_elem_is_string(&analyzer, ListAST::Create({StringAST::Create("a")})));
  EXPECT_FALSE(collection_elem_is_string(&analyzer, ListAST::Create()));

  auto lowered_type = [&](const std::string& name, std::vector<StyioAST*> args)
  {
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(NameAST::Create(name), std::move(args)));
    return matrix_intrinsic_lowered_type(&analyzer, call.get());
  };
  EXPECT_TRUE(matrix_intrinsic_lowered_type(&analyzer, nullptr).isUndefined());
  {
    std::unique_ptr<FuncCallAST> method(FuncCallAST::Create(
      NameAST::Create("receiver"),
      NameAST::Create("mat_rows"),
      {NameAST::Create("matrix_i")}));
    EXPECT_TRUE(matrix_intrinsic_lowered_type(&analyzer, method.get()).isUndefined());
  }
  EXPECT_EQ(lowered_type("mat_zeros_i64", {IntAST::Create("2"), IntAST::Create("3")}).name, "matrix[i64,2,3]");
  EXPECT_EQ(lowered_type("mat_identity", {IntAST::Create("3")}).name, "matrix[f64,3,3]");
  EXPECT_EQ(lowered_type("mat_shape", {NameAST::Create("matrix_i")}).name, "list[i64]");
  EXPECT_EQ(lowered_type("mat_cols", {NameAST::Create("matrix_i")}).name, "i64");
  EXPECT_EQ(lowered_type("mat_clone", {NameAST::Create("matrix_i")}).name, "matrix[i64,2,2]");
  EXPECT_EQ(lowered_type("mat_add", {NameAST::Create("matrix_i"), NameAST::Create("matrix_f")}).name, "matrix[f64,2,2]");
  EXPECT_EQ(lowered_type("mat_scale", {NameAST::Create("matrix_i"), FloatAST::Create("2.0")}).name, "matrix[f64,2,2]");
  EXPECT_EQ(lowered_type("mat_sum", {NameAST::Create("matrix_i")}).name, "i64");

  auto runtime_name = [&](const std::string& name, std::vector<StyioAST*> args)
  {
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(NameAST::Create(name), std::move(args)));
    return matrix_intrinsic_runtime_name(&analyzer, call.get());
  };
  EXPECT_EQ(runtime_name("mat_zeros", {IntAST::Create("1"), IntAST::Create("1")}), "__styio_matrix_new_f64");
  EXPECT_EQ(runtime_name("mat_identity_i64", {IntAST::Create("2")}), "__styio_matrix_identity_i64");
  EXPECT_EQ(runtime_name("mat_shape", {NameAST::Create("matrix_i")}), "__styio_matrix_shape");
  EXPECT_EQ(runtime_name("mat_get", {NameAST::Create("matrix_i"), IntAST::Create("0"), IntAST::Create("0")}), "__styio_matrix_get_i64");
  EXPECT_EQ(runtime_name("mat_set", {NameAST::Create("matrix_i"), IntAST::Create("0"), IntAST::Create("0"), IntAST::Create("5")}), "__styio_matrix_set_i64");
  EXPECT_EQ(runtime_name("mat_clone", {NameAST::Create("matrix_f")}), "__styio_matrix_clone_f64");
  EXPECT_EQ(runtime_name("mat_hadamard", {NameAST::Create("matrix_i"), NameAST::Create("matrix_i")}), "__styio_matrix_hadamard_i64");
  EXPECT_EQ(runtime_name("matmul", {NameAST::Create("matrix_f"), NameAST::Create("matrix_f")}), "__styio_matrix_matmul_f64");
  EXPECT_EQ(runtime_name("dot", {NameAST::Create("matrix_i"), NameAST::Create("matrix_i")}), "__styio_matrix_dot_i64");
  EXPECT_EQ(runtime_name("mat_sum", {NameAST::Create("matrix_f")}), "__styio_matrix_sum_f64");
}

TEST(StyioLoweringInternal, CloneResourceMethodAndPulseHelpersStayExplicit) {
  AstToStyioIRLowerer analyzer;

  {
    std::unique_ptr<StyioAST> cloned(clone_state_expr_with_subst(
      NameAST::Create("x"),
      "x",
      IntAST::Create("42")));
    ASSERT_EQ(cloned->getNodeType(), StyioNodeType::Integer);
    EXPECT_EQ(static_cast<IntAST*>(cloned.get())->getValue(), "42");
  }
  {
    std::unique_ptr<StyioAST> access(new ListOpAST(
      StyioNodeType::Access,
      NameAST::Create("xs"),
      NameAST::Create("slot")));
    std::unique_ptr<StyioAST> cloned(clone_state_expr_with_subst(access.get(), "", nullptr));
    EXPECT_EQ(cloned->getNodeType(), StyioNodeType::Access);
  }
  {
    std::unique_ptr<StyioAST> probe(HistoryProbeAST::Create(StateRefAST::Create(NameAST::Create("s")), IntAST::Create("1")));
    std::unique_ptr<StyioAST> cloned(clone_state_expr_with_subst(probe.get(), "s", NameAST::Create("not_state")));
    EXPECT_EQ(cloned->getNodeType(), StyioNodeType::HistoryProbe);
  }
  EXPECT_THROW((void)clone_state_expr_with_subst(NoneAST::Create(), "", nullptr), StyioTypeError);
  EXPECT_EQ(clone_resource_method_body_latest(nullptr, nullptr, {}), nullptr);

  expect_cloned_node(CommentAST::Create("note"), StyioNodeType::Comment);
  expect_cloned_node(EmptyAST::Create(), StyioNodeType::Empty);
  expect_cloned_node(FloatAST::Create("1.25"), StyioNodeType::Float);
  expect_cloned_node(BoolAST::Create(true), StyioNodeType::Bool);
  expect_cloned_node(StringAST::Create("text"), StyioNodeType::String);
  expect_cloned_node(CharAST::Create("x"), StyioNodeType::Char);
  expect_cloned_node(TupleAST::Create({IntAST::Create("1"), StringAST::Create("s")}), StyioNodeType::Tuple);
  expect_cloned_node(ListAST::Create({IntAST::Create("1"), IntAST::Create("2")}), StyioNodeType::List);
  expect_cloned_node(
    DictAST::Create({{StringAST::Create("k"), IntAST::Create("1")}}),
    StyioNodeType::Dict);
  expect_cloned_node(SetAST::Create({IntAST::Create("1")}), StyioNodeType::Set);
  expect_cloned_node(BinOpAST::Create(StyioOpType::Binary_Mul, IntAST::Create("2"), IntAST::Create("3")), StyioNodeType::BinOp);
  expect_cloned_node(new BinCompAST(CompType::GT, IntAST::Create("3"), IntAST::Create("2")), StyioNodeType::Compare);
  expect_cloned_node(CondAST::Create(LogicType::NOT, BoolAST::Create(false)), StyioNodeType::Condition);
  expect_cloned_node(CondAST::Create(LogicType::AND, BoolAST::Create(true), BoolAST::Create(false)), StyioNodeType::Condition);
  expect_cloned_node(WaveMergeAST::Create(BoolAST::Create(true), IntAST::Create("1"), IntAST::Create("0")), StyioNodeType::WaveMerge);
  expect_cloned_node(WaveDispatchAST::Create(BoolAST::Create(true), PassAST::Create(), BreakAST::Create()), StyioNodeType::WaveDispatch);
  expect_cloned_node(FallbackAST::Create(UndefinedLitAST::Create(), IntAST::Create("5")), StyioNodeType::Fallback);
  expect_cloned_node(FmtStrAST::Create({"left ", ""}, {IntAST::Create("1")}), StyioNodeType::FmtStr);
  expect_cloned_node(GuardSelectorAST::Create(NameAST::Create("stream"), BoolAST::Create(true)), StyioNodeType::GuardSelector);
  expect_cloned_node(EqProbeAST::Create(NameAST::Create("stream"), IntAST::Create("1")), StyioNodeType::EqProbeSelector);
  expect_cloned_node(new RangeAST(IntAST::Create("0"), IntAST::Create("3"), IntAST::Create("1")), StyioNodeType::Range);
  expect_cloned_node(TypeConvertAST::Create(IntAST::Create("1"), NumPromoTy::Int_To_Float), StyioNodeType::NumConvert);
  expect_cloned_node(new ListOpAST(StyioNodeType::Get_Reversed, NameAST::Create("xs")), StyioNodeType::Get_Reversed);
  expect_cloned_node(new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("xs"), IntAST::Create("0")), StyioNodeType::Access_By_Index);
  expect_cloned_node(
    new ListOpAST(StyioNodeType::Access_By_Slice, NameAST::Create("xs"), IntAST::Create("0"), IntAST::Create("2")),
    StyioNodeType::Access_By_Slice);
  expect_cloned_node(FuncCallAST::Create(NameAST::Create("f"), {IntAST::Create("1")}), StyioNodeType::Call);
  expect_cloned_node(
    FuncCallAST::Create(NameAST::Create("receiver"), NameAST::Create("method"), {StringAST::Create("x")}),
    StyioNodeType::Call);
  expect_cloned_node(AttrAST::Create(NameAST::Create("dict"), NameAST::Create("keys")), StyioNodeType::Attribute);
  expect_cloned_node(FileResourceAST::Create(StringAST::Create("out.txt"), true), StyioNodeType::FileResource);
  expect_cloned_node(StdStreamAST::CreateTerminalHandle(StdStreamKind::Stdout), StyioNodeType::StdoutResource);
  expect_cloned_node(EmptyResourceAST::Create(), StyioNodeType::EmptyResource);
  expect_cloned_node(ResourceReceiverAST::Create("file"), StyioNodeType::ResourceReceiver);
  expect_cloned_node(ResourceRefAST::CreateSelector(NameAST::Create("r"), ResourceSelectorKind::SliceFrom, -2), StyioNodeType::ResourceRef);
  expect_cloned_node(ResourceWriteAST::Create(StringAST::Create("payload"), StdStreamAST::Create(StdStreamKind::Stdout)), StyioNodeType::ResourceWrite);
  expect_cloned_node(ResourceRedirectAST::Create(NameAST::Create("fh"), EmptyResourceAST::Create()), StyioNodeType::ResourceRedirect);
  {
    std::vector<ResourceEffectAST::Handler> handlers;
    handlers.emplace_back("recover", StringAST::Create("handled"));
    expect_cloned_node(
      ResourceEffectAST::Create(
        ResourceWriteAST::Create(StringAST::Create("x"), StdStreamAST::Create(StdStreamKind::Stdout)),
        StringAST::Create("fallback"),
        false,
        std::move(handlers),
        true),
      StyioNodeType::ResourceEffect);
  }
  expect_cloned_node(InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), styio_data_type_from_name("string")), StyioNodeType::InstantPull);
  expect_cloned_node(VarAST::Create(NameAST::Create("v"), TypeAST::Create("i64")), StyioNodeType::Variable);
  expect_cloned_node(new VarAST(NameAST::Create("init"), TypeAST::Create("i64"), IntAST::Create("3")), StyioNodeType::Variable);
  expect_cloned_node(SeriesIntrinsicAST::Create(NameAST::Create("x"), SeriesIntrinsicOp::Avg, IntAST::Create("3")), StyioNodeType::SeriesIntrinsic);
  expect_cloned_node(ReturnAST::Create(IntAST::Create("1")), StyioNodeType::Return);
  expect_cloned_node(FlexBindAST::Create(VarAST::Create(NameAST::Create("m")), IntAST::Create("1")), StyioNodeType::MutBind);
  expect_cloned_node(FinalBindAST::Create(VarAST::Create(NameAST::Create("f")), IntAST::Create("1")), StyioNodeType::FinalBind);
  expect_cloned_node(PrintAST::Create({StringAST::Create("x")}), StyioNodeType::Print);
  expect_cloned_node(
    CasesAST::Create({{IntAST::Create("1"), ReturnAST::Create(IntAST::Create("2"))}}, ReturnAST::Create(IntAST::Create("0"))),
    StyioNodeType::Cases);
  expect_cloned_node(
    MatchCasesAST::make(
      NameAST::Create("x"),
      CasesAST::Create({{IntAST::Create("1"), ReturnAST::Create(IntAST::Create("2"))}}, ReturnAST::Create(IntAST::Create("0")))),
    StyioNodeType::MatchCases);
  expect_cloned_node(new InfiniteAST(IntAST::Create("0"), IntAST::Create("1")), StyioNodeType::Infinite);
  expect_cloned_node(new InfiniteAST(), StyioNodeType::Infinite);
  expect_cloned_node(ContinueAST::Create(2), StyioNodeType::Continue);
  {
    auto* block = BlockAST::Create({FinalBindAST::Create(VarAST::Create(NameAST::Create("x")), IntAST::Create("1"))});
    block->set_followings({PassAST::Create()});
    expect_cloned_node(block, StyioNodeType::Block);
  }
  expect_cloned_node(UndefinedLitAST::Create(), StyioNodeType::UndefLiteral);

  {
    std::unique_ptr<StyioAST> cloned(clone_state_expr_with_subst(
      NameAST::Create("source"),
      "source",
      ListAST::Create({StringAST::Create("replacement")})));
    ASSERT_EQ(cloned->getNodeType(), StyioNodeType::List);
  }

  {
    std::unique_ptr<ResourceMethodDefAST> method(ResourceMethodDefAST::Create(
      "file",
      "id",
      false,
      false,
      {ParamAST::Create(NameAST::Create("value"))},
      ReturnAST::Create(ResourceReceiverAST::Create("file"))));
    std::unique_ptr<StyioAST> body(clone_resource_method_body_latest(
      method.get(),
      NameAST::Create("actual_file"),
      {IntAST::Create("7")}));
    ASSERT_EQ(body->getNodeType(), StyioNodeType::Return);
    auto* ret = static_cast<ReturnAST*>(body.get());
    ASSERT_EQ(ret->getExpr()->getNodeType(), StyioNodeType::Id);
    EXPECT_EQ(static_cast<NameAST*>(ret->getExpr())->getAsStr(), "actual_file");
  }

  {
    std::unique_ptr<ResourceMethodDefAST> method(ResourceMethodDefAST::Create(
      "file",
      "wrap",
      false,
      false,
      {ParamAST::Create(NameAST::Create("payload"))},
      BlockAST::Create({
        FinalBindAST::Create(VarAST::Create(NameAST::Create("local")), NameAST::Create("payload")),
        ReturnAST::Create(ResourceReceiverAST::Create("file"))
      })));
    std::unique_ptr<StyioAST> body(clone_resource_method_body_latest(
      method.get(),
      NameAST::Create("actual_file"),
      {StringAST::Create("arg")}));
    ASSERT_EQ(body->getNodeType(), StyioNodeType::Block);
  }

  EXPECT_EQ(lower_resource_method_value_body_latest(&analyzer, ReturnAST::Create(nullptr)), nullptr);
  EXPECT_EQ(lower_resource_method_value_body_latest(&analyzer, BlockAST::Create()), nullptr);
  {
    std::unique_ptr<BlockAST> block(BlockAST::Create({ReturnAST::Create(IntAST::Create("1"))}));
    block->set_followings({PassAST::Create()});
    EXPECT_EQ(lower_resource_method_value_body_latest(&analyzer, block.get()), nullptr);
  }
  {
    std::unique_ptr<BlockAST> unsupported(BlockAST::Create({
      FunctionAST::Create(
        NameAST::Create("nested"),
        false,
        {},
        TypeAST::Create("i64"),
        BlockAST::Create({ReturnAST::Create(IntAST::Create("1"))})),
      ReturnAST::Create(IntAST::Create("2"))
    }));
    EXPECT_EQ(lower_resource_method_value_body_latest(&analyzer, unsupported.get()), nullptr);
  }
  {
    std::unique_ptr<BlockAST> supported(BlockAST::Create({
      FinalBindAST::Create(VarAST::Create(NameAST::Create("local"), TypeAST::Create("i64")), IntAST::Create("1")),
      PassAST::Create(),
      ReturnAST::Create(NameAST::Create("local"))
    }));
    std::unique_ptr<StyioIR> lowered(lower_resource_method_value_body_latest(&analyzer, supported.get()));
    ASSERT_NE(dynamic_cast<SGBlock*>(lowered.get()), nullptr);
    EXPECT_EQ(analyzer.local_binding_types.find("local"), analyzer.local_binding_types.end());
  }
  {
    std::unique_ptr<BlockAST> throwing_tail(BlockAST::Create({
      FinalBindAST::Create(VarAST::Create(NameAST::Create("local"), TypeAST::Create("i64")), IntAST::Create("1")),
      ReturnAST::Create(EmptyResourceAST::Create())
    }));
    EXPECT_THROW((void)lower_resource_method_value_body_latest(&analyzer, throwing_tail.get()), StyioTypeError);
    EXPECT_EQ(analyzer.local_binding_types.find("local"), analyzer.local_binding_types.end());
  }
  {
    EXPECT_FALSE(resource_method_value_preface_supported_latest(&analyzer, nullptr));
    EXPECT_TRUE(resource_method_value_preface_supported_latest(&analyzer, CommentAST::Create("ok")));
    EXPECT_TRUE(resource_method_value_preface_supported_latest(&analyzer, EmptyAST::Create()));
    EXPECT_TRUE(resource_method_value_preface_supported_latest(&analyzer, PrintAST::Create({StringAST::Create("ok")})));
    EXPECT_TRUE(resource_method_value_preface_supported_latest(
      &analyzer,
      ResourceWriteAST::Create(StringAST::Create("x"), StdStreamAST::Create(StdStreamKind::Stdout))));
    EXPECT_TRUE(resource_method_value_preface_supported_latest(
      &analyzer,
      ResourceRedirectAST::Create(StringAST::Create("x"), FileResourceAST::Create(StringAST::Create("out"), false))));
    EXPECT_TRUE(resource_method_value_preface_supported_latest(
      &analyzer,
      ResourceEffectAST::Create(StringAST::Create("op"), nullptr, true, {}, false)));
    EXPECT_FALSE(resource_method_value_preface_supported_latest(&analyzer, ReturnAST::Create(IntAST::Create("1"))));

    auto* list_bind = FlexBindAST::Create(
      VarAST::Create(NameAST::Create("list_local"), TypeAST::Create(styio_make_list_type("i64"))),
      ListAST::Create({IntAST::Create("1")}));
    EXPECT_TRUE(resource_method_value_preface_supported_latest(&analyzer, list_bind));
    bind_resource_method_preface_local_latest(&analyzer, list_bind);
    EXPECT_EQ(analyzer.local_binding_types["list_local"].name, "list[i64]");
    EXPECT_EQ(analyzer.resource_method_dynamic_local_binding_types["list_local"].name, "list[i64]");
    delete list_bind;

    auto* float_bind = FinalBindAST::Create(
      VarAST::Create(NameAST::Create("float_local"), TypeAST::Create("f64")),
      FloatAST::Create("1.0"));
    EXPECT_TRUE(resource_method_value_preface_supported_latest(&analyzer, float_bind));
    bind_resource_method_preface_local_latest(&analyzer, float_bind);
    EXPECT_EQ(analyzer.local_binding_types["float_local"].name, "f64");
    delete float_bind;
  }

  StateDeclAST* sd_out = nullptr;
  EXPECT_FALSE(body_returns_single_state_decl(nullptr, sd_out));
  {
    std::unique_ptr<StateDeclAST> sd(StateDeclAST::Create(
      IntAST::Create("2"),
      nullptr,
      nullptr,
      VarAST::Create(NameAST::Create("out"), TypeAST::Create("i64")),
      IntAST::Create("1")));
    EXPECT_TRUE(body_returns_single_state_decl(sd.get(), sd_out));
    EXPECT_EQ(sd_out, sd.get());
    EXPECT_TRUE(stmt_may_contain_pulse_state(&analyzer, sd.get()));
  }
  EXPECT_FALSE(simple_func_returns_single_state_decl(nullptr, sd_out));
  EXPECT_FALSE(function_ast_returns_single_state_decl(nullptr, sd_out));
  EXPECT_FALSE(pulse_block_has_state(&analyzer, nullptr));

  PulseScratch scratch;
  std::unordered_map<StyioAST*, StateDeclAST*> cache;
  EXPECT_EQ(resolve_state_decl_cached(&analyzer, PassAST::Create(), &scratch, cache), nullptr);
  {
    std::unique_ptr<FuncCallAST> missing(FuncCallAST::Create(NameAST::Create("missing"), {IntAST::Create("1")}));
    EXPECT_THROW((void)resolve_state_decl_impl(&analyzer, missing.get(), &scratch), StyioTypeError);
  }
  analyzer.func_defs["bad_kind"] = IntAST::Create("0");
  {
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(NameAST::Create("bad_kind"), {IntAST::Create("1")}));
    EXPECT_THROW((void)resolve_state_decl_impl(&analyzer, call.get(), &scratch), StyioTypeError);
  }
  analyzer.func_defs["bad_arity"] = SimpleFuncAST::Create(
    NameAST::Create("bad_arity"),
    {},
    StateDeclAST::Create(
      IntAST::Create("2"),
      nullptr,
      nullptr,
      VarAST::Create(NameAST::Create("out"), TypeAST::Create("i64")),
      NameAST::Create("x")));
  {
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(NameAST::Create("bad_arity"), {IntAST::Create("1")}));
    EXPECT_THROW((void)resolve_state_decl_impl(&analyzer, call.get(), &scratch), StyioTypeError);
  }
  analyzer.func_defs["not_state"] = SimpleFuncAST::Create(
    NameAST::Create("not_state"),
    {ParamAST::Create(NameAST::Create("x"))},
    IntAST::Create("1"));
  {
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(NameAST::Create("not_state"), {IntAST::Create("1")}));
    EXPECT_THROW((void)resolve_state_decl_impl(&analyzer, call.get(), &scratch), StyioTypeError);
  }
  analyzer.func_defs["state_fn"] = SimpleFuncAST::Create(
    NameAST::Create("state_fn"),
    {ParamAST::Create(NameAST::Create("x"))},
    StateDeclAST::Create(
      IntAST::Create("3"),
      nullptr,
      nullptr,
      VarAST::Create(NameAST::Create("out"), TypeAST::Create("i64")),
      NameAST::Create("x")));
  {
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(NameAST::Create("state_fn"), {IntAST::Create("9")}));
    StateDeclAST* resolved = resolve_state_decl_cached(&analyzer, call.get(), &scratch, cache);
    ASSERT_NE(resolved, nullptr);
    EXPECT_EQ(resolved, resolve_state_decl_cached(&analyzer, call.get(), &scratch, cache));
    ASSERT_EQ(resolved->getUpdateExpr()->getNodeType(), StyioNodeType::Integer);
  }
  {
    SGPulsePlan plan;
    EXPECT_THROW(
      (void)lower_state_decl_to_flexbind(
        &analyzer,
        StateDeclAST::Create(
          IntAST::Create("2"),
          nullptr,
          nullptr,
          VarAST::Create(NameAST::Create("missing_slot"), TypeAST::Create("i64")),
          IntAST::Create("1")),
        &plan),
      StyioTypeError);
  }

  {
    auto fixed_f64 = styio_make_topology_resource_type(
      styio_data_type_from_name("f64"),
      StyioResourceShapeKind::Fixed,
      4);
    EXPECT_EQ(resource_storage_type_latest(fixed_f64).name, "bounded_ring:f64:4");
    auto recent_bool = styio_make_topology_resource_type(
      styio_data_type_from_name("bool"),
      StyioResourceShapeKind::Recent,
      3);
    EXPECT_EQ(resource_storage_type_latest(recent_bool).name, "bounded_ring:bool:3");
    auto fixed_char = styio_make_topology_resource_type(
      styio_data_type_from_name("char"),
      StyioResourceShapeKind::Fixed,
      2);
    EXPECT_EQ(resource_storage_type_latest(fixed_char).name, "bounded_ring:char:2");
    auto recent_string = styio_make_topology_resource_type(
      styio_data_type_from_name("string"),
      StyioResourceShapeKind::Recent,
      2);
    EXPECT_EQ(resource_storage_type_latest(recent_string).name, "bounded_ring:string:2");
    auto fixed_list = styio_make_topology_resource_type(
      styio_make_list_type("i64"),
      StyioResourceShapeKind::Fixed,
      2);
    EXPECT_EQ(resource_storage_type_latest(fixed_list).name, "bounded_ring:list[i64]:2");
    auto fixed_i64 = styio_make_topology_resource_type(
      styio_data_type_from_name("i64"),
      StyioResourceShapeKind::Fixed,
      4);
    EXPECT_EQ(resource_storage_type_latest(fixed_i64).name, "bounded_ring:4");
    auto scalar_i64 = styio_make_topology_resource_type(
      styio_data_type_from_name("i64"),
      StyioResourceShapeKind::Scalar);
    EXPECT_EQ(resource_storage_type_latest(scalar_i64).name, "i64");
    EXPECT_EQ(
      resource_selector_snapshot_depth_latest(
        ResourceRefAST::Create(NameAST::Create("whole_resource")),
        scalar_i64),
      0);

    std::unique_ptr<StyioIR> zero_f64(zero_value_for_type_latest(resource_storage_type_latest(fixed_f64)));
    EXPECT_NE(dynamic_cast<SGConstFloat*>(zero_f64.get()), nullptr);
    std::unique_ptr<StyioIR> zero_bool(zero_value_for_type_latest(resource_storage_type_latest(recent_bool)));
    EXPECT_NE(dynamic_cast<SGConstBool*>(zero_bool.get()), nullptr);
    std::unique_ptr<StyioIR> zero_string(zero_value_for_type_latest(resource_storage_type_latest(recent_string)));
    EXPECT_NE(dynamic_cast<SGConstString*>(zero_string.get()), nullptr);
    std::unique_ptr<StyioIR> zero_fixed_i64(zero_value_for_type_latest(resource_storage_type_latest(fixed_i64)));
    EXPECT_NE(dynamic_cast<SGConstInt*>(zero_fixed_i64.get()), nullptr);
    std::unique_ptr<StyioIR> zero_i64(zero_value_for_type_latest(styio_data_type_from_name("i64")));
    EXPECT_NE(dynamic_cast<SGConstInt*>(zero_i64.get()), nullptr);
    std::unique_ptr<StyioIR> zero_plain_bool(zero_value_for_type_latest(styio_data_type_from_name("bool")));
    EXPECT_NE(dynamic_cast<SGConstBool*>(zero_plain_bool.get()), nullptr);
    std::unique_ptr<StyioIR> zero_plain_f64(zero_value_for_type_latest(styio_data_type_from_name("f64")));
    EXPECT_NE(dynamic_cast<SGConstFloat*>(zero_plain_f64.get()), nullptr);
    std::unique_ptr<StyioIR> zero_plain_string(zero_value_for_type_latest(styio_data_type_from_name("string")));
    EXPECT_NE(dynamic_cast<SGConstString*>(zero_plain_string.get()), nullptr);
  }
}

TEST(StyioLoweringInternal, DirectLoweringFailureAndVariantBranchesStayExplicit) {
  AstToStyioIRLowerer analyzer;

  EXPECT_THROW((void)VarTupleAST::Create({})->toStyioIR(&analyzer), StyioTypeError);
  EXPECT_THROW((void)analyzer.toStyioIR(VarTupleAST::Create({})), StyioTypeError);
  EXPECT_THROW((void)(new RangeAST(IntAST::Create("1"), IntAST::Create("2"), IntAST::Create("0")))->toStyioIR(&analyzer), StyioTypeError);
  EXPECT_THROW((void)(new SizeOfAST(nullptr))->toStyioIR(&analyzer), StyioTypeError);
  EXPECT_THROW((void)(new SizeOfAST(IntAST::Create("1")))->toStyioIR(&analyzer), StyioTypeError);
  EXPECT_THROW((void)(new ListOpAST(StyioNodeType::Access_By_Slice, IntAST::Create("1"), IntAST::Create("0"), IntAST::Create("1")))->toStyioIR(&analyzer), StyioTypeError);
  EXPECT_THROW((void)CondAST::Create(static_cast<LogicType>(999), IntAST::Create("1"))->toStyioIR(&analyzer), StyioTypeError);
  EXPECT_THROW((void)StdStreamAST::Create(StdStreamKind::Stdout)->toStyioIR(&analyzer), StyioTypeError);
  EXPECT_THROW((void)FuncCallAST::CreateCallable(NameAST::Create("cont"), {})->toStyioIR(&analyzer), StyioTypeError);
  EXPECT_THROW((void)FuncCallAST::Create(NameAST::Create("missing_lowering_fn"), {})->toStyioIR(&analyzer), StyioTypeError);
  EXPECT_THROW(
    (void)FuncCallAST::Create(
      FileResourceAST::Create(StringAST::Create("input.txt"), false),
      NameAST::Create("missing_resource_method"),
      {})->toStyioIR(&analyzer),
    StyioTypeError);
  {
    analyzer.local_binding_types["line_text"] = styio_data_type_from_name("string");
    EXPECT_THROW(
      (void)FuncCallAST::Create(
        NameAST::Create("line_text"),
        NameAST::Create("lines"),
        {IntAST::Create("1")})->toStyioIR(&analyzer),
      StyioTypeError);
  }
  {
    AstToStyioIRLowerer path_analyzer;
    seed_builtin_resource_methods(path_analyzer);
    std::unique_ptr<AttrAST> path(AttrAST::Create(
      FileResourceAST::Create(StringAST::Create("input.txt"), false),
      NameAST::Create("path")));
    std::unique_ptr<StyioIR> ir(path->toStyioIR(&path_analyzer));
    EXPECT_NE(dynamic_cast<SGConstString*>(ir.get()), nullptr);
  }
  {
    AstToStyioIRLowerer path_analyzer;
    seed_builtin_resource_methods(path_analyzer);
    path_analyzer.local_binding_types["loose_file"] = styio_make_file_handle_type("i64");
    std::unique_ptr<AttrAST> path(AttrAST::Create(
      NameAST::Create("loose_file"),
      NameAST::Create("path")));
    std::unique_ptr<StyioIR> ir(path->toStyioIR(&path_analyzer));
    EXPECT_NE(dynamic_cast<SGConstString*>(ir.get()), nullptr);
  }
  {
    AstToStyioIRLowerer topology_analyzer;
    std::unique_ptr<ResourceMethodDefAST> pressure(ResourceMethodDefAST::Create(
      "resource",
      "custom_pressure",
      false,
      true,
      {},
      ReturnAST::Create(IntAST::Create("1"))));
    ASSERT_NO_THROW(pressure->typeInfer(&topology_analyzer));
    topology_analyzer.local_binding_types["topo"] = styio_make_topology_resource_type(
      i64_type(),
      StyioResourceShapeKind::Scalar);
    EXPECT_THROW(
      (void)AttrAST::Create(
        NameAST::Create("topo"),
        NameAST::Create("custom_pressure"))->toStyioIR(&topology_analyzer),
      StyioTypeError);
  }
  EXPECT_THROW(
    (void)AttrAST::Create(NameAST::Create("attr_obj"), IntAST::Create("1"))->toStyioIR(&analyzer),
    StyioTypeError);
  {
    std::unique_ptr<SimpleFuncAST> one_arg(SimpleFuncAST::Create(
      NameAST::Create("one_arg"),
      {ParamAST::Create(NameAST::Create("x"))},
      IntAST::Create("1")));
    analyzer.func_defs["one_arg"] = one_arg.get();
    EXPECT_THROW(
      (void)FuncCallAST::Create(NameAST::Create("one_arg"), {})->toStyioIR(&analyzer),
      StyioTypeError);
  }
  EXPECT_THROW(
    (void)FlowBindAST::Create(nullptr, VarAST::Create(NameAST::Create("target")))->toStyioIR(&analyzer),
    StyioTypeError);
  EXPECT_THROW(
    (void)IterSeqAST::Create(
      ListAST::Create({IntAST::Create("1")}),
      {HashTagNameAST::Create({"tag"})})->toStyioIR(&analyzer),
    StyioTypeError);
  {
    std::unique_ptr<IterSeqAST> iter(IterSeqAST::Create(
      ListAST::Create({IntAST::Create("1")}),
      {HashTagNameAST::Create({"tag"})}));
    EXPECT_THROW((void)analyzer.toStyioIR(iter.get()), StyioTypeError);
  }
  EXPECT_THROW((void)HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("h")),
      IntAST::Create("1"))->toStyioIR(&analyzer),
    StyioTypeError);
  EXPECT_THROW((void)ResourceWriteAST::Create(
      StringAST::Create("x"),
      StdStreamAST::Create(StdStreamKind::Stdin))->toStyioIR(&analyzer),
    StyioTypeError);
  EXPECT_THROW((void)ResourceRedirectAST::Create(
      StringAST::Create("x"),
      NameAST::Create("not_resource"))->toStyioIR(&analyzer),
    StyioTypeError);

  {
    std::unique_ptr<ListAST> rowless(ListAST::Create({IntAST::Create("1")}));
    rowless->setDataType(styio_make_matrix_type("i64", 1, 1));
    EXPECT_THROW((void)rowless->toStyioIR(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<ResourceDeclAST> decl(ResourceDeclAST::Create(
      {{NameAST::Create("driven_slot"), TypeAST::Create("i64")}},
      BlockAST::Create({PassAST::Create()})));
    std::unique_ptr<StyioIR> ir(decl->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGBlock*>(ir.get()), nullptr);
  }
  {
    analyzer.local_binding_types["d"] = styio_make_dict_type("string", "i64");
    std::unique_ptr<SizeOfAST> size(new SizeOfAST(NameAST::Create("d")));
    std::unique_ptr<StyioIR> ir(size->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SCDictLen*>(ir.get()), nullptr);
  }
  {
    analyzer.local_binding_types["task"] = styio_make_task_type("unit");
    std::unique_ptr<HandleAcquireAST> acquire(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("done"), TypeAST::Create("i64")),
      NameAST::Create("task")));
    std::unique_ptr<StyioIR> ir(acquire->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SIOFlowBind*>(ir.get()), nullptr);
  }
  {
    analyzer.local_binding_types["list_i"] = styio_make_list_type("i64");
    std::unique_ptr<HandleAcquireAST> clone(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("copy"), TypeAST::Create(styio_make_list_type("i64"))),
      NameAST::Create("list_i"),
      HandleAcquireAST::BindMode::Flex));
    std::unique_ptr<StyioIR> ir(clone->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGFlexBind*>(ir.get()), nullptr);
  }
  {
    analyzer.local_binding_types["dict_i"] = styio_make_dict_type("string", "i64");
    std::unique_ptr<HandleAcquireAST> clone(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("final_copy"), TypeAST::Create(styio_make_dict_type("string", "i64"))),
      NameAST::Create("dict_i")));
    std::unique_ptr<StyioIR> ir(clone->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGFinalBind*>(ir.get()), nullptr);
  }
  {
    analyzer.local_binding_types["scalar_i"] = styio_data_type_from_name("i64");
    std::unique_ptr<HandleAcquireAST> clone(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("bad_copy"), TypeAST::Create("i64")),
      NameAST::Create("scalar_i"),
      HandleAcquireAST::BindMode::Flex));
    EXPECT_THROW((void)clone->toStyioIR(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<ResourceWriteAST> write(ResourceWriteAST::Create(
      StringAST::Create("x"),
      StdStreamAST::Create(StdStreamKind::Stdout)));
    std::unique_ptr<StyioIR> ir(write->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SIOStdStreamWrite*>(ir.get()), nullptr);
  }
  {
    std::unique_ptr<ResourceWriteAST> write(ResourceWriteAST::Create(
      DictAST::Create({{StringAST::Create("k"), IntAST::Create("1")}}),
      StdStreamAST::Create(StdStreamKind::Stdout)));
    std::unique_ptr<StyioIR> ir(write->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SIOStdStreamWrite*>(ir.get()), nullptr);
  }
  {
    std::unique_ptr<AttrAST> path(AttrAST::Create(
      FileResourceAST::Create(StringAST::Create("input.txt"), false),
      NameAST::Create("path")));
    std::unique_ptr<StyioIR> ir(path->toStyioIR(&analyzer));
    EXPECT_NE(ir, nullptr);
  }
  {
    LowererProbe probe;
    auto bind_as = [&](const std::string& name, StyioSemaContext::BindingValueKind kind, bool dynamic) {
      StyioSemaContext::BindingInfo info;
      info.value_kind = kind;
      info.dynamic_slot = dynamic;
      probe.binding_info_[name] = info;
    };
    bind_as("list_slot", StyioSemaContext::BindingValueKind::ListHandle, false);
    bind_as("dict_slot", StyioSemaContext::BindingValueKind::DictHandle, false);
    bind_as("matrix_slot", StyioSemaContext::BindingValueKind::MatrixHandle, false);
    bind_as("task_slot", StyioSemaContext::BindingValueKind::TaskHandle, false);
    bind_as("dynamic_unknown", StyioSemaContext::BindingValueKind::Unknown, true);

    std::unique_ptr<ParallelAssignAST> assign(ParallelAssignAST::Create(
      {
        NameAST::Create("list_slot"),
        NameAST::Create("dict_slot"),
        NameAST::Create("matrix_slot"),
        NameAST::Create("task_slot"),
        NameAST::Create("dynamic_unknown"),
      },
      {
        IntAST::Create("1"),
        IntAST::Create("2"),
        IntAST::Create("3"),
        IntAST::Create("4"),
        IntAST::Create("5"),
      }));
    std::unique_ptr<StyioIR> ir(assign->toStyioIR(&probe));
    auto* block = dynamic_cast<SGBlock*>(ir.get());
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->stmts.size(), 10u);

    auto* list_bind = dynamic_cast<SGFlexBind*>(block->stmts[5]);
    auto* dict_bind = dynamic_cast<SGFlexBind*>(block->stmts[6]);
    auto* matrix_bind = dynamic_cast<SGFlexBind*>(block->stmts[7]);
    auto* task_bind = dynamic_cast<SGFlexBind*>(block->stmts[8]);
    auto* dynamic_bind = dynamic_cast<SGFlexBind*>(block->stmts[9]);
    ASSERT_NE(list_bind, nullptr);
    ASSERT_NE(dict_bind, nullptr);
    ASSERT_NE(matrix_bind, nullptr);
    ASSERT_NE(task_bind, nullptr);
    ASSERT_NE(dynamic_bind, nullptr);
    EXPECT_TRUE(list_bind->var->is_dynamic_slot);
    EXPECT_TRUE(list_bind->var->is_list_slot);
    EXPECT_TRUE(dict_bind->var->is_dynamic_slot);
    EXPECT_FALSE(dict_bind->var->is_list_slot);
    EXPECT_TRUE(matrix_bind->var->is_dynamic_slot);
    EXPECT_FALSE(matrix_bind->var->is_list_slot);
    EXPECT_TRUE(task_bind->var->is_dynamic_slot);
    EXPECT_FALSE(task_bind->var->is_list_slot);
    EXPECT_TRUE(dynamic_bind->var->is_dynamic_slot);
    EXPECT_FALSE(dynamic_bind->var->is_list_slot);
  }
}

TEST(StyioLoweringInternal, OptimizerCanonicalizesNestedMatchRebindsAndScanEdges) {
  const StyioDataType i64 = styio_data_type_from_name("i64");
  auto make_var = [&](const std::string& name) {
    return SGVar::Create(SGResId::Create(name), SGType::Create(i64));
  };
  auto scrutinee_expr = [&]() {
    return SGBinOp::Create(
      SGResId::Create("input"),
      SGConstInt::Create(1),
      StyioOpType::Binary_Add,
      SGType::Create(i64),
      i64,
      i64);
  };

  {
    auto* rebind = SGFlexBind::Create(make_var("scr"), scrutinee_expr());
    std::unique_ptr<SGBlock> root(SGBlock::Create({
      SGMatch::Create(
        scrutinee_expr(),
        {{1, SGBlock::Create({SGReturn::Create(SGConstInt::Create(10))})}},
        SGBlock::Create({
          SGFinalBind::Create(
            make_var("local"),
            SCListGet::Create(SGResId::Create("items"), SGResId::Create("idx"))),
          rebind,
          SGIf::Create(
            SGResId::Create("other"),
            SGBlock::Create({
              SCListSet::Create(
                SGResId::Create("items"),
                SGConstInt::Create(0),
                SGResId::Create("scr")),
            }),
            SGBlock::Create({
              SCDictSet::Create(
                SGResId::Create("dict"),
                SGResId::Create("scr"),
                SGConstInt::Create(2)),
            })),
        }),
        SGMatchReprKind::ExprInt),
    }));

    EXPECT_EQ(styio::lowering::optimize_styio_ir(root.get()), root.get());
    ASSERT_EQ(root->stmts.size(), 1u);
    auto* hoisted = dynamic_cast<SGBlock*>(root->stmts[0]);
    ASSERT_NE(hoisted, nullptr);
    ASSERT_EQ(hoisted->stmts.size(), 2u);
    EXPECT_EQ(hoisted->stmts[0], rebind);
    auto* match = dynamic_cast<SGMatch*>(hoisted->stmts[1]);
    ASSERT_NE(match, nullptr);
    EXPECT_NE(dynamic_cast<SGResId*>(match->scrutinee), nullptr);
  }

  {
    std::unique_ptr<SGBlock> root(SGBlock::Create({
      SGFlexBind::Create(make_var("input"), SGConstInt::Create(0)),
      SGMatch::Create(
        scrutinee_expr(),
        {},
        SGBlock::Create({
          SCListSet::Create(SGResId::Create("items"), SGConstInt::Create(0), SGConstInt::Create(1)),
          SGFlexBind::Create(make_var("scr"), scrutinee_expr()),
          SGReturn::Create(SGResId::Create("scr")),
        }),
        SGMatchReprKind::ExprInt),
    }));

    EXPECT_EQ(styio::lowering::optimize_styio_ir(root.get()), root.get());
    ASSERT_EQ(root->stmts.size(), 2u);
    EXPECT_NE(dynamic_cast<SGMatch*>(root->stmts[1]), nullptr);
  }
}

TEST(StyioLoweringInternal, IteratorAndZipPulsePlansAttachToIterableIrNodes) {
  auto make_pulse_body = [](const std::string& input_name, const std::string& output_name) {
    return BlockAST::Create({
      StateDeclAST::Create(
        IntAST::Create("2"),
        nullptr,
        nullptr,
        VarAST::Create(NameAST::Create(output_name), TypeAST::Create("i64")),
        NameAST::Create(input_name)),
    });
  };
  auto make_i64_list = []() {
    auto* list = ListAST::Create({IntAST::Create("1"), IntAST::Create("2")});
    list->setDataType(styio_make_list_type("i64"));
    return list;
  };

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<IteratorAST> iter(IteratorAST::Create(
      StdStreamAST::Create(StdStreamKind::Stdin),
      {ParamAST::Create(NameAST::Create("line"))},
      make_pulse_body("line", "stdin_state")));
    std::unique_ptr<StyioIR> ir(iter->toStyioIR(&analyzer));
    auto* lowered = dynamic_cast<SIOStdStreamLineIter*>(ir.get());
    ASSERT_NE(lowered, nullptr);
    ASSERT_NE(lowered->pulse_plan, nullptr);
    EXPECT_GT(lowered->pulse_plan->total_bytes, 0);
    EXPECT_GE(lowered->pulse_region_id, 0);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<IteratorAST> iter(IteratorAST::Create(
      FileResourceAST::Create(StringAST::Create("in.txt"), false),
      {ParamAST::Create(NameAST::Create("line"))},
      make_pulse_body("line", "file_state")));
    std::unique_ptr<StyioIR> ir(iter->toStyioIR(&analyzer));
    auto* lowered = dynamic_cast<SIOFileLineIter*>(ir.get());
    ASSERT_NE(lowered, nullptr);
    ASSERT_NE(lowered->pulse_plan, nullptr);
    EXPECT_GT(lowered->pulse_plan->total_bytes, 0);
    EXPECT_GE(lowered->pulse_region_id, 0);
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["input_stream"] = styio_make_std_stream_type(StdStreamKind::Stdin, "string");
    std::unique_ptr<IteratorAST> iter(IteratorAST::Create(
      NameAST::Create("input_stream"),
      {ParamAST::Create(NameAST::Create("line"))},
      make_pulse_body("line", "named_stream_state")));
    std::unique_ptr<StyioIR> ir(iter->toStyioIR(&analyzer));
    auto* lowered = dynamic_cast<SIOStdStreamLineIter*>(ir.get());
    ASSERT_NE(lowered, nullptr);
    ASSERT_NE(lowered->pulse_plan, nullptr);
    EXPECT_GT(lowered->pulse_plan->total_bytes, 0);
    EXPECT_GE(lowered->pulse_region_id, 0);
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["file_handle"] = styio_make_file_handle_type("i64");
    std::unique_ptr<IteratorAST> iter(IteratorAST::Create(
      NameAST::Create("file_handle"),
      {ParamAST::Create(NameAST::Create("line"))},
      make_pulse_body("line", "named_file_state")));
    std::unique_ptr<StyioIR> ir(iter->toStyioIR(&analyzer));
    auto* lowered = dynamic_cast<SIOFileLineIter*>(ir.get());
    ASSERT_NE(lowered, nullptr);
    ASSERT_NE(lowered->pulse_plan, nullptr);
    EXPECT_GT(lowered->pulse_plan->total_bytes, 0);
    EXPECT_GE(lowered->pulse_region_id, 0);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<IteratorAST> iter(IteratorAST::Create(
      make_i64_list(),
      {ParamAST::Create(NameAST::Create("item"))},
      make_pulse_body("item", "list_state")));
    std::unique_ptr<StyioIR> ir(iter->toStyioIR(&analyzer));
    auto* lowered = dynamic_cast<SGForEach*>(ir.get());
    ASSERT_NE(lowered, nullptr);
    ASSERT_NE(lowered->pulse_plan, nullptr);
    EXPECT_GT(lowered->pulse_plan->total_bytes, 0);
    EXPECT_GE(lowered->pulse_region_id, 0);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StreamZipAST> zip(StreamZipAST::Create(
      make_i64_list(),
      {ParamAST::Create(NameAST::Create("left"))},
      FileResourceAST::Create(StringAST::Create("right.txt"), false),
      {ParamAST::Create(NameAST::Create("right"))},
      make_pulse_body("left", "zip_state")));
    std::unique_ptr<StyioIR> ir(zip->toStyioIR(&analyzer));
    auto* lowered = dynamic_cast<SIOStreamZip*>(ir.get());
    ASSERT_NE(lowered, nullptr);
    ASSERT_NE(lowered->pulse_plan, nullptr);
    EXPECT_GT(lowered->pulse_plan->total_bytes, 0);
    EXPECT_GE(lowered->pulse_region_id, 0);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StreamZipAST> zip(StreamZipAST::Create(
      StdStreamAST::Create(StdStreamKind::Stdout),
      {ParamAST::Create(NameAST::Create("left"))},
      make_i64_list(),
      {ParamAST::Create(NameAST::Create("right"))},
      BlockAST::Create({PassAST::Create()})));
    EXPECT_THROW((void)zip->toStyioIR(&analyzer), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StreamZipAST> zip(StreamZipAST::Create(
      StdStreamAST::Create(StdStreamKind::Stdin),
      {ParamAST::Create(NameAST::Create("left"))},
      StdStreamAST::Create(StdStreamKind::Stdin),
      {ParamAST::Create(NameAST::Create("right"))},
      BlockAST::Create({PassAST::Create()})));
    EXPECT_THROW((void)zip->toStyioIR(&analyzer), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<MainBlockAST> main_block(MainBlockAST::Create({
      IteratorAST::Create(
        make_i64_list(),
        {ParamAST::Create(NameAST::Create("item"))},
        make_pulse_body("item", "main_list_state")),
    }));
    std::unique_ptr<StyioIR> ir(main_block->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGMainEntry*>(ir.get()), nullptr);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<MainBlockAST> main_block(MainBlockAST::Create({
      IteratorAST::Create(
        FileResourceAST::Create(StringAST::Create("main.txt"), false),
        {ParamAST::Create(NameAST::Create("line"))},
        make_pulse_body("line", "main_file_state")),
    }));
    std::unique_ptr<StyioIR> ir(main_block->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGMainEntry*>(ir.get()), nullptr);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<MainBlockAST> main_block(MainBlockAST::Create({
      StreamZipAST::Create(
        make_i64_list(),
        {ParamAST::Create(NameAST::Create("left"))},
        FileResourceAST::Create(StringAST::Create("right.txt"), false),
        {ParamAST::Create(NameAST::Create("right"))},
        make_pulse_body("left", "main_zip_state")),
    }));
    std::unique_ptr<StyioIR> ir(main_block->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGMainEntry*>(ir.get()), nullptr);
  }
}

TEST(StyioLoweringInternal, AstAccessorAndFailClosedDispatchStayExplicit) {
  auto is_undefined = [](const StyioDataType& type) {
    return type.option == StyioDataTypeOption::Undefined;
  };

  {
    std::unique_ptr<MainBlockAST> main_block(MainBlockAST::Create({PassAST::Create()}));
    EXPECT_TRUE(is_undefined(main_block->getDataType()));
    std::unique_ptr<EOFAST> eof(EOFAST::Create());
    EXPECT_TRUE(is_undefined(eof->getDataType()));
    std::unique_ptr<BreakAST> br(BreakAST::Create(3));
    EXPECT_EQ(br->getDepth(), 1u);
  }
  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<MainBlockAST> main_block(MainBlockAST::Create({
      ResourceMethodDefAST::Create("file", "empty_body", false, false, {}, nullptr),
      FuncCallAST::Create(
        FileResourceAST::Create(StringAST::Create("input.txt"), false),
        NameAST::Create("empty_body"),
        {})
    }));
    EXPECT_THROW((void)main_block->toStyioIR(&analyzer), StyioTypeError);
  }
  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<MainBlockAST> main_block(MainBlockAST::Create({
      ResourceMethodDefAST::Create(
        "file",
        "inner_value",
        false,
        false,
        {},
        ReturnAST::Create(IntAST::Create("7"))),
      ResourceMethodDefAST::Create(
        "file",
        "outer_value",
        false,
        false,
        {},
        ReturnAST::Create(FuncCallAST::Create(
          ResourceReceiverAST::Create("file"),
          NameAST::Create("inner_value"),
          {}))),
      FuncCallAST::Create(
        FileResourceAST::Create(StringAST::Create("input.txt"), false),
        NameAST::Create("outer_value"),
        {})
    }));
    std::unique_ptr<StyioIR> ir(main_block->toStyioIR(&analyzer));
    ASSERT_NE(ir, nullptr);
  }
  {
    AstToStyioIRLowerer analyzer;
    auto* inner_path = ResourceMethodDefAST::Create(
      "file",
      "inner_path",
      false,
      true,
      {},
      ReturnAST::Create(IntAST::Create("9")));
    auto* outer_path = ResourceMethodDefAST::Create(
      "file",
      "outer_path",
      false,
      true,
      {},
      ReturnAST::Create(AttrAST::Create(
        ResourceReceiverAST::Create("file"),
        NameAST::Create("inner_path"))));
    ASSERT_NO_THROW(inner_path->typeInfer(&analyzer));
    ASSERT_NO_THROW(outer_path->typeInfer(&analyzer));
    std::unique_ptr<MainBlockAST> main_block(MainBlockAST::Create({
      inner_path,
      outer_path,
      AttrAST::Create(
        FileResourceAST::Create(StringAST::Create("input.txt"), false),
        NameAST::Create("outer_path"))
    }));
    std::unique_ptr<StyioIR> ir(main_block->toStyioIR(&analyzer));
    ASSERT_NE(ir, nullptr);
  }
  {
    std::unique_ptr<VarAST> typed(VarAST::Create(NameAST::Create("v"), TypeAST::Create("i64")));
    EXPECT_TRUE(typed->isTyped());
    EXPECT_EQ(typed->getTypeAsStr(), "i64");
    std::unique_ptr<ParamAST> param(ParamAST::Create(NameAST::Create("p"), TypeAST::Create()));
    EXPECT_TRUE(is_undefined(param->getDataType()));
    param->setDType(styio_data_type_from_name("bool"));
    EXPECT_EQ(param->getDType()->getTypeName(), "bool");
    std::unique_ptr<OptArgAST> opt(OptArgAST::Create(NameAST::Create("oa")));
    std::unique_ptr<OptKwArgAST> opt_kw(OptKwArgAST::Create(NameAST::Create("ok")));
    EXPECT_TRUE(is_undefined(opt->getDataType()));
    EXPECT_TRUE(is_undefined(opt_kw->getDataType()));
  }
  {
    std::unique_ptr<TupleAST> tuple(TupleAST::Create({IntAST::Create("1")}));
    tuple->setConsistency(false);
    EXPECT_FALSE(tuple->isConsistent());
    EXPECT_NE(tuple->getDTypeObj(), nullptr);
    std::unique_ptr<ListAST> list(ListAST::Create({IntAST::Create("1")}));
    list->setConsistency(false);
    EXPECT_FALSE(list->isConsistent());
    EXPECT_NE(list->getDTypeObj(), nullptr);
    std::unique_ptr<DictAST> dict(DictAST::Create({{StringAST::Create("k"), IntAST::Create("1")}}));
    dict->setConsistency(false);
    EXPECT_FALSE(dict->isConsistent());
    EXPECT_NE(dict->getDTypeObj(), nullptr);
  }
  {
    std::unique_ptr<ResPathAST> local(ResPathAST::Create(StyioPathType::local_relevant_any, "rel"));
    std::unique_ptr<RemotePathAST> remote(RemotePathAST::Create(StyioPathType::ipv4_addr, "127.0.0.1"));
    std::unique_ptr<WebUrlAST> web(WebUrlAST::Create(StyioPathType::url_https, "https://example.test"));
    std::unique_ptr<DBUrlAST> db(DBUrlAST::Create(StyioPathType::db_postgresql, "postgres://db"));
    EXPECT_TRUE(is_undefined(local->getDataType()));
    EXPECT_TRUE(is_undefined(remote->getDataType()));
    EXPECT_TRUE(is_undefined(web->getDataType()));
    EXPECT_TRUE(is_undefined(db->getDataType()));
  }
  {
    std::unique_ptr<PrintAST> print(PrintAST::Create({StringAST::Create("x")}));
    EXPECT_TRUE(is_undefined(print->getDataType()));
    std::unique_ptr<StructAST> structure(StructAST::Create(
      NameAST::Create("Point"),
      {ParamAST::Create(NameAST::Create("x"), TypeAST::Create("i64"))}));
    EXPECT_EQ(structure->getDataType().name, "Point");
    std::unique_ptr<ReadFileAST> read(new ReadFileAST(NameAST::Create("line"), StringAST::Create("in.txt")));
    EXPECT_TRUE(is_undefined(read->getDataType()));
    std::unique_ptr<StdStreamAST> invalid_stream(StdStreamAST::Create(static_cast<StdStreamKind>(99)));
    EXPECT_EQ(invalid_stream->getNodeType(), StyioNodeType::StdoutResource);
  }
  {
    std::unique_ptr<ResourceMethodDefAST> method(ResourceMethodDefAST::Create(
      "file",
      "id",
      false,
      false,
      {},
      ReturnAST::Create(ResourceReceiverAST::Create("file"))));
    EXPECT_TRUE(is_undefined(method->getDataType()));
    std::unique_ptr<ResourceOrderAST> order(ResourceOrderAST::Create(NameAST::Create("a"), NameAST::Create("b")));
    EXPECT_TRUE(is_undefined(order->getDataType()));
    std::unique_ptr<ResourceDeclAST> decl(ResourceDeclAST::Create(
      {{NameAST::Create("slot"), TypeAST::Create("i64")}},
      BlockAST::Create()));
    EXPECT_TRUE(is_undefined(decl->getDataType()));
    std::unique_ptr<ResourceRefAST> ref(ResourceRefAST::Create(NameAST::Create("r")));
    ASSERT_NE(ref->getName(), nullptr);
    EXPECT_EQ(ref->getName()->getAsStr(), "r");
    std::unique_ptr<EmptyResourceAST> empty(EmptyResourceAST::Create());
    EXPECT_EQ(empty->getDataType().name, "empty-resource");
  }
  {
    std::unique_ptr<HandleAcquireAST> acquire(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("h")),
      FileResourceAST::Create(StringAST::Create("input.txt"), false),
      HandleAcquireAST::BindMode::Flex));
    EXPECT_EQ(acquire->getBindMode(), HandleAcquireAST::BindMode::Flex);
    std::unique_ptr<ResourceWriteAST> write(ResourceWriteAST::Create(
      StringAST::Create("payload"),
      StdStreamAST::Create(StdStreamKind::Stdout)));
    EXPECT_TRUE(is_undefined(write->getDataType()));
    std::unique_ptr<StyioAST> write_data(write->release_data_latest());
    std::unique_ptr<StyioAST> write_resource(write->release_resource_latest());
    EXPECT_EQ(write->getData(), nullptr);
    EXPECT_EQ(write->getResource(), nullptr);
    std::unique_ptr<ResourceRedirectAST> redirect(ResourceRedirectAST::Create(
      StringAST::Create("payload"),
      NameAST::Create("target")));
    std::unique_ptr<StyioAST> redirect_data(redirect->release_data_latest());
    std::unique_ptr<StyioAST> redirect_resource(redirect->release_resource_latest());
    EXPECT_EQ(redirect->getData(), nullptr);
    EXPECT_EQ(redirect->getResource(), nullptr);
  }
  {
    std::unique_ptr<StateRefAST> state(StateRefAST::Create(NameAST::Create("s")));
    ASSERT_NE(state->getName(), nullptr);
    EXPECT_EQ(state->getName()->getAsStr(), "s");
    std::unique_ptr<HistoryProbeAST> history(HistoryProbeAST::Create(
      StateRefAST::Create(NameAST::Create("hist")),
      IntAST::Create("1")));
    EXPECT_EQ(history->getDataType().name, "i64");
    std::unique_ptr<StateDeclAST> state_decl(StateDeclAST::Create(
      IntAST::Create("2"),
      nullptr,
      nullptr,
      VarAST::Create(NameAST::Create("out"), TypeAST::Create("i64")),
      IntAST::Create("1")));
    EXPECT_TRUE(is_undefined(state_decl->getDataType()));
    std::unique_ptr<SnapshotDeclAST> snapshot(SnapshotDeclAST::Create(
      NameAST::Create("snap"),
      FileResourceAST::Create(StringAST::Create("snap.bin"), false)));
    EXPECT_TRUE(is_undefined(snapshot->getDataType()));
  }
  {
    std::unique_ptr<CheckEqualAST> check_eq(CheckEqualAST::Create({IntAST::Create("1")}));
    EXPECT_TRUE(is_undefined(check_eq->getDataType()));
    std::unique_ptr<CheckIsinAST> check_isin(new CheckIsinAST(ListAST::Create({IntAST::Create("1")})));
    EXPECT_TRUE(is_undefined(check_isin->getDataType()));
    std::unique_ptr<HashTagNameAST> tag(HashTagNameAST::Create({"route", "name"}));
    EXPECT_EQ(tag->getDataType().option, StyioDataTypeOption::String);
    std::unique_ptr<ForwardAST> forward(new ForwardAST());
    forward->setRetExpr(IntAST::Create("9"));
    EXPECT_EQ(forward->getCheckEq(), nullptr);
    EXPECT_EQ(forward->getCheckIsin(), nullptr);
    EXPECT_EQ(forward->getThen(), nullptr);
    EXPECT_EQ(forward->getCondFlow(), nullptr);
    EXPECT_NE(forward->getRetExpr(), nullptr);
    EXPECT_TRUE(is_undefined(forward->getDataType()));
  }
  {
    std::unique_ptr<BackwardAST> backward(BackwardAST::Create(
      NameAST::Create("obj"),
      VarTupleAST::Create({VarAST::Create(NameAST::Create("p"))}),
      {IntAST::Create("1")},
      {NameAST::Create("obj")}));
    EXPECT_TRUE(is_undefined(backward->getDataType()));
    std::unique_ptr<CODPAST> codp(CODPAST::Create("map", {NameAST::Create("x")}));
    EXPECT_EQ(codp->getNodeType(), StyioNodeType::Chain_Of_Data_Processing);
    EXPECT_TRUE(is_undefined(codp->getDataType()));
    std::unique_ptr<AnonyFuncAST> anon(new AnonyFuncAST(
      VarTupleAST::Create({VarAST::Create(NameAST::Create("x"))}),
      NameAST::Create("x")));
    EXPECT_NE(anon->getArgs(), nullptr);
    EXPECT_NE(anon->getThenExpr(), nullptr);
    EXPECT_TRUE(is_undefined(anon->getDataType()));
  }
  {
    std::unique_ptr<FunctionAST> unnamed(FunctionAST::Create(
      nullptr,
      false,
      {},
      TypeAST::Create("i64"),
      BlockAST::Create()));
    EXPECT_FALSE(unnamed->hasName());
    EXPECT_FALSE(unnamed->hasRetType());
    unnamed->setRetType(styio_data_type_from_name("f64"));

    std::variant<TypeAST*, TypeTupleAST*> tuple_ret(TypeTupleAST::Create({TypeAST::Create("i64")}));
    std::unique_ptr<FunctionAST> tuple_function(FunctionAST::Create(
      NameAST::Create("tuple_variant"),
      false,
      {},
      tuple_ret,
      BlockAST::Create()));
    EXPECT_TRUE(std::holds_alternative<TypeTupleAST*>(tuple_function->ret_type));

    std::variant<TypeAST*, TypeTupleAST*> ret(TypeAST::Create("i64"));
    std::unique_ptr<SimpleFuncAST> simple(SimpleFuncAST::Create(
      NameAST::Create("simple_variant"),
      {ParamAST::Create(NameAST::Create("x"), TypeAST::Create("i64"))},
      ret,
      NameAST::Create("x")));
    EXPECT_EQ(simple->getNodeType(), StyioNodeType::SimpleFunc);
    std::unique_ptr<SimpleFuncAST> empty_simple(SimpleFuncAST::Create());
    EXPECT_TRUE(is_undefined(empty_simple->getDataType()));
  }
  {
    std::unique_ptr<FinalBindAST> final_bind(FinalBindAST::Create(
      VarAST::Create(NameAST::Create("fixed"), TypeAST::Create("i64")),
      IntAST::Create("1")));
    EXPECT_TRUE(is_undefined(final_bind->getDataType()));
    std::unique_ptr<ParallelAssignAST> parallel(ParallelAssignAST::Create(
      {NameAST::Create("a")},
      {IntAST::Create("1")}));
    EXPECT_TRUE(is_undefined(parallel->getDataType()));
    std::unique_ptr<StreamZipAST> zip(StreamZipAST::Create(
      ListAST::Create({IntAST::Create("1")}),
      {ParamAST::Create(NameAST::Create("a"))},
      ListAST::Create({IntAST::Create("2")}),
      {ParamAST::Create(NameAST::Create("b"))},
      PassAST::Create()));
    EXPECT_TRUE(is_undefined(zip->getDataType()));
    std::unique_ptr<TaskGroupLaunchAST> group(TaskGroupLaunchAST::Create({TaskBlockAST::Create(BlockAST::Create())}));
    EXPECT_TRUE(is_undefined(group->getDataType()));
    std::unique_ptr<FlowBindAST> flow(FlowBindAST::Create(
      NameAST::Create("source"),
      VarAST::Create(NameAST::Create("target")),
      true));
    EXPECT_TRUE(flow->isPullDirection());
    std::unique_ptr<IterSeqAST> iter(IterSeqAST::Create(
      ListAST::Create({IntAST::Create("1")}),
      {HashTagNameAST::Create({"tag"})}));
    EXPECT_TRUE(is_undefined(iter->getDataType()));
    std::unique_ptr<ExtractorAST> extractor(ExtractorAST::Create(
      TupleAST::Create({IntAST::Create("1")}),
      CODPAST::Create("take", {})));
    EXPECT_EQ(extractor->getDataType().name, "TupleOp");
  }

  exercise_to_ir(CODPAST::Create("map", {NameAST::Create("x")}));
  exercise_to_ir(CheckEqualAST::Create({IntAST::Create("1")}));
  exercise_to_ir(new CheckIsinAST(ListAST::Create({IntAST::Create("1")})));
  exercise_to_ir(HashTagNameAST::Create({"tag"}));
  exercise_to_ir(BackwardAST::Create(
    NameAST::Create("obj"),
    VarTupleAST::Create({VarAST::Create(NameAST::Create("x"))}),
    {},
    {}));
  exercise_to_ir(new AnonyFuncAST(
    VarTupleAST::Create({VarAST::Create(NameAST::Create("x"))}),
    NameAST::Create("x")));
  exercise_to_ir(SnapshotDeclAST::Create(
    NameAST::Create("snap"),
    FileResourceAST::Create(StringAST::Create("snap.bin"), false)));
  exercise_type_infer(BackwardAST::Create(
    NameAST::Create("obj"),
    VarTupleAST::Create({VarAST::Create(NameAST::Create("x"))}),
    {},
    {}));
}

TEST(StyioLoweringInternal, AdditionalLoweringGuardBranchesStayExplicit) {
  AstToStyioIRLowerer analyzer;

  EXPECT_TRUE(lowering_type_convert_target_type(static_cast<NumPromoTy>(99)).isUndefined());
  EXPECT_TRUE(lowering_type_convert_source_fallback_type(static_cast<NumPromoTy>(99)).isUndefined());
  EXPECT_EQ(
    merge_tail_value_type(styio_make_list_type("i64"), styio_make_dict_type("string", "i64")).name,
    "list[i64]");

  {
    const std::string scrutinee = "value";
    std::unique_ptr<BinCompAST> cmp(new BinCompAST(
      CompType::EQ,
      NameAST::Create("other"),
      IntAST::Create("7")));
    EXPECT_FALSE(match_case_pattern_value_for_name(cmp.get(), &scrutinee).has_value());
  }
  {
    auto type = matrix_intrinsic_lowered_type(
      &analyzer,
      FuncCallAST::Create(NameAST::Create("mat_zeros"), {NameAST::Create("rows")}));
    EXPECT_EQ(styio_matrix_row_count(type), 0u);
    EXPECT_EQ(styio_matrix_col_count(type), 0u);
  }
  {
    auto type = matrix_intrinsic_lowered_type(
      &analyzer,
      FuncCallAST::Create(
        NameAST::Create("mat_zeros_i64"),
        {IntAST::Create("999999999999999999999999"), IntAST::Create("2")}));
    EXPECT_EQ(styio_matrix_row_count(type), 0u);
    EXPECT_EQ(styio_matrix_col_count(type), 2u);
  }
  {
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(NameAST::Create("norm"), {NameAST::Create("matrix_i")}));
    analyzer.local_binding_types["matrix_i"] = styio_make_matrix_type("f64", 2, 2);
    EXPECT_EQ(matrix_intrinsic_runtime_name(&analyzer, call.get()), "__styio_matrix_norm");
  }
  {
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(NameAST::Create("custom_runtime"), {}));
    EXPECT_TRUE(matrix_intrinsic_lowered_type(&analyzer, call.get()).isUndefined());
    EXPECT_EQ(matrix_intrinsic_runtime_name(&analyzer, call.get()), "custom_runtime");
  }
  {
    StyioRepr repr;
    std::unique_ptr<MainBlockAST> empty_main(MainBlockAST::Create({}));
    EXPECT_NE(empty_main->toString(&repr).find("{ }"), std::string::npos);

    std::unique_ptr<SGMainEntry> empty_ir(SGMainEntry::Create({}));
    EXPECT_EQ(empty_ir->toString(&repr), "styio.ir.main { }");

    std::unique_ptr<ListOpAST> unsupported_list_op(new ListOpAST(
      static_cast<StyioNodeType>(999),
      NameAST::Create("items"),
      IntAST::Create("0")));
    EXPECT_NE(unsupported_list_op->toString(&repr).find("undefined"), std::string::npos);
  }
  {
    std::unique_ptr<ListAST> typed_empty(ListAST::Create());
    typed_empty->setDataType(styio_make_list_type("string"));
    EXPECT_TRUE(collection_elem_is_string(&analyzer, typed_empty.get()));
  }

  bool has_string = false;
  bool has_int = false;
  bool has_float = false;
  scan_returns_for_value_kinds(nullptr, has_string, has_int, has_float);
  scan_returns_for_value_kinds(ReturnAST::Create(nullptr), has_string, has_int, has_float);
  EXPECT_FALSE(has_string);
  EXPECT_FALSE(has_int);
  EXPECT_FALSE(has_float);
  EXPECT_FALSE(ast_value_mentions_float(nullptr));

  EXPECT_EQ(series_intrinsic_helper_body(nullptr, nullptr), nullptr);
  {
    std::unique_ptr<FuncCallAST> missing(FuncCallAST::Create(
      NameAST::Create("series_missing"),
      {NameAST::Create("x")}));
    EXPECT_EQ(series_intrinsic_helper_body(&analyzer, missing.get()), nullptr);
  }
  {
    std::unique_ptr<FunctionAST> regular(FunctionAST::Create(
      NameAST::Create("series_regular"),
      false,
      {ParamAST::Create(NameAST::Create("x"))},
      TypeAST::Create("i64"),
      BlockAST::Create({ReturnAST::Create(NameAST::Create("x"))})));
    analyzer.func_defs["series_regular"] = regular.get();
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      NameAST::Create("series_regular"),
      {NameAST::Create("x")}));
    EXPECT_EQ(series_intrinsic_helper_body(&analyzer, call.get()), nullptr);
  }
  {
    std::unique_ptr<SimpleFuncAST> mismatch(SimpleFuncAST::Create(
      NameAST::Create("series_mismatch"),
      {ParamAST::Create(NameAST::Create("x")), ParamAST::Create(NameAST::Create("y"))},
      SeriesIntrinsicAST::Create(NameAST::Create("x"), SeriesIntrinsicOp::Avg, IntAST::Create("3"))));
    analyzer.func_defs["series_mismatch"] = mismatch.get();
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      NameAST::Create("series_mismatch"),
      {NameAST::Create("x")}));
    EXPECT_EQ(series_intrinsic_helper_body(&analyzer, call.get()), nullptr);
  }
  {
    std::unique_ptr<SimpleFuncAST> not_series(SimpleFuncAST::Create(
      NameAST::Create("series_not_body"),
      {ParamAST::Create(NameAST::Create("x"))},
      IntAST::Create("1")));
    analyzer.func_defs["series_not_body"] = not_series.get();
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      NameAST::Create("series_not_body"),
      {NameAST::Create("x")}));
    EXPECT_EQ(series_intrinsic_helper_body(&analyzer, call.get()), nullptr);
  }
  {
    std::unique_ptr<SimpleFuncAST> base_mismatch(SimpleFuncAST::Create(
      NameAST::Create("series_base_mismatch"),
      {ParamAST::Create(NameAST::Create("x"))},
      SeriesIntrinsicAST::Create(NameAST::Create("other"), SeriesIntrinsicOp::Avg, IntAST::Create("3"))));
    analyzer.func_defs["series_base_mismatch"] = base_mismatch.get();
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      NameAST::Create("series_base_mismatch"),
      {NameAST::Create("x")}));
    EXPECT_EQ(series_intrinsic_helper_body(&analyzer, call.get()), nullptr);
  }
  {
    std::unique_ptr<BinOpAST> expression(BinOpAST::Create(
      StyioOpType::Binary_Add,
      SeriesIntrinsicAST::Create(NameAST::Create("state"), SeriesIntrinsicOp::Avg, IntAST::Create("3")),
      IntAST::Create("1")));
    EXPECT_NE(find_series_intrinsic(&analyzer, expression.get()), nullptr);
  }
  {
    std::unique_ptr<SimpleFuncAST> avg_helper(SimpleFuncAST::Create(
      NameAST::Create("series_avg_step"),
      {ParamAST::Create(NameAST::Create("x"))},
      SeriesIntrinsicAST::Create(NameAST::Create("x"), SeriesIntrinsicOp::Avg, IntAST::Create("3"))));
    analyzer.func_defs["series_avg_step"] = avg_helper.get();
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      NameAST::Create("series_avg_step"),
      {IntAST::Create("9")}));
    std::unique_ptr<StyioIR> ir(lower_state_rhs(&analyzer, call.get(), 7));
    EXPECT_NE(dynamic_cast<SGSeriesAvgStep*>(ir.get()), nullptr);
  }
  EXPECT_EQ(find_series_intrinsic(&analyzer, nullptr), nullptr);
  EXPECT_TRUE(resource_method_preface_bind_type_latest(&analyzer, PassAST::Create()).isUndefined());
  bind_resource_method_preface_local_latest(&analyzer, PassAST::Create());
  {
    std::unique_ptr<FlexBindAST> undefined_bind(FlexBindAST::Create(
      VarAST::Create(NameAST::Create("preface_undefined")),
      PassAST::Create()));
    bind_resource_method_preface_local_latest(&analyzer, undefined_bind.get());
    EXPECT_EQ(analyzer.local_binding_types.count("preface_undefined"), 0u);
  }
  EXPECT_FALSE(stmt_may_contain_pulse_state(&analyzer, FuncCallAST::Create(NameAST::Create("missing"), {})));
  EXPECT_THROW((void)window_n_from_ast(StringAST::Create("bad")), StyioTypeError);
  {
    SGStateSlotDesc desc{};
    desc.kind = static_cast<SGStateSlotKind>(99);
    desc.win_n = 4;
    EXPECT_EQ(slot_byte_size(desc), 8);
  }
  {
    SGStateSlotDesc desc{};
    std::unique_ptr<StateDeclAST> state(StateDeclAST::Create(
      nullptr,
      nullptr,
      nullptr,
      VarAST::Create(NameAST::Create("out"), TypeAST::Create("i64")),
      IntAST::Create("1")));
    EXPECT_THROW(classify_state_slot(&analyzer, state.get(), desc), StyioTypeError);
  }
  {
    std::unique_ptr<TypeAST> cloned_type(clone_type_for_var(nullptr));
    ASSERT_NE(cloned_type, nullptr);
    EXPECT_TRUE(cloned_type->getDataType().isUndefined());
    EXPECT_EQ(clone_var_ast(nullptr), nullptr);
    std::unique_ptr<VarAST> seed(new VarAST(
      NameAST::Create("seed"),
      TypeAST::Create("i64"),
      IntAST::Create("5")));
    std::unique_ptr<VarAST> cloned_var(clone_var_ast(seed.get()));
    ASSERT_NE(cloned_var, nullptr);
    ASSERT_NE(cloned_var->val_init, nullptr);
    EXPECT_EQ(cloned_var->val_init->getNodeType(), StyioNodeType::Integer);
  }

  {
    std::unique_ptr<CharAST> bad_char(CharAST::Create("ab"));
    EXPECT_THROW((void)bad_char->toStyioIR(&analyzer), StyioTypeError);
  }
  {
    const auto max = std::numeric_limits<std::int64_t>::max();
    std::unique_ptr<RangeAST> overflow(new RangeAST(
      IntAST::Create(std::to_string(max - 1)),
      IntAST::Create(std::to_string(max)),
      IntAST::Create("2")));
    EXPECT_THROW((void)overflow->toStyioIR(&analyzer), StyioTypeError);
  }
  {
    const auto min = std::numeric_limits<std::int64_t>::min();
    std::unique_ptr<RangeAST> overflow(new RangeAST(
      IntAST::Create(std::to_string(min + 1)),
      IntAST::Create(std::to_string(min)),
      IntAST::Create("-2")));
    EXPECT_THROW((void)overflow->toStyioIR(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<ResourceRedirectAST> release(ResourceRedirectAST::Create(
      IntAST::Create("1"),
      EmptyResourceAST::Create()));
    EXPECT_THROW((void)release->toStyioIR(&analyzer), StyioTypeError);
  }
  {
    auto huge_resource = styio_make_topology_resource_type(
      styio_data_type_from_name("i64"),
      StyioResourceShapeKind::Fixed,
      static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1u);
    std::unique_ptr<ResourceRefAST> all(ResourceRefAST::CreateSelector(
      NameAST::Create("history"),
      ResourceSelectorKind::SnapshotAll));
    EXPECT_THROW((void)resource_selector_snapshot_depth_latest(all.get(), huge_resource), StyioTypeError);
  }
  {
    auto fixed_resource = styio_make_topology_resource_type(
      styio_data_type_from_name("i64"),
      StyioResourceShapeKind::Fixed,
      3);
    std::unique_ptr<ResourceRefAST> too_deep(ResourceRefAST::CreateSelector(
      NameAST::Create("history"),
      ResourceSelectorKind::SliceFrom,
      std::numeric_limits<int>::min()));
    EXPECT_THROW((void)resource_selector_snapshot_depth_latest(too_deep.get(), fixed_resource), StyioTypeError);
    std::unique_ptr<ResourceRefAST> whole(ResourceRefAST::Create(NameAST::Create("history")));
    EXPECT_THROW((void)lower_resource_selector_snapshot_latest(whole.get(), fixed_resource), StyioTypeError);
  }
  {
    LowererProbe probe;
    std::unique_ptr<ResourceRefAST> missing(ResourceRefAST::CreateSelector(
      NameAST::Create("missing"),
      ResourceSelectorKind::SliceFrom,
      -1));
    EXPECT_THROW((void)missing->toStyioIR(&probe), StyioTypeError);
  }
  {
    LowererProbe probe;
    probe.snapshot_var_names_.insert("snap");
    std::unique_ptr<StateRefAST> snap(StateRefAST::Create(NameAST::Create("snap")));
    std::unique_ptr<StyioIR> ir(snap->toStyioIR(&probe));
    EXPECT_NE(dynamic_cast<SGSnapshotShadowLoad*>(ir.get()), nullptr);
  }
  {
    LowererProbe probe;
    SGPulsePlan plan;
    probe.set_post_pulse_hist_context(5, &plan);
    std::unique_ptr<HistoryProbeAST> hist(HistoryProbeAST::Create(
      StateRefAST::Create(NameAST::Create("missing")),
      IntAST::Create("1")));
    EXPECT_THROW((void)hist->toStyioIR(&probe), StyioTypeError);
  }
  {
    analyzer.set_active_series_slot(7);
    std::unique_ptr<SeriesIntrinsicAST> avg(SeriesIntrinsicAST::Create(
      NameAST::Create("x"),
      SeriesIntrinsicOp::Avg,
      IntAST::Create("3")));
    std::unique_ptr<StyioIR> ir(avg->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGSeriesAvgStep*>(ir.get()), nullptr);
    analyzer.set_active_series_slot(-1);
  }
  {
    PulseScratch scratch;
    std::unordered_map<StyioAST*, StateDeclAST*> cache;
    std::unique_ptr<BlockAST> block(BlockAST::Create({PassAST::Create()}));
    std::unique_ptr<SGPulsePlan> plan(build_pulse_plan(&analyzer, block.get(), &scratch, cache));
    EXPECT_TRUE(plan->slots.empty());
    std::unique_ptr<SGBlock> lowered(lower_pulse_body(&analyzer, block.get(), plan.get(), &scratch, cache));
    ASSERT_NE(lowered, nullptr);
    ASSERT_EQ(lowered->stmts.size(), 1u);
    EXPECT_NE(dynamic_cast<SGNoOp*>(lowered->stmts[0]), nullptr);
  }
  {
    std::unique_ptr<TaskBlockAST> task(TaskBlockAST::Create(BlockAST::Create({PassAST::Create()})));
    std::unique_ptr<StyioIR> ir(task_>toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SIOTaskCreate*>(ir.get()), nullptr);
  }
  {
    std::unique_ptr<TaskGroupLaunchAST> group(TaskGroupLaunchAST::Create({
      TaskBlockAST::Create(BlockAST::Create({PassAST::Create()}))
    }));
    std::unique_ptr<StyioIR> ir(group->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SGEntry*>(ir.get()), nullptr);
  }
  {
    analyzer.local_binding_types["task_source"] = styio_make_task_type("i64");
    std::unique_ptr<FlowBindAST> flow(FlowBindAST::CreateAwait(
      NameAST::Create("task_source"),
      VarAST::Create(NameAST::Create("task_result"), TypeAST::Create("i64")),
      IntAST::Create("0")));
    std::unique_ptr<StyioIR> ir(flow->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SIOFlowBind*>(ir.get()), nullptr);
  }
  {
    analyzer.local_binding_types["scalar_source"] = styio_data_type_from_name("i64");
    std::unique_ptr<FlowBindAST> flow(FlowBindAST::Create(
      NameAST::Create("scalar_source"),
      VarAST::Create(NameAST::Create("scalar_result"))));
    std::unique_ptr<StyioIR> ir(flow->toStyioIR(&analyzer));
    EXPECT_NE(dynamic_cast<SIOFlowBind*>(ir.get()), nullptr);
  }
  {
    std::unique_ptr<StreamZipAST> zip(StreamZipAST::Create(
      ListAST::Create({IntAST::Create("1")}),
      {ParamAST::Create(NameAST::Create("left"))},
      StdStreamAST::Create(StdStreamKind::Stdout),
      {ParamAST::Create(NameAST::Create("right"))},
      BlockAST::Create({PassAST::Create()})));
    EXPECT_THROW((void)zip->toStyioIR(&analyzer), StyioTypeError);
  }
}

}  // namespace
