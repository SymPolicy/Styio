#pragma once
#ifndef STYIO_AST_H_
#define STYIO_AST_H_

// [C++]
#include <vector>

using std::vector;

// [Styio]
#include "../StyioToken/Token.hpp"
#include "../StyioToString/ToStringVisitor.hpp"
#include "../StyioAnalyzer/ASTAnalyzer.hpp"
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

  /* Type Hint */
  virtual const StyioNodeHint getNodeType() const = 0;

  virtual const StyioDataType getDataType() const = 0;

  /* StyioAST to String */
  virtual std::string toString(StyioRepr* visitor, int indent = 0) = 0;

  /* Type Inference */
  virtual void typeInfer(StyioAnalyzer* visitor) = 0;

  /* Code Gen. StyioIR */
  virtual StyioIR* toStyioIR(StyioAnalyzer* visitor) = 0;
};

/* ========================================================================== */

template <class Derived>
class StyioASTTraits : public StyioAST
{
public:
  using StyioAST::getNodeType;
  using StyioAST::getDataType;

  std::string toString(StyioRepr* visitor, int indent = 0) override {
    return visitor->toString(static_cast<Derived*>(this), indent);
  }

  void typeInfer(StyioAnalyzer* visitor) override {
    visitor->typeInfer(static_cast<Derived*>(this));
  }

  StyioIR* toStyioIR(StyioAnalyzer* visitor) override {
    return visitor->toStyioIR(static_cast<Derived*>(this));
  }
};

/* ========================================================================== */

class CommentAST : public StyioASTTraits<CommentAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Comment;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/* ========================================================================== */

class NameAST : public StyioASTTraits<NameAST>
{
private:
  string name_str;

public:
  NameAST(string name) :
      name_str(name) {
  }

  static NameAST* Create() {
    return new NameAST("");
  }

  static NameAST* Create(string name) {
    return new NameAST(name);
  }

  const string& getNameAsStr() {
    return name_str;
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Id;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class DTypeAST : public StyioASTTraits<DTypeAST>
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

  StyioDataType getType() {
    return data_type;
  }

  string getTypeName() {
    return reprDataType(data_type);
  }

  void setDType(StyioDataType type) {
    data_type = type;
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::DType;
  }

  const StyioDataType getDataType() const {
    return data_type;
  }
};

/* ========================================================================== */

/*
  NoneAST: None / Null / Nil
*/
class NoneAST : public StyioASTTraits<NoneAST>
{
public:
  NoneAST() {}

