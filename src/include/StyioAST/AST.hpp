#pragma once
#ifndef STYIO_AST_H_
#define STYIO_AST_H_

// [Styio]
#include "../StyioToken/Token.hpp"

// [LLVM]
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

using std::string;
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
  /* Type Checking */
  virtual void check(T* t) = 0;

  /* Get LLVM Type */
  virtual llvm::Type* getLLVMType(T* t) = 0;

  /* LLVM IR Generator */
  virtual llvm::Value* toLLVMIR(T* t) = 0;
};

template <typename T, typename... Types>
class Visitor<T, Types...> : public Visitor<Types...>
{
public:
  using Visitor<Types...>::check;
  using Visitor<Types...>::getLLVMType;
  using Visitor<Types...>::toLLVMIR;

  /* Type Checking */
  virtual void check(T* t) = 0;

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
class EmptyBlockAST;

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
class CheckIsInAST;
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
  class EmptyBlockAST,

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
  class CheckIsInAST,
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
  unique_ptr<llvm::LLVMContext> llvm_context;
  unique_ptr<llvm::Module> llvm_module;
  unique_ptr<llvm::IRBuilder<>> llvm_ir_builder;

  std::unordered_map<string, llvm::AllocaInst*> mutable_variables; /* [FlexBind] */
  std::unordered_map<string, llvm::Value*> named_values;           /* [FinalBind] Immutable Variables */

public:
  StyioToLLVM() :
      llvm_context(make_unique<llvm::LLVMContext>()),
      llvm_module(make_unique<llvm::Module>("styio", *llvm_context)),
      llvm_ir_builder(make_unique<llvm::IRBuilder<>>(*llvm_context)) {
  }

  ~StyioToLLVM() {}

  /* Utility Functions */
  llvm::Type* matchType(string type);

  llvm::AllocaInst* createAllocaFuncEntry(llvm::Function* TheFunction, llvm::StringRef VarName);

  /* Styio AST Type Checker*/

  void check(BoolAST* ast);

  void check(NoneAST* ast);

  void check(EOFAST* ast);

  void check(EmptyAST* ast);

  void check(EmptyBlockAST* ast);

  void check(PassAST* ast);

  void check(BreakAST* ast);

  void check(ReturnAST* ast);

  void check(CommentAST* ast);

  void check(IdAST* ast);

  void check(VarAST* ast);

  void check(ArgAST* ast);

  void check(OptArgAST* ast);

  void check(OptKwArgAST* ast);

  void check(VarTupleAST* ast);

  void check(DTypeAST* ast);

  void check(IntAST* ast);

  void check(FloatAST* ast);

  void check(CharAST* ast);

  void check(StringAST* ast);

  void check(TypeConvertAST* ast);

  void check(FmtStrAST* ast);

  void check(LocalPathAST* ast);

  void check(RemotePathAST* ast);

  void check(WebUrlAST* ast);

  void check(DBUrlAST* ast);

  void check(ListAST* ast);

  void check(TupleAST* ast);

  void check(SetAST* ast);

  void check(RangeAST* ast);

  void check(SizeOfAST* ast);

  void check(BinOpAST* ast);

  void check(BinCompAST* ast);

  void check(CondAST* ast);

  void check(CallAST* ast);

  void check(ListOpAST* ast);

  void check(ResourceAST* ast);

  void check(FlexBindAST* ast);

  void check(FinalBindAST* ast);

  void check(StructAST* ast);

  void check(ReadFileAST* ast);

  void check(PrintAST* ast);

  void check(ExtPackAST* ast);

  void check(BlockAST* ast);

  void check(CasesAST* ast);

  void check(CondFlowAST* ast);

  void check(CheckEqAST* ast);

  void check(CheckIsInAST* ast);

  void check(FromToAST* ast);

  void check(ForwardAST* ast);

  void check(InfiniteAST* ast);

  void check(AnonyFuncAST* ast);

  void check(FuncAST* ast);

  void check(LoopAST* ast);

  void check(IterAST* ast);

  void check(MatchCasesAST* ast);

  void check(MainBlockAST* ast);

  /* Get LLVM Type */

  llvm::Type* getLLVMType(BoolAST* ast);

  llvm::Type* getLLVMType(NoneAST* ast);

  llvm::Type* getLLVMType(EOFAST* ast);

