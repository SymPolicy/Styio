#include <gtest/gtest.h>

#include <memory>

#include "StyioLowering/AstToStyioIRLowerer.hpp"

#include "../src/StyioSema/TypeInfer.cpp"

namespace {

StyioDataType undefined_type() {
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

void expect_matrix_type(
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

class ExposedTypeInferLowerer : public AstToStyioIRLowerer {
 public:
  using StyioSemaContext::binding_info_;
  using StyioSemaContext::consumed_resource_names_;
  using StyioSemaContext::consumed_task_names_;
  using StyioSemaContext::fixed_assignment_names_;
  using StyioSemaContext::owned_resource_names_;
  using StyioSemaContext::resource_binding_types_;
  using StyioSemaContext::task_outer_resource_names_stack_;
};

TEST(StyioTypeInferInternal, MatrixLiteralAndReturnHelpersCoverAnonymousBranches) {
  AstToStyioIRLowerer analyzer;
  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType f64 = styio_data_type_from_name("f64");
  const StyioDataType string_type = styio_data_type_from_name("string");
  const StyioDataType matrix_f64_1x2 = styio_make_matrix_type("f64", 1, 2);

  EXPECT_EQ(merge_matrix_elem_types(undefined_type(), i64).name, "i64");
  EXPECT_EQ(merge_matrix_elem_types(i64, undefined_type()).name, "i64");
  EXPECT_EQ(merge_matrix_elem_types(i64, f64).name, "f64");
  EXPECT_EQ(
    merge_matrix_elem_types(StyioDataType{StyioDataTypeOption::Integer, "int", 32}, i64).name,
    "i64");
  EXPECT_THROW((void)merge_matrix_elem_types(i64, string_type), StyioTypeError);

  {
    std::unique_ptr<StyioAST> scalar(IntAST::Create("1"));
    EXPECT_THROW((void)infer_matrix_literal_info(&analyzer, scalar.get()), StyioTypeError);
  }
  {
    std::unique_ptr<StyioAST> empty(ListAST::Create({}));
    EXPECT_THROW((void)infer_matrix_literal_info(&analyzer, empty.get()), StyioTypeError);
  }
  {
    std::unique_ptr<StyioAST> bad_row(ListAST::Create({IntAST::Create("1")}));
    EXPECT_THROW((void)infer_matrix_literal_info(&analyzer, bad_row.get()), StyioTypeError);
  }
  {
    std::unique_ptr<StyioAST> empty_row(ListAST::Create({ListAST::Create({})}));
    EXPECT_THROW((void)infer_matrix_literal_info(&analyzer, empty_row.get()), StyioTypeError);
  }
  {
    std::unique_ptr<StyioAST> ragged(ListAST::Create({
      ListAST::Create({IntAST::Create("1")}),
      ListAST::Create({IntAST::Create("2"), IntAST::Create("3")})
    }));
    EXPECT_THROW((void)infer_matrix_literal_info(&analyzer, ragged.get()), StyioTypeError);
  }
  {
    std::unique_ptr<StyioAST> mixed(ListAST::Create({
      ListAST::Create({IntAST::Create("1"), FloatAST::Create("2.5")})
    }));
    MatrixLiteralInfo info = infer_matrix_literal_info(&analyzer, mixed.get());
    EXPECT_EQ(info.elem_type.name, "f64");
    EXPECT_EQ(info.rows, 1u);
    EXPECT_EQ(info.cols, 2u);
  }

  EXPECT_TRUE(apply_matrix_literal_context(&analyzer, nullptr, matrix_f64_1x2).isUndefined());
  {
    std::unique_ptr<StyioAST> scalar(IntAST::Create("1"));
    EXPECT_TRUE(apply_matrix_literal_context(&analyzer, scalar.get(), matrix_f64_1x2).isUndefined());
    EXPECT_TRUE(apply_matrix_literal_context(&analyzer, scalar.get(), i64).isUndefined());
  }
  {
    std::unique_ptr<StyioAST> ret(ReturnAST::Create(ListAST::Create({
      ListAST::Create({IntAST::Create("1"), FloatAST::Create("2.5")})
    })));
    expect_matrix_type(apply_matrix_literal_context(&analyzer, ret.get(), matrix_f64_1x2), "f64", 1, 2);
  }
  {
    std::unique_ptr<BlockAST> block(BlockAST::Create({
      ListAST::Create({ListAST::Create({IntAST::Create("1"), FloatAST::Create("2.5")})})
    }));
    block->set_followings({
      ListAST::Create({ListAST::Create({IntAST::Create("3"), IntAST::Create("4")})})
    });
    expect_matrix_type(apply_matrix_literal_context(&analyzer, block.get(), matrix_f64_1x2), "int", 1, 2);
  }

  EXPECT_NO_THROW(require_matrix_return_compatible_latest("plain", i64, i64));
  EXPECT_NO_THROW(require_matrix_return_compatible_latest("matrix_ok", matrix_f64_1x2, matrix_f64_1x2));
  EXPECT_THROW(require_matrix_return_compatible_latest("matrix_bad", matrix_f64_1x2, i64), StyioTypeError);
}

TEST(StyioTypeInferInternal, FlowAndMatchMergeHelpersCoverAnonymousBranches) {
  const StyioDataType undefined = undefined_type();
  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType f64 = styio_data_type_from_name("f64");
  const StyioDataType string_type = styio_data_type_from_name("string");
  const StyioDataType list_i64 = styio_make_list_type("i64");

  EXPECT_EQ(merge_cond_flow_branch_type(undefined, undefined, i64).name, "i64");
  EXPECT_TRUE(merge_cond_flow_branch_type(undefined, undefined, undefined).isUndefined());
  EXPECT_EQ(merge_cond_flow_branch_type(undefined, f64, undefined).name, "f64");
  EXPECT_EQ(merge_cond_flow_branch_type(i64, undefined, undefined).name, "i64");
  EXPECT_EQ(merge_cond_flow_branch_type(i64, f64, undefined).name, "f64");
  EXPECT_TRUE(merge_cond_flow_branch_type(string_type, list_i64, undefined).isUndefined());

  EXPECT_EQ(merge_match_value_type(undefined, string_type).name, "string");
  EXPECT_EQ(merge_match_value_type(i64, undefined).name, "i64");
  EXPECT_EQ(merge_match_value_type(string_type, i64).name, "string");
  EXPECT_EQ(merge_match_value_type(i64, f64).name, "f64");
  EXPECT_EQ(
    merge_match_value_type(
      styio_data_type_from_name("bool"),
      StyioDataType{StyioDataTypeOption::Char, "char", 8}).name,
    "i64");
  EXPECT_TRUE(merge_match_value_type(list_i64, styio_make_dict_type("string", "i64")).isUndefined());

  EXPECT_TRUE(match_result_type_supported(undefined));
  EXPECT_TRUE(match_result_type_supported(i64));
  EXPECT_TRUE(match_result_type_supported(string_type));
  EXPECT_FALSE(match_result_type_supported(list_i64));

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<CondFlowAST> then_only_binding(new CondFlowAST(
      StyioNodeType::CondFlow_Both,
      CondAST::Create(LogicType::RAW, BoolAST::Create(true)),
      BlockAST::Create({
        FinalBindAST::Create(VarAST::Create(NameAST::Create("then_only")), IntAST::Create("1"))
      }),
      BlockAST::Create({PassAST::Create()})));
    EXPECT_NO_THROW(then_only_binding->typeInfer(&analyzer));
    EXPECT_EQ(analyzer.local_binding_types.find("then_only"), analyzer.local_binding_types.end());
  }
}

TEST(StyioTypeInferInternal, CollectionListOperationAndTaskPullEdgesStayExplicit) {
  AstToStyioIRLowerer analyzer;
  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType bool_type = styio_data_type_from_name("bool");

  analyzer.local_binding_types["items"] = styio_make_list_type("bool");
  {
    std::unique_ptr<StyioAST> name(NameAST::Create("items"));
    EXPECT_EQ(infer_collection_elem_type(&analyzer, name.get()).name, "bool");
  }
  {
    std::unique_ptr<StyioAST> literal(ListAST::Create({IntAST::Create("1"), FloatAST::Create("2.5")}));
    EXPECT_EQ(infer_collection_elem_type(&analyzer, literal.get()).name, "i64");
  }
  {
    std::unique_ptr<StyioAST> scalar(StringAST::Create("plain"));
    EXPECT_EQ(infer_collection_elem_type(&analyzer, scalar.get()).name, "i64");
  }

  EXPECT_TRUE(type_is_bool(bool_type));
  EXPECT_FALSE(type_is_bool(i64));
  EXPECT_TRUE(type_is_text_serializable_iterable(styio_make_list_type("i64")));
  EXPECT_TRUE(type_is_text_serializable_iterable(styio_make_dict_type("string", "f64")));
  EXPECT_TRUE(type_is_text_serializable_iterable(styio_make_range_type("i64")));
  EXPECT_FALSE(type_is_text_serializable_iterable(styio_make_dict_type("i64", "f64")));
  EXPECT_FALSE(type_is_text_serializable_iterable(styio_make_list_type(styio_make_file_handle_type("i64").name)));

  {
    analyzer.local_binding_types["not_list"] = i64;
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      NameAST::Create("not_list"),
      NameAST::Create("push"),
      {IntAST::Create("1")}));
    EXPECT_THROW(call->typeInfer(&analyzer), StyioTypeError);
  }
  {
    analyzer.local_binding_types["file_handles"] =
      styio_make_list_type(styio_make_file_handle_type("i64").name);
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      NameAST::Create("file_handles"),
      NameAST::Create("push"),
      {ResourceReceiverAST::Create("file")}));
    EXPECT_THROW(call->typeInfer(&analyzer), StyioTypeError);
  }
  {
    analyzer.local_binding_types["ints"] = styio_make_list_type("i64");
    std::unique_ptr<FuncCallAST> wrong_count(FuncCallAST::Create(
      NameAST::Create("ints"),
      NameAST::Create("push"),
      {}));
    EXPECT_THROW(wrong_count->typeInfer(&analyzer), StyioTypeError);

    std::unique_ptr<FuncCallAST> wrong_value(FuncCallAST::Create(
      NameAST::Create("ints"),
      NameAST::Create("push"),
      {StringAST::Create("bad")}));
    EXPECT_THROW(wrong_value->typeInfer(&analyzer), StyioTypeError);
  }
  {
    analyzer.local_binding_types["task_string"] = styio_make_task_type("string");
    std::unique_ptr<FlowBindAST> bad_target(FlowBindAST::CreateAwait(
      NameAST::Create("task_string"),
      VarAST::Create(NameAST::Create("out"), TypeAST::Create("i64"))));
    EXPECT_THROW(bad_target->typeInfer(&analyzer), StyioTypeError);
  }
}

