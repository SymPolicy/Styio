#pragma once
#ifndef STYIO_SG_IR_H_
#define STYIO_SG_IR_H_

// [C++ STL]
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// [Styio]
#include "../../StyioToken/Token.hpp"
#include "../IRDecl.hpp"
#include "../StyioIR.hpp"

/*
  SG = Styio General. Default/general IR nodes live here.

  GenIR nodes should be directly converted to LLVM IR.
*/

/*
  SResId (Styio Resource Indentifier)
*/
class SGResId : public StyioIRTraits<SGResId>
{
private:
  std::string id;

public:
  bool has_history_selector = false;
  int history_offset = 0;

  SGResId(std::string id, bool has_history_selector = false, int history_offset = 0) :
      id(id),
      has_history_selector(has_history_selector),
      history_offset(history_offset) {
  }

  static SGResId* Create() {
    return styio::session_alloc::make_ir<SGResId>("");
  }

  static SGResId* Create(std::string id) {
    return styio::session_alloc::make_ir<SGResId>(id);
  }

  static SGResId* CreateHistory(std::string id, int offset) {
    return styio::session_alloc::make_ir<SGResId>(id, true, offset);
  }

  const std::string& as_str() {
    return id;
  }
};

class SGType : public StyioIRTraits<SGType>
{
public:
  StyioDataType data_type;

  SGType(StyioDataType data_type) :
      data_type(data_type) {
  }

  static SGType* Create(StyioDataType data_type) {
    return styio::session_alloc::make_ir<SGType>(data_type);
  }
};

class SGNoOp : public StyioIRTraits<SGNoOp>
{
public:
  static SGNoOp* Create() {
    return styio::session_alloc::make_ir<SGNoOp>();
  }
};

class SGConstBool : public StyioIRTraits<SGConstBool>
{
public:
  bool value;

  SGConstBool(bool value) :
      value(value) {
  }

  static SGConstBool* Create(bool value) {
    return styio::session_alloc::make_ir<SGConstBool>(value);
  }
};

/*
  Two Types of Numbers:
  - Constant Integer
  - Constant Float
*/
class SGConstInt : public StyioIRTraits<SGConstInt>
{
public:
  std::string value;
  size_t num_of_bit;

  SGConstInt(
    std::string value,
    size_t numbits
  ) :
      value(value),
      num_of_bit(numbits) {
  }

  static SGConstInt* Create(long value) {
    return styio::session_alloc::make_ir<SGConstInt>(std::to_string(value), 64);
  }

  static SGConstInt* Create(std::string value) {
    return styio::session_alloc::make_ir<SGConstInt>(value, 64);
  }

  static SGConstInt* Create(std::string value, size_t numbits) {
    return styio::session_alloc::make_ir<SGConstInt>(value, numbits);
  }
};

class SGConstFloat : public StyioIRTraits<SGConstFloat>
{
public:
  std::string value;

  SGConstFloat(std::string value) :
      value(value) {
  }

  static SGConstFloat* Create(std::string value) {
    return styio::session_alloc::make_ir<SGConstFloat>(value);
  }
};

class SGConstChar : public StyioIRTraits<SGConstChar>
{
public:
  char value;

  SGConstChar(char value) :
      value(value) {
  }

  static SGConstChar* Create(char value) {
    return styio::session_alloc::make_ir<SGConstChar>(value);
  }
};

class SGConstString : public StyioIRTraits<SGConstString>
{
public:
  std::string value;

  SGConstString(std::string value) :
      value(value) {
  }

  static SGConstString* Create(std::string value) {
    return styio::session_alloc::make_ir<SGConstString>(value);
  }
};

class SGFormatString : public StyioIRTraits<SGFormatString>
{
public:
  std::vector<string> frags;   /* fragments */
  std::vector<StyioIR*> exprs; /* expressions */

  SGFormatString(std::vector<string> fragments, std::vector<StyioIR*> expressions) :
      frags(std::move(fragments)), exprs(std::move(expressions)) {
  }

