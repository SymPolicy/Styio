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
StyioToLLVMIR::getLLVMType(CommentAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(NoneAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(EmptyAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(NameAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(DTypeAST* ast) {
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
StyioToLLVMIR::getLLVMType(BoolAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(IntAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(FloatAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(CharAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(StringAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(TypeConvertAST*) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(VarAST* ast) {
  return ast->getDType()->getLLVMType(this);
}

llvm::Type*
StyioToLLVMIR::getLLVMType(ArgAST* ast) {
  return ast->getDType()->getLLVMType(this);
}

llvm::Type*
StyioToLLVMIR::getLLVMType(OptArgAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(OptKwArgAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(FlexBindAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(FinalBindAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(InfiniteAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(StructAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(TupleAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(VarTupleAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(RangeAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(SetAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(ListAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(SizeOfAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(ListOpAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(BinCompAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(CondAST* ast) {
  return theBuilder->getInt32Ty();
}

/*
  Int -> Int => Pass
  Int -> Float => Pass

*/
llvm::Type*
StyioToLLVMIR::getLLVMType(BinOpAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(FmtStrAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(ResourceAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(LocalPathAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(RemotePathAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(WebUrlAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(DBUrlAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(ExtPackAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(ReadFileAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(EOFAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(BreakAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(PassAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(ReturnAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(CallAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(PrintAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(ForwardAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(CheckEqAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(CheckIsinAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(FromToAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(CondFlowAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(AnonyFuncAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(FuncAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(IterAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(LoopAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(CasesAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(MatchCasesAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(BlockAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::getLLVMType(MainBlockAST* ast) {
  return theBuilder->getInt32Ty();
}
