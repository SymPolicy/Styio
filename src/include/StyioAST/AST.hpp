#pragma once
#ifndef STYIO_AST_H_
#define STYIO_AST_H_

// // [Styio]
#include "../StyioToken/Token.hpp"

// [LLVM]
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"

// Generic Visitor
template <typename ... Types>
class Visitor;

template <typename T>
class Visitor<T> {
  public:
    /* Check */
    virtual void check(T* t) = 0;

    /* LLVM IR Generator */
    virtual llvm::Value* toLLVM(T* t) = 0;
};

template <typename T, typename ... Types>
class Visitor<T, Types ...> : public Visitor<Types ...> {
  public:
    using Visitor<Types ...>::check;
    using Visitor<Types ...>::toLLVM;

    /* Type Checker & Modifier */
    virtual void check(T* t) = 0;

    /* LLVM IR Generator */
    virtual llvm::Value* toLLVM(T* t) = 0;
};

/* Forward Declaration */

/* StyioAST */
class StyioAST;

/* Styio AST */
class CommentAST;

class NoneAST;
class EmptyAST;
class EmptyBlockAST;

class IdAST;
class DTypeAST;

class BoolAST;
class IntAST;
class FloatAST;
class CharAST;
class StringAST;

class NumPromoAST;

class VarAST;
class ArgAST;
class OptArgAST;
class OptKwArgAST;

class FlexBindAST;
class FinalBindAST;

class InfiniteAST;

class StructAST;
class TupleAST;
class VarTupleAST;
class RangeAST;

class SetAST;
class ListAST;

class SizeOfAST;
class ListOpAST;

class BinCompAST;
class CondAST;
class BinOpAST;

class FmtStrAST;
class ResourceAST;
class ExtPathAST;
class ExtLinkAST;
class ExtPackAST;

class ReadFileAST;

class EOFAST;
class BreakAST;
class PassAST;
class ReturnAST;

class CallAST;
class PrintAST;

class ForwardAST;
class CheckEqAST;
class CheckIsInAST;
class FromToAST;

class AnonyFuncAST;
class FuncAST;

class IterAST;
class LoopAST;

class CondFlowAST;

class CasesAST;
class MatchCasesAST;

class SideBlockAST;
class MainBlockAST;

using StyioVisitor = Visitor<
  class CommentAST,

  class NoneAST,
  class EmptyAST,
  class EmptyBlockAST,

  class IdAST,
  class DTypeAST,

  class BoolAST,
  class IntAST,
  class FloatAST,
  class CharAST,
  class StringAST,

  class NumPromoAST,

  class VarAST,
  class ArgAST,
  class OptArgAST,
  class OptKwArgAST,

  class FlexBindAST,
  class FinalBindAST,

  class InfiniteAST,

  class StructAST,
  class TupleAST,
  class VarTupleAST,
  class RangeAST,

  class SetAST,
  class ListAST,

  class SizeOfAST,
  class ListOpAST,

  class BinCompAST,
  class CondAST,
  class BinOpAST,

  class FmtStrAST,
  class ResourceAST,
  class ExtPathAST,
  class ExtLinkAST,
  class ExtPackAST,

  class ReadFileAST,

  class EOFAST,
  class BreakAST,
  class PassAST,
  class ReturnAST,

  class CallAST,
  class PrintAST,

  class ForwardAST,
  class CheckEqAST,
  class CheckIsInAST,
  class FromToAST,

  class AnonyFuncAST,
  class FuncAST,

  class IterAST,
  class LoopAST,

  class CondFlowAST,

  class CasesAST,
  class MatchCasesAST,

  class SideBlockAST,
  class MainBlockAST
>;

/* Styio -> LLVM Generator */
class StyioToLLVM;

class StyioToLLVM : public StyioVisitor {
  std::unique_ptr<llvm::LLVMContext> llvm_context;
  std::unique_ptr<llvm::Module> llvm_module;
  std::unique_ptr<llvm::IRBuilder<>> llvm_builder;

  public:
    StyioToLLVM () {
      llvm_context = std::make_unique<llvm::LLVMContext>();
      llvm_module = std::make_unique<llvm::Module>("styio", *llvm_context);
      llvm_builder = std::make_unique<llvm::IRBuilder<>>(*llvm_context);
    }

    ~StyioToLLVM () {}

    /* Styio AST Type Checker & Modifier */

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

    void check(NumPromoAST* ast);

    void check(FmtStrAST* ast);

    void check(ExtPathAST* ast);

    void check(ExtLinkAST* ast);

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

    void check(SideBlockAST* ast);

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

    /* Styio to LLVM IR Generator */

    void show();

    llvm::Type* match_type(std::string type);

    llvm::Value* toLLVM(BoolAST* ast);

    llvm::Value* toLLVM(NoneAST* ast);

    llvm::Value* toLLVM(EOFAST* ast);

    llvm::Value* toLLVM(EmptyAST* ast);

    llvm::Value* toLLVM(EmptyBlockAST* ast);

    llvm::Value* toLLVM(PassAST* ast);

    llvm::Value* toLLVM(BreakAST* ast);

    llvm::Value* toLLVM(ReturnAST* ast);

    llvm::Value* toLLVM(CommentAST* ast);

    llvm::Value* toLLVM(IdAST* ast);

    llvm::Value* toLLVM(VarAST* ast);

