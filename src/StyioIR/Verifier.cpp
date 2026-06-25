#include "Verifier.hpp"

#include <string>
#include <stdexcept>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>

#include "../StyioException/Exception.hpp"
#include "GenIR/GenIR.hpp"
#include "StyioIRWalker.hpp"

namespace styio::ir {
namespace {

namespace diag = styio::services::diagnostics;

enum class HandleState
{
  Unknown,
  Acquired,
  Released,
};

/*
  StyioIRVerifier — uses StyioIRWalker for centralized node dispatch
  and child traversal, adding IR structural validation on top.

  Previously this file contained its own dynamic_cast dispatch chain
  (~62 casts); that dispatch is now centralized in StyioIRWalker::dispatch().
*/
class StyioIRVerifier : public StyioIRWalker
{
  StyioIRVerifierResult result_;
  std::unordered_set<StyioIR*> visited_;
  std::unordered_map<std::string, HandleState> handle_states_;

  void
  add_error(
    std::string message,
    std::string code = std::string(diag::kIrVerifyContract)
  ) {
    result_.diagnostics.push_back(StyioIRVerifierDiagnostic{
      std::string(diag::kPhaseIrVerify),
      std::move(code),
      std::move(message),
    });
  }

  // Helper: walk a required child, reporting field name on null
  void
  walk_required(StyioIR* node, const char* field) {
    if (node == nullptr) {
      add_error(std::string("missing required StyioIR child: ") + field);
      return;
    }
    walk(node);
  }

  // Helper: walk an optional child (no error on null)
  void
  walk_optional(StyioIR* node) {
    if (node != nullptr) {
      walk(node);
    }
  }

  void
  note_acquire(const std::string& name) {
    if (!name.empty()) {
      handle_states_[name] = HandleState::Acquired;
    }
  }

  void
  note_release(const std::string& name) {
    if (!name.empty()) {
      handle_states_[name] = HandleState::Released;
    }
  }

  // ---------------------------------------------------------------
  // Override dispatch to add is_active check before traversal
  // ---------------------------------------------------------------
  void
  dispatch(StyioIR* node) override {
    if (node == nullptr) {
      add_error("missing StyioIR root");
      return;
    }
    if (!visited_.insert(node).second) {
      return;
    }
    if (!node->is_active()) {
      add_error(
        std::string("inactive StyioIR node reached codegen boundary: ") + typeid(*node).name(),
        std::string(diag::kIrVerifyInactiveNode));
      return;
    }
    // Delegate to centralized dispatch
    StyioIRWalker::dispatch(node);
  }

  void
  visitUnknown(StyioIR* node) override {
    add_error(
      std::string("unsupported StyioIR node reached verifier: ") + typeid(*node).name());
  }

  // ---------------------------------------------------------------
  // Override visit methods to add null-field validation.
  // The base walker handles child traversal; we add requirement
  // checks and handle-state tracking on top.
  // ---------------------------------------------------------------

  void
  visitSGStruct(SGStruct* node) override {
    walk_optional(node->name);
    for (auto* elem : node->elements) {
      walk_required(elem, "SGStruct.elements");
    }
  }

  void
  visitSGCast(SGCast* node) override {
    walk_required(node->value, "SGCast.value");
    walk_required(node->from_type, "SGCast.from_type");
    walk_required(node->to_type, "SGCast.to_type");
  }

  void
  visitSGBinOp(SGBinOp* node) override {
    walk_required(node->data_type, "SGBinOp.data_type");
    walk_required(node->lhs_expr, "SGBinOp.lhs_expr");
    walk_required(node->rhs_expr, "SGBinOp.rhs_expr");
  }

  void
  visitSGCond(SGCond* node) override {
    walk_optional(node->lhs_expr);
    walk_required(node->rhs_expr, "SGCond.rhs_expr");
  }

  void
  visitSGVar(SGVar* node) override {
    walk_required(node->var_name, "SGVar.var_name");
    walk_required(node->var_type, "SGVar.var_type");
    walk_optional(node->val_init);
  }

