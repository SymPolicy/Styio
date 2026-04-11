// [C++ STL]
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
#include "../StyioToken/Token.hpp"
#include "../StyioUtil/BoundedType.hpp"
#include "../StyioUtil/Util.hpp"
#include "CodeGenVisitor.hpp"

// [LLVM]
#include "llvm/ADT/APFloat.h"
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
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/LinkAllIR.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils.h"

namespace {
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

enum StyioDynTag : std::int64_t
{
  STYIO_DYN_UNDEF = 0,
  STYIO_DYN_BOOL = 1,
  STYIO_DYN_I64 = 2,
  STYIO_DYN_F64 = 3,
  STYIO_DYN_CSTR = 4,
  STYIO_DYN_LIST = 5,
};
}  // namespace

llvm::FunctionCallee
StyioToLLVM::free_cstr_fn() {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  return theModule->getOrInsertFunction(
    "styio_free_cstr",
    llvm::FunctionType::get(theBuilder->getVoidTy(), {char_ptr}, false));
}

void
StyioToLLVM::track_owned_cstr_temp(llvm::Value* v) {
  if (v && v->getType()->isPointerTy()) {
    owned_cstr_temps_.insert(v);
  }
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
    i64_val = theBuilder->CreateSExtOrTrunc(i64_val, theBuilder->getInt64Ty());
  }
  if (!f64_val->getType()->isDoubleTy()) {
    if (f64_val->getType()->isIntegerTy()) {
      f64_val = theBuilder->CreateSIToFP(f64_val, theBuilder->getDoubleTy());
    }
    else {
      f64_val = llvm::ConstantFP::get(theBuilder->getDoubleTy(), 0.0);
    }
  }
  if (!ptr_val->getType()->isPointerTy()) {
    ptr_val = llvm::ConstantPointerNull::get(llvm::PointerType::get(*theContext, 0));
  }

  theBuilder->CreateStore(llvm::ConstantInt::get(theBuilder->getInt64Ty(), tag), tag_gep);
  theBuilder->CreateStore(i64_val, i64_gep);
  theBuilder->CreateStore(f64_val, f64_gep);
  theBuilder->CreateStore(ptr_val, ptr_gep);
}

