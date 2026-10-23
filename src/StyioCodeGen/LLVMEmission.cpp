#include "LLVMEmission.hpp"

#include "../StyioException/Exception.hpp"
#include "../StyioIR/StyioIR.hpp"

namespace styio::codegen {

llvm::Value*
emit_llvm_ir(StyioIR* ir, StyioToLLVM* generator) {
  if (ir == nullptr) {
    throw StyioTypeError("LLVM emission requires a non-null StyioIR root");
  }
  if (generator == nullptr) {
    throw StyioTypeError("LLVM emission requires a non-null generator");
  }
  return ir->toLLVMIR(generator);
}

}  // namespace styio::codegen
