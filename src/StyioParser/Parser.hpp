#pragma once
#ifndef STYIO_PARSER_H_
#define STYIO_PARSER_H_

#include "../StyioToken/Token.hpp"

using std::pair;
using std::string;
using std::unordered_map;
using std::vector;

using std::cout;
using std::endl;

using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

class StyioContext;

class StyioContext
{
private:
  size_t curr_pos = 0; /* current position */

  string file_name;
  string code;
  vector<pair<size_t, size_t>> line_seps; /* line separations */

  shared_ptr<StyioAST> ast;
  unordered_map<string, shared_ptr<StyioAST>> constants;
  unordered_map<string, shared_ptr<StyioAST>> variables;

  bool debug_mode = false;

public:
  StyioContext(
    const string& file_name,
    const string& code_text,
    vector<pair<size_t, size_t>> line_seps,
    bool debug_mode = false
  ) :
      file_name(file_name),
      code(code_text),
      line_seps(line_seps),
      debug_mode(debug_mode) {
  }

  static StyioContext* Create(
    const string& file_name,
    const string& code_text,
    vector<pair<size_t, size_t>> line_seps,
    bool debug_mode = false
  ) {
    return new StyioContext(
      file_name,
      code_text,
      line_seps,
      debug_mode
    );
  }

  /* Get `code` */
  const string&
  get_code() const {
    return code;
  }

  /* Get `pos` */
  size_t get_curr_pos() {
    return curr_pos;
  }

  /* Get Current Character */
  char& get_curr_char() {
    return code.at(curr_pos);
  }

  size_t find_line_index(
    int p = -1
  ) {
    const size_t total_lines = line_seps.size();
    size_t line_index = 0;

    if (p < 0) {
      p = curr_pos;
    }

    if (debug_mode) {
      cout << "find_line_index(), starts with position: " << p << " current character: " << get_curr_char() << "\ninitial: line [" << line_index << "]" << endl;
    }

    bool binary_search = false;
    if (binary_search) {
      line_index = total_lines / 2;

      for (size_t i = 0; i < total_lines; i++) {
        if (debug_mode) {
          cout << "[" << line_index << "] is ";
        }

        if (p < line_seps[line_index].first) {
          line_index = line_index / 2;
          if (debug_mode) {
            cout << "too large, go to: [" << line_index << "]" << endl;
          }
        }
        else if (p > (line_seps[line_index].first + line_seps[line_index].second)) {
          line_index = (line_index + total_lines) / 2;
          if (debug_mode) {
            cout << "too small, go to: [" << line_index << "]" << endl;
          }
        }
        else {
          if (debug_mode) {
            cout << "result: [" << line_index << "]" << endl;
          }
          break;
        }
      }
    }
    else {
      for (size_t curr_line_index = 0; curr_line_index < total_lines; curr_line_index += 1) {
        if (line_seps[curr_line_index].first <= curr_pos
            && curr_pos <= (line_seps[curr_line_index].first + line_seps[curr_line_index].second)) {
          return curr_line_index;
        }
      }
    }

    return line_index;
  }

  string label_cur_line(
    int start = -1,
    std::string endswith = ""
  ) {
    string output("\n");

    if (start < 0)
      start = curr_pos;

    size_t lindex = find_line_index(start);
    size_t offset = start - line_seps[lindex].first;

    output += "File \"" + file_name + "\", Line " + std::to_string(lindex) + ", At " + std::to_string(offset) + ":\n\n";
    output += code.substr(line_seps[lindex].first, line_seps[lindex].second) + "\n";
    output += std::string(offset, ' ') + std::string("^");

    if (endswith.empty()) {
      output += std::string(line_seps[lindex].second - offset - 1, '-') + "\n";
    }
    else {
      output += " " + endswith + "\n";
    }

    return output;
  }

  // No Boundary Check !
  // | + n => move forward n steps
  // | - n => move backward n steps
  void move(size_t steps) {
    curr_pos += steps;
  }

  /* Check Value */
  bool check(char value) {
    return (code.at(curr_pos)) == value;
  }

  /* Check Value */
  bool check(const string& value) {
    return code.compare(curr_pos, value.size(), value) == 0;
  }

  /* Move Until */
  void move_until(char value) {
    while (not check(value)) {
      move(1);
    }
  }

  void move_until(const string& value) {
    while (not check(value)) {
      move(1);
    }
  }

  /* Check & Drop */
  bool check_drop(char value) {
    if (check(value)) {
      move(1);
      return true;
    }
    else {
      return false;
    }
  }

  /* Check & Drop */
  bool check_drop(const string& value) {
    if (check(value)) {
      move(value.size());
      return true;
    }
    else {
      return false;
    }
  }

