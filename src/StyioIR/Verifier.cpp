#include "Verifier.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <stdexcept>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>

#include "../StyioException/Exception.hpp"
#include "GenIR/GenIR.hpp"
#include "PortableCallableBody.hpp"
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
    if (!node->specialization_content_digest.empty()) {
      const bool canonical_digest =
        node->specialization_content_digest.size() == 64
        && std::all_of(
          node->specialization_content_digest.begin(),
          node->specialization_content_digest.end(),
          [](unsigned char ch)
          {
            return (ch >= '0' && ch <= '9')
              || (ch >= 'a' && ch <= 'f');
          });
      if (!canonical_digest) {
        add_error(
          "SGFunc specialization digest must be canonical lowercase sha256");
      }
      else if (node->func_name != nullptr
               && !node->func_name->as_str().ends_with(
                    node->specialization_content_digest)) {
        add_error(
          "SGFunc specialization symbol must end in its content digest");
      }
    }
    for (auto* arg : node->func_args) {
      walk_required(arg, "SGFunc.func_args");
    }
    walk_required(node->func_block, "SGFunc.func_block");
  }

  void
  visitSGCall(SGCall* node) override {
    if (node->is_indirect()) {
      walk_required(
        node->indirect_callee,
        "SGCall.indirect_callee");
      if (!styio_is_callable_type(node->callable_type)) {
        add_error(
          "SGCall indirect call requires a canonical callable type");
      }
    }
    else {
      walk_required(node->func_name, "SGCall.func_name");
    }
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

namespace {

using PortableTerm = PortableCallableTypeTerm;
using ConstraintKind = StyioSemaContext::CallableConstraintKind;

PortableTerm
portable_concrete_term(const std::string& name) {
  return portable_callable_term_from_data_type(
    name == "undefined"
      ? StyioDataType{
          StyioDataTypeOption::Undefined,
          "undefined",
          0}
      : styio_data_type_from_name(name));
}

bool
portable_term_is_undefined(const PortableTerm& term) {
  return term.kind == PortableTerm::Kind::Concrete
         && term.concrete.isUndefined();
}

bool
portable_term_is_numeric_concrete(const PortableTerm& term) {
  return term.kind == PortableTerm::Kind::Concrete
         && (term.concrete.isInteger() || term.concrete.isFloat());
}

bool
portable_term_is_comparable_concrete(const PortableTerm& term) {
  if (term.kind != PortableTerm::Kind::Concrete) {
    return false;
  }
  switch (term.concrete.option) {
    case StyioDataTypeOption::Bool:
    case StyioDataTypeOption::Integer:
    case StyioDataTypeOption::Float:
    case StyioDataTypeOption::Char:
    case StyioDataTypeOption::String:
      return true;
    default:
      return false;
  }
}

bool
portable_constraint_matches(
  const PortableCallableTypeConstraint& constraint,
  ConstraintKind kind,
  const PortableTerm& subject,
  const PortableTerm* argument = nullptr,
  const PortableTerm* result = nullptr
) {
  if (constraint.kind != kind
      || !portable_callable_terms_equal(
           constraint.subject,
           subject)) {
    return false;
  }
  if (argument != nullptr
      && !portable_callable_terms_equal(
           constraint.argument,
           *argument)) {
    return false;
  }
  if (result != nullptr
      && !portable_callable_terms_equal(
           constraint.result,
           *result)) {
    return false;
  }
  return true;
}

bool
portable_signature_has_constraint(
  const PortableCallableSignature& signature,
  ConstraintKind kind,
  const PortableTerm& subject,
  const PortableTerm* argument = nullptr,
  const PortableTerm* result = nullptr
) {
  return std::any_of(
    signature.constraints.begin(),
    signature.constraints.end(),
    [&](const auto& constraint)
    {
      return portable_constraint_matches(
        constraint,
        kind,
        subject,
        argument,
        result);
    });
}

bool
portable_term_is_numeric(
  const PortableTerm& term,
  const PortableCallableSignature& signature
) {
  return portable_term_is_numeric_concrete(term)
         || portable_signature_has_constraint(
              signature,
              ConstraintKind::Numeric,
              term);
}

bool
portable_term_is_comparable(
  const PortableTerm& term,
  const PortableCallableSignature& signature
) {
  return portable_term_is_comparable_concrete(term)
         || portable_signature_has_constraint(
              signature,
              ConstraintKind::Comparable,
              term)
         || portable_signature_has_constraint(
              signature,
              ConstraintKind::Numeric,
              term);
}

bool
portable_bind_pattern(
  const PortableTerm& pattern,
  const PortableTerm& actual,
  std::unordered_map<std::uint32_t, PortableTerm>& bindings
) {
  if (pattern.kind == PortableTerm::Kind::Variable) {
    auto [it, inserted] =
      bindings.emplace(pattern.variable, actual);
    return inserted
           || portable_callable_terms_equal(it->second, actual);
  }
  if (pattern.kind != actual.kind
      || pattern.arguments.size() != actual.arguments.size()) {
    return false;
  }
  if (pattern.kind == PortableTerm::Kind::Concrete
      && pattern.concrete.name != actual.concrete.name) {
    return false;
  }
  for (std::size_t i = 0; i < pattern.arguments.size(); ++i) {
    if (!portable_bind_pattern(
          pattern.arguments[i],
          actual.arguments[i],
          bindings)) {
      return false;
    }
  }
  return true;
}

std::optional<PortableTerm>
portable_substitute_term(
  const PortableTerm& term,
  const std::unordered_map<std::uint32_t, PortableTerm>& bindings
) {
  if (term.kind == PortableTerm::Kind::Variable) {
    auto binding = bindings.find(term.variable);
    if (binding == bindings.end()) {
      return std::nullopt;
    }
    return binding->second;
  }
  PortableTerm substituted = term;
  substituted.arguments.clear();
  for (const auto& argument : term.arguments) {
    auto value = portable_substitute_term(argument, bindings);
    if (!value.has_value()) {
      return std::nullopt;
    }
    substituted.arguments.push_back(std::move(*value));
  }
  return substituted;
}

bool
portable_constraint_is_proven(
  const PortableCallableTypeConstraint& constraint,
  const PortableCallableSignature& caller,
  const std::unordered_map<std::uint32_t, PortableTerm>& bindings
) {
  auto subject =
    portable_substitute_term(constraint.subject, bindings);
  if (!subject.has_value()) {
    return false;
  }
  switch (constraint.kind) {
    case ConstraintKind::Numeric:
      return portable_term_is_numeric(*subject, caller);
    case ConstraintKind::Comparable:
      return portable_term_is_comparable(*subject, caller);
    case ConstraintKind::Cloneable:
      if (subject->kind == PortableTerm::Kind::List
          || subject->kind == PortableTerm::Kind::Dict) {
        return true;
      }
      if (subject->kind == PortableTerm::Kind::Concrete) {
        const auto option = subject->concrete.option;
        return option == StyioDataTypeOption::Bool
               || option == StyioDataTypeOption::Integer
               || option == StyioDataTypeOption::Float
               || option == StyioDataTypeOption::Char
               || option == StyioDataTypeOption::String
               || styio_type_is_cloneable(subject->concrete);
      }
      return portable_signature_has_constraint(
        caller,
        ConstraintKind::Cloneable,
        *subject);
    case ConstraintKind::Iterable:
      if (subject->kind == PortableTerm::Kind::List
          || subject->kind == PortableTerm::Kind::Dict) {
        return true;
      }
      if (subject->kind == PortableTerm::Kind::Concrete
          && styio_type_is_iterable(subject->concrete)) {
        return true;
      }
      if (auto result =
            portable_substitute_term(
              constraint.result,
              bindings)) {
        return portable_signature_has_constraint(
          caller,
          ConstraintKind::Iterable,
          *subject,
          nullptr,
          &*result);
      }
      return false;
    case ConstraintKind::Indexable: {
      auto argument =
        portable_substitute_term(
          constraint.argument,
          bindings);
      auto result =
        portable_substitute_term(
          constraint.result,
          bindings);
      if (!argument.has_value() || !result.has_value()) {
        return false;
      }
      if (subject->kind == PortableTerm::Kind::List) {
        return portable_callable_terms_equal(
                 subject->arguments.at(0),
                 *result)
               && argument->kind == PortableTerm::Kind::Concrete
               && argument->concrete.isInteger();
      }
      if (subject->kind == PortableTerm::Kind::Dict) {
        return portable_callable_terms_equal(
                 subject->arguments.at(0),
                 *argument)
               && portable_callable_terms_equal(
                 subject->arguments.at(1),
                 *result);
      }
      return portable_signature_has_constraint(
        caller,
        ConstraintKind::Indexable,
        *subject,
        &*argument,
        &*result);
    }
  }
  return false;
}

class PortableCallableBodyVerifier
{
  PortableCallableBody& body_;
  const PortableCallableSignature& signature_;
  const PortableCallableCatalog& catalog_;
  bool require_encoded_types_;
  std::vector<PortableTerm> inferred_types_;
  std::vector<std::size_t> use_counts_;
  std::unordered_map<std::string, PortableTerm> locals_;
  std::size_t total_inputs_ = 0;

  std::string
  fail(std::size_t node, const std::string& detail) const {
    return "portable StyioIR node "
           + std::to_string(node) + " " + detail;
  }

  bool
  has_input_count(
    const PortableCallableNode& node,
    std::size_t expected
  ) const {
    return node.inputs.size() == expected;
  }

  std::optional<PortableTerm>
  infer_direct_call(
    const PortableCallableNode& node,
    std::string& error
  ) const {
    auto callee = catalog_.find(node.symbol);
    if (callee == catalog_.end()) {
      error =
        "references unbound direct callable `" + node.symbol + "`";
      return std::nullopt;
    }
    if (callee->second.params.size() != node.inputs.size()) {
      error =
        "has an argument count that does not match callable `"
        + node.symbol + "`";
      return std::nullopt;
    }
    std::unordered_map<std::uint32_t, PortableTerm> bindings;
    for (std::size_t i = 0; i < node.inputs.size(); ++i) {
      if (!portable_bind_pattern(
            callee->second.params[i],
            inferred_types_.at(node.inputs[i]),
            bindings)) {
        error =
          "has an argument type mismatch for callable `"
          + node.symbol + "`";
        return std::nullopt;
      }
    }
    for (const auto& constraint : callee->second.constraints) {
      if (!portable_constraint_is_proven(
            constraint,
            signature_,
            bindings)) {
        error =
          "cannot prove callable constraint for `" + node.symbol + "`";
        return std::nullopt;
      }
    }
    auto result =
      portable_substitute_term(callee->second.result, bindings);
    if (!result.has_value()) {
      error =
        "leaves the result of callable `" + node.symbol
        + "` underconstrained";
    }
    return result;
  }

  std::optional<PortableTerm>
  infer_indirect_call(
    const PortableCallableNode& node,
    std::string& error
  ) const {
    auto callee = locals_.find(node.symbol);
    if (callee == locals_.end()
        || callee->second.kind != PortableTerm::Kind::Concrete
        || !styio_is_callable_type(callee->second.concrete)) {
      error =
        "references unbound callable value `" + node.symbol + "`";
      return std::nullopt;
    }
    const auto params =
      styio_callable_param_types(callee->second.concrete);
    if (params.size() != node.inputs.size()) {
      error =
        "has an argument count that does not match callable value `"
        + node.symbol + "`";
      return std::nullopt;
    }
    for (std::size_t i = 0; i < params.size(); ++i) {
      const PortableTerm expected =
        portable_callable_term_from_data_type(params[i]);
      if (!portable_callable_terms_equal(
            expected,
            inferred_types_.at(node.inputs[i]))) {
        error =
          "has an argument type mismatch for callable value `"
          + node.symbol + "`";
        return std::nullopt;
      }
    }
    return portable_callable_term_from_data_type(
      styio_callable_result_type(callee->second.concrete));
  }

  std::optional<PortableTerm>
  infer_node(std::size_t index, std::string& error) {
    auto& node = body_.nodes[index];
    if (node.opcode == "load") {
      if (!node.inputs.empty() || node.symbol.empty()) {
        error = fail(index, "has an invalid load shape");
        return std::nullopt;
      }
      auto local = locals_.find(node.symbol);
      if (local == locals_.end()) {
        error =
          fail(
            index,
            "references unbound symbol `" + node.symbol + "`");
        return std::nullopt;
      }
      return local->second;
    }
    if (node.opcode == "bool") {
      if (!node.inputs.empty()
          || (node.value != "true" && node.value != "false")) {
        error = fail(index, "has an invalid bool literal");
        return std::nullopt;
      }
      return portable_concrete_term("bool");
    }
    if (node.opcode == "i64") {
      if (!node.inputs.empty() || node.value.empty()) {
        error = fail(index, "has an invalid i64 literal");
        return std::nullopt;
      }
      try {
        std::size_t used = 0;
        (void)std::stoll(node.value, &used, 10);
        if (used != node.value.size()) {
          throw std::invalid_argument("trailing");
        }
      }
      catch (const std::exception&) {
        error = fail(index, "has an invalid i64 literal");
        return std::nullopt;
      }
      return portable_concrete_term("i64");
    }
    if (node.opcode == "f64") {
      if (!node.inputs.empty() || node.value.empty()) {
        error = fail(index, "has an invalid f64 literal");
        return std::nullopt;
      }
      try {
        std::size_t used = 0;
        (void)std::stod(node.value, &used);
        if (used != node.value.size()) {
          throw std::invalid_argument("trailing");
        }
      }
      catch (const std::exception&) {
        error = fail(index, "has an invalid f64 literal");
        return std::nullopt;
      }
      return portable_concrete_term("f64");
    }
    if (node.opcode == "char") {
      if (!node.inputs.empty() || node.value.empty()) {
        error = fail(index, "has an invalid char literal");
        return std::nullopt;
      }
      return portable_concrete_term("char");
    }
    if (node.opcode == "string") {
      if (!node.inputs.empty()) {
        error = fail(index, "has an invalid string literal");
        return std::nullopt;
      }
      return portable_concrete_term("string");
    }
    if (node.opcode == "binary") {
      static const std::unordered_set<std::string> operations = {
        "add", "sub", "mul", "div", "pow", "mod",
      };
      if (!has_input_count(node, 2)
          || operations.count(node.operation) == 0) {
        error = fail(index, "has an invalid binary operation");
        return std::nullopt;
      }
      const auto& lhs = inferred_types_.at(node.inputs[0]);
      const auto& rhs = inferred_types_.at(node.inputs[1]);
      if (!portable_callable_terms_equal(lhs, rhs)
          || !portable_term_is_numeric(lhs, signature_)) {
        error = fail(index, "has a binary operand type mismatch");
        return std::nullopt;
      }
      return lhs;
    }
    if (node.opcode == "compare") {
      static const std::unordered_set<std::string> operations = {
        "eq", "gt", "ge", "lt", "le", "ne",
      };
      if (!has_input_count(node, 2)
          || operations.count(node.operation) == 0) {
        error = fail(index, "has an invalid comparison");
        return std::nullopt;
      }
      const auto& lhs = inferred_types_.at(node.inputs[0]);
      const auto& rhs = inferred_types_.at(node.inputs[1]);
      if (!portable_callable_terms_equal(lhs, rhs)
          || !portable_term_is_comparable(lhs, signature_)) {
        error = fail(index, "has a comparison operand type mismatch");
        return std::nullopt;
      }
      return portable_concrete_term("bool");
    }
    if (node.opcode == "logic") {
      const bool unary =
        node.operation == "raw" || node.operation == "not";
      const bool binary =
        node.operation == "and"
        || node.operation == "or"
        || node.operation == "xor";
      if ((!unary && !binary)
          || node.inputs.size() != (unary ? 1 : 2)) {
        error = fail(index, "has an invalid logical operation");
        return std::nullopt;
      }
      const PortableTerm boolean = portable_concrete_term("bool");
      for (const auto input : node.inputs) {
        if (!portable_callable_terms_equal(
              inferred_types_.at(input),
              boolean)) {
          error = fail(index, "has a non-boolean logical operand");
          return std::nullopt;
        }
      }
      return boolean;
    }
    if (node.opcode == "call") {
      auto result = infer_direct_call(node, error);
      if (!result.has_value() && !error.empty()) {
        error = fail(index, error);
      }
      return result;
    }
    if (node.opcode == "indirect_call") {
      auto result = infer_indirect_call(node, error);
      if (!result.has_value() && !error.empty()) {
        error = fail(index, error);
      }
      return result;
    }
    if (node.opcode == "list") {
      if (node.inputs.empty()) {
        error =
          fail(index, "cannot infer an empty portable list literal");
        return std::nullopt;
      }
      const auto& element = inferred_types_.at(node.inputs.front());
      for (const auto input : node.inputs) {
        if (!portable_callable_terms_equal(
              element,
              inferred_types_.at(input))) {
          error = fail(index, "has non-uniform list element types");
          return std::nullopt;
        }
      }
      PortableTerm list;
      list.kind = PortableTerm::Kind::List;
      list.arguments.push_back(element);
      return list;
    }
    if (node.opcode == "dict") {
      if (node.inputs.empty() || node.inputs.size() % 2 != 0) {
        error =
          fail(index, "has an invalid portable dictionary shape");
        return std::nullopt;
      }
      const auto& key = inferred_types_.at(node.inputs[0]);
      const auto& value = inferred_types_.at(node.inputs[1]);
      for (std::size_t input = 0;
           input < node.inputs.size();
           input += 2) {
        if (!portable_callable_terms_equal(
              key,
              inferred_types_.at(node.inputs[input]))
            || !portable_callable_terms_equal(
              value,
              inferred_types_.at(node.inputs[input + 1]))) {
          error = fail(index, "has non-uniform dictionary entry types");
          return std::nullopt;
        }
      }
      PortableTerm dict;
      dict.kind = PortableTerm::Kind::Dict;
      dict.arguments = {key, value};
      return dict;
    }
    if (node.opcode == "index") {
      if (!has_input_count(node, 2)) {
        error = fail(index, "has an invalid index operation");
        return std::nullopt;
      }
      const auto& base = inferred_types_.at(node.inputs[0]);
      const auto& key = inferred_types_.at(node.inputs[1]);
      if (base.kind == PortableTerm::Kind::List) {
        if (key.kind != PortableTerm::Kind::Concrete
            || !key.concrete.isInteger()) {
          error = fail(index, "has a non-integer list index");
          return std::nullopt;
        }
        return base.arguments.at(0);
      }
      if (base.kind == PortableTerm::Kind::Dict
          && portable_callable_terms_equal(
               key,
               base.arguments.at(0))) {
        return base.arguments.at(1);
      }
      error = fail(index, "indexes a non-indexable value");
      return std::nullopt;
    }
    if (node.opcode == "block") {
      PortableTerm result = portable_concrete_term("undefined");
      for (const auto input : node.inputs) {
        const auto& candidate = inferred_types_.at(input);
        if (!portable_term_is_undefined(candidate)) {
          result = candidate;
        }
      }
      return result;
    }
    if (node.opcode == "return") {
      if (!has_input_count(node, 1)) {
        error = fail(index, "has an invalid return operation");
        return std::nullopt;
      }
      return inferred_types_.at(node.inputs[0]);
    }
    if (node.opcode == "print") {
      return portable_concrete_term("undefined");
    }
    if (node.opcode == "final_bind"
        || node.opcode == "flex_bind") {
      if (!has_input_count(node, 1) || node.symbol.empty()) {
        error = fail(index, "has an invalid binding operation");
        return std::nullopt;
      }
      const auto value = inferred_types_.at(node.inputs[0]);
      auto local = locals_.find(node.symbol);
      if (node.opcode == "final_bind"
          && local != locals_.end()) {
        error =
          fail(
            index,
            "redefines bound symbol `" + node.symbol + "`");
        return std::nullopt;
      }
      if (local != locals_.end()
          && !portable_callable_terms_equal(
               local->second,
               value)) {
        error =
          fail(
            index,
            "changes the type of binding `" + node.symbol + "`");
        return std::nullopt;
      }
      locals_[node.symbol] = value;
      return portable_concrete_term("undefined");
    }
    if (node.opcode == "pass") {
      if (!node.inputs.empty()) {
        error = fail(index, "has an invalid pass operation");
        return std::nullopt;
      }
      return portable_concrete_term("undefined");
    }

    error =
      fail(index, "has unknown opcode `" + node.opcode + "`");
    return std::nullopt;
  }

public:
  PortableCallableBodyVerifier(
    PortableCallableBody& body,
    const PortableCallableSignature& signature,
    const PortableCallableCatalog& catalog,
    bool require_encoded_types
  ) :
      body_(body),
      signature_(signature),
      catalog_(catalog),
      require_encoded_types_(require_encoded_types) {
  }

  std::string
  verify() {
    if (body_.schema_version
          != kPortableCallableBodySchemaVersion
        || body_.format != kPortableCallableBodyFormat) {
      return "portable StyioIR has an unsupported schema or format";
    }
    if (body_.name != signature_.name) {
      return "portable StyioIR callable name does not match its contract";
    }
    if (body_.params.size() != signature_.params.size()) {
      return "portable StyioIR parameter count does not match its contract";
    }
    if (!portable_callable_terms_equal(
          body_.result,
          signature_.result)) {
      return "portable StyioIR result type does not match its contract";
    }
    if (body_.nodes.empty()
        || body_.nodes.size() > kMaximumPortableCallableNodes
        || body_.root >= body_.nodes.size()) {
      return "portable StyioIR has an invalid node table or root";
    }
    for (std::size_t i = 0; i < body_.params.size(); ++i) {
      const auto& param = body_.params[i];
      if (param.name.empty()
          || param.name.size()
               > kMaximumPortableCallableStringBytes
          || !portable_callable_terms_equal(
               param.type,
               signature_.params[i])) {
        return "portable StyioIR parameter does not match its contract";
      }
      if (!locals_.emplace(param.name, param.type).second) {
        return "portable StyioIR has a duplicate parameter `"
               + param.name + "`";
      }
    }

    inferred_types_.resize(body_.nodes.size());
    use_counts_.assign(body_.nodes.size(), 0);
    for (std::size_t i = 0; i < body_.nodes.size(); ++i) {
      auto& node = body_.nodes[i];
      if (node.opcode.size()
            > kMaximumPortableCallableStringBytes
          || node.symbol.size()
               > kMaximumPortableCallableStringBytes
          || node.value.size()
               > kMaximumPortableCallableStringBytes
          || node.operation.size()
               > kMaximumPortableCallableStringBytes) {
        return fail(i, "exceeds the supported string limit");
      }
      total_inputs_ += node.inputs.size();
      if (total_inputs_ > kMaximumPortableCallableInputs) {
        return "portable StyioIR exceeds the supported input limit";
      }
      for (const auto input : node.inputs) {
        if (input >= i) {
          return fail(
            i,
            "references a non-preceding node "
            + std::to_string(input));
        }
        ++use_counts_[input];
      }

      const PortableTerm encoded_type = node.type;
      const bool had_encoded_type = node.has_type;
      std::string error;
      auto inferred = infer_node(i, error);
      if (!inferred.has_value()) {
        return error.empty()
                 ? fail(i, "could not infer a type")
                 : error;
      }
      if (require_encoded_types_
          && (!had_encoded_type
              || !portable_callable_terms_equal(
                   encoded_type,
                   *inferred))) {
        return fail(i, "has a mismatched encoded type");
      }
      node.type = *inferred;
      node.has_type = true;
      inferred_types_[i] = std::move(*inferred);
    }

    for (std::size_t i = 0; i < use_counts_.size(); ++i) {
      const std::size_t expected = i == body_.root ? 0 : 1;
      if (use_counts_[i] != expected) {
        return "portable StyioIR must be a canonical rooted tree; node "
               + std::to_string(i) + " has "
               + std::to_string(use_counts_[i])
               + " parent references";
      }
    }
    if (!portable_callable_terms_equal(
          inferred_types_.at(body_.root),
          signature_.result)) {
      return "portable StyioIR root type does not match callable result";
    }
    return {};
  }
};

}  // namespace

std::string
verify_and_annotate_portable_callable_body(
  PortableCallableBody& body,
  const PortableCallableSignature& signature,
  const PortableCallableCatalog& catalog,
  bool require_encoded_types
) {
  PortableCallableBodyVerifier verifier(
    body,
    signature,
    catalog,
    require_encoded_types);
  return verifier.verify();
}

}  // namespace styio::ir
