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

inline bool check_binop_token (
  struct StyioCodeContext* code) {
  int pos = code -> cursor;
  while (isspace((code -> text.at(pos)))) {
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
    pos += 1;
  }

  return check_char(code -> text.at(pos), value);
}

void drop_white_spaces (
  struct StyioCodeContext* code,
  char& cur_char) {
  while (check_char(cur_char, ' ')) {
    move_to_the_next_char(code, cur_char);
  };
}

void drop_spaces (
  struct StyioCodeContext* code,
  char& cur_char) {
  while (isspace(cur_char)) {
    move_to_the_next_char(code, cur_char);
  };
}

void drop_spaces_and_comments (
  struct StyioCodeContext* code,
  char& cur_char) {
  /*
    Danger: No bound check!
  */
  while (true) {
    if (isspace(cur_char)) {
      move_to_the_next_char(code, cur_char);
    }
    else if (check_symbol(code, "//")) {
      pass_over_char(code, cur_char, '\n');
    }
    else if (check_symbol(code, "/*")) {
      pass_over_symbol(code, cur_char, "*/");
    }
    else {
      break;
    } 
  };
}

void move_until_char (
  struct StyioCodeContext* code,
  char& cur_char,
  char value) {
  /*
    Danger: No bound check!
  */
  while (true) {
    if (check_char(cur_char, value)) {
      break;
    }
    else {
      move_to_the_next_char(code, cur_char);
    }
  }
}

void pass_over_char (
  struct StyioCodeContext* code,
  char& cur_char,
  char value) {
  /*
    Danger: No bound check!
  */
  while (true) {
    if (check_char(cur_char, value)) {
      move_to_the_next_char(code, cur_char);
      break;
    }
    else {
      move_to_the_next_char(code, cur_char);
    }
  }
}

void pass_over_symbol (
  struct StyioCodeContext* code,
  char& cur_char,
  std::string value) {
  /*
    Danger!
  */
  while (true) {
    while (not check_char(cur_char, value.at(0))) {
      move_to_the_next_char(code, cur_char);
    }

    if (check_symbol(code, value)) {
      move_forward(code, cur_char, value.length());
      break;
    }
  }
}

void find_char_panic (
  struct StyioCodeContext* code,
  char& cur_char,
  char value) {
  drop_spaces(code, cur_char);

  if (not check_char(cur_char, value)) {
    std::string errmsg = std::string("Expecting ") + char(value) + " but got " + char(cur_char);
    throw StyioSyntaxError(errmsg);
  }
}

void find_symbol_panic (
  struct StyioCodeContext* code,
  char& cur_char,
  std::string value) {
  drop_spaces(code, cur_char);

  if (not check_symbol(code, value)) {
    std::string errmsg = std::string("Expecting ") + value + " but got " + code -> text.substr(code -> cursor, value.length());
    throw StyioSyntaxError(errmsg);
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
    return true;
  }

  return false;
}

inline bool check_and_drop_symbol (
  struct StyioCodeContext* code, 
  char& cur_char, 
  std::string value) {
  if ((code -> text.substr(code -> cursor, value.length())) == value) {
    move_forward(code, cur_char, value.length());
    return true;
  }
  else {
    return false;
  }
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
      move_to_the_next_char(code, cur_char);
    }
    else if (check_symbol(code, "//")) {
      pass_over_char(code, cur_char, '\n');
    }
    else if (check_symbol(code, "/*")) {
      pass_over_symbol(code, cur_char, "*/");
    }
    else {
      if (check_char(cur_char, value)) {
        move_to_the_next_char(code, cur_char);
        return true;
      }
      
      return false;
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
      move_to_the_next_char(code, cur_char);
    }
    else if (check_symbol(code, "//")) {
      pass_over_char(code, cur_char, '\n');
    }
    else if (check_symbol(code, "/*")) {
      pass_over_symbol(code, cur_char, "*/");
    }
    else {
      if (check_symbol(code, value)) {
        move_forward(code, cur_char, value.length());
        return true;
      }

      return false;
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
      move_to_the_next_char(code, cur_char);
    }
    else if (check_symbol(code, "//")) {
      pass_over_char(code, cur_char, '\n');
    }
    else if (check_symbol(code, "/*")) {
      pass_over_symbol(code, cur_char, "*/");
    }
    else {
      if (check_char(cur_char, value)) {
        move_to_the_next_char(code, cur_char);
        return true;
      }
      else {
        std::string errmsg = std::string("Expecting: ") + char(value) + "\n" 
          + "But Got: " + char(cur_char);
        throw StyioSyntaxError(errmsg);
      }
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
      move_to_the_next_char(code, cur_char);
    }
    else if (check_symbol(code, "//")) {
      pass_over_char(code, cur_char, '\n');
    }
    else if (check_symbol(code, "/*")) {
      pass_over_symbol(code, cur_char, "*/");
    }
    else {
      if ((code -> text.substr(code -> cursor, value.length())) == value) {
        move_forward(code, cur_char, value.length());
        return true;
      }
      else {
        std::string errmsg = std::string("Expecting: ") + value + "\n" 
          + "But Got: " + code -> text.substr(code -> cursor, value.length());
        throw StyioSyntaxError(errmsg);
      }
    } 
  }
}

