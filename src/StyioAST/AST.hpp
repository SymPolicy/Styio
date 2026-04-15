#pragma once
#ifndef STYIO_AST_H_
#define STYIO_AST_H_

// [C++]
#include <cstddef>
#include <memory>
#include <new>
#include <utility>
#include <unordered_set>
#include <variant>
#include <vector>

using std::vector;

// [Styio]
#include "../StyioAnalyzer/ASTAnalyzer.hpp"
#include "../StyioSession/SessionAllocation.hpp"
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

  static void*
  operator new(std::size_t sz) {
    void* mem = styio::session_alloc::allocate_ast_object(sz);
    if (!styio::session_alloc::ast_arena_active()) {
      tracked_nodes_.insert(static_cast<StyioAST*>(mem));
    }
    return mem;
  }

  static void
  operator delete(void* ptr) noexcept {
    if (ptr != nullptr) {
      tracked_nodes_.erase(static_cast<StyioAST*>(ptr));
    }
    styio::session_alloc::free_object(ptr);
  }

  static void
  destroy_all_tracked_nodes() noexcept {
    // Tracking is advisory only. Deleting every tracked node is unsafe once
    // some AST families start owning child nodes via unique_ptr.
    tracked_nodes_.clear();
  }

  static std::size_t
  tracked_node_count() noexcept {
    return tracked_nodes_.size();
  }

  /* Type Hint */
  virtual const StyioNodeType getNodeType() const = 0;

  virtual const StyioDataType getDataType() const = 0;

  /* StyioAST to String */
  virtual std::string toString(StyioRepr* visitor, int indent = 0) = 0;

  /* Type Inference */
  virtual void typeInfer(StyioAnalyzer* visitor) = 0;

  /* Code Gen. StyioIR */
  virtual StyioIR* toStyioIR(StyioAnalyzer* visitor) = 0;

private:
  inline static thread_local std::unordered_set<StyioAST*> tracked_nodes_;
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Comment;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Id;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class TypeAST : public StyioASTTraits<TypeAST>
{
public:
  StyioDataType type = StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};

  TypeAST() {}

  TypeAST(StyioDataType data_type) :
      type(data_type) {
  }

  TypeAST(
    string type_name
  ) {
    auto it = DTypeTable.find(type_name);
    if (it != DTypeTable.end()) {
      type = it->second;
    }
    else {
      type = StyioDataType{StyioDataTypeOption::Defined, type_name, 0};
    }
  }

  static TypeAST* Create() {
    return new TypeAST();
  }

  static TypeAST* Create(StyioDataType data_type) {
    return new TypeAST(data_type);
  }

  static TypeAST* Create(string type_name) {
    return new TypeAST(type_name);
  }

  /*
    Topology v2 bounded ring [|n|] in type position (name prefix bounded_ring:n).
    CodeGen: alloca [n x i64] + head cursor; reads return last written cell (see BoundedType.hpp).
  */
  static TypeAST* CreateBoundedRingBuffer(string capacity_digits) {
    auto* t = new TypeAST();
    t->type = StyioDataType{
      StyioDataTypeOption::Defined,
      std::string("bounded_ring:") + capacity_digits,
      0};
    return t;
  }

  void setType(StyioDataType new_type) {
    this->type = new_type;
  }

  string getTypeName() {
    return type.name;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::DType;
  }

  const StyioDataType getDataType() const {
    return type;
  }
};

class TypeTupleAST : public StyioASTTraits<TypeTupleAST>
{
  std::vector<std::unique_ptr<TypeAST>> type_owners_;

  void adopt_type_list(std::vector<TypeAST*> owned_types) {
    type_owners_.clear();
    type_list.clear();
    type_owners_.reserve(owned_types.size());
    type_list.reserve(owned_types.size());
    for (auto* type : owned_types) {
      type_owners_.emplace_back(type);
      type_list.push_back(type_owners_.back().get());
    }
  }

private:
  TypeTupleAST() {}

  TypeTupleAST(
    std::vector<TypeAST*> type_list
  ) :
      type_list() {
    adopt_type_list(type_list);
  }

public:
  std::vector<TypeAST*> type_list;

  static TypeTupleAST* Create() {
    return new TypeTupleAST();
  }

  static TypeTupleAST* Create(std::vector<TypeAST*> type_list) {
    return new TypeTupleAST(type_list);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::TypeTuple;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Tuple, "TypeTuple", 0};
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::None;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Empty;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Bool;
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Integer;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Integer, "int", num_of_bit};
  }
};

/*
  FloatAST: Float
*/
class FloatAST : public StyioASTTraits<FloatAST>
{
public:
  string value;
  TypeAST* data_type = TypeAST::Create(StyioDataType{StyioDataTypeOption::Float, "Float", 64});

  FloatAST(const string& value) :
      value(value) {
  }

  FloatAST(const string& value, StyioDataType type) :
      value(value) {
    data_type->type = type;
  }

  static FloatAST* Create(const string& value) {
    return new FloatAST(value);
  }