TEST(StyioTypeInferInternal, BinOpFlowBindAndIterSeqBranchesStayExplicit) {
  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["lhs"] = styio_data_type_from_name("i64");
    auto* nested_rhs = BinOpAST::Create(
      StyioOpType::Binary_Add,
      IntAST::Create("1"),
      FloatAST::Create("2.5"));
    nested_rhs->setDType(styio_data_type_from_name("f64"));
    std::unique_ptr<StyioAST> expr(BinOpAST::Create(
      StyioOpType::Binary_Add,
      NameAST::Create("lhs"),
      nested_rhs));
    expr->typeInfer(&analyzer);
    EXPECT_EQ(static_cast<BinOpAST*>(expr.get())->getType().name, "f64");
  }
  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<BinOpAST> expr(BinOpAST::Create(
      StyioOpType::Binary_Add,
      FloatAST::Create("1.25"),
      IntAST::Create("2")));
    expr->typeInfer(&analyzer);
    EXPECT_EQ(expr->getType().name, "f64");
  }
  {
    AstToStyioIRLowerer analyzer;
    auto* lhs = BinOpAST::Create(
      StyioOpType::Binary_Add,
      IntAST::Create("1"),
      IntAST::Create("2"));
    auto* rhs = BinOpAST::Create(
      StyioOpType::Binary_Add,
      IntAST::Create("3"),
      FloatAST::Create("4.5"));
    std::unique_ptr<BinOpAST> expr(BinOpAST::Create(
      StyioOpType::Binary_Mul,
      lhs,
      rhs));
    expr->typeInfer(&analyzer);
    EXPECT_EQ(expr->getType().name, "f64");
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
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["task"] = styio_make_task_type("i64");
    std::unique_ptr<FlowBindAST> bad_fallback(FlowBindAST::CreateAwait(
      NameAST::Create("task"),
      VarAST::Create(NameAST::Create("out"), TypeAST::Create("i64")),
      StringAST::Create("bad")
    ));
    EXPECT_THROW(bad_fallback->typeInfer(&analyzer), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["slot"] = styio_data_type_from_name("i64");
    std::unique_ptr<FlowBindAST> bad_target(FlowBindAST::Create(
      StringAST::Create("value"),
      VarAST::Create(NameAST::Create("slot"))
    ));
    EXPECT_THROW(bad_target->typeInfer(&analyzer), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> iter(IterSeqAST::Create(
      ListAST::Create({IntAST::Create("1")}),
      {HashTagNameAST::Create({"route"})}
    ));
    EXPECT_THROW(iter->typeInfer(&analyzer), StyioTypeError);
  }
  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<IterSeqAST> iter(IterSeqAST::Create(
      ListAST::Create({IntAST::Create("1")}),
      {HashTagNameAST::Create({"route"})}
    ));
    EXPECT_THROW(analyzer.typeInfer(iter.get()), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["scalar"] = styio_data_type_from_name("i64");
    std::unique_ptr<ParallelAssignAST> bad_target(ParallelAssignAST::Create(
      {new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("scalar"), IntAST::Create("0"))},
      {IntAST::Create("1")}));
    EXPECT_THROW(bad_target->typeInfer(&analyzer), StyioTypeError);
  }
  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["handles"] =
      styio_make_list_type(styio_make_file_handle_type("i64").name);
    std::unique_ptr<ParallelAssignAST> bad_elem(ParallelAssignAST::Create(
      {new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("handles"), IntAST::Create("0"))},
      {FileResourceAST::Create(StringAST::Create("input.txt"), false)}));
    EXPECT_THROW(bad_elem->typeInfer(&analyzer), StyioTypeError);
  }
  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<TupleAST> tuple(TupleAST::Create({NameAST::Create("unknown"), IntAST::Create("1")}));
    tuple->typeInfer(&analyzer);
    EXPECT_FALSE(tuple->isConsistent());
  }
}

