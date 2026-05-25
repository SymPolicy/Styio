#include "Verifier.hpp"

#include <string>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>

#include "../StyioException/Exception.hpp"
#include "GenIR/GenIR.hpp"

namespace styio::ir {
namespace {

enum class HandleState
{
  Unknown,
  Acquired,
  Released,
};

struct VerifierContext
{
  StyioIRVerifierResult result;
  std::unordered_set<const StyioIR*> visited;
  std::unordered_map<std::string, HandleState> handle_states;

  void add_error(std::string message) {
    result.diagnostics.push_back(StyioIRVerifierDiagnostic{
      "styioir",
      "STYIO_IR_CONTRACT",
      std::move(message),
    });
  }

  void visit_required(const StyioIR* node, const char* field) {
    if (node == nullptr) {
      add_error(std::string("missing required StyioIR child: ") + field);
      return;
    }
    visit(node);
  }

  void visit_optional(const StyioIR* node) {
    if (node != nullptr) {
      visit(node);
    }
  }

  void visit_vector(const std::vector<StyioIR*>& nodes, const char* field) {
    for (const auto* child : nodes) {
      visit_required(child, field);
    }
  }

  template <typename T>
  void visit_vector(const std::vector<T*>& nodes, const char* field) {
    for (const auto* child : nodes) {
      visit_required(child, field);
    }
  }

  void note_acquire(const std::string& name) {
    if (!name.empty()) {
      handle_states[name] = HandleState::Acquired;
    }
  }

  void note_release(const std::string& name) {
    if (!name.empty()) {
      handle_states[name] = HandleState::Released;
    }
  }

