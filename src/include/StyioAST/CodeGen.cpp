// [C++ STL]
#include <string>

// [Styio]
#include "../StyioToken/Token.hpp"
#include "../StyioUtil/Util.hpp"
#include "AST.hpp"

// [LLVM]
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/LinkAllIR.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

llvm::Value* TrueAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::getTrue(*context);

  return output;
}

llvm::Value* FalseAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::getFalse(*context);

  return output;
}

llvm::Value* NoneAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::getFalse(*context);

  return output;
}

llvm::Value* EndAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}


llvm::Value* EmptyAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* EmptyBlockAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* BreakAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* PassAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* ReturnAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* CommentAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* IdAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* ArgAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* KwArgAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* VarsTupleAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* TypeAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* TypedVarAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* IntAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* FloatAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* CharAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* StringAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* FmtStrAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* ExtPathAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* ExtLinkAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* ListAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* TupleAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* SetAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* RangeAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* SizeOfAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* BinOpAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* BinCompAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* CondAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* CallAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* ListOpAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* ResourceAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* FlexBindAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* FinalBindAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* StructAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* ReadFileAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* PrintAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* ExtPackAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* BlockAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* CasesAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* CondFlowAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* CheckEqAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* CheckIsInAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* FromToAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* ForwardAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* InfiniteAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* FuncAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* LoopAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}

llvm::Value* IterAST::codeGen(
  std::unique_ptr<llvm::LLVMContext> context) {

  auto output = llvm::ConstantInt::get(*context, llvm::APInt(
    /*nbits*/ 8, 
    /*value: uint64_t*/ 1, 
    /*isSigned: bool*/ false));

  return output;
}