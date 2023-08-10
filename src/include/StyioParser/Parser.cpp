#include <string>
#include <tuple>
#include <vector>
#include <fstream>
#include <iostream>
#include <memory>
#include <filesystem>

#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "../StyioAST/AST.hpp"
#include "../StyioUtil/Util.hpp"
#include "Parser.hpp"

/*
  Basic Utilities
*/

inline bool check_char (
  char& cur_char, 
  char value) {
  return cur_char == value;
}

inline bool check_symbol (
  struct StyioCodeContext* code,
  std::string value) {
  // std::cout << "check_symbol()\n" << "Expecting: " << value << "\n" << "But Got: " << code -> text.substr(code -> cursor, value.length()) << std::endl;
  return (code -> text.substr(code -> cursor, value.length())) == value;
}

inline void move_to_the_next_char (
  struct StyioCodeContext* code,
  char& cur_char
) {
  code -> cursor += 1;
  cur_char = code -> text.at(code -> cursor);
}

inline void move_forward (
  struct StyioCodeContext* code,
  char& cur_char,
  int steps) {
  code -> cursor += steps;
  cur_char = code -> text.at(code -> cursor);
}

inline void move_backward (
  struct StyioCodeContext* code,
  char& cur_char,
  int steps) {
  code -> cursor -= steps;
  cur_char = code -> text.at(code -> cursor);
}

inline bool check_binop_token (
  struct StyioCodeContext* code) {
  int pos = code -> cursor;
  while (isspace((code -> text.at(pos)))) {
    pos += 1; }

  char the_char = code -> text.at(pos);
  if (the_char == '+' || the_char == '-' || the_char == '*' || the_char == '%') { 
    return true; }
  else if (the_char == '/') {
    if ((code -> text.at(pos + 1)) == '*') { 
      return false; } 
    else { 
      return true; }
  }

  return false;
}

/*
  Advanced Utilities
    peak_next_char()

    drop_white_spaces()
    drop_spaces()
    
    move_until_char()
    
    pass_over_char()
    pass_over_symbol()
*/

bool peak_next_char (
  struct StyioCodeContext* code,
  char value) {
  int pos = code -> cursor;
  while (isspace((code -> text.at(pos)))) {
    pos += 1; }

  return check_char(code -> text.at(pos), value);
}

void drop_white_spaces (
  struct StyioCodeContext* code,
  char& cur_char) {
  while (check_char(cur_char, ' ')) {
    move_to_the_next_char(code, cur_char); }
}

void drop_spaces (
  struct StyioCodeContext* code,
  char& cur_char) {
  while (isspace(cur_char)) {
    move_to_the_next_char(code, cur_char); }
}

void move_until_char (
  struct StyioCodeContext* code,
  char& cur_char,
  char value) {
  /*
    Danger: No bound check!
  */
  while (true) 
  {
    if (check_char(cur_char, value)) {
      break; }
    else {
      move_to_the_next_char(code, cur_char); }
  }
}

void pass_over_char (
  struct StyioCodeContext* code,
  char& cur_char,
  char value) {
  /*
    Danger: No bound check!
  */
  while (true) 
  {
    if (check_char(cur_char, value)) {
      move_to_the_next_char(code, cur_char);
      break; }
    else {
      move_to_the_next_char(code, cur_char); }
  }
}

void pass_over_symbol (
  struct StyioCodeContext* code,
  char& cur_char,
  std::string value) {
  /*
    Danger!
  */
  while (true) 
  {
    while (not check_char(cur_char, value.at(0))) {
      move_to_the_next_char(code, cur_char); }

    if (check_symbol(code, value)) {
      move_forward(code, cur_char, value.length());
      break; }
    else {
      move_forward(code, cur_char, value.length()); }

    // std::cout << "pass_over_symbol() " << cur_char << std::endl;
  }
}

void drop_spaces_and_comments (
  struct StyioCodeContext* code,
  char& cur_char) {
  /*
    Danger: No bound check!
  */
  while (true) 
  {
    if (isspace(cur_char)) {
      move_to_the_next_char(code, cur_char); }
    else if (check_symbol(code, "//")) {
      pass_over_char(code, cur_char, '\n'); }
    else if (check_symbol(code, "/*")) {
      pass_over_symbol(code, cur_char, "*/"); }
    else {
      break; } 

    // std::cout << cur_char << std::endl;
  }
}

/*
  Complex Utilities:
    find_and_drop_char
    find_and_drop_char_panic
*/

inline bool check_and_drop_char (
  struct StyioCodeContext* code, 
  char& cur_char, 
  char value) {
  if (check_char(cur_char, value)) {
    move_to_the_next_char(code, cur_char);
    return true; }
  else { 
    return false; }
}

inline bool check_and_drop_symbol (
  struct StyioCodeContext* code, 
  char& cur_char, 
  std::string value) {
  if ((code -> text.substr(code -> cursor, value.length())) == value) {
    move_forward(code, cur_char, value.length());
    return true; }
  else { 
    return false; }
}

bool find_and_drop_char (
  struct StyioCodeContext* code,
  char& cur_char,
  char value) {
  /*
    Danger: No bound check!
  */
  while (true)
  {
    if (isspace(cur_char)) {
      move_to_the_next_char(code, cur_char); }
    else if (check_symbol(code, "//")) {
      pass_over_char(code, cur_char, '\n'); }
    else if (check_symbol(code, "/*")) {
      pass_over_symbol(code, cur_char, "*/"); }
    else {
      if (check_char(cur_char, value)) {
        move_to_the_next_char(code, cur_char);
        return true; }
      else {
        return false; }
    }
  }

  return false;
}

bool find_and_drop_symbol (
  struct StyioCodeContext* code,
  char& cur_char,
  std::string value) {
  /*
    Danger: No bound check!
  */
  while (true)
  {
    if (isspace(cur_char)) {
      move_to_the_next_char(code, cur_char); }
    else if (check_symbol(code, "//")) {
      pass_over_char(code, cur_char, '\n'); }
    else if (check_symbol(code, "/*")) {
      pass_over_symbol(code, cur_char, "*/"); }
    else {
      if (check_symbol(code, value)) {
        move_forward(code, cur_char, value.length());
        return true; }
      else { 
        return false; }
    }
  }

  return false;
}

bool find_and_drop_char_panic (
  struct StyioCodeContext* code,
  char& cur_char,
  char value) {
  /*
    Danger: No bound check!
  */
  while (true)
  {
    if (isspace(cur_char)) {
      move_to_the_next_char(code, cur_char); }
    else if (check_symbol(code, "//")) {
      pass_over_char(code, cur_char, '\n'); }
    else if (check_symbol(code, "/*")) {
      pass_over_symbol(code, cur_char, "*/"); }
    else {
      if (check_char(cur_char, value)) {
        move_to_the_next_char(code, cur_char);
        return true; }
      else {
        std::string errmsg = std::string("Expecting: ") + char(value) + "\n" 
          + "But Got: " + char(cur_char);
        throw StyioSyntaxError(errmsg); }
    } 
  }
}

bool find_and_drop_symbol_panic (
  struct StyioCodeContext* code,
  char& cur_char,
  std::string value) {
  /*
    Danger: No bound check!
  */
  while (true)
  {
    if (isspace(cur_char)) {
      move_to_the_next_char(code, cur_char); }
    else if (check_symbol(code, "//")) {
      pass_over_char(code, cur_char, '\n'); }
    else if (check_symbol(code, "/*")) {
      pass_over_symbol(code, cur_char, "*/"); }
    else {
      if ((code -> text.substr(code -> cursor, value.length())) == value) {
        move_forward(code, cur_char, value.length());
        return true; }
      else {
        std::string errmsg = std::string("Expecting: ") + value + "\n" 
          + "But Got: " + code -> text.substr(code -> cursor, value.length());
        throw StyioSyntaxError(errmsg); }
    }
  }
}

inline bool match_next_char_panic (
  struct StyioCodeContext* code, 
  char& cur_char,
  char value) {
  if (check_char(cur_char, value)) {
    move_to_the_next_char(code, cur_char);
    return true; }

  std::string errmsg = std::string("Expecting: ") + char(value) + "\n" 
    + "But Got " + char(cur_char) + "\n";
  throw StyioSyntaxError(errmsg);
}

inline void move_next_and_ignore (
  struct StyioCodeContext* code,
  char& cur_char) {
  move_to_the_next_char(code, cur_char);
  drop_spaces(code, cur_char);
}

/*
  =================
  - id
  
  - int
  - float

  - char
  - string
  =================
*/

