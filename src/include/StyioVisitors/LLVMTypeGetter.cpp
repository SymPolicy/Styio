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
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(NoneAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(EmptyAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(IdAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(DTypeAST* ast) {
  switch (ast->getDType()) {
    case StyioDataType::i32: {
      return theBuilder->getInt32Ty();
    } break;

    case StyioDataType::i64: {
      return theBuilder->getInt64Ty();
    } break;

    case StyioDataType::f64: {
      return theBuilder->getDoubleTy();
    } break;

    default: {
      return theBuilder->getInt32Ty();
    } break;
  }
}

llvm::Type*
StyioToLLVM::getLLVMType(BoolAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(IntAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(FloatAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(CharAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(StringAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(TypeConvertAST*) {
  return theBuilder->getInt32Ty();
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
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(OptKwArgAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(FlexBindAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(FinalBindAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(InfiniteAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(StructAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(TupleAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(VarTupleAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(RangeAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(SetAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(ListAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(SizeOfAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(ListOpAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(BinCompAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(CondAST* ast) {
  return theBuilder->getInt32Ty();
}

/*
  Int -> Int => Pass
  Int -> Float => Pass

*/
llvm::Type*
StyioToLLVM::getLLVMType(BinOpAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(FmtStrAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(ResourceAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(LocalPathAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(RemotePathAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(WebUrlAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(DBUrlAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(ExtPackAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(ReadFileAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(EOFAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(BreakAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(PassAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(ReturnAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(CallAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(PrintAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(ForwardAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(CheckEqAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(CheckIsinAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(FromToAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(CondFlowAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(AnonyFuncAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(FuncAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(IterAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(LoopAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(CasesAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(MatchCasesAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(BlockAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVM::getLLVMType(MainBlockAST* ast) {
  return theBuilder->getInt32Ty();
}
