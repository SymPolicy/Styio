#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassInstrumentation.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include <llvm/Transforms/Utils.h>

#include "StyioIR/IRDecl.hpp"
#include "StyioJIT/StyioJIT_ORC.hpp"
#include "StyioNative/NativeInterop.hpp"

// Expose StyioToLLVM internals for focused destructor ownership tests.
#define private public
#include "StyioCodeGen/CodeGenVisitor.hpp"
#undef private
#include "StyioException/Exception.hpp"
#include "StyioIR/GenIR/GenIR.hpp"

namespace {

void init_llvm_once() {
  static bool initialized = false;
  if (!initialized) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    initialized = true;
  }
}

std::unique_ptr<StyioToLLVM> make_generator() {
  init_llvm_once();
  llvm::ExitOnError exit_on_error;
  std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
  return std::make_unique<StyioToLLVM>(std::move(jit));
}

StyioDataType i64_type() {
  return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
}

StyioDataType i32_type() {
  return StyioDataType{StyioDataTypeOption::Integer, "i32", 32};
}

StyioDataType f32_type() {
  return StyioDataType{StyioDataTypeOption::Float, "f32", 32};
}

StyioDataType f64_type() {
  return StyioDataType{StyioDataTypeOption::Float, "f64", 64};
}

StyioDataType string_type() {
  return StyioDataType{StyioDataTypeOption::String, "string", 0};
}

StyioDataType bool_type() {
  return StyioDataType{StyioDataTypeOption::Bool, "bool", 1};
}

StyioDataType char_type() {
  return StyioDataType{StyioDataTypeOption::Char, "char", 8};
}

StyioDataType opaque_defined_type() {
  return StyioDataType{StyioDataTypeOption::Defined, "opaque", 0};
}

StyioDataType list_type(const std::string& elem_type = "i64") {
  return styio_make_list_type(elem_type);
}

StyioDataType dict_type(const std::string& value_type = "i64") {
  return styio_make_dict_type("string", value_type);
}

StyioDataType matrix_type(const std::string& elem_type = "i64") {
  return styio_make_matrix_type(elem_type, 1, 2);
}

StyioDataType task_type(const std::string& result_type = "i64") {
  StyioDataType type{
    StyioDataTypeOption::Defined,
    "task[" + result_type + "]",
    0,
    StyioHandleFamily::Task,
    StyioTypeState::Ready,
    0,
    result_type,
    "",
    false,
    -1,
    StyioValueFamily::TaskHandle};
  return type;
}

StyioDataType bounded_ring_type(const std::string& elem_type, std::uint64_t capacity) {
  return StyioDataType{
    StyioDataTypeOption::Defined,
    "bounded_ring:" + elem_type + ":" + std::to_string(capacity),
    0};
}

SGVar* var(const std::string& name, StyioDataType type) {
  return SGVar::Create(SGResId::Create(name), SGType::Create(std::move(type)));
}

SGVar* dynamic_var(const std::string& name, StyioDataType type) {
  SGVar* out = var(name, std::move(type));
  out->is_dynamic_slot = true;
  return out;
}

SCListLiteral* list_i64() {
  return SCListLiteral::Create({SGConstInt::Create(1), SGConstInt::Create(2)}, "i64");
}

SCDictLiteral* dict_i64() {
  return SCDictLiteral::Create({
    {SGConstString::Create("a"), SGConstInt::Create(1)},
    {SGConstString::Create("b"), SGConstInt::Create(2)},
  }, "i64");
}

SCMatrixLiteral* matrix_i64() {
  return SCMatrixLiteral::Create({SGConstInt::Create(1), SGConstInt::Create(2)}, "i64", 1, 2);
}

SCMatrixLiteral* matrix_f64() {
  return SCMatrixLiteral::Create({SGConstFloat::Create("1.5"), SGConstInt::Create(2)}, "f64", 1, 2);
}

void expect_codegen_ok(std::vector<StyioIR*> stmts, std::vector<std::string> needles) {
  auto generator = make_generator();
  std::unique_ptr<SGMainEntry> entry(SGMainEntry::Create(std::move(stmts)));
  EXPECT_NO_THROW(entry->toLLVMIR(generator.get()));
  const std::string ir = generator->dump_llvm_ir();
  for (const auto& needle : needles) {
    EXPECT_NE(ir.find(needle), std::string::npos) << needle << "\n" << ir;
  }
}

void expect_codegen_throws(std::vector<StyioIR*> stmts, const std::string& message) {
  auto generator = make_generator();
  std::unique_ptr<SGMainEntry> entry(SGMainEntry::Create(std::move(stmts)));
  try {
    (void)entry->toLLVMIR(generator.get());
    FAIL() << "expected codegen failure";
  }
  catch (const StyioTypeError& ex) {
    EXPECT_NE(std::string(ex.what()).find(message), std::string::npos) << ex.what();
  }
}

void expect_direct_codegen_throws(StyioIR* node, const std::string& message) {
  auto generator = make_generator();
  std::unique_ptr<StyioIR> owner(node);
  try {
    (void)owner->toLLVMIR(generator.get());
    FAIL() << "expected direct codegen failure";
  }
  catch (const StyioTypeError& ex) {
    EXPECT_NE(std::string(ex.what()).find(message), std::string::npos) << ex.what();
  }
}

void expect_direct_codegen_ok(StyioIR* node) {
  auto generator = make_generator();
  std::unique_ptr<StyioIR> owner(node);
  EXPECT_NO_THROW((void)owner->toLLVMIR(generator.get()));
}

}  // namespace

TEST(StyioCodeGenInternal, PassiveIrNodesAndScalarCastGuardsStayExplicit) {
  expect_codegen_ok({
    SGType::Create(i64_type()),
    SGFormatString::Create({"hello ", ""}, {SGConstInt::Create(7)}),
    SGStruct::Create(
      SGResId::Create("Pair"),
      {SGVar::Create(SGResId::Create("x"), SGType::Create(i64_type()))}),
    SGVar::Create(SGResId::Create("slot"), SGType::Create(i64_type())),
    SGFuncArg::Create("arg", SGType::Create(i64_type())),
  }, {});

	  expect_direct_codegen_throws(
	    SGCast::Create(SGType::Create(i64_type()), SGType::Create(f64_type())),
	    "cast lowering requires a value and target type");

  expect_direct_codegen_ok(SGExportDecl::Create({"direct_symbol"}));
  expect_direct_codegen_ok(SGExternBlock::Create("c", "int direct_symbol(void) { return 0; }\n"));

  expect_codegen_throws({
    SGCast::Create(
      SGConstBool::Create(true),
      SGType::Create(bool_type()),
      SGType::Create(string_type())),
  }, "unsupported scalar cast lowering");
}

