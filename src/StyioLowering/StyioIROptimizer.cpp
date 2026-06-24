/*
  StyioIR canonicalization and local optimization passes.
*/

#include "StyioIROptimizer.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "../StyioException/Exception.hpp"
#include "../StyioIR/GenIR/GenIR.hpp"
#include "../StyioIR/StyioIRWalker.hpp"
#include "../StyioToString/ToStringVisitor.hpp"

namespace styio::lowering {
namespace {

bool
same_type(const StyioDataType& lhs, const StyioDataType& rhs) {
  return lhs.equals(rhs);
}

std::optional<std::string>
res_id_name(StyioIR* ir) {
  if (auto* id = dynamic_cast<SGResId*>(ir)) {
    return id->as_str();
  }
  if (auto* dyn = dynamic_cast<SGDynLoad*>(ir)) {
    return dyn->var_name;
  }
  return std::nullopt;
}

bool
ir_expr_equiv(StyioIR* lhs, StyioIR* rhs) {
  if (lhs == rhs) {
    return true;
  }
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }

  if (auto* li = dynamic_cast<SGResId*>(lhs)) {
    auto* ri = dynamic_cast<SGResId*>(rhs);
    return ri != nullptr && li->as_str() == ri->as_str();
  }
  if (auto* li = dynamic_cast<SGDynLoad*>(lhs)) {
    auto* ri = dynamic_cast<SGDynLoad*>(rhs);
    return ri != nullptr && li->var_name == ri->var_name && li->kind == ri->kind;
  }
  if (auto* li = dynamic_cast<SGConstBool*>(lhs)) {
    auto* ri = dynamic_cast<SGConstBool*>(rhs);
    return ri != nullptr && li->value == ri->value;
  }
  if (auto* li = dynamic_cast<SGConstInt*>(lhs)) {
    auto* ri = dynamic_cast<SGConstInt*>(rhs);
    return ri != nullptr && li->value == ri->value && li->num_of_bit == ri->num_of_bit;
  }
  if (auto* lf = dynamic_cast<SGConstFloat*>(lhs)) {
    auto* rf = dynamic_cast<SGConstFloat*>(rhs);
    return rf != nullptr && lf->value == rf->value;
  }
  if (auto* lc = dynamic_cast<SGCast*>(lhs)) {
    auto* rc = dynamic_cast<SGCast*>(rhs);
    return rc != nullptr
      && lc->from_type != nullptr
      && rc->from_type != nullptr
      && lc->to_type != nullptr
      && rc->to_type != nullptr
      && same_type(lc->from_type->data_type, rc->from_type->data_type)
      && same_type(lc->to_type->data_type, rc->to_type->data_type)
      && ir_expr_equiv(lc->value, rc->value);
  }
  if (auto* ls = dynamic_cast<SGConstString*>(lhs)) {
    auto* rs = dynamic_cast<SGConstString*>(rhs);
    return rs != nullptr && ls->value == rs->value;
  }
  if (auto* lb = dynamic_cast<SGBinOp*>(lhs)) {
    auto* rb = dynamic_cast<SGBinOp*>(rhs);
    return rb != nullptr
      && lb->operand == rb->operand
      && same_type(lb->data_type->data_type, rb->data_type->data_type)
      && same_type(lb->lhs_type, rb->lhs_type)
      && same_type(lb->rhs_type, rb->rhs_type)
      && ir_expr_equiv(lb->lhs_expr, rb->lhs_expr)
      && ir_expr_equiv(lb->rhs_expr, rb->rhs_expr);
  }
  if (auto* lc = dynamic_cast<SGCond*>(lhs)) {
    auto* rc = dynamic_cast<SGCond*>(rhs);
    return rc != nullptr
      && lc->operand == rc->operand
      && ir_expr_equiv(lc->lhs_expr, rc->lhs_expr)
      && ir_expr_equiv(lc->rhs_expr, rc->rhs_expr);
  }
  if (auto* ll = dynamic_cast<SCListLen*>(lhs)) {
    auto* rl = dynamic_cast<SCListLen*>(rhs);
    return rl != nullptr && ir_expr_equiv(ll->list, rl->list);
  }
  if (auto* ld = dynamic_cast<SCDictLen*>(lhs)) {
    auto* rd = dynamic_cast<SCDictLen*>(rhs);
    return rd != nullptr && ir_expr_equiv(ld->dict, rd->dict);
  }