  static SGFormatString* Create(std::vector<string> fragments, std::vector<StyioIR*> expressions) {
    return styio::session_alloc::make_ir<SGFormatString>(fragments, expressions);
  };
};

class SGStruct : public StyioIRTraits<SGStruct>
{
public:
  SGResId* name;
  std::vector<SGVar*> elements;

  SGStruct(std::vector<SGVar*> elements) :
      elements(elements) {
  }

  SGStruct(SGResId* name, std::vector<SGVar*> elements) :
      name(name), elements(elements) {
  }

  static SGStruct* Create(std::vector<SGVar*> elements) {
    return styio::session_alloc::make_ir<SGStruct>(elements);
  }

  static SGStruct* Create(SGResId* name, std::vector<SGVar*> elements) {
    return styio::session_alloc::make_ir<SGStruct>(name, elements);
  }
};

class SGCast : public StyioIRTraits<SGCast>
{
public:
  StyioIR* value = nullptr;
  SGType* from_type;
  SGType* to_type;

  SGCast(StyioIR* value, SGType* from_type, SGType* to_type) :
      value(value), from_type(from_type), to_type(to_type) {
  }

  ~SGCast() override {
    delete value;
    delete from_type;
    delete to_type;
  }

  static SGCast* Create(StyioIR* value, SGType* from_type, SGType* to_type) {
    return styio::session_alloc::make_ir<SGCast>(value, from_type, to_type);
  };

  static SGCast* Create(SGType* from_type, SGType* to_type) {
    return styio::session_alloc::make_ir<SGCast>(nullptr, from_type, to_type);
  };
};

/* Binary Operation Expression */
class SGBinOp : public StyioIRTraits<SGBinOp>
{
public:
  SGType* data_type;
  StyioIR* lhs_expr;
  StyioIR* rhs_expr;
  StyioOpType operand;
  StyioDataType lhs_type{StyioDataTypeOption::Undefined, "undefined", 0};
  StyioDataType rhs_type{StyioDataTypeOption::Undefined, "undefined", 0};

  SGBinOp(StyioIR* lhs, StyioIR* rhs, StyioOpType op, SGType* data_type) :
      lhs_expr(std::move(lhs)), rhs_expr(std::move(rhs)), operand(op), data_type(std::move(data_type)) {
  }

  SGBinOp(
    StyioIR* lhs,
    StyioIR* rhs,
    StyioOpType op,
    SGType* data_type,
    StyioDataType lhs_data_type,
    StyioDataType rhs_data_type
  ) :
      lhs_expr(std::move(lhs)),
      rhs_expr(std::move(rhs)),
      operand(op),
      data_type(std::move(data_type)),
      lhs_type(std::move(lhs_data_type)),
      rhs_type(std::move(rhs_data_type)) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGBinOp() override {
    delete data_type;
    delete lhs_expr;
    delete rhs_expr;
  }

  static SGBinOp* Create(StyioIR* lhs, StyioIR* rhs, StyioOpType op, SGType* data_type) {
    return styio::session_alloc::make_ir<SGBinOp>(lhs, rhs, op, data_type);
  }

  static SGBinOp* Create(
    StyioIR* lhs,
    StyioIR* rhs,
    StyioOpType op,
    SGType* data_type,
    StyioDataType lhs_data_type,
    StyioDataType rhs_data_type
  ) {
    return styio::session_alloc::make_ir<SGBinOp>(lhs, rhs, op, data_type, std::move(lhs_data_type), std::move(rhs_data_type));
  }
};

class SGCond : public StyioIRTraits<SGCond>
{
public:
  StyioIR* lhs_expr;
  StyioIR* rhs_expr;
  StyioOpType operand;

  SGCond(StyioIR* lhs, StyioIR* rhs, StyioOpType op) :
      lhs_expr(std::move(lhs)), rhs_expr(std::move(rhs)), operand(op) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGCond() override {
    delete lhs_expr;
    delete rhs_expr;
  }

  static SGCond* Create(StyioIR* lhs, StyioIR* rhs, StyioOpType op) {
    return styio::session_alloc::make_ir<SGCond>(lhs, rhs, op);
  }
};