TEST(StyioCodeGenInternal, BuiltinListAndMatrixCoercionsStayExplicit) {
  expect_codegen_ok({
    SGCall::Create(SGResId::Create("__styio_list_pop"), {SGConstBool::Create(true)}),
    SGCall::Create(SGResId::Create("__styio_list_push_f64"), {
      SGConstBool::Create(true),
      SGConstInt::Create(7),
    }),
    SGCall::Create(SGResId::Create("__styio_list_push_char"), {
      SGConstInt::Create(1),
      SGConstFloat::Create("2.5"),
    }),
    SGCall::Create(SGResId::Create("__styio_list_push_cstr"), {
      SGConstInt::Create(1),
      SGConstInt::Create(9),
    }),
    SGCall::Create(SGResId::Create("__styio_list_push_bool"), {
      SGConstInt::Create(1),
      SGConstBool::Create(false),
    }),
    SGCall::Create(SGResId::Create("__styio_list_insert_i64"), {
      SGConstInt::Create(1),
      SGConstBool::Create(true),
      SGConstBool::Create(false),
    }),
    SGCall::Create(SGResId::Create("__styio_list_push_list"), {
      SGConstInt::Create(1),
      list_i64(),
    }),
    SGCall::Create(SGResId::Create("__styio_matrix_rows"), {
      SGConstString::Create("not-a-handle"),
    }),
    SGCall::Create(SGResId::Create("__styio_matrix_set_i64"), {
      SGConstFloat::Create("1.25"),
      SGConstBool::Create(true),
      SGConstFloat::Create("2.25"),
      SGConstString::Create("value"),
    }),
    SGCall::Create(SGResId::Create("__styio_matrix_set_f64"), {
      SGConstString::Create("matrix"),
      SGConstBool::Create(true),
      SGConstBool::Create(false),
      SGConstString::Create("value"),
    }),
	    SGCall::Create(SGResId::Create("__styio_matrix_scale_f64"), {
	      SGConstString::Create("matrix"),
	      SGConstInt::Create(3),
	    }),
	    SGBinOp::Create(
	      matrix_i64(),
	      matrix_i64(),
	      StyioOpType::Binary_Add,
	      SGType::Create(matrix_type("i64")),
	      matrix_type("i64"),
	      matrix_type("i64")),
	    SGBinOp::Create(
	      matrix_i64(),
	      matrix_i64(),
	      StyioOpType::Binary_Mul,
	      SGType::Create(matrix_type("i64")),
	      matrix_type("i64"),
	      matrix_type("i64")),
	    SGBinOp::Create(
	      matrix_i64(),
	      SGConstFloat::Create("2.5"),
	      StyioOpType::Binary_Mul,
	      SGType::Create(matrix_type("i64")),
	      matrix_type("i64"),
	      f64_type()),
	    SGBinOp::Create(
	      matrix_f64(),
	      SGConstInt::Create(2),
	      StyioOpType::Binary_Mul,
	      SGType::Create(matrix_type("f64")),
	      matrix_type("f64"),
	      i64_type()),
	  }, {
	    "styio_list_pop",
	    "styio_list_push_f64",
	    "styio_list_push_char",
	    "styio_matrix_rows",
	    "styio_matrix_set_f64",
	    "styio_matrix_scale_f64",
	    "styio_matrix_new_i64",
	  });
	}

TEST(StyioCodeGenInternal, BuiltinCallGuardsFailBeforeBadLlvmEmission) {
  expect_codegen_throws({
    SGCall::Create(SGResId::Create("__styio_list_pop"), {
      SGConstInt::Create(1),
      SGConstInt::Create(2),
    }),
  }, "runtime list pop expects 1 argument");

  expect_codegen_throws({
    SGCall::Create(SGResId::Create("__styio_string_lines"), {
      SGConstInt::Create(1),
    }),
  }, "runtime string.lines requires a string argument");
  expect_codegen_throws({
    SGCall::Create(SGResId::Create("__styio_string_lines"), {}),
  }, "runtime string.lines expects 1 argument");

  expect_codegen_throws({
    SGCall::Create(SGResId::Create("__styio_list_range_i64"), {
      SGConstInt::Create(1),
      SGConstFloat::Create("2.0"),
      SGConstInt::Create(1),
    }),
  }, "runtime range list requires integer arguments");
  expect_codegen_throws({
    SGCall::Create(SGResId::Create("__styio_list_range_i64"), {
      SGConstInt::Create(1),
      SGConstInt::Create(2),
    }),
  }, "runtime range list expects 3 arguments");

  expect_codegen_throws({
    SGCall::Create(SGResId::Create("__styio_list_insert_i64"), {
      SGConstInt::Create(1),
      SGConstInt::Create(2),
    }),
  }, "runtime list insert expects 3 argument");

  expect_codegen_throws({
    SGCall::Create(SGResId::Create("__styio_matrix_rows"), {}),
  }, "matrix runtime helper `styio_matrix_rows` expects 1 argument");
}

TEST(StyioCodeGenInternal, GetTypeUsesExistingFunctionReturnTypeForCalls) {
  auto generator = make_generator();
  auto* function =
    SGFunc::Create(
      SGType::Create(f64_type()),
      SGResId::Create("typed_call_target"),
      {},
      SGBlock::Create({SGReturn::Create(SGConstFloat::Create("1.0"))}));
  std::unique_ptr<SGMainEntry> entry(SGMainEntry::Create({function}));
  EXPECT_NO_THROW((void)entry->toLLVMIR(generator.get()));

  std::unique_ptr<StyioIR> call(SGCall::Create(SGResId::Create("typed_call_target"), {}));
  llvm::Type* call_type = call->toLLVMType(generator.get());
  ASSERT_NE(call_type, nullptr);
  EXPECT_TRUE(call_type->isDoubleTy());
}

TEST(StyioCodeGenInternal, GetTypeUsesBoundedRingElementTypeForResourceIds) {
  auto generator = make_generator();
  std::unique_ptr<SGMainEntry> entry(
    SGMainEntry::Create({
      SGFinalBind::Create(var("recent_values", bounded_ring_type("f64", 2)), SGConstFloat::Create("1.5")),
    }));
  EXPECT_NO_THROW((void)entry->toLLVMIR(generator.get()));

  std::unique_ptr<StyioIR> ring_name(SGResId::Create("recent_values"));
  llvm::Type* ring_element_type = ring_name->toLLVMType(generator.get());
  ASSERT_NE(ring_element_type, nullptr);
  EXPECT_TRUE(ring_element_type->isDoubleTy());
}

TEST(StyioCodeGenInternal, GetTypeCoversScalarFallbackDefaults) {
  auto generator = make_generator();
  auto expect_i64 = [&](StyioIR* node)
  {
    std::unique_ptr<StyioIR> owner(node);
    llvm::Type* type = node->toLLVMType(generator.get());
    ASSERT_NE(type, nullptr);
    EXPECT_TRUE(type->isIntegerTy(64));
  };

  expect_i64(SGMatch::Create(
    SGConstInt::Create(1),
    {},
    SGBlock::Create({SGNoOp::Create()}),
    SGMatchReprKind::ExprInt));
  expect_i64(SGFallback::Create(SGConstInt::Create(0), SGConstInt::Create(1)));
  expect_i64(SGDynLoad::Create("invalid_dynamic_load", static_cast<SGDynLoadKind>(255)));

  StyioDataType invalid_type{static_cast<StyioDataTypeOption>(255), "invalid", 0};
  std::unique_ptr<StyioIR> invalid_effect(
    SIOResourceEffect::Create(SGNoOp::Create(), nullptr, false, invalid_type, {}, true));
  llvm::Type* invalid_effect_type = invalid_effect->toLLVMType(generator.get());
  ASSERT_NE(invalid_effect_type, nullptr);
  EXPECT_TRUE(invalid_effect_type->isVoidTy());
}

TEST(StyioCodeGenInternal, ResourceEffectValueEdgesStayExplicit) {
  expect_codegen_ok({
    SIOResourceEffect::Create(SGNoOp::Create(), nullptr, false, i64_type(), {}, true),
    SIOResourceEffect::Create(
      SGConstInt::Create(7),
      nullptr,
      false,
      i64_type(),
      {SIOResourceEffect::Handler("io", SGConstInt::Create(1))},
      true),
  }, {
    "resource_effect_continue",
    "resource_handler",
  });

  expect_codegen_ok({
    SIOResourceEffect::Create(
      SGReturn::Create(SGConstInt::Create(9)),
      nullptr,
      false,
      i64_type(),
      {},
      true),
  }, {});

  expect_codegen_throws({
    SIOResourceEffect::Create(SGBreak::Create(), nullptr, false, i64_type(), {}, true),
  }, "break outside enclosing loop");
}

TEST(StyioCodeGenInternal, CodeGenFactoryCreatesGenerator) {
  init_llvm_once();
  llvm::ExitOnError exit_on_error;
  std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
  std::unique_ptr<StyioToLLVM> generator(StyioToLLVM::Create(std::move(jit)));

  ASSERT_NE(generator, nullptr);
  EXPECT_FALSE(generator->dump_llvm_ir().empty());
}

TEST(StyioCodeGenInternal, JitAddModuleUsesDefaultResourceTracker) {
  init_llvm_once();
  llvm::ExitOnError exit_on_error;
  std::unique_ptr<StyioJIT_ORC> jit = exit_on_error(StyioJIT_ORC::Create());
  auto context = std::make_unique<llvm::LLVMContext>();
  auto module = std::make_unique<llvm::Module>("empty_default_tracker", *context);
  module->setDataLayout(jit->getDataLayout());

  exit_on_error(jit->addModule(llvm::orc::ThreadSafeModule(std::move(module), std::move(context))));
}

