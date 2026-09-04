#pragma once
#ifndef STYIO_SEMA_CONTEXT_H_
#define STYIO_SEMA_CONTEXT_H_

// [STL]
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using std::string;
using std::unordered_map;

// [Styio]
#include "../StyioAST/ASTDecl.hpp"
#include "../StyioIR/CallableEffectRow.hpp"
#include "../StyioIR/IRDecl.hpp"
#include "../StyioIR/PortableCallableBody.hpp"
#include "../StyioResourceTopology/ResourceTopology.hpp"
#include "../StyioSession/SymbolInterner.hpp"
#include "../StyioSession/TypeTable.hpp"
#include "../StyioToken/Token.hpp"
#include "CallableSpecializationGraph.hpp"
#include "CallableUsage.hpp"

struct SGPulsePlan;

// Generic Visitor
template <typename... Types>
class AnalyzerVisitor;

template <typename T>
class AnalyzerVisitor<T>
{
public:
  virtual void typeInfer(T* t) = 0;

  virtual StyioIR* toStyioIR(T* t) = 0;
};

template <typename T, typename... Types>
class AnalyzerVisitor<T, Types...> : public AnalyzerVisitor<Types...>
{
public:
  using AnalyzerVisitor<Types...>::typeInfer;
  using AnalyzerVisitor<Types...>::toStyioIR;

  virtual void typeInfer(T* t) = 0;

  virtual StyioIR* toStyioIR(T* t) = 0;
};

using StyioSemaLoweringVisitor = AnalyzerVisitor<
  class CommentAST,

  class NoneAST,
  class EmptyAST,

  class BoolAST,
  class IntAST,
  class FloatAST,
  class CharAST,

  class StringAST,
  class SetAST,
  class ListAST,
  class DictAST,

  class StructAST,
  class TupleAST,

  class NameAST,
  class TypeAST,
  class TypeTupleAST,

  class VarAST,
  class ParamAST,
  class OptArgAST,
  class OptKwArgAST,

  class FlexBindAST,
  class FinalBindAST,
  class ParallelAssignAST,

  class BinCompAST,
  class CondAST,
  class BinOpAST,

  class UndefinedLitAST,
  class WaveMergeAST,
  class WaveDispatchAST,
  class FallbackAST,
  class GuardSelectorAST,
  class EqProbeAST,

  class FileResourceAST,
  class StdStreamAST,
  class HandleAcquireAST,
  class ResourceWriteAST,
  class ResourceRedirectAST,
  class ResourceEffectAST,

  class StateDeclAST,
  class StateRefAST,
  class HistoryProbeAST,
  class SeriesIntrinsicAST,

  class AnonyFuncAST,
  class FunctionAST,
  class SimpleFuncAST,

  class FuncCallAST,
  class AttrAST,

  class SizeOfAST,
  class TypeConvertAST,
  class ListOpAST,
  class RangeAST,

  class IteratorAST,
  class StreamZipAST,
  class SnapshotDeclAST,
  class InstantPullAST,
  class TaskBlockAST,
  class TaskGroupLaunchAST,
  class FlowBindAST,
  class IterSeqAST,
  class InfiniteLoopAST,

  class CondFlowAST,

  class EOFAST,
  class PassAST,
  class BreakAST,
  class ContinueAST,
  class ReturnAST,

  class CasesAST,
  class MatchCasesAST,

  class BlockAST,
  class MainBlockAST,

  class ExtPackAST,
  class ExportDeclAST,
  class ExternBlockAST,

  class InfiniteAST,

  class VarTupleAST,
  class ExtractorAST,

  class ForwardAST,
  class BackwardAST,

  class CheckEqualAST,
  class CheckIsinAST,
  class HashTagNameAST,

  class CODPAST,

  class FmtStrAST,

  class ResourceAST,
  class EmptyResourceAST,
  class ResourceReceiverAST,
  class ResourceMethodDefAST,
  class ResourceOrderAST,
  class ResourceDeclAST,
  class ResourceRefAST,

  class ResPathAST,
  class RemotePathAST,
  class WebUrlAST,
  class DBUrlAST,

  class PrintAST,
  class ReadFileAST>;

class StyioSemaContext : public StyioSemaLoweringVisitor
{
public:
  // String-keyed maps (backward compatible)
  unordered_map<string, StyioAST*> func_defs;
  unordered_map<string, StyioDataType> local_binding_types;
  unordered_map<string, StyioDataType> resource_method_dynamic_local_binding_types;

  // SymbolId-keyed maps (fast path for interned identifiers)
  unordered_map<styio::session::SymbolId, StyioAST*> func_defs_by_sid;
  unordered_map<styio::session::SymbolId, StyioDataType> local_binding_types_by_sid;
  unordered_map<styio::session::SymbolId, StyioDataType> resource_method_dynamic_local_binding_types_by_sid;
  struct NativeFunctionType {
    StyioDataType return_type;
    std::vector<StyioDataType> arg_types;
  };
  unordered_map<string, NativeFunctionType> native_func_defs;
  unordered_map<styio::session::SymbolId, NativeFunctionType> native_func_defs_by_sid;

