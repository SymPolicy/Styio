#pragma once
#ifndef STYIO_CODE_GEN_VISITOR_H_
#define STYIO_CODE_GEN_VISITOR_H_

// [STL]
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// [Styio]
#include "../StyioJIT/StyioJIT_ORC.hpp"
#include "../StyioAST/ASTDecl.hpp"

// [LLVM]
#include "llvm/Analysis/CGSCCPassManager.h" /* CGSCCAnalysisManager */
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h" /* FunctionPassManager */
#include "llvm/IR/Module.h"
#include "llvm/IR/PassInstrumentation.h" /* PassInstrumentationCallbacks */
#include "llvm/IR/PassManager.h"         /* LoopAnalysisManager, FunctionAnalysisManager, ModuleAnalysisManager */
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
  virtual llvm::Type* getLLVMType(T* t) = 0;

  /* LLVM IR Generator */
  virtual llvm::Value* toLLVMIR(T* t) = 0;
};


template <typename T, typename... Types>
class CodeGenVisitor<T, Types...> : public CodeGenVisitor<Types...>
{
public:
  using CodeGenVisitor<Types...>::getLLVMType;
  using CodeGenVisitor<Types...>::toLLVMIR;

  /* Get LLVM Type */
  virtual llvm::Type* getLLVMType(T* t) = 0;

  /* LLVM IR Generator */
  virtual llvm::Value* toLLVMIR(T* t) = 0;
};


using StyioCodeGenVisitor = CodeGenVisitor<
  class CommentAST,

  class NoneAST,
  class EmptyAST,

  class BoolAST,
  class IntAST,
  class FloatAST,
  class CharAST,

  class StringAST,
  class SetAST,
  class ListAST,

  class StructAST,
  class TupleAST,

  class IdAST,
  class DTypeAST,

  class VarAST,
  class ArgAST,
  class OptArgAST,
  class OptKwArgAST,

  class FlexBindAST,
  class FinalBindAST,

  class BinCompAST,
  class CondAST,
  class BinOpAST,

  class AnonyFuncAST,
  class FuncAST,

  class CallAST,

  class SizeOfAST,
  class TypeConvertAST,
  class ListOpAST,
  class RangeAST,

  class IterAST,
  class LoopAST,

  class CondFlowAST,

  class EOFAST,
  class PassAST,
  class BreakAST,
  class ReturnAST,

  class CasesAST,
  class MatchCasesAST,

  class BlockAST,
  class MainBlockAST,

  class ExtPackAST,

  class InfiniteAST,

  class VarTupleAST,

  class ForwardAST,
  class CheckEqAST,
  class CheckIsinAST,
  class FromToAST,

  class FmtStrAST,

  class ResourceAST,

  class LocalPathAST,
  class RemotePathAST,
  class WebUrlAST,
  class DBUrlAST,

  class PrintAST,
  class ReadFileAST>;


class StyioToLLVMIR : public StyioCodeGenVisitor
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

  unordered_map<string, FuncAST*> func_defs;