TEST(StyioCodeGenInternal, GeneratorDestructorClosesTrackedNativeHandles) {
  auto generator = make_generator();
  generator->native_library_handles_.push_back(nullptr);
  generator.reset();
  SUCCEED();
}

TEST(StyioCodeGenInternal, OwnershipHelpersIgnoreNullAndNonResourceValues) {
  auto generator = make_generator();
  llvm::LLVMContext context;
  llvm::Value* i32_value = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 7);

  EXPECT_EQ(generator->clone_cstr_for_runtime_owner(nullptr), nullptr);
  EXPECT_FALSE(generator->take_owned_cstr_temp(nullptr));
  generator->free_cstr_if_runtime_owned(nullptr);
  generator->store_bounded_ring_value(nullptr, nullptr, nullptr, nullptr, std::nullopt);
  generator->move_bounded_ring_value(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, std::nullopt);

  generator->track_owned_resource_temp(nullptr, StyioToLLVM::TempResourceKind::List);
  EXPECT_FALSE(generator->take_owned_resource_temp(nullptr).has_value());
  generator->free_resource_if_runtime_owned(nullptr, StyioToLLVM::TempResourceKind::List);
  EXPECT_EQ(generator->clone_resource_handle_for_runtime_owner(nullptr, StyioValueFamily::ListHandle), nullptr);

  llvm::Value* widened = generator->clone_resource_handle_for_runtime_owner(i32_value, StyioValueFamily::Integer);
  ASSERT_NE(widened, nullptr);
  EXPECT_TRUE(widened->getType()->isIntegerTy(64));
  generator->free_resource_handle_if_runtime_owned(nullptr, StyioValueFamily::ListHandle);
}

TEST(StyioCodeGenInternal, RuntimeReturnHelpersCoverGuardEdges) {
  auto generator = make_generator();
  llvm::LLVMContext context;
  llvm::IRBuilder<> builder(context);
  llvm::Module scratch_module("styio.codegen.internal", context);
  auto* scratch_fn_type = llvm::FunctionType::get(builder.getVoidTy(), false);
  auto* scratch_fn = llvm::Function::Create(
    scratch_fn_type, llvm::Function::ExternalLinkage, "scratch", scratch_module);
  auto* scratch_entry = llvm::BasicBlock::Create(context, "entry", scratch_fn);
  builder.SetInsertPoint(scratch_entry);

  styio::native::CType invalid_c_type;
  invalid_c_type.kind = static_cast<styio::native::CTypeKind>(255);
  EXPECT_TRUE(generator->native_c_type_to_llvm(invalid_c_type)->isIntegerTy(64));

  EXPECT_EQ(generator->default_runtime_return_value(nullptr), nullptr);
  EXPECT_EQ(generator->default_runtime_return_value(builder.getVoidTy()), nullptr);
  EXPECT_TRUE(generator->default_runtime_return_value(builder.getDoubleTy())->getType()->isDoubleTy());
  EXPECT_TRUE(generator->default_runtime_return_value(llvm::PointerType::get(context, 0))->getType()->isPointerTy());
  EXPECT_TRUE(generator->default_runtime_return_value(builder.getInt32Ty())->getType()->isIntegerTy(32));

  auto* struct_ty = llvm::StructType::create(context, "styio.test.ret");
  std::vector<llvm::Type*> struct_fields{builder.getInt64Ty()};
  struct_ty->setBody(struct_fields);
  EXPECT_TRUE(generator->default_runtime_return_value(struct_ty)->getType()->isStructTy());

  generator->emit_file_handle_slot_close(nullptr);
  generator->emit_bounded_ring_pending_commit("missing_ring");
  generator->emit_bounded_ring_pending_commits();
  generator->release_bounded_ring_cstr_array(nullptr, nullptr, 0, "missing_ring");
  generator->release_bounded_ring_handle_array(
    nullptr, nullptr, 0, StyioValueFamily::ListHandle, "missing_ring");
  auto* i64_array_type = llvm::ArrayType::get(builder.getInt64Ty(), 1);
  auto* i64_array = builder.CreateAlloca(i64_array_type);
  auto* ptr_array_type = llvm::ArrayType::get(llvm::PointerType::get(context, 0), 1);
  auto* ptr_array = builder.CreateAlloca(ptr_array_type);
  generator->release_bounded_ring_cstr_array(i64_array_type, i64_array, 1, "i64_ring");
  generator->release_bounded_ring_handle_array(
    ptr_array_type, ptr_array, 1, StyioValueFamily::ListHandle, "ptr_ring");

  auto* same_i64 = builder.getInt64(7);
  EXPECT_EQ(generator->coerce_for_return(same_i64, same_i64->getType()), same_i64);
  EXPECT_EQ(generator->coerce_for_return(nullptr, builder.getInt64Ty()), nullptr);
  EXPECT_EQ(generator->coerce_for_return(same_i64, nullptr), same_i64);
  EXPECT_TRUE(generator
                ->coerce_for_return(
                  llvm::ConstantFP::get(builder.getDoubleTy(), 1.0),
                  builder.getInt1Ty())
                ->getType()
                ->isIntegerTy(1));

  EXPECT_EQ(generator->cstr_to_i64_checked(nullptr), nullptr);
  EXPECT_EQ(generator->cstr_to_i64_checked(same_i64), same_i64);
  EXPECT_EQ(generator->cstr_to_f64_checked(nullptr), nullptr);
  EXPECT_EQ(generator->cstr_to_f64_checked(same_i64), same_i64);
  generator->free_resource_if_runtime_owned(
    llvm::ConstantInt::get(builder.getInt32Ty(), 1),
    StyioToLLVM::TempResourceKind::List);
}

TEST(StyioCodeGenInternal, PulseHelpersCoverMissingPlanCommitAndRegionEdges) {
  {
    auto generator = make_generator();
    generator->emit_pulse_commit_all(nullptr, nullptr);
  }

  auto plan = std::make_unique<SGPulsePlan>();
  plan->slots.push_back(SGStateSlotDesc{SGStateSlotKind::Acc, 0, 0, 8, 0, "", "missing"});
  plan->slots.push_back(SGStateSlotDesc{
    static_cast<SGStateSlotKind>(255),
    1,
    8,
    8,
    0,
    "",
    "invalid_slot"});
  plan->commits = {
    {0, "missing"},
    {1, "invalid_slot"},
  };
  plan->total_bytes = 16;

  auto* pulse_loop = SGForEach::Create(
    SCListLiteral::Create({SGConstInt::Create(1)}),
    "tick",
    "i64",
    SGBlock::Create({
      SGFlexBind::Create(var("invalid_slot", i64_type()), SGConstInt::Create(7)),
    }));
  pulse_loop->set_pulse_plan(std::move(plan));

  expect_codegen_ok({
    SGFlexBind::Create(var("avg_without_pulse", i64_type()), SGSeriesAvgStep::Create(0, SGConstInt::Create(1))),
    SGFlexBind::Create(var("max_without_pulse", i64_type()), SGSeriesMaxStep::Create(0, SGConstInt::Create(1))),
    pulse_loop,
    SGFlexBind::Create(var("missing_region_hist", i64_type()), SGStateHistLoad::Create(0, 1, 404)),
  }, {
    "avg_without_pulse",
    "max_without_pulse",
    "missing_region_hist",
  });
}

