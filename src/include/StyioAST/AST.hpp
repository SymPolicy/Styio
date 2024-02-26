#pragma once
#ifndef STYIO_AST_H_
#define STYIO_AST_H_

// [Styio]
#include "../StyioJIT/StyioJIT_ORC.hpp"
#include "../StyioToken/Token.hpp"

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
class Visitor;

template <typename T>
class Visitor<T>
{
public:
  /* Type Inference */
  virtual void typeInfer(T* t) = 0;

  /* Get LLVM Type */
  virtual llvm::Type* getLLVMType(T* t) = 0;

  /* LLVM IR Generator */
  virtual llvm::Value* toLLVMIR(T* t) = 0;
};

template <typename T, typename... Types>
class Visitor<T, Types...> : public Visitor<Types...>
{
public:
  using Visitor<Types...>::typeInfer;
  using Visitor<Types...>::getLLVMType;
  using Visitor<Types...>::toLLVMIR;

  /* Type Inference */
  virtual void typeInfer(T* t) = 0;

  /* Get LLVM Type */
  virtual llvm::Type* getLLVMType(T* t) = 0;

  /* LLVM IR Generator */
  virtual llvm::Value* toLLVMIR(T* t) = 0;
};

/* Forward Declaration */

/* Styio AST */
// Base
class StyioAST;

// Comment
class CommentAST;

// None / Empty
class NoneAST;
class EmptyAST;

/*
  Literals:
  - Bool (Boolean)
  - Int (Integer)
  - Float
  - Char (Character)
*/
class BoolAST;
class IntAST;
class FloatAST;
class CharAST;

/*
  Collections:
  - string [homo]

  - set [homo]
  - list [heter]

  - struct [heter]
  - tuple [heter]
*/
class StringAST;
class SetAST;
class ListAST;

class StructAST;
class TupleAST;

/*
  Variable:
  - Id
  - DType (Data Type)
*/
class IdAST;
class DTypeAST;

/*
  Variable Types:
  - Var ([Global|Local] Variables)
  - Arg (Function Arguments)
  - OptArg (Optional Function Arguments)
  - OptKwArg (Optional Function Arguments)
*/
class VarAST;
class ArgAST;
class OptArgAST;
class OptKwArgAST;

/*
  Assignment
  - FlexBind
  - FinalBind
*/
class FlexBindAST;
class FinalBindAST;

/*
  Binary Tree:
  - BinComp (Comparisons)
  - Cond (Condition)
  - BinOp (Binary Operations)
*/
class BinCompAST;
class CondAST;
class BinOpAST;

/*
  Function
  - Anonymous Function
  - Function

  - Call
*/
class AnonyFuncAST;
class FuncAST;

class CallAST;

/*
  Methods:
  - SizeOf

  - TypeOf
  - TypeConvert

  - Range
  - ListOp
*/
class SizeOfAST;

class TypeOfAST;
class TypeConvertAST;

class ListOpAST;

class RangeAST;

/*
  Iteration:
  - Iter (Iterator)
  - Loop
*/
class IterAST;
class LoopAST;

/*
  Control Flow
  - CondFlow (Conditional)

  - End-Of-Line
  - `pass` ..........
  - `break` ^^^^^^^^^
  - `continue` >>>>>>
  - `return` <<<<<<<<
*/
class CondFlowAST;

class EOFAST;
class PassAST;
class BreakAST;
class ContinueAST;
class ReturnAST;

/*
  Cases
  - Cases
  - MatchCases
*/
class CasesAST;
class MatchCasesAST;

/*
  Blocks
  - SideBlock
  - MainBlock
*/
class BlockAST;
class MainBlockAST;

/*
  Package Management
  - ExtPack
*/
class ExtPackAST;

/*
  Conceptions:
  - Infinite
*/
class InfiniteAST;

/*
  Intermediate Components:
  - VarTupleAST
*/
class VarTupleAST;

class ForwardAST;
class CheckEqAST;
class CheckIsinAST;
class FromToAST;

/*
  Features:
  - FmtStr (Format String)
*/
class FmtStrAST;

/*
  Resources:
  - ResourceAST

  - LocalPath
  - RemotePath
  - WebUrl
  - DBUrl
*/
class ResourceAST;

class LocalPathAST;
class RemotePathAST;
class WebUrlAST;
class DBUrlAST;

/*
  Methods (I/O)
  - print
  - read file
  - write file
*/
class PrintAST;
class ReadFileAST;
class WriteFileAST;

using StyioVisitor = Visitor<
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

/* Styio -> LLVM Generator */
class StyioToLLVM;

