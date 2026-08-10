// [C++ STL]
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// [Styio]
#include "../StyioException/Exception.hpp"
#include "../StyioIR/GenIR/GenIR.hpp"
#include "../StyioIR/Verifier.hpp"
#include "../StyioToken/Token.hpp"
#include "../StyioUtil/BoundedType.hpp"
#include "../StyioUtil/DynamicValue.hpp"
#include "../StyioUtil/IOIntrinsics.hpp"
#include "../StyioUtil/Util.hpp"
#include "CodeGenVisitor.hpp"

// [LLVM]
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/LinkAllIR.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/Cloning.h"

namespace {
constexpr llvm::StringLiteral
  kCallableSpecializationDigestAttribute =
    "styio.callable.specialization.digest";
constexpr llvm::StringLiteral
  kCallableSpecializationCacheMetadata =
    "styio.callable.specialization.cache";

struct StyioCallableSpecializationModule
{
  std::string content_digest;
  std::string symbol;
  std::unique_ptr<llvm::Module> module;
};

bool
styio_is_lower_sha256(llvm::StringRef value) {
  return value.size() == 64
    && llvm::all_of(
      value,
      [](char ch)
      {
        return (ch >= '0' && ch <= '9')
          || (ch >= 'a' && ch <= 'f');
      });
}

std::vector<StyioCallableSpecializationModule>
styio_partition_callable_specializations(
  llvm::Module& source
) {
  struct Candidate
  {
    llvm::Function* function = nullptr;
    std::string content_digest;
    std::string symbol;
  };

  std::vector<Candidate> candidates;
  for (llvm::Function& function : source) {
    if (!function.hasFnAttribute(
          kCallableSpecializationDigestAttribute)) {
      continue;
    }
    const llvm::StringRef digest =
      function.getFnAttribute(
        kCallableSpecializationDigestAttribute)
        .getValueAsString();
    if (function.isDeclaration()
        || !styio_is_lower_sha256(digest)
        || !function.getName().ends_with(digest)) {
      throw StyioTypeError(
        "LLVM callable specialization cache identity is invalid");
    }
    candidates.push_back(Candidate{
      &function,
      digest.str(),
      function.getName().str(),
    });
  }
  std::sort(
    candidates.begin(),
    candidates.end(),
    [](const Candidate& lhs, const Candidate& rhs)
    {
      return lhs.content_digest < rhs.content_digest;
    });
  for (std::size_t index = 1;
       index < candidates.size();
       ++index) {
    if (candidates[index - 1].content_digest
        == candidates[index].content_digest) {
      throw StyioTypeError(
        "LLVM callable specialization cache digest is not unique");
    }
  }

  for (llvm::GlobalVariable& global : source.globals()) {
    if (global.getName().starts_with("__styio_capture.")) {
      global.setLinkage(llvm::GlobalValue::ExternalLinkage);
    }
  }

  std::vector<StyioCallableSpecializationModule> partitions;
  partitions.reserve(candidates.size());
  for (const Candidate& candidate : candidates) {
    llvm::ValueToValueMapTy mapping;
    std::unique_ptr<llvm::Module> partition =
      llvm::CloneModule(
        source,
        mapping,
        [&](const llvm::GlobalValue* value)
        {
          return value == candidate.function
            || value->hasLocalLinkage();
        });
    partition->setModuleIdentifier(candidate.content_digest);
    auto* metadata = partition->getOrInsertNamedMetadata(
      kCallableSpecializationCacheMetadata);
    metadata->addOperand(
      llvm::MDNode::get(
        partition->getContext(),
        {
          llvm::MDString::get(
            partition->getContext(),
            candidate.content_digest),
          llvm::MDString::get(
            partition->getContext(),
            candidate.symbol),
        }));

    llvm::ModuleAnalysisManager analyses;
    (void)llvm::GlobalDCEPass().run(
      *partition,
      analyses);

    llvm::Function* cloned =
      partition->getFunction(candidate.symbol);
    std::string verifier_error;
    llvm::raw_string_ostream verifier_stream(verifier_error);
    if (cloned == nullptr
        || cloned->isDeclaration()
        || llvm::verifyModule(*partition, &verifier_stream)) {
      verifier_stream.flush();
      throw StyioTypeError(
        "LLVM callable specialization partition verification failed: "
        + verifier_error);
    }
    partitions.push_back(StyioCallableSpecializationModule{
      candidate.content_digest,
      candidate.symbol,
      std::move(partition),
    });
  }

  for (const Candidate& candidate : candidates) {
    candidate.function->deleteBody();
  }
  std::string verifier_error;
  llvm::raw_string_ostream verifier_stream(verifier_error);
  if (llvm::verifyModule(source, &verifier_stream)) {
    verifier_stream.flush();
    throw StyioTypeError(
      "LLVM main-module verification failed after callable "
      "specialization partitioning: " + verifier_error);
  }
  return partitions;
}

int64_t
styio_undef_i64() {
  return std::numeric_limits<int64_t>::min();
}

llvm::StructType*
styio_dynamic_cell_type(llvm::LLVMContext& ctx) {
  if (auto* existing = llvm::StructType::getTypeByName(ctx, "styio.dyncell")) {
    return existing;
  }
  auto* cell = llvm::StructType::create(ctx, "styio.dyncell");
  cell->setBody({
    llvm::Type::getInt64Ty(ctx),
    llvm::Type::getInt64Ty(ctx),
    llvm::Type::getDoubleTy(ctx),
    llvm::PointerType::get(ctx, 0),
  });
  return cell;
}

llvm::Type*
styio_bounded_ring_element_llvm_type(const StyioDataType& dt, llvm::IRBuilder<>* builder) {
  auto type_name = styio_bounded_ring_value_type_name(dt);
  if (type_name && *type_name == "f64") {
    return builder->getDoubleTy();
  }
  if (type_name && *type_name == "bool") {
    return builder->getInt1Ty();
  }
  if (type_name && *type_name == "char") {
    return builder->getInt8Ty();
  }
  if (type_name && *type_name == "string") {
    return llvm::PointerType::get(builder->getContext(), 0);
  }
  return builder->getInt64Ty();
}

llvm::Constant*
styio_zero_for_llvm_type(llvm::Type* ty, llvm::IRBuilder<>* builder) {
  if (ty->isDoubleTy()) {
    return llvm::ConstantFP::get(ty, 0.0);
  }
  if (ty->isIntegerTy()) {
    return llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(ty), 0);
  }
  return llvm::Constant::getNullValue(ty);
}

llvm::Value*
styio_coerce_bounded_ring_value(llvm::Value* value, llvm::Type* elem_ty, llvm::IRBuilder<>* builder) {
  if (value == nullptr || elem_ty == nullptr || value->getType() == elem_ty) {
    return value;
  }
  llvm::Type* value_ty = value->getType();
  if (elem_ty->isDoubleTy() && value_ty->isIntegerTy()) {
    return builder->CreateSIToFP(value, elem_ty);
  }
  if (elem_ty->isIntegerTy() && value_ty->isDoubleTy()) {
    return builder->CreateFPToSI(value, elem_ty);
  }
  if (elem_ty->isIntegerTy() && value_ty->isIntegerTy()) {
    if (value_ty->isIntegerTy(1)) {
      return builder->CreateZExt(value, elem_ty);
    }
    return builder->CreateSExtOrTrunc(value, elem_ty);
  }
  if (elem_ty->isPointerTy() && value_ty->isPointerTy()) {
    return value;
  }
  throw StyioTypeError("bounded resource ring value type mismatch");
}

llvm::Value*
styio_coerce_collection_value(
  llvm::Value* value,
  StyioValueFamily family,
  llvm::IRBuilder<>* builder,
  const char* context
) {
  const std::string label = context != nullptr && context[0] != '\0'
    ? context
    : "collection";
  auto fail = [&label]() -> llvm::Value* {
    throw StyioTypeError(label + " value type mismatch");
  };

  if (value == nullptr) {
    return fail();
  }

  if (family == StyioValueFamily::String) {
    return value->getType()->isPointerTy() ? value : fail();
  }

  if (family == StyioValueFamily::Float) {
    if (value->getType()->isDoubleTy()) {
      return value;
    }
    if (value->getType()->isIntegerTy()) {
      return builder->CreateSIToFP(value, builder->getDoubleTy());
    }
    return fail();
  }

  if (family == StyioValueFamily::Char) {
    if (!value->getType()->isIntegerTy()) {
      return fail();
    }
    return value->getType()->isIntegerTy(8)
      ? value
      : builder->CreateSExtOrTrunc(value, builder->getInt8Ty());
  }

  if (family == StyioValueFamily::ListHandle
      || family == StyioValueFamily::DictHandle
      || family == StyioValueFamily::MatrixHandle
      || family == StyioValueFamily::RangeHandle
      || family == StyioValueFamily::FileHandle
      || family == StyioValueFamily::StreamHandle
      || family == StyioValueFamily::TaskHandle) {
    return value->getType()->isIntegerTy(64) ? value : fail();
  }

  if (family == StyioValueFamily::Bool || family == StyioValueFamily::Integer) {
    if (!value->getType()->isIntegerTy()) {
      return fail();
    }
    return value->getType()->isIntegerTy(1)
      ? builder->CreateZExt(value, builder->getInt64Ty())
      : (value->getType()->isIntegerTy(64)
          ? value
          : builder->CreateSExtOrTrunc(value, builder->getInt64Ty()));
  }

  return fail();
}

std::optional<StyioValueFamily>
bounded_ring_handle_family_for_type(const StyioDataType& dt) {
  auto type_name = styio_bounded_ring_value_type_name(dt);
  if (!type_name.has_value()) {
    return std::nullopt;
  }
  StyioValueFamily family = styio_value_family_from_type_name(*type_name);
  if (family == StyioValueFamily::ListHandle
      || family == StyioValueFamily::DictHandle
      || family == StyioValueFamily::MatrixHandle) {
    return family;
  }
  return std::nullopt;
}

bool
ir_yields_list_handle(StyioIR* value) {
  if (auto* block = dynamic_cast<SGBlock*>(value)) {
    return !block->stmts.empty() && ir_yields_list_handle(block->stmts.back());
  }
  if (dynamic_cast<SCListLiteral*>(value)
      || dynamic_cast<SIOListReadStdin*>(value)
      || dynamic_cast<SCListClone*>(value)
      || dynamic_cast<SCListSlice*>(value)
      || dynamic_cast<SCMatrixRowsSlice*>(value)
      || dynamic_cast<SCDictKeys*>(value)
      || dynamic_cast<SCDictValues*>(value)) {
    return true;
  }
  if (auto* pull = dynamic_cast<SIOStdStreamPull*>(value)) {
    return styio_is_list_type(pull->result_type);
  }
  if (auto* load = dynamic_cast<SGDynLoad*>(value)) {
    return load->kind == SGDynLoadKind::ListHandle;
  }
  if (auto* get = dynamic_cast<SCListGet*>(value)) {
    return styio_value_family_from_type_name(get->elem_type) == StyioValueFamily::ListHandle;
  }
  if (auto* get = dynamic_cast<SCDictGet*>(value)) {
    return styio_value_family_from_type_name(get->value_type) == StyioValueFamily::ListHandle;
  }
  if (auto* call = dynamic_cast<SGCall*>(value)) {
    return call->func_name != nullptr
      && (call->func_name->as_str() == "__styio_string_lines"
          || call->func_name->as_str() == "__styio_list_range_i64");
  }
  return false;
}

bool
stream_zip_static_literal_supported(const SCListLiteral* literal) {
  if (literal == nullptr) {
    return false;
  }
  StyioValueFamily family = styio_value_family_from_type_name(
    literal->elem_type.empty() ? std::string("i64") : literal->elem_type);
  if (family == StyioValueFamily::Integer) {
    return std::all_of(
      literal->elems.begin(),
      literal->elems.end(),
      [](StyioIR* elem) { return dynamic_cast<SGConstInt*>(elem) != nullptr; });
  }
  if (family == StyioValueFamily::String) {
    return std::all_of(
      literal->elems.begin(),
      literal->elems.end(),
      [](StyioIR* elem) { return dynamic_cast<SGConstString*>(elem) != nullptr; });
  }
  return false;
}

bool
ir_yields_dict_handle(StyioIR* value) {
  if (auto* block = dynamic_cast<SGBlock*>(value)) {
    return !block->stmts.empty() && ir_yields_dict_handle(block->stmts.back());
  }
  if (dynamic_cast<SCDictLiteral*>(value)
      || dynamic_cast<SCDictClone*>(value)) {
    return true;
  }
  if (auto* load = dynamic_cast<SGDynLoad*>(value)) {
    return load->kind == SGDynLoadKind::DictHandle;
  }
  if (auto* get = dynamic_cast<SCListGet*>(value)) {
    return styio_value_family_from_type_name(get->elem_type) == StyioValueFamily::DictHandle;
  }
  if (auto* get = dynamic_cast<SCDictGet*>(value)) {
    return styio_value_family_from_type_name(get->value_type) == StyioValueFamily::DictHandle;
  }
  return false;
}

bool
ir_yields_matrix_handle(StyioIR* value) {
  if (auto* block = dynamic_cast<SGBlock*>(value)) {
    return !block->stmts.empty() && ir_yields_matrix_handle(block->stmts.back());
  }
  if (dynamic_cast<SCMatrixLiteral*>(value)
      || dynamic_cast<SCMatrixClone*>(value)) {
    return true;
  }
  if (auto* load = dynamic_cast<SGDynLoad*>(value)) {
    return load->kind == SGDynLoadKind::MatrixHandle;
  }
  if (auto* bin = dynamic_cast<SGBinOp*>(value)) {
    return styio_is_matrix_type(bin->data_type->data_type);
  }
  if (auto* call = dynamic_cast<SGCall*>(value)) {
    std::string name = call->func_name != nullptr ? call->func_name->as_str() : "";
    return name.rfind("__styio_matrix_new_", 0) == 0
      || name.rfind("__styio_matrix_identity_", 0) == 0
      || name.rfind("__styio_matrix_clone_", 0) == 0
      || name.rfind("__styio_matrix_add_", 0) == 0
      || name.rfind("__styio_matrix_sub_", 0) == 0
      || name.rfind("__styio_matrix_hadamard_", 0) == 0
      || name.rfind("__styio_matrix_matmul_", 0) == 0
      || name.rfind("__styio_matrix_scale_", 0) == 0
      || name.rfind("__styio_matrix_transpose_", 0) == 0;
  }
  return false;
}

bool
ir_yields_task_handle(StyioIR* value) {
  if (dynamic_cast<SIOTaskCreate*>(value)) {
    return true;
  }
  if (auto* load = dynamic_cast<SGDynLoad*>(value)) {
    return load->kind == SGDynLoadKind::TaskHandle;
  }
  return false;
}

bool
dynamic_slot_declared_type_controls_payload(const StyioDataType& type) {
  if (type.isUndefined()) {
    return false;
  }
  if (styio_is_list_type(type) || styio_is_dict_type(type) || styio_is_matrix_type(type)) {
    return true;
  }
  switch (styio_value_family_for_type(type)) {
    case StyioValueFamily::TaskHandle:
    case StyioValueFamily::String:
    case StyioValueFamily::Float:
    case StyioValueFamily::Bool:
    case StyioValueFamily::Integer:
      return true;
    default:
      return false;
  }
}
}  // namespace

StyioToLLVM::DynamicSlotPayload
StyioToLLVM::dynamic_slot_payload_for_value(StyioIR* source, llvm::Value* value) {
  DynamicSlotPayload payload;
  payload.tag = styio_dynamic_tag_value(StyioDynamicTag::Undef);

  if (ir_yields_list_handle(source)) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::List);
    payload.i64v = value;
  }
  else if (ir_yields_dict_handle(source)) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::Dict);
    payload.i64v = value;
  }
  else if (ir_yields_matrix_handle(source)) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::Matrix);
    payload.i64v = value;
  }
  else if (ir_yields_task_handle(source)) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::Task);
    payload.i64v = value;
  }
  else if (value->getType()->isPointerTy()) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::CStr);
    payload.ptrv = value;
  }
  else if (value->getType()->isDoubleTy()) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::F64);
    payload.f64v = value;
  }
  else if (value->getType()->isIntegerTy(1)) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::Bool);
    payload.i64v = value;
  }
  else if (value->getType()->isIntegerTy()) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::I64);
    payload.i64v = value;
  }

  return payload;
}

StyioToLLVM::DynamicSlotPayload
StyioToLLVM::dynamic_slot_payload_for_type(const StyioDataType& type, llvm::Value* value) {
  DynamicSlotPayload payload;
  payload.tag = styio_dynamic_tag_value(StyioDynamicTag::Undef);

  if (styio_is_list_type(type)) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::List);
    payload.i64v = value;
  }
  else if (styio_is_dict_type(type)) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::Dict);
    payload.i64v = value;
  }
  else if (styio_is_matrix_type(type)) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::Matrix);
    payload.i64v = value;
  }
  else if (type.handle_family == StyioHandleFamily::Task) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::Task);
    payload.i64v = value;
  }
  else if (type.option == StyioDataTypeOption::String) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::CStr);
    payload.ptrv = value;
  }
  else if (type.option == StyioDataTypeOption::Float) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::F64);
    payload.f64v = value;
  }
  else if (type.option == StyioDataTypeOption::Bool) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::Bool);
    payload.i64v = value;
  }
  else if (type.option == StyioDataTypeOption::Integer) {
    payload.tag = styio_dynamic_tag_value(StyioDynamicTag::I64);
    payload.i64v = value;
  }

  return payload;
}

void
StyioToLLVM::forget_dynamic_slot_payload_ownership(llvm::Value* value, std::int64_t tag) {
  if (tag == styio_dynamic_tag_value(StyioDynamicTag::CStr)) {
    forget_owned_cstr_temp(value);
  }
  if (styio_dynamic_tag_is_owned_resource(tag)) {
    forget_owned_resource_temp(value);
  }
}

llvm::FunctionCallee
StyioToLLVM::free_cstr_fn() {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  return theModule->getOrInsertFunction(
    "styio_free_cstr",
    llvm::FunctionType::get(theBuilder->getVoidTy(), {char_ptr}, false));
}

llvm::FunctionCallee
StyioToLLVM::clone_cstr_fn() {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  return theModule->getOrInsertFunction(
    "styio_clone_cstr",
    llvm::FunctionType::get(char_ptr, {char_ptr}, false));
}

llvm::FunctionCallee
StyioToLLVM::list_release_fn() {
  return theModule->getOrInsertFunction(
    "styio_list_release",
    llvm::FunctionType::get(theBuilder->getVoidTy(), {theBuilder->getInt64Ty()}, false));
}

llvm::FunctionCallee
StyioToLLVM::dict_release_fn() {
  return theModule->getOrInsertFunction(
    "styio_dict_release",
    llvm::FunctionType::get(theBuilder->getVoidTy(), {theBuilder->getInt64Ty()}, false));
}

llvm::FunctionCallee
StyioToLLVM::matrix_release_fn() {
  return theModule->getOrInsertFunction(
    "styio_matrix_release",
    llvm::FunctionType::get(theBuilder->getVoidTy(), {theBuilder->getInt64Ty()}, false));
}

llvm::FunctionCallee
StyioToLLVM::task_release_fn() {
  return theModule->getOrInsertFunction(
    "styio_task_release",
    llvm::FunctionType::get(theBuilder->getVoidTy(), {theBuilder->getInt64Ty()}, false));
}

void
StyioToLLVM::track_owned_cstr_temp(llvm::Value* v) {
  if (v && v->getType()->isPointerTy()) {
    owned_cstr_temps_.insert(v);
  }
}

llvm::Value*
StyioToLLVM::clone_cstr_for_runtime_owner(llvm::Value* v) {
  if (!v || !v->getType()->isPointerTy()) {
    return v;
  }
  llvm::Value* out = theBuilder->CreateCall(clone_cstr_fn(), {v});
  free_owned_cstr_temp_if_tracked(v);
  return out;
}

bool
StyioToLLVM::take_owned_cstr_temp(llvm::Value* v) {
  if (!v) {
    return false;
  }
  auto it = owned_cstr_temps_.find(v);
  if (it == owned_cstr_temps_.end()) {
    return false;
  }
  owned_cstr_temps_.erase(it);
  return true;
}

void
StyioToLLVM::forget_owned_cstr_temp(llvm::Value* v) {
  if (v) {
    owned_cstr_temps_.erase(v);
  }
}

void
StyioToLLVM::free_cstr_if_runtime_owned(llvm::Value* v) {
  if (!v || !v->getType()->isPointerTy()) {
    return;
  }
  theBuilder->CreateCall(free_cstr_fn(), {v});
}

void
StyioToLLVM::free_owned_cstr_temp_if_tracked(llvm::Value* v) {
  if (!take_owned_cstr_temp(v)) {
    return;
  }
  free_cstr_if_runtime_owned(v);
}

void
StyioToLLVM::store_bounded_ring_value(
  llvm::ArrayType* array_type,
  llvm::AllocaInst* array,
  llvm::Value* index,
  llvm::Value* value,
  std::optional<StyioValueFamily> handle_family) {
  if (array_type == nullptr || array == nullptr || index == nullptr || value == nullptr) {
    return;
  }
  llvm::Type* elem_ty = array_type->getElementType();
  llvm::Value* zero = llvm::ConstantInt::get(theBuilder->getInt64Ty(), 0);
  llvm::Value* gep = theBuilder->CreateInBoundsGEP(array_type, array, {zero, index});
  value = styio_coerce_bounded_ring_value(value, elem_ty, theBuilder.get());
  if (handle_family.has_value()) {
    llvm::Value* old = theBuilder->CreateLoad(elem_ty, gep);
    free_resource_handle_if_runtime_owned(old, *handle_family);
    llvm::Value* owned = clone_resource_handle_for_runtime_owner(value, *handle_family);
    theBuilder->CreateStore(owned, gep);
    free_owned_resource_temp_if_tracked(value);
    return;
  }
  if (elem_ty->isPointerTy()) {
    llvm::Value* old = theBuilder->CreateLoad(elem_ty, gep);
    free_cstr_if_runtime_owned(old);
    value = clone_cstr_for_runtime_owner(value);
  }
  theBuilder->CreateStore(value, gep);
}

void
StyioToLLVM::move_bounded_ring_value(
  llvm::ArrayType* dst_type,
  llvm::AllocaInst* dst_array,
  llvm::Value* dst_index,
  llvm::ArrayType* src_type,
  llvm::AllocaInst* src_array,
  llvm::Value* src_index,
  std::optional<StyioValueFamily> handle_family) {
  if (dst_type == nullptr || dst_array == nullptr || dst_index == nullptr
      || src_type == nullptr || src_array == nullptr || src_index == nullptr) {
    return;
  }
  llvm::Type* dst_elem_ty = dst_type->getElementType();
  llvm::Type* src_elem_ty = src_type->getElementType();
  llvm::Value* zero = llvm::ConstantInt::get(theBuilder->getInt64Ty(), 0);
  llvm::Value* src_gep = theBuilder->CreateInBoundsGEP(src_type, src_array, {zero, src_index});
  llvm::Value* dst_gep = theBuilder->CreateInBoundsGEP(dst_type, dst_array, {zero, dst_index});
  llvm::Value* value = theBuilder->CreateLoad(src_elem_ty, src_gep);
  value = styio_coerce_bounded_ring_value(value, dst_elem_ty, theBuilder.get());
  if (handle_family.has_value()) {
    llvm::Value* old = theBuilder->CreateLoad(dst_elem_ty, dst_gep);
    free_resource_handle_if_runtime_owned(old, *handle_family);
    theBuilder->CreateStore(value, dst_gep);
    theBuilder->CreateStore(styio_zero_for_llvm_type(src_elem_ty, theBuilder.get()), src_gep);
    return;
  }
  if (dst_elem_ty->isPointerTy()) {
    llvm::Value* old = theBuilder->CreateLoad(dst_elem_ty, dst_gep);
    free_cstr_if_runtime_owned(old);
  }
  theBuilder->CreateStore(value, dst_gep);
  if (src_elem_ty->isPointerTy()) {
    theBuilder->CreateStore(
      llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(src_elem_ty)),
      src_gep);
  }
}

void
StyioToLLVM::track_owned_resource_temp(llvm::Value* v, TempResourceKind kind) {
  if (!v) {
    return;
  }
  owned_resource_temps_[v] = kind;
}

std::optional<StyioToLLVM::TempResourceKind>
StyioToLLVM::take_owned_resource_temp(llvm::Value* v) {
  if (!v) {
    return std::nullopt;
  }
  auto it = owned_resource_temps_.find(v);
  if (it == owned_resource_temps_.end()) {
    return std::nullopt;
  }
  TempResourceKind kind = it->second;
  owned_resource_temps_.erase(it);
  return kind;
}

void
StyioToLLVM::forget_owned_resource_temp(llvm::Value* v) {
  if (v) {
    owned_resource_temps_.erase(v);
  }
}

void
StyioToLLVM::free_resource_if_runtime_owned(llvm::Value* v, TempResourceKind kind) {
  if (!v || !v->getType()->isIntegerTy(64)) {
    return;
  }
  switch (kind) {
    case TempResourceKind::List:
      theBuilder->CreateCall(list_release_fn(), {v});
      break;
    case TempResourceKind::Dict:
      theBuilder->CreateCall(dict_release_fn(), {v});
      break;
    case TempResourceKind::Matrix:
      theBuilder->CreateCall(matrix_release_fn(), {v});
      break;
  }
}

void
StyioToLLVM::free_owned_resource_temp_if_tracked(llvm::Value* v) {
  auto kind = take_owned_resource_temp(v);
  if (!kind.has_value()) {
    return;
  }
  free_resource_if_runtime_owned(v, *kind);
}

llvm::Value*
StyioToLLVM::clone_resource_handle_for_runtime_owner(llvm::Value* v, StyioValueFamily family) {
  if (v == nullptr) {
    return v;
  }
  if (!v->getType()->isIntegerTy(64)) {
    v = theBuilder->CreateSExtOrTrunc(v, theBuilder->getInt64Ty());
  }
  const char* clone_name = nullptr;
  if (family == StyioValueFamily::ListHandle) {
    clone_name = "styio_list_clone";
  }
  else if (family == StyioValueFamily::DictHandle) {
    clone_name = "styio_dict_clone";
  }
  else if (family == StyioValueFamily::MatrixHandle) {
    clone_name = "styio_matrix_clone";
  }
  if (clone_name == nullptr) {
    return v;
  }
  llvm::FunctionCallee clone_fn = theModule->getOrInsertFunction(
    clone_name,
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {theBuilder->getInt64Ty()}, false));
  return theBuilder->CreateCall(clone_fn, {v});
}

void
StyioToLLVM::free_resource_handle_if_runtime_owned(llvm::Value* v, StyioValueFamily family) {
  if (v == nullptr || !v->getType()->isIntegerTy(64)) {
    return;
  }
  if (family == StyioValueFamily::ListHandle) {
    theBuilder->CreateCall(list_release_fn(), {v});
  }
  else if (family == StyioValueFamily::DictHandle) {
    theBuilder->CreateCall(dict_release_fn(), {v});
  }
  else if (family == StyioValueFamily::MatrixHandle) {
    theBuilder->CreateCall(matrix_release_fn(), {v});
  }
}

llvm::StructType*
StyioToLLVM::dynamic_cell_type() {
  return styio_dynamic_cell_type(*theContext);
}

llvm::AllocaInst*
StyioToLLVM::create_entry_alloca(llvm::Type* type, const std::string& name) {
  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::BasicBlock* ent = &F->getEntryBlock();
  llvm::IRBuilder<> prealloc(ent, ent->getFirstInsertionPt());
  return prealloc.CreateAlloca(type, nullptr, name.c_str());
}

void
StyioToLLVM::store_dynamic_slot(
  llvm::AllocaInst* slot,
  std::int64_t tag,
  llvm::Value* i64v,
  llvm::Value* f64v,
  llvm::Value* ptrv
) {
  auto* cell_ty = dynamic_cell_type();
  llvm::Value* zero32 = theBuilder->getInt32(0);
  llvm::Value* tag_gep = theBuilder->CreateInBoundsGEP(cell_ty, slot, {zero32, theBuilder->getInt32(0)});
  llvm::Value* i64_gep = theBuilder->CreateInBoundsGEP(cell_ty, slot, {zero32, theBuilder->getInt32(1)});
  llvm::Value* f64_gep = theBuilder->CreateInBoundsGEP(cell_ty, slot, {zero32, theBuilder->getInt32(2)});
  llvm::Value* ptr_gep = theBuilder->CreateInBoundsGEP(cell_ty, slot, {zero32, theBuilder->getInt32(3)});

  llvm::Value* i64_val = i64v ? i64v : llvm::ConstantInt::get(theBuilder->getInt64Ty(), 0);
  llvm::Value* f64_val = f64v ? f64v : llvm::ConstantFP::get(theBuilder->getDoubleTy(), 0.0);
  llvm::Value* ptr_val = ptrv ? ptrv : llvm::ConstantPointerNull::get(llvm::PointerType::get(*theContext, 0));

  if (i64_val->getType()->isIntegerTy(1)) {
    i64_val = theBuilder->CreateZExt(i64_val, theBuilder->getInt64Ty());
  }
  else if (!i64_val->getType()->isIntegerTy(64)) {
    if (!i64_val->getType()->isIntegerTy()) {
      throw StyioTypeError("dynamic slot integer field received a non-integer value");
    }
    i64_val = theBuilder->CreateSExtOrTrunc(i64_val, theBuilder->getInt64Ty());
  }
  if (!f64_val->getType()->isDoubleTy()) {
    if (f64_val->getType()->isIntegerTy()) {
      f64_val = theBuilder->CreateSIToFP(f64_val, theBuilder->getDoubleTy());
    }
    else {
      throw StyioTypeError("dynamic slot floating field received a non-numeric value");
    }
  }
  if (!ptr_val->getType()->isPointerTy()) {
    throw StyioTypeError("dynamic slot pointer field received a non-pointer value");
  }

  theBuilder->CreateStore(llvm::ConstantInt::get(theBuilder->getInt64Ty(), tag), tag_gep);
  theBuilder->CreateStore(i64_val, i64_gep);
  theBuilder->CreateStore(f64_val, f64_gep);
  theBuilder->CreateStore(ptr_val, ptr_gep);
}

void
StyioToLLVM::init_dynamic_slot_undef(llvm::AllocaInst* slot) {
  store_dynamic_slot(slot, styio_dynamic_tag_value(StyioDynamicTag::Undef), nullptr, nullptr, nullptr);
}

