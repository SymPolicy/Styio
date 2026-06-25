#pragma once
#ifndef STYIO_IR_OPTIMIZER_H_
#define STYIO_IR_OPTIMIZER_H_

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "../StyioIR/Verifier.hpp"
#include "../StyioIR/StyioIR.hpp"

namespace styio::lowering {

struct StyioIRPassPipelineOptions
{
  unsigned opt_level = 1;
  bool verify_before = true;
  bool verify_after_each_pass = true;
  bool collect_timing = true;
  bool collect_ir_dumps = false;
};

struct StyioIRPassRecord
{
  std::string name;
  uint64_t duration_ns = 0;
  bool verifier_before_ok = true;
  bool verifier_after_ok = true;
  std::string ir_before;
  std::string ir_after;
};

struct StyioIRPassPipelineResult
{
  StyioIR* root = nullptr;
  std::vector<StyioIRPassRecord> passes;
  std::vector<styio::ir::StyioIRVerifierDiagnostic> diagnostics;
  std::string initial_ir;
  std::string final_ir;

  bool ok() const {
    return diagnostics.empty();
  }
};

class StyioIRPassManager
{
public:
  enum class PassKind
  {
    Canonicalization,
    ConstantFolding,
  };

  void add_canonicalization_pass();
  void add_constant_folding_pass();

  StyioIRPassPipelineResult run(
    StyioIR* root,
    const StyioIRPassPipelineOptions& options = StyioIRPassPipelineOptions{}) const;

private:
  std::vector<PassKind> passes_;
};

StyioIRPassManager default_styio_ir_pass_manager(unsigned opt_level = 1);
StyioIRPassPipelineResult run_default_styio_ir_pass_pipeline(
  StyioIR* root,
  const StyioIRPassPipelineOptions& options = StyioIRPassPipelineOptions{});
StyioIR* require_default_styio_ir_pass_pipeline(
  StyioIR* root,
  const StyioIRPassPipelineOptions& options = StyioIRPassPipelineOptions{});

StyioIR*
optimize_styio_ir(StyioIR* root);

/// Run constant folding over the IR tree. Replaces constant expressions
/// with their evaluated results. Does not reorder side effects.
void run_constant_fold_pass(StyioIR* root);

/// Dead statement elimination — removes pure statements whose results
/// are never used. Conservative: only removes literal-valued stmts.
void run_dead_stmt_elim_pass(StyioIR* root);

}  // namespace styio::lowering

#endif  // STYIO_IR_OPTIMIZER_H_
