#pragma once
#ifndef STYIO_IR_WALKER_H_
#define STYIO_IR_WALKER_H_

/*
  StyioIRWalker — unified IR tree walker base class.

  Centralizes the dynamic_cast dispatch and child-traversal logic
  that was previously duplicated in StyioIROptimizer.cpp (140 casts)
  and Verifier.cpp (62 casts).

  Usage:
    class MyPass : public StyioIRWalker {
      void visitSGBinOp(SGBinOp* node) override {
        // custom logic before children
        StyioIRWalker::visitSGBinOp(node);  // walk children
        // custom logic after children
      }
    };

  Complexity:
    - dispatch(): O(1) amortized per node (single successful dynamic_cast);
      O(N_types) worst-case (82 attempts). Total tree walk is O(N_nodes).
    - Default child traversal: O(N_children) per node.

  This walker does NOT require changes to any IR node class.
  All dispatch is centralized in dispatch().
*/

// [C++ STL]
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// [Styio]
#include "../StyioToken/Token.hpp"
#include "GenIR/GenIR.hpp"

namespace styio::ir {

class StyioIRWalker
{
public:
  virtual ~StyioIRWalker() = default;

  // ---------------------------------------------------------------
  // Entry point — walk a node, guarding null and calling hooks
  // ---------------------------------------------------------------
  void
  walk(StyioIR* node) {
    if (node == nullptr) {
      return;
    }
    beforeNode(node);
    dispatch(node);
    afterNode(node);
  }

  // ---------------------------------------------------------------
  // Walk a vector of nodes
  // ---------------------------------------------------------------
  void
  walkVector(const std::vector<StyioIR*>& nodes) {
    for (auto* child : nodes) {
      walk(child);
    }
  }

  // ---------------------------------------------------------------
  // Walk a vector of typed nodes (e.g. SGVar*, SGFuncArg*)
  // ---------------------------------------------------------------
  template <typename T>
  void
  walkTypedVector(const std::vector<T*>& nodes) {
    for (auto* child : nodes) {
      walk(child);
    }
  }

  // ---------------------------------------------------------------
  // Hooks — override in subclasses
  // ---------------------------------------------------------------
  virtual void
  beforeNode(StyioIR*) {
  }

  virtual void
  afterNode(StyioIR*) {
  }

  // ---------------------------------------------------------------
  // Per-node-type visit methods with default child traversal.
  // Override in subclasses to add custom behavior.
  // ---------------------------------------------------------------

  // --- SG domain (45 types) ---

