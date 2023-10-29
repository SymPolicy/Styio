#pragma once
#ifndef STYIO_PARSER_H_
#define STYIO_PARSER_H_

class StyioContext;

class StyioContext {
  private:
    std::shared_ptr<StyioContext> parent;
    std::vector<std::shared_ptr<StyioContext>> children;

  public:
    std::string code;
    int pos;

    StyioContext(
      const std::string& text) :
      code(text), 
      pos(0) {
        /* Construction */
      }

    StyioContext(
      std::shared_ptr<StyioContext> parent): 
      parent(parent) {
        /* Construction */
      }

    bool isRootCtx() {
      if (parent) { return true; }
      else { return false; } }

    char& get_cur_char() {
      return code.at(pos); }

    /* 
      + n => move forward n steps
      - n => move backward n steps
    */
    void move(
      int steps
    ) {
      pos += steps; }

    bool check(
      char value
    ) {
      return (code.at(pos)) == value; }

    bool check(
      const std::string& value
    ) {
      return (code.substr(pos, value.size())) == value; 
    }

    void move_until(
      char value
    ) {
      while (not check(value)) {
        move(1); }
    }

    bool check_drop(
      char value
    ) {
      if (check(value)) {
        move(1);
        return true; }
      else { 
        return false; }
    }

    bool check_drop(
      const std::string& value
    ) {
      if (check(value)) {
        move(value.size());
        return true; }
      else { 
        return false; }
    }

    bool find_drop(
      char value
    ) {
      /* ! No Boundary Check ! */
      while (true) {
        if (isspace(get_cur_char())) {
          move(1); }
        else if (check("//")) {
          pass_over('\n'); }
        else if (check("/*")) {
          pass_over("*/"); }
        else {
          if (check(value)) {
            move(1);
            return true; }
          else {
            return false; } } }

      return false;
    }

    bool find_drop(
      std::string value
    ) {
      /* ! No Boundary Check ! */
      while (true) {
        if (isspace(get_cur_char())) {
          move(1); }
        else if (check("//")) {
          pass_over('\n'); }
        else if (check("/*")) {
          pass_over("*/"); }
        else {
          if ((code.substr(pos, value.size())) == value) {
            move(value.size());
            return true; }
          else {
            return false; } } }
    }

    void pass_over (
      char value
    ) {
      /* ! No Boundary Check ! */
      while (true) {
        if (check(value)) {
          move(1);
          break; }
        else {
          move(1); } }
    }

    void pass_over (
      const std::string& value
    ) {
      /* ! No Boundary Check ! */
      while (true) {
        if (check(value)) {
          move(value.size());
          break; }
        else {
          move(1); } }
    }

    bool peak_check(
      int steps,
      char value
    ) {
      return (code.at(pos + steps) == value);
    }

    void drop_white_spaces() {
      while (check(' ')) {
        move(1); } 
    }

    void drop_all_spaces() {
      while (isspace(code.at(pos))) {
        move(1); } 
    }

    void drop_all_spaces_comments() {
      /* ! No Boundary Check ! */
      while (true) {
        if (isspace(code.at(pos))) {
          move(1); }
        else if (check("//")) {
          pass_over('\n'); }
        else if (check("/*")) {
          pass_over("*/"); }
        else {
          break; } }
    }

    bool match_panic(
      char value
    ) {
      if (check(value)) {
        move(1);
        return true; }
      
      std::string errmsg = std::string("Expecting: ") + value + "\n" 
        + "But Got: " + char(get_cur_char()) + "\n";
      throw StyioSyntaxError(errmsg);
    }

    bool find_drop_panic(
      char value
    ) {
      /* ! No Boundary Check ! */
      while (true) {
        if (isspace(get_cur_char())) {
          move(1); }
        else if (check("//")) {
          pass_over('\n'); }
        else if (check("/*")) {
          pass_over("*/"); }
        else {
          if (check(value)) {
            move(1);
            return true; }
          else {
            std::string errmsg = std::string("Expecting: ") + char(value) + "\n" 
              + "But Got: " + get_cur_char();
            throw StyioSyntaxError(errmsg); } } }
    }

    bool find_panic(
      const std::string& value
    ) {
      /* ! No Boundary Check ! */
      while (true) {
        if (isspace(get_cur_char())) {
          move(1); }
        else if (check("//")) {
          pass_over('\n'); }
        else if (check("/*")) {
          pass_over("*/"); }
        else {
          if (check(value)) {
            move(value.size());
            return true; }
          else {
            std::string errmsg = std::string("Expecting: ") + value + "\n" 
              + "But Got: " + get_cur_char();
            throw StyioSyntaxError(errmsg); } } }
    }

    bool check_binary() {
      drop_all_spaces_comments();

      if (code.at(pos) == '+' || code.at(pos) == '-' 
        || code.at(pos) == '*' || code.at(pos) == '%') { 
        return true; }
      else if (code.at(pos) == '/') {
        /* Comments */
        if ((code.at(pos + 1)) == '*'
          || code.at(pos + 1) == '/') { 
          return false; } 
        else { 
          return true; } }

      return false;
    }
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
  std::shared_ptr<StyioContext> context
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
  std::shared_ptr<StyioContext> context
);

