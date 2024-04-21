#pragma once
#ifndef STYIO_CODE_GEN_VISITOR_H_
#define STYIO_CODE_GEN_VISITOR_H_

// [STL]
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// [Styio]
#include "../StyioIR/IRDecl.hpp"
#include "../StyioJIT/StyioJIT_ORC.hpp"

// [LLVM]
#include "llvm/Analysis/CGSCCPassManager.h" /* CGSCCAnalysisManager */
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h" /* FunctionPassManager */
#include "llvm/IR/Module.h"
#include "llvm/IR/PassInstrumentation.h" /* PassInstrumentationCallbacks */
#include "llvm/IR/PassManager.h"         /* LoopAnalysisManager, FunctionAnalysisManager, ModuleAnalysisManager */
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassBuilder.h"                 /* PassBuilder */
#include "llvm/Passes/StandardInstrumentations.h"    /* StandardInstrumentations.h */
#include "llvm/Support/TargetSelect.h"               /* InitializeNativeTarget, InitializeNativeTargetAsmPrinter, InitializeNativeTargetAsmParser */
#include "llvm/Transforms/InstCombine/InstCombine.h" /* InstCombinePass */
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"         /* GVNPass */
#include "llvm/Transforms/Scalar/Reassociate.h" /* ReassociatePass */
#include "llvm/Transforms/Scalar/SimplifyCFG.h" /* SimplifyCFGPass */
#include "llvm/Transforms/Utils.h"

using std::string;
using std::unordered_map;
using std::vector;

using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

// Generic Visitor
template <typename... Types>
class CodeGenVisitor;

template <typename T>
class CodeGenVisitor<T>
{
public:
  /* Get LLVM Type */
  virtual llvm::Type* toLLVMType(T* t) = 0;

  /* LLVM IR Generator */
  virtual llvm::Value* toLLVMIR(T* t) = 0;
};

template <typename T, typename... Types>
class CodeGenVisitor<T, Types...> : public CodeGenVisitor<Types...>
{
public:
  using CodeGenVisitor<Types...>::toLLVMType;
  using CodeGenVisitor<Types...>::toLLVMIR;

  /* Get LLVM Type */
  virtual llvm::Type* toLLVMType(T* t) = 0;

  /* LLVM IR Generator */
  virtual llvm::Value* toLLVMIR(T* t) = 0;
};

using StyioCodeGenVisitor = CodeGenVisitor<
  class SGResId,

  class SGConstInt,
  class SGConstFloat,

  class SGConstString,
  class SGFormatString,

  class SGBinOp,

  class SGFuncArgs,
  class SGFunction,
  class SGCall,

  class SGBlock,
  class SGEntry,
  class SGMainEntry>;

class StyioToLLVM : public StyioCodeGenVisitor
{
  unique_ptr<llvm::LLVMContext> theContext;
  unique_ptr<llvm::Module> theModule;
  unique_ptr<llvm::IRBuilder<>> theBuilder;

  std::unique_ptr<StyioJIT_ORC> theORCJIT;

  unique_ptr<llvm::FunctionPassManager> theFPM;
  unique_ptr<llvm::LoopAnalysisManager> theLAM;
  unique_ptr<llvm::FunctionAnalysisManager> theFAM;
  unique_ptr<llvm::CGSCCAnalysisManager> theCGAM;
  unique_ptr<llvm::ModuleAnalysisManager> theMAM;
  unique_ptr<llvm::PassInstrumentationCallbacks> thePIC;
  unique_ptr<llvm::StandardInstrumentations> theSI;
  llvm::PassBuilder thePB;

  unordered_map<string, llvm::AllocaInst*> mut_vars; /* [FlexBind] Mutable Variables */
  unordered_map<string, llvm::Value*> named_values;  /* [FinalBind] Named Values = Immutable Variables */

public:
  StyioToLLVM(std::unique_ptr<StyioJIT_ORC> styio_jit) :
      theContext(std::make_unique<llvm::LLVMContext>()),
      theModule(std::make_unique<llvm::Module>("styio", *theContext)),
      theBuilder(std::make_unique<llvm::IRBuilder<>>(*theContext)),
      theORCJIT(std::move(styio_jit)),
      theFPM(std::make_unique<llvm::FunctionPassManager>()),
      theLAM(std::make_unique<llvm::LoopAnalysisManager>()),
      theFAM(std::make_unique<llvm::FunctionAnalysisManager>()),
      theCGAM(std::make_unique<llvm::CGSCCAnalysisManager>()),
      theMAM(std::make_unique<llvm::ModuleAnalysisManager>()),
      thePIC(std::make_unique<llvm::PassInstrumentationCallbacks>()),
      theSI(std::make_unique<llvm::StandardInstrumentations>(*theContext, /*DebugLogging*/ true)) {
    theModule->setDataLayout(theORCJIT->getDataLayout());

    theSI->registerCallbacks(*thePIC, theMAM.get());

    // Add transform passes.
    // Do simple "peephole" optimizations and bit-twiddling optimizations.
    theFPM->addPass(llvm::InstCombinePass());
    // Reassociate expressions.
    theFPM->addPass(llvm::ReassociatePass());
    // Eliminate common sub-expressions.
    theFPM->addPass(llvm::GVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    theFPM->addPass(llvm::SimplifyCFGPass());

    thePB.registerModuleAnalyses(*theMAM);
    thePB.registerFunctionAnalyses(*theFAM);
    thePB.crossRegisterProxies(*theLAM, *theFAM, *theCGAM, *theMAM);
  }

  ~StyioToLLVM() {}

  static StyioToLLVM* Create(std::unique_ptr<StyioJIT_ORC> styio_jit) {
    return new StyioToLLVM(std::move(styio_jit));
  }

  llvm::Type* toLLVMType(SGResId* node);
  llvm::Type* toLLVMType(SGConstInt* node);
  llvm::Type* toLLVMType(SGConstFloat* node);
  llvm::Type* toLLVMType(SGConstString* node);
  llvm::Type* toLLVMType(SGFormatString* node);
  llvm::Type* toLLVMType(SGBinOp* node);
  llvm::Type* toLLVMType(SGFuncArg* node);
  llvm::Type* toLLVMType(SGFunc* node);
  llvm::Type* toLLVMType(SGCall* node);
  llvm::Type* toLLVMType(SGBlock* node);
  llvm::Type* toLLVMType(SGEntry* node);
  llvm::Type* toLLVMType(SGMainEntry* node);

  /* LLVM Code Generation */
  llvm::Value* toLLVMIR(SGResId* node);

  llvm::Value* toLLVMIR(SGConstInt* node);
  llvm::Value* toLLVMIR(SGConstFloat* node);

  llvm::Value* toLLVMIR(SGConstString* node);
  llvm::Value* toLLVMIR(SGFormatString* node);

  llvm::Value* toLLVMIR(SGBinOp* node);

  llvm::Value* toLLVMIR(SGFuncArg* node);
  llvm::Value* toLLVMIR(SGFunc* node);
  llvm::Value* toLLVMIR(SGCall* node);

  llvm::Value* toLLVMIR(SGBlock* node);
  llvm::Value* toLLVMIR(SGEntry* node);
  llvm::Value* toLLVMIR(SGMainEntry* node);
};

#endif