  virtual void
  visitSGResId(SGResId* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGType(SGType* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGNoOp(SGNoOp* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGConstBool(SGConstBool* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGConstInt(SGConstInt* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGConstFloat(SGConstFloat* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGConstChar(SGConstChar* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGConstString(SGConstString* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGFormatString(SGFormatString* node) {
    walkVector(node->exprs);
  }

  virtual void
  visitSGStruct(SGStruct* node) {
    walk(node->name);
    walkTypedVector(node->elements);
  }

  virtual void
  visitSGTupleCreate(SGTupleCreate* node) {
    walkVector(node->elements);
  }

  virtual void
  visitSGTupleGet(SGTupleGet* node) {
    walk(node->tuple);
  }

  virtual void
  visitSGCast(SGCast* node) {
    walk(node->value);
    walk(node->from_type);
    walk(node->to_type);
  }

  virtual void
  visitSGBinOp(SGBinOp* node) {
    walk(node->data_type);
    walk(node->lhs_expr);
    walk(node->rhs_expr);
  }

  virtual void
  visitSGCond(SGCond* node) {
    walk(node->lhs_expr);
    walk(node->rhs_expr);
  }

  virtual void
  visitSGVar(SGVar* node) {
    walk(node->var_name);
    walk(node->var_type);
    walk(node->val_init);
  }

  virtual void
  visitSGFlexBind(SGFlexBind* node) {
    walk(node->var);
    walk(node->value);
  }

  virtual void
  visitSGFinalBind(SGFinalBind* node) {
    walk(node->var);
    walk(node->value);
  }

  virtual void
  visitSGDynLoad(SGDynLoad* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGFuncArg(SGFuncArg* node) {
    walk(node->arg_type);
  }

  virtual void
  visitSGFunc(SGFunc* node) {
    walk(node->ret_type);
    walk(node->func_name);
    walkTypedVector(node->func_args);
    walk(node->func_block);
  }

  virtual void
  visitSGCall(SGCall* node) {
    if (node->is_indirect()) {
      walk(node->indirect_callee);
    }
    else {
      walk(node->func_name);
    }
    walkVector(node->func_args);
  }

  virtual void
  visitSGExportDecl(SGExportDecl* /*node*/) {
    /* leaf (symbols are strings, not IR nodes) */
  }

  virtual void
  visitSGExternBlock(SGExternBlock* /*node*/) {
    /* leaf (body/source_paths are strings, not IR nodes) */
  }

  virtual void
  visitSGReturn(SGReturn* node) {
    walk(node->expr);
  }

  virtual void
  visitSGBlock(SGBlock* node) {
    walkVector(node->stmts);
  }

  virtual void
  visitSGEntry(SGEntry* node) {
    walkVector(node->stmts);
  }

  virtual void
  visitSGMainEntry(SGMainEntry* node) {
    walkVector(node->stmts);
  }

  virtual void
  visitSGLoop(SGLoop* node) {
    walk(node->cond);
    walk(node->body);
  }

  virtual void
  visitSGForEach(SGForEach* node) {
    walk(node->iterable);
    walk(node->body);
  }

  virtual void
  visitSGRangeFor(SGRangeFor* node) {
    walk(node->start);
    walk(node->end);
    walk(node->step);
    walk(node->body);
  }

  virtual void
  visitSGIf(SGIf* node) {
    walk(node->cond);
    walk(node->then_block);
    walk(node->else_block);
  }

  virtual void
  visitSGMatch(SGMatch* node) {
    walk(node->scrutinee);
    for (auto& arm : node->int_arms) {
      walk(arm.second);
    }
    walk(node->default_arm);
  }

  virtual void
  visitSGBreak(SGBreak* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGContinue(SGContinue* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGUndef(SGUndef* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGFallback(SGFallback* node) {
    walk(node->primary);
    walk(node->alternate);
  }

  virtual void
  visitSGWaveMerge(SGWaveMerge* node) {
    walk(node->cond);
    walk(node->true_val);
    walk(node->false_val);
  }

  virtual void
  visitSGWaveDispatch(SGWaveDispatch* node) {
    walk(node->cond);
    walk(node->true_arm);
    walk(node->false_arm);
  }

  virtual void
  visitSGGuardSelect(SGGuardSelect* node) {
    walk(node->base);
    walk(node->guard_cond);
  }

  virtual void
  visitSGEqProbe(SGEqProbe* node) {
    walk(node->base);
    walk(node->probe);
  }

  virtual void
  visitSGSnapshotDecl(SGSnapshotDecl* node) {
    walk(node->path_expr);
  }

  virtual void
  visitSGSnapshotShadowLoad(SGSnapshotShadowLoad* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGStateSnapLoad(SGStateSnapLoad* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGStateHistLoad(SGStateHistLoad* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSGSeriesAvgStep(SGSeriesAvgStep* node) {
    walk(node->x);
  }

  virtual void
  visitSGSeriesMaxStep(SGSeriesMaxStep* node) {
    walk(node->x);
  }

  // --- SC domain (21 types) ---

  virtual void
  visitSCListLiteral(SCListLiteral* node) {
    walkVector(node->elems);
  }

  virtual void
  visitSCDictLiteral(SCDictLiteral* node) {
    for (auto& entry : node->entries) {
      walk(entry.key);
      walk(entry.value);
    }
  }

  virtual void
  visitSCMatrixLiteral(SCMatrixLiteral* node) {
    walkVector(node->elems);
  }

  virtual void
  visitSCListClone(SCListClone* node) {
    walk(node->source);
  }

  virtual void
  visitSCMatrixClone(SCMatrixClone* node) {
    walk(node->source);
  }

  virtual void
  visitSCListLen(SCListLen* node) {
    walk(node->list);
  }

  virtual void
  visitSCListGet(SCListGet* node) {
    walk(node->list);
    walk(node->index);
  }

  virtual void
  visitSCListSlice(SCListSlice* node) {
    walk(node->list);
    walk(node->start);
    walk(node->end);
  }

  virtual void
  visitSCListSet(SCListSet* node) {
    walk(node->list);
    walk(node->index);
    walk(node->value);
  }

  virtual void
  visitSCListToString(SCListToString* node) {
    walk(node->list);
  }

  virtual void
  visitSCMatrixGet(SCMatrixGet* node) {
    walk(node->matrix);
    walk(node->row);
    walk(node->col);
  }

  virtual void
  visitSCMatrixRow(SCMatrixRow* node) {
    walk(node->matrix);
    walk(node->row);
  }

  virtual void
  visitSCMatrixRowsSlice(SCMatrixRowsSlice* node) {
    walk(node->matrix);
    walk(node->start);
    walk(node->end);
  }

  virtual void
  visitSCMatrixToString(SCMatrixToString* node) {
    walk(node->matrix);
  }

  virtual void
  visitSCDictClone(SCDictClone* node) {
    walk(node->source);
  }

  virtual void
  visitSCDictLen(SCDictLen* node) {
    walk(node->dict);
  }

  virtual void
  visitSCDictGet(SCDictGet* node) {
    walk(node->dict);
    walk(node->key);
  }

  virtual void
  visitSCDictSet(SCDictSet* node) {
    walk(node->dict);
    walk(node->key);
    walk(node->value);
  }

  virtual void
  visitSCDictKeys(SCDictKeys* node) {
    walk(node->dict);
  }

  virtual void
  visitSCDictValues(SCDictValues* node) {
    walk(node->dict);
  }

  virtual void
  visitSCDictToString(SCDictToString* node) {
    walk(node->dict);
  }

  // --- SIO domain (16 types) ---

  virtual void
  visitSIOHandleAcquire(SIOHandleAcquire* node) {
    walk(node->path_expr);
  }

  virtual void
  visitSIOHandleRelease(SIOHandleRelease* node) {
    walk(node->path_expr);
  }

  virtual void
  visitSIOFileLineIter(SIOFileLineIter* node) {
    walk(node->path_expr);
    walk(node->body);
  }

  virtual void
  visitSIOStreamZip(SIOStreamZip* node) {
    walk(node->iterable_a);
    walk(node->iterable_b);
    walk(node->body);
  }

  virtual void
  visitSIOInstantPull(SIOInstantPull* node) {
    walk(node->path_expr);
  }

  virtual void
  visitSIOListReadStdin(SIOListReadStdin* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSIOResourceWriteToFile(SIOResourceWriteToFile* node) {
    walk(node->data_expr);
    walk(node->path_expr);
  }

  virtual void
  visitSIOStdStreamWrite(SIOStdStreamWrite* node) {
    walkVector(node->exprs);
  }

  virtual void
  visitSIOResourceEffect(SIOResourceEffect* node) {
    walk(node->operation);
    walk(node->fallback);
    for (auto& handler : node->handlers) {
      walk(handler.body);
    }
  }

  virtual void
  visitSIOStdStreamLineIter(SIOStdStreamLineIter* node) {
    walk(node->body);
  }

  virtual void
  visitSIOStdStreamPull(SIOStdStreamPull* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSIOTaskCreate(SIOTaskCreate* node) {
    walk(node->body);
  }

  virtual void
  visitSIOFlowBind(SIOFlowBind* node) {
    walk(node->source_expr);
    walk(node->fallback_expr);
  }

  virtual void
  visitSIOPath(SIOPath* /*node*/) {
    /* leaf */
  }

  virtual void
  visitSIOPrint(SIOPrint* node) {
    walkVector(node->expr);
  }

  virtual void
  visitSIORead(SIORead* node) {
    walk(node->file_path);
  }

protected:
  // ---------------------------------------------------------------
  // Central dispatch — the only dynamic_cast chain in the system.
  // All passes share this single dispatch implementation.
  // ---------------------------------------------------------------
  virtual void
  dispatch(StyioIR* node) {
    // SG domain — ordered by expected frequency
    if (auto* n = dynamic_cast<SGBlock*>(node)) {
      visitSGBlock(n);
    }
    else if (auto* n = dynamic_cast<SGFlexBind*>(node)) {
      visitSGFlexBind(n);
    }
    else if (auto* n = dynamic_cast<SGCall*>(node)) {
      visitSGCall(n);
    }
    else if (auto* n = dynamic_cast<SGBinOp*>(node)) {
      visitSGBinOp(n);
    }
    else if (auto* n = dynamic_cast<SGReturn*>(node)) {
      visitSGReturn(n);
    }
    else if (auto* n = dynamic_cast<SGIf*>(node)) {
      visitSGIf(n);
    }
    else if (auto* n = dynamic_cast<SGMatch*>(node)) {
      visitSGMatch(n);
    }
    else if (auto* n = dynamic_cast<SGFinalBind*>(node)) {
      visitSGFinalBind(n);
    }
    else if (auto* n = dynamic_cast<SGConstInt*>(node)) {
      visitSGConstInt(n);
    }
    else if (auto* n = dynamic_cast<SGConstString*>(node)) {
      visitSGConstString(n);
    }
    else if (auto* n = dynamic_cast<SGResId*>(node)) {
      visitSGResId(n);
    }
    else if (auto* n = dynamic_cast<SGVar*>(node)) {
      visitSGVar(n);
    }
    else if (auto* n = dynamic_cast<SGType*>(node)) {
      visitSGType(n);
    }
    else if (auto* n = dynamic_cast<SGCond*>(node)) {
      visitSGCond(n);
    }
    else if (auto* n = dynamic_cast<SGConstFloat*>(node)) {
      visitSGConstFloat(n);
    }
    else if (auto* n = dynamic_cast<SGCast*>(node)) {
      visitSGCast(n);
    }
    else if (auto* n = dynamic_cast<SGLoop*>(node)) {
      visitSGLoop(n);
    }
    else if (auto* n = dynamic_cast<SGForEach*>(node)) {
      visitSGForEach(n);
    }
    else if (auto* n = dynamic_cast<SGConstBool*>(node)) {
      visitSGConstBool(n);
    }
    else if (auto* n = dynamic_cast<SGDynLoad*>(node)) {
      visitSGDynLoad(n);
    }
    else if (auto* n = dynamic_cast<SGFunc*>(node)) {
      visitSGFunc(n);
    }
    else if (auto* n = dynamic_cast<SGEntry*>(node)) {
      visitSGEntry(n);
    }
    else if (auto* n = dynamic_cast<SGMainEntry*>(node)) {
      visitSGMainEntry(n);
    }
    else if (auto* n = dynamic_cast<SGNoOp*>(node)) {
      visitSGNoOp(n);
    }
    else if (auto* n = dynamic_cast<SGConstChar*>(node)) {
      visitSGConstChar(n);
    }
    else if (auto* n = dynamic_cast<SGFormatString*>(node)) {
      visitSGFormatString(n);
    }
    else if (auto* n = dynamic_cast<SGStruct*>(node)) {
      visitSGStruct(n);
    }
    else if (auto* n = dynamic_cast<SGTupleCreate*>(node)) {
      visitSGTupleCreate(n);
    }
    else if (auto* n = dynamic_cast<SGTupleGet*>(node)) {
      visitSGTupleGet(n);
    }
    else if (auto* n = dynamic_cast<SGFuncArg*>(node)) {
      visitSGFuncArg(n);
    }
    else if (auto* n = dynamic_cast<SGExportDecl*>(node)) {
      visitSGExportDecl(n);
    }
    else if (auto* n = dynamic_cast<SGExternBlock*>(node)) {
      visitSGExternBlock(n);
    }
    else if (auto* n = dynamic_cast<SGRangeFor*>(node)) {
      visitSGRangeFor(n);
    }
    else if (auto* n = dynamic_cast<SGBreak*>(node)) {
      visitSGBreak(n);
    }
    else if (auto* n = dynamic_cast<SGContinue*>(node)) {
      visitSGContinue(n);
    }
    else if (auto* n = dynamic_cast<SGUndef*>(node)) {
      visitSGUndef(n);
    }
    else if (auto* n = dynamic_cast<SGFallback*>(node)) {
      visitSGFallback(n);
    }
    else if (auto* n = dynamic_cast<SGWaveMerge*>(node)) {
      visitSGWaveMerge(n);
    }
    else if (auto* n = dynamic_cast<SGWaveDispatch*>(node)) {
      visitSGWaveDispatch(n);
    }
    else if (auto* n = dynamic_cast<SGGuardSelect*>(node)) {
      visitSGGuardSelect(n);
    }
    else if (auto* n = dynamic_cast<SGEqProbe*>(node)) {
      visitSGEqProbe(n);
    }
    else if (auto* n = dynamic_cast<SGSnapshotDecl*>(node)) {
      visitSGSnapshotDecl(n);
    }
    else if (auto* n = dynamic_cast<SGSnapshotShadowLoad*>(node)) {
      visitSGSnapshotShadowLoad(n);
    }
    else if (auto* n = dynamic_cast<SGStateSnapLoad*>(node)) {
      visitSGStateSnapLoad(n);
    }
    else if (auto* n = dynamic_cast<SGStateHistLoad*>(node)) {
      visitSGStateHistLoad(n);
    }
    else if (auto* n = dynamic_cast<SGSeriesAvgStep*>(node)) {
      visitSGSeriesAvgStep(n);
    }
    else if (auto* n = dynamic_cast<SGSeriesMaxStep*>(node)) {
      visitSGSeriesMaxStep(n);
    }
    // SC domain
    else if (auto* n = dynamic_cast<SCListLiteral*>(node)) {
      visitSCListLiteral(n);
    }
    else if (auto* n = dynamic_cast<SCListGet*>(node)) {
      visitSCListGet(n);
    }
    else if (auto* n = dynamic_cast<SCListSlice*>(node)) {
      visitSCListSlice(n);
    }
    else if (auto* n = dynamic_cast<SCListSet*>(node)) {
      visitSCListSet(n);
    }
    else if (auto* n = dynamic_cast<SCListLen*>(node)) {
      visitSCListLen(n);
    }
    else if (auto* n = dynamic_cast<SCDictLiteral*>(node)) {
      visitSCDictLiteral(n);
    }
    else if (auto* n = dynamic_cast<SCDictGet*>(node)) {
      visitSCDictGet(n);
    }
    else if (auto* n = dynamic_cast<SCDictSet*>(node)) {
      visitSCDictSet(n);
    }
    else if (auto* n = dynamic_cast<SCDictLen*>(node)) {
      visitSCDictLen(n);
    }
    else if (auto* n = dynamic_cast<SCMatrixLiteral*>(node)) {
      visitSCMatrixLiteral(n);
    }
    else if (auto* n = dynamic_cast<SCMatrixGet*>(node)) {
      visitSCMatrixGet(n);
    }
    else if (auto* n = dynamic_cast<SCMatrixRow*>(node)) {
      visitSCMatrixRow(n);
    }
    else if (auto* n = dynamic_cast<SCMatrixRowsSlice*>(node)) {
      visitSCMatrixRowsSlice(n);
    }
    else if (auto* n = dynamic_cast<SCListClone*>(node)) {
      visitSCListClone(n);
    }
    else if (auto* n = dynamic_cast<SCMatrixClone*>(node)) {
      visitSCMatrixClone(n);
    }
    else if (auto* n = dynamic_cast<SCListToString*>(node)) {
      visitSCListToString(n);
    }
    else if (auto* n = dynamic_cast<SCMatrixToString*>(node)) {
      visitSCMatrixToString(n);
    }
    else if (auto* n = dynamic_cast<SCDictClone*>(node)) {
      visitSCDictClone(n);
    }
    else if (auto* n = dynamic_cast<SCDictKeys*>(node)) {
      visitSCDictKeys(n);
    }
    else if (auto* n = dynamic_cast<SCDictValues*>(node)) {
      visitSCDictValues(n);
    }
    else if (auto* n = dynamic_cast<SCDictToString*>(node)) {
      visitSCDictToString(n);
    }
    // SIO domain
    else if (auto* n = dynamic_cast<SIOResourceEffect*>(node)) {
      visitSIOResourceEffect(n);
    }
    else if (auto* n = dynamic_cast<SIOStreamZip*>(node)) {
      visitSIOStreamZip(n);
    }
    else if (auto* n = dynamic_cast<SIOFileLineIter*>(node)) {
      visitSIOFileLineIter(n);
    }
    else if (auto* n = dynamic_cast<SIOHandleAcquire*>(node)) {
      visitSIOHandleAcquire(n);
    }
    else if (auto* n = dynamic_cast<SIOHandleRelease*>(node)) {
      visitSIOHandleRelease(n);
    }
    else if (auto* n = dynamic_cast<SIOResourceWriteToFile*>(node)) {
      visitSIOResourceWriteToFile(n);
    }
    else if (auto* n = dynamic_cast<SIOStdStreamWrite*>(node)) {
      visitSIOStdStreamWrite(n);
    }
    else if (auto* n = dynamic_cast<SIOInstantPull*>(node)) {
      visitSIOInstantPull(n);
    }
    else if (auto* n = dynamic_cast<SIOStdStreamLineIter*>(node)) {
      visitSIOStdStreamLineIter(n);
    }
    else if (auto* n = dynamic_cast<SIOStdStreamPull*>(node)) {
      visitSIOStdStreamPull(n);
    }
    else if (auto* n = dynamic_cast<SIOTaskCreate*>(node)) {
      visitSIOTaskCreate(n);
    }
    else if (auto* n = dynamic_cast<SIOFlowBind*>(node)) {
      visitSIOFlowBind(n);
    }
    else if (auto* n = dynamic_cast<SIOPrint*>(node)) {
      visitSIOPrint(n);
    }
    else if (auto* n = dynamic_cast<SIORead*>(node)) {
      visitSIORead(n);
    }
    else if (auto* n = dynamic_cast<SIOPath*>(node)) {
      visitSIOPath(n);
    }
    else if (auto* n = dynamic_cast<SIOListReadStdin*>(node)) {
      visitSIOListReadStdin(n);
    }
    else {
      // Unknown IR node type — subclass may handle this
      visitUnknown(node);
    }
  }

  // ---------------------------------------------------------------
  // Fallback for unrecognized IR node types
  // ---------------------------------------------------------------
  virtual void
  visitUnknown(StyioIR* /*node*/) {
    /* no-op by default; subclasses may override for diagnostics */
  }
};

}  // namespace styio::ir

#endif  // STYIO_IR_WALKER_H_