    llvm::Value* toLLVM(ArgAST* ast);

    llvm::Value* toLLVM(OptArgAST* ast);

    llvm::Value* toLLVM(OptKwArgAST* ast);

    llvm::Value* toLLVM(VarTupleAST* ast);

    llvm::Value* toLLVM(DTypeAST* ast);

    llvm::Value* toLLVM(IntAST* ast);

    llvm::Value* toLLVM(FloatAST* ast);

    llvm::Value* toLLVM(CharAST* ast);

    llvm::Value* toLLVM(StringAST* ast);

    llvm::Value* toLLVM(NumPromoAST* ast);

    llvm::Value* toLLVM(FmtStrAST* ast);

    llvm::Value* toLLVM(ExtPathAST* ast);

    llvm::Value* toLLVM(ExtLinkAST* ast);

    llvm::Value* toLLVM(ListAST* ast);

    llvm::Value* toLLVM(TupleAST* ast);

    llvm::Value* toLLVM(SetAST* ast);

    llvm::Value* toLLVM(RangeAST* ast);

    llvm::Value* toLLVM(SizeOfAST* ast);

    llvm::Value* toLLVM(BinOpAST* ast);

    llvm::Value* toLLVM(BinCompAST* ast);

    llvm::Value* toLLVM(CondAST* ast);

    llvm::Value* toLLVM(CallAST* ast);

    llvm::Value* toLLVM(ListOpAST* ast);

    llvm::Value* toLLVM(ResourceAST* ast);

    llvm::Value* toLLVM(FlexBindAST* ast);

    llvm::Value* toLLVM(FinalBindAST* ast);

    llvm::Value* toLLVM(StructAST* ast);

    llvm::Value* toLLVM(ReadFileAST* ast);

    llvm::Value* toLLVM(PrintAST* ast);

    llvm::Value* toLLVM(ExtPackAST* ast);

    llvm::Value* toLLVM(SideBlockAST* ast);

    llvm::Value* toLLVM(CasesAST* ast);

    llvm::Value* toLLVM(CondFlowAST* ast);

    llvm::Value* toLLVM(CheckEqAST* ast);

    llvm::Value* toLLVM(CheckIsInAST* ast);

    llvm::Value* toLLVM(FromToAST* ast);

    llvm::Value* toLLVM(ForwardAST* ast);

    llvm::Value* toLLVM(InfiniteAST* ast);

    llvm::Value* toLLVM(AnonyFuncAST* ast);

    llvm::Value* toLLVM(FuncAST* ast);

    llvm::Value* toLLVM(LoopAST* ast);

    llvm::Value* toLLVM(IterAST* ast);

    llvm::Value* toLLVM(MatchCasesAST* ast);

    llvm::Value* toLLVM(MainBlockAST* ast);
};

/*
  StyioAST: Styio Base AST
*/
class StyioAST {
  public:
    virtual ~StyioAST() {}

    /* type hint */
    virtual StyioNodeHint hint() = 0;

    /* toString */
    virtual std::string toString(int indent = 0, bool colorful = false) = 0;
    virtual std::string toStringInline(int indent = 0, bool colorful = false) = 0;

    /* Type Checker & Modifier */
    virtual void check(StyioToLLVM* visitor) = 0;

    /* LLVM IR Generator */
    virtual llvm::Value* toLLVM(StyioToLLVM* generator) = 0;
};

template <class Derived>
class StyioNode : public StyioAST {
  public:
    using StyioAST::hint;
    using StyioAST::toString;
    using StyioAST::toStringInline;

    void check(StyioToLLVM* visitor) override {
      visitor -> check(static_cast<Derived*>(this)); }

    llvm::Value* toLLVM(StyioToLLVM* visitor) override {
      visitor -> check(static_cast<Derived*>(this));
      return visitor -> toLLVM(static_cast<Derived*>(this)); }
};

class CommentAST : public StyioNode<CommentAST> {
  private:
    std::string Text;

  public:
    CommentAST (std::string text): Text(text) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Comment; }

    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

/* NoneAST: None / Null / Nil */
class NoneAST : public StyioNode<NoneAST> {

  public:
    NoneAST () {}

    StyioNodeHint hint() override {
      return StyioNodeHint::None;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { }");
    }
};

/* EmptyAST: Empty Tuple / List / Set */
class EmptyAST : public StyioNode<EmptyAST> {
  public:
    EmptyAST() {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Empty;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { }");
    }
};

/* EmptyBlockAST: Block */
class EmptyBlockAST : public StyioNode<EmptyBlockAST> {
  public:
    EmptyBlockAST() {}

    StyioNodeHint hint() override {
      return StyioNodeHint::EmptyBlock;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { }");
    }
};

class BoolAST : public StyioNode<BoolAST> {
  bool Value;

  public:
    BoolAST (
      bool val): 
      Value(val) {}

    StyioNodeHint hint () override {
      return StyioNodeHint::Bool; }

    static std::unique_ptr<BoolAST> make (
      bool value
    ) {
      return std::make_unique<BoolAST>(value);
    }

    bool getValue() {
      return Value; }

    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

class CasesAST : public StyioNode<CasesAST> {
  std::vector<std::tuple<std::unique_ptr<StyioAST>, std::unique_ptr<StyioAST>>> Cases;
  std::unique_ptr<StyioAST> LastExpr;

