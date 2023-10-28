// [C++ STL]
#include <tuple>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <optional>


// [Styio]
#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "../StyioAST/AST.hpp"
#include "../StyioUtil/Util.hpp"
#include "Parser.hpp"


/*
  Basic Utilities
*/

inline bool check_binop_token (
  std::shared_ptr<StyioContext> context) {
  int pos = context -> pos;
  while (isspace((context -> code.at(pos)))) {
    pos += 1; }

  char the_char = context -> code.at(pos);
  if (the_char == '+' || the_char == '-' || the_char == '*' || the_char == '%') { 
    return true; }
  else if (the_char == '/') {
    if ((context -> code.at(pos + 1)) == '*') { 
      return false; } 
    else { 
      return true; } }

  return false;
}

/*
  Advanced Utilities
    check_over_next_char()

    drop_white_spaces()
    drop_spaces()
    
    move_until_char()
    
    pass_over_char()
    pass_over_symbol()
*/

// bool check_over_next_char (
//   std::shared_ptr<StyioContext> context,
//   char value) {
//   return check_char(context -> code.at(context -> pos + 1), value);
// }

/*
  Complex Utilities:
    find_and_drop_char
    find_and_drop_char_panic
*/

// bool find_and_drop_char_panic (
//   std::shared_ptr<StyioContext> context,
//   char value) {
//   /* ! No Boundary Check ! */
//   while (true) {
//     if (isspace(cur_char)) {
//       context -> move(1); }
//     else if (check_symbol(context, "//")) {
//       pass_over_char(context, cur_char, '\n'); }
//     else if (check_symbol(context, "/*")) {
//       pass_over_symbol(context, cur_char, "*/"); }
//     else {
//       if (check_char(cur_char, value)) {
//         context -> move(1);
//         return true; }
//       else {
//         std::string errmsg = std::string("Expecting: ") + char(value) + "\n" 
//           + "But Got: " + char(cur_char);
//         throw StyioSyntaxError(errmsg); } } }
// }

// bool find_and_drop_symbol_panic (
//   std::shared_ptr<StyioContext> context,
//   std::string value) {
//   /* ! No Boundary Check ! */
//   while (true) {
//     if (isspace(cur_char)) {
//       context -> move(1); }
//     else if (check_symbol(context, "//")) {
//       pass_over_char(context, cur_char, '\n'); }
//     else if (check_symbol(context, "/*")) {
//       pass_over_symbol(context, cur_char, "*/"); }
//     else {
//       if ((context -> code.substr(context -> pos, value.length())) == value) {
//         context -> move(value.size());
//         return true; }
//       else {
//         std::string errmsg = std::string("Expecting: ") + value + "\n" 
//           + "But Got: " + context -> code.substr(context -> pos, value.length());
//         throw StyioSyntaxError(errmsg); } } }
// }

inline bool match_next_char_panic (
  std::shared_ptr<StyioContext> context,
  char value) {
  if (check_char(cur_char, value)) {
    context -> move(1);
    return true; }

  std::string errmsg = std::string("Expecting: ") + char(value) + "\n" 
    + "But Got " + char(cur_char) + "\n";
  throw StyioSyntaxError(errmsg);
}

inline void move_next_and_ignore (
  std::shared_ptr<StyioContext> context) {
  context -> move(1);
  context -> drop_spaces();
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
  std::shared_ptr<StyioContext> context) {
  std::string idStr = "";
  /* it includes cur_char in the idStr without checking */
  do {
    idStr += cur_char;
    context -> move(1);
  } while (isalnum((cur_char)) || check_char(cur_char, '_'));

  return std::make_unique<IdAST>(idStr);
}

std::unique_ptr<IntAST> parse_int (
  std::shared_ptr<StyioContext> context) {
  std::string intStr = "";

  /* it includes cur_char in the idStr without checking */
  intStr += cur_char;
  context -> move(1);

  while (isdigit(context -> get_cur_char())) {
    intStr += cur_char;
    context -> move(1); };

  return std::make_unique<IntAST>(intStr);
}

std::unique_ptr<StyioAST> parse_int_or_float (
  std::shared_ptr<StyioContext> context) {
  std::unique_ptr<StyioAST> output;

  std::string numStr = "";

  /*
    Danger!
    when entering parse_int_or_float(), 
    the cur_char must be a digit
    this line will drop the next 1 character anyway!
  */
  numStr += cur_char;
  context -> move(1);

  while (isdigit(context -> get_cur_char())) {
    numStr += cur_char;
    context -> move(1); };

  if (context -> check_and_drop('.')) {
    if (isdigit(context -> get_cur_char())) {
      numStr += '.';

      while (isdigit(context -> get_cur_char())) {
        numStr += cur_char;
        context -> move(1); }

      return std::make_unique<FloatAST>(numStr); }
    else {
      return std::make_unique<IntAST>(numStr); }
  } 

  return std::make_unique<IntAST>(numStr); 
}

std::unique_ptr<StringAST> parse_str (
  std::shared_ptr<StyioContext> context) {
  /*
    Danger!
    when entering parse_str(), 
    the cur_char must be "
    this line will drop the next 1 character anyway!
  */
  context -> move(1);

  std::string textStr = "";
  
  while (not check_char(cur_char, '\"')) {
    textStr += cur_char;
    context -> move(1); }

  // eliminate " at the end
  context -> move(1);

  return std::make_unique<StringAST>(textStr);
}

std::unique_ptr<StyioAST> parse_char_or_string (
  std::shared_ptr<StyioContext> context) {
  /*
    Danger!
    when entering parse_char_or_string(), 
    the cur_char must be '
    this line will drop the next 1 character anyway!
  */
  context -> move(1);

  std::string textStr = "";
  
  while (not check_char(cur_char, '\'')) {
    textStr += cur_char;
    context -> move(1); };

  // eliminate ' at the end
  context -> move(1);

  if (textStr.size() == 1) {
    return std::make_unique<CharAST>(textStr); }
  else {
    return std::make_unique<StringAST>(textStr); }
}

std::unique_ptr<FmtStrAST> parse_fmt_str (
  std::shared_ptr<StyioContext> context) {
  /*
    Danger!
    when entering parse_fmt_str(), 
    the cur_char must be "
    this line will drop the next 1 character anyway!
  */
  context -> move(1);

  std::vector<std::string> fragments;
  std::vector<std::unique_ptr<StyioAST>> exprs;
  std::string textStr = "";

  while (not check_char(cur_char, '\"')) {
    if (check_char(cur_char, '{')) {
      
      if (check_over_next_char(context, '{')) {
        textStr += cur_char;
        move_forward(context, cur_char, 2); }
      else {
        context -> move(1);

        exprs.push_back(parse_expr(context));
        
        find_and_drop_char_panic(context, cur_char, '}');

        fragments.push_back(textStr);
        textStr.clear(); }
    }
    else if (check_char(cur_char, '}')) {
      if (check_over_next_char(context, '}')) {
        textStr += cur_char;
        move_forward(context, cur_char, 2); }
      else {
        std::string errmsg = std::string("Expecting: ") + "}" + "\n" 
          + "But Got: " + context -> code.at(context -> pos);
        throw StyioSyntaxError(errmsg);
      }
    }
    else {
      textStr += cur_char;
      context -> move(1); }
  }
  // this line drops " at the end anyway!
  context -> move(1);

  fragments.push_back(textStr);

  return std::make_unique<FmtStrAST>(
    std::move(fragments), 
    std::move(exprs));
}

