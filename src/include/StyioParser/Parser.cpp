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
  =================
*/

void go_ahead
(
  struct StyioCodeContext* code,
  char& cur_char
)
{
  code -> cursor += 1;

  cur_char = code -> text.at(code -> cursor);
}

inline bool can_be_ignored(char token) {
  return isspace(token);
}

void move_until_token (
  struct StyioCodeContext* code,
  char& cur_char) {
  while (can_be_ignored(code -> text.at(code -> cursor)))
  {
    code -> cursor += 1;
    cur_char = code -> text.at(code -> cursor);
  }
}

bool check_this_char
(
  char& cur_char, 
  char value
)
{
  return cur_char == value;
}

void drop_all_spaces 
(
  struct StyioCodeContext* code,
  char& cur_char
) 
{
  while (isspace(cur_char)) {
    go_ahead(code, cur_char);
  };
}

void drop_white_spaces 
(
  struct StyioCodeContext* code,
  char& cur_char
) 
{
  while (check_this_char(cur_char, ' ')) {
    go_ahead(code, cur_char);
  };
}

void check_and_drop (
  struct StyioCodeContext* code,
  char& cur_char,
  char value,
  int mode = 0) {
  switch (mode)
  {
  case 1:
    drop_white_spaces(code, cur_char);

    break;

  case 2:
    drop_all_spaces(code, cur_char);

    break;
  
  default:
    break;
  }

  if (check_this_char(cur_char, value)) {
    go_ahead(code, cur_char);
  }
  else {
    std::string errmsg = std::string("Expecting .:| ") + char(value) + " |:. , but got .:| " + char(cur_char) + " |:.";
    throw StyioSyntaxError(errmsg);
  }
}

bool peak_next_token (
  struct StyioCodeContext* code,
  char value,
  int mode = 0) {
  int start_with = code -> cursor;
  int move_forward = 0;

  switch (mode)
  {
  case 1:
    while ((code -> text.at(start_with + move_forward)) == ' ') {
      move_forward += 1;
    }
    
    break;

  case 2:
    while (isspace((code -> text.at(start_with + move_forward)))) {
      move_forward += 1;
    }

    break;
  
  default:
    break;
  }

  return (code -> text.at(start_with + move_forward)) == value;
}

bool check_binop_token (
  struct StyioCodeContext* code) {
  int start_with = code -> cursor;
  int move_forward = 0;

  while (isspace((code -> text.at(start_with + move_forward)))) {
    move_forward += 1;
  }

  switch (code -> text.at(start_with + move_forward))
  {
  case '+':
    return true;

    // You should NOT reach this line!
    break;

  case '-':
    return true;

    // You should NOT reach this line!
    break;

  case '*':
    return true;

    // You should NOT reach this line!
    break;

  case '/':
    {
      char& the_next_char = code -> text.at(start_with + move_forward + 1);

      if (check_this_char(the_next_char, '*')) {
        return false;
      } else {
        return true;
      }
    }

    // You should NOT reach this line!
    break;

  case '%':
    return true;

    // You should NOT reach this line!
    break;
  
  default:
    break;
  };

  return false;
}

void move_until_binop (
  struct StyioCodeContext* code,
  char& cur_char) {
  while (true) {
    if (check_this_char(cur_char, '+') 
      || check_this_char(cur_char, '-') 
      || check_this_char(cur_char, '*')
      || check_this_char(cur_char, '/')
      || check_this_char(cur_char, '%')) {
      break;
    } 
    else {
      go_ahead(code, cur_char);
    }
  };
}