  return false;
}

bool
is_speculatable_op(StyioOpType op) {
  switch (op) {
    case StyioOpType::Unary_Positive:
    case StyioOpType::Unary_Negative:
    case StyioOpType::Binary_Add:
    case StyioOpType::Binary_Sub:
    case StyioOpType::Binary_Mul:
    case StyioOpType::Bitwise_NOT:
    case StyioOpType::Bitwise_AND:
    case StyioOpType::Bitwise_OR:
    case StyioOpType::Bitwise_XOR:
    case StyioOpType::Bitwise_Left_Shift:
    case StyioOpType::Bitwise_Right_Shift:
    case StyioOpType::Greater_Than:
    case StyioOpType::Less_Than:
    case StyioOpType::Greater_Than_Equal:
    case StyioOpType::Less_Than_Equal:
    case StyioOpType::Equal:
    case StyioOpType::Not_Equal:
      return true;
    default:
      return false;
  }
}

bool
ir_expr_is_speculatable(StyioIR* ir) {
  if (ir == nullptr) {
    return false;
  }
  if (dynamic_cast<SGResId*>(ir)
      || dynamic_cast<SGDynLoad*>(ir)
      || dynamic_cast<SGConstBool*>(ir)
      || dynamic_cast<SGConstInt*>(ir)
      || dynamic_cast<SGConstFloat*>(ir)
      || dynamic_cast<SGConstString*>(ir)) {
    return true;
  }
  if (auto* cast = dynamic_cast<SGCast*>(ir)) {
    return ir_expr_is_speculatable(cast->value);
  }
  if (auto* op = dynamic_cast<SGBinOp*>(ir)) {
    return is_speculatable_op(op->operand)
      && ir_expr_is_speculatable(op->lhs_expr)
      && ir_expr_is_speculatable(op->rhs_expr);
  }
  if (auto* cond = dynamic_cast<SGCond*>(ir)) {
    return is_speculatable_op(cond->operand)
      && ir_expr_is_speculatable(cond->lhs_expr)
      && ir_expr_is_speculatable(cond->rhs_expr);
  }
  if (auto* len = dynamic_cast<SCListLen*>(ir)) {
    return ir_expr_is_speculatable(len->list);
  }
  if (auto* len = dynamic_cast<SCDictLen*>(ir)) {
    return ir_expr_is_speculatable(len->dict);
  }
  return false;
}

bool
ir_expr_has_no_runtime_effects(StyioIR* ir) {
  if (ir == nullptr) {
    return true;
  }
  if (dynamic_cast<SGResId*>(ir)
      || dynamic_cast<SGDynLoad*>(ir)
      || dynamic_cast<SGConstBool*>(ir)
      || dynamic_cast<SGConstInt*>(ir)
      || dynamic_cast<SGConstFloat*>(ir)
      || dynamic_cast<SGConstString*>(ir)) {
    return true;
  }
  if (auto* cast = dynamic_cast<SGCast*>(ir)) {
    return ir_expr_has_no_runtime_effects(cast->value);
  }
  if (auto* op = dynamic_cast<SGBinOp*>(ir)) {
    return ir_expr_has_no_runtime_effects(op->lhs_expr)
      && ir_expr_has_no_runtime_effects(op->rhs_expr);
  }
  if (auto* cond = dynamic_cast<SGCond*>(ir)) {
    return ir_expr_has_no_runtime_effects(cond->lhs_expr)
      && ir_expr_has_no_runtime_effects(cond->rhs_expr);
  }
  if (auto* len = dynamic_cast<SCListLen*>(ir)) {
    return ir_expr_has_no_runtime_effects(len->list);
  }
  if (auto* len = dynamic_cast<SCDictLen*>(ir)) {
    return ir_expr_has_no_runtime_effects(len->dict);
  }
  if (auto* get = dynamic_cast<SCListGet*>(ir)) {
    return ir_expr_has_no_runtime_effects(get->list)
      && ir_expr_has_no_runtime_effects(get->index);
  }
  if (auto* slice = dynamic_cast<SCListSlice*>(ir)) {
    return ir_expr_has_no_runtime_effects(slice->list)
      && ir_expr_has_no_runtime_effects(slice->start)
      && (slice->end == nullptr || ir_expr_has_no_runtime_effects(slice->end));
  }
  if (auto* get = dynamic_cast<SCDictGet*>(ir)) {
    return ir_expr_has_no_runtime_effects(get->dict)
      && ir_expr_has_no_runtime_effects(get->key);
  }
  if (auto* get = dynamic_cast<SCMatrixGet*>(ir)) {
    return ir_expr_has_no_runtime_effects(get->matrix)
      && ir_expr_has_no_runtime_effects(get->row)
      && ir_expr_has_no_runtime_effects(get->col);
  }
  if (auto* row = dynamic_cast<SCMatrixRow*>(ir)) {
    return ir_expr_has_no_runtime_effects(row->matrix)
      && ir_expr_has_no_runtime_effects(row->row);
  }
  return false;
}

bool
stmt_is_rebind_hoist_transparent(StyioIR* ir) {
  if (ir == nullptr) {
    return true;
  }
  if (auto* bind = dynamic_cast<SGFlexBind*>(ir)) {
    return ir_expr_has_no_runtime_effects(bind->value);
  }
  if (auto* bind = dynamic_cast<SGFinalBind*>(ir)) {
    return ir_expr_has_no_runtime_effects(bind->value);
  }
  if (auto* block = dynamic_cast<SGBlock*>(ir)) {
    return std::all_of(block->stmts.begin(), block->stmts.end(), [](StyioIR* stmt) {
      return stmt_is_rebind_hoist_transparent(stmt);
    });
  }
  return false;
}

void
collect_expr_reads(StyioIR* ir, std::unordered_set<std::string>& names) {
  if (ir == nullptr) {
    return;
  }
  if (auto name = res_id_name(ir)) {
    names.insert(*name);
    return;
  }
  if (auto* cast = dynamic_cast<SGCast*>(ir)) {
    collect_expr_reads(cast->value, names);
    return;
  }
  if (auto* op = dynamic_cast<SGBinOp*>(ir)) {
    collect_expr_reads(op->lhs_expr, names);
    collect_expr_reads(op->rhs_expr, names);
    return;
  }
  if (auto* cond = dynamic_cast<SGCond*>(ir)) {
    collect_expr_reads(cond->lhs_expr, names);
    collect_expr_reads(cond->rhs_expr, names);
    return;
  }
  if (auto* len = dynamic_cast<SCListLen*>(ir)) {
    collect_expr_reads(len->list, names);
    return;
  }
  if (auto* len = dynamic_cast<SCDictLen*>(ir)) {
    collect_expr_reads(len->dict, names);
    return;
  }
  if (auto* get = dynamic_cast<SCListGet*>(ir)) {
    collect_expr_reads(get->list, names);
    collect_expr_reads(get->index, names);
    return;
  }
  if (auto* slice = dynamic_cast<SCListSlice*>(ir)) {
    collect_expr_reads(slice->list, names);
    collect_expr_reads(slice->start, names);
    collect_expr_reads(slice->end, names);
    return;
  }
  if (auto* get = dynamic_cast<SCDictGet*>(ir)) {
    collect_expr_reads(get->dict, names);
    collect_expr_reads(get->key, names);
    return;
  }
}

bool
expr_reads_name(StyioIR* ir, const std::string& name) {
  std::unordered_set<std::string> reads;
  collect_expr_reads(ir, reads);
  return reads.count(name) != 0;
}

bool
stmt_reads_name(StyioIR* ir, const std::string& name);

bool
stmt_writes_name(StyioIR* ir, const std::string& name) {
  if (ir == nullptr) {
    return false;
  }
  if (auto* bind = dynamic_cast<SGFlexBind*>(ir)) {
    return bind->var && bind->var->var_name && bind->var->var_name->as_str() == name;
  }
  if (auto* bind = dynamic_cast<SGFinalBind*>(ir)) {
    return bind->var && bind->var->var_name && bind->var->var_name->as_str() == name;
  }
  if (auto* set = dynamic_cast<SCListSet*>(ir)) {
    if (auto list_name = res_id_name(set->list); list_name && *list_name == name) {
      return true;
    }
  }
  if (auto* set = dynamic_cast<SCDictSet*>(ir)) {
    if (auto dict_name = res_id_name(set->dict); dict_name && *dict_name == name) {
      return true;
    }
  }
  if (auto* block = dynamic_cast<SGBlock*>(ir)) {
    return std::any_of(block->stmts.begin(), block->stmts.end(), [&](StyioIR* stmt) {
      return stmt_writes_name(stmt, name);
    });
  }
  if (auto* m = dynamic_cast<SGMatch*>(ir)) {
    for (auto const& arm : m->int_arms) {
      if (stmt_writes_name(arm.second, name)) {
        return true;
      }
    }
    return stmt_writes_name(m->default_arm, name);
  }
  if (auto* loop = dynamic_cast<SGLoop*>(ir)) {
    return stmt_writes_name(loop->body, name);
  }
  if (auto* each = dynamic_cast<SGForEach*>(ir)) {
    return each->var == name || stmt_writes_name(each->body, name);
  }
  if (auto* range = dynamic_cast<SGRangeFor*>(ir)) {
    return range->var == name || stmt_writes_name(range->body, name);
  }
  if (auto* iff = dynamic_cast<SGIf*>(ir)) {
    return stmt_writes_name(iff->then_block, name) || stmt_writes_name(iff->else_block, name);
  }
  return false;
}

bool
stmt_writes_any(StyioIR* ir, const std::unordered_set<std::string>& names) {
  for (const std::string& name : names) {
    if (stmt_writes_name(ir, name)) {
      return true;
    }
  }
  return false;
}

bool
stmt_reads_name(StyioIR* ir, const std::string& name) {
  if (ir == nullptr) {
    return false;
  }
  if (auto* bind = dynamic_cast<SGFlexBind*>(ir)) {
    return expr_reads_name(bind->value, name);
  }
  if (auto* bind = dynamic_cast<SGFinalBind*>(ir)) {
    return expr_reads_name(bind->value, name);
  }
  if (auto* block = dynamic_cast<SGBlock*>(ir)) {
    return std::any_of(block->stmts.begin(), block->stmts.end(), [&](StyioIR* stmt) {
      return stmt_reads_name(stmt, name);
    });
  }
  if (auto* m = dynamic_cast<SGMatch*>(ir)) {
    if (expr_reads_name(m->scrutinee, name)) {
      return true;
    }
    for (auto const& arm : m->int_arms) {
      if (stmt_reads_name(arm.second, name)) {
        return true;
      }
    }
    return stmt_reads_name(m->default_arm, name);
  }
  if (auto* loop = dynamic_cast<SGLoop*>(ir)) {
    return expr_reads_name(loop->cond, name) || stmt_reads_name(loop->body, name);
  }
  if (auto* each = dynamic_cast<SGForEach*>(ir)) {
    return expr_reads_name(each->iterable, name) || stmt_reads_name(each->body, name);
  }
  if (auto* range = dynamic_cast<SGRangeFor*>(ir)) {
    return expr_reads_name(range->start, name)
      || expr_reads_name(range->end, name)
      || expr_reads_name(range->step, name)
      || stmt_reads_name(range->body, name);
  }
  if (auto* iff = dynamic_cast<SGIf*>(ir)) {
    return expr_reads_name(iff->cond, name)
      || stmt_reads_name(iff->then_block, name)
      || stmt_reads_name(iff->else_block, name);
  }
  if (auto* set = dynamic_cast<SCListSet*>(ir)) {
    return expr_reads_name(set->list, name)
      || expr_reads_name(set->index, name)
      || expr_reads_name(set->value, name);
  }
  if (auto* set = dynamic_cast<SCDictSet*>(ir)) {
    return expr_reads_name(set->dict, name)
      || expr_reads_name(set->key, name)
      || expr_reads_name(set->value, name);
  }
  if (auto* ret = dynamic_cast<SGReturn*>(ir)) {
    return expr_reads_name(ret->expr, name);
  }
  return expr_reads_name(ir, name);
}

bool
suffix_mentions_name(const std::vector<StyioIR*>& stmts, size_t begin, const std::string& name) {
  for (size_t i = begin; i < stmts.size(); ++i) {
    if (stmt_reads_name(stmts[i], name) || stmt_writes_name(stmts[i], name)) {
      return true;
    }
  }
  return false;
}

bool
suffix_reads_name(const std::vector<StyioIR*>& stmts, size_t begin, const std::string& name) {
  for (size_t i = begin; i < stmts.size(); ++i) {
    if (stmt_reads_name(stmts[i], name)) {
      return true;
    }
  }
  return false;
}

bool
prefix_writes_name(const std::vector<StyioIR*>& stmts, size_t end, const std::string& name) {
  for (size_t i = 0; i < end; ++i) {
    if (stmt_writes_name(stmts[i], name)) {
      return true;
    }
  }
  return false;
}

struct DefaultRebind
{
  size_t index = 0;
  SGFlexBind* bind = nullptr;
  std::string name;
};

std::optional<DefaultRebind>
find_repeated_default_rebind(SGMatch* match) {
  if (!match || !match->default_arm || !ir_expr_is_speculatable(match->scrutinee)) {
    return std::nullopt;
  }

  std::unordered_set<std::string> scrutinee_deps;
  collect_expr_reads(match->scrutinee, scrutinee_deps);

  auto& stmts = match->default_arm->stmts;
  for (size_t i = 0; i < stmts.size(); ++i) {
    auto* bind = dynamic_cast<SGFlexBind*>(stmts[i]);
    if (bind == nullptr || bind->var == nullptr || bind->var->var_name == nullptr) {
      continue;
    }
    const std::string target = bind->var->var_name->as_str();
    if (!ir_expr_equiv(bind->value, match->scrutinee)) {
      continue;
    }
    if (!suffix_reads_name(stmts, i + 1, target)) {
      continue;
    }

    bool prior_conflict = false;
    for (size_t j = 0; j < i; ++j) {
      prior_conflict = prior_conflict
        || stmt_reads_name(stmts[j], target)
        || stmt_writes_name(stmts[j], target)
        || stmt_writes_any(stmts[j], scrutinee_deps)
        || !stmt_is_rebind_hoist_transparent(stmts[j]);
    }
    if (!prior_conflict) {
      return DefaultRebind{i, bind, target};
    }
  }

  return std::nullopt;
}

StyioIR*
canonicalize_match_in_sequence(
  std::vector<StyioIR*>& owner_stmts,
  size_t match_index,
  SGMatch* match
) {
  auto rebind = find_repeated_default_rebind(match);
  if (!rebind.has_value()) {
    return match;
  }
  if (prefix_writes_name(owner_stmts, match_index, rebind->name)) {
    return match;
  }
  if (suffix_mentions_name(owner_stmts, match_index + 1, rebind->name)) {
    return match;
  }

  auto& default_stmts = match->default_arm->stmts;
  default_stmts.erase(default_stmts.begin() + static_cast<std::ptrdiff_t>(rebind->index));
  if (match->scrutinee != rebind->bind->value) {
    delete match->scrutinee;
  }
  match->scrutinee = SGResId::Create(rebind->name);
  return SGBlock::Create(std::vector<StyioIR*>{rebind->bind, match});
}

/*
  StyioIROptimizerWalker — uses StyioIRWalker dispatch for bottom-up
  optimization of StyioIR nodes. Each visit method recursively optimizes
  children before returning, replacing the ~265-line dynamic_cast chain
  that was previously in Optimizer::optimize_children.
*/
class StyioIROptimizerWalker : public styio::ir::StyioIRWalker
{
public:
  // Called by Optimizer::optimize() for non-sequence node types.
  // Uses walker dispatch instead of hand-rolled dynamic_cast chain.
  void optimize_children(StyioIR* ir) {
    walk(ir);
  }

private:
  // Entry-point types handle sequence optimization — these don't go
  // through optimize_children; they're handled by Optimizer::optimize().
  void visitSGBlock(SGBlock*) override { /* handled by optimize_sequence */ }
  void visitSGMainEntry(SGMainEntry*) override { /* handled by optimize_sequence */ }
  void visitSGEntry(SGEntry*) override { /* handled by optimize_sequence */ }