public:
  StyioToLLVMIR(std::unique_ptr<StyioJIT_ORC> styio_jit) :
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

  ~StyioToLLVMIR() {}

  static StyioToLLVMIR* Create(std::unique_ptr<StyioJIT_ORC> styio_jit) {
    return new StyioToLLVMIR(std::move(styio_jit));
  }

  /* Utility Functions */
  llvm::Type* matchType(string type);

  llvm::AllocaInst* createAllocaFuncEntry(llvm::Function* TheFunction, llvm::StringRef VarName);

  /* Get LLVM Type */

  llvm::Type* getLLVMType(BoolAST* ast);

  llvm::Type* getLLVMType(NoneAST* ast);

  llvm::Type* getLLVMType(EOFAST* ast);

  llvm::Type* getLLVMType(EmptyAST* ast);

  llvm::Type* getLLVMType(PassAST* ast);

  llvm::Type* getLLVMType(BreakAST* ast);

  llvm::Type* getLLVMType(ReturnAST* ast);

  llvm::Type* getLLVMType(CommentAST* ast);

  llvm::Type* getLLVMType(IdAST* ast);

  llvm::Type* getLLVMType(VarAST* ast);

  llvm::Type* getLLVMType(ArgAST* ast);

  llvm::Type* getLLVMType(OptArgAST* ast);

  llvm::Type* getLLVMType(OptKwArgAST* ast);

  llvm::Type* getLLVMType(VarTupleAST* ast);

  llvm::Type* getLLVMType(DTypeAST* ast);

  llvm::Type* getLLVMType(IntAST* ast);

  llvm::Type* getLLVMType(FloatAST* ast);

  llvm::Type* getLLVMType(CharAST* ast);

  llvm::Type* getLLVMType(StringAST* ast);

  llvm::Type* getLLVMType(TypeConvertAST* ast);

  llvm::Type* getLLVMType(FmtStrAST* ast);

  llvm::Type* getLLVMType(ListAST* ast);

  llvm::Type* getLLVMType(TupleAST* ast);

  llvm::Type* getLLVMType(SetAST* ast);

  llvm::Type* getLLVMType(RangeAST* ast);

  llvm::Type* getLLVMType(SizeOfAST* ast);

  llvm::Type* getLLVMType(BinOpAST* ast);

  llvm::Type* getLLVMType(BinCompAST* ast);

  llvm::Type* getLLVMType(CondAST* ast);

  llvm::Type* getLLVMType(CallAST* ast);

  llvm::Type* getLLVMType(ListOpAST* ast);

  llvm::Type* getLLVMType(ResourceAST* ast);

  llvm::Type* getLLVMType(LocalPathAST* ast);

  llvm::Type* getLLVMType(RemotePathAST* ast);

  llvm::Type* getLLVMType(WebUrlAST* ast);

  llvm::Type* getLLVMType(DBUrlAST* ast);

  llvm::Type* getLLVMType(FlexBindAST* ast);

  llvm::Type* getLLVMType(FinalBindAST* ast);

  llvm::Type* getLLVMType(StructAST* ast);

  llvm::Type* getLLVMType(ReadFileAST* ast);

  llvm::Type* getLLVMType(PrintAST* ast);

  llvm::Type* getLLVMType(ExtPackAST* ast);

  llvm::Type* getLLVMType(BlockAST* ast);

  llvm::Type* getLLVMType(CasesAST* ast);

  llvm::Type* getLLVMType(CondFlowAST* ast);

  llvm::Type* getLLVMType(CheckEqAST* ast);

  llvm::Type* getLLVMType(CheckIsinAST* ast);

  llvm::Type* getLLVMType(FromToAST* ast);

  llvm::Type* getLLVMType(ForwardAST* ast);

  llvm::Type* getLLVMType(InfiniteAST* ast);

  llvm::Type* getLLVMType(AnonyFuncAST* ast);

  llvm::Type* getLLVMType(FuncAST* ast);

  llvm::Type* getLLVMType(LoopAST* ast);

  llvm::Type* getLLVMType(IterAST* ast);

  llvm::Type* getLLVMType(MatchCasesAST* ast);

  llvm::Type* getLLVMType(MainBlockAST* ast);

  /* LLVM IR Generator */

  llvm::Value* toLLVMIR(BoolAST* ast);

  llvm::Value* toLLVMIR(NoneAST* ast);

  llvm::Value* toLLVMIR(EOFAST* ast);

  llvm::Value* toLLVMIR(EmptyAST* ast);

  llvm::Value* toLLVMIR(PassAST* ast);

  llvm::Value* toLLVMIR(BreakAST* ast);

  llvm::Value* toLLVMIR(ReturnAST* ast);

  llvm::Value* toLLVMIR(CommentAST* ast);

  llvm::Value* toLLVMIR(IdAST* ast);

  llvm::Value* toLLVMIR(VarAST* ast);

  llvm::Value* toLLVMIR(ArgAST* ast);

  llvm::Value* toLLVMIR(OptArgAST* ast);

  llvm::Value* toLLVMIR(OptKwArgAST* ast);

  llvm::Value* toLLVMIR(VarTupleAST* ast);

  llvm::Value* toLLVMIR(DTypeAST* ast);

  llvm::Value* toLLVMIR(IntAST* ast);

  llvm::Value* toLLVMIR(FloatAST* ast);

  llvm::Value* toLLVMIR(CharAST* ast);

  llvm::Value* toLLVMIR(StringAST* ast);

  llvm::Value* toLLVMIR(TypeConvertAST* ast);

  llvm::Value* toLLVMIR(FmtStrAST* ast);

  llvm::Value* toLLVMIR(ListAST* ast);

  llvm::Value* toLLVMIR(TupleAST* ast);

  llvm::Value* toLLVMIR(SetAST* ast);

  llvm::Value* toLLVMIR(RangeAST* ast);

  llvm::Value* toLLVMIR(SizeOfAST* ast);

  llvm::Value* toLLVMIR(BinOpAST* ast);

  llvm::Value* toLLVMIR(BinCompAST* ast);

  llvm::Value* toLLVMIR(CondAST* ast);

  llvm::Value* toLLVMIR(CallAST* ast);

  llvm::Value* toLLVMIR(ListOpAST* ast);

  llvm::Value* toLLVMIR(ResourceAST* ast);

  llvm::Value* toLLVMIR(LocalPathAST* ast);

  llvm::Value* toLLVMIR(RemotePathAST* ast);

  llvm::Value* toLLVMIR(WebUrlAST* ast);

  llvm::Value* toLLVMIR(DBUrlAST* ast);

  llvm::Value* toLLVMIR(FlexBindAST* ast);

  llvm::Value* toLLVMIR(FinalBindAST* ast);

  llvm::Value* toLLVMIR(StructAST* ast);

  llvm::Value* toLLVMIR(ReadFileAST* ast);

  llvm::Value* toLLVMIR(PrintAST* ast);

  llvm::Value* toLLVMIR(ExtPackAST* ast);

  llvm::Value* toLLVMIR(BlockAST* ast);

  llvm::Value* toLLVMIR(CasesAST* ast);

  llvm::Value* toLLVMIR(CondFlowAST* ast);

  llvm::Value* toLLVMIR(CheckEqAST* ast);

  llvm::Value* toLLVMIR(CheckIsinAST* ast);

  llvm::Value* toLLVMIR(FromToAST* ast);

  llvm::Value* toLLVMIR(ForwardAST* ast);

  llvm::Value* toLLVMIR(InfiniteAST* ast);

  llvm::Value* toLLVMIR(AnonyFuncAST* ast);

  llvm::Value* toLLVMIR(FuncAST* ast);

  llvm::Value* toLLVMIR(LoopAST* ast);

  llvm::Value* toLLVMIR(IterAST* ast);

  llvm::Value* toLLVMIR(MatchCasesAST* ast);

  // llvm::Value* toLLVMIR(MainBlockAST* ast);
  llvm::Function* toLLVMIR(MainBlockAST* ast);

  void print_llvm_ir();
  void print_test_results();

  void execute();
};

#endif