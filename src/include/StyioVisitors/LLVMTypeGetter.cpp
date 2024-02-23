// [C++ STL]
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioToken/Token.hpp"
#include "Util.hpp"

llvm::Type*
StyioToLLVM::getLLVMType(CommentAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(NoneAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(EmptyAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(EmptyBlockAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(IdAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(DTypeAST* ast) {
  switch (ast->getDType()) {
    case StyioDataType::i32: {
      return llvm_ir_builder->getInt32Ty();
    } break;

    case StyioDataType::i64: {
      return llvm_ir_builder->getInt64Ty();
    } break;

    case StyioDataType::f64: {
      return llvm_ir_builder->getDoubleTy();
    } break;

    default: {
      return llvm_ir_builder->getInt32Ty();
    } break;
  }
}

llvm::Type*
StyioToLLVM::getLLVMType(BoolAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(IntAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(FloatAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(CharAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(StringAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(TypeConvertAST*) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(VarAST* ast) {
  return ast->getDType()->getLLVMType(this);
}

llvm::Type*
StyioToLLVM::getLLVMType(ArgAST* ast) {
  return ast->getDType()->getLLVMType(this);
}

llvm::Type*
StyioToLLVM::getLLVMType(OptArgAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(OptKwArgAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(FlexBindAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(FinalBindAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(InfiniteAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(StructAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(TupleAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(VarTupleAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(RangeAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(SetAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(ListAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(SizeOfAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(ListOpAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(BinCompAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(CondAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

/*
  Int -> Int => Pass
  Int -> Float => Pass

*/
llvm::Type*
StyioToLLVM::getLLVMType(BinOpAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(FmtStrAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(ResourceAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(LocalPathAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(RemotePathAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(WebUrlAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(DBUrlAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(ExtPackAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(ReadFileAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(EOFAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(BreakAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(PassAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(ReturnAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(CallAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(PrintAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(ForwardAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(CheckEqAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(CheckIsInAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(FromToAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(CondFlowAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(AnonyFuncAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(FuncAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(IterAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(LoopAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(CasesAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(MatchCasesAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(BlockAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(MainBlockAST* ast) {
  return llvm_ir_builder->getInt32Ty();
}