std::unique_ptr<IdAST> parse_id (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::string idStr = "";

  /*
    Danger!
    when entering parse_id(), 
    the cur_char must be a valid character
    this line will include the next 1 character as part of id anyway!
  */
  do {
    idStr += cur_char;
    move_to_the_next_char(code, cur_char);
  } while (isalnum((cur_char)) || check_char(cur_char, '_'));

  return std::make_unique<IdAST>(idStr);
}

std::unique_ptr<IntAST> parse_int (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::string intStr = "";

  /*
    Danger!
    when entering parse_int(), 
    the cur_char must be a digit
    this line will drop the next 1 character anyway!
  */
  intStr += cur_char;
  move_to_the_next_char(code, cur_char);

  while (isdigit(cur_char)) {
    intStr += cur_char;
    move_to_the_next_char(code, cur_char); };

  return std::make_unique<IntAST>(intStr);
}

std::unique_ptr<StyioAST> parse_int_or_float (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output;

  std::string numStr = "";

  /*
    Danger!
    when entering parse_int_or_float(), 
    the cur_char must be a digit
    this line will drop the next 1 character anyway!
  */
  numStr += cur_char;
  move_to_the_next_char(code, cur_char);

  while (isdigit(cur_char)) {
    numStr += cur_char;
    move_to_the_next_char(code, cur_char); };

  if (check_and_drop_char(code, cur_char, '.')) {
    if (isdigit(cur_char)) {
      numStr += '.';

      while (isdigit(cur_char)) {
        numStr += cur_char;
        move_to_the_next_char(code, cur_char); }

      return std::make_unique<FloatAST>(numStr); }
    else {
      return std::make_unique<IntAST>(numStr); }
  } 

  return std::make_unique<IntAST>(numStr); 
}