void
StyioToLLVM::release_dynamic_slot_contents(llvm::AllocaInst* slot) {
  auto* cell_ty = dynamic_cell_type();
  llvm::Value* zero32 = theBuilder->getInt32(0);
  llvm::Value* tag_gep = theBuilder->CreateInBoundsGEP(cell_ty, slot, {zero32, theBuilder->getInt32(0)});
  llvm::Value* i64_gep = theBuilder->CreateInBoundsGEP(cell_ty, slot, {zero32, theBuilder->getInt32(1)});
  llvm::Value* ptr_gep = theBuilder->CreateInBoundsGEP(cell_ty, slot, {zero32, theBuilder->getInt32(3)});
  llvm::Value* tag = theBuilder->CreateLoad(theBuilder->getInt64Ty(), tag_gep);

  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::BasicBlock* done_bb = llvm::BasicBlock::Create(*theContext, "dynrel_done", F);
  llvm::BasicBlock* cstr_bb = llvm::BasicBlock::Create(*theContext, "dynrel_cstr", F);
  llvm::BasicBlock* list_bb = llvm::BasicBlock::Create(*theContext, "dynrel_list", F);
  llvm::BasicBlock* dict_bb = llvm::BasicBlock::Create(*theContext, "dynrel_dict", F);
  llvm::BasicBlock* matrix_bb = llvm::BasicBlock::Create(*theContext, "dynrel_matrix", F);

  llvm::Value* is_cstr = theBuilder->CreateICmpEQ(
    tag, theBuilder->getInt64(styio_dynamic_tag_value(StyioDynamicTag::CStr)));
  llvm::Value* is_list = theBuilder->CreateICmpEQ(
    tag, theBuilder->getInt64(styio_dynamic_tag_value(StyioDynamicTag::List)));
  llvm::Value* is_dict = theBuilder->CreateICmpEQ(
    tag, theBuilder->getInt64(styio_dynamic_tag_value(StyioDynamicTag::Dict)));
  llvm::Value* is_matrix = theBuilder->CreateICmpEQ(
    tag, theBuilder->getInt64(styio_dynamic_tag_value(StyioDynamicTag::Matrix)));
  llvm::Value* is_task = theBuilder->CreateICmpEQ(
    tag, theBuilder->getInt64(styio_dynamic_tag_value(StyioDynamicTag::Task)));
  theBuilder->CreateCondBr(is_cstr, cstr_bb, list_bb);

  theBuilder->SetInsertPoint(cstr_bb);
  llvm::Value* ptr = theBuilder->CreateLoad(llvm::PointerType::get(*theContext, 0), ptr_gep);
  free_cstr_if_runtime_owned(ptr);
  theBuilder->CreateBr(done_bb);

  theBuilder->SetInsertPoint(list_bb);
  llvm::BasicBlock* list_release_bb = llvm::BasicBlock::Create(*theContext, "dynrel_list_do", F);
  theBuilder->CreateCondBr(is_list, list_release_bb, dict_bb);

  theBuilder->SetInsertPoint(list_release_bb);
  llvm::Value* handle = theBuilder->CreateLoad(theBuilder->getInt64Ty(), i64_gep);
  theBuilder->CreateCall(list_release_fn(), {handle});
  theBuilder->CreateBr(done_bb);

  theBuilder->SetInsertPoint(dict_bb);
  llvm::BasicBlock* dict_release_bb = llvm::BasicBlock::Create(*theContext, "dynrel_dict_do", F);
  theBuilder->CreateCondBr(is_dict, dict_release_bb, matrix_bb);

  theBuilder->SetInsertPoint(dict_release_bb);
  llvm::Value* dict_handle = theBuilder->CreateLoad(theBuilder->getInt64Ty(), i64_gep);
  theBuilder->CreateCall(dict_release_fn(), {dict_handle});
  theBuilder->CreateBr(done_bb);

  theBuilder->SetInsertPoint(matrix_bb);
  llvm::BasicBlock* matrix_release_bb = llvm::BasicBlock::Create(*theContext, "dynrel_matrix_do", F);
  llvm::BasicBlock* task_bb = llvm::BasicBlock::Create(*theContext, "dynrel_task", F);
  theBuilder->CreateCondBr(is_matrix, matrix_release_bb, task_bb);

  theBuilder->SetInsertPoint(matrix_release_bb);
  llvm::Value* matrix_handle = theBuilder->CreateLoad(theBuilder->getInt64Ty(), i64_gep);
  theBuilder->CreateCall(matrix_release_fn(), {matrix_handle});
  theBuilder->CreateBr(done_bb);

  theBuilder->SetInsertPoint(task_bb);
  llvm::BasicBlock* task_release_bb = llvm::BasicBlock::Create(*theContext, "dynrel_task_do", F);
  theBuilder->CreateCondBr(is_task, task_release_bb, done_bb);

  theBuilder->SetInsertPoint(task_release_bb);
  llvm::Value* task_handle = theBuilder->CreateLoad(theBuilder->getInt64Ty(), i64_gep);
  theBuilder->CreateCall(task_release_fn(), {task_handle});
  theBuilder->CreateBr(done_bb);

  theBuilder->SetInsertPoint(done_bb);
  init_dynamic_slot_undef(slot);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGResId* node) {
  const string& name = node->as_str();

  if (node->function_reference) {
    llvm::Function* function = theModule->getFunction(name);
    if (function == nullptr) {
      throw StyioTypeError(
        "unknown callable specialization `" + name + "`");
    }
    return function;
  }

  if (bounded_ring_head_slot_.contains(name)) {
    llvm::AllocaInst* arr = mutable_variables[name];
    llvm::AllocaInst* headSlot = bounded_ring_head_slot_[name];
    std::uint64_t cap = bounded_ring_capacity_[name];
    llvm::Type* i64 = theBuilder->getInt64Ty();
    auto* arrTy = llvm::cast<llvm::ArrayType>(arr->getAllocatedType());
    llvm::Type* elem_ty = arrTy->getElementType();
    llvm::Value* head = theBuilder->CreateLoad(i64, headSlot);
    llvm::Value* zero = llvm::ConstantInt::get(i64, 0);
    llvm::Value* zero_elem = styio_zero_for_llvm_type(elem_ty, theBuilder.get());
    const int depth = node->has_history_selector && node->history_offset < 0
      ? -node->history_offset
      : 1;
    llvm::Value* depthv = llvm::ConstantInt::get(i64, static_cast<std::uint64_t>(depth));
    llvm::Value* has = theBuilder->CreateICmpUGE(head, depthv);
    llvm::Value* prev = theBuilder->CreateSub(head, depthv);
    llvm::Value* capv = llvm::ConstantInt::get(i64, cap);
    llvm::Value* prev_m = theBuilder->CreateURem(prev, capv);
    llvm::Value* idx = theBuilder->CreateSelect(has, prev_m, zero);
    llvm::Value* gep = theBuilder->CreateInBoundsGEP(arrTy, arr, {zero, idx});
    llvm::Value* cell = theBuilder->CreateLoad(elem_ty, gep);
    llvm::Value* selected = theBuilder->CreateSelect(has, cell, zero_elem);
    auto handle_family_it = bounded_ring_handle_family_.find(name);
    if (handle_family_it != bounded_ring_handle_family_.end()) {
      llvm::Value* owned = clone_resource_handle_for_runtime_owner(selected, handle_family_it->second);
      if (handle_family_it->second == StyioValueFamily::ListHandle) {
        track_owned_resource_temp(owned, TempResourceKind::List);
      }
      else if (handle_family_it->second == StyioValueFamily::DictHandle) {
        track_owned_resource_temp(owned, TempResourceKind::Dict);
      }
      else if (handle_family_it->second == StyioValueFamily::MatrixHandle) {
        track_owned_resource_temp(owned, TempResourceKind::Matrix);
      }
      return owned;
    }
    if (elem_ty->isPointerTy()) {
      llvm::Value* owned = clone_cstr_for_runtime_owner(selected);
      track_owned_cstr_temp(owned);
      return owned;
    }
    return selected;
  }

  if (named_values.contains(name)) {
    return named_values[name];
  }

  if (mutable_variables.contains(name)) {
    llvm::AllocaInst* variable = mutable_variables[name];
    return theBuilder->CreateLoad(variable->getAllocatedType(), variable);
  }

  auto capture = callable_capture_globals_.find(name);
  if (active_callable_capture_names_.contains(name)
      && capture != callable_capture_globals_.end()) {
    return theBuilder->CreateLoad(
      capture->second->getValueType(),
      capture->second);
  }

  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGType* node) {
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGNoOp* node) {
  (void)node;
  return nullptr;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGDynLoad* node) {
  auto it = mutable_variables.find(node->var_name);
  if (it == mutable_variables.end()) {
    switch (node->kind) {
      case SGDynLoadKind::Bool:
        return llvm::ConstantInt::getFalse(*theContext);
      case SGDynLoadKind::F64:
        return llvm::ConstantFP::get(theBuilder->getDoubleTy(), 0.0);
      case SGDynLoadKind::CString:
        return llvm::ConstantPointerNull::get(llvm::PointerType::get(*theContext, 0));
      case SGDynLoadKind::I64:
      case SGDynLoadKind::ListHandle:
      case SGDynLoadKind::DictHandle:
      case SGDynLoadKind::MatrixHandle:
      case SGDynLoadKind::TaskHandle:
        return theBuilder->getInt64(0);
    }
  }

  llvm::AllocaInst* slot = it->second;
  auto* cell_ty = dynamic_cell_type();
  llvm::Value* zero32 = theBuilder->getInt32(0);
  llvm::Value* i64_gep = theBuilder->CreateInBoundsGEP(cell_ty, slot, {zero32, theBuilder->getInt32(1)});
  llvm::Value* f64_gep = theBuilder->CreateInBoundsGEP(cell_ty, slot, {zero32, theBuilder->getInt32(2)});
  llvm::Value* ptr_gep = theBuilder->CreateInBoundsGEP(cell_ty, slot, {zero32, theBuilder->getInt32(3)});

  switch (node->kind) {
    case SGDynLoadKind::Bool: {
      llvm::Value* raw = theBuilder->CreateLoad(theBuilder->getInt64Ty(), i64_gep);
      return theBuilder->CreateICmpNE(raw, theBuilder->getInt64(0));
    }
    case SGDynLoadKind::I64:
    case SGDynLoadKind::ListHandle:
    case SGDynLoadKind::DictHandle:
    case SGDynLoadKind::MatrixHandle:
    case SGDynLoadKind::TaskHandle:
      return theBuilder->CreateLoad(theBuilder->getInt64Ty(), i64_gep);
    case SGDynLoadKind::F64:
      return theBuilder->CreateLoad(theBuilder->getDoubleTy(), f64_gep);
    case SGDynLoadKind::CString:
      return theBuilder->CreateLoad(llvm::PointerType::get(*theContext, 0), ptr_gep);
  }

  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGConstBool* node) {
  return llvm::ConstantInt::getBool(*theContext, node->value);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGConstInt* node) {
  long long v = std::stoll(node->value);
  return llvm::ConstantInt::get(
    theBuilder->getInt64Ty(),
    static_cast<uint64_t>(v),
    /*isSigned=*/true);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGConstFloat* node) {
  return llvm::ConstantFP::get(*theContext, llvm::APFloat(std::stod(node->value)));
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGConstChar* node) {
  return theBuilder->getInt8(node->value);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGConstString* node) {
  return theBuilder->CreateGlobalStringPtr(node->value, "styio_str");
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGFormatString* node) {
  auto output = theBuilder->getInt32(0);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGStruct* node) {
  auto output = theBuilder->getInt32(0);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGCast* node) {
  if (node == nullptr || node->value == nullptr || node->to_type == nullptr) {
    throw StyioTypeError("cast lowering requires a value and target type");
  }

  llvm::Value* value = node->value->toLLVMIR(this);
  llvm::Type* target_ty = node->to_type->toLLVMType(this);
  if (value == nullptr || target_ty == nullptr) {
    throw StyioTypeError("cast lowering produced an invalid value or target type");
  }
  if (value->getType() == target_ty) {
    return value;
  }

  if (target_ty->isDoubleTy()) {
    if (value->getType()->isIntegerTy()) {
      return theBuilder->CreateSIToFP(value, target_ty);
    }
    if (value->getType()->isPointerTy()) {
      return cstr_to_f64_checked(value);
    }
  }

  if (target_ty->isIntegerTy(1)) {
    if (value->getType()->isIntegerTy()) {
      return theBuilder->CreateICmpNE(
        value,
        llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(value->getType()), 0));
    }
    if (value->getType()->isFloatingPointTy()) {
      return theBuilder->CreateFCmpONE(
        value,
        llvm::ConstantFP::get(value->getType(), 0.0));
    }
  }

  if (target_ty->isIntegerTy()) {
    if (value->getType()->isIntegerTy()) {
      if (value->getType()->isIntegerTy(1)) {
        return theBuilder->CreateZExt(value, target_ty);
      }
      return theBuilder->CreateSExtOrTrunc(value, target_ty);
    }
    if (value->getType()->isFloatingPointTy()) {
      return theBuilder->CreateFPToSI(value, target_ty);
    }
    if (value->getType()->isPointerTy()) {
      llvm::Value* parsed = cstr_to_i64_checked(value);
      return target_ty->isIntegerTy(64)
        ? parsed
        : theBuilder->CreateSExtOrTrunc(parsed, target_ty);
    }
  }

  throw StyioTypeError("unsupported scalar cast lowering");
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGBinOp* node) {
  StyioDataType data_type = node->data_type->data_type;

  using Bin2 = std::function<llvm::Value*(llvm::Value*, llvm::Value*)>;
  llvm::Type* char_ptr_ty = llvm::PointerType::get(*theContext, 0);

  auto ptr_to_i64_for_arith = [&](llvm::Value* v) -> llvm::Value* {
    if (!v->getType()->isPointerTy()) {
      return v;
    }
    return cstr_to_i64_checked(v);
  };
  auto ptr_to_f64_for_arith = [&](llvm::Value* v) -> llvm::Value* {
    if (!v->getType()->isPointerTy()) {
      return v;
    }
    return cstr_to_f64_checked(v);
  };

  llvm::Value* const styioUndef = theBuilder->getInt64(styio_undef_i64());
  auto guarded_int_divrem = [&](llvm::Value* numerator, llvm::Value* divisor, bool is_remainder) -> llvm::Value* {
    if (!numerator->getType()->isIntegerTy() || !divisor->getType()->isIntegerTy()) {
      throw StyioTypeError("integer division lowering requires integer operands");
    }

    llvm::Type* result_ty = numerator->getType();
    if (divisor->getType() != result_ty) {
      divisor = theBuilder->CreateSExtOrTrunc(divisor, result_ty);
    }

    auto* int_ty = llvm::cast<llvm::IntegerType>(result_ty);
    const unsigned bits = int_ty->getBitWidth();
    llvm::Value* zero = llvm::ConstantInt::get(int_ty, 0, true);
    llvm::Value* one = llvm::ConstantInt::get(int_ty, 1, true);
    llvm::Value* false_value = llvm::ConstantInt::getFalse(*theContext);

    llvm::Value* numerator_is_undef = false_value;
    llvm::Value* divisor_is_undef = false_value;
    llvm::Value* fallback = zero;
    if (result_ty->isIntegerTy(64)) {
      numerator_is_undef = theBuilder->CreateICmpEQ(numerator, styioUndef);
      divisor_is_undef = theBuilder->CreateICmpEQ(divisor, styioUndef);
      fallback = styioUndef;
    }

    llvm::Value* divisor_is_zero = theBuilder->CreateICmpEQ(divisor, zero);
    llvm::Value* divisor_is_bad = theBuilder->CreateOr(divisor_is_undef, divisor_is_zero);

    llvm::Value* min_int = llvm::ConstantInt::get(int_ty, llvm::APInt::getSignedMinValue(bits));
    llvm::Value* minus_one = llvm::ConstantInt::getSigned(int_ty, -1);
    llvm::Value* would_overflow = theBuilder->CreateAnd(
      theBuilder->CreateICmpEQ(numerator, min_int),
      theBuilder->CreateICmpEQ(divisor, minus_one));

    llvm::Value* numerator_is_bad = theBuilder->CreateOr(numerator_is_undef, would_overflow);
    llvm::Value* bad = theBuilder->CreateOr(numerator_is_bad, divisor_is_bad);
    llvm::Value* safe_numerator = theBuilder->CreateSelect(numerator_is_bad, zero, numerator);
    llvm::Value* safe_divisor = theBuilder->CreateSelect(divisor_is_bad, one, divisor);
    llvm::Value* raw = is_remainder
      ? theBuilder->CreateSRem(safe_numerator, safe_divisor)
      : theBuilder->CreateSDiv(safe_numerator, safe_divisor);
    return theBuilder->CreateSelect(bad, fallback, raw);
  };

  auto do_self_assign = [&](Bin2 bi, Bin2 bf) -> llvm::Value* {
    auto* lid = static_cast<SGResId*>(node->lhs_expr);
    const std::string& varname = lid->as_str();
    if (not mutable_variables.contains(varname)) {
      throw StyioTypeError(
        std::string("compound assignment requires a mutable binding: ") + varname);
    }
    llvm::AllocaInst* slot = mutable_variables[varname];
    llvm::Type* slot_ty = slot->getAllocatedType();
    llvm::Value* cur = theBuilder->CreateLoad(slot_ty, slot);
    llvm::Value* r_val = node->rhs_expr->toLLVMIR(this);
    llvm::Value* next = nullptr;
    if (slot_ty->isDoubleTy()) {
      r_val = ptr_to_f64_for_arith(r_val);
      if (not r_val->getType()->isDoubleTy()) {
        r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
      }
      if (not cur->getType()->isDoubleTy()) {
        cur = theBuilder->CreateSIToFP(cur, theBuilder->getDoubleTy());
      }
      next = bf(cur, r_val);
    }
    else {
      r_val = ptr_to_i64_for_arith(r_val);
      if (r_val->getType()->isDoubleTy()) {
        cur = theBuilder->CreateSIToFP(cur, theBuilder->getDoubleTy());
      }
      next = bi(cur, r_val);
    }
    theBuilder->CreateStore(next, slot);
    return next;
  };

  llvm::Value* l_val = node->lhs_expr->toLLVMIR(this);
  llvm::Value* r_val = node->rhs_expr->toLLVMIR(this);
  const bool compares_strings =
    node->lhs_type.option == StyioDataTypeOption::String
    && node->rhs_type.option == StyioDataTypeOption::String;
  auto compare_strings = [&]() -> llvm::Value*
  {
    if (!l_val->getType()->isPointerTy()
        || !r_val->getType()->isPointerTy()) {
      throw StyioTypeError(
        "string comparison lowering requires string operands");
    }
    llvm::Type* i32 = theBuilder->getInt32Ty();
    llvm::FunctionCallee strcmp_fn = theModule->getOrInsertFunction(
      "strcmp",
      llvm::FunctionType::get(
        i32,
        {char_ptr_ty, char_ptr_ty},
        false));
    return theBuilder->CreateCall(strcmp_fn, {l_val, r_val});
  };

  if (styio_is_matrix_type(data_type)) {
    const bool lhs_matrix = styio_is_matrix_type(node->lhs_type);
    const bool rhs_matrix = styio_is_matrix_type(node->rhs_type);
    const bool result_f64 =
      styio_value_family_from_type_name(styio_matrix_elem_type_name(data_type))
      == StyioValueFamily::Float;
    auto matrix_suffix = [&](const StyioDataType& type) {
      return styio_value_family_from_type_name(styio_matrix_elem_type_name(type))
          == StyioValueFamily::Float
        ? std::string("f64")
        : std::string("i64");
    };
    auto coerce_i64 = [&](llvm::Value* v) -> llvm::Value* {
      if (v->getType()->isIntegerTy(64)) {
        return v;
      }
      if (v->getType()->isDoubleTy()) {
        return theBuilder->CreateFPToSI(v, theBuilder->getInt64Ty());
      }
      return v->getType()->isIntegerTy()
        ? theBuilder->CreateSExtOrTrunc(v, theBuilder->getInt64Ty())
        : theBuilder->getInt64(0);
    };
    auto coerce_f64 = [&](llvm::Value* v) -> llvm::Value* {
      if (v->getType()->isDoubleTy()) {
        return v;
      }
      return v->getType()->isIntegerTy()
        ? theBuilder->CreateSIToFP(v, theBuilder->getDoubleTy())
        : llvm::ConstantFP::get(theBuilder->getDoubleTy(), 0.0);
    };
    auto matrix_helper_call = [&](const std::string& name, std::vector<llvm::Value*> args) -> llvm::Value* {
      std::vector<llvm::Type*> tys;
      tys.reserve(args.size());
      for (llvm::Value* arg : args) {
        tys.push_back(arg->getType()->isDoubleTy() ? theBuilder->getDoubleTy() : theBuilder->getInt64Ty());
      }
      llvm::FunctionCallee fn = theModule->getOrInsertFunction(
        name,
        llvm::FunctionType::get(theBuilder->getInt64Ty(), tys, false));
      llvm::Value* out = theBuilder->CreateCall(fn, args);
      track_owned_resource_temp(out, TempResourceKind::Matrix);
      return out;
    };
    auto emit_inline_matrix_binary = [&]() -> llvm::Value* {
      if (!lhs_matrix || !rhs_matrix) {
        return nullptr;
      }
      const size_t rows = styio_matrix_row_count(data_type);
      const size_t cols = styio_matrix_col_count(data_type);
      const size_t lhs_cols = styio_matrix_col_count(node->lhs_type);
      if (rows == 0 || cols == 0 || rows > 4 || cols > 4 || lhs_cols > 4) {
        return nullptr;
      }
      if (matrix_suffix(node->lhs_type) != matrix_suffix(data_type)
          || matrix_suffix(node->rhs_type) != matrix_suffix(data_type)) {
        return nullptr;
      }
      if (node->operand != StyioOpType::Binary_Add
          && node->operand != StyioOpType::Binary_Sub
          && node->operand != StyioOpType::Binary_Mul) {
        return nullptr;
      }
      llvm::Type* elem_ty = result_f64 ? theBuilder->getDoubleTy() : theBuilder->getInt64Ty();
      std::string suffix = result_f64 ? "f64" : "i64";
      llvm::FunctionCallee new_fn = theModule->getOrInsertFunction(
        "styio_matrix_new_" + suffix,
        llvm::FunctionType::get(
          theBuilder->getInt64Ty(),
          {theBuilder->getInt64Ty(), theBuilder->getInt64Ty()},
          false));
      llvm::FunctionCallee data_fn = theModule->getOrInsertFunction(
        "styio_matrix_data_" + suffix,
        llvm::FunctionType::get(
          llvm::PointerType::get(*theContext, 0),
          {theBuilder->getInt64Ty()},
          false));
      llvm::Value* out = theBuilder->CreateCall(
        new_fn,
        {theBuilder->getInt64(static_cast<std::int64_t>(rows)),
         theBuilder->getInt64(static_cast<std::int64_t>(cols))});
      emit_runtime_error_guard_return();
      llvm::Value* lhs_ptr = theBuilder->CreateCall(data_fn, {coerce_i64(l_val)});
      llvm::Value* rhs_ptr = theBuilder->CreateCall(data_fn, {coerce_i64(r_val)});
      llvm::Value* out_ptr = theBuilder->CreateCall(data_fn, {out});
      emit_runtime_error_guard_return();
      auto load_at = [&](llvm::Value* ptr, size_t index) -> llvm::Value* {
        llvm::Value* gep = theBuilder->CreateInBoundsGEP(
          elem_ty,
          ptr,
          theBuilder->getInt64(static_cast<std::int64_t>(index)));
        return theBuilder->CreateLoad(elem_ty, gep);
      };
      auto store_at = [&](size_t index, llvm::Value* value) {
        llvm::Value* gep = theBuilder->CreateInBoundsGEP(
          elem_ty,
          out_ptr,
          theBuilder->getInt64(static_cast<std::int64_t>(index)));
        theBuilder->CreateStore(value, gep);
      };
      if (node->operand == StyioOpType::Binary_Add || node->operand == StyioOpType::Binary_Sub) {
        for (size_t i = 0; i < rows * cols; ++i) {
          llvm::Value* a = load_at(lhs_ptr, i);
          llvm::Value* b = load_at(rhs_ptr, i);
          llvm::Value* v = node->operand == StyioOpType::Binary_Add
            ? (result_f64 ? theBuilder->CreateFAdd(a, b) : theBuilder->CreateAdd(a, b))
            : (result_f64 ? theBuilder->CreateFSub(a, b) : theBuilder->CreateSub(a, b));
          store_at(i, v);
        }
      }
      else {
        for (size_t r = 0; r < rows; ++r) {
          for (size_t c = 0; c < cols; ++c) {
            llvm::Value* sum = result_f64
              ? static_cast<llvm::Value*>(llvm::ConstantFP::get(elem_ty, 0.0))
              : static_cast<llvm::Value*>(theBuilder->getInt64(0));
            for (size_t k = 0; k < lhs_cols; ++k) {
              llvm::Value* a = load_at(lhs_ptr, r * lhs_cols + k);
              llvm::Value* b = load_at(rhs_ptr, k * cols + c);
              llvm::Value* prod = result_f64
                ? theBuilder->CreateFMul(a, b)
                : theBuilder->CreateMul(a, b);
              sum = result_f64
                ? theBuilder->CreateFAdd(sum, prod)
                : theBuilder->CreateAdd(sum, prod);
            }
            store_at(r * cols + c, sum);
          }
        }
      }
      free_owned_resource_temp_if_tracked(l_val);
      free_owned_resource_temp_if_tracked(r_val);
      track_owned_resource_temp(out, TempResourceKind::Matrix);
      return out;
    };
    if (llvm::Value* out = emit_inline_matrix_binary()) {
      return out;
    }
    std::string suffix = result_f64 ? "f64" : "i64";
    if (lhs_matrix && rhs_matrix) {
      const char* op_name = node->operand == StyioOpType::Binary_Add
        ? "add"
        : (node->operand == StyioOpType::Binary_Sub ? "sub" : "matmul");
      llvm::Value* out = matrix_helper_call(
        std::string("styio_matrix_") + op_name + "_" + suffix,
        {coerce_i64(l_val), coerce_i64(r_val)});
      free_owned_resource_temp_if_tracked(l_val);
      free_owned_resource_temp_if_tracked(r_val);
      emit_runtime_error_guard_return();
      return out;
    }
    if (node->operand == StyioOpType::Binary_Mul && (lhs_matrix || rhs_matrix)) {
      llvm::Value* matrix = lhs_matrix ? l_val : r_val;
      llvm::Value* scalar = lhs_matrix ? r_val : l_val;
      llvm::Value* out = result_f64
        ? matrix_helper_call("styio_matrix_scale_f64", {coerce_i64(matrix), coerce_f64(scalar)})
        : matrix_helper_call("styio_matrix_scale_i64", {coerce_i64(matrix), coerce_i64(scalar)});
      free_owned_resource_temp_if_tracked(matrix);
      emit_runtime_error_guard_return();
      return out;
    }
  }

  switch (node->operand) {
    case StyioOpType::Binary_Add: {
      if (data_type.option == StyioDataTypeOption::String) {
        llvm::FunctionCallee cat = theModule->getOrInsertFunction(
          "styio_strcat_ab",
          llvm::FunctionType::get(char_ptr_ty, {char_ptr_ty, char_ptr_ty}, false));
        llvm::Value* a = l_val;
        llvm::Value* b = r_val;
        if (!a->getType()->isPointerTy()) {
          a = promote_to_cstr(a);
        }
        if (!b->getType()->isPointerTy()) {
          b = promote_to_cstr(b);
        }
        llvm::Value* out = theBuilder->CreateCall(cat, {a, b});
        free_owned_cstr_temp_if_tracked(a);
        free_owned_cstr_temp_if_tracked(b);
        track_owned_cstr_temp(out);
        return out;
      }
      if (data_type.isFloat() || l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
        l_val = ptr_to_f64_for_arith(l_val);
        r_val = ptr_to_f64_for_arith(r_val);
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFAdd(l_val, r_val);
      }
      if (data_type.isInteger() || (l_val->getType()->isIntegerTy() && r_val->getType()->isIntegerTy())) {
        l_val = ptr_to_i64_for_arith(l_val);
        r_val = ptr_to_i64_for_arith(r_val);
        if (r_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
          return theBuilder->CreateFAdd(l_val, r_val);
        }
        if (l_val->getType()->isIntegerTy(64) && r_val->getType()->isIntegerTy(64)) {
          llvm::Value* lu = theBuilder->CreateICmpEQ(l_val, styioUndef);
          llvm::Value* ru = theBuilder->CreateICmpEQ(r_val, styioUndef);
          llvm::Value* bad = theBuilder->CreateOr(lu, ru);
          llvm::Value* sum = theBuilder->CreateAdd(l_val, r_val);
          return theBuilder->CreateSelect(bad, styioUndef, sum);
        }
        return theBuilder->CreateAdd(l_val, r_val);
      }
    } break;

    case StyioOpType::Binary_Sub: {
      if (data_type.isFloat() || l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
        l_val = ptr_to_f64_for_arith(l_val);
        r_val = ptr_to_f64_for_arith(r_val);
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFSub(l_val, r_val);
      }
      if (data_type.isInteger() || (l_val->getType()->isIntegerTy() && r_val->getType()->isIntegerTy())) {
        l_val = ptr_to_i64_for_arith(l_val);
        r_val = ptr_to_i64_for_arith(r_val);
        if (l_val->getType()->isIntegerTy(64) && r_val->getType()->isIntegerTy(64)) {
          llvm::Value* lu = theBuilder->CreateICmpEQ(l_val, styioUndef);
          llvm::Value* ru = theBuilder->CreateICmpEQ(r_val, styioUndef);
          llvm::Value* bad = theBuilder->CreateOr(lu, ru);
          llvm::Value* out = theBuilder->CreateSub(l_val, r_val);
          return theBuilder->CreateSelect(bad, styioUndef, out);
        }
        return theBuilder->CreateSub(l_val, r_val);
      }
    } break;

    case StyioOpType::Binary_Mul: {
      if (data_type.isFloat() || l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
        l_val = ptr_to_f64_for_arith(l_val);
        r_val = ptr_to_f64_for_arith(r_val);
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFMul(l_val, r_val);
      }
      if (data_type.isInteger() || (l_val->getType()->isIntegerTy() && r_val->getType()->isIntegerTy())) {
        l_val = ptr_to_i64_for_arith(l_val);
        r_val = ptr_to_i64_for_arith(r_val);
        if (r_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
          return theBuilder->CreateFMul(l_val, r_val);
        }
        if (l_val->getType()->isIntegerTy(64) && r_val->getType()->isIntegerTy(64)) {
          llvm::Value* lu = theBuilder->CreateICmpEQ(l_val, styioUndef);
          llvm::Value* ru = theBuilder->CreateICmpEQ(r_val, styioUndef);
          llvm::Value* bad = theBuilder->CreateOr(lu, ru);
          llvm::Value* out = theBuilder->CreateMul(l_val, r_val);
          return theBuilder->CreateSelect(bad, styioUndef, out);
        }
        return theBuilder->CreateMul(l_val, r_val);
      }
    } break;

    case StyioOpType::Binary_Div: {
      if (data_type.isFloat() || l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
        l_val = ptr_to_f64_for_arith(l_val);
        r_val = ptr_to_f64_for_arith(r_val);
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFDiv(l_val, r_val);
      }
      if (data_type.isInteger() || (l_val->getType()->isIntegerTy() && r_val->getType()->isIntegerTy())) {
        l_val = ptr_to_i64_for_arith(l_val);
        r_val = ptr_to_i64_for_arith(r_val);
        if (l_val->getType()->isIntegerTy() && r_val->getType()->isIntegerTy()) {
          return guarded_int_divrem(l_val, r_val, false);
        }
      }
    } break;

    case StyioOpType::Binary_Pow: {
      llvm::Type* d = theBuilder->getDoubleTy();
      llvm::FunctionCallee pow_fn = theModule->getOrInsertFunction(
        "pow",
        llvm::FunctionType::get(d, {d, d}, false));
      l_val = ptr_to_f64_for_arith(l_val);
      r_val = ptr_to_f64_for_arith(r_val);
      llvm::Value* lf = l_val->getType()->isDoubleTy()
        ? l_val
        : theBuilder->CreateSIToFP(l_val, d);
      llvm::Value* rf = r_val->getType()->isDoubleTy()
        ? r_val
        : theBuilder->CreateSIToFP(r_val, d);
      llvm::Value* pr = theBuilder->CreateCall(pow_fn, {lf, rf});
      if (data_type.isInteger()) {
        return theBuilder->CreateFPToSI(pr, theBuilder->getInt64Ty());
      }
      return pr;
    } break;

    case StyioOpType::Binary_Mod: {
      if (data_type.isFloat() || l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
        l_val = ptr_to_f64_for_arith(l_val);
        r_val = ptr_to_f64_for_arith(r_val);
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFRem(l_val, r_val);
      }
      if (data_type.isInteger() || (l_val->getType()->isIntegerTy() && r_val->getType()->isIntegerTy())) {
        l_val = ptr_to_i64_for_arith(l_val);
        r_val = ptr_to_i64_for_arith(r_val);
        if (l_val->getType()->isIntegerTy() && r_val->getType()->isIntegerTy()) {
          return guarded_int_divrem(l_val, r_val, true);
        }
      }
    } break;

    case StyioOpType::Equal: {
      if (compares_strings) {
        return theBuilder->CreateICmpEQ(
          compare_strings(),
          theBuilder->getInt32(0));
      }
      if (l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
        l_val = ptr_to_f64_for_arith(l_val);
        r_val = ptr_to_f64_for_arith(r_val);
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFCmpOEQ(l_val, r_val);
      }
      return theBuilder->CreateICmpEQ(l_val, r_val);
    } break;

    case StyioOpType::Not_Equal: {
      if (compares_strings) {
        return theBuilder->CreateICmpNE(
          compare_strings(),
          theBuilder->getInt32(0));
      }
      if (l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
        l_val = ptr_to_f64_for_arith(l_val);
        r_val = ptr_to_f64_for_arith(r_val);
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFCmpONE(l_val, r_val);
      }
      return theBuilder->CreateICmpNE(l_val, r_val);
    } break;

    case StyioOpType::Greater_Than: {
      if (compares_strings) {
        return theBuilder->CreateICmpSGT(
          compare_strings(),
          theBuilder->getInt32(0));
      }
      if (l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
        l_val = ptr_to_f64_for_arith(l_val);
        r_val = ptr_to_f64_for_arith(r_val);
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFCmpOGT(l_val, r_val);
      }
      return theBuilder->CreateICmpSGT(l_val, r_val);
    } break;

    case StyioOpType::Greater_Than_Equal: {
      if (compares_strings) {
        return theBuilder->CreateICmpSGE(
          compare_strings(),
          theBuilder->getInt32(0));
      }
      if (l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
        l_val = ptr_to_f64_for_arith(l_val);
        r_val = ptr_to_f64_for_arith(r_val);
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFCmpOGE(l_val, r_val);
      }
      return theBuilder->CreateICmpSGE(l_val, r_val);
    } break;

    case StyioOpType::Less_Than: {
      if (compares_strings) {
        return theBuilder->CreateICmpSLT(
          compare_strings(),
          theBuilder->getInt32(0));
      }
      if (l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
        l_val = ptr_to_f64_for_arith(l_val);
        r_val = ptr_to_f64_for_arith(r_val);
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFCmpOLT(l_val, r_val);
      }
      return theBuilder->CreateICmpSLT(l_val, r_val);
    } break;

    case StyioOpType::Less_Than_Equal: {
      if (compares_strings) {
        return theBuilder->CreateICmpSLE(
          compare_strings(),
          theBuilder->getInt32(0));
      }
      if (l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
        l_val = ptr_to_f64_for_arith(l_val);
        r_val = ptr_to_f64_for_arith(r_val);
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFCmpOLE(l_val, r_val);
      }
      return theBuilder->CreateICmpSLE(l_val, r_val);
    } break;

    case StyioOpType::Self_Add_Assign: {
      return do_self_assign(
        [&](llvm::Value* a, llvm::Value* b) { return theBuilder->CreateAdd(a, b); },
        [&](llvm::Value* a, llvm::Value* b) { return theBuilder->CreateFAdd(a, b); });
    } break;

    case StyioOpType::Self_Sub_Assign: {
      return do_self_assign(
        [&](llvm::Value* a, llvm::Value* b) { return theBuilder->CreateSub(a, b); },
        [&](llvm::Value* a, llvm::Value* b) { return theBuilder->CreateFSub(a, b); });
    } break;

    case StyioOpType::Self_Mul_Assign: {
      return do_self_assign(
        [&](llvm::Value* a, llvm::Value* b) { return theBuilder->CreateMul(a, b); },
        [&](llvm::Value* a, llvm::Value* b) { return theBuilder->CreateFMul(a, b); });
    } break;

    case StyioOpType::Self_Div_Assign: {
      return do_self_assign(
        [&](llvm::Value* a, llvm::Value* b) { return guarded_int_divrem(a, b, false); },
        [&](llvm::Value* a, llvm::Value* b) { return theBuilder->CreateFDiv(a, b); });
    } break;

    case StyioOpType::Self_Mod_Assign: {
      return do_self_assign(
        [&](llvm::Value* a, llvm::Value* b) { return guarded_int_divrem(a, b, true); },
        [&](llvm::Value* a, llvm::Value* b) { return theBuilder->CreateFRem(a, b); });
    } break;

    default:
      throw StyioTypeError("unsupported binary operator in codegen");
  }

  throw StyioTypeError("unsupported binary operand types in codegen");
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGCond* node) {
  llvm::Value* L = node->lhs_expr->toLLVMIR(this);
  llvm::Value* R = node->rhs_expr->toLLVMIR(this);
  if (node->operand == StyioOpType::Logic_AND) {
    if (L->getType()->isIntegerTy(1) && R->getType()->isIntegerTy(64)) {
      return theBuilder->CreateSelect(L, R, theBuilder->getInt64(0));
    }
    if (R->getType()->isIntegerTy(1) && L->getType()->isIntegerTy(64)) {
      return theBuilder->CreateSelect(R, L, theBuilder->getInt64(0));
    }
  }
  auto to_bool = [&](llvm::Value* v) -> llvm::Value* {
    if (v->getType()->isIntegerTy(1)) {
      return v;
    }
    return theBuilder->CreateICmpNE(
      v,
      llvm::ConstantInt::get(
        llvm::cast<llvm::IntegerType>(v->getType()), 0));
  };
  L = to_bool(L);
  R = to_bool(R);
  if (node->operand == StyioOpType::Logic_NOT) {
    return theBuilder->CreateNot(L);
  }
  if (node->operand == StyioOpType::Logic_AND) {
    return theBuilder->CreateAnd(L, R);
  }
  if (node->operand == StyioOpType::Logic_XOR) {
    return theBuilder->CreateXor(L, R);
  }
  if (node->operand == StyioOpType::Logic_OR) {
    return theBuilder->CreateOr(L, R);
  }
  throw StyioTypeError("unsupported logical condition operator in codegen");
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGVar* node) {
  auto output = theBuilder->getInt32(0);
  return output;
}