TEST(StyioTypeInferInternal, MatrixIntrinsicDictAndResourceHelpersStayExplicit) {
  AstToStyioIRLowerer analyzer;
  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType f64 = styio_data_type_from_name("f64");
  const StyioDataType matrix_i64_2x3 = styio_make_matrix_type("i64", 2, 3);
  const StyioDataType matrix_f64_3x2 = styio_make_matrix_type("f64", 3, 2);

  analyzer.local_binding_types["mi23"] = matrix_i64_2x3;
  analyzer.local_binding_types["mf32"] = matrix_f64_3x2;
  analyzer.local_binding_types["scalar"] = i64;
  analyzer.local_binding_types["text"] = styio_data_type_from_name("string");

  auto infer_call = [&](FuncCallAST* call)
  {
    std::unique_ptr<FuncCallAST> owner(call);
    return infer_matrix_intrinsic_type(&analyzer, owner.get());
  };

  expect_matrix_type(
    infer_call(FuncCallAST::Create(NameAST::Create("mat_zeros"), {IntAST::Create("2"), IntAST::Create("3")})),
    "f64",
    2,
    3);
  expect_matrix_type(
    infer_call(FuncCallAST::Create(NameAST::Create("mat_identity_i64"), {IntAST::Create("4")})),
    "i64",
    4,
    4);
  EXPECT_EQ(
    infer_call(FuncCallAST::Create(NameAST::Create("mat_rows"), {NameAST::Create("mi23")})).name,
    "i64");
  EXPECT_EQ(
    infer_call(FuncCallAST::Create(NameAST::Create("mat_shape"), {NameAST::Create("mi23")})).name,
    "list[i64]");
  EXPECT_EQ(
    infer_call(FuncCallAST::Create(
      NameAST::Create("mat_get"),
      {NameAST::Create("mi23"), IntAST::Create("0"), IntAST::Create("1")})).name,
    "i64");
  EXPECT_EQ(
    infer_call(FuncCallAST::Create(
      NameAST::Create("mat_set"),
      {NameAST::Create("mi23"), IntAST::Create("0"), IntAST::Create("1"), IntAST::Create("9")})).name,
    "i64");
  expect_matrix_type(
    infer_call(FuncCallAST::Create(NameAST::Create("transpose"), {NameAST::Create("mi23")})),
    "i64",
    3,
    2);
  expect_matrix_type(
    infer_call(FuncCallAST::Create(
      NameAST::Create("matmul"),
      {NameAST::Create("mi23"), NameAST::Create("mf32")})),
    "f64",
    2,
    2);
  EXPECT_EQ(
    infer_call(FuncCallAST::Create(NameAST::Create("dot"), {NameAST::Create("mi23"), NameAST::Create("mi23")})).name,
    "i64");
  EXPECT_EQ(
    infer_call(FuncCallAST::Create(NameAST::Create("norm"), {NameAST::Create("mi23")})).name,
    "f64");
  EXPECT_TRUE(
    infer_call(FuncCallAST::Create(NameAST::Create("not_matrix_intrinsic"), {})).isUndefined());
  EXPECT_TRUE(
    infer_matrix_intrinsic_type(&analyzer, nullptr).isUndefined());
  {
    std::unique_ptr<FuncCallAST> method_call(FuncCallAST::Create(
      ResourceReceiverAST::Create("file"),
      NameAST::Create("rows"),
      {NameAST::Create("mi23")}));
    EXPECT_TRUE(infer_matrix_intrinsic_type(&analyzer, method_call.get()).isUndefined());
  }
  EXPECT_THROW(
    (void)infer_call(FuncCallAST::Create(NameAST::Create("mat_zeros"), {IntAST::Create("2")})),
    StyioTypeError);
  EXPECT_THROW(
    (void)infer_call(FuncCallAST::Create(NameAST::Create("mat_rows"), {NameAST::Create("scalar")})),
    StyioTypeError);
  EXPECT_THROW(
    (void)infer_call(FuncCallAST::Create(
      NameAST::Create("mat_set"),
      {NameAST::Create("mi23"), IntAST::Create("0"), IntAST::Create("1"), StringAST::Create("bad")})),
    StyioTypeError);
  EXPECT_THROW(
    (void)infer_call(FuncCallAST::Create(NameAST::Create("mat_scale"), {NameAST::Create("mi23"), NameAST::Create("text")})),
    StyioTypeError);
  EXPECT_THROW(
    (void)infer_call(FuncCallAST::Create(NameAST::Create("matmul"), {NameAST::Create("mi23"), NameAST::Create("mi23")})),
    StyioTypeError);

  expect_matrix_type(matrix_binary_result(matrix_i64_2x3, i64, StyioOpType::Binary_Mul), "i64", 2, 3);
  EXPECT_THROW((void)matrix_binary_result(matrix_i64_2x3, styio_data_type_from_name("string"), StyioOpType::Binary_Mul), StyioTypeError);
  EXPECT_THROW((void)matrix_binary_result(matrix_i64_2x3, i64, StyioOpType::Binary_Add), StyioTypeError);
  EXPECT_TRUE(matrix_binary_result(i64, f64, StyioOpType::Binary_Add).isUndefined());

  {
    std::unique_ptr<DictAST> empty(DictAST::Create());
    EXPECT_EQ(infer_dict_literal_type(&analyzer, empty.get()).name, "dict[string,i64]");
  }
  {
    std::unique_ptr<DictAST> mixed(DictAST::Create({
      {StringAST::Create("a"), IntAST::Create("1")},
      {StringAST::Create("b"), FloatAST::Create("2.5")},
    }));
    EXPECT_EQ(infer_dict_literal_type(&analyzer, mixed.get()).name, "dict[string,f64]");
  }
  {
    std::unique_ptr<DictAST> bad_key(DictAST::Create({
      {IntAST::Create("1"), IntAST::Create("2")},
    }));
    EXPECT_THROW((void)infer_dict_literal_type(&analyzer, bad_key.get()), StyioTypeError);
  }
  {
    std::unique_ptr<DictAST> bad_value(DictAST::Create({
      {StringAST::Create("resource"), ResourceReceiverAST::Create("file")},
    }));
    EXPECT_THROW((void)infer_dict_literal_type(&analyzer, bad_value.get()), StyioTypeError);
  }
}

TEST(StyioTypeInferInternal, FunctionReturnAndReceiverScanHelpersStayExplicit) {
  AstToStyioIRLowerer analyzer;
  const StyioDataType matrix_f64_1x2 = styio_make_matrix_type("f64", 1, 2);

  {
    std::unique_ptr<FunctionAST> fn(FunctionAST::Create(
      NameAST::Create("typed"),
      false,
      {},
      TypeAST::Create("f64"),
      BlockAST::Create({ReturnAST::Create(FloatAST::Create("1.25"))})
    ));
    EXPECT_EQ(func_ret_type_of_def(&analyzer, fn.get()).name, "f64");
  }
  {
    analyzer.push_active_function_body("matrix_fn");
    analyzer.record_inferred_function_return_type(matrix_f64_1x2);
    analyzer.pop_active_function_body();
    std::unique_ptr<FunctionAST> fn(FunctionAST::Create(
      NameAST::Create("matrix_fn"),
      false,
      {},
      TypeAST::Create(styio_make_matrix_type("f64")),
      BlockAST::Create({ReturnAST::Create(NameAST::Create("m"))})
    ));
    expect_matrix_type(func_ret_type_of_def(&analyzer, fn.get()), "f64", 1, 2);
  }
  {
    std::unique_ptr<SimpleFuncAST> fn(SimpleFuncAST::Create(
      NameAST::Create("simple_typed"),
      {},
      TypeAST::Create("bool"),
      BoolAST::Create(true)
    ));
    EXPECT_EQ(func_ret_type_of_def(&analyzer, fn.get()).name, "bool");
  }
  {
    analyzer.push_active_function_body("simple_inferred");
    analyzer.record_inferred_function_return_type(styio_data_type_from_name("string"));
    analyzer.pop_active_function_body();
    std::unique_ptr<SimpleFuncAST> fn(SimpleFuncAST::Create(
      NameAST::Create("simple_inferred"),
      {},
      BoolAST::Create(true)
    ));
    EXPECT_EQ(func_ret_type_of_def(&analyzer, fn.get()).name, "string");
  }
  {
    analyzer.push_active_function_body("simple_matrix");
    analyzer.record_inferred_function_return_type(matrix_f64_1x2);
    analyzer.pop_active_function_body();
    std::unique_ptr<SimpleFuncAST> fn(SimpleFuncAST::Create(
      NameAST::Create("simple_matrix"),
      {},
      TypeAST::Create(styio_make_matrix_type("f64")),
      NameAST::Create("matrix_value")
    ));
    expect_matrix_type(func_ret_type_of_def(&analyzer, fn.get()), "f64", 1, 2);
  }
  {
    std::unique_ptr<SimpleFuncAST> fn(SimpleFuncAST::Create(
      NameAST::Create("simple_tail"),
      {},
      IntAST::Create("7")
    ));
    EXPECT_EQ(func_ret_type_of_def(&analyzer, fn.get()).name, "int");
  }
  EXPECT_TRUE(func_ret_type_of_def(&analyzer, IntAST::Create("1")).isUndefined());

  EXPECT_FALSE(body_consumes_receiver(&analyzer, nullptr, "file"));
  EXPECT_TRUE(match_tail_value_expected(ReturnAST::Create(IntAST::Create("1"))));
  EXPECT_TRUE(match_tail_value_expected(AttrAST::Create(NameAST::Create("obj"), NameAST::Create("field"))));
  EXPECT_FALSE(match_tail_value_expected(PassAST::Create()));
  EXPECT_FALSE(static_i64_literal(IntAST::Create("999999999999999999999999")).has_value());
  EXPECT_EQ(
    binding_value_kind_for_type(styio_make_range_type("i64")),
    StyioSemaContext::BindingValueKind::ListHandle);
  EXPECT_EQ(
    binding_value_kind_for_type(styio_make_task_type("i64")),
    StyioSemaContext::BindingValueKind::TaskHandle);
  EXPECT_FALSE(resource_method_statement_preface_supported_latest(nullptr));
  EXPECT_TRUE(resource_method_statement_preface_supported_latest(PassAST::Create()));
  EXPECT_TRUE(resource_method_value_preface_shape_supported_latest(
    FlexBindAST::Create(VarAST::Create(NameAST::Create("x")), IntAST::Create("1"))));
  EXPECT_FALSE(resource_method_value_preface_shape_supported_latest(ReturnAST::Create(IntAST::Create("1"))));
  EXPECT_FALSE(resource_method_value_preface_supported_latest(&analyzer, ReturnAST::Create(IntAST::Create("1"))));
  EXPECT_TRUE(resource_method_scalar_value_type_supported_latest(styio_data_type_from_name("char")));
  EXPECT_TRUE(resource_method_local_container_type_supported_latest(styio_make_matrix_type("i64", 1, 1)));
  EXPECT_FALSE(resource_method_local_value_type_supported_latest(styio_make_file_handle_type("i64")));
  {
    auto* bind = FinalBindAST::Create(
      VarAST::Create(NameAST::Create("pref"), TypeAST::Create("i64")),
      IntAST::Create("1"));
    EXPECT_EQ(resource_method_preface_bind_type_latest(&analyzer, bind).name, "i64");
    bind_resource_method_preface_type_latest(&analyzer, bind);
    EXPECT_EQ(analyzer.local_binding_types["pref"].name, "i64");
    delete bind;
  }
  {
    std::unique_ptr<BlockAST> block(BlockAST::Create({PassAST::Create()}));
    block->set_followings({ReturnAST::Create(IntAST::Create("9"))});
    EXPECT_TRUE(resource_method_body_contains_return_latest(block.get()));
  }
  {
    std::unique_ptr<BlockAST> body(BlockAST::Create({
      FinalBindAST::Create(VarAST::Create(NameAST::Create("local"), TypeAST::Create("i64")), IntAST::Create("1")),
      ReturnAST::Create(NameAST::Create("local"))
    }));
    EXPECT_EQ(resource_method_simple_result_type_latest(&analyzer, body.get()).name, "i64");
    EXPECT_EQ(analyzer.local_binding_types.find("local"), analyzer.local_binding_types.end());
  }
  {
    std::unique_ptr<StyioAST> redirect(ResourceRedirectAST::Create(
      ResourceReceiverAST::Create("file"),
      EmptyResourceAST::Create()
    ));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, redirect.get(), "file"));
  }
  {
    std::unique_ptr<StyioAST> nested(ResourceWriteAST::Create(
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create()),
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("other"), EmptyResourceAST::Create())
    ));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, nested.get(), "file"));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, nested.get(), "other"));
    EXPECT_FALSE(body_consumes_receiver(&analyzer, nested.get(), "missing"));
  }
  {
    std::unique_ptr<StyioAST> bin(BinOpAST::Create(
      StyioOpType::Binary_Add,
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create()),
      IntAST::Create("1")));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, bin.get(), "file"));
  }
  {
    std::unique_ptr<StyioAST> attr(AttrAST::Create(
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create()),
      NameAST::Create("path")));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, attr.get(), "file"));
  }
  {
    std::unique_ptr<ResourceMethodDefAST> drain(ResourceMethodDefAST::Create(
      "file",
      "drain",
      false,
      false,
      {},
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create())
    ));
    EXPECT_NO_THROW(drain->typeInfer(&analyzer));
    std::unique_ptr<StyioAST> call(FuncCallAST::Create(
      ResourceReceiverAST::Create("file"),
      NameAST::Create("drain"),
      {}));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, call.get(), "file"));
  }
  {
    std::unique_ptr<ResourceMethodDefAST> nullable_param(ResourceMethodDefAST::Create(
      "file",
      "nullable",
      false,
      false,
      {nullptr},
      PassAST::Create()));
    EXPECT_NO_THROW(nullable_param->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<FunctionAST> tuple_return(FunctionAST::Create(
      NameAST::Create("tuple_return"),
      false,
      {},
      TypeTupleAST::Create({TypeAST::Create("i64"), TypeAST::Create("f64")}),
      BlockAST::Create({ReturnAST::Create(IntAST::Create("1"))})));
    EXPECT_THROW(tuple_return->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<StyioAST> block(BlockAST::Create({
      PassAST::Create(),
      FuncCallAST::Create(
        NameAST::Create("plain"),
        {ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create())})
    }));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, block.get(), "file"));
  }
  {
    std::unique_ptr<BlockAST> block(BlockAST::Create({PassAST::Create()}));
    block->set_followings({
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create())
    });
    EXPECT_TRUE(body_consumes_receiver(&analyzer, block.get(), "file"));
  }
  {
    analyzer.local_binding_types["fh"] = styio_make_file_handle_type("i64");
    std::unique_ptr<FlexBindAST> bad_handle(FlexBindAST::Create(
      VarAST::Create(NameAST::Create("copy")),
      NameAST::Create("fh")));
    EXPECT_THROW(bad_handle->typeInfer(&analyzer), StyioTypeError);

    std::unique_ptr<FlexBindAST> bad_empty(FlexBindAST::Create(
      VarAST::Create(NameAST::Create("empty")),
      EmptyResourceAST::Create()));
    EXPECT_THROW(bad_empty->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::vector<ResourceEffectAST::Handler> handlers;
    handlers.emplace_back("io", IntAST::Create("1"));
    std::unique_ptr<ResourceEffectAST> bad_handler(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), styio_data_type_from_name("string")),
      nullptr,
      false,
      std::move(handlers),
      true));
    EXPECT_THROW(bad_handler->typeInfer(&analyzer), StyioTypeError);
  }
}

