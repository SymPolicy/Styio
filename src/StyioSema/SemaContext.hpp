#pragma once
#ifndef STYIO_SEMA_CONTEXT_H_
#define STYIO_SEMA_CONTEXT_H_

// [STL]
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using std::string;
using std::unordered_map;

// [Styio]
#include "../StyioAST/ASTDecl.hpp"
#include "../StyioIR/IRDecl.hpp"
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
  class TypedStdinListAST,
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

  class ResPathAST,
  class RemotePathAST,
  class WebUrlAST,
  class DBUrlAST,

  class PrintAST,
  class ReadFileAST>;

using StyioAnalyzerVisitor = StyioSemaLoweringVisitor;

class StyioSemaContext : public StyioSemaLoweringVisitor
{
public:
  unordered_map<string, StyioAST*> func_defs;
  unordered_map<string, StyioDataType> local_binding_types;
  struct NativeFunctionType {
    StyioDataType return_type;
    std::vector<StyioDataType> arg_types;
  };
  unordered_map<string, NativeFunctionType> native_func_defs;

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

  bool is_snapshot_var(const std::string& s) const {
    return snapshot_var_names_.find(s) != snapshot_var_names_.end();
  }

  StyioSemaContext() {}

  virtual ~StyioSemaContext() {}

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
  void typeInfer(StateDeclAST* ast) override;
  void typeInfer(StateRefAST* ast) override;
  void typeInfer(HistoryProbeAST* ast) override;
  void typeInfer(SeriesIntrinsicAST* ast) override;
  void typeInfer(FuncCallAST* ast) override;
  void typeInfer(AttrAST* ast) override;
  void typeInfer(ListOpAST* ast) override;
  void typeInfer(ResourceAST* ast) override;
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
  void typeInfer(TypedStdinListAST* ast) override;
  void typeInfer(IterSeqAST* ast) override;
  void typeInfer(MatchCasesAST* ast) override;
  void typeInfer(MainBlockAST* ast) override;


public:
  enum class BindingValueKind : std::uint8_t {
    Unknown = 0,
    Bool,
    I64,
    F64,
    String,
    ListHandle,
    DictHandle,
    MatrixHandle,
  };

  struct BindingInfo
  {
    bool final_slot = false;
    bool dynamic_slot = false;
    bool resource_value = false;
    BindingValueKind value_kind = BindingValueKind::Unknown;
    StyioDataType declared_type{StyioDataTypeOption::Undefined, "undefined", 0};
  };

protected:
  SGPulsePlan* cur_pulse_plan_ = nullptr;
  int active_series_slot_ = -1;
  int post_pulse_hist_region_ = -1;
  SGPulsePlan* post_pulse_hist_plan_ = nullptr;
  std::unordered_set<std::string> snapshot_var_names_;
  /* Names bound by final assignment (x : T := …); may not be reassigned via flex (=). */
  std::unordered_set<std::string> fixed_assignment_names_;
  std::unordered_map<std::string, BindingInfo> binding_info_;
  std::unordered_set<ResourceWriteAST*> collect_bind_resource_writes_;
  std::unordered_set<HandleAcquireAST*> collect_bind_handle_acquires_;
  std::unordered_map<ResourceWriteAST*, StyioDataType> collect_bind_resource_write_types_;
  std::unordered_map<HandleAcquireAST*, StyioDataType> collect_bind_handle_acquire_types_;
};

#endif
