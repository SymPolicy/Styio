#pragma once
#ifndef STYIO_AST_H_
#define STYIO_AST_H_

// [C++]
#include <vector>

using std::vector;

// [Styio]
#include "../StyioAnalyzer/ASTAnalyzer.hpp"
#include "../StyioToString/ToStringVisitor.hpp"
#include "../StyioToken/Token.hpp"
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
  virtual const StyioASTType getNodeType() const = 0;

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
  using StyioAST::getDataType;
  using StyioAST::getNodeType;

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

  const StyioASTType getNodeType() const {
    return StyioASTType::Comment;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const string& getAsStr() {
    return name_str;
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::Id;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }
};

class DTypeAST : public StyioASTTraits<DTypeAST>
{
private:
  DTypeAST() {}

  DTypeAST(StyioDataType data_type) :
      data_type(data_type) {
  }

  DTypeAST(
    string type_name
  ) {
    auto it = DTypeTable.find(type_name);
    if (it != DTypeTable.end()) {
      data_type = it->second;
    }
    else {
      data_type = StyioDataType{StyioDataTypeOption::Defined, type_name, 0};
    }
  }

public:
  StyioDataType data_type = StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};

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
    return data_type.name;
  }

  void setDType(StyioDataType type) {
    data_type = type;
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::DType;
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

  const StyioASTType getNodeType() const {
    return StyioASTType::None;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::Empty;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::Bool;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Bool, "Boolean", 1};
  }
};

/*
  IntAST: Integer
*/
class IntAST : public StyioASTTraits<IntAST>
{
public:
  string value = "";
  size_t num_of_bit = 0;

  IntAST(string value) :
      value(value) {
  }

  IntAST(string value, size_t num_of_bit) :
      value(value), num_of_bit(num_of_bit) {
  }

  static IntAST* Create(string value) {
    return new IntAST(value);
  }

  static IntAST* Create(string value, size_t num_of_bit) {
    return new IntAST(value, num_of_bit);
  }

  const string& getValue() {
    return value;
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::Integer;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Integer, "Integer", num_of_bit};
  }
};

/*
  FloatAST: Float
*/
class FloatAST : public StyioASTTraits<FloatAST>
{
public:
  string value;
  DTypeAST* data_type = DTypeAST::Create(StyioDataType{StyioDataTypeOption::Float, "Float", 64});

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

  const StyioASTType getNodeType() const {
    return StyioASTType::Float;
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

  const StyioASTType getNodeType() const {
    return StyioASTType::Char;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }
};

/*
  StringAST: String
*/
class StringAST : public StyioASTTraits<StringAST>
{
private:
  string value;

  StringAST(std::string value) :
      value(value) {
  }

public:
  static StringAST* Create(std::string value) {
    return new StringAST(value);
  }

  const string& getValue() {
    return value;
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::String;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::Cases;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::Block;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::MainBlock;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }
};

class EOFAST : public StyioASTTraits<EOFAST>
{
private:
  EOFAST() {}

public:
  static EOFAST* Create() {
    return new EOFAST();
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::End;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }
};

class BreakAST : public StyioASTTraits<BreakAST>
{
public:
  BreakAST() {}

  const StyioASTType getNodeType() const {
    return StyioASTType::Break;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }
};

class PassAST : public StyioASTTraits<PassAST>
{
private:
  PassAST() {}

public:
  static PassAST* Create() {
    return new PassAST();
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::Pass;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::Return;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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
public:
  NameAST* var_name = NameAST::Create();   /* Variable Name */
  DTypeAST* var_type = DTypeAST::Create(); /* Variable Data Type */
  StyioAST* val_init = nullptr;            /* Variable Initial Value */

  VarAST(NameAST* name) :
      var_name(name),
      var_type(DTypeAST::Create()) {
  }

  VarAST(NameAST* name, DTypeAST* data_type) :
      var_name(name),
      var_type(data_type) {
  }

  VarAST(NameAST* name, DTypeAST* data_type, StyioAST* default_value) :
      var_name(name),
      var_type(data_type),
      val_init(default_value) {
  }

  static VarAST* Create(NameAST* name) {
    return new VarAST(name);
  }

  static VarAST* Create(NameAST* name, DTypeAST* data_type) {
    return new VarAST(name, data_type);
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::Variable;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }

  void setDataType(StyioDataType type) {
    var_type->setDType(type);
  }

  NameAST* getName() {
    return var_name;
  }

  const string& getNameAsStr() {
    return var_name->getAsStr();
  }

  DTypeAST* getDType() {
    return var_type;
  }

  string getTypeAsStr() {
    return var_type->getTypeName();
  }

  bool isTyped() {
    return (var_type && (var_type->getType().option != StyioDataTypeOption::Undefined));
  }
};

/*
  Function
  [+] Argument
*/
class ParamAST : public VarAST
{
private:
  ParamAST(NameAST* name) :
      VarAST(name),
      var_name(name) {
  }

