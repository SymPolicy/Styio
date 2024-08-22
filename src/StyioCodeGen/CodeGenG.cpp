// [C++ STL]
#include <cstdio>
#include <cstdlib>
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
  return llvm::ConstantDataArray::getString(*theContext, node->value, /* AddNull */ true);
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
  llvm::Value* l_val = node->lhs_expr->toLLVMIR(this);
  llvm::Value* r_val = node->rhs_expr->toLLVMIR(this);

  switch (node->operand) {
    case StyioOpType::Binary_Add: {
      if (data_type.isInteger()) {
        return theBuilder->CreateAdd(l_val, r_val);
      }
      else if (data_type.isFloat()) {
        return theBuilder->CreateFAdd(l_val, r_val);
      }
    } break;

    case StyioOpType::Binary_Sub: {
      if (data_type.isInteger()) {
        return theBuilder->CreateSub(l_val, r_val);
      }
      else if (data_type.isFloat()) {
        return theBuilder->CreateFSub(l_val, r_val);
      }
    } break;

    case StyioOpType::Binary_Mul: {
      if (data_type.isInteger()) {
        return theBuilder->CreateMul(l_val, r_val);
      }
      else if (data_type.isFloat()) {
        return theBuilder->CreateFMul(l_val, r_val);
      }
    } break;

    case StyioOpType::Binary_Div: {
      /* Signed Integer */
      if (data_type.isInteger()) {
        return theBuilder->CreateSDiv(l_val, r_val);
      }
      else if (data_type.isFloat()) {
        return theBuilder->CreateFDiv(l_val, r_val);
      }
    } break;

    case StyioOpType::Binary_Pow: {
    } break;

    case StyioOpType::Binary_Mod: {
    } break;

    case StyioOpType::Self_Add_Assign: {
    } break;

    case StyioOpType::Self_Sub_Assign: {
    } break;

    case StyioOpType::Self_Mul_Assign: {
    } break;

    case StyioOpType::Self_Div_Assign: {
    } break;

    default:
      break;
  }

  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGCond* node) {
  auto output = theBuilder->getInt32(0);
  return output;
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
