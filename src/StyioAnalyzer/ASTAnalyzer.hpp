#pragma once
#ifndef STYIO_AST_ANALYZER_VISITOR_H_
#define STYIO_AST_ANALYZER_VISITOR_H_

// [STL]
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

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

using StyioAnalyzerVisitor = AnalyzerVisitor<
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

class StyioAnalyzer : public StyioAnalyzerVisitor
{
public:
  unordered_map<string, StyioAST*> func_defs;
  unordered_map<string, StyioDataType> local_binding_types;

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

  StyioAnalyzer() {}

  ~StyioAnalyzer() {}

  /* Styio AST Type Inference */

  void typeInfer(BoolAST* ast);
  void typeInfer(NoneAST* ast);
  void typeInfer(EOFAST* ast);
  void typeInfer(EmptyAST* ast);
  void typeInfer(PassAST* ast);
  void typeInfer(BreakAST* ast);
  void typeInfer(ContinueAST* ast);
  void typeInfer(ReturnAST* ast);
  void typeInfer(CommentAST* ast);
  void typeInfer(NameAST* ast);
  void typeInfer(VarAST* ast);
  void typeInfer(ParamAST* ast);
  void typeInfer(OptArgAST* ast);
  void typeInfer(OptKwArgAST* ast);
  void typeInfer(VarTupleAST* ast);
  void typeInfer(ExtractorAST* ast);
  void typeInfer(TypeAST* ast);
  void typeInfer(TypeTupleAST* ast);
  void typeInfer(IntAST* ast);
  void typeInfer(FloatAST* ast);
  void typeInfer(CharAST* ast);
  void typeInfer(StringAST* ast);
  void typeInfer(TypeConvertAST* ast);
  void typeInfer(FmtStrAST* ast);
  void typeInfer(ResPathAST* ast);
  void typeInfer(RemotePathAST* ast);
  void typeInfer(WebUrlAST* ast);
  void typeInfer(DBUrlAST* ast);
  void typeInfer(ListAST* ast);
  void typeInfer(TupleAST* ast);
  void typeInfer(SetAST* ast);
  void typeInfer(RangeAST* ast);
  void typeInfer(SizeOfAST* ast);
  void typeInfer(BinOpAST* ast);
  void typeInfer(BinCompAST* ast);
  void typeInfer(CondAST* ast);
  void typeInfer(UndefinedLitAST* ast);
  void typeInfer(WaveMergeAST* ast);
  void typeInfer(WaveDispatchAST* ast);
  void typeInfer(FallbackAST* ast);
  void typeInfer(GuardSelectorAST* ast);
  void typeInfer(EqProbeAST* ast);
  void typeInfer(FileResourceAST* ast);
  void typeInfer(HandleAcquireAST* ast);
  void typeInfer(ResourceWriteAST* ast);
  void typeInfer(ResourceRedirectAST* ast);
  void typeInfer(StateDeclAST* ast);
  void typeInfer(StateRefAST* ast);
  void typeInfer(HistoryProbeAST* ast);
  void typeInfer(SeriesIntrinsicAST* ast);
  void typeInfer(FuncCallAST* ast);
  void typeInfer(AttrAST* ast);
  void typeInfer(ListOpAST* ast);
  void typeInfer(ResourceAST* ast);
  void typeInfer(FlexBindAST* ast);
  void typeInfer(FinalBindAST* ast);
  void typeInfer(StructAST* ast);
  void typeInfer(ReadFileAST* ast);
  void typeInfer(PrintAST* ast);
  void typeInfer(ExtPackAST* ast);
  void typeInfer(BlockAST* ast);
  void typeInfer(CasesAST* ast);
  void typeInfer(CondFlowAST* ast);
  void typeInfer(CheckEqualAST* ast);
  void typeInfer(CheckIsinAST* ast);
  void typeInfer(HashTagNameAST* ast);
  void typeInfer(ForwardAST* ast);
  void typeInfer(BackwardAST* ast);
  void typeInfer(CODPAST* ast);
  void typeInfer(InfiniteAST* ast);
  void typeInfer(AnonyFuncAST* ast);
  void typeInfer(FunctionAST* ast);
  void typeInfer(SimpleFuncAST* ast);
  void typeInfer(InfiniteLoopAST* ast);
  void typeInfer(IteratorAST* ast);
  void typeInfer(StreamZipAST* ast);
  void typeInfer(SnapshotDeclAST* ast);
  void typeInfer(InstantPullAST* ast);
  void typeInfer(IterSeqAST* ast);
  void typeInfer(MatchCasesAST* ast);
  void typeInfer(MainBlockAST* ast);

  StyioIR* toStyioIR(NameAST* ast);
  StyioIR* toStyioIR(BoolAST* ast);
  StyioIR* toStyioIR(NoneAST* ast);
  StyioIR* toStyioIR(EOFAST* ast);
  StyioIR* toStyioIR(EmptyAST* ast);
  StyioIR* toStyioIR(PassAST* ast);
  StyioIR* toStyioIR(BreakAST* ast);
  StyioIR* toStyioIR(ContinueAST* ast);
  StyioIR* toStyioIR(ReturnAST* ast);
  StyioIR* toStyioIR(CommentAST* ast);
  