TEST(StyioTypeInferInternal, TailDictResourceAndFunctionReturnHelperEdgesStayExplicit) {
  AstToStyioIRLowerer analyzer;

  EXPECT_TRUE(match_branch_tail_type(&analyzer, nullptr).isUndefined());
  EXPECT_TRUE(match_branch_tail_type(&analyzer, BlockAST::Create({})).isUndefined());
  EXPECT_THROW(
    (void)match_branch_tail_type(&analyzer, ReturnAST::Create(PassAST::Create())),
    StyioTypeError);

  EXPECT_TRUE(function_body_tail_type_latest(&analyzer, nullptr).isUndefined());
  EXPECT_TRUE(function_body_tail_type_latest(&analyzer, ReturnAST::Create(nullptr)).isUndefined());
  EXPECT_TRUE(function_body_tail_type_latest(&analyzer, BlockAST::Create({})).isUndefined());

  {
    std::unique_ptr<DictAST> dict(DictAST::Create({
      {StringAST::Create("ok"), IntAST::Create("1")},
      {StringAST::Create("bad"), ListAST::Create({IntAST::Create("2")})},
    }));
    EXPECT_THROW((void)infer_dict_literal_type(&analyzer, dict.get()), StyioTypeError);
  }
  {
    std::unique_ptr<DictAST> dict(DictAST::Create({
      {StringAST::Create("skip"), PassAST::Create()},
      {StringAST::Create("ok"), IntAST::Create("3")},
    }));
    EXPECT_EQ(infer_dict_literal_type(&analyzer, dict.get()).name, "dict[string,i64]");
  }

  EXPECT_EQ(resource_family_for_expr(&analyzer, nullptr), "");
  EXPECT_EQ(resource_family_for_expr(&analyzer, ResourceRefAST::Create(NameAST::Create("res"))), "resource");

  {
    analyzer.push_active_function_body("matrix_fn");
    analyzer.record_inferred_function_return_type(styio_make_matrix_type("f64", 2, 3));
    analyzer.pop_active_function_body();
    std::unique_ptr<FunctionAST> fn(FunctionAST::Create(
      NameAST::Create("matrix_fn"),
      false,
      {},
      TypeAST::Create(styio_make_matrix_type("f64", 1, 1)),
      BlockAST::Create({ReturnAST::Create(ListAST::Create({
        ListAST::Create({FloatAST::Create("1.0")})
      }))})));
    StyioDataType ret = func_ret_type_of_def(&analyzer, fn.get());
    expect_matrix_type(ret, "f64", 2, 3);
  }
  {
    std::unique_ptr<PassAST> pass(PassAST::Create());
    EXPECT_TRUE(func_ret_type_of_def(&analyzer, pass.get()).isUndefined());
  }
}

