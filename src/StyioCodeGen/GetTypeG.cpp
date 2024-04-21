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
#include "Util.hpp"

llvm::Type*
toLLVMType(SGResId* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGConstInt* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGConstFloat* node) {
  return theBuilder->getDoubleTy();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGConstString* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGFormatString* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGBinOp* node) {
  return node->data_type->toLLVMType(this);
};

llvm::Type*
StyioToLLVM::toLLVMType(SGFuncArg* node) {
  return node->arg_type->toLLVMType(this);
};

llvm::Type*
StyioToLLVM::toLLVMType(SGFunc* node) {
  return node->ret_type->toLLVMType(this);
};

llvm::Type*
StyioToLLVM::toLLVMType(SGCall* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGBlock* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGEntry* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGMainEntry* node) {
  return theBuilder->getInt64Ty();
};