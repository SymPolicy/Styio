// [C++ STL]
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// [Styio]
#include "../StyioException/Exception.hpp"
#include "../StyioIR/GenIR/GenIR.hpp"
#include "../StyioToken/Token.hpp"
#include "CodeGenVisitor.hpp"
#include "../StyioUtil/Util.hpp"

// [LLVM]
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

// MIGRATION-NEEDED: M-CODEGEN-01 (docs/rollups/MIGRATION-LEDGER.md)
// All three implementations below return i64 as a stand-in for a real IO
// type system. Replace with proper handle/string/byte-stream types when
// the IO type contract is finalized.
llvm::Type*
StyioToLLVM::toLLVMType(SIOPath* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SIOPrint* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SIORead* node) {
  return theBuilder->getInt64Ty();
};