class SGVar : public StyioIRTraits<SGVar>
{
public:
  SGResId* var_name;
  SGType* var_type;
  StyioIR* val_init = nullptr;
  bool is_dynamic_slot = false;
  bool is_list_slot = false;

  SGVar(SGResId* id, SGType* type) :
      var_name(id), var_type(type) {
  }

  SGVar(SGResId* id, SGType* type, StyioIR* value) :
      var_name(id), var_type(type), val_init(value) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGVar() override {
    delete var_name;
    delete var_type;
    delete val_init;
  }

  static SGVar* Create(SGResId* id, SGType* type) {
    return styio::session_alloc::make_ir<SGVar>(id, type);
  }

  static SGVar* Create(SGResId* id, SGType* type, StyioIR* value) {
    return styio::session_alloc::make_ir<SGVar>(id, type, value);
  }
};

class SGFlexBind : public StyioIRTraits<SGFlexBind>
{
public:
  SGVar* var;
  StyioIR* value;
  bool pending_resource_write = false;

  SGFlexBind(SGVar* var, StyioIR* value, bool pending = false) :
      var(var), value(value), pending_resource_write(pending) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGFlexBind() override {
    delete var;
    delete value;
  }

  static SGFlexBind* Create(SGVar* id, StyioIR* value, bool pending = false) {
    return styio::session_alloc::make_ir<SGFlexBind>(id, value, pending);
  }
};

class SGFinalBind : public StyioIRTraits<SGFinalBind>
{
public:
  SGVar* var;
  StyioIR* value;

  SGFinalBind(SGVar* var, StyioIR* value) :
      var(var), value(value) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGFinalBind() override {
    delete var;
    delete value;
  }

  static SGFinalBind* Create(SGVar* id, StyioIR* value) {
    return styio::session_alloc::make_ir<SGFinalBind>(id, value);
  }
};

enum class SGDynLoadKind : std::uint8_t
{
  Bool,
  I64,
  F64,
  CString,
  ListHandle,
  DictHandle,
  MatrixHandle,
  TaskHandle,
};

class SGDynLoad : public StyioIRTraits<SGDynLoad>
{
public:
  std::string var_name;
  SGDynLoadKind kind = SGDynLoadKind::I64;

  SGDynLoad(std::string name, SGDynLoadKind k) :
      var_name(std::move(name)), kind(k) {
  }

  static SGDynLoad* Create(std::string name, SGDynLoadKind kind) {
    return styio::session_alloc::make_ir<SGDynLoad>(std::move(name), kind);
  }
};

class SGFuncArg : public StyioIRTraits<SGFuncArg>
{
public:
  std::string id;
  SGType* arg_type;

  SGFuncArg(std::string id, SGType* type) :
      id(id), arg_type(type) {
  }

  ~SGFuncArg() override {
    delete arg_type;
  }

  static SGFuncArg* Create(std::string id, SGType* type) {
    return styio::session_alloc::make_ir<SGFuncArg>(id, type);
  }
};

class SGFunc : public StyioIRTraits<SGFunc>
{
public:
  SGType* ret_type;
  SGResId* func_name;
  std::vector<SGFuncArg*> func_args;
  SGBlock* func_block;

  SGFunc(
    SGType* ret_type,
    SGResId* func_name,
    std::vector<SGFuncArg*> func_args,
    SGBlock* func_block
  ) :
      ret_type(ret_type),
      func_name(func_name),
      func_args(func_args),
      func_block(func_block) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGFunc() override;

  static SGFunc* Create(SGType* ret_type, SGResId* func_name, std::vector<SGFuncArg*> func_args, SGBlock* func_block) {
    return styio::session_alloc::make_ir<SGFunc>(ret_type, func_name, func_args, func_block);
  }
};

class SGCall : public StyioIRTraits<SGCall>
{
public:
  SGResId* func_name;
  std::vector<StyioIR*> func_args;

  SGCall(SGResId* func_name, std::vector<StyioIR*> func_args) :
      func_name(std::move(func_name)), func_args(std::move(func_args)) {
  }

