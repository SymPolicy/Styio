// [C++ STL]
#include <string>
#include <memory>
#include <vector>

// [Styio]
#include "../StyioUtil/Util.hpp"
#include "../StyioToken/Token.hpp"
#include "AST.hpp"

void TrueAST::accept(StyioVisitor* visitor) {
  visitor -> visit_true(this);
}

void FalseAST::accept(StyioVisitor* visitor) {
  visitor -> visit_false(this);
}

void NoneAST::accept(StyioVisitor* visitor) {
  visitor -> visit_none(this);
}

void EndAST::accept(StyioVisitor* visitor) {
  visitor -> visit_end(this);
}

void EmptyAST::accept(StyioVisitor* visitor) {
  visitor -> visit_empty(this);
}

void EmptyBlockAST::accept(StyioVisitor* visitor) {
  visitor -> visit_empty_block(this);
}

void PassAST::accept(StyioVisitor* visitor) {
  visitor -> visit_pass(this);
}

void BreakAST::accept(StyioVisitor* visitor) {
  visitor -> visit_break(this);
}

void ReturnAST::accept(StyioVisitor* visitor) {
  visitor -> visit_return(this);
}

void CommentAST::accept(StyioVisitor* visitor) {
  visitor -> visit_comment(this);
}

void IdAST::accept(StyioVisitor* visitor) {
  visitor -> visit_id(this);
}

void ArgAST::accept(StyioVisitor* visitor) {
  visitor -> visit_arg(this);
}

void KwArgAST::accept(StyioVisitor* visitor) {
  visitor -> visit_kwarg(this);
}

void VarsTupleAST::accept(StyioVisitor* visitor) {
  visitor -> visit_vars_tuple(this);
}

void TypeAST::accept(StyioVisitor* visitor) {
  visitor -> visit_type(this);
}

void TypedVarAST::accept(StyioVisitor* visitor) {
  visitor -> visit_typed_var(this);
}

void IntAST::accept(StyioVisitor* visitor) {
  visitor -> visit_int(this);
}

void FloatAST::accept(StyioVisitor* visitor) {
  visitor -> visit_float(this);
}

void CharAST::accept(StyioVisitor* visitor) {
  visitor -> visit_char(this);
}

void StringAST::accept(StyioVisitor* visitor) {
  visitor -> visit_string(this);
}

void FmtStrAST::accept(StyioVisitor* visitor) {
  visitor -> visit_fmt_str(this);
}

void ExtPathAST::accept(StyioVisitor* visitor) {
  visitor -> visit_ext_path(this);
}

void ExtLinkAST::accept(StyioVisitor* visitor) {
  visitor -> visit_ext_link(this);
}

void ListAST::accept(StyioVisitor* visitor) {
  visitor -> visit_list(this);
}

void TupleAST::accept(StyioVisitor* visitor) {
  visitor -> visit_tuple(this);
}

void SetAST::accept(StyioVisitor* visitor) {
  visitor -> visit_set(this);
}

void RangeAST::accept(StyioVisitor* visitor) {
  visitor -> visit_range(this);
}

void SizeOfAST::accept(StyioVisitor* visitor) {
  visitor -> visit_size_of(this);
}

void BinOpAST::accept(StyioVisitor* visitor) {
  visitor -> visit_bin_op(this);
}

void BinCompAST::accept(StyioVisitor* visitor) {
  visitor -> visit_bin_comp(this);
}

void CondAST::accept(StyioVisitor* visitor) {
  visitor -> visit_cond(this);
}

void CallAST::accept(StyioVisitor* visitor) {
  visitor -> visit_call(this);
}

void ListOpAST::accept(StyioVisitor* visitor) {
  visitor -> visit_list_op(this);
}

void ResourceAST::accept(StyioVisitor* visitor) {
  visitor -> visit_resources(this);
}

void FlexBindAST::accept(StyioVisitor* visitor) {
  visitor -> visit_flex_bind(this);
}

void FinalBindAST::accept(StyioVisitor* visitor) {
  visitor -> visit_final_bind(this);
}

void StructAST::accept(StyioVisitor* visitor) {
  visitor -> visit_struct(this);
}

void ReadFileAST::accept(StyioVisitor* visitor) {
  visitor -> visit_read_file(this);
}

void PrintAST::accept(StyioVisitor* visitor) {
  visitor -> visit_print(this);
}

void ExtPackAST::accept(StyioVisitor* visitor) {
  visitor -> visit_ext_pack(this);
}

void BlockAST::accept(StyioVisitor* visitor) {
  visitor -> visit_block(this);
}

void CasesAST::accept(StyioVisitor* visitor) {
  visitor -> visit_cases(this);
}

void CondFlowAST::accept(StyioVisitor* visitor) {
  visitor -> visit_cond_flow(this);
}

void CheckEqAST::accept(StyioVisitor* visitor) {
  visitor -> visit_check_equal(this);
}

void CheckIsInAST::accept(StyioVisitor* visitor) {
  visitor -> visit_check_isin(this);
}

void FromToAST::accept(StyioVisitor* visitor) {
  visitor -> visit_from_to(this);
}

void ForwardAST::accept(StyioVisitor* visitor) {
  visitor -> visit_forward(this);
}

void InfiniteAST::accept(StyioVisitor* visitor) {
  visitor -> visit_infinite(this);
}

void FuncAST::accept(StyioVisitor* visitor) {
  visitor -> visit_function(this);
}

void LoopAST::accept(StyioVisitor* visitor) {
  visitor -> visit_loop(this);
}

void IterAST::accept(StyioVisitor* visitor) {
  visitor -> visit_iterator(this);
}