  public:
    CasesAST(
      std::unique_ptr<StyioAST> expr): 
      LastExpr(std::move(expr)) {}

    CasesAST(
      std::vector<std::tuple<std::unique_ptr<StyioAST>, std::unique_ptr<StyioAST>>> cases,
      std::unique_ptr<StyioAST> expr): 
      Cases(std::move(cases)),
      LastExpr(std::move(expr)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Cases;
    }

    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
    };

class SideBlockAST : public StyioNode<SideBlockAST> {
  std::unique_ptr<StyioAST> Resources;
  std::vector<std::unique_ptr<StyioAST>> Stmts;

  public:
    SideBlockAST(
      std::unique_ptr<StyioAST> resources,
      std::vector<std::unique_ptr<StyioAST>> stmts): 
      Resources(std::move(resources)),
      Stmts(std::move(stmts)) {}

    SideBlockAST(
      std::vector<std::unique_ptr<StyioAST>> stmts): 
      Stmts(std::move(stmts)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Block; }

    const std::vector<std::unique_ptr<StyioAST>>& getStmts() const {
      return Stmts; }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
    };

class MainBlockAST : public StyioNode<MainBlockAST> {
  std::unique_ptr<StyioAST> Resources;
  std::vector<std::unique_ptr<StyioAST>> Stmts;

  public:
    MainBlockAST(
      std::unique_ptr<StyioAST> resources,
      std::vector<std::unique_ptr<StyioAST>> stmts): 
      Resources(std::move(resources)),
      Stmts(std::move(stmts)) {}

    MainBlockAST(
      std::vector<std::unique_ptr<StyioAST>> stmts): 
      Stmts(std::move(stmts)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::MainBlock; }

    const std::vector<std::unique_ptr<StyioAST>>& getStmts() const {
      return Stmts; }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

class EOFAST : public StyioNode<EOFAST> {

  public:
    EOFAST () {}

    StyioNodeHint hint() override {
      return StyioNodeHint::End;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { }");
    }
};



class BreakAST : public StyioNode<BreakAST> {

  public:
    BreakAST () {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Break;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { }");
    }
};

class PassAST : public StyioNode<PassAST> {

  public:
    PassAST () {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Pass;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { }");
    }
};

class ReturnAST : public StyioNode<ReturnAST> {
  std::unique_ptr<StyioAST> Expr;

  public:
    ReturnAST (
      std::unique_ptr<StyioAST> expr): 
      Expr(std::move(expr)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Return; }

    const std::unique_ptr<StyioAST>& getExpr() {
      return Expr; }

    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};



/*
  =================
    Variable
  =================
*/

class IdAST : public StyioNode<IdAST> {
  std::string Id = "";

  public:
    IdAST(
      const std::string& id): 
      Id(id) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Id; }

    const std::string& getIdStr() const {
      return Id; }

    /* make shared pointer */
    static std::unique_ptr<IdAST> make(const std::string& id) {
      return std::make_unique<IdAST>(id); }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

class DTypeAST : public StyioNode<DTypeAST> {
  std::string TypeStr = "";

  public:
    DTypeAST(
      const std::string& dtype): 
      TypeStr(dtype) { }

    StyioNodeHint hint() override {
      return StyioNodeHint::DType; }

    static std::unique_ptr<DTypeAST> make (
      std::string dtype
    ) {
      return std::make_unique<DTypeAST>(dtype);
    }

    const std::string& getTypeStr() const {
      return TypeStr; }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

/*
  VarAST
  |- Global Variable
  |- Local Variable
  |- Temporary Variable
*/
class VarAST : public StyioNode<VarAST> {
  private:
    std::string Name = "";            /* Variable Name */
    std::shared_ptr<DTypeAST> DType;  /* Data Type */
    std::unique_ptr<StyioAST> DValue; /* Default Value */

  public:
    VarAST():
      Name("") {}

    VarAST(
      const std::string& name): 
      Name(name) {}

    VarAST(
      const std::string& name,
      std::shared_ptr<DTypeAST> data_type): 
      Name(name), 
      DType(std::move(data_type)) {}

    VarAST(
      const std::string& name,
      std::shared_ptr<DTypeAST> data_type,
      std::unique_ptr<StyioAST> default_value): 
      Name(name), 
      DType(std::move(data_type)),
      DValue(std::move(default_value)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Var; }

    const std::string& getName() const {
      return Name; }

    bool hasType() {
      if (DType) { return true; }
      else { return false; } }

    const std::shared_ptr<DTypeAST>& getType() const {
      return DType; }

    const std::string& getTypeStr() const {
      return DType -> getTypeStr(); }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

/*
  Function
  [+] Argument
*/
class ArgAST : public VarAST {
  private:
    std::string Name = "";            /* Variable Name */
    std::unique_ptr<DTypeAST> DType;  /* Data Type */
    std::unique_ptr<StyioAST> DValue; /* Default Value */

  public:
    ArgAST (
      const std::string& name):
      VarAST(
        name
      ),
      Name(name) {}

    ArgAST (
      const std::string& name,
      std::unique_ptr<DTypeAST> dtype): 
      VarAST(
        name, 
        DTypeAST::make(dtype -> getTypeStr())
      ),
      Name(name), 
      DType(std::move(dtype)) {}

