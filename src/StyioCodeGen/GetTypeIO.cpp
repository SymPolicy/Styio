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

llvm::Type*
StyioToLLVM::toLLVMType(SIOPath* node) {
  (void)node;
  return llvm::PointerType::get(*theContext, 0);
};

llvm::Type*
StyioToLLVM::toLLVMType(SIOPrint* node) {
  (void)node;
  return theBuilder->getVoidTy();
};

llvm::Type*
StyioToLLVM::toLLVMType(SIORead* node) {
  (void)node;
  return theBuilder->getVoidTy();
};