std::unique_ptr<StringAST> parse_string (
  struct StyioCodeContext* code, 
  char& cur_char) {
  /*
    Danger!
    when entering parse_string(), 
    the cur_char must be "
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  std::string textStr = "";
  
  while (not check_char(cur_char, '\"')) {
    textStr += cur_char;
    move_to_the_next_char(code, cur_char); }

  // eliminate " at the end
  move_to_the_next_char(code, cur_char);

  return std::make_unique<StringAST>(textStr);
}

std::unique_ptr<StyioAST> parse_char_or_string (
  struct StyioCodeContext* code, 
  char& cur_char) {
  /*
    Danger!
    when entering parse_char_or_string(), 
    the cur_char must be '
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  std::string textStr = "";
  
  while (not check_char(cur_char, '\'')) {
    textStr += cur_char;
    move_to_the_next_char(code, cur_char); };

  // eliminate ' at the end
  move_to_the_next_char(code, cur_char);

  if (textStr.size() == 1) {
    return std::make_unique<CharAST>(textStr); }
  else {
    return std::make_unique<StringAST>(textStr); }
}

std::unique_ptr<StyioAST> parse_path_or_link (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output;

  /*
    Danger!
    when entering parse_path_or_link(), 
    the cur_char must be @
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  if (check_and_drop_char(code, cur_char, '(')) {
    if (check_char(cur_char, '\"')) {
      std::unique_ptr<StringAST> path = parse_string(code, cur_char);

      find_and_drop_char_panic(code, cur_char, ')');

      return std::make_unique<ExtPathAST>(std::move(path));
    }
    else
    {
      std::string errmsg = std::string("@(___) // Resource: Unexpected resource, starts with .:| ") + char(cur_char) + " |:.";
      throw StyioSyntaxError(errmsg);
    }
  }
  else
  {
    std::string errmsg = std::string("@(___) // Resource: Expecting ( at the start, but got .:| ") + char(cur_char) + " |:.";
    throw StyioSyntaxError(errmsg);
  };

  return output;
}

std::unique_ptr<TypeAST> parse_type (
  struct StyioCodeContext* code,
  char& cur_char) {
  std::string text = "";

  if (isalpha((cur_char)) || check_char(cur_char, '_')) {
    text += cur_char;
    move_to_the_next_char(code, cur_char); }
  else {
    std::string errmsg = std::string("parse_type() // Can't recognize ") + char(cur_char);
    throw StyioSyntaxError(errmsg); }

  while (isalnum((cur_char)) || check_char(cur_char, '_')) {
    text += cur_char;
    move_to_the_next_char(code, cur_char);
  }

  return std::make_unique<TypeAST>(text);
}

/*
  Basic Collection
  - typed_var
  - Fill (Variable Tuple)
  - Resources
*/

std::unique_ptr<StyioAST> parse_typed_var (
  struct StyioCodeContext* code,
  char& cur_char) {
  std::unique_ptr<IdAST> var = parse_id(code, cur_char);
  
  drop_white_spaces(code, cur_char);
  
  if (check_and_drop_char(code, cur_char, ':')) {
    drop_spaces(code, cur_char);

    return std::make_unique<TypedVarAST>(
      std::move(var),
      parse_type(code, cur_char)); }
  else {
    return var; }  
}

std::unique_ptr<VarsTupleAST> parse_vars_tuple (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::vector<std::unique_ptr<StyioAST>> vars;

  /*
    Danger!
    when entering parse_vars_tuple(), 
    the cur_char must be (
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  do {
    drop_spaces_and_comments(code, cur_char);

    if (check_and_drop_char(code, cur_char, ')')) {
      return std::make_unique<VarsTupleAST>(std::move(vars)); }
    else {
      if (check_and_drop_char(code, cur_char, '*')) { 
        if (check_and_drop_char(code, cur_char, '*')) {
          drop_white_spaces(code, cur_char);
          vars.push_back(std::move(std::make_unique<KwArgAST>(parse_id(code, cur_char)))); }
        else {
          vars.push_back(std::move(std::make_unique<ArgAST>(parse_id(code, cur_char)))); }
      }
      else{
        vars.push_back(std::move(parse_typed_var(code, cur_char))); }
    }
  } while (check_and_drop_char(code, cur_char, ','));

  drop_spaces_and_comments(code, cur_char);

  find_and_drop_char_panic(code, cur_char, ')');

  return std::make_unique<VarsTupleAST>(std::move(vars));
}

std::unique_ptr<ResourceAST> parse_resources (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<ResourceAST> output;

  std::vector<std::unique_ptr<StyioAST>> resources;

  /*
    Danger!
    when entering parse_resources(), 
    the cur_char must be @
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  if (check_and_drop_char(code, cur_char, '(')) {
    do {
      drop_spaces_and_comments(code, cur_char);

      std::unique_ptr<IdAST> varname = parse_id(code, cur_char);

      drop_spaces(code, cur_char);

      if (check_and_drop_char(code, cur_char, '<')) {
        match_next_char_panic(code, cur_char, '-'); }

      drop_spaces(code, cur_char);

      resources.push_back(std::make_unique<FinalBindAST>(
        std::move(varname), 
        parse_value(code, cur_char)));
    } while (check_and_drop_char(code, cur_char, ','));
    
    if (check_and_drop_char(code, cur_char, ')')) { }
    else {
      std::string errmsg = std::string("@(expr) // Expecting ) at the end, but got ") + char(cur_char) + "";
      throw StyioSyntaxError(errmsg); }

    output = std::make_unique<ResourceAST>(std::move(resources));
  }
  else {
    std::string errmsg = std::string("@(expr) // Expecting ( after @, but got ") + char(cur_char) + "";
    throw StyioSyntaxError(errmsg); }

  return output;
}

/*
  Expression
  - Value
  - Binary Comparison
*/

std::unique_ptr<StyioAST> parse_item_for_cond (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output;

  drop_spaces(code, cur_char);

  output = parse_value(code, cur_char);
  
  drop_spaces(code, cur_char);

  switch (cur_char)
  {
  case '=':
    {
      move_to_the_next_char(code, cur_char);

      if (check_char(cur_char, '=')) {
        move_to_the_next_char(code, cur_char);

        /*
          Equal
            expr == expr
        */

        // drop all spaces after ==
        drop_spaces(code, cur_char);
        
        output = std::make_unique<BinCompAST>(
          CompType::EQ,
          std::move(output),
          parse_value(code, cur_char));
      };
    }

    break;

  case '!':
    {
      move_to_the_next_char(code, cur_char);

      if (check_char(cur_char, '=')) {
        move_to_the_next_char(code, cur_char);

        /*
          Not Equal
            expr != expr
        */

        // drop all spaces after !=
        drop_spaces(code, cur_char);

        output = std::make_unique<BinCompAST>(
          CompType::NE,
          std::move(output),
          parse_value(code, cur_char));
      };
    }

    break;

  case '>':
    {
      move_to_the_next_char(code, cur_char);

      if (check_char(cur_char, '=')) {
        move_to_the_next_char(code, cur_char);

        /*
          Greater Than and Equal
            expr >= expr
        */

        // drop all spaces after >=
        drop_spaces(code, cur_char);

        output = std::make_unique<BinCompAST>(
          CompType::GE,
          std::move(output),
          parse_value(code, cur_char));
      }
      else {
        /*
          Greater Than
            expr > expr
        */

        // drop all spaces after >
        drop_spaces(code, cur_char);

        output = std::make_unique<BinCompAST>(
          CompType::GT,
          std::move(output),
          parse_value(code, cur_char));
      };
    }

    break;

  case '<':
    {
      move_to_the_next_char(code, cur_char);

      if (check_char(cur_char, '=')) {
        move_to_the_next_char(code, cur_char);

        /*
          Less Than and Equal
            expr <= expr
        */

        // drop all spaces after <=
        drop_spaces(code, cur_char);

        output = std::make_unique<BinCompAST>(
          CompType::LE,
          std::move(output),
          parse_value(code, cur_char));
      }
      else {
        /*
          Less Than
            expr < expr
        */

        // drop all spaces after <
        drop_spaces(code, cur_char);

        output = std::make_unique<BinCompAST>(
          CompType::LT,
          std::move(output),
          parse_value(code, cur_char));
      };
    }

    break;

  default:
    break;
  }

  return output;
}

/*
  Value Expression
*/

/*
  Call
    id(args)

  List Operation:
    id[expr]

  Binary Operation:
    id +  id
    id -  id
    id *  id
    id ** id
    id /  id
    id %  id
*/
std::unique_ptr<StyioAST> parse_id_or_value (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output;

  if (isalpha(cur_char) || check_char(cur_char, '_')) {
    output = parse_id(code, cur_char); }

  if (check_char(cur_char, '[')) {
    output = parse_list_op(code, cur_char, std::move(output)); } 
  else if (check_char(cur_char, '(')) {
    output = parse_call(code, cur_char); }

  drop_spaces(code, cur_char);

  if (check_binop_token(code)) {
    output = parse_binop_rhs(code, cur_char, std::move(output)); };

  return output;
}

std::unique_ptr<StyioAST> parse_value (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output;

  if (isalpha(cur_char) || check_char(cur_char, '_')) {
    return parse_id_or_value(code, cur_char); }
  else if (isdigit(cur_char)) {
    return parse_int_or_float(code, cur_char); }
  else if (check_char(cur_char, '|')) {
    return parse_size_of(code, cur_char); }

  std::string errmsg = std::string("parse_value() // Unexpected value expression, starting with .:| ") + char(cur_char) + " |:.";
  throw StyioParseError(errmsg);
}

std::unique_ptr<StyioAST> parse_item_for_binop (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output (new NoneAST());

  if (isalpha(cur_char) || check_char(cur_char, '_')) {
    return parse_id(code, cur_char); }
  else if (isdigit(cur_char)) {
    return parse_int_or_float(code, cur_char); }

  switch (cur_char)
  {
  case '\"':
    return parse_string(code, cur_char);

  case '\'':
    return parse_char_or_string(code, cur_char);

  case '[':
    move_to_the_next_char(code, cur_char);

    drop_spaces_and_comments(code, cur_char);

    if (check_and_drop_char(code, cur_char, ']')) {
      return std::make_unique<EmptyAST>(); }
    else {
      return parse_list_or_loop(code, cur_char); }

    // You should NOT reach this line!
    break;

  case '|':
    return parse_size_of(code, cur_char);

    // You should NOT reach this line!
    break;
  
  default:
    break;
  }

  return output;
}

std::unique_ptr<StyioAST> parse_expr (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output (new NoneAST());

  if (isalpha(cur_char) || check_char(cur_char, '_')) 
  {
    output = parse_id(code, cur_char);
    
    drop_spaces(code, cur_char);

    if (check_binop_token(code)) {
      output = parse_binop_rhs(code, cur_char, std::move(output)); };

    return output;
  }
  else if (isdigit(cur_char)) 
  {
    output = parse_int_or_float(code, cur_char);

    drop_white_spaces(code, cur_char);

    if (check_binop_token(code)) {
      output = parse_binop_rhs(code, cur_char, std::move(output)); };

    return output;
  };

  switch (cur_char)
  {
  case '\"':
    {
      return parse_string(code, cur_char);
    }

  case '\'':
    {
      return parse_char_or_string(code, cur_char);
    }

  case '[':
    {
      move_to_the_next_char(code, cur_char);

      drop_spaces_and_comments(code, cur_char);

      if (check_and_drop_char(code, cur_char, ']')) {
        output = std::make_unique<EmptyAST>(); }
      else {
        output = parse_list_or_loop(code, cur_char); }
    }

    // You should NOT reach this line!
    break;

  case '|':
    {
      output = parse_size_of(code, cur_char);

      drop_white_spaces(code, cur_char);

      if (check_binop_token(code)) {
        output = parse_binop_rhs(code, cur_char, std::move(output)); }
    }

    // You should NOT reach this line!
    break;
  
  default:
    break;
  }

  return output;
}

std::unique_ptr<StyioAST> parse_tuple (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::vector<std::unique_ptr<StyioAST>> exprs;

  /*
    Danger!
    when entering parse_tuple(), 
    the cur_char must be (
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  do {
    drop_spaces_and_comments(code, cur_char);

    if (check_and_drop_char(code, cur_char, ')')) {
      return std::make_unique<TupleAST>(std::move(exprs)); }
    else {
      exprs.push_back(parse_expr(code, cur_char));
      drop_white_spaces(code, cur_char); }
  } while (check_and_drop_char(code, cur_char, ','));

  check_and_drop_char(code, cur_char, ')');

  if (exprs.size() == 0) {
    return std::make_unique<EmptyAST>(); }
  else {
    return std::make_unique<TupleAST>(std::move(exprs)); }
}

std::unique_ptr<StyioAST> parse_list (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::vector<std::unique_ptr<StyioAST>> exprs;

  /*
    Danger!
    when entering parse_list(), 
    the cur_char must be [
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  do {
    drop_spaces_and_comments(code, cur_char);

    if (check_and_drop_char(code, cur_char, ']')) {
      return std::make_unique<ListAST>(std::move(exprs)); }
    else {
      exprs.push_back(parse_expr(code, cur_char));
      drop_white_spaces(code, cur_char); }
  } while (check_and_drop_char(code, cur_char, ','));

  check_and_drop_char(code, cur_char, '[');
  
  if (exprs.size() == 0) {
    return std::make_unique<EmptyAST>(); }
  else {
    return std::make_unique<ListAST>(std::move(exprs)); }
}

std::unique_ptr<StyioAST> parse_set (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::vector<std::unique_ptr<StyioAST>> exprs;

  /*
    Danger!
    when entering parse_set(), 
    the cur_char must be {
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  do {
    drop_spaces_and_comments(code, cur_char);

    if (check_and_drop_char(code, cur_char, '}')) {
      return std::make_unique<SetAST>(std::move(exprs)); }
    else {
      exprs.push_back(parse_expr(code, cur_char));
      drop_white_spaces(code, cur_char); }
  } while (check_and_drop_char(code, cur_char, ','));

  check_and_drop_char(code, cur_char, '}');

  if (exprs.size() == 0) {
    return std::make_unique<EmptyAST>(); }
  else {
    return std::make_unique<SetAST>(std::move(exprs)); }
}

std::unique_ptr<StyioAST> parse_iterable (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output (new EmptyAST());

  if (isalpha(cur_char) || check_char(cur_char, '_')) 
  {
    output = parse_id(code, cur_char);
    
    drop_spaces(code, cur_char);

    if (check_binop_token(code)) {
      output = parse_binop_rhs(code, cur_char, std::move(output)); };

    return output;
  }
  else
  {
    switch (cur_char)
    {
    case '(':
      {
        move_to_the_next_char(code, cur_char);

        std::vector<std::unique_ptr<StyioAST>> exprs;
        do {
          drop_spaces_and_comments(code, cur_char);

          if (check_and_drop_char(code, cur_char, ')')) {
            return std::make_unique<TupleAST>(std::move(exprs)); }
          else {
            exprs.push_back(parse_expr(code, cur_char));
            drop_white_spaces(code, cur_char); }
        } while (check_and_drop_char(code, cur_char, ','));

        check_and_drop_char(code, cur_char, ')');

        if (exprs.size() == 0) {
          return std::make_unique<EmptyAST>(); }
        else {
          return std::make_unique<TupleAST>(std::move(exprs)); }
      }

    case '[':
      {
        move_to_the_next_char(code, cur_char);

        std::vector<std::unique_ptr<StyioAST>> exprs;
        do {
          drop_spaces_and_comments(code, cur_char);

          if (check_and_drop_char(code, cur_char, ']')) {
            return std::make_unique<ListAST>(std::move(exprs)); }
          else {
            exprs.push_back(parse_expr(code, cur_char));
            drop_white_spaces(code, cur_char); }
        } while (check_and_drop_char(code, cur_char, ','));

        check_and_drop_char(code, cur_char, ']');

        if (exprs.size() == 0) {
          return std::make_unique<EmptyAST>(); }
        else {
          return std::make_unique<ListAST>(std::move(exprs)); }
      }

    case '{':
      {
        move_to_the_next_char(code, cur_char);

        std::vector<std::unique_ptr<StyioAST>> exprs;
        do {
          drop_spaces_and_comments(code, cur_char);

          if (check_and_drop_char(code, cur_char, '}')) {
            return std::make_unique<SetAST>(std::move(exprs)); }
          else {
            exprs.push_back(parse_expr(code, cur_char));
            drop_white_spaces(code, cur_char); }
        } while (check_and_drop_char(code, cur_char, ','));

        check_and_drop_char(code, cur_char, '}');

        if (exprs.size() == 0) {
          return std::make_unique<EmptyAST>(); }
        else {
          return std::make_unique<SetAST>(std::move(exprs)); }
      }
    
    default:
      break;
    }
  }

  return output;
}

/*
  Basic Operation:
  - Size Of / Get Length
  
  - List Operation
  - Call

  - Binary Operation
*/

std::unique_ptr<SizeOfAST> parse_size_of (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<SizeOfAST> output;

  // eliminate | at the start
  move_to_the_next_char(code, cur_char);
       
  if (isalpha(cur_char) || check_char(cur_char, '_'))
  {
    std::unique_ptr<StyioAST> var = parse_id_or_value(code, cur_char);

    // eliminate | at the end
    if (check_char(cur_char, '|')) {
      move_to_the_next_char(code, cur_char);

      output = std::make_unique<SizeOfAST>(std::move(var));
    }
    else
    {
      std::string errmsg = std::string("|expr| // SizeOf: Expecting | at the end, but got .:| ") + char(cur_char) + " |:.";
      throw StyioSyntaxError(errmsg);
    }
  }
  else
  {
    std::string errmsg = std::string("|expr| // SizeOf: Unexpected expression, starting with .:| ") + char(cur_char) + " |:.";
    throw StyioParseError(errmsg);
  }

  return output;
}

/*
  Invoke / Call
*/

std::unique_ptr<StyioAST> parse_call 
(
  struct StyioCodeContext* code,
  char& cur_char) {
  return std::make_unique<NoneAST>();
}

/*
  List Operation

  | [*] get_index_by_item
    : [?= item]
  
  | [*] insert_item_by_index
    : [+: index <- item]
  
  | [*] remove_item_by_index
    : [-: index]
  | [*] remove_many_items_by_indices
    : [-: (i0, i1, ...)]
  | [*] remove_item_by_value
    : [-: ?= item]
  | [ ] remove_many_items_by_values
    : [-: ?^ (v0, v1, ...)]

  | [*] get_reversed
    : [<]
  | [ ] get_index_by_item_from_right
    : [[<] ?= item]
  | [ ] remove_item_by_value_from_right
    : [[<] -: ?= value]
  | [ ] remove_many_items_by_values_from_right
    : [[<] -: ?^ (v0, v1, ...)]
*/

std::unique_ptr<ListOpAST> parse_list_op (
  struct StyioCodeContext* code, 
  char& cur_char,
  std::unique_ptr<StyioAST> theList) {
  /*
    Danger!
    when entering parse_list_op(), 
    the cur_char must be [
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  std::unique_ptr<ListOpAST> output;

  do
  {
    if (isdigit(cur_char)) {
      output = std::make_unique<ListOpAST>(
        StyioType::Access_By_Index,
        std::move(theList), 
        parse_int(code, cur_char)); }
    else
    {
      switch (cur_char)
      {
      /*
        list["any"]
      */
      case '"':
        {
          output = std::make_unique<ListOpAST>(
            StyioType::Access_By_Name,
            std::move(theList), 
            parse_string(code, cur_char)); 
        }
        
        // You should NOT reach this line!
        break;

      /*
        list[<]
      */
      case '<':
        {
          move_to_the_next_char(code, cur_char);

          output = std::make_unique<ListOpAST>(
            StyioType::Get_Reversed,
            std::move(theList));
        }

        // You should NOT reach this line!
        break;

      // list[?= item]
      case '?':
        {
          move_to_the_next_char(code, cur_char);

          if (check_and_drop_char(code, cur_char, '='))
          {
            output = std::make_unique<ListOpAST>(
              StyioType::Get_Index_By_Value,
              std::move(theList), 
              parse_expr(code, cur_char));
          }
          else
          {
            std::string errmsg = std::string("Missing `=` for `?=` after `?= item`, but got `") + char(cur_char) + "`";
            throw StyioSyntaxError(errmsg);
          }
        }

        // You should NOT reach this line!
        break;
      
      case '+':
        {
          move_to_the_next_char(code, cur_char);

          if (check_and_drop_char(code, cur_char, ':'))
          {
            // eliminate white spaces after +:
            drop_white_spaces(code, cur_char);

            if (isdigit(cur_char))
            {
              /*
                list[+: index <- value]
              */

              std::unique_ptr<IntAST> index = parse_int(code, cur_char);

              // eliminate white spaces between index and <-
              drop_white_spaces(code, cur_char);

              if (check_and_drop_symbol(code, cur_char, "<-"))
              {
                // eliminate white spaces between <- and the value to be inserted
                drop_white_spaces(code, cur_char);

                output = std::make_unique<ListOpAST>(
                  StyioType::Insert_Item_By_Index,
                  std::move(theList), 
                  std::move(index),
                  std::move(parse_expr(code, cur_char)));
              }
              else
              {
                std::string errmsg = std::string("Expecting `<-` after `+: index`, but got `") + char(cur_char) + "`";
                throw StyioSyntaxError(errmsg);
              }
            }
          }
          else
          {
            std::string errmsg = std::string("Expecting integer index after `+:`, but got `") + char(cur_char) + "`";
            throw StyioSyntaxError(errmsg);
          }
        }

        // You should NOT reach this line!
        break;
      
      case '-':
        {
          move_to_the_next_char(code, cur_char);

          if (check_and_drop_char(code, cur_char, ':'))
          {
            // eliminate white spaces after -:
            drop_white_spaces(code, cur_char);

            if (isdigit(cur_char))
            {
              /*
                list[-: index]
              */

              std::unique_ptr<IntAST> theIndex = parse_int(code, cur_char);

              output = std::make_unique<ListOpAST>(
                StyioType::Remove_Item_By_Index,
                std::move(theList), 
                std::move(theIndex));
            }
            else
            {
              switch (cur_char)
              {
              case '(':
              {
                /*
                  list[-: (i0, i1, ...)]
                */

                output = std::make_unique<ListOpAST>(
                  StyioType::Remove_Items_By_Many_Indices,
                  std::move(theList), 
                  std::move(parse_iterable(code, cur_char)));
              }

                // You should NOT reach this line!
                break;

              case '?':
              {
                move_to_the_next_char(code, cur_char);

                switch (cur_char)
                {
                case '=':
                {
                  /*
                    list[-: ?= value]
                  */

                  move_to_the_next_char(code, cur_char);

                  // drop white spaces after ?=
                  drop_white_spaces(code, cur_char);

                  output = std::make_unique<ListOpAST>(
                    StyioType::Remove_Item_By_Value,
                    std::move(theList), 
                    parse_expr(code, cur_char));
                }
                
                  // You should NOT reach this line!
                  break;
                
                case '^':
                {
                  /*
                    list[-: ?^ (v0, v1, ...)]
                  */

                  move_to_the_next_char(code, cur_char);

                  // drop white spaces after ?^
                  drop_white_spaces(code, cur_char);

                  if (check_char(cur_char, '(') 
                    || check_char(cur_char, '[')
                    || check_char(cur_char, '{'))
                  {
                    move_to_the_next_char(code, cur_char);
                  }
                }
                
                // You should NOT reach this line!
                break;
                
                default:
                  break;
                }
              }
              
              default:
                break;
              }
            }
          }
          else
          {
            std::string errmsg = std::string("Missing `:` for `-:`, got `") + char(cur_char) + "`";
            throw StyioSyntaxError(errmsg);
          }
        }

        // You should NOT reach this line!
        break;

      default:
        {
          std::string errmsg = std::string("Unexpected List[Operation], starts with ") + char(cur_char);
          throw StyioSyntaxError(errmsg);
        }

        // You should NOT reach this line!
        break;
      }
    }

    drop_spaces_and_comments(code, cur_char);

    find_and_drop_char_panic(code, cur_char, ']');

  } while (check_char(cur_char, '['));

  return output;
}

