// [C++ STL]
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <tuple>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "../StyioUtil/Util.hpp"
#include "Parser.hpp"

using std::string;
using std::vector;

void
here() {
  std::cout << "here" << std::endl;
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

std::string
parse_token_as_str(StyioContext& context) {
  string name = "";

  while (context.check_isalnum_()) {
    name += context.get_curr_char();
    context.move(1);
  }

  return name;
}

NameAST*
parse_name(StyioContext& context) {
  string name = "";
  /* it will include cur_char in the id without checking */
  do {
    name += context.get_curr_char();
    context.move(1);
  } while (context.check_isalnum_());

  return NameAST::Create(name);
}

IntAST*
parse_int(StyioContext& context) {
  string digits = "";

  /* it will include cur_char in the digits without checking */
  do {
    digits += context.get_curr_char();
    context.move(1);
  } while (context.check_isdigit());

  return IntAST::Create(digits);
}

StyioAST*
parse_int_or_float(StyioContext& context) {
  string digits = "";
  /* it will include cur_char in the digits without checking */
  do {
    digits += context.get_curr_char();
    context.move(1);
  } while (context.check_isdigit());

  // int f_exp = 0; /* Float Exponent (Base: 10) */
  if (context.check('.')) {
    if (context.peak_isdigit(1)) {
      digits += ".";
      context.move(1); /* cur_char moves from . to the next */
      do {
        digits += context.get_curr_char();
        context.move(1);
        // f_exp += 1;
      } while (context.check_isdigit());

      return FloatAST::Create(digits);
    }
    else {
      return IntAST::Create(digits);
    }
  }

  return IntAST::Create(digits);
}

StringAST*
parse_string(StyioContext& context) {
  /*
    Danger!
    when entering parse_string(),
    the context -> get_curr_char() must be "
    this line will drop the next 1 character anyway!
  */
  context.move(1);

  string textStr = "";

  while (not context.check('\"')) {
    textStr += context.get_curr_char();
    context.move(1);
  }

  // eliminate " at the end
  context.move(1);

  return StringAST::Create(textStr);
}

StyioAST*
parse_char_or_string(StyioContext& context) {
  /*
    Danger!
    when entering parse_char_or_string(),
    the context -> get_curr_char() must be '
    this line will drop the next 1 character anyway!
  */
  context.move(1);
  string text = "";

  while (not context.check('\'')) {
    text += context.get_curr_char();
    context.move(1);
  }

  // eliminate ' at the end
  context.move(1);

  if (text.size() == 1) {
    return CharAST::Create(text);
  }
  else {
    return StringAST::Create(text);
  }
}

FmtStrAST*
parse_fmt_str(StyioContext& context) {
  /*
    Danger!
    when entering parse_fmt_str(),
    the context -> get_curr_char() must be "
    this line will drop the next 1 character anyway!
  */
  context.move(1);

  vector<string> fragments;
  vector<StyioAST*> exprs;
  string textStr = "";

  while (not context.check('\"')) {
    if (context.check('{')) {
      if (context.check_ahead(1, '{')) {
        textStr += context.get_curr_char();
        context.move(2);
      }
      else {
        context.move(1);

        exprs.push_back(parse_expr(context));

        context.find_drop('}');

        fragments.push_back(textStr);
        textStr.clear();
      }
    }
    else if (context.check('}')) {
      if (context.check_ahead(1, '}')) {
        textStr += context.get_curr_char();
        context.move(2);
      }
      else {
        string errmsg = string("Expecting: ") + "}" + "\n" + "But Got: " + context.get_curr_char();
        throw StyioSyntaxError(errmsg);
      }
    }
    else {
      textStr += context.get_curr_char();
      context.move(1);
    }
  }
  // this line drops " at the end anyway!
  context.move(1);

  fragments.push_back(textStr);

  return FmtStrAST::Create(fragments, exprs);
}

StyioAST*
parse_path(StyioContext& context) {
  context.move(1);

  string text = "";

  while (not context.check('"')) {
    text += context.get_curr_char();
    context.move(1);
  }

  // drop " at the end
  context.move(1);

  if (text.starts_with("/")) {
    return LocalPathAST::Create(StyioPathType::local_absolute_unix_like, text);
  }
  else if (std::isupper(text.at(0)) && text.at(1) == ':') {
    return LocalPathAST::Create(StyioPathType::local_absolute_windows, text);
  }
  else if (text.starts_with("http://")) {
    return WebUrlAST::Create(StyioPathType::url_http, text);
  }
  else if (text.starts_with("https://")) {
    return WebUrlAST::Create(StyioPathType::url_https, text);
  }
  else if (text.starts_with("ftp://")) {
    return WebUrlAST::Create(StyioPathType::url_ftp, text);
  }
  else if (text.starts_with("mysql://")) {
    return DBUrlAST::Create(StyioPathType::db_mysql, text);
  }
  else if (text.starts_with("postgres://")) {
    return DBUrlAST::Create(StyioPathType::db_postgresql, text);
  }
  else if (text.starts_with("mongo://")) {
    return DBUrlAST::Create(StyioPathType::db_mongo, text);
  }
  else if (text.starts_with("localhost") || text.starts_with("127.0.0.1")) {
    return RemotePathAST::Create(StyioPathType::url_localhost, text);
  }
  else if (is_ipv4_at_start(text)) {
    return RemotePathAST::Create(StyioPathType::ipv4_addr, text);
  }
  else if (is_ipv6_at_start(text)) {
    return RemotePathAST::Create(StyioPathType::ipv6_addr, text);
  }
  else if (text.starts_with("\\\\")) {
    return RemotePathAST::Create(StyioPathType::remote_windows, text);
  }

  return LocalPathAST::Create(StyioPathType::local_relevant_any, text);
}

DTypeAST*
parse_dtype(StyioContext& context) {
  string text = "";

  if (context.check_isal_()) {
    text += context.get_curr_char();
    context.move(1);
  }

  while (context.check_isalnum_()) {
    text += context.get_curr_char();
    context.move(1);
  }

  return DTypeAST::Create(text);
}

/*
  Basic Collection
  - typed_var
  - Fill (Variable Tuple)
  - Resources
*/

ArgAST*
parse_argument(StyioContext& context) {
  string namestr = "";
  /* it includes cur_char in the name without checking */
  do {
    namestr += context.get_curr_char();
    context.move(1);
  } while (context.check_isalnum_());

  NameAST* name = NameAST::Create(namestr);
  DTypeAST* data_type;
  StyioAST* default_value;

  context.drop_white_spaces();

  if (context.check_drop(':')) {
    context.drop_white_spaces();

    data_type = parse_dtype(context);

    context.drop_white_spaces();

    if (context.check_drop('=')) {
      context.drop_white_spaces();

      default_value = parse_expr(context);

      return ArgAST::Create(name, data_type, default_value);
    }
    else {
      return ArgAST::Create(name, data_type);
    }
  }
  else {
    return ArgAST::Create(name);
  }
}

VarTupleAST*
parse_var_tuple(StyioContext& context) {
  vector<VarAST*> vars;

  /* cur_char must be `(` which will be removed without checking */
  context.move(1);

  do {
    context.drop_all_spaces_comments();

    if (context.check_drop(')')) {
      return new VarTupleAST(vars);
    }
    else {
      if (context.check_drop('*')) {
        // if (context.check_drop('*')) {
        //   vars.push_back(OptKwArgAST::Create(parse_id(context)));
        // }
        // else {
        //   vars.push_back(OptArgAST::Create(parse_id(context)));
        // }
      }
      else {
        vars.push_back(parse_argument(context));
      }
    }
  } while (context.check_drop(','));

  context.find_drop_panic(')');

  return VarTupleAST::Create(vars);
}

ResourceAST*
parse_resources(
  StyioContext& context
) {
  ResourceAST* output;

  vector<StyioAST*> resources;

  /*
    Danger!
    when entering parse_resources(),
    the context -> get_curr_char() must be @
    this line will drop the next 1 character anyway!
  */
  context.move(1);

  if (context.check_drop('(')) {
    do {
      context.drop_all_spaces_comments();

      if (context.check('"')) {
        resources.push_back(parse_path(context));
      }
      else if (context.check_isal_()) {
        NameAST* varname = parse_name(context);

        context.find_drop_panic("<-");

        context.drop_all_spaces_comments();

        resources.push_back(
          new FinalBindAST(
            VarAST::Create(varname),
            parse_value(context)
          )
        );
      }

    } while (context.check_drop(','));

    context.find_drop_panic(')');

    output = new ResourceAST((resources));
  }
  else {
    string errmsg = string("@(expr) // Expecting ( after @, but got ") + char(context.get_curr_char()) + "";
    throw StyioSyntaxError(errmsg);
  }

  return output;
}

/*
  Expression
  - Value
  - Binary Comparison
*/

StyioAST*
parse_cond_item(StyioContext& context) {
  StyioAST* output;

  context.drop_all_spaces();

  output = parse_value(context);

  context.drop_all_spaces();

  switch (context.get_curr_char()) {
    case '=': {
      context.move(1);

      if (context.check('=')) {
        context.move(1);

        /*
          Equal
            expr == expr
        */

        // drop all spaces after ==
        context.drop_all_spaces();

        output = new BinCompAST(
          CompType::EQ, (output), parse_value(context)
        );
      };
    }

    break;

    case '!': {
      context.move(1);

      if (context.check('=')) {
        context.move(1);

        /*
          Not Equal
            expr != expr
        */

        // drop all spaces after !=
        context.drop_all_spaces();

        output = new BinCompAST(
          CompType::NE, (output), parse_value(context)
        );
      };
    }

    break;

    case '>': {
      context.move(1);

      if (context.check('=')) {
        context.move(1);

        /*
          Greater Than and Equal
            expr >= expr
        */

        // drop all spaces after >=
        context.drop_all_spaces();

        output = new BinCompAST(
          CompType::GE, (output), parse_value(context)
        );
      }
      else {
        /*
          Greater Than
            expr > expr
        */

        // drop all spaces after >
        context.drop_all_spaces();

        output = new BinCompAST(
          CompType::GT, (output), parse_value(context)
        );
      };
    }

    break;

    case '<': {
      context.move(1);

      if (context.check('=')) {
        context.move(1);

        /*
          Less Than and Equal
            expr <= expr
        */

        // drop all spaces after <=
        context.drop_all_spaces();

        output = new BinCompAST(
          CompType::LE, (output), parse_value(context)
        );
      }
      else {
        /*
          Less Than
            expr < expr
        */

        // drop all spaces after <
        context.drop_all_spaces();

        output = new BinCompAST(
          CompType::LT, (output), parse_value(context)
        );
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
StyioAST*
parse_id_or_value(StyioContext& context) {
  StyioAST* output;

  if (context.check_isal_()) {
    auto varname = parse_name(context);

    if (context.check('[')) {
      output = parse_list_op(context, varname);
    }
    else if (context.check('(')) {
      output = parse_call(context, varname);
    }
    else {
      output = varname;
    }
  }

  context.drop_all_spaces_comments();

  switch (context.get_curr_char()) {
    case '+': {
      context.move(1);
      output = parse_binop_rhs(context, output, TokenKind::Binary_Add);
    } break;

    case '-': {
      context.move(1);
      output = parse_binop_rhs(context, output, TokenKind::Binary_Sub);
    } break;

    case '*': {
      context.move(1);
      if (context.check_drop('*')) {
        output = parse_binop_rhs(context, output, TokenKind::Binary_Pow);
      }
      else {
        output = parse_binop_rhs(context, output, TokenKind::Binary_Mul);
      }
    } break;

    case '/': {
      context.move(1);
      output = parse_binop_rhs(context, output, TokenKind::Binary_Div);
    } break;

    case '%': {
      context.move(1);
      output = parse_binop_rhs(context, output, TokenKind::Binary_Mod);
    } break;

    case '.': {
      context.move(1);
      output = parse_chain_of_call(context, output);
    } break;

    default:
      break;
  }

  return output;
}

StyioAST*
parse_value(StyioContext& context) {
  StyioAST* output;

  if (isalpha(context.get_curr_char()) || context.check('_')) {
    return parse_id_or_value(context);
  }
  else if (isdigit(context.get_curr_char())) {
    return parse_int_or_float(context);
  }
  else if (context.check('|')) {
    return parse_size_of(context);
  }

  string errmsg = string("parse_value() // Unexpected value expression, starting with ") + char(context.get_curr_char());
  throw StyioParseError(errmsg);
}

StyioAST*
parse_binop_item(StyioContext& context) {
  StyioAST* output = NoneAST::Create();

  if (context.check_isal_()) {
    return parse_name(context);
  }
  else if (context.check_isdigit()) {
    return parse_int_or_float(context);
  }

  switch (context.get_curr_char()) {
    case '\"': {
      return parse_string(context);
    } break;  // You should NOT reach this line!

    case '\'': {
      return parse_char_or_string(context);
    } break;  // You should NOT reach this line!

    case '[': {
      context.move(1);
      context.drop_all_spaces_comments();

      if (context.check_drop(']')) {
        return ListAST::Create();
      }
      else {
        return parse_list_or_loop(context);
      }
    } break;  // You should NOT reach this line!

    default:
      break;
  }

  return output;
}

StyioAST*
parse_expr(StyioContext& context) {
  StyioAST* output;

  if (context.check_isal_()) {
    output = parse_name(context);

    context.drop_all_spaces_comments();

    switch (context.get_curr_char()) {
      case '+': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Add);
      } break;

      case '-': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Sub);
      } break;

      case '*': {
        context.move(1);
        if (context.check_drop('*')) {
          output = parse_binop_rhs(context, output, TokenKind::Binary_Pow);
        }
        else {
          output = parse_binop_rhs(context, output, TokenKind::Binary_Mul);
        }
      } break;

      case '/': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Div);
      } break;

      case '%': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Mod);
      } break;

      case '[': {
        output = parse_list_op(context, output);
      } break;

      default:
        break;
    }

    return output;
  }
  else if (context.check_isdigit()) {
    output = parse_int_or_float(context);

    context.drop_all_spaces_comments();

    switch (context.get_curr_char()) {
      case '+': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Add);
      } break;

      case '-': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Sub);
      } break;

      case '*': {
        context.move(1);
        if (context.check_drop('*')) {
          output = parse_binop_rhs(context, output, TokenKind::Binary_Pow);
        }
        else {
          output = parse_binop_rhs(context, output, TokenKind::Binary_Mul);
        }
      } break;

      case '/': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Div);
      } break;

      case '%': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Mod);
      } break;

      default:
        break;
    }

    return output;
  }

  switch (context.get_curr_char()) {
    case '\'': {
      return parse_char_or_string(context);
    }

    case '\"': {
      return parse_string(context);
    }

    case '[': {
      context.move(1);

      context.drop_all_spaces_comments();

      if (context.check_drop(']')) {
        output = ListAST::Create();
      }
      else {
        output = parse_list_or_loop(context);
      }

      return output;
    }

    break;

    case '(': {
      return parse_tuple(context);
    }

    break;

    case '{': {
      return parse_set(context);
    }

    break;

    case '|': {
      output = parse_size_of(context);

      context.drop_all_spaces_comments();

      switch (context.get_curr_char()) {
        case '+': {
          context.move(1);
          output = parse_binop_rhs(context, output, TokenKind::Binary_Add);
        } break;

        case '-': {
          context.move(1);
          output = parse_binop_rhs(context, output, TokenKind::Binary_Sub);
        } break;

        case '*': {
          context.move(1);
          if (context.check_drop('*')) {
            output = parse_binop_rhs(context, output, TokenKind::Binary_Pow);
          }
          else {
            output = parse_binop_rhs(context, output, TokenKind::Binary_Mul);
          }
        } break;

        case '/': {
          context.move(1);
          output = parse_binop_rhs(context, output, TokenKind::Binary_Div);
        } break;

        case '%': {
          context.move(1);
          output = parse_binop_rhs(context, output, TokenKind::Binary_Mod);
        } break;

        default:
          break;
      }

      return output;
    }

    break;

    case '\\': {
      context.move(1);

      if (context.check_drop('t')) {
        context.check_drop('\\');
        return new BoolAST(true);
      }
      else if (context.check_drop('f')) {
        context.check_drop('\\');
        return new BoolAST(false);
      }
    }

    break;

    case '$': {
      context.move(1);

      return parse_fmt_str(context);
    }

    default: {
      output = new NoneAST();
    } break;
  }

  return output;
}

