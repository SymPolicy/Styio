#pragma once
#ifndef STYIO_SEMA_CONTEXT_H_
#define STYIO_SEMA_CONTEXT_H_

// [STL]
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using std::string;
using std::unordered_map;

// [Styio]
#include "../StyioAST/ASTDecl.hpp"
#include "../StyioIR/IRDecl.hpp"
#include "../StyioSession/SymbolInterner.hpp"
#include "../StyioSession/TypeTable.hpp"
#include "../StyioToken/Token.hpp"

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
    consumed_resource_names_.insert(name);
    if (sid == styio::session::kInvalidSymbolId) {
      sid = intern_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      consumed_resource_names_by_sid_.insert(sid);
    }
  }

  bool is_consumed_resource_name(
    styio::session::SymbolId sid,
    std::string_view name
  ) const {
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId
        && consumed_resource_names_by_sid_.count(sid) != 0) {
      return true;
    }
    return consumed_resource_names_.count(std::string(name)) != 0;
  }

  void erase_consumed_resource_name(
    const std::string& name,
    styio::session::SymbolId sid
  ) {
    consumed_resource_names_.erase(name);
    if (sid == styio::session::kInvalidSymbolId) {
      sid = lookup_semantic_symbol(name);
    }
    if (sid != styio::session::kInvalidSymbolId) {
      consumed_resource_names_by_sid_.erase(sid);
    }
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

  StyioSemaContext() {}

  virtual ~StyioSemaContext() {}

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
  struct CallableTypeTerm
  {
    enum class Kind : std::uint8_t {
      Variable = 0,
      Concrete,
      List,
      Dict,
    };

    Kind kind = Kind::Concrete;
    std::uint32_t variable = 0;
    StyioDataType concrete{
      StyioDataTypeOption::Undefined, "undefined", 0
    };
    std::vector<CallableTypeTerm> arguments;
  };

  enum class CallableConstraintKind : std::uint8_t {
    Numeric = 0,
    Comparable,
    Indexable,
    Iterable,
    Cloneable,
  };

  struct CallableTypeConstraint
  {
    CallableConstraintKind kind = CallableConstraintKind::Numeric;
    CallableTypeTerm subject;
    CallableTypeTerm argument;
    CallableTypeTerm result;
    std::string canonical;
  };

  struct CallableTypeScheme
  {
    std::string name;
    std::vector<CallableTypeTerm> params;
    CallableTypeTerm result;
    std::vector<CallableTypeConstraint> constraints;
    std::vector<std::uint32_t> quantified_variables;
    bool recursive_group = false;
    std::string canonical_relation;
  };

  enum class CallableEffectKind : std::uint32_t {
    None = 0,
    Output = 1u << 0,
    Resource = 1u << 1,
    Task = 1u << 2,
    Handler = 1u << 3,
    Native = 1u << 4,
    Capture = 1u << 5,
    Unknown = 1u << 6,
  };

  struct CallableEffectSummary
  {
    std::uint32_t effect_bits = 0;
    bool closed = true;
    bool relation_seed = false;
    std::vector<std::string> captures;
    std::vector<std::string> direct_callees;
    std::string canonical = "pure";

    bool proven_pure() const {
      return closed && effect_bits == 0;
    }
  };

  struct CallableSpecialization
  {
    std::string source_name;
    std::string lowered_name;
    std::vector<StyioDataType> param_types;
    StyioDataType result_type{
      StyioDataTypeOption::Undefined, "undefined", 0
    };
    std::string canonical_key;
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

  const CallableEffectSummary* find_callable_effect_summary(
    std::string_view name
  ) const;

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
  std::unordered_map<std::string, CallableEffectSummary> callable_effect_summaries_;
  std::unordered_map<std::string, std::vector<StyioDataType>>
    effect_monomorphic_instances_;
  std::unordered_map<std::string, std::vector<CallableSpecialization>> callable_specializations_;
  std::unordered_set<std::string> active_callable_specialization_checks_;
  std::optional<CallableSpecialization> active_callable_specialization_;
  std::string active_resource_receiver_family_;
  styio::session::TypeTable* type_table_ = nullptr;
  styio::session::SymbolInterner* type_table_symbols_ = nullptr;
};

#endif
