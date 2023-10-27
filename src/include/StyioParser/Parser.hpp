#pragma once
#ifndef STYIO_PARSER_H_
#define STYIO_PARSER_H_

struct StyioCodeContext
{
  std::string text;
  int cursor;
};

template <typename Enumeration>
auto type_to_int (Enumeration const value)
    -> typename std::underlying_type<Enumeration>::type
{
    return static_cast<typename std::underlying_type<Enumeration>::type>(value);
}

/*
  =================
    Variable
  =================
*/

/*
  parse_id
*/
std::unique_ptr<IdAST> parse_id (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  =================
    Scalar Value
  =================
*/

/*
  parse_int
*/
std::unique_ptr<IntAST> parse_int 
(
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_int_or_float
*/
std::unique_ptr<StyioAST> parse_int_or_float 
(
  struct StyioCodeContext* code, 
  char& cur_char
);

/*
  parse_str
*/
std::unique_ptr<StringAST> parse_str (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_fmt_str
*/
std::unique_ptr<FmtStrAST> parse_fmt_str (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_path_or_link
*/
std::unique_ptr<StyioAST> parse_path_or_link (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_fill_arg
*/
std::unique_ptr<FillArgAST> parse_fill_arg (
  struct StyioCodeContext* code,
  char& cur_char
);

std::unique_ptr<StyioAST> parse_tuple (
  struct StyioCodeContext* code,
  char& cur_char
);

std::unique_ptr<StyioAST> parse_list (
  struct StyioCodeContext* code,
  char& cur_char
);

std::unique_ptr<StyioAST> parse_set (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  =================
    Basic Operation
  =================
*/

/*
  parse_size_of
*/
std::unique_ptr<SizeOfAST> parse_size_of 
(
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_call
*/
std::unique_ptr<StyioAST> parse_call 
(
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_bin_rhs
*/
std::unique_ptr<StyioAST> parse_item_for_binop (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_binop_rhs
*/
std::unique_ptr<BinOpAST> parse_binop_rhs (
  struct StyioCodeContext* code,
  char& cur_char, 
  std::unique_ptr<StyioAST> lhs_ast
);

/*
  parse_item_for_cond
*/
std::unique_ptr<StyioAST> parse_item_for_cond (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_cond
*/
std::unique_ptr<CondAST> parse_cond (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_cond_flow
*/
std::unique_ptr<StyioAST> parse_cond_flow (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_list_op
*/
std::unique_ptr<StyioAST> parse_list_op (
  struct StyioCodeContext* code,
  char& cur_char,
  std::unique_ptr<StyioAST> theList
);

/*
  parse_vars_tuple
*/
std::unique_ptr<VarTupleAST> parse_vars_tuple (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_loop_or_iter
*/
std::unique_ptr<StyioAST> parse_loop_or_iter (
  struct StyioCodeContext* code,
  char& cur_char,
  std::unique_ptr<StyioAST> collection
);

/*
  parse_list_or_loop
*/
std::unique_ptr<StyioAST> parse_list_or_loop (struct StyioCodeContext* code,char& cur_char);

/*
  parse_loop
*/
std::unique_ptr<StyioAST> parse_loop (struct StyioCodeContext* code,char& cur_char);

/*
  parse_simple_value
*/
std::unique_ptr<StyioAST> parse_num_val (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_expr
*/
std::unique_ptr<StyioAST> parse_expr (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_resources
*/
std::unique_ptr<ResourceAST> parse_resources (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_bind_final
*/
std::unique_ptr<FinalBindAST> parse_bind_final (
  struct StyioCodeContext* code,
  char& cur_char, 
  std::unique_ptr<IdAST> id_ast
);

/*
  parse_pipeline
*/
std::unique_ptr<StyioAST> parse_pipeline (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_read_file
*/
std::unique_ptr<StyioAST> parse_read_file (
  struct StyioCodeContext* code,
  char& cur_char, 
  IdAST* id_ast
);

/*
  parse_one_or_many_repr
*/
std::unique_ptr<StyioAST> parse_one_or_many_repr (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_print
*/
std::unique_ptr<StyioAST> parse_print (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_panic
*/
std::unique_ptr<StyioAST> parse_panic (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_stmt
*/
std::unique_ptr<StyioAST> parse_stmt (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_ext_elem
*/
std::string parse_ext_elem(struct StyioCodeContext* code,char& cur_char);

/*
  parse_ext_pack

  Dependencies should be written like a list of paths
  like this -> ["ab/c", "x/yz"]

  // 1. The dependencies should be parsed before any domain (statement/expression). 
  // 2. The left square bracket `[` is only eliminated after entering this function (parse_ext_pack)
  |-- "[" <PATH>+ "]"

  If ? ( "the program starts with a left square bracket `[`" ),
  then -> { 
    "parse_ext_pack() starts";
    "eliminate the left square bracket `[`";
    "parse dependency paths, which take comma `,` as delimeter";
    "eliminate the right square bracket `]`";
  } 
  else :  { 
    "parse_ext_pack() should NOT be invoked in this case";
    "if starts with left curly brace `{`, try parseSpace()";
    "otherwise, try parseScript()";
  }
*/
std::unique_ptr<ExtPackAST> parse_ext_pack (struct StyioCodeContext* code,char& cur_char);

/*
  parse_cases
*/
std::unique_ptr<StyioAST> parse_cases (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_block
*/
std::unique_ptr<StyioAST> parse_block (
  struct StyioCodeContext* code,
  char& cur_char
);

std::unique_ptr<StyioAST> parse_forward (
  struct StyioCodeContext* code,
  char& cur_char,
  bool labeled = false
);


std::unique_ptr<MainBlockAST> parse_main_block (std::string styio_code);

#endif