void
StyioToLLVM::emit_bounded_ring_pending_commit(const std::string& name) {
  auto arr_it = mutable_variables.find(name);
  auto head_it = bounded_ring_head_slot_.find(name);
  auto cap_it = bounded_ring_capacity_.find(name);
  auto pending_it = bounded_ring_pending_slot_.find(name);
  auto count_it = bounded_ring_pending_count_slot_.find(name);
  std::optional<StyioValueFamily> handle_family;
  auto handle_family_it = bounded_ring_handle_family_.find(name);
  if (handle_family_it != bounded_ring_handle_family_.end()) {
    handle_family = handle_family_it->second;
  }
  if (arr_it == mutable_variables.end()
      || head_it == bounded_ring_head_slot_.end()
      || cap_it == bounded_ring_capacity_.end()
      || pending_it == bounded_ring_pending_slot_.end()
      || count_it == bounded_ring_pending_count_slot_.end()) {
    return;
  }

  llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
  if (cur == nullptr || cur->getTerminator() != nullptr) {
    return;
  }

  llvm::Function* F = cur->getParent();
  llvm::Type* i64 = theBuilder->getInt64Ty();
  llvm::Value* zero = llvm::ConstantInt::get(i64, 0);
  llvm::Value* one = llvm::ConstantInt::get(i64, 1);
  llvm::Value* capv = llvm::ConstantInt::get(i64, cap_it->second);
  llvm::AllocaInst* pending_count_slot = count_it->second;
  llvm::Value* pending_count = theBuilder->CreateLoad(i64, pending_count_slot);
  llvm::Value* over_cap = theBuilder->CreateICmpUGT(pending_count, capv);
  llvm::Value* start = theBuilder->CreateSelect(
    over_cap,
    theBuilder->CreateSub(pending_count, capv),
    zero);
  llvm::AllocaInst* idx_slot = theBuilder->CreateAlloca(i64, nullptr, name + ".pending.commit.i");
  theBuilder->CreateStore(start, idx_slot);

  llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(*theContext, "resource_commit_hdr", F);
  llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, "resource_commit_body", F);
  llvm::BasicBlock* done_bb = llvm::BasicBlock::Create(*theContext, "resource_commit_done", F);
  theBuilder->CreateBr(hdr_bb);

  theBuilder->SetInsertPoint(hdr_bb);
  llvm::Value* i = theBuilder->CreateLoad(i64, idx_slot);
  llvm::Value* more = theBuilder->CreateICmpULT(i, pending_count);
  theBuilder->CreateCondBr(more, body_bb, done_bb);

  theBuilder->SetInsertPoint(body_bb);
  auto* ring_ty = llvm::cast<llvm::ArrayType>(arr_it->second->getAllocatedType());
  auto* pending_ty = llvm::cast<llvm::ArrayType>(pending_it->second->getAllocatedType());
  llvm::Value* pending_idx = theBuilder->CreateURem(i, capv);
  llvm::Value* head = theBuilder->CreateLoad(i64, head_it->second);
  llvm::Value* ring_idx = theBuilder->CreateURem(head, capv);
  move_bounded_ring_value(
    ring_ty,
    arr_it->second,
    ring_idx,
    pending_ty,
    pending_it->second,
    pending_idx,
    handle_family);
  theBuilder->CreateStore(theBuilder->CreateAdd(head, one), head_it->second);
  theBuilder->CreateStore(theBuilder->CreateAdd(i, one), idx_slot);
  theBuilder->CreateBr(hdr_bb);

  theBuilder->SetInsertPoint(done_bb);
  theBuilder->CreateStore(zero, pending_count_slot);
}

void
StyioToLLVM::emit_bounded_ring_pending_commits() {
  std::vector<std::string> names;
  names.reserve(bounded_ring_pending_count_slot_.size());
  for (const auto& kv : bounded_ring_pending_count_slot_) {
    names.push_back(kv.first);
  }
  for (const std::string& name : names) {
    emit_bounded_ring_pending_commit(name);
    llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
    if (cur == nullptr || cur->getTerminator() != nullptr) {
      break;
    }
  }
}

/*
  FlexBind

  Other Names For Search:
  - Flexible Binding
  - Mutable Variable
  - Mutable Assignment
*/
llvm::Value*
StyioToLLVM::toLLVMIR(SGFlexBind* node) {
  std::string varname = node->var->var_name->as_str();
  llvm::AllocaInst* variable;
  bool is_existing_slot = false;

  auto capture = callable_capture_globals_.find(varname);
  if (active_callable_capture_names_.contains(varname)
      && capture != callable_capture_globals_.end()) {
    llvm::Value* next_value = node->value->toLLVMIR(this);
    if (next_value->getType() != capture->second->getValueType()) {
      throw StyioTypeError(
        "affine capture `" + varname
        + "` initializer does not match its program-static storage type"
      );
    }
    theBuilder->CreateStore(next_value, capture->second);
    return capture->second;
  }

  if (named_values.contains(varname)) {
    /* ERROR */
    throw StyioTypeError(
      std::string("immutable binding cannot be reassigned with `=`: ") + varname);
  }

  if (release_tracked_file_handle_binding(varname)) {
    emit_runtime_error_guard_return();
  }

  if (bounded_ring_head_slot_.contains(varname)) {
    llvm::AllocaInst* arr = mutable_variables[varname];
    std::uint64_t cap = bounded_ring_capacity_[varname];
    llvm::Type* i64 = theBuilder->getInt64Ty();
    auto* arrTy = llvm::cast<llvm::ArrayType>(arr->getAllocatedType());
    llvm::Type* elem_ty = arrTy->getElementType();
    std::optional<StyioValueFamily> handle_family;
    auto handle_family_it = bounded_ring_handle_family_.find(varname);
    if (handle_family_it != bounded_ring_handle_family_.end()) {
      handle_family = handle_family_it->second;
    }
    llvm::Value* next_value = node->value->toLLVMIR(this);
    next_value = styio_coerce_bounded_ring_value(next_value, elem_ty, theBuilder.get());
    if (node->pending_resource_write
        && bounded_ring_pending_slot_.contains(varname)
        && bounded_ring_pending_count_slot_.contains(varname)) {
      llvm::AllocaInst* pending = bounded_ring_pending_slot_[varname];
      llvm::AllocaInst* pending_count_slot = bounded_ring_pending_count_slot_[varname];
      auto* pending_ty = llvm::cast<llvm::ArrayType>(pending->getAllocatedType());
      llvm::Value* pending_count = theBuilder->CreateLoad(i64, pending_count_slot);
      llvm::Value* idx = theBuilder->CreateURem(
        pending_count,
        llvm::ConstantInt::get(i64, cap));
      store_bounded_ring_value(pending_ty, pending, idx, next_value, handle_family);
      theBuilder->CreateStore(
        theBuilder->CreateAdd(pending_count, llvm::ConstantInt::get(i64, 1)),
        pending_count_slot);
      return pending;
    }

    llvm::AllocaInst* headSlot = bounded_ring_head_slot_[varname];
    llvm::Value* head = theBuilder->CreateLoad(i64, headSlot);
    llvm::Value* idx = theBuilder->CreateURem(head, llvm::ConstantInt::get(i64, cap));
    store_bounded_ring_value(arrTy, arr, idx, next_value, handle_family);
    llvm::Value* next_head = theBuilder->CreateAdd(head, llvm::ConstantInt::get(i64, 1));
    theBuilder->CreateStore(next_head, headSlot);
    return arr;
  }

  if (node->var->is_dynamic_slot) {
    if (mutable_variables.contains(varname)) {
      variable = mutable_variables[varname];
      is_existing_slot = true;
    }
    else {
      variable = create_entry_alloca(dynamic_cell_type(), varname);
      init_dynamic_slot_undef(variable);
      mutable_variables[varname] = variable;
      dynamic_variable_names_.insert(varname);
      register_dynamic_slot_for_raii(variable);
    }

    llvm::Value* next_value = node->value->toLLVMIR(this);
    DynamicSlotPayload payload = dynamic_slot_payload_for_value(node->value, next_value);
    const StyioDataType& declared_type = node->var->var_type->data_type;
    if (dynamic_slot_declared_type_controls_payload(declared_type)) {
      payload = dynamic_slot_payload_for_type(declared_type, next_value);
    }

    if (is_existing_slot) {
      release_dynamic_slot_contents(variable);
    }
    store_dynamic_slot(variable, payload.tag, payload.i64v, payload.f64v, payload.ptrv);
    forget_dynamic_slot_payload_ownership(next_value, payload.tag);
    return variable;
  }

  if (mutable_variables.contains(varname)) {
    variable = mutable_variables[varname];
    is_existing_slot = true;
  }
  else {
    llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
    llvm::BasicBlock* ent = &F->getEntryBlock();
    llvm::IRBuilder<> prealloc(ent, ent->getFirstInsertionPt());
    variable = prealloc.CreateAlloca(
      node->toLLVMType(this),
      nullptr,
      varname.c_str()
    );

    mutable_variables[varname] = variable;
  }

  llvm::Value* next_value = node->value->toLLVMIR(this);
  const bool is_string_slot =
    node->var->var_type->data_type.option == StyioDataTypeOption::String
    || (variable->getAllocatedType()->isPointerTy()
        && node->var->var_type->data_type.option
             != StyioDataTypeOption::Func);

  theBuilder->CreateStore(next_value, variable);
  if (is_string_slot) {
    if (!is_existing_slot) {
      register_cstr_slot_for_raii(variable);
    }
    forget_owned_cstr_temp(next_value);
  }

  return variable;
}

/*
  named_values stores only the llvm::value,
  if required, use llvm::value instead of load inst.
*/
llvm::Value*
StyioToLLVM::toLLVMIR(SGFinalBind* node) {
  std::string varname = node->var->var_name->as_str();
  auto capture = callable_capture_globals_.find(varname);
  if (active_callable_capture_names_.contains(varname)
      && capture != callable_capture_globals_.end()) {
    llvm::Value* value = node->value->toLLVMIR(this);
    if (value->getType() != capture->second->getValueType()) {
      throw StyioTypeError(
        "affine capture `" + varname
        + "` initializer does not match its program-static storage type"
      );
    }
    theBuilder->CreateStore(value, capture->second);
    return capture->second;
  }
  if (named_values.contains(varname)) {
    /* ERROR */
    throw StyioTypeError(
      std::string("immutable binding cannot be redefined with `:=`: ") + varname);
  }

  if (node->var->is_dynamic_slot) {
    llvm::AllocaInst* variable = create_entry_alloca(dynamic_cell_type(), varname);
    init_dynamic_slot_undef(variable);
    llvm::Value* value = node->value->toLLVMIR(this);
    DynamicSlotPayload payload = dynamic_slot_payload_for_value(node->value, value);
    const StyioDataType& declared_type = node->var->var_type->data_type;
    if (dynamic_slot_declared_type_controls_payload(declared_type)) {
      payload = dynamic_slot_payload_for_type(declared_type, value);
    }

    store_dynamic_slot(variable, payload.tag, payload.i64v, payload.f64v, payload.ptrv);
    forget_dynamic_slot_payload_ownership(value, payload.tag);
    mutable_variables[varname] = variable;
    dynamic_variable_names_.insert(varname);
    register_dynamic_slot_for_raii(variable);
    return variable;
  }

  if (auto cap = styio_bounded_ring_capacity(node->var->var_type->data_type)) {
    llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
    llvm::BasicBlock* ent = &F->getEntryBlock();
    llvm::IRBuilder<> prealloc(ent, ent->getFirstInsertionPt());
    llvm::Type* i64 = theBuilder->getInt64Ty();
    llvm::Type* elem_ty = styio_bounded_ring_element_llvm_type(node->var->var_type->data_type, theBuilder.get());
    std::optional<StyioValueFamily> handle_family =
      bounded_ring_handle_family_for_type(node->var->var_type->data_type);
    auto* arrTy = llvm::ArrayType::get(elem_ty, *cap);
    llvm::AllocaInst* arr = prealloc.CreateAlloca(arrTy, nullptr, varname);
    llvm::AllocaInst* head = prealloc.CreateAlloca(i64, nullptr, varname + ".head");
    llvm::AllocaInst* pending = prealloc.CreateAlloca(arrTy, nullptr, varname + ".pending");
    llvm::AllocaInst* pending_count = prealloc.CreateAlloca(i64, nullptr, varname + ".pending.count");
    prealloc.CreateStore(llvm::ConstantAggregateZero::get(arrTy), arr);
    prealloc.CreateStore(llvm::ConstantAggregateZero::get(arrTy), pending);
    prealloc.CreateStore(llvm::ConstantInt::get(i64, 0), head);
    prealloc.CreateStore(llvm::ConstantInt::get(i64, 0), pending_count);
    llvm::Value* val = node->value->toLLVMIR(this);
    val = styio_coerce_bounded_ring_value(val, elem_ty, theBuilder.get());
    llvm::Value* z = llvm::ConstantInt::get(i64, 0);
    store_bounded_ring_value(arrTy, arr, z, val, handle_family);
    theBuilder->CreateStore(llvm::ConstantInt::get(i64, 1), head);
    mutable_variables[varname] = arr;
    bounded_ring_head_slot_[varname] = head;
    bounded_ring_capacity_[varname] = *cap;
    bounded_ring_pending_slot_[varname] = pending;
    bounded_ring_pending_count_slot_[varname] = pending_count;
    if (handle_family.has_value()) {
      bounded_ring_handle_family_[varname] = *handle_family;
    }
    if (elem_ty->isPointerTy() || handle_family.has_value()) {
      register_bounded_ring_cstr_for_raii(varname);
    }
    return arr;
  }

  llvm::AllocaInst* variable = theBuilder->CreateAlloca(
    node->toLLVMType(this),
    nullptr,
    varname.c_str()
  );

  auto value = node->value->toLLVMIR(this);
  named_values[varname] = value;

  theBuilder->CreateStore(value, variable);
  if (variable->getAllocatedType()->isPointerTy()
      && node->var->var_type->data_type.option
           != StyioDataTypeOption::Func) {
    register_cstr_slot_for_raii(variable);
    forget_owned_cstr_temp(value);
  }

  return variable;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGFuncArg* node) {
  return theBuilder->getInt64(0);
}

llvm::Type*
StyioToLLVM::native_c_type_to_llvm(const styio::native::CType& type) {
  switch (type.kind) {
    case styio::native::CTypeKind::Void:
      return theBuilder->getVoidTy();
    case styio::native::CTypeKind::Bool:
      return theBuilder->getInt1Ty();
    case styio::native::CTypeKind::I8:
      return theBuilder->getInt8Ty();
    case styio::native::CTypeKind::I16:
      return theBuilder->getInt16Ty();
    case styio::native::CTypeKind::I32:
      return theBuilder->getInt32Ty();
    case styio::native::CTypeKind::I64:
      return theBuilder->getInt64Ty();
    case styio::native::CTypeKind::F32:
      return theBuilder->getFloatTy();
    case styio::native::CTypeKind::F64:
      return theBuilder->getDoubleTy();
    case styio::native::CTypeKind::Pointer:
      return llvm::PointerType::get(*theContext, 0);
  }
  return theBuilder->getInt64Ty();
}

void
StyioToLLVM::declare_native_extern_block(
  SGExternBlock* node,
  const std::vector<std::string>& export_symbols
) {
  auto loaded = styio::native::compile_and_load_block(node->abi, node->body, node->source_paths, export_symbols);
  if (loaded.handle != nullptr) {
    native_library_handles_.push_back(loaded.handle);
  }

  std::unordered_map<std::string, void*> symbol_by_name;
  for (const auto& symbol : loaded.symbols) {
    symbol_by_name[symbol.name] = symbol.address;
  }

  for (const auto& sig : loaded.functions) {
    std::vector<llvm::Type*> params;
    params.reserve(sig.params.size());
    for (const auto& param : sig.params) {
      params.push_back(native_c_type_to_llvm(param.type));
    }

    llvm::Type* ret_ty = native_c_type_to_llvm(sig.return_type);
    auto* fty = llvm::FunctionType::get(ret_ty, params, false);
    if (llvm::Function* existing = theModule->getFunction(sig.name)) {
      if (existing->getFunctionType() != fty) {
        throw StyioTypeError("native function `" + sig.name + "` conflicts with an existing function type");
      }
    }
    else {
      llvm::Function::Create(
        fty,
        llvm::GlobalValue::ExternalLinkage,
        sig.name,
        *theModule);
    }

    auto symbol_it = symbol_by_name.find(sig.name);
    if (symbol_it == symbol_by_name.end() || symbol_it->second == nullptr) {
      throw StyioTypeError("native function `" + sig.name + "` has no loaded address");
    }
    if (llvm::Error err = theORCJIT->defineAbsoluteSymbol(sig.name, symbol_it->second)) {
      std::string emsg;
      llvm::handleAllErrors(
        std::move(err),
        [&](const llvm::ErrorInfoBase& e) { emsg = e.message(); });
      throw StyioTypeError("failed to register native symbol `" + sig.name + "` with JIT: " + emsg);
    }
  }
}

void
StyioToLLVM::collect_sgfuncs_postorder(SGFunc* node, std::vector<SGFunc*>& out) {
  for (auto* stmt : node->func_block->stmts) {
    if (auto* inner = dynamic_cast<SGFunc*>(stmt)) {
      collect_sgfuncs_postorder(inner, out);
    }
  }
  out.push_back(node);
}

void
StyioToLLVM::declare_sgfunc(SGFunc* node) {
  std::string fname = node->func_name->as_str();
  if (theModule->getFunction(fname)) {
    return;
  }

  std::vector<llvm::Type*> llvm_func_args;
  for (auto* arg : node->func_args) {
    llvm_func_args.push_back(arg->toLLVMType(this));
  }

  llvm::Type* ret_ty = node->ret_type->toLLVMType(this);
  auto* fty = llvm::FunctionType::get(ret_ty, llvm_func_args, false);
  llvm::Function* F = llvm::Function::Create(
    fty,
    llvm::GlobalValue::ExternalLinkage,
    fname,
    *theModule);

  size_t i = 0;
  for (llvm::Argument& arg : F->args()) {
    arg.setName(node->func_args[i++]->id);
  }
  if (!node->specialization_content_digest.empty()) {
    F->addFnAttr(
      kCallableSpecializationDigestAttribute,
      node->specialization_content_digest);
  }
}

llvm::Value*
StyioToLLVM::coerce_for_return(llvm::Value* v, llvm::Type* want_ty) {
  if (!v || !want_ty) {
    return v;
  }
  if (v->getType() == want_ty) {
    return v;
  }
  if (want_ty->isDoubleTy() && v->getType()->isIntegerTy()) {
    return theBuilder->CreateSIToFP(v, want_ty);
  }
  if (want_ty->isIntegerTy() && v->getType()->isDoubleTy()) {
    return theBuilder->CreateFPToSI(v, want_ty);
  }
  if (want_ty->isIntegerTy(64) && v->getType()->isIntegerTy(1)) {
    return theBuilder->CreateZExt(v, want_ty);
  }
  if (want_ty->isIntegerTy(1) && v->getType()->isIntegerTy()) {
    return theBuilder->CreateICmpNE(
      v,
      llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(v->getType()), 0));
  }
  if (want_ty->isIntegerTy(1) && v->getType()->isFloatingPointTy()) {
    return theBuilder->CreateFCmpONE(v, llvm::ConstantFP::get(v->getType(), 0.0));
  }
  if (want_ty->isIntegerTy() && v->getType()->isIntegerTy()) {
    return theBuilder->CreateSExtOrTrunc(v, want_ty);
  }
  return v;
}

llvm::Value*
StyioToLLVM::truncate_for_main_ret(llvm::Value* v) {
  if (!v) {
    return theBuilder->getInt32(0);
  }
  if (v->getType()->isVoidTy()) {
    return theBuilder->getInt32(0);
  }
  if (v->getType()->isIntegerTy(32)) {
    return v;
  }
  if (v->getType()->isIntegerTy(1)) {
    return theBuilder->CreateZExt(v, theBuilder->getInt32Ty());
  }
  if (v->getType()->isIntegerTy(64)) {
    return theBuilder->CreateTrunc(v, theBuilder->getInt32Ty());
  }
  if (v->getType()->isDoubleTy()) {
    return theBuilder->CreateFPToSI(v, theBuilder->getInt32Ty());
  }
  return theBuilder->getInt32(0);
}

llvm::Value*
StyioToLLVM::default_runtime_return_value(llvm::Type* ret_ty) {
  if (ret_ty == nullptr || ret_ty->isVoidTy()) {
    return nullptr;
  }
  if (ret_ty->isFloatingPointTy()) {
    return llvm::ConstantFP::get(ret_ty, 0.0);
  }
  if (ret_ty->isPointerTy()) {
    return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ret_ty));
  }
  if (ret_ty->isIntegerTy()) {
    return llvm::ConstantInt::get(ret_ty, 0);
  }
  return llvm::Constant::getNullValue(ret_ty);
}

void
StyioToLLVM::emit_file_handle_slot_close(llvm::AllocaInst* slot) {
  if (slot == nullptr) {
    return;
  }
  llvm::FunctionCallee close_fn = theModule->getOrInsertFunction(
    "styio_file_close",
    llvm::FunctionType::get(
      theBuilder->getVoidTy(),
      {theBuilder->getInt64Ty()},
      false));
  llvm::Value* h = theBuilder->CreateLoad(theBuilder->getInt64Ty(), slot);
  theBuilder->CreateCall(close_fn, {h});
  theBuilder->CreateStore(theBuilder->getInt64(0), slot);
}

bool
StyioToLLVM::release_tracked_file_handle_binding(const std::string& var_name) {
  auto it = file_handle_var_slots_.find(var_name);
  if (it == file_handle_var_slots_.end()) {
    return false;
  }
  emit_file_handle_slot_close(it->second);
  file_handle_var_slots_.erase(it);
  return true;
}

void
StyioToLLVM::emit_runtime_error_guard_return_after_cleanup() {
  llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
  if (cur == nullptr || cur->getTerminator() != nullptr) {
    return;
  }

  llvm::Function* fn = cur->getParent();
  llvm::FunctionCallee has_error = theModule->getOrInsertFunction(
    "styio_runtime_has_error",
    llvm::FunctionType::get(theBuilder->getInt32Ty(), false));
  llvm::Value* has_err = theBuilder->CreateCall(has_error, {});
  llvm::Value* bad = theBuilder->CreateICmpNE(has_err, theBuilder->getInt32(0));

  llvm::BasicBlock* abort_bb = llvm::BasicBlock::Create(*theContext, "runtime_fail", fn);
  llvm::BasicBlock* cont_bb = llvm::BasicBlock::Create(*theContext, "runtime_ok", fn);
  theBuilder->CreateCondBr(bad, abort_bb, cont_bb);

  theBuilder->SetInsertPoint(abort_bb);
  llvm::Type* ret_ty = fn->getReturnType();
  if (ret_ty->isVoidTy()) {
    theBuilder->CreateRetVoid();
  }
  else {
    theBuilder->CreateRet(default_runtime_return_value(ret_ty));
  }

  theBuilder->SetInsertPoint(cont_bb);
}

void
StyioToLLVM::emit_runtime_error_guard_return() {
  llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
  if (cur == nullptr || cur->getTerminator() != nullptr) {
    return;
  }

  llvm::Function* fn = cur->getParent();
  llvm::FunctionCallee has_error = theModule->getOrInsertFunction(
    "styio_runtime_has_error",
    llvm::FunctionType::get(theBuilder->getInt32Ty(), false));
  llvm::Value* has_err = theBuilder->CreateCall(has_error, {});
  llvm::Value* bad = theBuilder->CreateICmpNE(has_err, theBuilder->getInt32(0));

  llvm::BasicBlock* abort_bb = llvm::BasicBlock::Create(*theContext, "runtime_fail", fn);
  llvm::BasicBlock* cont_bb = llvm::BasicBlock::Create(*theContext, "runtime_ok", fn);
  theBuilder->CreateCondBr(bad, abort_bb, cont_bb);

  theBuilder->SetInsertPoint(abort_bb);
  emit_active_scope_cleanup();
  llvm::Type* ret_ty = fn->getReturnType();
  if (ret_ty->isVoidTy()) {
    theBuilder->CreateRetVoid();
  }
  else {
    theBuilder->CreateRet(default_runtime_return_value(ret_ty));
  }

  theBuilder->SetInsertPoint(cont_bb);
}

llvm::Value*
StyioToLLVM::cstr_to_i64_checked(llvm::Value* v) {
  if (v == nullptr || !v->getType()->isPointerTy()) {
    return v;
  }
  llvm::FunctionCallee conv = theModule->getOrInsertFunction(
    "styio_cstr_to_i64",
    llvm::FunctionType::get(
      theBuilder->getInt64Ty(),
      {llvm::PointerType::get(*theContext, 0)},
      false));
  llvm::Value* out = theBuilder->CreateCall(conv, {v});
  if (resource_effect_operation_depth_ == 0) {
    emit_runtime_error_guard_return();
  }
  return out;
}

llvm::Value*
StyioToLLVM::cstr_to_f64_checked(llvm::Value* v) {
  if (v == nullptr || !v->getType()->isPointerTy()) {
    return v;
  }
  llvm::FunctionCallee conv = theModule->getOrInsertFunction(
    "styio_cstr_to_f64",
    llvm::FunctionType::get(
      theBuilder->getDoubleTy(),
      {llvm::PointerType::get(*theContext, 0)},
      false));
  llvm::Value* out = theBuilder->CreateCall(conv, {v});
  if (resource_effect_operation_depth_ == 0) {
    emit_runtime_error_guard_return();
  }
  return out;
}