std::unique_ptr<StyioAST> parse_loop_or_iter (
  struct StyioCodeContext* code, 
  char& cur_char,
  std::unique_ptr<StyioAST> iterOverIt) {

  if ((iterOverIt -> hint()) == StyioType::Infinite) {
    return std::make_unique<LoopAST>(
      parse_forward(code, cur_char, false)); }
  else {
    return std::make_unique<IterAST>(
      std::move(iterOverIt),
      parse_forward(code, cur_char, false)); }
}

std::unique_ptr<StyioAST> parse_list_or_loop (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output;

  std::vector<std::unique_ptr<StyioAST>> elements;

  std::unique_ptr<StyioAST> startEl = parse_expr(code, cur_char);

  drop_white_spaces(code, cur_char);

  if (check_and_drop_char(code, cur_char, '.')) {
    while (check_char(cur_char, '.')) {
      move_to_the_next_char(code, cur_char); }

    drop_white_spaces(code, cur_char);
      
    std::unique_ptr<StyioAST> endEl = parse_expr(code, cur_char);

    drop_white_spaces(code, cur_char);

    match_next_char_panic(code, cur_char, ']');

    std::cout << startEl << "; " << endEl << std::endl;

    if (startEl -> hint() == StyioType::Int 
      && endEl -> hint() == StyioType::Id) {
      output = std::make_unique<InfiniteAST>(
        std::move(startEl), 
        std::move(endEl)); }
    else
    if (startEl -> hint() == StyioType::Int 
      && endEl -> hint() == StyioType::Int) {
      output = std::make_unique<RangeAST>(
        std::move(startEl), 
        std::move(endEl), 
        std::make_unique<IntAST>("1")); }
    else {
      std::string errmsg = std::string("Unexpected Range / List / Loop: ")
        + "starts with: " + std::to_string(type_to_int(startEl -> hint())) + ", "
        + "ends with: " + std::to_string(type_to_int(endEl -> hint())) + ".";
      throw StyioSyntaxError(errmsg); }
  }
  else if (check_and_drop_char(code, cur_char, ',')) {
    elements.push_back(std::move(startEl));

    do {
      drop_spaces_and_comments(code, cur_char);

      if (check_and_drop_char(code, cur_char, ']')) {
        return std::make_unique<ListAST>(std::move(elements)); }
      else {
        elements.push_back(parse_expr(code, cur_char)); }
    } while (check_and_drop_char(code, cur_char, ','));
  }
  else {
    elements.push_back(std::move(startEl));

    return std::make_unique<ListAST>(std::move(elements));
  }

  while (check_char(cur_char, '[')) {
      output = parse_list_op(code, cur_char, std::move(output)); }

  drop_spaces(code, cur_char);

  if (check_and_drop_symbol(code, cur_char, ">>")) {
    output = parse_loop_or_iter(code, cur_char, std::move(output)); }

  return output;
}

