/*
  Type Inference Implementation

  - Label Types
  - Find Recursive Type
*/

// [C++ STL]
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioIR/GenIR/GenIR.hpp"
#include "../StyioIR/IOIR/IOIR.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "Util.hpp"

namespace {

int
alloc_pulse_region_id() {
  static int n = 0;
  return n++;
}

SGType*
func_ret_to_sgtype(
  const std::variant<TypeAST*, TypeTupleAST*>& ret_type,
  StyioAnalyzer* an
) {
  if (ret_type.valueless_by_exception()) {
    return SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
  }
  if (std::holds_alternative<TypeTupleAST*>(ret_type)) {
    return SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
  }
  TypeAST* t = std::get<TypeAST*>(ret_type);
  if (!t || t->getDataType().option == StyioDataTypeOption::Undefined) {
    return SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
  }
  return static_cast<SGType*>(t->toStyioIR(an));
}

SGFuncArg*
param_to_sgarg(ParamAST* p, StyioAnalyzer* an) {
  StyioDataTypeOption opty = p->var_type->getDataType().option;
  SGType* ty = (opty == StyioDataTypeOption::Undefined)
    ? SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64})
    : static_cast<SGType*>(p->var_type->toStyioIR(an));
  return SGFuncArg::Create(p->getName(), ty);
}

SGBlock*
lower_func_body(StyioAnalyzer* an, StyioAST* body) {
  if (!body) {
    return SGBlock::Create({});
  }
  if (auto* blk = dynamic_cast<BlockAST*>(body)) {
    return static_cast<SGBlock*>(blk->toStyioIR(an));
  }
  std::vector<StyioIR*> one;
  one.push_back(body->toStyioIR(an));
  return SGBlock::Create(std::move(one));
}

bool
stmt_has_return_tree(StyioAST* ast) {
  if (!ast) {
    return false;
  }
  if (ast->getNodeType() == StyioNodeType::Return) {
    return true;
  }
  if (ast->getNodeType() == StyioNodeType::MatchCases) {
    auto* m = static_cast<MatchCasesAST*>(ast);
    CasesAST* c = m->getCases();
    for (auto const& pr : c->case_list) {
      if (stmt_has_return_tree(pr.second)) {
        return true;
      }
    }
    return stmt_has_return_tree(c->case_default);
  }
  if (auto* b = dynamic_cast<BlockAST*>(ast)) {
    for (auto* s : b->stmts) {
      if (stmt_has_return_tree(s)) {
        return true;
      }
    }
  }
  return false;
}

void
scan_returns_for_str_int(StyioAST* ast, bool& has_str, bool& has_int) {
  if (!ast) {
    return;
  }
  if (ast->getNodeType() == StyioNodeType::Return) {
    StyioAST* e = static_cast<ReturnAST*>(ast)->getExpr();
    if (e->getNodeType() == StyioNodeType::String) {
      has_str = true;
    }
    else if (e->getNodeType() == StyioNodeType::Integer) {
      has_int = true;
    }
    else {
      has_int = true;
    }
    return;
  }
  if (ast->getNodeType() == StyioNodeType::MatchCases) {
    auto* m = static_cast<MatchCasesAST*>(ast);
    CasesAST* c = m->getCases();
    for (auto const& pr : c->case_list) {
      scan_returns_for_str_int(pr.second, has_str, has_int);
    }
    scan_returns_for_str_int(c->case_default, has_str, has_int);
    return;
  }
  if (auto* b = dynamic_cast<BlockAST*>(ast)) {
    for (auto* s : b->stmts) {
      scan_returns_for_str_int(s, has_str, has_int);
    }
  }
}

SGMatchReprKind
classify_cases(CasesAST* c) {
  bool any = false;
  for (auto const& pr : c->case_list) {
    if (stmt_has_return_tree(pr.second)) {
      any = true;
    }
  }
  if (stmt_has_return_tree(c->case_default)) {
    any = true;
  }
  if (!any) {
    return SGMatchReprKind::Stmt;
  }
  bool hs = false;
  bool hi = false;
  for (auto const& pr : c->case_list) {
    scan_returns_for_str_int(pr.second, hs, hi);
  }
  scan_returns_for_str_int(c->case_default, hs, hi);
  if (hs && hi) {
    return SGMatchReprKind::ExprMixed;
  }
  if (hs) {
    /* All arms yield strings (or only strings detected): phi must be i8* */
    return SGMatchReprKind::ExprMixed;
  }
  return SGMatchReprKind::ExprInt;
}