    ArgAST (
      const std::string& name,
      std::unique_ptr<DTypeAST> data_type,
      std::unique_ptr<StyioAST> default_value): 
      VarAST(
        name, 
        DTypeAST::make(data_type -> getTypeStr()), /* Copy -> VarAST */
        std::move(default_value)
      ),
      Name(name), 
      DType(std::move(data_type)),
      DValue(std::move(default_value)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Arg; }

    static std::shared_ptr<ArgAST> make (
      const std::string& id) {
      return std::make_shared<ArgAST>(
        id
      );
    }

    static std::shared_ptr<ArgAST> make (
      const std::string& id,
      std::unique_ptr<DTypeAST> data_type) {
      return std::make_shared<ArgAST>(
        id,
        std::move(data_type)
      );
    }

    static std::shared_ptr<ArgAST> make (
      const std::string& id,
      std::unique_ptr<DTypeAST> data_type,
      std::unique_ptr<StyioAST> default_value) {
      return std::make_shared<ArgAST>(
        id,
        std::move(data_type),
        std::move(default_value)
      );
    }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

class OptArgAST : public VarAST {
  std::unique_ptr<IdAST> Id;

  public:
    OptArgAST(
      std::unique_ptr<IdAST> id): 
      Id(std::move(id)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::OptArg; }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

class OptKwArgAST : public VarAST {
  std::unique_ptr<IdAST> Id;

  public:
    OptKwArgAST(
      std::unique_ptr<IdAST> id): 
      Id(std::move(id)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::OptKwArg; }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

class VarTupleAST : public StyioNode<VarTupleAST> {
  std::vector<std::shared_ptr<VarAST>> Vars;

  public:
    VarTupleAST(
      std::vector<std::shared_ptr<VarAST>> vars): 
      Vars(std::move(vars)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::VarTuple; }

    const std::vector<std::shared_ptr<VarAST>>& getArgs() {
      return Vars; }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

/*
  =================
    Scalar Value
  =================
*/

/*
  IntAST: Integer
*/
class IntAST : public StyioNode<IntAST> {
  std::string Value;
  StyioDataType DType = StyioDataType::i32;

  public:
    IntAST (
      std::string val):
      Value(val) {}
    
    IntAST (
      std::string val,
      StyioDataType dtype):
      Value(val), 
      DType(dtype) {}

    StyioNodeHint hint () override {
      return StyioNodeHint::Int; }

    const std::string& getValue() const {
      return Value; }

    StyioDataType getType() const {
      return DType; }

    static std::unique_ptr<IntAST> make (
      std::string value
    ) {
      return std::make_unique<IntAST>(value);
    }

    static std::unique_ptr<IntAST> make (
      std::string value,
      StyioDataType dtype
    ) {
      return std::make_unique<IntAST>(
        value,
        dtype
      );
    }

    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

/*
  FloatAST: Float
*/
class FloatAST : public StyioNode<FloatAST> {
  private:
    StyioDataType DType = StyioDataType::f64;

    std::string Value;

    // std::string Significand = "";
    // int Base = 10;
    // int Exponent = 0;

  public:
    FloatAST(
      std::string value): 
      Value(value) {}

    // FloatAST(
    //   std::string significand,
    //   int exponent): 
    //   Significand(significand),
    //   Exponent(exponent) {}

    // FloatAST(
    //   std::string significand,
    //   int exponent,
    //   StyioDataType data_type): 
    //   Significand(significand),
    //   Exponent(exponent),
    //   DType(data_type) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Float; }

    StyioDataType getType() {
      return DType; }

    const std::string& getValue() const {
      return Value;
    }

    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

/*
  CharAST: Character
*/
class CharAST : public StyioNode<CharAST> {
  std::string Value;

  public:
    CharAST(std::string val) : Value(val) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Char;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + " { \'" + Value + "\' }";
    }
    };

/*
  StringAST: String
*/
class StringAST : public StyioNode<StringAST> {
  std::string Value;

  public:
    StringAST(std::string val) : Value(val) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::String;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return "\"" + Value + "\"";
    }
    };

/*
  FmtStrAST: String
*/
class FmtStrAST : public StyioNode<FmtStrAST> {
  std::vector<std::string> Fragments;
  std::vector<std::unique_ptr<StyioAST>> Exprs;

  public:
    FmtStrAST(
      std::vector<std::string> fragments,
      std::vector<std::unique_ptr<StyioAST>> expressions) : 
      Fragments(fragments), 
      Exprs(std::move(expressions)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::FmtStr;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + " { \"" + "\" }";
    }
};

class NumPromoAST : public StyioNode<NumPromoAST> {
  std::shared_ptr<StyioAST> Value;
  NumPromoTy PromoType;

  public:
    NumPromoAST (
      std::shared_ptr<StyioAST> val,
      NumPromoTy promo_type):
      Value(std::move(val)),
      PromoType(promo_type) {}

    StyioNodeHint hint () override {
      return StyioNodeHint::NumConvert; }

    static std::shared_ptr<NumPromoAST> make (
      std::shared_ptr<StyioAST> value,
      NumPromoTy promo_type
    ) {
      return std::make_unique<NumPromoAST>(
        std::move(value),
        promo_type
      );
    }