class StyioToLLVM : public StyioVisitor
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

  /* Utility Functions */
  llvm::Type* matchType(string type);

  llvm::AllocaInst* createAllocaFuncEntry(llvm::Function* TheFunction, llvm::StringRef VarName);

  /* Styio AST Type typeInferer*/

  void typeInfer(BoolAST* ast);

  void typeInfer(NoneAST* ast);

  void typeInfer(EOFAST* ast);

  void typeInfer(EmptyAST* ast);

  void typeInfer(PassAST* ast);

  void typeInfer(BreakAST* ast);

  void typeInfer(ReturnAST* ast);

  void typeInfer(CommentAST* ast);

  void typeInfer(IdAST* ast);

  void typeInfer(VarAST* ast);

  void typeInfer(ArgAST* ast);

  void typeInfer(OptArgAST* ast);

  void typeInfer(OptKwArgAST* ast);

  void typeInfer(VarTupleAST* ast);

  void typeInfer(DTypeAST* ast);

  void typeInfer(IntAST* ast);

  void typeInfer(FloatAST* ast);

  void typeInfer(CharAST* ast);

  void typeInfer(StringAST* ast);

  void typeInfer(TypeConvertAST* ast);

  void typeInfer(FmtStrAST* ast);

  void typeInfer(LocalPathAST* ast);

  void typeInfer(RemotePathAST* ast);

  void typeInfer(WebUrlAST* ast);

  void typeInfer(DBUrlAST* ast);

  void typeInfer(ListAST* ast);

  void typeInfer(TupleAST* ast);

  void typeInfer(SetAST* ast);

  void typeInfer(RangeAST* ast);

  void typeInfer(SizeOfAST* ast);

  void typeInfer(BinOpAST* ast);

  void typeInfer(BinCompAST* ast);

  void typeInfer(CondAST* ast);

  void typeInfer(CallAST* ast);

  void typeInfer(ListOpAST* ast);

  void typeInfer(ResourceAST* ast);

  void typeInfer(FlexBindAST* ast);

  void typeInfer(FinalBindAST* ast);

  void typeInfer(StructAST* ast);

  void typeInfer(ReadFileAST* ast);

  void typeInfer(PrintAST* ast);

  void typeInfer(ExtPackAST* ast);

  void typeInfer(BlockAST* ast);

  void typeInfer(CasesAST* ast);

  void typeInfer(CondFlowAST* ast);

  void typeInfer(CheckEqAST* ast);

  void typeInfer(CheckIsinAST* ast);

  void typeInfer(FromToAST* ast);

  void typeInfer(ForwardAST* ast);

  void typeInfer(InfiniteAST* ast);

  void typeInfer(AnonyFuncAST* ast);

  void typeInfer(FuncAST* ast);

  void typeInfer(LoopAST* ast);

  void typeInfer(IterAST* ast);

  void typeInfer(MatchCasesAST* ast);

  void typeInfer(MainBlockAST* ast);

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

  void print_type_infer(StyioAST* program);

  void print_llvm_ir();
  void print_test_results();

  void execute();
};

/*
  StyioAST: Styio Base AST
*/
class StyioAST
{
public:
  virtual ~StyioAST() {}

  /* type hint */
  virtual StyioNodeHint hint() = 0;

  /* toString */
  virtual string toString(
    int indent = 0,
    bool colorful = false
  ) = 0;
  virtual string toStringInline(
    int indent = 0,
    bool colorful = false
  ) = 0;

  /* Type Inference */
  virtual void typeInfer(StyioToLLVM* visitor) = 0;

  /* Get LLVM Type */
  virtual llvm::Type* getLLVMType(StyioToLLVM* generator) = 0;

  /* LLVM IR Generator */
  virtual llvm::Value* toLLVMIR(StyioToLLVM* generator) = 0;
};

template <class Derived>
class StyioNode : public StyioAST
{
public:
  using StyioAST::hint;
  using StyioAST::toString;
  using StyioAST::toStringInline;

  void typeInfer(StyioToLLVM* visitor) override {
    visitor->typeInfer(static_cast<Derived*>(this));
  }

  llvm::Type* getLLVMType(StyioToLLVM* visitor) override {
    return visitor->getLLVMType(static_cast<Derived*>(this));
  }

  llvm::Value* toLLVMIR(StyioToLLVM* visitor) override {
    visitor->typeInfer(static_cast<Derived*>(this));
    return visitor->toLLVMIR(static_cast<Derived*>(this));
  }
};

class CommentAST : public StyioNode<CommentAST>
{
private:
  string Text;

public:
  CommentAST(const string& text) :
      Text(text) {
  }