bool
stmt_may_contain_pulse_state(StyioAnalyzer* an, StyioAST* s) {
  if (dynamic_cast<StateDeclAST*>(s)) {
    return true;
  }
  if (auto* fc = dynamic_cast<FuncCallAST*>(s)) {
    auto it = an->func_defs.find(fc->getNameAsStr());
    if (it == an->func_defs.end()) {
      return false;
    }
    if (auto* sf = dynamic_cast<SimpleFuncAST*>(it->second)) {
      auto* blk = dynamic_cast<BlockAST*>(sf->ret_expr);
      if (blk && blk->stmts.size() == 1 && dynamic_cast<StateDeclAST*>(blk->stmts[0])) {
        return true;
      }
    }
  }
  return false;
}

bool
pulse_block_has_state(StyioAnalyzer* an, BlockAST* blk) {
  if (!blk) {
    return false;
  }
  for (auto* s : blk->stmts) {
    if (stmt_may_contain_pulse_state(an, s)) {
      return true;
    }
  }
  return false;
}

SeriesIntrinsicAST*
find_series_intrinsic(StyioAST* e) {
  if (!e) {
    return nullptr;
  }
  if (auto* s = dynamic_cast<SeriesIntrinsicAST*>(e)) {
    return s;
  }
  if (auto* b = dynamic_cast<BinOpAST*>(e)) {
    auto* L = find_series_intrinsic(b->LHS);
    if (L) {
      return L;
    }
    return find_series_intrinsic(b->RHS);
  }
  if (auto* w = dynamic_cast<WaveMergeAST*>(e)) {
    auto* L = find_series_intrinsic(w->getCond());
    if (L) {
      return L;
    }
    L = find_series_intrinsic(w->getTrueVal());
    if (L) {
      return L;
    }
    return find_series_intrinsic(w->getFalseVal());
  }
  return nullptr;
}

int
window_n_from_ast(StyioAST* w) {
  auto* li = dynamic_cast<IntAST*>(w);
  if (!li) {
    throw StyioNotImplemented("window size for series intrinsic must be integer literal");
  }
  return static_cast<int>(std::stoll(li->value));
}

int
slot_byte_size(const SGStateSlotDesc& d) {
  const int n = d.win_n;
  switch (d.kind) {
    case SGStateSlotKind::Acc:
      return 8;
    case SGStateSlotKind::Track:
      return 8 + 8 * n + 8;
    case SGStateSlotKind::WinAvg:
    case SGStateSlotKind::WinMax:
      return n * 8 + 32 + n * 8 + 8;
    default:
      return 8;
  }
}

void
classify_state_slot(StateDeclAST* sd, SGStateSlotDesc& d) {
  if (sd->getAccName()) {
    d.kind = SGStateSlotKind::Acc;
    d.win_n = 0;
    return;
  }
  auto* si = find_series_intrinsic(sd->getUpdateExpr());
  if (si && si->getOp() == SeriesIntrinsicOp::Avg) {
    d.kind = SGStateSlotKind::WinAvg;
    d.win_n = window_n_from_ast(si->getWindow());
    return;
  }
  if (si && si->getOp() == SeriesIntrinsicOp::Max) {
    d.kind = SGStateSlotKind::WinMax;
    d.win_n = window_n_from_ast(si->getWindow());
    return;
  }
  d.kind = SGStateSlotKind::Track;
  if (!sd->getWindowHeader()) {
    throw StyioNotImplemented("@[n] header required for non-accum state");
  }
  d.win_n = static_cast<int>(std::stoll(sd->getWindowHeader()->value));
}

StyioAST*
subst_param_in_expr(StyioAST* e, const std::string& pname, StyioAST* repl) {
  if (!e) {
    return nullptr;
  }
  if (auto* id = dynamic_cast<NameAST*>(e)) {
    if (id->getAsStr() == pname) {
      return repl;
    }
    return e;
  }
  if (auto* b = dynamic_cast<BinOpAST*>(e)) {
    return BinOpAST::Create(
      b->operand,
      subst_param_in_expr(b->LHS, pname, repl),
      subst_param_in_expr(b->RHS, pname, repl));
  }
  if (auto* w = dynamic_cast<WaveMergeAST*>(e)) {
    return WaveMergeAST::Create(
      subst_param_in_expr(w->getCond(), pname, repl),
      subst_param_in_expr(w->getTrueVal(), pname, repl),
      subst_param_in_expr(w->getFalseVal(), pname, repl));
  }
  if (auto* h = dynamic_cast<HistoryProbeAST*>(e)) {
    return HistoryProbeAST::Create(
      h->getTarget(),
      subst_param_in_expr(h->getDepth(), pname, repl));
  }
  if (dynamic_cast<StateRefAST*>(e)) {
    return e;
  }
  if (auto* si = dynamic_cast<SeriesIntrinsicAST*>(e)) {
    return SeriesIntrinsicAST::Create(
      subst_param_in_expr(si->getBase(), pname, repl),
      si->getOp(),
      subst_param_in_expr(si->getWindow(), pname, repl));
  }
  return e;
}