StyioAST*
parse_tuple(StyioContext& context) {
  vector<StyioAST*> exprs;

  /*
    Danger!
    when entering parse_tuple(),
    the context -> get_curr_char() must be (
    this line will drop the next 1 character anyway!
  */
  context.move(1);

  do {
    context.drop_all_spaces_comments();

    if (context.check_drop(')')) {
      return TupleAST::Create(exprs);
    }
    else {
      exprs.push_back(parse_expr(context));
      context.drop_white_spaces();
    }
  } while (context.check_drop(','));

  context.find_drop_panic(')');

  return TupleAST::Create(exprs);
}

/*
  While entering parse_tuple_no_braces(),
  curr_char should be the next element (or at least part of the next element),
  but not a comma (,).
*/
StyioAST*
parse_tuple_no_braces(StyioContext& context, StyioAST* first_element) {
  vector<StyioAST*> exprs;

  if (first_element) {
    exprs.push_back(first_element);
  }

  do {
    context.drop_all_spaces_comments();

    exprs.push_back(parse_expr(context));
  } while (context.check_drop(','));

  TupleAST* the_tuple = TupleAST::Create(exprs);

  // no check for right brace ')'

  if (context.check_tuple_ops()) {
    return parse_tuple_operations(context, the_tuple);
  }

  return the_tuple;
}