void
StyioToLLVM::define_sgfunc_body(SGFunc* node) {
  std::string fname = node->func_name->as_str();
  llvm::Function* F = theModule->getFunction(fname);
  if (!F) {
    return;
  }

  if (!F->empty() && F->getEntryBlock().getTerminator()) {
    return;
  }

  auto saved_mut = mutable_variables;
  auto saved_named = named_values;
  auto saved_ring_h = bounded_ring_head_slot_;
  auto saved_ring_c = bounded_ring_capacity_;
  auto saved_ring_pending = bounded_ring_pending_slot_;
  auto saved_ring_pending_count = bounded_ring_pending_count_slot_;
  auto saved_ring_handle_family = bounded_ring_handle_family_;
  auto saved_bounded_ring_cstr_scopes = bounded_ring_cstr_scope_stack_;
  auto saved_dyn_names = dynamic_variable_names_;
  auto saved_list_names = list_slot_names_;
  auto saved_file_scopes = file_handle_scope_stack_;
  auto saved_cstr_scopes = cstr_slot_scope_stack_;
  auto saved_dynamic_scopes = dynamic_slot_scope_stack_;
  auto saved_owned_cstr = owned_cstr_temps_;
  auto saved_owned_resource = owned_resource_temps_;
  auto saved_active_captures = active_callable_capture_names_;
  mutable_variables.clear();
  named_values.clear();
  bounded_ring_head_slot_.clear();
  bounded_ring_capacity_.clear();
  bounded_ring_pending_slot_.clear();
  bounded_ring_pending_count_slot_.clear();
  bounded_ring_handle_family_.clear();
  bounded_ring_cstr_scope_stack_.clear();
  dynamic_variable_names_.clear();
  list_slot_names_.clear();
  file_handle_scope_stack_.clear();
  cstr_slot_scope_stack_.clear();
  dynamic_slot_scope_stack_.clear();
  owned_cstr_temps_.clear();
  owned_resource_temps_.clear();
  active_callable_capture_names_.clear();
  active_callable_capture_names_.insert(
    node->capture_names.begin(),
    node->capture_names.end());

  llvm::BasicBlock* block = llvm::BasicBlock::Create(
    *theContext,
    (fname + "_entry"),
    F);
  theBuilder->SetInsertPoint(block);
  push_file_handle_scope();

  size_t ai = 0;
  for (llvm::Argument& arg : F->args()) {
    SGFuncArg* sg = node->func_args[ai++];
    llvm::Type* at = sg->arg_type->toLLVMType(this);
    llvm::AllocaInst* slot = theBuilder->CreateAlloca(
      at,
      nullptr,
      std::string(arg.getName()));
    theBuilder->CreateStore(&arg, slot);
    mutable_variables[std::string(arg.getName())] = slot;
  }

  for (auto* stmt : node->func_block->stmts) {
    if (dynamic_cast<SGFunc*>(stmt)) {
      stmt->toLLVMIR(this);
      continue;
    }

    stmt->toLLVMIR(this);
    if (theBuilder->GetInsertBlock()->getTerminator()) {
      break;
    }
  }

  llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
  if (cur && !cur->getTerminator()) {
    llvm::Type* rt = node->ret_type->toLLVMType(this);
    pop_file_handle_scope();
    theBuilder->CreateRet(default_runtime_return_value(rt));
  }

  mutable_variables = std::move(saved_mut);
  named_values = std::move(saved_named);
  bounded_ring_head_slot_ = std::move(saved_ring_h);
  bounded_ring_capacity_ = std::move(saved_ring_c);
  bounded_ring_pending_slot_ = std::move(saved_ring_pending);
  bounded_ring_pending_count_slot_ = std::move(saved_ring_pending_count);
  bounded_ring_handle_family_ = std::move(saved_ring_handle_family);
  bounded_ring_cstr_scope_stack_ = std::move(saved_bounded_ring_cstr_scopes);
  dynamic_variable_names_ = std::move(saved_dyn_names);
  list_slot_names_ = std::move(saved_list_names);
  file_handle_scope_stack_ = std::move(saved_file_scopes);
  cstr_slot_scope_stack_ = std::move(saved_cstr_scopes);
  dynamic_slot_scope_stack_ = std::move(saved_dynamic_scopes);
  owned_cstr_temps_ = std::move(saved_owned_cstr);
  owned_resource_temps_ = std::move(saved_owned_resource);
  active_callable_capture_names_ =
    std::move(saved_active_captures);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGFunc* node) {
  /* Bodies are emitted from SGMainEntry after a full declare pass. */
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGCall* node) {
  if (node->is_indirect()) {
    if (!styio_is_callable_type(node->callable_type)) {
      throw StyioTypeError(
        "indirect call requires a canonical callable type");
    }
    const std::vector<StyioDataType> param_types =
      styio_callable_param_types(node->callable_type);
    const StyioDataType result_type =
      styio_callable_result_type(node->callable_type);
    if (node->func_args.size() != param_types.size()) {
      throw StyioTypeError(
        "indirect callable of type `" + node->callable_type.name
        + "` expects " + std::to_string(param_types.size())
        + " argument(s), got "
        + std::to_string(node->func_args.size()));
    }

    std::vector<llvm::Type*> llvm_param_types;
    llvm_param_types.reserve(param_types.size());
    for (const auto& type : param_types) {
      llvm_param_types.push_back(toLLVMType(type));
    }
    llvm::Type* llvm_result_type =
      toLLVMType(result_type);
    auto* function_type = llvm::FunctionType::get(
      llvm_result_type,
      llvm_param_types,
      false);

    llvm::Value* callee =
      node->indirect_callee->toLLVMIR(this);
    if (callee == nullptr || !callee->getType()->isPointerTy()) {
      throw StyioTypeError(
        "indirect callable value did not lower to a function pointer");
    }

    std::vector<llvm::Value*> args;
    args.reserve(node->func_args.size());
    for (std::size_t i = 0; i < node->func_args.size(); ++i) {
      llvm::Value* value =
        node->func_args[i]->toLLVMIR(this);
      llvm::Type* target = llvm_param_types[i];
      if (target->isDoubleTy()
          && value->getType()->isPointerTy()) {
        value = cstr_to_f64_checked(value);
      }
      else if (target->isDoubleTy()
               && value->getType()->isIntegerTy()) {
        value = theBuilder->CreateSIToFP(value, target);
      }
      else if (target->isIntegerTy()
               && value->getType()->isDoubleTy()) {
        value = theBuilder->CreateFPToSI(value, target);
      }
      else if (target->isIntegerTy()
               && value->getType()->isPointerTy()) {
        value = cstr_to_i64_checked(value);
        if (target->getIntegerBitWidth() != 64) {
          value = theBuilder->CreateIntCast(
            value, target, true);
        }
      }
      else if (target->isIntegerTy()
               && value->getType()->isIntegerTy()
               && target != value->getType()) {
        value = theBuilder->CreateIntCast(
          value, target, true);
      }
      else if (target->isFloatTy()
               && value->getType()->isDoubleTy()) {
        value = theBuilder->CreateFPTrunc(value, target);
      }
      else if (target->isDoubleTy()
               && value->getType()->isFloatTy()) {
        value = theBuilder->CreateFPExt(value, target);
      }
      else if (target != value->getType()) {
        throw StyioTypeError(
          "indirect callable argument " + std::to_string(i)
          + " cannot be lowered from `" + param_types[i].name
          + "` to its declared ABI type");
      }
      args.push_back(value);
    }

    llvm::Value* output =
      theBuilder->CreateCall(function_type, callee, args);
    emit_runtime_error_guard_return();
    return output;
  }

  std::string fname = node->func_name->as_str();
  auto builtin_list_family_from_suffix = [&](const std::string& suffix) -> StyioValueFamily {
    if (suffix == "bool") {
      return StyioValueFamily::Bool;
    }
    if (suffix == "char") {
      return StyioValueFamily::Char;
    }
    if (suffix == "f64") {
      return StyioValueFamily::Float;
    }
    if (suffix == "cstr") {
      return StyioValueFamily::String;
    }
    if (suffix == "list") {
      return StyioValueFamily::ListHandle;
    }
    if (suffix == "dict") {
      return StyioValueFamily::DictHandle;
    }
    if (suffix == "matrix") {
      return StyioValueFamily::MatrixHandle;
    }
    return StyioValueFamily::Integer;
  };
  auto builtin_list_value_type = [&](StyioValueFamily family) -> llvm::Type* {
    if (family == StyioValueFamily::Char) {
      return theBuilder->getInt8Ty();
    }
    if (family == StyioValueFamily::Float) {
      return theBuilder->getDoubleTy();
    }
    if (family == StyioValueFamily::String) {
      return llvm::PointerType::get(*theContext, 0);
    }
    return theBuilder->getInt64Ty();
  };
  auto coerce_builtin_list_value = [&](llvm::Value* raw, StyioValueFamily family) -> llvm::Value* {
    return styio_coerce_collection_value(
      raw,
      family,
      theBuilder.get(),
      "runtime list operation");
  };

  if (fname == "__styio_list_pop") {
    if (node->func_args.size() != 1) {
      throw StyioTypeError(
        "runtime list pop expects 1 argument, got "
        + std::to_string(node->func_args.size()));
    }
    llvm::FunctionCallee pop_fn = theModule->getOrInsertFunction(
      "styio_list_pop",
      llvm::FunctionType::get(theBuilder->getVoidTy(), {theBuilder->getInt64Ty()}, false));
    llvm::Value* list_raw = node->func_args[0]->toLLVMIR(this);
    llvm::Value* list = list_raw;
    if (!list->getType()->isIntegerTy(64)) {
      list = theBuilder->CreateSExtOrTrunc(list, theBuilder->getInt64Ty());
    }
    theBuilder->CreateCall(pop_fn, {list});
    free_owned_resource_temp_if_tracked(list_raw);
    return theBuilder->getInt64(0);
  }

  if (fname == "__styio_string_lines") {
    if (node->func_args.size() != 1) {
      throw StyioTypeError(
        "runtime string.lines expects 1 argument, got "
        + std::to_string(node->func_args.size()));
    }
    llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
    llvm::FunctionCallee lines_fn = theModule->getOrInsertFunction(
      "styio_string_lines",
      llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr}, false));
    llvm::Value* raw = node->func_args[0]->toLLVMIR(this);
    if (!raw->getType()->isPointerTy()) {
      throw StyioTypeError("runtime string.lines requires a string argument");
    }
    llvm::Value* out = theBuilder->CreateCall(lines_fn, {raw});
    free_owned_cstr_temp_if_tracked(raw);
    track_owned_resource_temp(out, TempResourceKind::List);
    return out;
  }

  if (fname == "__styio_list_range_i64") {
    if (node->func_args.size() != 3) {
      throw StyioTypeError(
        "runtime range list expects 3 arguments, got "
        + std::to_string(node->func_args.size()));
    }

    auto coerce_i64 = [&](llvm::Value* raw) -> llvm::Value*
    {
      if (raw->getType()->isIntegerTy(64)) {
        return raw;
      }
      if (raw->getType()->isIntegerTy()) {
        return theBuilder->CreateSExtOrTrunc(raw, theBuilder->getInt64Ty());
      }
      throw StyioTypeError("runtime range list requires integer arguments");
    };

    llvm::Value* start = coerce_i64(node->func_args[0]->toLLVMIR(this));
    llvm::Value* end = coerce_i64(node->func_args[1]->toLLVMIR(this));
    llvm::Value* step = coerce_i64(node->func_args[2]->toLLVMIR(this));

    llvm::IntegerType* i64t = theBuilder->getInt64Ty();
    llvm::FunctionCallee new_fn = theModule->getOrInsertFunction(
      "styio_list_new_i64",
      llvm::FunctionType::get(i64t, {}, false));
    llvm::FunctionCallee push_fn = theModule->getOrInsertFunction(
      "styio_list_push_i64",
      llvm::FunctionType::get(theBuilder->getVoidTy(), {i64t, i64t}, false));
    llvm::Value* list = theBuilder->CreateCall(new_fn, {});

    llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
    llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(*theContext, "range_list_hdr", F);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, "range_list_body", F);
    llvm::BasicBlock* step_bb = llvm::BasicBlock::Create(*theContext, "range_list_step", F);
    llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "range_list_exit", F);
    llvm::AllocaInst* cur_slot = theBuilder->CreateAlloca(i64t, nullptr, "range.list.cur");
    theBuilder->CreateStore(start, cur_slot);
    theBuilder->CreateBr(hdr_bb);

    theBuilder->SetInsertPoint(hdr_bb);
    llvm::Value* cur = theBuilder->CreateLoad(i64t, cur_slot);
    llvm::Value* zero = theBuilder->getInt64(0);
    llvm::Value* is_zero = theBuilder->CreateICmpEQ(step, zero);
    llvm::Value* is_pos = theBuilder->CreateICmpSGT(step, zero);
    llvm::Value* pos_go = theBuilder->CreateICmpSLE(cur, end);
    llvm::Value* neg_go = theBuilder->CreateICmpSGE(cur, end);
    llvm::Value* non_zero_go = theBuilder->CreateSelect(is_pos, pos_go, neg_go);
    llvm::Value* go = theBuilder->CreateSelect(is_zero, llvm::ConstantInt::getFalse(*theContext), non_zero_go);
    theBuilder->CreateCondBr(go, body_bb, exit_bb);

    theBuilder->SetInsertPoint(body_bb);
    llvm::Value* body_cur = theBuilder->CreateLoad(i64t, cur_slot);
    theBuilder->CreateCall(push_fn, {list, body_cur});
    theBuilder->CreateBr(step_bb);

    theBuilder->SetInsertPoint(step_bb);
    llvm::Value* next = theBuilder->CreateAdd(theBuilder->CreateLoad(i64t, cur_slot), step);
    theBuilder->CreateStore(next, cur_slot);
    theBuilder->CreateBr(hdr_bb);

    theBuilder->SetInsertPoint(exit_bb);
    track_owned_resource_temp(list, TempResourceKind::List);
    return list;
  }

  bool is_builtin_list_push = false;
  bool is_builtin_list_insert = false;
  std::string builtin_suffix;
  if (fname.rfind("__styio_list_push_", 0) == 0) {
    is_builtin_list_push = true;
    builtin_suffix = fname.substr(std::string("__styio_list_push_").size());
  }
  else if (fname.rfind("__styio_list_insert_", 0) == 0) {
    is_builtin_list_insert = true;
    builtin_suffix = fname.substr(std::string("__styio_list_insert_").size());
  }
  if (is_builtin_list_push || is_builtin_list_insert) {
    const bool has_index = is_builtin_list_insert;
    const size_t expected_args = has_index ? 3 : 2;
    if (node->func_args.size() != expected_args) {
      throw StyioTypeError(
        std::string(has_index ? "runtime list insert" : "runtime list push")
        + " expects " + std::to_string(expected_args) + " argument(s), got "
        + std::to_string(node->func_args.size()));
    }

    StyioValueFamily value_family = builtin_list_family_from_suffix(builtin_suffix);
    llvm::Type* value_type = builtin_list_value_type(value_family);
    llvm::FunctionCallee list_fn = theModule->getOrInsertFunction(
      std::string(has_index ? "styio_list_insert_" : "styio_list_push_") + builtin_suffix,
      llvm::FunctionType::get(
        theBuilder->getVoidTy(),
        has_index
          ? std::vector<llvm::Type*>{theBuilder->getInt64Ty(), theBuilder->getInt64Ty(), value_type}
          : std::vector<llvm::Type*>{theBuilder->getInt64Ty(), value_type},
        false));

    llvm::Value* list_raw = node->func_args[0]->toLLVMIR(this);
    llvm::Value* list = list_raw;
    if (!list->getType()->isIntegerTy(64)) {
      list = theBuilder->CreateSExtOrTrunc(list, theBuilder->getInt64Ty());
    }

    llvm::Value* index = nullptr;
    if (has_index) {
      index = node->func_args[1]->toLLVMIR(this);
      if (!index->getType()->isIntegerTy(64)) {
        index = theBuilder->CreateSExtOrTrunc(index, theBuilder->getInt64Ty());
      }
    }

    llvm::Value* value_raw = node->func_args[has_index ? 2 : 1]->toLLVMIR(this);
    llvm::Value* value = coerce_builtin_list_value(value_raw, value_family);
    if (has_index) {
      theBuilder->CreateCall(list_fn, {list, index, value});
    }
    else {
      theBuilder->CreateCall(list_fn, {list, value});
    }
    if (value_family == StyioValueFamily::String) {
      free_owned_cstr_temp_if_tracked(value_raw);
    }
    else if (value_family == StyioValueFamily::ListHandle
             || value_family == StyioValueFamily::DictHandle
             || value_family == StyioValueFamily::MatrixHandle) {
      free_owned_resource_temp_if_tracked(value_raw);
    }
    free_owned_resource_temp_if_tracked(list_raw);
    return theBuilder->getInt64(0);
  }

  if (fname.rfind("__styio_matrix_", 0) == 0) {
    std::string runtime_name = fname.substr(2);
    llvm::Type* i64 = theBuilder->getInt64Ty();
    llvm::Type* f64 = theBuilder->getDoubleTy();
    auto coerce_i64 = [&](llvm::Value* v) -> llvm::Value* {
      if (v->getType()->isIntegerTy(64)) {
        return v;
      }
      if (v->getType()->isDoubleTy()) {
        return theBuilder->CreateFPToSI(v, i64);
      }
      if (v->getType()->isIntegerTy()) {
        return theBuilder->CreateSExtOrTrunc(v, i64);
      }
      return theBuilder->getInt64(0);
    };
    auto coerce_f64 = [&](llvm::Value* v) -> llvm::Value* {
      if (v->getType()->isDoubleTy()) {
        return v;
      }
      if (v->getType()->isIntegerTy()) {
        return theBuilder->CreateSIToFP(v, f64);
      }
      return llvm::ConstantFP::get(f64, 0.0);
    };
    auto emit_call = [&](llvm::Type* ret_ty, std::vector<llvm::Type*> params) -> llvm::Value* {
      if (node->func_args.size() != params.size()) {
        throw StyioTypeError(
          "matrix runtime helper `" + runtime_name + "` expects "
          + std::to_string(params.size()) + " argument(s), got "
          + std::to_string(node->func_args.size()));
      }
      llvm::FunctionCallee fn = theModule->getOrInsertFunction(
        runtime_name,
        llvm::FunctionType::get(ret_ty, params, false));
      std::vector<llvm::Value*> args;
      args.reserve(params.size());
      for (size_t i = 0; i < params.size(); ++i) {
        llvm::Value* raw = node->func_args[i]->toLLVMIR(this);
        args.push_back(params[i]->isDoubleTy() ? coerce_f64(raw) : coerce_i64(raw));
      }
      return theBuilder->CreateCall(fn, args);
    };
    auto matrix_result = [&](llvm::Value* out) -> llvm::Value* {
      track_owned_resource_temp(out, TempResourceKind::Matrix);
      return out;
    };
    auto list_result = [&](llvm::Value* out) -> llvm::Value* {
      track_owned_resource_temp(out, TempResourceKind::List);
      return out;
    };

    if (runtime_name == "styio_matrix_new_i64"
        || runtime_name == "styio_matrix_new_f64") {
      return matrix_result(emit_call(i64, {i64, i64}));
    }
    if (runtime_name == "styio_matrix_identity_i64"
        || runtime_name == "styio_matrix_identity_f64"
        || runtime_name == "styio_matrix_clone_i64"
        || runtime_name == "styio_matrix_clone_f64"
        || runtime_name == "styio_matrix_transpose_i64"
        || runtime_name == "styio_matrix_transpose_f64") {
      return matrix_result(emit_call(i64, {i64}));
    }
    if (runtime_name == "styio_matrix_rows"
        || runtime_name == "styio_matrix_cols") {
      return emit_call(i64, {i64});
    }
    if (runtime_name == "styio_matrix_shape") {
      return list_result(emit_call(i64, {i64}));
    }
    if (runtime_name == "styio_matrix_get_i64") {
      return emit_call(i64, {i64, i64, i64});
    }
    if (runtime_name == "styio_matrix_get_f64") {
      return emit_call(f64, {i64, i64, i64});
    }
    if (runtime_name == "styio_matrix_set_i64") {
      emit_call(theBuilder->getVoidTy(), {i64, i64, i64, i64});
      return theBuilder->getInt64(0);
    }
    if (runtime_name == "styio_matrix_set_f64") {
      emit_call(theBuilder->getVoidTy(), {i64, i64, i64, f64});
      return theBuilder->getInt64(0);
    }
    if (runtime_name == "styio_matrix_add_i64"
        || runtime_name == "styio_matrix_add_f64"
        || runtime_name == "styio_matrix_sub_i64"
        || runtime_name == "styio_matrix_sub_f64"
        || runtime_name == "styio_matrix_hadamard_i64"
        || runtime_name == "styio_matrix_hadamard_f64"
        || runtime_name == "styio_matrix_matmul_i64"
        || runtime_name == "styio_matrix_matmul_f64") {
      return matrix_result(emit_call(i64, {i64, i64}));
    }
    if (runtime_name == "styio_matrix_scale_i64") {
      return matrix_result(emit_call(i64, {i64, i64}));
    }
    if (runtime_name == "styio_matrix_scale_f64") {
      return matrix_result(emit_call(i64, {i64, f64}));
    }
    if (runtime_name == "styio_matrix_dot_i64") {
      return emit_call(i64, {i64, i64});
    }
    if (runtime_name == "styio_matrix_dot_f64") {
      return emit_call(f64, {i64, i64});
    }
    if (runtime_name == "styio_matrix_sum_i64") {
      return emit_call(i64, {i64});
    }
    if (runtime_name == "styio_matrix_sum_f64"
        || runtime_name == "styio_matrix_norm") {
      return emit_call(f64, {i64});
    }
  }

  llvm::Function* callee = theModule->getFunction(fname);
  if (!callee) {
    throw StyioTypeError("unknown function `" + fname + "`");
  }

  llvm::FunctionType* ft = callee->getFunctionType();
  if (node->func_args.size() != ft->getNumParams()) {
    throw StyioTypeError(
      "function `" + fname + "` expects "
      + std::to_string(ft->getNumParams()) + " argument(s), got "
      + std::to_string(node->func_args.size()));
  }

  std::vector<llvm::Value*> args;
  for (size_t i = 0; i < node->func_args.size(); ++i) {
    llvm::Value* av = node->func_args[i]->toLLVMIR(this);
    llvm::Type* pt = ft->getParamType(i);
    if (pt->isDoubleTy() && av->getType()->isPointerTy()) {
      av = cstr_to_f64_checked(av);
    }
    else if (pt->isDoubleTy() && av->getType()->isIntegerTy()) {
      av = theBuilder->CreateSIToFP(av, pt);
    }
    else if (pt->isIntegerTy() && av->getType()->isDoubleTy()) {
      av = theBuilder->CreateFPToSI(av, pt);
    }
    else if (pt->isIntegerTy() && av->getType()->isPointerTy()) {
      av = cstr_to_i64_checked(av);
      if (pt->getIntegerBitWidth() != 64) {
        av = theBuilder->CreateIntCast(av, pt, true);
      }
    }
    else if (pt->isIntegerTy() && av->getType()->isIntegerTy() && pt != av->getType()) {
      av = theBuilder->CreateIntCast(av, pt, true);
    }
    else if (pt->isFloatTy() && av->getType()->isDoubleTy()) {
      av = theBuilder->CreateFPTrunc(av, pt);
    }
    else if (pt->isDoubleTy() && av->getType()->isFloatTy()) {
      av = theBuilder->CreateFPExt(av, pt);
    }
    args.push_back(av);
  }

  llvm::Value* out = theBuilder->CreateCall(ft, callee, args);
  emit_runtime_error_guard_return();
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGExportDecl* node) {
  (void)node;
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGExternBlock* node) {
  (void)node;
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGReturn* node) {
  llvm::Value* v = node->expr->toLLVMIR(this);
  llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
  if (cur == nullptr || cur->getTerminator() != nullptr) {
    return v;
  }
  llvm::Function* fn = cur->getParent();
  if (fn == nullptr || fn->getReturnType()->isVoidTy()) {
    return theBuilder->CreateRetVoid();
  }
  llvm::Value* ret = coerce_for_return(v, fn->getReturnType());
  if (ret == nullptr) {
    ret = default_runtime_return_value(fn->getReturnType());
  }
  if (emit_active_file_handle_cleanup()) {
    emit_runtime_error_guard_return_after_cleanup();
    if (theBuilder->GetInsertBlock()->getTerminator()) {
      return ret;
    }
  }
  return theBuilder->CreateRet(ret);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGBlock* node) {
  styio::ir::StyioIRVerifierOptions verifier_options;
  verifier_options.defer_unresolved_loop_control = !loop_stack_.empty();
  styio::ir::require_verified_styio_ir(node, verifier_options);
  push_file_handle_scope();
  llvm::Value* last = nullptr;
  StyioIR* last_ir = nullptr;
  for (auto const& s : node->stmts) {
    last = s->toLLVMIR(this);
    last_ir = s;
    if (theBuilder->GetInsertBlock()->getTerminator()) {
      break;
    }
  }
  llvm::BasicBlock* bcur = theBuilder->GetInsertBlock();
  if (bcur && !bcur->getTerminator()) {
    const bool scope_has_dynamic_slots =
      !dynamic_slot_scope_stack_.empty() && !dynamic_slot_scope_stack_.back().empty();
    const bool scope_has_cstr_slots =
      !cstr_slot_scope_stack_.empty() && !cstr_slot_scope_stack_.back().empty();
    if (last != nullptr && last_ir != nullptr && scope_has_dynamic_slots) {
      if (ir_yields_list_handle(last_ir)) {
        last = clone_resource_handle_for_runtime_owner(last, StyioValueFamily::ListHandle);
        track_owned_resource_temp(last, TempResourceKind::List);
      }
      else if (ir_yields_dict_handle(last_ir)) {
        last = clone_resource_handle_for_runtime_owner(last, StyioValueFamily::DictHandle);
        track_owned_resource_temp(last, TempResourceKind::Dict);
      }
      else if (ir_yields_matrix_handle(last_ir)) {
        last = clone_resource_handle_for_runtime_owner(last, StyioValueFamily::MatrixHandle);
        track_owned_resource_temp(last, TempResourceKind::Matrix);
      }
    }
    if (last != nullptr && scope_has_cstr_slots && last->getType()->isPointerTy()) {
      last = clone_cstr_for_runtime_owner(last);
      track_owned_cstr_temp(last);
    }
    pop_file_handle_scope();
  }
  else {
    discard_file_handle_scope_metadata();
  }
  return last ? last : theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGEntry* node) {
  styio::ir::require_verified_styio_ir(node);
  llvm::Value* last = nullptr;
  for (auto* stmt : node->stmts) {
    last = stmt->toLLVMIR(this);
    if (theBuilder->GetInsertBlock()->getTerminator()) {
      break;
    }
  }
  return last ? last : theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGMainEntry* node) {
  styio::ir::require_verified_styio_ir(node);
  std::vector<std::string> export_symbols;
  for (auto* s : node->stmts) {
    if (auto* export_decl = dynamic_cast<SGExportDecl*>(s)) {
      export_symbols.insert(export_symbols.end(), export_decl->symbols.begin(), export_decl->symbols.end());
    }
  }

  for (auto* s : node->stmts) {
    if (auto* extern_block = dynamic_cast<SGExternBlock*>(s)) {
      const std::vector<std::string>& active_export_symbols =
        extern_block->exported_symbols.empty()
          ? export_symbols
          : extern_block->exported_symbols;
      declare_native_extern_block(extern_block, active_export_symbols);
    }
  }

  std::vector<SGFunc*> ordered_funcs;
  for (auto* s : node->stmts) {
    if (auto* f = dynamic_cast<SGFunc*>(s)) {
      collect_sgfuncs_postorder(f, ordered_funcs);
    }
  }

  std::unordered_set<std::string> requested_captures;
  for (auto* function : ordered_funcs) {
    requested_captures.insert(
      function->capture_names.begin(),
      function->capture_names.end());
  }
  callable_capture_globals_.clear();
  for (auto* statement : node->stmts) {
    SGVar* variable = nullptr;
    if (auto* binding = dynamic_cast<SGFlexBind*>(statement)) {
      variable = binding->var;
    }
    else if (auto* binding =
               dynamic_cast<SGFinalBind*>(statement)) {
      variable = binding->var;
    }
    if (variable == nullptr
        || requested_captures.count(
             variable->var_name->as_str()) == 0) {
      continue;
    }
    const std::string& name =
      variable->var_name->as_str();
    if (callable_capture_globals_.count(name) != 0) {
      continue;
    }
    llvm::Type* type =
      variable->var_type->toLLVMType(this);
    auto* storage = new llvm::GlobalVariable(
      *theModule,
      type,
      false,
      llvm::GlobalValue::InternalLinkage,
      llvm::Constant::getNullValue(type),
      "__styio_capture." + name);
    callable_capture_globals_[name] = storage;
  }
  for (const auto& capture : requested_captures) {
    if (callable_capture_globals_.count(capture) == 0) {
      throw StyioTypeError(
        "affine capture `" + capture
        + "` has no top-level program-static binding"
      );
    }
  }
  active_callable_capture_names_.clear();

  for (auto* f : ordered_funcs) {
    declare_sgfunc(f);
  }

  for (auto* f : ordered_funcs) {
    define_sgfunc_body(f);
  }

  llvm::Function* main_func = llvm::Function::Create(
    llvm::FunctionType::get(theBuilder->getInt32Ty(), false),
    llvm::Function::ExternalLinkage,
    "main",
    *theModule);
  llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(*theContext, "main_entry", main_func);

  theBuilder->SetInsertPoint(entry_block);
  active_callable_capture_names_ = requested_captures;

  push_file_handle_scope();

  llvm::Value* last_main = nullptr;
  for (auto const& s : node->stmts) {
    if (dynamic_cast<SGFunc*>(s) || dynamic_cast<SGExportDecl*>(s) || dynamic_cast<SGExternBlock*>(s)) {
      continue;
    }

    last_main = s->toLLVMIR(this);
    if (theBuilder->GetInsertBlock()->getTerminator()) {
      break;
    }
  }

  llvm::BasicBlock* mcur = theBuilder->GetInsertBlock();
  if (mcur && !mcur->getTerminator()) {
    emit_bounded_ring_pending_commits();
    pop_file_handle_scope();
    theBuilder->CreateRet(truncate_for_main_ret(last_main));
  }
  active_callable_capture_names_.clear();

  return main_func;
}

llvm::Value*
StyioToLLVM::promote_to_cstr(llvm::Value* v) {
  llvm::PointerType* char_ptr = llvm::PointerType::get(*theContext, 0);
  if (v->getType()->isPointerTy()) {
    return v;
  }

  if (v->getType()->isIntegerTy(8)) {
    llvm::Value* wi = theBuilder->CreateSExtOrTrunc(v, theBuilder->getInt64Ty());
    llvm::FunctionCallee char_cstr = theModule->getOrInsertFunction(
      "styio_char_cstr",
      llvm::FunctionType::get(char_ptr, {theBuilder->getInt64Ty()}, false));
    return theBuilder->CreateCall(char_cstr, {wi});
  }

  if (v->getType()->isIntegerTy()) {
    llvm::Value* wi = v->getType()->isIntegerTy(64)
      ? v
      : theBuilder->CreateSExtOrTrunc(v, theBuilder->getInt64Ty());
    llvm::FunctionCallee i64c = theModule->getOrInsertFunction(
      "styio_i64_dec_cstr",
      llvm::FunctionType::get(char_ptr, {theBuilder->getInt64Ty()}, false));
    return theBuilder->CreateCall(i64c, {wi});
  }

  if (v->getType()->isDoubleTy()) {
    llvm::FunctionCallee f64c = theModule->getOrInsertFunction(
      "styio_f64_dec_cstr",
      llvm::FunctionType::get(char_ptr, {theBuilder->getDoubleTy()}, false));
    return theBuilder->CreateCall(f64c, {v});
  }

  return theBuilder->CreateGlobalStringPtr("", "styio_empty");
}