TEST(StyioCodeGenInternal, ScalarCastConditionAndDynamicSlotGuardsStayExplicit) {
  expect_codegen_ok({
    SGCast::Create(
      SGConstInt::Create(7),
      SGType::Create(i64_type()),
      SGType::Create(i32_type())),
    SGCast::Create(
      SGConstBool::Create(true),
      SGType::Create(bool_type()),
      SGType::Create(i64_type())),
    SGCast::Create(
      SGConstInt::Create(65),
      SGType::Create(i64_type()),
      SGType::Create(char_type())),
  }, {});

  expect_codegen_throws({
    SGCast::Create(
      SGNoOp::Create(),
      SGType::Create(i64_type()),
      SGType::Create(i64_type())),
  }, "cast lowering produced an invalid value or target type");

  expect_codegen_throws({
    SGBinOp::Create(
      SGConstString::Create("left"),
      SGConstString::Create("right"),
      StyioOpType::Binary_Sub,
      SGType::Create(string_type())),
  }, "unsupported binary operand types in codegen");

  expect_codegen_throws({
    SGCond::Create(SGConstInt::Create(1), SGConstInt::Create(0), StyioOpType::Binary_Add),
  }, "unsupported logical condition operator in codegen");

  expect_codegen_ok({
    SGBinOp::Create(
      SGConstInt::Create(2),
      SGConstFloat::Create("3.5"),
      StyioOpType::Binary_Add,
      SGType::Create(i64_type())),
    SGBinOp::Create(
      SGConstInt::Create(2),
      SGConstFloat::Create("3.5"),
      StyioOpType::Binary_Mul,
      SGType::Create(i64_type())),
  }, {});

  expect_codegen_throws({
    SGFinalBind::Create(dynamic_var("dyn_bad_i64", i64_type()), SGConstString::Create("not-int")),
  }, "dynamic slot integer field received a non-integer value");

  expect_codegen_throws({
    SGFinalBind::Create(dynamic_var("dyn_bad_f64", f64_type()), SGConstString::Create("not-float")),
  }, "dynamic slot floating field received a non-numeric value");

  expect_codegen_throws({
    SGFinalBind::Create(dynamic_var("dyn_bad_string", string_type()), SGConstInt::Create(7)),
  }, "dynamic slot pointer field received a non-pointer value");
}

TEST(StyioCodeGenInternal, CollectionHandleLiteralsAndAccessorsStayExplicit) {
  expect_codegen_ok({
    SCListLiteral::Create({SGConstBool::Create(true), SGConstInt::Create(0)}, "bool"),
    SCListLiteral::Create({SGConstChar::Create('n')}, "i64"),
    SCListLiteral::Create({SGConstInt::Create(65), SGConstFloat::Create("2.5")}, "char"),
    SCListLiteral::Create({SGConstInt::Create(1), SGConstString::Create("not-number")}, "f64"),
    SCListLiteral::Create({SGConstString::Create("ok"), SGConstInt::Create(7)}, "string"),
    SCListLiteral::Create({list_i64(), list_i64()}, "list[i64]"),
    SCListLiteral::Create({dict_i64()}, "dict[string,i64]"),
    SCListLiteral::Create({matrix_i64()}, "matrix[i64,1,2]"),

    SCMatrixLiteral::Create({SGConstBool::Create(true), SGConstInt::Create(2)}, "i64", 1, 2),
    SCMatrixLiteral::Create({SGConstInt::Create(1), SGConstString::Create("bad")}, "f64", 1, 2),

    SCDictLiteral::Create({{SGConstString::Create("flag"), SGConstBool::Create(true)}}, "bool"),
    SCDictLiteral::Create({{SGConstString::Create("narrow"), SGConstChar::Create('n')}}, "i64"),
    SCDictLiteral::Create({{SGConstString::Create("ratio"), SGConstInt::Create(2)}}, "f64"),
    SCDictLiteral::Create({{SGConstString::Create("fallback_ratio"), SGConstString::Create("bad")}}, "f64"),
    SCDictLiteral::Create({{SGConstString::Create("text"), SGConstInt::Create(9)}}, "string"),
    SCDictLiteral::Create({{SGConstString::Create("list"), list_i64()}}, "list[i64]"),
    SCDictLiteral::Create({{SGConstString::Create("dict"), dict_i64()}}, "dict[string,i64]"),

    SCListClone::Create(SGConstBool::Create(true)),
    SCListLen::Create(SGConstBool::Create(true)),
    SCListGet::Create(SGConstBool::Create(true), SGConstBool::Create(true), "string"),
    SCListGet::Create(SGConstBool::Create(true), SGConstBool::Create(false), "bool"),
    SCListGet::Create(SGConstBool::Create(true), SGConstInt::Create(0), "list[i64]"),
    SCListGet::Create(SGConstBool::Create(true), SGConstInt::Create(0), "dict[string,i64]"),
    SCListGet::Create(SGConstBool::Create(true), SGConstInt::Create(0), "matrix[i64,1,2]"),
    SCListSlice::Create(SGConstBool::Create(true), SGConstBool::Create(false), nullptr, "i64"),
    SCListSlice::Create(SGConstBool::Create(true), SGConstBool::Create(false), SGConstBool::Create(true), "i64"),
    SCListSet::Create(SGConstBool::Create(true), SGConstBool::Create(false), SGConstChar::Create('n'), "i64"),
    SCListSet::Create(SGConstBool::Create(true), SGConstBool::Create(false), SGConstInt::Create(1), "string"),
    SCListSet::Create(SGConstBool::Create(true), SGConstBool::Create(false), SGConstString::Create("bad"), "f64"),
    SCListSet::Create(SGConstBool::Create(true), SGConstBool::Create(false), SGConstString::Create("bad"), "char"),
    SCListSet::Create(SGConstBool::Create(true), SGConstBool::Create(false), list_i64(), "list[i64]"),
    SCListSet::Create(SGConstBool::Create(true), SGConstBool::Create(false), dict_i64(), "dict[string,i64]"),
    SCListSet::Create(SGConstBool::Create(true), SGConstBool::Create(false), matrix_i64(), "matrix[i64,1,2]"),
    SCListToString::Create(SGConstBool::Create(true)),

    SCMatrixClone::Create(SGConstBool::Create(true), "f64"),
    SCMatrixGet::Create(SGConstBool::Create(true), SGConstBool::Create(false), SGConstBool::Create(true), "f64"),
    SCMatrixRow::Create(SGConstBool::Create(true), SGConstBool::Create(false), "f64"),
    SCMatrixRowsSlice::Create(SGConstBool::Create(true), SGConstBool::Create(false), nullptr, "f64"),
    SCMatrixRowsSlice::Create(SGConstBool::Create(true), SGConstBool::Create(false), SGConstBool::Create(true), "i64"),
    SCMatrixToString::Create(SGConstBool::Create(true)),

    SCDictClone::Create(SGConstBool::Create(true)),
    SCDictLen::Create(SGConstBool::Create(true)),
    SCDictGet::Create(SGConstBool::Create(true), SGConstString::Create("flag"), "bool"),
    SCDictGet::Create(SGConstBool::Create(true), SGConstString::Create("text"), "string"),
    SCDictGet::Create(SGConstBool::Create(true), SGConstString::Create("ratio"), "f64"),
    SCDictGet::Create(SGConstBool::Create(true), SGConstString::Create("list"), "list[i64]"),
    SCDictGet::Create(SGConstBool::Create(true), SGConstString::Create("dict"), "dict[string,i64]"),
    SCDictSet::Create(SGConstBool::Create(true), SGConstString::Create("narrow"), SGConstChar::Create('n'), "i64"),
    SCDictSet::Create(SGConstBool::Create(true), SGConstString::Create("text"), SGConstInt::Create(1), "string"),
    SCDictSet::Create(SGConstBool::Create(true), SGConstString::Create("ratio"), SGConstString::Create("bad"), "f64"),
    SCDictSet::Create(SGConstBool::Create(true), SGConstString::Create("list"), list_i64(), "list[i64]"),
    SCDictSet::Create(SGConstBool::Create(true), SGConstString::Create("dict"), dict_i64(), "dict[string,i64]"),
    SCDictKeys::Create(SGConstBool::Create(true)),
    SCDictValues::Create(SGConstBool::Create(true), "bool"),
    SCDictValues::Create(SGConstBool::Create(true), "string"),
    SCDictValues::Create(SGConstBool::Create(true), "f64"),
    SCDictValues::Create(SGConstBool::Create(true), "list[i64]"),
    SCDictValues::Create(SGConstBool::Create(true), "dict[string,i64]"),
    SCDictToString::Create(SGConstBool::Create(true)),

    SIOListReadStdin::Create("string"),
    matrix_f64(),
  }, {
    "styio_list_new_list",
    "styio_list_get_matrix",
    "styio_list_set_matrix",
    "styio_matrix_clone_f64",
    "styio_dict_new_dict",
    "styio_dict_values_dict",
    "styio_list_cstr_read_stdin",
  });
}

