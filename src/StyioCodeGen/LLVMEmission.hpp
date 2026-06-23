#pragma once
#ifndef STYIO_LLVM_EMISSION_H_
#define STYIO_LLVM_EMISSION_H_

#include "CodeGenVisitor.hpp"
#include "../StyioIR/IRDecl.hpp"

namespace styio::codegen {

llvm::Value* emit_llvm_ir(StyioIR* ir, StyioToLLVM* generator);

}  // namespace styio::codegen

#endif  // STYIO_LLVM_EMISSION_H_