  void visit(const StyioIR* node) {
    if (node == nullptr) {
      add_error("missing StyioIR root");
      return;
    }
    if (!visited.insert(node).second) {
      return;
    }
    if (!node->is_active()) {
      add_error(std::string("inactive StyioIR node reached codegen boundary: ") + typeid(*node).name());
      return;
    }

    if (auto* n = dynamic_cast<const SGStruct*>(node)) {
      visit_optional(n->name);
      visit_vector(n->elements, "SGStruct.elements");
      return;
    }
    if (auto* n = dynamic_cast<const SGCast*>(node)) {
      visit_required(n->value, "SGCast.value");
      visit_required(n->from_type, "SGCast.from_type");
      visit_required(n->to_type, "SGCast.to_type");
      return;
    }
    if (auto* n = dynamic_cast<const SGBinOp*>(node)) {
      visit_required(n->data_type, "SGBinOp.data_type");
      visit_required(n->lhs_expr, "SGBinOp.lhs_expr");
      visit_required(n->rhs_expr, "SGBinOp.rhs_expr");
      return;
    }
    if (auto* n = dynamic_cast<const SGCond*>(node)) {
      visit_optional(n->lhs_expr);
      visit_required(n->rhs_expr, "SGCond.rhs_expr");
      return;
    }
    if (auto* n = dynamic_cast<const SGVar*>(node)) {
      visit_required(n->var_name, "SGVar.var_name");
      visit_required(n->var_type, "SGVar.var_type");
      visit_optional(n->val_init);
      return;
    }
    if (auto* n = dynamic_cast<const SGFlexBind*>(node)) {
      visit_required(n->var, "SGFlexBind.var");
      visit_required(n->value, "SGFlexBind.value");
      return;
    }
    if (auto* n = dynamic_cast<const SGFinalBind*>(node)) {
      visit_required(n->var, "SGFinalBind.var");
      visit_required(n->value, "SGFinalBind.value");
      return;
    }
    if (auto* n = dynamic_cast<const SGFuncArg*>(node)) {
      visit_required(n->arg_type, "SGFuncArg.arg_type");
      return;
    }
    if (auto* n = dynamic_cast<const SGFunc*>(node)) {
      visit_required(n->ret_type, "SGFunc.ret_type");
      visit_required(n->func_name, "SGFunc.func_name");
      visit_vector(n->func_args, "SGFunc.func_args");
      visit_required(n->func_block, "SGFunc.func_block");
      return;
    }
    if (auto* n = dynamic_cast<const SGCall*>(node)) {
      visit_required(n->func_name, "SGCall.func_name");
      visit_vector(n->func_args, "SGCall.func_args");
      return;
    }
    if (auto* n = dynamic_cast<const SGReturn*>(node)) {
      visit_required(n->expr, "SGReturn.expr");
      return;
    }
    if (auto* n = dynamic_cast<const SGBlock*>(node)) {
      visit_vector(n->stmts, "SGBlock.stmts");
      return;
    }
    if (auto* n = dynamic_cast<const SGEntry*>(node)) {
      visit_vector(n->stmts, "SGEntry.stmts");
      return;
    }
    if (auto* n = dynamic_cast<const SGMainEntry*>(node)) {
      visit_vector(n->stmts, "SGMainEntry.stmts");
      return;
    }
    if (auto* n = dynamic_cast<const SGLoop*>(node)) {
      visit_optional(n->cond);
      visit_required(n->body, "SGLoop.body");
      return;
    }
    if (auto* n = dynamic_cast<const SGSeriesAvgStep*>(node)) {
      visit_required(n->x, "SGSeriesAvgStep.x");
      return;
    }
    if (auto* n = dynamic_cast<const SGSeriesMaxStep*>(node)) {
      visit_required(n->x, "SGSeriesMaxStep.x");
      return;
    }
    if (auto* n = dynamic_cast<const SGForEach*>(node)) {
      visit_required(n->iterable, "SGForEach.iterable");
      visit_required(n->body, "SGForEach.body");
      return;
    }
    if (auto* n = dynamic_cast<const SGRangeFor*>(node)) {
      visit_required(n->start, "SGRangeFor.start");
      visit_required(n->end, "SGRangeFor.end");
      visit_required(n->step, "SGRangeFor.step");
      visit_required(n->body, "SGRangeFor.body");
      return;
    }
    if (auto* n = dynamic_cast<const SGIf*>(node)) {
      visit_required(n->cond, "SGIf.cond");
      visit_required(n->then_block, "SGIf.then_block");
      visit_optional(n->else_block);
      return;
    }
    if (auto* n = dynamic_cast<const SGMatch*>(node)) {
      visit_required(n->scrutinee, "SGMatch.scrutinee");
      for (const auto& arm : n->int_arms) {
        visit_required(arm.second, "SGMatch.int_arms");
      }
      visit_optional(n->default_arm);
      return;
    }
    if (auto* n = dynamic_cast<const SGFallback*>(node)) {
      visit_required(n->primary, "SGFallback.primary");
      visit_required(n->alternate, "SGFallback.alternate");
      return;
    }
    if (auto* n = dynamic_cast<const SGWaveMerge*>(node)) {
      visit_required(n->cond, "SGWaveMerge.cond");
      visit_required(n->true_val, "SGWaveMerge.true_val");
      visit_required(n->false_val, "SGWaveMerge.false_val");
      return;
    }
    if (auto* n = dynamic_cast<const SGWaveDispatch*>(node)) {
      visit_required(n->cond, "SGWaveDispatch.cond");
      visit_required(n->true_arm, "SGWaveDispatch.true_arm");
      visit_required(n->false_arm, "SGWaveDispatch.false_arm");
      return;
    }
    if (auto* n = dynamic_cast<const SGGuardSelect*>(node)) {
      visit_required(n->base, "SGGuardSelect.base");
      visit_required(n->guard_cond, "SGGuardSelect.guard_cond");
      return;
    }
    if (auto* n = dynamic_cast<const SGEqProbe*>(node)) {
      visit_required(n->base, "SGEqProbe.base");
      visit_required(n->probe, "SGEqProbe.probe");
      return;
    }
    if (auto* n = dynamic_cast<const SGSnapshotDecl*>(node)) {
      visit_required(n->path_expr, "SGSnapshotDecl.path_expr");
      return;
    }
    if (auto* n = dynamic_cast<const SGFormatString*>(node)) {
      visit_vector(n->exprs, "SGFormatString.exprs");
      return;
    }
    if (auto* n = dynamic_cast<const SCListLiteral*>(node)) {
      visit_vector(n->elems, "SCListLiteral.elems");
      return;
    }
    if (auto* n = dynamic_cast<const SCDictLiteral*>(node)) {
      for (const auto& entry : n->entries) {
        visit_required(entry.key, "SCDictLiteral.key");
        visit_required(entry.value, "SCDictLiteral.value");
      }
      return;
    }
    if (auto* n = dynamic_cast<const SCMatrixLiteral*>(node)) {
      visit_vector(n->elems, "SCMatrixLiteral.elems");
      return;
    }
    if (auto* n = dynamic_cast<const SCListClone*>(node)) {
      visit_required(n->source, "SCListClone.source");
      return;
    }
    if (auto* n = dynamic_cast<const SCListLen*>(node)) {
      visit_required(n->list, "SCListLen.list");
      return;
    }
    if (auto* n = dynamic_cast<const SCListGet*>(node)) {
      visit_required(n->list, "SCListGet.list");
      visit_required(n->index, "SCListGet.index");
      return;
    }
    if (auto* n = dynamic_cast<const SCListSet*>(node)) {
      visit_required(n->list, "SCListSet.list");
      visit_required(n->index, "SCListSet.index");
      visit_required(n->value, "SCListSet.value");
      return;
    }
    if (auto* n = dynamic_cast<const SCListToString*>(node)) {
      visit_required(n->list, "SCListToString.list");
      return;
    }
    if (auto* n = dynamic_cast<const SCMatrixGet*>(node)) {
      visit_required(n->matrix, "SCMatrixGet.matrix");
      visit_required(n->row, "SCMatrixGet.row");
      visit_required(n->col, "SCMatrixGet.col");
      return;
    }
    if (auto* n = dynamic_cast<const SCMatrixRow*>(node)) {
      visit_required(n->matrix, "SCMatrixRow.matrix");
      visit_required(n->row, "SCMatrixRow.row");
      return;
    }
    if (auto* n = dynamic_cast<const SCMatrixToString*>(node)) {
      visit_required(n->matrix, "SCMatrixToString.matrix");
      return;
    }
    if (auto* n = dynamic_cast<const SCDictClone*>(node)) {
      visit_required(n->source, "SCDictClone.source");
      return;
    }
    if (auto* n = dynamic_cast<const SCDictLen*>(node)) {
      visit_required(n->dict, "SCDictLen.dict");
      return;
    }
    if (auto* n = dynamic_cast<const SCDictGet*>(node)) {
      visit_required(n->dict, "SCDictGet.dict");
      visit_required(n->key, "SCDictGet.key");
      return;
    }
    if (auto* n = dynamic_cast<const SCDictSet*>(node)) {
      visit_required(n->dict, "SCDictSet.dict");
      visit_required(n->key, "SCDictSet.key");
      visit_required(n->value, "SCDictSet.value");
      return;
    }
    if (auto* n = dynamic_cast<const SCDictKeys*>(node)) {
      visit_required(n->dict, "SCDictKeys.dict");
      return;
    }
    if (auto* n = dynamic_cast<const SCDictValues*>(node)) {
      visit_required(n->dict, "SCDictValues.dict");
      return;
    }
    if (auto* n = dynamic_cast<const SCDictToString*>(node)) {
      visit_required(n->dict, "SCDictToString.dict");
      return;
    }
    if (auto* n = dynamic_cast<const SIOHandleAcquire*>(node)) {
      visit_required(n->path_expr, "SIOHandleAcquire.path_expr");
      note_acquire(n->var_name);
      return;
    }
    if (auto* n = dynamic_cast<const SIOHandleRelease*>(node)) {
      visit_optional(n->path_expr);
      note_release(n->var_name);
      return;
    }
    if (auto* n = dynamic_cast<const SIOFileLineIter*>(node)) {
      visit_optional(n->path_expr);
      visit_required(n->body, "SIOFileLineIter.body");
      return;
    }
    if (auto* n = dynamic_cast<const SIOStreamZip*>(node)) {
      visit_required(n->iterable_a, "SIOStreamZip.iterable_a");
      visit_required(n->iterable_b, "SIOStreamZip.iterable_b");
      visit_required(n->body, "SIOStreamZip.body");
      return;
    }
    if (auto* n = dynamic_cast<const SIOInstantPull*>(node)) {
      visit_required(n->path_expr, "SIOInstantPull.path_expr");
      return;
    }
    if (auto* n = dynamic_cast<const SIOResourceWriteToFile*>(node)) {
      visit_required(n->data_expr, "SIOResourceWriteToFile.data_expr");
      visit_required(n->path_expr, "SIOResourceWriteToFile.path_expr");
      return;
    }
    if (auto* n = dynamic_cast<const SIOStdStreamWrite*>(node)) {
      visit_vector(n->exprs, "SIOStdStreamWrite.exprs");
      return;
    }
    if (auto* n = dynamic_cast<const SIOResourceEffect*>(node)) {
      visit_required(n->operation, "SIOResourceEffect.operation");
      for (const auto& handler : n->handlers) {
        visit_required(handler.body, "SIOResourceEffect.handler");
      }
      visit_optional(n->fallback);
      return;
    }
    if (auto* n = dynamic_cast<const SIOStdStreamLineIter*>(node)) {
      visit_required(n->body, "SIOStdStreamLineIter.body");
      return;
    }
    if (auto* n = dynamic_cast<const SIOTaskCreate*>(node)) {
      visit_required(n->body, "SIOTaskCreate.body");
      return;
    }
    if (auto* n = dynamic_cast<const SIOFlowBind*>(node)) {
      visit_required(n->source_expr, "SIOFlowBind.source_expr");
      visit_optional(n->fallback_expr);
      return;
    }
    if (auto* n = dynamic_cast<const SIOPrint*>(node)) {
      visit_vector(n->expr, "SIOPrint.expr");
      return;
    }
    if (auto* n = dynamic_cast<const SIORead*>(node)) {
      visit_required(n->file_path, "SIORead.file_path");
      return;
    }
  }
};

}  // namespace

StyioIRVerifierResult
verify_styio_ir(const StyioIR* root) {
  VerifierContext context;
  context.visit(root);
  return context.result;
}

void
require_verified_styio_ir(const StyioIR* root) {
  StyioIRVerifierResult result = verify_styio_ir(root);
  if (result.ok()) {
    return;
  }
  throw StyioTypeError("StyioIR verifier failed: " + result.diagnostics.front().message);
}

}  // namespace styio::ir
