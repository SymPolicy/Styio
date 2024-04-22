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
  const string& resid = node->id;

  if (named_values.contains(resid)) {
    return named_values[resid];
  }

  if (mut_vars.contains(resid)) {
    llvm::AllocaInst* variable = mut_vars[resid];
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
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGFinalBind* node) {
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGFuncArg* node) {
  return theBuilder->getInt64(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SGFunc* node) {
  auto latest_insert_point = theBuilder->saveIP();

  std::string fname = node->func_name->id;

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

void
StyioToLLVM::execute() {
  auto RT = theORCJIT->getMainJITDylib().createResourceTracker();
  auto TSM = llvm::orc::ThreadSafeModule(std::move(theModule), std::move(theContext));
  llvm::ExitOnError exit_on_error;
  exit_on_error(theORCJIT->addModule(std::move(TSM), RT));

  // Look up the JIT'd code entry point.
  auto ExprSymbol = theORCJIT->lookup("main");
  if (!ExprSymbol) {
    std::cout << "not found" << std::endl;
    return;
  }

  std::cout << "after look up" << std::endl;

  // Cast the entry point address to a function pointer.
  int (*FP)() = ExprSymbol->getAddress().toPtr<int (*)()>();

  // Call into JIT'd code.
  std::cout << "result: " << FP() << std::endl;

  std::cout << "after run jit'd code" << std::endl;
}