TEST(StyioCodeGenInternal, DynamicLoadDefaultsAndFallbackValuesStayExplicit) {
  expect_codegen_ok({
    SGResId::Create("missing_name_falls_back_to_zero"),
    SGDynLoad::Create("missing_bool", SGDynLoadKind::Bool),
    SGDynLoad::Create("missing_i64", SGDynLoadKind::I64),
    SGDynLoad::Create("missing_f64", SGDynLoadKind::F64),
    SGDynLoad::Create("missing_cstr", SGDynLoadKind::CString),
    SGDynLoad::Create("missing_list", SGDynLoadKind::ListHandle),
    SGDynLoad::Create("missing_dict", SGDynLoadKind::DictHandle),
    SGDynLoad::Create("missing_matrix", SGDynLoadKind::MatrixHandle),
    SGDynLoad::Create("missing_task", SGDynLoadKind::TaskHandle),

    SGFunc::Create(
      SGType::Create(i64_type()),
      SGResId::Create("default_i64"),
      {},
      SGBlock::Create({})),
    SGFunc::Create(
      SGType::Create(bool_type()),
      SGResId::Create("default_bool"),
      {},
      SGBlock::Create({})),
    SGCall::Create(SGResId::Create("default_i64"), {}),
    SGCall::Create(SGResId::Create("default_bool"), {}),

    SGCall::Create(SGResId::Create("__styio_list_push_f64"), {
      SGConstInt::Create(1),
      SGConstString::Create("not-a-number"),
    }),
    SGCall::Create(SGResId::Create("__styio_list_push_i64"), {
      SGConstInt::Create(1),
      SGConstChar::Create('x'),
    }),
  }, {
    "default_i64",
    "default_bool",
    "styio_list_push_f64",
    "styio_list_push",
  });
}

TEST(StyioCodeGenInternal, MainReturnTruncationCoversFloatAndVoidValues) {
  expect_codegen_ok({
    SGConstFloat::Create("2.75"),
  }, {
    "ret i32 2",
  });

  expect_codegen_ok({
    SGExportDecl::Create({"native_void_tail"}),
    SGExternBlock::Create("c", "void native_void_tail(void) {}\n"),
    SGCall::Create(SGResId::Create("native_void_tail"), {}),
  }, {
    "native_void_tail",
    "ret i32 0",
  });
}

TEST(StyioCodeGenInternal, UserCallArgumentCastsAndMixedMatchPromotionStayExplicit) {
  expect_codegen_ok({
    SGFunc::Create(
      SGType::Create(i64_type()),
      SGResId::Create("take_i32"),
      {SGFuncArg::Create("v", SGType::Create(i32_type()))},
      SGBlock::Create({SGReturn::Create(SGResId::Create("v"))})),
    SGFunc::Create(
      SGType::Create(f32_type()),
      SGResId::Create("take_f32"),
      {SGFuncArg::Create("v", SGType::Create(f32_type()))},
      SGBlock::Create({SGReturn::Create(SGResId::Create("v"))})),
    SGFunc::Create(
      SGType::Create(f64_type()),
      SGResId::Create("take_f64"),
      {SGFuncArg::Create("v", SGType::Create(f64_type()))},
      SGBlock::Create({SGReturn::Create(SGResId::Create("v"))})),
    SGFunc::Create(
      SGType::Create(string_type()),
      SGResId::Create("take_string"),
      {SGFuncArg::Create("v", SGType::Create(string_type()))},
      SGBlock::Create({SGReturn::Create(SGResId::Create("v"))})),
    SGFunc::Create(
      SGType::Create(f64_type()),
      SGResId::Create("default_f64"),
      {},
      SGBlock::Create({})),
    SGFunc::Create(
      SGType::Create(string_type()),
      SGResId::Create("default_string"),
      {},
      SGBlock::Create({})),
    SGCall::Create(SGResId::Create("take_i32"), {SGConstString::Create("123")}),
    SGCall::Create(SGResId::Create("take_f32"), {SGConstFloat::Create("1.5")}),
    SGCall::Create(SGResId::Create("take_f64"), {SGConstFloat::Create("2.5")}),
    SGCall::Create(SGResId::Create("take_f64"), {
      SGCall::Create(SGResId::Create("take_f32"), {SGConstFloat::Create("3.5")}),
    }),
    SGCall::Create(SGResId::Create("take_string"), {SGConstChar::Create('x')}),
    SGCall::Create(SGResId::Create("default_f64"), {}),
    SGCall::Create(SGResId::Create("default_string"), {}),
    SGMatch::Create(
      SGConstInt::Create(1),
      {{1, SGBlock::Create({SGReturn::Create(SGConstChar::Create('c'))})}},
      SGBlock::Create({SGReturn::Create(SGConstBool::Create(false))}),
      SGMatchReprKind::ExprMixed),
  }, {
    "take_i32",
    "take_f32",
    "take_string",
    "styio_char_cstr",
  });
}

