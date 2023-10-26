// [C++ STL]
#include <string>
#include <memory>
#include <vector>

// [Styio]
#include "../StyioUtil/Util.hpp"
#include "../StyioToken/Token.hpp"
#include "AST.hpp"

void TrueAST::accept(StyioToLLVM* generator) {
  generator -> gen_true(this);
}

void FalseAST::accept(StyioToLLVM* generator) {
  generator -> gen_false(this);
}

void NoneAST::accept(StyioToLLVM* generator) {
  generator -> gen_none(this);
}

void EndAST::accept(StyioToLLVM* generator) {
  generator -> gen_end(this);
}

void EmptyAST::accept(StyioToLLVM* generator) {
  generator -> gen_empty(this);
}

void EmptyBlockAST::accept(StyioToLLVM* generator) {
  generator -> gen_empty_block(this);
}

void PassAST::accept(StyioToLLVM* generator) {
  generator -> gen_pass(this);
}

void BreakAST::accept(StyioToLLVM* generator) {
  generator -> gen_break(this);
}

void ReturnAST::accept(StyioToLLVM* generator) {
  generator -> gen_return(this);
}

void CommentAST::accept(StyioToLLVM* generator) {
  generator -> gen_comment(this);
}

void IdAST::accept(StyioToLLVM* generator) {
  generator -> gen_id(this);
}

void ArgAST::accept(StyioToLLVM* generator) {
  generator -> gen_arg(this);
}

void KwArgAST::accept(StyioToLLVM* generator) {
  generator -> gen_kwarg(this);
}

void VarsTupleAST::accept(StyioToLLVM* generator) {
  generator -> gen_vars_tuple(this);
}

void TypeAST::accept(StyioToLLVM* generator) {
  generator -> gen_type(this);
}

void TypedVarAST::accept(StyioToLLVM* generator) {
  generator -> gen_typed_var(this);
}

void IntAST::accept(StyioToLLVM* generator) {
  generator -> gen_int(this);
}

void FloatAST::accept(StyioToLLVM* generator) {
  generator -> gen_float(this);
}

void CharAST::accept(StyioToLLVM* generator) {
  generator -> gen_char(this);
}

void StringAST::accept(StyioToLLVM* generator) {
  generator -> gen_string(this);
}

void FmtStrAST::accept(StyioToLLVM* generator) {
  generator -> gen_fmt_str(this);
}

void ExtPathAST::accept(StyioToLLVM* generator) {
  generator -> gen_ext_path(this);
}

void ExtLinkAST::accept(StyioToLLVM* generator) {
  generator -> gen_ext_link(this);
}

void ListAST::accept(StyioToLLVM* generator) {
  generator -> gen_list(this);
}

void TupleAST::accept(StyioToLLVM* generator) {
  generator -> gen_tuple(this);
}

void SetAST::accept(StyioToLLVM* generator) {
  generator -> gen_set(this);
}

void RangeAST::accept(StyioToLLVM* generator) {
  generator -> gen_range(this);
}

void SizeOfAST::accept(StyioToLLVM* generator) {
  generator -> gen_size_of(this);
}

void BinOpAST::accept(StyioToLLVM* generator) {
  generator -> gen_bin_op(this);
}

void BinCompAST::accept(StyioToLLVM* generator) {
  generator -> gen_bin_comp(this);
}

void CondAST::accept(StyioToLLVM* generator) {
  generator -> gen_cond(this);
}

void CallAST::accept(StyioToLLVM* generator) {
  generator -> gen_call(this);
}

void ListOpAST::accept(StyioToLLVM* generator) {
  generator -> gen_list_op(this);
}

void ResourceAST::accept(StyioToLLVM* generator) {
  generator -> gen_resources(this);
}

void FlexBindAST::accept(StyioToLLVM* generator) {
  generator -> gen_flex_bind(this);
}

void FinalBindAST::accept(StyioToLLVM* generator) {
  generator -> gen_final_bind(this);
}

void StructAST::accept(StyioToLLVM* generator) {
  generator -> gen_struct(this);
}

void ReadFileAST::accept(StyioToLLVM* generator) {
  generator -> gen_read_file(this);
}

void PrintAST::accept(StyioToLLVM* generator) {
  generator -> gen_print(this);
}

void ExtPackAST::accept(StyioToLLVM* generator) {
  generator -> gen_ext_pack(this);
}

void SideBlockAST::accept(StyioToLLVM* generator) {
  generator -> gen_block(this);
}

void CasesAST::accept(StyioToLLVM* generator) {
  generator -> gen_cases(this);
}

void CondFlowAST::accept(StyioToLLVM* generator) {
  generator -> gen_cond_flow(this);
}

void CheckEqAST::accept(StyioToLLVM* generator) {
  generator -> gen_check_equal(this);
}

void CheckIsInAST::accept(StyioToLLVM* generator) {
  generator -> gen_check_isin(this);
}

void FromToAST::accept(StyioToLLVM* generator) {
  generator -> gen_from_to(this);
}

void ForwardAST::accept(StyioToLLVM* generator) {
  generator -> gen_forward(this);
}

void InfiniteAST::accept(StyioToLLVM* generator) {
  generator -> gen_infinite(this);
}

void FuncAST::accept(StyioToLLVM* generator) {
  generator -> gen_function(this);
}

void LoopAST::accept(StyioToLLVM* generator) {
  generator -> gen_loop(this);
}

void IterAST::accept(StyioToLLVM* generator) {
  generator -> gen_iterator(this);
}

void MainBlockAST::accept(StyioToLLVM* generator) {
  generator -> gen_main_block(this);
}