llvm::Value*
StyioToLLVM::evaluate_arm_block_value(SGBlock* b, bool mixed_phi) {
  llvm::IntegerType* i64t = theBuilder->getInt64Ty();
  for (auto* s : b->stmts) {
    if (auto* r = dynamic_cast<SGReturn*>(s)) {
      llvm::Value* v = r->expr->toLLVMIR(this);
      return mixed_phi ? promote_to_cstr(v) : v;
    }
    if (auto* m = dynamic_cast<SGMatch*>(s)) {
      if (m->repr_kind != SGMatchReprKind::Stmt) {
        llvm::Value* v = m->toLLVMIR(this);
        if (mixed_phi) {
          if (m->repr_kind == SGMatchReprKind::ExprMixed) {
            return v;
          }
          return promote_to_cstr(v);
        }
        return v;
      }
    }
    s->toLLVMIR(this);
    llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
    if (cur && cur->getTerminator()) {
      return nullptr;
    }
  }
  return llvm::ConstantInt::get(i64t, 0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGLoop* node) {
  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "styloop_exit", F);
  llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, "styloop_body", F);

  if (node->tag == SGLoopTag::Infinite) {
    theBuilder->CreateBr(body_bb);
    loop_stack_.push_back(LoopFrame{exit_bb, body_bb, file_handle_scope_stack_.size()});
    theBuilder->SetInsertPoint(body_bb);
    node->body->toLLVMIR(this);
    emit_bounded_ring_pending_commits();
    llvm::BasicBlock* bcur = theBuilder->GetInsertBlock();
    if (bcur && !bcur->getTerminator()) {
      theBuilder->CreateBr(body_bb);
    }
    theBuilder->SetInsertPoint(exit_bb);
    loop_stack_.pop_back();
    return nullptr;
  }

  llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(*theContext, "styloop_cond", F);
  theBuilder->CreateBr(cond_bb);
  theBuilder->SetInsertPoint(cond_bb);
  llvm::Value* cv = node->cond->toLLVMIR(this);
  llvm::Value* c = cv;
  if (!cv->getType()->isIntegerTy(1)) {
    c = theBuilder->CreateICmpNE(
      cv,
      llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(cv->getType()), 0));
  }
  theBuilder->CreateCondBr(c, body_bb, exit_bb);
  loop_stack_.push_back(LoopFrame{exit_bb, cond_bb, file_handle_scope_stack_.size()});
  theBuilder->SetInsertPoint(body_bb);
  node->body->toLLVMIR(this);
  emit_bounded_ring_pending_commits();
  llvm::BasicBlock* b2 = theBuilder->GetInsertBlock();
  if (b2 && !b2->getTerminator()) {
    theBuilder->CreateBr(cond_bb);
  }
  theBuilder->SetInsertPoint(exit_bb);
  loop_stack_.pop_back();
  return nullptr;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGForEach* node) {
  auto* lit = dynamic_cast<SCListLiteral*>(node->iterable);
  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::IntegerType* i64t = theBuilder->getInt64Ty();
  llvm::Value* zero = llvm::ConstantInt::get(i64t, 0);
  llvm::Value* one = llvm::ConstantInt::get(i64t, 1);

  llvm::AllocaInst* ledger_alloc = nullptr;
  llvm::AllocaInst* snap_alloc = nullptr;
  int pulse_sz = 0;
  if (node->pulse_plan && node->pulse_plan->total_bytes > 0) {
    pulse_sz = node->pulse_plan->total_bytes;
    llvm::ArrayType* paty =
      llvm::ArrayType::get(theBuilder->getInt8Ty(), static_cast<unsigned>(pulse_sz));
    ledger_alloc = theBuilder->CreateAlloca(paty, nullptr, "pulse_ledger");
    snap_alloc = theBuilder->CreateAlloca(paty, nullptr, "pulse_snap");
    llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
    llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
    llvm::Value* si8 = theBuilder->CreateBitCast(snap_alloc, i8p);
    theBuilder->CreateMemSet(
      li8,
      llvm::ConstantInt::get(theBuilder->getInt8Ty(), 0),
      llvm::ConstantInt::get(theBuilder->getInt64Ty(), pulse_sz),
      llvm::MaybeAlign(8));
    theBuilder->CreateMemSet(
      si8,
      llvm::ConstantInt::get(theBuilder->getInt8Ty(), 0),
      llvm::ConstantInt::get(theBuilder->getInt64Ty(), pulse_sz),
      llvm::MaybeAlign(8));
  }

  auto run_pulse_prologue = [&]() {
    if (pulse_sz > 0) {
      llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
      llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
      llvm::Value* si8 = theBuilder->CreateBitCast(snap_alloc, i8p);
      pulse_copy_ledger_to_snap(li8, si8, pulse_sz);
      pulse_ledger_base_ = li8;
      pulse_snap_base_ = si8;
      pulse_active_plan_ = node->pulse_plan.get();
    }
  };

  auto run_pulse_epilogue = [&]() {
    if (pulse_sz > 0) {
      llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
      llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
      emit_pulse_commit_all(li8, node->pulse_plan.get());
      pulse_ledger_base_ = nullptr;
      pulse_snap_base_ = nullptr;
      pulse_active_plan_ = nullptr;
    }
    emit_bounded_ring_pending_commits();
  };

  auto finish_pulse_region = [&]() {
    if (pulse_sz > 0 && node->pulse_region_id >= 0) {
      llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
      llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
      pulse_region_ledgers_[node->pulse_region_id] = {li8, node->pulse_plan.get()};
    }
  };

  bool const_i64_literal = lit != nullptr && !lit->elems.empty() && node->elem_type == "i64";
  if (const_i64_literal) {
    for (auto* e : lit->elems) {
      if (dynamic_cast<SGConstInt*>(e) == nullptr) {
        const_i64_literal = false;
        break;
      }
    }
  }

  if (const_i64_literal) {
    llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "foreach_exit", F);
    llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(*theContext, "foreach_hdr", F);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, "foreach_body", F);
    llvm::BasicBlock* step_bb = llvm::BasicBlock::Create(*theContext, "foreach_step", F);

    std::vector<llvm::Constant*> cs;
    for (auto* e : lit->elems) {
      auto* ci = static_cast<SGConstInt*>(e);
      cs.push_back(llvm::ConstantInt::get(i64t, std::stoll(ci->value)));
    }
    llvm::ArrayType* at = llvm::ArrayType::get(i64t, cs.size());
    llvm::Constant* init = llvm::ConstantArray::get(at, cs);
    llvm::GlobalVariable* gv = new llvm::GlobalVariable(
      *theModule,
      at,
      true,
      llvm::GlobalValue::PrivateLinkage,
      init,
      "styio_fe_lit");

    llvm::AllocaInst* idx_slot = theBuilder->CreateAlloca(i64t, nullptr, "fe_idx");
    theBuilder->CreateStore(zero, idx_slot);
    theBuilder->CreateBr(hdr_bb);

    theBuilder->SetInsertPoint(hdr_bb);
    llvm::Value* idxv = theBuilder->CreateLoad(i64t, idx_slot);
    llvm::Value* n = llvm::ConstantInt::get(i64t, lit->elems.size());
    llvm::Value* go = theBuilder->CreateICmpSLT(idxv, n);
    theBuilder->CreateCondBr(go, body_bb, exit_bb);

    loop_stack_.push_back(LoopFrame{exit_bb, step_bb, file_handle_scope_stack_.size()});
    theBuilder->SetInsertPoint(body_bb);
    llvm::Value* idx = theBuilder->CreateLoad(i64t, idx_slot);
    llvm::Value* z32 = theBuilder->getInt32(0);
    llvm::Value* gep = theBuilder->CreateInBoundsGEP(at, gv, {z32, idx});
    llvm::Value* el = theBuilder->CreateLoad(i64t, gep);

    llvm::AllocaInst* vs = theBuilder->CreateAlloca(i64t, nullptr, node->var);
    theBuilder->CreateStore(el, vs);
    mutable_variables[node->var] = vs;

    emit_snapshot_shadow_reload();
    run_pulse_prologue();
    node->body->toLLVMIR(this);
    run_pulse_epilogue();
    mutable_variables.erase(node->var);

    llvm::BasicBlock* bcur = theBuilder->GetInsertBlock();
    if (bcur && !bcur->getTerminator()) {
      theBuilder->CreateBr(step_bb);
    }

    theBuilder->SetInsertPoint(step_bb);
    llvm::Value* nx = theBuilder->CreateAdd(theBuilder->CreateLoad(i64t, idx_slot), one);
    theBuilder->CreateStore(nx, idx_slot);
    theBuilder->CreateBr(hdr_bb);

    theBuilder->SetInsertPoint(exit_bb);
    finish_pulse_region();
    loop_stack_.pop_back();
    return nullptr;
  }

  StyioValueFamily elem_family = styio_value_family_from_type_name(node->elem_type);
  const bool elem_string = elem_family == StyioValueFamily::String;
  const bool elem_float = elem_family == StyioValueFamily::Float;
  const bool elem_bool = elem_family == StyioValueFamily::Bool;
  const bool elem_char = elem_family == StyioValueFamily::Char;
  const bool elem_list = elem_family == StyioValueFamily::ListHandle;
  const bool elem_dict = elem_family == StyioValueFamily::DictHandle;
  const bool elem_matrix = elem_family == StyioValueFamily::MatrixHandle;
  llvm::Type* elem_ty = elem_string
    ? static_cast<llvm::Type*>(llvm::PointerType::get(*theContext, 0))
    : (elem_float
        ? static_cast<llvm::Type*>(theBuilder->getDoubleTy())
        : (elem_char
            ? static_cast<llvm::Type*>(theBuilder->getInt8Ty())
            : (elem_bool
                ? static_cast<llvm::Type*>(theBuilder->getInt1Ty())
                : static_cast<llvm::Type*>(i64t))));
  llvm::Type* get_ty = elem_string
    ? static_cast<llvm::Type*>(llvm::PointerType::get(*theContext, 0))
    : (elem_float
        ? static_cast<llvm::Type*>(theBuilder->getDoubleTy())
        : (elem_char
            ? static_cast<llvm::Type*>(theBuilder->getInt8Ty())
            : static_cast<llvm::Type*>(i64t)));
  const char* get_name = elem_string
    ? "styio_list_get_cstr"
    : (elem_float
        ? "styio_list_get_f64"
        : (elem_char
            ? "styio_list_get_char"
            : (elem_bool
                ? "styio_list_get_bool"
                : (elem_list
                    ? "styio_list_get_list"
                    : (elem_dict
                        ? "styio_list_get_dict"
                        : (elem_matrix ? "styio_list_get_matrix" : "styio_list_get"))))));

  llvm::FunctionCallee len_fn = theModule->getOrInsertFunction(
    "styio_list_len",
    llvm::FunctionType::get(i64t, {i64t}, false));
  llvm::FunctionCallee get_fn = theModule->getOrInsertFunction(
    get_name,
    llvm::FunctionType::get(get_ty, {i64t, i64t}, false));

  llvm::Value* iterable = node->iterable->toLLVMIR(this);
  if (!iterable->getType()->isIntegerTy(64)) {
    iterable = theBuilder->CreateSExtOrTrunc(iterable, i64t);
  }
  std::optional<TempResourceKind> iterable_kind = take_owned_resource_temp(iterable);
  const bool release_iterable =
    iterable_kind.has_value() && *iterable_kind == TempResourceKind::List;

  llvm::AllocaInst* list_slot = theBuilder->CreateAlloca(i64t, nullptr, node->var + ".iter");
  llvm::AllocaInst* idx_slot = theBuilder->CreateAlloca(i64t, nullptr, "fe_idx");
  theBuilder->CreateStore(iterable, list_slot);
  theBuilder->CreateStore(zero, idx_slot);

  llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "foreach_rt_exit", F);
  llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(*theContext, "foreach_rt_hdr", F);
  llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, "foreach_rt_body", F);
  llvm::BasicBlock* step_bb = llvm::BasicBlock::Create(*theContext, "foreach_rt_step", F);
  theBuilder->CreateBr(hdr_bb);

  theBuilder->SetInsertPoint(hdr_bb);
  llvm::Value* idxv = theBuilder->CreateLoad(i64t, idx_slot);
  llvm::Value* list_handle = theBuilder->CreateLoad(i64t, list_slot);
  llvm::Value* len = theBuilder->CreateCall(len_fn, {list_handle});
  llvm::Value* go = theBuilder->CreateICmpSLT(idxv, len);
  theBuilder->CreateCondBr(go, body_bb, exit_bb);

  loop_stack_.push_back(LoopFrame{exit_bb, step_bb, file_handle_scope_stack_.size()});
  theBuilder->SetInsertPoint(body_bb);
  llvm::Value* idx = theBuilder->CreateLoad(i64t, idx_slot);
  llvm::Value* cur_list = theBuilder->CreateLoad(i64t, list_slot);
  llvm::Value* elem = theBuilder->CreateCall(get_fn, {cur_list, idx});
  if (elem_bool) {
    elem = theBuilder->CreateICmpNE(elem, theBuilder->getInt64(0));
  }

  llvm::AllocaInst* vs = theBuilder->CreateAlloca(elem_ty, nullptr, node->var);
  theBuilder->CreateStore(elem, vs);
  mutable_variables[node->var] = vs;

  emit_snapshot_shadow_reload();
  run_pulse_prologue();
  node->body->toLLVMIR(this);
  run_pulse_epilogue();
  mutable_variables.erase(node->var);

  llvm::BasicBlock* bcur = theBuilder->GetInsertBlock();
  if (bcur && !bcur->getTerminator()) {
    if (elem_string) {
      llvm::Value* cur = theBuilder->CreateLoad(elem_ty, vs);
      free_cstr_if_runtime_owned(cur);
    }
    else if (elem_list) {
      llvm::Value* cur = theBuilder->CreateLoad(i64t, vs);
      theBuilder->CreateCall(list_release_fn(), {cur});
    }
    else if (elem_dict) {
      llvm::Value* cur = theBuilder->CreateLoad(i64t, vs);
      theBuilder->CreateCall(dict_release_fn(), {cur});
    }
    else if (elem_matrix) {
      llvm::Value* cur = theBuilder->CreateLoad(i64t, vs);
      theBuilder->CreateCall(matrix_release_fn(), {cur});
    }
    theBuilder->CreateBr(step_bb);
  }

  theBuilder->SetInsertPoint(step_bb);
  llvm::Value* nx = theBuilder->CreateAdd(theBuilder->CreateLoad(i64t, idx_slot), one);
  theBuilder->CreateStore(nx, idx_slot);
  theBuilder->CreateBr(hdr_bb);

  theBuilder->SetInsertPoint(exit_bb);
  if (release_iterable) {
    llvm::Value* owned = theBuilder->CreateLoad(i64t, list_slot);
    theBuilder->CreateCall(list_release_fn(), {owned});
  }
  finish_pulse_region();
  loop_stack_.pop_back();
  return nullptr;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGRangeFor* node) {
  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::IntegerType* i64t = theBuilder->getInt64Ty();
  llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(*theContext, "rangefor_hdr", F);
  llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, "rangefor_body", F);
  llvm::BasicBlock* step_bb = llvm::BasicBlock::Create(*theContext, "rangefor_step", F);
  llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "rangefor_exit", F);

  llvm::Value* start = node->start->toLLVMIR(this);
  llvm::Value* end = node->end->toLLVMIR(this);
  llvm::Value* step = node->step->toLLVMIR(this);
  if (!start->getType()->isIntegerTy(64)) {
    start = theBuilder->CreateSExtOrTrunc(start, i64t);
  }
  if (!end->getType()->isIntegerTy(64)) {
    end = theBuilder->CreateSExtOrTrunc(end, i64t);
  }
  if (!step->getType()->isIntegerTy(64)) {
    step = theBuilder->CreateSExtOrTrunc(step, i64t);
  }

  llvm::AllocaInst* idx_slot = theBuilder->CreateAlloca(i64t, nullptr, node->var + ".idx");
  theBuilder->CreateStore(start, idx_slot);
  theBuilder->CreateBr(hdr_bb);

  theBuilder->SetInsertPoint(hdr_bb);
  llvm::Value* cur = theBuilder->CreateLoad(i64t, idx_slot);
  llvm::Value* is_zero = theBuilder->CreateICmpEQ(step, theBuilder->getInt64(0));
  llvm::Value* is_pos = theBuilder->CreateICmpSGT(step, theBuilder->getInt64(0));
  llvm::Value* pos_go = theBuilder->CreateICmpSLE(cur, end);
  llvm::Value* neg_go = theBuilder->CreateICmpSGE(cur, end);
  llvm::Value* go_non_zero = theBuilder->CreateSelect(is_pos, pos_go, neg_go);
  llvm::Value* go = theBuilder->CreateSelect(is_zero, llvm::ConstantInt::getFalse(*theContext), go_non_zero);
  theBuilder->CreateCondBr(go, body_bb, exit_bb);

  loop_stack_.push_back(LoopFrame{exit_bb, step_bb, file_handle_scope_stack_.size()});

  theBuilder->SetInsertPoint(body_bb);
  llvm::AllocaInst* vs = theBuilder->CreateAlloca(i64t, nullptr, node->var);
  theBuilder->CreateStore(cur, vs);
  mutable_variables[node->var] = vs;
  node->body->toLLVMIR(this);
  emit_bounded_ring_pending_commits();
  llvm::BasicBlock* bcur = theBuilder->GetInsertBlock();
  if (bcur && !bcur->getTerminator()) {
    theBuilder->CreateBr(step_bb);
  }

  theBuilder->SetInsertPoint(step_bb);
  llvm::Value* next = theBuilder->CreateAdd(theBuilder->CreateLoad(i64t, idx_slot), step);
  theBuilder->CreateStore(next, idx_slot);
  theBuilder->CreateBr(hdr_bb);

  theBuilder->SetInsertPoint(exit_bb);
  loop_stack_.pop_back();
  return nullptr;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGIf* node) {
  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*theContext, "styif_then", F);
  llvm::BasicBlock* else_bb = node->else_block ? llvm::BasicBlock::Create(*theContext, "styif_else", F) : nullptr;
  llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "styif_exit", F);

  llvm::Value* cv = node->cond->toLLVMIR(this);
  llvm::Value* c = cv;
  if (!cv->getType()->isIntegerTy(1)) {
    if (cv->getType()->isIntegerTy()) {
      c = theBuilder->CreateICmpNE(
        cv,
        llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(cv->getType()), 0));
    }
    else {
      c = theBuilder->CreateFCmpONE(cv, llvm::ConstantFP::get(cv->getType(), 0.0));
    }
  }
  theBuilder->CreateCondBr(c, then_bb, else_bb ? else_bb : exit_bb);

  theBuilder->SetInsertPoint(then_bb);
  node->then_block->toLLVMIR(this);
  if (auto* cur = theBuilder->GetInsertBlock(); cur && !cur->getTerminator()) {
    theBuilder->CreateBr(exit_bb);
  }

  if (else_bb) {
    theBuilder->SetInsertPoint(else_bb);
    node->else_block->toLLVMIR(this);
    if (auto* cur = theBuilder->GetInsertBlock(); cur && !cur->getTerminator()) {
      theBuilder->CreateBr(exit_bb);
    }
  }

  theBuilder->SetInsertPoint(exit_bb);
  return nullptr;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCListLiteral* node) {
  StyioValueFamily elem_family = styio_value_family_from_type_name(node->elem_type);
  const char* new_name = "styio_list_new_i64";
  const char* push_name = "styio_list_push_i64";
  llvm::Type* push_value_type = theBuilder->getInt64Ty();
  switch (elem_family) {
    case StyioValueFamily::Bool:
      new_name = "styio_list_new_bool";
      push_name = "styio_list_push_bool";
      break;
    case StyioValueFamily::Char:
      new_name = "styio_list_new_char";
      push_name = "styio_list_push_char";
      push_value_type = theBuilder->getInt8Ty();
      break;
    case StyioValueFamily::Float:
      new_name = "styio_list_new_f64";
      push_name = "styio_list_push_f64";
      push_value_type = theBuilder->getDoubleTy();
      break;
    case StyioValueFamily::String:
      new_name = "styio_list_new_cstr";
      push_name = "styio_list_push_cstr";
      push_value_type = llvm::PointerType::get(*theContext, 0);
      break;
    case StyioValueFamily::ListHandle:
      new_name = "styio_list_new_list";
      push_name = "styio_list_push_list";
      break;
    case StyioValueFamily::DictHandle:
      new_name = "styio_list_new_dict";
      push_name = "styio_list_push_dict";
      break;
    case StyioValueFamily::MatrixHandle:
      new_name = "styio_list_new_matrix";
      push_name = "styio_list_push_matrix";
      break;
    case StyioValueFamily::Integer:
    default:
      break;
  }
  llvm::FunctionCallee new_fn = theModule->getOrInsertFunction(
    new_name,
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {}, false));
  llvm::FunctionCallee push_fn = theModule->getOrInsertFunction(
    push_name,
    llvm::FunctionType::get(
      theBuilder->getVoidTy(),
      {theBuilder->getInt64Ty(), push_value_type},
      false));
  llvm::Value* list = theBuilder->CreateCall(new_fn, {});
  for (auto* elem : node->elems) {
    llvm::Value* value = elem->toLLVMIR(this);
    value = styio_coerce_collection_value(
      value,
      elem_family,
      theBuilder.get(),
      "list literal");
    theBuilder->CreateCall(push_fn, {list, value});
    if (elem_family == StyioValueFamily::String) {
      free_owned_cstr_temp_if_tracked(value);
    }
    else if (elem_family == StyioValueFamily::ListHandle
             || elem_family == StyioValueFamily::DictHandle
             || elem_family == StyioValueFamily::MatrixHandle) {
      free_owned_resource_temp_if_tracked(value);
    }
  }
  track_owned_resource_temp(list, TempResourceKind::List);
  return list;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCMatrixLiteral* node) {
  const bool is_f64 = styio_value_family_from_type_name(node->elem_type) == StyioValueFamily::Float;
  const char* new_name = is_f64 ? "styio_matrix_new_f64" : "styio_matrix_new_i64";
  const char* set_name = is_f64 ? "styio_matrix_set_f64" : "styio_matrix_set_i64";
  llvm::Type* value_type = is_f64 ? theBuilder->getDoubleTy() : theBuilder->getInt64Ty();
  llvm::FunctionCallee new_fn = theModule->getOrInsertFunction(
    new_name,
    llvm::FunctionType::get(
      theBuilder->getInt64Ty(),
      {theBuilder->getInt64Ty(), theBuilder->getInt64Ty()},
      false));
  llvm::FunctionCallee set_fn = theModule->getOrInsertFunction(
    set_name,
    llvm::FunctionType::get(
      theBuilder->getVoidTy(),
      {theBuilder->getInt64Ty(), theBuilder->getInt64Ty(), theBuilder->getInt64Ty(), value_type},
      false));
  llvm::Value* matrix = theBuilder->CreateCall(
    new_fn,
    {theBuilder->getInt64(static_cast<std::int64_t>(node->rows)),
     theBuilder->getInt64(static_cast<std::int64_t>(node->cols))});
  emit_runtime_error_guard_return();
  for (size_t i = 0; i < node->elems.size(); ++i) {
    llvm::Value* value = node->elems[i]->toLLVMIR(this);
    value = styio_coerce_collection_value(
      value,
      is_f64 ? StyioValueFamily::Float : StyioValueFamily::Integer,
      theBuilder.get(),
      "matrix literal");
    theBuilder->CreateCall(
      set_fn,
      {matrix,
       theBuilder->getInt64(static_cast<std::int64_t>(i / node->cols)),
       theBuilder->getInt64(static_cast<std::int64_t>(i % node->cols)),
       value});
  }
  track_owned_resource_temp(matrix, TempResourceKind::Matrix);
  return matrix;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCDictLiteral* node) {
  StyioValueFamily value_family = styio_value_family_from_type_name(node->value_type);
  const char* new_name = "styio_dict_new_i64";
  const char* set_name = "styio_dict_set_i64";
  llvm::Type* set_value_type = theBuilder->getInt64Ty();
  switch (value_family) {
    case StyioValueFamily::Bool:
      new_name = "styio_dict_new_bool";
      set_name = "styio_dict_set_bool";
      break;
    case StyioValueFamily::Float:
      new_name = "styio_dict_new_f64";
      set_name = "styio_dict_set_f64";
      set_value_type = theBuilder->getDoubleTy();
      break;
    case StyioValueFamily::String:
      new_name = "styio_dict_new_cstr";
      set_name = "styio_dict_set_cstr";
      set_value_type = llvm::PointerType::get(*theContext, 0);
      break;
    case StyioValueFamily::ListHandle:
      new_name = "styio_dict_new_list";
      set_name = "styio_dict_set_list";
      break;
    case StyioValueFamily::DictHandle:
      new_name = "styio_dict_new_dict";
      set_name = "styio_dict_set_dict";
      break;
    case StyioValueFamily::Integer:
    default:
      break;
  }
  llvm::FunctionCallee new_fn = theModule->getOrInsertFunction(
    new_name,
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {}, false));
  llvm::FunctionCallee set_fn = theModule->getOrInsertFunction(
    set_name,
    llvm::FunctionType::get(
      theBuilder->getVoidTy(),
      {theBuilder->getInt64Ty(), llvm::PointerType::get(*theContext, 0), set_value_type},
      false));

  llvm::Value* dict = theBuilder->CreateCall(new_fn, {});
  for (const auto& entry : node->entries) {
    llvm::Value* key = entry.key->toLLVMIR(this);
    llvm::Value* value = entry.value->toLLVMIR(this);
    value = styio_coerce_collection_value(
      value,
      value_family,
      theBuilder.get(),
      "dict literal");
    theBuilder->CreateCall(set_fn, {dict, key, value});
    free_owned_cstr_temp_if_tracked(key);
    if (value_family == StyioValueFamily::String) {
      free_owned_cstr_temp_if_tracked(value);
    }
    else if (value_family == StyioValueFamily::ListHandle
             || value_family == StyioValueFamily::DictHandle) {
      free_owned_resource_temp_if_tracked(value);
    }
  }
  track_owned_resource_temp(dict, TempResourceKind::Dict);
  return dict;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGBreak* node) {
  (void)node;
  if (loop_stack_.empty()) {
    throw StyioTypeError("break outside enclosing loop");
  }
  const LoopFrame& frame = loop_stack_.back();
  emit_scope_cleanup_to_depth(frame.resource_scope_depth);
  llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
  if (cur == nullptr || cur->getTerminator() != nullptr) {
    return nullptr;
  }
  theBuilder->CreateBr(frame.break_dest);
  return nullptr;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGContinue* node) {
  (void)node;
  if (loop_stack_.empty()) {
    throw StyioTypeError("continue outside enclosing loop");
  }
  const LoopFrame& frame = loop_stack_.back();
  emit_scope_cleanup_to_depth(frame.resource_scope_depth);
  llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
  if (cur == nullptr || cur->getTerminator() != nullptr) {
    return nullptr;
  }
  theBuilder->CreateBr(frame.continue_dest);
  return nullptr;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGUndef* node) {
  (void)node;
  return theBuilder->getInt64(styio_undef_i64());
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGFallback* node) {
  llvm::Value* p = node->primary->toLLVMIR(this);
  llvm::Value* a = node->alternate->toLLVMIR(this);
  llvm::Value* u = theBuilder->getInt64(styio_undef_i64());
  if (p->getType()->isIntegerTy(64) && a->getType()->isPointerTy()) {
    llvm::Value* isU = theBuilder->CreateICmpEQ(p, u);
    llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
    llvm::BasicBlock* b_alt = llvm::BasicBlock::Create(*theContext, "fb_alt", F);
    llvm::BasicBlock* b_num = llvm::BasicBlock::Create(*theContext, "fb_num", F);
    llvm::BasicBlock* b_m = llvm::BasicBlock::Create(*theContext, "fb_merge", F);
    theBuilder->CreateCondBr(isU, b_alt, b_num);
    theBuilder->SetInsertPoint(b_alt);
    theBuilder->CreateBr(b_m);
    theBuilder->SetInsertPoint(b_num);
    llvm::Value* ps = promote_to_cstr(p);
    theBuilder->CreateBr(b_m);
    theBuilder->SetInsertPoint(b_m);
    llvm::PHINode* phi = theBuilder->CreatePHI(
      llvm::PointerType::get(*theContext, 0), 2, "fb_phi");
    phi->addIncoming(a, b_alt);
    phi->addIncoming(ps, b_num);
    const bool owns_alt = take_owned_cstr_temp(a);
    if (owns_alt) {
      track_owned_cstr_temp(phi);
    }
    return phi;
  }
  if (p->getType()->isIntegerTy(64) && a->getType()->isIntegerTy(64)) {
    llvm::Value* isU = theBuilder->CreateICmpEQ(p, u);
    return theBuilder->CreateSelect(isU, a, p);
  }
  return p;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGWaveMerge* node) {
  llvm::Value* c = node->cond->toLLVMIR(this);
  llvm::Value* t = node->true_val->toLLVMIR(this);
  llvm::Value* f = node->false_val->toLLVMIR(this);
  if (c->getType()->isIntegerTy(64)) {
    c = theBuilder->CreateICmpNE(
      c,
      llvm::ConstantInt::get(theBuilder->getInt64Ty(), 0, true));
  }
  llvm::Value* out = theBuilder->CreateSelect(c, t, f);
  if (out->getType()->isPointerTy()) {
    const bool owns_true = take_owned_cstr_temp(t);
    const bool owns_false = take_owned_cstr_temp(f);
    if (owns_true || owns_false) {
      track_owned_cstr_temp(out);
    }
  }
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGWaveDispatch* node) {
  llvm::Value* c = node->cond->toLLVMIR(this);
  if (c->getType()->isIntegerTy(64)) {
    c = theBuilder->CreateICmpNE(
      c,
      llvm::ConstantInt::get(theBuilder->getInt64Ty(), 0, true));
  }
  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::BasicBlock* bt = llvm::BasicBlock::Create(*theContext, "wave_disp_t", F);
  llvm::BasicBlock* bf = llvm::BasicBlock::Create(*theContext, "wave_disp_f", F);
  llvm::BasicBlock* bm = llvm::BasicBlock::Create(*theContext, "wave_disp_m", F);
  theBuilder->CreateCondBr(c, bt, bf);
  theBuilder->SetInsertPoint(bt);
  (void)node->true_arm->toLLVMIR(this);
  if (not theBuilder->GetInsertBlock()->getTerminator()) {
    theBuilder->CreateBr(bm);
  }
  theBuilder->SetInsertPoint(bf);
  (void)node->false_arm->toLLVMIR(this);
  if (not theBuilder->GetInsertBlock()->getTerminator()) {
    theBuilder->CreateBr(bm);
  }
  theBuilder->SetInsertPoint(bm);
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGGuardSelect* node) {
  llvm::Value* b = node->base->toLLVMIR(this);
  llvm::Value* g = node->guard_cond->toLLVMIR(this);
  if (g->getType()->isIntegerTy(64)) {
    g = theBuilder->CreateICmpNE(
      g,
      llvm::ConstantInt::get(theBuilder->getInt64Ty(), 0, true));
  }
  llvm::Value* u = theBuilder->getInt64(styio_undef_i64());
  llvm::Value* out = theBuilder->CreateSelect(g, b, u);
  if (out->getType()->isPointerTy()) {
    if (take_owned_cstr_temp(b)) {
      track_owned_cstr_temp(out);
    }
  }
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGEqProbe* node) {
  llvm::Value* b = node->base->toLLVMIR(this);
  llvm::Value* v = node->probe->toLLVMIR(this);
  llvm::Value* eq = theBuilder->CreateICmpEQ(b, v);
  llvm::Value* u = theBuilder->getInt64(styio_undef_i64());
  return theBuilder->CreateSelect(eq, b, u);
}

void
StyioToLLVM::release_bounded_ring_cstr_array(
  llvm::ArrayType* array_type,
  llvm::AllocaInst* array,
  std::uint64_t capacity,
  const std::string& label) {
  if (array_type == nullptr || array == nullptr || capacity == 0) {
    return;
  }
  llvm::Type* elem_ty = array_type->getElementType();
  if (!elem_ty->isPointerTy()) {
    return;
  }
  llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
  if (cur == nullptr || cur->getTerminator() != nullptr) {
    return;
  }
  llvm::Function* F = cur->getParent();
  llvm::Type* i64 = theBuilder->getInt64Ty();
  llvm::Value* zero = llvm::ConstantInt::get(i64, 0);
  llvm::Value* one = llvm::ConstantInt::get(i64, 1);
  llvm::Value* cap = llvm::ConstantInt::get(i64, capacity);
  llvm::AllocaInst* idx_slot = theBuilder->CreateAlloca(i64, nullptr, label + ".cleanup.i");
  theBuilder->CreateStore(zero, idx_slot);

  llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(*theContext, label + "_cleanup_hdr", F);
  llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, label + "_cleanup_body", F);
  llvm::BasicBlock* done_bb = llvm::BasicBlock::Create(*theContext, label + "_cleanup_done", F);
  theBuilder->CreateBr(hdr_bb);

  theBuilder->SetInsertPoint(hdr_bb);
  llvm::Value* i = theBuilder->CreateLoad(i64, idx_slot);
  llvm::Value* more = theBuilder->CreateICmpULT(i, cap);
  theBuilder->CreateCondBr(more, body_bb, done_bb);

  theBuilder->SetInsertPoint(body_bb);
  llvm::Value* gep = theBuilder->CreateInBoundsGEP(array_type, array, {zero, i});
  llvm::Value* value = theBuilder->CreateLoad(elem_ty, gep);
  free_cstr_if_runtime_owned(value);
  theBuilder->CreateStore(styio_zero_for_llvm_type(elem_ty, theBuilder.get()), gep);
  theBuilder->CreateStore(theBuilder->CreateAdd(i, one), idx_slot);
  theBuilder->CreateBr(hdr_bb);

  theBuilder->SetInsertPoint(done_bb);
}

void
StyioToLLVM::release_bounded_ring_handle_array(
  llvm::ArrayType* array_type,
  llvm::AllocaInst* array,
  std::uint64_t capacity,
  StyioValueFamily family,
  const std::string& label) {
  if (array_type == nullptr || array == nullptr || capacity == 0) {
    return;
  }
  llvm::Type* elem_ty = array_type->getElementType();
  if (!elem_ty->isIntegerTy(64)) {
    return;
  }
  llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
  if (cur == nullptr || cur->getTerminator() != nullptr) {
    return;
  }
  llvm::Function* F = cur->getParent();
  llvm::Type* i64 = theBuilder->getInt64Ty();
  llvm::Value* zero = llvm::ConstantInt::get(i64, 0);
  llvm::Value* one = llvm::ConstantInt::get(i64, 1);
  llvm::Value* cap = llvm::ConstantInt::get(i64, capacity);
  llvm::AllocaInst* idx_slot = theBuilder->CreateAlloca(i64, nullptr, label + ".cleanup.i");
  theBuilder->CreateStore(zero, idx_slot);

  llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(*theContext, label + "_cleanup_hdr", F);
  llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, label + "_cleanup_body", F);
  llvm::BasicBlock* done_bb = llvm::BasicBlock::Create(*theContext, label + "_cleanup_done", F);
  theBuilder->CreateBr(hdr_bb);

  theBuilder->SetInsertPoint(hdr_bb);
  llvm::Value* i = theBuilder->CreateLoad(i64, idx_slot);
  llvm::Value* more = theBuilder->CreateICmpULT(i, cap);
  theBuilder->CreateCondBr(more, body_bb, done_bb);

  theBuilder->SetInsertPoint(body_bb);
  llvm::Value* gep = theBuilder->CreateInBoundsGEP(array_type, array, {zero, i});
  llvm::Value* value = theBuilder->CreateLoad(elem_ty, gep);
  free_resource_handle_if_runtime_owned(value, family);
  theBuilder->CreateStore(styio_zero_for_llvm_type(elem_ty, theBuilder.get()), gep);
  theBuilder->CreateStore(theBuilder->CreateAdd(i, one), idx_slot);
  theBuilder->CreateBr(hdr_bb);

  theBuilder->SetInsertPoint(done_bb);
}

void
StyioToLLVM::release_bounded_ring_cstr_storage(const std::string& name) {
  auto arr_it = mutable_variables.find(name);
  auto pending_it = bounded_ring_pending_slot_.find(name);
  auto cap_it = bounded_ring_capacity_.find(name);
  if (arr_it == mutable_variables.end()
      || pending_it == bounded_ring_pending_slot_.end()
      || cap_it == bounded_ring_capacity_.end()) {
    return;
  }
  auto* arr_ty = llvm::dyn_cast<llvm::ArrayType>(arr_it->second->getAllocatedType());
  auto* pending_ty = llvm::dyn_cast<llvm::ArrayType>(pending_it->second->getAllocatedType());
  if (arr_ty == nullptr || pending_ty == nullptr) {
    return;
  }
  auto handle_family_it = bounded_ring_handle_family_.find(name);
  if (handle_family_it != bounded_ring_handle_family_.end()) {
    release_bounded_ring_handle_array(arr_ty, arr_it->second, cap_it->second, handle_family_it->second, name + ".ring");
    release_bounded_ring_handle_array(pending_ty, pending_it->second, cap_it->second, handle_family_it->second, name + ".pending");
    return;
  }
  if (!arr_ty->getElementType()->isPointerTy()) {
    return;
  }
  release_bounded_ring_cstr_array(arr_ty, arr_it->second, cap_it->second, name + ".ring");
  release_bounded_ring_cstr_array(pending_ty, pending_it->second, cap_it->second, name + ".pending");
}

void
StyioToLLVM::push_file_handle_scope() {
  file_handle_scope_stack_.emplace_back();
  cstr_slot_scope_stack_.emplace_back();
  dynamic_slot_scope_stack_.emplace_back();
  bounded_ring_cstr_scope_stack_.emplace_back();
}

void
StyioToLLVM::register_file_handle_for_raii(const std::string& var_name) {
  if (!file_handle_scope_stack_.empty()) {
    file_handle_scope_stack_.back().push_back(var_name);
  }
}

void
StyioToLLVM::register_cstr_slot_for_raii(llvm::AllocaInst* slot) {
  if (slot && !cstr_slot_scope_stack_.empty()) {
    cstr_slot_scope_stack_.back().push_back(slot);
  }
}

void
StyioToLLVM::register_dynamic_slot_for_raii(llvm::AllocaInst* slot) {
  if (slot && !dynamic_slot_scope_stack_.empty()) {
    dynamic_slot_scope_stack_.back().push_back(slot);
  }
}

void
StyioToLLVM::register_bounded_ring_cstr_for_raii(const std::string& name) {
  if (!name.empty() && !bounded_ring_cstr_scope_stack_.empty()) {
    bounded_ring_cstr_scope_stack_.back().push_back(name);
  }
}

bool
StyioToLLVM::emit_active_file_handle_cleanup() {
  std::unordered_set<llvm::AllocaInst*> closed_file_slots;
  bool emitted_cleanup = false;
  if (!file_handle_scope_stack_.empty()) {
    for (auto scope_it = file_handle_scope_stack_.rbegin();
         scope_it != file_handle_scope_stack_.rend();
         ++scope_it) {
      for (const std::string& v : *scope_it) {
        auto it = mutable_variables.find(v);
        if (it == mutable_variables.end()) {
          continue;
        }
        llvm::AllocaInst* slot = it->second;
        if (!closed_file_slots.insert(slot).second) {
          continue;
        }
        emit_file_handle_slot_close(slot);
        emitted_cleanup = true;
      }
    }
  }
  return emitted_cleanup;
}

void
StyioToLLVM::emit_scope_cleanup_to_depth(std::size_t keep_depth) {
  if (keep_depth >= file_handle_scope_stack_.size()) {
    return;
  }

  std::unordered_set<llvm::AllocaInst*> closed_file_slots;
  bool emitted_file_cleanup = false;
  for (std::size_t scope_ix = file_handle_scope_stack_.size(); scope_ix-- > keep_depth;) {
    for (const std::string& v : file_handle_scope_stack_[scope_ix]) {
      auto it = mutable_variables.find(v);
      if (it == mutable_variables.end()) {
        continue;
      }
      llvm::AllocaInst* slot = it->second;
      if (!closed_file_slots.insert(slot).second) {
        continue;
      }
      emit_file_handle_slot_close(slot);
      emitted_file_cleanup = true;
    }
  }

  std::unordered_set<llvm::AllocaInst*> freed_cstr_slots;
  for (std::size_t scope_ix = cstr_slot_scope_stack_.size(); scope_ix-- > keep_depth;) {
    for (llvm::AllocaInst* slot : cstr_slot_scope_stack_[scope_ix]) {
      if (slot == nullptr || !slot->getAllocatedType()->isPointerTy()) {
        continue;
      }
      if (!freed_cstr_slots.insert(slot).second) {
        continue;
      }
      llvm::Value* s = theBuilder->CreateLoad(slot->getAllocatedType(), slot);
      free_cstr_if_runtime_owned(s);
    }
  }

  std::unordered_set<llvm::AllocaInst*> released_dynamic_slots;
  for (std::size_t scope_ix = dynamic_slot_scope_stack_.size(); scope_ix-- > keep_depth;) {
    for (llvm::AllocaInst* slot : dynamic_slot_scope_stack_[scope_ix]) {
      if (slot == nullptr) {
        continue;
      }
      if (!released_dynamic_slots.insert(slot).second) {
        continue;
      }
      release_dynamic_slot_contents(slot);
    }
  }

  std::unordered_set<std::string> released_bounded_rings;
  for (std::size_t scope_ix = bounded_ring_cstr_scope_stack_.size(); scope_ix-- > keep_depth;) {
    for (const std::string& name : bounded_ring_cstr_scope_stack_[scope_ix]) {
      if (!released_bounded_rings.insert(name).second) {
        continue;
      }
      release_bounded_ring_cstr_storage(name);
    }
  }

  if (emitted_file_cleanup) {
    emit_runtime_error_guard_return_after_cleanup();
  }
}