StyioAST*
parse_list(StyioContext& context) {
  vector<StyioAST*> exprs;

  /*
    Danger!
    when entering parse_list(),
    the context -> get_curr_char() must be [
    this line will drop the next 1 character anyway!
  */
  context.move(1);

  do {
    context.drop_all_spaces_comments();

    if (context.check_drop(']')) {
      return new ListAST((exprs));
    }
    else {
      exprs.push_back(parse_expr(context));
      context.drop_white_spaces();
    }
  } while (context.check_drop(','));

  context.check_drop(']');

  return ListAST::Create(exprs);
}

StyioAST*
parse_set(StyioContext& context) {
  vector<StyioAST*> exprs;

  /*
    Danger!
    when entering parse_set(),
    the current character must be {
    this line will drop the next 1 character anyway!
  */
  context.move(1);

  do {
    context.drop_all_spaces_comments();

    if (context.check_drop('}')) {
      return SetAST::Create(exprs);
    }
    else {
      exprs.push_back(parse_expr(context));
      context.drop_white_spaces();
    }
  } while (context.check_drop(','));

  context.find_drop_panic('}');

  return SetAST::Create(exprs);
}

StyioAST*
parse_struct(StyioContext& context, NameAST* name) {
  vector<ArgAST*> elems;

  do {
    context.drop_all_spaces_comments();

    if (context.check_drop('}')) {
      return StructAST::Create(name, elems);
    }
    else {
      elems.push_back(parse_argument(context));
    }
  } while (context.check_drop(',') or context.check_drop(';'));

  context.find_drop_panic('}');

  return StructAST::Create(name, elems);
}