  ~SGCall() override {
    delete func_name;
    styio_delete_ir_nodes(func_args);
  }

  static SGCall* Create(SGResId* func_name, std::vector<StyioIR*> func_args) {
    return styio::session_alloc::make_ir<SGCall>(std::move(func_name), std::move(func_args));
  }
};

class SGExportDecl : public StyioIRTraits<SGExportDecl>
{
public:
  std::vector<std::string> symbols;

  explicit SGExportDecl(std::vector<std::string> symbols) :
      symbols(std::move(symbols)) {
  }

  static SGExportDecl* Create(std::vector<std::string> symbols) {
    return styio::session_alloc::make_ir<SGExportDecl>(std::move(symbols));
  }
};

class SGExternBlock : public StyioIRTraits<SGExternBlock>
{
public:
  std::string abi;
  std::string body;
  std::vector<std::string> source_paths;
  std::vector<std::string> exported_symbols;

  SGExternBlock(
    std::string abi,
    std::string body,
    std::vector<std::string> source_paths = {},
    std::vector<std::string> exported_symbols = {}
  ) :
      abi(std::move(abi)),
      body(std::move(body)),
      source_paths(std::move(source_paths)),
      exported_symbols(std::move(exported_symbols)) {
  }

  static SGExternBlock* Create(
    std::string abi,
    std::string body,
    std::vector<std::string> source_paths = {},
    std::vector<std::string> exported_symbols = {}
  ) {
    return styio::session_alloc::make_ir<SGExternBlock>(
      std::move(abi),
      std::move(body),
      std::move(source_paths),
      std::move(exported_symbols));
  }
};

class SGReturn : public StyioIRTraits<SGReturn>
{
public:
  StyioIR* expr;

  SGReturn(StyioIR* expr) :
      expr(expr) {
  }

  ~SGReturn() override {
    delete expr;
  }

  static SGReturn* Create(StyioIR* expr) {
    return styio::session_alloc::make_ir<SGReturn>(expr);
  }
};

class SGBlock : public StyioIRTraits<SGBlock>
{
public:
  std::vector<StyioIR*> stmts;

  SGBlock(std::vector<StyioIR*> stmts) :
      stmts(std::move(stmts)) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGBlock() override {
    styio_delete_ir_nodes(stmts);
  }

  static SGBlock* Create(std::vector<StyioIR*> stmts) {
    return styio::session_alloc::make_ir<SGBlock>(std::move(stmts));
  }
};

inline SGFunc::~SGFunc() {
  delete ret_type;
  delete func_name;
  styio_delete_ir_nodes(func_args);
  delete func_block;
}

class SGEntry : public StyioIRTraits<SGEntry>
{
public:
  std::vector<StyioIR*> stmts;

  SGEntry(std::vector<StyioIR*> stmts) :
      stmts(std::move(stmts)) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGEntry() override {
    styio_delete_ir_nodes(stmts);
  }

  static SGEntry* Create(std::vector<StyioIR*> stmts) {
    return styio::session_alloc::make_ir<SGEntry>(std::move(stmts));
  }
};

class SGMainEntry : public StyioIRTraits<SGMainEntry>
{
public:
  std::vector<StyioIR*> stmts;

  SGMainEntry(std::vector<StyioIR*> stmts) :
      stmts(stmts) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGMainEntry() override {
    styio_delete_ir_nodes(stmts);
  }

  static SGMainEntry* Create(std::vector<StyioIR*> stmts) {
    return styio::session_alloc::make_ir<SGMainEntry>(stmts);
  }
};

enum class SGLoopTag
{
  Infinite,
  WhileCond,
};

class SGLoop : public StyioIRTraits<SGLoop>
{
public:
  SGLoopTag tag;
  StyioIR* cond = nullptr;
  SGBlock* body = nullptr;

  SGLoop(SGLoopTag t, StyioIR* c, SGBlock* b) :
      tag(t), cond(c), body(b) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGLoop() override {
    delete cond;
    delete body;
  }