  static NoneAST* Create() {
    return new NoneAST();
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::None;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/*
  EmptyAST: Empty
*/
class EmptyAST : public StyioASTTraits<EmptyAST>
{
public:
  EmptyAST() {}

  static EmptyAST* Create() {
    return new EmptyAST();
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Empty;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/*
  BoolAST: Boolean
*/
class BoolAST : public StyioASTTraits<BoolAST>
{
  bool Value;

public:
  BoolAST(bool value) :
      Value(value) {
  }

  static BoolAST* Create(bool value) {
    return new BoolAST(value);
  }

  bool getValue() {
    return Value;
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Bool;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::i1;
  }
};

/*
  IntAST: Integer
*/
class IntAST : public StyioASTTraits<IntAST>
{
private:
  string value;
  DTypeAST* data_type = DTypeAST::Create(StyioDataType::i32);

public:
  IntAST(string value) :
      value(value) {
  }

  IntAST(string value, StyioDataType type) :
      value(value) {
        data_type->setDType(type);
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Int;
  }

  const StyioDataType getDataType() const {
    return data_type->getDataType();
  }

  void setDataType(StyioDataType type) {
    data_type->setDType(type);
  }
};

/*
  FloatAST: Float
*/
class FloatAST : public StyioASTTraits<FloatAST>
{
private:
  string value;
  DTypeAST* data_type = DTypeAST::Create(StyioDataType::f64);

public:
  FloatAST(const string& value) :
      value(value) {
  }

  FloatAST(const string& value, StyioDataType type) :
      value(value) {
    data_type->setDType(type);
  }

  static FloatAST* Create(const string& value) {
    return new FloatAST(value);
  }

  const string& getValue() {
    return value;
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Float;
  }

  const StyioDataType getDataType() const {
    return data_type->getDataType();
  }
};

/*
  CharAST: Single Character
*/
class CharAST : public StyioASTTraits<CharAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Char;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/*
  StringAST: String
*/
class StringAST : public StyioASTTraits<StringAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::String;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/* ========================================================================== */

class CasesAST : public StyioASTTraits<CasesAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Cases;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class BlockAST : public StyioASTTraits<BlockAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Block;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class MainBlockAST : public StyioASTTraits<MainBlockAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::MainBlock;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class EOFAST : public StyioASTTraits<EOFAST>
{
public:
  EOFAST() {}

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::End;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class BreakAST : public StyioASTTraits<BreakAST>
{
public:
  BreakAST() {}

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Break;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class PassAST : public StyioASTTraits<PassAST>
{
public:
  PassAST() {}

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Pass;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class ReturnAST : public StyioASTTraits<ReturnAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Return;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
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
class VarAST : public StyioASTTraits<VarAST>
{
private:
  NameAST* var_name_ = NameAST::Create();   /* Variable Name */
  DTypeAST* var_dtype = DTypeAST::Create(); /* Variable Data Type */
  StyioAST* var_init_val = nullptr;         /* Variable Initial Value */

public:
  VarAST(NameAST* name) :
      var_name_(name),
      var_dtype(DTypeAST::Create()) {
  }

  VarAST(NameAST* name, DTypeAST* data_type) :
      var_name_(name),
      var_dtype(data_type) {
  }

  VarAST(NameAST* name, DTypeAST* data_type, StyioAST* default_value) :
      var_name_(name),
      var_dtype(data_type),
      var_init_val(default_value) {
  }

  static VarAST* Create(NameAST* name) {
    return new VarAST(name);
  }

  static VarAST* Create(NameAST* name, DTypeAST* data_type) {
    return new VarAST(name, data_type);
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Variable;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }

  void setDataType(StyioDataType type) {
    var_dtype->setDType(type);
  }

  NameAST* getName() {
    return var_name_;
  }

  const string& getNameAsStr() {
    return var_name_->getNameAsStr();
  }

  DTypeAST* getDType() {
    return var_dtype;
  }
  
  string getTypeAsStr() {
    return var_dtype->getTypeName();
  }

  bool isTyped() {
    return (var_dtype && (var_dtype->getType() != StyioDataType::undefined));
  }
};

/*
  Function
  [+] Argument
*/
class ArgAST : public VarAST
{
private:
  NameAST* var_name = NameAST::Create("");  /* Variable Name */
  DTypeAST* var_dtype = DTypeAST::Create(); /* Variable Data Type */
  StyioAST* var_init_value = nullptr;       /* Variable Initial Value */

public:
  ArgAST(NameAST* name) :
      VarAST(name),
      var_name(name) {
  }

  ArgAST(
    NameAST* name,
    DTypeAST* data_type
  ) :
      VarAST(name, data_type),
      var_name(name),
      var_dtype(data_type) {
  }

  ArgAST(NameAST* name, DTypeAST* data_type, StyioAST* default_value) :
      VarAST(name, data_type, default_value),
      var_name(name),
      var_dtype(data_type),
      var_init_value(default_value) {
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Arg;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }

  static ArgAST* Create(NameAST* name) {
    return new ArgAST(name);
  }

  static ArgAST* Create(NameAST* name, DTypeAST* data_type) {
    return new ArgAST(name, data_type);
  }

  static ArgAST* Create(NameAST* name, DTypeAST* data_type, StyioAST* default_value) {
    return new ArgAST(name, data_type, default_value);
  }

  const string& getName() {
    return var_name->getNameAsStr();
  }

  bool isTyped() {
    return (
      var_dtype != nullptr
      && (var_dtype->getType() != StyioDataType::undefined)
    );
  }

  DTypeAST* getDType() {
    return var_dtype;
  }

  void setDType(StyioDataType type) {
    return var_dtype->setDType(type);
  }
};

class OptArgAST : public VarAST
{
private:
  NameAST* var_name = nullptr;

public:
  OptArgAST(NameAST* name) :
      VarAST(name),
      var_name(name) {
  }

  static OptArgAST* Create(NameAST* name) {
    return new OptArgAST(name);
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::OptArg;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class OptKwArgAST : public VarAST
{
  NameAST* var_name = nullptr;

public:
  OptKwArgAST(NameAST* name) :
      VarAST(name),
      var_name(name) {
  }

  static OptKwArgAST* Create(NameAST* id) {
    return new OptKwArgAST(id);
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::OptKwArg;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class VarTupleAST : public StyioASTTraits<VarTupleAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::VarTuple;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/*
  FmtStrAST: String
*/
class FmtStrAST : public StyioASTTraits<FmtStrAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::FmtStr;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class TypeConvertAST : public StyioASTTraits<TypeConvertAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::NumConvert;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
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
class LocalPathAST : public StyioASTTraits<LocalPathAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::LocalPath;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/*
  ipv4 / ipv6 / example.com
*/
class RemotePathAST : public StyioASTTraits<RemotePathAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::RemotePath;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/*
  Web URL
  - HTTP
  - HTTPS
  - FTP
*/
class WebUrlAST : public StyioASTTraits<WebUrlAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::WebUrl;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/* Database Access URL */
class DBUrlAST : public StyioASTTraits<DBUrlAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::DBUrl;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
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

/* Tuple */
class TupleAST : public StyioASTTraits<TupleAST>
{
  vector<StyioAST*> elements;
  bool consistency = false;
  DTypeAST* consistent_type = DTypeAST::Create();

public:
  TupleAST(vector<StyioAST*> elems) :
      elements(elems) {
  }

  static TupleAST* Create(vector<StyioAST*> elems) {
    return new TupleAST(std::move(elems));
  }

  const vector<StyioAST*>& getElements() {
    return elements;
  }

  DTypeAST* getDTypeObj() {
    return consistent_type;
  }

  void setConsistency(bool value) {
    consistency = value;
  }

  bool isConsistent() {
    return consistency;
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Tuple;
  }

  const StyioDataType getDataType() const {
    return consistent_type->getDataType();
  }

  void setDataType(StyioDataType type) {
    consistent_type->setDType(type);
  }
};

/*
  ListAST: List (Extendable)
*/
class ListAST : public StyioASTTraits<ListAST>
{
  vector<StyioAST*> elements_;
  bool consistency = false;
  DTypeAST* consistent_type = DTypeAST::Create();

public:
  ListAST() {
  }

  ListAST(vector<StyioAST*> elems) :
      elements_(elems) {
  }

  static ListAST* Create() {
    return new ListAST();
  }

  static ListAST* Create(vector<StyioAST*> elems) {
    return new ListAST(std::move(elems));
  }

  const vector<StyioAST*>& getElements() {
    return elements_;
  }

  DTypeAST* getDTypeObj() {
    return consistent_type;
  }

  void setConsistency(bool value) {
    consistency = value;
  }

  bool isConsistent() {
    return consistency;
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::List;
  }

  const StyioDataType getDataType() const {
    return consistent_type->getDataType();
  }

  void setDataType(StyioDataType type) {
    consistent_type->setDType(type);
  }
};

class SetAST : public StyioASTTraits<SetAST>
{
  vector<StyioAST*> elements_;

public:
  SetAST(vector<StyioAST*> elems) :
      elements_(elems) {
  }

  static SetAST* Create(vector<StyioAST*> elems) {
    return new SetAST(std::move(elems));
  }

  const vector<StyioAST*>& getElements() {
    return elements_;
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Set;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/*
  RangeAST: Loop
*/
class RangeAST : public StyioASTTraits<RangeAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Range;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
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
class SizeOfAST : public StyioASTTraits<SizeOfAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::SizeOf;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
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
class BinOpAST : public StyioASTTraits<BinOpAST>
{
  DTypeAST* data_type = DTypeAST::Create();

  TokenKind operand;
  StyioAST* LHS = nullptr;
  StyioAST* RHS = nullptr;

public:
  BinOpAST(TokenKind op, StyioAST* lhs, StyioAST* rhs) :
      operand(op), LHS(lhs), RHS(rhs) {
  }

  static BinOpAST* Create(TokenKind op, StyioAST* lhs, StyioAST* rhs) {
    return new BinOpAST(op, lhs, rhs);
  }

  TokenKind getOp() {
    return operand;
  }

  StyioAST* getLHS() {
    return LHS;
  }

  StyioAST* getRHS() {
    return RHS;
  }

  StyioDataType getType() {
    return data_type->getType();
  }

  void setDType(StyioDataType type) {
    return data_type->setDType(type);
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::BinOp;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class BinCompAST : public StyioASTTraits<BinCompAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Compare;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class CondAST : public StyioASTTraits<CondAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Condition;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class CallAST : public StyioASTTraits<CallAST>
{
private:
  NameAST* func_name = nullptr;
  vector<StyioAST*> func_args;

public:
  CallAST(
    NameAST* func_name,
    vector<StyioAST*> arguments
  ) :
      func_name(func_name),
      func_args(arguments) {
  }

  NameAST* getFuncName() {
    return func_name;
  }

  const string& getNameAsStr() {
    return func_name->getNameAsStr();
  }

  const vector<StyioAST*>& getArgList() {
    return func_args;
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Call;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class ListOpAST : public StyioASTTraits<ListOpAST>
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

  const StyioNodeHint getNodeType() const {
    return OpType;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
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
class ResourceAST : public StyioASTTraits<ResourceAST>
{
  vector<StyioAST*> Resources;

public:
  ResourceAST(vector<StyioAST*> resources) :
      Resources(resources) {
  }

  const vector<StyioAST*>& getResList() {
    return Resources;
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Resources;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
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
class FlexBindAST : public StyioASTTraits<FlexBindAST>
{
  VarAST* variable = nullptr;
  StyioAST* value = nullptr;

public:
  FlexBindAST(VarAST* variable, StyioAST* value) :
      variable(variable), value(value) {
  }

  static FlexBindAST* Create(VarAST* variable, StyioAST* value) {
    return new FlexBindAST(variable, value);
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::MutBind;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }

  VarAST* getVar() {
    return variable;
  }

  StyioAST* getValue() {
    return value;
  }

  const string& getNameAsStr() {
    return variable->getName()->getNameAsStr();
  }
};

/*
  FinalBindAST: Immutable Assignment (Final Binding)
*/
class FinalBindAST : public StyioASTTraits<FinalBindAST>
{
  NameAST* varName = nullptr;
  StyioAST* valExpr = nullptr;

public:
  FinalBindAST(NameAST* var, StyioAST* val) :
      varName(var), valExpr(val) {
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::FinalBind;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }

  NameAST* getVarName() {
    return varName;
  }

  StyioAST* getValue() {
    return valExpr;
  }

  const string& getName() {
    return varName->getNameAsStr();
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
class StructAST : public StyioASTTraits<StructAST>
{
  NameAST* FName = nullptr;
  VarTupleAST* FVars = nullptr;
  StyioAST* FBlock = nullptr;

public:
  StructAST(NameAST* name, VarTupleAST* vars, StyioAST* block) :
      FName((name)), FVars(vars), FBlock((block)) {
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Struct;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
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
class ReadFileAST : public StyioASTTraits<ReadFileAST>
{
  NameAST* varId = nullptr;
  StyioAST* valExpr = nullptr;

public:
  ReadFileAST(NameAST* var, StyioAST* val) :
      varId(var), valExpr(val) {
  }

  NameAST* getId() {
    return varId;
  }

  StyioAST* getValue() {
    return valExpr;
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::ReadFile;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/*
  PrintAST: Write to Standard Output (Print)
*/
class PrintAST : public StyioASTTraits<PrintAST>
{
  vector<StyioAST*> Exprs;

public:
  PrintAST(vector<StyioAST*> exprs) :
      Exprs(exprs) {
  }

  const vector<StyioAST*>& getExprs() {
    return Exprs;
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Print;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
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
class ExtPackAST : public StyioASTTraits<ExtPackAST>
{
  vector<string> PackPaths;

public:
  ExtPackAST(vector<string> paths) :
      PackPaths(paths) {
  }

  const vector<string>& getPaths() {
    return PackPaths;
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::ExtPack;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/*
  =================
    Abstract Level: Block
  =================
*/

class CondFlowAST : public StyioASTTraits<CondFlowAST>
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

  const StyioNodeHint getNodeType() const {
    return WhatFlow;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
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

class CheckEqAST : public StyioASTTraits<CheckEqAST>
{
  StyioAST* Value = nullptr;

public:
  CheckEqAST(StyioAST* value) :
      Value(value) {
  }

  StyioAST* getValue() {
    return Value;
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::CheckEq;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

class CheckIsinAST : public StyioASTTraits<CheckIsinAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::CheckIsin;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/*
  FromToAST
*/
class FromToAST : public StyioASTTraits<FromToAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::FromTo;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
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

class ForwardAST : public StyioASTTraits<ForwardAST>
{
  VarTupleAST* Params = nullptr;

  CheckEqAST* ExtraEq = nullptr;
  CheckIsinAST* ExtraIsin = nullptr;

  StyioAST* ThenExpr = nullptr;
  CondFlowAST* ThenCondFlow = nullptr;

  StyioAST* RetExpr = nullptr;

private:
  StyioNodeHint Type = StyioNodeHint::Forward;

public:
  ForwardAST(StyioAST* expr) :
      ThenExpr(expr) {
    Type = StyioNodeHint::Forward;
    if (ThenExpr->getNodeType() != StyioNodeHint::Block) {
      RetExpr = expr;
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
    StyioAST* then_expr
  ) :
      Params(vars),
      ThenExpr(then_expr) {
    Type = StyioNodeHint::Fill_Forward;
    if (ThenExpr->getNodeType() != StyioNodeHint::Block) {
      RetExpr = then_expr;
    }
  }

  ForwardAST(
    VarTupleAST* vars,
    CheckEqAST* value,
    StyioAST* whatnext
  ) :
      Params(vars), ExtraEq(value), ThenExpr(whatnext) {
    Type = StyioNodeHint::Fill_If_Equal_To_Forward;
  }

  ForwardAST(VarTupleAST* vars, CheckIsinAST* isin, StyioAST* whatnext) :
      Params(vars), ExtraIsin(isin), ThenExpr(whatnext) {
    Type = StyioNodeHint::Fill_If_Is_in_Forward;
  }

  ForwardAST(VarTupleAST* vars, CasesAST* cases) :
      Params(vars), ThenExpr(cases) {
    Type = StyioNodeHint::Fill_Cases_Forward;
  }

  ForwardAST(VarTupleAST* vars, CondFlowAST* condflow) :
      Params(vars), ThenCondFlow(condflow) {
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
    return Params && (!(Params->getParams().empty()));
  }

  VarTupleAST* getVarTuple() {
    return Params;
  }

  const vector<VarAST*>& getParams() {
    return Params->getParams();
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
    return RetExpr;
  }

  void setRetExpr(StyioAST* expr) {
    RetExpr = expr;
  }

  const StyioNodeHint getNodeType() const {
    return Type;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
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
class InfiniteAST : public StyioASTTraits<InfiniteAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Infinite;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/*
  MatchCases
*/
class MatchCasesAST : public StyioASTTraits<MatchCasesAST>
{
  StyioAST* Value = nullptr;
  CasesAST* Cases = nullptr;

public:
  /* v ?= { _ => ... } */
  MatchCasesAST(StyioAST* value, CasesAST* cases) :
      Value(value), Cases((cases)) {
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::MatchCases;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
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
class AnonyFuncAST : public StyioASTTraits<AnonyFuncAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::AnonyFunc;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/*
  FuncAST: Function
*/
class FuncAST : public StyioASTTraits<FuncAST>
{
private:
  NameAST* Name = nullptr;
  DTypeAST* RetType = DTypeAST::Create();
  ForwardAST* Forward = nullptr;

public:
  bool isFinal;

  /*
    A function that contains sufficient information for the code generation
      without refering additional information from any other definition or statement
      is called self-completed.
  */
  bool isSelfCompleted;

  FuncAST(
    ForwardAST* forward,
    bool isFinal
  ) :
      Forward((forward)),
      isFinal(isFinal) {
  }

  FuncAST(
    NameAST* name,
    ForwardAST* forward,
    bool isFinal
  ) :
      Name((name)),
      Forward((forward)),
      isFinal(isFinal) {
  }

  FuncAST(
    NameAST* name,
    DTypeAST* type,
    ForwardAST* forward,
    bool isFinal
  ) :
      Name(name),
      RetType(type),
      Forward(forward),
      isFinal(isFinal) {
  }

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Func;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }

  bool hasName() {
    if (Name) {
      return true;
    }
    else {
      return false;
    }
  }

  NameAST* getId() {
    return Name;
  }

  string getFuncName() {
    return Name->getNameAsStr();
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

  void setRetType(StyioDataType type) {
    if (RetType != nullptr) {
      RetType->setDType(type);
    }
    else {
      RetType = DTypeAST::Create(type);
    }
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
      param_map[param->getNameAsStr()] = param;
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
class LoopAST : public StyioASTTraits<LoopAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Loop;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

/*
  IterBounded: <List/Range> >> {}
*/
class IterAST : public StyioASTTraits<IterAST>
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

  const StyioNodeHint getNodeType() const {
    return StyioNodeHint::Iterator;
  }

  const StyioDataType getDataType() const {
    return StyioDataType::undefined;
  }
};

#endif