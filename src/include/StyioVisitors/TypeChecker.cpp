// [C++ STL]
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <optional>

// [Styio]
#include "../StyioToken/Token.hpp"
#include "../StyioAST/AST.hpp"

std::unique_ptr<BoolAST> StyioChecker::visit_true(TrueAST* ast) {
  return BoolAST::make(true);
}

std::shared_ptr<BoolAST> StyioChecker::visit_false(FalseAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_none(NoneAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_end(EndAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_empty(EmptyAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_empty_block(EmptyBlockAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_pass(PassAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_break(BreakAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_return(ReturnAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_comment(CommentAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_id(IdAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_var(VarAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_arg(ArgAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_opt_arg(OptArgAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_opt_kwarg(OptKwArgAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_var_tuple(VarTupleAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_type(DTypeAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_typed_var(TypedVarAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_int(IntAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_float(FloatAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_char(CharAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_string(StringAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_fmt_str(FmtStrAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_ext_path(ExtPathAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_ext_link(ExtLinkAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_list(ListAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_tuple(TupleAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_set(SetAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_range(RangeAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_size_of(SizeOfAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_bin_op(BinOpAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_bin_comp(BinCompAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_cond(CondAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_call(CallAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_list_op(ListOpAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_resources(ResourceAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_flex_bind(FlexBindAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_final_bind(FinalBindAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_struct(StructAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_read_file(ReadFileAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_print(PrintAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_ext_pack(ExtPackAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_cases(CasesAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_cond_flow(CondFlowAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_check_equal(CheckEqAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_check_isin(CheckIsInAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_from_to(FromToAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_forward(ForwardAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_infinite(InfiniteAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_function(FuncAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_loop(LoopAST* ast) {
  return BoolAST::make(false);
}

std::shared_ptr<StyioNaive> StyioChecker::visit_iterator(IterAST* ast) {
  return BoolAST::make(false);
}


std::shared_ptr<StyioNaive> StyioChecker::visit_side_block(SideBlockAST* ast) {
  return BoolAST::make(false);
}


std::shared_ptr<StyioNaive> StyioChecker::visit_main(MainBlockAST* ast) {
  return BoolAST::make(false);
}