std::unique_ptr<StyioAST> parse_loop (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output;

  while (check_char(cur_char, '.')) { 
    move_to_the_next_char(code, cur_char); }

  find_and_drop_char_panic(code, cur_char, ']');

  drop_spaces(code, cur_char);

  if (check_and_drop_symbol(code, cur_char, ">>")) {
    drop_spaces(code, cur_char);

    return std::make_unique<LoopAST>(
      parse_forward(code, cur_char));
  }

  return std::make_unique<InfiniteAST>();
}

std::unique_ptr<BinOpAST> parse_binop_rhs (
  struct StyioCodeContext* code, 
  char& cur_char, 
  std::unique_ptr<StyioAST> lhs_ast
) 
{
  std::unique_ptr<BinOpAST> output;

  switch (cur_char)
  {
    // BIN_ADD := <ID> "+" <EXPR>
    case '+':
      {
        move_next_and_ignore(code, cur_char);

        // <ID> "+" |-- 
        output = std::make_unique<BinOpAST>(
          BinOpType::BIN_ADD, 
          std::move(lhs_ast), 
          std::move(parse_item_for_binop(code, cur_char)));
      };

      // You should NOT reach this line!
      break;

    // BIN_SUB := <ID> "-" <EXPR>
    case '-':
      {
        move_next_and_ignore(code, cur_char);

        // <ID> "-" |--
        output = std::make_unique<BinOpAST>(
          BinOpType::BIN_SUB, 
          std::move(lhs_ast), 
          std::move(parse_item_for_binop(code, cur_char)));
      };

      // You should NOT reach this line!
      break;

    // BIN_MUL | BIN_POW
    case '*':
      {
        move_to_the_next_char(code, cur_char);
        // BIN_POW := <ID> "**" <EXPR>
        if (check_char(cur_char, '*'))
        {
          move_next_and_ignore(code, cur_char);

          // <ID> "**" |--
          output = std::make_unique<BinOpAST>(
            BinOpType::BIN_POW, 
            std::move(lhs_ast), 
            std::move(parse_item_for_binop(code, cur_char)));
        } 
        // BIN_MUL := <ID> "*" <EXPR>
        else 
        {
          drop_spaces(code, cur_char);

          // <ID> "*" |--
          output = std::make_unique<BinOpAST>(
            BinOpType::BIN_MUL, 
            std::move(lhs_ast), 
            std::move(parse_item_for_binop(code, cur_char)));
        }
      };
      // You should NOT reach this line!
      break;
      
    // BIN_DIV := <ID> "/" <EXPR>
    case '/':
      {
        move_next_and_ignore(code, cur_char);

        // <ID> "/" |-- 
        output = std::make_unique<BinOpAST>(
          BinOpType::BIN_DIV, 
          std::move(lhs_ast), 
          std::move(parse_item_for_binop(code, cur_char)));
      };

      // You should NOT reach this line!
      break;

    // BIN_MOD := <ID> "%" <EXPR> 
    case '%':
      {
        move_next_and_ignore(code, cur_char);

        // <ID> "%" |-- 
        output = std::make_unique<BinOpAST>(
          BinOpType::BIN_MOD, 
          std::move(lhs_ast), 
          std::move(parse_item_for_binop(code, cur_char)));
      };

      // You should NOT reach this line!
      break;
    
    default:
      std::string errmsg = std::string("Unexpected BinOp.Operator: `") + char(cur_char) + "`.";
      throw StyioSyntaxError(errmsg);

      // You should NOT reach this line!
      break;
  }

  drop_spaces(code, cur_char);

  while (check_binop_token(code)) 
  {
    drop_spaces(code, cur_char);

    output = parse_binop_rhs(code, cur_char, std::move(output));
  }

  return output;
}

std::unique_ptr<CondAST> parse_cond_rhs (
  struct StyioCodeContext* code, 
  char& cur_char,
  std::unique_ptr<StyioAST> lhsExpr
)
{
  std::unique_ptr<CondAST> condExpr;

  drop_spaces(code, cur_char);

  switch (cur_char)
  {
  case '&':
    {
      move_to_the_next_char(code, cur_char);

      if (check_char(cur_char, '&'))
      {
        move_to_the_next_char(code, cur_char);
      };

      /*
        support:
          expr && \n
          expression
      */

      drop_spaces(code, cur_char);

      condExpr = std::make_unique<CondAST>(
        LogicType::AND,
        std::move(lhsExpr),
        parse_cond(code, cur_char)
      );
    }

    break;

  case '|':
    {
      move_to_the_next_char(code, cur_char);

      if (check_char(cur_char, '|'))
      {
        move_to_the_next_char(code, cur_char);
      };

      /*
        support:
          expr || \n
          expression
      */

      drop_spaces(code, cur_char);

      condExpr = std::make_unique<CondAST>(
        LogicType::OR,
        std::move(lhsExpr),
        parse_cond(code, cur_char)
      );
    }

    break;

  case '^':
    {
      move_to_the_next_char(code, cur_char);

      /*
        support:
          expr ^ \n
          expression
      */

      drop_spaces(code, cur_char);

      condExpr = std::make_unique<CondAST>(
        LogicType::OR,
        std::move(lhsExpr),
        parse_cond(code, cur_char)
      );
    }

    break;

  case '!':
    {
      move_to_the_next_char(code, cur_char);

      if (check_char(cur_char, '('))
      {
        move_to_the_next_char(code, cur_char);

        /*
          support:
            !( \n
              expr
            )
        */
        drop_spaces(code, cur_char);

        condExpr = std::make_unique<CondAST>(
          LogicType::NOT,
          parse_cond(code, cur_char)
        );

        find_and_drop_char_panic(code, cur_char, ')');
      }
    }

    break;

  default:
    break;
  }

  drop_spaces(code, cur_char);

  while (!(check_char(cur_char, ')')))
  {
    condExpr = std::move(parse_cond_rhs(code, cur_char, std::move(condExpr)));
  }
  
  return condExpr;
}