void drop_until (
  struct StyioCodeContext* code,
  char& cur_char,
  char value) {
  while (not check_this_char(cur_char, value)) {
    go_ahead(code, cur_char);
  };
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

std::unique_ptr<IdAST> parse_id 
(
  struct StyioCodeContext* code, 
  char& cur_char
)
{
  std::unique_ptr<IdAST> output;

  std::string idStr = "";

  // [a-zA-Z][a-zA-Z0-9_]*
  do {
    idStr += cur_char;
    go_ahead(code, cur_char);
  } while (
    isalnum((cur_char)) 
    || check_this_char(cur_char, '_')
  );

  output = std::make_unique<IdAST>(idStr);
  return output;
}

std::unique_ptr<IntAST> parse_int 
(
  struct StyioCodeContext* code, 
  char& cur_char
)
{
  std::unique_ptr<IntAST> output;

  std::string intStr = "";

  // push the current character into string
  intStr += cur_char;

  // progress to the next
  go_ahead(code, cur_char);

  // [0-9]*
  while (isdigit(cur_char))
  {
    intStr += cur_char;
    go_ahead(code, cur_char);
  };

  output = std::make_unique<IntAST>(intStr);
  return output;
}

std::unique_ptr<StyioAST> parse_int_or_float 
(
  struct StyioCodeContext* code, 
  char& cur_char
)
{
  std::unique_ptr<StyioAST> output;

  std::string numStr = "";
  numStr += cur_char;
  go_ahead(code, cur_char);

  // [0-9]*
  while (isdigit(cur_char))
  {
    numStr += cur_char;
    go_ahead(code, cur_char);
  };

  if (check_this_char(cur_char, '.')) 
  {
    go_ahead(code, cur_char);

    if (isdigit(cur_char))
    {
      numStr += '.';

      while (isdigit(cur_char))
      {
        numStr += cur_char;
        go_ahead(code, cur_char);
      };

      output = std::make_unique<FloatAST>(numStr);
    }
    else
    {
      output = std::make_unique<IntAST>(numStr);
    };
  } 
  else 
  {
    output = std::make_unique<IntAST>(numStr);
  }

  return output;
}

std::unique_ptr<StringAST> parse_string 
(
  struct StyioCodeContext* code, 
  char& cur_char
)
{
  std::unique_ptr<StringAST> output;

  // eliminate the first(start) double quote
  go_ahead(code, cur_char);

  std::string textStr = "";
  
  while (cur_char != '\"')
  {
    textStr += cur_char;
    go_ahead(code, cur_char);
  };

  // eliminate the second(end) double quote
  go_ahead(code, cur_char);

  output = std::make_unique<StringAST>(textStr);

  return output;
}

std::unique_ptr<StyioAST> parse_char_or_string 
(
  struct StyioCodeContext* code, 
  char& cur_char
)
{
  std::unique_ptr<StyioAST> output;

  // eliminate the first(start) single quote
  go_ahead(code, cur_char);

  std::string textStr = "";
  
  while (cur_char != '\'')
  {
    textStr += cur_char;
    go_ahead(code, cur_char);
  };

  // eliminate the second(end) single quote
  go_ahead(code, cur_char);

  if (textStr.length() == 1)
  {
    output = std::make_unique<CharAST>(textStr);
  }
  else
  {
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

  // eliminate @
  go_ahead(code, cur_char);

  if (check_this_char(cur_char, '(')) {
    // eliminate (
    go_ahead(code, cur_char);

    if (check_this_char(cur_char, '\"')) {
      std::unique_ptr<StringAST> path = parse_string(code, cur_char);

      if (check_this_char(cur_char, ')')) {
        // eliminate )
        go_ahead(code, cur_char);
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
    while (not check_this_char(cur_char, '\n'))
    {
      commentText += char(cur_char);

      go_ahead(code, cur_char);
    }

    return std::make_unique<CommentAST>(commentText);
  }
  else if (mode == 2) {
    while (true)
    {
      while (not check_this_char(cur_char, '*'))
      {
        commentText += char(cur_char);

        go_ahead(code, cur_char);
      }

      // eliminate the detected *
      go_ahead(code, cur_char);

      if (check_this_char(cur_char, '/')) {
        go_ahead(code, cur_char);

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
  char& cur_char
) 
{
  std::unique_ptr<FillingAST> output;

  std::vector<std::unique_ptr<StyioAST>> vars;

  if (check_this_char(cur_char, '(')) {
    go_ahead(code, cur_char);
  }
  else {
    std::string errmsg = std::string("(x, y, ...) // Expecting ( at the start, but got .:| ") + char(cur_char) + " |:.";
    throw StyioSyntaxError(errmsg);
  };

  vars.push_back(std::move(parse_id(code, cur_char)));

  move_until_token(code, cur_char);

  while (check_this_char(cur_char, ','))
  {
    go_ahead(code, cur_char);

    move_until_token(code, cur_char);

    if (check_this_char(cur_char, ')'))
    {
      break;
    };

    vars.push_back(std::move(parse_id(code, cur_char)));
  }

  if (check_this_char(cur_char, ')'))
  {
    go_ahead(code, cur_char);
  };

  output = std::make_unique<FillingAST>(std::move(vars));

  return output;
}

std::unique_ptr<ResourceAST> parse_resources (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<ResourceAST> output;

  std::vector<std::unique_ptr<StyioAST>> resources;

  // eliminate @ at the start
  go_ahead(code, cur_char);

  if (check_this_char(cur_char, '(')) 
  {
    go_ahead(code, cur_char);

    move_until_token(code, cur_char);

    if (isalpha(cur_char) || check_this_char(cur_char, '_')) {
      // "@" "(" |--
      std::unique_ptr<IdAST> tmp_res_name = parse_id(code, cur_char);

      move_until_token(code, cur_char);

      if (check_this_char(cur_char, '<')) {
        go_ahead(code, cur_char);

        check_and_drop(code, cur_char, '-', 0);
      };

      move_until_token(code, cur_char);

      std::unique_ptr<StyioAST> tmp_expr = std::make_unique<FinalBindAST>(
        std::move(tmp_res_name), 
        parse_value(code, cur_char));

      resources.push_back(std::move(tmp_expr));
    };

    move_until_token(code, cur_char);

    // "@" "(" [<ID> |--
    while (check_this_char(cur_char, ','))
    {
      go_ahead(code, cur_char);

      move_until_token(code, cur_char);

      if (isalpha(cur_char) || check_this_char(cur_char, '_')) {
        
        std::unique_ptr<IdAST> tmp_res_name = parse_id(code, cur_char);

        move_until_token(code, cur_char);

        if (check_this_char(cur_char, '<')) {
          go_ahead(code, cur_char);

          check_and_drop(code, cur_char, '-', 0);
        };

        move_until_token(code, cur_char);

        std::unique_ptr<StyioAST> tmp_value = parse_value(code, cur_char);

        resources.push_back(std::move(
          std::make_unique<FinalBindAST>(
            std::move(tmp_res_name), 
            std::move(tmp_value))));
      };
    };
    
    if (check_this_char(cur_char, ')')) 
    {
      go_ahead(code, cur_char);
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

  move_until_token(code, cur_char);

  output = parse_value(code, cur_char);
  
  move_until_token(code, cur_char);

  switch (cur_char)
  {
  case '=':
    {
      go_ahead(code, cur_char);

      if (check_this_char(cur_char, '='))
      {
        go_ahead(code, cur_char);

        /*
          Equal
            expr == expr
        */

        // drop all spaces after ==
        drop_all_spaces(code, cur_char);
        
        output = std::make_unique<BinCompAST>(
          CompType::EQ,
          std::move(output),
          parse_value(code, cur_char));
      };
    }

    break;

  case '!':
    {
      go_ahead(code, cur_char);

      if (check_this_char(cur_char, '='))
      {
        go_ahead(code, cur_char);

        /*
          Not Equal
            expr != expr
        */

        // drop all spaces after !=
        drop_all_spaces(code, cur_char);

        output = std::make_unique<BinCompAST>(
          CompType::NE,
          std::move(output),
          parse_value(code, cur_char));
      };
    }

    break;

  case '>':
    {
      go_ahead(code, cur_char);

      if (check_this_char(cur_char, '='))
      {
        go_ahead(code, cur_char);

        /*
          Greater Than and Equal
            expr >= expr
        */

        // drop all spaces after >=
        drop_all_spaces(code, cur_char);

        output = std::make_unique<BinCompAST>(
          CompType::GE,
          std::move(output),
          parse_value(code, cur_char));
      }
      else
      {
        /*
          Greater Than
            expr > expr
        */

        // drop all spaces after >
        drop_all_spaces(code, cur_char);

        output = std::make_unique<BinCompAST>(
          CompType::GT,
          std::move(output),
          parse_value(code, cur_char));
      };
    }

    break;

  case '<':
    {
      go_ahead(code, cur_char);

      if (check_this_char(cur_char, '='))
      {
        go_ahead(code, cur_char);

        /*
          Less Than and Equal
            expr <= expr
        */

        // drop all spaces after <=
        drop_all_spaces(code, cur_char);

        output = std::make_unique<BinCompAST>(
          CompType::LE,
          std::move(output),
          parse_value(code, cur_char));
      }
      else
      {
        /*
          Less Than
            expr < expr
        */

        // drop all spaces after <
        drop_all_spaces(code, cur_char);

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

  if (isalpha(cur_char) || check_this_char(cur_char, '_')) 
  {
    output = parse_id(code, cur_char);
  }

  if (check_this_char(cur_char, '[')) {
    output = parse_list_op(code, cur_char, std::move(output));
  } 
  else if (check_this_char(cur_char, '(')) {
    output = parse_call(code, cur_char);
  }

  move_until_token(code, cur_char);

  if (is_binary_token(cur_char)) {
    output = parse_binop_rhs(code, cur_char, std::move(output));
  };

  return output;
}

std::unique_ptr<StyioAST> parse_value (
  struct StyioCodeContext* code, 
  char& cur_char) {
  std::unique_ptr<StyioAST> output;

  if (isalpha(cur_char) || check_this_char(cur_char, '_')) 
  {
    output = parse_id_or_value(code, cur_char);

    return output;
  }
  else if (isdigit(cur_char)) 
  {
    return parse_int_or_float(code, cur_char);
  }
  else if (check_this_char(cur_char, '|'))
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
  if (isalpha(cur_char) || check_this_char(cur_char, '_')) 
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
      go_ahead(code, cur_char);

      drop_white_spaces(code, cur_char);

      if (check_this_char(cur_char, ']')) {
        go_ahead(code, cur_char);

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
  if (isalpha(cur_char) || check_this_char(cur_char, '_')) 
  {
    // parse id
    output = parse_id(code, cur_char);
    
    // ignore white spaces after id
    drop_white_spaces(code, cur_char);

    if (is_binary_token(cur_char))
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

    if (is_binary_token(cur_char))
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
      go_ahead(code, cur_char);

      drop_white_spaces(code, cur_char);

      if (check_this_char(cur_char, ']')) {
        go_ahead(code, cur_char);

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

      if (is_binary_token(cur_char))
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
  go_ahead(code, cur_char);
       
  if (isalpha(cur_char) || check_this_char(cur_char, '_'))
  {
    std::unique_ptr<StyioAST> var = parse_id_or_value(code, cur_char);

    // eliminate | at the end
    if (check_this_char(cur_char, '|')) {
      go_ahead(code, cur_char);

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
  go_ahead(code, cur_char);

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

        go_ahead(code, cur_char);

        output = std::make_unique<ListOpAST>(
          ListOpType::Get_Reversed,
          std::move(theList));
      }

      // You should NOT reach this line!
      break;

    // list[?= item]
    case '?':
      {
        go_ahead(code, cur_char);

        if (check_this_char(cur_char, '='))
        {
          go_ahead(code, cur_char);

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
        go_ahead(code, cur_char);

        if (check_this_char(cur_char, ':'))
        {
          go_ahead(code, cur_char);

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

            if (check_this_char(cur_char, '<'))
            {
              go_ahead(code, cur_char);

              if (check_this_char(cur_char, '-'))
              {
                go_ahead(code, cur_char);

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
        go_ahead(code, cur_char);

        if (check_this_char(cur_char, ':'))
        {
          go_ahead(code, cur_char);

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
              go_ahead(code, cur_char);
              
              // drop white spaces between '(' and the first index
              drop_white_spaces(code, cur_char);

              std::vector<std::unique_ptr<IntAST>> indices;

              indices.push_back(std::move(parse_int(code, cur_char)));

              // drop white spaces between first index and ,
              drop_white_spaces(code, cur_char);

              while (check_this_char(cur_char, ','))
              {
                // remove ,
                go_ahead(code, cur_char);

                // drop white spaces between , and next index
                drop_white_spaces(code, cur_char);

                if (check_this_char(cur_char, ')'))
                {
                  break;
                }

                indices.push_back(std::move(parse_int(code, cur_char)));
              }

              // drop white spaces between , and )
              drop_white_spaces(code, cur_char);
              
              if (check_this_char(cur_char, ')'))
              {
                go_ahead(code, cur_char);

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
              go_ahead(code, cur_char);

              switch (cur_char)
              {
              case '=':
              {
                /*
                  list[-: ?= value]
                */

                go_ahead(code, cur_char);

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

                go_ahead(code, cur_char);

                // drop white spaces after ?^
                drop_white_spaces(code, cur_char);

                if (check_this_char(cur_char, '(') 
                  || check_this_char(cur_char, '[')
                  || check_this_char(cur_char, '{'))
                {
                  go_ahead(code, cur_char);
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
  if (check_this_char(cur_char, ']'))
  {
    // eliminate ] at the end
    go_ahead(code, cur_char);
    
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

  drop_all_spaces(code, cur_char);

  // eliminate the start
  if (check_this_char(cur_char, '('))
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

  drop_all_spaces(code, cur_char);

  if (check_this_char(cur_char, '?'))
  {
    go_ahead(code, cur_char);

    switch (cur_char)
    {
    /*
      ?= Value
    */
    case '=':
      {
        go_ahead(code, cur_char);

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

  if (check_this_char(cur_char, '='))
  {
    go_ahead(code, cur_char);

    if (check_this_char(cur_char, '>'))
    {
      go_ahead(code, cur_char);
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
  drop_all_spaces(code, cur_char);

  if (check_this_char(cur_char, '{'))
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

  move_until_token(code, cur_char);

  switch (cur_char)
  {
  case '.':
    {
      go_ahead(code, cur_char);

      while (check_this_char(cur_char, '.'))
      {
        go_ahead(code, cur_char);
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

      if (check_this_char(cur_char, ']')) 
      {
        go_ahead(code, cur_char);
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
          go_ahead(code, cur_char);

          if (check_this_char(cur_char, '>'))
          {
            // If: >>, Then: Iteration
            go_ahead(code, cur_char);
            
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

      while (check_this_char(cur_char, ','))
      {
        // eliminate ,
        go_ahead(code, cur_char);

        drop_white_spaces(code, cur_char);

        if (check_this_char(cur_char, ']')) 
        {
          go_ahead(code, cur_char);

          theList = std::make_unique<ListAST>(std::move(elements));
        };

        elements.push_back(std::move(parse_value(code, cur_char)));
      };

      drop_white_spaces(code, cur_char);

      if (check_this_char(cur_char, ']')) 
      {
        go_ahead(code, cur_char);

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
          go_ahead(code, cur_char);

          if (check_this_char(cur_char, '>'))
          {
            // If: >>, Then: Iteration
            go_ahead(code, cur_char);

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

  while (check_this_char(cur_char, '.')) 
  { 
    // eliminate all .
    go_ahead(code, cur_char);

    if (check_this_char(cur_char, ']')) 
    {
      go_ahead(code, cur_char);

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
      go_ahead(code, cur_char);

      if (check_this_char(cur_char, '>'))
      {
        go_ahead(code, cur_char);

        // eliminate white spaces after >>
        drop_white_spaces(code, cur_char);

        if (isalpha(cur_char) 
            || check_this_char(cur_char, '_')
            || check_this_char(cur_char, '(')) 
        {
          output = parse_iter(code, cur_char, std::unique_ptr<StyioAST>(new InfiniteAST()));
        }
        else
        if (check_this_char(cur_char, '{'))
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
        go_ahead(code, cur_char);

        move_until_token(code, cur_char);

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
        go_ahead(code, cur_char);

        move_until_token(code, cur_char);

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
        go_ahead(code, cur_char);
        // BIN_POW := <ID> "**" <EXPR>
        if (check_this_char(cur_char, '*'))
        {
          go_ahead(code, cur_char);

          move_until_token(code, cur_char);

          // <ID> "**" |--
          output = std::make_unique<BinOpAST>(
            BinOpType::BIN_POW, 
            std::move(lhs_ast), 
            std::move(parse_item_for_binop(code, cur_char)));
        } 
        // BIN_MUL := <ID> "*" <EXPR>
        else 
        {
          move_until_token(code, cur_char);

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
        go_ahead(code, cur_char);

        move_until_token(code, cur_char);

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
        go_ahead(code, cur_char);

        move_until_token(code, cur_char);

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

  move_until_token(code, cur_char);

  while (check_binop_token(code)) 
  {
    move_until_binop(code, cur_char);

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

  drop_all_spaces(code, cur_char);

  switch (cur_char)
  {
  case '&':
    {
      go_ahead(code, cur_char);

      if (check_this_char(cur_char, '&'))
      {
        go_ahead(code, cur_char);
      };

      /*
        support:
          expr && \n
          expression
      */

      drop_all_spaces(code, cur_char);

      condExpr = std::make_unique<CondAST>(
        LogicType::AND,
        std::move(lhsExpr),
        parse_cond(code, cur_char)
      );
    }

    break;

  case '|':
    {
      go_ahead(code, cur_char);

      if (check_this_char(cur_char, '|'))
      {
        go_ahead(code, cur_char);
      };

      /*
        support:
          expr || \n
          expression
      */

      drop_all_spaces(code, cur_char);

      condExpr = std::make_unique<CondAST>(
        LogicType::OR,
        std::move(lhsExpr),
        parse_cond(code, cur_char)
      );
    }

    break;

  case '^':
    {
      go_ahead(code, cur_char);

      /*
        support:
          expr ^ \n
          expression
      */

      drop_all_spaces(code, cur_char);

      condExpr = std::make_unique<CondAST>(
        LogicType::OR,
        std::move(lhsExpr),
        parse_cond(code, cur_char)
      );
    }

    break;

  case '!':
    {
      go_ahead(code, cur_char);

      if (check_this_char(cur_char, '('))
      {
        go_ahead(code, cur_char);

        /*
          support:
            !( \n
              expr
            )
        */
        drop_all_spaces(code, cur_char);

        condExpr = std::make_unique<CondAST>(
          LogicType::NOT,
          parse_cond(code, cur_char)
        );

        check_and_drop(code, cur_char, ')', 2);
      }
    }

    break;

  default:
    break;
  }

  drop_all_spaces(code, cur_char);

  while (!(check_this_char(cur_char, ')')))
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

  if (check_this_char(cur_char, '('))
  {
    go_ahead(code, cur_char);

    lhsExpr = std::move(parse_cond(code, cur_char));

    check_and_drop(code, cur_char, ')', 2);
  }
  else
  if (check_this_char(cur_char, '!'))
  {
    go_ahead(code, cur_char);

    if (check_this_char(cur_char, '('))
    {
      go_ahead(code, cur_char);

      /*
        support:
          !( \n
            expr
          )
      */
      drop_all_spaces(code, cur_char);

      lhsExpr = std::move(parse_cond(code, cur_char));

      drop_all_spaces(code, cur_char);

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
  drop_all_spaces(code, cur_char);

  if (check_this_char(cur_char, '&')
    || check_this_char(cur_char, '|'))
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
  go_ahead(code, cur_char);

  std::unique_ptr<CondAST> condition;
  
  if (check_this_char(cur_char, '(')) {
    go_ahead(code, cur_char);

    condition = parse_cond(code, cur_char);

    check_and_drop(code, cur_char, ')', 2);

    /*
      support:
        ?() \n
        \t\

        ?() \n
        \f\
    */
    drop_all_spaces(code, cur_char);

    if (check_this_char(cur_char, '\\'))
    {
      go_ahead(code, cur_char);

      std::unique_ptr<StyioAST> block;

      if (check_this_char(cur_char, 't'))
      {
        go_ahead(code, cur_char);

        check_and_drop(code, cur_char, '\\', 0);

        /*
          support:
            \t\ \n
            {}
        */

        drop_all_spaces(code, cur_char);

        block = parse_exec_block(code, cur_char);

        /*
          support:
            \t\ {} \n
            \f\
        */
        drop_all_spaces(code, cur_char);

        if (check_this_char(cur_char, '\\'))
        {
          go_ahead(code, cur_char);

          check_and_drop(code, cur_char, 'f', 0);

          if (check_this_char(cur_char, '\\'))
          {
            go_ahead(code, cur_char);

            /*
              support:
                \f\ \n
                {}
            */
            drop_all_spaces(code, cur_char);

            std::unique_ptr<StyioAST> blockElse = parse_exec_block(code, cur_char);

            return std::make_unique<CondFlowAST>(
              FlowType::Both,
              std::move(condition),
              std::move(block),
              std::move(blockElse)
            );
          };
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
      else if (check_this_char(cur_char, 'f'))
      {
        go_ahead(code, cur_char);

        check_and_drop(code, cur_char, '\\', 0);

        /*
          support:
            \f\ \n
            {}
        */
        drop_all_spaces(code, cur_char);

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
  
  if (check_this_char(cur_char, '\n')) 
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
  
  if (check_this_char(cur_char, '\n')) 
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
  go_ahead(code, cur_char);

  // after #
  drop_white_spaces(code, cur_char);

  if (isalpha(cur_char) 
    || check_this_char(cur_char, '_'))
  {
    pipeName = parse_id(code, cur_char);

    pwithName = true;
  };
  
  // after function name
  drop_all_spaces(code, cur_char);

  if (check_this_char(cur_char, ':'))
  {
    go_ahead(code, cur_char);

    pisFinal = true;
  };

  if (check_this_char(cur_char, '='))
  {
    go_ahead(code, cur_char);
  };

  move_until_token(code, cur_char);

  if (check_this_char(cur_char, '('))
  {
    pipeVars = parse_filling(code, cur_char);
  }
  else
  {
    std::string errmsg = std::string("Expecting ( after function name, but got ") + char(cur_char);
    throw StyioSyntaxError(errmsg);
  };

  move_until_token(code, cur_char);
  
  if (check_this_char(cur_char, '='))
  {
    go_ahead(code, cur_char);

    if (check_this_char(cur_char, '>'))
    {
      go_ahead(code, cur_char);
    };
  };

  drop_all_spaces(code, cur_char);

  if (check_this_char(cur_char, '{'))
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
  if (check_this_char(cur_char, '@'))
  {
    return std::make_unique<ReadFileAST>(std::move(id_ast), parse_path_or_link(code, cur_char));
  }
  else
  {
    std::string errmsg = std::string("Unexpected Read.Path, starts with character `") + char(cur_char) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

std::unique_ptr<StyioAST> parse_write_stdout (
  struct StyioCodeContext* code, 
  char& cur_char
)
{
  std::unique_ptr<StyioAST> output;

  // eliminate >
  go_ahead(code, cur_char);
  
  if (check_this_char(cur_char, '_')) {
    // eliminate _
    go_ahead(code, cur_char);

    drop_white_spaces(code, cur_char);

    if (check_this_char(cur_char, '('))
    {
      // eliminate (
      go_ahead(code, cur_char);

      drop_all_spaces(code, cur_char);

      output = std::make_unique<WriteStdOutAST>(parse_expr(code, cur_char));

      drop_all_spaces(code, cur_char);

      if (check_this_char(cur_char, ')'))
      {
        go_ahead(code, cur_char);
      }
      else
      {
        std::string errmsg = std::string("Missing `)` for >_(), got `") + char(cur_char) + "`.";
        throw StyioSyntaxError(errmsg);
      };
    }
    else
    {
      std::string errmsg = std::string("Unrecognized `expr` for >_(`expr`), got `") + char(cur_char) + "`";
      throw StyioSyntaxError(errmsg);
    };
  };

  return output;
}

std::unique_ptr<StyioAST> parse_stmt (
  struct StyioCodeContext* code, 
  char& cur_char) {
  
  move_until_token(code, cur_char);

  // <ID>
  if (isalpha(cur_char) 
    || check_this_char(cur_char, '_')) 
  {
    // parse id
    std::unique_ptr<IdAST> id_ast = parse_id(code, cur_char);

    if (check_this_char(cur_char, '['))
    {
      return parse_list_op(code, cur_char, std::move(id_ast));
    }
    
    // ignore white spaces after id
    drop_white_spaces(code, cur_char);

    // check next character
    switch (cur_char)
    {
      // <LF>
      case '\n':
        {
          go_ahead(code, cur_char);

          return id_ast;
        };

        // You should NOT reach this line!
        break;

      // <ID> = <EXPR>
      case '=':
        {
          go_ahead(code, cur_char);

          drop_white_spaces(code, cur_char);

          // <ID> = |--
          return parse_mut_assign(code, cur_char, std::move(id_ast));
        };

        // You should NOT reach this line!
        break;
      
      // <ID> := <EXPR>
      case ':':
        {
          go_ahead(code, cur_char);
          if (check_this_char(cur_char, '='))
          {
            go_ahead(code, cur_char);

            drop_white_spaces(code, cur_char);
            
            // <ID> := |--
            return parse_fix_assign(code, cur_char, std::move(id_ast));
          }
          else
          {
            std::string errmsg = std::string("Unexpected `:`");
            throw StyioSyntaxError(errmsg);
          }
        };

        // You should NOT reach this line!
        break;

      // <ID> <- <EXPR>
      case '<':
        {
          // eliminate <
          go_ahead(code, cur_char);

          if (check_this_char(cur_char, '-'))
          {
            // eliminate -
            go_ahead(code, cur_char);

            drop_white_spaces(code, cur_char);
            
            // <ID> <- |--
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

      // ID >> Layer
      case '>':
        {
          go_ahead(code, cur_char);

          if (check_this_char(cur_char, '>'))
          {
            // If: >>, Then: Iteration
            go_ahead(code, cur_char);
            
            return parse_iter(code, cur_char, std::move(id_ast));
          }
        }
        
        // You should NOT reach this line!
        break;

      // BIN_ADD := <ID> "+" <EXPR>
      case '+':
        {
          // <ID> |-- 
          return parse_binop_rhs(code, cur_char, std::move(id_ast));
        };

        // You should NOT reach this line!
        break;

      // BIN_SUB := <ID> "-" <EXPR>
      case '-':
        {
          // <ID> |--
          return parse_binop_rhs(code, cur_char, std::move(id_ast));
        };

        // You should NOT reach this line!
        break;

      // BIN_MUL | BIN_POW
      case '*':
        {
          // <ID> |--
          return parse_binop_rhs(code, cur_char, std::move(id_ast));
        };
        // You should NOT reach this line!
        break;
        
      // BIN_DIV := <ID> "/" <EXPR>
      case '/':
        {
          // <ID> |-- 
          return parse_binop_rhs(code, cur_char, std::move(id_ast));
        };

        // You should NOT reach this line!
        break;

      // BIN_MOD := <ID> "%" <EXPR> 
      case '%':
        {
          // <ID> |-- 
          return parse_binop_rhs(code, cur_char, std::move(id_ast));
        };

        // You should NOT reach this line!
        break;

      // LIST_OP |--
      case '[':
        {
          // <ID> |-- 

          return parse_list_op(code, cur_char, std::move(id_ast));
        };

        // You should NOT reach this line!
        break;
      
      default:
        break;
    }
  }

  if (isdigit(cur_char)) {
    std::unique_ptr<StyioAST> numAST = parse_int_or_float(code, cur_char);

    move_until_token(code, cur_char);

    if (is_binary_token(cur_char)) {
      return parse_binop_rhs(code, cur_char, std::move(numAST));
    } else {
      return numAST;
    }
  }

  switch (cur_char)
  {
  case EOF:
    return std::make_unique<EndAST>();

    // You should NOT reach this line!
    break;

  case '.':
    {
      while (check_this_char(cur_char, '.'))
      {
        go_ahead(code, cur_char);
      }
      
      return std::make_unique<PassAST>();
    }

    // You should NOT reach this line!
    break;

  /*
    Resources

    @(expr <- expr)
  */
  case '@':
    {
      std::unique_ptr<ResourceAST> resources = parse_resources(code, cur_char);

      if (peak_next_token(code, '-', 2))
      {
        check_and_drop(code, cur_char, '-', 2);

        check_and_drop(code, cur_char, '>', 0);

        drop_all_spaces(code, cur_char);

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
  
  /*
    "String"
  */
  case '\"':
    {
      return parse_string(code, cur_char);
    }

    break;

  /*
    Conditionals

    ?(expr) 
    :) {} 
    :( {}

    ?(expr) :( {}
  */
  case '?':
    {
      return parse_cond_flow(code, cur_char);
    }
    
    // You should NOT reach this line!
    break;

  /*
    List:
      [v0, v1, v2, ..., vn]
    
    Iterator:
      [...] >> {}

      List >> {}
  */
  case '[':
    {
      go_ahead(code, cur_char);

      if (check_this_char(cur_char, '.')) 
      {
        // eliminate the first dot .
        go_ahead(code, cur_char);

        if (check_this_char(cur_char, ']')) 
        {
          std::string errmsg = std::string("[.] is not infinite, please use [..] or [...] instead.");
          throw StyioSyntaxError(errmsg);
        };

        return parse_loop(code, cur_char);
      }
      else
      {
        return parse_list_expr(code, cur_char);
      }
    }
    
    // You should NOT reach this line!
    break;

  /*
    >_(expr)
  */
  case '>':
    {
      return parse_write_stdout(code, cur_char);
    }

    // You should NOT reach this line!
    break;

  /*
    # function 
      <: {
        interfaces
      } 
      := (items) => {
        statements
      }
  */
  case '#':
    {
      return parse_pipeline(code, cur_char);
    }

    // You should NOT reach this line!
    break;

  case '=':
    {
      go_ahead(code, cur_char);

      if (check_this_char(cur_char, '>'))
      {
        go_ahead(code, cur_char);
      };

      drop_white_spaces(code, cur_char);
      
      return std::make_unique<ReturnAST>(parse_expr(code, cur_char));
    }

    // You should NOT reach this line!
    break;

  case '/':
    {
      go_ahead(code, cur_char);

      if (check_this_char(cur_char, '/')) {
        go_ahead(code, cur_char);

        return parse_comment(code, cur_char, 1);
      }
      else if (check_this_char(cur_char, '*')) {
        go_ahead(code, cur_char);

        return parse_comment(code, cur_char, 2);
      }
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

  if (check_this_char(cur_char, '\"'))
  {
    // eliminate double quote symbol " at the start of dependency item
    go_ahead(code, cur_char);

    while (cur_char != '\"') 
    {
      if (check_this_char(cur_char, ',')) 
      {
        std::string errmsg = std::string("An \" was expected after") + itemStr + "however, a delimeter `,` was detected. ";
        throw StyioSyntaxError(errmsg);
      }

      itemStr += cur_char;

      go_ahead(code, cur_char);
    };

    // eliminate double quote symbol " at the end of dependency item
    go_ahead(code, cur_char);

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
  go_ahead(code, cur_char);

  std::vector<std::string> dependencies;

  drop_all_spaces(code, cur_char);

  // add the first dependency path to the list
  dependencies.push_back(std::move(parse_ext_elem(code, cur_char)));

  std::string pathStr = "";
  
  while (check_this_char(cur_char, ',')) {
    // eliminate comma ","
    go_ahead(code, cur_char);

    // reset pathStr to empty ""
    pathStr = ""; 

    drop_all_spaces(code, cur_char);
    
    // add the next dependency path to the list
    dependencies.push_back(std::move(parse_ext_elem(code, cur_char)));
  };

  if (check_this_char(cur_char, ']')) {
    // eliminate right square bracket `]` after dependency list
    go_ahead(code, cur_char);
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
  go_ahead(code, cur_char);

  while (1)
  {
    drop_all_spaces(code, cur_char);
    
    if (check_this_char(cur_char, '}'))
    {
      // eliminate } at the end
      go_ahead(code, cur_char);

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

    // fprintf(stderr, "[>_<] HERE!\n");

    if ((stmt -> hint()) != StyioType::Comment) {
      std::cout << "\033[1;33m[>_<]\033[0m " << stmt -> toString(0, false) << std::endl;
    }
  };
}