  static SGLoop* CreateInfinite(SGBlock* b) {
    return styio::session_alloc::make_ir<SGLoop>(SGLoopTag::Infinite, nullptr, b);
  }

  static SGLoop* CreateWhile(StyioIR* c, SGBlock* b) {
    return styio::session_alloc::make_ir<SGLoop>(SGLoopTag::WhileCond, c, b);
  }
};

enum class SGStateSlotKind : std::uint8_t
{
  Acc,
  Track,
  WinAvg,
  WinMax,
};

struct SGStateSlotDesc
{
  SGStateSlotKind kind = SGStateSlotKind::Acc;
  int id = 0;
  int offset = 0;
  int size = 0;
  int win_n = 0;
  std::string acc_name;
  std::string export_name;
};

struct SGPulsePlan
{
  std::vector<SGStateSlotDesc> slots;
  /* After each pulse, copy export flex var into ledger for each slot. */
  std::vector<std::pair<int, std::string>> commits;
  int total_bytes = 0;
  std::unordered_map<std::string, int> ref_to_slot;
};

class SGStateSnapLoad : public StyioIRTraits<SGStateSnapLoad>
{
public:
  int slot_id = 0;

  explicit SGStateSnapLoad(int s) :
      slot_id(s) {
  }

  static SGStateSnapLoad* Create(int s) {
    return styio::session_alloc::make_ir<SGStateSnapLoad>(s);
  }
};

class SGStateHistLoad : public StyioIRTraits<SGStateHistLoad>
{
public:
  int slot_id = 0;
  int depth = 1;
  /* >=0: load from finalized pulse ledger after matching SGForEach/SIOFileLineIter exit */
  int pulse_region_id = -1;

  SGStateHistLoad(int s, int d, int region = -1) :
      slot_id(s), depth(d), pulse_region_id(region) {
  }

  static SGStateHistLoad* Create(int s, int d, int region = -1) {
    return styio::session_alloc::make_ir<SGStateHistLoad>(s, d, region);
  }
};

class SGSeriesAvgStep : public StyioIRTraits<SGSeriesAvgStep>
{
public:
  int slot_id = 0;
  StyioIR* x = nullptr;

  SGSeriesAvgStep(int s, StyioIR* xi) :
      slot_id(s), x(xi) {
  }

  ~SGSeriesAvgStep() override {
    delete x;
  }

  static SGSeriesAvgStep* Create(int s, StyioIR* xi) {
    return styio::session_alloc::make_ir<SGSeriesAvgStep>(s, xi);
  }
};

class SGSeriesMaxStep : public StyioIRTraits<SGSeriesMaxStep>
{
public:
  int slot_id = 0;
  StyioIR* x = nullptr;

  SGSeriesMaxStep(int s, StyioIR* xi) :
      slot_id(s), x(xi) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGSeriesMaxStep() override {
    delete x;
  }

  static SGSeriesMaxStep* Create(int s, StyioIR* xi) {
    return styio::session_alloc::make_ir<SGSeriesMaxStep>(s, xi);
  }
};

class SGForEach : public StyioIRTraits<SGForEach>
{
public:
  StyioIR* iterable = nullptr;
  std::string var;
  std::string elem_type = "i64";
  SGBlock* body = nullptr;
  std::unique_ptr<SGPulsePlan> pulse_plan;
  int pulse_region_id = -1;

  SGForEach(StyioIR* it, std::string v, std::string et, SGBlock* b) :
      iterable(it), var(std::move(v)), elem_type(std::move(et)), body(b) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGForEach() override {
    delete iterable;
    delete body;
  }

  static SGForEach* Create(StyioIR* it, std::string v, std::string elem_type, SGBlock* b) {
    return styio::session_alloc::make_ir<SGForEach>(it, std::move(v), std::move(elem_type), b);
  }

  void set_pulse_plan(std::unique_ptr<SGPulsePlan> p) {
    pulse_plan = std::move(p);
  }
};

class SGRangeFor : public StyioIRTraits<SGRangeFor>
{
public:
  StyioIR* start = nullptr;
  StyioIR* end = nullptr;
  StyioIR* step = nullptr;
  std::string var;
  SGBlock* body = nullptr;

