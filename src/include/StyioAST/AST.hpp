#pragma once
#ifndef STYIO_AST_H_
#define STYIO_AST_H_

// [Styio]
#include "../StyioToken/Token.hpp"
#include "../StyioVisitors/ASTAnalyzer.hpp"
#include "../StyioVisitors/CodeGenVisitor.hpp"
#include "ASTDecl.hpp"

// [LLVM]
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

/* ========================================================================== */

/*
  StyioAST: Styio Base AST
*/
class StyioAST
{
public:
  virtual ~StyioAST() {}

  /* type hint */
  virtual StyioNodeHint hint() = 0;

  
  virtual std::string toString(StyioAnalyzer* visitor, int indent) = 0;

  /* Type Inference */
  virtual void typeInfer(StyioAnalyzer* visitor) = 0;

  /* Get LLVM Type */
  virtual llvm::Type* getLLVMType(StyioToLLVMIR* visitor) = 0;

  /* LLVM IR Generator */
  virtual llvm::Value* toLLVMIR(StyioToLLVMIR* visitor) = 0;
};

/* ========================================================================== */

template <class Derived>
class StyioNode : public StyioAST
{
public:
  using StyioAST::hint;

  std::string toString(StyioAnalyzer* visitor, int indent) override {
    return visitor->toString(static_cast<Derived*>(this), indent);
  }

  void typeInfer(StyioAnalyzer* visitor) override {
    visitor->typeInfer(static_cast<Derived*>(this));
  }

  llvm::Type* getLLVMType(StyioToLLVMIR* visitor) override {
    return visitor->getLLVMType(static_cast<Derived*>(this));
  }

  llvm::Value* toLLVMIR(StyioToLLVMIR* visitor) override {
    return visitor->toLLVMIR(static_cast<Derived*>(this));
  }
};

/* ========================================================================== */

class CommentAST : public StyioNode<CommentAST>
{
private:
  string text;

public:
  CommentAST(const string& text) :
      text(text) {
  }

  static CommentAST* Create(const string& text) {
    return new CommentAST(text);
  }

  const string& getText() {
    return text;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Comment;
  }

  

  
};

/* ========================================================================== */

class IdAST : public StyioNode<IdAST>
{
private:
  string Id;

public:
  IdAST(string id) :
      Id(id) {
  }

  static IdAST* Create(string id) {
    return new IdAST(id);
  }

  const string& getId() {
    return Id;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Id;
  }

  
  

  
};

class DTypeAST : public StyioNode<DTypeAST>
{
private:
  StyioDataType data_type = StyioDataType::undefined;

public:
  DTypeAST() {}

  DTypeAST(StyioDataType data_type) :
      data_type(data_type) {
  }

  DTypeAST(
    string type_name
  ) {
    auto it = DType_Table.find(type_name);
    if (it != DType_Table.end()) {
      data_type = it->second;
    }
  }

  static DTypeAST* Create() {
    return new DTypeAST();
  }

  static DTypeAST* Create(StyioDataType data_type) {
    return new DTypeAST(data_type);
  }

  static DTypeAST* Create(string type_name) {
    return new DTypeAST(type_name);
  }

  StyioDataType getDType() {
    return data_type;
  }

  string getTypeName() {
    return reprDataType(data_type);
  }

  void setDType(StyioDataType type) {
    data_type = type;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::DType;
  }

  
  

  
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

  const string& getValue() {
    return value;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Char;
  }

  

  
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

  const string& getValue() {
    return value;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::String;
  }

  

  
};

/* ========================================================================== */

class CasesAST : public StyioNode<CasesAST>
{
  std::vector<std::pair<StyioAST*, StyioAST*>> Cases;
  StyioAST* LastExpr = nullptr;

public:
  CasesAST(StyioAST* expr) :
      LastExpr((expr)) {
  }

  CasesAST(std::vector<std::pair<StyioAST*, StyioAST*>> cases, StyioAST* expr) :
      Cases(cases), LastExpr(expr) {
  }

  static CasesAST* Create(StyioAST* expr) {
    return new CasesAST(expr);
  }

  static CasesAST* Create(std::vector<std::pair<StyioAST*, StyioAST*>> cases, StyioAST* expr) {
    return new CasesAST(cases, expr);
  }

  const std::vector<std::pair<StyioAST*, StyioAST*>>& getCases() {
    return Cases;
  }

  StyioAST* getLastExpr() {
    return LastExpr;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Cases;
  }

  

  
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

  vector<StyioAST*> getStmts() {
    return Stmts;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Block;
  }

  

  

  
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

  const vector<StyioAST*>& getStmts() {
    return Stmts;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::MainBlock;
  }

  

  
  

  
};

class EOFAST : public StyioNode<EOFAST>
{
public:
  EOFAST() {}

  StyioNodeHint hint() override {
    return StyioNodeHint::End;
  }

  

  
};

class BreakAST : public StyioNode<BreakAST>
{
public:
  BreakAST() {}

  StyioNodeHint hint() override {
    return StyioNodeHint::Break;
  }

  

  
};

