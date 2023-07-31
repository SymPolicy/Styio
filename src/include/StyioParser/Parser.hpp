#ifndef STYIO_PARSER_H_
#define STYIO_PARSER_H_

struct StyioCodeContext
{
  std::string text;
  int cursor;
};

template <typename Enumeration>
auto type_to_int(Enumeration const value)
    -> typename std::underlying_type<Enumeration>::type
{
    return static_cast<typename std::underlying_type<Enumeration>::type>(value);
}

void get_next_char(struct StyioCodeContext* code,int& cur_char);

bool check_this_char(int& cur_char, char value);

void drop_all_spaces (struct StyioCodeContext* code,int& cur_char);

void drop_white_spaces (struct StyioCodeContext* code,int& cur_char);

/*
  =================
    Variable
  =================
*/

/*
  parse_id
*/
IdAST* parse_id (struct StyioCodeContext* code,int& cur_char);

/*
  =================
    Scalar Value
  =================
*/

/*
  parse_int
*/
IntAST* parse_int (struct StyioCodeContext* code,int& cur_char);

/*
  parse_int_or_float
*/
StyioAST* parse_int_or_float (struct StyioCodeContext* code,int& cur_char);

/*
  parse_string
*/
StringAST* parse_string (struct StyioCodeContext* code,int& cur_char);

/*
  parse_ext_res
*/
StyioAST* parse_ext_res (struct StyioCodeContext* code,int& cur_char);

/*
  =================
    Basic Operation
  =================
*/

/*
  parse_size_of
*/
SizeOfAST* parse_size_of (struct StyioCodeContext* code,int& cur_char);

/*
  parse_bin_rhs
*/
StyioAST* parse_val_for_binop (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_binop_rhs
*/
BinOpAST* parse_binop_rhs (
  struct StyioCodeContext* code,
  int& cur_char, 
  StyioAST* lhs_ast
);

/*
  parse_cond_elem
*/
StyioAST* parse_val_for_cond (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_cond
*/
CondAST* parse_cond (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_cond_flow
*/
StyioAST* parse_cond_flow (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_list_op
*/
ListOpAST* parse_list_op (
  struct StyioCodeContext* code,
  int& cur_char,
  StyioAST* theList
);

/*
  parse_filling
*/
FillingAST* parse_filling (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_iter
*/
StyioAST* parse_iter (
  struct StyioCodeContext* code,
  int& cur_char,
  StyioAST* collection
);

/*
  parse_list_elem
*/
StyioAST* parse_list_elem (struct StyioCodeContext* code,int& cur_char);

/*
  parse_list_expr
*/
StyioAST* parse_list_expr (struct StyioCodeContext* code,int& cur_char);

/*
  parse_loop
*/
StyioAST* parse_loop (struct StyioCodeContext* code,int& cur_char);

/*
  parse_simple_value
*/
StyioAST* parse_simple_value (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_expr
*/
StyioAST* parse_expr (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_resources
*/
ResourceAST* parse_resources (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_mut_assign
*/
FlexBindAST* parse_mut_assign (
  struct StyioCodeContext* code,
  int& cur_char, 
  IdAST* id_ast
);

/*
  parse_fix_assign
*/
FinalBindAST* parse_fix_assign (
  struct StyioCodeContext* code,
  int& cur_char, 
  IdAST* id_ast
);

/*
  parse_pipeline
*/
StyioAST* parse_pipeline (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_read_file
*/
StyioAST* parse_read_file (
  struct StyioCodeContext* code,
  int& cur_char, 
  IdAST* id_ast
);

/*
  parse_write_stdout
*/
StyioAST* parse_write_stdout (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_stmt
*/
StyioAST* parse_stmt (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_ext_elem
*/
std::string parse_ext_elem(struct StyioCodeContext* code,int& cur_char);

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
ExtPackAST* parse_ext_pack (struct StyioCodeContext* code,int& cur_char);

/*
  parse_case_block
*/
StyioAST* parse_case_block (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_exec_block
*/
StyioAST* parse_exec_block (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_program
*/
void parse_program (std::string styio_code);

#endif