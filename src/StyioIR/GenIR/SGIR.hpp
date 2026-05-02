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
  SGResId(std::string id) :
      id(id) {
  }

  static SGResId* Create() {
    return new SGResId("");
  }

  static SGResId* Create(std::string id) {
    return new SGResId(id);
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
    return new SGType(data_type);
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
    return new SGConstBool(value);
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
    return new SGConstInt(std::to_string(value), 64);
  }

  static SGConstInt* Create(std::string value) {
    return new SGConstInt(value, 64);
  }

  static SGConstInt* Create(std::string value, size_t numbits) {
    return new SGConstInt(value, numbits);
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
    return new SGConstFloat(value);
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
    return new SGConstChar(value);
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
    return new SGConstString(value);
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
    return new SGFormatString(fragments, expressions);
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
    return new SGStruct(elements);
  }

  static SGStruct* Create(SGResId* name, std::vector<SGVar*> elements) {
    return new SGStruct(name, elements);
  }
};

class SGCast : public StyioIRTraits<SGCast>
{
public:
  SGType* from_type;
  SGType* to_type;

  SGCast(SGType* from_type, SGType* to_type) :
      from_type(from_type), to_type(to_type) {
  }

  static SGCast* Create(SGType* from_type, SGType* to_type) {
    return new SGCast(from_type, to_type);
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

  SGBinOp(StyioIR* lhs, StyioIR* rhs, StyioOpType op, SGType* data_type) :
      lhs_expr(std::move(lhs)), rhs_expr(std::move(rhs)), operand(op), data_type(std::move(data_type)) {
  }

  static SGBinOp* Create(StyioIR* lhs, StyioIR* rhs, StyioOpType op, SGType* data_type) {
    return new SGBinOp(lhs, rhs, op, data_type);
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

  static SGCond* Create(StyioIR* lhs, StyioIR* rhs, StyioOpType op) {
    return new SGCond(lhs, rhs, op);
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

  static SGVar* Create(SGResId* id, SGType* type) {
    return new SGVar(id, type);
  }

  static SGVar* Create(SGResId* id, SGType* type, StyioIR* value) {
    return new SGVar(id, type, value);
  }
};

class SGFlexBind : public StyioIRTraits<SGFlexBind>
{
public:
  SGVar* var;
  StyioIR* value;

  SGFlexBind(SGVar* var, StyioIR* value) :
      var(var), value(value) {
  }

  static SGFlexBind* Create(SGVar* id, StyioIR* value) {
    return new SGFlexBind(id, value);
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

  static SGFinalBind* Create(SGVar* id, StyioIR* value) {
    return new SGFinalBind(id, value);
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
    return new SGDynLoad(std::move(name), kind);
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

  static SGFuncArg* Create(std::string id, SGType* type) {
    return new SGFuncArg(id, type);
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

  static SGFunc* Create(SGType* ret_type, SGResId* func_name, std::vector<SGFuncArg*> func_args, SGBlock* func_block) {
    return new SGFunc(ret_type, func_name, func_args, func_block);
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

  static SGCall* Create(SGResId* func_name, std::vector<StyioIR*> func_args) {
    return new SGCall(std::move(func_name), std::move(func_args));
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
    return new SGExportDecl(std::move(symbols));
  }
};

class SGExternBlock : public StyioIRTraits<SGExternBlock>
{
public:
  std::string abi;
  std::string body;

  SGExternBlock(std::string abi, std::string body) :
      abi(std::move(abi)),
      body(std::move(body)) {
  }

  static SGExternBlock* Create(std::string abi, std::string body) {
    return new SGExternBlock(std::move(abi), std::move(body));
  }
};

class SGReturn : public StyioIRTraits<SGReturn>
{
public:
  StyioIR* expr;

  SGReturn(StyioIR* expr) :
      expr(expr) {
  }

  static SGReturn* Create(StyioIR* expr) {
    return new SGReturn(expr);
  }
};

class SGBlock : public StyioIRTraits<SGBlock>
{
public:
  std::vector<StyioIR*> stmts;

  SGBlock(std::vector<StyioIR*> stmts) :
      stmts(std::move(stmts)) {
  }

  static SGBlock* Create(std::vector<StyioIR*> stmts) {
    return new SGBlock(std::move(stmts));
  }
};

class SGEntry : public StyioIRTraits<SGEntry>
{
public:
  std::vector<StyioIR*> stmts;

  SGEntry(std::vector<StyioIR*> stmts) :
      stmts(std::move(stmts)) {
  }

  static SGEntry* Create(std::vector<StyioIR*> stmts) {
    return new SGEntry(std::move(stmts));
  }
};

class SGMainEntry : public StyioIRTraits<SGMainEntry>
{
public:
  std::vector<StyioIR*> stmts;

  SGMainEntry(std::vector<StyioIR*> stmts) :
      stmts(stmts) {
  }

  static SGMainEntry* Create(std::vector<StyioIR*> stmts) {
    return new SGMainEntry(stmts);
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

  static SGLoop* CreateInfinite(SGBlock* b) {
    return new SGLoop(SGLoopTag::Infinite, nullptr, b);
  }

  static SGLoop* CreateWhile(StyioIR* c, SGBlock* b) {
    return new SGLoop(SGLoopTag::WhileCond, c, b);
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
    return new SGStateSnapLoad(s);
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
    return new SGStateHistLoad(s, d, region);
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

  static SGSeriesAvgStep* Create(int s, StyioIR* xi) {
    return new SGSeriesAvgStep(s, xi);
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

  static SGSeriesMaxStep* Create(int s, StyioIR* xi) {
    return new SGSeriesMaxStep(s, xi);
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

  static SGForEach* Create(StyioIR* it, std::string v, std::string elem_type, SGBlock* b) {
    return new SGForEach(it, std::move(v), std::move(elem_type), b);
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

  static SGRangeFor* Create(StyioIR* s, StyioIR* e, StyioIR* st, std::string v, SGBlock* b) {
    return new SGRangeFor(s, e, st, std::move(v), b);
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

  static SGIf* Create(StyioIR* cond, SGBlock* then_block, SGBlock* else_block = nullptr) {
    return new SGIf(cond, then_block, else_block);
  }
};

enum class SGMatchReprKind
{
  Stmt,
  ExprInt,
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

  static SGMatch* Create(
    StyioIR* s,
    std::vector<std::pair<std::int64_t, SGBlock*>> arms,
    SGBlock* d,
    SGMatchReprKind k
  ) {
    return new SGMatch(s, std::move(arms), d, k);
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
    return new SGBreak(d);
  }
};

class SGContinue : public StyioIRTraits<SGContinue>
{
public:
  unsigned depth = 1;

  explicit SGContinue(unsigned d) :
      depth(d) {
  }

  static SGContinue* Create(unsigned d = 1) {
    return new SGContinue(d);
  }
};

class SGUndef : public StyioIRTraits<SGUndef>
{
public:
  static SGUndef* Create() {
    return new SGUndef();
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

  static SGFallback* Create(StyioIR* p, StyioIR* a) {
    return new SGFallback(p, a);
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

  static SGWaveMerge* Create(StyioIR* c, StyioIR* t, StyioIR* f) {
    return new SGWaveMerge(c, t, f);
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

  static SGWaveDispatch* Create(StyioIR* c, StyioIR* t, StyioIR* f) {
    return new SGWaveDispatch(c, t, f);
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

  static SGGuardSelect* Create(StyioIR* b, StyioIR* c) {
    return new SGGuardSelect(b, c);
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

  static SGEqProbe* Create(StyioIR* b, StyioIR* p) {
    return new SGEqProbe(b, p);
  }
};

class SGSnapshotDecl : public StyioIRTraits<SGSnapshotDecl>
{
public:
  std::string var_name;
  StyioIR* path_expr = nullptr;

  static SGSnapshotDecl* Create(std::string v, StyioIR* p) {
    auto* x = new SGSnapshotDecl();
    x->var_name = std::move(v);
    x->path_expr = p;
    return x;
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
    return new SGSnapshotShadowLoad(std::move(v));
  }
};

#endif