  /* Find & Drop */
  bool find_drop(char value) {
    /* ! No Boundary Check ! */
    while (true) {
      if (isspace(get_curr_char())) {
        move(1);
      }
      else if (check("//")) {
        pass_over('\n');
      }
      else if (check("/*")) {
        pass_over("*/");
      }
      else {
        if (check(value)) {
          move(1);
          return true;
        }
        else {
          return false;
        }
      }
    }

    return false;
  }

  /* Find & Drop */
  bool find_drop(string value) {
    /* ! No Boundary Check ! */
    while (true) {
      if (isspace(get_curr_char())) {
        move(1);
      }
      else if (check("//")) {
        pass_over('\n');
      }
      else if (check("/*")) {
        pass_over("*/");
      }
      else {
        if ((code.substr(curr_pos, value.size())) == value) {
          move(value.size());
          return true;
        }
        else {
          return false;
        }
      }
    }
  }

  /* Pass Over */
  void pass_over(char value) {
    /* ! No Boundary Check ! */
    while (true) {
      if (check(value)) {
        move(1);
        break;
      }
      else {
        move(1);
      }
    }
  }

  /* Pass Over */
  void pass_over(const string& value) {
    /* ! No Boundary Check ! */
    while (true) {
      if (check(value)) {
        move(value.size());
        break;
      }
      else {
        move(1);
      }
    }
  }

  /* Peak Check */
  bool check_ahead(int steps, char value) {
    return (code.at(curr_pos + steps) == value);
  }

  /*
    usage:
    1. the current position is after a known operator
    like: a + b
             ^     right here, after +, the current position is a white space
    2. there is a variable or a value behind the current position
    like: 1 + 2
             ^     this space is followed by the value of 2
    3. the expected operator is behind that variable or value
    like: 1 + 2 * 3
             ^     curr_pos is a white space, the expected operator is *, which is behind 2.
  */
  string peak_operator(int num = 1) {
    int tmp_pos = curr_pos;
    int offset = 1;

    for (size_t i = 0; i < num; i++) {
      while (true) {
        if (isspace(code.at(tmp_pos))) {
          tmp_pos += 1;
        }
        else if (code.compare(tmp_pos, 2, string("//")) == 0) {
          tmp_pos += 2;

          while (code.at(tmp_pos) != '\n') {
            tmp_pos += 1;
          } /* warning: no boundary check */
          tmp_pos += 1;
        }
        /* match */ /* like */ /* this */
        else if (code.compare(tmp_pos, 2, string("/*")) == 0) {
          tmp_pos += 2;

          while (code.compare(tmp_pos, 2, string("*/")) != 0) {
            tmp_pos += 1;
          } /* warning: no boundary check */
          tmp_pos += 2;
        } /* warning: no boundary check */
        else if (isalnum(code.at(tmp_pos)) || (code.at(tmp_pos) == '_')) {
          tmp_pos += 1;
        }
        else if (code.at(tmp_pos) == EOF) {
          return "EOF";
        }
        else {
          break;
        }
      }

      /* that is: not space, not alpha, not number, not _ , and not comment*/
      while (
        not(isspace(code.at(tmp_pos))                      /* not space */
            || code.compare(tmp_pos, 2, string("/*")) != 0 /* not comment */
            || isalnum(code.at(tmp_pos)) || (code.at(tmp_pos) == '_') /* not alpha, not number, not _ */)
      ) {
        offset += 1;
      }
    }

    // std::cout << "peak tmp_pos: " << tmp_pos << " " << code.at(tmp_pos) << std::endl;
    // std::cout << "peak offset: " << offset << std::endl;
    // std::cout << "peak operator: " << code.substr(tmp_pos, offset) << std::endl;

    return code.substr(tmp_pos, offset);
  }

  bool peak_isdigit(int steps) {
    return isdigit(code.at(curr_pos + steps));
  }

  /* Drop White Spaces */
  void drop_white_spaces() {
    while (check(' ')) {
      move(1);
    }
  }

  /* Drop Spaces */
  void drop_all_spaces() {
    while (isspace(code.at(curr_pos))) {
      move(1);
    }
  }

  /* Drop Spaces & Comments */
  void drop_all_spaces_comments() {
    /* ! No Boundary Check ! */
    while (true) {
      if (isspace(code.at(curr_pos))) {
        move(1);
      }
      else if (check("//")) {
        pass_over('\n');
      }
      else if (check("/*")) {
        pass_over("*/");
      }
      else {
        break;
      }
    }
  }

  /* Match(Next) -> Panic */
  bool check_drop_panic(char value, std::string errmsg = "") {
    if (check(value)) {
      move(1);
      return true;
    }

    if (errmsg.empty()) {
      throw StyioSyntaxError(string("check_drop_panic(char)") + label_cur_line(curr_pos, std::string("which is expected to be ") + std::string(1, char(value))));
    }
    else {
      throw StyioSyntaxError(label_cur_line(curr_pos, errmsg));
    }
  }