TEST(StyioCodeGenInternal, NumericOperatorsNativeExternsAndReturnCoercionsStayExplicit) {
  const std::string native_body =
    "#include <stdint.h>\n"
    "#include <stdbool.h>\n"
    "void native_void(void) {}\n"
    "_Bool native_bool(_Bool x) { return x; }\n"
    "int8_t native_i8(int8_t x) { return x; }\n"
    "int16_t native_i16(int16_t x) { return x; }\n"
    "int64_t native_i64(int64_t x) { return x; }\n"
    "float native_f32(float x) { return x; }\n"
    "double native_f64(double x) { return x; }\n"
    "const char* native_ptr(const char* x) { return x; }\n";

  expect_codegen_ok({
    SGExportDecl::Create({
      "native_void",
      "native_bool",
      "native_i8",
      "native_i16",
      "native_i64",
      "native_f32",
      "native_f64",
      "native_ptr",
    }),
    SGExternBlock::Create("c", native_body),

    SGFunc::Create(
      SGType::Create(f32_type()),
      SGResId::Create("return_f32"),
      {},
      SGBlock::Create({SGReturn::Create(SGConstFloat::Create("1.25"))})),
    SGFunc::Create(
      SGType::Create(bool_type()),
      SGResId::Create("return_bool"),
      {},
      SGBlock::Create({SGReturn::Create(SGConstFloat::Create("1.0"))})),
    SGFunc::Create(
      SGType::Create(bool_type()),
      SGResId::Create("return_bool_from_f32"),
      {},
      SGBlock::Create({SGReturn::Create(
        SGCall::Create(SGResId::Create("native_f32"), {SGConstFloat::Create("0.5")}))})),
    SGFunc::Create(
      SGType::Create(string_type()),
      SGResId::Create("return_string"),
      {},
      SGBlock::Create({SGReturn::Create(SGConstChar::Create('q'))})),

    SGFlexBind::Create(var("fmut", f64_type()), SGConstFloat::Create("1.0")),
    SGBinOp::Create(SGResId::Create("fmut"), SGConstInt::Create(2), StyioOpType::Self_Add_Assign, SGType::Create(f64_type())),
    SGBinOp::Create(SGResId::Create("fmut"), SGConstInt::Create(3), StyioOpType::Self_Sub_Assign, SGType::Create(f64_type())),
    SGBinOp::Create(SGResId::Create("fmut"), SGConstInt::Create(4), StyioOpType::Self_Mul_Assign, SGType::Create(f64_type())),
    SGBinOp::Create(SGResId::Create("fmut"), SGConstInt::Create(5), StyioOpType::Self_Div_Assign, SGType::Create(f64_type())),
    SGBinOp::Create(SGResId::Create("fmut"), SGConstInt::Create(6), StyioOpType::Self_Mod_Assign, SGType::Create(f64_type())),

    SGBinOp::Create(SGConstChar::Create('a'), SGConstChar::Create(2), StyioOpType::Binary_Add, SGType::Create(i64_type())),
    SGBinOp::Create(SGConstChar::Create('d'), SGConstChar::Create(1), StyioOpType::Binary_Sub, SGType::Create(i64_type())),
    SGBinOp::Create(SGConstChar::Create(3), SGConstChar::Create(4), StyioOpType::Binary_Mul, SGType::Create(i64_type())),
    SGBinOp::Create(SGConstChar::Create(8), SGConstChar::Create(2), StyioOpType::Binary_Div, SGType::Create(i64_type())),
    SGBinOp::Create(SGConstChar::Create(9), SGConstChar::Create(4), StyioOpType::Binary_Mod, SGType::Create(i64_type())),
    SGBinOp::Create(SGConstInt::Create(2), SGConstInt::Create(3), StyioOpType::Binary_Pow, SGType::Create(i64_type())),
    SGBinOp::Create(SGConstInt::Create(2), SGConstFloat::Create("3.5"), StyioOpType::Binary_Pow, SGType::Create(f64_type())),
	    SGBinOp::Create(SGConstString::Create("1.5"), SGConstInt::Create(2), StyioOpType::Binary_Add, SGType::Create(f64_type())),
	    SGBinOp::Create(SGConstInt::Create(10), SGConstFloat::Create("2.5"), StyioOpType::Binary_Add, SGType::Create(i64_type())),
	    SGBinOp::Create(SGConstInt::Create(10), SGConstFloat::Create("1.5"), StyioOpType::Binary_Sub, SGType::Create(f64_type())),
	    SGBinOp::Create(SGConstInt::Create(3), SGConstFloat::Create("2.5"), StyioOpType::Binary_Mul, SGType::Create(f64_type())),
	    SGBinOp::Create(SGConstInt::Create(3), SGConstFloat::Create("2.5"), StyioOpType::Binary_Mul, SGType::Create(i64_type())),
	    SGBinOp::Create(SGConstInt::Create(9), SGConstFloat::Create("2.0"), StyioOpType::Binary_Div, SGType::Create(f64_type())),
	    SGBinOp::Create(SGConstInt::Create(9), SGConstFloat::Create("2.0"), StyioOpType::Binary_Mod, SGType::Create(f64_type())),
    SGBinOp::Create(SGConstInt::Create(1), SGConstFloat::Create("1.0"), StyioOpType::Equal, SGType::Create(bool_type())),
    SGBinOp::Create(SGConstInt::Create(1), SGConstFloat::Create("2.0"), StyioOpType::Not_Equal, SGType::Create(bool_type())),
    SGBinOp::Create(SGConstInt::Create(3), SGConstFloat::Create("2.0"), StyioOpType::Greater_Than, SGType::Create(bool_type())),
    SGBinOp::Create(SGConstInt::Create(3), SGConstFloat::Create("3.0"), StyioOpType::Greater_Than_Equal, SGType::Create(bool_type())),
    SGBinOp::Create(SGConstInt::Create(2), SGConstFloat::Create("3.0"), StyioOpType::Less_Than, SGType::Create(bool_type())),
    SGBinOp::Create(SGConstInt::Create(2), SGConstFloat::Create("2.0"), StyioOpType::Less_Than_Equal, SGType::Create(bool_type())),
    SGBinOp::Create(SGConstChar::Create('x'), SGConstInt::Create(7), StyioOpType::Binary_Add, SGType::Create(string_type())),
    SGMatch::Create(
      SGConstInt::Create(1),
      {{1, SGBlock::Create({SGReturn::Create(
        SGCall::Create(SGResId::Create("native_f32"), {SGConstFloat::Create("0.25")}))})}},
      SGBlock::Create({SGReturn::Create(SGConstString::Create("fallback"))}),
      SGMatchReprKind::ExprMixed),
    SGMatch::Create(
      SGConstInt::Create(0),
      {},
      SGBlock::Create({SGReturn::Create(SGConstInt::Create(42))}),
      SGMatchReprKind::ExprInt),
    SGMatch::Create(
      SGConstInt::Create(1),
      {{1, SGBlock::Create({SGReturn::Create(
        SGBinOp::Create(
          SGConstString::Create("left"),
          SGConstString::Create("arm"),
          StyioOpType::Binary_Add,
          SGType::Create(string_type())))})}},
      SGBlock::Create({SGReturn::Create(
        SGBinOp::Create(
          SGConstString::Create("right"),
          SGConstString::Create("default"),
          StyioOpType::Binary_Add,
          SGType::Create(string_type())))}),
      SGMatchReprKind::ExprMixed),

    SGCall::Create(SGResId::Create("native_void"), {}),
    SGCall::Create(SGResId::Create("native_bool"), {SGConstBool::Create(true)}),
    SGCall::Create(SGResId::Create("native_i8"), {SGConstString::Create("12")}),
    SGCall::Create(SGResId::Create("native_i16"), {SGConstFloat::Create("12.5")}),
    SGCall::Create(SGResId::Create("native_i64"), {SGConstBool::Create(true)}),
    SGCall::Create(SGResId::Create("native_f32"), {SGConstFloat::Create("2.25")}),
    SGCall::Create(SGResId::Create("native_f64"), {SGConstString::Create("2.5")}),
    SGCall::Create(SGResId::Create("native_f64"), {SGConstInt::Create(7)}),
    SGCall::Create(SGResId::Create("native_f64"), {
      SGCall::Create(SGResId::Create("native_f32"), {SGConstFloat::Create("3.25")}),
    }),
    SGCall::Create(SGResId::Create("native_ptr"), {SGConstString::Create("ok")}),
    SGCall::Create(SGResId::Create("return_f32"), {}),
    SGCall::Create(SGResId::Create("return_bool"), {}),
    SGCall::Create(SGResId::Create("return_bool_from_f32"), {}),
    SGCall::Create(SGResId::Create("return_string"), {}),
    SGConstInt::Create(0),
  }, {
    "native_bool",
    "native_i8",
    "native_f64",
    "styio_cstr_to_i64",
    "styio_cstr_to_f64",
    "styio_char_cstr",
    "styio_empty",
  });

  expect_codegen_throws({
    SGFlexBind::Create(var("mutable_value", i64_type()), SGConstInt::Create(1)),
    SGFinalBind::Create(var("fixed_value", i64_type()), SGConstInt::Create(1)),
    SGFlexBind::Create(var("fixed_value", i64_type()), SGConstInt::Create(2)),
  }, "immutable binding cannot be reassigned");

  expect_codegen_throws({
    SGFinalBind::Create(var("fixed_again", i64_type()), SGConstInt::Create(1)),
    SGFinalBind::Create(var("fixed_again", i64_type()), SGConstInt::Create(2)),
  }, "immutable binding cannot be redefined");
}