  SGRangeFor(StyioIR* s, StyioIR* e, StyioIR* st, std::string v, SGBlock* b) :
      start(s), end(e), step(st), var(std::move(v)), body(b) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGRangeFor() override {
    delete start;
    delete end;
    delete step;
    delete body;
  }

  static SGRangeFor* Create(StyioIR* s, StyioIR* e, StyioIR* st, std::string v, SGBlock* b) {
    return styio::session_alloc::make_ir<SGRangeFor>(s, e, st, std::move(v), b);
  }
};

class SGIf : public StyioIRTraits<SGIf>
{
public:
  StyioIR* cond = nullptr;
  SGBlock* then_block = nullptr;
  SGBlock* else_block = nullptr;

  SGIf(StyioIR* c, SGBlock* t, SGBlock* e) :
      cond(c), then_block(t), else_block(e) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGIf() override {
    delete cond;
    delete then_block;
    delete else_block;
  }

  static SGIf* Create(StyioIR* cond, SGBlock* then_block, SGBlock* else_block = nullptr) {
    return styio::session_alloc::make_ir<SGIf>(cond, then_block, else_block);
  }
};

enum class SGMatchReprKind
{
  Stmt,
  ExprInt,
  ExprFloat,
  ExprBool,
  ExprChar,
  ExprMixed,
};

class SGMatch : public StyioIRTraits<SGMatch>
{
public:
  StyioIR* scrutinee = nullptr;
  std::vector<std::pair<std::int64_t, SGBlock*>> int_arms;
  SGBlock* default_arm = nullptr;
  SGMatchReprKind repr_kind = SGMatchReprKind::Stmt;

  SGMatch(
    StyioIR* s,
    std::vector<std::pair<std::int64_t, SGBlock*>> arms,
    SGBlock* d,
    SGMatchReprKind k
  ) :
      scrutinee(s),
      int_arms(std::move(arms)),
      default_arm(d),
      repr_kind(k) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGMatch() override {
    delete scrutinee;
    for (auto& arm : int_arms) {
      delete arm.second;
    }
    int_arms.clear();
    delete default_arm;
  }

  static SGMatch* Create(
    StyioIR* s,
    std::vector<std::pair<std::int64_t, SGBlock*>> arms,
    SGBlock* d,
    SGMatchReprKind k
  ) {
    return styio::session_alloc::make_ir<SGMatch>(s, std::move(arms), d, k);
  }
};

class SGBreak : public StyioIRTraits<SGBreak>
{
public:
  unsigned depth = 1;

  explicit SGBreak(unsigned d) :
      depth(1) {
    (void)d;
  }

  static SGBreak* Create(unsigned d = 1) {
    return styio::session_alloc::make_ir<SGBreak>(d);
  }
};

class SGContinue : public StyioIRTraits<SGContinue>
{
public:
  SGContinue() {
  }

  static SGContinue* Create() {
    return styio::session_alloc::make_ir<SGContinue>();
  }
};

class SGUndef : public StyioIRTraits<SGUndef>
{
public:
  static SGUndef* Create() {
    return styio::session_alloc::make_ir<SGUndef>();
  }
};

class SGFallback : public StyioIRTraits<SGFallback>
{
public:
  StyioIR* primary = nullptr;
  StyioIR* alternate = nullptr;

  SGFallback(StyioIR* p, StyioIR* a) :
      primary(p), alternate(a) {
  }

  ~SGFallback() override {
    delete primary;
    delete alternate;
  }

  static SGFallback* Create(StyioIR* p, StyioIR* a) {
    return styio::session_alloc::make_ir<SGFallback>(p, a);
  }
};

class SGWaveMerge : public StyioIRTraits<SGWaveMerge>
{
public:
  StyioIR* cond = nullptr;
  StyioIR* true_val = nullptr;
  StyioIR* false_val = nullptr;

