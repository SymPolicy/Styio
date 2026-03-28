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

llvm::Value*
StyioToLLVM::toLLVMIR(SGFunc* node) {
  auto latest_insert_point = theBuilder->saveIP();

  std::string fname = node->func_name->as_str();

  if (node->func_args.empty()) {
    llvm::Function* llvm_func = llvm::Function::Create(
      llvm::FunctionType::get(
        /* Result (Type) */ node->ret_type->toLLVMType(this),
        /* isVarArg */ false
      ),
      llvm::GlobalValue::ExternalLinkage,
      fname,
      *theModule
    );

    llvm::BasicBlock* llvm_basic_block = llvm::BasicBlock::Create(
      *theContext,
      (fname + "_entry"),
      llvm_func
    );

    theBuilder->SetInsertPoint(llvm_basic_block);

    node->func_block->toLLVMIR(this);
  }
  else {
    std::vector<llvm::Type*> llvm_func_args;
    for (auto& arg : node->func_args) {
      llvm_func_args.push_back(arg->toLLVMType(this));
    }

    llvm::Function* llvm_func =
      llvm::Function::Create(
        llvm::FunctionType::get(
          /* Result (Type) */ node->ret_type->toLLVMType(this),
          /* Params (Type) */ llvm_func_args,
          /* isVarArg */ false
        ),
        llvm::GlobalValue::ExternalLinkage,
        fname,
        *theModule
      );

    for (size_t i = 0; i < llvm_func->arg_size(); i++) {
      llvm_func->getArg(i)->setName(node->func_args[i]->id);
    }

    llvm::BasicBlock* block = llvm::BasicBlock::Create(
      *theContext,
      (fname + "_entry"),
      llvm_func
    );

    theBuilder->SetInsertPoint(block);

    /* Initialize Arguments */
    // for (auto& arg : llvm_func->args()) {
    //   llvm::AllocaInst* alloca_inst = theBuilder->CreateAlloca(
    //     llvm::Type::getInt32Ty(*theContext),
    //     nullptr,
    //     arg.getName()
    //   );

    //   auto init_val = theBuilder->getInt32(0);

    //   theBuilder->CreateStore(&arg, alloca_inst);

    //   // mut_vars[std::string(arg.getName())] = alloca_inst;
    // }

    node->func_block->toLLVMIR(this);

    // for (auto& arg : llvm_func->args()) {
    //   mut_vars.erase(std::string(arg.getName()));
    // }
  }

  theBuilder->restoreIP(latest_insert_point);

  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGCall* node) {
  return theBuilder->getInt64(0);
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
  /*
    Get Void Type: llvm::Type::getVoidTy(*llvm_context)
    Use Void Type: nullptr
  */
  llvm::Function* main_func = llvm::Function::Create(
    llvm::FunctionType::get(theBuilder->getInt32Ty(), false),
    llvm::Function::ExternalLinkage,
    "main",
    *theModule
  );
  llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(*theContext, "main_entry", main_func);

  /* Add statements to the current basic block */
  theBuilder->SetInsertPoint(entry_block);

  for (auto const& s : node->stmts) {
    s->toLLVMIR(this);
  }

  // entry_block->getInstList()

  theBuilder->CreateRet(theBuilder->getInt32(0));

  return main_func;
}
