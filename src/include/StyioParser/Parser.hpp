#pragma once
#ifndef STYIO_PARSER_H_
#define STYIO_PARSER_H_

using std::string;
using std::vector;
using std::unordered_map;

using std::cout;
using std::endl;

using std::unique_ptr;
using std::shared_ptr;
using std::make_unique;
using std::make_shared;

/*
  Context ~ ForwardAST
*/
class StyioContext;

class StyioContext {
  private:
    string code;
    int pos;

    shared_ptr<StyioAST> ast;
    unordered_map<string, shared_ptr<StyioAST>> constants;
    unordered_map<string, shared_ptr<StyioAST>> variables;

    shared_ptr<StyioContext> parent;
    vector<shared_ptr<StyioContext>> children;

  public:
    StyioContext(
      const string& text) :
      code(text), 
      pos(0) {
        /* Construction */ }

    StyioContext(
      StyioContext* parent): 
      parent(parent) {
      code = parent -> getCode();
      pos = parent -> getPos(); }

    /* Get `code` */
    const string&  getCode() {
      return code; }

    /* Get `pos` */
    int getPos() {
      return pos; }

    /* Tree: isRoot() */
    bool isRootCtx() {
      if (parent) { return true; }
      else { return false; } }

    /* Tree: getChild() */
    shared_ptr<StyioContext> getChild() {
      return make_shared<StyioContext>(this); }

    /* Get Current Character */
    char& get_cur_char() {
      return code.at(pos); }

    // + n => move forward n steps
    // - n => move backward n steps
    void move(
      int steps
    ) {
      pos += steps; }

    /* Check Value */
    bool check(
      char value
    ) {
      return (code.at(pos)) == value; }

    /* Check Value */
    bool check(
      const string& value
    ) {
      return (code.substr(pos, value.size())) == value; 
    }

    /* Move Until */
    void move_until(
      char value
    ) {
      while (not check(value)) {
        move(1); }
    }

    /* Check & Drop */
    bool check_drop(
      char value
    ) {
      if (check(value)) {
        move(1);
        return true; }
      else { 
        return false; }
    }

    /* Check & Drop */
    bool check_drop(
      const string& value
    ) {
      if (check(value)) {
        move(value.size());
        return true; }
      else { 
        return false; }
    }

    /* Find & Drop */
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