  SGWaveMerge(StyioIR* c, StyioIR* t, StyioIR* f) :
      cond(c), true_val(t), false_val(f) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGWaveMerge() override {
    delete cond;
    delete true_val;
    delete false_val;
  }

  static SGWaveMerge* Create(StyioIR* c, StyioIR* t, StyioIR* f) {
    return styio::session_alloc::make_ir<SGWaveMerge>(c, t, f);
  }
};

class SGWaveDispatch : public StyioIRTraits<SGWaveDispatch>
{
public:
  StyioIR* cond = nullptr;
  StyioIR* true_arm = nullptr;
  StyioIR* false_arm = nullptr;

  SGWaveDispatch(StyioIR* c, StyioIR* t, StyioIR* f) :
      cond(c), true_arm(t), false_arm(f) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGWaveDispatch() override {
    delete cond;
    delete true_arm;
    delete false_arm;
  }

  static SGWaveDispatch* Create(StyioIR* c, StyioIR* t, StyioIR* f) {
    return styio::session_alloc::make_ir<SGWaveDispatch>(c, t, f);
  }
};

class SGGuardSelect : public StyioIRTraits<SGGuardSelect>
{
public:
  StyioIR* base = nullptr;
  StyioIR* guard_cond = nullptr;

  SGGuardSelect(StyioIR* b, StyioIR* c) :
      base(b), guard_cond(c) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGGuardSelect() override {
    delete base;
    delete guard_cond;
  }

  static SGGuardSelect* Create(StyioIR* b, StyioIR* c) {
    return styio::session_alloc::make_ir<SGGuardSelect>(b, c);
  }
};

class SGEqProbe : public StyioIRTraits<SGEqProbe>
{
public:
  StyioIR* base = nullptr;
  StyioIR* probe = nullptr;

  SGEqProbe(StyioIR* b, StyioIR* p) :
      base(b), probe(p) {
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGEqProbe() override {
    delete base;
    delete probe;
  }

  static SGEqProbe* Create(StyioIR* b, StyioIR* p) {
    return styio::session_alloc::make_ir<SGEqProbe>(b, p);
  }
};

class SGSnapshotDecl : public StyioIRTraits<SGSnapshotDecl>
{
public:
  std::string var_name;
  StyioIR* path_expr = nullptr;

  static SGSnapshotDecl* Create(std::string v, StyioIR* p) {
    auto* x = new SGSnapshotDecl();
    styio::session_alloc::track_raw_allocation(x);
    x->var_name = std::move(v);
    x->path_expr = p;
    return x;
  }

  void collect_children(std::vector<StyioIR*>& out) override;
  ~SGSnapshotDecl() override {
    delete path_expr;
  }
};

class SGSnapshotShadowLoad : public StyioIRTraits<SGSnapshotShadowLoad>
{
public:
  std::string var_name;

  explicit SGSnapshotShadowLoad(std::string v) :
      var_name(std::move(v)) {
  }