/*
  parse_int_or_float
*/
std::unique_ptr<StyioAST> parse_int_or_float 
(
  std::shared_ptr<StyioContext> context, 
  char& cur_char
);

/*
  parse_str
*/
std::unique_ptr<StringAST> parse_str (
  std::shared_ptr<StyioContext> context
);

/*
  parse_fmt_str
*/
std::unique_ptr<FmtStrAST> parse_fmt_str (
  std::shared_ptr<StyioContext> context
);

/*
  parse_path_or_link
*/
std::unique_ptr<StyioAST> parse_path_or_link (
  std::shared_ptr<StyioContext> context
);

/*
  parse_fill_arg
*/
std::unique_ptr<FillArgAST> parse_fill_arg (
  std::shared_ptr<StyioContext> context
);

std::unique_ptr<StyioAST> parse_tuple (
  std::shared_ptr<StyioContext> context
);

std::unique_ptr<StyioAST> parse_list (
  std::shared_ptr<StyioContext> context
);

std::unique_ptr<StyioAST> parse_set (
  std::shared_ptr<StyioContext> context
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
  std::shared_ptr<StyioContext> context
);

/*
  parse_call
*/
std::unique_ptr<StyioAST> parse_call 
(
  std::shared_ptr<StyioContext> context
);

/*
  parse_bin_rhs
*/
std::unique_ptr<StyioAST> parse_item_for_binop (
  std::shared_ptr<StyioContext> context
);

/*
  parse_binop_rhs
*/
std::unique_ptr<BinOpAST> parse_binop_rhs (
  std::shared_ptr<StyioContext> context, 
  std::unique_ptr<StyioAST> lhs_ast
);

/*
  parse_item_for_cond
*/
std::unique_ptr<StyioAST> parse_item_for_cond (
  std::shared_ptr<StyioContext> context
);

/*
  parse_cond
*/
std::unique_ptr<CondAST> parse_cond (
  std::shared_ptr<StyioContext> context
);

/*
  parse_cond_flow
*/
std::unique_ptr<StyioAST> parse_cond_flow (
  std::shared_ptr<StyioContext> context
);

/*
  parse_list_op
*/
std::unique_ptr<StyioAST> parse_list_op (
  std::shared_ptr<StyioContext> context,
  std::unique_ptr<StyioAST> theList
);

/*
  parse_vars_tuple
*/
std::unique_ptr<VarTupleAST> parse_vars_tuple (
  std::shared_ptr<StyioContext> context
);

/*
  parse_loop_or_iter
*/
std::unique_ptr<StyioAST> parse_loop_or_iter (
  std::shared_ptr<StyioContext> context,
  std::unique_ptr<StyioAST> collection
);

/*
  parse_list_or_loop
*/
std::unique_ptr<StyioAST> parse_list_or_loop (
  std::shared_ptr<StyioContext> context
);

/*
  parse_loop
*/
std::unique_ptr<StyioAST> parse_loop (std::shared_ptr<StyioContext> context,char& cur_char);

/*
  parse_simple_value
*/
std::unique_ptr<StyioAST> parse_num_val (
  std::shared_ptr<StyioContext> context
);

/*
  parse_expr
*/
std::unique_ptr<StyioAST> parse_expr (
  std::shared_ptr<StyioContext> context
);

/*
  parse_resources
*/
std::unique_ptr<ResourceAST> parse_resources (
  std::shared_ptr<StyioContext> context
);

/*
  parse_bind_final
*/
std::unique_ptr<FinalBindAST> parse_bind_final (
  std::shared_ptr<StyioContext> context, 
  std::unique_ptr<IdAST> id_ast
);

/*
  parse_pipeline
*/
std::unique_ptr<StyioAST> parse_pipeline (
  std::shared_ptr<StyioContext> context
);

/*
  parse_read_file
*/
std::unique_ptr<StyioAST> parse_read_file (
  std::shared_ptr<StyioContext> context, 
  IdAST* id_ast
);

/*
  parse_one_or_many_repr
*/
std::unique_ptr<StyioAST> parse_one_or_many_repr (
  std::shared_ptr<StyioContext> context
);

/*
  parse_print
*/
std::unique_ptr<StyioAST> parse_print (
  std::shared_ptr<StyioContext> context
);

/*
  parse_panic
*/
std::unique_ptr<StyioAST> parse_panic (
  std::shared_ptr<StyioContext> context
);

/*
  parse_stmt
*/
std::unique_ptr<StyioAST> parse_stmt (
  std::shared_ptr<StyioContext> context
);

/*
  parse_ext_elem
*/
std::string parse_ext_elem(std::shared_ptr<StyioContext> context,char& cur_char);

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
std::unique_ptr<ExtPackAST> parse_ext_pack (std::shared_ptr<StyioContext> context,char& cur_char);

/*
  parse_cases
*/
std::unique_ptr<StyioAST> parse_cases (
  std::shared_ptr<StyioContext> context
);

/*
  parse_block
*/
std::unique_ptr<StyioAST> parse_block (
  std::shared_ptr<StyioContext> context
);

std::unique_ptr<ForwardAST> parse_forward (
  std::shared_ptr<StyioContext> context,
  bool ispipe = false
);


std::unique_ptr<MainBlockAST> parse_main_block (std::string styio_code);

#endif