std::unique_ptr<CondAST> parse_cond (
  struct StyioCodeContext* code, 
  char& cur_char
)
{
  std::unique_ptr<StyioAST> lhsExpr;

  if (check_char(cur_char, '('))
  {
    move_to_the_next_char(code, cur_char);

    lhsExpr = std::move(parse_cond(code, cur_char));

    find_and_drop_char_panic(code, cur_char, ')');
  }
  else
  if (check_char(cur_char, '!'))
  {
    move_to_the_next_char(code, cur_char);

    if (check_char(cur_char, '('))
    {
      move_to_the_next_char(code, cur_char);

      /*
        support:
          !( \n
            expr
          )
      */
      drop_spaces(code, cur_char);

      lhsExpr = std::move(parse_cond(code, cur_char));

      drop_spaces(code, cur_char);

      return std::make_unique<CondAST>(
        LogicType::NOT,
        std::move(lhsExpr)
      );
    }
    else
    {
      std::string errmsg = std::string("!(expr) // Expecting ( after !, but got ") + char(cur_char);
      throw StyioSyntaxError(errmsg);
    };
  }
  else
  {
    lhsExpr = std::move(parse_item_for_cond(code, cur_char));
  };

  // drop all spaces after first value
  drop_spaces(code, cur_char);

  if (check_char(cur_char, '&')
    || check_char(cur_char, '|'))
  {
    return parse_cond_rhs(code, cur_char, std::move(lhsExpr));
  }
  else
  {
    return std::make_unique<CondAST>(
      LogicType::RAW,
      std::move(lhsExpr)
    );
  }

  std::string errmsg = std::string("parse_cond() : You should not reach this line!") + char(cur_char);
  throw StyioParseError(errmsg);
}

std::unique_ptr<StyioAST> parse_cond_flow (
  struct StyioCodeContext* code, 
  char& cur_char){
  /*
    Danger!
    when entering parse_cond_flow(), 
    the cur_char must be ?
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  drop_white_spaces(code, cur_char);

  if (check_and_drop_char(code, cur_char, '(')) {
    std::unique_ptr<CondAST> condition = parse_cond(code, cur_char);

    find_and_drop_char_panic(code, cur_char, ')');

    /*
      support:
        ?() \n
        \t\

        ?() \n
        \f\
    */
    drop_spaces_and_comments(code, cur_char);

    if (check_and_drop_char(code, cur_char, '\\'))
    {
      std::unique_ptr<StyioAST> block;

      if (check_and_drop_char(code, cur_char, 't'))
      {
        check_and_drop_char(code, cur_char, '\\');

        /*
          support:
            \t\ \n
            {}
        */

        drop_spaces_and_comments(code, cur_char);

        block = parse_block(code, cur_char);

        /*
          support:
            \t\ {} \n
            \f\
        */
        drop_spaces_and_comments(code, cur_char);

        if (check_and_drop_char(code, cur_char, '\\'))
        {
          match_next_char_panic(code, cur_char, 'f');

          check_and_drop_char(code, cur_char, '\\');

          /*
            support:
              \f\ \n
              {}
          */
          drop_spaces_and_comments(code, cur_char);

          std::unique_ptr<StyioAST> blockElse = parse_block(code, cur_char);

          return std::make_unique<CondFlowAST>(
            FlowType::Both,
            std::move(condition),
            std::move(block),
            std::move(blockElse)
          );
        }
        else {
          return std::make_unique<CondFlowAST>(
            FlowType::True,
            std::move(condition),
            std::move(block)
          ); 
        } 
      }
      else if (check_and_drop_char(code, cur_char, 'f')) 
      {
        check_and_drop_char(code, cur_char, '\\');

        /*
          support:
            \f\ \n
            {}
        */
        drop_spaces_and_comments(code, cur_char);

        block = parse_block(code, cur_char);

        return std::make_unique<CondFlowAST>(
          FlowType::False,
          std::move(condition),
          std::move(block)); }
      else 
      {
        std::string errmsg = std::string("parse_cond_flow() // Unexpected character ") + cur_char;
        throw StyioSyntaxError(errmsg); 
      }
    }
  }
  else {
    std::string errmsg = std::string("Missing （ for ?(`expr`).");
    throw StyioSyntaxError(errmsg); 
  }

  std::string errmsg = std::string("parse_cond_flow() // You should not reach the end of this function.");
  throw StyioParseError(errmsg); 
}