TEST(StyioCodeGenInternal, DynamicSlotsRingsScopesAndControlFlowStayExplicit) {
  expect_codegen_ok({
    SGFinalBind::Create(dynamic_var("dyn_i64", i64_type()), SGConstInt::Create(1)),
    SGFinalBind::Create(dynamic_var("dyn_bool", bool_type()), SGConstBool::Create(true)),
    SGFinalBind::Create(dynamic_var("dyn_f64", f64_type()), SGConstFloat::Create("1.25")),
    SGFinalBind::Create(dynamic_var("dyn_text", string_type()), SGConstString::Create("first")),
    SGFlexBind::Create(dynamic_var("dyn_text", string_type()), SGConstString::Create("second")),
    SGFinalBind::Create(dynamic_var("dyn_opaque", opaque_defined_type()), SGConstInt::Create(5)),
    SGFinalBind::Create(dynamic_var("dyn_list", list_type()), list_i64()),
    SGFlexBind::Create(dynamic_var("dyn_list", list_type()), list_i64()),
    SGFinalBind::Create(dynamic_var("dyn_dict", dict_type()), dict_i64()),
    SGFinalBind::Create(dynamic_var("dyn_matrix", matrix_type()), matrix_i64()),
    SGFinalBind::Create(
      dynamic_var("dyn_task", task_type()),
      SIOTaskCreate::Create(SGBlock::Create({SGReturn::Create(SGConstInt::Create(7))}), i64_type())),
    SGDynLoad::Create("dyn_bool", SGDynLoadKind::Bool),
    SGDynLoad::Create("dyn_i64", SGDynLoadKind::I64),
    SGDynLoad::Create("dyn_f64", SGDynLoadKind::F64),
    SGDynLoad::Create("dyn_text", SGDynLoadKind::CString),
    SGDynLoad::Create("dyn_list", SGDynLoadKind::ListHandle),
    SGDynLoad::Create("dyn_dict", SGDynLoadKind::DictHandle),
    SGDynLoad::Create("dyn_matrix", SGDynLoadKind::MatrixHandle),
    SGDynLoad::Create("dyn_task", SGDynLoadKind::TaskHandle),

    SGFinalBind::Create(var("recent_text", bounded_ring_type("string", 2)), SGConstString::Create("a")),
    SGFlexBind::Create(var("recent_text", bounded_ring_type("string", 2)), SGConstString::Create("b"), true),
    SGFinalBind::Create(var("recent_i64", bounded_ring_type("i64", 2)), SGConstInt::Create(1)),
    SGFlexBind::Create(var("recent_i64", bounded_ring_type("i64", 2)), SGConstInt::Create(2), true),
    SGFlexBind::Create(var("recent_i64", bounded_ring_type("i64", 2)), SGConstInt::Create(3)),
    SGFinalBind::Create(var("recent_lists", bounded_ring_type("list[i64]", 2)), list_i64()),
    SGFlexBind::Create(var("recent_lists", bounded_ring_type("list[i64]", 2)), list_i64(), true),

    SGFallback::Create(SGUndef::Create(), SGConstString::Create("fallback")),
    SGWaveMerge::Create(SGConstInt::Create(1), SGConstString::Create("left"), SGConstString::Create("right")),
    SGGuardSelect::Create(SGConstInt::Create(42), SGConstInt::Create(1)),
    SIOFileLineIter::CreateFromPath(
      SGConstString::Create("/tmp/styio-codegen-internal.txt"),
      "line",
      SGBlock::Create({SIOStdStreamWrite::Create(
        SIOStdStreamWrite::Stream::Stderr,
        {SGResId::Create("line")})})),
    SGRangeFor::Create(
      SGConstInt::Create(0),
      SGConstInt::Create(2),
      SGConstInt::Create(1),
      "i",
      SGBlock::Create({
        SGFinalBind::Create(dynamic_var("loop_dyn_text", string_type()), SGConstString::Create("inner")),
        SGFinalBind::Create(dynamic_var("loop_dyn_list", list_type()), list_i64()),
        SGFinalBind::Create(var("loop_recent_text", bounded_ring_type("string", 2)), SGConstString::Create("a")),
        SGIf::Create(
          SGConstBool::Create(true),
          SGBlock::Create({SGContinue::Create()}),
          SGBlock::Create({SGBreak::Create()})),
      })),
    SGRangeFor::Create(
      SGConstBool::Create(false),
      SGConstChar::Create(2),
      SGConstBool::Create(true),
      "wide_i",
      SGBlock::Create({SGNoOp::Create()})),
  }, {
    "styio_list_release",
    "styio_dict_release",
    "styio_matrix_release",
    "styio_task_release",
    "styio_free_cstr",
    "resource_commit_hdr",
    "fline_hdr",
    "rangefor_hdr",
	    "styio_file_read_line",
	  });

	  expect_codegen_throws({
	    SGFinalBind::Create(var("bad_ring", bounded_ring_type("string", 2)), SGConstInt::Create(1)),
	  }, "bounded resource ring value type mismatch");

	  expect_codegen_throws({
	    SIOStreamZip::Create(
	      SGConstInt::Create(1),
	      false,
	      false,
	      "a",
	      SGConstInt::Create(2),
	      false,
	      false,
	      "b",
	      false,
	      false,
	      "i64",
	      "i64",
	      SGBlock::Create({SGNoOp::Create()})),
	  }, "unsupported stream zip lowering");
	}

TEST(StyioCodeGenInternal, TaskFlowIoAndScopedStringEdgesStayExplicit) {
  expect_codegen_ok({
    SIOPath::Create("fixture.txt"),
    SIORead::Create(SIOPath::Create("fixture.txt")),
    SIOPrint::Create({
      SGConstBool::Create(true),
      SGConstChar::Create('z'),
      SGVar::Create(SGResId::Create("passive_i32"), SGType::Create(i32_type())),
      SGConstInt::Create(99),
      SGConstFloat::Create("4.25"),
      SGConstString::Create("done"),
    }),

    SGBinOp::Create(
      matrix_f64(),
      matrix_i64(),
      StyioOpType::Binary_Add,
      SGType::Create(matrix_type("i64")),
      matrix_type("f64"),
      matrix_type("i64")),
    SGBinOp::Create(
      matrix_i64(),
      matrix_i64(),
      StyioOpType::Binary_Div,
      SGType::Create(matrix_type("i64")),
      matrix_type("i64"),
      matrix_type("i64")),
    SGBinOp::Create(
      SGConstInt::Create(5),
      SGConstString::Create("2.5"),
      StyioOpType::Binary_Add,
      SGType::Create(i64_type())),
    SGBinOp::Create(
      SGConstInt::Create(5),
      SGConstString::Create("2.5"),
      StyioOpType::Binary_Mul,
      SGType::Create(i64_type())),

    SGFinalBind::Create(var("owned_text", string_type()), SGBinOp::Create(
      SGConstString::Create("left"),
      SGConstInt::Create(7),
      StyioOpType::Binary_Add,
      SGType::Create(string_type()))),
    SGGuardSelect::Create(
      SGBinOp::Create(
        SGConstString::Create("guard"),
        SGConstInt::Create(1),
        StyioOpType::Binary_Add,
        SGType::Create(string_type())),
      SGConstInt::Create(1)),
    SGWaveMerge::Create(
      SGConstInt::Create(1),
      SGBinOp::Create(
        SGConstString::Create("wave"),
        SGConstInt::Create(2),
        StyioOpType::Binary_Add,
        SGType::Create(string_type())),
      SGConstString::Create("fallback")),
    SGIf::Create(
      SGConstFloat::Create("1.0"),
      SGBlock::Create({SGConstInt::Create(1)}),
      SGBlock::Create({SGConstInt::Create(0)})),
    SGBlock::Create({
      SGFinalBind::Create(var("scoped_tail_text", string_type()), SGConstString::Create("held")),
      SGConstString::Create("tail"),
    }),

    SGFinalBind::Create(var("recent_f64", bounded_ring_type("f64", 2)), SGConstInt::Create(1)),
    SGFinalBind::Create(var("recent_i64_from_float", bounded_ring_type("i64", 2)), SGConstFloat::Create("2.5")),

    SGFlexBind::Create(var("outer_mut", i64_type()), SGConstInt::Create(10)),
    SGFinalBind::Create(var("outer_const", string_type()), SGConstString::Create("cap")),
    SGFinalBind::Create(dynamic_var("outer_dyn", i64_type()), SGConstInt::Create(3)),
    SIOTaskCreate::Create(SGBlock::Create({
      SGReturn::Create(SGBinOp::Create(
        SGResId::Create("outer_mut"),
        SGConstInt::Create(1),
        StyioOpType::Binary_Add,
        SGType::Create(i64_type()))),
      SGVar::Create(
        SGResId::Create("free_var_init"),
        SGType::Create(i64_type()),
        SGResId::Create("outer_mut")),
      SGVar::Create(SGResId::Create("outer_const"), SGType::Create(string_type())),
      SGDynLoad::Create("outer_dyn", SGDynLoadKind::I64),
      SGCond::Create(SGResId::Create("outer_mut"), SGConstInt::Create(0), StyioOpType::Greater_Than),
      SGFlexBind::Create(var("task_local", i64_type()), SGResId::Create("outer_mut")),
      SGIf::Create(
        SGConstBool::Create(true),
        SGBlock::Create({SIOStdStreamWrite::Create(
          SIOStdStreamWrite::Stream::Stdout,
          {SGResId::Create("outer_const")})}),
        SGBlock::Create({SIOPrint::Create({SGResId::Create("outer_mut")})})),
      SIOFlowBind::Create(
        SGResId::Create("outer_mut"),
        "task_flow",
        i64_type(),
        false,
        SGResId::Create("outer_const"),
        false),
    }), i64_type()),
    SIOTaskCreate::Create(SGBlock::Create({SGReturn::Create(SGConstInt::Create(4))}), f64_type()),
    SIOTaskCreate::Create(SGBlock::Create({SGReturn::Create(SGConstBool::Create(true))}), i64_type()),
    SIOTaskCreate::Create(SGBlock::Create({SGReturn::Create(SGConstFloat::Create("8.5"))}), i64_type()),
    SIOTaskCreate::Create(SGBlock::Create({SGReturn::Create(SGConstString::Create("11"))}), i64_type()),
    SIOTaskCreate::Create(SGBlock::Create({SGReturn::Create(SGConstChar::Create('q'))}), i64_type()),
    SIOTaskCreate::Create(SGBlock::Create({SGReturn::Create(SGConstInt::Create(12))}), string_type()),

    SGFinalBind::Create(dynamic_var("flow_dyn", string_type()), SGConstString::Create("before")),
    SIOFlowBind::Create(SGConstString::Create("after"), "flow_dyn", string_type(), false),
    SIOFlowBind::Create(SGConstInt::Create(1), "await_bool", bool_type(), true, SGConstInt::Create(0), true),
    SIOFlowBind::Create(SGConstInt::Create(1), "await_f64", f64_type(), true, SGConstBool::Create(true), true),
    SIOFlowBind::Create(SGConstInt::Create(1), "await_string", string_type(), true, SGConstInt::Create(7), true),
    SIOFlowBind::Create(SGConstInt::Create(1), "flow_i64", i64_type(), false),
    SIOFlowBind::Create(SGConstBool::Create(true), "flow_i64", i64_type(), false),
    SIOFlowBind::Create(SGConstInt::Create(2), "flow_f64", f64_type(), false),
    SIOFlowBind::Create(SGConstFloat::Create("2.5"), "flow_i32", i32_type(), false),
    SIOFlowBind::Create(SGConstInt::Create(3), "flow_i32_from_i64", i32_type(), false),

    SGRangeFor::Create(
      SGConstInt::Create(0),
      SGConstInt::Create(1),
      SGConstInt::Create(1),
      "j",
      SGBlock::Create({
        SGFinalBind::Create(var("loop_owned_text", string_type()), SGBinOp::Create(
          SGConstString::Create("loop"),
          SGResId::Create("j"),
          StyioOpType::Binary_Add,
          SGType::Create(string_type()))),
        SGFinalBind::Create(dynamic_var("loop_dyn_dict", dict_type()), dict_i64()),
        SGIf::Create(
          SGConstBool::Create(true),
          SGBlock::Create({SGContinue::Create()}),
          SGBlock::Create({SGBreak::Create()})),
      })),
  }, {
    "styio_stdout_write_cstr",
    "styio_task_i64_spawn",
    "styio_task_f64_spawn",
    "styio_task_cstr_spawn",
    "styio_task_f64_pull",
    "styio_task_cstr_pull",
    "await_merge",
    "styio_matrix_add_i64",
    "styio_matrix_matmul_i64",
    "styio_free_cstr",
  });
}

