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

llvm::Value* StyioVisitor::visit_true(TrueAST* ast) {
  auto output = llvm::ConstantInt::getTrue(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_false(FalseAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_none(NoneAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_end(EndAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_empty(EmptyAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_empty_block(EmptyBlockAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_pass(PassAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_break(BreakAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_return(ReturnAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_comment(CommentAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_id(IdAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_arg(ArgAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_kwarg(KwArgAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_vars_tuple(VarsTupleAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_type(TypeAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_typed_var(TypedVarAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_int(IntAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_float(FloatAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_char(CharAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_string(StringAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_fmt_str(FmtStrAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_ext_path(ExtPathAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_ext_link(ExtLinkAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_list(ListAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_tuple(TupleAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_set(SetAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_range(RangeAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_size_of(SizeOfAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_bin_op(BinOpAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_bin_comp(BinCompAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_cond(CondAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_call(CallAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_list_op(ListOpAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_resources(ResourceAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_flex_bind(FlexBindAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_final_bind(FinalBindAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_struct(StructAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_read_file(ReadFileAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_print(PrintAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_ext_pack(ExtPackAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_block(BlockAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_cases(CasesAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_cond_flow(CondFlowAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_check_equal(CheckEqAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_check_isin(CheckIsInAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_from_to(FromToAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_forward(ForwardAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_infinite(InfiniteAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_function(FuncAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_loop(LoopAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}

llvm::Value* StyioVisitor::visit_iterator(IterAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*context);
  return output;
}