TEST(StyioTypeInferInternal, HelperFallbacksAndBindingEdgesStayExplicit) {
  AstToStyioIRLowerer analyzer;

  EXPECT_TRUE(params_of_func_def(IntAST::Create("1")).empty());
  EXPECT_TRUE(type_convert_target_type(static_cast<NumPromoTy>(99)).isUndefined());
  EXPECT_TRUE(type_convert_source_fallback_type(static_cast<NumPromoTy>(99)).isUndefined());
  EXPECT_TRUE(resource_method_preface_bind_type_latest(&analyzer, PassAST::Create()).isUndefined());
  EXPECT_FALSE(resource_method_body_contains_return_latest(nullptr));
  EXPECT_TRUE(resource_method_simple_result_type_latest(&analyzer, nullptr).isUndefined());
  {
    std::unique_ptr<BlockAST> unsupported_preface(BlockAST::Create({
      FinalBindAST::Create(
        VarAST::Create(NameAST::Create("bad_handle_local"), TypeAST::Create(styio_make_file_handle_type("i64"))),
        StringAST::Create("input.txt")),
      ReturnAST::Create(IntAST::Create("1"))
    }));
    EXPECT_TRUE(resource_method_simple_result_type_latest(&analyzer, unsupported_preface.get()).isUndefined());
  }
  EXPECT_THROW(
    (void)resource_method_simple_result_type_latest(
      &analyzer,
      BlockAST::Create({
        FinalBindAST::Create(VarAST::Create(NameAST::Create("ok")), IntAST::Create("1")),
        PassAST::Create(),
        ReturnAST::Create(TypeConvertAST::Create(IntAST::Create("bad"), NumPromoTy::Bool_To_Int)),
      })),
    StyioTypeError);

  EXPECT_TRUE(infer_expr_type(&analyzer, nullptr).isUndefined());
  EXPECT_EQ(infer_expr_type(&analyzer, CharAST::Create("c")).name, "char");
  EXPECT_EQ(infer_expr_type(&analyzer, FlowBindAST::Create(
    IntAST::Create("1"),
    VarAST::Create(NameAST::Create("flow")))).name, "undefined");
  {
    auto* match = MatchCasesAST::make(
      IntAST::Create("1"),
      CasesAST::Create({{IntAST::Create("1"), ReturnAST::Create(IntAST::Create("2"))}}, nullptr));
    match->setDataType(styio_data_type_from_name("i64"));
    std::unique_ptr<MatchCasesAST> owner(match);
    EXPECT_EQ(infer_expr_type(&analyzer, owner.get()).name, "i64");
  }
  EXPECT_TRUE(infer_expr_type(&analyzer, AttrAST::Create(
    NameAST::Create("obj"),
    IntAST::Create("1"))).isUndefined());
  EXPECT_TRUE(infer_expr_type(&analyzer, new ListOpAST(
    StyioNodeType::Access_By_Index,
    IntAST::Create("1"),
    IntAST::Create("0"))).isUndefined());
  EXPECT_TRUE(infer_expr_type(&analyzer, new ListOpAST(
    StyioNodeType::Access_By_Slice,
    IntAST::Create("1"),
    IntAST::Create("0"))).isUndefined());
  EXPECT_TRUE(infer_expr_type(&analyzer, new ListOpAST(
    StyioNodeType::Access_By_Name,
    IntAST::Create("1"),
    NameAST::Create("field"))).isUndefined());
  EXPECT_EQ(infer_expr_type(&analyzer, WaveMergeAST::Create(
    BoolAST::Create(true),
    IntAST::Create("1"),
    FloatAST::Create("2.5"))).name, "f64");

  analyzer.local_binding_types["dict"] = styio_make_dict_type("string", "string");
  EXPECT_EQ(infer_expr_type(&analyzer, AttrAST::Create(
    NameAST::Create("dict"),
    NameAST::Create("values"))).name, "list[string]");
  {
    std::unique_ptr<ResourceMethodDefAST> prop(ResourceMethodDefAST::Create(
      "file",
      "custom_prop",
      true,
      false,
      {},
      ReturnAST::Create(IntAST::Create("1"))));
    EXPECT_NO_THROW(prop->typeInfer(&analyzer));
    analyzer.local_binding_types["fh2"] = styio_make_file_handle_type("i64");
    EXPECT_EQ(infer_expr_type(&analyzer, AttrAST::Create(
      NameAST::Create("fh2"),
      NameAST::Create("custom_prop"))).name, "i64");
  }
  StyioDataType stdout_handle{
    StyioDataTypeOption::Defined,
    "@stdout",
    0,
    StyioHandleFamily::Stream,
    StyioTypeState::Ready,
    0,
    "string",
    "",
    true,
    static_cast<int>(StdStreamKind::Stdout),
    StyioValueFamily::StreamHandle};
  EXPECT_EQ(resource_family_for_type(stdout_handle), "stdout");
  EXPECT_EQ(
    resource_family_for_type(styio_make_topology_resource_type(
      styio_data_type_from_name("i64"),
      StyioResourceShapeKind::Sequence)),
    "resource");
  EXPECT_FALSE(resource_effect_index_operation_supported_latest(&analyzer, nullptr));
  EXPECT_FALSE(resource_effect_index_operation_supported_latest(
    &analyzer,
    new ListOpAST(StyioNodeType::Access_By_Name, NameAST::Create("dict"), NameAST::Create("key"))));
  EXPECT_FALSE(resource_effect_iterator_operation_supported_latest(&analyzer, nullptr));
  EXPECT_FALSE(resource_effect_operation_supported_latest(&analyzer, nullptr));

  EXPECT_THROW(TypeConvertAST::Create(IntAST::Create("1"), NumPromoTy::Bool_To_Int)->typeInfer(&analyzer), StyioTypeError);
  EXPECT_THROW(TypeConvertAST::Create(FloatAST::Create("1.0"), NumPromoTy::Int_To_Float)->typeInfer(&analyzer), StyioTypeError);
  {
    std::unique_ptr<ParamAST> param(ParamAST::Create(NameAST::Create("p")));
    std::unique_ptr<OptArgAST> opt_arg(OptArgAST::Create(NameAST::Create("arg")));
    std::unique_ptr<OptKwArgAST> opt_kw(OptKwArgAST::Create(NameAST::Create("kw")));
    EXPECT_NO_THROW(param->typeInfer(&analyzer));
    EXPECT_NO_THROW(opt_arg->typeInfer(&analyzer));
    EXPECT_NO_THROW(opt_kw->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<FlexBindAST> bind(FlexBindAST::Create(
      VarAST::Create(NameAST::Create("char_slot")),
      CharAST::Create("x")));
    EXPECT_NO_THROW(bind->typeInfer(&analyzer));
    EXPECT_EQ(bind->getVar()->getDType()->getDataType().name, "char");
  }
  {
    std::unique_ptr<FlexBindAST> bind(FlexBindAST::Create(
      VarAST::Create(NameAST::Create("tuple_slot")),
      TupleAST::Create({IntAST::Create("1"), IntAST::Create("2")})));
    EXPECT_NO_THROW(bind->typeInfer(&analyzer));
    EXPECT_FALSE(bind->getVar()->getDType()->getDataType().isUndefined());
  }
  {
    analyzer.local_binding_types["matrix_value"] = styio_make_matrix_type("i64", 1, 2);
    std::unique_ptr<FlexBindAST> bind(FlexBindAST::Create(
      VarAST::Create(NameAST::Create("matrix_exact"), TypeAST::Create(styio_make_matrix_type("i64", 1, 2))),
      NameAST::Create("matrix_value")));
    EXPECT_NO_THROW(bind->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<BinOpAST> expr(BinOpAST::Create(
      StyioOpType::Binary_Add,
      IntAST::Create("1"),
      IntAST::Create("2")));
    std::unique_ptr<FinalBindAST> bind(FinalBindAST::Create(
      VarAST::Create(NameAST::Create("typed_sum"), TypeAST::Create("i64")),
      expr.release()));
    EXPECT_NO_THROW(bind->typeInfer(&analyzer));
    EXPECT_EQ(static_cast<BinOpAST*>(bind->getValue())->getType().name, "i64");
  }
  {
    auto* match = MatchCasesAST::make(
      IntAST::Create("1"),
      CasesAST::Create({{IntAST::Create("1"), ReturnAST::Create(IntAST::Create("2"))}}, nullptr));
    match->setDataType(styio_data_type_from_name("i64"));
    std::unique_ptr<FinalBindAST> bind(FinalBindAST::Create(
      VarAST::Create(NameAST::Create("match_bound")),
      match));
    EXPECT_NO_THROW(bind->typeInfer(&analyzer));
    EXPECT_EQ(bind->getVar()->getDType()->getDataType().name, "int");
  }
}

TEST(StyioTypeInferInternal, ErrorGuardsAndUnsupportedBranchesStayExplicit) {
  ExposedTypeInferLowerer analyzer;
  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType f64 = styio_data_type_from_name("f64");
  const StyioDataType string_type = styio_data_type_from_name("string");

  {
    std::unique_ptr<ListAST> first_undefined(ListAST::Create({PassAST::Create(), IntAST::Create("1")}));
    EXPECT_EQ(infer_list_literal_type(&analyzer, first_undefined.get()).name, "list[i64]");
    std::unique_ptr<ListAST> next_undefined(ListAST::Create({IntAST::Create("1"), PassAST::Create()}));
    EXPECT_EQ(infer_list_literal_type(&analyzer, next_undefined.get()).name, "list[int]");
  }
  {
    std::unique_ptr<FunctionAST> tuple_return(FunctionAST::Create(
      NameAST::Create("tuple_decl"),
      false,
      {},
      TypeTupleAST::Create({TypeAST::Create("i64")}),
      BlockAST::Create({ReturnAST::Create(IntAST::Create("1"))})));
    EXPECT_TRUE(declared_function_return_type_latest(tuple_return.get()).isUndefined());
    std::unique_ptr<PassAST> pass(PassAST::Create());
    EXPECT_TRUE(declared_function_return_type_latest(pass.get()).isUndefined());
  }

  EXPECT_FALSE(match_pattern_supported_latest(nullptr, nullptr));
  {
    std::unique_ptr<BinCompAST> non_eq(new BinCompAST(
      CompType::NE,
      NameAST::Create("x"),
      IntAST::Create("1")));
    std::string scrutinee = "x";
    EXPECT_FALSE(match_pattern_supported_latest(non_eq.get(), &scrutinee));
  }
  EXPECT_TRUE(container_value_assignable(i64, undefined_type()));
  EXPECT_TRUE(container_value_assignable(f64, i64));
  EXPECT_FALSE(container_value_assignable(i64, string_type));

  {
    std::unique_ptr<ResourceEffectAST> effect(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), string_type),
      nullptr,
      false,
      {},
      true));
    apply_stdin_resource_effect_expected_type(effect.get(), undefined_type());
    apply_stdin_resource_effect_expected_type(effect.get(), i64);
    EXPECT_EQ(static_cast<InstantPullAST*>(effect->getOperation())->getDataType().name, "i64");
  }
  {
    std::unique_ptr<ResourceEffectAST> discard(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), string_type),
      nullptr,
      true,
      {},
      false));
    apply_stdin_resource_effect_expected_type(discard.get(), i64);
    EXPECT_EQ(static_cast<InstantPullAST*>(discard->getOperation())->getDataType().name, "string");
    std::unique_ptr<ResourceEffectAST> stdout_effect(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdout), string_type),
      nullptr,
      false,
      {},
      true));
    apply_stdin_resource_effect_expected_type(stdout_effect.get(), i64);
    EXPECT_EQ(static_cast<InstantPullAST*>(stdout_effect->getOperation())->getDataType().name, "string");
  }

  EXPECT_TRUE(infer_predefined_list_operation_type(&analyzer, nullptr).isUndefined());
  EXPECT_TRUE(infer_predefined_string_operation_type(&analyzer, nullptr).isUndefined());
  {
    analyzer.local_binding_types["scalar"] = i64;
    std::unique_ptr<FuncCallAST> list_push(FuncCallAST::Create(
      NameAST::Create("scalar"),
      NameAST::Create("push"),
      {IntAST::Create("1")}));
    EXPECT_TRUE(infer_predefined_list_operation_type(&analyzer, list_push.get()).isUndefined());
    std::unique_ptr<FuncCallAST> string_lines(FuncCallAST::Create(
      NameAST::Create("scalar"),
      NameAST::Create("lines"),
      {}));
    EXPECT_TRUE(infer_predefined_string_operation_type(&analyzer, string_lines.get()).isUndefined());
  }

  {
    std::unique_ptr<SizeOfAST> missing(new SizeOfAST(nullptr));
    EXPECT_THROW(missing->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<SizeOfAST> scalar(new SizeOfAST(IntAST::Create("1")));
    EXPECT_THROW(scalar->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<ListOpAST> slice_bad_base(new ListOpAST(
      StyioNodeType::Access_By_Slice,
      IntAST::Create("1"),
      IntAST::Create("0"),
      IntAST::Create("1")));
    EXPECT_THROW(slice_bad_base->typeInfer(&analyzer), StyioTypeError);
    analyzer.local_binding_types["items"] = styio_make_list_type("i64");
    std::unique_ptr<ListOpAST> slice_bad_end(new ListOpAST(
      StyioNodeType::Access_By_Slice,
      NameAST::Create("items"),
      IntAST::Create("0"),
      StringAST::Create("bad")));
    EXPECT_THROW(slice_bad_end->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<ListOpAST> append_value(new ListOpAST(
      StyioNodeType::Append_Value,
      NameAST::Create("items"),
      IntAST::Create("1")));
    EXPECT_NO_THROW(append_value->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<ParallelAssignAST> bad_target(ParallelAssignAST::Create(
      {PassAST::Create()},
      {IntAST::Create("1")}));
    EXPECT_THROW(bad_target->typeInfer(&analyzer), StyioTypeError);
    analyzer.local_binding_types["matrix_index"] = styio_make_matrix_type("i64", 1, 1);
    std::unique_ptr<ParallelAssignAST> matrix_index(ParallelAssignAST::Create(
      {new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("matrix_index"), IntAST::Create("0"))},
      {IntAST::Create("1")}));
    EXPECT_THROW(matrix_index->typeInfer(&analyzer), StyioTypeError);
    analyzer.local_binding_types["handle_list"] = styio_make_list_type(styio_make_file_handle_type("i64").name);
    std::unique_ptr<ParallelAssignAST> handle_index(ParallelAssignAST::Create(
      {new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("handle_list"), IntAST::Create("0"))},
      {FileResourceAST::Create(StringAST::Create("input.txt"), false)}));
    EXPECT_THROW(handle_index->typeInfer(&analyzer), StyioTypeError);
  }

  analyzer.local_binding_types["task"] = styio_make_task_type("i64");
  {
    std::unique_ptr<HandleAcquireAST> undeclared_pull(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("out")),
      NameAST::Create("task")));
    EXPECT_THROW(undeclared_pull->typeInfer(&analyzer), StyioTypeError);
  }
  {
    analyzer.local_binding_types["out"] = i64;
    analyzer.fixed_assignment_names_.insert("out");
    std::unique_ptr<HandleAcquireAST> final_pull(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("out")),
      NameAST::Create("task")));
    EXPECT_THROW(final_pull->typeInfer(&analyzer), StyioTypeError);
    analyzer.fixed_assignment_names_.erase("out");
  }
  {
    analyzer.local_binding_types["need_string"] = string_type;
    std::unique_ptr<HandleAcquireAST> mismatched_pull(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("need_string")),
      NameAST::Create("task")));
    EXPECT_THROW(mismatched_pull->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<HandleAcquireAST> untyped_source(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("source_out")),
      PassAST::Create()));
    EXPECT_THROW(untyped_source->typeInfer(&analyzer), StyioTypeError);
    StyioSemaContext::BindingInfo clone_info;
    clone_info.resource_value = true;
    clone_info.value_kind = StyioSemaContext::BindingValueKind::ListHandle;
    clone_info.declared_type = styio_make_list_type("i64");
    analyzer.binding_info_["cloneable_list"] = clone_info;
    std::unique_ptr<HandleAcquireAST> final_clone(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("clone_final")),
      NameAST::Create("cloneable_list")));
    EXPECT_THROW(final_clone->typeInfer(&analyzer), StyioTypeError);
  }

  {
    std::unique_ptr<ResourceWriteAST> destroy_sink(ResourceWriteAST::Create(
      IntAST::Create("1"),
      EmptyResourceAST::Create()));
    EXPECT_THROW(destroy_sink->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<ResourceRedirectAST> bad_destroy(ResourceRedirectAST::Create(
      NameAST::Create("not_resource"),
      EmptyResourceAST::Create()));
    EXPECT_THROW(bad_destroy->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<ResourceRedirectAST> scalar_destroy(ResourceRedirectAST::Create(
      IntAST::Create("1"),
      EmptyResourceAST::Create()));
    EXPECT_THROW(scalar_destroy->typeInfer(&analyzer), StyioTypeError);
  }
  {
    StyioSemaContext::BindingInfo info;
    info.resource_value = true;
    info.declared_type = styio_make_file_handle_type("i64");
    analyzer.binding_info_["fh"] = info;
    analyzer.task_outer_resource_names_stack_.push_back({"fh"});
    std::unique_ptr<ResourceRedirectAST> outer_destroy(ResourceRedirectAST::Create(
      NameAST::Create("fh"),
      EmptyResourceAST::Create()));
    EXPECT_THROW(outer_destroy->typeInfer(&analyzer), StyioTypeError);
    analyzer.task_outer_resource_names_stack_.clear();
    analyzer.consumed_resource_names_.insert("fh");
    std::unique_ptr<ResourceRedirectAST> double_destroy(ResourceRedirectAST::Create(
      NameAST::Create("fh"),
      EmptyResourceAST::Create()));
    EXPECT_THROW(double_destroy->typeInfer(&analyzer), StyioTypeError);
  }

  {
    std::unique_ptr<ResourceEffectAST> unsupported(ResourceEffectAST::Create(
      IntAST::Create("1"),
      nullptr,
      false,
      {},
      true));
    EXPECT_THROW(unsupported->typeInfer(&analyzer), StyioTypeError);
    std::vector<ResourceEffectAST::Handler> unknown_handlers;
    unknown_handlers.emplace_back("mystery", IntAST::Create("1"));
    std::unique_ptr<ResourceEffectAST> unknown(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), i64),
      nullptr,
      false,
      std::move(unknown_handlers),
      true));
    EXPECT_THROW(unknown->typeInfer(&analyzer), StyioTypeError);
    std::vector<ResourceEffectAST::Handler> duplicate_handlers;
    duplicate_handlers.emplace_back("io", IntAST::Create("1"));
    duplicate_handlers.emplace_back("io", IntAST::Create("2"));
    std::unique_ptr<ResourceEffectAST> duplicate(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), i64),
      nullptr,
      false,
      std::move(duplicate_handlers),
      true));
    EXPECT_THROW(duplicate->typeInfer(&analyzer), StyioTypeError);
    std::vector<ResourceEffectAST::Handler> empty_handlers;
    empty_handlers.emplace_back("bounds", EmptyResourceAST::Create());
    std::unique_ptr<ResourceEffectAST> empty_handler(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), i64),
      nullptr,
      false,
      std::move(empty_handlers),
      true));
    EXPECT_THROW(empty_handler->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<ResourceEffectAST> empty_fallback(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), i64),
      EmptyResourceAST::Create(),
      false,
      {},
      true));
    EXPECT_THROW(empty_fallback->typeInfer(&analyzer), StyioTypeError);
  }

  {
    std::unique_ptr<FuncCallAST> insert_count(FuncCallAST::Create(
      NameAST::Create("items"),
      NameAST::Create("insert"),
      {IntAST::Create("0")}));
    EXPECT_THROW(insert_count->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<FuncCallAST> insert_index(FuncCallAST::Create(
      NameAST::Create("items"),
      NameAST::Create("insert"),
      {StringAST::Create("bad"), IntAST::Create("1")}));
    EXPECT_THROW(insert_index->typeInfer(&analyzer), StyioTypeError);
    analyzer.local_binding_types["text"] = string_type;
    std::unique_ptr<FuncCallAST> lines_arg(FuncCallAST::Create(
      NameAST::Create("text"),
      NameAST::Create("lines"),
      {IntAST::Create("1")}));
    EXPECT_THROW(lines_arg->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<ResourceMethodDefAST> prop(ResourceMethodDefAST::Create(
      "file",
      "prop_call",
      false,
      true,
      {},
      ReturnAST::Create(IntAST::Create("1"))));
    EXPECT_NO_THROW(prop->typeInfer(&analyzer));
    std::unique_ptr<FuncCallAST> prop_call(FuncCallAST::Create(
      ResourceReceiverAST::Create("file"),
      NameAST::Create("prop_call"),
      {}));
    EXPECT_THROW(prop_call->typeInfer(&analyzer), StyioTypeError);
    analyzer.local_binding_types["file_handle"] = styio_make_file_handle_type("i64");
  }

  {
    std::unique_ptr<AttrAST> bad_attr(AttrAST::Create(NameAST::Create("text"), IntAST::Create("1")));
    EXPECT_THROW(bad_attr->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<AttrAST> bad_size(AttrAST::Create(IntAST::Create("1"), NameAST::Create("length")));
    EXPECT_THROW(bad_size->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<AttrAST> bad_pressure(AttrAST::Create(IntAST::Create("1"), NameAST::Create("pressure")));
    EXPECT_THROW(bad_pressure->typeInfer(&analyzer), StyioTypeError);
    analyzer.resource_binding_types_["topo"] = styio_make_topology_resource_type(i64, StyioResourceShapeKind::Sequence);
    std::unique_ptr<AttrAST> resource_pressure(AttrAST::Create(NameAST::Create("topo"), NameAST::Create("pressure")));
    EXPECT_THROW(resource_pressure->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<AttrAST> unresolved_method(AttrAST::Create(NameAST::Create("file_handle"), NameAST::Create("missing")));
    EXPECT_THROW(unresolved_method->typeInfer(&analyzer), StyioTypeError);
  }

  {
    std::unique_ptr<FlowBindAST> bare(FlowBindAST::Create(
      nullptr,
      VarAST::Create(NameAST::Create("bare"))));
    EXPECT_THROW(bare->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<FlowBindAST> bare_with_fallback(FlowBindAST::CreateAwait(
      nullptr,
      VarAST::Create(NameAST::Create("bare_fb")),
      IntAST::Create("0")));
    EXPECT_THROW(bare_with_fallback->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<FlowBindAST> already_declared(FlowBindAST::CreateAwait(
      NameAST::Create("task"),
      VarAST::Create(NameAST::Create("out2"))));
    analyzer.local_binding_types["out2"] = i64;
    EXPECT_THROW(already_declared->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<FlowBindAST> await_non_task(FlowBindAST::CreateAwait(
      IntAST::Create("1"),
      VarAST::Create(NameAST::Create("await_scalar"))));
    EXPECT_THROW(await_non_task_>typeInfer(&analyzer), StyioTypeError);
    analyzer.consumed_task_names_.insert("task");
    std::unique_ptr<FlowBindAST> consumed_task(FlowBindAST::CreateAwait(
      NameAST::Create("task"),
      VarAST::Create(NameAST::Create("task_value"))));
    EXPECT_THROW(consumed_task_>typeInfer(&analyzer), StyioTypeError);
  }

  {
    EXPECT_THROW(analyzer.typeInfer(static_cast<MatchCasesAST*>(nullptr)), StyioTypeError);
    std::unique_ptr<MatchCasesAST> null_pattern(MatchCasesAST::make(
      IntAST::Create("1"),
      CasesAST::Create({{nullptr, ReturnAST::Create(IntAST::Create("2"))}}, nullptr)));
    EXPECT_THROW(null_pattern->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<MatchCasesAST> unsupported_pattern(MatchCasesAST::make(
      IntAST::Create("1"),
      CasesAST::Create({{BoolAST::Create(true), ReturnAST::Create(IntAST::Create("2"))}}, nullptr)));
    EXPECT_THROW(unsupported_pattern->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<MatchCasesAST> unsupported_branch(MatchCasesAST::make(
      IntAST::Create("1"),
      CasesAST::Create({{IntAST::Create("1"), ReturnAST::Create(ListAST::Create({IntAST::Create("2")}))}}, nullptr)));
    EXPECT_THROW(unsupported_branch->typeInfer(&analyzer), StyioTypeError);
  }

  {
    std::unique_ptr<DictAST> later_handle_value(DictAST::Create({
      {StringAST::Create("ok"), IntAST::Create("1")},
      {StringAST::Create("bad"), FileResourceAST::Create(StringAST::Create("input.txt"), false)},
    }));
    EXPECT_THROW((void)infer_dict_literal_type(&analyzer, later_handle_value.get()), StyioTypeError);
  }

  {
    analyzer.resource_binding_types_["task_snapshot"] = styio_make_topology_resource_type(
      styio_make_task_type("i64"),
      StyioResourceShapeKind::Fixed,
      2);
    std::unique_ptr<ResourceRefAST> task_slice(ResourceRefAST::CreateSelector(
      NameAST::Create("task_snapshot"),
      ResourceSelectorKind::SnapshotAll));
    EXPECT_THROW(task_slice->typeInfer(&analyzer), StyioTypeError);

    analyzer.resource_binding_types_["huge_history"] = styio_make_topology_resource_type(
      i64,
      StyioResourceShapeKind::Fixed,
      static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1u);
    std::unique_ptr<ResourceRefAST> huge_slice(ResourceRefAST::CreateSelector(
      NameAST::Create("huge_history"),
      ResourceSelectorKind::SnapshotAll));
    EXPECT_THROW(huge_slice->typeInfer(&analyzer), StyioTypeError);
  }

  {
    std::unique_ptr<ListAST> undefined_cell_matrix(ListAST::Create({
      ListAST::Create({PassAST::Create()}),
    }));
    MatrixLiteralInfo info = infer_matrix_literal_info(&analyzer, undefined_cell_matrix.get());
    EXPECT_EQ(info.elem_type.name, "i64");
  }
  EXPECT_FALSE(static_i64_literal(nullptr).has_value());
  EXPECT_FALSE(match_tail_value_expected(nullptr));
  EXPECT_EQ(merge_dict_value_types(undefined_type(), i64).name, "i64");
  EXPECT_EQ(merge_dict_value_types(i64, undefined_type()).name, "i64");
  EXPECT_TRUE(func_param_accepts_arg(i64, undefined_type()));
  EXPECT_TRUE(task_result_type_from_task_type(i64).isUndefined());
  EXPECT_EQ(task_result_type_from_task_type(styio_make_task_type("unit")).name, "i64");
  EXPECT_EQ(infer_task_block_result_type(&analyzer, nullptr).name, "i64");
  {
    std::unique_ptr<DictAST> second_undefined(DictAST::Create({
      {StringAST::Create("ok"), IntAST::Create("1")},
      {StringAST::Create("skip"), PassAST::Create()},
    }));
    EXPECT_EQ(infer_dict_literal_type(&analyzer, second_undefined.get()).name, "dict[string,int]");
  }
  {
    std::unique_ptr<FuncCallAST> clone_call(FuncCallAST::Create(
      NameAST::Create("mat_clone"),
      {NameAST::Create("matrix_index")}));
    EXPECT_TRUE(styio_is_matrix_type(infer_matrix_intrinsic_type(&analyzer, clone_call.get())));
  }
  {
    std::unique_ptr<ResourceMethodDefAST> path_prop(ResourceMethodDefAST::Create(
      "file",
      "path",
      false,
      true,
      {},
      ReturnAST::Create(IntAST::Create("1"))));
    EXPECT_NO_THROW(path_prop->typeInfer(&analyzer));
    std::unique_ptr<AttrAST> builtin_path(AttrAST::Create(
      NameAST::Create("file_handle"),
      NameAST::Create("path")));
    EXPECT_EQ(infer_expr_type(&analyzer, builtin_path.get()).name, "string");
  }
  {
    std::unique_ptr<FuncCallAST> unknown_call(FuncCallAST::Create(NameAST::Create("missing_fn"), {}));
    EXPECT_TRUE(infer_expr_type(&analyzer, unknown_call.get()).isUndefined());
  }
  {
    std::unique_ptr<FuncCallAST> callee_consumes(FuncCallAST::Create(
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create()),
      NameAST::Create("ignored"),
      {}));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, callee_consumes.get(), "file"));
  }
  {
    std::unique_ptr<BinOpAST> bool_concat(BinOpAST::Create(
      StyioOpType::Binary_Add,
      BoolAST::Create(true),
      BoolAST::Create(false)));
    EXPECT_FALSE(infer_numeric_string_coercion(&analyzer, bool_concat.get(), bool_concat->getLHS(), bool_concat->getRHS()));
    std::unique_ptr<BinOpAST> string_bool(BinOpAST::Create(
      StyioOpType::Binary_Add,
      StringAST::Create("text"),
      BoolAST::Create(false)));
    EXPECT_FALSE(infer_numeric_string_coercion(&analyzer, string_bool.get(), string_bool->getLHS(), string_bool->getRHS()));
  }
  {
    std::unique_ptr<ParamAST> param(ParamAST::Create(NameAST::Create("direct_param")));
    std::unique_ptr<OptArgAST> opt_arg(OptArgAST::Create(NameAST::Create("direct_arg")));
    std::unique_ptr<OptKwArgAST> opt_kw(OptKwArgAST::Create(NameAST::Create("direct_kw")));
    std::unique_ptr<VarTupleAST> var_tuple(VarTupleAST::Create({VarAST::Create(NameAST::Create("tuple_var"))}));
    EXPECT_NO_THROW(analyzer.typeInfer(param.get()));
    EXPECT_NO_THROW(analyzer.typeInfer(opt_arg.get()));
    EXPECT_NO_THROW(analyzer.typeInfer(opt_kw.get()));
    EXPECT_NO_THROW(analyzer.typeInfer(var_tuple.get()));
  }
  {
    analyzer.local_binding_types["already_bound_resource"] = i64;
    std::unique_ptr<HandleAcquireAST> redefined(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("already_bound_resource")),
      FileResourceAST::Create(StringAST::Create("input.txt"), false)));
    EXPECT_THROW(redefined->typeInfer(&analyzer), StyioTypeError);

    analyzer.fixed_assignment_names_.insert("fixed_clone_target");
    std::unique_ptr<HandleAcquireAST> fixed_clone(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("fixed_clone_target")),
      NameAST::Create("cloneable_list"),
      HandleAcquireAST::BindMode::Flex));
    EXPECT_THROW(fixed_clone->typeInfer(&analyzer), StyioTypeError);

    StyioSemaContext::BindingInfo non_cloneable;
    non_cloneable.resource_value = true;
    non_cloneable.value_kind = StyioSemaContext::BindingValueKind::I64;
    non_cloneable.declared_type = i64;
    analyzer.binding_info_["non_cloneable_resource"] = non_cloneable;
    std::unique_ptr<HandleAcquireAST> bad_clone(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("bad_clone_target")),
      NameAST::Create("non_cloneable_resource"),
      HandleAcquireAST::BindMode::Flex));
    EXPECT_THROW(bad_clone->typeInfer(&analyzer), StyioTypeError);
  }
  {
    auto* match = MatchCasesAST::make(
      IntAST::Create("1"),
      CasesAST::Create({{IntAST::Create("1"), ReturnAST::Create(IntAST::Create("2"))}}, nullptr));
    match->setDataType(i64);
    std::unique_ptr<FinalBindAST> bind(FinalBindAST::Create(
      VarAST::Create(NameAST::Create("match_final_extra")),
      match));
    EXPECT_NO_THROW(bind->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<BinOpAST> missing_rhs(BinOpAST::Create(
      StyioOpType::Binary_Add,
      IntAST::Create("1"),
      nullptr));
    EXPECT_NO_THROW(missing_rhs->typeInfer(&analyzer));
    EXPECT_TRUE(missing_rhs->getType().isUndefined());

    std::unique_ptr<BinOpAST> self_assign_unknown(BinOpAST::Create(
      StyioOpType::Self_Add_Assign,
      NameAST::Create("missing_self"),
      IntAST::Create("1")));
    EXPECT_NO_THROW(self_assign_unknown->typeInfer(&analyzer));
    EXPECT_EQ(self_assign_unknown->getType().name, "i64");

    std::unique_ptr<BinOpAST> float_unsupported(BinOpAST::Create(
      StyioOpType::Bitwise_AND,
      FloatAST::Create("1.0"),
      FloatAST::Create("2.0")));
    EXPECT_NO_THROW(float_unsupported->typeInfer(&analyzer));

    auto* lhs_bin = BinOpAST::Create(
      StyioOpType::Binary_Add,
      IntAST::Create("1"),
      IntAST::Create("2"));
    lhs_bin->setDType(i64);
    std::unique_ptr<BinOpAST> rhs_unknown(BinOpAST::Create(
      StyioOpType::Binary_Add,
      lhs_bin,
      NameAST::Create("rhs_missing")));
    EXPECT_NO_THROW(rhs_unknown->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<ResourceDeclAST> with_driver(ResourceDeclAST::Create(
      {{NameAST::Create("driven_resource"), TypeAST::Create(i64)}},
      BlockAST::Create({PassAST::Create()})));
    EXPECT_NO_THROW(with_driver->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<ResourceMethodDefAST> accepts_untyped(ResourceMethodDefAST::Create(
      "file",
      "accepts_untyped",
      false,
      false,
      {ParamAST::Create(NameAST::Create("value"))},
      ReturnAST::Create(IntAST::Create("1"))));
    EXPECT_NO_THROW(accepts_untyped->typeInfer(&analyzer));
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      ResourceReceiverAST::Create("file"),
      NameAST::Create("accepts_untyped"),
      {IntAST::Create("7")}));
    EXPECT_NO_THROW(call->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<ResourceMethodDefAST> close_again(ResourceMethodDefAST::Create(
      "file",
      "close_again",
      false,
      false,
      {ParamAST::Create(NameAST::Create("ignored"))},
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create())));
    EXPECT_NO_THROW(close_again->typeInfer(&analyzer));
    analyzer.local_binding_types["destroyed_handle"] = styio_make_file_handle_type("i64");
    StyioSemaContext::BindingInfo handle_info;
    handle_info.resource_value = true;
    handle_info.declared_type = styio_make_file_handle_type("i64");
    analyzer.binding_info_["destroyed_handle"] = handle_info;
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      NameAST::Create("destroyed_handle"),
      NameAST::Create("close_again"),
      {ResourceRedirectAST::Create(NameAST::Create("destroyed_handle"), EmptyResourceAST::Create())}));
    EXPECT_THROW(call->typeInfer(&analyzer), StyioTypeError);
    analyzer.binding_info_.erase("destroyed_handle");
    analyzer.local_binding_types.erase("destroyed_handle");
  }
  {
    std::unique_ptr<SimpleFuncAST> echo(SimpleFuncAST::Create(
      NameAST::Create("echo_untyped"),
      {ParamAST::Create(NameAST::Create("value"))},
      NameAST::Create("value")));
    analyzer.func_defs["echo_untyped"] = echo.get();
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      NameAST::Create("echo_untyped"),
      {IntAST::Create("5")}));
    EXPECT_NO_THROW(call->typeInfer(&analyzer));
    EXPECT_EQ(infer_expr_type(&analyzer, call.get()).name, "int");
  }
  {
    analyzer.local_binding_types["final_flow_target"] = i64;
    analyzer.fixed_assignment_names_.insert("final_flow_target");
    std::unique_ptr<FlowBindAST> final_flow(FlowBindAST::Create(
      IntAST::Create("1"),
      VarAST::Create(NameAST::Create("final_flow_target"))));
    EXPECT_THROW(final_flow->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<MatchCasesAST> bad_default(MatchCasesAST::make(
      IntAST::Create("1"),
      CasesAST::Create({}, ReturnAST::Create(ListAST::Create({IntAST::Create("1")})))));
    EXPECT_THROW(bad_default->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<MainBlockAST> exported(MainBlockAST::Create({
      ExportDeclAST::Create({"exported_name"}),
    }));
    EXPECT_NO_THROW(exported->typeInfer(&analyzer));
  }
}

}  // namespace