TEST(StyioCodeGenInternal, TaskCaptureScannerCoversNestedReturnExpressions) {
  expect_codegen_ok({
    SGFlexBind::Create(var("outer_task_value", i64_type()), SGConstInt::Create(2)),
    SIOTaskCreate::Create(
      SGBlock::Create({
        SGReturn::Create(SGBlock::Create({
          SGFlexBind::Create(var("inner_task_value", i64_type()), SGResId::Create("outer_task_value")),
          SGFinalBind::Create(var("inner_task_const", i64_type()), SGResId::Create("inner_task_value")),
          SGResId::Create("inner_task_const"),
        })),
      }),
      i64_type()),
    SIOTaskCreate::Create(
      SGBlock::Create({
        SGReturn::Create(SGFlexBind::Create(
          var("bind_expr_task_value", i64_type()),
          SGConstInt::Create(4))),
      }),
      i64_type()),
    SIOTaskCreate::Create(
      SGBlock::Create({
        SGReturn::Create(SGFinalBind::Create(
          var("final_expr_task_value", i64_type()),
          SGConstInt::Create(5))),
      }),
      i64_type()),
  }, {
    "outer_task_value.capture",
    "styio_task_i64_spawn",
  });
}

TEST(StyioCodeGenInternal, TaskCaptureScannerSkipsUnsupportedCaptureSlots) {
  expect_codegen_ok({
    SGFinalBind::Create(var("wide_slot", bounded_ring_type("i64", 2)), SGConstInt::Create(1)),
    SIOTaskCreate::Create(
      SGBlock::Create({
        SGReturn::Create(SGConstInt::Create(1)),
        SGResId::Create("wide_slot"),
      }),
      i64_type()),
  }, {
    "styio_task_i64_spawn",
  });
}

TEST(StyioCodeGenInternal, ResourceEffectZipNativeAndDriverEdgesStayExplicit) {
  expect_codegen_ok({
    SGFinalBind::Create(var("recent_i64_from_bool", bounded_ring_type("i64", 2)), SGConstBool::Create(true)),
    SGBinOp::Create(
      matrix_i64(),
      SGConstBool::Create(true),
      StyioOpType::Binary_Mul,
      SGType::Create(matrix_type("i64")),
      matrix_type("i64"),
      bool_type()),
    SGMatch::Create(
      SGConstInt::Create(1),
      {{1, SGBlock::Create({SGReturn::Create(SGConstBool::Create(true))})}},
      SGBlock::Create({SGReturn::Create(SGConstInt::Create(0))}),
      SGMatchReprKind::ExprFloat),
    SGForEach::Create(
      SGConstBool::Create(true),
      "defensive_item",
      "i64",
      SGBlock::Create({SGNoOp::Create()})),
    SIOStreamZip::Create(
      SGConstString::Create("stdin"),
      false,
      true,
      "zip_line",
      SCListLiteral::Create({SGConstBool::Create(true)}, "bool"),
      false,
      false,
      "zip_flag",
      false,
      false,
      "i64",
      "bool",
      SGBlock::Create({SGNoOp::Create()})),
    SIOStreamZip::Create(
      SCListLiteral::Create({SGConstInt::Create(1)}, "i64"),
      false,
      false,
      "zip_static_a",
      SCListLiteral::Create({SGConstInt::Create(2)}, "i64"),
      false,
      false,
      "zip_static_b",
      true,
      true,
      "i64",
      "i64",
      SGBlock::Create({SGNoOp::Create()})),
    SIOStreamZip::Create(
      SCListLiteral::Create({SGConstInt::Create(3)}, "i64"),
      false,
      false,
      "zip_list_file_a",
      SGConstString::Create("right.txt"),
      true,
      false,
      "zip_list_file_b",
      true,
      true,
      "i64",
      "string",
      SGBlock::Create({SGNoOp::Create()})),
    SIOStreamZip::Create(
      SGConstString::Create("left.txt"),
      true,
      false,
      "zip_file_list_a",
      SCListLiteral::Create({SGConstInt::Create(4)}, "i64"),
      false,
      false,
      "zip_file_list_b",
      true,
      true,
      "string",
      "i64",
      SGBlock::Create({SGNoOp::Create()})),
    SIOStreamZip::Create(
      SGConstString::Create("left-num.txt"),
      true,
      false,
      "zip_file_num_a",
      SCListLiteral::Create({SGConstInt::Create(5)}, "i64"),
      false,
      false,
      "zip_file_num_b",
      false,
      false,
      "i64",
      "i64",
      SGBlock::Create({SGNoOp::Create()})),
    SIOStreamZip::Create(
      SCListLiteral::Create({list_i64()}, "list[i64]"),
      false,
      false,
      "left",
      SCListLiteral::Create({dict_i64()}, "dict[string,i64]"),
      false,
      false,
      "right",
      false,
      false,
      "list[i64]",
      "dict[string,i64]",
      SGBlock::Create({SIOPrint::Create({SGResId::Create("left"), SGResId::Create("right")})})),
  }, {
    "styio_matrix_scale_i64",
    "styio_list_release",
    "styio_dict_release",
  });

  auto generator = make_generator();
  testing::internal::CaptureStderr();
  EXPECT_NO_THROW(generator->execute());
  const std::string stderr_text = testing::internal::GetCapturedStderr();
  EXPECT_NE(stderr_text.find("main not found"), std::string::npos);
}
