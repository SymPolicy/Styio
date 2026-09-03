#pragma once
#ifndef STYIO_AST_TO_STYIO_IR_LOWERER_H_
#define STYIO_AST_TO_STYIO_IR_LOWERER_H_

#include <cstdint>

#include "../StyioSema/SemaContext.hpp"
#include "StyioIROptimizer.hpp"

class AstToStyioIRLowerer : public StyioSemaContext
{
public:
  AstToStyioIRLowerer() {}
  ~AstToStyioIRLowerer() override {}

  StyioIR* toStyioIR(CommentAST* ast) override;
  StyioIR* toStyioIR(NoneAST* ast) override;
  StyioIR* toStyioIR(EmptyAST* ast) override;
  StyioIR* toStyioIR(NameAST* ast) override;
  StyioIR* toStyioIR(TypeAST* ast) override;
  StyioIR* toStyioIR(TypeTupleAST* ast) override;
  StyioIR* toStyioIR(BoolAST* ast) override;
  StyioIR* toStyioIR(IntAST* ast) override;
  StyioIR* toStyioIR(FloatAST* ast) override;
  StyioIR* toStyioIR(CharAST* ast) override;
  StyioIR* toStyioIR(StringAST* ast) override;
  StyioIR* toStyioIR(TypeConvertAST* ast) override;
  StyioIR* toStyioIR(VarAST* ast) override;
  StyioIR* toStyioIR(ParamAST* ast) override;
  StyioIR* toStyioIR(OptArgAST* ast) override;
  StyioIR* toStyioIR(OptKwArgAST* ast) override;
  StyioIR* toStyioIR(FlexBindAST* ast) override;
  StyioIR* toStyioIR(FinalBindAST* ast) override;
  StyioIR* toStyioIR(ParallelAssignAST* ast) override;
  StyioIR* toStyioIR(InfiniteAST* ast) override;
  StyioIR* toStyioIR(StructAST* ast) override;
  StyioIR* toStyioIR(TupleAST* ast) override;
  StyioIR* toStyioIR(VarTupleAST* ast) override;
  StyioIR* toStyioIR(ExtractorAST* ast) override;
  StyioIR* toStyioIR(RangeAST* ast) override;
  StyioIR* toStyioIR(SetAST* ast) override;
  StyioIR* toStyioIR(ListAST* ast) override;
  StyioIR* toStyioIR(DictAST* ast) override;
  StyioIR* toStyioIR(SizeOfAST* ast) override;
  StyioIR* toStyioIR(ListOpAST* ast) override;
  StyioIR* toStyioIR(BinCompAST* ast) override;
  StyioIR* toStyioIR(CondAST* ast) override;
  StyioIR* toStyioIR(UndefinedLitAST* ast) override;
  StyioIR* toStyioIR(WaveMergeAST* ast) override;
  StyioIR* toStyioIR(WaveDispatchAST* ast) override;
  StyioIR* toStyioIR(FallbackAST* ast) override;
  StyioIR* toStyioIR(GuardSelectorAST* ast) override;
  StyioIR* toStyioIR(EqProbeAST* ast) override;
  StyioIR* toStyioIR(FileResourceAST* ast) override;
  StyioIR* toStyioIR(StdStreamAST* ast) override;
  StyioIR* toStyioIR(HandleAcquireAST* ast) override;
  StyioIR* toStyioIR(ResourceWriteAST* ast) override;
  StyioIR* toStyioIR(ResourceRedirectAST* ast) override;
  StyioIR* toStyioIR(ResourceEffectAST* ast) override;
  StyioIR* lowerResourceSinkWriteLatest(StyioAST* data, StyioAST* resource, bool redirect_mode);
  StyioIR* toStyioIR(BinOpAST* ast) override;
  StyioIR* toStyioIR(FmtStrAST* ast) override;
  StyioIR* toStyioIR(ResourceAST* ast) override;
  StyioIR* toStyioIR(EmptyResourceAST* ast) override;
  StyioIR* toStyioIR(ResourceReceiverAST* ast) override;
  StyioIR* toStyioIR(ResourceMethodDefAST* ast) override;
  StyioIR* toStyioIR(ResourceOrderAST* ast) override;
  StyioIR* toStyioIR(ResourceDeclAST* ast) override;
  StyioIR* toStyioIR(ResourceRefAST* ast) override;
  StyioIR* toStyioIR(ResPathAST* ast) override;
  StyioIR* toStyioIR(RemotePathAST* ast) override;
  StyioIR* toStyioIR(WebUrlAST* ast) override;
  StyioIR* toStyioIR(DBUrlAST* ast) override;
  StyioIR* toStyioIR(ExtPackAST* ast) override;
  StyioIR* toStyioIR(ExportDeclAST* ast) override;
  StyioIR* toStyioIR(ExternBlockAST* ast) override;
  StyioIR* toStyioIR(ReadFileAST* ast) override;
  StyioIR* toStyioIR(EOFAST* ast) override;
  StyioIR* toStyioIR(BreakAST* ast) override;
  StyioIR* toStyioIR(ContinueAST* ast) override;
  StyioIR* toStyioIR(PassAST* ast) override;
  StyioIR* toStyioIR(ReturnAST* ast) override;
  StyioIR* toStyioIR(FuncCallAST* ast) override;
  StyioIR* toStyioIR(AttrAST* ast) override;
  StyioIR* toStyioIR(PrintAST* ast) override;
  StyioIR* toStyioIR(ForwardAST* ast) override;
  StyioIR* toStyioIR(BackwardAST* ast) override;
  StyioIR* toStyioIR(CODPAST* ast) override;
  StyioIR* toStyioIR(CheckEqualAST* ast) override;
  StyioIR* toStyioIR(CheckIsinAST* ast) override;
  StyioIR* toStyioIR(HashTagNameAST* ast) override;
  StyioIR* toStyioIR(CondFlowAST* ast) override;
  StyioIR* toStyioIR(AnonyFuncAST* ast) override;
  StyioIR* toStyioIR(FunctionAST* ast) override;
  StyioIR* toStyioIR(SimpleFuncAST* ast) override;
  StyioIR* toStyioIR(IteratorAST* ast) override;
  StyioIR* toStyioIR(StreamZipAST* ast) override;
  StyioIR* toStyioIR(SnapshotDeclAST* ast) override;
  StyioIR* toStyioIR(InstantPullAST* ast) override;
  StyioIR* toStyioIR(TaskBlockAST* ast) override;
  StyioIR* toStyioIR(TaskGroupLaunchAST* ast) override;
  StyioIR* toStyioIR(FlowBindAST* ast) override;
  StyioIR* toStyioIR(IterSeqAST* ast) override;
  StyioIR* toStyioIR(InfiniteLoopAST* ast) override;
  StyioIR* toStyioIR(CasesAST* ast) override;
  StyioIR* toStyioIR(StateDeclAST* ast) override;
  StyioIR* toStyioIR(StateRefAST* ast) override;
  StyioIR* toStyioIR(HistoryProbeAST* ast) override;
  StyioIR* toStyioIR(SeriesIntrinsicAST* ast) override;
  StyioIR* toStyioIR(MatchCasesAST* ast) override;
  StyioIR* toStyioIR(BlockAST* ast) override;
  StyioIR* toStyioIR(MainBlockAST* ast) override;
  StyioAST* resolveResourceReceiverExprLatest(StyioAST* expr) const;