  /* (Char) Find & Drop -> Panic */
  bool find_drop_panic(char value) {
    /* ! No Boundary Check ! */
    while (true) {
      if (isspace(get_curr_char())) {
        move(1);
      }
      else if (check("//")) {
        pass_over('\n');
      }
      else if (check("/*")) {
        pass_over("*/");
      }
      else {
        if (check(value)) {
          move(1);
          return true;
        }
        else {
          string errmsg = string("find_drop_panic(char)") + label_cur_line(curr_pos, std::string("which is expected to be ") + std::string(1, char(value)));
          throw StyioSyntaxError(errmsg);
        }
      }
    }
  }

  /* (String) Find & Drop -> Panic */
  bool find_drop_panic(string value) {
    /* ! No Boundary Check ! */
    while (true) {
      if (isspace(get_curr_char()))
        move(1);
      else if (check("//"))
        pass_over('\n');
      else if (check("/*"))
        pass_over("*/");
      else {
        if (check(value)) {
          move(value.size());
          return true;
        }
        else {
          string errmsg = string("find_drop_panic(string)") + label_cur_line(curr_pos, std::string("which is expected to be ") + value);
          throw StyioSyntaxError(errmsg);
        }
      }
    }
  }

  /* Find & Drop -> Panic */
  bool find_panic(const string& value) {
    /* ! No Boundary Check ! */
    while (true) {
      if (isspace(get_curr_char())) {
        move(1);
      }
      else if (check("//")) {
        pass_over('\n');
      }
      else if (check("/*")) {
        pass_over("*/");
      }
      else {
        if (check(value)) {
          move(value.size());
          return true;
        }
        else {
          string errmsg = string("find_panic(string)") + label_cur_line(curr_pos, std::string("which is expected to be ") + value);
          throw StyioSyntaxError(errmsg);
        }
      }
    }
  }

  /* Check isalpha or _ */
  bool check_isal_() {
    return isalpha(code.at(curr_pos)) || (code.at(curr_pos) == '_');
  }

  /* Check isalpha or isnum or _ */
  bool check_isalnum_() {
    return isalnum(code.at(curr_pos)) || (code.at(curr_pos) == '_');
  }

  /* Check isdigit */
  bool check_isdigit() {
    return isdigit(code.at(curr_pos));
  }

  /* Tuple Operations */
  bool check_tuple_ops() {
    return check("<<")     // extract
           or check(">>")  // iterate
           or check("=>")  // next
      ;
  }

  /* Check Chain of Data Processing */
  bool check_codp() {
    return check("filter")
           or check("sort")
           or check("map")
           or check("slice")
           or check("print");
  }

  /* Check Binary Operator */
  bool check_binop() {
    if (code.at(curr_pos) == '+' || code.at(curr_pos) == '-') {
      return true;
    }
    else if (code.at(curr_pos) == '*' || code.at(curr_pos) == '%') {
      return true;
    }
    else if (code.at(curr_pos) == '/') {
      /* Comments */
      if ((code.at(curr_pos + 1)) == '*' || code.at(curr_pos + 1) == '/') {
        return false;
      }
      else {
        return true;
      }
    }
    else if (code.at(curr_pos) == '%') {
      return true;
    }

    return false;
  }

  std::tuple<bool, TokenKind> get_binop_token() {
    switch (code.at(curr_pos)) {
      case '+': {
        return {true, TokenKind::Binary_Add};
      } break;

      case '-': {
        return {true, TokenKind::Binary_Sub};
      } break;

      case '*': {
        return {true, TokenKind::Binary_Mul};
      } break;

      case '/': {
        switch (code.at(curr_pos + 1)) {
          case '*': {
            return {false, TokenKind::Comment_MultiLine};
          } break;

          case '/': {
            return {false, TokenKind::Comment_SingleLine};
          } break;

          default: {
            return {true, TokenKind::Binary_Div};
          } break;
        }
      } break;

      case '%': {
        return {true, TokenKind::Binary_Mod};
      } break;

      default:
        break;
    }

    return {false, TokenKind::Undefined};
  }

  void
  show_code_with_linenum() {
    for (size_t i = 0; i < line_seps.size(); i++) {
      std::string line = code.substr(line_seps.at(i).first, line_seps.at(i).second);

      std::regex newline_regex("\n");
      std::string replaced_text = std::regex_replace(line, newline_regex, "[NEWLINE]");

      std::cout
        << "|" << i << "|-[" << line_seps.at(i).first << ":" << (line_seps.at(i).first + line_seps.at(i).second) << "] "
        << line << std::endl;
    }
  }
};

