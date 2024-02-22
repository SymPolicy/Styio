#pragma once
#ifndef STYIO_PARSER_H_
#define STYIO_PARSER_H_

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

/*
  Context ~ ForwardAST
*/
class StyioContext;

class StyioContext
{
private:
  size_t curr_pos; /* current position */

  string file_name;
  string code;
  vector<pair<size_t, size_t>> line_seps; /* line separations */

  shared_ptr<StyioAST> ast;
  unordered_map<string, shared_ptr<StyioAST>> constants;
  unordered_map<string, shared_ptr<StyioAST>> variables;

  shared_ptr<StyioContext> parent;
  vector<shared_ptr<StyioContext>> children;

public:
  StyioContext(
    const string& file_name,
    const string& code_text,
    vector<pair<size_t, size_t>> line_seps
  ) :
      file_name(file_name),
      code(code_text),
      line_seps(line_seps),
      curr_pos(0) {
  }

  StyioContext(
    StyioContext* parent
  ) :
      parent(parent) {
    code = parent->get_code();
    curr_pos = parent->get_curr_pos();
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

  /* Tree: isRoot() */
  bool isRootCtx() {
    if (parent) {
      return true;
    }
    else {
      return false;
    }
  }

  /* Tree: get_child() */
  shared_ptr<StyioContext>
  get_child() {
    return make_shared<StyioContext>(this);
  }

  /* Get Current Character */
  char& get_curr_char() {
    return code.at(curr_pos);
  }

  size_t find_line_index(
    int p = -1
  ) {
    const size_t total_lines = line_seps.size();
    size_t line_index = total_lines / 2;

    if (p < 0)
      p = curr_pos;

    cout << "find_line_index(), at pos: " << p << "\ninitial: line [" << line_index << "]" << endl;

    while (
      p < line_seps[line_index].first
      || p > (line_seps[line_index].first + line_seps[line_index].second)
    ) {
      cout << "[" << line_index << "] is ";
      if (p < line_seps[line_index].first) {
        line_index = line_index / 2;
        cout << "too large, go to: [" << line_index << "]" << endl;
      }
      else {
        line_index = (line_index + total_lines) / 2;
        cout << "too small, go to: [" << line_index << "]" << endl;
      }
    }

    cout << "result: [" << line_index << "]" << endl;

    return line_index;
  }

  string label_cur_line(
    int start = -1
  ) {
    string output("\n");

    if (start < 0)
      start = curr_pos;

    size_t lindex = find_line_index(start);
    size_t offset = curr_pos - line_seps[lindex].first;

    output += "File \"" + file_name + "\", Line " + std::to_string(lindex) + ":\n\n";
    output += code.substr(line_seps[lindex].first, line_seps[lindex].second) + "\n";
    output += std::string(offset, ' ')
              + std::string(line_seps[lindex].second - offset, '^')
              + "\n";

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
  bool peak_check(int steps, char value) {
    return (code.at(curr_pos + steps) == value);
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
  bool check_drop_panic(char value) {
    if (check(value)) {
      move(1);
      return true;
    }

    string errmsg = string("Expecting: ") + value + "\n" + "But Got: " + char(get_curr_char()) + "\n";
    throw StyioSyntaxError(errmsg);
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
          string errmsg = string("Expecting: ") + char(value) + "\n" + "But Got: " + get_curr_char();
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
          string errmsg = string("Expecting: ") + value + "\n" + "But Got: " + code.substr(curr_pos, value.size());
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
          string errmsg = string("Expecting: ") + value + "\n" + "But Got: " + get_curr_char();
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

  /* Check Binary Operator */
  bool check_binop() {
    if (code.at(curr_pos) == '+' || code.at(curr_pos) == '-' || code.at(curr_pos) == '*' || code.at(curr_pos) == '%') {
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

    return false;
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
unique_ptr<IdAST>
parse_id(shared_ptr<StyioContext> context);

/*
  =================
    Scalar Value
  =================
*/

/*
  parse_int
*/
unique_ptr<IntAST>
parse_int(shared_ptr<StyioContext> context);

/*
  parse_int_or_float
*/
unique_ptr<StyioAST>
parse_int_or_float(shared_ptr<StyioContext> context, char& cur_char);

/*
  parse_string
*/
unique_ptr<StringAST>
parse_string(shared_ptr<StyioContext> context);

/*
  parse_fmt_str
*/
unique_ptr<FmtStrAST>
parse_fmt_str(shared_ptr<StyioContext> context);

/*
  parse_path
*/
unique_ptr<StyioAST>
parse_path(shared_ptr<StyioContext> context);

/*
  parse_fill_arg
*/
shared_ptr<ArgAST>
parse_argument(shared_ptr<StyioContext> context);

unique_ptr<StyioAST>
parse_tuple(shared_ptr<StyioContext> context);

unique_ptr<StyioAST>
parse_list(shared_ptr<StyioContext> context);

unique_ptr<StyioAST>
parse_set(shared_ptr<StyioContext> context);

/*
  =================
    Basic Operation
  =================
*/

/*
  parse_size_of
*/
unique_ptr<SizeOfAST>
parse_size_of(shared_ptr<StyioContext> context);

/*
  parse_call
*/
unique_ptr<CallAST>
parse_call(
  shared_ptr<StyioContext> context, 
  unique_ptr<IdAST> func_name
);

/*
  parse_bin_rhs
*/
shared_ptr<StyioAST>
parse_item_for_binop(shared_ptr<StyioContext> context);

/*
  parse_binop_rhs
*/
unique_ptr<BinOpAST>
parse_binop_rhs(shared_ptr<StyioContext> context, shared_ptr<StyioAST> lhs_ast);

/*
  parse_item_for_cond
*/
unique_ptr<StyioAST>
parse_item_for_cond(shared_ptr<StyioContext> context);

/*
  parse_cond
*/
unique_ptr<CondAST>
parse_cond(shared_ptr<StyioContext> context);

/*
  parse_cond_flow
*/
unique_ptr<StyioAST>
parse_cond_flow(shared_ptr<StyioContext> context);

/*
  parse_list_op
*/
unique_ptr<StyioAST>
parse_list_op(shared_ptr<StyioContext> context, unique_ptr<StyioAST> theList);

/*
  parse_var_tuple
*/
shared_ptr<VarTupleAST>
parse_var_tuple(shared_ptr<StyioContext> context);

/*
  parse_loop_or_iter
*/
unique_ptr<StyioAST>
parse_loop_or_iter(shared_ptr<StyioContext> context, unique_ptr<StyioAST> collection);

/*
  parse_list_or_loop
*/
unique_ptr<StyioAST>
parse_list_or_loop(shared_ptr<StyioContext> context);

/*
  parse_loop
*/
unique_ptr<StyioAST>
parse_loop(shared_ptr<StyioContext> context, char& cur_char);

/*
  parse_simple_value
*/
unique_ptr<StyioAST>
parse_num_val(shared_ptr<StyioContext> context);

/*
  parse_expr
*/
unique_ptr<StyioAST>
parse_expr(shared_ptr<StyioContext> context);

/*
  parse_resources
*/
unique_ptr<ResourceAST>
parse_resources(shared_ptr<StyioContext> context);

/*
  parse_bind_final
*/
unique_ptr<FinalBindAST>
parse_bind_final(shared_ptr<StyioContext> context, unique_ptr<IdAST> id_ast);

/*
  parse_pipeline
*/
unique_ptr<StyioAST>
parse_func(shared_ptr<StyioContext> context);

/*
  parse_read_file
*/
unique_ptr<StyioAST>
parse_read_file(shared_ptr<StyioContext> context, IdAST* id_ast);

/*
  parse_one_or_many_repr
*/
unique_ptr<StyioAST>
parse_one_or_many_repr(shared_ptr<StyioContext> context);

/*
  parse_print
*/
unique_ptr<StyioAST>
parse_print(shared_ptr<StyioContext> context);

/*
  parse_panic
*/
unique_ptr<StyioAST>
parse_panic(shared_ptr<StyioContext> context);

/*
  parse_stmt
*/
unique_ptr<StyioAST>
parse_stmt(shared_ptr<StyioContext> context);

/*
  parse_ext_elem
*/
string
parse_ext_elem(shared_ptr<StyioContext> context, char& cur_char);

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
unique_ptr<ExtPackAST>
parse_ext_pack(shared_ptr<StyioContext> context, char& cur_char);

/*
  parse_cases
*/
unique_ptr<CasesAST>
parse_cases(shared_ptr<StyioContext> context);

/*
  parse_block
*/
unique_ptr<StyioAST>
parse_block(shared_ptr<StyioContext> context);

unique_ptr<ForwardAST>
parse_forward(shared_ptr<StyioContext> context, bool ispipe = false);

shared_ptr<MainBlockAST>
parse_main_block(shared_ptr<StyioContext> context);

#endif