    static std::unique_ptr<IntAST> make (
      std::string value,
      StyioDataType dtype
    ) {
      return std::make_unique<IntAST>(
        value,
        dtype
      );
    }

    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

/*
  =================
    Data Resource Identifier
  =================
*/

/*
  ExtPathAST: (File) Path
*/
class ExtPathAST : public StyioNode<ExtPathAST> {
  std::unique_ptr<StringAST> Path;

  public:
    ExtPathAST (
      std::unique_ptr<StringAST> path): 
      Path(std::move(path)) 
      {

      }

    StyioNodeHint hint() override {
      return StyioNodeHint::ExtPath;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") + Path -> toStringInline(indent + 1, colorful) + " }";
    }
    };

/*
  ExtLinkAST: (Web) Link
*/
class ExtLinkAST : public StyioNode<ExtLinkAST> {
  std::string Link;

  public:
    ExtLinkAST (std::string link): Link(link) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::ExtLink;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
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
class ListAST : public StyioNode<ListAST> {
  std::vector<std::unique_ptr<StyioAST>> Elems;

  public:
    ListAST(
      std::vector<std::unique_ptr<StyioAST>> elems): 
      Elems(std::move(elems)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::List;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
    };

class TupleAST : public StyioNode<TupleAST> {
  std::vector<std::unique_ptr<StyioAST>> Elems;

  public:
    TupleAST(
      std::vector<std::unique_ptr<StyioAST>> elems): 
      Elems(std::move(elems)) 
      {

      }

    StyioNodeHint hint() override {
      return StyioNodeHint::Tuple;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
    };

class SetAST : public StyioNode<SetAST> {
  std::vector<std::unique_ptr<StyioAST>> Elems;

  public:
    SetAST(
      std::vector<std::unique_ptr<StyioAST>> elems): 
      Elems(std::move(elems)) 
      {

      }

    StyioNodeHint hint() override {
      return StyioNodeHint::Set;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
    };

/*
  RangeAST: Loop
*/
class RangeAST : public StyioNode<RangeAST>
{
  std::unique_ptr<StyioAST> StartVal;
  std::unique_ptr<StyioAST> EndVal;
  std::unique_ptr<StyioAST> StepVal;

  public:
    RangeAST(
      std::unique_ptr<StyioAST> start, 
      std::unique_ptr<StyioAST> end, 
      std::unique_ptr<StyioAST> step): 
      StartVal(std::move(start)), 
      EndVal(std::move(end)), 
      StepVal(std::move(step)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Range; }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
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
  std::unique_ptr<StyioAST> Value;

  public:
    SizeOfAST(
      std::unique_ptr<StyioAST> value): 
      Value(std::move(value)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::SizeOf; }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
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
  StyioNodeHint Op;
  std::shared_ptr<StyioAST> LHS;
  std::shared_ptr<StyioAST> RHS;

  public:
    BinOpAST(
      StyioNodeHint op, 
      std::shared_ptr<StyioAST> lhs, 
      std::shared_ptr<StyioAST> rhs): 
      Op(op),
      LHS(lhs), 
      RHS(rhs) {}

    StyioNodeHint hint() override {
      return Op; }

    const std::shared_ptr<StyioAST>& getLhs() const {
      return LHS; }

    const std::shared_ptr<StyioAST>& getRhs() const {
      return RHS; }

    void setLhs(
      std::shared_ptr<StyioAST> value
    ) {
      LHS.reset();
      LHS = std::move(value);
    }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

class BinCompAST: public StyioNode<BinCompAST>
{
  CompType CompSign;
  std::unique_ptr<StyioAST> LhsExpr;
  std::unique_ptr<StyioAST> RhsExpr;

  public:
    BinCompAST(
      CompType sign, 
      std::unique_ptr<StyioAST> lhs, 
      std::unique_ptr<StyioAST> rhs): 
      CompSign(sign), 
      LhsExpr(std::move(lhs)), 
      RhsExpr(std::move(rhs)) 
      {

      }

    StyioNodeHint hint() override {
      return StyioNodeHint::Compare;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
    };

class CondAST: public StyioNode<CondAST>
{
  LogicType LogicOp;
  
  /*
    RAW: expr
    NOT: !(expr)
  */
  std::unique_ptr<StyioAST> ValExpr;

  /*
    AND: expr && expr
    OR : expr || expr
  */
  std::unique_ptr<StyioAST> LhsExpr;
  std::unique_ptr<StyioAST> RhsExpr;

  public:
    CondAST(
      LogicType op, 
      std::unique_ptr<StyioAST> val): 
      LogicOp(op), 
      ValExpr(std::move(val))
      {

      }
    
    CondAST(
      LogicType op, 
      std::unique_ptr<StyioAST> lhs, 
      std::unique_ptr<StyioAST> rhs): 
      LogicOp(op), 
      LhsExpr(std::move(lhs)), 
      RhsExpr(std::move(rhs)) 
      {

      }

    StyioNodeHint hint() override {
      return StyioNodeHint::Condition;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
    };

class CallAST : public StyioNode<CallAST> {
  std::unique_ptr<StyioAST> Func;

  public:
    CallAST(
      std::unique_ptr<StyioAST> func) : 
      Func(std::move(func)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Call; }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
    };

class ListOpAST : public StyioNode<ListOpAST>
{
  StyioNodeHint OpType;
  std::unique_ptr<StyioAST> TheList;