  SGPulsePlan* cur_pulse_plan() {
    return cur_pulse_plan_;
  }

  void set_cur_pulse_plan(SGPulsePlan* p) {
    cur_pulse_plan_ = p;
  }

  int active_series_slot() {
    return active_series_slot_;
  }

  void set_active_series_slot(int s) {
    active_series_slot_ = s;
  }

  void set_post_pulse_hist_context(int region_id, SGPulsePlan* plan) {
    post_pulse_hist_region_ = region_id;
    post_pulse_hist_plan_ = plan;
  }

  void record_snapshot_var_name(
    const std::string& name,
    styio::session::SymbolId sid
  ) {
    snapshot_var_names_.insert(name);
    if (sid == styio::session::kInvalidSymbolId) {
      sid = intern_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      snapshot_var_names_by_sid_.insert(sid);
    }
  }

  void record_consumed_task_name(
    const std::string& name,
    styio::session::SymbolId sid
  ) {
    consumed_task_names_.insert(name);
    if (sid == styio::session::kInvalidSymbolId) {
      sid = intern_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      consumed_task_names_by_sid_.insert(sid);
    }
  }

  bool is_consumed_task_name(
    styio::session::SymbolId sid,
    std::string_view name
  ) const {
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId
        && consumed_task_names_by_sid_.count(sid) != 0) {
      return true;
    }
    return consumed_task_names_.count(std::string(name)) != 0;
  }

  void clear_consumed_task_names() {
    consumed_task_names_.clear();
    consumed_task_names_by_sid_.clear();
  }

  void record_consumed_resource_name(
    const std::string& name,
    styio::session::SymbolId sid
  ) {
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      if (consumed_resource_names_by_sid_.insert(sid).second) {
        ++resource_typestate_dataflow_stats_.fact_insertion_count;
      }
      return;
    }
    if (consumed_resource_names_.insert(name).second) {
      ++resource_typestate_dataflow_stats_.fact_insertion_count;
    }
  }

  bool is_consumed_resource_name(
    styio::session::SymbolId sid,
    std::string_view name
  ) const {
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      return consumed_resource_names_by_sid_.count(sid) != 0;
    }
    return consumed_resource_names_.count(std::string(name)) != 0;
  }

  void erase_consumed_resource_name(
    const std::string& name,
    styio::session::SymbolId sid
  ) {
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      consumed_resource_names_by_sid_.erase(sid);
      return;
    }
    consumed_resource_names_.erase(name);
  }

  void clear_consumed_resource_names() {
    consumed_resource_names_.clear();
    consumed_resource_names_by_sid_.clear();
  }

  void record_owned_resource_name(
    const std::string& name,
    styio::session::SymbolId sid
  ) {
    owned_resource_names_.insert(name);
    if (sid == styio::session::kInvalidSymbolId) {
      sid = intern_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      owned_resource_names_by_sid_.insert(sid);
    }
  }

  bool is_task_outer_resource_name(
    styio::session::SymbolId sid,
    std::string_view name
  ) const {
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (!task_outer_resource_names_by_sid_stack_.empty()
        && sid != styio::session::kInvalidSymbolId
        && task_outer_resource_names_by_sid_stack_.back().count(sid) != 0) {
      return true;
    }
    return !task_outer_resource_names_stack_.empty()
           && task_outer_resource_names_stack_.back().count(std::string(name)) != 0;
  }

  void clear_owned_resource_names() {
    owned_resource_names_.clear();
    owned_resource_names_by_sid_.clear();
  }

  bool is_snapshot_var(const std::string& s) const {
    auto sid = lookup_semantic_symbol(s);
    if (sid != styio::session::kInvalidSymbolId
        && snapshot_var_names_by_sid_.count(sid) != 0) {
      return true;
    }
    return snapshot_var_names_.find(s) != snapshot_var_names_.end();
  }

  StyioSemaContext() :
      semantic_identity_scope_(styio::semantic_identity::Scope::anonymous()) {}

  explicit StyioSemaContext(styio::semantic_identity::Scope scope) :
      semantic_identity_scope_(std::move(scope)) {}

  virtual ~StyioSemaContext() {}

  const styio::semantic_identity::Scope& semantic_identity_scope() const noexcept {
    return semantic_identity_scope_;
  }

  void attach_type_table(
    styio::session::TypeTable& table,
    styio::session::SymbolInterner& symbols
  ) {
    type_table_ = &table;
    type_table_symbols_ = &symbols;
  }

  styio::session::TypeId maybe_intern_type(const StyioDataType& type) {
    if (type.isUndefined() || type_table_ == nullptr || type_table_symbols_ == nullptr) {
      return styio::session::kInvalidTypeId;
    }
    return type_table_->intern(type, *type_table_symbols_);
  }

  bool types_equal(const StyioDataType& lhs, const StyioDataType& rhs) {
    if (!lhs.isUndefined() && !rhs.isUndefined()
        && type_table_ != nullptr && type_table_symbols_ != nullptr) {
      const auto lhs_id = type_table_->intern(lhs, *type_table_symbols_);
      const auto rhs_id = type_table_->intern(rhs, *type_table_symbols_);
      if (lhs_id != styio::session::kInvalidTypeId
          && rhs_id != styio::session::kInvalidTypeId) {
        return type_table_->equals(lhs_id, rhs_id);
      }
    }
    return lhs.equals(rhs);
  }

  void push_active_function_body(const std::string& name);
  void push_active_function_body(const std::string& name, styio::session::SymbolId sid);
  void pop_active_function_body();
  void record_inferred_function_return_type(const StyioDataType& type);
  StyioDataType inferred_function_return_type(const std::string& name) const;
  StyioDataType inferred_function_return_type(
    styio::session::SymbolId sid,
    std::string_view name
  ) const;

  /* Styio AST Type Inference */

  void typeInfer(BoolAST* ast) override;
  void typeInfer(NoneAST* ast) override;
  void typeInfer(EOFAST* ast) override;
  void typeInfer(EmptyAST* ast) override;
  void typeInfer(PassAST* ast) override;
  void typeInfer(BreakAST* ast) override;
  void typeInfer(ContinueAST* ast) override;
  void typeInfer(ReturnAST* ast) override;
  void typeInfer(CommentAST* ast) override;
  void typeInfer(NameAST* ast) override;
  void typeInfer(VarAST* ast) override;
  void typeInfer(ParamAST* ast) override;
  void typeInfer(OptArgAST* ast) override;
  void typeInfer(OptKwArgAST* ast) override;
  void typeInfer(VarTupleAST* ast) override;
  void typeInfer(ExtractorAST* ast) override;
  void typeInfer(TypeAST* ast) override;
  void typeInfer(TypeTupleAST* ast) override;
  void typeInfer(IntAST* ast) override;
  void typeInfer(FloatAST* ast) override;
  void typeInfer(CharAST* ast) override;
  void typeInfer(StringAST* ast) override;
  void typeInfer(TypeConvertAST* ast) override;
  void typeInfer(FmtStrAST* ast) override;
  void typeInfer(ResPathAST* ast) override;
  void typeInfer(RemotePathAST* ast) override;
  void typeInfer(WebUrlAST* ast) override;
  void typeInfer(DBUrlAST* ast) override;
  void typeInfer(ListAST* ast) override;
  void typeInfer(DictAST* ast) override;
  void typeInfer(TupleAST* ast) override;
  void typeInfer(SetAST* ast) override;
  void typeInfer(RangeAST* ast) override;
  void typeInfer(SizeOfAST* ast) override;
  void typeInfer(BinOpAST* ast) override;
  void typeInfer(BinCompAST* ast) override;
  void typeInfer(CondAST* ast) override;
  void typeInfer(UndefinedLitAST* ast) override;
  void typeInfer(WaveMergeAST* ast) override;
  void typeInfer(WaveDispatchAST* ast) override;
  void typeInfer(FallbackAST* ast) override;
  void typeInfer(GuardSelectorAST* ast) override;
  void typeInfer(EqProbeAST* ast) override;
  void typeInfer(FileResourceAST* ast) override;
  void typeInfer(StdStreamAST* ast) override;
  void typeInfer(HandleAcquireAST* ast) override;
  void typeInfer(ResourceWriteAST* ast) override;
  void typeInfer(ResourceRedirectAST* ast) override;
  void typeInfer(ResourceEffectAST* ast) override;
  void typeInfer(StateDeclAST* ast) override;
  void typeInfer(StateRefAST* ast) override;
  void typeInfer(HistoryProbeAST* ast) override;
  void typeInfer(SeriesIntrinsicAST* ast) override;
  void typeInfer(FuncCallAST* ast) override;
  void typeInfer(AttrAST* ast) override;
  void typeInfer(ListOpAST* ast) override;
  void typeInfer(ResourceAST* ast) override;
  void typeInfer(EmptyResourceAST* ast) override;
  void typeInfer(ResourceReceiverAST* ast) override;
  void typeInfer(ResourceMethodDefAST* ast) override;
  void typeInfer(ResourceOrderAST* ast) override;
  void typeInfer(ResourceDeclAST* ast) override;
  void typeInfer(ResourceRefAST* ast) override;
  void typeInfer(FlexBindAST* ast) override;
  void typeInfer(FinalBindAST* ast) override;
  void typeInfer(ParallelAssignAST* ast) override;
  void typeInfer(StructAST* ast) override;
  void typeInfer(ReadFileAST* ast) override;
  void typeInfer(PrintAST* ast) override;
  void typeInfer(ExtPackAST* ast) override;
  void typeInfer(ExportDeclAST* ast) override;
  void typeInfer(ExternBlockAST* ast) override;
  void typeInfer(BlockAST* ast) override;
  void typeInfer(CasesAST* ast) override;
  void typeInfer(CondFlowAST* ast) override;
  void typeInfer(CheckEqualAST* ast) override;
  void typeInfer(CheckIsinAST* ast) override;
  void typeInfer(HashTagNameAST* ast) override;
  void typeInfer(ForwardAST* ast) override;
  void typeInfer(BackwardAST* ast) override;
  void typeInfer(CODPAST* ast) override;
  void typeInfer(InfiniteAST* ast) override;
  void typeInfer(AnonyFuncAST* ast) override;
  void typeInfer(FunctionAST* ast) override;
  void typeInfer(SimpleFuncAST* ast) override;
  void typeInfer(InfiniteLoopAST* ast) override;
  void typeInfer(IteratorAST* ast) override;
  void typeInfer(StreamZipAST* ast) override;
  void typeInfer(SnapshotDeclAST* ast) override;
  void typeInfer(InstantPullAST* ast) override;
  void typeInfer(TaskBlockAST* ast) override;
  void typeInfer(TaskGroupLaunchAST* ast) override;
  void typeInfer(FlowBindAST* ast) override;
  void typeInfer(IterSeqAST* ast) override;
  void typeInfer(MatchCasesAST* ast) override;
  void typeInfer(MainBlockAST* ast) override;