  ParamAST(
    NameAST* name,
    DTypeAST* data_type
  ) :
      VarAST(name, data_type),
      var_name(name),
      var_type(data_type) {
  }

  ParamAST(NameAST* name, DTypeAST* data_type, StyioAST* default_value) :
      VarAST(name, data_type, default_value),
      var_name(name),
      var_type(data_type),
      val_init(default_value) {
  }

public:
  NameAST* var_name = NameAST::Create(""); /* Variable Name */
  DTypeAST* var_type = DTypeAST::Create(); /* Variable Data Type */
  StyioAST* val_init = nullptr;            /* Variable Initial Value */

  const StyioASTType getNodeType() const {
    return StyioASTType::Arg;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }

  static ParamAST* Create(NameAST* name) {
    return new ParamAST(name);
  }

  static ParamAST* Create(NameAST* name, DTypeAST* data_type) {
    return new ParamAST(name, data_type);
  }

  static ParamAST* Create(NameAST* name, DTypeAST* data_type, StyioAST* default_value) {
    return new ParamAST(name, data_type, default_value);
  }

  const string& getName() {
    return var_name->getAsStr();
  }

  bool isTyped() {
    return (
      var_type != nullptr
      && (var_type->getType().option != StyioDataTypeOption::Undefined)
    );
  }

  DTypeAST* getDType() {
    return var_type;
  }

