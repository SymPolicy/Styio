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
#include "../StyioUtil/Util.hpp"
#include "CodeGenVisitor.hpp"

llvm::Type*
StyioToLLVM::toLLVMType(SGResId* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGType* node) {
  switch (node->data_type.option) {
    case StyioDataTypeOption::Bool:
      return theBuilder->getInt1Ty();
    case StyioDataTypeOption::Integer:
      return theBuilder->getInt64Ty();
    case StyioDataTypeOption::Float:
      return theBuilder->getDoubleTy();
    case StyioDataTypeOption::String:
      return llvm::PointerType::get(*theContext, 0);
    default:
      return theBuilder->getInt64Ty();
  }
};

llvm::Type*
StyioToLLVM::toLLVMType(SGConstBool* node) {
  return theBuilder->getInt1Ty();
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
  return llvm::PointerType::get(*theContext, 0);
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