StyioAST*
parse_iterable(StyioContext& context) {
  StyioAST* output = EmptyAST::Create();

  if (isalpha(context.get_curr_char()) || context.check('_')) {
    output = parse_name(context);

    context.drop_all_spaces_comments();

    switch (context.get_curr_char()) {
      case '+': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Add);
      } break;

      case '-': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Sub);
      } break;

      case '*': {
        context.move(1);
        if (context.check_drop('*')) {
          output = parse_binop_rhs(context, output, TokenKind::Binary_Pow);
        }
        else {
          output = parse_binop_rhs(context, output, TokenKind::Binary_Mul);
        }
      } break;

      case '/': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Div);
      } break;

      case '%': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Mod);
      } break;

      default:
        break;
    }

    return output;
  }
  else {
    switch (context.get_curr_char()) {
      case '(': {
        context.move(1);

        vector<StyioAST*> exprs;
        do {
          context.drop_all_spaces_comments();

          if (context.check_drop(')')) {
            return new TupleAST((exprs));
          }
          else {
            exprs.push_back(parse_expr(context));
            context.drop_white_spaces();
          }
        } while (context.check_drop(','));

        context.check_drop(')');

        return TupleAST::Create(exprs);
      }

      case '[': {
        context.move(1);

        vector<StyioAST*> exprs;
        do {
          context.drop_all_spaces_comments();

          if (context.check_drop(']')) {
            return new ListAST((exprs));
          }
          else {
            exprs.push_back(parse_expr(context));
            context.drop_white_spaces();
          }
        } while (context.check_drop(','));

        context.check_drop(']');

        return ListAST::Create(exprs);
      }

      case '{': {
        context.move(1);

        vector<StyioAST*> exprs;
        do {
          context.drop_all_spaces_comments();

          if (context.check_drop('}')) {
            return new SetAST((exprs));
          }
          else {
            exprs.push_back(parse_expr(context));
            context.drop_white_spaces();
          }
        } while (context.check_drop(','));

        context.check_drop('}');

        return SetAST::Create(exprs);
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

SizeOfAST*
parse_size_of(StyioContext& context) {
  SizeOfAST* output;

  // eliminate | at the start
  context.move(1);

  if (isalpha(context.get_curr_char()) || context.check('_')) {
    StyioAST* var = parse_id_or_value(context);

    // eliminate | at the end
    if (context.check('|')) {
      context.move(1);

      output = new SizeOfAST((var));
    }
    else {
      string errmsg = string("|expr| // SizeOf: Expecting | at the end, but got .:| ") + char(context.get_curr_char()) + " |:.";
      throw StyioSyntaxError(errmsg);
    }
  }
  else {
    string errmsg = string("|expr| // SizeOf: Unexpected expression, starting with .:| ") + char(context.get_curr_char()) + " |:.";
    throw StyioParseError(errmsg);
  }

  return output;
}

/*
  Invoke / Call
*/

CallAST*
parse_call(
  StyioContext& context,
  NameAST* func_name
) {
  context.check_drop_panic('(');

  vector<StyioAST*> exprs;

  while (not context.check(')')) {
    exprs.push_back(parse_expr(context));
    context.find_drop(',');
    context.drop_all_spaces_comments();
  }

  context.check_drop_panic(')');

  return new CallAST(
    func_name,
    exprs
  );
}

AttrAST*
parse_attr(
  StyioContext& context
) {
  auto main_name = NameAST::Create(parse_token_as_str(context));

  StyioAST* attr_name;
  if (context.find_drop('.')) {
    attr_name = NameAST::Create(parse_token_as_str(context));
  }
  else if (context.find_drop('[')) {
    /* Object["name"] */
    if (context.check('"')) {
      attr_name = parse_string(context);
    }
    /*
      Object[any_expr]
    */
    else {
      attr_name = parse_expr(context);
    }
  }

  return AttrAST::Create(main_name, attr_name);
}

/*
  parse_chain_of_call takes an alphabeta name as the start, not a dot (e.g. '.').

  person.name
         ^ where parse_chain_of_call() starts
*/
StyioAST*
parse_chain_of_call(
  StyioContext& context,
  StyioAST* callee
) {
  while (true) {
    std::string curr_token = parse_token_as_str(context);
    context.drop_all_spaces_comments();

    if (context.check_drop('.')) {
      AttrAST* temp = AttrAST::Create(callee, NameAST::Create(curr_token));
      return parse_chain_of_call(context, temp);
    }
    else if (context.check('(')) {
      CallAST* temp = parse_call(context, NameAST::Create(curr_token));

      if (context.check_drop('.')) {
        return parse_chain_of_call(context, temp);
      }
      else {
        temp->func_callee = callee;
        return temp;
      }
    }

    return AttrAST::Create(callee, NameAST::Create(curr_token));
  }
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

StyioAST*
parse_list_op(StyioContext& context, StyioAST* theList) {
  StyioAST* output;

  /*
    the current character must be [
    this line will drop the next 1 character anyway!
  */
  context.move(1);

  do {
    if (isalpha(context.get_curr_char()) || context.check('_')) {
      output = new ListOpAST(
        StyioASTType::Access, (theList), parse_id_or_value(context)
      );
    }
    else if (isdigit(context.get_curr_char())) {
      std::cout << "parse list op access by index" << std::endl;
      output = new ListOpAST(
        StyioASTType::Access_By_Index, (theList), parse_int(context)
      );
    }
    else {
      switch (context.get_curr_char()) {
        /*
          list["any"]
        */
        case '"': {
          output = new ListOpAST(StyioASTType::Access_By_Name, (theList), parse_string(context));
        }

        // You should NOT reach this line!
        break;

        /*
          list[<]
        */
        case '<': {
          context.move(1);

          while (context.check('<')) {
            context.move(1);
          }

          output = new ListOpAST(StyioASTType::Get_Reversed, (theList));
        }

        // You should NOT reach this line!
        break;

        // list[?= item]
        case '?': {
          context.move(1);

          if (context.check_drop('=')) {
            context.drop_all_spaces_comments();

            output = new ListOpAST(StyioASTType::Get_Index_By_Value, (theList), parse_expr(context));
          }
          else if (context.check_drop('^')) {
            context.drop_all_spaces_comments();

            output = new ListOpAST(StyioASTType::Get_Indices_By_Many_Values, (theList), parse_iterable(context));
          }
          else {
            string errmsg = string("Expecting ?= or ?^, but got ") + char(context.get_curr_char());
            throw StyioSyntaxError(errmsg);
          }
        }

        // You should NOT reach this line!
        break;

        /*
          list[^index]
          list[^index <- value]
        */
        case '^': {
          context.move(1);

          context.drop_white_spaces();

          StyioAST* index = parse_int(context);

          context.drop_white_spaces();

          /*
            list[^index <- value]
          */
          if (context.check_drop("<-")) {
            context.drop_white_spaces();

            output = new ListOpAST(StyioASTType::Insert_Item_By_Index, (theList), (index), parse_expr(context));
          }
          // list[^index]
          else {
            output = new ListOpAST(StyioASTType::Access_By_Index, (theList), (index));
          }
        }
        // You should NOT reach this line!
        break;

        /*
          list[+: value]
        */
        case '+': {
          context.move(1);

          context.check_drop_panic(':');

          context.drop_white_spaces();

          StyioAST* expr = parse_expr(context);

          context.drop_white_spaces();

          output = new ListOpAST(
            StyioASTType::Append_Value, (theList), (expr)
          );
        }

        // You should NOT reach this line!
        break;

        case '-': {
          context.move(1);

          context.check_drop_panic(':');

          context.drop_white_spaces();

          /*
            list[-: ^index]
          */
          if (context.check_drop('^')) {
            context.drop_white_spaces();

            if (isdigit(context.get_curr_char())) {
              output = new ListOpAST(StyioASTType::Remove_Item_By_Index, (theList), (parse_int(context)));
            }
            else {
              /*
                list[-: ^(i0, i1, ...)]
              */
              output = new ListOpAST(
                StyioASTType::Remove_Items_By_Many_Indices,
                (theList),
                (parse_iterable(context))
              );
            }
          }
          else if (context.check_drop('?')) {
            switch (context.get_curr_char()) {
              /*
                list[-: ?= value]
              */
              case '=': {
                context.move(1);

                context.drop_white_spaces();

                output = new ListOpAST(StyioASTType::Remove_Item_By_Value, (theList), parse_expr(context));
              }

              break;

              /*
                list[-: ?^ (v0, v1, ...)]
              */
              case '^': {
                context.move(1);

                context.drop_white_spaces();

                output = new ListOpAST(
                  StyioASTType::Remove_Items_By_Many_Values,
                  (theList),
                  parse_iterable(context)
                );
              }

              break;

              default:
                break;
            }
          }
          else {
            output = new ListOpAST(StyioASTType::Remove_Item_By_Value, (theList), parse_expr(context));
          }
        }

        // You should NOT reach this line!
        break;

        case ']': {
          output = (theList);
        }

        // You should NOT reach this line!
        break;

        default: {
          string errmsg = string("Unexpected List[Operation], starts with ") + char(context.get_curr_char());
          throw StyioSyntaxError(errmsg);
        }

        // You should NOT reach this line!
        break;
      }
    }
  } while (context.check_drop('['));

  context.find_drop(']');

  return output;
}

StyioAST*
parse_loop_or_iter(StyioContext& context, StyioAST* iterOverIt) {
  context.drop_all_spaces_comments();

  if ((iterOverIt->getNodeType()) == StyioASTType::Infinite) {
    return new InfiniteLoopAST(parse_forward(context, false));
  }
  else {
    return new IteratorAST(iterOverIt, parse_forward(context, false));
  }
}

StyioAST*
parse_list_or_loop(StyioContext& context) {
  StyioAST* output;

  vector<StyioAST*> elements;

  StyioAST* startEl = parse_expr(context);

  context.drop_white_spaces();

  if (context.check_drop('.')) {
    while (context.check('.')) {
      context.move(1);
    }

    context.drop_white_spaces();

    StyioAST* endEl = parse_expr(context);

    context.drop_white_spaces();

    context.check_drop_panic(']');

    if (startEl->getNodeType() == StyioASTType::Integer && endEl->getNodeType() == StyioASTType::Id) {
      output = new InfiniteAST((startEl), (endEl));
    }
    else if (startEl->getNodeType() == StyioASTType::Integer && endEl->getNodeType() == StyioASTType::Integer) {
      output = new RangeAST(
        (startEl), (endEl), IntAST::Create("1")
      );
    }
    else {
      string errmsg = string("Unexpected Range / List / Loop: ") + "starts with: " + std::to_string(type_to_int(startEl->getNodeType())) + ", " + "ends with: " + std::to_string(type_to_int(endEl->getNodeType())) + ".";
      throw StyioSyntaxError(errmsg);
    }
  }
  else if (context.check_drop(',')) {
    elements.push_back((startEl));

    do {
      context.drop_all_spaces_comments();

      if (context.check_drop(']')) {
        return new ListAST(elements);
      }
      else {
        elements.push_back(parse_expr(context));
      }
    } while (context.check_drop(','));

    context.find_drop_panic(']');

    output = new ListAST((elements));
  }
  else {
    elements.push_back((startEl));

    context.find_drop_panic(']');

    output = new ListAST((elements));
  }

  while (context.check('[')) {
    output = parse_list_op(context, (output));
  }

  context.drop_all_spaces();

  if (context.check_drop(">>")) {
    output = parse_loop_or_iter(context, (output));
  }

  return output;
}

StyioAST*
parse_loop(StyioContext& context) {
  StyioAST* output;

  while (context.check('.')) {
    context.move(1);
  }

  context.find_drop_panic(']');

  context.drop_all_spaces();

  if (context.check_drop(">>")) {
    context.drop_all_spaces();

    return new InfiniteLoopAST(parse_forward(context, false));
  }

  return new InfiniteAST();
}

/*
  The LHS of BinOp should be recognized before entering parse_binop_with_lhs

  += -= *= /= should be recognized before entering parse_binop_with_lhs,
  and should be treated as a statement rather than a binary operation expression
  parse_binop_with_lhs only handle the following operators:

  Unary_Positive + a
  Unary_Negative - a
  Binary_Pow     a ** b
  Binary_Mul     a * b
  Binary_Div     a / b
  Binary_Mod     a % b
  Binary_Add     a + b
  Binary_Sub     a - b

  For boolean expressions, go to parse_bool_expr.

  hi, you need to pass the precedence as a parameter,
  the reason is that, the internal part should know where to stop,
  and that depends on the outer part.
  the outer part should know where to create binop,
  and this information comes from the internal part.
*/
BinOpAST*
parse_binop_rhs(
  StyioContext& context,
  StyioAST* lhs_ast,
  TokenKind curr_token
) {
  BinOpAST* output;

  context.drop_all_spaces_comments();

  StyioAST* rhs_ast = parse_binop_item(context);

  context.drop_all_spaces_comments();

  TokenKind next_token;
  switch (context.get_curr_char()) {
    case '+': {
      context.move(1);
      next_token = TokenKind::Binary_Add;
    } break;

    case '-': {
      context.move(1);
      next_token = TokenKind::Binary_Sub;
    } break;

    case '*': {
      context.move(1);
      if (context.check_drop('*')) {
        next_token = TokenKind::Binary_Pow;
      }
      else {
        next_token = TokenKind::Binary_Mul;
      }
    } break;

    case '/': {
      context.move(1);
      next_token = TokenKind::Binary_Div;
    } break;

    case '%': {
      context.move(1);
      next_token = TokenKind::Binary_Mod;
    } break;

    default: {
      return BinOpAST::Create(curr_token, lhs_ast, rhs_ast);
    } break;
  }

  if (next_token > curr_token) {
    output = BinOpAST::Create(curr_token, lhs_ast, parse_binop_rhs(context, rhs_ast, next_token));
  }
  else {
    output = parse_binop_rhs(context, BinOpAST::Create(curr_token, lhs_ast, rhs_ast), next_token);
  }

  return output;
}

CondAST*
parse_cond_rhs(StyioContext& context, StyioAST* lhsExpr) {
  CondAST* condExpr;

  context.drop_all_spaces();

  switch (context.get_curr_char()) {
    case '&': {
      context.move(1);

      context.check_drop('&');

      /*
        support:
          expr && \n
          expression
      */

      context.drop_all_spaces();

      condExpr = new CondAST(
        LogicType::AND, (lhsExpr), parse_cond(context)
      );
    }

    break;

    case '|': {
      context.move(1);

      if (context.check('|')) {
        context.move(1);
      };

      /*
        support:
          expr || \n
          expression
      */

      context.drop_all_spaces();

      condExpr = new CondAST(
        LogicType::OR, (lhsExpr), parse_cond(context)
      );
    }

    break;

    case '^': {
      context.move(1);

      /*
        support:
          expr ^ \n
          expression
      */

      context.drop_all_spaces();

      condExpr = new CondAST(
        LogicType::OR, (lhsExpr), parse_cond(context)
      );
    }

    break;

    case '!': {
      context.move(1);

      if (context.check('(')) {
        context.move(1);

        /*
          support:
            !( \n
              expr
            )
        */
        context.drop_all_spaces();

        condExpr = new CondAST(LogicType::NOT, parse_cond(context));

        context.find_drop_panic(')');
      }
    }

    break;

    default:
      break;
  }

  context.drop_all_spaces();

  while (!(context.check(')'))) {
    condExpr = (parse_cond_rhs(context, (condExpr)));
  }

  return condExpr;
}

CondAST*
parse_cond(StyioContext& context) {
  StyioAST* lhsExpr;

  if (context.check('(')) {
    context.move(1);

    lhsExpr = (parse_cond(context));

    context.find_drop_panic(')');
  }
  else if (context.check('!')) {
    context.move(1);

    if (context.check('(')) {
      context.move(1);

      /*
        support:
          !( \n
            expr
          )
      */
      context.drop_all_spaces();

      lhsExpr = (parse_cond(context));

      context.drop_all_spaces();

      return new CondAST(LogicType::NOT, (lhsExpr));
    }
    else {
      string errmsg = string("!(expr) // Expecting ( after !, but got ") + char(context.get_curr_char());
      throw StyioSyntaxError(errmsg);
    };
  }
  else {
    lhsExpr = (parse_cond_item(context));
  };

  // drop all spaces after first value
  context.drop_all_spaces();

  if (context.check('&') || context.check('|')) {
    return parse_cond_rhs(context, (lhsExpr));
  }
  else {
    return new CondAST(LogicType::RAW, (lhsExpr));
  }

  string errmsg = string("parse_cond() : You should not reach this line!") + char(context.get_curr_char());
  throw StyioParseError(errmsg);
}

StyioAST*
parse_cond_flow(StyioContext& context) {
  /*
    Danger!
    when entering parse_cond_flow(),
    the context -> get_curr_char() must be ?
    this line will drop the next 1 character anyway!
  */
  context.move(1);

  context.drop_white_spaces();

  if (context.check_drop('(')) {
    CondAST* condition = parse_cond(context);

    context.find_drop_panic(')');

    /*
      support:
        ?() \n
        \t\

        ?() \n
        \f\
    */
    context.drop_all_spaces_comments();

    if (context.check_drop('\\')) {
      StyioAST* block;

      if (context.check_drop('t')) {
        context.check_drop('\\');

        /*
          support:
            \t\ \n
            {}
        */

        context.drop_all_spaces_comments();

        block = parse_block(context);

        /*
          support:
            \t\ {} \n
            \f\
        */
        context.drop_all_spaces_comments();

        if (context.check_drop('\\')) {
          context.check_drop_panic('f');

          context.check_drop('\\');

          /*
            support:
              \f\ \n
              {}
          */
          context.drop_all_spaces_comments();

          StyioAST* blockElse = parse_block(context);

          return new CondFlowAST(StyioASTType::CondFlow_Both, (condition), (block), (blockElse));
        }
        else {
          return new CondFlowAST(StyioASTType::CondFlow_True, (condition), (block));
        }
      }
      else if (context.check_drop('f')) {
        context.check_drop('\\');

        /*
          support:
            \f\ \n
            {}
        */
        context.drop_all_spaces_comments();

        block = parse_block(context);

        return new CondFlowAST(StyioASTType::CondFlow_False, (condition), (block));
      }
      else {
        string errmsg = string("parse_cond_flow() // Unexpected character ") + context.get_curr_char();
        throw StyioSyntaxError(errmsg);
      }
    }
  }
  else {
    string errmsg = string("Missing ( for ?(`expr`).");
    throw StyioSyntaxError(errmsg);
  }

  throw StyioSyntaxError(context.label_cur_line(), string("Invalid Syntax"));
}

StyioAST*
parse_template(StyioContext& context) {
  context.move(1); /* this line drops cur_char without checking */
  context.drop_white_spaces();

  /* # func_name ... */
  if (context.check_isal_()) {
    auto func_name = parse_name(context);

    context.drop_all_spaces_comments();

    if (context.check_drop(':')) {
      /* f := ... */
      if (context.check_drop('=')) {
        context.drop_all_spaces();

        if (context.check_drop('{')) {
          return parse_struct(context, func_name);
        }
        else {
          return new FuncAST(
            func_name,
            parse_forward(context, true),
            true
          );
        }
      }
      /* f : ... */
      else {
        context.drop_all_spaces_comments();

        auto dtype = parse_dtype(context);

        context.drop_all_spaces_comments();

        /* f : type := ...*/
        if (context.check_drop(':')) {
          if (context.check_drop('=')) {
            context.drop_all_spaces_comments();

            return new FuncAST(func_name, dtype, parse_forward(context, true), true);
          }
          else {
            /* Error */
          }
        }
        /* f : type = ... */
        else if (context.check_drop('=')) {
          /* f : type => ... */
          if (context.check_drop('>')) {
            context.move(-2);
            context.drop_all_spaces_comments();

            return new FuncAST(func_name, dtype, parse_forward(context, true), false);
          }
          /* f : type = ... */
          else {
            context.drop_all_spaces_comments();

            return new FuncAST(func_name, dtype, parse_forward(context, true), false);
          }
        }
      }

      string errmsg = string("parse_pipeline() // Inheritance, Type Hint.");
      throw StyioNotImplemented(errmsg);
    }
    else if (context.check('=')) {
      /* f => ... */
      if (context.check("=>")) {
        return new FuncAST(
          func_name,
          parse_forward(context, true),
          /* isFinal */ false
        ); /* Should `f => {}` be flexible or final? */
      }
      /* f = ... */
      else {
        context.drop_all_spaces();

        return new FuncAST(
          func_name, parse_forward(context, true), false
        );
      }
    }
  }

  context.drop_all_spaces();
  return parse_forward(context, true);
}

/*
  Return:
    [?] AnonyFunc
    [?] MatchCases
*/
ForwardAST*
parse_forward(StyioContext& context, bool is_func) {
  ForwardAST* output;

  VarTupleAST* args;
  bool has_args = false;

  if (is_func) {
    if (context.check('(')) {
      args = parse_var_tuple(context);
      has_args = true;
    }
  }
  else if (context.check_drop('#')) {
    context.drop_white_spaces();
    if (context.check('(')) {
      args = parse_var_tuple(context);
      has_args = true;
    }
    else {
      string errmsg = string("parse_forward() // Expecting ( after #, but got ") + char(context.get_curr_char());
      throw StyioSyntaxError(errmsg);
    }
  }

  context.drop_all_spaces();

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
  switch (context.get_curr_char()) {
    case '?': {
      context.move(1);

      switch (context.get_curr_char()) {
        /*
          ?= value
        */
        case '=': {
          context.move(1);
          context.drop_white_spaces();

          auto cases = parse_cases(context);

          /* #(args) ?= cases */
          if (context.check('{')) {
            if (has_args) {
              output = new ForwardAST((cases));
            }
            else {
              output = new ForwardAST((cases));
            }

            return output;
          }
          /* #(args) ?= value => then */
          else {
            CheckEqAST* extra_check;
            StyioAST* then;

            extra_check = new CheckEqAST(parse_expr(context));

            context.drop_all_spaces();

            if (context.check_drop("=>")) {
              context.drop_all_spaces();

              if (context.check('{')) {
                then = parse_block(context);
              }
              else {
                then = parse_expr(context);
              }

              if (has_args) {
                output = new ForwardAST(
                  (args), (extra_check), (then)
                );
              }
              else {
                output = new ForwardAST((extra_check), (then));
              }
            }
            else {
              string errmsg = string(
                                "parse_forward() // Expecting `=>` after `?= "
                                "value`, but got "
                              )
                              + char(context.get_curr_char());
              throw StyioSyntaxError(errmsg);
            }
          }
        }

        break;
        /*
          ?^ [Iterable]
        */
        case '^': {
          context.move(1);

          StyioAST* nextExpr;

          StyioAST* iterable;

          context.drop_white_spaces();

          switch (context.get_curr_char()) {
            case '(': {
              context.move(1);

              vector<StyioAST*> exprs;
              do {
                context.drop_all_spaces_comments();

                if (context.check(')')) {
                  break;
                }
                else {
                  exprs.push_back(parse_expr(context));
                }
              } while (context.check_drop(','));

              context.find_drop(')');

              iterable = new TupleAST((exprs));
            } break;

            case '[': {
              context.move(1);

              vector<StyioAST*> exprs;
              do {
                context.drop_all_spaces_comments();

                if (context.check(']')) {
                  break;
                }
                else {
                  exprs.push_back(parse_expr(context));
                }
              } while (context.check_drop(','));

              context.find_drop(']');

              iterable = new ListAST((exprs));
            }

            break;

            case '{': {
              context.move(1);

              vector<StyioAST*> exprs;
              do {
                context.drop_all_spaces_comments();

                if (context.check('}')) {
                  break;
                }
                else {
                  exprs.push_back(parse_expr(context));
                }
              } while (context.check_drop(','));

              context.find_drop('}');

              iterable = new SetAST(exprs);
            } break;

            default: {
              if (isalpha(context.get_curr_char()) || context.check('_')) {
                iterable = parse_id_or_value(context);
              }
              else {
                string errmsg = string(
                                  "parse_forward() // Unexpected collection, "
                                  "starting with "
                                )
                                + char(context.get_curr_char());
                throw StyioSyntaxError(errmsg);
              }
            } break;
          }

          context.drop_all_spaces_comments();

          if (context.check_drop("=>")) {
            context.drop_all_spaces();

            if (context.check('{')) {
              nextExpr = parse_block(context);
            }
            else {
              nextExpr = parse_expr(context);
            }

            if (has_args) {
              output = new ForwardAST(
                (args),
                new CheckIsinAST((iterable)),
                (nextExpr)
              );
            }
            else {
              output = new ForwardAST(
                new CheckIsinAST((iterable)),
                (nextExpr)
              );
            }
          }
          else {
            string errmsg = string(
                              "parse_forward() // Expecting `=>` after `?^ "
                              "iterable`, but got "
                            )
                            + char(context.get_curr_char());
            throw StyioSyntaxError(errmsg);
          }
        }

        break;

        /*
          ?(Condition)
          \t\ { }

          ?(Condition)
          \f\ { }
        */
        default:
          context.move(-1);

          if (has_args) {
            output = new ForwardAST((args), parse_cond_flow(context));
          }
          else {
            output = new ForwardAST(parse_cond_flow(context));
          }

          break;
      }
    } break;

    /*
      support:

      => \n
      { }
    */
    case '=': {
      context.move(1);

      context.check_drop_panic('>');

      context.drop_all_spaces();

      if (context.check('{')) {
        if (has_args) {
          output = ForwardAST::Create(args, parse_block(context));
        }
        else {
          output = ForwardAST::Create(parse_block(context));
        }
      }
      else if (context.check_codp()) {
        output = ForwardAST::Create(parse_codp(context));
      }
      else {
        StyioAST* expr = parse_expr(context);

        if (has_args) {
          output = ForwardAST::Create(args, ReturnAST::Create(expr));
        }
        else {
          output = ForwardAST::Create(ReturnAST::Create(expr));
        }
      }
    } break;

    /*
      support:

      { }
    */
    case '{': {
      if (has_args) {
        output = new ForwardAST(args, parse_block(context));
      }
      else {
        output = new ForwardAST(parse_block(context));
      }
    } break;

    default:
      string errmsg = string("parse_forward() // Unexpected character ") + char(context.get_curr_char());
      throw StyioSyntaxError(errmsg);

      break;
  }

  return output;
}

ExtractorAST*
parse_tuple_operations(StyioContext& context, TupleAST* the_tuple) {
  ExtractorAST* result;
  
  if (context.check_drop("<<")) {
    // parse_extractor
  }
  else if (context.check_drop(">>")) {
    // parse_iterator
  }
  else if (context.check_drop("=>")) {
    // parse_forward
  }
  else {
    // Exception: Tuple Operation Not Found (Unacceptable Inside parse_tuple_operations Function)
  }


  return result;
}

/*
  parse_codp takes the name of operation as a start,
  but not a `=>` symbol.
*/
CODPAST*
parse_codp(StyioContext& context, CODPAST* prev_op) {
  CODPAST* curr_op;

  string name = parse_token_as_str(context);

  context.find_drop_panic('{');

  vector<StyioAST*> op_args;

  if (name == "filter") {
    context.drop_all_spaces_comments();
    op_args.push_back(parse_cond(context));
  }
  else if (name == "sort" or name == "map") {
    do {
      context.drop_all_spaces_comments();
      op_args.push_back(parse_attr(context));
    } while (context.find_drop(','));
  }
  else if (name == "slice") {
    do {
      context.drop_all_spaces_comments();
      op_args.push_back(parse_int(context));
    } while (context.find_drop(','));
  }

  context.find_drop_panic('}');

  curr_op = CODPAST::Create(name, op_args, prev_op);

  if (prev_op != nullptr) {
    prev_op->NextOp = curr_op;
  }

  if (context.find_drop("=>")) {
    context.drop_all_spaces_comments();
    parse_codp(context, curr_op);
  }

  return curr_op;
}

StyioAST*
parse_read_file(StyioContext& context, NameAST* id_ast) {
  if (context.check_drop('@')) {
    context.check_drop_panic('(');

    if (context.check('"')) {
      auto path = parse_path(context);

      context.find_drop_panic(')');

      return new ReadFileAST((id_ast), (path));
    }
    else {
      string errmsg = string("Expecting id or string, but got ` ") + char(context.get_curr_char()) + " `";
      throw StyioSyntaxError(errmsg);
    }
  }
  else {
    string errmsg = string("parse_read_file() // Expecting @ as first character but got ` ") + char(context.get_curr_char()) + " `";
    throw StyioSyntaxError(errmsg);
  }
}

StyioAST*
parse_print(StyioContext& context) {
  StyioAST* output;

  vector<StyioAST*> exprs;

  /* Expecting >_ , Move Forward Without Checking */
  context.move(2);

  /* Expecting ( ! Must Have ! */
  context.check_drop_panic('(');

  do {
    context.drop_all_spaces_comments();

    if (context.check_drop(')')) {
      return new PrintAST(exprs);
    }
    else {
      exprs.push_back(parse_expr(context));
    }
  } while (context.check_drop(','));

  context.find_drop_panic(')');
  return new PrintAST(exprs);
}

// StyioAST* parse_panic (
//   StyioContext& context) {
//   do
//   {
//     /*
//       Danger!
//       when entering parse_panic(),
//       the following symbol must be !
//       this line will drop the next 1 character anyway!
//     */
//     context -> move(1);
//   } while (context -> check('!'));

//   if (context -> find_drop('(')) {
//     /*
//       parse_one_or_many_repr
//       parse_fmt_str
//     */

//   } else {

//   }
// }

StyioAST*
parse_stmt(
  StyioContext& context
) {
  context.drop_all_spaces_comments();

  if (context.check_isal_()) {
    auto this_name = parse_name(context);

    switch (context.get_curr_char()) {
      case '[': {
        return parse_list_op(context, (this_name));
      } break;

      case '(': {
        return parse_call(context, (this_name));
      } break;

      default:
        break;
    }

    context.drop_all_spaces_comments();

    switch (context.get_curr_char()) {
      case '+': {
        context.move(1);
        return parse_binop_rhs(context, this_name, TokenKind::Binary_Add);
      } break;

      case '-': {
        context.move(1);
        return parse_binop_rhs(context, this_name, TokenKind::Binary_Sub);
      } break;

      case '*': {
        context.move(1);
        if (context.check_drop('*')) {
          return parse_binop_rhs(context, this_name, TokenKind::Binary_Pow);
        }
        else {
          return parse_binop_rhs(context, this_name, TokenKind::Binary_Mul);
        }
      } break;

      case '/': {
        context.move(1);
        return parse_binop_rhs(context, this_name, TokenKind::Binary_Div);
      } break;

      case '%': {
        context.move(1);
        return parse_binop_rhs(context, this_name, TokenKind::Binary_Mod);
      } break;

      case '=': {
        context.move(1);

        context.drop_all_spaces_comments();

        return FlexBindAST::Create(VarAST::Create(this_name), parse_expr(context));
      } break;  // You should NOT reach this line!

      case ':': {
        context.move(1);

        if (context.check_drop('=')) {
          context.drop_all_spaces_comments();

          return new FinalBindAST(VarAST::Create(this_name), parse_expr(context));
        }
        else {
          context.drop_white_spaces();

          auto var_data_type = parse_dtype(context);

          context.drop_white_spaces();

          if (context.check_drop(':')) {
            if (context.check_drop('=')) {
              context.drop_white_spaces();

              return new FinalBindAST(VarAST::Create(this_name), parse_expr(context));
            }
          }
          else if (context.check_drop('=')) {
            context.drop_white_spaces();

            return FlexBindAST::Create(VarAST::Create(this_name, var_data_type), (parse_expr(context)));
          }
          else {
            string errmsg = string("parse_stmt() // Expecting = or := after type, but got ") + context.get_curr_char();
            throw StyioSyntaxError(errmsg);
          }
        }
      } break;  // You should NOT reach this line!

      case '<': {
        context.move(1);

        if (context.check_drop('-')) {
          context.drop_all_spaces();

          return parse_read_file(context, (this_name));
        }
        else {
          string errmsg = string("Expecting `-` after `<`, but found `") + char(context.get_curr_char()) + "`.";
          throw StyioSyntaxError(errmsg);
        }
      } break;  // You should NOT reach this line!

      case '>': {
        context.move(1);

        if (context.check_drop('>')) {
          context.drop_all_spaces();

          return parse_loop_or_iter(context, this_name);
        }
      } break;  // You should NOT reach this line!

      case ',': {
        context.move(1);

        return parse_tuple_no_braces(context, VarAST::Create(this_name));
      } break;  // You should NOT reach this line!

      default: {
        // Nothing
      } break;
    }
  }
  // Int / Float
  else if (context.check_isdigit()) {
    StyioAST* output = parse_int_or_float(context);

    context.drop_all_spaces_comments();

    switch (context.get_curr_char()) {
      case '+': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Add);
      } break;

      case '-': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Sub);
      } break;

      case '*': {
        context.move(1);
        if (context.check_drop('*')) {
          output = parse_binop_rhs(context, output, TokenKind::Binary_Pow);
        }
        else {
          output = parse_binop_rhs(context, output, TokenKind::Binary_Mul);
        }
      } break;

      case '/': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Div);
      } break;

      case '%': {
        context.move(1);
        output = parse_binop_rhs(context, output, TokenKind::Binary_Mod);
      } break;

      default: {
      } break;
    }

    return output;
  }
  // Print
  else if (context.check(">_")) {
    return parse_print(context);
  }

  switch (context.get_curr_char()) {
    case EOF: {
      return new EOFAST();
    } break;  // You should NOT reach this line!

    case '\"': {
      return parse_string(context);
    } break;  // You should NOT reach this line!

    case '?': {
      return parse_cond_flow(context);
    } break;  // You should NOT reach this line!

    case '!': {
      // return parse_panic(context);
    } break;  // You should NOT reach this line!

    case '#': {
      return parse_template(context);
    } break;  // You should NOT reach this line!

    case '.': {
      context.move(1);
      while (context.check('.')) {
        context.move(1);
      }
      return new PassAST();
    } break;  // You should NOT reach this line!

    case '^': {
      context.move(1);

      while (context.check('^')) {
        context.move(1);
      }

      return new BreakAST();
    } break;  // You should NOT reach this line!

    case '@': {
      auto resources = parse_resources(context);

      context.drop_all_spaces_comments();

      if (context.check_drop("->")) {
        context.drop_all_spaces_comments();

        return new FromToAST((resources), parse_block(context));
      }
      else if (context.check_drop(">>")) {
        context.drop_all_spaces_comments();

        auto forward = parse_forward(context, false);

        return IteratorAST::Create(resources, forward);
      }
      else {
        return resources;
      }
    } break;  // You should NOT reach this line!

    case '[': {
      context.move(1);

      context.drop_all_spaces_comments();

      if (context.check_drop('.')) {
        return parse_loop(context);
      }
      else {
        return parse_list_or_loop(context);
      }
    } break;  // You should NOT reach this line!

    case '{': {
      return parse_set(context);
    } break;  // You should NOT reach this line!

    case '(': {
      return parse_tuple(context);
    } break;  // You should NOT reach this line!

    case '=': {
      context.move(1);

      context.check_drop_panic('>');

      context.drop_all_spaces_comments();

      if (context.check_codp()) {
        return parse_codp(context);
      }
      else {
        return new ReturnAST(parse_expr(context));
      }
    } break;  // You should NOT reach this line!

    default:
      break;
  }

  throw StyioSyntaxError(context.label_cur_line(), "Unrecognized statement");
}