struct PulseScratch {
  std::vector<std::unique_ptr<StateDeclAST>> heap_decls;
};

StateDeclAST*
resolve_state_decl_impl(StyioAnalyzer* an, StyioAST* stmt, PulseScratch* scratch) {
  if (auto* sd = dynamic_cast<StateDeclAST*>(stmt)) {
    return sd;
  }
  auto* fc = dynamic_cast<FuncCallAST*>(stmt);
  if (!fc) {
    return nullptr;
  }
  auto it = an->func_defs.find(fc->getNameAsStr());
  if (it == an->func_defs.end()) {
    throw StyioNotImplemented("unknown function in pulse body");
  }
  auto* sf = dynamic_cast<SimpleFuncAST*>(it->second);
  if (!sf || sf->params.size() != 1 || fc->getArgList().size() != 1) {
    throw StyioNotImplemented("only simple single-arg func->state inlining supported");
  }
  auto* blk = dynamic_cast<BlockAST*>(sf->ret_expr);
  if (!blk || blk->stmts.size() != 1) {
    throw StyioNotImplemented("inlined state func must be a single-statement block");
  }
  auto* sd = dynamic_cast<StateDeclAST*>(blk->stmts[0]);
  if (!sd) {
    throw StyioNotImplemented("inlined func body must be a state decl");
  }
  const std::string& pn = sf->params[0]->getName();
  StyioAST* rep = fc->getArgList()[0];
  StyioAST* new_rhs = subst_param_in_expr(sd->getUpdateExpr(), pn, rep);
  auto* created = StateDeclAST::Create(
    sd->getWindowHeader(),
    sd->getAccName(),
    sd->getAccInit(),
    sd->getExportVar(),
    new_rhs);
  scratch->heap_decls.emplace_back(created);
  return created;
}

StateDeclAST*
resolve_state_decl_cached(
  StyioAnalyzer* an,
  StyioAST* stmt,
  PulseScratch* scratch,
  std::unordered_map<StyioAST*, StateDeclAST*>& cache
) {
  auto itc = cache.find(stmt);
  if (itc != cache.end()) {
    return itc->second;
  }
  StateDeclAST* sd = resolve_state_decl_impl(an, stmt, scratch);
  cache[stmt] = sd;
  return sd;
}

std::unique_ptr<SGPulsePlan>
build_pulse_plan(
  StyioAnalyzer* an,
  BlockAST* blk,
  PulseScratch* scratch,
  std::unordered_map<StyioAST*, StateDeclAST*>& cache
) {
  auto plan = std::make_unique<SGPulsePlan>();
  int off = 0;
  int id = 0;
  for (auto* stmt : blk->stmts) {
    StateDeclAST* sd = resolve_state_decl_cached(an, stmt, scratch, cache);
    if (!sd) {
      continue;
    }
    SGStateSlotDesc d{};
    d.id = id;
    classify_state_slot(sd, d);
    d.offset = off;
    d.size = slot_byte_size(d);
    off += d.size;
    d.acc_name = sd->getAccName() ? sd->getAccName()->getAsStr() : "";
    d.export_name = sd->getExportVar()->getNameAsStr();
    plan->slots.push_back(d);
    plan->commits.push_back({id, d.export_name});
    if (not d.acc_name.empty()) {
      plan->ref_to_slot[d.acc_name] = id;
    }
    plan->ref_to_slot[d.export_name] = id;
    id += 1;
  }
  plan->total_bytes = off;
  return plan;
}

