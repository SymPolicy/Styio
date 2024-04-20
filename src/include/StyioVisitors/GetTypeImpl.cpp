// [C++ STL]
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "Util.hpp"

llvm::Type*
StyioToLLVMIR::toLLVMType(CommentAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(NoneAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(EmptyAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(NameAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(DTypeAST* ast) {
  switch (ast->getType()) {
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
    } break;
  }

  return nullptr;
}

llvm::Type*
StyioToLLVMIR::toLLVMType(BoolAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(IntAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(FloatAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(CharAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(StringAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(TypeConvertAST*) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(VarAST* ast) {
  if (ast->getDType() != nullptr) {
    /* VarAST -> DTypeAST -> llvm::Type */
    return ast->getDType()->toLLVMType(this);
  }

  return nullptr;
}

llvm::Type*
StyioToLLVMIR::toLLVMType(ArgAST* ast) {
  return ast->getDType()->toLLVMType(this);
}

llvm::Type*
StyioToLLVMIR::toLLVMType(OptArgAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(OptKwArgAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(FlexBindAST* ast) {
  if (ast->getDataType() == StyioDataType::undefined) {
    return ast->getValue()->toLLVMType(this);
  }

  return ast->getVar()->toLLVMType(this);
}

llvm::Type*
StyioToLLVMIR::toLLVMType(FinalBindAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(InfiniteAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(StructAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(TupleAST* ast) {
  return llvm::ArrayType::get(ast->getDTypeObj()->toLLVMType(this), ast->getElements().size());
}

llvm::Type*
StyioToLLVMIR::toLLVMType(VarTupleAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(RangeAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(SetAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(ListAST* ast) {
  if (ast->getDataType() == StyioDataType::undefined) {
    return nullptr;
  }

  // /* element type */
  // return llvm::VectorType::get(
  //   /* Element Type */ ast->getDTypeObj()->toLLVMType(this), 
  //   /* Element Size */ ast->getElements().size(),
  //   /* Scalable */ true);

  return llvm::ArrayType::get(ast->getDTypeObj()->toLLVMType(this), ast->getElements().size());
}

llvm::Type*
StyioToLLVMIR::toLLVMType(SizeOfAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(ListOpAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(BinCompAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(CondAST* ast) {
  return theBuilder->getInt32Ty();
}

/*
  Int -> Int => Pass
  Int -> Float => Pass

*/
llvm::Type*
StyioToLLVMIR::toLLVMType(BinOpAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(FmtStrAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(ResourceAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(LocalPathAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(RemotePathAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(WebUrlAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(DBUrlAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(ExtPackAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(ReadFileAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(EOFAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(BreakAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(PassAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(ReturnAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(CallAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(PrintAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(ForwardAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(CheckEqAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(CheckIsinAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(FromToAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(CondFlowAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(AnonyFuncAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(FuncAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(IterAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(LoopAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(CasesAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(MatchCasesAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(BlockAST* ast) {
  return theBuilder->getInt32Ty();
}

llvm::Type*
StyioToLLVMIR::toLLVMType(MainBlockAST* ast) {
  return theBuilder->getInt32Ty();
}