  // ---- SG domain ----

  void visitSGFlexBind(SGFlexBind* node) override {
    node->value = optimize(node->value);
  }

  void visitSGFinalBind(SGFinalBind* node) override {
    node->value = optimize(node->value);
  }

  void visitSGBinOp(SGBinOp* node) override {
    node->lhs_expr = optimize(node->lhs_expr);
    node->rhs_expr = optimize(node->rhs_expr);
  }

  void visitSGCast(SGCast* node) override {
    node->value = optimize(node->value);
  }

  void visitSGCond(SGCond* node) override {
    node->lhs_expr = optimize(node->lhs_expr);
    node->rhs_expr = optimize(node->rhs_expr);
  }

  void visitSGReturn(SGReturn* node) override {
    node->expr = optimize(node->expr);
  }

  void visitSGFunc(SGFunc* node) override {
    optimize_block(node->func_block);
  }

  void visitSGCall(SGCall* node) override {
    for (auto*& arg : node->func_args) {
      arg = optimize(arg);
    }
  }

  void visitSGLoop(SGLoop* node) override {
    if (node->cond) {
      node->cond = optimize(node->cond);
    }
    optimize_block(node->body);
  }

  void visitSGForEach(SGForEach* node) override {
    node->iterable = optimize(node->iterable);
    optimize_block(node->body);
  }