  llvm::Type* getLLVMType(EmptyAST* ast);

  llvm::Type* getLLVMType(EmptyBlockAST* ast);

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

  llvm::Type* getLLVMType(CheckIsInAST* ast);

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

  llvm::Value* toLLVMIR(EmptyBlockAST* ast);

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

  llvm::Value* toLLVMIR(CheckIsInAST* ast);

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

  void print_type_checking(shared_ptr<StyioAST> program);

  int run_llvm_ir(shared_ptr<MainBlockAST> program);
  void print_llvm_ir(int lli_result);
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

  /* Type Checking */
  virtual void check(StyioToLLVM* visitor) = 0;

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

  void check(StyioToLLVM* visitor) override {
    visitor->check(static_cast<Derived*>(this));
  }

  llvm::Type* getLLVMType(StyioToLLVM* visitor) override {
    return visitor->getLLVMType(static_cast<Derived*>(this));
  }

  llvm::Value* toLLVMIR(StyioToLLVM* visitor) override {
    visitor->check(static_cast<Derived*>(this));
    return visitor->toLLVMIR(static_cast<Derived*>(this));
  }
};

class CommentAST : public StyioNode<CommentAST>
{
private:
  string Text;

public:
  CommentAST(string text) :
      Text(text) {
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

/* NoneAST: None / Null / Nil */
class NoneAST : public StyioNode<NoneAST>
{
public:
  NoneAST() {}

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

/* EmptyAST: Empty Tuple / List / Set */
class EmptyAST : public StyioNode<EmptyAST>
{
public:
  EmptyAST() {}

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

/* EmptyBlockAST: Block */
class EmptyBlockAST : public StyioNode<EmptyBlockAST>
{
public:
  EmptyBlockAST() {}

  StyioNodeHint hint() override {
    return StyioNodeHint::EmptyBlock;
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

class BoolAST : public StyioNode<BoolAST>
{
  bool Value;

public:
  BoolAST(bool val) :
      Value(val) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Bool;
  }

  static unique_ptr<BoolAST> make(bool value) {
    return make_unique<BoolAST>(value);
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

class CasesAST : public StyioNode<CasesAST>
{
  vector<std::tuple<unique_ptr<StyioAST>, unique_ptr<StyioAST>>> Cases;
  unique_ptr<StyioAST> LastExpr;

public:
  CasesAST(unique_ptr<StyioAST> expr) :
      LastExpr(std::move(expr)) {
  }

  CasesAST(vector<std::tuple<unique_ptr<StyioAST>, unique_ptr<StyioAST>>> cases, unique_ptr<StyioAST> expr) :
      Cases(std::move(cases)), LastExpr(std::move(expr)) {
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
  unique_ptr<StyioAST> Resources;
  vector<unique_ptr<StyioAST>> Stmts;

public:
  BlockAST(unique_ptr<StyioAST> resources, vector<unique_ptr<StyioAST>> stmts) :
      Resources(std::move(resources)),
      Stmts(std::move(stmts)) {
  }

  BlockAST(vector<unique_ptr<StyioAST>> stmts) :
      Stmts(std::move(stmts)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Block;
  }

  const vector<unique_ptr<StyioAST>>& getStmts() const {
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
  unique_ptr<StyioAST> Resources;
  vector<unique_ptr<StyioAST>> Stmts;

public:
  MainBlockAST(
    unique_ptr<StyioAST> resources,
    vector<unique_ptr<StyioAST>> stmts
  ) :
      Resources(std::move(resources)),
      Stmts(std::move(stmts)) {
  }

  MainBlockAST(
    vector<unique_ptr<StyioAST>> stmts
  ) :
      Stmts(std::move(stmts)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::MainBlock;
  }

  const vector<unique_ptr<StyioAST>>& getStmts() const {
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
  unique_ptr<StyioAST> Expr;

public:
  ReturnAST(unique_ptr<StyioAST> expr) :
      Expr(std::move(expr)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Return;
  }

  const unique_ptr<StyioAST>& getExpr() {
    return Expr;
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

class IdAST : public StyioNode<IdAST>
{
  string Id = "";

public:
  IdAST(const string& id) :
      Id(id) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Id;
  }

  const string& getAsStr() const {
    return Id;
  }

  /* make shared pointer */
  static unique_ptr<IdAST> make(const string& id) {
    return make_unique<IdAST>(id);
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
  string TypeName;
  StyioDataType DType = StyioDataType::undefined;

public:
  DTypeAST(StyioDataType dtype): DType(dtype) {

  }

  DTypeAST(const string& dtype): TypeName(dtype) {
    auto it = DType_Table.find(dtype);
    if (it != DType_Table.end()) {
      DType = it->second;
    }
    else {
      DType = StyioDataType::undefined;
    }
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::DType;
  }

  const StyioDataType getDtype() const {
    return DType;
  }

  const string& getTypeName() const {
    return TypeName;
  }

  static unique_ptr<DTypeAST> make(string dtype) {
    return make_unique<DTypeAST>(dtype);
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
  VarAST
  |- Global Variable
  |- Local Variable
  |- Temporary Variable
*/
class VarAST : public StyioNode<VarAST>
{
private:
  string Name = "";            /* Variable Name */
  shared_ptr<DTypeAST> DType;  /* Data Type */
  unique_ptr<StyioAST> DValue; /* Default Value */

public:
  VarAST() :
      Name("") {
  }

  VarAST(const string& name) :
      Name(name) {
  }

  VarAST(const string& name, shared_ptr<DTypeAST> data_type) :
      Name(name), DType(std::move(data_type)) {
  }

  VarAST(const string& name, shared_ptr<DTypeAST> data_type, unique_ptr<StyioAST> default_value) :
      Name(name), DType(std::move(data_type)), DValue(std::move(default_value)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Var;
  }

  const string& getName() const {
    return Name;
  }

  bool hasType() {
    if (DType) {
      return true;
    }
    else {
      return false;
    }
  }

  const shared_ptr<DTypeAST>& getDType() const {
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

/*
  Function
  [+] Argument
*/
class ArgAST : public VarAST
{
private:
  string Name = "";            /* Variable Name */
  unique_ptr<DTypeAST> DType;  /* Data Type */
  unique_ptr<StyioAST> DValue; /* Default Value */

public:
  ArgAST(const string& name) :
      VarAST(name), Name(name) {
  }

  ArgAST(const string& name, unique_ptr<DTypeAST> data_type) :
      VarAST(name, DTypeAST::make(data_type->getTypeName())), Name(name), DType(std::move(data_type)) {
  }

  ArgAST(const string& name, unique_ptr<DTypeAST> data_type, unique_ptr<StyioAST> default_value) :
      VarAST(name, DTypeAST::make(data_type->getTypeName()), /* Copy -> VarAST */
             std::move(default_value)),
      Name(name),
      DType(std::move(data_type)),
      DValue(std::move(default_value)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Arg;
  }

  static shared_ptr<ArgAST> make(const string& id) {
    return std::make_shared<ArgAST>(id);
  }

  static shared_ptr<ArgAST> make(const string& id, unique_ptr<DTypeAST> data_type) {
    return std::make_shared<ArgAST>(id, std::move(data_type));
  }

  static shared_ptr<ArgAST> make(const string& id, unique_ptr<DTypeAST> data_type, unique_ptr<StyioAST> default_value) {
    return std::make_shared<ArgAST>(
      id, std::move(data_type), std::move(default_value)
    );
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
  unique_ptr<IdAST> Id;

public:
  OptArgAST(unique_ptr<IdAST> id) :
      Id(std::move(id)) {
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
  unique_ptr<IdAST> Id;

public:
  OptKwArgAST(unique_ptr<IdAST> id) :
      Id(std::move(id)) {
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
  vector<shared_ptr<VarAST>> Vars;

public:
  VarTupleAST(vector<shared_ptr<VarAST>> vars) :
      Vars(std::move(vars)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::VarTuple;
  }

  const vector<shared_ptr<VarAST>>& getArgs() {
    return Vars;
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
    Scalar Value
  =================
*/

/*
  IntAST: Integer
*/
class IntAST : public StyioNode<IntAST>
{
private:
  string Value;
  StyioDataType DType = StyioDataType::undefined;

public:
  IntAST(string val) :
      Value(val) {
  }

  IntAST(string val, StyioDataType dtype) :
      Value(val), DType(dtype) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Int;
  }

  const string& getValue() const {
    return Value;
  }

  StyioDataType getType() const {
    return DType;
  }

  void setType(StyioDataType type) {
    this->DType = type;
  }

  static unique_ptr<IntAST> make(string value) {
    return make_unique<IntAST>(value);
  }

  static unique_ptr<IntAST> make(string value, StyioDataType dtype) {
    return make_unique<IntAST>(value, dtype);
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
  StyioDataType DType = StyioDataType::f64;

  string Value;

  // string Significand = "";
  // int Base = 10;
  // int Exponent = 0;

public:
  FloatAST(string value) :
      Value(value) {
  }

  // FloatAST(
  //   string significand,
  //   int exponent):
  //   Significand(significand),
  //   Exponent(exponent) {}

  // FloatAST(
  //   string significand,
  //   int exponent,
  //   StyioDataType data_type):
  //   Significand(significand),
  //   Exponent(exponent),
  //   DType(data_type) {}

  StyioNodeHint hint() override {
    return StyioNodeHint::Float;
  }

  StyioDataType getType() {
    return DType;
  }

  const string& getValue() const {
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
  CharAST: Character
*/
class CharAST : public StyioNode<CharAST>
{
  string Value;

public:
  CharAST(
    string val
  ) :
      Value(val) {
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
  string Value;

public:
  StringAST(string val) :
      Value(val) {
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

/*
  FmtStrAST: String
*/
class FmtStrAST : public StyioNode<FmtStrAST>
{
  vector<string> Fragments;
  vector<unique_ptr<StyioAST>> Exprs;

public:
  FmtStrAST(vector<string> fragments, vector<unique_ptr<StyioAST>> expressions) :
      Fragments(fragments), Exprs(std::move(expressions)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::FmtStr;
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

class TypeConvertAST : public StyioNode<TypeConvertAST>
{
  shared_ptr<StyioAST> Value;
  NumPromoTy PromoType;

public:
  TypeConvertAST(
    shared_ptr<StyioAST> val,
    NumPromoTy promo_type
  ) :
      Value(std::move(val)), PromoType(promo_type) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::NumConvert;
  }

  static shared_ptr<TypeConvertAST> make(
    shared_ptr<StyioAST> value,
    NumPromoTy promo_type
  ) {
    return make_unique<TypeConvertAST>(std::move(value), promo_type);
  }

  static unique_ptr<IntAST> make(
    string value,
    StyioDataType dtype
  ) {
    return make_unique<IntAST>(value, dtype);
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
      Path(std::move(path)) {
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
      Path(std::move(path)) {
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
      Type(type), Path(std::move(path)) {
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
      Type(type), Path(std::move(path)) {
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
  vector<unique_ptr<StyioAST>> Elems;

public:
  ListAST(vector<unique_ptr<StyioAST>> elems) :
      Elems(std::move(elems)) {
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
  vector<unique_ptr<StyioAST>> Elems;

public:
  TupleAST(vector<unique_ptr<StyioAST>> elems) :
      Elems(std::move(elems)) {
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
  vector<unique_ptr<StyioAST>> Elems;

public:
  SetAST(vector<unique_ptr<StyioAST>> elems) :
      Elems(std::move(elems)) {
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
  unique_ptr<StyioAST> StartVal;
  unique_ptr<StyioAST> EndVal;
  unique_ptr<StyioAST> StepVal;

public:
  RangeAST(unique_ptr<StyioAST> start, unique_ptr<StyioAST> end, unique_ptr<StyioAST> step) :
      StartVal(std::move(start)), EndVal(std::move(end)), StepVal(std::move(step)) {
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
  unique_ptr<StyioAST> Value;

public:
  SizeOfAST(
    unique_ptr<StyioAST> value
  ) :
      Value(std::move(value)) {
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
  shared_ptr<StyioAST> LHS;
  shared_ptr<StyioAST> RHS;

public:
  BinOpAST(StyioNodeHint op, shared_ptr<StyioAST> lhs, shared_ptr<StyioAST> rhs) :
      Operand(op), LHS(lhs), RHS(rhs) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::BinOp;
  }

  const StyioNodeHint getOperand() const {
    return Operand;
  }

  const shared_ptr<StyioAST>& getLhs() const {
    return LHS;
  }

  const shared_ptr<StyioAST>& getRhs() const {
    return RHS;
  }

  // void setLhs(shared_ptr<StyioAST> value) {
  //   LHS.reset();
  //   LHS = std::move(value);
  // }

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
  unique_ptr<StyioAST> LhsExpr;
  unique_ptr<StyioAST> RhsExpr;

public:
  BinCompAST(CompType sign, unique_ptr<StyioAST> lhs, unique_ptr<StyioAST> rhs) :
      CompSign(sign), LhsExpr(std::move(lhs)), RhsExpr(std::move(rhs)) {
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
  unique_ptr<StyioAST> ValExpr;

  /*
    AND: expr && expr
    OR : expr || expr
  */
  unique_ptr<StyioAST> LhsExpr;
  unique_ptr<StyioAST> RhsExpr;

public:
  CondAST(LogicType op, unique_ptr<StyioAST> val) :
      LogicOp(op), ValExpr(std::move(val)) {
  }

  CondAST(LogicType op, unique_ptr<StyioAST> lhs, unique_ptr<StyioAST> rhs) :
      LogicOp(op), LhsExpr(std::move(lhs)), RhsExpr(std::move(rhs)) {
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
  unique_ptr<StyioAST> Func;

public:
  CallAST(unique_ptr<StyioAST> func) :
      Func(std::move(func)) {
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
  unique_ptr<StyioAST> TheList;

  unique_ptr<StyioAST> Slot1;
  unique_ptr<StyioAST> Slot2;

public:
  /*
    Get_Reversed
      [<]
  */
  ListOpAST(StyioNodeHint opType, unique_ptr<StyioAST> theList) :
      OpType(opType), TheList(std::move(theList)) {
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
  ListOpAST(StyioNodeHint opType, unique_ptr<StyioAST> theList, unique_ptr<StyioAST> item) :
      OpType(opType), TheList(std::move(theList)), Slot1(std::move(item)) {
  }

  /*
    Insert_Item_By_Index
      [+: index <- value]
  */
  ListOpAST(StyioNodeHint opType, unique_ptr<StyioAST> theList, unique_ptr<StyioAST> index, unique_ptr<StyioAST> value) :
      OpType(opType), TheList(std::move(theList)), Slot1(std::move(index)), Slot2(std::move(value)) {
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
  vector<unique_ptr<StyioAST>> Resources;

public:
  ResourceAST(vector<unique_ptr<StyioAST>> resources) :
      Resources(std::move(resources)) {
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
  unique_ptr<IdAST> varName;
  unique_ptr<StyioAST> valExpr;
  StyioDataType valType;

public:
  FlexBindAST(unique_ptr<IdAST> var, unique_ptr<StyioAST> val) :
      varName(std::move(var)), valExpr(std::move(val)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::MutBind;
  }

  const string& getName() const {
    return varName->getAsStr();
  }

  const unique_ptr<StyioAST>& getValue() const {
    return valExpr;
  }

  const StyioNodeHint getValueHint() const {
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
  unique_ptr<IdAST> varName;
  unique_ptr<StyioAST> valExpr;

public:
  FinalBindAST(unique_ptr<IdAST> var, unique_ptr<StyioAST> val) :
      varName(std::move(var)), valExpr(std::move(val)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::FixBind;
  }

  const string& getName() const {
    return varName->getAsStr();
  }

  const unique_ptr<StyioAST>& getValue() const {
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
  unique_ptr<IdAST> FName;
  shared_ptr<VarTupleAST> FVars;
  unique_ptr<StyioAST> FBlock;

public:
  StructAST(unique_ptr<IdAST> name, shared_ptr<VarTupleAST> vars, unique_ptr<StyioAST> block) :
      FName(std::move(name)), FVars(vars), FBlock(std::move(block)) {
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
  unique_ptr<IdAST> varId;
  unique_ptr<StyioAST> valExpr;

public:
  ReadFileAST(unique_ptr<IdAST> var, unique_ptr<StyioAST> val) :
      varId(std::move(var)), valExpr(std::move(val)) {
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
  vector<unique_ptr<StyioAST>> Exprs;

public:
  PrintAST(vector<unique_ptr<StyioAST>> exprs) :
      Exprs(std::move(exprs)) {
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
  unique_ptr<CondAST> CondExpr;
  unique_ptr<StyioAST> ThenBlock;
  unique_ptr<StyioAST> ElseBlock;

public:
  StyioNodeHint WhatFlow;

  CondFlowAST(StyioNodeHint whatFlow, unique_ptr<CondAST> condition, unique_ptr<StyioAST> block) :
      WhatFlow(whatFlow), CondExpr(std::move(condition)), ThenBlock(std::move(block)) {
  }

  CondFlowAST(StyioNodeHint whatFlow, unique_ptr<CondAST> condition, unique_ptr<StyioAST> blockThen, unique_ptr<StyioAST> blockElse) :
      WhatFlow(whatFlow), CondExpr(std::move(condition)), ThenBlock(std::move(blockThen)), ElseBlock(std::move(blockElse)) {
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

    For each step of iteration, check if the element match the value
  expression, if match case is true, then execute the branch.

  MatchCases: Fill + Cases
    >> Element(Single) ?= {
      v0 => {}
      v1 => {}
      _  => {}
    }

    For each step of iteration, check if the element match any value
  expression, if match case is true, then execute the branch.

  ExtraIsin: Fill + CheckIsIn
    >> Element(Single) ?^ IterableExpr(Collection) => {
      ...
    }

    For each step of iteration, check if the element is in the following
  collection, if match case is true, then execute the branch.

  ExtraCond: Fill + CondFlow
    >> Elements ? (Condition) \t\ {
      ...
    }

    For each step of iteration, check the given condition,
    if condition is true, then execute the branch.

    >> Elements ? (Condition) \f\ {
      ...
    }

    For each step of iteration, check the given condition,
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
  unique_ptr<StyioAST> Value;

public:
  CheckEqAST(unique_ptr<StyioAST> value) :
      Value(std::move(value)) {
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

class CheckIsInAST : public StyioNode<CheckIsInAST>
{
  unique_ptr<StyioAST> Iterable;

public:
  CheckIsInAST(
    unique_ptr<StyioAST> value
  ) :
      Iterable(std::move(value)) {
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
  unique_ptr<StyioAST> FromWhat;
  unique_ptr<StyioAST> ToWhat;

public:
  FromToAST(unique_ptr<StyioAST> from_expr, unique_ptr<StyioAST> to_expr) :
      FromWhat(std::move(from_expr)), ToWhat(std::move(to_expr)) {
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
  shared_ptr<VarTupleAST> Args;

  unique_ptr<CheckEqAST> ExtraEq;
  unique_ptr<CheckIsInAST> ExtraIsin;

  unique_ptr<StyioAST> ThenExpr;
  unique_ptr<CondFlowAST> ThenCondFlow;

private:
  StyioNodeHint Type = StyioNodeHint::Forward;

public:
  ForwardAST(unique_ptr<StyioAST> expr) :
      ThenExpr(std::move(expr)) {
    Type = StyioNodeHint::Forward;
  }

  ForwardAST(unique_ptr<CheckEqAST> value, unique_ptr<StyioAST> whatnext) :
      ExtraEq(std::move(value)), ThenExpr(std::move(whatnext)) {
    Type = StyioNodeHint::If_Equal_To_Forward;
  }

  ForwardAST(unique_ptr<CheckIsInAST> isin, unique_ptr<StyioAST> whatnext) :
      ExtraIsin(std::move(isin)), ThenExpr(std::move(whatnext)) {
    Type = StyioNodeHint::If_Is_In_Forward;
  }

  ForwardAST(unique_ptr<CasesAST> cases) :
      ThenExpr(std::move(cases)) {
    Type = StyioNodeHint::Cases_Forward;
  }

  ForwardAST(unique_ptr<CondFlowAST> condflow) :
      ThenCondFlow(std::move(condflow)) {
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
    shared_ptr<VarTupleAST> vars,
    unique_ptr<StyioAST> whatnext
  ) :
      Args(std::move(vars)),
      ThenExpr(std::move(whatnext)) {
    Type = StyioNodeHint::Fill_Forward;
  }

  ForwardAST(
    shared_ptr<VarTupleAST> vars,
    unique_ptr<CheckEqAST> value,
    unique_ptr<StyioAST> whatnext
  ) :
      Args(std::move(vars)), ExtraEq(std::move(value)), ThenExpr(std::move(whatnext)) {
    Type = StyioNodeHint::Fill_If_Equal_To_Forward;
  }

  ForwardAST(shared_ptr<VarTupleAST> vars, unique_ptr<CheckIsInAST> isin, unique_ptr<StyioAST> whatnext) :
      Args(std::move(vars)), ExtraIsin(std::move(isin)), ThenExpr(std::move(whatnext)) {
    Type = StyioNodeHint::Fill_If_Is_in_Forward;
  }

  ForwardAST(shared_ptr<VarTupleAST> vars, unique_ptr<CasesAST> cases) :
      Args(std::move(vars)), ThenExpr(std::move(cases)) {
    Type = StyioNodeHint::Fill_Cases_Forward;
  }

  ForwardAST(shared_ptr<VarTupleAST> vars, unique_ptr<CondFlowAST> condflow) :
      Args(std::move(vars)), ThenCondFlow(std::move(condflow)) {
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

  StyioNodeHint hint() override {
    return Type;
  }

  bool hasArgs() {
    return Args && (!(Args->getArgs().empty()));
  }

  const vector<shared_ptr<VarAST>>& getArgs() {
    return Args->getArgs();
  }

  const unique_ptr<StyioAST>& getThen() {
    return ThenExpr;
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
  unique_ptr<StyioAST> Start;
  unique_ptr<StyioAST> IncEl;

public:
  InfiniteAST() {
    WhatType = InfiniteType::Original;
  }

  InfiniteAST(unique_ptr<StyioAST> start, unique_ptr<StyioAST> incEl) :
      Start(std::move(start)), IncEl(std::move(incEl)) {
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
  shared_ptr<StyioAST> Value;
  unique_ptr<CasesAST> Cases;

public:
  /* v ?= { _ => ... } */
  MatchCasesAST(shared_ptr<StyioAST> value, unique_ptr<CasesAST> cases) :
      Value(value), Cases(std::move(cases)) {
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::MatchCases;
  }

  static shared_ptr<MatchCasesAST> make(shared_ptr<StyioAST> value, unique_ptr<CasesAST> cases) {
    return std::make_shared<MatchCasesAST>(value, std::move(cases));
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
    [?] ExtraCheck
    [+] ThenExpr
*/
class AnonyFuncAST : public StyioNode<AnonyFuncAST>
{
private:
  shared_ptr<VarTupleAST> Args;
  unique_ptr<StyioAST> ThenExpr;

public:
  /* #() => Then */
  AnonyFuncAST(shared_ptr<VarTupleAST> vars, unique_ptr<StyioAST> then) :
      Args(vars), ThenExpr(std::move(then)) {
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
  unique_ptr<IdAST> Name;
  unique_ptr<DTypeAST> RetType;
  unique_ptr<ForwardAST> Forward;

  bool isFinal;

public:
  FuncAST(
    unique_ptr<ForwardAST> forward,
    bool isFinal
  ) :
      Forward(std::move(forward)),
      isFinal(isFinal) {
  }

  FuncAST(
    unique_ptr<IdAST> name,
    unique_ptr<ForwardAST> forward,
    bool isFinal
  ) :
      Name(std::move(name)),
      Forward(std::move(forward)),
      isFinal(isFinal) {
  }

  FuncAST(
    unique_ptr<IdAST> name,
    unique_ptr<DTypeAST> type,
    unique_ptr<ForwardAST> forward,
    bool isFinal
  ) :
      Name(std::move(name)),
      RetType(std::move(type)),
      Forward(std::move(forward)),
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

  const unique_ptr<IdAST>& getId() {
    return Name;
  }

  string getName() {
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

  const unique_ptr<DTypeAST>& getRetType() {
    return RetType;
  }

  const unique_ptr<ForwardAST>& getForward() const {
    return Forward;
  }

  bool hasArgs() {
    return Forward->hasArgs();
  }

  const vector<shared_ptr<VarAST>>& getArgList() {
    return Forward->getArgs();
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
  unique_ptr<ForwardAST> Forward;

public:
  LoopAST(unique_ptr<ForwardAST> expr) :
      Forward(std::move(expr)) {
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
  unique_ptr<StyioAST> Collection;
  unique_ptr<ForwardAST> Forward;

public:
  IterAST(unique_ptr<StyioAST> collection, unique_ptr<ForwardAST> forward) :
      Collection(std::move(collection)), Forward(std::move(forward)) {
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