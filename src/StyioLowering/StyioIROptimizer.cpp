/*
  StyioIR canonicalization and local optimization passes.
*/

#include "StyioIROptimizer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "../StyioIR/GenIR/GenIR.hpp"
#include "../StyioIR/IOIR/IOIR.hpp"

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
      || dynamic_cast<SGConstString*>(ir)
      || dynamic_cast<SGCast*>(ir)) {
    return true;
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

class Optimizer
{
public:
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
    optimize_children(ir);
    return ir;
  }

private:
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

  void optimize_block(SGBlock*& block) {
    if (block) {
      block = static_cast<SGBlock*>(optimize(block));
    }
  }

  void optimize_children(StyioIR* ir) {
    if (auto* bind = dynamic_cast<SGFlexBind*>(ir)) {
      bind->value = optimize(bind->value);
      return;
    }
    if (auto* bind = dynamic_cast<SGFinalBind*>(ir)) {
      bind->value = optimize(bind->value);
      return;
    }
    if (auto* op = dynamic_cast<SGBinOp*>(ir)) {
      op->lhs_expr = optimize(op->lhs_expr);
      op->rhs_expr = optimize(op->rhs_expr);
      return;
    }
    if (auto* cond = dynamic_cast<SGCond*>(ir)) {
      cond->lhs_expr = optimize(cond->lhs_expr);
      cond->rhs_expr = optimize(cond->rhs_expr);
      return;
    }
    if (auto* ret = dynamic_cast<SGReturn*>(ir)) {
      ret->expr = optimize(ret->expr);
      return;
    }
    if (auto* func = dynamic_cast<SGFunc*>(ir)) {
      optimize_block(func->func_block);
      return;
    }
    if (auto* call = dynamic_cast<SGCall*>(ir)) {
      for (auto*& arg : call->func_args) {
        arg = optimize(arg);
      }
      return;
    }
    if (auto* loop = dynamic_cast<SGLoop*>(ir)) {
      if (loop->cond) {
        loop->cond = optimize(loop->cond);
      }
      optimize_block(loop->body);
      return;
    }
    if (auto* each = dynamic_cast<SGForEach*>(ir)) {
      each->iterable = optimize(each->iterable);
      optimize_block(each->body);
      return;
    }
    if (auto* range = dynamic_cast<SGRangeFor*>(ir)) {
      range->start = optimize(range->start);
      range->end = optimize(range->end);
      range->step = optimize(range->step);
      optimize_block(range->body);
      return;
    }
    if (auto* iff = dynamic_cast<SGIf*>(ir)) {
      iff->cond = optimize(iff->cond);
      optimize_block(iff->then_block);
      optimize_block(iff->else_block);
      return;
    }
    if (auto* match = dynamic_cast<SGMatch*>(ir)) {
      match->scrutinee = optimize(match->scrutinee);
      for (auto& arm : match->int_arms) {
        optimize_block(arm.second);
      }
      optimize_block(match->default_arm);
      return;
    }
    if (auto* step = dynamic_cast<SGSeriesAvgStep*>(ir)) {
      step->x = optimize(step->x);
      return;
    }
    if (auto* step = dynamic_cast<SGSeriesMaxStep*>(ir)) {
      step->x = optimize(step->x);
      return;
    }
    if (auto* fallback = dynamic_cast<SGFallback*>(ir)) {
      fallback->primary = optimize(fallback->primary);
      fallback->alternate = optimize(fallback->alternate);
      return;
    }
    if (auto* wave = dynamic_cast<SGWaveMerge*>(ir)) {
      wave->cond = optimize(wave->cond);
      wave->true_val = optimize(wave->true_val);
      wave->false_val = optimize(wave->false_val);
      return;
    }
    if (auto* wave = dynamic_cast<SGWaveDispatch*>(ir)) {
      wave->cond = optimize(wave->cond);
      wave->true_arm = optimize(wave->true_arm);
      wave->false_arm = optimize(wave->false_arm);
      return;
    }
    if (auto* guard = dynamic_cast<SGGuardSelect*>(ir)) {
      guard->base = optimize(guard->base);
      guard->guard_cond = optimize(guard->guard_cond);
      return;
    }
    if (auto* probe = dynamic_cast<SGEqProbe*>(ir)) {
      probe->base = optimize(probe->base);
      probe->probe = optimize(probe->probe);
      return;
    }
    if (auto* snap = dynamic_cast<SGSnapshotDecl*>(ir)) {
      snap->path_expr = optimize(snap->path_expr);
      return;
    }
    if (auto* list = dynamic_cast<SCListLiteral*>(ir)) {
      for (auto*& elem : list->elems) {
        elem = optimize(elem);
      }
      return;
    }
    if (auto* dict = dynamic_cast<SCDictLiteral*>(ir)) {
      for (auto& entry : dict->entries) {
        entry.key = optimize(entry.key);
        entry.value = optimize(entry.value);
      }
      return;
    }
    if (auto* matrix = dynamic_cast<SCMatrixLiteral*>(ir)) {
      for (auto*& elem : matrix->elems) {
        elem = optimize(elem);
      }
      return;
    }
    if (auto* clone = dynamic_cast<SCListClone*>(ir)) {
      clone->source = optimize(clone->source);
      return;
    }
    if (auto* len = dynamic_cast<SCListLen*>(ir)) {
      len->list = optimize(len->list);
      return;
    }
    if (auto* len = dynamic_cast<SCDictLen*>(ir)) {
      len->dict = optimize(len->dict);
      return;
    }
    if (auto* get = dynamic_cast<SCListGet*>(ir)) {
      get->list = optimize(get->list);
      get->index = optimize(get->index);
      return;
    }
    if (auto* get = dynamic_cast<SCDictGet*>(ir)) {
      get->dict = optimize(get->dict);
      get->key = optimize(get->key);
      return;
    }
    if (auto* set = dynamic_cast<SCListSet*>(ir)) {
      set->list = optimize(set->list);
      set->index = optimize(set->index);
      set->value = optimize(set->value);
      return;
    }
    if (auto* set = dynamic_cast<SCDictSet*>(ir)) {
      set->dict = optimize(set->dict);
      set->key = optimize(set->key);
      set->value = optimize(set->value);
      return;
    }
    if (auto* str = dynamic_cast<SCListToString*>(ir)) {
      str->list = optimize(str->list);
      return;
    }
    if (auto* get = dynamic_cast<SCMatrixGet*>(ir)) {
      get->matrix = optimize(get->matrix);
      get->row = optimize(get->row);
      get->col = optimize(get->col);
      return;
    }
    if (auto* row = dynamic_cast<SCMatrixRow*>(ir)) {
      row->matrix = optimize(row->matrix);
      row->row = optimize(row->row);
      return;
    }
    if (auto* str = dynamic_cast<SCMatrixToString*>(ir)) {
      str->matrix = optimize(str->matrix);
      return;
    }
    if (auto* clone = dynamic_cast<SCDictClone*>(ir)) {
      clone->source = optimize(clone->source);
      return;
    }
    if (auto* keys = dynamic_cast<SCDictKeys*>(ir)) {
      keys->dict = optimize(keys->dict);
      return;
    }
    if (auto* values = dynamic_cast<SCDictValues*>(ir)) {
      values->dict = optimize(values->dict);
      return;
    }
    if (auto* str = dynamic_cast<SCDictToString*>(ir)) {
      str->dict = optimize(str->dict);
      return;
    }
    if (auto* acquire = dynamic_cast<SIOHandleAcquire*>(ir)) {
      acquire->path_expr = optimize(acquire->path_expr);
      return;
    }
    if (auto* release = dynamic_cast<SIOHandleRelease*>(ir)) {
      release->path_expr = optimize(release->path_expr);
      return;
    }
    if (auto* iter = dynamic_cast<SIOFileLineIter*>(ir)) {
      iter->path_expr = optimize(iter->path_expr);
      optimize_block(iter->body);
      return;
    }
    if (auto* zip = dynamic_cast<SIOStreamZip*>(ir)) {
      zip->iterable_a = optimize(zip->iterable_a);
      zip->iterable_b = optimize(zip->iterable_b);
      optimize_block(zip->body);
      return;
    }
    if (auto* pull = dynamic_cast<SIOInstantPull*>(ir)) {
      pull->path_expr = optimize(pull->path_expr);
      return;
    }
    if (auto* write = dynamic_cast<SIOResourceWriteToFile*>(ir)) {
      write->data_expr = optimize(write->data_expr);
      write->path_expr = optimize(write->path_expr);
      return;
    }
    if (auto* write = dynamic_cast<SIOStdStreamWrite*>(ir)) {
      for (auto*& expr : write->exprs) {
        expr = optimize(expr);
      }
      return;
    }
    if (auto* iter = dynamic_cast<SIOStdStreamLineIter*>(ir)) {
      optimize_block(iter->body);
      return;
    }
    if (auto* print = dynamic_cast<SIOPrint*>(ir)) {
      for (auto*& expr : print->expr) {
        expr = optimize(expr);
      }
      return;
    }
    if (auto* read = dynamic_cast<SIORead*>(ir)) {
      read->file_path = static_cast<SIOPath*>(optimize(read->file_path));
      return;
    }
  }
};

}  // namespace

StyioIR*
optimize_styio_ir(StyioIR* root) {
  Optimizer optimizer;
  return optimizer.optimize(root);
}

}  // namespace styio::lowering