std::unique_ptr<StyioAST> parse_pipeline (
  struct StyioCodeContext* code, 
  char& cur_char) {
  /*
    Danger!
    when entering parse_pipeline(), 
    the cur_char must be #
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  drop_white_spaces(code, cur_char);

  if (isalpha(cur_char) || check_char(cur_char, '_')) 
  {
    std::unique_ptr<IdAST> name = parse_id(code, cur_char);

    drop_spaces_and_comments(code, cur_char);

    if (check_and_drop_char(code, cur_char, ':')) 
    {
      if (check_and_drop_char(code, cur_char, '=')) 
      {
        drop_spaces(code, cur_char);

        return std::make_unique<FuncAST>(
          std::move(name),
          parse_forward(code, cur_char, true),
          true);
      }

      std::string errmsg = std::string("parse_pipeline() // Inheritance, Type Hint.");
      throw StyioNotImplemented(errmsg);
    }
    else if (check_and_drop_char(code, cur_char, '=')) {
      if (check_char(cur_char, '>')) 
      {
        move_backward(code, cur_char, 1);

        return std::make_unique<FuncAST>(
          std::move(name),
          parse_forward(code, cur_char, true),
          false);
      }
      else 
      {
        drop_spaces(code, cur_char);

        return std::make_unique<FuncAST>(
          std::move(name),
          parse_forward(code, cur_char, true),
          false);
      }
    }
  }

  drop_spaces(code, cur_char);
  return parse_forward(code, cur_char, true);
}

std::unique_ptr<StyioAST> parse_forward (
  struct StyioCodeContext* code,
  char& cur_char,
  bool ispipe) {
  std::unique_ptr<StyioAST> output;

  std::unique_ptr<VarsTupleAST> tmpvars;
  bool hasVars = false;

  if (ispipe) {
    if (check_char(cur_char, '(')) {
      tmpvars = parse_vars_tuple(code, cur_char);
      hasVars = true; }
  }
  else if (check_and_drop_char(code, cur_char, '#')) {
    drop_white_spaces(code, cur_char);

    if (check_char(cur_char, '(')) {
      tmpvars = parse_vars_tuple(code, cur_char);
      hasVars = true; }
    else {
      std::string errmsg = std::string("parse_forward() // Expecting ( after #, but got ") + char(cur_char);
      throw StyioSyntaxError(errmsg); }
  }

  drop_spaces(code, cur_char);

  /*
    support:
    
    (x, y) \n
    ?=

    (x, y) \n
    ?^

    (x, y) \n
    ?()

    (x, y) \n
    =>

  */
  switch (cur_char)
  {
  case '?':
    {
      move_to_the_next_char(code, cur_char);

      switch (cur_char)
      {
      /*
        ?= Value
      */
      case '=':
        {
          move_to_the_next_char(code, cur_char);

          drop_spaces(code, cur_char);

          if (check_and_drop_char(code, cur_char, '{')) 
          {
            if (hasVars) {
              output = std::make_unique<ForwardAST>(
                std::move(tmpvars), 
                parse_cases(code, cur_char)); }
            else {
              output = std::make_unique<ForwardAST>(
                parse_cases(code, cur_char)); } 
          }
          else {
            std::unique_ptr<CheckEqAST> value;
            std::unique_ptr<StyioAST> then;

            // drop white spaces after ?=
            drop_white_spaces(code, cur_char);
            
            value = std::make_unique<CheckEqAST>(parse_value(code, cur_char));

            drop_spaces(code, cur_char);

            if (check_and_drop_symbol(code, cur_char, "=>")) 
            {
              drop_spaces(code, cur_char);

              if (check_char(cur_char, '{')) { 
                then = parse_block(code, cur_char); }
              else {
                then = parse_expr(code, cur_char); }

              if (hasVars) {
                output = std::make_unique<ForwardAST>(
                  std::move(tmpvars), 
                  std::move(value), 
                  std::move(then)); }
              else {
                output = std::make_unique<ForwardAST>(
                  std::move(value), 
                  std::move(then)); } 
            }
            else 
            {
              std::string errmsg = std::string("parse_forward() // Expecting `=>` after `?= value`, but got ") + char(cur_char);
                throw StyioSyntaxError(errmsg); 
            }
          }
        } 

        break;
      /*
        ?^ [Iterable]
      */
      case '^':
        {
          move_to_the_next_char(code, cur_char);

          std::unique_ptr<StyioAST> nextExpr;

          std::unique_ptr<StyioAST> iterable;

          drop_white_spaces(code, cur_char);
          
          switch (cur_char)
          {
          case '(':
            {
              move_to_the_next_char(code, cur_char);

              std::vector<std::unique_ptr<StyioAST>> exprs;
              do {
                drop_spaces_and_comments(code, cur_char);

                if (check_char(cur_char, ')')) {
                  break; }
                else {
                  exprs.push_back(parse_expr(code, cur_char)); }
              } while (check_and_drop_char(code, cur_char, ','));

              find_and_drop_char(code, cur_char, ')');

              iterable = std::make_unique<TupleAST>(std::move(exprs));
            }
            break;
          
          case '[':
            {
              move_to_the_next_char(code, cur_char);

              std::vector<std::unique_ptr<StyioAST>> exprs;
              do {
                drop_spaces_and_comments(code, cur_char);

                if (check_char(cur_char, ']')) {
                  break; }
                else {
                  exprs.push_back(parse_expr(code, cur_char)); }
              } while (check_and_drop_char(code, cur_char, ','));

              find_and_drop_char(code, cur_char, ']');

              iterable = std::make_unique<ListAST>(std::move(exprs));
            }

            break;
          
          case '{':
            {
              move_to_the_next_char(code, cur_char);

              std::vector<std::unique_ptr<StyioAST>> exprs;
              do {
                drop_spaces_and_comments(code, cur_char);

                if (check_char(cur_char, '}')) {
                  break; }
                else {
                  exprs.push_back(parse_expr(code, cur_char)); }
              } while (check_and_drop_char(code, cur_char, ','));

              find_and_drop_char(code, cur_char, '}');

              iterable = std::make_unique<SetAST>(std::move(exprs));
            }
            break;
          
          default:
            {
              if (isalpha(cur_char) || check_char(cur_char, '_')) {
                iterable = parse_id_or_value(code, cur_char); } 
              else {
                std::string errmsg = std::string("parse_forward() // Unexpected collection, starting with ") + char(cur_char);
                throw StyioSyntaxError(errmsg); }
            }
            break;
          }

          drop_spaces_and_comments(code, cur_char);

          if (check_and_drop_symbol(code, cur_char, "=>")) {
            drop_spaces(code, cur_char);

            if (check_char(cur_char, '{')) { 
              nextExpr = parse_block(code, cur_char); }
            else {
              nextExpr = parse_expr(code, cur_char); }

            if (hasVars) {
              output = std::make_unique<ForwardAST>(
                std::move(tmpvars), 
                std::make_unique<CheckIsInAST>(std::move(iterable)), 
                std::move(nextExpr)); }
            else {
              output = std::make_unique<ForwardAST>(
                std::make_unique<CheckIsInAST>(std::move(iterable)), 
                std::move(nextExpr)); } }
          else {
            std::string errmsg = std::string("parse_forward() // Expecting `=>` after `?^ iterable`, but got ") + char(cur_char);
              throw StyioSyntaxError(errmsg); }
        }

        break;

      /*
        ?(Condition) 
        \t\ { }
        
        ?(Condition) 
        \f\ { }
      */
      default:
        move_backward(code, cur_char, 1);

        if (hasVars) {
          output = std::make_unique<ForwardAST>(
            std::move(tmpvars),
            parse_cond_flow(code, cur_char)); }
        else {
          output = std::make_unique<ForwardAST>(
            parse_cond_flow(code, cur_char)); }

        break;
      }
    }
    break;

  /*
    support:

    => \n
    { }
  */
  case '=':
    {
      move_to_the_next_char(code, cur_char);

      match_next_char_panic(code, cur_char, '>');

      drop_spaces(code, cur_char);

      if (check_char(cur_char, '{')) 
      {
        if (hasVars) {
          output = std::make_unique<ForwardAST>(
            std::move(tmpvars), 
            parse_block(code, cur_char)); }
        else {
          output = std::make_unique<ForwardAST>(
            parse_block(code, cur_char)); } 
      }
      else 
      {
        if (hasVars) {
          output = std::make_unique<ForwardAST>(
            std::move(tmpvars), 
            parse_expr(code, cur_char)); }
        else {
          output = std::make_unique<ForwardAST>(
            parse_expr(code, cur_char)); }
      }
    }
    break;

  /*
    support:

    { }
  */
  case '{':
    {
      if (hasVars) {
        output = std::make_unique<ForwardAST>(
          std::move(tmpvars), 
          parse_block(code, cur_char)); }
      else {
        output = std::make_unique<ForwardAST>(
          parse_block(code, cur_char)); }
    }
    break;
  
  default:
    std::string errmsg = std::string("parse_forward() // Unexpected character ") + char(cur_char);
    throw StyioSyntaxError(errmsg);

    break;
  }

  drop_spaces_and_comments(code, cur_char);

  while (check_and_drop_symbol(code, cur_char, "|>")) 
  {
    drop_spaces(code, cur_char);

    output = std::make_unique<FromToAST>(
      std::move(output),
      parse_forward(code, cur_char));

    drop_spaces_and_comments(code, cur_char); 
  }

  return output;
}

std::unique_ptr<StyioAST> parse_read_file (
  struct StyioCodeContext* code, 
  char& cur_char, 
  std::unique_ptr<IdAST> id_ast) {
  if (check_char(cur_char, '@')) {
    return std::make_unique<ReadFileAST>(
      std::move(id_ast), 
      parse_path_or_link(code, cur_char)); }
  else {
    std::string errmsg = std::string("Unexpected Read.Path, starting with character `") + char(cur_char) + "`";
    throw StyioSyntaxError(errmsg); }
}

std::unique_ptr<StyioAST> parse_print (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output;

  std::vector<std::unique_ptr<StyioAST>> exprs;

  /*
    Danger!
    when entering parse_print(), 
    the following symbol must be >_
    this line will drop the next 2 characters anyway!
  */
  move_forward(code, cur_char, 2);

  match_next_char_panic(code, cur_char, '(');

  do {
    drop_spaces_and_comments(code, cur_char);

    if (check_and_drop_char(code, cur_char, ')')) {
      return std::make_unique<PrintAST>(std::move(exprs)); }
    else {
      exprs.push_back(parse_expr(code, cur_char)); }
  } while (check_and_drop_char(code, cur_char, ','));

  find_and_drop_char_panic(code, cur_char, ')');
  return std::make_unique<PrintAST>(std::move(exprs));
}

