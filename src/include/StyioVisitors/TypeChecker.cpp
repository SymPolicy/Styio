// [C++ STL]
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <optional>

// [Styio]
#include "../StyioToken/Token.hpp"
#include "../StyioAST/AST.hpp"

void StyioChecker::visit_true(TrueAST* ast) {
  
}

void StyioChecker::visit_false(FalseAST* ast) {
  
}

void StyioChecker::visit_none(NoneAST* ast) {
  
}

void StyioChecker::visit_end(EndAST* ast) {
  
}

void StyioChecker::visit_empty(EmptyAST* ast) {
  
}

void StyioChecker::visit_empty_block(EmptyBlockAST* ast) {
  
}

void StyioChecker::visit_pass(PassAST* ast) {
  
}

void StyioChecker::visit_break(BreakAST* ast) {
  
}

void StyioChecker::visit_return(ReturnAST* ast) {
  
}

void StyioChecker::visit_comment(CommentAST* ast) {
  
}

void StyioChecker::visit_id(IdAST* ast) {
  
}

void StyioChecker::visit_var(VarAST* ast) {
  
}

void StyioChecker::visit_arg(ArgAST* ast) {
  
}

void StyioChecker::visit_opt_arg(OptArgAST* ast) {
  
}

void StyioChecker::visit_opt_kwarg(OptKwArgAST* ast) {
  
}

void StyioChecker::visit_var_tuple(VarTupleAST* ast) {
  
}

void StyioChecker::visit_type(DTypeAST* ast) {
  
}

void StyioChecker::visit_int(IntAST* ast) {
  
}

void StyioChecker::visit_float(FloatAST* ast) {
  
}

void StyioChecker::visit_char(CharAST* ast) {
  
}

void StyioChecker::visit_string(StringAST* ast) {
  
}

void StyioChecker::visit_fmt_str(FmtStrAST* ast) {
  
}

void StyioChecker::visit_ext_path(ExtPathAST* ast) {
  
}

void StyioChecker::visit_ext_link(ExtLinkAST* ast) {
  
}

void StyioChecker::visit_list(ListAST* ast) {
  
}

void StyioChecker::visit_tuple(TupleAST* ast) {
  
}

void StyioChecker::visit_set(SetAST* ast) {
  
}

void StyioChecker::visit_range(RangeAST* ast) {
  
}

void StyioChecker::visit_size_of(SizeOfAST* ast) {
  
}

void StyioChecker::visit_bin_op(BinOpAST* ast) {
  
}

void StyioChecker::visit_bin_comp(BinCompAST* ast) {
  
}

void StyioChecker::visit_cond(CondAST* ast) {
  
}

void StyioChecker::visit_call(CallAST* ast) {
  
}

void StyioChecker::visit_list_op(ListOpAST* ast) {
  
}

void StyioChecker::visit_resources(ResourceAST* ast) {
  
}

void StyioChecker::visit_flex_bind(FlexBindAST* ast) {
  
}

void StyioChecker::visit_final_bind(FinalBindAST* ast) {
  
}

void StyioChecker::visit_struct(StructAST* ast) {
  
}

void StyioChecker::visit_read_file(ReadFileAST* ast) {
  
}

void StyioChecker::visit_print(PrintAST* ast) {
  
}

void StyioChecker::visit_ext_pack(ExtPackAST* ast) {
  
}

void StyioChecker::visit_cases(CasesAST* ast) {
  
}

void StyioChecker::visit_cond_flow(CondFlowAST* ast) {
  
}

void StyioChecker::visit_check_equal(CheckEqAST* ast) {
  
}

void StyioChecker::visit_check_isin(CheckIsInAST* ast) {
  
}

void StyioChecker::visit_from_to(FromToAST* ast) {
  
}

void StyioChecker::visit_forward(ForwardAST* ast) {
  
}

void StyioChecker::visit_infinite(InfiniteAST* ast) {
  
}

void StyioChecker::visit_function(FuncAST* ast) {
  
}

void StyioChecker::visit_loop(LoopAST* ast) {
  
}

void StyioChecker::visit_iterator(IterAST* ast) {
  
}


void StyioChecker::visit_side_block(SideBlockAST* ast) {
  
}


void StyioChecker::visit_main(
  MainBlockAST* ast
) {
  auto& stmts = ast -> getStmts();
  for (auto const& s: stmts) {
    s -> accept(this); }
}
