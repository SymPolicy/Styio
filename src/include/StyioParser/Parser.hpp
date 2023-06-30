#ifndef STYIO_PARSER_H_
#define STYIO_PARSER_H_

template <typename Enumeration>
auto type_to_int(Enumeration const value)
    -> typename std::underlying_type<Enumeration>::type
{
    return static_cast<typename std::underlying_type<Enumeration>::type>(value);
}

int get_next_char();

void get_next_char(int& cur_char);

bool check_this_char(int& cur_char, char value);

void drop_all_spaces (int& cur_char);

void drop_white_spaces (int& cur_char);

/*
  parse_id
*/
IdAST* parse_id (std::vector<int>& tok_ctx, int& cur_char);

/*
  parse_int
*/
IntAST* parse_int (std::vector<int>& tok_ctx, int& cur_char);

/*
  parse_int_or_float
*/
StyioAST* parse_int_or_float (std::vector<int>& tok_ctx, int& cur_char);

/*
  parse_string
*/
StringAST* parse_string (std::vector<int>& tok_ctx, int& cur_char);

/*
  parse_size_of
*/
SizeOfAST* parse_size_of (std::vector<int>& tok_ctx, int& cur_char);

/*
  parse_ext_res
*/
StyioAST* parse_ext_res (std::vector<int>& tok_ctx, int& cur_char);

/*
  parse_list_elem
*/
StyioAST* parse_list_elem (std::vector<int>& tok_ctx, int& cur_char);

/*
  parse_list_op
*/
ListOpAST* parse_list_op (
  std::vector<int>& tok_ctx, 
  int& cur_char,
  StyioAST* theList
);

/*
  parse_multi_vars
*/
std::vector<IdAST*> parse_multi_vars (
  std::vector<int>& tok_ctx, 
  int& cur_char
);

/*
  parse_iter_over
*/
StyioAST* parse_iter_over (
  std::vector<int>& tok_ctx, 
  int& cur_char,
  StyioAST* collection
);

/*
  parse_list_expr
*/
StyioAST* parse_list_expr (std::vector<int>& tok_ctx, int& cur_char);

/*
  parse_loop
*/
InfLoopAST* parse_loop (std::vector<int>& tok_ctx, int& cur_char);

/*
  parse_bin_rhs
*/
StyioAST* parse_bin_rhs (
  std::vector<int>& tok_ctx, 
  int& cur_char
);

/*
  parse_bin_op
*/
BinOpAST* parse_bin_op (
  std::vector<int>& tok_ctx, 
  int& cur_char, 
  StyioAST* lhs_ast
);

/*
  parse_simple_value
*/
StyioAST* parse_simple_value (
  std::vector<int>& tok_ctx, 
  int& cur_char
);

/*
  parse_value_expr
*/
StyioAST* parse_value_expr (
  std::vector<int>& tok_ctx, 
  int& cur_char
);

/*
  parse_mut_assign
*/
MutAssignAST* parse_mut_assign (
  std::vector<int>& tok_ctx, 
  int& cur_char, 
  IdAST* id_ast
);

/*
  parse_fix_assign
*/
FixAssignAST* parse_fix_assign (
  std::vector<int>& tok_ctx, 
  int& cur_char, 
  IdAST* id_ast
);

/*
  parse_read_file
*/
StyioAST* parse_read_file (
  std::vector<int>& tok_ctx, 
  int& cur_char, 
  IdAST* id_ast
);

/*
  parse_stmt
*/
StyioAST* parse_stmt (std::vector<int>& tok_ctx, int& cur_char);

/*
  parse_ext_elem
*/
std::string parse_ext_elem(std::vector<int>& tok_ctx, int& cur_char);

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
ExtPackAST* parse_ext_pack (std::vector<int>& tok_ctx, int& cur_char);

/*
  parse_block
*/
BlockAST* parse_block (std::vector<int>& tok_ctx, int& cur_char);

/*
  parse_program
*/
void parse_program ();

#endif