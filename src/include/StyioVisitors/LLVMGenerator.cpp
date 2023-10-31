// [C++ STL]
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <optional>

// [Styio]
#include "../StyioToken/Token.hpp"
#include "../StyioAST/AST.hpp"

// [LLVM]
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Verifier.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/LinkAllIR.h"
#include "llvm/IR/DerivedTypes.h"

llvm::Type* StyioToLLVM::match_type(
  std::string type) {
    if (type == "i32") {
      return llvm_builder -> getInt32Ty(); }
    else if (type == "i64") {
      return llvm_builder -> getInt64Ty(); }
    else if (type == "f32") {
      return llvm_builder -> getFloatTy(); }
    else if (type == "f64") {
      return llvm_builder -> getDoubleTy(); }
    
    else if (type == "i1") {
      return llvm_builder -> getInt1Ty(); }
    else if (type == "i8") {
      return llvm_builder -> getInt8Ty(); }
    else if (type == "i16") {
      return llvm_builder -> getInt16Ty(); }
    else if (type == "i128") {
      return llvm_builder -> getInt128Ty(); }

  return llvm_builder -> getInt1Ty();
}

void StyioToLLVM::show() {
  llvm_module -> print(llvm::errs(), nullptr);
}

/*
  StyioNaive
  [:] BoolAST
*/

llvm::Value* StyioToLLVM::toLLVM(BoolAST* ast) {
  if ( ast -> getValue() ) {
    return llvm::ConstantInt::getTrue(*llvm_context);
  }
  else {
    return llvm::ConstantInt::getFalse(*llvm_context);
  }
}

llvm::Value* StyioToLLVM::toLLVM(NoneAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(EOFAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(EmptyAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(EmptyBlockAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(PassAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(BreakAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(ReturnAST* ast) {
  auto zero = llvm_builder -> getInt32(0);

  llvm_builder -> CreateRet(zero);

  return zero;
}

llvm::Value* StyioToLLVM::toLLVM(CommentAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(IdAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(VarAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(ArgAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(OptArgAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(OptKwArgAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(VarTupleAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  auto& vars = ast -> getArgs();
  for (auto const& s: vars) {
    s -> toLLVM(this); }

  return output;
}

llvm::Value* StyioToLLVM::toLLVM(DTypeAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(IntAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(FloatAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(CharAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(StringAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(FmtStrAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(ResourceAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(ExtPathAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(ExtLinkAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(ExtPackAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}


llvm::Value* StyioToLLVM::toLLVM(ListAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(TupleAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(SetAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(RangeAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(SizeOfAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(BinOpAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  llvm::Value* l_val = ast -> getLhs() -> toLLVM(this);
  llvm::Value* r_val = ast -> getRhs() -> toLLVM(this);

  switch (ast -> hint())
  {
  case StyioNodeHint::Bin_Add:
    llvm_builder -> CreateAdd(l_val, r_val, "add");

    break;

  case StyioNodeHint::Bin_Sub:
    llvm_builder -> CreateSub(l_val, r_val, "sub");

    break;

  case StyioNodeHint::Bin_Mul:
    llvm_builder -> CreateMul(l_val, r_val, "mul");

    break;

  case StyioNodeHint::Bin_Div:
    // llvm_builder -> CreateFDiv(l_val, r_val, "add");

    break;

  case StyioNodeHint::Bin_Pow:
    // llvm_builder -> CreateFAdd(l_val, r_val, "add");

    break;

  case StyioNodeHint::Bin_Mod:
    // llvm_builder -> CreateFAdd(l_val, r_val, "add");

    break;

  case StyioNodeHint::Inc_Add:
    /* code */
    break;

  case StyioNodeHint::Inc_Sub:
    /* code */
    break;

  case StyioNodeHint::Inc_Mul:
    /* code */
    break;

  case StyioNodeHint::Inc_Div:
    /* code */
    break;
  
  default:
    break;
  }

  return output;
}

llvm::Value* StyioToLLVM::toLLVM(BinCompAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(CondAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(CallAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(ListOpAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(FlexBindAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(FinalBindAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(StructAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(ReadFileAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(PrintAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}


llvm::Value* StyioToLLVM::toLLVM(ForwardAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  switch ( ast -> hint() )
  {
  case StyioNodeHint::Fill_Forward:
    {
      // ast -> getArgs() -> toLLVM(this);
      ast -> getThen() -> toLLVM(this);
    }
    break;
  
  default:
    break;
  }

  return output;
}

llvm::Value* StyioToLLVM::toLLVM(CheckEqAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(CheckIsInAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(FromToAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(InfiniteAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}


llvm::Value* StyioToLLVM::toLLVM(AnonyFuncAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(FuncAST* ast) {
  if ( ast -> hasName()
    && ast -> hasArgs()
    && ast -> hasRetType())
  {
    /* FuncAST -> Forward -> VarTuple -> [<VarAST>..] */
    auto& sf_args = ast -> getForward() -> getArgs() -> getArgs();

    std::vector<llvm::Type*> lf_args;
    for (auto& arg: sf_args) {
      lf_args.push_back(match_type(arg -> getTypeStr())); }

    llvm::FunctionType* lf_type = llvm::FunctionType::get(
      /* Result (Type) */ match_type(ast -> getRetTypeStr()),
      /* Params (Type) */ lf_args,
      /* isVarArg */ false);

    llvm::Function* lfunc = llvm::Function::Create(
      lf_type, 
      llvm::GlobalValue::ExternalLinkage,
      ast -> getName(),
      *llvm_module);

    for (size_t i = 0; i < lf_args.size(); i++) {
      lfunc -> getArg(i) -> setName(sf_args.at(i) -> getName()); }

    llvm::BasicBlock* block = llvm::BasicBlock::Create(
      *llvm_context,
      "entry",
      lfunc);
    
    llvm_builder -> SetInsertPoint(block);

    ast -> getForward() -> toLLVM(this);

    llvm::verifyFunction(*lfunc); 
  }

  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(IterAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(LoopAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(CondFlowAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(CasesAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(MatchCasesAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(SideBlockAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  auto& stmts = ast -> getStmts();
  for (auto const& s: stmts) {
    s -> toLLVM(this); }
  
  return output;
}

llvm::Value* StyioToLLVM::toLLVM(MainBlockAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  auto& stmts = ast -> getStmts();
  for (auto const& s: stmts) {
    s -> toLLVM(this); }

  return output;
}