  const string& getValue() {
    return value;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Float;
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Char;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::String;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class BlockAST : public StyioASTTraits<BlockAST>
{
private:
  std::vector<std::unique_ptr<StyioAST>> stmt_owners_;
  std::vector<std::unique_ptr<StyioAST>> following_owners_;

  void adopt_stmts(vector<StyioAST*> owned_stmts) {
    stmt_owners_.clear();
    stmts.clear();
    stmt_owners_.reserve(owned_stmts.size());
    stmts.reserve(owned_stmts.size());
    for (auto* stmt : owned_stmts) {
      stmt_owners_.emplace_back(stmt);
      stmts.push_back(stmt_owners_.back().get());
    }
  }

  void adopt_followings(vector<StyioAST*> owned_followings) {
    following_owners_.clear();
    followings.clear();
    following_owners_.reserve(owned_followings.size());
    followings.reserve(owned_followings.size());
    for (auto* following : owned_followings) {
      following_owners_.emplace_back(following);
      followings.push_back(following_owners_.back().get());
    }
  }

  BlockAST() {
  }

  BlockAST(vector<StyioAST*> stmts) :
      stmts() {
    adopt_stmts(std::move(stmts));
  }

public:
  std::vector<StyioAST*> stmts;
  std::vector<StyioAST*> followings;

  static BlockAST* Create() {
    return new BlockAST();
  }

  static BlockAST* Create(vector<StyioAST*> stmts) {
    return new BlockAST(stmts);
  }

  void set_followings(vector<StyioAST*> forward_nodes) {
    adopt_followings(std::move(forward_nodes));
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Block;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class MainBlockAST : public StyioASTTraits<MainBlockAST>
{
  std::unique_ptr<StyioAST> resources_owner_;
  std::vector<std::unique_ptr<StyioAST>> stmt_owners_;
  StyioAST* Resources = nullptr;
  vector<StyioAST*> Stmts;

  void adopt_stmts(vector<StyioAST*> owned_stmts) {
    stmt_owners_.clear();
    Stmts.clear();
    stmt_owners_.reserve(owned_stmts.size());
    Stmts.reserve(owned_stmts.size());
    for (auto* stmt : owned_stmts) {
      stmt_owners_.emplace_back(stmt);
      Stmts.push_back(stmt_owners_.back().get());
    }
  }

public:
  MainBlockAST(
    StyioAST* resources,
    vector<StyioAST*> stmts
  ) :
      resources_owner_(resources),
      Resources(resources_owner_.get()) {
    adopt_stmts(std::move(stmts));
  }

  MainBlockAST(
    vector<StyioAST*> stmts
  ) :
      Stmts() {
    adopt_stmts(std::move(stmts));
  }

  static MainBlockAST* Create(
    vector<StyioAST*> stmts
  ) {
    return new MainBlockAST(stmts);
  }

  const vector<StyioAST*>& getStmts() {
    return Stmts;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::MainBlock;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::End;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class BreakAST : public StyioASTTraits<BreakAST>
{
  unsigned depth_ = 1;

public:
  explicit BreakAST(unsigned d = 1) :
      depth_(d) {
  }

  static BreakAST* Create(unsigned d = 1) {
    return new BreakAST(d);
  }

  unsigned getDepth() const {
    return depth_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Break;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class ContinueAST : public StyioASTTraits<ContinueAST>
{
  unsigned depth_ = 1;

public:
  explicit ContinueAST(unsigned d = 1) :
      depth_(d) {
  }

  static ContinueAST* Create(unsigned d = 1) {
    return new ContinueAST(d);
  }

  unsigned getDepth() const {
    return depth_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Continue;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Pass;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class ReturnAST : public StyioASTTraits<ReturnAST>
{
  std::unique_ptr<StyioAST> expr_owner_;
  StyioAST* Expr = nullptr;

public:
  ReturnAST(StyioAST* expr) :
      expr_owner_(expr), Expr(expr_owner_.get()) {
  }

  static ReturnAST* Create(StyioAST* expr) {
    return new ReturnAST(expr);
  }

  StyioAST* getExpr() {
    return Expr;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Return;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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
  std::unique_ptr<NameAST> var_name_owner_;
  std::unique_ptr<TypeAST> var_type_owner_;
  std::unique_ptr<StyioAST> val_init_owner_;

public:
  NameAST* var_name = nullptr; /* Variable Name */
  TypeAST* var_type = nullptr; /* Variable Data Type */
  StyioAST* val_init = nullptr;          /* Variable Initial Value */

  VarAST(NameAST* name) :
      var_name_owner_(name),
      var_type_owner_(TypeAST::Create()),
      var_name(var_name_owner_.get()),
      var_type(var_type_owner_.get()) {
  }

  VarAST(NameAST* name, TypeAST* data_type) :
      var_name_owner_(name),
      var_type_owner_(data_type),
      var_name(var_name_owner_.get()),
      var_type(var_type_owner_.get()) {
  }

  VarAST(NameAST* name, TypeAST* data_type, StyioAST* default_value) :
      var_name_owner_(name),
      var_type_owner_(data_type),
      val_init_owner_(default_value),
      var_name(var_name_owner_.get()),
      var_type(var_type_owner_.get()),
      val_init(val_init_owner_.get()) {
  }

  static VarAST* Create(NameAST* name) {
    return new VarAST(name);
  }

  static VarAST* Create(NameAST* name, TypeAST* data_type) {
    return new VarAST(name, data_type);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Variable;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }

  void setDataType(StyioDataType type) {
    var_type->type = type;
  }

  NameAST* getName() {
    return var_name;
  }

  const string& getNameAsStr() {
    return var_name->getAsStr();
  }

  TypeAST* getDType() {
    return var_type;
  }

  string getTypeAsStr() {
    return var_type->getTypeName();
  }

  bool isTyped() {
    return (var_type && (var_type->getDataType().option != StyioDataTypeOption::Undefined));
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
      var_name(VarAST::var_name),
      var_type(VarAST::var_type),
      val_init(VarAST::val_init) {
  }

  ParamAST(
    NameAST* name,
    TypeAST* data_type
  ) :
      VarAST(name, data_type),
      var_name(VarAST::var_name),
      var_type(VarAST::var_type),
      val_init(VarAST::val_init) {
  }

  ParamAST(
    NameAST* name,
    TypeAST* data_type,
    StyioAST* default_value
  ) :
      VarAST(name, data_type, default_value),
      var_name(VarAST::var_name),
      var_type(VarAST::var_type),
      val_init(VarAST::val_init) {
  }

public:
  using VarAST::isTyped;

  NameAST* var_name = nullptr;  /* Variable Name */
  TypeAST* var_type = nullptr;  /* Variable Data Type */
  StyioAST* val_init = nullptr; /* Variable Initial Value */

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Param;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }

  static ParamAST* Create(NameAST* name) {
    return new ParamAST(name);
  }

  static ParamAST* Create(NameAST* name, TypeAST* data_type) {
    return new ParamAST(name, data_type);
  }

  static ParamAST* Create(NameAST* name, TypeAST* data_type, StyioAST* default_value) {
    return new ParamAST(name, data_type, default_value);
  }

  const string& getName() {
    return var_name->getAsStr();
  }

  TypeAST* getDType() {
    return var_type;
  }

  void setDType(StyioDataType type) {
    return var_type->setType(type);
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::OptArg;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::OptKwArg;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

/*
  FmtStrAST: String
*/
class FmtStrAST : public StyioASTTraits<FmtStrAST>
{
  std::vector<std::unique_ptr<StyioAST>> expr_owners_;
  vector<string> Fragments;
  vector<StyioAST*> Exprs;

  void adopt_expressions(vector<StyioAST*> expressions) {
    expr_owners_.reserve(expressions.size());
    Exprs.clear();

    for (auto* expr : expressions) {
      expr_owners_.emplace_back(expr);
      Exprs.push_back(expr_owners_.back().get());
    }
  }

public:
  FmtStrAST(vector<string> fragments, vector<StyioAST*> expressions) :
      Fragments(fragments) {
    adopt_expressions(std::move(expressions));
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::FmtStr;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class TypeConvertAST : public StyioASTTraits<TypeConvertAST>
{
  std::unique_ptr<StyioAST> value_owner_;
  StyioAST* Value = nullptr;
  NumPromoTy PromoType;

public:
  TypeConvertAST(
    StyioAST* val,
    NumPromoTy promo_type
  ) :
      value_owner_(val), Value(value_owner_.get()), PromoType(promo_type) {
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::NumConvert;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::LocalPath;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::RemotePath;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::WebUrl;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::DBUrl;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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
private:
  std::vector<std::unique_ptr<StyioAST>> element_owners_;
  std::unique_ptr<TypeAST> consistent_type_owner_;

  void adopt_elements(vector<StyioAST*> elems) {
    element_owners_.reserve(elems.size());
    elements.clear();

    for (auto* elem : elems) {
      element_owners_.emplace_back(elem);
      elements.push_back(element_owners_.back().get());
    }
  }

public:
  vector<StyioAST*> elements;

  bool consistency = false;
  TypeAST* consistent_type = nullptr;

  TupleAST(vector<StyioAST*> elems) :
      consistent_type_owner_(TypeAST::Create()),
      consistent_type(consistent_type_owner_.get()) {
    adopt_elements(std::move(elems));
  }

  TupleAST(vector<VarAST*> elems) :
      consistent_type_owner_(TypeAST::Create()),
      consistent_type(consistent_type_owner_.get()) {
    vector<StyioAST*> as_exprs;
    as_exprs.reserve(elems.size());
    for (auto* elem : elems) {
      as_exprs.push_back(elem);
    }
    adopt_elements(std::move(as_exprs));
  }

  static TupleAST* Create(vector<StyioAST*> elems) {
    return new TupleAST(std::move(elems));
  }

  const vector<StyioAST*>& getElements() {
    return elements;
  }

  TypeAST* getDTypeObj() {
    return consistent_type;
  }

  void setConsistency(bool value) {
    consistency = value;
  }

  bool isConsistent() {
    return consistency;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Tuple;
  }

  const StyioDataType getDataType() const {
    return consistent_type->getDataType();
  }

  void setDataType(StyioDataType type) {
    consistent_type->setType(type);
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Parameters;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

/*
  ListAST: List (Extendable)
*/
class ListAST : public StyioASTTraits<ListAST>
{
  std::vector<std::unique_ptr<StyioAST>> element_owners_;
  std::unique_ptr<TypeAST> consistent_type_owner_;

  void adopt_elements(vector<StyioAST*> elems) {
    element_owners_.reserve(elems.size());
    elements_.clear();

    for (auto* elem : elems) {
      element_owners_.emplace_back(elem);
      elements_.push_back(element_owners_.back().get());
    }
  }

  vector<StyioAST*> elements_;
  bool consistency = false;
  TypeAST* consistent_type = nullptr;

public:
  ListAST() :
      consistent_type_owner_(TypeAST::Create()),
      consistent_type(consistent_type_owner_.get()) {
  }

  ListAST(vector<StyioAST*> elems) :
      consistent_type_owner_(TypeAST::Create()),
      consistent_type(consistent_type_owner_.get()) {
    adopt_elements(std::move(elems));
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

  TypeAST* getDTypeObj() {
    return consistent_type;
  }

  void setConsistency(bool value) {
    consistency = value;
  }

  bool isConsistent() {
    return consistency;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::List;
  }

  const StyioDataType getDataType() const {
    return consistent_type->getDataType();
  }

  void setDataType(StyioDataType type) {
    consistent_type->setType(type);
  }
};

class DictAST : public StyioASTTraits<DictAST>
{
public:
  struct EntryView
  {
    StyioAST* key = nullptr;
    StyioAST* value = nullptr;
  };

private:
  std::vector<std::unique_ptr<StyioAST>> key_owners_;
  std::vector<std::unique_ptr<StyioAST>> value_owners_;
  std::vector<EntryView> entries_;
  std::unique_ptr<TypeAST> consistent_type_owner_;
  bool consistency = false;
  TypeAST* consistent_type = nullptr;

  void adopt_entries(std::vector<std::pair<StyioAST*, StyioAST*>> entries) {
    key_owners_.clear();
    value_owners_.clear();
    entries_.clear();
    key_owners_.reserve(entries.size());
    value_owners_.reserve(entries.size());
    entries_.reserve(entries.size());

    for (auto& entry : entries) {
      key_owners_.emplace_back(entry.first);
      value_owners_.emplace_back(entry.second);
      entries_.push_back(EntryView{
        key_owners_.back().get(),
        value_owners_.back().get()});
    }
  }

public:
  DictAST() :
      consistent_type_owner_(TypeAST::Create()),
      consistent_type(consistent_type_owner_.get()) {
  }

  explicit DictAST(std::vector<std::pair<StyioAST*, StyioAST*>> entries) :
      consistent_type_owner_(TypeAST::Create()),
      consistent_type(consistent_type_owner_.get()) {
    adopt_entries(std::move(entries));
  }

  static DictAST* Create() {
    return new DictAST();
  }

  static DictAST* Create(std::vector<std::pair<StyioAST*, StyioAST*>> entries) {
    return new DictAST(std::move(entries));
  }

  const std::vector<EntryView>& getEntries() {
    return entries_;
  }

  TypeAST* getDTypeObj() {
    return consistent_type;
  }

  void setConsistency(bool value) {
    consistency = value;
  }

  bool isConsistent() {
    return consistency;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Dict;
  }

  const StyioDataType getDataType() const {
    return consistent_type->getDataType();
  }

  void setDataType(StyioDataType type) {
    consistent_type->setType(type);
  }
};

class SetAST : public StyioASTTraits<SetAST>
{
  std::vector<std::unique_ptr<StyioAST>> element_owners_;

  void adopt_elements(vector<StyioAST*> elems) {
    element_owners_.reserve(elems.size());
    elements_.clear();

    for (auto* elem : elems) {
      element_owners_.emplace_back(elem);
      elements_.push_back(element_owners_.back().get());
    }
  }

  vector<StyioAST*> elements_;

public:
  SetAST(vector<StyioAST*> elems) :
      elements_() {
    adopt_elements(std::move(elems));
  }

  static SetAST* Create(vector<StyioAST*> elems) {
    return new SetAST(std::move(elems));
  }

  const vector<StyioAST*>& getElements() {
    return elements_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Set;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

/*
  RangeAST: Loop
*/
class RangeAST : public StyioASTTraits<RangeAST>
{
  std::unique_ptr<StyioAST> start_owner_;
  std::unique_ptr<StyioAST> end_owner_;
  std::unique_ptr<StyioAST> step_owner_;

  StyioAST* StartVal = nullptr;
  StyioAST* EndVal = nullptr;
  StyioAST* StepVal = nullptr;

public:
  RangeAST(StyioAST* start, StyioAST* end, StyioAST* step) :
      start_owner_(start),
      end_owner_(end),
      step_owner_(step),
      StartVal(start_owner_.get()),
      EndVal(end_owner_.get()),
      StepVal(step_owner_.get()) {
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Range;
  }

  const StyioDataType getDataType() const {
    return styio_make_range_type("i64");
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
  std::unique_ptr<StyioAST> value_owner_;
  StyioAST* Value = nullptr;

public:
  SizeOfAST(
    StyioAST* value
  ) :
      value_owner_(value), Value(value_owner_.get()) {
  }

  StyioAST* getValue() {
    return Value;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::SizeOf;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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
private:
  std::unique_ptr<TypeAST> data_type_owner_;
  std::unique_ptr<StyioAST> lhs_owner_;
  std::unique_ptr<StyioAST> rhs_owner_;

public:
  TypeAST* data_type = nullptr;

  StyioOpType operand;
  StyioAST* LHS = nullptr;
  StyioAST* RHS = nullptr;

  BinOpAST(StyioOpType op, StyioAST* lhs, StyioAST* rhs) :
      data_type_owner_(TypeAST::Create()),
      lhs_owner_(lhs),
      rhs_owner_(rhs),
      operand(op),
      LHS(lhs_owner_.get()),
      RHS(rhs_owner_.get()) {
    data_type = data_type_owner_.get();
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
    return data_type->type;
  }

  void setDType(StyioDataType type) {
    data_type->setType(type);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::BinOp;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class BinCompAST : public StyioASTTraits<BinCompAST>
{
  CompType CompSign;
  std::unique_ptr<StyioAST> lhs_owner_;
  std::unique_ptr<StyioAST> rhs_owner_;
  StyioAST* LhsExpr = nullptr;
  StyioAST* RhsExpr = nullptr;

public:
  BinCompAST(CompType sign, StyioAST* lhs, StyioAST* rhs) :
      CompSign(sign),
      lhs_owner_(lhs),
      rhs_owner_(rhs),
      LhsExpr(lhs_owner_.get()),
      RhsExpr(rhs_owner_.get()) {
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Compare;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

/* M4: algebraic absence @ and wave / fallback */
class UndefinedLitAST : public StyioASTTraits<UndefinedLitAST>
{
  UndefinedLitAST() = default;

public:
  static UndefinedLitAST* Create() {
    return new UndefinedLitAST();
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::UndefLiteral;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class WaveMergeAST : public StyioASTTraits<WaveMergeAST>
{
  std::unique_ptr<StyioAST> cond_owner_;
  std::unique_ptr<StyioAST> true_owner_;
  std::unique_ptr<StyioAST> false_owner_;
  StyioAST* cond_ = nullptr;
  StyioAST* true_val_ = nullptr;
  StyioAST* false_val_ = nullptr;

  WaveMergeAST(StyioAST* c, StyioAST* t, StyioAST* f) :
      cond_owner_(c),
      true_owner_(t),
      false_owner_(f),
      cond_(cond_owner_.get()),
      true_val_(true_owner_.get()),
      false_val_(false_owner_.get()) {
  }

public:
  static WaveMergeAST* Create(StyioAST* c, StyioAST* t, StyioAST* f) {
    return new WaveMergeAST(c, t, f);
  }

  StyioAST* getCond() {
    return cond_;
  }
  StyioAST* getTrueVal() {
    return true_val_;
  }
  StyioAST* getFalseVal() {
    return false_val_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::WaveMerge;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class WaveDispatchAST : public StyioASTTraits<WaveDispatchAST>
{
  std::unique_ptr<StyioAST> cond_owner_;
  std::unique_ptr<StyioAST> true_owner_;
  std::unique_ptr<StyioAST> false_owner_;
  StyioAST* cond_ = nullptr;
  StyioAST* true_arm_ = nullptr;
  StyioAST* false_arm_ = nullptr;

  WaveDispatchAST(StyioAST* c, StyioAST* t, StyioAST* f) :
      cond_owner_(c),
      true_owner_(t),
      false_owner_(f),
      cond_(cond_owner_.get()),
      true_arm_(true_owner_.get()),
      false_arm_(false_owner_.get()) {
  }

public:
  static WaveDispatchAST* Create(StyioAST* c, StyioAST* t, StyioAST* f) {
    return new WaveDispatchAST(c, t, f);
  }

  StyioAST* getCond() {
    return cond_;
  }
  StyioAST* getTrueArm() {
    return true_arm_;
  }
  StyioAST* getFalseArm() {
    return false_arm_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::WaveDispatch;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class FallbackAST : public StyioASTTraits<FallbackAST>
{
  std::unique_ptr<StyioAST> primary_owner_;
  std::unique_ptr<StyioAST> alternate_owner_;
  StyioAST* primary_ = nullptr;
  StyioAST* alt_ = nullptr;

  FallbackAST(StyioAST* p, StyioAST* a) :
      primary_owner_(p),
      alternate_owner_(a),
      primary_(primary_owner_.get()),
      alt_(alternate_owner_.get()) {
  }

public:
  static FallbackAST* Create(StyioAST* p, StyioAST* a) {
    return new FallbackAST(p, a);
  }

  StyioAST* getPrimary() {
    return primary_;
  }
  StyioAST* getAlternate() {
    return alt_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Fallback;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class GuardSelectorAST : public StyioASTTraits<GuardSelectorAST>
{
  std::unique_ptr<StyioAST> base_owner_;
  std::unique_ptr<StyioAST> cond_owner_;
  StyioAST* base_ = nullptr;
  StyioAST* cond_ = nullptr;

  GuardSelectorAST(StyioAST* b, StyioAST* c) :
      base_owner_(b),
      cond_owner_(c),
      base_(base_owner_.get()),
      cond_(cond_owner_.get()) {
  }

public:
  static GuardSelectorAST* Create(StyioAST* b, StyioAST* c) {
    return new GuardSelectorAST(b, c);
  }

  StyioAST* getBase() {
    return base_;
  }
  StyioAST* getCond() {
    return cond_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::GuardSelector;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class EqProbeAST : public StyioASTTraits<EqProbeAST>
{
  std::unique_ptr<StyioAST> base_owner_;
  std::unique_ptr<StyioAST> probe_owner_;
  StyioAST* base_ = nullptr;
  StyioAST* probe_val_ = nullptr;

  EqProbeAST(StyioAST* b, StyioAST* v) :
      base_owner_(b),
      probe_owner_(v),
      base_(base_owner_.get()),
      probe_val_(probe_owner_.get()) {
  }

public:
  static EqProbeAST* Create(StyioAST* b, StyioAST* v) {
    return new EqProbeAST(b, v);
  }

  StyioAST* getBase() {
    return base_;
  }
  StyioAST* getProbeValue() {
    return probe_val_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::EqProbeSelector;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class CondAST : public StyioASTTraits<CondAST>
{
  LogicType LogicOp;
  std::unique_ptr<StyioAST> value_owner_;
  std::unique_ptr<StyioAST> lhs_owner_;
  std::unique_ptr<StyioAST> rhs_owner_;

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
      LogicOp(op),
      value_owner_(val),
      ValExpr(value_owner_.get()) {
  }

  CondAST(LogicType op, StyioAST* lhs, StyioAST* rhs) :
      LogicOp(op),
      lhs_owner_(lhs),
      rhs_owner_(rhs),
      LhsExpr(lhs_owner_.get()),
      RhsExpr(rhs_owner_.get()) {
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Condition;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class FuncCallAST : public StyioASTTraits<FuncCallAST>
{
private:
  std::unique_ptr<StyioAST> func_callee_owner_;
  std::unique_ptr<NameAST> func_name_owner_;
  std::vector<std::unique_ptr<StyioAST>> func_arg_owners_;

  void adopt_arguments(vector<StyioAST*> arguments) {
    func_arg_owners_.reserve(arguments.size());
    func_args.clear();

    for (auto* arg : arguments) {
      func_arg_owners_.emplace_back(arg);
      func_args.push_back(func_arg_owners_.back().get());
    }
  }

public:
  StyioAST* func_callee = nullptr;
  NameAST* func_name = nullptr;
  vector<StyioAST*> func_args;

  FuncCallAST(
    NameAST* func_name,
    vector<StyioAST*> arguments
  ) :
      func_name_owner_(func_name),
      func_name(func_name_owner_.get()) {
    adopt_arguments(std::move(arguments));
  }

  FuncCallAST(
    StyioAST* func_callee,
    NameAST* func_name,
    vector<StyioAST*> arguments
  ) :
      func_callee_owner_(func_callee),
      func_name_owner_(func_name),
      func_callee(func_callee_owner_.get()),
      func_name(func_name_owner_.get()) {
    adopt_arguments(std::move(arguments));
  }

  static FuncCallAST* Create(
    NameAST* func_name,
    vector<StyioAST*> arguments
  ) {
    return new FuncCallAST(func_name, arguments);
  }

  static FuncCallAST* Create(
    StyioAST* func_callee,
    NameAST* func_name,
    vector<StyioAST*> arguments
  ) {
    return new FuncCallAST(func_callee, func_name, arguments);
  }

  void setFuncCallee(StyioAST* callee) {
    func_callee_owner_.reset(callee);
    func_callee = func_callee_owner_.get();
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Call;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class ListOpAST : public StyioASTTraits<ListOpAST>
{
  std::unique_ptr<StyioAST> list_owner_;
  std::unique_ptr<StyioAST> slot1_owner_;
  std::unique_ptr<StyioAST> slot2_owner_;

  StyioNodeType OpType;
  StyioAST* TheList = nullptr;

  StyioAST* Slot1 = nullptr;
  StyioAST* Slot2 = nullptr;

public:
  /*
    Get_Reversed
      [<]
  */
  ListOpAST(StyioNodeType opType, StyioAST* theList) :
      list_owner_(theList),
      OpType(opType),
      TheList(list_owner_.get()) {
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
  ListOpAST(StyioNodeType opType, StyioAST* theList, StyioAST* item) :
      list_owner_(theList),
      slot1_owner_(item),
      OpType(opType),
      TheList(list_owner_.get()),
      Slot1(slot1_owner_.get()) {
  }

  /*
    Insert_Item_By_Index
      [+: index <- value]
  */
  ListOpAST(StyioNodeType opType, StyioAST* theList, StyioAST* index, StyioAST* value) :
      list_owner_(theList),
      slot1_owner_(index),
      slot2_owner_(value),
      OpType(opType),
      TheList(list_owner_.get()),
      Slot1(slot1_owner_.get()),
      Slot2(slot2_owner_.get()) {
  }

  StyioNodeType getOp() {
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

  const StyioNodeType getNodeType() const {
    return OpType;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class AttrAST : public StyioASTTraits<AttrAST>
{
private:
  std::unique_ptr<StyioAST> body_owner_;
  std::unique_ptr<StyioAST> attr_owner_;

public:
  StyioAST* body = nullptr;
  StyioAST* attr = nullptr;

  AttrAST(
    StyioAST* body,
    StyioAST* attr
  ) :
      body_owner_(body),
      attr_owner_(attr),
      body(body_owner_.get()),
      attr(attr_owner_.get()) {
  }

  static AttrAST* Create(StyioAST* body, StyioAST* attr) {
    return new AttrAST(body, attr);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Attribute;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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
  std::vector<std::pair<std::unique_ptr<StyioAST>, std::string>> resource_owners_;

  void adopt_resources(std::vector<std::pair<StyioAST*, std::string>> resources) {
    resource_owners_.reserve(resources.size());
    res_list.clear();

    for (auto& entry : resources) {
      resource_owners_.emplace_back(std::unique_ptr<StyioAST>(entry.first), entry.second);
      res_list.emplace_back(resource_owners_.back().first.get(), resource_owners_.back().second);
    }
  }

  ResourceAST(std::vector<std::pair<StyioAST*, std::string>> res_list) :
      res_list() {
    adopt_resources(std::move(res_list));
  }

public:
  std::vector<std::pair<StyioAST*, std::string>> res_list;

  static ResourceAST* Create(std::vector<std::pair<StyioAST*, std::string>> resources_with_types) {
    return new ResourceAST(resources_with_types);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Resources;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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
  std::unique_ptr<VarAST> variable_owner_;
  std::unique_ptr<StyioAST> value_owner_;

  VarAST* variable = nullptr;
  StyioAST* value = nullptr;

public:
  FlexBindAST(VarAST* variable, StyioAST* value) :
      variable_owner_(variable),
      value_owner_(value),
      variable(variable_owner_.get()),
      value(value_owner_.get()) {
  }

  static FlexBindAST* Create(VarAST* variable, StyioAST* value) {
    return new FlexBindAST(variable, value);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::MutBind;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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
  std::unique_ptr<VarAST> var_owner_;
  std::unique_ptr<StyioAST> value_owner_;

  VarAST* var_name = nullptr;
  StyioAST* val_expr = nullptr;

public:
  FinalBindAST(VarAST* var, StyioAST* val) :
      var_owner_(var),
      value_owner_(val),
      var_name(var_owner_.get()),
      val_expr(value_owner_.get()) {
  }

  static FinalBindAST* Create(VarAST* var, StyioAST* val) {
    return new FinalBindAST(var, val);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::FinalBind;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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

class ParallelAssignAST : public StyioASTTraits<ParallelAssignAST>
{
  std::vector<std::unique_ptr<StyioAST>> lhs_owners_;
  std::vector<std::unique_ptr<StyioAST>> rhs_owners_;
  std::vector<StyioAST*> lhs_;
  std::vector<StyioAST*> rhs_;

  static void adopt_exprs(
    std::vector<StyioAST*> input,
    std::vector<std::unique_ptr<StyioAST>>& owners,
    std::vector<StyioAST*>& views
  ) {
    owners.clear();
    views.clear();
    owners.reserve(input.size());
    views.reserve(input.size());
    for (auto* expr : input) {
      owners.emplace_back(expr);
      views.push_back(owners.back().get());
    }
  }

  ParallelAssignAST(std::vector<StyioAST*> lhs, std::vector<StyioAST*> rhs) :
      lhs_owners_(),
      rhs_owners_(),
      lhs_(),
      rhs_() {
    adopt_exprs(std::move(lhs), lhs_owners_, lhs_);
    adopt_exprs(std::move(rhs), rhs_owners_, rhs_);
  }

public:
  static ParallelAssignAST* Create(std::vector<StyioAST*> lhs, std::vector<StyioAST*> rhs) {
    return new ParallelAssignAST(std::move(lhs), std::move(rhs));
  }

  const std::vector<StyioAST*>& getLHS() const {
    return lhs_;
  }

  const std::vector<StyioAST*>& getRHS() const {
    return rhs_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::ParallelAssign;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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
private:
  std::unique_ptr<NameAST> name_owner_;
  std::vector<std::unique_ptr<ParamAST>> arg_owners_;

  void adopt_args(std::vector<ParamAST*> arguments) {
    arg_owners_.reserve(arguments.size());
    args.clear();

    for (auto* arg : arguments) {
      arg_owners_.emplace_back(arg);
      args.push_back(arg_owners_.back().get());
    }
  }

public:
  NameAST* name = nullptr;
  std::vector<ParamAST*> args;

  StructAST(NameAST* name, std::vector<ParamAST*> arguments) :
      name_owner_(name),
      name(name_owner_.get()) {
    adopt_args(std::move(arguments));
  }

  static StructAST* Create(NameAST* name, std::vector<ParamAST*> arguments) {
    return new StructAST(name, arguments);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Struct;
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
  std::unique_ptr<NameAST> var_owner_;
  std::unique_ptr<StyioAST> value_owner_;

  NameAST* varId = nullptr;
  StyioAST* valExpr = nullptr;

public:
  ReadFileAST(NameAST* var, StyioAST* val) :
      var_owner_(var),
      value_owner_(val),
      varId(var_owner_.get()),
      valExpr(value_owner_.get()) {
  }

  NameAST* getId() {
    return varId;
  }

  StyioAST* getValue() {
    return valExpr;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::ReadFile;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

/*
  M5: @file{"path"} or @{"path"} (auto)
*/
class FileResourceAST : public StyioASTTraits<FileResourceAST>
{
  std::unique_ptr<StyioAST> path_owner_;
  StyioAST* path_expr_ = nullptr;
  bool auto_detect_ = false;

  FileResourceAST(StyioAST* path, bool auto_det) :
      path_owner_(path),
      path_expr_(path_owner_.get()),
      auto_detect_(auto_det) {
  }

public:
  static FileResourceAST* Create(StyioAST* path, bool auto_detect) {
    return new FileResourceAST(path, auto_detect);
  }

  StyioAST* getPath() {
    return path_expr_;
  }

  bool isAutoDetect() const {
    return auto_detect_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::FileResource;
  }

  const StyioDataType getDataType() const {
    return styio_make_file_handle_type("i64");
  }
};

/*
  M9: @stdout, @stderr, @stdin
*/
class StdStreamAST : public StyioASTTraits<StdStreamAST>
{
  StdStreamKind kind_;

  explicit StdStreamAST(StdStreamKind k) :
      kind_(k) {
  }

public:
  static StdStreamAST* Create(StdStreamKind k) {
    return new StdStreamAST(k);
  }

  StdStreamKind getStreamKind() const {
    return kind_;
  }

  const StyioNodeType getNodeType() const {
    switch (kind_) {
      case StdStreamKind::Stdin:  return StyioNodeType::StdinResource;
      case StdStreamKind::Stdout: return StyioNodeType::StdoutResource;
      case StdStreamKind::Stderr: return StyioNodeType::StderrResource;
    }
    return StyioNodeType::StdoutResource;
  }

  const StyioDataType getDataType() const {
    return styio_make_std_stream_type(kind_, "string");
  }
};

class HandleAcquireAST : public StyioASTTraits<HandleAcquireAST>
{
public:
  enum class BindMode
  {
    Final,
    Flex,
  };

private:
  std::unique_ptr<VarAST> var_owner_;
  std::unique_ptr<StyioAST> resource_owner_;

  VarAST* var_ = nullptr;
  StyioAST* resource_ = nullptr;
  BindMode bind_mode_ = BindMode::Final;

  HandleAcquireAST(VarAST* v, StyioAST* r, BindMode mode) :
      var_owner_(v),
      resource_owner_(r),
      var_(var_owner_.get()),
      resource_(resource_owner_.get()),
      bind_mode_(mode) {
  }

public:
  static HandleAcquireAST* Create(VarAST* v, StyioAST* r, BindMode mode = BindMode::Final) {
    return new HandleAcquireAST(v, r, mode);
  }

  VarAST* getVar() {
    return var_;
  }

  StyioAST* getResource() {
    return resource_;
  }

  BindMode getBindMode() const {
    return bind_mode_;
  }

  bool isFlexBind() const {
    return bind_mode_ == BindMode::Flex;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::HandleAcquire;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class ResourceWriteAST : public StyioASTTraits<ResourceWriteAST>
{
  std::unique_ptr<StyioAST> data_owner_;
  std::unique_ptr<StyioAST> resource_owner_;

  StyioAST* data_ = nullptr;
  StyioAST* resource_ = nullptr;

  ResourceWriteAST(StyioAST* d, StyioAST* r) :
      data_owner_(d),
      resource_owner_(r),
      data_(data_owner_.get()),
      resource_(resource_owner_.get()) {
  }

public:
  static ResourceWriteAST* Create(StyioAST* d, StyioAST* r) {
    return new ResourceWriteAST(d, r);
  }

  StyioAST* getData() {
    return data_;
  }

  StyioAST* getResource() {
    return resource_;
  }

  StyioAST* release_data_latest() {
    data_ = nullptr;
    return data_owner_.release();
  }

  StyioAST* release_resource_latest() {
    resource_ = nullptr;
    return resource_owner_.release();
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::ResourceWrite;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class ResourceRedirectAST : public StyioASTTraits<ResourceRedirectAST>
{
  std::unique_ptr<StyioAST> data_owner_;
  std::unique_ptr<StyioAST> resource_owner_;

  StyioAST* data_ = nullptr;
  StyioAST* resource_ = nullptr;

  ResourceRedirectAST(StyioAST* d, StyioAST* r) :
      data_owner_(d),
      resource_owner_(r),
      data_(data_owner_.get()),
      resource_(resource_owner_.get()) {
  }

public:
  static ResourceRedirectAST* Create(StyioAST* d, StyioAST* r) {
    return new ResourceRedirectAST(d, r);
  }

  StyioAST* getData() {
    return data_;
  }

  StyioAST* getResource() {
    return resource_;
  }

  StyioAST* release_data_latest() {
    data_ = nullptr;
    return data_owner_.release();
  }

  StyioAST* release_resource_latest() {
    resource_ = nullptr;
    return resource_owner_.release();
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::ResourceRedirect;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

/* M6: pulse state ledger */
class StateRefAST : public StyioASTTraits<StateRefAST>
{
  std::unique_ptr<NameAST> name_owner_;
  NameAST* name_ = nullptr;

  explicit StateRefAST(NameAST* n) :
      name_owner_(n),
      name_(name_owner_.get()) {
  }

public:
  static StateRefAST* Create(NameAST* n) {
    return new StateRefAST(n);
  }

  NameAST* getName() const {
    return name_;
  }

  std::string getNameStr() const {
    return name_->getAsStr();
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::StateRef;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  }
};

class HistoryProbeAST : public StyioASTTraits<HistoryProbeAST>
{
  std::unique_ptr<StateRefAST> target_owner_;
  std::unique_ptr<StyioAST> depth_owner_;
  StateRefAST* target_ = nullptr;
  StyioAST* depth_ = nullptr;

  HistoryProbeAST(StateRefAST* t, StyioAST* d) :
      target_owner_(t),
      depth_owner_(d),
      target_(target_owner_.get()),
      depth_(depth_owner_.get()) {
  }

public:
  static HistoryProbeAST* Create(StateRefAST* t, StyioAST* d) {
    return new HistoryProbeAST(t, d);
  }

  StateRefAST* getTarget() const {
    return target_;
  }

  StyioAST* getDepth() const {
    return depth_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::HistoryProbe;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  }
};

enum class SeriesIntrinsicOp
{
  Avg,
  Max,
};

class SeriesIntrinsicAST : public StyioASTTraits<SeriesIntrinsicAST>
{
  std::unique_ptr<StyioAST> base_owner_;
  StyioAST* base_ = nullptr;
  SeriesIntrinsicOp op_ = SeriesIntrinsicOp::Avg;
  std::unique_ptr<StyioAST> window_owner_;
  StyioAST* window_ = nullptr;

  SeriesIntrinsicAST(StyioAST* b, SeriesIntrinsicOp o, StyioAST* w) :
      base_owner_(b),
      base_(base_owner_.get()),
      op_(o),
      window_owner_(w),
      window_(window_owner_.get()) {
  }

public:
  static SeriesIntrinsicAST* Create(StyioAST* b, SeriesIntrinsicOp o, StyioAST* w) {
    return new SeriesIntrinsicAST(b, o, w);
  }

  StyioAST* getBase() const {
    return base_;
  }

  SeriesIntrinsicOp getOp() const {
    return op_;
  }

  StyioAST* getWindow() const {
    return window_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::SeriesIntrinsic;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  }
};

/*
  @[window | name = init](export = expr)
*/
class StateDeclAST : public StyioASTTraits<StateDeclAST>
{
  std::unique_ptr<IntAST> window_header_owner_;
  std::unique_ptr<NameAST> acc_name_owner_;
  std::unique_ptr<StyioAST> acc_init_owner_;
  std::unique_ptr<VarAST> export_var_owner_;
  std::unique_ptr<StyioAST> update_expr_owner_;

  /* window-only header: e.g. @[3] */
  IntAST* window_header_ = nullptr;
  /* accumulator: @[total = 0] */
  NameAST* acc_name_ = nullptr;
  StyioAST* acc_init_ = nullptr;
  /* (export = rhs) */
  VarAST* export_var_ = nullptr;
  StyioAST* update_expr_ = nullptr;

  StateDeclAST(
    IntAST* wh,
    NameAST* an,
    StyioAST* ai,
    VarAST* ev,
    StyioAST* upd
  ) :
      window_header_owner_(wh),
      acc_name_owner_(an),
      acc_init_owner_(ai),
      export_var_owner_(ev),
      update_expr_owner_(upd),
      window_header_(window_header_owner_.get()),
      acc_name_(acc_name_owner_.get()),
      acc_init_(acc_init_owner_.get()),
      export_var_(export_var_owner_.get()),
      update_expr_(update_expr_owner_.get()) {
  }

public:
  static StateDeclAST* Create(
    IntAST* wh,
    NameAST* an,
    StyioAST* ai,
    VarAST* ev,
    StyioAST* upd
  ) {
    return new StateDeclAST(wh, an, ai, ev, upd);
  }

  IntAST* getWindowHeader() const {
    return window_header_;
  }

  NameAST* getAccName() const {
    return acc_name_;
  }

  StyioAST* getAccInit() const {
    return acc_init_;
  }

  VarAST* getExportVar() const {
    return export_var_;
  }

  StyioAST* getUpdateExpr() const {
    return update_expr_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::StateDecl;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

/*
  PrintAST: Write to Standard Output (Print)
*/
class PrintAST : public StyioASTTraits<PrintAST>
{
private:
  std::vector<std::unique_ptr<StyioAST>> expr_owners_;

  void adopt_exprs(std::vector<StyioAST*> owned_exprs) {
    expr_owners_.clear();
    exprs.clear();
    expr_owners_.reserve(owned_exprs.size());
    exprs.reserve(owned_exprs.size());
    for (auto* expr : owned_exprs) {
      expr_owners_.emplace_back(expr);
      exprs.push_back(expr_owners_.back().get());
    }
  }

  explicit PrintAST(vector<StyioAST*> owned_exprs) {
    adopt_exprs(std::move(owned_exprs));
  }

public:
  vector<StyioAST*> exprs;

  static PrintAST* Create(vector<StyioAST*> exprs) {
    return new PrintAST(exprs);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Print;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::ExtPack;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

/*
  =================
    Abstract Level: Block
  =================
*/

class CondFlowAST : public StyioASTTraits<CondFlowAST>
{
private:
  std::unique_ptr<CondAST> cond_owner_;
  std::unique_ptr<StyioAST> then_owner_;
  std::unique_ptr<StyioAST> else_owner_;

  CondAST* CondExpr = nullptr;
  StyioAST* ThenBlock = nullptr;
  StyioAST* ElseBlock = nullptr;

public:
  StyioNodeType WhatFlow;

  CondFlowAST(StyioNodeType whatFlow, CondAST* condition, StyioAST* block) :
      cond_owner_(condition),
      then_owner_(block),
      CondExpr(cond_owner_.get()),
      ThenBlock(then_owner_.get()),
      WhatFlow(whatFlow) {
  }

  CondFlowAST(StyioNodeType whatFlow, CondAST* condition, StyioAST* blockThen, StyioAST* blockElse) :
      cond_owner_(condition),
      then_owner_(blockThen),
      else_owner_(blockElse),
      CondExpr(cond_owner_.get()),
      ThenBlock(then_owner_.get()),
      ElseBlock(else_owner_.get()),
      WhatFlow(whatFlow) {
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

  const StyioNodeType getNodeType() const {
    return WhatFlow;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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

  MatchValueAST: Fill + CheckEq + Block
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

class CasesAST : public StyioASTTraits<CasesAST>
{
private:
  std::vector<std::pair<std::unique_ptr<StyioAST>, std::unique_ptr<StyioAST>>> case_owners_;
  std::unique_ptr<StyioAST> default_owner_;

  void adopt_cases(std::vector<std::pair<StyioAST*, StyioAST*>> cases) {
    case_owners_.reserve(cases.size());
    case_list.clear();

    for (auto& entry : cases) {
      case_owners_.emplace_back(
        std::unique_ptr<StyioAST>(entry.first),
        std::unique_ptr<StyioAST>(entry.second));
      case_list.emplace_back(case_owners_.back().first.get(), case_owners_.back().second.get());
    }
  }

public:
  std::vector<std::pair<StyioAST*, StyioAST*>> case_list;
  StyioAST* case_default = nullptr;

  CasesAST(StyioAST* expr) :
      default_owner_(expr),
      case_default(default_owner_.get()) {
  }

  CasesAST(std::vector<std::pair<StyioAST*, StyioAST*>> cases, StyioAST* expr) :
      default_owner_(expr),
      case_default(default_owner_.get()) {
    adopt_cases(std::move(cases));
  }

  static CasesAST* Create(StyioAST* expr) {
    return new CasesAST(expr);
  }

  static CasesAST* Create(std::vector<std::pair<StyioAST*, StyioAST*>> cases, StyioAST* expr) {
    return new CasesAST(cases, expr);
  }

  const std::vector<std::pair<StyioAST*, StyioAST*>>& getCases() {
    return case_list;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Cases;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

/*
  MatchCases
*/
class MatchCasesAST : public StyioASTTraits<MatchCasesAST>
{
  std::unique_ptr<StyioAST> value_owner_;
  std::unique_ptr<CasesAST> cases_owner_;

  StyioAST* Value = nullptr;
  CasesAST* Cases = nullptr;

public:
  /* v ?= { _ => ... } */
  MatchCasesAST(StyioAST* value, CasesAST* cases) :
      value_owner_(value),
      cases_owner_(cases),
      Value(value_owner_.get()),
      Cases(cases_owner_.get()) {
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::MatchCases;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }

  static MatchCasesAST* make(StyioAST* value, CasesAST* cases) {
    return new MatchCasesAST(value, cases);
  }

  StyioAST* getScrutinee() const {
    return Value;
  }

  CasesAST* getCases() const {
    return Cases;
  }
};

/* Match a Value Expression to See if they equal */
class CheckEqualAST : public StyioASTTraits<CheckEqualAST>
{
  std::vector<std::unique_ptr<StyioAST>> right_value_owners_;

  void adopt_right_values(std::vector<StyioAST*> values) {
    right_value_owners_.clear();
    right_values.clear();
    right_value_owners_.reserve(values.size());
    right_values.reserve(values.size());
    for (auto* value : values) {
      right_value_owners_.emplace_back(value);
      right_values.push_back(right_value_owners_.back().get());
    }
  }

public:
  std::vector<StyioAST*> right_values;

  CheckEqualAST(
    std::vector<StyioAST*> right
  ) :
      right_values() {
    adopt_right_values(right);
  }

  static CheckEqualAST* Create(
    std::vector<StyioAST*> values
  ) {
    return new CheckEqualAST(values);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::CheckEq;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class CheckIsinAST : public StyioASTTraits<CheckIsinAST>
{
  std::unique_ptr<StyioAST> iterable_owner_;
  StyioAST* Iterable = nullptr;

public:
  CheckIsinAST(
    StyioAST* value
  ) :
      iterable_owner_(value),
      Iterable(iterable_owner_.get()) {
  }

  StyioAST* getIterable() {
    return Iterable;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::CheckIsin;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class HashTagNameAST : public StyioASTTraits<HashTagNameAST>
{
private:
  HashTagNameAST(std::vector<std::string> words) :
      words(words) {
  }

public:
  std::vector<std::string> words;

  static HashTagNameAST* Create(std::vector<std::string> words) {
    return new HashTagNameAST(words);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::HashTagName;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::String, "#name", 0};
  }
};

/*
  ExtraEq:
    ?= Expr => Block

  ExtraIsIn:
    ?^ Expr => Block

  next_expr:
    => Block

  ThenCondFlow:
    ?(Expr) \t\ Block \f\ Block
*/

class ForwardAST : public StyioASTTraits<ForwardAST>
{
  std::vector<std::unique_ptr<ParamAST>> params_owners_;
  std::vector<ParamAST*> params;
  std::unique_ptr<BlockAST> block_owner_;
  BlockAST* block = nullptr;

  std::unique_ptr<CheckEqualAST> extra_eq_owner_;
  CheckEqualAST* ExtraEq = nullptr;
  std::unique_ptr<CheckIsinAST> extra_isin_owner_;
  CheckIsinAST* ExtraIsin = nullptr;

  std::unique_ptr<StyioAST> next_expr_owner_;
  StyioAST* next_expr = nullptr;
  std::unique_ptr<CondFlowAST> then_cond_flow_owner_;
  CondFlowAST* ThenCondFlow = nullptr;

  std::unique_ptr<StyioAST> ret_expr_owner_;
  StyioAST* RetExpr = nullptr;

private:
  StyioNodeType Type = StyioNodeType::Forward;

public:
  ForwardAST() :
      block_owner_(BlockAST::Create()),
      block(block_owner_.get()) {
  }

  CheckEqualAST* getCheckEq() {
    return ExtraEq;
  }

  CheckIsinAST* getCheckIsin() {
    return ExtraIsin;
  }

  StyioAST* getThen() {
    return next_expr;
  }

  CondFlowAST* getCondFlow() {
    return ThenCondFlow;
  }

  StyioAST* getRetExpr() {
    return RetExpr;
  }

  void setRetExpr(StyioAST* expr) {
    ret_expr_owner_.reset(expr);
    RetExpr = ret_expr_owner_.get();
  }

  const StyioNodeType getNodeType() const {
    return Type;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

/* Backward */
class BackwardAST : public StyioASTTraits<BackwardAST>
{
  std::unique_ptr<StyioAST> object_owner_;
  std::unique_ptr<VarTupleAST> params_owner_;
  std::vector<std::unique_ptr<StyioAST>> operation_owners_;
  std::vector<std::unique_ptr<StyioAST>> ret_expr_owners_;

  static void adopt_exprs(
    std::vector<StyioAST*> exprs,
    std::vector<std::unique_ptr<StyioAST>>& owners,
    std::vector<StyioAST*>& views
  ) {
    owners.clear();
    views.clear();
    owners.reserve(exprs.size());
    views.reserve(exprs.size());
    for (auto* expr : exprs) {
      owners.emplace_back(expr);
      views.push_back(owners.back().get());
    }
  }

private:
  BackwardAST(StyioAST* obj, VarTupleAST* params, std::vector<StyioAST*> ops, std::vector<StyioAST*> rets) :
      object_owner_(obj),
      params_owner_(params),
      object(object_owner_.get()),
      params(params_owner_.get()) {
    adopt_exprs(std::move(ops), operation_owners_, operations);
    adopt_exprs(std::move(rets), ret_expr_owners_, ret_exprs);
  }

public:
  StyioAST* object = nullptr;
  VarTupleAST* params = nullptr;
  std::vector<StyioAST*> operations;
  std::vector<StyioAST*> ret_exprs;  // return-able expressions

  static BackwardAST* Create(StyioAST* obj, VarTupleAST* params, std::vector<StyioAST*> ops, std::vector<StyioAST*> rets) {
    return new BackwardAST(obj, params, ops, rets);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Backward;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

/* Chain of Data Processing */
class CODPAST : public StyioASTTraits<CODPAST>
{
  std::vector<std::unique_ptr<StyioAST>> op_arg_owners_;
  std::unique_ptr<CODPAST> next_op_owner_;

  static void adopt_op_args(
    std::vector<StyioAST*> op_args,
    std::vector<std::unique_ptr<StyioAST>>& owners,
    std::vector<StyioAST*>& views
  ) {
    owners.clear();
    views.clear();
    owners.reserve(op_args.size());
    views.reserve(op_args.size());
    for (auto* arg : op_args) {
      owners.emplace_back(arg);
      views.push_back(owners.back().get());
    }
  }

public:
  std::string OpName = "";
  vector<StyioAST*> OpArgs;
  CODPAST* PrevOp = nullptr;
  CODPAST* NextOp = nullptr;

  CODPAST(std::string op_name, vector<StyioAST*> op_body) :
      OpName(op_name) {
    adopt_op_args(std::move(op_body), op_arg_owners_, OpArgs);
  }

  CODPAST(std::string op_name, vector<StyioAST*> op_body, CODPAST* prev_op) :
      OpName(op_name), PrevOp(prev_op) {
    adopt_op_args(std::move(op_body), op_arg_owners_, OpArgs);
  }

  CODPAST(std::string op_name, vector<StyioAST*> op_body, CODPAST* prev_op, CODPAST* next_op) :
      OpName(op_name), PrevOp(prev_op) {
    adopt_op_args(std::move(op_body), op_arg_owners_, OpArgs);
    setNextOp(next_op);
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

  void setNextOp(CODPAST* next_op) {
    next_op_owner_.reset(next_op);
    NextOp = next_op_owner_.get();
    if (NextOp != nullptr) {
      NextOp->PrevOp = this;
    }
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Chain_Of_Data_Processing;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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
  std::unique_ptr<StyioAST> start_owner_;
  std::unique_ptr<StyioAST> inc_el_owner_;
  InfiniteType WhatType;
  StyioAST* Start = nullptr;
  StyioAST* IncEl = nullptr;

public:
  InfiniteAST() {
    WhatType = InfiniteType::Original;
  }

  InfiniteAST(StyioAST* start, StyioAST* incEl) :
      start_owner_(start),
      inc_el_owner_(incEl),
      Start(start_owner_.get()),
      IncEl(inc_el_owner_.get()) {
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

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Infinite;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
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
  std::unique_ptr<VarTupleAST> args_owner_;
  std::unique_ptr<StyioAST> then_expr_owner_;
  VarTupleAST* Args = nullptr;
  StyioAST* ThenExpr = nullptr;

public:
  /* #() => Then */
  AnonyFuncAST(VarTupleAST* vars, StyioAST* then) :
      args_owner_(vars),
      then_expr_owner_(then),
      Args(args_owner_.get()),
      ThenExpr(then_expr_owner_.get()) {
  }

  VarTupleAST* getArgs() {
    return Args;
  }

  StyioAST* getThenExpr() {
    return ThenExpr;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::AnonyFunc;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

/*
  FuncAST: Function
*/
class FunctionAST : public StyioASTTraits<FunctionAST>
{
private:
  std::unique_ptr<NameAST> func_name_owner_;
  std::vector<std::unique_ptr<ParamAST>> params_owner_;
  std::unique_ptr<TypeAST> ret_type_owner_;
  std::unique_ptr<TypeTupleAST> ret_type_tuple_owner_;
  std::unique_ptr<StyioAST> func_body_owner_;

  void adopt_params(std::vector<ParamAST*> owned_params) {
    params_owner_.clear();
    params.clear();
    params_owner_.reserve(owned_params.size());
    params.reserve(owned_params.size());
    for (auto* param : owned_params) {
      params_owner_.emplace_back(param);
      params.push_back(params_owner_.back().get());
    }
  }

  void adopt_ret_type(TypeAST* type) {
    ret_type_tuple_owner_.reset();
    ret_type_owner_.reset(type);
    ret_type = ret_type_owner_.get();
  }

  void adopt_ret_type(TypeTupleAST* type) {
    ret_type_owner_.reset();
    ret_type_tuple_owner_.reset(type);
    ret_type = ret_type_tuple_owner_.get();
  }

  void adopt_ret_type(std::variant<TypeAST*, TypeTupleAST*> type) {
    if (std::holds_alternative<TypeAST*>(type)) {
      adopt_ret_type(std::get<TypeAST*>(type));
      return;
    }
    adopt_ret_type(std::get<TypeTupleAST*>(type));
  }

  FunctionAST(
    NameAST* name,
    bool is_unique,
    std::vector<ParamAST*> params,
    TypeAST* ret_type_value,
    StyioAST* body
  ) :
      func_name_owner_(name),
      is_unique(is_unique),
      func_name(func_name_owner_.get()) {
    adopt_params(std::move(params));
    adopt_ret_type(ret_type_value);
    func_body_owner_.reset(body);
    func_body = func_body_owner_.get();
  }

  FunctionAST(
    NameAST* name,
    bool is_unique,
    std::vector<ParamAST*> params,
    TypeTupleAST* ret_type_value,
    StyioAST* body
  ) :
      func_name_owner_(name),
      is_unique(is_unique),
      func_name(func_name_owner_.get()) {
    adopt_params(std::move(params));
    adopt_ret_type(ret_type_value);
    func_body_owner_.reset(body);
    func_body = func_body_owner_.get();
  }

  FunctionAST(
    NameAST* name,
    bool is_unique,
    std::vector<ParamAST*> params,
    std::variant<TypeAST*, TypeTupleAST*> ret_type_value,
    StyioAST* body
  ) :
      func_name_owner_(name),
      is_unique(is_unique),
      func_name(func_name_owner_.get()) {
    adopt_params(std::move(params));
    adopt_ret_type(ret_type_value);
    func_body_owner_.reset(body);
    func_body = func_body_owner_.get();
  }

public:
  NameAST* func_name = nullptr;
  bool is_unique = false;
  std::vector<ParamAST*> params;
  std::variant<TypeAST*, TypeTupleAST*> ret_type;

  /*
    Forward (BlockAST)
      => {}
    Iterator (IteratorAST)
      >> () => {}
    Extractor (ExtractorAST)
      << () => {}
    Conditional (CondFlowAST)
      ? () => {} : {}
    Match Cases (CasesAST)
      ?= {}
  */
  StyioAST* func_body = nullptr;

  /*
    A function that contains sufficient information for the code generation
      without refering additional information from any other definition or statement
      is called self-completed.
  */
  bool is_self_completed;

  static FunctionAST* Create(
    NameAST* name,
    bool is_unique,
    std::vector<ParamAST*> params,
    TypeAST* ret_type,
    StyioAST* body
  ) {
    return new FunctionAST(name, is_unique, params, ret_type, body);
  }

  static FunctionAST* Create(
    NameAST* name,
    bool is_unique,
    std::vector<ParamAST*> params,
    TypeTupleAST* ret_type,
    StyioAST* body
  ) {
    return new FunctionAST(name, is_unique, params, ret_type, body);
  }

  static FunctionAST* Create(
    NameAST* name,
    bool is_unique,
    std::vector<ParamAST*> params,
    std::variant<TypeAST*, TypeTupleAST*> ret_type,
    StyioAST* body
  ) {
    return new FunctionAST(name, is_unique, params, ret_type, body);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Func;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }

  bool hasName() {
    if (func_name) {
      return true;
    }
    else {
      return false;
    }
  }

  bool hasRetType() {
    if (ret_type.valueless_by_exception()) {
      return true;
    }
    else {
      return false;
    }
  }

  const std::string& getNameAsStr() {
    return func_name->getAsStr();
  }

  void setRetType(StyioDataType type) {
  }

  bool allArgsTyped() {
    return std::all_of(
      params.begin(),
      params.end(),
      [](ParamAST* param)
      {
        return param->isTyped();
      }
    );
  }

  unordered_map<string, ParamAST*> getParamMap() {
    unordered_map<string, ParamAST*> param_map;

    for (auto p : params) {
      param_map[p->getNameAsStr()] = p;
    }

    return param_map;
  }
};

class SimpleFuncAST : public StyioASTTraits<SimpleFuncAST>
{
private:
  std::unique_ptr<NameAST> func_name_owner_;
  std::vector<std::unique_ptr<ParamAST>> params_owner_;
  std::unique_ptr<TypeAST> ret_type_owner_;
  std::unique_ptr<TypeTupleAST> ret_type_tuple_owner_;
  std::unique_ptr<StyioAST> ret_expr_owner_;

  void adopt_params(std::vector<ParamAST*> owned_params) {
    params_owner_.clear();
    params.clear();
    params_owner_.reserve(owned_params.size());
    params.reserve(owned_params.size());
    for (auto* param : owned_params) {
      params_owner_.emplace_back(param);
      params.push_back(params_owner_.back().get());
    }
  }

  void adopt_ret_type(TypeAST* type) {
    ret_type_tuple_owner_.reset();
    ret_type_owner_.reset(type);
    ret_type = ret_type_owner_.get();
  }

  void adopt_ret_type(TypeTupleAST* type) {
    ret_type_owner_.reset();
    ret_type_tuple_owner_.reset(type);
    ret_type = ret_type_tuple_owner_.get();
  }

  void adopt_ret_type(std::variant<TypeAST*, TypeTupleAST*> type) {
    if (std::holds_alternative<TypeAST*>(type)) {
      adopt_ret_type(std::get<TypeAST*>(type));
      return;
    }
    adopt_ret_type(std::get<TypeTupleAST*>(type));
  }

  void adopt_ret_expr(StyioAST* expr) {
    ret_expr_owner_.reset(expr);
    ret_expr = ret_expr_owner_.get();
  }

  SimpleFuncAST() {}

  SimpleFuncAST(
    NameAST* func_name_value,
    std::vector<ParamAST*> params,
    StyioAST* ret_expr
  ) :
      func_name_owner_(func_name_value),
      func_name(func_name_owner_.get()) {
    adopt_params(std::move(params));
    adopt_ret_expr(ret_expr);
  }

  SimpleFuncAST(
    NameAST* func_name_value,
    bool is_unique,
    std::vector<ParamAST*> params,
    StyioAST* ret_expr
  ) :
      func_name_owner_(func_name_value),
      is_unique(is_unique),
      func_name(func_name_owner_.get()) {
    adopt_params(std::move(params));
    adopt_ret_expr(ret_expr);
  }

  /* TypeAST */

  SimpleFuncAST(
    NameAST* func_name_value,
    std::vector<ParamAST*> params,
    TypeAST* ret_type_value,
    StyioAST* ret_expr
  ) :
      func_name_owner_(func_name_value),
      func_name(func_name_owner_.get()) {
    adopt_params(std::move(params));
    adopt_ret_type(ret_type_value);
    adopt_ret_expr(ret_expr);
  }

  SimpleFuncAST(
    NameAST* func_name_value,
    bool is_unique,
    std::vector<ParamAST*> params,
    TypeAST* ret_type_value,
    StyioAST* ret_expr
  ) :
      func_name_owner_(func_name_value),
      is_unique(is_unique),
      func_name(func_name_owner_.get()) {
    adopt_params(std::move(params));
    adopt_ret_type(ret_type_value);
    adopt_ret_expr(ret_expr);
  }

  /* TypeTupleAST */

  SimpleFuncAST(
    NameAST* func_name_value,
    std::vector<ParamAST*> params,
    TypeTupleAST* ret_type_value,
    StyioAST* ret_expr
  ) :
      func_name_owner_(func_name_value),
      func_name(func_name_owner_.get()) {
    adopt_params(std::move(params));
    adopt_ret_type(ret_type_value);
    adopt_ret_expr(ret_expr);
  }

  SimpleFuncAST(
    NameAST* func_name_value,
    bool is_unique,
    std::vector<ParamAST*> params,
    TypeTupleAST* ret_type_value,
    StyioAST* ret_expr
  ) :
      func_name_owner_(func_name_value),
      is_unique(is_unique),
      func_name(func_name_owner_.get()) {
    adopt_params(std::move(params));
    adopt_ret_type(ret_type_value);
    adopt_ret_expr(ret_expr);
  }

  /* std::variant<TypeAST*, TypeTupleAST*> */

  SimpleFuncAST(
    NameAST* func_name_value,
    std::vector<ParamAST*> params,
    std::variant<TypeAST*, TypeTupleAST*> ret_type_value,
    StyioAST* ret_expr
  ) :
      func_name_owner_(func_name_value),
      func_name(func_name_owner_.get()) {
    adopt_params(std::move(params));
    adopt_ret_type(ret_type_value);
    adopt_ret_expr(ret_expr);
  }

  SimpleFuncAST(
    NameAST* func_name_value,
    bool is_unique,
    std::vector<ParamAST*> params,
    std::variant<TypeAST*, TypeTupleAST*> ret_type_value,
    StyioAST* ret_expr
  ) :
      func_name_owner_(func_name_value),
      is_unique(is_unique),
      func_name(func_name_owner_.get()) {
    adopt_params(std::move(params));
    adopt_ret_type(ret_type_value);
    adopt_ret_expr(ret_expr);
  }

public:
  NameAST* func_name = nullptr; /* */
  bool is_unique = false;
  std::vector<ParamAST*> params;
  std::variant<TypeAST*, TypeTupleAST*> ret_type; /* */
  StyioAST* ret_expr = nullptr;                   /* */

  static SimpleFuncAST* Create() {
    return new SimpleFuncAST();
  }

  static SimpleFuncAST* Create(
    NameAST* func_name,
    bool is_unique,
    std::vector<ParamAST*> params,
    StyioAST* ret_expr
  ) {
    return new SimpleFuncAST(func_name, is_unique, params, ret_expr);
  }

  static SimpleFuncAST* Create(
    NameAST* func_name,
    std::vector<ParamAST*> params,
    StyioAST* ret_expr
  ) {
    return new SimpleFuncAST(func_name, params, ret_expr);
  }

  /* TypeAST */

  static SimpleFuncAST* Create(
    NameAST* func_name,
    bool is_unique,
    std::vector<ParamAST*> params,
    TypeAST* ret_type,
    StyioAST* ret_expr
  ) {
    return new SimpleFuncAST(func_name, is_unique, params, ret_type, ret_expr);
  }

  static SimpleFuncAST* Create(
    NameAST* func_name,
    std::vector<ParamAST*> params,
    TypeAST* ret_type,
    StyioAST* ret_expr
  ) {
    return new SimpleFuncAST(func_name, params, ret_type, ret_expr);
  }

  /* TypeTupleAST */

  static SimpleFuncAST* Create(
    NameAST* func_name,
    bool is_unique,
    std::vector<ParamAST*> params,
    TypeTupleAST* ret_type,
    StyioAST* ret_expr
  ) {
    return new SimpleFuncAST(func_name, is_unique, params, ret_type, ret_expr);
  }

  static SimpleFuncAST* Create(
    NameAST* func_name,
    std::vector<ParamAST*> params,
    TypeTupleAST* ret_type,
    StyioAST* ret_expr
  ) {
    return new SimpleFuncAST(func_name, params, ret_type, ret_expr);
  }

  /* std::variant<TypeAST*, TypeTupleAST*> */

  static SimpleFuncAST* Create(
    NameAST* func_name,
    bool is_unique,
    std::vector<ParamAST*> params,
    std::variant<TypeAST*, TypeTupleAST*> ret_type,
    StyioAST* ret_expr
  ) {
    return new SimpleFuncAST(func_name, is_unique, params, ret_type, ret_expr);
  }

  static SimpleFuncAST* Create(
    NameAST* func_name,
    std::vector<ParamAST*> params,
    std::variant<TypeAST*, TypeTupleAST*> ret_type,
    StyioAST* ret_expr
  ) {
    return new SimpleFuncAST(func_name, params, ret_type, ret_expr);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::SimpleFunc;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "TupleOp", 0};
  }
};

/*
  =================
    Iterator
  =================
*/

/*
  Infinite / while loop: [...] => { } or [...] ?(cond) >> { }
*/
class InfiniteLoopAST : public StyioASTTraits<InfiniteLoopAST>
{
  std::unique_ptr<StyioAST> while_cond_owner_;
  std::unique_ptr<BlockAST> body_owner_;
  StyioAST* while_cond_ = nullptr;
  BlockAST* body_ = nullptr;

  InfiniteLoopAST(StyioAST* cond, BlockAST* body) :
      while_cond_owner_(cond),
      body_owner_(body),
      while_cond_(while_cond_owner_.get()),
      body_(body_owner_.get()) {
  }

public:
  static InfiniteLoopAST* CreateInfinite(BlockAST* body) {
    return new InfiniteLoopAST(nullptr, body);
  }

  static InfiniteLoopAST* CreateWhile(StyioAST* cond, BlockAST* body) {
    return new InfiniteLoopAST(cond, body);
  }

  /* Legacy empty loop (unused list/loop char parser path). */
  static InfiniteLoopAST* Create() {
    return new InfiniteLoopAST(nullptr, BlockAST::Create({}));
  }

  StyioAST* getWhileCond() const {
    return while_cond_;
  }

  BlockAST* getBody() const {
    return body_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Loop;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

/*
  Iterator:
    collection >> operations
*/

/*
  Iterator:
    collection >> operations
*/
class IteratorAST : public StyioASTTraits<IteratorAST>
{
private:
  std::unique_ptr<StyioAST> collection_owner_;
  std::vector<std::unique_ptr<ParamAST>> params_owners_;
  std::vector<std::unique_ptr<StyioAST>> following_owners_;

  void adopt_params(std::vector<ParamAST*> owned_params) {
    params_owners_.clear();
    params.clear();
    params_owners_.reserve(owned_params.size());
    params.reserve(owned_params.size());
    for (auto* param : owned_params) {
      params_owners_.emplace_back(param);
      params.push_back(params_owners_.back().get());
    }
  }

public:
  StyioAST* collection = nullptr;
  std::vector<ParamAST*> params;
  std::vector<StyioAST*> following;

  IteratorAST(
    StyioAST* collection
  ) :
      collection_owner_(collection),
      collection(collection_owner_.get()) {
  }

  IteratorAST(
    StyioAST* collection,
    std::vector<ParamAST*> params
  ) :
      collection_owner_(collection),
      collection(collection_owner_.get()) {
    adopt_params(std::move(params));
  }

  static IteratorAST* Create(
    StyioAST* collection,
    std::vector<ParamAST*> params
  ) {
    return new IteratorAST(collection, params);
  }

  IteratorAST(
    StyioAST* collection,
    std::vector<ParamAST*> params,
    std::vector<StyioAST*> following
  ) :
      collection_owner_(collection),
      collection(collection_owner_.get()) {
    adopt_params(std::move(params));
    append_followings(std::move(following));
  }

  static IteratorAST* Create(
    StyioAST* collection,
    std::vector<ParamAST*> params,
    std::vector<StyioAST*> following
  ) {
    return new IteratorAST(collection, params, following);
  }

  static IteratorAST* Create(
    StyioAST* collection,
    std::vector<ParamAST*> params,
    StyioAST* forward_expr
  ) {
    std::vector<StyioAST*> forward_followings{forward_expr};
    return new IteratorAST(collection, params, forward_followings);
  }

  void append_followings(std::vector<StyioAST*> extra_following) {
    following_owners_.reserve(following_owners_.size() + extra_following.size());
    following.reserve(following.size() + extra_following.size());
    for (auto* node : extra_following) {
      following_owners_.emplace_back(node);
      following.push_back(following_owners_.back().get());
    }
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::Iterator;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class StreamZipAST : public StyioASTTraits<StreamZipAST>
{
  std::unique_ptr<StyioAST> collection_a_owner_;
  std::vector<std::unique_ptr<ParamAST>> params_a_owners_;
  StyioAST* collection_a_ = nullptr;
  std::vector<ParamAST*> params_a_;
  std::unique_ptr<StyioAST> collection_b_owner_;
  std::vector<std::unique_ptr<ParamAST>> params_b_owners_;
  StyioAST* collection_b_ = nullptr;
  std::vector<ParamAST*> params_b_;
  std::vector<std::unique_ptr<StyioAST>> following_owners_;
  std::vector<StyioAST*> following_;

  static void adopt_params(
    std::vector<ParamAST*> params,
    std::vector<std::unique_ptr<ParamAST>>& owners,
    std::vector<ParamAST*>& views
  ) {
    owners.clear();
    views.clear();
    owners.reserve(params.size());
    views.reserve(params.size());
    for (auto* param : params) {
      owners.emplace_back(param);
      views.push_back(owners.back().get());
    }
  }

  void adopt_following(std::vector<StyioAST*> fol) {
    following_owners_.clear();
    following_.clear();
    following_owners_.reserve(fol.size());
    following_.reserve(fol.size());
    for (auto* node : fol) {
      following_owners_.emplace_back(node);
      following_.push_back(following_owners_.back().get());
    }
  }

  StreamZipAST(
    StyioAST* ca,
    std::vector<ParamAST*> pa,
    StyioAST* cb,
    std::vector<ParamAST*> pb,
    std::vector<StyioAST*> fol
  ) :
      collection_a_owner_(ca),
      collection_a_(collection_a_owner_.get()),
      collection_b_owner_(cb),
      collection_b_(collection_b_owner_.get()) {
    adopt_params(std::move(pa), params_a_owners_, params_a_);
    adopt_params(std::move(pb), params_b_owners_, params_b_);
    adopt_following(std::move(fol));
  }

public:
  static StreamZipAST* Create(
    StyioAST* ca,
    std::vector<ParamAST*> pa,
    StyioAST* cb,
    std::vector<ParamAST*> pb,
    StyioAST* body
  ) {
    std::vector<StyioAST*> fol;
    fol.push_back(body);
    return new StreamZipAST(ca, std::move(pa), cb, std::move(pb), std::move(fol));
  }

  StyioAST* getCollectionA() {
    return collection_a_;
  }
  StyioAST* getCollectionB() {
    return collection_b_;
  }
  std::vector<ParamAST*>& getParamsA() {
    return params_a_;
  }
  std::vector<ParamAST*>& getParamsB() {
    return params_b_;
  }
  std::vector<StyioAST*>& getFollowing() {
    return following_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::StreamZip;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class SnapshotDeclAST : public StyioASTTraits<SnapshotDeclAST>
{
  std::unique_ptr<NameAST> var_owner_;
  std::unique_ptr<FileResourceAST> resource_owner_;
  NameAST* var_ = nullptr;
  FileResourceAST* resource_ = nullptr;

  SnapshotDeclAST(NameAST* v, FileResourceAST* r) :
      var_owner_(v),
      resource_owner_(r),
      var_(var_owner_.get()),
      resource_(resource_owner_.get()) {
  }

public:
  static SnapshotDeclAST* Create(NameAST* v, FileResourceAST* r) {
    return new SnapshotDeclAST(v, r);
  }

  NameAST* getVar() {
    return var_;
  }
  FileResourceAST* getResource() {
    return resource_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::SnapshotDecl;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

class InstantPullAST : public StyioASTTraits<InstantPullAST>
{
  std::unique_ptr<StyioAST> resource_owner_;
  StyioAST* resource_ = nullptr;

  explicit InstantPullAST(StyioAST* r) :
      resource_owner_(r),
      resource_(resource_owner_.get()) {
  }

public:
  static InstantPullAST* Create(StyioAST* r) {
    return new InstantPullAST(r);
  }

  /* Legacy convenience: still accepts FileResourceAST* */
  static InstantPullAST* Create(FileResourceAST* r) {
    return new InstantPullAST(static_cast<StyioAST*>(r));
  }

  StyioAST* getResource() {
    return resource_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::InstantPull;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  }
};

class TypedStdinListAST : public StyioASTTraits<TypedStdinListAST>
{
  std::unique_ptr<TypeAST> list_type_owner_;
  TypeAST* list_type_ = nullptr;

  explicit TypedStdinListAST(TypeAST* t) :
      list_type_owner_(t),
      list_type_(list_type_owner_.get()) {
  }

public:
  static TypedStdinListAST* Create(TypeAST* t) {
    return new TypedStdinListAST(t);
  }

  TypeAST* getListType() {
    return list_type_;
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::TypedStdinList;
  }

  const StyioDataType getDataType() const {
    return list_type_->getDataType();
  }
};

class IterSeqAST : public IteratorAST
{
private:
  std::vector<std::unique_ptr<HashTagNameAST>> hash_tag_owners_;

  static void adopt_hash_tags(
    std::vector<HashTagNameAST*> tags,
    std::vector<std::unique_ptr<HashTagNameAST>>& owners,
    std::vector<HashTagNameAST*>& views
  ) {
    owners.clear();
    views.clear();
    owners.reserve(tags.size());
    views.reserve(tags.size());
    for (auto* tag : tags) {
      owners.emplace_back(tag);
      views.push_back(owners.back().get());
    }
  }

  IterSeqAST(
    StyioAST* collection,
    std::vector<HashTagNameAST*> hash_tags_in
  ) :
      IteratorAST(collection) {
    adopt_hash_tags(std::move(hash_tags_in), hash_tag_owners_, hash_tags);
  }

  IterSeqAST(
    StyioAST* collection,
    std::vector<ParamAST*> params,
    std::vector<HashTagNameAST*> hash_tags_in
  ) :
      IteratorAST(collection, params) {
    adopt_hash_tags(std::move(hash_tags_in), hash_tag_owners_, hash_tags);
  }

public:
  using IteratorAST::collection;
  using IteratorAST::following;
  using IteratorAST::params;
  std::vector<HashTagNameAST*> hash_tags;

  static IterSeqAST* Create(
    StyioAST* collection,
    std::vector<HashTagNameAST*> hash_tags
  ) {
    return new IterSeqAST(collection, hash_tags);
  }

  static IterSeqAST* Create(
    StyioAST* collection,
    std::vector<ParamAST*> params,
    std::vector<HashTagNameAST*> hash_tags
  ) {
    return new IterSeqAST(collection, params, hash_tags);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::IterSeq;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
};

/*
  Extractor:
    collection << operations
*/
class ExtractorAST : public StyioASTTraits<ExtractorAST>
{
private:
  std::unique_ptr<StyioAST> tuple_owner_;
  std::unique_ptr<StyioAST> op_owner_;

  ExtractorAST(StyioAST* theTuple, StyioAST* theOpOnIt) :
      tuple_owner_(theTuple),
      op_owner_(theOpOnIt),
      theTuple(tuple_owner_.get()),
      theOpOnIt(op_owner_.get()) {
  }

public:
  StyioAST* theTuple;
  StyioAST* theOpOnIt;

  static ExtractorAST* Create(StyioAST* the_tuple, StyioAST* the_op) {
    return new ExtractorAST(the_tuple, the_op);
  }

  const StyioNodeType getNodeType() const {
    return StyioNodeType::TupleOperation;
  }

  const StyioDataType getDataType() const {
    return StyioDataType{StyioDataTypeOption::Undefined, "TupleOp", 0};
  }
};

#endif