  void
  visitSGFlexBind(SGFlexBind* node) override {
    walk_required(node->var, "SGFlexBind.var");
    walk_required(node->value, "SGFlexBind.value");
  }

  void
  visitSGFinalBind(SGFinalBind* node) override {
    walk_required(node->var, "SGFinalBind.var");
    walk_required(node->value, "SGFinalBind.value");
  }

  void
  visitSGFuncArg(SGFuncArg* node) override {
    walk_required(node->arg_type, "SGFuncArg.arg_type");
  }

  void
  visitSGFunc(SGFunc* node) override {
    walk_required(node->ret_type, "SGFunc.ret_type");
    walk_required(node->func_name, "SGFunc.func_name");
    for (auto* arg : node->func_args) {
      walk_required(arg, "SGFunc.func_args");
    }
    walk_required(node->func_block, "SGFunc.func_block");
  }

  void
  visitSGCall(SGCall* node) override {
    walk_required(node->func_name, "SGCall.func_name");
    for (auto* arg : node->func_args) {
      walk_required(arg, "SGCall.func_args");
    }
  }

  void
  visitSGReturn(SGReturn* node) override {
    walk_required(node->expr, "SGReturn.expr");
  }

  void
  visitSGBlock(SGBlock* node) override {
    for (auto* stmt : node->stmts) {
      walk_required(stmt, "SGBlock.stmts");
    }
  }

  void
  visitSGEntry(SGEntry* node) override {
    for (auto* stmt : node->stmts) {
      walk_required(stmt, "SGEntry.stmts");
    }
  }

  void
  visitSGMainEntry(SGMainEntry* node) override {
    for (auto* stmt : node->stmts) {
      walk_required(stmt, "SGMainEntry.stmts");
    }
  }

  void
  visitSGLoop(SGLoop* node) override {
    walk_optional(node->cond);
    walk_required(node->body, "SGLoop.body");
  }

  void
  visitSGSeriesAvgStep(SGSeriesAvgStep* node) override {
    walk_required(node->x, "SGSeriesAvgStep.x");
  }

  void
  visitSGSeriesMaxStep(SGSeriesMaxStep* node) override {
    walk_required(node->x, "SGSeriesMaxStep.x");
  }

  void
  visitSGForEach(SGForEach* node) override {
    walk_required(node->iterable, "SGForEach.iterable");
    walk_required(node->body, "SGForEach.body");
  }

  void
  visitSGRangeFor(SGRangeFor* node) override {
    walk_required(node->start, "SGRangeFor.start");
    walk_required(node->end, "SGRangeFor.end");
    walk_required(node->step, "SGRangeFor.step");
    walk_required(node->body, "SGRangeFor.body");
  }

  void
  visitSGIf(SGIf* node) override {
    walk_required(node->cond, "SGIf.cond");
    walk_required(node->then_block, "SGIf.then_block");
    walk_optional(node->else_block);
  }

  void
  visitSGMatch(SGMatch* node) override {
    walk_required(node->scrutinee, "SGMatch.scrutinee");
    for (const auto& arm : node->int_arms) {
      walk_required(arm.second, "SGMatch.int_arms");
    }
    walk_optional(node->default_arm);
  }

  void
  visitSGFallback(SGFallback* node) override {
    walk_required(node->primary, "SGFallback.primary");
    walk_required(node->alternate, "SGFallback.alternate");
  }

  void
  visitSGWaveMerge(SGWaveMerge* node) override {
    walk_required(node->cond, "SGWaveMerge.cond");
    walk_required(node->true_val, "SGWaveMerge.true_val");
    walk_required(node->false_val, "SGWaveMerge.false_val");
  }

  void
  visitSGWaveDispatch(SGWaveDispatch* node) override {
    walk_required(node->cond, "SGWaveDispatch.cond");
    walk_required(node->true_arm, "SGWaveDispatch.true_arm");
    walk_required(node->false_arm, "SGWaveDispatch.false_arm");
  }