inline bool match_next_char_panic (
  struct StyioCodeContext* code, 
  char& cur_char,
  char value) {
  if (check_char(cur_char, value)) {
    move_to_the_next_char(code, cur_char);
    return true;
  }

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
  } while (
    isalnum((cur_char)) 
    || check_char(cur_char, '_')
  );

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

  while (isdigit(cur_char))
  {
    intStr += cur_char;
    move_to_the_next_char(code, cur_char);
  };

  return std::make_unique<IntAST>(intStr);
}

std::unique_ptr<StyioAST> parse_int_or_float 
(
  struct StyioCodeContext* code, 
  char& cur_char
)
{
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
    move_to_the_next_char(code, cur_char);
  };

  if (check_char(cur_char, '.')) {
    move_to_the_next_char(code, cur_char);

    if (isdigit(cur_char)) {
      numStr += '.';

      while (isdigit(cur_char)) {
        numStr += cur_char;
        move_to_the_next_char(code, cur_char);
      };

      output = std::make_unique<FloatAST>(numStr);
    }
    else {
      output = std::make_unique<IntAST>(numStr);
    };
  } 
  else {
    output = std::make_unique<IntAST>(numStr);
  }

  return output;
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
    move_to_the_next_char(code, cur_char);
  };

  // eliminate " at the end
  move_to_the_next_char(code, cur_char);

  return std::make_unique<StringAST>(textStr);
}