  void visitSGRangeFor(SGRangeFor* node) override {
    node->start = optimize(node->start);
    node->end = optimize(node->end);
    node->step = optimize(node->step);
    optimize_block(node->body);
  }

  void visitSGIf(SGIf* node) override {
    node->cond = optimize(node->cond);
    optimize_block(node->then_block);
    optimize_block(node->else_block);
  }

  void visitSGMatch(SGMatch* node) override {
    node->scrutinee = optimize(node->scrutinee);
    for (auto& arm : node->int_arms) {
      optimize_block(arm.second);
    }
    optimize_block(node->default_arm);
  }

  void visitSGSeriesAvgStep(SGSeriesAvgStep* node) override {
    node->x = optimize(node->x);
  }

  void visitSGSeriesMaxStep(SGSeriesMaxStep* node) override {
    node->x = optimize(node->x);
  }

  void visitSGFallback(SGFallback* node) override {
    node->primary = optimize(node->primary);
    node->alternate = optimize(node->alternate);
  }

  void visitSGWaveMerge(SGWaveMerge* node) override {
    node->cond = optimize(node->cond);
    node->true_val = optimize(node->true_val);
    node->false_val = optimize(node->false_val);
  }

  void visitSGWaveDispatch(SGWaveDispatch* node) override {
    node->cond = optimize(node->cond);
    node->true_arm = optimize(node->true_arm);
    node->false_arm = optimize(node->false_arm);
  }