  void enable_pipeline_profile(bool enabled) noexcept {
    pipeline_profile_enabled_ = enabled;
  }

  bool pipeline_profile_enabled() const noexcept {
    return pipeline_profile_enabled_;
  }

  styio::lowering::StyioIRPassPipelineResult* pipeline_profile_sink() noexcept {
    return pipeline_profile_enabled_ ? &pipeline_profile_result_ : nullptr;
  }

  const styio::lowering::StyioIRPassPipelineResult& pipeline_profile_result() const noexcept {
    return pipeline_profile_result_;
  }

  std::uint64_t lowering_statement_duration_ns() const noexcept {
    return lowering_statement_duration_ns_;
  }

  std::uint64_t lowering_statement_count() const noexcept {
    return lowering_statement_count_;
  }

  std::uint64_t resource_validation_duration_ns() const noexcept {
    return resource_validation_duration_ns_;
  }

  void record_resource_validation_duration(std::uint64_t duration_ns) noexcept override {
    resource_validation_duration_ns_ += duration_ns;
  }

  std::uint64_t resource_validation_skipped() const noexcept {
    return resource_validation_skipped_;
  }

  void record_resource_validation_skipped() noexcept override {
    ++resource_validation_skipped_;
  }

  std::uint64_t resource_fast_path_probe_duration_ns() const noexcept {
    return resource_fast_path_probe_duration_ns_;
  }

  void record_resource_fast_path_probe_duration(std::uint64_t duration_ns) noexcept override {
    resource_fast_path_probe_duration_ns_ += duration_ns;
  }

  void record_lowering_statement_duration(std::uint64_t duration_ns) noexcept {
    lowering_statement_duration_ns_ += duration_ns;
    ++lowering_statement_count_;
  }

protected:
  bool resource_topology_profile_enabled() const noexcept override {
    return pipeline_profile_enabled_;
  }

private:
  std::unordered_set<std::string> active_function_parameter_names_;
  std::unordered_map<std::string, FileResourceAST*> file_resource_bindings_;
  std::unordered_map<std::string, std::unordered_map<std::string, ResourceMethodDefAST*>> resource_method_body_defs_;
  std::unordered_map<std::string, StyioAST*> resource_receiver_expr_bindings_;
  styio::lowering::StyioIRPassPipelineResult pipeline_profile_result_;
  bool pipeline_profile_enabled_ = false;
  std::uint64_t lowering_statement_duration_ns_ = 0;
  std::uint64_t lowering_statement_count_ = 0;
  std::uint64_t resource_validation_duration_ns_ = 0;
  std::uint64_t resource_validation_skipped_ = 0;
  std::uint64_t resource_fast_path_probe_duration_ns_ = 0;
};

#endif
