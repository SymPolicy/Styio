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

inline bool can_be_ignored (char token) {
  return isspace(token);
}

inline bool check_token (
  char& cur_char, 
  char value) {
  return cur_char == value;
}

void find_token_panic (
  struct StyioCodeContext* code,
  char& cur_char,
  char value) {
  
  move_across_ignored(code, cur_char);

  if (not check_token(cur_char, value)) {
    std::string errmsg = std::string("Expecting ") + char(value) + " but got " + char(cur_char);
    throw StyioSyntaxError(errmsg);
  }
}

bool peak_next_token (
  struct StyioCodeContext* code,
  char value) {
  int pos = code -> cursor;

  while (can_be_ignored((code -> text.at(pos)))) {
    pos += 1;
  }

  return check_token(code -> text.at(pos), value);
}

inline void move_to_the_next (
  struct StyioCodeContext* code,
  char& cur_char
) {
  code -> cursor += 1;
  cur_char = code -> text.at(code -> cursor);
}

void move_across_ignored (
  struct StyioCodeContext* code,
  char& cur_char) {
  while (can_be_ignored(code -> text.at(code -> cursor)))
  {
    code -> cursor += 1;
    cur_char = code -> text.at(code -> cursor);
  }
}

void move_until (
  struct StyioCodeContext* code,
  char& cur_char,
  char value) {
  while (not check_token(cur_char, value)) {
    move_to_the_next(code, cur_char);
  }
}

void move_across (
  struct StyioCodeContext* code,
  char& cur_char,
  char value) {
  while (not check_token(cur_char, value)) {
    move_to_the_next(code, cur_char);
  }

  move_to_the_next(code, cur_char);
}

bool find_and_drop (
  struct StyioCodeContext* code,
  char& cur_char,
  char value) {
  move_across_ignored(code, cur_char);

  if (check_token(cur_char, value)) {
    move_to_the_next(code, cur_char);
    return true;
  }

  return false;
}

void find_and_drop_panic (
  struct StyioCodeContext* code,
  char& cur_char,
  char value) {
  
  move_across_ignored(code, cur_char);

  if (check_token(cur_char, value)) {
    move_to_the_next(code, cur_char);
  }
  else {
    std::string errmsg = std::string("Expecting ") + char(value) + " but got " + char(cur_char);
    throw StyioSyntaxError(errmsg);
  }
}

inline bool check_next_and_move (
  struct StyioCodeContext* code, 
  char& cur_char, 
  char value) {
  char next = code -> text.at(code -> cursor);

  if (check_token(next, value)) {
    move_to_the_next(code, cur_char);
    return true;
  }

  return false;
}

inline void check_next_and_move_panic (
  struct StyioCodeContext* code, 
  char& cur_char,
  char value) {
  char next = code -> text.at(code -> cursor);

  if (check_token(next, value)) {
    move_to_the_next(code, cur_char);
  }
  else {
    std::string errmsg = std::string("Expecting ") + char(value) + " but got " + char(cur_char);
    throw StyioSyntaxError(errmsg);
  }
}

inline void match_and_ignore (
  struct StyioCodeContext* code,
  char& cur_char) {
  move_to_the_next(code, cur_char);
  move_across_ignored(code, cur_char);
}

void drop_spaces (
  struct StyioCodeContext* code,
  char& cur_char) {
  while (isspace(cur_char)) {
    move_to_the_next(code, cur_char);
  };
}

void drop_white_spaces (
  struct StyioCodeContext* code,
  char& cur_char) {
  while (check_token(cur_char, ' ')) {
    move_to_the_next(code, cur_char);
  };
}

inline bool check_binop_token (
  struct StyioCodeContext* code) {
  int pos = code -> cursor;

  while (can_be_ignored((code -> text.at(pos)))) {
    pos += 1;
  }

  char the_char = code -> text.at(pos);

  if (the_char == '+' 
    || the_char == '-' 
    || the_char == '*' 
    || the_char == '%') { 
    return true; 
  }
  else if (the_char == '/') {
    if ((code -> text.at(pos + 1)) == '*') { 
      return false; 
    } 
    else { 
      return true; 
    }
  }

  return false;
}

std::unique_ptr<CommentAST> parse_comment(
  struct StyioCodeContext* code,
  char& cur_char,
  int mode);

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
  char& cur_char);

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
  parse_string
*/
std::unique_ptr<StringAST> parse_string 
(
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_path_or_link
*/
std::unique_ptr<StyioAST> parse_path_or_link 
(
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
std::unique_ptr<ListOpAST> parse_list_op (
  struct StyioCodeContext* code,
  char& cur_char,
  std::unique_ptr<StyioAST> theList
);

/*
  parse_filling
*/
std::unique_ptr<FillingAST> parse_filling (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_iter
*/
std::unique_ptr<StyioAST> parse_iter (
  struct StyioCodeContext* code,
  char& cur_char,
  std::unique_ptr<StyioAST> collection
);

/*
  parse_list_expr
*/
std::unique_ptr<StyioAST> parse_list_expr (struct StyioCodeContext* code,char& cur_char);

/*
  parse_loop
*/
std::unique_ptr<StyioAST> parse_loop (struct StyioCodeContext* code,char& cur_char);

/*
  parse_simple_value
*/
std::unique_ptr<StyioAST> parse_value (
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
  parse_mut_assign
*/
std::unique_ptr<FlexBindAST> parse_mut_assign (
  struct StyioCodeContext* code,
  char& cur_char, 
  IdAST* id_ast
);

/*
  parse_fix_assign
*/
std::unique_ptr<FinalBindAST> parse_fix_assign (
  struct StyioCodeContext* code,
  char& cur_char, 
  IdAST* id_ast
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
  parse_write_stdout
*/
std::unique_ptr<StyioAST> parse_write_stdout (
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
  parse_case_block
*/
std::unique_ptr<StyioAST> parse_case_block (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_exec_block
*/
std::unique_ptr<StyioAST> parse_exec_block (
  struct StyioCodeContext* code,
  char& cur_char
);

/*
  parse_program
*/
void parse_program (std::string styio_code);

#endif