#pragma once
#ifndef STYIO_IR_VERIFIER_H_
#define STYIO_IR_VERIFIER_H_

#include <cstddef>
#include <string>
#include <vector>

#include "StyioServices/DiagnosticContract.hpp"

class StyioIR;

namespace styio::ir {

struct StyioIRVerifierDiagnostic
{
  std::string phase = std::string(styio::services::diagnostics::kPhaseIrVerify);
  std::string code = std::string(styio::services::diagnostics::kIrVerifyContract);
  std::string message;
};

struct StyioIRVerifierOptions
{
  // Construction-phase fragments may defer only zero-depth loop-control
  // diagnostics; all other structural verification remains enabled.
  bool defer_unresolved_loop_control = false;

  // Mutating passes require tree ownership so deleting or replacing one edge
  // cannot leave another edge dangling. General verification keeps the
  // existing DAG-compatible behavior unless this boundary opts into the
  // stronger ownership check.
  bool require_unique_ownership = false;
};

struct StyioIRVerifierResult
{
  std::vector<StyioIRVerifierDiagnostic> diagnostics;
  // Privacy-safe structural work count used by compiler phase profiling.
  std::size_t nodes_visited = 0;
  // Candidate bits collected during the same verifier walk.  The lowering
  // pipeline consumes these to avoid a second full-tree applicability scan.
  bool has_dead_suffix_candidate = false;
  bool has_canonicalization_candidate = false;
  bool has_constant_folding_candidate = false;

  bool ok() const {
    return diagnostics.empty();
  }
};

StyioIRVerifierResult verify_styio_ir(
  const StyioIR* root,
  const StyioIRVerifierOptions& options = StyioIRVerifierOptions{});
void require_verified_styio_ir(
  const StyioIR* root,
  const StyioIRVerifierOptions& options = StyioIRVerifierOptions{});

}  // namespace styio::ir

#endif  // STYIO_IR_VERIFIER_H_