  void visitSGGuardSelect(SGGuardSelect* node) override {
    node->base = optimize(node->base);
    node->guard_cond = optimize(node->guard_cond);
  }

  void visitSGEqProbe(SGEqProbe* node) override {
    node->base = optimize(node->base);
    node->probe = optimize(node->probe);
  }

  void visitSGSnapshotDecl(SGSnapshotDecl* node) override {
    node->path_expr = optimize(node->path_expr);
  }

  // ---- SC domain ----

  void visitSCListLiteral(SCListLiteral* node) override {
    for (auto*& elem : node->elems) {
      elem = optimize(elem);
    }
  }

  void visitSCDictLiteral(SCDictLiteral* node) override {
    for (auto& entry : node->entries) {
      entry.key = optimize(entry.key);
      entry.value = optimize(entry.value);
    }
  }

  void visitSCMatrixLiteral(SCMatrixLiteral* node) override {
    for (auto*& elem : node->elems) {
      elem = optimize(elem);
    }
  }

  void visitSCListClone(SCListClone* node) override {
    node->source = optimize(node->source);
  }

  void visitSCMatrixClone(SCMatrixClone* node) override {
    node->source = optimize(node->source);
  }

  void visitSCListLen(SCListLen* node) override {
    node->list = optimize(node->list);
  }