  std::unique_ptr<StyioAST> Slot1;
  std::unique_ptr<StyioAST> Slot2;

  public:
    /*
      Get_Reversed
        [<]
    */
    ListOpAST(
      StyioNodeHint opType,
      std::unique_ptr<StyioAST> theList): 
      OpType(opType),
      TheList(std::move(theList)) 
      {

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
    ListOpAST(
      StyioNodeHint opType, 
      std::unique_ptr<StyioAST> theList, 
      std::unique_ptr<StyioAST> item): 
      OpType(opType), 
      TheList(std::move(theList)), 
      Slot1(std::move(item)) 
      {

      }

    /*
      Insert_Item_By_Index
        [+: index <- value]
    */
    ListOpAST(
      StyioNodeHint opType, 
      std::unique_ptr<StyioAST> theList, 
      std::unique_ptr<StyioAST> index, 
      std::unique_ptr<StyioAST> value): 
      OpType(opType), 
      TheList(std::move(theList)), 
      Slot1(std::move(index)), 
      Slot2(std::move(value)) 
      {

      }

    StyioNodeHint hint() override {
      return OpType;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
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
class ResourceAST : public StyioNode<ResourceAST> {
  std::vector<std::unique_ptr<StyioAST>> Resources;

  public:
    ResourceAST(
      std::vector<std::unique_ptr<StyioAST>> resources): 
      Resources(std::move(resources)) 
      {

      }

    StyioNodeHint hint() override {
      return StyioNodeHint::Resources;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
    };

/*
  =================
    Statement: Variable Assignment (Variable-Value Binding)
  =================
*/

/*
  FlexBindAST: Mutable Assignment (Flexible Binding)
*/
class FlexBindAST : public StyioNode<FlexBindAST> {
  std::unique_ptr<IdAST> varId;
  std::unique_ptr<StyioAST> valExpr;

  public:
    FlexBindAST(
      std::unique_ptr<IdAST> var, 
      std::unique_ptr<StyioAST> val) : 
      varId(std::move(var)), 
      valExpr(std::move(val)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::MutBind; }

    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
    };

/*
  FinalBindAST: Immutable Assignment (Final Binding)
*/
class FinalBindAST : public StyioNode<FinalBindAST> {
  std::unique_ptr<IdAST> VarId;
  std::unique_ptr<StyioAST> ValExpr;

  public:
    FinalBindAST(
      std::unique_ptr<IdAST> var, 
      std::unique_ptr<StyioAST> val) : 
      VarId(std::move(var)), 
      ValExpr(std::move(val)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::FixBind; }

    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
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
class StructAST : public StyioNode<StructAST> {
  std::unique_ptr<IdAST> FName;
  std::shared_ptr<VarTupleAST> FVars;
  std::unique_ptr<StyioAST> FBlock;

  public:
    StructAST(
      std::unique_ptr<IdAST> name, 
      std::shared_ptr<VarTupleAST> vars,
      std::unique_ptr<StyioAST> block) : 
      FName(std::move(name)), 
      FVars(vars),
      FBlock(std::move(block)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Struct; }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

/*
  =================
    OS Utility: IO Stream
  =================
*/

/*
  ReadFileAST: Read (File)
*/
class ReadFileAST : public StyioNode<ReadFileAST> {
  std::unique_ptr<IdAST> varId;
  std::unique_ptr<StyioAST> valExpr;

  public:
    ReadFileAST(
      std::unique_ptr<IdAST> var, 
      std::unique_ptr<StyioAST> val) : 
      varId(std::move(var)), 
      valExpr(std::move(val)) 
      {

      }

    StyioNodeHint hint() override {
      return StyioNodeHint::ReadFile;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
    };

/*
  PrintAST: Write to Standard Output (Print)
*/
class PrintAST : public StyioNode<PrintAST> {
  std::vector<std::unique_ptr<StyioAST>> Exprs;

  public:
    PrintAST(
      std::vector<std::unique_ptr<StyioAST>> exprs): 
      Exprs(std::move(exprs)) {

      }

    StyioNodeHint hint() override {
      return StyioNodeHint::Print;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
    };

/*
  =================
    Abstract Level: Dependencies
  =================
*/

/*
  ExtPackAST: External Packages
*/
class ExtPackAST : public StyioNode<ExtPackAST> {
  std::vector<std::string> PackPaths;

  public:
    ExtPackAST(
      std::vector<std::string> paths): 
      PackPaths(paths) 
      {

      }

    StyioNodeHint hint() override {
      return StyioNodeHint::ExtPack;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
    };

/*
  =================
    Abstract Level: Block 
  =================
*/

class CondFlowAST : public StyioNode<CondFlowAST> {
  std::unique_ptr<CondAST> CondExpr;
  std::unique_ptr<StyioAST> ThenBlock;
  std::unique_ptr<StyioAST> ElseBlock;

  public:
    StyioNodeHint WhatFlow;

    CondFlowAST(
      StyioNodeHint whatFlow,
      std::unique_ptr<CondAST> condition,
      std::unique_ptr<StyioAST> block): 
      WhatFlow(whatFlow),
      CondExpr(std::move(condition)),
      ThenBlock(std::move(block))
      {

      }

    CondFlowAST(
      StyioNodeHint whatFlow,
      std::unique_ptr<CondAST> condition,
      std::unique_ptr<StyioAST> blockThen,
      std::unique_ptr<StyioAST> blockElse): 
      WhatFlow(whatFlow),
      CondExpr(std::move(condition)),
      ThenBlock(std::move(blockThen)),
      ElseBlock(std::move(blockElse))
      {

      }

    StyioNodeHint hint() override {
      return WhatFlow;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
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
    
    For each step of iteration, check if the element match the value expression, 
    if match case is true, then execute the branch. 

  MatchCases: Fill + Cases
    >> Element(Single) ?= {
      v0 => {}
      v1 => {}
      _  => {}
    }
    
    For each step of iteration, check if the element match any value expression, 
    if match case is true, then execute the branch. 

  ExtraIsin: Fill + CheckIsIn
    >> Element(Single) ?^ IterableExpr(Collection) => {
      ...
    }

    For each step of iteration, check if the element is in the following collection,
    if match case is true, then execute the branch. 

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

    3. Is this value expression using any variable that was NOT previously defined?

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

class CheckEqAST : public StyioNode<CheckEqAST> {
  std::unique_ptr<StyioAST> Value;

  public:
    CheckEqAST(
      std::unique_ptr<StyioAST> value): 
      Value(std::move(value)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::CheckEq; }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

class CheckIsInAST : public StyioNode<CheckIsInAST> {
  std::unique_ptr<StyioAST> Iterable;

  public:
    CheckIsInAST(
      std::unique_ptr<StyioAST> value): 
      Iterable(std::move(value)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::CheckIsin; }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

/*
  FromToAST
*/
class FromToAST : public StyioNode<FromToAST> {
  std::unique_ptr<StyioAST> FromWhat;
  std::unique_ptr<StyioAST> ToWhat;

  public:
    FromToAST(
      std::unique_ptr<StyioAST> from_expr,
      std::unique_ptr<StyioAST> to_expr) : 
      FromWhat(std::move(from_expr)), 
      ToWhat(std::move(to_expr)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::FromTo; }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
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

class ForwardAST : public StyioNode<ForwardAST> {
  std::shared_ptr<VarTupleAST> Args;
  
  std::unique_ptr<CheckEqAST> ExtraEq;
  std::unique_ptr<CheckIsInAST> ExtraIsin;

  std::unique_ptr<StyioAST> ThenExpr;
  std::unique_ptr<CondFlowAST> ThenCondFlow;


  private:
    StyioNodeHint Type = StyioNodeHint::Forward;

  public:
    ForwardAST(
      std::unique_ptr<StyioAST> expr):
      ThenExpr(std::move(expr)) {
        Type = StyioNodeHint::Forward; }

    ForwardAST(
      std::unique_ptr<CheckEqAST> value,
      std::unique_ptr<StyioAST> whatnext):
      ExtraEq(std::move(value)),
      ThenExpr(std::move(whatnext)) { 
        Type = StyioNodeHint::If_Equal_To_Forward; }

    ForwardAST(
      std::unique_ptr<CheckIsInAST> isin,
      std::unique_ptr<StyioAST> whatnext): 
      ExtraIsin(std::move(isin)),
      ThenExpr(std::move(whatnext)) {
        Type = StyioNodeHint::If_Is_In_Forward; }

    ForwardAST(
      std::unique_ptr<CasesAST> cases): 
      ThenExpr(std::move(cases)) {
        Type = StyioNodeHint::Cases_Forward; }

    ForwardAST(
      std::unique_ptr<CondFlowAST> condflow): 
      ThenCondFlow(std::move(condflow)) {
        if ((condflow -> WhatFlow) == StyioNodeHint::CondFlow_True) {
          Type = StyioNodeHint::If_True_Forward; }
        else if ((condflow -> WhatFlow) == StyioNodeHint::CondFlow_False) {
          Type = StyioNodeHint::If_False_Forward; }
        else {
          Type = StyioNodeHint::If_Both_Forward; } }

    ForwardAST(
      std::shared_ptr<VarTupleAST> vars,
      std::unique_ptr<StyioAST> whatnext): 
      Args(std::move(vars)),
      ThenExpr(std::move(whatnext)) {
        Type = StyioNodeHint::Fill_Forward; }

    ForwardAST(
      std::shared_ptr<VarTupleAST> vars,
      std::unique_ptr<CheckEqAST> value,
      std::unique_ptr<StyioAST> whatnext): 
      Args(std::move(vars)),
      ExtraEq(std::move(value)),
      ThenExpr(std::move(whatnext)) {
        Type = StyioNodeHint::Fill_If_Equal_To_Forward; }

    ForwardAST(
      std::shared_ptr<VarTupleAST> vars,
      std::unique_ptr<CheckIsInAST> isin,
      std::unique_ptr<StyioAST> whatnext): 
      Args(std::move(vars)),
      ExtraIsin(std::move(isin)),
      ThenExpr(std::move(whatnext)) {
        Type = StyioNodeHint::Fill_If_Is_in_Forward; }

    
    ForwardAST(
      std::shared_ptr<VarTupleAST> vars,
      std::unique_ptr<CasesAST> cases): 
      Args(std::move(vars)),
      ThenExpr(std::move(cases)) {
        Type = StyioNodeHint::Fill_Cases_Forward; }

    ForwardAST(
      std::shared_ptr<VarTupleAST> vars,
      std::unique_ptr<CondFlowAST> condflow): 
      Args(std::move(vars)),
      ThenCondFlow(std::move(condflow)) {
      switch (condflow -> WhatFlow)
      {
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
      return Type; }

    bool hasArgs() {
      if (Args) { return true; }
      else { return false; } }

    const std::shared_ptr<VarTupleAST>& getArgs() {
      return Args; }

    const std::unique_ptr<StyioAST>& getThen() {
      return ThenExpr; }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
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
class InfiniteAST : public StyioNode<InfiniteAST> {
  InfiniteType WhatType;
  std::unique_ptr<StyioAST> Start;
  std::unique_ptr<StyioAST> IncEl;

  public:
    InfiniteAST() 
    {
      WhatType = InfiniteType::Original;
    }

    InfiniteAST(
      std::unique_ptr<StyioAST> start, 
      std::unique_ptr<StyioAST> incEl): 
      Start(std::move(start)), 
      IncEl(std::move(incEl)) 
      {
        WhatType = InfiniteType::Incremental;
      }

    StyioNodeHint hint() override {
      return StyioNodeHint::Infinite;
    }

    std::string toString(int indent = 0, bool colorful = false) override;

    std::string toStringInline(int indent = 0, bool colorful = false) override {
      return reprNodeType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }
    };

/*
  MatchCases
*/
class MatchCasesAST : public StyioNode<MatchCasesAST> {
  std::shared_ptr<StyioAST> Value; 
  std::unique_ptr<CasesAST> Cases;

  public:
    /* v ?= { _ => ... } */
    MatchCasesAST(
      std::shared_ptr<StyioAST> value,
      std::unique_ptr<CasesAST> cases): 
      Value(value),
      Cases(std::move(cases)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::MatchCases; }

    static std::shared_ptr<MatchCasesAST> make (
      std::shared_ptr<StyioAST> value,
      std::unique_ptr<CasesAST> cases
    ) {
      return std::make_shared<MatchCasesAST>(
        value,
        std::move(cases)
      );
    }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

/*
  Anonymous Function
    [+] Arguments
    [?] ExtraCheck
    [+] ThenExpr
*/
class AnonyFuncAST : public StyioNode<AnonyFuncAST> {
  private:
    std::shared_ptr<VarTupleAST> Args;
    std::unique_ptr<StyioAST> ThenExpr;

  public:
    /* #() => Then */
    AnonyFuncAST(
      std::shared_ptr<VarTupleAST> vars,
      std::unique_ptr<StyioAST> then): 
      Args(vars),
      ThenExpr(std::move(then)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::AnonyFunc; }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

/*
  FuncAST: Function
*/
class FuncAST : public StyioNode<FuncAST> {
  std::unique_ptr<IdAST> Name;
  std::unique_ptr<DTypeAST> RetType;
  std::unique_ptr<ForwardAST> Forward;

  bool isFinal;

  public:
    FuncAST(
      std::unique_ptr<ForwardAST> forward,
      bool isFinal) : 
      Forward(std::move(forward)),
      isFinal(isFinal) {}

    FuncAST(
      std::unique_ptr<IdAST> name, 
      std::unique_ptr<ForwardAST> forward,
      bool isFinal) : 
      Name(std::move(name)), 
      Forward(std::move(forward)),
      isFinal(isFinal) {}

    FuncAST(
      std::unique_ptr<IdAST> name, 
      std::unique_ptr<DTypeAST> type,
      std::unique_ptr<ForwardAST> forward,
      bool isFinal): 
      Name(std::move(name)), 
      RetType(std::move(type)),
      Forward(std::move(forward)),
      isFinal(isFinal) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Func; }

    bool hasName() {
      if (Name) { return true; }
      else { return false; } }

    const std::unique_ptr<IdAST>& getId() {
      return Name; }

    std::string getName() {
      return Name -> getIdStr(); }

    bool hasRetType() {
      if (RetType) { return true; }
      else { return false; } }

    const std::unique_ptr<DTypeAST>& getRetType() {
      return RetType; }

    const std::string& getRetTypeStr() {
      return RetType -> getTypeStr(); }

    const std::unique_ptr<ForwardAST>& getForward() const {
      return Forward; }

    bool hasArgs() {
      return Forward -> hasArgs(); }

    /* toString */
    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

/*
  =================
    Iterator
  =================
*/

/*
  IterInfinite: [...] >> {}
*/
class LoopAST : public StyioNode<LoopAST> {
  std::unique_ptr<ForwardAST> Forward;

  public:
    LoopAST(
      std::unique_ptr<ForwardAST> expr):
      Forward(std::move(expr)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Loop; }

    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

/*
  IterBounded: <List/Range> >> {}
*/
class IterAST : public StyioNode<IterAST> {
  std::unique_ptr<StyioAST> Collection;
  std::unique_ptr<ForwardAST> Forward;

  public:
    IterAST(
      std::unique_ptr<StyioAST> collection,
      std::unique_ptr<ForwardAST> forward): 
      Collection(std::move(collection)),
      Forward(std::move(forward)) {}

    StyioNodeHint hint() override {
      return StyioNodeHint::Iterator; }

    std::string toString(int indent = 0, bool colorful = false) override;
    std::string toStringInline(int indent = 0, bool colorful = false) override;
};

#endif