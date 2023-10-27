// [C++ STL]
#include <string>
#include <memory>
#include <vector>

// [Styio]
#include "../StyioUtil/Util.hpp"
#include "../StyioToken/Token.hpp"
#include "AST.hpp"

void TrueAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_true(this);
}

void FalseAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_false(this);
}

void NoneAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_none(this);
}

void EndAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_end(this);
}

void EmptyAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_empty(this);
}

void EmptyBlockAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_empty_block(this);
}

void PassAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_pass(this);
}

void BreakAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_break(this);
}

void ReturnAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_return(this);
}

void CommentAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_comment(this);
}

void IdAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_id(this);
}

void VarAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_var(this);
}

void FillArgAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_fill_arg(this);
}

void ArgAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_arg(this);
}

void KwArgAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_kwarg(this);
}

void VarTupleAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_var_tuple(this);
}

void DTypeAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_type(this);
}

void TypedVarAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_typed_var(this);
}

void IntAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_int(this);
}

void FloatAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_float(this);
}

void CharAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_char(this);
}

void StringAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_string(this);
}

void FmtStrAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_fmt_str(this);
}

void ExtPathAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_ext_path(this);
}

void ExtLinkAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_ext_link(this);
}

void ListAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_list(this);
}

void TupleAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_tuple(this);
}

void SetAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_set(this);
}

void RangeAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_range(this);
}

void SizeOfAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_size_of(this);
}

void BinOpAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_bin_op(this);
}

void BinCompAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_bin_comp(this);
}

void CondAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_cond(this);
}

void CallAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_call(this);
}

void ListOpAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_list_op(this);
}

void ResourceAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_resources(this);
}

void FlexBindAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_flex_bind(this);
}

void FinalBindAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_final_bind(this);
}

void StructAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_struct(this);
}

void ReadFileAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_read_file(this);
}

void PrintAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_print(this);
}

void ExtPackAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_ext_pack(this);
}

void SideBlockAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_side_block(this);
}

void CasesAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_cases(this);
}

void CondFlowAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_cond_flow(this);
}

void CheckEqAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_check_equal(this);
}

void CheckIsInAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_check_isin(this);
}

void FromToAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_from_to(this);
}

void ForwardAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_forward(this);
}

void InfiniteAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_inf(this);
}

void FuncAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_func(this);
}

void LoopAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_loop(this);
}

void IterAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_iterator(this);
}

void MainBlockAST::toLLVM(StyioToLLVM* generator) {
  generator -> visit_main(this);
}