  void
  visitSGGuardSelect(SGGuardSelect* node) override {
    walk_required(node->base, "SGGuardSelect.base");
    walk_required(node->guard_cond, "SGGuardSelect.guard_cond");
  }

  void
  visitSGEqProbe(SGEqProbe* node) override {
    walk_required(node->base, "SGEqProbe.base");
    walk_required(node->probe, "SGEqProbe.probe");
  }

  void
  visitSGSnapshotDecl(SGSnapshotDecl* node) override {
    walk_required(node->path_expr, "SGSnapshotDecl.path_expr");
  }

  void
  visitSGFormatString(SGFormatString* node) override {
    for (auto* expr : node->exprs) {
      walk_required(expr, "SGFormatString.exprs");
    }
  }

  // SC domain — add null-requirement checks

  void
  visitSCListLiteral(SCListLiteral* node) override {
    for (auto* elem : node->elems) {
      walk_required(elem, "SCListLiteral.elems");
    }
  }

  void
  visitSCDictLiteral(SCDictLiteral* node) override {
    for (const auto& entry : node->entries) {
      walk_required(entry.key, "SCDictLiteral.key");
      walk_required(entry.value, "SCDictLiteral.value");
    }
  }

  void
  visitSCMatrixLiteral(SCMatrixLiteral* node) override {
    for (auto* elem : node->elems) {
      walk_required(elem, "SCMatrixLiteral.elems");
    }
  }

  void
  visitSCListClone(SCListClone* node) override {
    walk_required(node->source, "SCListClone.source");
  }

  void
  visitSCMatrixClone(SCMatrixClone* node) override {
    walk_required(node->source, "SCMatrixClone.source");
  }

  void
  visitSCListLen(SCListLen* node) override {
    walk_required(node->list, "SCListLen.list");
  }

  void
  visitSCListGet(SCListGet* node) override {
    walk_required(node->list, "SCListGet.list");
    walk_required(node->index, "SCListGet.index");
  }

  void
  visitSCListSlice(SCListSlice* node) override {
    walk_required(node->list, "SCListSlice.list");
    walk_required(node->start, "SCListSlice.start");
    walk_optional(node->end);
  }

  void
  visitSCListSet(SCListSet* node) override {
    walk_required(node->list, "SCListSet.list");
    walk_required(node->index, "SCListSet.index");
    walk_required(node->value, "SCListSet.value");
  }

  void
  visitSCListToString(SCListToString* node) override {
    walk_required(node->list, "SCListToString.list");
  }

  void
  visitSCMatrixGet(SCMatrixGet* node) override {
    walk_required(node->matrix, "SCMatrixGet.matrix");
    walk_required(node->row, "SCMatrixGet.row");
    walk_required(node->col, "SCMatrixGet.col");
  }

  void
  visitSCMatrixRow(SCMatrixRow* node) override {
    walk_required(node->matrix, "SCMatrixRow.matrix");
    walk_required(node->row, "SCMatrixRow.row");
  }

  void
  visitSCMatrixRowsSlice(SCMatrixRowsSlice* node) override {
    walk_required(node->matrix, "SCMatrixRowsSlice.matrix");
    walk_required(node->start, "SCMatrixRowsSlice.start");
    walk_optional(node->end);
  }

  void
  visitSCMatrixToString(SCMatrixToString* node) override {
    walk_required(node->matrix, "SCMatrixToString.matrix");
  }

  void
  visitSCDictClone(SCDictClone* node) override {
    walk_required(node->source, "SCDictClone.source");
  }

  void
  visitSCDictLen(SCDictLen* node) override {
    walk_required(node->dict, "SCDictLen.dict");
  }

  void
  visitSCDictGet(SCDictGet* node) override {
    walk_required(node->dict, "SCDictGet.dict");
    walk_required(node->key, "SCDictGet.key");
  }

  void
  visitSCDictSet(SCDictSet* node) override {
    walk_required(node->dict, "SCDictSet.dict");
    walk_required(node->key, "SCDictSet.key");
    walk_required(node->value, "SCDictSet.value");
  }

  void
  visitSCDictKeys(SCDictKeys* node) override {
    walk_required(node->dict, "SCDictKeys.dict");
  }