public:
  struct CallableConstraintSolverStats
  {
    std::size_t run_count = 0;
    std::size_t input_constraint_count = 0;
    std::size_t attempt_count = 0;
    std::size_t requeue_count = 0;
    std::size_t strict_binding_event_count = 0;
    std::size_t defaulted_variable_count = 0;
    std::size_t peak_frontier_count = 0;
    std::size_t peak_blocked_count = 0;
    std::size_t peak_live_waiter_count = 0;
    std::size_t peak_scheduler_storage_slots = 0;
  };

  struct CallableUsageRequirement
  {
    std::uint32_t variable = 0;
    styio::sema::CallableUsageSet usages;
  };

  struct CallableTypeScheme
  {
    std::string name;
    std::vector<styio::ir::PortableCallableTypeTerm> params;
    styio::ir::PortableCallableTypeTerm result;
    std::vector<styio::ir::PortableCallableTypeConstraint> constraints;
    std::vector<CallableUsageRequirement> usage_requirements;
    std::vector<std::uint32_t> quantified_variables;
    bool recursive_group = false;
    std::string canonical_relation;
  };

  struct CallableEffectRowFacts
  {
    styio::ir::CallableEffectRow row;
    bool relation_seed = false;
    std::vector<std::string> captures;
    std::vector<std::string> direct_callees;

    bool proven_pure() const {
      return row.proven_pure();
    }
  };

  enum class CallableCaptureMode : std::uint8_t {
    SharedBorrow = 0,
    ExclusiveBorrow,
    Consume,
  };

  struct CallableCaptureFact
  {
    std::string name;
    CallableCaptureMode mode = CallableCaptureMode::SharedBorrow;
  };

  struct ImportedCallableDefinition
  {
    std::string module_id;
    bool exported = false;
    bool has_scheme = false;
    StyioAST* definition = nullptr;
    CallableTypeScheme scheme;
    CallableEffectRowFacts effects;
    std::vector<StyioDataType> concrete_params;
    StyioDataType concrete_result{
      StyioDataTypeOption::Undefined, "undefined", 0
    };
    std::unordered_set<std::string> visible_from_modules;
    std::string portable_body_digest;
    std::string interface_abi_digest;
  };

  struct CallableSpecialization
  {
    std::string source_name;
    std::string owner_module;
    std::string lowered_name;
    std::vector<StyioDataType> param_types;
    StyioDataType result_type{
      StyioDataTypeOption::Undefined, "undefined", 0
    };
    std::string canonical_key;
    std::string content_digest;
    std::string portable_body_digest;
    std::string interface_abi_digest;
  };

  enum class BindingValueKind : std::uint8_t {
    Unknown = 0,
    Bool,
    I64,
    F64,
    String,
    ListHandle,
    DictHandle,
    MatrixHandle,
    TaskHandle,
  };

  struct BindingInfo
  {
    bool final_slot = false;
    bool dynamic_slot = false;
    bool resource_value = false;
    BindingValueKind value_kind = BindingValueKind::Unknown;
    StyioDataType declared_type{StyioDataTypeOption::Undefined, "undefined", 0};
  };

  struct ResourceMethodInfo
  {
    bool final_binding = false;
    bool consuming = false;
    bool property = false;
    StyioDataType result_type{StyioDataTypeOption::Undefined, "undefined", 0};
    std::size_t param_count = 0;
    std::vector<std::string> param_names;
    std::vector<StyioDataType> param_types;
  };

  struct ResourceTypestateDataflowStats
  {
    std::size_t branch_snapshot_count = 0;
    std::size_t join_count = 0;
    std::size_t fact_insertion_count = 0;
    std::size_t peak_temporary_fact_slots = 0;
  };

  struct ResourceTypestateFactSnapshot
  {
    std::unordered_set<std::string> names;
    std::unordered_set<styio::session::SymbolId> symbols;

    std::size_t size() const {
      return names.size() + symbols.size();
    }
  };

  const ResourceTypestateDataflowStats&
  resource_typestate_dataflow_stats() const {
    return resource_typestate_dataflow_stats_;
  }

  void reset_resource_typestate_dataflow_stats() {
    resource_typestate_dataflow_stats_ = {};
    resource_typestate_temporary_fact_slots_ = 0;
  }

  ResourceTypestateFactSnapshot snapshot_resource_typestate_facts() {
    ResourceTypestateFactSnapshot snapshot{
      consumed_resource_names_, consumed_resource_names_by_sid_};
    resource_typestate_temporary_fact_slots_ += snapshot.size();
    resource_typestate_dataflow_stats_.peak_temporary_fact_slots = std::max(
      resource_typestate_dataflow_stats_.peak_temporary_fact_slots,
      resource_typestate_temporary_fact_slots_);
    return snapshot;
  }

  void release_resource_typestate_snapshot(
    const ResourceTypestateFactSnapshot& snapshot
  ) {
    resource_typestate_temporary_fact_slots_ -= snapshot.size();
  }

  void install_resource_typestate_facts(
    const ResourceTypestateFactSnapshot& snapshot
  ) {
    consumed_resource_names_ = snapshot.names;
    consumed_resource_names_by_sid_ = snapshot.symbols;
  }

  styio::session::SymbolId intern_semantic_symbol(std::string_view spelling) {
    if (type_table_symbols_ == nullptr) {
      return styio::session::kInvalidSymbolId;
    }
    return type_table_symbols_->intern(spelling);
  }

  styio::session::SymbolId lookup_semantic_symbol(std::string_view spelling) const {
    if (type_table_symbols_ == nullptr) {
      return styio::session::kInvalidSymbolId;
    }
    return type_table_symbols_->lookup(spelling);
  }

  void record_function_def(
    const std::string& name,
    styio::session::SymbolId sid,
    StyioAST* def
  );

  StyioAST* find_function_def(
    styio::session::SymbolId sid,
    std::string_view name
  ) const {
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      auto sid_it = func_defs_by_sid.find(sid);
      if (sid_it != func_defs_by_sid.end()) {
        return sid_it->second;
      }
    }
    auto it = func_defs.find(std::string(name));
    if (it != func_defs.end()) {
      return it->second;
    }
    return nullptr;
  }

  void record_native_function_def(
    const std::string& name,
    styio::session::SymbolId sid,
    const NativeFunctionType& def
  ) {
    native_func_defs[name] = def;
    maybe_intern_type(def.return_type);
    for (const auto& arg_type : def.arg_types) {
      maybe_intern_type(arg_type);
    }
    if (sid == styio::session::kInvalidSymbolId) {
      sid = intern_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      native_func_defs_by_sid[sid] = def;
    }
  }

  const NativeFunctionType* find_native_function_def(
    styio::session::SymbolId sid,
    std::string_view name
  ) const {
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      auto sid_it = native_func_defs_by_sid.find(sid);
      if (sid_it != native_func_defs_by_sid.end()) {
        return &sid_it->second;
      }
    }
    auto it = native_func_defs.find(std::string(name));
    if (it != native_func_defs.end()) {
      return &it->second;
    }
    return nullptr;
  }

  void record_local_binding_type(
    const std::string& name,
    styio::session::SymbolId sid,
    const StyioDataType& type
  ) {
    local_binding_types[name] = type;
    maybe_intern_type(type);
    if (sid == styio::session::kInvalidSymbolId) {
      sid = intern_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      local_binding_types_by_sid[sid] = type;
    }
  }

  const StyioDataType* find_local_binding_type(
    styio::session::SymbolId sid,
    std::string_view name
  ) const {
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      auto sid_it = local_binding_types_by_sid.find(sid);
      if (sid_it != local_binding_types_by_sid.end()) {
        return &sid_it->second;
      }
    }
    auto it = local_binding_types.find(std::string(name));
    if (it != local_binding_types.end()) {
      return &it->second;
    }
    return nullptr;
  }

  void record_resource_method_dynamic_local_binding_type(
    const std::string& name,
    styio::session::SymbolId sid,
    const StyioDataType& type
  ) {
    resource_method_dynamic_local_binding_types[name] = type;
    maybe_intern_type(type);
    if (sid == styio::session::kInvalidSymbolId) {
      sid = intern_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      resource_method_dynamic_local_binding_types_by_sid[sid] = type;
    }
  }

  const StyioDataType* find_resource_method_dynamic_local_binding_type(
    styio::session::SymbolId sid,
    std::string_view name
  ) const {
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      auto sid_it = resource_method_dynamic_local_binding_types_by_sid.find(sid);
      if (sid_it != resource_method_dynamic_local_binding_types_by_sid.end()) {
        return &sid_it->second;
      }
    }
    auto it = resource_method_dynamic_local_binding_types.find(std::string(name));
    if (it != resource_method_dynamic_local_binding_types.end()) {
      return &it->second;
    }
    return nullptr;
  }

  void record_fixed_assignment_name(
    const std::string& name,
    styio::session::SymbolId sid
  ) {
    fixed_assignment_names_.insert(name);
    if (sid == styio::session::kInvalidSymbolId) {
      sid = intern_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      fixed_assignment_names_by_sid_.insert(sid);
    }
  }

  bool is_fixed_assignment_name(
    styio::session::SymbolId sid,
    std::string_view name
  ) const {
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId
        && fixed_assignment_names_by_sid_.count(sid) != 0) {
      return true;
    }
    return fixed_assignment_names_.count(std::string(name)) != 0;
  }

  void record_resource_binding_type(
    const std::string& name,
    styio::session::SymbolId sid,
    const StyioDataType& type
  ) {
    resource_binding_types_[name] = type;
    maybe_intern_type(type);
    if (sid == styio::session::kInvalidSymbolId) {
      sid = intern_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      resource_binding_types_by_sid_[sid] = type;
    }
  }

  const StyioDataType* find_resource_binding_type(
    styio::session::SymbolId sid,
    std::string_view name
  ) const {
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      auto sid_it = resource_binding_types_by_sid_.find(sid);
      if (sid_it != resource_binding_types_by_sid_.end()) {
        return &sid_it->second;
      }
    }
    auto it = resource_binding_types_.find(std::string(name));
    if (it != resource_binding_types_.end()) {
      return &it->second;
    }
    return nullptr;
  }

  void record_binding_info(
    const std::string& name,
    styio::session::SymbolId sid,
    const BindingInfo& info
  ) {
    binding_info_[name] = info;
    maybe_intern_type(info.declared_type);
    if (sid == styio::session::kInvalidSymbolId) {
      sid = intern_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      binding_info_by_sid_[sid] = info;
    }
  }

  const BindingInfo* find_binding_info(
    styio::session::SymbolId sid,
    std::string_view name
  ) const {
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      auto sid_it = binding_info_by_sid_.find(sid);
      if (sid_it != binding_info_by_sid_.end()) {
        return &sid_it->second;
      }
    }
    auto it = binding_info_.find(std::string(name));
    if (it != binding_info_.end()) {
      return &it->second;
    }
    return nullptr;
  }

  BindingInfo* find_mutable_binding_info(
    styio::session::SymbolId sid,
    std::string_view name
  ) {
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      auto sid_it = binding_info_by_sid_.find(sid);
      if (sid_it != binding_info_by_sid_.end()) {
        return &sid_it->second;
      }
    }
    auto it = binding_info_.find(std::string(name));
    if (it != binding_info_.end()) {
      return &it->second;
    }
    return nullptr;
  }

  const ResourceMethodInfo* find_resource_method(
    const std::string& family,
    const std::string& method) const {
    auto family_it = resource_method_defs_.find(family);
    if (family_it == resource_method_defs_.end()) {
      return nullptr;
    }
    auto method_it = family_it->second.find(method);
    if (method_it == family_it->second.end()) {
      return nullptr;
    }
    return &method_it->second;
  }

  void prepare_callable_type_schemes(MainBlockAST* ast);

  const CallableTypeScheme* find_callable_type_scheme(
    std::string_view name
  ) const;

  const CallableEffectRowFacts* find_callable_effect_row(
    std::string_view name
  ) const;

  const std::vector<CallableCaptureFact>* find_callable_capture_facts(
    std::string_view name
  ) const;

  const std::unordered_map<std::string, CallableTypeScheme>&
  callable_type_scheme_facts() const {
    return callable_type_schemes_;
  }

  const CallableConstraintSolverStats&
  callable_constraint_solver_stats() const {
    return callable_constraint_solver_stats_;
  }

  const std::unordered_map<std::string, CallableEffectRowFacts>&
  callable_effect_row_facts() const {
    return callable_effect_rows_;
  }

  void install_imported_callable_definition(
    std::string module_id,
    bool exported,
    bool has_scheme,
    StyioAST* definition,
    CallableTypeScheme scheme,
    CallableEffectRowFacts effects,
    std::vector<StyioDataType> concrete_params,
    StyioDataType concrete_result,
    std::vector<std::string> visible_from_modules,
    std::string portable_body_digest,
    std::string interface_abi_digest
  );

  const std::vector<ImportedCallableDefinition>&
  imported_callable_definitions() const {
    return imported_callable_definitions_;
  }

  const ImportedCallableDefinition* find_imported_callable_definition(
    std::string_view name
  ) const;

  bool imported_callable_is_visible(std::string_view name) const;

  void register_imported_callable_definitions();

  void configure_callable_specialization_environment(
    std::string backend_abi,
    std::string dependency_digest
  );

  void enforce_effect_monomorphic_instance(
    std::string_view name,
    const std::vector<StyioDataType>& arg_types
  );

  CallableSpecialization instantiate_callable_type_scheme(
    FuncCallAST* call,
    const std::vector<StyioDataType>& arg_types
  );

  const std::vector<CallableSpecialization>& callable_specializations(
    std::string_view name
  ) const;

  bool callable_has_runtime_specializations(std::string_view name) const;

  bool imported_concrete_callable_is_reachable(
    std::string_view name
  ) const;

  void prepare_imported_concrete_callable_body(
    std::string_view name
  );

  std::size_t callable_specialization_count() const {
    return callable_specialization_graph_.node_count();
  }

  void prepare_callable_specialization_body(
    StyioAST* def,
    const CallableSpecialization& specialization
  );

  void activate_callable_specialization(
    const CallableSpecialization& specialization
  ) {
    active_callable_specialization_ = specialization;
  }

  void clear_active_callable_specialization() {
    active_callable_specialization_.reset();
  }

  const CallableSpecialization* active_callable_specialization(
    std::string_view source_name
  ) const {
    if (!active_callable_specialization_.has_value()
        || active_callable_specialization_->source_name != source_name) {
      return nullptr;
    }
    return &*active_callable_specialization_;
  }

