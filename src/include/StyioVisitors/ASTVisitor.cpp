// [C++ STL]
#include <string>
#include <memory>
#include <vector>
#include <iostream>

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

llvm::Value* StyioToLLVM::visit_true(TrueAST* ast) {
  auto output = llvm::ConstantInt::getTrue(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_false(FalseAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_none(NoneAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_end(EndAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_empty(EmptyAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_empty_block(EmptyBlockAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_pass(PassAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_break(BreakAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_return(ReturnAST* ast) {
  auto zero = llvm_builder -> getInt32(0);

  llvm_builder -> CreateRet(zero);

  return zero;
}

llvm::Value* StyioToLLVM::visit_comment(CommentAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_id(IdAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_var(VarAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_fill_arg(FillArgAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  return output;
}

llvm::Value* StyioToLLVM::visit_arg(ArgAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_kwarg(KwArgAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_var_tuple(VarTupleAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  auto& vars = ast -> getArgs();
  for (auto const& s: vars) {
    s -> toLLVM(this); }

  return output;
}

llvm::Value* StyioToLLVM::visit_type(DTypeAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_typed_var(TypedVarAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_int(IntAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_float(FloatAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_char(CharAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_string(StringAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_fmt_str(FmtStrAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_ext_path(ExtPathAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_ext_link(ExtLinkAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_list(ListAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_tuple(TupleAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_set(SetAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_range(RangeAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_size_of(SizeOfAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_bin_op(BinOpAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_bin_comp(BinCompAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_cond(CondAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_call(CallAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_list_op(ListOpAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_resources(ResourceAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_flex_bind(FlexBindAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_final_bind(FinalBindAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_struct(StructAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_read_file(ReadFileAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_print(PrintAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_ext_pack(ExtPackAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_cases(CasesAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_cond_flow(CondFlowAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_check_equal(CheckEqAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_check_isin(CheckIsInAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_from_to(FromToAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_forward(ForwardAST* ast) {
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

llvm::Value* StyioToLLVM::visit_inf(InfiniteAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_func(FuncAST* ast) {
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

llvm::Value* StyioToLLVM::visit_loop(LoopAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value* StyioToLLVM::visit_iterator(IterAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}


llvm::Value* StyioToLLVM::visit_side_block(SideBlockAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  auto& stmts = ast -> getStmts();
  for (auto const& s: stmts) {
    s -> toLLVM(this); }
  
  return output;
}


llvm::Value* StyioToLLVM::visit_main(MainBlockAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  auto& stmts = ast -> getStmts();
  for (auto const& s: stmts) {
    s -> toLLVM(this); }

  return output;
}