class PassAST : public StyioNode<PassAST>
{
public:
  PassAST() {}

  StyioNodeHint hint() override {
    return StyioNodeHint::Pass;
  }

  

  
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
  string Name;                                                  /* Variable Name */
  DTypeAST* DType = DTypeAST::Create(StyioDataType::undefined); /* Data Type */
  StyioAST* DValue = nullptr;                                   /* Default Value */

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

  
  

  
};

/*
  Function
  [+] Argument
*/
class ArgAST : public VarAST
{
private:
  string Name;                                                  /* Variable Name */
  DTypeAST* DType = DTypeAST::Create(StyioDataType::undefined); /* Data Type */
  StyioAST* DValue = nullptr;                                   /* Default Value */

public:
  ArgAST(const string& name) :
      VarAST(name),
      Name(name) {
    std::cout << "got data type: " << DType->getTypeName() << std::endl;
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
    return new ArgAST(id, data_type, default_value);
  }

  const string& getName() {
    return Name;
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

  void setDType(StyioDataType type) {
    return DType->setDType(type);
  }

  
  

  
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

  static FmtStrAST* Create(vector<string> fragments, vector<StyioAST*> expressions) {
    return new FmtStrAST(fragments, expressions);
  };

  const vector<string>& getFragments() {
    return Fragments;
  }

  const vector<StyioAST*>& getExprs() {
    return Exprs;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::FmtStr;
  }
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
      Value(val), PromoType(promo_type) {
  }

  static TypeConvertAST* Create(
    StyioAST* value,
    NumPromoTy promo_type
  ) {
    return new TypeConvertAST(value, promo_type);
  }

  StyioAST* getValue() {
    return Value;
  }

  NumPromoTy getPromoTy() {
    return PromoType;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::NumConvert;
  }

  

  
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

  const string& getPath() {
    return Path;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::LocalPath;
  }
  
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

  const string& getPath() {
    return Path;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::RemotePath;
  }

  

  
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

  const string& getPath() {
    return Path;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::WebUrl;
  }

  

  
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

  const string& getPath() {
    return Path;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::DBUrl;
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
class ListAST : public StyioNode<ListAST>
{
  vector<StyioAST*> Elems;

public:
  ListAST(vector<StyioAST*> elems) :
      Elems(elems) {
  }

  const vector<StyioAST*>& getElements() {
    return Elems;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::List;
  }

  

  
};

class TupleAST : public StyioNode<TupleAST>
{
  vector<StyioAST*> Elems;

public:
  TupleAST(vector<StyioAST*> elems) :
      Elems(elems) {
  }

  const vector<StyioAST*>& getElements() {
    return Elems;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Tuple;
  }

  

  
};

class SetAST : public StyioNode<SetAST>
{
  vector<StyioAST*> Elems;

public:
  SetAST(vector<StyioAST*> elems) :
      Elems(elems) {
  }

  const vector<StyioAST*>& getElements() {
    return Elems;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Set;
  }

  

  
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

  StyioAST* getStart() {
    return StartVal;
  }

  StyioAST* getEnd() {
    return EndVal;
  }

  StyioAST* getStep() {
    return StepVal;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Range;
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
  StyioAST* Value = nullptr;

public:
  SizeOfAST(
    StyioAST* value
  ) :
      Value(value) {
  }

  StyioAST* getValue() {
    return Value;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::SizeOf;
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
  StyioNodeHint Operand;
  StyioAST* LHS = nullptr;
  StyioAST* RHS = nullptr;

public:
  BinOpAST(StyioNodeHint op, StyioAST* lhs, StyioAST* rhs) :
      Operand(op), LHS(lhs), RHS(rhs) {
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

  StyioNodeHint hint() override {
    return StyioNodeHint::BinOp;
  }
};

class BinCompAST : public StyioNode<BinCompAST>
{
  CompType CompSign;
  StyioAST* LhsExpr = nullptr;
  StyioAST* RhsExpr = nullptr;

public:
  BinCompAST(CompType sign, StyioAST* lhs, StyioAST* rhs) :
      CompSign(sign), LhsExpr(lhs), RhsExpr(rhs) {
  }

  CompType getSign() {
    return CompSign;
  }

  StyioAST* getLHS() {
    return LhsExpr;
  }

  StyioAST* getRHS() {
    return RhsExpr;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Compare;
  }

  

  
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
      LogicOp(op), ValExpr(val) {
  }

  CondAST(LogicType op, StyioAST* lhs, StyioAST* rhs) :
      LogicOp(op), LhsExpr(lhs), RhsExpr(rhs) {
  }

  LogicType getSign() {
    return LogicOp;
  }

  StyioAST* getValue() {
    return ValExpr;
  }

  StyioAST* getLHS() {
    return LhsExpr;
  }

  StyioAST* getRHS() {
    return RhsExpr;
  }

  

  StyioNodeHint hint() override {
    return StyioNodeHint::Condition;
  }

  

  
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
    return func_name->getId();
  }

  const vector<StyioAST*>& getArgs() {
    return func_args;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Call;
  }

  

  
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

  StyioNodeHint getOp() {
    return OpType;
  }

  StyioAST* getList() {
    return TheList;
  }

  StyioAST* getSlot1() {
    return Slot1;
  }

  StyioAST* getSlot2() {
    return Slot2;
  }

  StyioNodeHint hint() override {
    return OpType;
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
class ResourceAST : public StyioNode<ResourceAST>
{
  vector<StyioAST*> Resources;

public:
  ResourceAST(vector<StyioAST*> resources) :
      Resources(resources) {
  }

  const vector<StyioAST*>& getResList() {
    return Resources;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Resources;
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

  IdAST* getVarName() {
    return varName;
  }

  const string& getName() {
    return varName->getId();
  }

  StyioAST* getValue() {
    return valExpr;
  }

  StyioNodeHint getValueHint() {
    return valExpr->hint();
  }

  

  
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

  IdAST* getVarName() {
    return varName;
  }

  StyioAST* getValue() {
    return valExpr;
  }

  const string& getName() {
    return varName->getId();
  }
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
      varId(var), valExpr(val) {
  }

  IdAST* getId() {
    return varId;
  }

  StyioAST* getValue() {
    return valExpr;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::ReadFile;
  }

  

  
};

/*
  PrintAST: Write to Standard Output (Print)
*/
class PrintAST : public StyioNode<PrintAST>
{
  vector<StyioAST*> Exprs;

public:
  PrintAST(vector<StyioAST*> exprs) :
      Exprs(exprs) {
  }

  const vector<StyioAST*>& getExprs() {
    return Exprs;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Print;
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
class ExtPackAST : public StyioNode<ExtPackAST>
{
  vector<string> PackPaths;

public:
  ExtPackAST(vector<string> paths) :
      PackPaths(paths) {
  }

  const vector<string>& getPaths() {
    return PackPaths;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::ExtPack;
  }

  

  
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

  CondAST* getCond() {
    return CondExpr;
  }

  StyioAST* getThen() {
    return ThenBlock;
  }

  StyioAST* getElse() {
    return ElseBlock;
  }

  StyioNodeHint hint() override {
    return WhatFlow;
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
      Value(value) {
  }

  StyioAST* getValue() {
    return Value;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::CheckEq;
  }

  
  

  
};

class CheckIsinAST : public StyioNode<CheckIsinAST>
{
  StyioAST* Iterable = nullptr;

public:
  CheckIsinAST(
    StyioAST* value
  ) :
      Iterable(value) {
  }

  StyioAST* getIterable() {
    return Iterable;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::CheckIsin;
  }

  
  

  
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

  StyioAST* getFromExpr() {
    return FromWhat;
  }

  StyioAST* getToExpr() {
    return ToWhat;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::FromTo;
  }

  
  

  
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

  bool withParams() {
    return params && (!(params->getParams().empty()));
  }

  VarTupleAST* getVarTuple() {
    return params;
  }

  const vector<VarAST*>& getParams() {
    return params->getParams();
  }

  CheckEqAST* getCheckEq() {
    return ExtraEq;
  }

  CheckIsinAST* getCheckIsin() {
    return ExtraIsin;
  }

  StyioAST* getThen() {
    return ThenExpr;
  }

  CondFlowAST* getCondFlow() {
    return ThenCondFlow;
  }

  StyioAST* getRetExpr() {
    return ret_expr;
  }

  StyioNodeHint hint() override {
    return Type;
  }
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
      Start(start), IncEl(incEl) {
    WhatType = InfiniteType::Incremental;
  }

  InfiniteType getType() {
    return WhatType;
  }

  StyioAST* getStart() {
    return Start;
  }

  StyioAST* getIncEl() {
    return IncEl;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Infinite;
  }
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
      Args(vars), ThenExpr(then) {
  }

  VarTupleAST* getArgs() {
    return Args;
  }

  StyioAST* getThenExpr() {
    return ThenExpr;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::AnonyFunc;
  }

  
  

  
};

/*
  FuncAST: Function
*/
class FuncAST : public StyioNode<FuncAST>
{
private:
  IdAST* Name = nullptr;
  DTypeAST* RetType = nullptr;
  ForwardAST* Forward = nullptr;

public:
  bool isFinal;

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

  IdAST* getId() {
    return Name;
  }

  string getFuncName() {
    return Name->getId();
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

  vector<VarAST*> getAllArgs() {
    return Forward->getParams();
  }

  unordered_map<string, VarAST*> getParamMap() {
    unordered_map<string, VarAST*> param_map;

    for (auto param : Forward->getParams()) {
      param_map[param->getName()] = param;
    }

    return param_map;
  }

  
  

  
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

  ForwardAST* getForward() {
    return Forward;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Loop;
  }

  

  
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

  StyioAST* getIterable() {
    return Collection;
  }

  ForwardAST* getForward() {
    return Forward;
  }

  StyioNodeHint hint() override {
    return StyioNodeHint::Iterator;
  }

  

  
};

#endif