  static CommentAST* Create(const string& text) {
    return new CommentAST(text);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Comment;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/* ========================================================================== */

class IdAST : public StyioNode<IdAST>
{
  string Id;

public:
  IdAST(const string& id) :
      Id(id) {
  }

  static IdAST* Create(const string& id) {
    return new IdAST(id);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Id;
  }

  const string& getAsStr() const {
    return Id;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

class DTypeAST : public StyioNode<DTypeAST>
{
private:
  string type_name;
  StyioDataType data_type = StyioDataType::undefined;

public:
  DTypeAST(StyioDataType data_type) :
      data_type(data_type) {
  }

  DTypeAST(
    const string& type_name
  ) :
      type_name(type_name) {
    auto it = DType_Table.find(type_name);
    if (it != DType_Table.end()) {
      data_type = it->second;
    }
    else {
      data_type = StyioDataType::undefined;
    }
  }

  static DTypeAST* Create() {
    return new DTypeAST(StyioDataType::undefined);
  }

  static DTypeAST* Create(StyioDataType data_type) {
    return new DTypeAST(data_type);
  }

  static DTypeAST* Create(const string& type_name) {
    return new DTypeAST(type_name);
  }

  StyioDataType getDType() {
    return data_type;
  }

  const string& getTypeName() const {
    return type_name;
  }

  void setDType(StyioDataType type) {
    data_type = type;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::DType;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/* ========================================================================== */

/*
  NoneAST: None / Null / Nil
*/
class NoneAST : public StyioNode<NoneAST>
{
public:
  NoneAST() {}

  static NoneAST* Create() {
    return new NoneAST();
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::None;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  EmptyAST: Empty
*/
class EmptyAST : public StyioNode<EmptyAST>
{
public:
  EmptyAST() {}

  static EmptyAST* Create() {
    return new EmptyAST();
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Empty;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  BoolAST: Boolean
*/
class BoolAST : public StyioNode<BoolAST>
{
  bool Value;

public:
  BoolAST(bool value) :
      Value(value) {
  }

  static BoolAST* Create(bool value) {
    return new BoolAST(value);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Bool;
  }

  bool getValue() {
    return Value;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  IntAST: Integer
*/
class IntAST : public StyioNode<IntAST>
{
private:
  string value;
  StyioDataType data_type = StyioDataType::i32;

public:
  IntAST(string value) :
      value(value) {
  }

  IntAST(string value, StyioDataType data_type) :
      value(value), data_type(data_type) {
  }

  static IntAST* Create(string value) {
    return new IntAST(value);
  }

  static IntAST* Create(string value, StyioDataType data_type) {
    return new IntAST(value, data_type);
  }

  const string& getValue() {
    return value;
  }

  StyioDataType getType() {
    return data_type;
  }

  void setType(StyioDataType type) {
    this->data_type = type;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Int;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  FloatAST: Float
*/
class FloatAST : public StyioNode<FloatAST>
{
private:
  string value;
  StyioDataType data_type = StyioDataType::f64;

public:
  FloatAST(const string& value) :
      value(value) {
  }

  static FloatAST* Create(const string& value) {
    return new FloatAST(value);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Float;
  }

  StyioDataType getType() {
    return data_type;
  }

  const string& getValue() {
    return value;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  CharAST: Single Character
*/
class CharAST : public StyioNode<CharAST>
{
  string value;

public:
  CharAST(
    const string& value
  ) :
      value(value) {
  }

  static CharAST* Create(const string& value) {
    return new CharAST(value);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Char;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  StringAST: String
*/
class StringAST : public StyioNode<StringAST>
{
  string value;

public:
  StringAST(const string& value) :
      value(value) {
  }

  static StringAST* Create(const string& value) {
    return new StringAST(value);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::String;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/* ========================================================================== */

class CasesAST : public StyioNode<CasesAST>
{
  vector<std::pair<StyioAST*, StyioAST*>> Cases;
  StyioAST* LastExpr = nullptr;

public:
  CasesAST(StyioAST* expr) :
      LastExpr((expr)) {
  }

  CasesAST(vector<std::pair<StyioAST*, StyioAST*>> cases, StyioAST* expr) :
      Cases(cases), LastExpr(expr) {
  }

  static CasesAST* Create(StyioAST* expr) {
    return new CasesAST(expr);
  }

  static CasesAST* Create(vector<std::pair<StyioAST*, StyioAST*>> cases, StyioAST* expr) {
    return new CasesAST(cases, expr);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Cases;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

class BlockAST : public StyioNode<BlockAST>
{
  StyioAST* Resources = nullptr;
  vector<StyioAST*> Stmts;

public:
  BlockAST(StyioAST* resources, vector<StyioAST*> stmts) :
      Resources(resources),
      Stmts(stmts) {
  }

  BlockAST(vector<StyioAST*> stmts) :
      Stmts(stmts) {
  }

  static BlockAST* Create(vector<StyioAST*> stmts) {
    return new BlockAST(stmts);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Block;
  }

  vector<StyioAST*> getStmts() {
    return Stmts;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

class MainBlockAST : public StyioNode<MainBlockAST>
{
  StyioAST* Resources = nullptr;
  vector<StyioAST*> Stmts;

public:
  MainBlockAST(
    StyioAST* resources,
    vector<StyioAST*> stmts
  ) :
      Resources((resources)),
      Stmts((stmts)) {
  }

  MainBlockAST(
    vector<StyioAST*> stmts
  ) :
      Stmts((stmts)) {
  }

  static MainBlockAST* Create(
    vector<StyioAST*> stmts
  ) {
    return new MainBlockAST(stmts);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::MainBlock;
  }

  vector<StyioAST*> getStmts() {
    return Stmts;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

class EOFAST : public StyioNode<EOFAST>
{
public:
  EOFAST() {}

  StyioNodeHint hint() override {
    return StyioNodeHint::End;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

class BreakAST : public StyioNode<BreakAST>
{
public:
  BreakAST() {}

  StyioNodeHint hint() override {
    return StyioNodeHint::Break;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

class PassAST : public StyioNode<PassAST>
{
public:
  PassAST() {}

  StyioNodeHint hint() override {
    return StyioNodeHint::Pass;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

class ReturnAST : public StyioNode<ReturnAST>
{
  StyioAST* Expr = nullptr;

public:
  ReturnAST(StyioAST* expr) :
      Expr(expr) {
  }

  static ReturnAST* Create(StyioAST* expr) {
    return new ReturnAST(expr);
  }

  StyioAST* getExpr() {
    return Expr;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Return;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  =================
    Variable
  =================
*/

/*
  VarAST
  |- Global Variable
  |- Local Variable
  |- Temporary Variable
*/
class VarAST : public StyioNode<VarAST>
{
private:
  string Name;                /* Variable Name */
  DTypeAST* DType = nullptr;  /* Data Type */
  StyioAST* DValue = nullptr; /* Default Value */

public:
  VarAST() :
      Name("") {
  }

  VarAST(const string& name) :
      Name(name), DType(DTypeAST::Create()) {
  }

  VarAST(const string& name, DTypeAST* data_type) :
      Name(name), DType(data_type) {
  }

  VarAST(const string& name, DTypeAST* data_type, StyioAST* default_value) :
      Name(name), DType(data_type), DValue(default_value) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Var;
  }

  const string& getName() {
    return Name;
  }

  bool isTyped() {
    return (
      DType
      && (DType->getDType() != StyioDataType::undefined)
    );
  }

  DTypeAST* getDType() {
    return DType;
  }

  void setDType(StyioDataType type) {
    return DType->setDType(type);
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  Function
  [+] Argument
*/
class ArgAST : public VarAST
{
private:
  string Name;                /* Variable Name */
  DTypeAST* DType = nullptr;  /* Data Type */
  StyioAST* DValue = nullptr; /* Default Value */

public:
  ArgAST(const string& name) :
      VarAST(name),
      Name(name) {
  }

  ArgAST(
    const string& name,
    DTypeAST* data_type
  ) :
      VarAST(name, data_type),
      Name(name),
      DType(data_type) {
  }

  ArgAST(const string& name, DTypeAST* data_type, StyioAST* default_value) :
      VarAST(name, data_type, default_value),
      Name(name),
      DType(data_type),
      DValue(default_value) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Arg;
  }

  static ArgAST* Create(const string& id) {
    return new ArgAST(id);
  }

  static ArgAST* Create(const string& id, DTypeAST* data_type) {
    return new ArgAST(id, data_type);
  }

  static ArgAST* Create(const string& id, DTypeAST* data_type, StyioAST* default_value) {
    return new ArgAST(
      id, data_type, default_value
    );
  }

  bool isTyped() {
    return (
      DType != nullptr
      && (DType->getDType() != StyioDataType::undefined)
    );
  }

  DTypeAST* getDType() {
    return DType;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

class OptArgAST : public VarAST
{
  IdAST* Id = nullptr;

public:
  OptArgAST(IdAST* id) :
      Id(id) {
  }

  static OptArgAST* Create(IdAST* id) {
    return new OptArgAST(id);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::OptArg;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

class OptKwArgAST : public VarAST
{
  IdAST* Id = nullptr;

public:
  OptKwArgAST(IdAST* id) :
      Id(id) {
  }

  static OptKwArgAST* Create(IdAST* id) {
    return new OptKwArgAST(id);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::OptKwArg;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

class VarTupleAST : public StyioNode<VarTupleAST>
{
private:
  vector<VarAST*> Vars;

public:
  VarTupleAST(vector<VarAST*> vars) :
      Vars(vars) {
  }

  static VarTupleAST* Create(vector<VarAST*> vars) {
    return new VarTupleAST(vars);
  }

  const vector<VarAST*>& getParams() {
    return Vars;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::VarTuple;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  FmtStrAST: String
*/
class FmtStrAST : public StyioNode<FmtStrAST>
{
  vector<string> Fragments;
  vector<StyioAST*> Exprs;

public:
  FmtStrAST(vector<string> fragments, vector<StyioAST*> expressions) :
      Fragments(fragments), Exprs((expressions)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::FmtStr;
  }

  static FmtStrAST* Create(vector<string> fragments, vector<StyioAST*> expressions) {
    return new FmtStrAST(fragments, expressions);
  };

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

class TypeConvertAST : public StyioNode<TypeConvertAST>
{
  StyioAST* Value = nullptr;
  NumPromoTy PromoType;

public:
  TypeConvertAST(
    StyioAST* val,
    NumPromoTy promo_type
  ) :
      Value((val)), PromoType(promo_type) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::NumConvert;
  }

  static TypeConvertAST* Create(
    StyioAST* value,
    NumPromoTy promo_type
  ) {
    return new TypeConvertAST(value, promo_type);
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  =================
    Data Resource Identifier
  =================
*/

/*
  Local [ File | Directory ] Path
*/
class LocalPathAST : public StyioNode<LocalPathAST>
{
  string Path;
  StyioPathType Type;

public:
  LocalPathAST(
    StyioPathType type,
    string path
  ) :
      Type(type),
      Path(path) {
  }

  static LocalPathAST* Create(
    StyioPathType type,
    string path
  ) {
    return new LocalPathAST(type, path);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::LocalPath;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  ipv4 / ipv6 / example.com
*/
class RemotePathAST : public StyioNode<RemotePathAST>
{
  string Path;
  StyioPathType Type;

public:
  RemotePathAST(
    StyioPathType type,
    string path
  ) :
      Type(type),
      Path(path) {
  }

  static RemotePathAST* Create(
    StyioPathType type,
    string path
  ) {
    return new RemotePathAST(type, path);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::RemotePath;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  Web URL
  - HTTP
  - HTTPS
  - FTP
*/
class WebUrlAST : public StyioNode<WebUrlAST>
{
  string Path;
  StyioPathType Type;

public:
  WebUrlAST(StyioPathType type, string path) :
      Type(type), Path(path) {
  }

  static WebUrlAST* Create(StyioPathType type, string path) {
    return new WebUrlAST(type, path);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::WebUrl;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/* Database Access URL */
class DBUrlAST : public StyioNode<DBUrlAST>
{
  string Path;
  StyioPathType Type;

public:
  DBUrlAST(StyioPathType type, string path) :
      Type(type), Path(path) {
  }

  static DBUrlAST* Create(StyioPathType type, string path) {
    return new DBUrlAST(type, path);
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::DBUrl;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  =================
    Collection
      Tuple
      List
      Set
  =================
*/

/*
  ListAST: List (Extendable)
*/
class ListAST : public StyioNode<ListAST>
{
  vector<StyioAST*> Elems;

public:
  ListAST(vector<StyioAST*> elems) :
      Elems((elems)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::List;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

class TupleAST : public StyioNode<TupleAST>
{
  vector<StyioAST*> Elems;

public:
  TupleAST(vector<StyioAST*> elems) :
      Elems((elems)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Tuple;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

class SetAST : public StyioNode<SetAST>
{
  vector<StyioAST*> Elems;

public:
  SetAST(vector<StyioAST*> elems) :
      Elems((elems)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Set;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  RangeAST: Loop
*/
class RangeAST : public StyioNode<RangeAST>
{
  StyioAST* StartVal = nullptr;
  StyioAST* EndVal = nullptr;
  StyioAST* StepVal = nullptr;

public:
  RangeAST(StyioAST* start, StyioAST* end, StyioAST* step) :
      StartVal((start)), EndVal((end)), StepVal((step)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Range;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  =================
    Basic Operation
  =================
*/

/*
  SizeOfAST: Get Size(Length) Of A Collection
*/
class SizeOfAST : public StyioNode<SizeOfAST>
{
  StyioAST* Value = nullptr;

public:
  SizeOfAST(
    StyioAST* value
  ) :
      Value(value) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::SizeOf;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  BinOpAST: Binary Operation

  | Variable
  | Scalar Value
    - Int
      {
        // General Operation
        Int (+, -, *, **, /, %) Int => Int

        // Bitwise Operation
        Int (&, |, >>, <<, ^) Int => Int
      }
    - Float
      {
        // General Operation
        Float (+, -, *, **, /, %) Int => Float
        Float (+, -, *, **, /, %) Float => Float
      }
    - Char
      {
        // Only Support Concatenation
        Char + Char => String
      }
  | Collection
    - Empty
      | Empty Tuple
      | Empty List (Extendable)
    - Tuple <Any>
      {
        // Only Support Concatenation
        Tuple<Any> + Tuple<Any> => Tuple
      }
    - Array <Scalar Value> // Immutable, Fixed Length
      {
        // (Only Same Length) Elementwise Operation
        Array<Any>[Length] (+, -, *, **, /, %) Array<Any>[Length] => Array<Any>

        // General Operation
        Array<Int> (+, -, *, **, /, %) Int => Array<Int>
        Array<Float> (+, -, *, **, /, %) Int => Array<Float>

        Array<Int> (+, -, *, **, /, %) Float => Array<Float>
        Array<Float> (+, -, *, **, /, %) Float => Array<Float>
      }
    - List
      {
        // Only Support Concatenation
        List<Any> + List<Any> => List<Any>
      }
    - String
      {
        // Only Support Concatenation
        String + String => String
      }
  | Blcok (With Return Value)
*/
class BinOpAST : public StyioNode<BinOpAST>
{
  StyioNodeHint Operand;
  StyioAST* LHS = nullptr;
  StyioAST* RHS = nullptr;

public:
  BinOpAST(StyioNodeHint op, StyioAST* lhs, StyioAST* rhs) :
      Operand(op), LHS(lhs), RHS(rhs) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::BinOp;
  }

  StyioNodeHint getOperand() {
    return Operand;
  }

  StyioAST* getLhs() {
    return LHS;
  }

  StyioAST* getRhs() {
    return RHS;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

class BinCompAST : public StyioNode<BinCompAST>
{
  CompType CompSign;
  StyioAST* LhsExpr = nullptr;
  StyioAST* RhsExpr = nullptr;

public:
  BinCompAST(CompType sign, StyioAST* lhs, StyioAST* rhs) :
      CompSign(sign), LhsExpr((lhs)), RhsExpr((rhs)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Compare;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

class CondAST : public StyioNode<CondAST>
{
  LogicType LogicOp;

  /*
    RAW: expr
    NOT: !(expr)
  */
  StyioAST* ValExpr = nullptr;

  /*
    AND: expr && expr
    OR : expr || expr
  */
  StyioAST* LhsExpr = nullptr;
  StyioAST* RhsExpr = nullptr;

public:
  CondAST(LogicType op, StyioAST* val) :
      LogicOp(op), ValExpr((val)) {
  }

  CondAST(LogicType op, StyioAST* lhs, StyioAST* rhs) :
      LogicOp(op), LhsExpr((lhs)), RhsExpr((rhs)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Condition;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

class CallAST : public StyioNode<CallAST>
{
private:
  IdAST* func_name = nullptr;
  vector<StyioAST*> func_args;

public:
  CallAST(
    IdAST* func_name,
    vector<StyioAST*> arguments
  ) :
      func_name(func_name),
      func_args(arguments) {
  }

  const string& getName() {
    return func_name->getAsStr();
  }

  const vector<StyioAST*>& getArgs() {
    return func_args;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Call;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

class ListOpAST : public StyioNode<ListOpAST>
{
  StyioNodeHint OpType;
  StyioAST* TheList = nullptr;

  StyioAST* Slot1 = nullptr;
  StyioAST* Slot2 = nullptr;

public:
  /*
    Get_Reversed
      [<]
  */
  ListOpAST(StyioNodeHint opType, StyioAST* theList) :
      OpType(opType), TheList((theList)) {
  }

  /*
    Access_By_Index
      [index]

    Access_By_Name
      ["name"]

    Append_Value
      [+: value]

    Remove_Item_By_Index
      [-: index]

    Get_Index_By_Value
      [?= value]

    Get_Indices_By_Many_Values
      [?^ values]

    Remove_Item_By_Value
      [-: ?= value]

    Remove_Items_By_Many_Indices
      [-: (i0, i1, ...)]

    Remove_Items_By_Many_Values
      [-: ?^ (v0, v1, ...)]

    Get_Index_By_Item_From_Right
      [[<] ?= value]

    Remove_Item_By_Value_From_Right
      [[<] -: ?= value]

    Remove_Items_By_Many_Values_From_Right
      [[<] -: ?^ (v0, v1, ...)]
  */
  ListOpAST(StyioNodeHint opType, StyioAST* theList, StyioAST* item) :
      OpType(opType), TheList((theList)), Slot1((item)) {
  }

  /*
    Insert_Item_By_Index
      [+: index <- value]
  */
  ListOpAST(StyioNodeHint opType, StyioAST* theList, StyioAST* index, StyioAST* value) :
      OpType(opType), TheList((theList)), Slot1((index)), Slot2((value)) {
  }

  StyioNodeHint hint() override {
    return OpType;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  =================
    Statement: Resources
  =================
*/

/*
  ResourceAST: Resources

  Definition =
    Declaration (Neccessary)
    + | Assignment (Optional)
      | Import (Optional)
*/
class ResourceAST : public StyioNode<ResourceAST>
{
  vector<StyioAST*> Resources;

public:
  ResourceAST(vector<StyioAST*> resources) :
      Resources((resources)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Resources;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  =================
    Statement: Variable Assignment (Variable-Value Binding)
  =================
*/

/*
  FlexBindAST: Mutable Assignment (Flexible Binding)
*/
class FlexBindAST : public StyioNode<FlexBindAST>
{
  IdAST* varName = nullptr;
  StyioAST* valExpr = nullptr;
  StyioDataType valType;

public:
  FlexBindAST(IdAST* var, StyioAST* val) :
      varName((var)), valExpr((val)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::MutBind;
  }

  const string& getName() {
    return varName->getAsStr();
  }

  StyioAST* getValue() {
    return valExpr;
  }

  StyioNodeHint getValueHint() {
    return valExpr->hint();
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  FinalBindAST: Immutable Assignment (Final Binding)
*/
class FinalBindAST : public StyioNode<FinalBindAST>
{
  IdAST* varName = nullptr;
  StyioAST* valExpr = nullptr;

public:
  FinalBindAST(IdAST* var, StyioAST* val) :
      varName((var)), valExpr((val)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::FixBind;
  }

  const string& getName() {
    return varName->getAsStr();
  }

  StyioAST* getValue() {
    return valExpr;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  =================
    Pipeline
      Function
      Structure (Struct)
      Evaluation
  =================
*/

/*
  StructAST: Structure
*/
class StructAST : public StyioNode<StructAST>
{
  IdAST* FName = nullptr;
  VarTupleAST* FVars = nullptr;
  StyioAST* FBlock = nullptr;

public:
  StructAST(IdAST* name, VarTupleAST* vars, StyioAST* block) :
      FName((name)), FVars(vars), FBlock((block)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Struct;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  =================
    OS Utility: IO Stream
  =================
*/

/*
  ReadFileAST: Read (File)
*/
class ReadFileAST : public StyioNode<ReadFileAST>
{
  IdAST* varId = nullptr;
  StyioAST* valExpr = nullptr;

public:
  ReadFileAST(IdAST* var, StyioAST* val) :
      varId((var)), valExpr((val)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::ReadFile;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  PrintAST: Write to Standard Output (Print)
*/
class PrintAST : public StyioNode<PrintAST>
{
  vector<StyioAST*> Exprs;

public:
  PrintAST(vector<StyioAST*> exprs) :
      Exprs((exprs)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Print;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  =================
    Abstract Level: Dependencies
  =================
*/

/*
  ExtPackAST: External Packages
*/
class ExtPackAST : public StyioNode<ExtPackAST>
{
  vector<string> PackPaths;

public:
  ExtPackAST(vector<string> paths) :
      PackPaths(paths) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::ExtPack;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  =================
    Abstract Level: Block
  =================
*/

class CondFlowAST : public StyioNode<CondFlowAST>
{
  CondAST* CondExpr = nullptr;
  StyioAST* ThenBlock = nullptr;
  StyioAST* ElseBlock = nullptr;

public:
  StyioNodeHint WhatFlow;

  CondFlowAST(StyioNodeHint whatFlow, CondAST* condition, StyioAST* block) :
      WhatFlow(whatFlow), CondExpr((condition)), ThenBlock((block)) {
  }

  CondFlowAST(StyioNodeHint whatFlow, CondAST* condition, StyioAST* blockThen, StyioAST* blockElse) :
      WhatFlow(whatFlow), CondExpr((condition)), ThenBlock((blockThen)), ElseBlock((blockElse)) {
  }

  StyioNodeHint hint() override {
    return WhatFlow;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  =================
    Abstract Level: Layers
  =================
*/

/*
  ICBSLayerAST: Intermediate Connection Between Scopes

  Run: Block
    => {

    }

  Fill: Fill + Block
    >> () => {}

  MatchValue: Fill + CheckEq + Block
    >> Element(Single) ?= ValueExpr(Single) => {
      ...
    }

    For each step of iteration, typeInfer if the element match the value
  expression, if match case is true, then execute the branch.

  MatchCases: Fill + Cases
    >> Element(Single) ?= {
      v0 => {}
      v1 => {}
      _  => {}
    }

    For each step of iteration, typeInfer if the element match any value
  expression, if match case is true, then execute the branch.

  ExtraIsin: Fill + CheckIsin
    >> Element(Single) ?^ IterableExpr(Collection) => {
      ...
    }

    For each step of iteration, typeInfer if the element is in the following
  collection, if match case is true, then execute the branch.

  ExtraCond: Fill + CondFlow
    >> Elements ? (Condition) \t\ {
      ...
    }

    For each step of iteration, typeInfer the given condition,
    if condition is true, then execute the branch.

    >> Elements ? (Condition) \f\ {
      ...
    }

    For each step of iteration, typeInfer the given condition,
    if condition is false, then execute the branch.

  Rules:
    1. If: a variable NOT not exists in its outer scope
       Then: create a variable and refresh it for each step

    2. If: a value expression can be evaluated with respect to its outer scope
       And: the value expression changes along with the iteration
       Then: refresh the value expression for each step

  How to parse ICBSLayer (for each element):
    1. Is this a [variable] or a [value expression]?

      Variable => 2
      Value Expression => 3

      (Hint: A value expression is something can be evaluated to a value
      after performing a series operations.)

    2. Is this variable previously defined or declared?

      Yes => 4
      No => 5

    3. Is this value expression using any variable that was NOT previously
  defined?

      Yes => 6
      No => 7

    4. Is this variable still mutable?

      Yes => 8
      No => 9

    5. Great! This element is a [temporary variable]
      that can ONLY be used in the following block.

      For each step of the iteration, you should:
        - Refresh this temporary variable before the start of the block.

    6. Error! Why is it using something that does NOT exists?
      This is an illegal value expression,
      you should throw an exception for this.

    7. Great! This element is a [changing value expression]
      that the value is changing while iteration.

      For each step of the iteration, you should:
        - Evaluate the value expression before the start of the block.

    8. Great! This element is a [changing variable]
      that the value of thisvariable is changing while iteration.

      (Note: The value of this variable should be changed by
        some statements inside the following code block.
        However, if you can NOT find such statements,
        do nothing.)

      For each step of the iteration, you should:
        - Refresh this variable before the start of the block.

    9. Error! Why are you trying to update a value that can NOT be changed?
      There must be something wrong,
      you should throw an exception for this.
*/

class CheckEqAST : public StyioNode<CheckEqAST>
{
  StyioAST* Value = nullptr;

public:
  CheckEqAST(StyioAST* value) :
      Value((value)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::CheckEq;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

class CheckIsinAST : public StyioNode<CheckIsinAST>
{
  StyioAST* Iterable = nullptr;

public:
  CheckIsinAST(
    StyioAST* value
  ) :
      Iterable((value)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::CheckIsin;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  FromToAST
*/
class FromToAST : public StyioNode<FromToAST>
{
  StyioAST* FromWhat = nullptr;
  StyioAST* ToWhat = nullptr;

public:
  FromToAST(StyioAST* from_expr, StyioAST* to_expr) :
      FromWhat((from_expr)), ToWhat((to_expr)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::FromTo;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  ExtraEq:
    ?= Expr => Block

  ExtraIsIn:
    ?^ Expr => Block

  ThenExpr:
    => Block

  ThenCondFlow:
    ?(Expr) \t\ Block \f\ Block
*/

class ForwardAST : public StyioNode<ForwardAST>
{
  VarTupleAST* params = nullptr;

  CheckEqAST* ExtraEq = nullptr;
  CheckIsinAST* ExtraIsin = nullptr;

  StyioAST* ThenExpr = nullptr;
  CondFlowAST* ThenCondFlow = nullptr;

  StyioAST* ret_expr = nullptr;

private:
  StyioNodeHint Type = StyioNodeHint::Forward;

public:
  ForwardAST(StyioAST* expr) :
      ThenExpr(expr) {
    Type = StyioNodeHint::Forward;
    if (ThenExpr->hint() != StyioNodeHint::Block) {
      ret_expr = expr;
    }
  }

  ForwardAST(CheckEqAST* value, StyioAST* whatnext) :
      ExtraEq(value), ThenExpr(whatnext) {
    Type = StyioNodeHint::If_Equal_To_Forward;
  }

  ForwardAST(CheckIsinAST* isin, StyioAST* whatnext) :
      ExtraIsin(isin), ThenExpr(whatnext) {
    Type = StyioNodeHint::If_Is_In_Forward;
  }

  ForwardAST(CasesAST* cases) :
      ThenExpr(cases) {
    Type = StyioNodeHint::Cases_Forward;
  }

  ForwardAST(CondFlowAST* condflow) :
      ThenCondFlow(condflow) {
    if ((condflow->WhatFlow) == StyioNodeHint::CondFlow_True) {
      Type = StyioNodeHint::If_True_Forward;
    }
    else if ((condflow->WhatFlow) == StyioNodeHint::CondFlow_False) {
      Type = StyioNodeHint::If_False_Forward;
    }
    else {
      Type = StyioNodeHint::If_Both_Forward;
    }
  }

  ForwardAST(
    VarTupleAST* vars,
    StyioAST* whatnext
  ) :
      params(vars),
      ThenExpr(whatnext) {
    Type = StyioNodeHint::Fill_Forward;
  }

  ForwardAST(
    VarTupleAST* vars,
    CheckEqAST* value,
    StyioAST* whatnext
  ) :
      params(vars), ExtraEq(value), ThenExpr(whatnext) {
    Type = StyioNodeHint::Fill_If_Equal_To_Forward;
  }

  ForwardAST(VarTupleAST* vars, CheckIsinAST* isin, StyioAST* whatnext) :
      params(vars), ExtraIsin(isin), ThenExpr(whatnext) {
    Type = StyioNodeHint::Fill_If_Is_in_Forward;
  }

  ForwardAST(VarTupleAST* vars, CasesAST* cases) :
      params(vars), ThenExpr(cases) {
    Type = StyioNodeHint::Fill_Cases_Forward;
  }

  ForwardAST(VarTupleAST* vars, CondFlowAST* condflow) :
      params(vars), ThenCondFlow(condflow) {
    switch (condflow->WhatFlow) {
      case StyioNodeHint::CondFlow_True:
        Type = StyioNodeHint::Fill_If_True_Forward;
        break;

      case StyioNodeHint::CondFlow_False:
        Type = StyioNodeHint::Fill_If_False_Forward;
        break;

      case StyioNodeHint::CondFlow_Both:
        Type = StyioNodeHint::Fill_If_Both_Forward;
        break;

      default:
        break;
    }
  }

  StyioAST* getRetExpr() {
    return ret_expr;
  }

  bool withParams() {
    return params && (!(params->getParams().empty()));
  }

  const vector<VarAST*>& getParams() {
    return params->getParams();
  }

  StyioAST* getThen() {
    return ThenExpr;
  }

  StyioNodeHint hint() override {
    return Type;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  =================
    Infinite loop
  =================
*/

/*
InfLoop: Infinite Loop
  incEl Increment Element
*/
class InfiniteAST : public StyioNode<InfiniteAST>
{
  InfiniteType WhatType;
  StyioAST* Start = nullptr;
  StyioAST* IncEl = nullptr;

public:
  InfiniteAST() {
    WhatType = InfiniteType::Original;
  }

  InfiniteAST(StyioAST* start, StyioAST* incEl) :
      Start((start)), IncEl((incEl)) {
    WhatType = InfiniteType::Incremental;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Infinite;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0,
    bool colorful = false
  ) override;
};

/*
  MatchCases
*/
class MatchCasesAST : public StyioNode<MatchCasesAST>
{
  StyioAST* Value = nullptr;
  CasesAST* Cases = nullptr;

public:
  /* v ?= { _ => ... } */
  MatchCasesAST(StyioAST* value, CasesAST* cases) :
      Value(value), Cases((cases)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::MatchCases;
  }

  static MatchCasesAST* make(StyioAST* value, CasesAST* cases) {
    return new MatchCasesAST(value, cases);
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  Anonymous Function
    [+] Arguments
    [?] ExtratypeInfer
    [+] ThenExpr
*/
class AnonyFuncAST : public StyioNode<AnonyFuncAST>
{
private:
  VarTupleAST* Args = nullptr;
  StyioAST* ThenExpr = nullptr;

public:
  /* #() => Then */
  AnonyFuncAST(VarTupleAST* vars, StyioAST* then) :
      Args(vars), ThenExpr((then)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::AnonyFunc;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  FuncAST: Function
*/
class FuncAST : public StyioNode<FuncAST>
{
  IdAST* Name = nullptr;
  DTypeAST* RetType = nullptr;
  ForwardAST* Forward = nullptr;

  bool isFinal;

public:
  FuncAST(
    ForwardAST* forward,
    bool isFinal
  ) :
      Forward((forward)),
      isFinal(isFinal) {
  }

  FuncAST(
    IdAST* name,
    ForwardAST* forward,
    bool isFinal
  ) :
      Name((name)),
      Forward((forward)),
      isFinal(isFinal) {
  }

  FuncAST(
    IdAST* name,
    DTypeAST* type,
    ForwardAST* forward,
    bool isFinal
  ) :
      Name(name),
      RetType(type),
      Forward(forward),
      isFinal(isFinal) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Func;
  }

  bool hasName() {
    if (Name) {
      return true;
    }
    else {
      return false;
    }
  }

  const IdAST* getId() {
    return Name;
  }

  string getFuncName() {
    return Name->getAsStr();
  }

  bool hasRetType() {
    if (RetType) {
      return true;
    }
    else {
      return false;
    }
  }

  DTypeAST* getRetType() {
    return RetType;
  }

  ForwardAST* getForward() {
    return Forward;
  }

  bool hasArgs() {
    return Forward->withParams();
  }

  bool allArgsTyped() {
    auto Args = Forward->getParams();

    return std::all_of(
      Args.begin(),
      Args.end(),
      [](VarAST* var)
      {
        return var->isTyped();
      }
    );
  }

  const vector<VarAST*>& getAllArgs() {
    return Forward->getParams();
  }

  unordered_map<string, VarAST*> getParamMap() {
    unordered_map<string, VarAST*> param_map;

    for (auto param : Forward->getParams()) {
      param_map[param->getName()] = param;
    }

    return param_map;
  }

  /* toString */
  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  =================
    Iterator
  =================
*/

/*
  IterInfinite: [...] >> {}
*/
class LoopAST : public StyioNode<LoopAST>
{
private:
  ForwardAST* Forward = nullptr;

public:
  LoopAST(ForwardAST* expr) :
      Forward(expr) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Loop;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

/*
  IterBounded: <List/Range> >> {}
*/
class IterAST : public StyioNode<IterAST>
{
  StyioAST* Collection = nullptr;
  ForwardAST* Forward = nullptr;

public:
  IterAST(StyioAST* collection, ForwardAST* forward) :
      Collection(collection), Forward(forward) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Iterator;
  }

  string toString(
    int indent = 0,
    bool colorful = false
  ) override;

  string toStringInline(
    int indent = 0, bool colorful = false
  ) override;
};

#endif