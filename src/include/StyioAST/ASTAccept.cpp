// [C++ STL]
#include <string>
#include <memory>
#include <vector>

// [Styio]
#include "../StyioUtil/Util.hpp"
#include "../StyioToken/Token.hpp"
#include "AST.hpp"

llvm::Value* TrueAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_true(this);
}

llvm::Value* FalseAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_false(this);
}

llvm::Value* NoneAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_none(this);
}

llvm::Value* EndAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_end(this);
}

llvm::Value* EmptyAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_empty(this);
}

llvm::Value* EmptyBlockAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_empty_block(this);
}

llvm::Value* PassAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_pass(this);
}

llvm::Value* BreakAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_break(this);
}

llvm::Value* ReturnAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_return(this);
}

llvm::Value* CommentAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_comment(this);
}

llvm::Value* IdAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_id(this);
}

llvm::Value* VarAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_var(this);
}

llvm::Value* ArgAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_fill_arg(this);
}

llvm::Value* OptArgAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_arg(this);
}

llvm::Value* OptKwArgAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_kwarg(this);
}

llvm::Value* VarTupleAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_var_tuple(this);
}

llvm::Value* DTypeAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_type(this);
}

llvm::Value* TypedVarAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_typed_var(this);
}

llvm::Value* IntAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_int(this);
}

llvm::Value* FloatAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_float(this);
}

llvm::Value* CharAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_char(this);
}

llvm::Value* StringAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_string(this);
}

llvm::Value* FmtStrAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_fmt_str(this);
}

llvm::Value* ExtPathAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_ext_path(this);
}

llvm::Value* ExtLinkAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_ext_link(this);
}

llvm::Value* ListAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_list(this);
}

llvm::Value* TupleAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_tuple(this);
}

llvm::Value* SetAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_set(this);
}

llvm::Value* RangeAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_range(this);
}

llvm::Value* SizeOfAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_size_of(this);
}

llvm::Value* BinOpAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_bin_op(this);
}

llvm::Value* BinCompAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_bin_comp(this);
}

llvm::Value* CondAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_cond(this);
}

llvm::Value* CallAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_call(this);
}

llvm::Value* ListOpAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_list_op(this);
}

llvm::Value* ResourceAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_resources(this);
}

llvm::Value* FlexBindAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_flex_bind(this);
}

llvm::Value* FinalBindAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_final_bind(this);
}

llvm::Value* StructAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_struct(this);
}

llvm::Value* ReadFileAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_read_file(this);
}

llvm::Value* PrintAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_print(this);
}

llvm::Value* ExtPackAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_ext_pack(this);
}

llvm::Value* SideBlockAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_side_block(this);
}

llvm::Value* CasesAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_cases(this);
}

llvm::Value* CondFlowAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_cond_flow(this);
}

llvm::Value* CheckEqAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_check_equal(this);
}

llvm::Value* CheckIsInAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_check_isin(this);
}

llvm::Value* FromToAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_from_to(this);
}

llvm::Value* ForwardAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_forward(this);
}

llvm::Value* InfiniteAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_inf(this);
}

llvm::Value* FuncAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_func(this);
}

llvm::Value* LoopAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_loop(this);
}

llvm::Value* IterAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_iterator(this);
}

llvm::Value* MainBlockAST::toLLVM(StyioToLLVM* generator) {
  return generator -> visit_main(this);
}