void
StyioToLLVM::emit_active_scope_cleanup() {
  (void)emit_active_file_handle_cleanup();
  std::unordered_set<llvm::AllocaInst*> freed_cstr_slots;
  for (auto scope_it = cstr_slot_scope_stack_.rbegin();
       scope_it != cstr_slot_scope_stack_.rend();
       ++scope_it) {
    for (llvm::AllocaInst* slot : *scope_it) {
      if (slot == nullptr || !slot->getAllocatedType()->isPointerTy()) {
        continue;
      }
      if (!freed_cstr_slots.insert(slot).second) {
        continue;
      }
      llvm::Value* s = theBuilder->CreateLoad(slot->getAllocatedType(), slot);
      free_cstr_if_runtime_owned(s);
    }
  }

  std::unordered_set<llvm::AllocaInst*> released_dynamic_slots;
  for (auto scope_it = dynamic_slot_scope_stack_.rbegin();
       scope_it != dynamic_slot_scope_stack_.rend();
       ++scope_it) {
    for (llvm::AllocaInst* slot : *scope_it) {
      if (slot == nullptr) {
        continue;
      }
      if (!released_dynamic_slots.insert(slot).second) {
        continue;
      }
      release_dynamic_slot_contents(slot);
    }
  }

  std::unordered_set<std::string> released_bounded_rings;
  for (auto scope_it = bounded_ring_cstr_scope_stack_.rbegin();
       scope_it != bounded_ring_cstr_scope_stack_.rend();
       ++scope_it) {
    for (const std::string& name : *scope_it) {
      if (!released_bounded_rings.insert(name).second) {
        continue;
      }
      release_bounded_ring_cstr_storage(name);
    }
  }
}

void
StyioToLLVM::discard_file_handle_scope_metadata() {
  if (!file_handle_scope_stack_.empty()) {
    file_handle_scope_stack_.pop_back();
  }
  if (!cstr_slot_scope_stack_.empty()) {
    cstr_slot_scope_stack_.pop_back();
  }
  if (!dynamic_slot_scope_stack_.empty()) {
    dynamic_slot_scope_stack_.pop_back();
  }
  if (!bounded_ring_cstr_scope_stack_.empty()) {
    bounded_ring_cstr_scope_stack_.pop_back();
  }
}

void
StyioToLLVM::pop_file_handle_scope() {
  if (file_handle_scope_stack_.empty()) {
    return;
  }
  bool emitted_file_cleanup = false;
  if (!file_handle_scope_stack_.back().empty()) {
    std::unordered_set<llvm::AllocaInst*> closed_slots;
    for (const std::string& v : file_handle_scope_stack_.back()) {
      auto it = mutable_variables.find(v);
      if (it != mutable_variables.end()) {
        llvm::AllocaInst* slot = it->second;
        if (closed_slots.insert(slot).second) {
          emit_file_handle_slot_close(slot);
          emitted_file_cleanup = true;
        }
      }
    }
  }

  std::unordered_set<llvm::AllocaInst*> freed_cstr_slots;
  if (!cstr_slot_scope_stack_.empty()) {
    for (llvm::AllocaInst* slot : cstr_slot_scope_stack_.back()) {
      if (slot == nullptr || !slot->getAllocatedType()->isPointerTy()) {
        continue;
      }
      if (!freed_cstr_slots.insert(slot).second) {
        continue;
      }
      llvm::Value* s = theBuilder->CreateLoad(slot->getAllocatedType(), slot);
      free_cstr_if_runtime_owned(s);
    }
    cstr_slot_scope_stack_.pop_back();
  }
  if (!dynamic_slot_scope_stack_.empty()) {
    std::unordered_set<llvm::AllocaInst*> released_dynamic_slots;
    for (llvm::AllocaInst* slot : dynamic_slot_scope_stack_.back()) {
      if (slot == nullptr) {
        continue;
      }
      if (!released_dynamic_slots.insert(slot).second) {
        continue;
      }
      release_dynamic_slot_contents(slot);
    }
    dynamic_slot_scope_stack_.pop_back();
  }
  if (!bounded_ring_cstr_scope_stack_.empty()) {
    std::unordered_set<std::string> released_bounded_rings;
    for (const std::string& name : bounded_ring_cstr_scope_stack_.back()) {
      if (!released_bounded_rings.insert(name).second) {
        continue;
      }
      release_bounded_ring_cstr_storage(name);
    }
    bounded_ring_cstr_scope_stack_.pop_back();
  }
  file_handle_scope_stack_.pop_back();
  if (emitted_file_cleanup) {
    emit_runtime_error_guard_return_after_cleanup();
  }
}

void
StyioToLLVM::emit_snapshot_shadow_reload() {
  if (snapshot_path_exprs_.empty()) {
    return;
  }
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee read_fn = theModule->getOrInsertFunction(
    "styio_read_file_i64line",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr}, false));
  for (auto const& pr : snapshot_path_exprs_) {
    auto it = mutable_variables.find(pr.first);
    if (it == mutable_variables.end()) {
      continue;
    }
    llvm::Value* p = pr.second->toLLVMIR(this);
    llvm::Value* v = theBuilder->CreateCall(read_fn, {p});
    theBuilder->CreateStore(v, it->second);
  }
}

namespace {

std::string
path_key_from_path_ir(StyioIR* path_expr) {
  if (auto* cs = dynamic_cast<SGConstString*>(path_expr)) {
    return cs->value;
  }
  return {};
}

}  // namespace

llvm::Value*
StyioToLLVM::toLLVMIR(SIOHandleAcquire* node) {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee open_fn = theModule->getOrInsertFunction(
    node->is_auto ? "styio_file_open_auto" : "styio_file_open",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr}, false));
  llvm::Value* path = node->path_expr->toLLVMIR(this);
  std::string pkey = path_key_from_path_ir(node->path_expr);
  llvm::BasicBlock* resource_effect_rebind_done = nullptr;
  if (release_tracked_file_handle_binding(node->var_name)) {
    if (resource_effect_operation_depth_ == 0) {
      emit_runtime_error_guard_return();
    }
    else {
      llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
      if (cur != nullptr && cur->getTerminator() == nullptr) {
        llvm::Function* fn = cur->getParent();
        llvm::FunctionCallee has_error = theModule->getOrInsertFunction(
          "styio_runtime_has_error",
          llvm::FunctionType::get(theBuilder->getInt32Ty(), false));
        llvm::Value* has_err = theBuilder->CreateCall(has_error, {});
        llvm::Value* failed = theBuilder->CreateICmpNE(has_err, theBuilder->getInt32(0));
        llvm::BasicBlock* open_bb = llvm::BasicBlock::Create(
          *theContext, "file_rebind_cleanup_ok", fn);
        resource_effect_rebind_done = llvm::BasicBlock::Create(
          *theContext, "file_rebind_cleanup_done", fn);
        theBuilder->CreateCondBr(failed, resource_effect_rebind_done, open_bb);
        theBuilder->SetInsertPoint(open_bb);
      }
    }
  }

  auto reopen_slot_if_closed = [&](llvm::AllocaInst* existing_slot)
  {
    llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
    if (existing_slot == nullptr || cur == nullptr || cur->getTerminator() != nullptr) {
      return;
    }
    llvm::Function* fn = cur->getParent();
    llvm::Value* current = theBuilder->CreateLoad(theBuilder->getInt64Ty(), existing_slot);
    llvm::Value* is_closed = theBuilder->CreateICmpEQ(current, theBuilder->getInt64(0));
    llvm::BasicBlock* reopen_bb = llvm::BasicBlock::Create(*theContext, "file_reopen", fn);
    llvm::BasicBlock* cont_bb = llvm::BasicBlock::Create(*theContext, "file_open_ok", fn);
    theBuilder->CreateCondBr(is_closed, reopen_bb, cont_bb);

    theBuilder->SetInsertPoint(reopen_bb);
    llvm::Value* h = theBuilder->CreateCall(open_fn, {path});
    theBuilder->CreateStore(h, existing_slot);
    theBuilder->CreateBr(cont_bb);

    theBuilder->SetInsertPoint(cont_bb);
  };

  llvm::AllocaInst* slot = nullptr;
  if (!pkey.empty()) {
    auto sit = file_singleton_path_slots_.find(pkey);
    if (sit != file_singleton_path_slots_.end()) {
      slot = sit->second;
      reopen_slot_if_closed(slot);
    }
  }
  if (!slot) {
    llvm::Value* h = theBuilder->CreateCall(open_fn, {path});
    slot = create_entry_alloca(
      theBuilder->getInt64Ty(),
      node->var_name);
    theBuilder->CreateStore(h, slot);
    if (!pkey.empty()) {
      file_singleton_path_slots_[pkey] = slot;
    }
  }
  mutable_variables[node->var_name] = slot;
  file_handle_var_slots_[node->var_name] = slot;
  register_file_handle_for_raii(node->var_name);
  if (resource_effect_rebind_done != nullptr) {
    llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
    if (cur != nullptr && cur->getTerminator() == nullptr) {
      theBuilder->CreateBr(resource_effect_rebind_done);
    }
    theBuilder->SetInsertPoint(resource_effect_rebind_done);
    return theBuilder->getInt64(0);
  }
  if (resource_effect_operation_depth_ == 0) {
    emit_runtime_error_guard_return();
  }
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SIOFileLineIter* node) {
  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee open_fn = theModule->getOrInsertFunction(
    "styio_file_open",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr}, false));
  llvm::FunctionCallee read_fn = theModule->getOrInsertFunction(
    "styio_file_read_line",
    llvm::FunctionType::get(char_ptr, {theBuilder->getInt64Ty()}, false));

  llvm::AllocaInst* h_slot = nullptr;
  llvm::Value* h0 = nullptr;
  std::string temp_handle_name;
  if (node->from_path) {
    llvm::Value* path = node->path_expr->toLLVMIR(this);
    h0 = theBuilder->CreateCall(open_fn, {path});
    h_slot = theBuilder->CreateAlloca(theBuilder->getInt64Ty(), nullptr, "file_iter_h");
    theBuilder->CreateStore(h0, h_slot);
    temp_handle_name =
      "__styio_file_iter_h_" + std::to_string(file_handle_temp_counter_++);
    mutable_variables[temp_handle_name] = h_slot;
    register_file_handle_for_raii(temp_handle_name);
    if (resource_effect_operation_depth_ == 0) {
      emit_runtime_error_guard_return();
    }
  }
  else {
    auto it = mutable_variables.find(node->handle_var);
    if (it == mutable_variables.end()) {
      return theBuilder->getInt64(0);
    }
    h_slot = it->second;
    llvm::FunctionCallee rewind_fn = theModule->getOrInsertFunction(
      "styio_file_rewind",
      llvm::FunctionType::get(theBuilder->getVoidTy(), {theBuilder->getInt64Ty()}, false));
    llvm::Value* hrw = theBuilder->CreateLoad(theBuilder->getInt64Ty(), h_slot);
    theBuilder->CreateCall(rewind_fn, {hrw});
  }

  llvm::BasicBlock* hdr = llvm::BasicBlock::Create(*theContext, "fline_hdr", F);
  llvm::BasicBlock* body = llvm::BasicBlock::Create(*theContext, "fline_body", F);
  llvm::BasicBlock* maybe_done = llvm::BasicBlock::Create(*theContext, "fline_maybe_done", F);
  llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "fline_exit", F);

  llvm::AllocaInst* ledger_alloc = nullptr;
  llvm::AllocaInst* snap_alloc = nullptr;
  int pulse_sz = 0;
  if (node->pulse_plan && node->pulse_plan->total_bytes > 0) {
    pulse_sz = node->pulse_plan->total_bytes;
    llvm::ArrayType* paty =
      llvm::ArrayType::get(theBuilder->getInt8Ty(), static_cast<unsigned>(pulse_sz));
    ledger_alloc = theBuilder->CreateAlloca(paty, nullptr, "pulse_ledger_f");
    snap_alloc = theBuilder->CreateAlloca(paty, nullptr, "pulse_snap_f");
    llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
    llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
    llvm::Value* si8 = theBuilder->CreateBitCast(snap_alloc, i8p);
    theBuilder->CreateMemSet(
      li8,
      llvm::ConstantInt::get(theBuilder->getInt8Ty(), 0),
      llvm::ConstantInt::get(theBuilder->getInt64Ty(), pulse_sz),
      llvm::MaybeAlign(8));
    theBuilder->CreateMemSet(
      si8,
      llvm::ConstantInt::get(theBuilder->getInt8Ty(), 0),
      llvm::ConstantInt::get(theBuilder->getInt64Ty(), pulse_sz),
      llvm::MaybeAlign(8));
  }

  theBuilder->CreateBr(hdr);
  theBuilder->SetInsertPoint(hdr);
  llvm::Value* h = theBuilder->CreateLoad(theBuilder->getInt64Ty(), h_slot);
  llvm::Value* lineptr = theBuilder->CreateCall(read_fn, {h});
  llvm::Value* null_line = llvm::ConstantPointerNull::get(
    llvm::cast<llvm::PointerType>(char_ptr));
  llvm::Value* done = theBuilder->CreateICmpEQ(lineptr, null_line);
  theBuilder->CreateCondBr(done, maybe_done, body);

  theBuilder->SetInsertPoint(maybe_done);
  if (resource_effect_operation_depth_ == 0) {
    emit_runtime_error_guard_return();
  }
  if (llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
      cur != nullptr && cur->getTerminator() == nullptr) {
    theBuilder->CreateBr(exit_bb);
  }

  theBuilder->SetInsertPoint(body);
  llvm::AllocaInst* line_slot = theBuilder->CreateAlloca(char_ptr, nullptr, node->line_var);
  theBuilder->CreateStore(lineptr, line_slot);
  mutable_variables[node->line_var] = line_slot;

  emit_snapshot_shadow_reload();

  if (pulse_sz > 0) {
    llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
    llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
    llvm::Value* si8 = theBuilder->CreateBitCast(snap_alloc, i8p);
    pulse_copy_ledger_to_snap(li8, si8, pulse_sz);
    pulse_ledger_base_ = li8;
    pulse_snap_base_ = si8;
    pulse_active_plan_ = node->pulse_plan.get();
  }

  node->body->toLLVMIR(this);

  if (pulse_sz > 0) {
    llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
    llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
    emit_pulse_commit_all(li8, node->pulse_plan.get());
    pulse_ledger_base_ = nullptr;
    pulse_snap_base_ = nullptr;
    pulse_active_plan_ = nullptr;
  }
  emit_bounded_ring_pending_commits();

  mutable_variables.erase(node->line_var);
  llvm::BasicBlock* b2 = theBuilder->GetInsertBlock();
  if (b2 && !b2->getTerminator()) {
    theBuilder->CreateBr(hdr);
  }

  theBuilder->SetInsertPoint(exit_bb);
  if (pulse_sz > 0 && node->pulse_region_id >= 0) {
    llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
    llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
    pulse_region_ledgers_[node->pulse_region_id] = {li8, node->pulse_plan.get()};
  }
  if (node->from_path) {
    emit_file_handle_slot_close(h_slot);
    if (!temp_handle_name.empty()) {
      mutable_variables.erase(temp_handle_name);
    }
    if (resource_effect_operation_depth_ == 0) {
      emit_runtime_error_guard_return_after_cleanup();
    }
  }
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SIOHandleRelease* node) {
  auto close_slot = [&](llvm::AllocaInst* slot) -> llvm::Value* {
    emit_file_handle_slot_close(slot);
    return theBuilder->getInt64(0);
  };

  if (node->from_path) {
    std::string pkey = path_key_from_path_ir(node->path_expr);
    if (!pkey.empty()) {
      auto sit = file_singleton_path_slots_.find(pkey);
      if (sit != file_singleton_path_slots_.end()) {
        llvm::Value* out = close_slot(sit->second);
        if (resource_effect_operation_depth_ == 0) {
          emit_runtime_error_guard_return();
        }
        return out;
      }
    }
    llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
    llvm::FunctionCallee open_fn = theModule->getOrInsertFunction(
      node->is_auto ? "styio_file_open_auto" : "styio_file_open",
      llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr}, false));
    llvm::Value* path = node->path_expr->toLLVMIR(this);
    llvm::Value* h = theBuilder->CreateCall(open_fn, {path});
    llvm::FunctionCallee close_fn = theModule->getOrInsertFunction(
      "styio_file_close",
      llvm::FunctionType::get(theBuilder->getVoidTy(), {theBuilder->getInt64Ty()}, false));
    theBuilder->CreateCall(close_fn, {h});
    if (resource_effect_operation_depth_ == 0) {
      emit_runtime_error_guard_return();
    }
    return theBuilder->getInt64(0);
  }

  auto it = mutable_variables.find(node->var_name);
  if (it == mutable_variables.end()) {
    return theBuilder->getInt64(0);
  }
  llvm::Value* out = close_slot(it->second);
  file_handle_var_slots_.erase(node->var_name);
  if (resource_effect_operation_depth_ == 0) {
    emit_runtime_error_guard_return();
  }
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGSnapshotDecl* node) {
  llvm::AllocaInst* slot = theBuilder->CreateAlloca(
    theBuilder->getInt64Ty(), nullptr, node->var_name.c_str());
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee read_fn = theModule->getOrInsertFunction(
    "styio_read_file_i64line",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr}, false));
  llvm::Value* p = node->path_expr->toLLVMIR(this);
  llvm::Value* v = theBuilder->CreateCall(read_fn, {p});
  theBuilder->CreateStore(v, slot);
  mutable_variables[node->var_name] = slot;
  snapshot_path_exprs_.emplace_back(node->var_name, node->path_expr);
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGSnapshotShadowLoad* node) {
  auto it = mutable_variables.find(node->var_name);
  if (it == mutable_variables.end()) {
    return theBuilder->getInt64(0);
  }
  return theBuilder->CreateLoad(theBuilder->getInt64Ty(), it->second);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SIOInstantPull* node) {
  if (node->from_handle) {
    auto slot_it = mutable_variables.find(node->handle_var);
    if (slot_it == mutable_variables.end()) {
      return theBuilder->getInt64(0);
    }
    llvm::FunctionCallee read_fn = theModule->getOrInsertFunction(
      "styio_file_read_i64line_from_handle",
      llvm::FunctionType::get(theBuilder->getInt64Ty(), {theBuilder->getInt64Ty()}, false));
    llvm::Value* handle = theBuilder->CreateLoad(theBuilder->getInt64Ty(), slot_it->second);
    llvm::Value* out = theBuilder->CreateCall(read_fn, {handle});
    if (resource_effect_operation_depth_ == 0) {
      emit_runtime_error_guard_return();
    }
    return out;
  }
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee read_fn = theModule->getOrInsertFunction(
    "styio_read_file_i64line",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr}, false));
  llvm::Value* p = node->path_expr->toLLVMIR(this);
  return theBuilder->CreateCall(read_fn, {p});
}