  static SGSnapshotShadowLoad* Create(std::string v) {
    return styio::session_alloc::make_ir<SGSnapshotShadowLoad>(std::move(v));
  }
};


// --- out-of-line collect_children() (TASK-08) ---
inline void SGBinOp::collect_children(std::vector<StyioIR*>& out) {
  if (data_type) out.push_back(static_cast<StyioIR*>(data_type));
  if (lhs_expr) out.push_back(static_cast<StyioIR*>(lhs_expr));
  if (rhs_expr) out.push_back(static_cast<StyioIR*>(rhs_expr));
}

inline void SGCond::collect_children(std::vector<StyioIR*>& out) {
  if (lhs_expr) out.push_back(static_cast<StyioIR*>(lhs_expr));
  if (rhs_expr) out.push_back(static_cast<StyioIR*>(rhs_expr));
}

inline void SGVar::collect_children(std::vector<StyioIR*>& out) {
  if (var_name) out.push_back(static_cast<StyioIR*>(var_name));
  if (var_type) out.push_back(static_cast<StyioIR*>(var_type));
  if (val_init) out.push_back(static_cast<StyioIR*>(val_init));
}

inline void SGFlexBind::collect_children(std::vector<StyioIR*>& out) {
  if (var) out.push_back(static_cast<StyioIR*>(var));
  if (value) out.push_back(static_cast<StyioIR*>(value));
}

inline void SGFinalBind::collect_children(std::vector<StyioIR*>& out) {
  if (var) out.push_back(static_cast<StyioIR*>(var));
  if (value) out.push_back(static_cast<StyioIR*>(value));
}

inline void SGFunc::collect_children(std::vector<StyioIR*>& out) {
  if (func_name) out.push_back(static_cast<StyioIR*>(func_name));
  for (auto* c : func_args) out.push_back(static_cast<StyioIR*>(c));
}

inline void SGBlock::collect_children(std::vector<StyioIR*>& out) {
  for (auto* c : stmts) out.push_back(static_cast<StyioIR*>(c));
}

inline void SGEntry::collect_children(std::vector<StyioIR*>& out) {
  for (auto* c : stmts) out.push_back(static_cast<StyioIR*>(c));
}

inline void SGMainEntry::collect_children(std::vector<StyioIR*>& out) {
  for (auto* c : stmts) out.push_back(static_cast<StyioIR*>(c));
}

inline void SGLoop::collect_children(std::vector<StyioIR*>& out) {
  if (cond) out.push_back(static_cast<StyioIR*>(cond));
  if (body) out.push_back(static_cast<StyioIR*>(body));
}

inline void SGSeriesMaxStep::collect_children(std::vector<StyioIR*>& out) {
  if (x) out.push_back(static_cast<StyioIR*>(x));
}

inline void SGForEach::collect_children(std::vector<StyioIR*>& out) {
  if (iterable) out.push_back(static_cast<StyioIR*>(iterable));
  if (body) out.push_back(static_cast<StyioIR*>(body));
}

inline void SGRangeFor::collect_children(std::vector<StyioIR*>& out) {
  if (start) out.push_back(static_cast<StyioIR*>(start));
  if (end) out.push_back(static_cast<StyioIR*>(end));
  if (step) out.push_back(static_cast<StyioIR*>(step));
  if (body) out.push_back(static_cast<StyioIR*>(body));
}

inline void SGIf::collect_children(std::vector<StyioIR*>& out) {
  if (cond) out.push_back(static_cast<StyioIR*>(cond));
  if (then_block) out.push_back(static_cast<StyioIR*>(then_block));
  if (else_block) out.push_back(static_cast<StyioIR*>(else_block));
}

inline void SGMatch::collect_children(std::vector<StyioIR*>& out) {
  if (scrutinee) out.push_back(static_cast<StyioIR*>(scrutinee));
  for (auto& p : int_arms) out.push_back(static_cast<StyioIR*>(p.second));
}

inline void SGWaveMerge::collect_children(std::vector<StyioIR*>& out) {
  if (cond) out.push_back(static_cast<StyioIR*>(cond));
  if (true_val) out.push_back(static_cast<StyioIR*>(true_val));
  if (false_val) out.push_back(static_cast<StyioIR*>(false_val));
}

inline void SGWaveDispatch::collect_children(std::vector<StyioIR*>& out) {
  if (cond) out.push_back(static_cast<StyioIR*>(cond));
  if (true_arm) out.push_back(static_cast<StyioIR*>(true_arm));
  if (false_arm) out.push_back(static_cast<StyioIR*>(false_arm));
}

inline void SGGuardSelect::collect_children(std::vector<StyioIR*>& out) {
  if (base) out.push_back(static_cast<StyioIR*>(base));
  if (guard_cond) out.push_back(static_cast<StyioIR*>(guard_cond));
}

inline void SGEqProbe::collect_children(std::vector<StyioIR*>& out) {
  if (base) out.push_back(static_cast<StyioIR*>(base));
  if (probe) out.push_back(static_cast<StyioIR*>(probe));
}

inline void SGSnapshotDecl::collect_children(std::vector<StyioIR*>& out) {
  if (path_expr) out.push_back(static_cast<StyioIR*>(path_expr));
}


#endif