protected:
  enum class ResourceTopologyLifecycle : std::uint8_t
  {
    NotAnalyzed,
    ScalarNoop,
    Validated,
  };

  void reset_resource_topology_analysis() noexcept {
    resource_topology_artifact_.reset();
    resource_topology_root_ = nullptr;
    resource_topology_lifecycle_ = ResourceTopologyLifecycle::NotAnalyzed;
  }

  void publish_scalar_resource_topology_noop(const MainBlockAST* root) noexcept {
    resource_topology_artifact_.reset();
    resource_topology_root_ = root;
    resource_topology_lifecycle_ = ResourceTopologyLifecycle::ScalarNoop;
  }

  void publish_validated_resource_topology(
    const MainBlockAST* root,
    styio::resource_topology::ValidatedArtifact artifact
  ) {
    resource_topology_artifact_.emplace(std::move(artifact));
    resource_topology_root_ = root;
    resource_topology_lifecycle_ = ResourceTopologyLifecycle::Validated;
  }

  ResourceTopologyLifecycle resource_topology_lifecycle() const noexcept {
    return resource_topology_lifecycle_;
  }

  const styio::resource_topology::ValidatedArtifact*
  resource_topology_artifact_for(const MainBlockAST* root) const noexcept {
    if (resource_topology_lifecycle_ != ResourceTopologyLifecycle::Validated
        || resource_topology_root_ != root) {
      return nullptr;
    }
    return &*resource_topology_artifact_;
  }

  const styio::resource_topology::ValidatedArtifact*
  require_resource_topology_for_lowering(const MainBlockAST* root) const {
    if (resource_topology_root_ != root
        || resource_topology_lifecycle_ == ResourceTopologyLifecycle::NotAnalyzed) {
      throw std::logic_error(
        "lowering requires matching Sema resource topology state");
    }
    if (resource_topology_lifecycle_ == ResourceTopologyLifecycle::ScalarNoop) {
      return nullptr;
    }
    if (!resource_topology_artifact_.has_value()) {
      throw std::logic_error(
        "lowering requires a validated Sema resource topology artifact");
    }
    return &*resource_topology_artifact_;
  }

  virtual bool resource_topology_profile_enabled() const noexcept {
    return false;
  }

  virtual void record_resource_validation_duration(std::uint64_t) noexcept {}
  virtual void record_resource_validation_skipped() noexcept {}
  virtual void record_resource_fast_path_probe_duration(std::uint64_t) noexcept {}

  SGPulsePlan* cur_pulse_plan_ = nullptr;
  int active_series_slot_ = -1;
  int post_pulse_hist_region_ = -1;
  SGPulsePlan* post_pulse_hist_plan_ = nullptr;
  /* String-keyed maps (backward compatible) */
  std::unordered_set<std::string> snapshot_var_names_;
  /* Names bound by final assignment (x : T := …); may not be reassigned via flex (=). */
  std::unordered_set<std::string> fixed_assignment_names_;
  std::unordered_map<std::string, BindingInfo> binding_info_;
  std::unordered_map<std::string, std::unordered_map<std::string, ResourceMethodInfo>> resource_method_defs_;
  std::unordered_map<std::string, StyioDataType> resource_binding_types_;

  /* SymbolId-keyed maps (fast path for interned identifiers) */
  std::unordered_set<styio::session::SymbolId> snapshot_var_names_by_sid_;
  std::unordered_set<styio::session::SymbolId> fixed_assignment_names_by_sid_;
  std::unordered_map<styio::session::SymbolId, BindingInfo> binding_info_by_sid_;
  std::unordered_map<styio::session::SymbolId, StyioDataType> resource_binding_types_by_sid_;
  std::unordered_set<styio::session::SymbolId> consumed_task_names_by_sid_;
  std::unordered_set<styio::session::SymbolId> consumed_resource_names_by_sid_;
  std::unordered_set<styio::session::SymbolId> owned_resource_names_by_sid_;
  std::unordered_set<ResourceWriteAST*> collect_bind_resource_writes_;
  std::unordered_set<HandleAcquireAST*> collect_bind_handle_acquires_;
  std::unordered_map<ResourceWriteAST*, StyioDataType> collect_bind_resource_write_types_;
  std::unordered_map<HandleAcquireAST*, StyioDataType> collect_bind_handle_acquire_types_;
  std::unordered_set<std::string> consumed_task_names_;
  std::unordered_set<std::string> consumed_resource_names_;
  ResourceTypestateDataflowStats resource_typestate_dataflow_stats_;
  std::size_t resource_typestate_temporary_fact_slots_ = 0;
  std::unordered_set<std::string> owned_resource_names_;
  std::vector<std::unordered_set<std::string>> task_outer_resource_names_stack_;
  std::vector<std::unordered_set<styio::session::SymbolId>> task_outer_resource_names_by_sid_stack_;
  std::unordered_set<std::string> active_function_body_inference_;
  std::vector<std::string> active_function_body_stack_;
  std::unordered_map<std::string, StyioDataType> inferred_function_return_types_;
  std::unordered_set<styio::session::SymbolId> active_function_body_inference_by_sid_;
  std::vector<styio::session::SymbolId> active_function_body_sid_stack_;
  std::unordered_map<styio::session::SymbolId, StyioDataType> inferred_function_return_types_by_sid_;
  std::unordered_map<std::string, CallableTypeScheme> callable_type_schemes_;
  CallableConstraintSolverStats callable_constraint_solver_stats_;
  std::unordered_map<std::string, CallableEffectRowFacts> callable_effect_rows_;
  std::unordered_map<std::string, std::vector<CallableCaptureFact>>
    callable_capture_facts_;
  std::vector<ImportedCallableDefinition> imported_callable_definitions_;
  std::unordered_map<std::string, std::size_t>
    imported_callable_definition_indices_;
  std::unordered_map<std::string, std::vector<StyioDataType>>
    effect_monomorphic_instances_;
  std::unordered_map<std::string, std::vector<CallableSpecialization>> callable_specializations_;
  std::unordered_map<std::string, CallableSpecialization>
    callable_specialization_cache_;
  std::unordered_map<const StyioAST*, std::string>
    callable_semantic_body_digests_;
  std::unordered_map<std::string, std::string>
    callable_definition_dependency_digests_;
  std::unordered_set<std::string>
    reachable_imported_concrete_callables_;
  styio::sema::CallableSpecializationGraph
    callable_specialization_graph_;
  std::string callable_specialization_backend_abi_ =
    "styio.specialization.backend.unspecified.v1";
  std::string callable_specialization_dependency_digest_ =
    "styio.specialization.dependencies.none.v1";
  std::optional<CallableSpecialization> active_callable_specialization_;
  std::string active_resource_receiver_family_;
  styio::session::TypeTable* type_table_ = nullptr;
  styio::session::SymbolInterner* type_table_symbols_ = nullptr;
  std::optional<styio::resource_topology::ValidatedArtifact>
    resource_topology_artifact_;
  styio::semantic_identity::Scope semantic_identity_scope_;
  const MainBlockAST* resource_topology_root_ = nullptr;
  ResourceTopologyLifecycle resource_topology_lifecycle_ =
    ResourceTopologyLifecycle::NotAnalyzed;
};

#endif