string
parse_ext_elem(StyioContext& context) {
  string itemStr;

  if (context.check('\"')) {
    // eliminate double quote symbol " at the start of dependency item
    context.move(1);

    while (context.get_curr_char() != '\"') {
      if (context.check(',')) {
        string errmsg = string("An \" was expected after") + itemStr + "however, a delimeter `,` was detected. ";
        throw StyioSyntaxError(errmsg);
      }

      itemStr += context.get_curr_char();

      context.move(1);
    };

    // eliminate double quote symbol " at the end of dependency item
    context.move(1);

    return itemStr;
  }
  else {
    string errmsg = string(
                      "Dependencies should be wrapped with double quote like "
                      "\"abc/xyz\", rather than starting with the character `"
                    )
                    + char(context.get_curr_char()) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

ExtPackAST*
parse_ext_pack(StyioContext& context) {
  // eliminate left square (box) bracket [
  context.move(1);

  vector<string> dependencies;

  context.drop_all_spaces();

  // add the first dependency path to the list
  dependencies.push_back((parse_ext_elem(context)));

  string pathStr = "";

  while (context.check(',')) {
    // eliminate comma ","
    context.move(1);

    // reset pathStr to empty ""
    pathStr = "";

    context.drop_all_spaces();

    // add the next dependency path to the list
    dependencies.push_back((parse_ext_elem(context)));
  };

  if (context.check(']')) {
    // eliminate right square bracket `]` after dependency list
    context.move(1);
  };

  ExtPackAST* output = new ExtPackAST(dependencies);

  return output;
}

CasesAST*
parse_cases(StyioContext& context) {
  vector<std::pair<StyioAST*, StyioAST*>> pairs;
  StyioAST* _default_stmt;

  /*
    Danger!
    the context -> get_curr_char() must be {
    this line will drop the next 1 character anyway!
  */
  context.move(1);

  while (true) {
    context.drop_all_spaces_comments();
    if (context.check_drop('_')) {
      context.find_drop("=>");

      context.drop_all_spaces_comments();

      if (context.check('{')) {
        _default_stmt = parse_block(context);
      }
      else {
        _default_stmt = parse_stmt(context);
      }

      break;
    }

    StyioAST* left = parse_expr(context);

    context.find_drop("=>");

    context.drop_all_spaces_comments();

    StyioAST* right;
    if (context.check('{')) {
      right = parse_block(context);
    }
    else {
      right = parse_stmt(context);
    }

    pairs.push_back(std::make_pair(left, right));
  }

  context.find_drop_panic('}');

  if (pairs.size() == 0) {
    return CasesAST::Create(_default_stmt);
  }
  else {
    return CasesAST::Create(pairs, _default_stmt);
  }
}

StyioAST*
parse_block(StyioContext& context) {
  vector<StyioAST*> stmtBuffer;

  /*
    Danger!
    when entering parse_block(),
    the context -> get_curr_char() must be {
    this line will drop the next 1 character anyway!
  */
  context.move(1);

  while (true) {
    context.drop_all_spaces_comments();

    if (context.check_drop('}')) {
      break;
    }
    else {
      stmtBuffer.push_back(parse_stmt(context));
    };
  };

  return BlockAST::Create(std::move(stmtBuffer));
}

MainBlockAST*
parse_main_block(StyioContext& context) {
  vector<StyioAST*> stmtBuffer;
  while (true) {
    StyioAST* stmt = parse_stmt(context);

    if ((stmt->getNodeType()) == StyioASTType::End) {
      break;
    }
    else if ((stmt->getNodeType()) == StyioASTType::Comment) {
      continue;
    }
    else {
      stmtBuffer.push_back(stmt);
    }
  }

  return MainBlockAST::Create(stmtBuffer);
}