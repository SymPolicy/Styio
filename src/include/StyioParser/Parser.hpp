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

void get_next_char
(
  struct StyioCodeContext* code,
  int& cur_char
);

void drop_all_spaces 
(
  struct StyioCodeContext* code,
  int& cur_char
);

void drop_white_spaces 
(
  struct StyioCodeContext* code,
  int& cur_char
);

bool check_this_char
(
  int& cur_char, 
  char value
);

/*
  =================
    Variable
  =================
*/

/*
  parse_id
*/
std::unique_ptr<IdAST> parse_id 
(
  struct StyioCodeContext* code,
  int& cur_char);

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
  int& cur_char
);

/*
  parse_int_or_float
*/
std::unique_ptr<StyioAST> parse_int_or_float 
(
  struct StyioCodeContext* code, 
  int& cur_char
);

/*
  parse_string
*/
std::unique_ptr<StringAST> parse_string 
(
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_path_or_link
*/
std::unique_ptr<StyioAST> parse_path_or_link 
(
  struct StyioCodeContext* code,
  int& cur_char
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
  int& cur_char
);

/*
  parse_call
*/
std::unique_ptr<StyioAST> parse_call 
(
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_bin_rhs
*/
std::unique_ptr<StyioAST> parse_item_for_binop (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_binop_rhs
*/
std::unique_ptr<BinOpAST> parse_binop_rhs (
  struct StyioCodeContext* code,
  int& cur_char, 
  std::unique_ptr<StyioAST> lhs_ast
);

/*
  parse_item_for_cond
*/
std::unique_ptr<StyioAST> parse_item_for_cond (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_cond
*/
std::unique_ptr<CondAST> parse_cond (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_cond_flow
*/
std::unique_ptr<StyioAST> parse_cond_flow (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_list_op
*/
std::unique_ptr<ListOpAST> parse_list_op (
  struct StyioCodeContext* code,
  int& cur_char,
  std::unique_ptr<StyioAST> theList
);

/*
  parse_filling
*/
std::unique_ptr<FillingAST> parse_filling (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_iter
*/
std::unique_ptr<StyioAST> parse_iter (
  struct StyioCodeContext* code,
  int& cur_char,
  std::unique_ptr<StyioAST> collection
);

/*
  parse_list_expr
*/
std::unique_ptr<StyioAST> parse_list_expr (struct StyioCodeContext* code,int& cur_char);

/*
  parse_loop
*/
std::unique_ptr<StyioAST> parse_loop (struct StyioCodeContext* code,int& cur_char);

/*
  parse_simple_value
*/
std::unique_ptr<StyioAST> parse_value (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_expr
*/
std::unique_ptr<StyioAST> parse_expr (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_resources
*/
std::unique_ptr<ResourceAST> parse_resources (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_mut_assign
*/
std::unique_ptr<FlexBindAST> parse_mut_assign (
  struct StyioCodeContext* code,
  int& cur_char, 
  IdAST* id_ast
);

/*
  parse_fix_assign
*/
std::unique_ptr<FinalBindAST> parse_fix_assign (
  struct StyioCodeContext* code,
  int& cur_char, 
  IdAST* id_ast
);

/*
  parse_pipeline
*/
std::unique_ptr<StyioAST> parse_pipeline (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_read_file
*/
std::unique_ptr<StyioAST> parse_read_file (
  struct StyioCodeContext* code,
  int& cur_char, 
  IdAST* id_ast
);

/*
  parse_write_stdout
*/
std::unique_ptr<StyioAST> parse_write_stdout (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_stmt
*/
std::unique_ptr<StyioAST> parse_stmt (
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
std::unique_ptr<ExtPackAST> parse_ext_pack (struct StyioCodeContext* code,int& cur_char);

/*
  parse_case_block
*/
std::unique_ptr<StyioAST> parse_case_block (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_exec_block
*/
std::unique_ptr<StyioAST> parse_exec_block (
  struct StyioCodeContext* code,
  int& cur_char
);

/*
  parse_program
*/
void parse_program (std::string styio_code);

/*
  read_and_parse
*/
void read_and_parse(std::filesystem::path filepath);

#endif