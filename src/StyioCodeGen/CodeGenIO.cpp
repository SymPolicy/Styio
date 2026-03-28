// [C++ STL]
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// [Styio]
#include "../StyioIR/IRDecl.hpp"
#include "../StyioIR/StyioIR.hpp"
#include "../StyioIR/IOIR/IOIR.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "CodeGenVisitor.hpp"
#include "../StyioUtil/Util.hpp"

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
StyioToLLVM::toLLVMIR(SIOPath* node) {
  auto output = theBuilder->getInt32(0);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVMIR(SIOPrint* node) {
  llvm::Type* char_ptr = llvm::PointerType::get(*theContext, 0);
  llvm::FunctionCallee printf_fn = theModule->getOrInsertFunction(
    "printf",
    llvm::FunctionType::get(theBuilder->getInt32Ty(), char_ptr, true));

  llvm::FunctionCallee puts_fn = theModule->getOrInsertFunction(
    "puts",
    llvm::FunctionType::get(theBuilder->getInt32Ty(), char_ptr, false));

  for (StyioIR* part : node->expr) {
    llvm::Value* v = part->toLLVMIR(this);

    if (v->getType()->isIntegerTy(1)) {
      llvm::Value* tstr = theBuilder->CreateGlobalStringPtr("true", "styio_true_nl");
      llvm::Value* fstr = theBuilder->CreateGlobalStringPtr("false", "styio_false_nl");
      llvm::Value* pick = theBuilder->CreateSelect(v, tstr, fstr);
      theBuilder->CreateCall(puts_fn, {pick});
    }
    else if (v->getType()->isIntegerTy(64)) {
      llvm::Value* sent = llvm::ConstantInt::get(
        theBuilder->getInt64Ty(),
        static_cast<uint64_t>(std::numeric_limits<int64_t>::min()),
        true);
      llvm::Value* isU = theBuilder->CreateICmpEQ(v, sent);
      llvm::Function* F = theBuilder->GetInsertBlock()->getParent();
      llvm::BasicBlock* b_at = llvm::BasicBlock::Create(*theContext, "print_at", F);
      llvm::BasicBlock* b_num = llvm::BasicBlock::Create(*theContext, "print_i64", F);
      llvm::BasicBlock* b_done = llvm::BasicBlock::Create(*theContext, "print_done", F);
      theBuilder->CreateCondBr(isU, b_at, b_num);
      theBuilder->SetInsertPoint(b_at);
      llvm::Value* ats = theBuilder->CreateGlobalStringPtr("@", "styio_print_at");
      theBuilder->CreateCall(puts_fn, {ats});
      theBuilder->CreateBr(b_done);
      theBuilder->SetInsertPoint(b_num);
      llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%lld\n", "styio_fmt_i64");
      theBuilder->CreateCall(printf_fn, {fmt, v});
      theBuilder->CreateBr(b_done);
      theBuilder->SetInsertPoint(b_done);
    }
    else if (v->getType()->isDoubleTy()) {
      llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%.6f\n", "styio_fmt_f64");
      theBuilder->CreateCall(printf_fn, {fmt, v});
    }
    else if (v->getType()->isPointerTy()) {
      llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%s\n", "styio_fmt_str");
      theBuilder->CreateCall(printf_fn, {fmt, v});
    }
    else {
      llvm::Value* fmt = theBuilder->CreateGlobalStringPtr("%lld\n", "styio_fmt_fallback");
      llvm::Value* as_i64 = theBuilder->CreatePtrToInt(v, theBuilder->getInt64Ty());
      theBuilder->CreateCall(printf_fn, {fmt, as_i64});
    }
  }

  return theBuilder->getInt32(0);
}

llvm::Value*
StyioToLLVM::toLLVMIR(SIORead* node) {
  auto output = theBuilder->getInt32(0);
  return output;
}