StyioIR*
lower_state_rhs(StyioAnalyzer* an, StyioAST* rhs, int slot_id) {
  if (auto* fc = dynamic_cast<FuncCallAST*>(rhs)) {
    if (fc->getNameAsStr() == "get_ma" && fc->getArgList().size() == 2) {
      auto it = an->func_defs.find("get_ma");
      if (it != an->func_defs.end()) {
        if (auto* sf = dynamic_cast<SimpleFuncAST*>(it->second)) {
          if (sf->params.size() == 2) {
            auto* body = dynamic_cast<SeriesIntrinsicAST*>(sf->ret_expr);
            if (body && body->getOp() == SeriesIntrinsicOp::Avg) {
              an->set_active_series_slot(slot_id);
              StyioIR* xi = fc->getArgList()[0]->toStyioIR(an);
              an->set_active_series_slot(-1);
              return SGSeriesAvgStep::Create(slot_id, xi);
            }
          }
        }
      }
    }
  }
  an->set_active_series_slot(slot_id);
  StyioIR* r = rhs->toStyioIR(an);
  an->set_active_series_slot(-1);
  return r;
}

SGFlexBind*
lower_state_decl_to_flexbind(StyioAnalyzer* an, StateDeclAST* sd, SGPulsePlan* plan) {
  int sid = -1;
  for (size_t i = 0; i < plan->slots.size(); ++i) {
    if (plan->slots[i].export_name == sd->getExportVar()->getNameAsStr()) {
      sid = static_cast<int>(i);
      break;
    }
  }
  if (sid < 0) {
    throw StyioNotImplemented("state slot not in plan");
  }
  StyioIR* rhs = lower_state_rhs(an, sd->getUpdateExpr(), sid);
  return SGFlexBind::Create(
    static_cast<SGVar*>(sd->getExportVar()->toStyioIR(an)),
    rhs);
}

SGBlock*
lower_pulse_body(
  StyioAnalyzer* an,
  BlockAST* blk,
  SGPulsePlan* plan,
  PulseScratch* scratch,
  std::unordered_map<StyioAST*, StateDeclAST*>& cache
) {
  an->set_cur_pulse_plan(plan);
  std::vector<StyioIR*> stmts;
  for (auto* s : blk->stmts) {
    StateDeclAST* sd = resolve_state_decl_cached(an, s, scratch, cache);
    if (sd) {
      stmts.push_back(lower_state_decl_to_flexbind(an, sd, plan));
    }
    else {
      stmts.push_back(s->toStyioIR(an));
    }
  }
  an->set_cur_pulse_plan(nullptr);
  return SGBlock::Create(std::move(stmts));
}

}  // namespace

static StyioOpType
comp_type_to_op(CompType ct) {
  switch (ct) {
    case CompType::EQ:
      return StyioOpType::Equal;
    case CompType::NE:
      return StyioOpType::Not_Equal;
    case CompType::GT:
      return StyioOpType::Greater_Than;
    case CompType::GE:
      return StyioOpType::Greater_Than_Equal;
    case CompType::LT:
      return StyioOpType::Less_Than;
    case CompType::LE:
      return StyioOpType::Less_Than_Equal;
    default:
      return StyioOpType::Equal;
  }
}