  void setDType(StyioDataType type) {
    return var_type->setDType(type);
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

  const StyioASTType getNodeType() const {
    return StyioASTType::OptArg;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::OptKwArg;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::FmtStr;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::NumConvert;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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
class ResPathAST : public StyioASTTraits<ResPathAST>
{
  string Path;
  StyioPathType Type;

public:
  ResPathAST(
    StyioPathType type,
    string path
  ) :
      Type(type),
      Path(path) {
  }

  static ResPathAST* Create(
    StyioPathType type,
    string path
  ) {
    return new ResPathAST(type, path);
  }

  const string& getPath() {
    return Path;
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::LocalPath;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::RemotePath;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::WebUrl;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::DBUrl;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  TupleAST(vector<VarAST*> elems) :
      elements(elems.begin(), elems.end()) {
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

  const StyioASTType getNodeType() const {
    return StyioASTType::Tuple;
  }

  const StyioDataType getDataType() const {
    return consistent_type->getDataType();
  }

  void setDataType(StyioDataType type) {
    consistent_type->setDType(type);
  }
};

class VarTupleAST : public TupleAST
{
private:
  std::vector<VarAST*> Vars;

public:
  VarTupleAST(std::vector<VarAST*> vars) :
      TupleAST(vars),
      Vars(vars) {
  }

  static VarTupleAST* Create(std::vector<VarAST*> vars) {
    return new VarTupleAST(vars);
  }

  const vector<VarAST*>& getParams() {
    return Vars;
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::Parameters;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::List;
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

  const StyioASTType getNodeType() const {
    return StyioASTType::Set;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::Range;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::SizeOf;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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
public:
  DTypeAST* data_type = DTypeAST::Create();

  StyioOpType operand;
  StyioAST* LHS = nullptr;
  StyioAST* RHS = nullptr;

  BinOpAST(StyioOpType op, StyioAST* lhs, StyioAST* rhs) :
      operand(op), LHS(lhs), RHS(rhs) {
  }

  static BinOpAST* Create(StyioOpType op, StyioAST* lhs, StyioAST* rhs) {
    return new BinOpAST(op, lhs, rhs);
  }

  StyioOpType getOp() {
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

  const StyioASTType getNodeType() const {
    return StyioASTType::BinOp;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::Compare;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  static CondAST* Create(LogicType op, StyioAST* val) {
    return new CondAST(op, val);
  }

  static CondAST* Create(LogicType op, StyioAST* lhs, StyioAST* rhs) {
    return new CondAST(op, lhs, rhs);
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

  const StyioASTType getNodeType() const {
    return StyioASTType::Condition;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }
};

class CallAST : public StyioASTTraits<CallAST>
{
public:
  StyioAST* func_callee = nullptr;
  NameAST* func_name = nullptr;
  vector<StyioAST*> func_args;

  CallAST(
    NameAST* func_name,
    vector<StyioAST*> arguments
  ) :
      func_name(func_name),
      func_args(arguments) {
  }

  CallAST(
    StyioAST* func_callee,
    NameAST* func_name,
    vector<StyioAST*> arguments
  ) :
      func_callee(func_callee),
      func_name(func_name),
      func_args(arguments) {
  }

  NameAST* getFuncName() {
    return func_name;
  }

  const string& getNameAsStr() {
    return func_name->getAsStr();
  }

  const vector<StyioAST*>& getArgList() {
    return func_args;
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::Call;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }
};

class ListOpAST : public StyioASTTraits<ListOpAST>
{
  StyioASTType OpType;
  StyioAST* TheList = nullptr;

  StyioAST* Slot1 = nullptr;
  StyioAST* Slot2 = nullptr;

public:
  /*
    Get_Reversed
      [<]
  */
  ListOpAST(StyioASTType opType, StyioAST* theList) :
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
  ListOpAST(StyioASTType opType, StyioAST* theList, StyioAST* item) :
      OpType(opType), TheList((theList)), Slot1((item)) {
  }

  /*
    Insert_Item_By_Index
      [+: index <- value]
  */
  ListOpAST(StyioASTType opType, StyioAST* theList, StyioAST* index, StyioAST* value) :
      OpType(opType), TheList((theList)), Slot1((index)), Slot2((value)) {
  }

  StyioASTType getOp() {
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

  const StyioASTType getNodeType() const {
    return OpType;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }
};

class AttrAST : public StyioASTTraits<AttrAST>
{
public:
  StyioAST* body = nullptr;
  StyioAST* attr = nullptr;

  AttrAST(
    StyioAST* body,
    StyioAST* attr
  ) :
      body(body),
      attr(attr) {
  }

  static AttrAST* Create(StyioAST* body, StyioAST* attr) {
    return new AttrAST(body, attr);
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::Attribute;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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
private:
  ResourceAST(std::vector<std::pair<StyioAST*, std::string>> res_list) :
      res_list(res_list) {
  }

public:
  std::vector<std::pair<StyioAST*, std::string>> res_list;

  static ResourceAST* Create(std::vector<std::pair<StyioAST*, std::string>> resources_with_types) {
    return new ResourceAST(resources_with_types);
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::Resources;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::MutBind;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }

  VarAST* getVar() {
    return variable;
  }

  StyioAST* getValue() {
    return value;
  }

  const string& getNameAsStr() {
    return variable->getName()->getAsStr();
  }
};

/*
  FinalBindAST: Immutable Assignment (Final Binding)
*/
class FinalBindAST : public StyioASTTraits<FinalBindAST>
{
  VarAST* var_name = nullptr;
  StyioAST* val_expr = nullptr;

public:
  FinalBindAST(VarAST* var, StyioAST* val) :
      var_name(var), val_expr(val) {
  }

  static FinalBindAST* Create(VarAST* var, StyioAST* val) {
    return FinalBindAST::Create(var, val);
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::FinalBind;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }

  VarAST* getVar() {
    return var_name;
  }

  StyioAST* getValue() {
    return val_expr;
  }

  const string& getName() {
    return var_name->getNameAsStr();
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
public:
  NameAST* name = nullptr;
  std::vector<ParamAST*> args;

  StructAST(NameAST* name, std::vector<ParamAST*> arguments) :
      name(name), args(arguments) {
  }

  static StructAST* Create(NameAST* name, std::vector<ParamAST*> arguments) {
    return new StructAST(name, arguments);
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::Struct;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Struct, name->getAsStr(), 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::ReadFile;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }
};

/*
  PrintAST: Write to Standard Output (Print)
*/
class PrintAST : public StyioASTTraits<PrintAST>
{
private:
  vector<StyioAST*> Exprs;

  PrintAST(vector<StyioAST*> exprs) :
      Exprs(exprs) {
  }

public:
  static PrintAST* Create(vector<StyioAST*> exprs) {
    return new PrintAST(exprs);
  }

  const vector<StyioAST*>& getExprs() {
    return Exprs;
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::Print;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::ExtPack;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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
  StyioASTType WhatFlow;

  CondFlowAST(StyioASTType whatFlow, CondAST* condition, StyioAST* block) :
      WhatFlow(whatFlow), CondExpr((condition)), ThenBlock((block)) {
  }

  CondFlowAST(StyioASTType whatFlow, CondAST* condition, StyioAST* blockThen, StyioAST* blockElse) :
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

  const StyioASTType getNodeType() const {
    return WhatFlow;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::CheckEq;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::CheckIsin;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::FromTo;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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
  StyioASTType Type = StyioASTType::Forward;

public:
  ForwardAST(StyioAST* expr) :
      ThenExpr(expr) {
    Type = StyioASTType::Forward;
    if (ThenExpr->getNodeType() != StyioASTType::Block) {
      RetExpr = expr;
    }
  }

  static ForwardAST* Create(StyioAST* expr) {
    return new ForwardAST(expr);
  }

  ForwardAST(CheckEqAST* value, StyioAST* whatnext) :
      ExtraEq(value), ThenExpr(whatnext) {
    Type = StyioASTType::If_Equal_To_Forward;
  }

  static ForwardAST* Create(CheckEqAST* value, StyioAST* whatnext) {
    return new ForwardAST(value, whatnext);
  }

  ForwardAST(CheckIsinAST* isin, StyioAST* whatnext) :
      ExtraIsin(isin), ThenExpr(whatnext) {
    Type = StyioASTType::If_Is_In_Forward;
  }

  static ForwardAST* Create(CheckIsinAST* isin, StyioAST* whatnext) {
    return new ForwardAST(isin, whatnext);
  }

  ForwardAST(CasesAST* cases) :
      ThenExpr(cases) {
    Type = StyioASTType::Cases_Forward;
  }

  static ForwardAST* Create(CasesAST* cases) {
    return new ForwardAST(cases);
  }

  ForwardAST(CondFlowAST* condflow) :
      ThenCondFlow(condflow) {
    if ((condflow->WhatFlow) == StyioASTType::CondFlow_True) {
      Type = StyioASTType::If_True_Forward;
    }
    else if ((condflow->WhatFlow) == StyioASTType::CondFlow_False) {
      Type = StyioASTType::If_False_Forward;
    }
    else {
      Type = StyioASTType::If_Both_Forward;
    }
  }

  static ForwardAST* Create(CondFlowAST* condflow) {
    return new ForwardAST(condflow);
  }

  ForwardAST(
    VarTupleAST* vars,
    StyioAST* then_expr
  ) :
      Params(vars),
      ThenExpr(then_expr) {
    Type = StyioASTType::Fill_Forward;
    if (ThenExpr->getNodeType() != StyioASTType::Block) {
      RetExpr = then_expr;
    }
  }

  static ForwardAST* Create(VarTupleAST* vars, StyioAST* whatnext) {
    return new ForwardAST(vars, whatnext);
  }

  ForwardAST(
    VarTupleAST* vars,
    CheckEqAST* value,
    StyioAST* whatnext
  ) :
      Params(vars), ExtraEq(value), ThenExpr(whatnext) {
    Type = StyioASTType::Fill_If_Equal_To_Forward;
  }

  static ForwardAST* Create(
    VarTupleAST* vars,
    CheckEqAST* value,
    StyioAST* whatnext
  ) {
    return new ForwardAST(vars, value, whatnext);
  }

  ForwardAST(VarTupleAST* vars, CheckIsinAST* isin, StyioAST* whatnext) :
      Params(vars), ExtraIsin(isin), ThenExpr(whatnext) {
    Type = StyioASTType::Fill_If_Is_in_Forward;
  }

  static ForwardAST* Create(
    VarTupleAST* vars,
    CheckIsinAST* isin,
    StyioAST* whatnext
  ) {
    return new ForwardAST(vars, isin, whatnext);
  }

  ForwardAST(VarTupleAST* vars, CasesAST* cases) :
      Params(vars), ThenExpr(cases) {
    Type = StyioASTType::Fill_Cases_Forward;
  }

  static ForwardAST* Create(
    VarTupleAST* vars,
    CasesAST* cases
  ) {
    return new ForwardAST(vars, cases);
  }

  ForwardAST(VarTupleAST* vars, CondFlowAST* condflow) :
      Params(vars), ThenCondFlow(condflow) {
    switch (condflow->WhatFlow) {
      case StyioASTType::CondFlow_True:
        Type = StyioASTType::Fill_If_True_Forward;
        break;

      case StyioASTType::CondFlow_False:
        Type = StyioASTType::Fill_If_False_Forward;
        break;

      case StyioASTType::CondFlow_Both:
        Type = StyioASTType::Fill_If_Both_Forward;
        break;

      default:
        break;
    }
  }

  static ForwardAST* Create(
    VarTupleAST* vars,
    CondFlowAST* condflow
  ) {
    return new ForwardAST(vars, condflow);
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

  const StyioASTType getNodeType() const {
    return Type;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }
};

/* Backward */
class BackwardAST : public StyioASTTraits<BackwardAST>
{
private:
  BackwardAST(StyioAST* obj, VarTupleAST* params, std::vector<StyioAST*> ops, std::vector<StyioAST*> rets) :
      object(obj), params(params), operations(ops), ret_exprs(rets) {
  }

public:
  StyioAST* object = nullptr;
  VarTupleAST* params = nullptr;
  std::vector<StyioAST*> operations;
  std::vector<StyioAST*> ret_exprs;  // return-able expressions

  static BackwardAST* Create(StyioAST* obj, VarTupleAST* params, std::vector<StyioAST*> ops, std::vector<StyioAST*> rets) {
    return new BackwardAST(obj, params, ops, rets);
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::Backward;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }
};

/* Chain of Data Processing */
class CODPAST : public StyioASTTraits<CODPAST>
{
public:
  std::string OpName = "";
  vector<StyioAST*> OpArgs;
  CODPAST* PrevOp = nullptr;
  CODPAST* NextOp = nullptr;

  CODPAST(std::string op_name, vector<StyioAST*> op_body) :
      OpName(op_name), OpArgs(op_body) {
  }

  CODPAST(std::string op_name, vector<StyioAST*> op_body, CODPAST* prev_op) :
      OpName(op_name), OpArgs(op_body), PrevOp(prev_op) {
  }

  CODPAST(std::string op_name, vector<StyioAST*> op_body, CODPAST* prev_op, CODPAST* next_op) :
      OpName(op_name), OpArgs(op_body), PrevOp(prev_op), NextOp(next_op) {
  }

  static CODPAST* Create(std::string op_name, vector<StyioAST*> op_body) {
    return new CODPAST(op_name, op_body);
  }

  static CODPAST* Create(std::string op_name, vector<StyioAST*> op_body, CODPAST* prev_op) {
    return new CODPAST(op_name, op_body, prev_op);
  }

  static CODPAST* Create(std::string op_name, vector<StyioAST*> op_body, CODPAST* prev_op, CODPAST* next_op) {
    return new CODPAST(op_name, op_body, prev_op, next_op);
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::Chain_Of_Data_Processing;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::Infinite;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::MatchCases;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::AnonyFunc;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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

  const StyioASTType getNodeType() const {
    return StyioASTType::Func;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
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
  Infinite Loop: [...] >> {}
*/
class InfiniteLoopAST : public StyioASTTraits<InfiniteLoopAST>
{
private:
  ForwardAST* Forward = nullptr;

public:
  InfiniteLoopAST(ForwardAST* expr) :
      Forward(expr) {
  }

  ForwardAST* getForward() {
    return Forward;
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::Loop;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }
};

/*
  Iterator:
    collection >> operations
*/
class IteratorAST : public StyioASTTraits<IteratorAST>
{
private:
  IteratorAST(
    StyioAST* collection,
    std::vector<ParamAST*> params,
    BlockAST* block
  ) :
      collection(collection),
      params(params),
      block(block) {
  }

public:
  StyioAST* collection = nullptr;
  std::vector<ParamAST*> params;
  BlockAST* block = nullptr;

  static IteratorAST* Create(
    StyioAST* collection,
    std::vector<ParamAST*> params,
    BlockAST* block
  ) {
    return new IteratorAST(collection, params, block);
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::Iterator;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
  }
};

/*
  Extractor:
    collection << operations
*/
class ExtractorAST : public StyioASTTraits<ExtractorAST>
{
private:
  ExtractorAST(StyioAST* theTuple, StyioAST* theOpOnIt) :
      theTuple(theTuple), theOpOnIt(theOpOnIt) {
  }

public:
  StyioAST* theTuple;
  StyioAST* theOpOnIt;

  ExtractorAST* Create(StyioAST* the_tuple, StyioAST* the_op) {
    return new ExtractorAST(the_tuple, the_op);
  }

  const StyioASTType getNodeType() const {
    return StyioASTType::TupleOperation;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "TupleOp", 0};
  }
};

#endif