llvm::Value*
StyioToLLVM::toLLVMIR(SIOListReadStdin* node) {
  const char* read_name = styio_stdin_list_read_intrinsic_name_for_elem(node->elem_type);
  llvm::FunctionCallee read_fn = theModule->getOrInsertFunction(
    read_name,
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {}, false));
  llvm::Value* out = theBuilder->CreateCall(read_fn, {});
  track_owned_resource_temp(out, TempResourceKind::List);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCListClone* node) {
  llvm::FunctionCallee clone_fn = theModule->getOrInsertFunction(
    "styio_list_clone",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {theBuilder->getInt64Ty()}, false));
  llvm::Value* src = node->source->toLLVMIR(this);
  if (!src->getType()->isIntegerTy(64)) {
    src = theBuilder->CreateSExtOrTrunc(src, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(clone_fn, {src});
  track_owned_resource_temp(out, TempResourceKind::List);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCMatrixClone* node) {
  const bool is_f64 =
    styio_value_family_from_type_name(node->elem_type) == StyioValueFamily::Float;
  llvm::FunctionCallee clone_fn = theModule->getOrInsertFunction(
    is_f64 ? "styio_matrix_clone_f64" : "styio_matrix_clone_i64",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {theBuilder->getInt64Ty()}, false));
  llvm::Value* src = node->source->toLLVMIR(this);
  if (!src->getType()->isIntegerTy(64)) {
    src = theBuilder->CreateSExtOrTrunc(src, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(clone_fn, {src});
  emit_runtime_error_guard_return();
  track_owned_resource_temp(out, TempResourceKind::Matrix);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCListLen* node) {
  llvm::FunctionCallee len_fn = theModule->getOrInsertFunction(
    "styio_list_len",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {theBuilder->getInt64Ty()}, false));
  llvm::Value* list = node->list->toLLVMIR(this);
  if (!list->getType()->isIntegerTy(64)) {
    list = theBuilder->CreateSExtOrTrunc(list, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(len_fn, {list});
  free_owned_resource_temp_if_tracked(list);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCListGet* node) {
  StyioValueFamily elem_family = styio_value_family_from_type_name(node->elem_type);
  const bool string_elem = elem_family == StyioValueFamily::String;
  const bool float_elem = elem_family == StyioValueFamily::Float;
  const bool bool_elem = elem_family == StyioValueFamily::Bool;
  const bool char_elem = elem_family == StyioValueFamily::Char;
  const bool list_elem = elem_family == StyioValueFamily::ListHandle;
  const bool dict_elem = elem_family == StyioValueFamily::DictHandle;
  const bool matrix_elem = elem_family == StyioValueFamily::MatrixHandle;
  llvm::Type* result_type = string_elem
    ? static_cast<llvm::Type*>(llvm::PointerType::get(*theContext, 0))
    : (float_elem
        ? static_cast<llvm::Type*>(theBuilder->getDoubleTy())
        : (char_elem
            ? static_cast<llvm::Type*>(theBuilder->getInt8Ty())
            : static_cast<llvm::Type*>(theBuilder->getInt64Ty())));
  llvm::FunctionCallee get_fn = theModule->getOrInsertFunction(
    string_elem
      ? "styio_list_get_cstr"
      : (float_elem
          ? "styio_list_get_f64"
          : (char_elem
              ? "styio_list_get_char"
              : (bool_elem
                  ? "styio_list_get_bool"
                  : (list_elem
                      ? "styio_list_get_list"
                      : (dict_elem
                          ? "styio_list_get_dict"
                          : (matrix_elem ? "styio_list_get_matrix" : "styio_list_get")))))),
    llvm::FunctionType::get(
      result_type,
      {theBuilder->getInt64Ty(), theBuilder->getInt64Ty()},
      false));
  llvm::Value* list = node->list->toLLVMIR(this);
  llvm::Value* idx = node->index->toLLVMIR(this);
  if (!list->getType()->isIntegerTy(64)) {
    list = theBuilder->CreateSExtOrTrunc(list, theBuilder->getInt64Ty());
  }
  if (!idx->getType()->isIntegerTy(64)) {
    idx = theBuilder->CreateSExtOrTrunc(idx, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(get_fn, {list, idx});
  free_owned_resource_temp_if_tracked(list);
  if (resource_effect_operation_depth_ == 0) {
    emit_runtime_error_guard_return();
  }
  if (string_elem) {
    track_owned_cstr_temp(out);
  }
  if (list_elem) {
    track_owned_resource_temp(out, TempResourceKind::List);
  }
  if (dict_elem) {
    track_owned_resource_temp(out, TempResourceKind::Dict);
  }
  if (matrix_elem) {
    track_owned_resource_temp(out, TempResourceKind::Matrix);
  }
  if (bool_elem) {
    return theBuilder->CreateICmpNE(out, theBuilder->getInt64(0));
  }
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCListSlice* node) {
  llvm::FunctionCallee slice_fn = theModule->getOrInsertFunction(
    "styio_list_slice",
    llvm::FunctionType::get(
      theBuilder->getInt64Ty(),
      {
        theBuilder->getInt64Ty(),
        theBuilder->getInt64Ty(),
        theBuilder->getInt64Ty(),
        theBuilder->getInt32Ty(),
      },
      false));
  llvm::Value* list = node->list->toLLVMIR(this);
  llvm::Value* start = node->start->toLLVMIR(this);
  llvm::Value* end = node->end != nullptr
    ? node->end->toLLVMIR(this)
    : theBuilder->getInt64(0);
  if (!list->getType()->isIntegerTy(64)) {
    list = theBuilder->CreateSExtOrTrunc(list, theBuilder->getInt64Ty());
  }
  if (!start->getType()->isIntegerTy(64)) {
    start = theBuilder->CreateSExtOrTrunc(start, theBuilder->getInt64Ty());
  }
  if (!end->getType()->isIntegerTy(64)) {
    end = theBuilder->CreateSExtOrTrunc(end, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(
    slice_fn,
    {list, start, end, theBuilder->getInt32(node->end != nullptr ? 1 : 0)});
  free_owned_resource_temp_if_tracked(list);
  if (resource_effect_operation_depth_ == 0) {
    emit_runtime_error_guard_return();
  }
  track_owned_resource_temp(out, TempResourceKind::List);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCListSet* node) {
  StyioValueFamily value_family = styio_value_family_from_type_name(node->elem_type);
  llvm::Type* set_value_type = theBuilder->getInt64Ty();
  const char* set_name = "styio_list_set";
  switch (value_family) {
    case StyioValueFamily::Bool:
      set_name = "styio_list_set_bool";
      break;
    case StyioValueFamily::Char:
      set_name = "styio_list_set_char";
      set_value_type = theBuilder->getInt8Ty();
      break;
    case StyioValueFamily::Float:
      set_name = "styio_list_set_f64";
      set_value_type = theBuilder->getDoubleTy();
      break;
    case StyioValueFamily::String:
      set_name = "styio_list_set_cstr";
      set_value_type = llvm::PointerType::get(*theContext, 0);
      break;
    case StyioValueFamily::ListHandle:
      set_name = "styio_list_set_list";
      break;
    case StyioValueFamily::DictHandle:
      set_name = "styio_list_set_dict";
      break;
    case StyioValueFamily::MatrixHandle:
      set_name = "styio_list_set_matrix";
      break;
    case StyioValueFamily::Integer:
    default:
      break;
  }
  llvm::FunctionCallee set_fn = theModule->getOrInsertFunction(
    set_name,
    llvm::FunctionType::get(
      theBuilder->getVoidTy(),
      {theBuilder->getInt64Ty(), theBuilder->getInt64Ty(), set_value_type},
      false));
  llvm::Value* list_raw = node->list->toLLVMIR(this);
  llvm::Value* list = list_raw;
  llvm::Value* idx = node->index->toLLVMIR(this);
  llvm::Value* value_raw = node->value->toLLVMIR(this);
  llvm::Value* value = value_raw;
  if (!list->getType()->isIntegerTy(64)) {
    list = theBuilder->CreateSExtOrTrunc(list, theBuilder->getInt64Ty());
  }
  if (!idx->getType()->isIntegerTy(64)) {
    idx = theBuilder->CreateSExtOrTrunc(idx, theBuilder->getInt64Ty());
  }
  value = styio_coerce_collection_value(
    value,
    value_family,
    theBuilder.get(),
    "list set");
  theBuilder->CreateCall(set_fn, {list, idx, value});
  if (value_family == StyioValueFamily::String) {
    free_owned_cstr_temp_if_tracked(value_raw);
  }
  else if (value_family == StyioValueFamily::ListHandle
           || value_family == StyioValueFamily::DictHandle
           || value_family == StyioValueFamily::MatrixHandle) {
    free_owned_resource_temp_if_tracked(value_raw);
  }
  free_owned_resource_temp_if_tracked(list_raw);
  return nullptr;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCListToString* node) {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee str_fn = theModule->getOrInsertFunction(
    "styio_list_to_cstr",
    llvm::FunctionType::get(char_ptr, {theBuilder->getInt64Ty()}, false));
  llvm::Value* list = node->list->toLLVMIR(this);
  if (!list->getType()->isIntegerTy(64)) {
    list = theBuilder->CreateSExtOrTrunc(list, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(str_fn, {list});
  free_owned_resource_temp_if_tracked(list);
  track_owned_cstr_temp(out);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCMatrixGet* node) {
  const bool is_f64 = styio_value_family_from_type_name(node->elem_type) == StyioValueFamily::Float;
  llvm::FunctionCallee get_fn = theModule->getOrInsertFunction(
    is_f64 ? "styio_matrix_get_f64" : "styio_matrix_get_i64",
    llvm::FunctionType::get(
      is_f64 ? theBuilder->getDoubleTy() : theBuilder->getInt64Ty(),
      {theBuilder->getInt64Ty(), theBuilder->getInt64Ty(), theBuilder->getInt64Ty()},
      false));
  llvm::Value* matrix = node->matrix->toLLVMIR(this);
  llvm::Value* row = node->row->toLLVMIR(this);
  llvm::Value* col = node->col->toLLVMIR(this);
  if (!matrix->getType()->isIntegerTy(64)) {
    matrix = theBuilder->CreateSExtOrTrunc(matrix, theBuilder->getInt64Ty());
  }
  if (!row->getType()->isIntegerTy(64)) {
    row = theBuilder->CreateSExtOrTrunc(row, theBuilder->getInt64Ty());
  }
  if (!col->getType()->isIntegerTy(64)) {
    col = theBuilder->CreateSExtOrTrunc(col, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(get_fn, {matrix, row, col});
  free_owned_resource_temp_if_tracked(matrix);
  if (resource_effect_operation_depth_ == 0) {
    emit_runtime_error_guard_return();
  }
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCMatrixRow* node) {
  const bool is_f64 = styio_value_family_from_type_name(node->elem_type) == StyioValueFamily::Float;
  llvm::FunctionCallee row_fn = theModule->getOrInsertFunction(
    is_f64 ? "styio_matrix_row_f64" : "styio_matrix_row_i64",
    llvm::FunctionType::get(
      theBuilder->getInt64Ty(),
      {theBuilder->getInt64Ty(), theBuilder->getInt64Ty()},
      false));
  llvm::Value* matrix = node->matrix->toLLVMIR(this);
  llvm::Value* row = node->row->toLLVMIR(this);
  if (!matrix->getType()->isIntegerTy(64)) {
    matrix = theBuilder->CreateSExtOrTrunc(matrix, theBuilder->getInt64Ty());
  }
  if (!row->getType()->isIntegerTy(64)) {
    row = theBuilder->CreateSExtOrTrunc(row, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(row_fn, {matrix, row});
  free_owned_resource_temp_if_tracked(matrix);
  if (resource_effect_operation_depth_ == 0) {
    emit_runtime_error_guard_return();
  }
  track_owned_resource_temp(out, TempResourceKind::List);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCMatrixRowsSlice* node) {
  const bool is_f64 = styio_value_family_from_type_name(node->elem_type) == StyioValueFamily::Float;
  llvm::FunctionCallee slice_fn = theModule->getOrInsertFunction(
    is_f64 ? "styio_matrix_rows_slice_f64" : "styio_matrix_rows_slice_i64",
    llvm::FunctionType::get(
      theBuilder->getInt64Ty(),
      {
        theBuilder->getInt64Ty(),
        theBuilder->getInt64Ty(),
        theBuilder->getInt64Ty(),
        theBuilder->getInt32Ty(),
      },
      false));
  llvm::Value* matrix = node->matrix->toLLVMIR(this);
  llvm::Value* start = node->start->toLLVMIR(this);
  llvm::Value* end = node->end != nullptr
    ? node->end->toLLVMIR(this)
    : theBuilder->getInt64(0);
  if (!matrix->getType()->isIntegerTy(64)) {
    matrix = theBuilder->CreateSExtOrTrunc(matrix, theBuilder->getInt64Ty());
  }
  if (!start->getType()->isIntegerTy(64)) {
    start = theBuilder->CreateSExtOrTrunc(start, theBuilder->getInt64Ty());
  }
  if (!end->getType()->isIntegerTy(64)) {
    end = theBuilder->CreateSExtOrTrunc(end, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(
    slice_fn,
    {matrix, start, end, theBuilder->getInt32(node->end != nullptr ? 1 : 0)});
  free_owned_resource_temp_if_tracked(matrix);
  if (resource_effect_operation_depth_ == 0) {
    emit_runtime_error_guard_return();
  }
  track_owned_resource_temp(out, TempResourceKind::List);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCMatrixToString* node) {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee str_fn = theModule->getOrInsertFunction(
    "styio_matrix_to_cstr",
    llvm::FunctionType::get(char_ptr, {theBuilder->getInt64Ty()}, false));
  llvm::Value* matrix = node->matrix->toLLVMIR(this);
  if (!matrix->getType()->isIntegerTy(64)) {
    matrix = theBuilder->CreateSExtOrTrunc(matrix, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(str_fn, {matrix});
  free_owned_resource_temp_if_tracked(matrix);
  emit_runtime_error_guard_return();
  track_owned_cstr_temp(out);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCDictClone* node) {
  llvm::FunctionCallee clone_fn = theModule->getOrInsertFunction(
    "styio_dict_clone",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {theBuilder->getInt64Ty()}, false));
  llvm::Value* src = node->source->toLLVMIR(this);
  if (!src->getType()->isIntegerTy(64)) {
    src = theBuilder->CreateSExtOrTrunc(src, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(clone_fn, {src});
  track_owned_resource_temp(out, TempResourceKind::Dict);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCDictLen* node) {
  llvm::FunctionCallee len_fn = theModule->getOrInsertFunction(
    "styio_dict_len",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {theBuilder->getInt64Ty()}, false));
  llvm::Value* dict = node->dict->toLLVMIR(this);
  if (!dict->getType()->isIntegerTy(64)) {
    dict = theBuilder->CreateSExtOrTrunc(dict, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(len_fn, {dict});
  free_owned_resource_temp_if_tracked(dict);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCDictGet* node) {
  StyioValueFamily value_family = styio_value_family_from_type_name(node->value_type);
  const bool string_value = value_family == StyioValueFamily::String;
  const bool float_value = value_family == StyioValueFamily::Float;
  const bool bool_value = value_family == StyioValueFamily::Bool;
  const bool list_value = value_family == StyioValueFamily::ListHandle;
  const bool dict_value = value_family == StyioValueFamily::DictHandle;
  llvm::Type* result_type = string_value
    ? static_cast<llvm::Type*>(llvm::PointerType::get(*theContext, 0))
    : (float_value
        ? static_cast<llvm::Type*>(theBuilder->getDoubleTy())
        : static_cast<llvm::Type*>(theBuilder->getInt64Ty()));
  llvm::FunctionCallee get_fn = theModule->getOrInsertFunction(
    string_value
      ? "styio_dict_get_cstr"
      : (float_value
          ? "styio_dict_get_f64"
          : (bool_value
              ? "styio_dict_get_bool"
              : (list_value
                  ? "styio_dict_get_list"
                  : (dict_value ? "styio_dict_get_dict" : "styio_dict_get_i64")))),
    llvm::FunctionType::get(
      result_type,
      {theBuilder->getInt64Ty(), llvm::PointerType::get(*theContext, 0)},
      false));
  llvm::Value* dict = node->dict->toLLVMIR(this);
  llvm::Value* key = node->key->toLLVMIR(this);
  if (!dict->getType()->isIntegerTy(64)) {
    dict = theBuilder->CreateSExtOrTrunc(dict, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(get_fn, {dict, key});
  free_owned_cstr_temp_if_tracked(key);
  free_owned_resource_temp_if_tracked(dict);
  if (resource_effect_operation_depth_ == 0) {
    emit_runtime_error_guard_return();
  }
  if (string_value) {
    track_owned_cstr_temp(out);
  }
  if (list_value) {
    track_owned_resource_temp(out, TempResourceKind::List);
  }
  if (dict_value) {
    track_owned_resource_temp(out, TempResourceKind::Dict);
  }
  if (bool_value) {
    return theBuilder->CreateICmpNE(out, theBuilder->getInt64(0));
  }
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCDictSet* node) {
  StyioValueFamily value_family = styio_value_family_from_type_name(node->value_type);
  llvm::Type* set_value_type = theBuilder->getInt64Ty();
  const char* set_name = "styio_dict_set_i64";
  switch (value_family) {
    case StyioValueFamily::Bool:
      set_name = "styio_dict_set_bool";
      break;
    case StyioValueFamily::Float:
      set_name = "styio_dict_set_f64";
      set_value_type = theBuilder->getDoubleTy();
      break;
    case StyioValueFamily::String:
      set_name = "styio_dict_set_cstr";
      set_value_type = llvm::PointerType::get(*theContext, 0);
      break;
    case StyioValueFamily::ListHandle:
      set_name = "styio_dict_set_list";
      break;
    case StyioValueFamily::DictHandle:
      set_name = "styio_dict_set_dict";
      break;
    case StyioValueFamily::Integer:
    default:
      break;
  }
  llvm::FunctionCallee set_fn = theModule->getOrInsertFunction(
    set_name,
    llvm::FunctionType::get(
      theBuilder->getVoidTy(),
      {theBuilder->getInt64Ty(), llvm::PointerType::get(*theContext, 0), set_value_type},
      false));
  llvm::Value* dict = node->dict->toLLVMIR(this);
  llvm::Value* key = node->key->toLLVMIR(this);
  llvm::Value* value = node->value->toLLVMIR(this);
  if (!dict->getType()->isIntegerTy(64)) {
    dict = theBuilder->CreateSExtOrTrunc(dict, theBuilder->getInt64Ty());
  }
  value = styio_coerce_collection_value(
    value,
    value_family,
    theBuilder.get(),
    "dict set");
  theBuilder->CreateCall(set_fn, {dict, key, value});
  free_owned_cstr_temp_if_tracked(key);
  if (value_family == StyioValueFamily::String) {
    free_owned_cstr_temp_if_tracked(value);
  }
  else if (value_family == StyioValueFamily::ListHandle
           || value_family == StyioValueFamily::DictHandle) {
    free_owned_resource_temp_if_tracked(value);
  }
  free_owned_resource_temp_if_tracked(dict);
  return nullptr;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCDictKeys* node) {
  llvm::FunctionCallee keys_fn = theModule->getOrInsertFunction(
    "styio_dict_keys",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {theBuilder->getInt64Ty()}, false));
  llvm::Value* dict = node->dict->toLLVMIR(this);
  if (!dict->getType()->isIntegerTy(64)) {
    dict = theBuilder->CreateSExtOrTrunc(dict, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(keys_fn, {dict});
  free_owned_resource_temp_if_tracked(dict);
  track_owned_resource_temp(out, TempResourceKind::List);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCDictValues* node) {
  StyioValueFamily value_family = styio_value_family_from_type_name(node->value_type);
  llvm::FunctionCallee values_fn = theModule->getOrInsertFunction(
    value_family == StyioValueFamily::String
      ? "styio_dict_values_cstr"
      : (value_family == StyioValueFamily::Float
          ? "styio_dict_values_f64"
          : (value_family == StyioValueFamily::Bool
              ? "styio_dict_values_bool"
              : (value_family == StyioValueFamily::ListHandle
                  ? "styio_dict_values_list"
                  : (value_family == StyioValueFamily::DictHandle
                      ? "styio_dict_values_dict"
                      : "styio_dict_values_i64")))),
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {theBuilder->getInt64Ty()}, false));
  llvm::Value* dict = node->dict->toLLVMIR(this);
  if (!dict->getType()->isIntegerTy(64)) {
    dict = theBuilder->CreateSExtOrTrunc(dict, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(values_fn, {dict});
  free_owned_resource_temp_if_tracked(dict);
  track_owned_resource_temp(out, TempResourceKind::List);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SCDictToString* node) {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee str_fn = theModule->getOrInsertFunction(
    "styio_dict_to_cstr",
    llvm::FunctionType::get(char_ptr, {theBuilder->getInt64Ty()}, false));
  llvm::Value* dict = node->dict->toLLVMIR(this);
  if (!dict->getType()->isIntegerTy(64)) {
    dict = theBuilder->CreateSExtOrTrunc(dict, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(str_fn, {dict});
  free_owned_resource_temp_if_tracked(dict);
  track_owned_cstr_temp(out);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SIOStreamZip* node) {
  if (!node->barrier_facts.is_canonical()) {
    throw StyioTypeError(
      "SIOStreamZip.barrier_facts must be canonical before codegen");
  }

  const bool materialized_list_zip =
    !node->a_is_file && !node->b_is_file
    && !node->a_is_stdin && !node->b_is_stdin;

  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::IntegerType* i64t = theBuilder->getInt64Ty();
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::Value* z32 = theBuilder->getInt32(0);
  llvm::Value* zero = llvm::ConstantInt::get(i64t, 0);
  llvm::Value* one = llvm::ConstantInt::get(i64t, 1);

  llvm::FunctionCallee open_fn = theModule->getOrInsertFunction(
    "styio_file_open",
    llvm::FunctionType::get(i64t, {char_ptr}, false));
  llvm::FunctionCallee read_fn = theModule->getOrInsertFunction(
    "styio_file_read_line",
    llvm::FunctionType::get(char_ptr, {i64t}, false));
  auto stdin_read_fn = [&]() {
    return theModule->getOrInsertFunction(
      "styio_stdin_read_line",
      llvm::FunctionType::get(char_ptr, {}, false));
  };
  auto register_temp_file_handle = [&](llvm::AllocaInst* slot, const char* label)
  {
    if (slot == nullptr) {
      return std::string();
    }
    std::string name =
      std::string("__styio_") + label + "_" + std::to_string(file_handle_temp_counter_++);
    mutable_variables[name] = slot;
    register_file_handle_for_raii(name);
    return name;
  };
  auto close_temp_file_handle = [&](llvm::AllocaInst* slot, const std::string& name)
  {
    if (slot != nullptr) {
      emit_file_handle_slot_close(slot);
    }
    if (!name.empty()) {
      mutable_variables.erase(name);
    }
  };
  auto guard_runtime_error_if_unwrapped = [&]()
  {
    if (resource_effect_operation_depth_ == 0) {
      emit_runtime_error_guard_return();
    }
  };
  auto guard_cleanup_error_if_unwrapped = [&]()
  {
    if (resource_effect_operation_depth_ == 0) {
      emit_runtime_error_guard_return_after_cleanup();
    }
  };

  auto* lit_a = dynamic_cast<SCListLiteral*>(node->iterable_a);
  auto* lit_b = dynamic_cast<SCListLiteral*>(node->iterable_b);
  const bool static_literal_zip =
    !node->a_is_file && !node->b_is_file && !node->a_is_stdin && !node->b_is_stdin
    && stream_zip_static_literal_supported(lit_a)
    && stream_zip_static_literal_supported(lit_b);

  llvm::AllocaInst* ledger_alloc = nullptr;
  llvm::AllocaInst* snap_alloc = nullptr;
  int pulse_sz = 0;
  if (node->pulse_plan && node->pulse_plan->total_bytes > 0) {
    pulse_sz = node->pulse_plan->total_bytes;
    llvm::ArrayType* paty =
      llvm::ArrayType::get(theBuilder->getInt8Ty(), static_cast<unsigned>(pulse_sz));
    ledger_alloc = theBuilder->CreateAlloca(paty, nullptr, "pulse_ledger_z");
    snap_alloc = theBuilder->CreateAlloca(paty, nullptr, "pulse_snap_z");
    llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
    llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
    llvm::Value* si8 = theBuilder->CreateBitCast(snap_alloc, i8p);
    theBuilder->CreateMemSet(
      li8,
      llvm::ConstantInt::get(theBuilder->getInt8Ty(), 0),
      llvm::ConstantInt::get(i64t, pulse_sz),
      llvm::MaybeAlign(8));
    theBuilder->CreateMemSet(
      si8,
      llvm::ConstantInt::get(theBuilder->getInt8Ty(), 0),
      llvm::ConstantInt::get(i64t, pulse_sz),
      llvm::MaybeAlign(8));
  }

  auto run_pulse_prologue = [&]() {
    if (pulse_sz > 0) {
      llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
      llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
      llvm::Value* si8 = theBuilder->CreateBitCast(snap_alloc, i8p);
      pulse_copy_ledger_to_snap(li8, si8, pulse_sz);
      pulse_ledger_base_ = li8;
      pulse_snap_base_ = si8;
      pulse_active_plan_ = node->pulse_plan.get();
    }
  };
  auto run_pulse_epilogue = [&]() {
    if (pulse_sz > 0) {
      llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
      llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
      emit_pulse_commit_all(li8, node->pulse_plan.get());
      pulse_ledger_base_ = nullptr;
      pulse_snap_base_ = nullptr;
      pulse_active_plan_ = nullptr;
    }
    emit_bounded_ring_pending_commits();
  };
  auto abandon_uncommitted_pulse_frame = [&]() {
    pulse_ledger_base_ = nullptr;
    pulse_snap_base_ = nullptr;
    pulse_active_plan_ = nullptr;
  };

  auto finish_zip = [&]() {
    if (pulse_sz > 0 && node->pulse_region_id >= 0) {
      llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
      llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
      pulse_region_ledgers_[node->pulse_region_id] = {li8, node->pulse_plan.get()};
    }
  };

  auto zip_value_family = [](const std::string& elem_type)
  {
    return styio_value_family_from_type_name(elem_type.empty() ? std::string("i64") : elem_type);
  };
  auto zip_slot_type = [&](StyioValueFamily family) -> llvm::Type*
  {
    if (family == StyioValueFamily::String) {
      return llvm::PointerType::get(*theContext, 0);
    }
    if (family == StyioValueFamily::Char) {
      return theBuilder->getInt8Ty();
    }
    if (family == StyioValueFamily::Float) {
      return theBuilder->getDoubleTy();
    }
    if (family == StyioValueFamily::Bool) {
      return theBuilder->getInt1Ty();
    }
    return i64t;
  };
  auto zip_get_type = [&](StyioValueFamily family) -> llvm::Type*
  {
    if (family == StyioValueFamily::String) {
      return llvm::PointerType::get(*theContext, 0);
    }
    if (family == StyioValueFamily::Char) {
      return theBuilder->getInt8Ty();
    }
    if (family == StyioValueFamily::Float) {
      return theBuilder->getDoubleTy();
    }
    return i64t;
  };
  auto zip_get_name = [](StyioValueFamily family) -> const char*
  {
    if (family == StyioValueFamily::String) {
      return "styio_list_get_cstr";
    }
    if (family == StyioValueFamily::Float) {
      return "styio_list_get_f64";
    }
    if (family == StyioValueFamily::Char) {
      return "styio_list_get_char";
    }
    if (family == StyioValueFamily::Bool) {
      return "styio_list_get_bool";
    }
    if (family == StyioValueFamily::ListHandle) {
      return "styio_list_get_list";
    }
    if (family == StyioValueFamily::DictHandle) {
      return "styio_list_get_dict";
    }
    if (family == StyioValueFamily::MatrixHandle) {
      return "styio_list_get_matrix";
    }
    return "styio_list_get";
  };
  auto release_zip_elem = [&](StyioValueFamily family, llvm::AllocaInst* slot)
  {
    if (family == StyioValueFamily::String) {
      llvm::Value* cur = theBuilder->CreateLoad(zip_slot_type(family), slot);
      free_cstr_if_runtime_owned(cur);
    }
    else if (family == StyioValueFamily::ListHandle) {
      llvm::Value* cur = theBuilder->CreateLoad(i64t, slot);
      theBuilder->CreateCall(list_release_fn(), {cur});
    }
    else if (family == StyioValueFamily::DictHandle) {
      llvm::Value* cur = theBuilder->CreateLoad(i64t, slot);
      theBuilder->CreateCall(dict_release_fn(), {cur});
    }
    else if (family == StyioValueFamily::MatrixHandle) {
      llvm::Value* cur = theBuilder->CreateLoad(i64t, slot);
      theBuilder->CreateCall(matrix_release_fn(), {cur});
    }
  };
  auto unsupported_zip = []() -> void
  {
    throw StyioTypeError(
      "unsupported stream zip lowering (supported sources: list literals, "
      "materialized list handles, @file streams, @stdin streams, and finite stream/list pairs)");
  };

  if (materialized_list_zip && !static_literal_zip) {
    if (!ir_yields_list_handle(node->iterable_a) || !ir_yields_list_handle(node->iterable_b)) {
      unsupported_zip();
    }
    StyioValueFamily family_a = zip_value_family(node->a_elem_type);
    StyioValueFamily family_b = zip_value_family(node->b_elem_type);
    llvm::FunctionCallee len_fn = theModule->getOrInsertFunction(
      "styio_list_len",
      llvm::FunctionType::get(i64t, {i64t}, false));
    llvm::FunctionCallee get_a_fn = theModule->getOrInsertFunction(
      zip_get_name(family_a),
      llvm::FunctionType::get(zip_get_type(family_a), {i64t, i64t}, false));
    llvm::FunctionCallee get_b_fn = theModule->getOrInsertFunction(
      zip_get_name(family_b),
      llvm::FunctionType::get(zip_get_type(family_b), {i64t, i64t}, false));

    llvm::Value* list_a = node->iterable_a->toLLVMIR(this);
    if (!list_a->getType()->isIntegerTy(64)) {
      list_a = theBuilder->CreateSExtOrTrunc(list_a, i64t);
    }
    std::optional<TempResourceKind> list_a_kind = take_owned_resource_temp(list_a);
    const bool release_list_a =
      list_a_kind.has_value() && *list_a_kind == TempResourceKind::List;

    llvm::Value* list_b = node->iterable_b->toLLVMIR(this);
    if (!list_b->getType()->isIntegerTy(64)) {
      list_b = theBuilder->CreateSExtOrTrunc(list_b, i64t);
    }
    std::optional<TempResourceKind> list_b_kind = take_owned_resource_temp(list_b);
    const bool release_list_b =
      list_b_kind.has_value() && *list_b_kind == TempResourceKind::List;

    llvm::AllocaInst* list_a_slot = theBuilder->CreateAlloca(i64t, nullptr, "zip_rt_a");
    llvm::AllocaInst* list_b_slot = theBuilder->CreateAlloca(i64t, nullptr, "zip_rt_b");
    llvm::AllocaInst* idx_slot = theBuilder->CreateAlloca(i64t, nullptr, "zip_rt_i");
    theBuilder->CreateStore(list_a, list_a_slot);
    theBuilder->CreateStore(list_b, list_b_slot);
    theBuilder->CreateStore(zero, idx_slot);

    llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "zip_rt_exit", F);
    llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(*theContext, "zip_rt_hdr", F);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, "zip_rt_body", F);
    llvm::BasicBlock* step_bb = llvm::BasicBlock::Create(*theContext, "zip_rt_step", F);
    theBuilder->CreateBr(hdr_bb);

    theBuilder->SetInsertPoint(hdr_bb);
    llvm::Value* idxv = theBuilder->CreateLoad(i64t, idx_slot);
    llvm::Value* cur_list_a = theBuilder->CreateLoad(i64t, list_a_slot);
    llvm::Value* cur_list_b = theBuilder->CreateLoad(i64t, list_b_slot);
    llvm::Value* len_a = theBuilder->CreateCall(len_fn, {cur_list_a});
    llvm::Value* len_b = theBuilder->CreateCall(len_fn, {cur_list_b});
    llvm::Value* ok_a = theBuilder->CreateICmpSLT(idxv, len_a);
    llvm::Value* ok_b = theBuilder->CreateICmpSLT(idxv, len_b);
    theBuilder->CreateCondBr(theBuilder->CreateAnd(ok_a, ok_b), body_bb, exit_bb);

    loop_stack_.push_back(LoopFrame{exit_bb, step_bb, file_handle_scope_stack_.size()});
    theBuilder->SetInsertPoint(body_bb);
    emit_snapshot_shadow_reload();
    llvm::Value* idx = theBuilder->CreateLoad(i64t, idx_slot);
    llvm::Value* elem_a = theBuilder->CreateCall(
      get_a_fn,
      {theBuilder->CreateLoad(i64t, list_a_slot), idx});
    llvm::Value* elem_b = theBuilder->CreateCall(
      get_b_fn,
      {theBuilder->CreateLoad(i64t, list_b_slot), idx});
    if (family_a == StyioValueFamily::Bool) {
      elem_a = theBuilder->CreateICmpNE(elem_a, theBuilder->getInt64(0));
    }
    if (family_b == StyioValueFamily::Bool) {
      elem_b = theBuilder->CreateICmpNE(elem_b, theBuilder->getInt64(0));
    }

    llvm::AllocaInst* slot_a =
      theBuilder->CreateAlloca(zip_slot_type(family_a), nullptr, node->var_a);
    llvm::AllocaInst* slot_b =
      theBuilder->CreateAlloca(zip_slot_type(family_b), nullptr, node->var_b);
    theBuilder->CreateStore(elem_a, slot_a);
    theBuilder->CreateStore(elem_b, slot_b);
    mutable_variables[node->var_a] = slot_a;
    mutable_variables[node->var_b] = slot_b;

    run_pulse_prologue();
    node->body->toLLVMIR(this);

    mutable_variables.erase(node->var_a);
    mutable_variables.erase(node->var_b);

    llvm::BasicBlock* bcur = theBuilder->GetInsertBlock();
    if (bcur && !bcur->getTerminator()) {
      run_pulse_epilogue();
      release_zip_elem(family_a, slot_a);
      release_zip_elem(family_b, slot_b);
      theBuilder->CreateBr(step_bb);
    }
    else {
      abandon_uncommitted_pulse_frame();
    }

    theBuilder->SetInsertPoint(step_bb);
    llvm::Value* nx = theBuilder->CreateAdd(theBuilder->CreateLoad(i64t, idx_slot), one);
    theBuilder->CreateStore(nx, idx_slot);
    theBuilder->CreateBr(hdr_bb);

    theBuilder->SetInsertPoint(exit_bb);
    if (release_list_a) {
      theBuilder->CreateCall(list_release_fn(), {theBuilder->CreateLoad(i64t, list_a_slot)});
    }
    if (release_list_b) {
      theBuilder->CreateCall(list_release_fn(), {theBuilder->CreateLoad(i64t, list_b_slot)});
    }
    loop_stack_.pop_back();
    finish_zip();
    return theBuilder->getInt64(0);
  }

  auto emit_stream_list_zip = [&](bool stream_left) -> bool
  {
    const bool stream_is_file = stream_left ? node->a_is_file : node->b_is_file;
    const bool stream_is_stdin = stream_left ? node->a_is_stdin : node->b_is_stdin;
    if (!stream_is_file && !stream_is_stdin) {
      return false;
    }
    StyioIR* stream_ir = stream_left ? node->iterable_a : node->iterable_b;
    StyioIR* list_ir = stream_left ? node->iterable_b : node->iterable_a;
    if (!ir_yields_list_handle(list_ir)) {
      return false;
    }

    const bool stream_elem_string = stream_left ? node->a_elem_string : node->b_elem_string;
    const StyioValueFamily list_family = zip_value_family(
      stream_left ? node->b_elem_type : node->a_elem_type);
    const std::string& stream_var = stream_left ? node->var_a : node->var_b;
    const std::string& list_var = stream_left ? node->var_b : node->var_a;

    llvm::FunctionCallee len_fn = theModule->getOrInsertFunction(
      "styio_list_len",
      llvm::FunctionType::get(i64t, {i64t}, false));
    llvm::FunctionCallee get_fn = theModule->getOrInsertFunction(
      zip_get_name(list_family),
      llvm::FunctionType::get(zip_get_type(list_family), {i64t, i64t}, false));

    llvm::AllocaInst* h_slot = nullptr;
    std::string h_slot_name;
    if (stream_is_file) {
      llvm::Value* path = stream_ir->toLLVMIR(this);
      llvm::Value* h0 = theBuilder->CreateCall(open_fn, {path});
      h_slot = theBuilder->CreateAlloca(i64t, nullptr, "zip_stream_list_h");
      theBuilder->CreateStore(h0, h_slot);
      h_slot_name = register_temp_file_handle(h_slot, "zip_stream_list_h");
      guard_runtime_error_if_unwrapped();
    }
    llvm::Value* list_value = list_ir->toLLVMIR(this);
    if (!list_value->getType()->isIntegerTy(64)) {
      list_value = theBuilder->CreateSExtOrTrunc(list_value, i64t);
    }
    std::optional<TempResourceKind> list_kind = take_owned_resource_temp(list_value);
    const bool release_list =
      list_kind.has_value() && *list_kind == TempResourceKind::List;

    llvm::AllocaInst* list_slot = theBuilder->CreateAlloca(i64t, nullptr, "zip_stream_list_l");
    llvm::AllocaInst* idx_slot = theBuilder->CreateAlloca(i64t, nullptr, "zip_stream_list_i");
    theBuilder->CreateStore(list_value, list_slot);
    theBuilder->CreateStore(zero, idx_slot);

    llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "zip_stream_list_exit", F);
    llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(*theContext, "zip_stream_list_hdr", F);
    llvm::BasicBlock* read_bb = llvm::BasicBlock::Create(*theContext, "zip_stream_list_read", F);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, "zip_stream_list_body", F);
    llvm::BasicBlock* step_bb = llvm::BasicBlock::Create(*theContext, "zip_stream_list_step", F);
    theBuilder->CreateBr(hdr_bb);

    theBuilder->SetInsertPoint(hdr_bb);
    llvm::Value* idxv = theBuilder->CreateLoad(i64t, idx_slot);
    llvm::Value* list_len = theBuilder->CreateCall(
      len_fn,
      {theBuilder->CreateLoad(i64t, list_slot)});
    llvm::Value* idx_ok = theBuilder->CreateICmpSLT(idxv, list_len);
    theBuilder->CreateCondBr(idx_ok, read_bb, exit_bb);

    theBuilder->SetInsertPoint(read_bb);
    llvm::Value* line = stream_is_stdin
                          ? theBuilder->CreateCall(stdin_read_fn(), {})
                          : theBuilder->CreateCall(
                              read_fn,
                              {theBuilder->CreateLoad(i64t, h_slot)});
    llvm::Value* null_line = llvm::ConstantPointerNull::get(
      llvm::cast<llvm::PointerType>(char_ptr));
    llvm::Value* got_line = theBuilder->CreateICmpNE(line, null_line);
    theBuilder->CreateCondBr(got_line, body_bb, exit_bb);

    loop_stack_.push_back(LoopFrame{exit_bb, step_bb, file_handle_scope_stack_.size()});
    theBuilder->SetInsertPoint(body_bb);
    emit_snapshot_shadow_reload();
    llvm::Value* idx = theBuilder->CreateLoad(i64t, idx_slot);
    llvm::Value* list_elem = theBuilder->CreateCall(
      get_fn,
      {theBuilder->CreateLoad(i64t, list_slot), idx});
    if (list_family == StyioValueFamily::Bool) {
      list_elem = theBuilder->CreateICmpNE(list_elem, theBuilder->getInt64(0));
    }
    llvm::Value* stream_elem = line;
    llvm::Type* stream_slot_ty = char_ptr;
    if (!stream_elem_string) {
      stream_elem = cstr_to_i64_checked(line);
      stream_slot_ty = i64t;
    }

    llvm::AllocaInst* stream_slot =
      theBuilder->CreateAlloca(stream_slot_ty, nullptr, stream_var);
    llvm::AllocaInst* list_elem_slot =
      theBuilder->CreateAlloca(zip_slot_type(list_family), nullptr, list_var);
    theBuilder->CreateStore(stream_elem, stream_slot);
    theBuilder->CreateStore(list_elem, list_elem_slot);
    if (stream_left) {
      mutable_variables[node->var_a] = stream_slot;
      mutable_variables[node->var_b] = list_elem_slot;
    }
    else {
      mutable_variables[node->var_a] = list_elem_slot;
      mutable_variables[node->var_b] = stream_slot;
    }

    run_pulse_prologue();
    node->body->toLLVMIR(this);
    run_pulse_epilogue();

    mutable_variables.erase(node->var_a);
    mutable_variables.erase(node->var_b);

    llvm::BasicBlock* bcur = theBuilder->GetInsertBlock();
    if (bcur && !bcur->getTerminator()) {
      release_zip_elem(list_family, list_elem_slot);
      theBuilder->CreateBr(step_bb);
    }

    theBuilder->SetInsertPoint(step_bb);
    llvm::Value* nx = theBuilder->CreateAdd(theBuilder->CreateLoad(i64t, idx_slot), one);
    theBuilder->CreateStore(nx, idx_slot);
    theBuilder->CreateBr(hdr_bb);

    theBuilder->SetInsertPoint(exit_bb);
    if (stream_is_file) {
      close_temp_file_handle(h_slot, h_slot_name);
      guard_cleanup_error_if_unwrapped();
    }
    if (release_list) {
      theBuilder->CreateCall(list_release_fn(), {theBuilder->CreateLoad(i64t, list_slot)});
    }
    loop_stack_.pop_back();
    finish_zip();
    return true;
  };

  if (static_literal_zip) {
    size_t na = lit_a->elems.size();
    size_t nb = lit_b->elems.size();

    llvm::ArrayType* at_a = nullptr;
    llvm::GlobalVariable* gv_a = nullptr;
    if (node->a_elem_string) {
      std::vector<llvm::Constant*> sp;
      for (auto* e : lit_a->elems) {
        auto* ss = dynamic_cast<SGConstString*>(e);
        llvm::Constant* cp = ss ? llvm::cast<llvm::Constant>(
          theBuilder->CreateGlobalStringPtr(ss->value, "zip_sa"))
                                : llvm::ConstantPointerNull::get(
                                    llvm::cast<llvm::PointerType>(char_ptr));
        sp.push_back(cp);
      }
      at_a = llvm::ArrayType::get(char_ptr, sp.size());
      gv_a = new llvm::GlobalVariable(
        *theModule,
        at_a,
        true,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantArray::get(at_a, sp),
        "zip_lsa");
    }
    else {
      std::vector<llvm::Constant*> ca;
      for (auto* e : lit_a->elems) {
        int64_t v = 0;
        if (auto* ci = dynamic_cast<SGConstInt*>(e)) {
          v = std::stoll(ci->value);
        }
        ca.push_back(llvm::ConstantInt::get(i64t, v, true));
      }
      at_a = llvm::ArrayType::get(i64t, ca.size());
      gv_a = new llvm::GlobalVariable(
        *theModule,
        at_a,
        true,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantArray::get(at_a, ca),
        "zip_la");
    }

    llvm::ArrayType* at_b = nullptr;
    llvm::GlobalVariable* gv_b = nullptr;
    if (node->b_elem_string) {
      std::vector<llvm::Constant*> sp;
      for (auto* e : lit_b->elems) {
        auto* ss = dynamic_cast<SGConstString*>(e);
        llvm::Constant* cp = ss ? llvm::cast<llvm::Constant>(
          theBuilder->CreateGlobalStringPtr(ss->value, "zip_sb"))
                                : llvm::ConstantPointerNull::get(
                                    llvm::cast<llvm::PointerType>(char_ptr));
        sp.push_back(cp);
      }
      at_b = llvm::ArrayType::get(char_ptr, sp.size());
      gv_b = new llvm::GlobalVariable(
        *theModule,
        at_b,
        true,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantArray::get(at_b, sp),
        "zip_lsb");
    }
    else {
      std::vector<llvm::Constant*> cb;
      for (auto* e : lit_b->elems) {
        int64_t v = 0;
        if (auto* ci = dynamic_cast<SGConstInt*>(e)) {
          v = std::stoll(ci->value);
        }
        cb.push_back(llvm::ConstantInt::get(i64t, v, true));
      }
      at_b = llvm::ArrayType::get(i64t, cb.size());
      gv_b = new llvm::GlobalVariable(
        *theModule,
        at_b,
        true,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantArray::get(at_b, cb),
        "zip_lb");
    }

    llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "zip_ll_exit", F);
    llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(*theContext, "zip_ll_hdr", F);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, "zip_ll_body", F);
    llvm::BasicBlock* step_bb = llvm::BasicBlock::Create(*theContext, "zip_ll_step", F);

    llvm::AllocaInst* idx_slot = theBuilder->CreateAlloca(i64t, nullptr, "zip_i");
    theBuilder->CreateStore(zero, idx_slot);
    theBuilder->CreateBr(hdr_bb);

    theBuilder->SetInsertPoint(hdr_bb);
    llvm::Value* iv = theBuilder->CreateLoad(i64t, idx_slot);
    llvm::Value* lim_a =
      llvm::ConstantInt::get(i64t, static_cast<uint64_t>(na), /*signed=*/true);
    llvm::Value* lim_b =
      llvm::ConstantInt::get(i64t, static_cast<uint64_t>(nb), /*signed=*/true);
    llvm::Value* ready_a = theBuilder->CreateICmpSLT(iv, lim_a);
    llvm::Value* ready_b = theBuilder->CreateICmpSLT(iv, lim_b);
    theBuilder->CreateCondBr(
      theBuilder->CreateAnd(ready_a, ready_b), body_bb, exit_bb);

    loop_stack_.push_back(LoopFrame{exit_bb, step_bb, file_handle_scope_stack_.size()});
    theBuilder->SetInsertPoint(body_bb);
    emit_snapshot_shadow_reload();
    llvm::Value* idx = theBuilder->CreateLoad(i64t, idx_slot);
    llvm::Type* elem_ty_a = node->a_elem_string ? char_ptr : static_cast<llvm::Type*>(i64t);
    llvm::Value* gep_a = theBuilder->CreateInBoundsGEP(at_a, gv_a, {z32, idx});
    llvm::Value* ev_a = node->a_elem_string ? theBuilder->CreateLoad(char_ptr, gep_a)
                                            : theBuilder->CreateLoad(i64t, gep_a);
    llvm::Type* elem_ty_b = node->b_elem_string ? char_ptr : static_cast<llvm::Type*>(i64t);
    llvm::Value* gep_b = theBuilder->CreateInBoundsGEP(at_b, gv_b, {z32, idx});
    llvm::Value* ev_b = node->b_elem_string ? theBuilder->CreateLoad(char_ptr, gep_b)
                                            : theBuilder->CreateLoad(i64t, gep_b);

    llvm::AllocaInst* slot_a = theBuilder->CreateAlloca(elem_ty_a, nullptr, node->var_a);
    llvm::AllocaInst* slot_b = theBuilder->CreateAlloca(elem_ty_b, nullptr, node->var_b);
    theBuilder->CreateStore(ev_a, slot_a);
    theBuilder->CreateStore(ev_b, slot_b);
    mutable_variables[node->var_a] = slot_a;
    mutable_variables[node->var_b] = slot_b;

    run_pulse_prologue();
    node->body->toLLVMIR(this);

    mutable_variables.erase(node->var_a);
    mutable_variables.erase(node->var_b);

    llvm::BasicBlock* bcur = theBuilder->GetInsertBlock();
    if (bcur && !bcur->getTerminator()) {
      run_pulse_epilogue();
      theBuilder->CreateBr(step_bb);
    }
    else {
      abandon_uncommitted_pulse_frame();
    }

    theBuilder->SetInsertPoint(step_bb);
    llvm::Value* nx = theBuilder->CreateAdd(theBuilder->CreateLoad(i64t, idx_slot), one);
    theBuilder->CreateStore(nx, idx_slot);
    theBuilder->CreateBr(hdr_bb);

    theBuilder->SetInsertPoint(exit_bb);
    loop_stack_.pop_back();
    finish_zip();
    return theBuilder->getInt64(0);
  }

  if (lit_a && node->b_is_file) {
    size_t na = lit_a->elems.size();
    llvm::ArrayType* at_a = nullptr;
    llvm::GlobalVariable* gv_a = nullptr;
    if (node->a_elem_string) {
      std::vector<llvm::Constant*> sp;
      for (auto* e : lit_a->elems) {
        auto* ss = dynamic_cast<SGConstString*>(e);
        llvm::Constant* cp = ss ? llvm::cast<llvm::Constant>(
          theBuilder->CreateGlobalStringPtr(ss->value, "zip_lfa"))
                                : llvm::ConstantPointerNull::get(
                                    llvm::cast<llvm::PointerType>(char_ptr));
        sp.push_back(cp);
      }
      at_a = llvm::ArrayType::get(char_ptr, sp.size());
      gv_a = new llvm::GlobalVariable(
        *theModule,
        at_a,
        true,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantArray::get(at_a, sp),
        "zip_lfsa");
    }
    else {
      std::vector<llvm::Constant*> ca;
      for (auto* e : lit_a->elems) {
        int64_t v = 0;
        if (auto* ci = dynamic_cast<SGConstInt*>(e)) {
          v = std::stoll(ci->value);
        }
        ca.push_back(llvm::ConstantInt::get(i64t, v, true));
      }
      at_a = llvm::ArrayType::get(i64t, ca.size());
      gv_a = new llvm::GlobalVariable(
        *theModule,
        at_a,
        true,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantArray::get(at_a, ca),
        "zip_lfa");
    }

    llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "zip_lf_exit", F);
    llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(*theContext, "zip_lf_hdr", F);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, "zip_lf_body", F);
    llvm::BasicBlock* step_bb = llvm::BasicBlock::Create(*theContext, "zip_lf_step", F);

    llvm::Value* pb = node->iterable_b->toLLVMIR(this);
    llvm::Value* h0 = theBuilder->CreateCall(open_fn, {pb});
    llvm::AllocaInst* hb = theBuilder->CreateAlloca(i64t, nullptr, "zip_lf_h");
    theBuilder->CreateStore(h0, hb);
    std::string hb_name = register_temp_file_handle(hb, "zip_lf_h");
    guard_runtime_error_if_unwrapped();

    llvm::AllocaInst* idx_slot = theBuilder->CreateAlloca(i64t, nullptr, "zip_lf_i");
    theBuilder->CreateStore(zero, idx_slot);
    theBuilder->CreateBr(hdr_bb);

    theBuilder->SetInsertPoint(hdr_bb);
    llvm::Value* iv = theBuilder->CreateLoad(i64t, idx_slot);
    llvm::Value* lim =
      llvm::ConstantInt::get(i64t, static_cast<uint64_t>(na), /*signed=*/true);
    llvm::Value* idx_ok = theBuilder->CreateICmpSLT(iv, lim);
    llvm::BasicBlock* read_bb = llvm::BasicBlock::Create(*theContext, "zip_lf_read", F);
    theBuilder->CreateCondBr(idx_ok, read_bb, exit_bb);

    theBuilder->SetInsertPoint(read_bb);
    llvm::Value* hh = theBuilder->CreateLoad(i64t, hb);
    llvm::Value* ln = theBuilder->CreateCall(read_fn, {hh});
    llvm::Value* null_ln = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(char_ptr));
    llvm::Value* got = theBuilder->CreateICmpNE(ln, null_ln);
    theBuilder->CreateCondBr(got, body_bb, exit_bb);

    loop_stack_.push_back(LoopFrame{exit_bb, step_bb, file_handle_scope_stack_.size()});
    theBuilder->SetInsertPoint(body_bb);
    emit_snapshot_shadow_reload();
    llvm::Value* idx = theBuilder->CreateLoad(i64t, idx_slot);
    llvm::Type* elem_ty_a = node->a_elem_string ? char_ptr : static_cast<llvm::Type*>(i64t);
    llvm::Value* gep_a = theBuilder->CreateInBoundsGEP(at_a, gv_a, {z32, idx});
    llvm::Value* ev_a = node->a_elem_string ? theBuilder->CreateLoad(char_ptr, gep_a)
                                            : theBuilder->CreateLoad(i64t, gep_a);
    llvm::AllocaInst* slot_a = theBuilder->CreateAlloca(elem_ty_a, nullptr, node->var_a);
    llvm::Value* val_b = ln;
    llvm::Type* slot_ty_b = char_ptr;
    if (!node->b_elem_string) {
      val_b = cstr_to_i64_checked(ln);
      slot_ty_b = i64t;
    }
    llvm::AllocaInst* slot_b = theBuilder->CreateAlloca(slot_ty_b, nullptr, node->var_b);
    theBuilder->CreateStore(ev_a, slot_a);
    theBuilder->CreateStore(val_b, slot_b);
    mutable_variables[node->var_a] = slot_a;
    mutable_variables[node->var_b] = slot_b;

    run_pulse_prologue();
    node->body->toLLVMIR(this);
    run_pulse_epilogue();

    mutable_variables.erase(node->var_a);
    mutable_variables.erase(node->var_b);

    llvm::BasicBlock* b2 = theBuilder->GetInsertBlock();
    if (b2 && !b2->getTerminator()) {
      theBuilder->CreateBr(step_bb);
    }

    theBuilder->SetInsertPoint(step_bb);
    llvm::Value* nx = theBuilder->CreateAdd(theBuilder->CreateLoad(i64t, idx_slot), one);
    theBuilder->CreateStore(nx, idx_slot);
    theBuilder->CreateBr(hdr_bb);

    theBuilder->SetInsertPoint(exit_bb);
    close_temp_file_handle(hb, hb_name);
    guard_cleanup_error_if_unwrapped();
    loop_stack_.pop_back();
    finish_zip();
    return theBuilder->getInt64(0);
  }

  if (node->a_is_file && lit_b) {
    size_t nb = lit_b->elems.size();
    llvm::ArrayType* at_b = nullptr;
    llvm::GlobalVariable* gv_b = nullptr;
    if (node->b_elem_string) {
      std::vector<llvm::Constant*> sp;
      for (auto* e : lit_b->elems) {
        auto* ss = dynamic_cast<SGConstString*>(e);
        llvm::Constant* cp = ss ? llvm::cast<llvm::Constant>(
          theBuilder->CreateGlobalStringPtr(ss->value, "zip_flsb"))
                                : llvm::ConstantPointerNull::get(
                                    llvm::cast<llvm::PointerType>(char_ptr));
        sp.push_back(cp);
      }
      at_b = llvm::ArrayType::get(char_ptr, sp.size());
      gv_b = new llvm::GlobalVariable(
        *theModule,
        at_b,
        true,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantArray::get(at_b, sp),
        "zip_flsb");
    }
    else {
      std::vector<llvm::Constant*> cb;
      for (auto* e : lit_b->elems) {
        int64_t v = 0;
        if (auto* ci = dynamic_cast<SGConstInt*>(e)) {
          v = std::stoll(ci->value);
        }
        cb.push_back(llvm::ConstantInt::get(i64t, v, true));
      }
      at_b = llvm::ArrayType::get(i64t, cb.size());
      gv_b = new llvm::GlobalVariable(
        *theModule,
        at_b,
        true,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantArray::get(at_b, cb),
        "zip_flb");
    }

    llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "zip_fl_exit", F);
    llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(*theContext, "zip_fl_hdr", F);
    llvm::BasicBlock* read_bb = llvm::BasicBlock::Create(*theContext, "zip_fl_read", F);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, "zip_fl_body", F);
    llvm::BasicBlock* step_bb = llvm::BasicBlock::Create(*theContext, "zip_fl_step", F);

    llvm::Value* pa = node->iterable_a->toLLVMIR(this);
    llvm::Value* h0a = theBuilder->CreateCall(open_fn, {pa});
    llvm::AllocaInst* ha = theBuilder->CreateAlloca(i64t, nullptr, "zip_fl_h");
    theBuilder->CreateStore(h0a, ha);
    std::string ha_name = register_temp_file_handle(ha, "zip_fl_h");
    guard_runtime_error_if_unwrapped();

    llvm::AllocaInst* idx_slot = theBuilder->CreateAlloca(i64t, nullptr, "zip_fl_i");
    theBuilder->CreateStore(zero, idx_slot);
    theBuilder->CreateBr(hdr_bb);

    theBuilder->SetInsertPoint(hdr_bb);
    llvm::Value* iv = theBuilder->CreateLoad(i64t, idx_slot);
    llvm::Value* lim =
      llvm::ConstantInt::get(i64t, static_cast<uint64_t>(nb), /*signed=*/true);
    llvm::Value* idx_ok = theBuilder->CreateICmpSLT(iv, lim);
    theBuilder->CreateCondBr(idx_ok, read_bb, exit_bb);

    theBuilder->SetInsertPoint(read_bb);
    llvm::Value* hh = theBuilder->CreateLoad(i64t, ha);
    llvm::Value* ln = theBuilder->CreateCall(read_fn, {hh});
    llvm::Value* null_ln = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(char_ptr));
    llvm::Value* got = theBuilder->CreateICmpNE(ln, null_ln);
    theBuilder->CreateCondBr(got, body_bb, exit_bb);

    loop_stack_.push_back(LoopFrame{exit_bb, step_bb, file_handle_scope_stack_.size()});
    theBuilder->SetInsertPoint(body_bb);
    emit_snapshot_shadow_reload();
    llvm::Value* idx = theBuilder->CreateLoad(i64t, idx_slot);
    llvm::Type* elem_ty_b = node->b_elem_string ? char_ptr : static_cast<llvm::Type*>(i64t);
    llvm::Value* gep_b = theBuilder->CreateInBoundsGEP(at_b, gv_b, {z32, idx});
    llvm::Value* ev_b = node->b_elem_string ? theBuilder->CreateLoad(char_ptr, gep_b)
                                            : theBuilder->CreateLoad(i64t, gep_b);
    llvm::Value* val_a = ln;
    llvm::Type* slot_ty_a = char_ptr;
    if (!node->a_elem_string) {
      val_a = cstr_to_i64_checked(ln);
      slot_ty_a = i64t;
    }
    llvm::AllocaInst* slot_a = theBuilder->CreateAlloca(slot_ty_a, nullptr, node->var_a);
    llvm::AllocaInst* slot_b = theBuilder->CreateAlloca(elem_ty_b, nullptr, node->var_b);
    theBuilder->CreateStore(val_a, slot_a);
    theBuilder->CreateStore(ev_b, slot_b);
    mutable_variables[node->var_a] = slot_a;
    mutable_variables[node->var_b] = slot_b;

    run_pulse_prologue();
    node->body->toLLVMIR(this);
    run_pulse_epilogue();

    mutable_variables.erase(node->var_a);
    mutable_variables.erase(node->var_b);

    llvm::BasicBlock* b2 = theBuilder->GetInsertBlock();
    if (b2 && !b2->getTerminator()) {
      theBuilder->CreateBr(step_bb);
    }

    theBuilder->SetInsertPoint(step_bb);
    llvm::Value* nx = theBuilder->CreateAdd(theBuilder->CreateLoad(i64t, idx_slot), one);
    theBuilder->CreateStore(nx, idx_slot);
    theBuilder->CreateBr(hdr_bb);

    theBuilder->SetInsertPoint(exit_bb);
    close_temp_file_handle(ha, ha_name);
    guard_cleanup_error_if_unwrapped();
    loop_stack_.pop_back();
    finish_zip();
    return theBuilder->getInt64(0);
  }

  if (!node->a_is_file
      && !node->a_is_stdin
      && (node->b_is_file || node->b_is_stdin)
      && emit_stream_list_zip(false)) {
    return theBuilder->getInt64(0);
  }

  if ((node->a_is_file || node->a_is_stdin)
      && !node->b_is_file
      && !node->b_is_stdin
      && emit_stream_list_zip(true)) {
    return theBuilder->getInt64(0);
  }

  if ((node->a_is_file || node->a_is_stdin) && (node->b_is_file || node->b_is_stdin)) {
    const bool both_files = node->a_is_file && node->b_is_file;
    llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(
      *theContext,
      both_files ? "zip_ff_exit" : "zip_streams_exit",
      F);
    llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(
      *theContext,
      both_files ? "zip_ff_hdr" : "zip_streams_hdr",
      F);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(
      *theContext,
      both_files ? "zip_ff_body" : "zip_streams_body",
      F);

    llvm::AllocaInst* ha = nullptr;
    llvm::AllocaInst* hb = nullptr;
    std::string ha_name;
    std::string hb_name;
    if (node->a_is_file) {
      llvm::Value* pa = node->iterable_a->toLLVMIR(this);
      ha = theBuilder->CreateAlloca(
        i64t,
        nullptr,
        both_files ? "zip_ff_ha" : "zip_streams_ha");
      llvm::Value* h0a = theBuilder->CreateCall(open_fn, {pa});
      theBuilder->CreateStore(h0a, ha);
      ha_name = register_temp_file_handle(
        ha,
        both_files ? "zip_ff_ha" : "zip_streams_ha");
      guard_runtime_error_if_unwrapped();
    }
    if (node->b_is_file) {
      llvm::Value* pb = node->iterable_b->toLLVMIR(this);
      hb = theBuilder->CreateAlloca(
        i64t,
        nullptr,
        both_files ? "zip_ff_hb" : "zip_streams_hb");
      llvm::Value* h0b = theBuilder->CreateCall(open_fn, {pb});
      theBuilder->CreateStore(h0b, hb);
      hb_name = register_temp_file_handle(
        hb,
        both_files ? "zip_ff_hb" : "zip_streams_hb");
      guard_runtime_error_if_unwrapped();
    }

    theBuilder->CreateBr(hdr_bb);
    theBuilder->SetInsertPoint(hdr_bb);
    llvm::Value* la = node->a_is_stdin
                        ? theBuilder->CreateCall(stdin_read_fn(), {})
                        : theBuilder->CreateCall(read_fn, {theBuilder->CreateLoad(i64t, ha)});
    llvm::Value* lb = node->b_is_stdin
                        ? theBuilder->CreateCall(stdin_read_fn(), {})
                        : theBuilder->CreateCall(read_fn, {theBuilder->CreateLoad(i64t, hb)});
    llvm::Value* null_ln = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(char_ptr));
    llvm::Value* da = theBuilder->CreateICmpEQ(la, null_ln);
    llvm::Value* db = theBuilder->CreateICmpEQ(lb, null_ln);
    llvm::Value* stop = theBuilder->CreateOr(da, db);
    theBuilder->CreateCondBr(stop, exit_bb, body_bb);

    loop_stack_.push_back(LoopFrame{exit_bb, hdr_bb, file_handle_scope_stack_.size()});
    theBuilder->SetInsertPoint(body_bb);
    emit_snapshot_shadow_reload();
    llvm::Value* val_a = la;
    llvm::Value* val_b = lb;
    llvm::Type* slot_ty_a = char_ptr;
    llvm::Type* slot_ty_b = char_ptr;
    if (!node->a_elem_string) {
      val_a = cstr_to_i64_checked(la);
      slot_ty_a = i64t;
    }
    if (!node->b_elem_string) {
      val_b = cstr_to_i64_checked(lb);
      slot_ty_b = i64t;
    }
    llvm::AllocaInst* slot_a = theBuilder->CreateAlloca(slot_ty_a, nullptr, node->var_a);
    llvm::AllocaInst* slot_b = theBuilder->CreateAlloca(slot_ty_b, nullptr, node->var_b);
    theBuilder->CreateStore(val_a, slot_a);
    theBuilder->CreateStore(val_b, slot_b);
    mutable_variables[node->var_a] = slot_a;
    mutable_variables[node->var_b] = slot_b;

    run_pulse_prologue();
    node->body->toLLVMIR(this);
    run_pulse_epilogue();

    mutable_variables.erase(node->var_a);
    mutable_variables.erase(node->var_b);

    llvm::BasicBlock* b2 = theBuilder->GetInsertBlock();
    if (b2 && !b2->getTerminator()) {
      theBuilder->CreateBr(hdr_bb);
    }

    theBuilder->SetInsertPoint(exit_bb);
    if (node->a_is_file) {
      close_temp_file_handle(ha, ha_name);
    }
    if (node->b_is_file) {
      close_temp_file_handle(hb, hb_name);
    }
    if (node->a_is_file || node->b_is_file) {
      guard_cleanup_error_if_unwrapped();
    }
    loop_stack_.pop_back();
    finish_zip();
    return theBuilder->getInt64(0);
  }

  unsupported_zip();
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SIOResourceWriteToFile* node) {
  (void)node->is_auto_path;
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee openw = theModule->getOrInsertFunction(
    "styio_file_open_write",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr}, false));
  llvm::FunctionCallee write_fn = theModule->getOrInsertFunction(
    "styio_file_write_cstr",
    llvm::FunctionType::get(
      theBuilder->getVoidTy(),
      {theBuilder->getInt64Ty(), char_ptr},
      false));
  llvm::FunctionCallee close_fn = theModule->getOrInsertFunction(
    "styio_file_close",
    llvm::FunctionType::get(theBuilder->getVoidTy(), {theBuilder->getInt64Ty()}, false));

  auto emit_data_expr = [&]() {
    llvm::Value* data = node->data_expr->toLLVMIR(this);
    if (node->promote_data_to_cstr || !data->getType()->isPointerTy()) {
      data = promote_to_cstr(data);
    }
    if (node->append_newline) {
      llvm::Value* nl = theBuilder->CreateGlobalStringPtr("\n", "styio_w_nl");
      llvm::FunctionCallee cat = theModule->getOrInsertFunction(
        "styio_strcat_ab",
        llvm::FunctionType::get(char_ptr, {char_ptr, char_ptr}, false));
      llvm::Value* with_nl = theBuilder->CreateCall(cat, {data, nl});
      free_owned_cstr_temp_if_tracked(data);
      data = with_nl;
      track_owned_cstr_temp(data);
    }
    return data;
  };

  if (!node->required_handle_var.empty()) {
    auto slot_it = mutable_variables.find(node->required_handle_var);
    if (slot_it != mutable_variables.end()) {
      llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
      llvm::Function* fn = cur != nullptr ? cur->getParent() : nullptr;
      if (cur != nullptr && fn != nullptr && cur->getTerminator() == nullptr) {
        llvm::Value* data = emit_data_expr();
        const bool data_is_owned_temp = take_owned_cstr_temp(data);
        auto emit_data_cleanup = [&]() {
          if (data_is_owned_temp) {
            free_cstr_if_runtime_owned(data);
          }
        };
        auto emit_path_write = [&]() {
          llvm::Value* path = node->path_expr->toLLVMIR(this);
          llvm::Value* h = theBuilder->CreateCall(openw, {path});
          theBuilder->CreateCall(write_fn, {h, data});
          emit_data_cleanup();
          theBuilder->CreateCall(close_fn, {h});
        };
        llvm::Value* handle = theBuilder->CreateLoad(theBuilder->getInt64Ty(), slot_it->second);
        llvm::Value* invalid = theBuilder->CreateICmpEQ(handle, theBuilder->getInt64(0));
        llvm::BasicBlock* invalid_bb =
          llvm::BasicBlock::Create(*theContext, "file_write_invalid_handle", fn);
        llvm::BasicBlock* write_bb =
          llvm::BasicBlock::Create(*theContext, "file_write_path", fn);
        llvm::BasicBlock* cont_bb =
          llvm::BasicBlock::Create(*theContext, "file_write_done", fn);
        theBuilder->CreateCondBr(invalid, invalid_bb, write_bb);

        theBuilder->SetInsertPoint(invalid_bb);
        llvm::Value* empty = theBuilder->CreateGlobalStringPtr("", "styio_invalid_write_empty");
        theBuilder->CreateCall(write_fn, {handle, empty});
        emit_data_cleanup();
        theBuilder->CreateBr(cont_bb);

        theBuilder->SetInsertPoint(write_bb);
        emit_path_write();
        if (llvm::BasicBlock* after_write = theBuilder->GetInsertBlock();
            after_write != nullptr && after_write->getTerminator() == nullptr) {
          theBuilder->CreateBr(cont_bb);
        }

        theBuilder->SetInsertPoint(cont_bb);
        if (resource_effect_operation_depth_ == 0) {
          emit_runtime_error_guard_return();
        }
        return theBuilder->getInt64(0);
      }
    }
  }

  llvm::Value* path = node->path_expr->toLLVMIR(this);
  llvm::Value* h = theBuilder->CreateCall(openw, {path});
  llvm::Value* data = emit_data_expr();
  theBuilder->CreateCall(write_fn, {h, data});
  free_owned_cstr_temp_if_tracked(data);
  theBuilder->CreateCall(close_fn, {h});
  if (resource_effect_operation_depth_ == 0) {
    emit_runtime_error_guard_return();
  }
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SIOResourceEffect* node) {
  llvm::Value* operation_value = nullptr;
  if (node->operation != nullptr) {
    ++resource_effect_operation_depth_;
    try {
      operation_value = node->operation->toLLVMIR(this);
    }
    catch (...) {
      --resource_effect_operation_depth_;
      throw;
    }
    --resource_effect_operation_depth_;
  }
  llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
  if (cur == nullptr || cur->getTerminator() != nullptr) {
    return theBuilder->getInt64(0);
  }

  llvm::FunctionCallee has_error = theModule->getOrInsertFunction(
    "styio_runtime_has_error",
    llvm::FunctionType::get(theBuilder->getInt32Ty(), false));
  llvm::FunctionCallee clear_error = theModule->getOrInsertFunction(
    "styio_runtime_clear_error",
    llvm::FunctionType::get(theBuilder->getVoidTy(), false));
  llvm::Type* result_ty = node->toLLVMType(this);
  const bool produces_value =
    node->value_required && result_ty != nullptr && !result_ty->isVoidTy();
  auto result_value = [&](llvm::Value* value) -> llvm::Value*
  {
    if (!produces_value) {
      return nullptr;
    }
    if (value == nullptr || value->getType()->isVoidTy()) {
      return default_runtime_return_value(result_ty);
    }
    return coerce_for_return(value, result_ty);
  };

  if (node->discard) {
    llvm::Function* fn = cur->getParent();
    llvm::Value* has_err = theBuilder->CreateCall(has_error, {});
    llvm::Value* bad = theBuilder->CreateICmpNE(has_err, theBuilder->getInt32(0));
    llvm::BasicBlock* clear_bb = llvm::BasicBlock::Create(*theContext, "resource_discard_clear", fn);
    llvm::BasicBlock* cont_bb = llvm::BasicBlock::Create(*theContext, "resource_discard_continue", fn);
    theBuilder->CreateCondBr(bad, clear_bb, cont_bb);

    theBuilder->SetInsertPoint(clear_bb);
    theBuilder->CreateCall(clear_error, {});
    theBuilder->CreateBr(cont_bb);

    theBuilder->SetInsertPoint(cont_bb);
    return theBuilder->getInt64(0);
  }

  if (node->fallback == nullptr && node->handlers.empty()) {
    emit_runtime_error_guard_return();
    return produces_value ? result_value(operation_value) : theBuilder->getInt64(0);
  }

  llvm::Function* fn = cur->getParent();
  llvm::Value* has_err = theBuilder->CreateCall(has_error, {});
  llvm::Value* bad = theBuilder->CreateICmpNE(has_err, theBuilder->getInt32(0));
  llvm::BasicBlock* dispatch_bb = llvm::BasicBlock::Create(*theContext, "resource_effect_dispatch", fn);
  llvm::BasicBlock* success_bb = llvm::BasicBlock::Create(*theContext, "resource_effect_success", fn);
  llvm::BasicBlock* cont_bb = llvm::BasicBlock::Create(*theContext, "resource_effect_continue", fn);
  theBuilder->CreateCondBr(bad, dispatch_bb, success_bb);

  std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> incoming_values;

  theBuilder->SetInsertPoint(success_bb);
  if (produces_value) {
    incoming_values.emplace_back(result_value(operation_value), success_bb);
  }
  theBuilder->CreateBr(cont_bb);

  theBuilder->SetInsertPoint(dispatch_bb);
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee matches_effect = theModule->getOrInsertFunction(
    "styio_runtime_error_matches_effect",
    llvm::FunctionType::get(theBuilder->getInt32Ty(), {char_ptr}, false));

  for (const auto& handler : node->handlers) {
    llvm::BasicBlock* handler_bb = llvm::BasicBlock::Create(*theContext, "resource_handler", fn);
    llvm::BasicBlock* next_bb = llvm::BasicBlock::Create(*theContext, "resource_handler_next", fn);
    llvm::Value* name = theBuilder->CreateGlobalStringPtr(handler.effect_name);
    llvm::Value* matched = theBuilder->CreateCall(matches_effect, {name});
    llvm::Value* is_match = theBuilder->CreateICmpNE(matched, theBuilder->getInt32(0));
    theBuilder->CreateCondBr(is_match, handler_bb, next_bb);

    theBuilder->SetInsertPoint(handler_bb);
    theBuilder->CreateCall(clear_error, {});
    llvm::Value* handler_value = nullptr;
    if (handler.body != nullptr) {
      handler_value = handler.body->toLLVMIR(this);
    }
    emit_runtime_error_guard_return();
    llvm::BasicBlock* after_handler = theBuilder->GetInsertBlock();
    if (after_handler != nullptr && after_handler->getTerminator() == nullptr) {
      if (produces_value) {
        incoming_values.emplace_back(result_value(handler_value), after_handler);
      }
      theBuilder->CreateBr(cont_bb);
    }

    theBuilder->SetInsertPoint(next_bb);
  }

  if (node->fallback != nullptr) {
    theBuilder->CreateCall(clear_error, {});
    llvm::Value* fallback_value = node->fallback->toLLVMIR(this);
    emit_runtime_error_guard_return();
    llvm::BasicBlock* after_fallback = theBuilder->GetInsertBlock();
    if (after_fallback != nullptr && after_fallback->getTerminator() == nullptr) {
      if (produces_value) {
        incoming_values.emplace_back(result_value(fallback_value), after_fallback);
      }
      theBuilder->CreateBr(cont_bb);
    }
  }
  else {
    emit_runtime_error_guard_return();
    llvm::BasicBlock* after_unmatched = theBuilder->GetInsertBlock();
    if (after_unmatched != nullptr && after_unmatched->getTerminator() == nullptr) {
      if (produces_value) {
        incoming_values.emplace_back(default_runtime_return_value(result_ty), after_unmatched);
      }
      theBuilder->CreateBr(cont_bb);
    }
  }

  theBuilder->SetInsertPoint(cont_bb);
  if (produces_value) {
    if (incoming_values.empty()) {
      return default_runtime_return_value(result_ty);
    }
    llvm::PHINode* phi = theBuilder->CreatePHI(
      result_ty,
      static_cast<unsigned>(incoming_values.size()),
      "resource_effect_value"
    );
    for (auto& incoming : incoming_values) {
      phi->addIncoming(incoming.first, incoming.second);
    }
    return phi;
  }
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGMatch* node) {
  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::IntegerType* i64ti = theBuilder->getInt64Ty();
  auto coerce_match_scrutinee_to_i64 = [&](llvm::Value* v) -> llvm::Value* {
    llvm::Type* ty = v->getType();
    if (ty->isIntegerTy(64)) {
      return v;
    }
    if (ty->isIntegerTy()) {
      return theBuilder->CreateSExtOrTrunc(v, i64ti);
    }
    throw StyioTypeError("match scrutinee must be integer-typed");
  };

  if (node->repr_kind == SGMatchReprKind::Stmt) {
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*theContext, "match_merge", F);
    if (not node->int_arms.empty()) {
      llvm::Value* sv = coerce_match_scrutinee_to_i64(node->scrutinee->toLLVMIR(this));
      llvm::BasicBlock* def_bb = llvm::BasicBlock::Create(*theContext, "match_def", F);
      llvm::SwitchInst* sw = theBuilder->CreateSwitch(sv, def_bb, node->int_arms.size());
      for (auto const& p : node->int_arms) {
        llvm::BasicBlock* cbb = llvm::BasicBlock::Create(*theContext, "match_case", F);
        sw->addCase(llvm::ConstantInt::get(i64ti, p.first), cbb);
        theBuilder->SetInsertPoint(cbb);
        p.second->toLLVMIR(this);
        llvm::BasicBlock* cb2 = theBuilder->GetInsertBlock();
        if (cb2 && !cb2->getTerminator()) {
          theBuilder->CreateBr(merge_bb);
        }
      }
      theBuilder->SetInsertPoint(def_bb);
      if (node->default_arm) {
        node->default_arm->toLLVMIR(this);
      }
      llvm::BasicBlock* d2 = theBuilder->GetInsertBlock();
      if (d2 && !d2->getTerminator()) {
        theBuilder->CreateBr(merge_bb);
      }
    }
    else {
      if (node->default_arm) {
        node->default_arm->toLLVMIR(this);
      }
      llvm::BasicBlock* d2 = theBuilder->GetInsertBlock();
      if (d2 && !d2->getTerminator()) {
        theBuilder->CreateBr(merge_bb);
      }
    }
    theBuilder->SetInsertPoint(merge_bb);
    return nullptr;
  }

  bool mixed = node->repr_kind == SGMatchReprKind::ExprMixed;
  bool as_float = node->repr_kind == SGMatchReprKind::ExprFloat;
  bool as_bool = node->repr_kind == SGMatchReprKind::ExprBool;
  bool as_char = node->repr_kind == SGMatchReprKind::ExprChar;
  llvm::Type* merge_ty = llvm::Type::getInt64Ty(*theContext);
  if (mixed) {
    merge_ty = llvm::PointerType::get(*theContext, 0);
  }
  else if (as_float) {
    merge_ty = llvm::Type::getDoubleTy(*theContext);
  }
  else if (as_bool) {
    merge_ty = theBuilder->getInt1Ty();
  }
  else if (as_char) {
    merge_ty = theBuilder->getInt8Ty();
  }

  if (node->int_arms.empty()) {
    return evaluate_arm_block_value(node->default_arm, mixed);
  }

  llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*theContext, "mexpr_merge", F);
  llvm::PHINode* phi = llvm::PHINode::Create(merge_ty, 0, "mphi", merge_bb);
  bool phi_maybe_owns_cstr = false;

  llvm::Value* sv = coerce_match_scrutinee_to_i64(node->scrutinee->toLLVMIR(this));

  llvm::BasicBlock* def_bb = llvm::BasicBlock::Create(*theContext, "mexpr_def", F);
  llvm::SwitchInst* sw = theBuilder->CreateSwitch(sv, def_bb, node->int_arms.size());

  for (auto const& p : node->int_arms) {
    llvm::BasicBlock* cbb = llvm::BasicBlock::Create(*theContext, "mexpr_arm", F);
    sw->addCase(llvm::ConstantInt::get(i64ti, p.first), cbb);
    theBuilder->SetInsertPoint(cbb);
    llvm::Value* vv = evaluate_arm_block_value(p.second, mixed);
    if (!mixed) {
      vv = coerce_for_return(vv, merge_ty);
    }
    llvm::BasicBlock* from = theBuilder->GetInsertBlock();
    if (from && !from->getTerminator()) {
      theBuilder->CreateBr(merge_bb);
      phi->addIncoming(vv, from);
      if (mixed && take_owned_cstr_temp(vv)) {
        phi_maybe_owns_cstr = true;
      }
    }
  }

  theBuilder->SetInsertPoint(def_bb);
  llvm::Value* dv = nullptr;
  if (node->default_arm) {
    dv = evaluate_arm_block_value(node->default_arm, mixed);
  }
  else {
    if (as_float) {
      dv = llvm::ConstantFP::get(merge_ty, 0.0);
    }
    else if (mixed) {
      dv = llvm::ConstantInt::get(i64ti, 0);
    }
    else {
      dv = llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(merge_ty), 0);
    }
    if (mixed) {
      dv = promote_to_cstr(dv);
    }
  }
  if (!mixed) {
    dv = coerce_for_return(dv, merge_ty);
  }
  llvm::BasicBlock* df = theBuilder->GetInsertBlock();
  if (df && !df->getTerminator()) {
    theBuilder->CreateBr(merge_bb);
    phi->addIncoming(dv, df);
    if (mixed && take_owned_cstr_temp(dv)) {
      phi_maybe_owns_cstr = true;
    }
  }

  theBuilder->SetInsertPoint(merge_bb);
  if (mixed && phi_maybe_owns_cstr) {
    track_owned_cstr_temp(phi);
  }
  return phi;
}

// ---- end SG node toLLVMIR implementations ----

/* ----------------------------------------------------------------------
 *  Merged from src/StyioCodeGen/CodeGen.cpp on 2026-05-22.
 *  Top-level driver entry points: print, execute, dump.
 * ---------------------------------------------------------------------- */

void
StyioToLLVM::print_llvm_ir() {
  /* Use the same LLVM stream as Module::print so banner + IR stay ordered (cout may be buffered separately). */
  if (styio_stdout_plain()) {
    llvm::outs() << "LLVM IR\n";
  }
  else {
    llvm::outs() << "\033[1;32mLLVM IR\033[0m\n";
  }

  theModule->print(llvm::outs(), nullptr);
  llvm::outs() << "\n";
  llvm::outs().flush();
}

void
StyioToLLVM::execute() {
  std::string verifier_error;
  llvm::raw_string_ostream verifier_stream(verifier_error);
  if (llvm::verifyModule(*theModule, &verifier_stream)) {
    verifier_stream.flush();
    throw StyioTypeError("LLVM module verification failed: " + verifier_error);
  }
  llvm::Function* entrypoint = theModule->getFunction("main");
  if (entrypoint == nullptr || entrypoint->isDeclaration()) {
    std::cerr << "styio: main not found" << std::endl;
    return;
  }
  auto RT = theORCJIT->getMainJITDylib().createResourceTracker();
  llvm::ExitOnError exit_on_error;
  if (theORCJIT->callableCacheEnabled()) {
    auto specialization_modules =
      styio_partition_callable_specializations(*theModule);
    theBuilder.reset();
    llvm::orc::ThreadSafeContext thread_context(
      std::move(theContext));
    for (auto& specialization : specialization_modules) {
      auto specialization_tsm = llvm::orc::ThreadSafeModule(
        std::move(specialization.module),
        thread_context);
      exit_on_error(
        theORCJIT->addModule(
          std::move(specialization_tsm),
          RT));
    }
    auto main_tsm = llvm::orc::ThreadSafeModule(
      std::move(theModule),
      thread_context);
    exit_on_error(
      theORCJIT->addModule(
        std::move(main_tsm),
        RT));
  }
  else {
    auto TSM = llvm::orc::ThreadSafeModule(
      std::move(theModule),
      std::move(theContext));
    exit_on_error(
      theORCJIT->addModule(
        std::move(TSM),
        RT));
  }

  auto ExprSymbol = theORCJIT->lookup("main");
  if (!ExprSymbol) {
    std::cerr << "styio: main not found" << std::endl;
    return;
  }

  int (*FP)() = ExprSymbol->getAddress().toPtr<int (*)()>();
  FP();
}

std::string
StyioToLLVM::dump_llvm_ir() const {
  std::string out;
  llvm::raw_string_ostream os(out);
  theModule->print(os, nullptr);
  os.flush();
  return out;
}