std::unique_ptr<StyioAST> parse_char_or_string (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output;

  /*
    Danger!
    when entering parse_char_or_string(), 
    the cur_char must be '
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  std::string textStr = "";
  
  while (cur_char != '\'') {
    textStr += cur_char;
    move_to_the_next_char(code, cur_char);
  };

  // eliminate ' at the end
  move_to_the_next_char(code, cur_char);

  if (textStr.length() == 1) {
    output = std::make_unique<CharAST>(textStr);
  }
  else {
    output = std::make_unique<StringAST>(textStr);
  }

  return output;
}

std::unique_ptr<StyioAST> parse_path_or_link (
  struct StyioCodeContext* code, 
  char& cur_char
)
{
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

      if (check_char(cur_char, ')')) {
        // eliminate )
        move_to_the_next_char(code, cur_char);
      }
      else
      {
        std::string errmsg = std::string("@(___) // Resource: Expecting ) at the end, but got .:| ") + char(cur_char) + " |:.";
        throw StyioSyntaxError(errmsg);
      };

      output = std::make_unique<ExtPathAST>(std::move(path));
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

std::unique_ptr<CommentAST> parse_comment (
  struct StyioCodeContext* code,
  char& cur_char,
  int mode) {
  std::string commentText = "";

  if (mode == 1) {
    while (not check_char(cur_char, '\n'))
    {
      commentText += char(cur_char);

      move_to_the_next_char(code, cur_char);
    }

    return std::make_unique<CommentAST>(commentText);
  }
  else if (mode == 2) {
    while (true)
    {
      while (not check_char(cur_char, '*'))
      {
        commentText += char(cur_char);

        move_to_the_next_char(code, cur_char);
      }

      // eliminate the detected *
      move_to_the_next_char(code, cur_char);

      if (check_char(cur_char, '/')) {
        move_to_the_next_char(code, cur_char);

        return std::make_unique<CommentAST>(commentText);
      }
    }
  }

  return std::make_unique<CommentAST>(commentText);
}

/*
  Basic Collection
  - Filling (Variable Tuple)
  - Resources
*/

std::unique_ptr<FillingAST> parse_filling (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<FillingAST> output;

  std::vector<std::unique_ptr<StyioAST>> vars;

  /*
    Danger!
    when entering parse_filling(), 
    the cur_char must be (
    this line will drop the next 1 character anyway!
  */
  move_to_the_next_char(code, cur_char);

  vars.push_back(std::move(parse_id(code, cur_char)));

  drop_spaces(code, cur_char);

  while (check_char(cur_char, ','))
  {
    move_to_the_next_char(code, cur_char);

    drop_spaces(code, cur_char);

    if (check_char(cur_char, ')'))
    {
      break;
    };

    vars.push_back(std::move(parse_id(code, cur_char)));
  }

  find_and_drop_char_panic(code, cur_char, ')');

  output = std::make_unique<FillingAST>(std::move(vars));

  return output;
}

std::unique_ptr<ResourceAST> parse_resources (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<ResourceAST> output;

  std::vector<std::unique_ptr<StyioAST>> resources;

  // eliminate @ at the start
  move_to_the_next_char(code, cur_char);

  if (check_char(cur_char, '(')) 
  {
    move_to_the_next_char(code, cur_char);

    drop_spaces(code, cur_char);

    if (isalpha(cur_char) || check_char(cur_char, '_')) {
      // "@" "(" |--
      std::unique_ptr<IdAST> tmp_res_name = parse_id(code, cur_char);

      drop_spaces(code, cur_char);

      if (check_char(cur_char, '<')) {
        move_to_the_next_char(code, cur_char);

        match_next_char_panic(code, cur_char, '-');
      };

      drop_spaces(code, cur_char);

      std::unique_ptr<StyioAST> tmp_expr = std::make_unique<FinalBindAST>(
        std::move(tmp_res_name), 
        parse_value(code, cur_char));

      resources.push_back(std::move(tmp_expr));
    };

    drop_spaces(code, cur_char);

    // "@" "(" [<ID> |--
    while (check_char(cur_char, ','))
    {
      move_to_the_next_char(code, cur_char);

      drop_spaces(code, cur_char);

      if (isalpha(cur_char) || check_char(cur_char, '_')) {
        
        std::unique_ptr<IdAST> tmp_res_name = parse_id(code, cur_char);

        drop_spaces(code, cur_char);

        if (check_char(cur_char, '<')) {
          move_to_the_next_char(code, cur_char);

          match_next_char_panic(code, cur_char, '-');
        };

        drop_spaces(code, cur_char);

        std::unique_ptr<StyioAST> tmp_value = parse_value(code, cur_char);

        resources.push_back(std::move(
          std::make_unique<FinalBindAST>(
            std::move(tmp_res_name), 
            std::move(tmp_value))));
      };
    };
    
    if (check_char(cur_char, ')')) 
    {
      move_to_the_next_char(code, cur_char);
    }
    else
    {
      std::string errmsg = std::string("@(expr) // Expecting ) at the end, but got ") + char(cur_char) + "";
      throw StyioSyntaxError(errmsg);
    };

    
    output = std::make_unique<ResourceAST>(std::move(resources));
  }
  else
  {
    std::string errmsg = std::string("@(expr) // Expecting ( after @, but got ") + char(cur_char) + "";
    throw StyioSyntaxError(errmsg);
  };

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

  if (isalpha(cur_char) || check_char(cur_char, '_')) 
  {
    output = parse_id(code, cur_char);
  }

  if (check_char(cur_char, '[')) {
    output = parse_list_op(code, cur_char, std::move(output));
  } 
  else if (check_char(cur_char, '(')) {
    output = parse_call(code, cur_char);
  }

  drop_spaces(code, cur_char);

  if (check_binop_token(code)) {
    output = parse_binop_rhs(code, cur_char, std::move(output));
  };

  return output;
}

std::unique_ptr<StyioAST> parse_value (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output;

  if (isalpha(cur_char) || check_char(cur_char, '_')) 
  {
    output = parse_id_or_value(code, cur_char);

    return output;
  }
  else if (isdigit(cur_char)) 
  {
    return parse_int_or_float(code, cur_char);
  }
  else if (check_char(cur_char, '|'))
  {
    return parse_size_of(code, cur_char);
  };

  std::string errmsg = std::string("parse_value() // Unexpected value expression, starting with .:| ") + char(cur_char) + " |:.";
  throw StyioParseError(errmsg);
}

std::unique_ptr<StyioAST> parse_item_for_binop (
  struct StyioCodeContext* code, 
  char& cur_char
)
{
  std::unique_ptr<StyioAST> output (new NoneAST());

  // <ID>
  if (isalpha(cur_char) || check_char(cur_char, '_')) 
  {
    // parse id
    output = parse_id(code, cur_char);

    return output;
  }
  else
  if (isdigit(cur_char)) {
    output = parse_int_or_float(code, cur_char);

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

      drop_white_spaces(code, cur_char);

      if (check_char(cur_char, ']')) {
        move_to_the_next_char(code, cur_char);

        output = std::make_unique<EmptyListAST>();
      }
      else
      {
        output = parse_list_expr(code, cur_char);
      }
    }

    // You should NOT reach this line!
    break;

  case '|':
    {
      output = parse_size_of(code, cur_char);
    }

    // You should NOT reach this line!
    break;
  
  default:
    break;
  }

  return output;
}