void
StyioToLLVM::init_dynamic_slot_undef(llvm::AllocaInst* slot) {
  store_dynamic_slot(slot, STYIO_DYN_UNDEF, nullptr, nullptr, nullptr);
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

  llvm::Value* is_cstr = theBuilder->CreateICmpEQ(tag, theBuilder->getInt64(STYIO_DYN_CSTR));
  llvm::Value* is_list = theBuilder->CreateICmpEQ(tag, theBuilder->getInt64(STYIO_DYN_LIST));
  theBuilder->CreateCondBr(is_cstr, cstr_bb, list_bb);

  theBuilder->SetInsertPoint(cstr_bb);
  llvm::Value* ptr = theBuilder->CreateLoad(llvm::PointerType::get(*theContext, 0), ptr_gep);
  free_cstr_if_runtime_owned(ptr);
  theBuilder->CreateBr(done_bb);

  theBuilder->SetInsertPoint(list_bb);
  llvm::BasicBlock* list_release_bb = llvm::BasicBlock::Create(*theContext, "dynrel_list_do", F);
  theBuilder->CreateCondBr(is_list, list_release_bb, done_bb);

  theBuilder->SetInsertPoint(list_release_bb);
  llvm::Value* handle = theBuilder->CreateLoad(theBuilder->getInt64Ty(), i64_gep);
  llvm::FunctionCallee release_fn = theModule->getOrInsertFunction(
    "styio_list_release",
    llvm::FunctionType::get(theBuilder->getVoidTy(), {theBuilder->getInt64Ty()}, false));
  theBuilder->CreateCall(release_fn, {handle});
  theBuilder->CreateBr(done_bb);

  theBuilder->SetInsertPoint(done_bb);
  init_dynamic_slot_undef(slot);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGResId* node) {
  const string& name = node->as_str();

  if (bounded_ring_head_slot_.contains(name)) {
    llvm::AllocaInst* arr = mutable_variables[name];
    llvm::AllocaInst* headSlot = bounded_ring_head_slot_[name];
    std::uint64_t cap = bounded_ring_capacity_[name];
    llvm::Type* i64 = theBuilder->getInt64Ty();
    auto* arrTy = llvm::cast<llvm::ArrayType>(arr->getAllocatedType());
    llvm::Value* head = theBuilder->CreateLoad(i64, headSlot);
    llvm::Value* zero = llvm::ConstantInt::get(i64, 0);
    llvm::Value* one = llvm::ConstantInt::get(i64, 1);
    llvm::Value* has = theBuilder->CreateICmpULT(zero, head);
    llvm::Value* prev = theBuilder->CreateSub(head, one);
    llvm::Value* capv = llvm::ConstantInt::get(i64, cap);
    llvm::Value* prev_m = theBuilder->CreateURem(prev, capv);
    llvm::Value* idx = theBuilder->CreateSelect(has, prev_m, zero);
    llvm::Value* gep = theBuilder->CreateInBoundsGEP(arrTy, arr, {zero, idx});
    llvm::Value* cell = theBuilder->CreateLoad(i64, gep);
    return theBuilder->CreateSelect(has, cell, zero);
  }

  if (named_values.contains(name)) {
    return named_values[name];
  }

  if (mutable_variables.contains(name)) {
    llvm::AllocaInst* variable = mutable_variables[name];
    return theBuilder->CreateLoad(variable->getAllocatedType(), variable);
  }

  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGType* node) {
  return theBuilder->getInt64(0);
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
  auto output = theBuilder->getInt32(0);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGBinOp* node) {
  StyioDataType data_type = node->data_type->data_type;

  using Bin2 = std::function<llvm::Value*(llvm::Value*, llvm::Value*)>;

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
      if (not r_val->getType()->isDoubleTy()) {
        r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
      }
      if (not cur->getType()->isDoubleTy()) {
        cur = theBuilder->CreateSIToFP(cur, theBuilder->getDoubleTy());
      }
      next = bf(cur, r_val);
    }
    else {
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
  llvm::Value* const styioUndef = theBuilder->getInt64(styio_undef_i64());
  llvm::Type* char_ptr_ty = llvm::PointerType::get(*theContext, 0);

  auto ptr_to_i64_for_arith = [&](llvm::Value* v) -> llvm::Value* {
    if (!v->getType()->isPointerTy()) {
      return v;
    }
    llvm::FunctionCallee cvt = theModule->getOrInsertFunction(
      "styio_cstr_to_i64",
      llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr_ty}, false));
    return theBuilder->CreateCall(cvt, {v});
  };

  switch (node->operand) {
    case StyioOpType::Binary_Add: {
      if (data_type.option == StyioDataTypeOption::String) {
        llvm::FunctionCallee i64c = theModule->getOrInsertFunction(
          "styio_i64_dec_cstr",
          llvm::FunctionType::get(char_ptr_ty, {theBuilder->getInt64Ty()}, false));
        llvm::FunctionCallee cat = theModule->getOrInsertFunction(
          "styio_strcat_ab",
          llvm::FunctionType::get(char_ptr_ty, {char_ptr_ty, char_ptr_ty}, false));
        llvm::Value* a = l_val;
        llvm::Value* b = r_val;
        if (!a->getType()->isPointerTy()) {
          llvm::Value* ai = a->getType()->isIntegerTy(64)
            ? a
            : theBuilder->CreateSExtOrTrunc(a, theBuilder->getInt64Ty());
          a = theBuilder->CreateCall(i64c, {ai});
        }
        if (!b->getType()->isPointerTy()) {
          llvm::Value* bi = b->getType()->isIntegerTy(64)
            ? b
            : theBuilder->CreateSExtOrTrunc(b, theBuilder->getInt64Ty());
          b = theBuilder->CreateCall(i64c, {bi});
        }
        llvm::Value* out = theBuilder->CreateCall(cat, {a, b});
        free_owned_cstr_temp_if_tracked(a);
        free_owned_cstr_temp_if_tracked(b);
        track_owned_cstr_temp(out);
        return out;
      }
      if (data_type.isFloat() || l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
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
        if (l_val->getType()->isIntegerTy(64) && r_val->getType()->isIntegerTy(64)) {
          llvm::Value* lu = theBuilder->CreateICmpEQ(l_val, styioUndef);
          llvm::Value* ru = theBuilder->CreateICmpEQ(r_val, styioUndef);
          llvm::Value* bad = theBuilder->CreateOr(lu, ru);
          llvm::Value* out = theBuilder->CreateSDiv(l_val, r_val);
          return theBuilder->CreateSelect(bad, styioUndef, out);
        }
        return theBuilder->CreateSDiv(l_val, r_val);
      }
    } break;

    case StyioOpType::Binary_Pow: {
      llvm::Type* d = theBuilder->getDoubleTy();
      llvm::FunctionCallee pow_fn = theModule->getOrInsertFunction(
        "pow",
        llvm::FunctionType::get(d, {d, d}, false));
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
        if (l_val->getType()->isIntegerTy(64) && r_val->getType()->isIntegerTy(64)) {
          llvm::Value* lu = theBuilder->CreateICmpEQ(l_val, styioUndef);
          llvm::Value* ru = theBuilder->CreateICmpEQ(r_val, styioUndef);
          llvm::Value* bad = theBuilder->CreateOr(lu, ru);
          llvm::Value* out = theBuilder->CreateSRem(l_val, r_val);
          return theBuilder->CreateSelect(bad, styioUndef, out);
        }
        return theBuilder->CreateSRem(l_val, r_val);
      }
    } break;

    case StyioOpType::Equal: {
      if (l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
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
      if (l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
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
      if (l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
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
      if (l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
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
      if (l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
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
      if (l_val->getType()->isDoubleTy() || r_val->getType()->isDoubleTy()) {
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
        [&](llvm::Value* a, llvm::Value* b) { return theBuilder->CreateSDiv(a, b); },
        [&](llvm::Value* a, llvm::Value* b) { return theBuilder->CreateFDiv(a, b); });
    } break;

    case StyioOpType::Self_Mod_Assign: {
      return do_self_assign(
        [&](llvm::Value* a, llvm::Value* b) { return theBuilder->CreateSRem(a, b); },
        [&](llvm::Value* a, llvm::Value* b) { return theBuilder->CreateFRem(a, b); });
    } break;

    default:
      break;
  }

  return theBuilder->getInt64(0);
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
  if (node->operand == StyioOpType::Logic_AND) {
    return theBuilder->CreateAnd(L, R);
  }
  if (node->operand == StyioOpType::Logic_OR) {
    return theBuilder->CreateOr(L, R);
  }
  return L;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGVar* node) {
  auto output = theBuilder->getInt32(0);
  return output;
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

  if (named_values.contains(varname)) {
    /* ERROR */
    throw StyioTypeError(
      std::string("immutable binding cannot be reassigned with `=`: ") + varname);
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
    std::int64_t tag = STYIO_DYN_UNDEF;
    llvm::Value* i64v = nullptr;
    llvm::Value* f64v = nullptr;
    llvm::Value* ptrv = nullptr;

    if (dynamic_cast<SGListReadStdin*>(node->value)
        || dynamic_cast<SGListClone*>(node->value)
        || (dynamic_cast<SGDynLoad*>(node->value)
            && static_cast<SGDynLoad*>(node->value)->kind == SGDynLoadKind::ListHandle)) {
      tag = STYIO_DYN_LIST;
      i64v = next_value;
    }
    else if (next_value->getType()->isPointerTy()) {
      tag = STYIO_DYN_CSTR;
      ptrv = next_value;
    }
    else if (next_value->getType()->isDoubleTy()) {
      tag = STYIO_DYN_F64;
      f64v = next_value;
    }
    else if (next_value->getType()->isIntegerTy(1)) {
      tag = STYIO_DYN_BOOL;
      i64v = next_value;
    }
    else if (next_value->getType()->isIntegerTy()) {
      tag = STYIO_DYN_I64;
      i64v = next_value;
    }

    if (is_existing_slot) {
      release_dynamic_slot_contents(variable);
    }
    store_dynamic_slot(variable, tag, i64v, f64v, ptrv);
    if (tag == STYIO_DYN_CSTR) {
      forget_owned_cstr_temp(next_value);
    }
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
    || variable->getAllocatedType()->isPointerTy();

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
  if (named_values.contains(varname)) {
    /* ERROR */
    throw StyioTypeError(
      std::string("immutable binding cannot be redefined with `:=`: ") + varname);
  }

  if (node->var->is_dynamic_slot) {
    llvm::AllocaInst* variable = create_entry_alloca(dynamic_cell_type(), varname);
    init_dynamic_slot_undef(variable);
    llvm::Value* value = node->value->toLLVMIR(this);
    std::int64_t tag = STYIO_DYN_UNDEF;
    llvm::Value* i64v = nullptr;
    llvm::Value* f64v = nullptr;
    llvm::Value* ptrv = nullptr;

    if (dynamic_cast<SGListReadStdin*>(node->value)
        || dynamic_cast<SGListClone*>(node->value)
        || (dynamic_cast<SGDynLoad*>(node->value)
            && static_cast<SGDynLoad*>(node->value)->kind == SGDynLoadKind::ListHandle)) {
      tag = STYIO_DYN_LIST;
      i64v = value;
    }
    else if (value->getType()->isPointerTy()) {
      tag = STYIO_DYN_CSTR;
      ptrv = value;
    }
    else if (value->getType()->isDoubleTy()) {
      tag = STYIO_DYN_F64;
      f64v = value;
    }
    else if (value->getType()->isIntegerTy(1)) {
      tag = STYIO_DYN_BOOL;
      i64v = value;
    }
    else if (value->getType()->isIntegerTy()) {
      tag = STYIO_DYN_I64;
      i64v = value;
    }

    store_dynamic_slot(variable, tag, i64v, f64v, ptrv);
    if (tag == STYIO_DYN_CSTR) {
      forget_owned_cstr_temp(value);
    }
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
    llvm::Type* arrTy = llvm::ArrayType::get(i64, *cap);
    llvm::AllocaInst* arr = prealloc.CreateAlloca(arrTy, nullptr, varname);
    llvm::AllocaInst* head = prealloc.CreateAlloca(i64, nullptr, varname + ".head");
    prealloc.CreateStore(llvm::ConstantInt::get(i64, 0), head);
    llvm::Value* val = node->value->toLLVMIR(this);
    llvm::Value* z = llvm::ConstantInt::get(i64, 0);
    llvm::Value* gep0 = prealloc.CreateInBoundsGEP(arrTy, arr, {z, z});
    prealloc.CreateStore(val, gep0);
    prealloc.CreateStore(llvm::ConstantInt::get(i64, 1), head);
    mutable_variables[varname] = arr;
    bounded_ring_head_slot_[varname] = head;
    bounded_ring_capacity_[varname] = *cap;
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
  if (variable->getAllocatedType()->isPointerTy()) {
    register_cstr_slot_for_raii(variable);
    forget_owned_cstr_temp(value);
  }

  return variable;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGFuncArg* node) {
  return theBuilder->getInt64(0);
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
  mutable_variables.clear();
  named_values.clear();
  bounded_ring_head_slot_.clear();
  bounded_ring_capacity_.clear();

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

  llvm::Value* last = nullptr;
  for (auto* stmt : node->func_block->stmts) {
    if (dynamic_cast<SGFunc*>(stmt)) {
      stmt->toLLVMIR(this);
      continue;
    }

    last = stmt->toLLVMIR(this);
    if (theBuilder->GetInsertBlock()->getTerminator()) {
      break;
    }
  }

  llvm::BasicBlock* cur = theBuilder->GetInsertBlock();
  if (cur && !cur->getTerminator()) {
    llvm::Type* rt = node->ret_type->toLLVMType(this);
    if (!last) {
      if (rt->isFloatingPointTy()) {
        last = llvm::ConstantFP::get(rt, 0.0);
      }
      else {
        last = llvm::ConstantInt::get(rt, 0);
      }
    }
    else {
      last = coerce_for_return(last, rt);
    }

    pop_file_handle_scope();
    theBuilder->CreateRet(last);
  }

  mutable_variables = std::move(saved_mut);
  named_values = std::move(saved_named);
  bounded_ring_head_slot_ = std::move(saved_ring_h);
  bounded_ring_capacity_ = std::move(saved_ring_c);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGFunc* node) {
  /* Bodies are emitted from SGMainEntry after a full declare pass. */
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGCall* node) {
  llvm::Function* callee = theModule->getFunction(node->func_name->as_str());
  if (!callee) {
    return theBuilder->getInt64(0);
  }

  llvm::FunctionType* ft = callee->getFunctionType();
  std::vector<llvm::Value*> args;
  for (size_t i = 0; i < node->func_args.size(); ++i) {
    llvm::Value* av = node->func_args[i]->toLLVMIR(this);
    llvm::Type* pt = ft->getParamType(i);
    if (pt->isDoubleTy() && av->getType()->isIntegerTy()) {
      av = theBuilder->CreateSIToFP(av, pt);
    }
    else if (pt->isIntegerTy() && av->getType()->isDoubleTy()) {
      av = theBuilder->CreateFPToSI(av, pt);
    }
    else if (pt->isIntegerTy() && av->getType()->isPointerTy()) {
      llvm::FunctionCallee conv = theModule->getOrInsertFunction(
        "styio_cstr_to_i64",
        llvm::FunctionType::get(theBuilder->getInt64Ty(), {llvm::PointerType::get(*theContext, 0)}, false));
      av = theBuilder->CreateCall(conv, {av});
      if (pt->getIntegerBitWidth() != 64) {
        av = theBuilder->CreateIntCast(av, pt, true);
      }
    }
    args.push_back(av);
  }

  return theBuilder->CreateCall(ft, callee, args);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGReturn* node) {
  return theBuilder->CreateRet(node->expr->toLLVMIR(this));
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGBlock* node) {
  push_file_handle_scope();
  llvm::Value* last = nullptr;
  for (auto const& s : node->stmts) {
    last = s->toLLVMIR(this);
    if (theBuilder->GetInsertBlock()->getTerminator()) {
      break;
    }
  }
  llvm::BasicBlock* bcur = theBuilder->GetInsertBlock();
  if (bcur && !bcur->getTerminator()) {
    pop_file_handle_scope();
  }
  return last ? last : theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGEntry* node) {
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGMainEntry* node) {
  std::vector<SGFunc*> ordered_funcs;
  for (auto* s : node->stmts) {
    if (auto* f = dynamic_cast<SGFunc*>(s)) {
      collect_sgfuncs_postorder(f, ordered_funcs);
    }
  }

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

  push_file_handle_scope();

  llvm::Value* last_main = nullptr;
  for (auto const& s : node->stmts) {
    if (dynamic_cast<SGFunc*>(s)) {
      continue;
    }

    last_main = s->toLLVMIR(this);
    if (theBuilder->GetInsertBlock()->getTerminator()) {
      break;
    }
  }

  llvm::BasicBlock* mcur = theBuilder->GetInsertBlock();
  if (mcur && !mcur->getTerminator()) {
    pop_file_handle_scope();
    theBuilder->CreateRet(truncate_for_main_ret(last_main));
  }

  return main_func;
}

llvm::Value*
StyioToLLVM::promote_to_cstr(llvm::Value* v) {
  llvm::PointerType* char_ptr = llvm::PointerType::get(*theContext, 0);
  if (v->getType()->isPointerTy()) {
    return v;
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
    loop_stack_.push_back(LoopFrame{exit_bb, body_bb});
    theBuilder->SetInsertPoint(body_bb);
    node->body->toLLVMIR(this);
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
  loop_stack_.push_back(LoopFrame{exit_bb, cond_bb});
  theBuilder->SetInsertPoint(body_bb);
  node->body->toLLVMIR(this);
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
  auto* lit = dynamic_cast<SGListLiteral*>(node->iterable);
  if (!lit || lit->elems.empty()) {
    return nullptr;
  }

  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::IntegerType* i64t = theBuilder->getInt64Ty();
  llvm::Value* zero = llvm::ConstantInt::get(i64t, 0);
  llvm::Value* one = llvm::ConstantInt::get(i64t, 1);
  llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "foreach_exit", F);
  llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(*theContext, "foreach_hdr", F);
  llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, "foreach_body", F);
  llvm::BasicBlock* step_bb = llvm::BasicBlock::Create(*theContext, "foreach_step", F);

  std::vector<llvm::Constant*> cs;
  for (auto* e : lit->elems) {
    if (auto* ci = dynamic_cast<SGConstInt*>(e)) {
      cs.push_back(llvm::ConstantInt::get(i64t, std::stoll(ci->value)));
    }
    else {
      cs.push_back(llvm::ConstantInt::get(i64t, 0));
    }
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

  theBuilder->CreateBr(hdr_bb);

  theBuilder->SetInsertPoint(hdr_bb);
  llvm::Value* idxv = theBuilder->CreateLoad(i64t, idx_slot);
  llvm::Value* n = llvm::ConstantInt::get(i64t, lit->elems.size());
  llvm::Value* go = theBuilder->CreateICmpSLT(idxv, n);
  theBuilder->CreateCondBr(go, body_bb, exit_bb);

  loop_stack_.push_back(LoopFrame{exit_bb, step_bb});

  theBuilder->SetInsertPoint(body_bb);
  llvm::Value* idx = theBuilder->CreateLoad(i64t, idx_slot);
  llvm::Value* z32 = theBuilder->getInt32(0);
  llvm::Value* gep = theBuilder->CreateInBoundsGEP(at, gv, {z32, idx});
  llvm::Value* el = theBuilder->CreateLoad(i64t, gep);

  llvm::AllocaInst* vs = theBuilder->CreateAlloca(i64t, nullptr, node->var);
  theBuilder->CreateStore(el, vs);
  mutable_variables[node->var] = vs;

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

  llvm::BasicBlock* bcur = theBuilder->GetInsertBlock();
  if (bcur && !bcur->getTerminator()) {
    theBuilder->CreateBr(step_bb);
  }

  theBuilder->SetInsertPoint(step_bb);
  llvm::Value* nx = theBuilder->CreateAdd(theBuilder->CreateLoad(i64t, idx_slot), one);
  theBuilder->CreateStore(nx, idx_slot);
  theBuilder->CreateBr(hdr_bb);

  theBuilder->SetInsertPoint(exit_bb);
  if (pulse_sz > 0 && node->pulse_region_id >= 0) {
    llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
    llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
    pulse_region_ledgers_[node->pulse_region_id] = {li8, node->pulse_plan.get()};
  }
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

  loop_stack_.push_back(LoopFrame{exit_bb, step_bb});

  theBuilder->SetInsertPoint(body_bb);
  llvm::AllocaInst* vs = theBuilder->CreateAlloca(i64t, nullptr, node->var);
  theBuilder->CreateStore(cur, vs);
  mutable_variables[node->var] = vs;
  node->body->toLLVMIR(this);
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
StyioToLLVM::toLLVMIR(SGListLiteral* node) {
  (void)node;
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGBreak* node) {
  if (loop_stack_.empty() || node->depth == 0 || node->depth > loop_stack_.size()) {
    return nullptr;
  }
  size_t ix = loop_stack_.size() - node->depth;
  theBuilder->CreateBr(loop_stack_[ix].break_dest);
  return nullptr;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGContinue* node) {
  if (loop_stack_.empty() || node->depth == 0 || node->depth > loop_stack_.size()) {
    return nullptr;
  }
  size_t ix = loop_stack_.size() - node->depth;
  theBuilder->CreateBr(loop_stack_[ix].continue_dest);
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
StyioToLLVM::push_file_handle_scope() {
  file_handle_scope_stack_.emplace_back();
  cstr_slot_scope_stack_.emplace_back();
  dynamic_slot_scope_stack_.emplace_back();
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
StyioToLLVM::pop_file_handle_scope() {
  if (file_handle_scope_stack_.empty()) {
    return;
  }
  llvm::FunctionCallee close_fn = theModule->getOrInsertFunction(
    "styio_file_close",
    llvm::FunctionType::get(
      theBuilder->getVoidTy(),
      {theBuilder->getInt64Ty()},
      false));
  std::unordered_set<llvm::AllocaInst*> closed_slots;
  for (const std::string& v : file_handle_scope_stack_.back()) {
    auto it = mutable_variables.find(v);
    if (it != mutable_variables.end()) {
      llvm::AllocaInst* slot = it->second;
      if (closed_slots.insert(slot).second) {
        llvm::Value* h = theBuilder->CreateLoad(theBuilder->getInt64Ty(), slot);
        theBuilder->CreateCall(close_fn, {h});
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
  file_handle_scope_stack_.pop_back();
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
StyioToLLVM::toLLVMIR(SGHandleAcquire* node) {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee open_fn = theModule->getOrInsertFunction(
    node->is_auto ? "styio_file_open_auto" : "styio_file_open",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr}, false));
  llvm::Value* path = node->path_expr->toLLVMIR(this);
  std::string pkey = path_key_from_path_ir(node->path_expr);
  llvm::AllocaInst* slot = nullptr;
  if (!pkey.empty()) {
    auto sit = file_singleton_path_slots_.find(pkey);
    if (sit != file_singleton_path_slots_.end()) {
      slot = sit->second;
    }
  }
  if (!slot) {
    llvm::Value* h = theBuilder->CreateCall(open_fn, {path});
    slot = theBuilder->CreateAlloca(
      theBuilder->getInt64Ty(),
      nullptr,
      node->var_name.c_str());
    theBuilder->CreateStore(h, slot);
    if (!pkey.empty()) {
      file_singleton_path_slots_[pkey] = slot;
    }
  }
  mutable_variables[node->var_name] = slot;
  if (!pkey.empty()) {
    if (file_singleton_raii_paths_.insert(pkey).second) {
      register_file_handle_for_raii(node->var_name);
    }
  }
  else {
    register_file_handle_for_raii(node->var_name);
  }
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGFileLineIter* node) {
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
  if (node->from_path) {
    llvm::Value* path = node->path_expr->toLLVMIR(this);
    h0 = theBuilder->CreateCall(open_fn, {path});
    h_slot = theBuilder->CreateAlloca(theBuilder->getInt64Ty(), nullptr, "file_iter_h");
    theBuilder->CreateStore(h0, h_slot);
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
  theBuilder->CreateCondBr(done, exit_bb, body);

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
    llvm::FunctionCallee close_fn = theModule->getOrInsertFunction(
      "styio_file_close",
      llvm::FunctionType::get(theBuilder->getVoidTy(), {theBuilder->getInt64Ty()}, false));
    llvm::Value* hf = theBuilder->CreateLoad(theBuilder->getInt64Ty(), h_slot);
    theBuilder->CreateCall(close_fn, {hf});
  }
  return theBuilder->getInt64(0);
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
StyioToLLVM::toLLVMIR(SGInstantPull* node) {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee read_fn = theModule->getOrInsertFunction(
    "styio_read_file_i64line",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {char_ptr}, false));
  llvm::Value* p = node->path_expr->toLLVMIR(this);
  return theBuilder->CreateCall(read_fn, {p});
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGListReadStdin* node) {
  (void)node;
  llvm::FunctionCallee read_fn = theModule->getOrInsertFunction(
    "styio_list_i64_read_stdin",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {}, false));
  return theBuilder->CreateCall(read_fn, {});
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGListClone* node) {
  llvm::FunctionCallee clone_fn = theModule->getOrInsertFunction(
    "styio_list_clone",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {theBuilder->getInt64Ty()}, false));
  llvm::Value* src = node->source->toLLVMIR(this);
  if (!src->getType()->isIntegerTy(64)) {
    src = theBuilder->CreateSExtOrTrunc(src, theBuilder->getInt64Ty());
  }
  return theBuilder->CreateCall(clone_fn, {src});
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGListLen* node) {
  llvm::FunctionCallee len_fn = theModule->getOrInsertFunction(
    "styio_list_len",
    llvm::FunctionType::get(theBuilder->getInt64Ty(), {theBuilder->getInt64Ty()}, false));
  llvm::Value* list = node->list->toLLVMIR(this);
  if (!list->getType()->isIntegerTy(64)) {
    list = theBuilder->CreateSExtOrTrunc(list, theBuilder->getInt64Ty());
  }
  return theBuilder->CreateCall(len_fn, {list});
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGListGet* node) {
  llvm::FunctionCallee get_fn = theModule->getOrInsertFunction(
    "styio_list_get",
    llvm::FunctionType::get(
      theBuilder->getInt64Ty(),
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
  return theBuilder->CreateCall(get_fn, {list, idx});
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGListSet* node) {
  llvm::FunctionCallee set_fn = theModule->getOrInsertFunction(
    "styio_list_set",
    llvm::FunctionType::get(
      theBuilder->getVoidTy(),
      {theBuilder->getInt64Ty(), theBuilder->getInt64Ty(), theBuilder->getInt64Ty()},
      false));
  llvm::Value* list = node->list->toLLVMIR(this);
  llvm::Value* idx = node->index->toLLVMIR(this);
  llvm::Value* value = node->value->toLLVMIR(this);
  if (!list->getType()->isIntegerTy(64)) {
    list = theBuilder->CreateSExtOrTrunc(list, theBuilder->getInt64Ty());
  }
  if (!idx->getType()->isIntegerTy(64)) {
    idx = theBuilder->CreateSExtOrTrunc(idx, theBuilder->getInt64Ty());
  }
  if (value->getType()->isIntegerTy(1)) {
    value = theBuilder->CreateZExt(value, theBuilder->getInt64Ty());
  }
  else if (!value->getType()->isIntegerTy(64)) {
    value = theBuilder->CreateSExtOrTrunc(value, theBuilder->getInt64Ty());
  }
  theBuilder->CreateCall(set_fn, {list, idx, value});
  return nullptr;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGListToString* node) {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee str_fn = theModule->getOrInsertFunction(
    "styio_list_to_cstr",
    llvm::FunctionType::get(char_ptr, {theBuilder->getInt64Ty()}, false));
  llvm::Value* list = node->list->toLLVMIR(this);
  if (!list->getType()->isIntegerTy(64)) {
    list = theBuilder->CreateSExtOrTrunc(list, theBuilder->getInt64Ty());
  }
  llvm::Value* out = theBuilder->CreateCall(str_fn, {list});
  track_owned_cstr_temp(out);
  return out;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGStreamZip* node) {
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
  llvm::FunctionCallee close_fn = theModule->getOrInsertFunction(
    "styio_file_close",
    llvm::FunctionType::get(theBuilder->getVoidTy(), {i64t}, false));

  auto* lit_a = dynamic_cast<SGListLiteral*>(node->iterable_a);
  auto* lit_b = dynamic_cast<SGListLiteral*>(node->iterable_b);

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
  };

  auto finish_zip = [&]() {
    if (pulse_sz > 0 && node->pulse_region_id >= 0) {
      llvm::Type* i8p = llvm::PointerType::get(*theContext, 0);
      llvm::Value* li8 = theBuilder->CreateBitCast(ledger_alloc, i8p);
      pulse_region_ledgers_[node->pulse_region_id] = {li8, node->pulse_plan.get()};
    }
  };

  if (lit_a && lit_b && !node->a_is_file && !node->b_is_file) {
    size_t na = lit_a->elems.size();
    size_t nb = lit_b->elems.size();
    size_t nmin = na < nb ? na : nb;

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
    llvm::Value* lim =
      llvm::ConstantInt::get(i64t, static_cast<uint64_t>(nmin), /*signed=*/true);
    llvm::Value* ok = theBuilder->CreateICmpSLT(iv, lim);
    theBuilder->CreateCondBr(ok, body_bb, exit_bb);

    loop_stack_.push_back(LoopFrame{exit_bb, step_bb});
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
    run_pulse_epilogue();

    mutable_variables.erase(node->var_a);
    mutable_variables.erase(node->var_b);

    llvm::BasicBlock* bcur = theBuilder->GetInsertBlock();
    if (bcur && !bcur->getTerminator()) {
      theBuilder->CreateBr(step_bb);
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

    loop_stack_.push_back(LoopFrame{exit_bb, step_bb});
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
      llvm::FunctionCallee c2i = theModule->getOrInsertFunction(
        "styio_cstr_to_i64",
        llvm::FunctionType::get(i64t, {char_ptr}, false));
      val_b = theBuilder->CreateCall(c2i, {ln});
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
    llvm::Value* hf = theBuilder->CreateLoad(i64t, hb);
    theBuilder->CreateCall(close_fn, {hf});
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

    loop_stack_.push_back(LoopFrame{exit_bb, step_bb});
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
      llvm::FunctionCallee c2i = theModule->getOrInsertFunction(
        "styio_cstr_to_i64",
        llvm::FunctionType::get(i64t, {char_ptr}, false));
      val_a = theBuilder->CreateCall(c2i, {ln});
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
    llvm::Value* hfe = theBuilder->CreateLoad(i64t, ha);
    theBuilder->CreateCall(close_fn, {hfe});
    loop_stack_.pop_back();
    finish_zip();
    return theBuilder->getInt64(0);
  }

  if (node->a_is_file && node->b_is_file) {
    llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(*theContext, "zip_ff_exit", F);
    llvm::BasicBlock* hdr_bb = llvm::BasicBlock::Create(*theContext, "zip_ff_hdr", F);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*theContext, "zip_ff_body", F);

    llvm::Value* pa = node->iterable_a->toLLVMIR(this);
    llvm::Value* pb = node->iterable_b->toLLVMIR(this);
    llvm::Value* h0a = theBuilder->CreateCall(open_fn, {pa});
    llvm::Value* h0b = theBuilder->CreateCall(open_fn, {pb});
    llvm::AllocaInst* ha = theBuilder->CreateAlloca(i64t, nullptr, "zip_ff_ha");
    llvm::AllocaInst* hb = theBuilder->CreateAlloca(i64t, nullptr, "zip_ff_hb");
    theBuilder->CreateStore(h0a, ha);
    theBuilder->CreateStore(h0b, hb);

    theBuilder->CreateBr(hdr_bb);
    theBuilder->SetInsertPoint(hdr_bb);
    llvm::Value* la = theBuilder->CreateCall(read_fn, {theBuilder->CreateLoad(i64t, ha)});
    llvm::Value* lb = theBuilder->CreateCall(read_fn, {theBuilder->CreateLoad(i64t, hb)});
    llvm::Value* null_ln = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(char_ptr));
    llvm::Value* da = theBuilder->CreateICmpEQ(la, null_ln);
    llvm::Value* db = theBuilder->CreateICmpEQ(lb, null_ln);
    llvm::Value* stop = theBuilder->CreateOr(da, db);
    theBuilder->CreateCondBr(stop, exit_bb, body_bb);

    loop_stack_.push_back(LoopFrame{exit_bb, hdr_bb});
    theBuilder->SetInsertPoint(body_bb);
    emit_snapshot_shadow_reload();
    llvm::FunctionCallee c2i_ff = theModule->getOrInsertFunction(
      "styio_cstr_to_i64",
      llvm::FunctionType::get(i64t, {char_ptr}, false));
    llvm::Value* val_a = la;
    llvm::Value* val_b = lb;
    llvm::Type* slot_ty_a = char_ptr;
    llvm::Type* slot_ty_b = char_ptr;
    if (!node->a_elem_string) {
      val_a = theBuilder->CreateCall(c2i_ff, {la});
      slot_ty_a = i64t;
    }
    if (!node->b_elem_string) {
      val_b = theBuilder->CreateCall(c2i_ff, {lb});
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
    theBuilder->CreateCall(close_fn, {theBuilder->CreateLoad(i64t, ha)});
    theBuilder->CreateCall(close_fn, {theBuilder->CreateLoad(i64t, hb)});
    loop_stack_.pop_back();
    finish_zip();
    return theBuilder->getInt64(0);
  }

  throw StyioTypeError(
    "unsupported stream zip lowering (supported sources: list literal and @file stream)");
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGResourceWriteToFile* node) {
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

  llvm::Value* path = node->path_expr->toLLVMIR(this);
  llvm::Value* h = theBuilder->CreateCall(openw, {path});
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
  theBuilder->CreateCall(write_fn, {h, data});
  free_owned_cstr_temp_if_tracked(data);
  theBuilder->CreateCall(close_fn, {h});
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
  llvm::Type* merge_ty = mixed
    ? static_cast<llvm::Type*>(llvm::PointerType::get(*theContext, 0))
    : static_cast<llvm::Type*>(llvm::Type::getInt64Ty(*theContext));

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
    dv = llvm::ConstantInt::get(i64ti, 0);
    if (mixed) {
      dv = promote_to_cstr(dv);
    }
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