    /* Find & Drop */
    bool find_drop(
      string value
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

    /* Pass Over */
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

    /* Pass Over */
    void pass_over (
      const string& value
    ) {
      /* ! No Boundary Check ! */
      while (true) {
        if (check(value)) {
          move(value.size());
          break; }
        else {
          move(1); } }
    }

    /* Peak Check */
    bool peak_check(
      int steps,
      char value
    ) {
      return (code.at(pos + steps) == value);
    }
    
    bool peak_isdigit(
      int steps
    ) {
      return isdigit(code.at(pos + steps));
    }

    /* Drop White Spaces */
    void drop_white_spaces() {
      while (check(' ')) {
        move(1); } 
    }

    /* Drop Spaces */
    void drop_all_spaces() {
      while (isspace(code.at(pos))) {
        move(1); } 
    }

    /* Drop Spaces & Comments */
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

    /* Match(Next) -> Panic */
    bool match_panic(
      char value
    ) {
      if (check(value)) {
        move(1);
        return true; }
      
      string errmsg = string("Expecting: ") + value + "\n" 
        + "But Got: " + char(get_cur_char()) + "\n";
      throw StyioSyntaxError(errmsg);
    }

    /* Find & Drop -> Panic */
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
            string errmsg = string("Expecting: ") + char(value) + "\n" 
              + "But Got: " + get_cur_char();
            throw StyioSyntaxError(errmsg); } } }
    }

    /* Find & Drop -> Panic */
    bool find_panic(
      const string& value
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
            string errmsg = string("Expecting: ") + value + "\n" 
              + "But Got: " + get_cur_char();
            throw StyioSyntaxError(errmsg); } } }
    }

    /* Check isalpha or _ */
    bool check_isal_() {
      return isalpha(code.at(pos)) || (code.at(pos) == '_');
    }

    /* Check isalpha or isnum or _ */
    bool check_isalnum_() {
      return isalnum(code.at(pos)) || (code.at(pos) == '_');
    }

    /* Check isdigit */
    bool check_isdigit() {
      return isdigit(code.at(pos));
    }

    /* Check Binary Operator */
    bool check_binop() {
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
unique_ptr<IdAST> parse_id (
  shared_ptr<StyioContext> context
);

/*
  =================
    Scalar Value
  =================
*/

/*
  parse_int
*/
unique_ptr<IntAST> parse_int 
(
  shared_ptr<StyioContext> context
);

/*
  parse_int_or_float
*/
unique_ptr<StyioAST> parse_int_or_float 
(
  shared_ptr<StyioContext> context, 
  char& cur_char
);

/*
  parse_str
*/
unique_ptr<StringAST> parse_str (
  shared_ptr<StyioContext> context
);

/*
  parse_fmt_str
*/
unique_ptr<FmtStrAST> parse_fmt_str (
  shared_ptr<StyioContext> context
);

/*
  parse_path_or_link
*/
unique_ptr<StyioAST> parse_path_or_link (
  shared_ptr<StyioContext> context
);

/*
  parse_fill_arg
*/
shared_ptr<ArgAST> parse_argument (
  shared_ptr<StyioContext> context
);

unique_ptr<StyioAST> parse_tuple (
  shared_ptr<StyioContext> context
);

unique_ptr<StyioAST> parse_list (
  shared_ptr<StyioContext> context
);

unique_ptr<StyioAST> parse_set (
  shared_ptr<StyioContext> context
);

/*
  =================
    Basic Operation
  =================
*/

/*
  parse_size_of
*/
unique_ptr<SizeOfAST> parse_size_of 
(
  shared_ptr<StyioContext> context
);

/*
  parse_call
*/
unique_ptr<StyioAST> parse_call 
(
  shared_ptr<StyioContext> context
);

/*
  parse_bin_rhs
*/
shared_ptr<StyioAST> parse_item_for_binop (
  shared_ptr<StyioContext> context
);

/*
  parse_binop_rhs
*/
unique_ptr<BinOpAST> parse_binop_rhs (
  shared_ptr<StyioContext> context, 
  shared_ptr<StyioAST> lhs_ast
);

/*
  parse_item_for_cond
*/
unique_ptr<StyioAST> parse_item_for_cond (
  shared_ptr<StyioContext> context
);

/*
  parse_cond
*/
unique_ptr<CondAST> parse_cond (
  shared_ptr<StyioContext> context
);

/*
  parse_cond_flow
*/
unique_ptr<StyioAST> parse_cond_flow (
  shared_ptr<StyioContext> context
);

/*
  parse_list_op
*/
unique_ptr<StyioAST> parse_list_op (
  shared_ptr<StyioContext> context,
  unique_ptr<StyioAST> theList
);

/*
  parse_var_tuple
*/
shared_ptr<VarTupleAST> parse_var_tuple (
  shared_ptr<StyioContext> context
);

/*
  parse_loop_or_iter
*/
unique_ptr<StyioAST> parse_loop_or_iter (
  shared_ptr<StyioContext> context,
  unique_ptr<StyioAST> collection
);

/*
  parse_list_or_loop
*/
unique_ptr<StyioAST> parse_list_or_loop (
  shared_ptr<StyioContext> context
);

/*
  parse_loop
*/
unique_ptr<StyioAST> parse_loop (shared_ptr<StyioContext> context,char& cur_char);

/*
  parse_simple_value
*/
unique_ptr<StyioAST> parse_num_val (
  shared_ptr<StyioContext> context
);

/*
  parse_expr
*/
unique_ptr<StyioAST> parse_expr (
  shared_ptr<StyioContext> context
);

/*
  parse_resources
*/
unique_ptr<ResourceAST> parse_resources (
  shared_ptr<StyioContext> context
);

/*
  parse_bind_final
*/
unique_ptr<FinalBindAST> parse_bind_final (
  shared_ptr<StyioContext> context, 
  unique_ptr<IdAST> id_ast
);

/*
  parse_pipeline
*/
unique_ptr<StyioAST> parse_func (
  shared_ptr<StyioContext> context
);

/*
  parse_read_file
*/
unique_ptr<StyioAST> parse_read_file (
  shared_ptr<StyioContext> context, 
  IdAST* id_ast
);

/*
  parse_one_or_many_repr
*/
unique_ptr<StyioAST> parse_one_or_many_repr (
  shared_ptr<StyioContext> context
);

/*
  parse_print
*/
unique_ptr<StyioAST> parse_print (
  shared_ptr<StyioContext> context
);

/*
  parse_panic
*/
unique_ptr<StyioAST> parse_panic (
  shared_ptr<StyioContext> context
);

/*
  parse_stmt
*/
unique_ptr<StyioAST> parse_stmt (
  shared_ptr<StyioContext> context
);

/*
  parse_ext_elem
*/
string parse_ext_elem(shared_ptr<StyioContext> context,char& cur_char);

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
unique_ptr<ExtPackAST> parse_ext_pack (shared_ptr<StyioContext> context,char& cur_char);

/*
  parse_cases
*/
unique_ptr<CasesAST> parse_cases (
  shared_ptr<StyioContext> context
);

/*
  parse_block
*/
unique_ptr<StyioAST> parse_block (
  shared_ptr<StyioContext> context
);

unique_ptr<ForwardAST> parse_forward (
  shared_ptr<StyioContext> context,
  bool ispipe = false
);


shared_ptr<MainBlockAST> parse_main_block (
  shared_ptr<StyioContext> context
);

#endif