std::unique_ptr<StyioAST> parse_expr (
  struct StyioCodeContext* code, 
  char& cur_char
)
{
  std::unique_ptr<StyioAST> output (new NoneAST());

  // <ID>
  if (isalpha(cur_char) || check_char(cur_char, '_')) 
  {
    // parse id
    output = parse_id(code, cur_char);
    
    // ignore white spaces after id
    drop_white_spaces(code, cur_char);

    if (check_binop_token(code))
    {
      output = parse_binop_rhs(code, cur_char, std::move(output));
    };

    return output;
  }
  else
  if (isdigit(cur_char)) {
    output = parse_int_or_float(code, cur_char);

    // ignore white spaces after number
    drop_white_spaces(code, cur_char);

    if (check_binop_token(code))
    {
      output = parse_binop_rhs(code, cur_char, std::move(output));
    };

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

      drop_white_spaces(code, cur_char);

      if (check_char(cur_char, ']')) {
        move_to_the_next_char(code, cur_char);

        output = std::make_unique<EmptyListAST>();
      }
      else
      {
        output = parse_list_expr(code, cur_char);
      }
    }

    // You should NOT reach this line!
    break;

  case '|':
    {
      output = parse_size_of(code, cur_char);

      drop_white_spaces(code, cur_char);

      if (check_binop_token(code))
      {
        output = parse_binop_rhs(code, cur_char, std::move(output));
      }
    }

    // You should NOT reach this line!
    break;
  
  default:
    break;
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
  std::unique_ptr<StyioAST> theList
) 
{
  // eliminate [ at the start
  move_to_the_next_char(code, cur_char);

  std::unique_ptr<ListOpAST> output;

  if (isdigit(cur_char))
  {
    output = std::make_unique<ListOpAST>(
      ListOpType::Access_Via_Index,
      std::move(theList), 
      parse_int(code, cur_char));
  }
  else
  {
    switch (cur_char)
    {
    case '"':
      {
        /*
          list["any"]
        */
        
        output = std::make_unique<ListOpAST>(
          ListOpType::Access_Via_Name,
          std::move(theList), 
          parse_string(code, cur_char));
      }
      
      // You should NOT reach this line!
      break;

    case '<':
      {
        /*
          list[<]
        */

        move_to_the_next_char(code, cur_char);

        output = std::make_unique<ListOpAST>(
          ListOpType::Get_Reversed,
          std::move(theList));
      }

      // You should NOT reach this line!
      break;

    // list[?= item]
    case '?':
      {
        move_to_the_next_char(code, cur_char);

        if (check_char(cur_char, '='))
        {
          move_to_the_next_char(code, cur_char);

          output = std::make_unique<ListOpAST>(
            ListOpType::Get_Index_By_Item,
            std::move(theList), 
            parse_expr(code, cur_char));
        }
        else
        {
          std::string errmsg = std::string("Missing `=` for `?=` after `?= item`, but got `") + char(cur_char) + "`";
          throw StyioSyntaxError(errmsg);
        };
      }

      // You should NOT reach this line!
      break;
    
    case '+':
      {
        move_to_the_next_char(code, cur_char);

        if (check_char(cur_char, ':'))
        {
          move_to_the_next_char(code, cur_char);

          // eliminate white spaces after +:
          drop_white_spaces(code, cur_char);

          if (isdigit(cur_char))
          {
            /*
              list[+: index <- value]
            */

            std::unique_ptr<IntAST> theIndex = parse_int(code, cur_char);

            // eliminate white spaces between index and <-
            drop_white_spaces(code, cur_char);

            if (check_char(cur_char, '<'))
            {
              move_to_the_next_char(code, cur_char);

              if (check_char(cur_char, '-'))
              {
                move_to_the_next_char(code, cur_char);

                // eliminate white spaces between <- and the value to be inserted
                drop_white_spaces(code, cur_char);

                // the item to be inserted into the list
                std::unique_ptr<StyioAST> theItemIns = parse_expr(code, cur_char);

                output = std::make_unique<ListOpAST>(
                  ListOpType::Insert_Item_By_Index,
                  std::move(theList), 
                  std::move(theIndex),
                  std::move(theItemIns));
              }
              else
              {
                std::string errmsg = std::string("Missing `-` for `<-` after `+: index`, got `") + char(cur_char) + "`";
                throw StyioSyntaxError(errmsg);
              };
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

        if (check_char(cur_char, ':'))
        {
          move_to_the_next_char(code, cur_char);

          // eliminate white spaces after -:
          drop_white_spaces(code, cur_char);

          if (isdigit(cur_char))
          {
            /*
              list[-: index]
            */

            std::unique_ptr<IntAST> theIndex = parse_int(code, cur_char);

            // eliminate white spaces between index
            drop_white_spaces(code, cur_char);

            output = std::make_unique<ListOpAST>(
              ListOpType::Remove_Item_By_Index,
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

              // eliminate ( at the start
              move_to_the_next_char(code, cur_char);
              
              // drop white spaces between '(' and the first index
              drop_white_spaces(code, cur_char);

              std::vector<std::unique_ptr<IntAST>> indices;

              indices.push_back(std::move(parse_int(code, cur_char)));

              // drop white spaces between first index and ,
              drop_white_spaces(code, cur_char);

              while (check_char(cur_char, ','))
              {
                // remove ,
                move_to_the_next_char(code, cur_char);

                // drop white spaces between , and next index
                drop_white_spaces(code, cur_char);

                if (check_char(cur_char, ')'))
                {
                  break;
                }

                indices.push_back(std::move(parse_int(code, cur_char)));
              }

              // drop white spaces between , and )
              drop_white_spaces(code, cur_char);
              
              if (check_char(cur_char, ')'))
              {
                move_to_the_next_char(code, cur_char);

                output = std::make_unique<ListOpAST>(
                  ListOpType::Remove_Many_Items_By_Indices,
                  std::move(theList), 
                  std::move(indices));
              }
              else
              {
                std::string errmsg = std::string("Expecting `)` after `-: (i0, i1, ...`, but got `") + char(cur_char) + "`";
                throw StyioSyntaxError(errmsg);
              }
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
                  ListOpType::Remove_Item_By_Value,
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

  // check ] at the end
  if (check_char(cur_char, ']'))
  {
    // eliminate ] at the end
    move_to_the_next_char(code, cur_char);
    
    return output;
  }
  else
  {
    std::string errmsg = std::string("Missing `]` after List[Operation], got `") + char(cur_char) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

std::unique_ptr<StyioAST> parse_iter (
  struct StyioCodeContext* code, 
  char& cur_char,
  std::unique_ptr<StyioAST> iterOverIt) {
  std::unique_ptr<FillingAST> iterTmpVars;
  std::unique_ptr<StyioAST> iterMatch;
  std::unique_ptr<StyioAST> iterFilter;
  std::unique_ptr<StyioAST> iterBlock;

  bool hasVars = false;
  bool hasMatch = false;
  bool hasFilter = false;

  drop_spaces(code, cur_char);

  // eliminate the start
  if (check_char(cur_char, '('))
  {
    iterTmpVars = parse_filling(code, cur_char);
    hasVars = true;
  }

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

  drop_spaces(code, cur_char);

  if (check_char(cur_char, '?'))
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

        // drop white spaces after ?=
        drop_white_spaces(code, cur_char);
        
        iterMatch = std::make_unique<CheckEqAST>(parse_value(code, cur_char));
        hasMatch = true;
      }

      break;
    /*
      ?^ [Iterable]
    */
    case '^':
      {

      }

      break;

    /*
      ?(Condition) 
      \t\ {

      }
      
      ?(Condition) 
      \f\ {

      }
    */
    case '(':
      {

      }

      break;
    
    default:
      std::string errmsg = std::string("Unexpected character after ?: `") + char(cur_char) + "`";
      throw StyioSyntaxError(errmsg);

      break;
    }
  };

  if (check_char(cur_char, '='))
  {
    move_to_the_next_char(code, cur_char);

    if (check_char(cur_char, '>'))
    {
      move_to_the_next_char(code, cur_char);
    }
    else
    {
      std::string errmsg = std::string("Missing `>` for `=>`.");
      throw StyioSyntaxError(errmsg);
    };
  };

  /*
    support:

    => \n
    { }
  */
  drop_spaces(code, cur_char);

  if (check_char(cur_char, '{'))
  {
    iterBlock = parse_exec_block(code, cur_char);
  }
  else
  {
    std::string errmsg = std::string("Cannot find block after `=>`.");
    throw StyioSyntaxError(errmsg);
  };

  if ((iterOverIt -> hint()) == StyioType::InfLoop)
  {
    if (hasVars)
    {
      return std::make_unique<LoopAST>(
        std::move(iterTmpVars), 
        std::move(iterBlock));
    }
    else
    {
      return std::make_unique<LoopAST>(
        std::move(iterBlock));
    };
  }
  else if ((iterOverIt -> hint()) == StyioType::List 
    || (iterOverIt -> hint()) == StyioType::Range)
  {
    if (hasVars)
    {
      return std::make_unique<IterBounded>(
        std::move(iterOverIt), 
        std::move(iterTmpVars), 
        std::move(iterBlock));
    }
    else
    {
      return std::make_unique<IterBounded>(
        std::move(iterOverIt), 
        std::move(iterBlock));
    };
  }
  else
  {
    std::string errmsg = std::string("Cannot recognize the collection for the iterator: ") + std::to_string(type_to_int(iterOverIt -> hint()));
    throw StyioSyntaxError(errmsg);
  };
}

std::unique_ptr<StyioAST> parse_list_expr (
  struct StyioCodeContext* code, 
  char& cur_char
) 
{
  std::unique_ptr<StyioAST> output;

  std::vector<std::unique_ptr<StyioAST>> elements;

  std::unique_ptr<StyioAST> startEl = parse_expr(code, cur_char);
  elements.push_back(std::move(startEl));

  drop_spaces(code, cur_char);

  switch (cur_char)
  {
  case '.':
    {
      move_to_the_next_char(code, cur_char);

      while (check_char(cur_char, '.'))
      {
        move_to_the_next_char(code, cur_char);
      }
      
      std::unique_ptr<StyioAST> endEl = parse_expr(code, cur_char);

      std::unique_ptr<StyioAST> list_loop;

      if (startEl -> hint() == StyioType::Int 
        && endEl -> hint() == StyioType::Id)
      {
        list_loop = std::make_unique<InfiniteAST>(
          std::move(startEl), 
          std::move(endEl));
      }
      else
      if (startEl -> hint() == StyioType::Int 
        && endEl -> hint() == StyioType::Int)
      {
        list_loop = std::make_unique<RangeAST>(
          std::move(startEl), 
          std::move(endEl), 
          std::make_unique<IntAST>("1"));
      }
      else
      {
        std::string errmsg = std::string("Unexpected Range / List / Loop: ")
          + "Start <"
          + std::to_string(type_to_int(startEl -> hint()))
          + ">, "
          + "End <"
          + std::to_string(type_to_int(endEl -> hint()))
          + ">.";
        throw StyioSyntaxError(errmsg);
      }

      if (check_char(cur_char, ']')) 
      {
        move_to_the_next_char(code, cur_char);
      }
      else
      {
        std::string errmsg = std::string("Missing `]` after List / Range / Loop: `") + char(cur_char) + "`";
        throw StyioSyntaxError(errmsg);
      };

      drop_white_spaces(code, cur_char);

      switch (cur_char)
      {
      case '\n':
        {
          // If: LF, Then: Statement Ends
          return list_loop;
        }
        
        // You should NOT reach this line!
        break;

      case '>':
        {
          move_to_the_next_char(code, cur_char);

          if (check_char(cur_char, '>'))
          {
            // If: >>, Then: Iteration
            move_to_the_next_char(code, cur_char);
            
            return parse_iter(code, cur_char, std::move(list_loop));
          }
        }
        
        // You should NOT reach this line!
        break;

      case '[':
        {
          return parse_list_op(code, cur_char, std::move(list_loop));
        }
        
        // You should NOT reach this line!
        break;
      
      default:
        {
          std::string errmsg = std::string("Unexpected character after List / Range / Loop: `") + char(cur_char) + "`";
          throw StyioSyntaxError(errmsg);
        };
        
        // You should NOT reach this line!
        break;
      }
    }

    // You should not reach this line!
    break;

  case ',':
    {
      std::unique_ptr<ListAST> theList;

      while (check_char(cur_char, ','))
      {
        // eliminate ,
        move_to_the_next_char(code, cur_char);

        drop_white_spaces(code, cur_char);

        if (check_char(cur_char, ']')) 
        {
          move_to_the_next_char(code, cur_char);

          theList = std::make_unique<ListAST>(std::move(elements));
        };

        elements.push_back(std::move(parse_value(code, cur_char)));
      };

      drop_white_spaces(code, cur_char);

      if (check_char(cur_char, ']')) 
      {
        move_to_the_next_char(code, cur_char);

        theList = std::make_unique<ListAST>(std::move(elements));
      }
      else
      {
        std::string errmsg = std::string("Missing `]` after List / Range / Loop: `") + char(cur_char) + "`";
        throw StyioSyntaxError(errmsg);
      };

      drop_white_spaces(code, cur_char);

      switch (cur_char)
      {
      case '\n':
        {
          // If: LF, Then: Statement Ends
          return theList;
        }
        
        // You should NOT reach this line!
        break;

      case '>':
        {
          move_to_the_next_char(code, cur_char);

          if (check_char(cur_char, '>'))
          {
            // If: >>, Then: Iteration
            move_to_the_next_char(code, cur_char);

            return parse_iter(code, cur_char, std::move(theList));
          }

          // TODO: Iteration Over List / Range / Loop

          return theList;
        }
        
        // You should NOT reach this line!
        break;

      case '[':
        {
          output = parse_list_op(code, cur_char, std::move(theList));
        }
        
        // You should NOT reach this line!
        break;
      
      default:
        {
          std::string errmsg = std::string("Unexpected character after List / Range / Loop: `") + char(cur_char) + "`";
          throw StyioSyntaxError(errmsg);
        };
        
        // You should NOT reach this line!
        break;
      }
    }

    // You should not reach this line!
    break;
  
  default:
    break;
  }

  return output;
}

std::unique_ptr<StyioAST> parse_loop (
  struct StyioCodeContext* code, 
  char& cur_char
)
{
  std::unique_ptr<StyioAST> output;

  while (check_char(cur_char, '.')) 
  { 
    // eliminate all .
    move_to_the_next_char(code, cur_char);

    if (check_char(cur_char, ']')) 
    {
      move_to_the_next_char(code, cur_char);

      break;
    };
  };

  // eliminate white spaces after [...]
  drop_white_spaces(code, cur_char);

  if (isdigit(cur_char))
  { 
    std::string errmsg = std::string("A finite list must have both start and end values.");
    throw StyioSyntaxError(errmsg);
  };

  switch (cur_char)
  {
  case '>':
    {
      move_to_the_next_char(code, cur_char);

      if (check_char(cur_char, '>'))
      {
        move_to_the_next_char(code, cur_char);

        // eliminate white spaces after >>
        drop_white_spaces(code, cur_char);

        if (isalpha(cur_char) 
            || check_char(cur_char, '_')
            || check_char(cur_char, '(')) 
        {
          output = parse_iter(code, cur_char, std::unique_ptr<StyioAST>(new InfiniteAST()));
        }
        else
        if (check_char(cur_char, '{'))
        {
          /*
            the { at the start will be eliminated inside parse_exec_block() function
          */

          output = std::make_unique<LoopAST>(parse_exec_block(code, cur_char));
        }
      }
    }

    // You should not reach this line!
    break;

  case '(':
    {
      output = parse_iter(code, cur_char, std::make_unique<InfiniteAST>());
    }

    // You should not reach this line!
    break;
  
  case '\n':
    {
      output = std::unique_ptr<InfiniteAST>(new InfiniteAST());
    }

    // You should not reach this line!
    break;

  default:
    break;
  }

  return output;
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
  char& cur_char
)
{
  // eliminate ?
  move_to_the_next_char(code, cur_char);

  std::unique_ptr<CondAST> condition;
  
  if (check_char(cur_char, '(')) {
    move_to_the_next_char(code, cur_char);

    condition = parse_cond(code, cur_char);

    find_and_drop_char_panic(code, cur_char, ')');

    /*
      support:
        ?() \n
        \t\

        ?() \n
        \f\
    */
    drop_spaces(code, cur_char);

    if (check_char(cur_char, '\\'))
    {
      move_to_the_next_char(code, cur_char);

      std::unique_ptr<StyioAST> block;

      if (check_char(cur_char, 't'))
      {
        move_to_the_next_char(code, cur_char);

        match_next_char_panic(code, cur_char, '\\');

        /*
          support:
            \t\ \n
            {}
        */

        drop_spaces(code, cur_char);

        block = parse_exec_block(code, cur_char);

        /*
          support:
            \t\ {} \n
            \f\
        */
        drop_spaces(code, cur_char);

        if (check_char(cur_char, '\\'))
        {
          move_to_the_next_char(code, cur_char);

          match_next_char_panic(code, cur_char, 'f');

          match_next_char_panic(code, cur_char, '\\');

          /*
            support:
              \f\ \n
              {}
          */
          drop_spaces(code, cur_char);

          std::unique_ptr<StyioAST> blockElse = parse_exec_block(code, cur_char);

          return std::make_unique<CondFlowAST>(
            FlowType::Both,
            std::move(condition),
            std::move(block),
            std::move(blockElse)
          );
        }
        else
        {
          return std::make_unique<CondFlowAST>(
            FlowType::True,
            std::move(condition),
            std::move(block)
          );
        };
      }
      else if (check_char(cur_char, 'f'))
      {
        move_to_the_next_char(code, cur_char);

        match_next_char_panic(code, cur_char, '\\');

        /*
          support:
            \f\ \n
            {}
        */
        drop_spaces(code, cur_char);

        block = parse_exec_block(code, cur_char);

        return std::make_unique<CondFlowAST>(
          FlowType::False,
          std::move(condition),
          std::move(block)
        );
      }
      else
      {

      }
    }
  }
  else 
  {
    std::string errmsg = std::string("Missing `(` for ?(`expr`).");
    throw StyioSyntaxError(errmsg);
  };

  return condition;
}

std::unique_ptr<FlexBindAST> parse_mut_assign (
  struct StyioCodeContext* code, 
  char& cur_char, 
  std::unique_ptr<IdAST> id_ast
)
{
  std::unique_ptr<FlexBindAST> output = std::make_unique<FlexBindAST>(
    std::move(id_ast), 
    std::move(parse_expr(code, cur_char)));
  
  if (check_char(cur_char, '\n')) 
  {
    return output;
  }
  else
  {
    std::string errmsg = std::string("Unexpected character after binding (flexible): ") + char(cur_char);
    throw StyioSyntaxError(errmsg);
  }
}

std::unique_ptr<FinalBindAST> parse_fix_assign (
  struct StyioCodeContext* code, 
  char& cur_char, 
  std::unique_ptr<IdAST> id_ast) {
  std::unique_ptr<FinalBindAST> output = std::make_unique<FinalBindAST>(
    std::move(id_ast), 
    std::move(parse_expr(code, cur_char)));
  
  if (check_char(cur_char, '\n')) 
  {
    return output;
  }
  else
  {
    std::string errmsg = std::string("Unexpected character after binding (fixed): `") + char(cur_char) + "` after Assign(Mutable)";
    throw StyioSyntaxError(errmsg);
  }
}

std::unique_ptr<StyioAST> parse_pipeline (
  struct StyioCodeContext* code, 
  char& cur_char
)
{
  std::unique_ptr<IdAST> pipeName;
  std::unique_ptr<FillingAST> pipeVars;
  std::unique_ptr<StyioAST> pipeBlock;
  bool pwithName = false;
  bool pisFinal = false;

  // eliminate # at the start
  move_to_the_next_char(code, cur_char);

  // after #
  drop_white_spaces(code, cur_char);

  if (isalpha(cur_char) 
    || check_char(cur_char, '_'))
  {
    pipeName = parse_id(code, cur_char);

    pwithName = true;
  };
  
  // after function name
  drop_spaces(code, cur_char);

  if (check_char(cur_char, ':'))
  {
    move_to_the_next_char(code, cur_char);

    pisFinal = true;
  };

  if (check_char(cur_char, '='))
  {
    move_to_the_next_char(code, cur_char);
  };

  drop_spaces(code, cur_char);

  if (check_char(cur_char, '('))
  {
    pipeVars = parse_filling(code, cur_char);
  }
  else
  {
    std::string errmsg = std::string("Expecting ( after function name, but got ") + char(cur_char);
    throw StyioSyntaxError(errmsg);
  };

  drop_spaces(code, cur_char);
  
  if (check_char(cur_char, '='))
  {
    move_to_the_next_char(code, cur_char);

    if (check_char(cur_char, '>'))
    {
      move_to_the_next_char(code, cur_char);
    };
  };

  drop_spaces(code, cur_char);

  if (check_char(cur_char, '{'))
  {
    pipeBlock = parse_exec_block(code, cur_char);

    if (pwithName)
    {
      return std::make_unique<FuncAST>(
        std::move(pipeName),
        std::make_unique<ForwardAST>(std::move(pipeVars), std::move(pipeBlock)),
        pisFinal
      );
    }
    else
    {
      return std::make_unique<FuncAST>(
        std::make_unique<ForwardAST>(std::move(pipeBlock)),
        pisFinal
      );
    };
  }
  
  std::string errmsg = std::string("Something wrong with parse_pipeline, got ") + char(cur_char);
  throw StyioSyntaxError(errmsg);
}

std::unique_ptr<StyioAST> parse_read_file (
  struct StyioCodeContext* code, 
  char& cur_char, 
  std::unique_ptr<IdAST> id_ast
) 
{
  if (check_char(cur_char, '@'))
  {
    return std::make_unique<ReadFileAST>(std::move(id_ast), parse_path_or_link(code, cur_char));
  }
  else
  {
    std::string errmsg = std::string("Unexpected Read.Path, starts with character `") + char(cur_char) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

std::unique_ptr<StyioAST> parse_print (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output;

  /*
    Danger!
    when entering parse_print(), 
    the following symbol must be >_
    this line will drop the next 2 characters anyway!
  */
  move_forward(code, cur_char, 2);

  match_next_char_panic(code, cur_char, '(');

  drop_spaces(code, cur_char);

  output = std::make_unique<PrintAST>(parse_expr(code, cur_char));

  find_and_drop_char_panic(code, cur_char, ')');

  return output;
}

std::unique_ptr<StyioAST> parse_stmt (
  struct StyioCodeContext* code, 
  char& cur_char) {
  // <ID>
  if (isalpha(cur_char) || check_char(cur_char, '_')) 
  {
    std::unique_ptr<IdAST> id_ast = parse_id(code, cur_char);

    if (check_char(cur_char, '[')) {
      return parse_list_op(code, cur_char, std::move(id_ast));
    }
    
    drop_spaces_and_comments(code, cur_char);

    if (check_binop_token(code)) {
      return parse_binop_rhs(code, cur_char, std::move(id_ast));
    } 

    switch (cur_char)
    {
      case '=':
        {
          move_to_the_next_char(code, cur_char);

          drop_spaces_and_comments(code, cur_char);

          return parse_mut_assign(code, cur_char, std::move(id_ast));
        };

        // You should NOT reach this line!
        break;
      
      case ':':
        {
          move_to_the_next_char(code, cur_char);

          if (check_and_drop_char(code, cur_char, '=')) {
            drop_spaces_and_comments(code, cur_char);
            
            return parse_fix_assign(code, cur_char, std::move(id_ast));
          }
          else {
            std::string errmsg = std::string("Unexpected `:`");
            throw StyioSyntaxError(errmsg);
          }
        };

        // You should NOT reach this line!
        break;

      case '<':
        {
          move_to_the_next_char(code, cur_char);

          if (check_and_drop_char(code, cur_char, '-'))
          {
            drop_spaces_and_comments(code, cur_char);
            
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
            return parse_iter(code, cur_char, std::move(id_ast));
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

    drop_spaces(code, cur_char);

    if (check_binop_token(code)) {
      return parse_binop_rhs(code, cur_char, std::move(numAST));
    } else {
      return numAST;
    }
  }
  // Print
  else if (check_symbol(code, ">_")) {
    return parse_print(code, cur_char);
  }
  // Comments
  else if (check_symbol(code, "//")) {
    pass_over_char(code, cur_char, '\n');
  }
  else if (check_symbol(code, "/*")) {
    pass_over_symbol(code, cur_char, "*/");
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

      if (find_and_drop_symbol(code, cur_char, "->")) {
        drop_spaces_and_comments(code, cur_char);

        return std::make_unique<ConnectAST>(
          std::move(resources), 
          parse_exec_block(code, cur_char));
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

      if (check_and_drop_char(code, cur_char, '.')) {
        return parse_loop(code, cur_char); }
      else {
        return parse_list_expr(code, cur_char); }
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
  char& cur_char
) 
{ 
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

std::unique_ptr<StyioAST> parse_case_block (
  struct StyioCodeContext* code, 
  char& cur_char
)
{
  return std::make_unique<NoneAST>();
}

std::unique_ptr<StyioAST> parse_exec_block (
  struct StyioCodeContext* code, 
  char& cur_char
) 
{
  std::vector<std::unique_ptr<StyioAST>> stmtBuffer;

  // eliminate { at the start
  move_to_the_next_char(code, cur_char);

  while (1)
  {
    drop_spaces(code, cur_char);
    
    if (check_char(cur_char, '}'))
    {
      // eliminate } at the end
      move_to_the_next_char(code, cur_char);

      break;
    }
    else
    {
      stmtBuffer.push_back(std::move(parse_stmt(code, cur_char)));
    };
  };

  if (stmtBuffer.size() == 0)
  {
    return std::make_unique<EmptyBlockAST>();
  }
  else
  {
    return std::make_unique<BlockAST>(std::move(stmtBuffer));
  };
}

void parse_program (std::string styio_code) 
{
  char cur_char = styio_code.at(0);

  struct StyioCodeContext styio_code_context = {
    styio_code,
    0
  };

  StyioCodeContext* ctx_ptr = &styio_code_context;

  while (true) 
  {
    std::unique_ptr<StyioAST> stmt = parse_stmt(ctx_ptr, cur_char);

    if ((stmt -> hint()) == StyioType::End) break;

    if ((stmt -> hint()) != StyioType::Comment) {
      std::cout << "\033[1;33m[>_<]\033[0m " << stmt -> toString(0, false) << std::endl;
    }
  };
}