#pragma once
#ifndef STYIO_IR_VERIFIER_H_
#define STYIO_IR_VERIFIER_H_

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

struct StyioIRVerifierResult
{
  std::vector<StyioIRVerifierDiagnostic> diagnostics;

  bool ok() const {
    return diagnostics.empty();
  }
};

StyioIRVerifierResult verify_styio_ir(const StyioIR* root);
void require_verified_styio_ir(const StyioIR* root);

}  // namespace styio::ir

#endif  // STYIO_IR_VERIFIER_H_