StyioIR*
StyioAnalyzer::toStyioIR(CommentAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(NoneAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(EmptyAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(NameAST* ast) {
  return SGResId::Create(ast->getAsStr());
}

StyioIR*
StyioAnalyzer::toStyioIR(TypeAST* ast) {
  return SGType::Create(ast->type);
}

StyioIR*
StyioAnalyzer::toStyioIR(TypeTupleAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(BoolAST* ast) {
  return SGConstBool::Create(ast->getValue());
}

StyioIR*
StyioAnalyzer::toStyioIR(IntAST* ast) {
  return SGConstInt::Create(ast->value, ast->num_of_bit);
}

StyioIR*
StyioAnalyzer::toStyioIR(FloatAST* ast) {
  return SGConstFloat::Create(ast->value);
}

StyioIR*
StyioAnalyzer::toStyioIR(CharAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(StringAST* ast) {
  const std::string& raw = ast->getValue();
  std::string inner = raw;
  if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
    inner = raw.substr(1, raw.size() - 2);
  }
  return SGConstString::Create(inner);
}

StyioIR*
StyioAnalyzer::toStyioIR(TypeConvertAST*) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(VarAST* ast) {
  if (ast->val_init) {
    return SGVar::Create(
      static_cast<SGResId*>(ast->var_name->toStyioIR(this)),
      static_cast<SGType*>(ast->var_type->toStyioIR(this)),
      ast->val_init->toStyioIR(this)
    );
  }
  else {
    return SGVar::Create(
      static_cast<SGResId*>(ast->var_name->toStyioIR(this)),
      static_cast<SGType*>(ast->var_type->toStyioIR(this))
    );
  }
}

StyioIR*
StyioAnalyzer::toStyioIR(ParamAST* ast) {
  if (ast->val_init) {
    return SGVar::Create(
      static_cast<SGResId*>(ast->var_name->toStyioIR(this)),
      static_cast<SGType*>(ast->var_type->toStyioIR(this)),
      ast->val_init->toStyioIR(this)
    );
  }
  else {
    return SGVar::Create(
      static_cast<SGResId*>(ast->var_name->toStyioIR(this)),
      static_cast<SGType*>(ast->var_type->toStyioIR(this))
    );
  }
}

StyioIR*
StyioAnalyzer::toStyioIR(OptArgAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(OptKwArgAST* ast) {
  return SGConstInt::Create(0);
}

/*
  The declared type is always the *top* priority
  because the programmer wrote in that way!
*/
StyioIR*
StyioAnalyzer::toStyioIR(FlexBindAST* ast) {
  return SGFlexBind::Create(
    static_cast<SGVar*>(ast->getVar()->toStyioIR(this)), 
    ast->getValue()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(FinalBindAST* ast) {
  return SGFinalBind::Create(
    static_cast<SGVar*>(ast->getVar()->toStyioIR(this)), 
    ast->getValue()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(InfiniteAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(StructAST* ast) {
  std::vector<SGVar*> elems;

  for (auto arg : ast->args) {
    elems.push_back(static_cast<SGVar*>(arg->toStyioIR(this)));
  }

  return SGStruct::Create(SGResId::Create(ast->name->getAsStr()), elems);
}

StyioIR*
StyioAnalyzer::toStyioIR(TupleAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(VarTupleAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ExtractorAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(RangeAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(SetAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ListAST* ast) {
  std::vector<StyioIR*> el;
  for (auto* e : ast->getElements()) {
    el.push_back(e->toStyioIR(this));
  }
  return SGListLiteral::Create(std::move(el));
}

StyioIR*
StyioAnalyzer::toStyioIR(SizeOfAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ListOpAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(BinCompAST* ast) {
  StyioOpType op = comp_type_to_op(ast->getSign());
  return SGBinOp::Create(
    ast->getLHS()->toStyioIR(this),
    ast->getRHS()->toStyioIR(this),
    op,
    SGType::Create(StyioDataType{StyioDataTypeOption::Bool, "bool", 1}));
}

StyioIR*
StyioAnalyzer::toStyioIR(CondAST* ast) {
  switch (ast->getSign()) {
    case LogicType::AND:
      return SGCond::Create(
        ast->getLHS()->toStyioIR(this),
        ast->getRHS()->toStyioIR(this),
        StyioOpType::Logic_AND);
    case LogicType::OR:
      return SGCond::Create(
        ast->getLHS()->toStyioIR(this),
        ast->getRHS()->toStyioIR(this),
        StyioOpType::Logic_OR);
    case LogicType::RAW:
      return ast->getValue()->toStyioIR(this);
    default:
      return ast->getValue() ? ast->getValue()->toStyioIR(this) : SGConstInt::Create(0);
  }
}

StyioIR*
StyioAnalyzer::toStyioIR(UndefinedLitAST* ast) {
  (void)ast;
  return SGUndef::Create();
}

StyioIR*
StyioAnalyzer::toStyioIR(WaveMergeAST* ast) {
  return SGWaveMerge::Create(
    ast->getCond()->toStyioIR(this),
    ast->getTrueVal()->toStyioIR(this),
    ast->getFalseVal()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(WaveDispatchAST* ast) {
  return SGWaveDispatch::Create(
    ast->getCond()->toStyioIR(this),
    ast->getTrueArm()->toStyioIR(this),
    ast->getFalseArm()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(FallbackAST* ast) {
  return SGFallback::Create(
    ast->getPrimary()->toStyioIR(this),
    ast->getAlternate()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(GuardSelectorAST* ast) {
  return SGGuardSelect::Create(
    ast->getBase()->toStyioIR(this),
    ast->getCond()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(EqProbeAST* ast) {
  return SGEqProbe::Create(
    ast->getBase()->toStyioIR(this),
    ast->getProbeValue()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(FileResourceAST* ast) {
  return ast->getPath()->toStyioIR(this);
}

StyioIR*
StyioAnalyzer::toStyioIR(HandleAcquireAST* ast) {
  auto* fr = dynamic_cast<FileResourceAST*>(ast->getResource());
  if (!fr) {
    throw StyioNotImplemented("handle acquire needs @file{...} or @{...}");
  }
  return SGHandleAcquire::Create(
    ast->getVar()->getNameAsStr(),
    fr->getPath()->toStyioIR(this),
    fr->isAutoDetect());
}

StyioIR*
StyioAnalyzer::toStyioIR(ResourceWriteAST* ast) {
  auto* fr = dynamic_cast<FileResourceAST*>(ast->getResource());
  if (!fr) {
    throw StyioNotImplemented("<< target must be a file resource");
  }
  return SGResourceWriteToFile::Create(
    ast->getData()->toStyioIR(this),
    fr->getPath()->toStyioIR(this),
    fr->isAutoDetect(),
    false);
}

StyioIR*
StyioAnalyzer::toStyioIR(ResourceRedirectAST* ast) {
  auto* fr = dynamic_cast<FileResourceAST*>(ast->getResource());
  if (!fr) {
    throw StyioNotImplemented("-> target must be a file resource");
  }
  return SGResourceWriteToFile::Create(
    ast->getData()->toStyioIR(this),
    fr->getPath()->toStyioIR(this),
    fr->isAutoDetect(),
    true);
}

/*
  Int -> Int => Pass
  Int -> Float => Pass
*/
StyioIR*
StyioAnalyzer::toStyioIR(BinOpAST* ast) {
  return SGBinOp::Create(
    ast->LHS->toStyioIR(this),
    ast->RHS->toStyioIR(this),
    ast->operand,
    static_cast<SGType*>(ast->data_type->toStyioIR(this)));
}

StyioIR*
StyioAnalyzer::toStyioIR(FmtStrAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ResourceAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ResPathAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(RemotePathAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(WebUrlAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(DBUrlAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ExtPackAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ReadFileAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(EOFAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(BreakAST* ast) {
  return SGBreak::Create(ast->getDepth());
}

StyioIR*
StyioAnalyzer::toStyioIR(ContinueAST* ast) {
  return SGContinue::Create(ast->getDepth());
}

StyioIR*
StyioAnalyzer::toStyioIR(PassAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(ReturnAST* ast) {
  return SGReturn::Create(ast->getExpr()->toStyioIR(this));
}

StyioIR*
StyioAnalyzer::toStyioIR(FuncCallAST* ast) {
  std::vector<StyioIR*> args;
  for (auto* a : ast->getArgList()) {
    args.push_back(a->toStyioIR(this));
  }
  return SGCall::Create(
    SGResId::Create(ast->getNameAsStr()),
    std::move(args));
}

StyioIR*
StyioAnalyzer::toStyioIR(AttrAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(PrintAST* ast) {
  std::vector<StyioIR*> parts;
  for (auto* e : ast->exprs) {
    parts.push_back(e->toStyioIR(this));
  }
  return SIOPrint::Create(parts);
}

StyioIR*
StyioAnalyzer::toStyioIR(ForwardAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(BackwardAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(CODPAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(CheckEqualAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(CheckIsinAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(HashTagNameAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(CondFlowAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(AnonyFuncAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(FunctionAST* ast) {
  int saved_hist_r = post_pulse_hist_region_;
  SGPulsePlan* saved_hist_p = post_pulse_hist_plan_;
  set_post_pulse_hist_context(-1, nullptr);
  std::vector<SGFuncArg*> fargs;
  for (auto* p : ast->params) {
    fargs.push_back(param_to_sgarg(p, this));
  }
  SGType* rt = func_ret_to_sgtype(ast->ret_type, this);
  if (auto* blk = dynamic_cast<BlockAST*>(ast->func_body)) {
    if (blk->stmts.size() == 1 && blk->stmts[0]->getNodeType() == StyioNodeType::MatchCases) {
      auto* mc = static_cast<MatchCasesAST*>(blk->stmts[0]);
      CasesAST* c = mc->getCases();
      bool hs = false;
      bool hi = false;
      for (auto const& pr : c->case_list) {
        scan_returns_for_str_int(pr.second, hs, hi);
      }
      scan_returns_for_str_int(c->case_default, hs, hi);
      if (hs) {
        rt = SGType::Create(StyioDataType{StyioDataTypeOption::String, "string", 0});
      }
    }
  }
  SGBlock* body = lower_func_body(this, ast->func_body);
  SGFunc* fn = SGFunc::Create(
    rt,
    SGResId::Create(ast->getNameAsStr()),
    std::move(fargs),
    body);
  set_post_pulse_hist_context(saved_hist_r, saved_hist_p);
  return fn;
}

StyioIR*
StyioAnalyzer::toStyioIR(SimpleFuncAST* ast) {
  int saved_hist_r = post_pulse_hist_region_;
  SGPulsePlan* saved_hist_p = post_pulse_hist_plan_;
  set_post_pulse_hist_context(-1, nullptr);
  std::vector<SGFuncArg*> fargs;
  for (auto* p : ast->params) {
    fargs.push_back(param_to_sgarg(p, this));
  }
  SGType* rt = func_ret_to_sgtype(ast->ret_type, this);
  if (auto* blk = dynamic_cast<BlockAST*>(ast->ret_expr)) {
    if (blk->stmts.size() == 1 && blk->stmts[0]->getNodeType() == StyioNodeType::MatchCases) {
      auto* mc = static_cast<MatchCasesAST*>(blk->stmts[0]);
      CasesAST* c = mc->getCases();
      bool hs = false;
      bool hi = false;
      for (auto const& pr : c->case_list) {
        scan_returns_for_str_int(pr.second, hs, hi);
      }
      scan_returns_for_str_int(c->case_default, hs, hi);
      if (hs) {
        /* Any <| "..." arm: LLVM return must be i8* (and may mix with snprintf ints). */
        rt = SGType::Create(StyioDataType{StyioDataTypeOption::String, "string", 0});
      }
    }
  }
  SGBlock* body = lower_func_body(this, ast->ret_expr);
  SGFunc* fn = SGFunc::Create(
    rt,
    SGResId::Create(ast->func_name->getAsStr()),
    std::move(fargs),
    body);
  set_post_pulse_hist_context(saved_hist_r, saved_hist_p);
  return fn;
}

StyioIR*
StyioAnalyzer::toStyioIR(IteratorAST* ast) {
  std::string vname = "it";
  if (!ast->params.empty()) {
    vname = ast->params[0]->getName();
  }
  SGBlock* body = SGBlock::Create({});
  std::unique_ptr<SGPulsePlan> pplan;
  if (!ast->following.empty()) {
    auto* abody = dynamic_cast<BlockAST*>(ast->following[0]);
    if (abody && pulse_block_has_state(this, abody)) {
      PulseScratch scratch;
      std::unordered_map<StyioAST*, StateDeclAST*> cache;
      pplan = build_pulse_plan(this, abody, &scratch, cache);
      body = lower_pulse_body(this, abody, pplan.get(), &scratch, cache);
    }
    else {
      body = lower_func_body(this, ast->following[0]);
    }
  }
  if (ast->collection->getNodeType() == StyioNodeType::FileResource) {
    auto* fr = static_cast<FileResourceAST*>(ast->collection);
    auto* fl = SGFileLineIter::CreateFromPath(
      fr->getPath()->toStyioIR(this),
      std::move(vname),
      body);
    if (pplan) {
      fl->set_pulse_plan(std::move(pplan));
      if (fl->pulse_plan && fl->pulse_plan->total_bytes > 0) {
        fl->pulse_region_id = alloc_pulse_region_id();
      }
    }
    return fl;
  }
  if (ast->collection->getNodeType() == StyioNodeType::Id) {
    auto* nm = static_cast<NameAST*>(ast->collection);
    auto* fl = SGFileLineIter::CreateFromHandle(
      nm->getAsStr(),
      std::move(vname),
      body);
    if (pplan) {
      fl->set_pulse_plan(std::move(pplan));
      if (fl->pulse_plan && fl->pulse_plan->total_bytes > 0) {
        fl->pulse_region_id = alloc_pulse_region_id();
      }
    }
    return fl;
  }
  auto* fe = SGForEach::Create(ast->collection->toStyioIR(this), std::move(vname), body);
  if (pplan) {
    fe->set_pulse_plan(std::move(pplan));
    if (fe->pulse_plan && fe->pulse_plan->total_bytes > 0) {
      fe->pulse_region_id = alloc_pulse_region_id();
    }
  }
  return fe;
}

StyioIR*
StyioAnalyzer::toStyioIR(IterSeqAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(InfiniteLoopAST* ast) {
  SGBlock* b = lower_func_body(this, ast->getBody());
  if (ast->getWhileCond()) {
    return SGLoop::CreateWhile(ast->getWhileCond()->toStyioIR(this), b);
  }
  return SGLoop::CreateInfinite(b);
}

StyioIR*
StyioAnalyzer::toStyioIR(CasesAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(StateDeclAST* ast) {
  (void)ast;
  return SGConstInt::Create(0);
}

StyioIR*
StyioAnalyzer::toStyioIR(StateRefAST* ast) {
  auto* pl = cur_pulse_plan();
  if (!pl) {
    throw StyioNotImplemented("$state only valid inside pulse body");
  }
  auto it = pl->ref_to_slot.find(ast->getNameStr());
  if (it == pl->ref_to_slot.end()) {
    throw StyioNotImplemented("unknown state reference");
  }
  return SGStateSnapLoad::Create(it->second);
}

StyioIR*
StyioAnalyzer::toStyioIR(HistoryProbeAST* ast) {
  auto* pl = cur_pulse_plan();
  int ledger_region = -1;
  if (!pl) {
    pl = post_pulse_hist_plan_;
    ledger_region = post_pulse_hist_region_;
    if (!pl || ledger_region < 0) {
      throw StyioNotImplemented(
        "history probe only valid inside pulse body or after foreach/file line iter with pulse");
    }
  }
  std::string nm = ast->getTarget()->getNameStr();
  auto it = pl->ref_to_slot.find(nm);
  if (it == pl->ref_to_slot.end()) {
    throw StyioNotImplemented("unknown state in history probe");
  }
  int dep = window_n_from_ast(ast->getDepth());
  return SGStateHistLoad::Create(it->second, dep, ledger_region);
}

StyioIR*
StyioAnalyzer::toStyioIR(SeriesIntrinsicAST* ast) {
  int sid = active_series_slot();
  if (sid < 0) {
    throw StyioNotImplemented("series intrinsic needs enclosing state slot");
  }
  StyioIR* bx = ast->getBase()->toStyioIR(this);
  if (ast->getOp() == SeriesIntrinsicOp::Avg) {
    return SGSeriesAvgStep::Create(sid, bx);
  }
  return SGSeriesMaxStep::Create(sid, bx);
}

StyioIR*
StyioAnalyzer::toStyioIR(MatchCasesAST* ast) {
  CasesAST* c = ast->getCases();
  SGMatchReprKind rk = classify_cases(c);
  StyioIR* scr = ast->getScrutinee()->toStyioIR(this);
  std::vector<std::pair<std::int64_t, SGBlock*>> arms;
  for (auto const& pr : c->case_list) {
    auto* li = dynamic_cast<IntAST*>(pr.first);
    if (!li) {
      throw StyioNotImplemented("match arms need integer literal patterns in this milestone");
    }
    arms.push_back({std::stoll(li->value), lower_func_body(this, pr.second)});
  }
  SGBlock* def = nullptr;
  if (c->case_default) {
    def = lower_func_body(this, c->case_default);
  }
  return SGMatch::Create(scr, std::move(arms), def, rk);
}

StyioIR*
StyioAnalyzer::toStyioIR(BlockAST* ast) {
  std::vector<StyioIR*> ir_stmts;
  for (auto* s : ast->stmts) {
    ir_stmts.push_back(s->toStyioIR(this));
  }
  return SGBlock::Create(ir_stmts);
}

StyioIR*
StyioAnalyzer::toStyioIR(MainBlockAST* ast) {
  std::vector<StyioIR*> ir_stmts;
  int pending_region = -1;
  SGPulsePlan* pending_plan = nullptr;

  for (auto stmt : ast->getStmts()) {
    set_post_pulse_hist_context(pending_region, pending_plan);
    StyioIR* ir = stmt->toStyioIR(this);
    if (auto* fe = dynamic_cast<SGForEach*>(ir)) {
      if (fe->pulse_plan && fe->pulse_plan->total_bytes > 0) {
        pending_region = fe->pulse_region_id;
        pending_plan = fe->pulse_plan.get();
      }
    }
    else if (auto* fl = dynamic_cast<SGFileLineIter*>(ir)) {
      if (fl->pulse_plan && fl->pulse_plan->total_bytes > 0) {
        pending_region = fl->pulse_region_id;
        pending_plan = fl->pulse_plan.get();
      }
    }
    ir_stmts.push_back(ir);
  }
  set_post_pulse_hist_context(-1, nullptr);

  return SGMainEntry::Create(ir_stmts);
}
