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

llvm::Type*
StyioToLLVM::toLLVMType(SGResId* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGType* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGConstBool* node) {
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
StyioToLLVM::toLLVMType(SGConstChar* node) {
  return theBuilder->getInt64Ty();
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
StyioToLLVM::toLLVMType(SGStruct* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGCast* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGBinOp* node) {
  return node->data_type->toLLVMType(this);
};

llvm::Type*
StyioToLLVM::toLLVMType(SGCond* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGVar* node) {
  return node->var_type->toLLVMType(this);
};

llvm::Type*
StyioToLLVM::toLLVMType(SGFlexBind* node) {
  return node->var->toLLVMType(this);
};

llvm::Type*
StyioToLLVM::toLLVMType(SGFinalBind* node) {
  return node->var->toLLVMType(this);
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
StyioToLLVM::toLLVMType(SGReturn* node) {
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