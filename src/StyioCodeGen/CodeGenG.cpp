// [C++ STL]
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// [Styio]
#include "../StyioException/Exception.hpp"
#include "../StyioIR/GenIR/GenIR.hpp"
#include "../StyioToken/Token.hpp"
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

llvm::Value*
StyioToLLVM::toLLVMIR(SGResId* node) {
  const string& name = node->as_str();

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
StyioToLLVM::toLLVMIR(SGConstBool* node) {
  return llvm::ConstantInt::getBool(*theContext, node->value);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGConstInt* node) {
  return theBuilder->getInt64(std::stol(node->value));
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
      throw StyioNotImplemented("compound assignment requires a mutable binding");
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

  switch (node->operand) {
    case StyioOpType::Binary_Add: {
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
        if (r_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
          return theBuilder->CreateFAdd(l_val, r_val);
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
        if (r_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
          return theBuilder->CreateFMul(l_val, r_val);
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

  if (named_values.contains(varname)) {
    /* ERROR */
    throw StyioNotImplemented("if a immutable variable is re-defined ...");
  }

  if (mutable_variables.contains(varname)) {
    variable = mutable_variables[varname];
  }
  else {
    variable = theBuilder->CreateAlloca(
      node->toLLVMType(this),
      nullptr,
      varname.c_str()
    );

    theBuilder->CreateStore(
      node->value->toLLVMIR(this),
      variable
    );

    mutable_variables[varname] = variable;
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
    throw StyioNotImplemented("if a immutable variable is re-defined ...");
  }

  llvm::AllocaInst* variable = theBuilder->CreateAlloca(
    node->toLLVMType(this),
    nullptr,
    varname.c_str()
  );

  auto value = node->value->toLLVMIR(this);
  named_values[varname] = value;

  theBuilder->CreateStore(value, variable);

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
  mutable_variables.clear();
  named_values.clear();

  llvm::BasicBlock* block = llvm::BasicBlock::Create(
    *theContext,
    (fname + "_entry"),
    F);
  theBuilder->SetInsertPoint(block);

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

    theBuilder->CreateRet(last);
  }

  mutable_variables = std::move(saved_mut);
  named_values = std::move(saved_named);
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
  llvm::Value* last = nullptr;
  for (auto const& s : node->stmts) {
    last = s->toLLVMIR(this);
    if (theBuilder->GetInsertBlock()->getTerminator()) {
      break;
    }
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

  llvm::FunctionCallee snprintf_fn = theModule->getOrInsertFunction(
    "snprintf",
    llvm::FunctionType::get(
      theBuilder->getInt32Ty(),
      {char_ptr, llvm::Type::getInt64Ty(*theContext), char_ptr},
      true));

  llvm::Type* a64 = llvm::ArrayType::get(theBuilder->getInt8Ty(), 64);
  llvm::AllocaInst* buf = theBuilder->CreateAlloca(a64, nullptr, "fmtbuf");
  llvm::Value* z32 = theBuilder->getInt32(0);
  llvm::Value* ge0 = theBuilder->CreateInBoundsGEP(a64, buf, {z32, z32});

  if (v->getType()->isIntegerTy()) {
    llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%lld", "styio_fmt_ll");
    llvm::Value* wi = v->getType()->isIntegerTy(64)
      ? v
      : theBuilder->CreateSExtOrTrunc(v, theBuilder->getInt64Ty());
    theBuilder->CreateCall(
      snprintf_fn,
      {ge0, theBuilder->getInt64(63), fmt, wi});
    return ge0;
  }

  if (v->getType()->isDoubleTy()) {
    llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%.6f", "styio_fmt_lf");
    theBuilder->CreateCall(
      snprintf_fn,
      {ge0, theBuilder->getInt64(63), fmt, v});
    return ge0;
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

  node->body->toLLVMIR(this);

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
StyioToLLVM::toLLVMIR(SGMatch* node) {
  llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
  llvm::IntegerType* i64ti = theBuilder->getInt64Ty();

  if (node->repr_kind == SGMatchReprKind::Stmt) {
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*theContext, "match_merge", F);
    if (not node->int_arms.empty()) {
      llvm::Value* sv = node->scrutinee->toLLVMIR(this);
      if (not sv->getType()->isIntegerTy(64)) {
        sv = theBuilder->CreateSExtOrTrunc(sv, i64ti);
      }
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

  llvm::Value* sv = node->scrutinee->toLLVMIR(this);
  if (not sv->getType()->isIntegerTy(64)) {
    sv = theBuilder->CreateSExtOrTrunc(sv, i64ti);
  }

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
  }

  theBuilder->SetInsertPoint(merge_bb);
  return phi;
}
