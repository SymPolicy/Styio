// [C++ STL]
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <optional>

// [Styio]
#include "../StyioToken/Token.hpp"
#include "../StyioAST/AST.hpp"

std::shared_ptr<StyioAST> StyioChecker::visit_true(TrueAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_false(FalseAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_none(NoneAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_end(EndAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_empty(EmptyAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_empty_block(EmptyBlockAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_pass(PassAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_break(BreakAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_return(ReturnAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_comment(CommentAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_id(IdAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_var(VarAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_fill_arg(ArgAST* ast) {

}

std::shared_ptr<StyioAST> StyioChecker::visit_arg(OptArgAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_kwarg(OptKwArgAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_var_tuple(VarTupleAST* ast) {

}

std::shared_ptr<StyioAST> StyioChecker::visit_type(DTypeAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_typed_var(TypedVarAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_int(IntAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_float(FloatAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_char(CharAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_string(StringAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_fmt_str(FmtStrAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_ext_path(ExtPathAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_ext_link(ExtLinkAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_list(ListAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_tuple(TupleAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_set(SetAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_range(RangeAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_size_of(SizeOfAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_bin_op(BinOpAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_bin_comp(BinCompAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_cond(CondAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_call(CallAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_list_op(ListOpAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_resources(ResourceAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_flex_bind(FlexBindAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_final_bind(FinalBindAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_struct(StructAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_read_file(ReadFileAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_print(PrintAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_ext_pack(ExtPackAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_cases(CasesAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_cond_flow(CondFlowAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_check_equal(CheckEqAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_check_isin(CheckIsInAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_from_to(FromToAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_forward(ForwardAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_inf(InfiniteAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_func(FuncAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_loop(LoopAST* ast) {
  
}

std::shared_ptr<StyioAST> StyioChecker::visit_iterator(IterAST* ast) {
  
}


std::shared_ptr<StyioAST> StyioChecker::visit_side_block(SideBlockAST* ast) {
  
}


std::shared_ptr<StyioAST> StyioChecker::visit_main(MainBlockAST* ast) {
  
}