  void visitSCDictLen(SCDictLen* node) override {
    node->dict = optimize(node->dict);
  }

  void visitSCListGet(SCListGet* node) override {
    node->list = optimize(node->list);
    node->index = optimize(node->index);
  }

  void visitSCListSlice(SCListSlice* node) override {
    node->list = optimize(node->list);
    node->start = optimize(node->start);
    node->end = optimize(node->end);
  }

  void visitSCDictGet(SCDictGet* node) override {
    node->dict = optimize(node->dict);
    node->key = optimize(node->key);
  }

  void visitSCListSet(SCListSet* node) override {
    node->list = optimize(node->list);
    node->index = optimize(node->index);
    node->value = optimize(node->value);
  }

  void visitSCDictSet(SCDictSet* node) override {
    node->dict = optimize(node->dict);
    node->key = optimize(node->key);
    node->value = optimize(node->value);
  }

  void visitSCListToString(SCListToString* node) override {
    node->list = optimize(node->list);
  }

  void visitSCMatrixGet(SCMatrixGet* node) override {
    node->matrix = optimize(node->matrix);
    node->row = optimize(node->row);
    node->col = optimize(node->col);
  }

  void visitSCMatrixRow(SCMatrixRow* node) override {
    node->matrix = optimize(node->matrix);
    node->row = optimize(node->row);
  }

