// [C++ STL]
#include <string>
#include <memory>
#include <vector>

// [Styio]
#include "../StyioToken/Token.hpp"
#include "../StyioAST/AST.hpp"

// [LLVM]
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Constants.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/LinkAllIR.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

llvm::Value* StyioToLLVM::show() {
  llvm_mod -> print(llvm::errs(), nullptr);
};

llvm::Value* StyioToLLVM::gen_true(TrueAST* ast) {
  auto output = llvm::ConstantInt::getTrue(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_false(FalseAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_none(NoneAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_end(EndAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_empty(EmptyAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_empty_block(EmptyBlockAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_pass(PassAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_break(BreakAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_return(ReturnAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_comment(CommentAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_id(IdAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_arg(ArgAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_kwarg(KwArgAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_vars_tuple(VarsTupleAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_type(TypeAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_typed_var(TypedVarAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_int(IntAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_float(FloatAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_char(CharAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_string(StringAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_fmt_str(FmtStrAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_ext_path(ExtPathAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_ext_link(ExtLinkAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_list(ListAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_tuple(TupleAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_set(SetAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_range(RangeAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_size_of(SizeOfAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_bin_op(BinOpAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_bin_comp(BinCompAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_cond(CondAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_call(CallAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_list_op(ListOpAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_resources(ResourceAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_flex_bind(FlexBindAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_final_bind(FinalBindAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_struct(StructAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_read_file(ReadFileAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_print(PrintAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_ext_pack(ExtPackAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_cases(CasesAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_cond_flow(CondFlowAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_check_equal(CheckEqAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_check_isin(CheckIsInAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_from_to(FromToAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_forward(ForwardAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_infinite(InfiniteAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_function(FuncAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_loop(LoopAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}

llvm::Value* StyioToLLVM::gen_iterator(IterAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}


llvm::Value* StyioToLLVM::gen_block(SideBlockAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}


llvm::Value* StyioToLLVM::gen_main_block(MainBlockAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_ctx);
  return output;
}