std::unique_ptr<StyioAST> parse_stmt (
  struct StyioCodeContext* code, 
  char& cur_char) {
  drop_spaces_and_comments(code, cur_char);
  
  if (isalpha(cur_char) || check_char(cur_char, '_')) 
  {
    std::unique_ptr<IdAST> id_ast = parse_id(code, cur_char);

    if (check_char(cur_char, '[')) {
      return parse_list_op(code, cur_char, std::move(id_ast)); }
    
    drop_spaces_and_comments(code, cur_char);

    if (check_binop_token(code)) {
      return parse_binop_rhs(code, cur_char, std::move(id_ast)); } 

    switch (cur_char)
    {
      case '=':
        {
          move_to_the_next_char(code, cur_char);

          drop_spaces_and_comments(code, cur_char);

          return std::make_unique<FlexBindAST>(
            std::move(id_ast), 
            std::move(parse_expr(code, cur_char)));
        };

        // You should NOT reach this line!
        break;
      
      case ':':
        {
          move_to_the_next_char(code, cur_char);

          if (check_and_drop_char(code, cur_char, '=')) 
          {
            drop_spaces_and_comments(code, cur_char);
            
            return std::make_unique<FinalBindAST>(
              std::move(id_ast), 
              std::move(parse_expr(code, cur_char)));
          }
          else 
          {
            drop_white_spaces(code, cur_char);

            std::unique_ptr<TypeAST> type = parse_type(code, cur_char);

            drop_white_spaces(code, cur_char);

            if (check_and_drop_char(code, cur_char, ':')) 
            {
              if (check_and_drop_char(code, cur_char, '=')) 
              {
                drop_white_spaces(code, cur_char);
                
                return std::make_unique<FinalBindAST>(
                  std::move(id_ast), 
                  std::move(parse_expr(code, cur_char)));
              }
            }
            else if (check_and_drop_char(code, cur_char, '='))
            {
              drop_white_spaces(code, cur_char);
              
              return std::make_unique<FlexBindAST>(
                std::move(id_ast), 
                std::move(parse_expr(code, cur_char)));
            }
            else 
            {
              std::string errmsg = std::string("parse_stmt() // Expecting = or := after type, but got ") + cur_char;
              throw StyioSyntaxError(errmsg);
            }
          }
        };

        // You should NOT reach this line!
        break;

      case '<':
        {
          move_to_the_next_char(code, cur_char);

          if (check_and_drop_char(code, cur_char, '-'))
          {
            drop_spaces(code, cur_char);

            return parse_read_file(code, cur_char, std::move(id_ast));
          }
          else
          {
            std::string errmsg = std::string("Expecting `-` after `<`, but found `") + char(cur_char) + "`.";
            throw StyioSyntaxError(errmsg);
          }
        };

        // You should NOT reach this line!
        break;

      case '>':
        {
          move_to_the_next_char(code, cur_char);

          if (check_and_drop_char(code, cur_char, '>')) 
          { 
            drop_spaces(code, cur_char);

            return parse_loop_or_iter(code, cur_char, std::move(id_ast)); 
          }
        }
        
        // You should NOT reach this line!
        break;

      default:
        break;
    }
  }
  // Int / Float
  else if (isdigit(cur_char)) {
    std::unique_ptr<StyioAST> numAST = parse_int_or_float(code, cur_char);

    drop_spaces_and_comments(code, cur_char);

    if (check_binop_token(code)) {
      return parse_binop_rhs(code, cur_char, std::move(numAST)); } 
    else { return numAST; }
  }
  // Print
  else if (check_symbol(code, ">_")) {
    return parse_print(code, cur_char);
  }

  switch (cur_char)
  {
  case EOF:
    return std::make_unique<EndAST>();

    // You should NOT reach this line!
    break;

  case '\"':
    return parse_string(code, cur_char);

    // You should NOT reach this line!
    break;

  case '?':
    return parse_cond_flow(code, cur_char);
    
    // You should NOT reach this line!
    break;

  case '#':
    return parse_pipeline(code, cur_char);

    // You should NOT reach this line!
    break;

  case '.':
    {
      move_to_the_next_char(code, cur_char);
      while (check_char(cur_char, '.')) {
        move_to_the_next_char(code, cur_char);
      }
      return std::make_unique<PassAST>();
    }

    // You should NOT reach this line!
    break;

  case '@':
    {
      std::unique_ptr<ResourceAST> resources = parse_resources(code, cur_char);

      drop_spaces_and_comments(code, cur_char);

      if (check_and_drop_symbol(code, cur_char, "->")) {
        drop_spaces_and_comments(code, cur_char);

        return std::make_unique<FromToAST>(
          std::move(resources), 
          parse_block(code, cur_char));
      }
      else
      {
        return resources;
      };
    };

    // You should NOT reach this line!
    break;

  case '[':
    {
      move_to_the_next_char(code, cur_char);

      drop_spaces_and_comments(code, cur_char);

      if (check_and_drop_char(code, cur_char, '.')) {
        return parse_loop(code, cur_char); }
      else {
        return parse_list_or_loop(code, cur_char); }
    }
    
    // You should NOT reach this line!
    break;

  case '=':
    {
      move_to_the_next_char(code, cur_char);

      if (check_and_drop_char(code, cur_char, '>'))
      {
        drop_white_spaces(code, cur_char);
      
        return std::make_unique<ReturnAST>(parse_expr(code, cur_char));
      }

      std::string errmsg = std::string("parse_stmt // => ");
      throw StyioSyntaxError(errmsg);
    }

    // You should NOT reach this line!
    break;
    
  default:
    break;
  }

  std::string errmsg = std::string("Unrecognized statement, starting with `") + char(cur_char) + "`";
  throw StyioSyntaxError(errmsg);
}

std::string parse_ext_elem(
  struct StyioCodeContext* code, 
  char& cur_char
)
{
  std::string itemStr;

  if (check_char(cur_char, '\"'))
  {
    // eliminate double quote symbol " at the start of dependency item
    move_to_the_next_char(code, cur_char);

    while (cur_char != '\"') 
    {
      if (check_char(cur_char, ',')) 
      {
        std::string errmsg = std::string("An \" was expected after") + itemStr + "however, a delimeter `,` was detected. ";
        throw StyioSyntaxError(errmsg);
      }

      itemStr += cur_char;

      move_to_the_next_char(code, cur_char);
    };

    // eliminate double quote symbol " at the end of dependency item
    move_to_the_next_char(code, cur_char);

    return itemStr;
  }
  else
  {
    std::string errmsg = std::string("Dependencies should be wrapped with double quote like \"abc/xyz\", rather than starting with the character `") + char(cur_char) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

std::unique_ptr<ExtPackAST> parse_ext_pack (
  struct StyioCodeContext* code, 
  char& cur_char) { 
  // eliminate left square (box) bracket [
  move_to_the_next_char(code, cur_char);

  std::vector<std::string> dependencies;

  drop_spaces(code, cur_char);

  // add the first dependency path to the list
  dependencies.push_back(std::move(parse_ext_elem(code, cur_char)));

  std::string pathStr = "";
  
  while (check_char(cur_char, ',')) {
    // eliminate comma ","
    move_to_the_next_char(code, cur_char);

    // reset pathStr to empty ""
    pathStr = ""; 

    drop_spaces(code, cur_char);
    
    // add the next dependency path to the list
    dependencies.push_back(std::move(parse_ext_elem(code, cur_char)));
  };

  if (check_char(cur_char, ']')) {
    // eliminate right square bracket `]` after dependency list
    move_to_the_next_char(code, cur_char);
  };

  std::unique_ptr<ExtPackAST> output = std::make_unique<ExtPackAST>(dependencies);

  return output;
}

std::unique_ptr<StyioAST> parse_cases (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::vector<std::tuple<std::unique_ptr<StyioAST>, std::unique_ptr<StyioAST>>> pairs;
  std::unique_ptr<StyioAST> _default_stmt;

  /*
    Danger!
    when entering parse_cases(), 
    the cur_char must be {
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  while (true)
  {
    drop_spaces_and_comments(code, cur_char);
    if (check_and_drop_char(code, cur_char, '_')) {

      find_and_drop_symbol(code, cur_char, "=>");
      
      drop_spaces_and_comments(code, cur_char);
      
      if (check_char(cur_char, '{')) {
        _default_stmt = parse_block(code, cur_char); }
      else {
        _default_stmt = parse_stmt(code, cur_char); }

      break;
    }

    std::unique_ptr<StyioAST> left = parse_expr(code, cur_char);
    
    find_and_drop_symbol(code, cur_char, "=>");

    drop_spaces_and_comments(code, cur_char);

    std::unique_ptr<StyioAST> right;
    if (check_char(cur_char, '{')) {
      right = parse_block(code, cur_char); }
    else {
      right = parse_stmt(code, cur_char); }

    pairs.push_back(std::make_tuple(std::move(left), std::move(right)));
  }

  find_and_drop_char_panic(code, cur_char, '}');

  if (pairs.size() == 0) {
    return std::make_unique<CasesAST>(
      std::move(_default_stmt)); }
  else {
    return std::make_unique<CasesAST>(
      std::move(pairs), 
      std::move(_default_stmt)); }
}

std::unique_ptr<StyioAST> parse_block (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::vector<std::unique_ptr<StyioAST>> stmtBuffer;

  /*
    Danger!
    when entering parse_block(), 
    the cur_char must be {
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  while (true)
  {
    drop_spaces_and_comments(code, cur_char);
    
    if (check_and_drop_char(code, cur_char, '}')) {
      break; }
    else {
      stmtBuffer.push_back(std::move(parse_stmt(code, cur_char))); };
  };

  if (stmtBuffer.size() == 0) {
    return std::make_unique<EmptyBlockAST>(); }
  else {
    return std::make_unique<BlockAST>(std::move(stmtBuffer));};
}

void parse_program (std::string styio_code) 
{
  struct StyioCodeContext styio_code_context = { styio_code, 0 };

  StyioCodeContext* ctx_ptr = &styio_code_context;

  char cur_char = styio_code.at(0);

  while (true) {
    std::unique_ptr<StyioAST> stmt = parse_stmt(ctx_ptr, cur_char);

    if ((stmt -> hint()) == StyioType::End) { break; }

    if ((stmt -> hint()) != StyioType::Comment) {
      std::cout << "\033[1;33m[>_<]\033[0m " << stmt -> toString(0, false) << std::endl; }
    
    // std::cout << "\033[1;33m[>_<]\033[0m" << std::endl;
  };
}