  void visitSCMatrixRowsSlice(SCMatrixRowsSlice* node) override {
    node->matrix = optimize(node->matrix);
    node->start = optimize(node->start);
    node->end = optimize(node->end);
  }

  void visitSCMatrixToString(SCMatrixToString* node) override {
    node->matrix = optimize(node->matrix);
  }

  void visitSCDictClone(SCDictClone* node) override {
    node->source = optimize(node->source);
  }

  void visitSCDictKeys(SCDictKeys* node) override {
    node->dict = optimize(node->dict);
  }

  void visitSCDictValues(SCDictValues* node) override {
    node->dict = optimize(node->dict);
  }

  void visitSCDictToString(SCDictToString* node) override {
    node->dict = optimize(node->dict);
  }

  // ---- SIO domain ----

  void visitSIOHandleAcquire(SIOHandleAcquire* node) override {
    node->path_expr = optimize(node->path_expr);
  }

  void visitSIOHandleRelease(SIOHandleRelease* node) override {
    node->path_expr = optimize(node->path_expr);
  }

  void visitSIOFileLineIter(SIOFileLineIter* node) override {
    node->path_expr = optimize(node->path_expr);
    optimize_block(node->body);
  }

  void visitSIOStreamZip(SIOStreamZip* node) override {
    node->iterable_a = optimize(node->iterable_a);
    node->iterable_b = optimize(node->iterable_b);
    optimize_block(node->body);
  }

  void visitSIOInstantPull(SIOInstantPull* node) override {
    if (!node->from_handle) {
      node->path_expr = optimize(node->path_expr);
    }
  }

  void visitSIOResourceWriteToFile(SIOResourceWriteToFile* node) override {
    node->data_expr = optimize(node->data_expr);
    node->path_expr = optimize(node->path_expr);
  }

  void visitSIOStdStreamWrite(SIOStdStreamWrite* node) override {
    for (auto*& expr : node->exprs) {
      expr = optimize(expr);
    }
  }

  void visitSIOResourceEffect(SIOResourceEffect* node) override {
    node->operation = optimize(node->operation);
    for (auto& handler : node->handlers) {
      handler.body = optimize(handler.body);
    }
    node->fallback = optimize(node->fallback);
  }

  void visitSIOStdStreamLineIter(SIOStdStreamLineIter* node) override {
    optimize_block(node->body);
  }

  void visitSIOTaskCreate(SIOTaskCreate* node) override {
    optimize_block(node->body);
  }

  void visitSIOFlowBind(SIOFlowBind* node) override {
    node->source_expr = optimize(node->source_expr);
    node->fallback_expr = optimize(node->fallback_expr);
  }

  void visitSIOPrint(SIOPrint* node) override {
    for (auto*& expr : node->expr) {
      expr = optimize(expr);
    }
  }

  void visitSIORead(SIORead* node) override {
    if (node->file_path) {
      node->file_path = static_cast<SIOPath*>(optimize(node->file_path));
    }
  }

private:
  // These are set by the owning Optimizer.
  friend class Optimizer;
  std::function<StyioIR*(StyioIR*)> optimize;
  std::function<void(SGBlock*&)> optimize_block;
};

class Optimizer
{
public:
  Optimizer() {
    walker_.optimize = [this](StyioIR* ir) { return this->optimize(ir); };
    walker_.optimize_block = [this](SGBlock*& block) { this->optimize_block_impl(block); };
  }

  StyioIR* optimize(StyioIR* ir) {
    if (auto* block = dynamic_cast<SGBlock*>(ir)) {
      optimize_sequence(block->stmts);
      return block;
    }
    if (auto* main = dynamic_cast<SGMainEntry*>(ir)) {
      optimize_sequence(main->stmts);
      return main;
    }
    if (auto* entry = dynamic_cast<SGEntry*>(ir)) {
      optimize_sequence(entry->stmts);
      return entry;
    }
    walker_.optimize_children(ir);
    return ir;
  }

private:
  StyioIROptimizerWalker walker_;

  void optimize_sequence(std::vector<StyioIR*>& stmts) {
    for (auto*& stmt : stmts) {
      stmt = optimize(stmt);
    }
    for (size_t i = 0; i < stmts.size(); ++i) {
      if (auto* match = dynamic_cast<SGMatch*>(stmts[i])) {
        stmts[i] = canonicalize_match_in_sequence(stmts, i, match);
      }
    }
  }