  void
  visitSCDictValues(SCDictValues* node) override {
    walk_required(node->dict, "SCDictValues.dict");
  }

  void
  visitSCDictToString(SCDictToString* node) override {
    walk_required(node->dict, "SCDictToString.dict");
  }

  // SIO domain — add null-requirement checks + handle state tracking

  void
  visitSIOHandleAcquire(SIOHandleAcquire* node) override {
    walk_required(node->path_expr, "SIOHandleAcquire.path_expr");
    note_acquire(node->var_name);
  }

  void
  visitSIOHandleRelease(SIOHandleRelease* node) override {
    walk_optional(node->path_expr);
    note_release(node->var_name);
  }

  void
  visitSIOFileLineIter(SIOFileLineIter* node) override {
    walk_optional(node->path_expr);
    walk_required(node->body, "SIOFileLineIter.body");
  }

  void
  visitSIOStreamZip(SIOStreamZip* node) override {
    walk_required(node->iterable_a, "SIOStreamZip.iterable_a");
    walk_required(node->iterable_b, "SIOStreamZip.iterable_b");
    walk_required(node->body, "SIOStreamZip.body");
  }

  void
  visitSIOInstantPull(SIOInstantPull* node) override {
    if (node->from_handle) {
      if (node->handle_var.empty()) {
        add_error("SIOInstantPull.handle_var is empty");
      }
      if (node->path_expr != nullptr) {
        add_error("SIOInstantPull.path_expr must be empty for handle pulls");
      }
    }
    else {
      if (!node->handle_var.empty()) {
        add_error("SIOInstantPull.handle_var must be empty for path pulls");
      }
      walk_required(node->path_expr, "SIOInstantPull.path_expr");
    }
  }

  void
  visitSIOResourceWriteToFile(SIOResourceWriteToFile* node) override {
    walk_required(node->data_expr, "SIOResourceWriteToFile.data_expr");
    walk_required(node->path_expr, "SIOResourceWriteToFile.path_expr");
  }

  void
  visitSIOStdStreamWrite(SIOStdStreamWrite* node) override {
    for (auto* expr : node->exprs) {
      walk_required(expr, "SIOStdStreamWrite.exprs");
    }
  }

  void
  visitSIOResourceEffect(SIOResourceEffect* node) override {
    walk_required(node->operation, "SIOResourceEffect.operation");
    for (const auto& handler : node->handlers) {
      walk_required(handler.body, "SIOResourceEffect.handler");
    }
    walk_optional(node->fallback);
  }

  void
  visitSIOStdStreamLineIter(SIOStdStreamLineIter* node) override {
    walk_required(node->body, "SIOStdStreamLineIter.body");
  }

  void
  visitSIOTaskCreate(SIOTaskCreate* node) override {
    walk_required(node->body, "SIOTaskCreate.body");
  }

  void
  visitSIOFlowBind(SIOFlowBind* node) override {
    walk_required(node->source_expr, "SIOFlowBind.source_expr");
    walk_optional(node->fallback_expr);
  }

  void
  visitSIOPrint(SIOPrint* node) override {
    for (auto* expr : node->expr) {
      walk_required(expr, "SIOPrint.expr");
    }
  }

  void
  visitSIORead(SIORead* node) override {
    walk_required(node->file_path, "SIORead.file_path");
  }

public:
  StyioIRVerifierResult
  result() const {
    return result_;
  }
};

}  // namespace

StyioIRVerifierResult
verify_styio_ir(const StyioIR* root) {
  if (root == nullptr) {
    StyioIRVerifierResult null_result;
    null_result.diagnostics.push_back(StyioIRVerifierDiagnostic{
      std::string(diag::kPhaseIrVerify),
      std::string(diag::kIrVerifyContract),
      "missing StyioIR root",
    });
    return null_result;
  }
  StyioIRVerifier verifier;
  // Const-cast is safe: the verifier never mutates IR nodes.
  verifier.walk(const_cast<StyioIR*>(root));
  return verifier.result();
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