std::unique_ptr<StyioAST> parse_path_or_link (
  std::shared_ptr<StyioContext> context) {
  std::unique_ptr<StyioAST> output;

  /*
    Danger!
    when entering parse_path_or_link(), 
    the cur_char must be @
    this line will drop the next 1 character anyway!
  */
  context -> move(1);

  if (context -> check_and_drop('(')) {
    if (check_char(cur_char, '\"')) {
      std::unique_ptr<StringAST> path = parse_str(context);

      find_and_drop_char_panic(context, cur_char, ')');

      return std::make_unique<ExtPathAST>(std::move(path)); }
    else {
      std::string errmsg = std::string("@(___) // Resource: Unexpected resource, starts with .:| ") + char(cur_char) + " |:.";
      throw StyioSyntaxError(errmsg); } }
  else {
    std::string errmsg = std::string("@(___) // Resource: Expecting ( at the start, but got .:| ") + char(cur_char) + " |:.";
    throw StyioSyntaxError(errmsg); }

  return output;
}

std::shared_ptr<DTypeAST> parse_dtype (
  std::shared_ptr<StyioContext> context) {
  std::string text = "";

  if (isalpha((cur_char)) || check_char(cur_char, '_')) {
    text += cur_char;
    context -> move(1); }

  while (isalnum((cur_char)) || check_char(cur_char, '_')) {
    text += cur_char;
    context -> move(1); }

  return std::make_shared<DTypeAST>(text);
}

/*
  Basic Collection
  - typed_var
  - Fill (Variable Tuple)
  - Resources
*/

std::unique_ptr<FillArgAST> parse_fill_arg (
  std::shared_ptr<StyioContext> context) {
  std::string name = "";
  /* it includes cur_char in the idStr without checking */
  do {
    name += cur_char;
    context -> move(1);
  } while (isalnum((cur_char)) || check_char(cur_char, '_'));
  
  context -> drop_white_spaces();
  
  if (context -> check_and_drop(':')) {
    context -> drop_spaces();

    return std::make_unique<FillArgAST>(
      name,
      parse_dtype(context)); }
  else {
    return std::make_unique<FillArgAST>(
      name); }  
}

std::unique_ptr<VarTupleAST> parse_vars_tuple (
  std::shared_ptr<StyioContext> context) {
  std::vector<std::unique_ptr<VarAST>> vars;

  /*
    Danger!
    when entering parse_vars_tuple(), 
    the cur_char must be (
    this line will drop the next 1 character anyway!
  */
  context -> move(1);

  do {
    context -> arrive();

    if (context -> check_and_drop(')')) {
      return std::make_unique<VarTupleAST>(std::move(vars)); }
    else {
      if (context -> check_and_drop('*')) { 
        if (context -> check_and_drop('*')) {
          context -> drop_white_spaces();
          vars.push_back(std::move(std::make_unique<KwArgAST>(parse_id(context)))); }
        else {
          vars.push_back(std::move(std::make_unique<ArgAST>(parse_id(context)))); }
      }
      else{
        vars.push_back(std::move(parse_fill_arg(context))); }
    }
  } while (context -> check_and_drop(','));

  context -> arrive();

  find_and_drop_char_panic(context, cur_char, ')');

  return std::make_unique<VarTupleAST>(std::move(vars));
}