  void optimize_block_impl(SGBlock*& block) {
    if (block) {
      block = static_cast<SGBlock*>(optimize(block));
    }
  }
};

}  // namespace

#ifndef STYIO_IR_OPTIMIZER_INTERNAL_TEST_INCLUDE

const char*
pass_name(StyioIRPassManager::PassKind kind) {
  switch (kind) {
    case StyioIRPassManager::PassKind::Canonicalization:
      return "styioir-canonicalization";
  }
  return "styioir-unknown";
}

bool
append_verifier_diagnostics(
  const StyioIR* root,
  std::vector<styio::ir::StyioIRVerifierDiagnostic>& diagnostics
) {
  try {
    styio::ir::StyioIRVerifierResult verifier = styio::ir::verify_styio_ir(root);
    diagnostics.insert(
      diagnostics.end(),
      verifier.diagnostics.begin(),
      verifier.diagnostics.end());
    return verifier.ok();
  }
  catch (const std::exception& ex) {
    diagnostics.push_back(styio::ir::StyioIRVerifierDiagnostic{
      std::string(styio::services::diagnostics::kPhaseIrVerify),
      std::string(styio::services::diagnostics::kIrVerifyContract),
      ex.what(),
    });
    return false;
  }
}

std::string
render_ir_dump(StyioIR* root) {
  if (root == nullptr) {
    return "<null StyioIR>";
  }
  StyioRepr repr;
  return root->toString(&repr);
}

void
StyioIRPassManager::add_canonicalization_pass() {
  passes_.push_back(PassKind::Canonicalization);
}

StyioIRPassPipelineResult
StyioIRPassManager::run(
  StyioIR* root,
  const StyioIRPassPipelineOptions& options
) const {
  StyioIRPassPipelineResult result;
  result.root = root;
  if (options.collect_ir_dumps) {
    result.initial_ir = render_ir_dump(result.root);
  }

  auto finish = [&]() {
    if (options.collect_ir_dumps) {
      result.final_ir = render_ir_dump(result.root);
    }
    return result;
  };

  if (options.verify_before && !append_verifier_diagnostics(result.root, result.diagnostics)) {
    return finish();
  }

  for (PassKind pass : passes_) {
    StyioIRPassRecord record;
    record.name = pass_name(pass);
    if (options.collect_ir_dumps) {
      record.ir_before = render_ir_dump(result.root);
    }

    if (options.verify_before && !append_verifier_diagnostics(result.root, result.diagnostics)) {
      record.verifier_before_ok = false;
      result.passes.push_back(record);
      return finish();
    }

    const auto started = std::chrono::steady_clock::now();
    switch (pass) {
      case PassKind::Canonicalization: {
        Optimizer optimizer;
        result.root = optimizer.optimize(result.root);
        break;
      }
    }
    const auto ended = std::chrono::steady_clock::now();
    if (options.collect_timing) {
      record.duration_ns =
        static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(ended - started).count());
    }
    if (options.collect_ir_dumps) {
      record.ir_after = render_ir_dump(result.root);
    }

    if (options.verify_after_each_pass
        && !append_verifier_diagnostics(result.root, result.diagnostics)) {
      record.verifier_after_ok = false;
      result.passes.push_back(record);
      return finish();
    }
    result.passes.push_back(record);
  }

  return finish();
}

StyioIRPassManager
default_styio_ir_pass_manager(unsigned opt_level) {
  StyioIRPassManager manager;
  if (opt_level > 0) {
    manager.add_canonicalization_pass();
  }
  return manager;
}

StyioIRPassPipelineResult
run_default_styio_ir_pass_pipeline(
  StyioIR* root,
  const StyioIRPassPipelineOptions& options
) {
  return default_styio_ir_pass_manager(options.opt_level).run(root, options);
}

StyioIR*
require_default_styio_ir_pass_pipeline(
  StyioIR* root,
  const StyioIRPassPipelineOptions& options
) {
  StyioIRPassPipelineResult result = run_default_styio_ir_pass_pipeline(root, options);
  if (result.ok()) {
    return result.root;
  }
  throw StyioTypeError("StyioIR pass pipeline failed: " + result.diagnostics.front().message);
}

#endif  // STYIO_IR_OPTIMIZER_INTERNAL_TEST_INCLUDE

StyioIR*
optimize_styio_ir(StyioIR* root) {
  Optimizer optimizer;
  return optimizer.optimize(root);
}

}  // namespace styio::lowering
