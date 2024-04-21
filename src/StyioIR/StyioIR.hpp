#pragma once
#ifndef STYIO_IR_BASE_H_
#define STYIO_IR_BASE_H_

// [C++ STL]
#include <string>

// [LLVM]
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

// [Styio]
#include "../StyioToString/ToStringVisitor.hpp"
#include "../StyioVisitors/CodeGenVisitor.hpp"
#include "IRDecl.hpp"

class StyioIRBase
{
public:
  virtual ~StyioIRBase() {}

  /* StyioAST to String */
  virtual std::string toString(StyioRepr* visitor, int indent = 0) = 0;

  /* Get LLVM Type */
  virtual llvm::Type* toLLVMType(StyioToLLVMIR* visitor) = 0;

  /* LLVM IR Generator */
  virtual llvm::Value* toLLVMIR(StyioToLLVMIR* visitor) = 0;
};

template <class Derived>
class StyioIR : public StyioIRBase
{
public:
  std::string toString(StyioRepr* visitor, int indent = 0) override {
    return visitor->toString(static_cast<Derived*>(this), indent);
  }

  llvm::Type* toLLVMType(StyioToLLVMIR* visitor) override {
    return visitor->toLLVMType(static_cast<Derived*>(this));
  }

  llvm::Value* toLLVMIR(StyioToLLVMIR* visitor) override {
    return visitor->toLLVMIR(static_cast<Derived*>(this));
  }
};

#endif // STYIO_IR_BASE_H_