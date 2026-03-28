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
      if (data_type.isInteger()) {
        if (r_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
          return theBuilder->CreateFAdd(l_val, r_val);
        }
        return theBuilder->CreateAdd(l_val, r_val);
      }
      else if (data_type.isFloat()) {
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFAdd(l_val, r_val);
      }
    } break;

    case StyioOpType::Binary_Sub: {
      if (data_type.isInteger()) {
        return theBuilder->CreateSub(l_val, r_val);
      }
      else if (data_type.isFloat()) {
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFSub(l_val, r_val);
      }
    } break;

    case StyioOpType::Binary_Mul: {
      if (data_type.isInteger()) {
        if (r_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
          return theBuilder->CreateFMul(l_val, r_val);
        }
        return theBuilder->CreateMul(l_val, r_val);
      }
      else if (data_type.isFloat()) {
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFMul(l_val, r_val);
      }
    } break;

    case StyioOpType::Binary_Div: {
      if (data_type.isInteger()) {
        return theBuilder->CreateSDiv(l_val, r_val);
      }
      else if (data_type.isFloat()) {
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFDiv(l_val, r_val);
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
      if (data_type.isInteger()) {
        return theBuilder->CreateSRem(l_val, r_val);
      }
      else if (data_type.isFloat()) {
        if (not l_val->getType()->isDoubleTy()) {
          l_val = theBuilder->CreateSIToFP(l_val, theBuilder->getDoubleTy());
        }
        if (not r_val->getType()->isDoubleTy()) {
          r_val = theBuilder->CreateSIToFP(r_val, theBuilder->getDoubleTy());
        }
        return theBuilder->CreateFRem(l_val, r_val);
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
  for (auto const& s : node->stmts) {
    s->toLLVMIR(this);
  }

  return theBuilder->getInt64(0);
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