template <typename Enumeration>
auto
type_to_int(Enumeration const value) ->
  typename std::underlying_type<Enumeration>::type {
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
NameAST*
parse_name(StyioContext& context);

/*
  =================
    Scalar Value
  =================
*/

/*
  parse_int
*/
IntAST*
parse_int(StyioContext& context);

/*
  parse_int_or_float
*/
StyioAST*
parse_int_or_float(StyioContext& context, char& cur_char);

/*
  parse_string
*/
StringAST*
parse_string(StyioContext& context);

/*
  parse_fmt_str
*/
FmtStrAST*
parse_fmt_str(StyioContext& context);

/*
  parse_path
*/
StyioAST*
parse_path(StyioContext& context);

/*
  parse_fill_arg
*/
ArgAST*
parse_argument(StyioContext& context);

StyioAST*
parse_tuple(StyioContext& context);

StyioAST*
parse_tuple_no_braces(StyioContext& context, StyioAST* first_element = nullptr);

StyioAST*
parse_list(StyioContext& context);

StyioAST*
parse_set(StyioContext& context);

StyioAST*
parse_struct(StyioContext& context);

/*
  =================
    Basic Operation
  =================
*/

/*
  parse_size_of
*/
SizeOfAST*
parse_size_of(StyioContext& context);

/*
  parse_call
*/
CallAST*
parse_call(
  StyioContext& context,
  NameAST* func_name
);

AttrAST*
parse_attr(
  StyioContext& context
);

StyioAST*
parse_chain_of_call(
  StyioContext& context,
  StyioAST* callee
);

/*
  parse_bin_rhs
*/
StyioAST*
parse_binop_item(StyioContext& context);

/*
  parse_binop_rhs
*/
BinOpAST*
parse_binop_rhs(StyioContext& context, StyioAST* lhs_ast, TokenKind curr_tok);

/*
  parse_cond_item
*/
StyioAST*
parse_cond_item(StyioContext& context);

/*
  parse_cond: parse conditional expressions

  The following operators can be handled by parse_cond():
  >  Greater_Than
  <  Less_Than
  >= Greater_Than_Equal
  <= Less_Than_Equal
  == Eqaul
  != Not_Equal
  && Logic_AND
  ⊕ Logic_XOR
  || Logic_OR
*/
CondAST*
parse_cond(StyioContext& context);

/*
  parse_cond_flow
*/
StyioAST*
parse_cond_flow(StyioContext& context);

/*
  parse_list_op
*/
StyioAST*
parse_list_op(StyioContext& context, StyioAST* theList);

/*
  parse_var_tuple
*/
VarTupleAST*
parse_var_tuple(StyioContext& context);

/*
  parse_loop_or_iter
*/
StyioAST*
parse_loop_or_iter(StyioContext& context, StyioAST* collection);

/*
  parse_list_or_loop
*/
StyioAST*
parse_list_or_loop(StyioContext& context);

/*
  parse_loop
*/
StyioAST*
parse_loop(StyioContext& context, char& cur_char);

/*
  parse_simple_value
*/
StyioAST*
parse_value_expr(StyioContext& context);

/*
  parse_expr
*/
StyioAST*
parse_expr(StyioContext& context);

/*
  parse_resources
*/
ResourceAST*
parse_resources(StyioContext& context);

/*
  parse_pipeline
*/
StyioAST*
parse_template(StyioContext& context);

/*
  parse_read_file
*/
StyioAST*
parse_read_file(StyioContext& context, NameAST* id_ast);

/*
  parse_one_or_many_repr
*/
StyioAST*
parse_one_or_many_repr(StyioContext& context);

/*
  parse_print
*/
StyioAST*
parse_print(StyioContext& context);

/*
  parse_panic
*/
StyioAST*
parse_panic(StyioContext& context);

/*
  parse_stmt
*/
StyioAST*
parse_stmt(StyioContext& context);

/*
  parse_ext_elem
*/
string
parse_ext_elem(StyioContext& context);

/*
  parse_ext_pack

  Dependencies should be written like a list of paths
  like this -> ["ab/c", "x/yz"]

  // 1. The dependencies should be parsed before any domain
  (statement/expression).
  // 2. The left square bracket `[` is only eliminated after entering this
  function (parse_ext_pack)
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
ExtPackAST*
parse_ext_pack(StyioContext& context);

/*
  parse_cases
*/
CasesAST*
parse_cases(StyioContext& context);

/*
  parse_block
*/
StyioAST*
parse_block(StyioContext& context);

ForwardAST*
parse_forward(StyioContext& context, bool is_func = false);

BackwardAST*
parse_backward(StyioContext& context, bool is_func = false);

ExtractorAST*
parse_tuple_operations(StyioContext& context, TupleAST* the_tuple);

CODPAST*
parse_codp(StyioContext& context, CODPAST* prev_op = nullptr);

MainBlockAST*
parse_main_block(StyioContext& context);

#endif