  StyioIR* toStyioIR(VarAST* ast);
  StyioIR* toStyioIR(ParamAST* ast);
  StyioIR* toStyioIR(OptArgAST* ast);
  StyioIR* toStyioIR(OptKwArgAST* ast);
  StyioIR* toStyioIR(VarTupleAST* ast);
  StyioIR* toStyioIR(ExtractorAST* ast);
  StyioIR* toStyioIR(TypeAST* ast);
  StyioIR* toStyioIR(TypeTupleAST* ast);
  StyioIR* toStyioIR(IntAST* ast);
  StyioIR* toStyioIR(FloatAST* ast);
  StyioIR* toStyioIR(CharAST* ast);
  StyioIR* toStyioIR(StringAST* ast);
  StyioIR* toStyioIR(TypeConvertAST* ast);
  StyioIR* toStyioIR(FmtStrAST* ast);
  StyioIR* toStyioIR(ResPathAST* ast);
  StyioIR* toStyioIR(RemotePathAST* ast);
  StyioIR* toStyioIR(WebUrlAST* ast);
  StyioIR* toStyioIR(DBUrlAST* ast);
  StyioIR* toStyioIR(ListAST* ast);
  StyioIR* toStyioIR(TupleAST* ast);
  StyioIR* toStyioIR(SetAST* ast);
  StyioIR* toStyioIR(RangeAST* ast);
  StyioIR* toStyioIR(SizeOfAST* ast);
  StyioIR* toStyioIR(BinOpAST* ast);
  StyioIR* toStyioIR(BinCompAST* ast);
  StyioIR* toStyioIR(CondAST* ast);
  StyioIR* toStyioIR(UndefinedLitAST* ast);
  StyioIR* toStyioIR(WaveMergeAST* ast);
  StyioIR* toStyioIR(WaveDispatchAST* ast);
  StyioIR* toStyioIR(FallbackAST* ast);
  StyioIR* toStyioIR(GuardSelectorAST* ast);
  StyioIR* toStyioIR(EqProbeAST* ast);
  StyioIR* toStyioIR(FileResourceAST* ast);
  StyioIR* toStyioIR(HandleAcquireAST* ast);
  StyioIR* toStyioIR(ResourceWriteAST* ast);
  StyioIR* toStyioIR(ResourceRedirectAST* ast);
  StyioIR* toStyioIR(StateDeclAST* ast);
  StyioIR* toStyioIR(StateRefAST* ast);
  StyioIR* toStyioIR(HistoryProbeAST* ast);
  StyioIR* toStyioIR(SeriesIntrinsicAST* ast);
  StyioIR* toStyioIR(FuncCallAST* ast);
  StyioIR* toStyioIR(AttrAST* ast);
  StyioIR* toStyioIR(ListOpAST* ast);
  StyioIR* toStyioIR(ResourceAST* ast);
  StyioIR* toStyioIR(FlexBindAST* ast);
  StyioIR* toStyioIR(FinalBindAST* ast);
  StyioIR* toStyioIR(StructAST* ast);
  StyioIR* toStyioIR(ReadFileAST* ast);
  StyioIR* toStyioIR(PrintAST* ast);
  StyioIR* toStyioIR(ExtPackAST* ast);
  StyioIR* toStyioIR(BlockAST* ast);
  StyioIR* toStyioIR(CasesAST* ast);
  StyioIR* toStyioIR(CondFlowAST* ast);
  StyioIR* toStyioIR(CheckEqualAST* ast);
  StyioIR* toStyioIR(CheckIsinAST* ast);
  StyioIR* toStyioIR(HashTagNameAST* ast);
  StyioIR* toStyioIR(ForwardAST* ast);
  StyioIR* toStyioIR(BackwardAST* ast);
  StyioIR* toStyioIR(CODPAST* ast);
  StyioIR* toStyioIR(InfiniteAST* ast);
  StyioIR* toStyioIR(AnonyFuncAST* ast);
  StyioIR* toStyioIR(FunctionAST* ast);
  StyioIR* toStyioIR(SimpleFuncAST* ast);
  StyioIR* toStyioIR(InfiniteLoopAST* ast);
  StyioIR* toStyioIR(IteratorAST* ast);
  StyioIR* toStyioIR(StreamZipAST* ast);
  StyioIR* toStyioIR(SnapshotDeclAST* ast);
  StyioIR* toStyioIR(InstantPullAST* ast);
  StyioIR* toStyioIR(IterSeqAST* ast);
  StyioIR* toStyioIR(MatchCasesAST* ast);
  StyioIR* toStyioIR(MainBlockAST* ast);

private:
  SGPulsePlan* cur_pulse_plan_ = nullptr;
  int active_series_slot_ = -1;
  int post_pulse_hist_region_ = -1;
  SGPulsePlan* post_pulse_hist_plan_ = nullptr;
  std::unordered_set<std::string> snapshot_var_names_;
  /* Names bound by final assignment (x : T := …); may not be reassigned via flex (=). */
  std::unordered_set<std::string> fixed_assignment_names_;
};

#endif