std::unique_ptr<ResourceAST> parse_resources (
  std::shared_ptr<StyioContext> context) {
  std::unique_ptr<ResourceAST> output;

  std::vector<std::unique_ptr<StyioAST>> resources;

  /*
    Danger!
    when entering parse_resources(), 
    the cur_char must be @
    this line will drop the next 1 character anyway!
  */
  context -> move(1);

  if (context -> check_and_drop('(')) {
    do {
      context -> arrive();

      std::unique_ptr<IdAST> varname = parse_id(context);

      context -> drop_spaces();

      if (context -> check_and_drop('<')) {
        match_next_char_panic(context, cur_char, '-'); }

      context -> drop_spaces();

      resources.push_back(std::make_unique<FinalBindAST>(
        std::move(varname), 
        parse_num_val(context)));
    } while (context -> check_and_drop(','));
    
    if (context -> check_and_drop(')')) { }
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
  std::shared_ptr<StyioContext> context) {
  std::unique_ptr<StyioAST> output;

  context -> drop_spaces();

  output = parse_num_val(context);
  
  context -> drop_spaces();

  switch (context -> get_cur_char())
  {
  case '=':
    {
      context -> move(1);

      if (check_char(cur_char, '=')) {
        context -> move(1);

        /*
          Equal
            expr == expr
        */

        // drop all spaces after ==
        context -> drop_spaces();
        
        output = std::make_unique<BinCompAST>(
          CompType::EQ,
          std::move(output),
          parse_num_val(context));
      };
    }

    break;

  case '!':
    {
      context -> move(1);

      if (check_char(cur_char, '=')) {
        context -> move(1);

        /*
          Not Equal
            expr != expr
        */

        // drop all spaces after !=
        context -> drop_spaces();

        output = std::make_unique<BinCompAST>(
          CompType::NE,
          std::move(output),
          parse_num_val(context));
      };
    }

    break;

  case '>':
    {
      context -> move(1);

      if (check_char(cur_char, '=')) {
        context -> move(1);

        /*
          Greater Than and Equal
            expr >= expr
        */

        // drop all spaces after >=
        context -> drop_spaces();

        output = std::make_unique<BinCompAST>(
          CompType::GE,
          std::move(output),
          parse_num_val(context));
      }
      else {
        /*
          Greater Than
            expr > expr
        */

        // drop all spaces after >
        context -> drop_spaces();

        output = std::make_unique<BinCompAST>(
          CompType::GT,
          std::move(output),
          parse_num_val(context));
      };
    }

    break;

  case '<':
    {
      context -> move(1);

      if (check_char(cur_char, '=')) {
        context -> move(1);

        /*
          Less Than and Equal
            expr <= expr
        */

        // drop all spaces after <=
        context -> drop_spaces();

        output = std::make_unique<BinCompAST>(
          CompType::LE,
          std::move(output),
          parse_num_val(context));
      }
      else {
        /*
          Less Than
            expr < expr
        */

        // drop all spaces after <
        context -> drop_spaces();

        output = std::make_unique<BinCompAST>(
          CompType::LT,
          std::move(output),
          parse_num_val(context));
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
  std::shared_ptr<StyioContext> context) {
  std::unique_ptr<StyioAST> output;

  if (isalpha(context -> get_cur_char()) || context -> check('_')) {
    output = parse_id(context); }

  if (check_char(cur_char, '[')) {
    output = parse_list_op(context, cur_char, std::move(output)); } 
  else if (check_char(cur_char, '(')) {
    output = parse_call(context); }

  context -> drop_spaces();

  if (check_binop_token(code)) {
    output = parse_binop_rhs(context, std::move(output)); };

  return output;
}

std::unique_ptr<StyioAST> parse_num_val (
  std::shared_ptr<StyioContext> context) {
  std::unique_ptr<StyioAST> output;

  if (isalpha(context -> get_cur_char()) || context -> check('_')) {
    return parse_id_or_value(context); }
  else if (isdigit(context -> get_cur_char())) {
    return parse_int_or_float(context); }
  else if (check_char(cur_char, '|')) {
    return parse_size_of(context); }

  std::string errmsg = std::string("parse_num_val() // Unexpected value expression, starting with .:| ") + char(cur_char) + " |:.";
  throw StyioParseError(errmsg);
}

std::unique_ptr<StyioAST> parse_item_for_binop (
  std::shared_ptr<StyioContext> context) {
  std::unique_ptr<StyioAST> output (new NoneAST());

  if (isalpha(context -> get_cur_char()) || context -> check('_')) {
    return parse_id(context); }
  else if (isdigit(context -> get_cur_char())) {
    return parse_int_or_float(context); }

  switch (context -> get_cur_char())
  {
  case '\"':
    return parse_str(context);

  case '\'':
    return parse_char_or_string(context);

  case '[':
    context -> move(1);

    context -> arrive();

    if (context -> check_and_drop(']')) {
      return std::make_unique<EmptyAST>(); }
    else {
      return parse_list_or_loop(context); }

    // You should NOT reach this line!
    break;

  case '|':
    return parse_size_of(context);

    // You should NOT reach this line!
    break;
  
  default:
    break;
  }

  return output;
}

std::unique_ptr<StyioAST> parse_expr (
  std::shared_ptr<StyioContext> context) {
  std::unique_ptr<StyioAST> output;

  if (isalpha(context -> get_cur_char()) || context -> check('_')) 
  {
    output = parse_id(context);
    
    context -> drop_spaces();

    if (check_binop_token(code)) {
      output = parse_binop_rhs(context, std::move(output)); }

    return output;
  }
  else if (isdigit(context -> get_cur_char())) 
  {
    output = parse_int_or_float(context);

    context -> drop_white_spaces();

    if (check_binop_token(code)) {
      output = parse_binop_rhs(context, std::move(output)); }

    return output;
  }

  switch (context -> get_cur_char())
  {
  case '\'':
    {
      return parse_char_or_string(context);
    }
  
  case '\"':
    {
      return parse_str(context);
    }

  case '[':
    {
      context -> move(1);

      context -> arrive();

      if (context -> check_and_drop(']')) {
        output = std::make_unique<EmptyAST>(); }
      else {
        output = parse_list_or_loop(context); }

      return output;
    }

    break;

  case '(':
    {
      return parse_tuple(context);
    }

    break;

  case '{':
    {
      return parse_set(context);
    }

    break;

  case '|':
    {
      output = parse_size_of(context);

      context -> drop_white_spaces();

      if (check_binop_token(code)) {
        output = parse_binop_rhs(context, std::move(output)); }

      return output;
    }

    break;

  case '\\':
    {
      context -> move(1);

      if (context -> check_and_drop('t')) {
        context -> check_and_drop('\\'); 
        return std::make_unique<TrueAST>(); }
      else if (context -> check_and_drop('f')) {
        context -> check_and_drop('\\'); 
        return std::make_unique<FalseAST>(); }
    }

    break;

  case '$':
    {
      context -> move(1);

      return parse_fmt_str(context);
    }
  
  default:
    {
      output = std::make_unique<NoneAST>();
    }
    break;
  }

  return output;
}

std::unique_ptr<StyioAST> parse_tuple (
  std::shared_ptr<StyioContext> context) {
  std::vector<std::unique_ptr<StyioAST>> exprs;

  /*
    Danger!
    when entering parse_tuple(), 
    the cur_char must be (
    this line will drop the next 1 character anyway!
  */
  context -> move(1);

  do {
    context -> arrive();

    if (context -> check_and_drop(')')) {
      return std::make_unique<TupleAST>(std::move(exprs)); }
    else {
      exprs.push_back(parse_expr(context));
      context -> drop_white_spaces(); }
  } while (context -> check_and_drop(','));

  context -> check_and_drop(')');

  if (exprs.size() == 0) {
    return std::make_unique<EmptyAST>(); }
  else {
    return std::make_unique<TupleAST>(std::move(exprs)); }
}

std::unique_ptr<StyioAST> parse_list (
  std::shared_ptr<StyioContext> context) {
  std::vector<std::unique_ptr<StyioAST>> exprs;

  /*
    Danger!
    when entering parse_list(), 
    the cur_char must be [
    this line will drop the next 1 character anyway!
  */
  context -> move(1);

  do {
    context -> arrive();

    if (context -> check_and_drop(']')) {
      return std::make_unique<ListAST>(std::move(exprs)); }
    else {
      exprs.push_back(parse_expr(context));
      context -> drop_white_spaces(); }
  } while (context -> check_and_drop(','));

  context -> check_and_drop(']');
  
  if (exprs.size() == 0) {
    return std::make_unique<EmptyAST>(); }
  else {
    return std::make_unique<ListAST>(std::move(exprs)); }
}

std::unique_ptr<StyioAST> parse_set (
  std::shared_ptr<StyioContext> context) {
  std::vector<std::unique_ptr<StyioAST>> exprs;

  /*
    Danger!
    when entering parse_set(), 
    the cur_char must be {
    this line will drop the next 1 character anyway!
  */
  context -> move(1);

  do {
    context -> arrive();

    if (context -> check_and_drop('}')) {
      return std::make_unique<SetAST>(std::move(exprs)); }
    else {
      exprs.push_back(parse_expr(context));
      context -> drop_white_spaces(); }
  } while (context -> check_and_drop(','));

  context -> check_and_drop('}');

  if (exprs.size() == 0) {
    return std::make_unique<EmptyAST>(); }
  else {
    return std::make_unique<SetAST>(std::move(exprs)); }
}

std::unique_ptr<StyioAST> parse_iterable (
  std::shared_ptr<StyioContext> context) {
  std::unique_ptr<StyioAST> output (new EmptyAST());

  if (isalpha(context -> get_cur_char()) || context -> check('_')) 
  {
    output = parse_id(context);
    
    context -> drop_spaces();

    if (check_binop_token(code)) {
      output = parse_binop_rhs(context, std::move(output)); };

    return output;
  }
  else
  {
    switch (context -> get_cur_char())
    {
    case '(':
      {
        context -> move(1);

        std::vector<std::unique_ptr<StyioAST>> exprs;
        do {
          context -> arrive();

          if (context -> check_and_drop(')')) {
            return std::make_unique<TupleAST>(std::move(exprs)); }
          else {
            exprs.push_back(parse_expr(context));
            context -> drop_white_spaces(); }
        } while (context -> check_and_drop(','));

        context -> check_and_drop(')');

        if (exprs.size() == 0) {
          return std::make_unique<EmptyAST>(); }
        else {
          return std::make_unique<TupleAST>(std::move(exprs)); }
      }

    case '[':
      {
        context -> move(1);

        std::vector<std::unique_ptr<StyioAST>> exprs;
        do {
          context -> arrive();

          if (context -> check_and_drop(']')) {
            return std::make_unique<ListAST>(std::move(exprs)); }
          else {
            exprs.push_back(parse_expr(context));
            context -> drop_white_spaces(); }
        } while (context -> check_and_drop(','));

        context -> check_and_drop(']');

        if (exprs.size() == 0) {
          return std::make_unique<EmptyAST>(); }
        else {
          return std::make_unique<ListAST>(std::move(exprs)); }
      }

    case '{':
      {
        context -> move(1);

        std::vector<std::unique_ptr<StyioAST>> exprs;
        do {
          context -> arrive();

          if (context -> check_and_drop('}')) {
            return std::make_unique<SetAST>(std::move(exprs)); }
          else {
            exprs.push_back(parse_expr(context));
            context -> drop_white_spaces(); }
        } while (context -> check_and_drop(','));

        context -> check_and_drop('}');

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
  std::shared_ptr<StyioContext> context) {
  std::unique_ptr<SizeOfAST> output;

  // eliminate | at the start
  context -> move(1);
       
  if (isalpha(context -> get_cur_char()) || context -> check('_'))
  {
    std::unique_ptr<StyioAST> var = parse_id_or_value(context);

    // eliminate | at the end
    if (check_char(cur_char, '|')) {
      context -> move(1);

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
  std::shared_ptr<StyioContext> context) {
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

std::unique_ptr<StyioAST> parse_list_op (
  std::shared_ptr<StyioContext> context,
  std::unique_ptr<StyioAST> theList) {
  std::unique_ptr<StyioAST> output;

  /*
    Danger!
    when entering parse_list_op(), 
    the cur_char must be [
    this line will drop the next 1 character anyway!
  */
  context -> move(1);

  do
  {
    if (isalpha(context -> get_cur_char()) || context -> check('_'))
    {
      output = std::make_unique<ListOpAST>(
        StyioNodeHint::Access,
        std::move(theList),
        parse_id_or_value(context));
    }
    else if (isdigit(context -> get_cur_char())) 
    {
      output = std::make_unique<ListOpAST>(
        StyioNodeHint::Access_By_Index,
        std::move(theList),
        parse_int(context));
    }
    else
    {
      switch (context -> get_cur_char())
      {
      /*
        list["any"]
      */
      case '"':
        {
          output = std::make_unique<ListOpAST>(
            StyioNodeHint::Access_By_Name,
            std::move(theList),
            parse_str(context)); 
        }
        
        // You should NOT reach this line!
        break;

      /*
        list[<]
      */
      case '<':
        {
          context -> move(1);

          while (check_char(cur_char, '<')) {
            context -> move(1); }

          output = std::make_unique<ListOpAST>(
            StyioNodeHint::Get_Reversed,
            std::move(theList));
        }

        // You should NOT reach this line!
        break;

      // list[?= item]
      case '?':
        {
          context -> move(1);

          if (context -> check_and_drop('='))
          {
            context -> arrive();

            output = std::make_unique<ListOpAST>(
              StyioNodeHint::Get_Index_By_Value,
              std::move(theList),
              parse_expr(context));
          }
          else if (context -> check_and_drop('^'))
          {
            context -> arrive();

            output = std::make_unique<ListOpAST>(
              StyioNodeHint::Get_Indices_By_Many_Values,
              std::move(theList),
              parse_iterable(context));
          }
          else
          {
            std::string errmsg = std::string("Expecting ?= or ?^, but got ") + char(cur_char);
            throw StyioSyntaxError(errmsg);
          }
        }

        // You should NOT reach this line!
        break;

      /*
        list[^index]
        list[^index <- value]
      */
      case '^':
        {
          context -> move(1);

          context -> drop_white_spaces();

          std::unique_ptr<StyioAST> index = parse_int(context);

          context -> drop_white_spaces();

          /*
            list[^index <- value]
          */
          if (check_and_drop_symbol(context, cur_char, "<-")) 
          {
            context -> drop_white_spaces();

            output = std::make_unique<ListOpAST>(
              StyioNodeHint::Insert_Item_By_Index,
              std::move(theList),
              std::move(index),
              parse_expr(context)); 
          }
          // list[^index]
          else 
          {
            output = std::make_unique<ListOpAST>(
              StyioNodeHint::Access_By_Index,
              std::move(theList),
              std::move(index)); 
          }
        }
        // You should NOT reach this line!
        break;
      
      /*
        list[+: value]
      */
      case '+':
        {
          context -> move(1);

          match_next_char_panic(context, cur_char, ':');

          context -> drop_white_spaces();

          std::unique_ptr<StyioAST> expr = parse_expr(context);

          context -> drop_white_spaces();

          output = std::make_unique<ListOpAST>(
            StyioNodeHint::Append_Value,
            std::move(theList),
            std::move(expr));
        }

        // You should NOT reach this line!
        break;
      
      case '-':
        {
          context -> move(1);

          match_next_char_panic(context, cur_char, ':');

          context -> drop_white_spaces();
          
          /*
            list[-: ^index]
          */
          if (context -> check_and_drop('^'))
          {
            context -> drop_white_spaces();

            if (isdigit(context -> get_cur_char()))
            {
              output = std::make_unique<ListOpAST>(
              StyioNodeHint::Remove_Item_By_Index,
              std::move(theList),
              std::move(parse_int(context)));
            }
            else
            {
              /*
                list[-: ^(i0, i1, ...)]
              */
              output = std::make_unique<ListOpAST>(
                StyioNodeHint::Remove_Items_By_Many_Indices,
                std::move(theList),
                std::move(parse_iterable(context)));
            }
          }
          else if (context -> check_and_drop('?')) 
          {
            switch (context -> get_cur_char())
            {
            /*
              list[-: ?= value]
            */
            case '=':
              {
                context -> move(1);

                context -> drop_white_spaces();

                output = std::make_unique<ListOpAST>(
                  StyioNodeHint::Remove_Item_By_Value,
                  std::move(theList),
                  parse_expr(context));
              }

              break;
            
            /*
              list[-: ?^ (v0, v1, ...)]
            */
            case '^':
              {
                context -> move(1);

                context -> drop_white_spaces();

                output = std::make_unique<ListOpAST>(
                  StyioNodeHint::Remove_Items_By_Many_Values,
                  std::move(theList),
                  parse_iterable(context));
              }

              break;
            
            default:
              break;
            }
          }
          else 
          {
            output = std::make_unique<ListOpAST>(
              StyioNodeHint::Remove_Item_By_Value,
              std::move(theList),
              parse_expr(context));
          }
        }

        // You should NOT reach this line!
        break;

      case ']':
        {
          output = std::move(theList);
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
  } while (check_char(cur_char, '['));

  find_and_drop_char(context, cur_char, ']');

  return output;
}

std::unique_ptr<StyioAST> parse_loop_or_iter (
  std::shared_ptr<StyioContext> context,
  std::unique_ptr<StyioAST> iterOverIt) {

  context -> arrive();

  if ((iterOverIt -> hint()) == StyioNodeHint::Infinite) {
    return std::make_unique<LoopAST>(
      parse_forward(context, false)); }
  else {
    return std::make_unique<IterAST>(
      std::move(iterOverIt),
      parse_forward(context, false)); }
}


std::unique_ptr<StyioAST> parse_list_or_loop (
  std::shared_ptr<StyioContext> context) {
  std::unique_ptr<StyioAST> output;

  std::vector<std::unique_ptr<StyioAST>> elements;

  std::unique_ptr<StyioAST> startEl = parse_expr(context);

  context -> drop_white_spaces();

  if (context -> check_and_drop('.')) {
    while (check_char(cur_char, '.')) {
      context -> move(1); }

    context -> drop_white_spaces();
      
    std::unique_ptr<StyioAST> endEl = parse_expr(context);

    context -> drop_white_spaces();

    match_next_char_panic(context, cur_char, ']');

    if (startEl -> hint() == StyioNodeHint::Int 
      && endEl -> hint() == StyioNodeHint::Id) {
      output = std::make_unique<InfiniteAST>(
        std::move(startEl), 
        std::move(endEl)); }
    else
    if (startEl -> hint() == StyioNodeHint::Int 
      && endEl -> hint() == StyioNodeHint::Int) {
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
  else if (context -> check_and_drop(',')) {
    elements.push_back(std::move(startEl));

    do {
      context -> arrive();

      if (context -> check_and_drop(']')) {
        return std::make_unique<ListAST>(std::move(elements)); }
      else {
        elements.push_back(parse_expr(context)); }
    } while (context -> check_and_drop(','));

    find_and_drop_char_panic(context, cur_char, ']');

    output = std::make_unique<ListAST>(std::move(elements));
  }
  else {
    elements.push_back(std::move(startEl));

    find_and_drop_char_panic(context, cur_char, ']');

    output = std::make_unique<ListAST>(std::move(elements));
  }

  while (check_char(cur_char, '[')) {
      output = parse_list_op(context, cur_char, std::move(output)); }

  context -> drop_spaces();

  if (check_and_drop_symbol(context, cur_char, ">>")) {
    output = parse_loop_or_iter(context, cur_char, std::move(output)); }

  return output;
}

std::unique_ptr<StyioAST> parse_loop (
  std::shared_ptr<StyioContext> context) {
  std::unique_ptr<StyioAST> output;

  while (check_char(cur_char, '.')) { 
    context -> move(1); }

  find_and_drop_char_panic(context, cur_char, ']');

  context -> drop_spaces();

  if (check_and_drop_symbol(context, cur_char, ">>")) {
    context -> drop_spaces();

    return std::make_unique<LoopAST>(
      parse_forward(context));
  }

  return std::make_unique<InfiniteAST>();
}

std::unique_ptr<BinOpAST> parse_binop_rhs (
  std::shared_ptr<StyioContext> context, 
  std::unique_ptr<StyioAST> lhs_ast) {
  std::unique_ptr<BinOpAST> output;

  switch (context -> get_cur_char())
  {
    // Bin_Add := <ID> "+" <EXPR>
    case '+':
      {
        move_next_and_ignore(context);

        if (context -> check_and_drop('=')) 
        {
          context -> drop_spaces();

          output = std::make_unique<BinOpAST>(
          StyioNodeHint::Inc_Add, 
          std::move(lhs_ast), 
          std::move(parse_item_for_binop(context)));

          return output;
        }
        else
        {
          output = std::make_unique<BinOpAST>(
          StyioNodeHint::Bin_Add, 
          std::move(lhs_ast), 
          std::move(parse_item_for_binop(context)));
        }
      };

      // You should NOT reach this line!
      break;

    // Bin_Sub := <ID> "-" <EXPR>
    case '-':
      {
        move_next_and_ignore(context);

        if (context -> check_and_drop('=')) 
        {
          context -> drop_spaces();

          output = std::make_unique<BinOpAST>(
          StyioNodeHint::Inc_Sub, 
          std::move(lhs_ast), 
          std::move(parse_item_for_binop(context)));

          return output;
        }
        else
        {
          output = std::make_unique<BinOpAST>(
            StyioNodeHint::Bin_Sub, 
            std::move(lhs_ast), 
            std::move(parse_item_for_binop(context)));
        }
      };

      // You should NOT reach this line!
      break;

    // Bin_Mul | Bin_Pow
    case '*':
      {
        context -> move(1);
        // Bin_Pow := <ID> "**" <EXPR>
        if (context -> check_and_drop('*'))
        {
          move_next_and_ignore(context);

          // <ID> "**" |--
          output = std::make_unique<BinOpAST>(
            StyioNodeHint::Bin_Pow, 
            std::move(lhs_ast), 
            std::move(parse_item_for_binop(context)));
        }
        else if (context -> check_and_drop('=')) 
        {
          context -> drop_spaces();

          output = std::make_unique<BinOpAST>(
            StyioNodeHint::Inc_Mul, 
            std::move(lhs_ast), 
            std::move(parse_item_for_binop(context)));

          return output;
        }
        // Bin_Mul := <ID> "*" <EXPR>
        else 
        {
          context -> drop_spaces();

          // <ID> "*" |--
          output = std::make_unique<BinOpAST>(
            StyioNodeHint::Bin_Mul, 
            std::move(lhs_ast), 
            std::move(parse_item_for_binop(context)));
        }
      };
      // You should NOT reach this line!
      break;
      
    // Bin_Div := <ID> "/" <EXPR>
    case '/':
      {
        move_next_and_ignore(context);

        if (context -> check_and_drop('=')) 
        {
          context -> drop_spaces();
          
          output = std::make_unique<BinOpAST>(
            StyioNodeHint::Inc_Div, 
            std::move(lhs_ast), 
            std::move(parse_item_for_binop(context)));

          return output;
        }
        else
        {
          output = std::make_unique<BinOpAST>(
            StyioNodeHint::Bin_Div, 
            std::move(lhs_ast), 
            std::move(parse_item_for_binop(context)));
        }
      };

      // You should NOT reach this line!
      break;

    // Bin_Mod := <ID> "%" <EXPR> 
    case '%':
      {
        move_next_and_ignore(context);

        // <ID> "%" |-- 
        output = std::make_unique<BinOpAST>(
          StyioNodeHint::Bin_Mod, 
          std::move(lhs_ast), 
          std::move(parse_item_for_binop(context)));
      };

      // You should NOT reach this line!
      break;
    
    default:
      std::string errmsg = std::string("Unexpected BinOp.Operator: `") + char(cur_char) + "`.";
      throw StyioSyntaxError(errmsg);

      // You should NOT reach this line!
      break;
  }

  context -> drop_spaces();

  while (check_binop_token(code)) 
  {
    context -> drop_spaces();

    output = parse_binop_rhs(context, std::move(output));
  }

  return output;
}

std::unique_ptr<CondAST> parse_cond_rhs (
  std::shared_ptr<StyioContext> context,
  std::unique_ptr<StyioAST> lhsExpr
)
{
  std::unique_ptr<CondAST> condExpr;

  context -> drop_spaces();

  switch (context -> get_cur_char())
  {
  case '&':
    {
      context -> move(1);

      context -> check_and_drop('&');

      /*
        support:
          expr && \n
          expression
      */

      context -> drop_spaces();

      condExpr = std::make_unique<CondAST>(
        LogicType::AND,
        std::move(lhsExpr),
        parse_cond(context)
      );
    }

    break;

  case '|':
    {
      context -> move(1);

      if (check_char(cur_char, '|'))
      {
        context -> move(1);
      };

      /*
        support:
          expr || \n
          expression
      */

      context -> drop_spaces();

      condExpr = std::make_unique<CondAST>(
        LogicType::OR,
        std::move(lhsExpr),
        parse_cond(context)
      );
    }

    break;

  case '^':
    {
      context -> move(1);

      /*
        support:
          expr ^ \n
          expression
      */

      context -> drop_spaces();

      condExpr = std::make_unique<CondAST>(
        LogicType::OR,
        std::move(lhsExpr),
        parse_cond(context)
      );
    }

    break;

  case '!':
    {
      context -> move(1);

      if (check_char(cur_char, '('))
      {
        context -> move(1);

        /*
          support:
            !( \n
              expr
            )
        */
        context -> drop_spaces();

        condExpr = std::make_unique<CondAST>(
          LogicType::NOT,
          parse_cond(context)
        );

        find_and_drop_char_panic(context, cur_char, ')');
      }
    }

    break;

  default:
    break;
  }

  context -> drop_spaces();

  while (!(check_char(cur_char, ')')))
  {
    condExpr = std::move(parse_cond_rhs(context, cur_char, std::move(condExpr)));
  }
  
  return condExpr;
}

std::unique_ptr<CondAST> parse_cond (
  std::shared_ptr<StyioContext> context
)
{
  std::unique_ptr<StyioAST> lhsExpr;

  if (check_char(cur_char, '('))
  {
    context -> move(1);

    lhsExpr = std::move(parse_cond(context));

    find_and_drop_char_panic(context, cur_char, ')');
  }
  else
  if (check_char(cur_char, '!'))
  {
    context -> move(1);

    if (check_char(cur_char, '('))
    {
      context -> move(1);

      /*
        support:
          !( \n
            expr
          )
      */
      context -> drop_spaces();

      lhsExpr = std::move(parse_cond(context));

      context -> drop_spaces();

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
    lhsExpr = std::move(parse_item_for_cond(context));
  };

  // drop all spaces after first value
  context -> drop_spaces();

  if (check_char(cur_char, '&')
    || check_char(cur_char, '|'))
  {
    return parse_cond_rhs(context, cur_char, std::move(lhsExpr));
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
  std::shared_ptr<StyioContext> context){
  /*
    Danger!
    when entering parse_cond_flow(), 
    the cur_char must be ?
    this line will drop the next 1 character anyway!
  */
  context -> move(1);

  context -> drop_white_spaces();

  if (context -> check_and_drop('(')) {
    std::unique_ptr<CondAST> condition = parse_cond(context);

    find_and_drop_char_panic(context, cur_char, ')');

    /*
      support:
        ?() \n
        \t\

        ?() \n
        \f\
    */
    context -> arrive();

    if (context -> check_and_drop('\\'))
    {
      std::unique_ptr<StyioAST> block;

      if (context -> check_and_drop('t'))
      {
        context -> check_and_drop('\\');

        /*
          support:
            \t\ \n
            {}
        */

        context -> arrive();

        block = parse_block(context);

        /*
          support:
            \t\ {} \n
            \f\
        */
        context -> arrive();

        if (context -> check_and_drop('\\'))
        {
          match_next_char_panic(context, cur_char, 'f');

          context -> check_and_drop('\\');

          /*
            support:
              \f\ \n
              {}
          */
          context -> arrive();

          std::unique_ptr<StyioAST> blockElse = parse_block(context);

          return std::make_unique<CondFlowAST>(
            StyioNodeHint::CondFlow_Both,
            std::move(condition),
            std::move(block),
            std::move(blockElse));
        }
        else 
        {
          return std::make_unique<CondFlowAST>(
            StyioNodeHint::CondFlow_True,
            std::move(condition),
            std::move(block)); 
        }
      }
      else if (context -> check_and_drop('f')) 
      {
        context -> check_and_drop('\\');

        /*
          support:
            \f\ \n
            {}
        */
        context -> arrive();

        block = parse_block(context);

        return std::make_unique<CondFlowAST>(
          StyioNodeHint::CondFlow_False,
          std::move(condition),
          std::move(block)); }
      else 
      {
        std::string errmsg = std::string("parse_cond_flow() // Unexpected character ") + cur_char;
        throw StyioSyntaxError(errmsg); 
      }
    }
  }
  else 
  {
    std::string errmsg = std::string("Missing （ for ?(`expr`).");
    throw StyioSyntaxError(errmsg);
  }

  std::string errmsg = std::string("parse_cond_flow() // You should not reach the end of this function. Char: ") + cur_char;
  throw StyioParseError(errmsg);
}

std::unique_ptr<StyioAST> parse_pipeline (
  std::shared_ptr<StyioContext> context) {
  /*
    Danger!
    when entering parse_pipeline(), 
    the cur_char must be #
    this line will drop the next 1 character anyway!
  */
  context -> move(1);

  context -> drop_white_spaces();

  if (isalpha(context -> get_cur_char()) || context -> check('_')) 
  {
    std::unique_ptr<IdAST> name = parse_id(context);

    context -> arrive();

    if (context -> check_and_drop(':')) 
    {
      if (context -> check_and_drop('=')) {
        context -> drop_spaces();

        return std::make_unique<FuncAST>(
          std::move(name),
          parse_forward(context, true),
          true);
      }

      context -> arrive();

      std::shared_ptr<DTypeAST> dtype = parse_dtype(context);

      context -> arrive();

      if (context -> check_and_drop(':')) {
        if (context -> check_and_drop('=')) {
          context -> arrive();

          return std::make_unique<FuncAST>(
            std::move(name),
            std::move(dtype),
            parse_forward(context, true),
            true);
        }
      }
      else if (context -> check_and_drop('=')) {
        context -> arrive();

        return std::make_unique<FuncAST>(
          std::move(name),
          std::move(dtype),
          parse_forward(context, true),
          false);
      }

      std::string errmsg = std::string("parse_pipeline() // Inheritance, Type Hint.");
      throw StyioNotImplemented(errmsg);
    }
    else if (context -> check_and_drop('=')) {
      if (check_char(cur_char, '>')) 
      {
        move_backward(context, cur_char, 1);

        return std::make_unique<FuncAST>(
          std::move(name),
          parse_forward(context, true),
          false);
      }
      else 
      {
        context -> drop_spaces();

        return std::make_unique<FuncAST>(
          std::move(name),
          parse_forward(context, true),
          false);
      }
    }
  }

  context -> drop_spaces();
  return parse_forward(context, true);
}

std::unique_ptr<ForwardAST> parse_forward (
  std::shared_ptr<StyioContext> context,
  bool ispipe) {
  std::unique_ptr<ForwardAST> output;

  std::unique_ptr<VarTupleAST> tmpvars;
  bool hasVars = false;

  if (ispipe) {
    if (check_char(cur_char, '(')) {
      tmpvars = parse_vars_tuple(context);
      hasVars = true; } }
  else if (context -> check_and_drop('#')) {
    context -> drop_white_spaces();

    if (check_char(cur_char, '(')) {
      tmpvars = parse_vars_tuple(context);
      hasVars = true; }
    else {
      std::string errmsg = std::string("parse_forward() // Expecting ( after #, but got ") + char(cur_char);
      throw StyioSyntaxError(errmsg); } }

  context -> drop_spaces();

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
  switch (context -> get_cur_char())
  {
  case '?':
    {
      context -> move(1);

      switch (context -> get_cur_char())
      {
      /*
        ?= Value
      */
      case '=':
        {
          context -> move(1);

          context -> drop_spaces();

          if (context -> check_and_drop('{')) {
            if (hasVars) {
              output = std::make_unique<ForwardAST>(
                std::move(tmpvars), 
                parse_cases(context)); }
            else {
              output = std::make_unique<ForwardAST>(
                parse_cases(context)); } }
          else {
            std::unique_ptr<CheckEqAST> value;
            std::unique_ptr<StyioAST> then;

            context -> drop_white_spaces();
            
            value = std::make_unique<CheckEqAST>(parse_expr(context));

            context -> drop_spaces();

            if (check_and_drop_symbol(context, cur_char, "=>")) {
              context -> drop_spaces();

              if (check_char(cur_char, '{')) { 
                then = parse_block(context); }
              else {
                then = parse_expr(context); }

              if (hasVars) {
                output = std::make_unique<ForwardAST>(
                  std::move(tmpvars), 
                  std::move(value), 
                  std::move(then)); }
              else {
                output = std::make_unique<ForwardAST>(
                  std::move(value), 
                  std::move(then)); } }
            else {
              std::string errmsg = std::string("parse_forward() // Expecting `=>` after `?= value`, but got ") + char(cur_char);
                throw StyioSyntaxError(errmsg); }
          }
        } 

        break;
      /*
        ?^ [Iterable]
      */
      case '^':
        {
          context -> move(1);

          std::unique_ptr<StyioAST> nextExpr;

          std::unique_ptr<StyioAST> iterable;

          context -> drop_white_spaces();
          
          switch (context -> get_cur_char())
          {
          case '(':
            {
              context -> move(1);

              std::vector<std::unique_ptr<StyioAST>> exprs;
              do {
                context -> arrive();

                if (check_char(cur_char, ')')) {
                  break; }
                else {
                  exprs.push_back(parse_expr(context)); }
              } while (context -> check_and_drop(','));

              find_and_drop_char(context, cur_char, ')');

              iterable = std::make_unique<TupleAST>(std::move(exprs));
            }
            break;
          
          case '[':
            {
              context -> move(1);

              std::vector<std::unique_ptr<StyioAST>> exprs;
              do {
                context -> arrive();

                if (check_char(cur_char, ']')) {
                  break; }
                else {
                  exprs.push_back(parse_expr(context)); }
              } while (context -> check_and_drop(','));

              find_and_drop_char(context, cur_char, ']');

              iterable = std::make_unique<ListAST>(std::move(exprs));
            }

            break;
          
          case '{':
            {
              context -> move(1);

              std::vector<std::unique_ptr<StyioAST>> exprs;
              do {
                context -> arrive();

                if (check_char(cur_char, '}')) {
                  break; }
                else {
                  exprs.push_back(parse_expr(context)); }
              } while (context -> check_and_drop(','));

              find_and_drop_char(context, cur_char, '}');

              iterable = std::make_unique<SetAST>(std::move(exprs));
            }
            break;
          
          default:
            {
              if (isalpha(context -> get_cur_char()) || context -> check('_')) {
                iterable = parse_id_or_value(context); } 
              else {
                std::string errmsg = std::string("parse_forward() // Unexpected collection, starting with ") + char(cur_char);
                throw StyioSyntaxError(errmsg); }
            }
            break;
          }

          context -> arrive();

          if (check_and_drop_symbol(context, cur_char, "=>")) {
            context -> drop_spaces();

            if (check_char(cur_char, '{')) { 
              nextExpr = parse_block(context); }
            else {
              nextExpr = parse_expr(context); }

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
        move_backward(context, cur_char, 1);

        if (hasVars) {
          output = std::make_unique<ForwardAST>(
            std::move(tmpvars),
            parse_cond_flow(context)); }
        else {
          output = std::make_unique<ForwardAST>(
            parse_cond_flow(context)); }

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
      context -> move(1);

      match_next_char_panic(context, cur_char, '>');

      context -> drop_spaces();

      if (check_char(cur_char, '{')) {
        if (hasVars) {
          output = std::make_unique<ForwardAST>(
            std::move(tmpvars), 
            parse_block(context)); }
        else {
          output = std::make_unique<ForwardAST>(
            parse_block(context)); } }
      else {
        if (hasVars) {
          output = std::make_unique<ForwardAST>(
            std::move(tmpvars), 
            parse_expr(context)); }
        else {
          output = std::make_unique<ForwardAST>(
            parse_expr(context)); } }
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
          parse_block(context)); }
      else {
        output = std::make_unique<ForwardAST>(
          parse_block(context)); }
    }
    break;
  
  default:
    std::string errmsg = std::string("parse_forward() // Unexpected character ") + char(cur_char);
    throw StyioSyntaxError(errmsg);

    break;
  }

  // context -> arrive();

  // while (check_and_drop_symbol(context, cur_char, "|>")) {
  //   context -> drop_spaces();

  //   output = std::make_unique<FromToAST>(
  //     std::move(output),
  //     parse_forward(context));

  //   context -> arrive(); 
  // }

  return output;
}

std::unique_ptr<StyioAST> parse_read_file (
  std::shared_ptr<StyioContext> context, 
  std::unique_ptr<IdAST> id_ast) {
  if (check_char(cur_char, '@')) {
    return std::make_unique<ReadFileAST>(
      std::move(id_ast), 
      parse_path_or_link(context)); }
  else {
    std::string errmsg = std::string("Unexpected Read.Path, starting with character `") + char(cur_char) + "`";
    throw StyioSyntaxError(errmsg); }
}

std::unique_ptr<StyioAST> parse_print (
  std::shared_ptr<StyioContext> context) {
  std::unique_ptr<StyioAST> output;

  std::vector<std::unique_ptr<StyioAST>> exprs;

  /*
    Danger!
    when entering parse_print(), 
    the following symbol must be >_
    this line will drop the next 2 characters anyway!
  */
  move_forward(context, cur_char, 2);

  match_next_char_panic(context, cur_char, '(');

  do {
    context -> arrive();

    if (context -> check_and_drop(')')) {
      return std::make_unique<PrintAST>(std::move(exprs)); }
    else {
      exprs.push_back(parse_expr(context)); }
  } while (context -> check_and_drop(','));

  find_and_drop_char_panic(context, cur_char, ')');
  return std::make_unique<PrintAST>(std::move(exprs));
}

std::unique_ptr<StyioAST> parse_panic (
  std::shared_ptr<StyioContext> context) {
  do
  {
    /*
      Danger!
      when entering parse_panic(), 
      the following symbol must be !
      this line will drop the next 1 character anyway!
    */
    context -> move(1);
  } while (check_char(cur_char, '!'));
  
  if (find_and_drop_char(context, cur_char, '(')) {
    /*
      parse_one_or_many_repr
      parse_fmt_str
    */

    
  } else {

  }
}

std::unique_ptr<StyioAST> parse_stmt (
  std::shared_ptr<StyioContext> context) {
  context -> arrive();
  
  if (isalpha(context -> get_cur_char()) || context -> check('_')) 
  {
    std::unique_ptr<IdAST> id_ast = parse_id(context);

    if (check_char(cur_char, '[')) {
      return parse_list_op(context, cur_char, std::move(id_ast)); }
    
    context -> arrive();

    if (check_binop_token(code)) {
      return parse_binop_rhs(context, std::move(id_ast)); } 

    switch (context -> get_cur_char())
    {
      case '=':
        {
          context -> move(1);

          context -> arrive();

          return std::make_unique<FlexBindAST>(
            std::move(id_ast), 
            std::move(parse_expr(context)));
        };

        // You should NOT reach this line!
        break;
      
      case ':':
        {
          context -> move(1);

          if (context -> check_and_drop('=')) 
          {
            context -> arrive();
            
            return std::make_unique<FinalBindAST>(
              std::move(id_ast), 
              std::move(parse_expr(context)));
          }
          else 
          {
            context -> drop_white_spaces();

            std::shared_ptr<DTypeAST> type = parse_dtype(context);

            context -> drop_white_spaces();

            if (context -> check_and_drop(':')) 
            {
              if (context -> check_and_drop('=')) 
              {
                context -> drop_white_spaces();
                
                return std::make_unique<FinalBindAST>(
                  std::move(id_ast), 
                  std::move(parse_expr(context)));
              }
            }
            else if (context -> check_and_drop('='))
            {
              context -> drop_white_spaces();
              
              return std::make_unique<FlexBindAST>(
                std::move(id_ast), 
                std::move(parse_expr(context)));
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
          context -> move(1);

          if (context -> check_and_drop('-'))
          {
            context -> drop_spaces();

            return parse_read_file(context, cur_char, std::move(id_ast));
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
          context -> move(1);

          if (context -> check_and_drop('>')) 
          { 
            context -> drop_spaces();

            return parse_loop_or_iter(context, cur_char, std::move(id_ast)); 
          }
        }
        
        // You should NOT reach this line!
        break;

      default:
        break;
    }
  }
  // Int / Float
  else if (isdigit(context -> get_cur_char())) {
    std::unique_ptr<StyioAST> numAST = parse_int_or_float(context);

    context -> arrive();

    if (check_binop_token(code)) {
      return parse_binop_rhs(context, std::move(numAST)); } 
    else { return numAST; }
  }
  // Print
  else if (check_symbol(context, ">_")) {
    return parse_print(context);
  }

  switch (context -> get_cur_char())
  {
  case EOF:
    return std::make_unique<EndAST>();

    // You should NOT reach this line!
    break;

  case '\"':
    return parse_str(context);

    // You should NOT reach this line!
    break;

  case '?':
    return parse_cond_flow(context);
    
    // You should NOT reach this line!
    break;

  case '!':
    return parse_panic(context);
    
    // You should NOT reach this line!
    break;

  case '#':
    return parse_pipeline(context);

    // You should NOT reach this line!
    break;

  case '.':
    {
      context -> move(1);
      while (check_char(cur_char, '.')) {
        context -> move(1);
      }
      return std::make_unique<PassAST>();
    }

    // You should NOT reach this line!
    break;

  case '^':
    {
      context -> move(1);

      while (check_char(cur_char, '^')) {
        context -> move(1); }

      return std::make_unique<BreakAST>();
    }

    // You should NOT reach this line!
    break;

  case '@':
    {
      std::unique_ptr<ResourceAST> resources = parse_resources(context);

      context -> arrive();

      if (check_and_drop_symbol(context, cur_char, "->")) {
        context -> arrive();

        return std::make_unique<FromToAST>(
          std::move(resources), 
          parse_block(context));
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
      context -> move(1);

      context -> arrive();

      if (context -> check_and_drop('.')) {
        return parse_loop(context); }
      else {
        return parse_list_or_loop(context); }
    }
    
    // You should NOT reach this line!
    break;

  case '=':
    {
      context -> move(1);

      if (context -> check_and_drop('>'))
      {
        context -> drop_white_spaces();
      
        return std::make_unique<ReturnAST>(parse_expr(context));
      }
      else 
      {
        std::string errmsg = std::string("parse_stmt() // =");
        throw StyioSyntaxError(errmsg);
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
  std::shared_ptr<StyioContext> context
)
{
  std::string itemStr;

  if (check_char(cur_char, '\"'))
  {
    // eliminate double quote symbol " at the start of dependency item
    context -> move(1);

    while (cur_char != '\"') 
    {
      if (check_char(cur_char, ',')) 
      {
        std::string errmsg = std::string("An \" was expected after") + itemStr + "however, a delimeter `,` was detected. ";
        throw StyioSyntaxError(errmsg);
      }

      itemStr += cur_char;

      context -> move(1);
    };

    // eliminate double quote symbol " at the end of dependency item
    context -> move(1);

    return itemStr;
  }
  else
  {
    std::string errmsg = std::string("Dependencies should be wrapped with double quote like \"abc/xyz\", rather than starting with the character `") + char(cur_char) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

std::unique_ptr<ExtPackAST> parse_ext_pack (
  std::shared_ptr<StyioContext> context) { 
  // eliminate left square (box) bracket [
  context -> move(1);

  std::vector<std::string> dependencies;

  context -> drop_spaces();

  // add the first dependency path to the list
  dependencies.push_back(std::move(parse_ext_elem(context)));

  std::string pathStr = "";
  
  while (check_char(cur_char, ',')) {
    // eliminate comma ","
    context -> move(1);

    // reset pathStr to empty ""
    pathStr = ""; 

    context -> drop_spaces();
    
    // add the next dependency path to the list
    dependencies.push_back(std::move(parse_ext_elem(context)));
  };

  if (check_char(cur_char, ']')) {
    // eliminate right square bracket `]` after dependency list
    context -> move(1);
  };

  std::unique_ptr<ExtPackAST> output = std::make_unique<ExtPackAST>(dependencies);

  return output;
}

std::unique_ptr<StyioAST> parse_cases (
  std::shared_ptr<StyioContext> context) {
  std::vector<std::tuple<std::unique_ptr<StyioAST>, std::unique_ptr<StyioAST>>> pairs;
  std::unique_ptr<StyioAST> _default_stmt;

  /*
    Danger!
    when entering parse_cases(), 
    the cur_char must be {
    this line will drop the next 1 character anyway!
  */
  context -> move(1);

  while (true)
  {
    context -> arrive();
    if (context -> check_and_drop('_')) {

      find_and_drop_symbol(context, cur_char, "=>");
      
      context -> arrive();
      
      if (check_char(cur_char, '{')) {
        _default_stmt = parse_block(context); }
      else {
        _default_stmt = parse_stmt(context); }

      break;
    }

    std::unique_ptr<StyioAST> left = parse_expr(context);
    
    find_and_drop_symbol(context, cur_char, "=>");

    context -> arrive();

    std::unique_ptr<StyioAST> right;
    if (check_char(cur_char, '{')) {
      right = parse_block(context); }
    else {
      right = parse_stmt(context); }

    pairs.push_back(std::make_tuple(std::move(left), std::move(right)));
  }

  find_and_drop_char_panic(context, cur_char, '}');

  if (pairs.size() == 0) {
    return std::make_unique<CasesAST>(
      std::move(_default_stmt)); }
  else {
    return std::make_unique<CasesAST>(
      std::move(pairs), 
      std::move(_default_stmt)); }
}

std::unique_ptr<StyioAST> parse_block (
  std::shared_ptr<StyioContext> context
) {
  std::vector<std::unique_ptr<StyioAST>> stmtBuffer;

  /*
    Danger!
    when entering parse_block(), 
    the cur_char must be {
    this line will drop the next 1 character anyway!
  */
  context -> move(1);

  while (true)
  {
    context -> arrive();
    
    if (context -> check_and_drop('}')) {
      break; }
    else {
      stmtBuffer.push_back(std::move(parse_stmt(context))); };
  };

  if (stmtBuffer.size() == 0) {
    return std::make_unique<EmptyBlockAST>(); }
  else {
    return std::make_unique<SideBlockAST>(std::move(stmtBuffer));};
}

std::unique_ptr<MainBlockAST> parse_main_block (
  std::string styio_code 
) {
  auto styio_code_context = std::make_shared<StyioContext>(styio_code);
  // struct StyioCodeContext styio_code_context = { styio_context, 0 };
  // StyioCodeContext* ctx_ptr = &styio_code_context;
  // char cur_char = styio_code.at(0);

  std::vector<std::unique_ptr<StyioAST>> stmtBuffer;

  while (true) {
    std::unique_ptr<StyioAST> stmt = parse_stmt(styio_code_context);

    if ((stmt -> hint()) == StyioNodeHint::End) { 
      break; }
    else if ((stmt -> hint()) == StyioNodeHint::Comment) {
      continue; }
    else {
      std::cout << "\033[1;33m[>_<]\033[0m " << stmt -> toString() << "\n" << std::endl;
      stmtBuffer.push_back(std::move(stmt)); }
  }

  return std::make_unique<MainBlockAST>(std::move(stmtBuffer));
}