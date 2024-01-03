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

using std::cout;
using std::endl;

using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

/*
  =================
  - id

  - int
  - float

  - char
  - string
  =================
*/

unique_ptr<IdAST>
parse_id(shared_ptr<StyioContext> context) {
  string name = "";
  /* it will include cur_char in the id without checking */
  do {
    name += context->get_curr_char();
    context->move(1);
  } while (context->check_isalnum_());

  return IdAST::make(name);
}

unique_ptr<IntAST>
parse_int(shared_ptr<StyioContext> context) {
  string digits = "";

  /* it will include cur_char in the digits without checking */
  do {
    digits += context->get_curr_char();
    context->move(1);
  } while (context->check_isdigit());

  return IntAST::make(digits);
}

unique_ptr<StyioAST>
parse_int_or_float(shared_ptr<StyioContext> context) {
  string digits = "";
  /* it will include cur_char in the digits without checking */
  do {
    digits += context->get_curr_char();
    context->move(1);
  } while (context->check_isdigit());

  // int f_exp = 0; /* Float Exponent (Base: 10) */
  if (context->check('.')) {
    if (context->peak_isdigit(1)) {
      digits += ".";
      context->move(1); /* cur_char moves from . to the next */
      do {
        digits += context->get_curr_char();
        context->move(1);
        // f_exp += 1;
      } while (context->check_isdigit());

      return make_unique<FloatAST>(digits);
    }
    else {
      return make_unique<IntAST>(digits);
    }
  }

  return make_unique<IntAST>(digits);
}

unique_ptr<StringAST>
parse_string(shared_ptr<StyioContext> context) {
  /*
    Danger!
    when entering parse_string(),
    the context -> get_curr_char() must be "
    this line will drop the next 1 character anyway!
  */
  context->move(1);

  string textStr = "";

  while (not context->check('\"')) {
    textStr += context->get_curr_char();
    context->move(1);
  }

  // eliminate " at the end
  context->move(1);

  return make_unique<StringAST>(textStr);
}

unique_ptr<StyioAST>
parse_char_or_string(shared_ptr<StyioContext> context) {
  /*
    Danger!
    when entering parse_char_or_string(),
    the context -> get_curr_char() must be '
    this line will drop the next 1 character anyway!
  */
  context->move(1);
  string text = "";

  while (not context->check('\'')) {
    text += context->get_curr_char();
    context->move(1);
  }

  // eliminate ' at the end
  context->move(1);

  if (text.size() == 1) {
    return make_unique<CharAST>(text);
  }
  else {
    return make_unique<StringAST>(text);
  }
}

unique_ptr<FmtStrAST>
parse_fmt_str(shared_ptr<StyioContext> context) {
  /*
    Danger!
    when entering parse_fmt_str(),
    the context -> get_curr_char() must be "
    this line will drop the next 1 character anyway!
  */
  context->move(1);

  vector<string> fragments;
  vector<unique_ptr<StyioAST>> exprs;
  string textStr = "";

  while (not context->check('\"')) {
    if (context->check('{')) {
      if (context->peak_check(1, '{')) {
        textStr += context->get_curr_char();
        context->move(2);
      }
      else {
        context->move(1);

        exprs.push_back(parse_expr(context));

        context->find_drop('}');

        fragments.push_back(textStr);
        textStr.clear();
      }
    }
    else if (context->check('}')) {
      if (context->peak_check(1, '}')) {
        textStr += context->get_curr_char();
        context->move(2);
      }
      else {
        string errmsg = string("Expecting: ") + "}" + "\n" + "But Got: " + context->get_curr_char();
        throw StyioSyntaxError(errmsg);
      }
    }
    else {
      textStr += context->get_curr_char();
      context->move(1);
    }
  }
  // this line drops " at the end anyway!
  context->move(1);

  fragments.push_back(textStr);

  return make_unique<FmtStrAST>(std::move(fragments), std::move(exprs));
}

unique_ptr<StyioAST>
parse_path(shared_ptr<StyioContext> context) {
  context->move(1);

  string text = "";

  while (not context->check('"')) {
    text += context->get_curr_char();
    context->move(1);
  }

  // drop " at the end
  context->move(1);

  if (text.starts_with("/")) {
    return make_unique<LocalPathAST>(StyioPathType::local_absolute_unix_like, text);
  }
  else if (std::isupper(text.at(0)) && text.at(1) == ':') {
    return make_unique<LocalPathAST>(StyioPathType::local_absolute_windows, text);
  }
  else if (text.starts_with("http://")) {
    return make_unique<WebUrlAST>(StyioPathType::url_http, text);
  }
  else if (text.starts_with("https://")) {
    return make_unique<WebUrlAST>(StyioPathType::url_https, text);
  }
  else if (text.starts_with("ftp://")) {
    return make_unique<WebUrlAST>(StyioPathType::url_ftp, text);
  }
  else if (text.starts_with("mysql://")) {
    return make_unique<DBUrlAST>(StyioPathType::db_mysql, text);
  }
  else if (text.starts_with("postgres://")) {
    return make_unique<DBUrlAST>(StyioPathType::db_postgresql, text);
  }
  else if (text.starts_with("mongo://")) {
    return make_unique<DBUrlAST>(StyioPathType::db_mongo, text);
  }
  else if (text.starts_with("localhost") || text.starts_with("127.0.0.1")) {
    return make_unique<RemotePathAST>(StyioPathType::url_localhost, text);
  }
  else if (is_ipv4_at_start(text)) {
    return make_unique<RemotePathAST>(StyioPathType::ipv4_addr, text);
  }
  else if (is_ipv6_at_start(text)) {
    return make_unique<RemotePathAST>(StyioPathType::ipv6_addr, text);
  }
  else if (text.starts_with("\\\\")) {
    return make_unique<RemotePathAST>(StyioPathType::remote_windows, text);
  }

  return make_unique<LocalPathAST>(StyioPathType::local_relevant_any, text);
}

unique_ptr<DTypeAST>
parse_dtype(shared_ptr<StyioContext> context) {
  string text = "";

  if (context->check_isal_()) {
    text += context->get_curr_char();
    context->move(1);
  }

  while (context->check_isalnum_()) {
    text += context->get_curr_char();
    context->move(1);
  }

  return DTypeAST::make(text);
}

/*
  Basic Collection
  - typed_var
  - Fill (Variable Tuple)
  - Resources
*/

shared_ptr<ArgAST>
parse_argument(shared_ptr<StyioContext> context) {
  string name = "";
  /* it includes cur_char in the name without checking */
  do {
    name += context->get_curr_char();
    context->move(1);
  } while (context->check_isalnum_());

  unique_ptr<DTypeAST> data_type;
  unique_ptr<StyioAST> default_value;

  context->drop_white_spaces();

  if (context->check_drop(':')) {
    context->drop_white_spaces();

    data_type = parse_dtype(context);

    context->drop_white_spaces();

    if (context->check_drop('=')) {
      context->drop_white_spaces();

      default_value = parse_expr(context);

      return ArgAST::make(name, std::move(data_type), std::move(default_value));
    }
    else {
      return ArgAST::make(name, std::move(data_type));
    }
  }
  else {
    return ArgAST::make(name);
  }
}

shared_ptr<VarTupleAST>
parse_var_tuple(shared_ptr<StyioContext> context) {
  vector<shared_ptr<VarAST>> vars;

  /* cur_char must be `(` which will be removed without checking */
  context->move(1);

  do {
    context->drop_all_spaces_comments();

    if (context->check_drop(')')) {
      return make_shared<VarTupleAST>(std::move(vars));
    }
    else {
      if (context->check_drop('*')) {
        if (context->check_drop('*')) {
          vars.push_back(make_shared<OptKwArgAST>(parse_id(context)));
        }
        else {
          vars.push_back(make_shared<OptArgAST>(parse_id(context)));
        }
      }
      else {
        vars.push_back(parse_argument(context));
      }
    }
  } while (context->check_drop(','));

  context->find_drop_panic(')');

  return make_shared<VarTupleAST>(vars);
}

unique_ptr<ResourceAST>
parse_resources(
  shared_ptr<StyioContext> context
) {
  unique_ptr<ResourceAST> output;

  vector<unique_ptr<StyioAST>> resources;

  /*
    Danger!
    when entering parse_resources(),
    the context -> get_curr_char() must be @
    this line will drop the next 1 character anyway!
  */
  context->move(1);

  if (context->check_drop('(')) {
    do {
      context->drop_all_spaces_comments();

      if (context->check('"')) {
        resources.push_back(parse_path(context));
      }
      else if (context->check_isal_()) {
        unique_ptr<IdAST> varname = parse_id(context);

        context->find_drop_panic("<-");

        context->drop_all_spaces_comments();

        resources.push_back(
          make_unique<FinalBindAST>(
            std::move(varname),
            parse_num_val(context)
          )
        );
      }

    } while (context->check_drop(','));

    context->find_drop_panic(')');

    output = make_unique<ResourceAST>(std::move(resources));
  }
  else {
    string errmsg = string("@(expr) // Expecting ( after @, but got ") + char(context->get_curr_char()) + "";
    throw StyioSyntaxError(errmsg);
  }

  return output;
}

/*
  Expression
  - Value
  - Binary Comparison
*/

unique_ptr<StyioAST>
parse_item_for_cond(shared_ptr<StyioContext> context) {
  unique_ptr<StyioAST> output;

  context->drop_all_spaces();

  output = parse_num_val(context);

  context->drop_all_spaces();

  switch (context->get_curr_char()) {
    case '=': {
      context->move(1);

      if (context->check('=')) {
        context->move(1);

        /*
          Equal
            expr == expr
        */

        // drop all spaces after ==
        context->drop_all_spaces();

        output = make_unique<BinCompAST>(
          CompType::EQ, std::move(output), parse_num_val(context)
        );
      };
    }

    break;

    case '!': {
      context->move(1);

      if (context->check('=')) {
        context->move(1);

        /*
          Not Equal
            expr != expr
        */

        // drop all spaces after !=
        context->drop_all_spaces();

        output = make_unique<BinCompAST>(
          CompType::NE, std::move(output), parse_num_val(context)
        );
      };
    }

    break;

    case '>': {
      context->move(1);

      if (context->check('=')) {
        context->move(1);

        /*
          Greater Than and Equal
            expr >= expr
        */

        // drop all spaces after >=
        context->drop_all_spaces();

        output = make_unique<BinCompAST>(
          CompType::GE, std::move(output), parse_num_val(context)
        );
      }
      else {
        /*
          Greater Than
            expr > expr
        */

        // drop all spaces after >
        context->drop_all_spaces();

        output = make_unique<BinCompAST>(
          CompType::GT, std::move(output), parse_num_val(context)
        );
      };
    }

    break;

    case '<': {
      context->move(1);

      if (context->check('=')) {
        context->move(1);

        /*
          Less Than and Equal
            expr <= expr
        */

        // drop all spaces after <=
        context->drop_all_spaces();

        output = make_unique<BinCompAST>(
          CompType::LE, std::move(output), parse_num_val(context)
        );
      }
      else {
        /*
          Less Than
            expr < expr
        */

        // drop all spaces after <
        context->drop_all_spaces();

        output = make_unique<BinCompAST>(
          CompType::LT, std::move(output), parse_num_val(context)
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
unique_ptr<StyioAST>
parse_id_or_value(shared_ptr<StyioContext> context) {
  unique_ptr<StyioAST> output;

  if (isalpha(context->get_curr_char()) || context->check('_')) {
    output = parse_id(context);
  }

  if (context->check('[')) {
    output = parse_list_op(context, std::move(output));
  }
  else if (context->check('(')) {
    output = parse_call(context);
  }

  context->drop_all_spaces_comments();

  if (context->check_binop()) {
    output = parse_binop_rhs(context, std::move(output));
  };

  return output;
}

unique_ptr<StyioAST>
parse_num_val(shared_ptr<StyioContext> context) {
  unique_ptr<StyioAST> output;

  if (isalpha(context->get_curr_char()) || context->check('_')) {
    return parse_id_or_value(context);
  }
  else if (isdigit(context->get_curr_char())) {
    return parse_int_or_float(context);
  }
  else if (context->check('|')) {
    return parse_size_of(context);
  }

  string errmsg = string("parse_num_val() // Unexpected value expression, starting with ") + char(context->get_curr_char());
  throw StyioParseError(errmsg);
}

shared_ptr<StyioAST>
parse_item_for_binop(shared_ptr<StyioContext> context) {
  shared_ptr<StyioAST> output = make_shared<NoneAST>();

  if (context->check_isal_()) {
    return parse_id(context);
  }
  else if (context->check_isdigit()) {
    return parse_int_or_float(context);
  }

  switch (context->get_curr_char()) {
    case '\"':
      return parse_string(context);

    case '\'':
      return parse_char_or_string(context);

    case '[':
      context->move(1);

      context->drop_all_spaces_comments();

      if (context->check_drop(']')) {
        return make_unique<EmptyAST>();
      }
      else {
        return parse_list_or_loop(context);
      }

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

unique_ptr<StyioAST>
parse_expr(shared_ptr<StyioContext> context) {
  unique_ptr<StyioAST> output;

  if (isalpha(context->get_curr_char()) || context->check('_')) {
    output = parse_id(context);

    context->drop_all_spaces_comments();

    if (context->check_binop()) {
      output = parse_binop_rhs(context, std::move(output));
    }

    return output;
  }
  else if (isdigit(context->get_curr_char())) {
    output = parse_int_or_float(context);

    context->drop_all_spaces_comments();

    if (context->check_binop()) {
      output = parse_binop_rhs(context, std::move(output));
    }

    return output;
  }

  switch (context->get_curr_char()) {
    case '\'': {
      return parse_char_or_string(context);
    }

    case '\"': {
      return parse_string(context);
    }

    case '[': {
      context->move(1);

      context->drop_all_spaces_comments();

      if (context->check_drop(']')) {
        output = make_unique<EmptyAST>();
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

      context->drop_all_spaces_comments();

      if (context->check_binop()) {
        output = parse_binop_rhs(context, std::move(output));
      }

      return output;
    }

    break;

    case '\\': {
      context->move(1);

      if (context->check_drop('t')) {
        context->check_drop('\\');
        return make_unique<BoolAST>(true);
      }
      else if (context->check_drop('f')) {
        context->check_drop('\\');
        return make_unique<BoolAST>(false);
      }
    }

    break;

    case '$': {
      context->move(1);

      return parse_fmt_str(context);
    }

    default: {
      output = make_unique<NoneAST>();
    } break;
  }

  return output;
}

unique_ptr<StyioAST>
parse_tuple(shared_ptr<StyioContext> context) {
  vector<unique_ptr<StyioAST>> exprs;

  /*
    Danger!
    when entering parse_tuple(),
    the context -> get_curr_char() must be (
    this line will drop the next 1 character anyway!
  */
  context->move(1);

  do {
    context->drop_all_spaces_comments();

    if (context->check_drop(')')) {
      return make_unique<TupleAST>(std::move(exprs));
    }
    else {
      exprs.push_back(parse_expr(context));
      context->drop_white_spaces();
    }
  } while (context->check_drop(','));

  context->check_drop(')');

  if (exprs.size() == 0) {
    return make_unique<EmptyAST>();
  }
  else {
    return make_unique<TupleAST>(std::move(exprs));
  }
}

unique_ptr<StyioAST>
parse_list(shared_ptr<StyioContext> context) {
  vector<unique_ptr<StyioAST>> exprs;

  /*
    Danger!
    when entering parse_list(),
    the context -> get_curr_char() must be [
    this line will drop the next 1 character anyway!
  */
  context->move(1);

  do {
    context->drop_all_spaces_comments();

    if (context->check_drop(']')) {
      return make_unique<ListAST>(std::move(exprs));
    }
    else {
      exprs.push_back(parse_expr(context));
      context->drop_white_spaces();
    }
  } while (context->check_drop(','));

  context->check_drop(']');

  if (exprs.size() == 0) {
    return make_unique<EmptyAST>();
  }
  else {
    return make_unique<ListAST>(std::move(exprs));
  }
}

unique_ptr<StyioAST>
parse_set(shared_ptr<StyioContext> context) {
  vector<unique_ptr<StyioAST>> exprs;

  /*
    Danger!
    when entering parse_set(),
    the context -> get_curr_char() must be {
    this line will drop the next 1 character anyway!
  */
  context->move(1);

  do {
    context->drop_all_spaces_comments();

    if (context->check_drop('}')) {
      return make_unique<SetAST>(std::move(exprs));
    }
    else {
      exprs.push_back(parse_expr(context));
      context->drop_white_spaces();
    }
  } while (context->check_drop(','));

  context->check_drop('}');

  if (exprs.size() == 0) {
    return make_unique<EmptyAST>();
  }
  else {
    return make_unique<SetAST>(std::move(exprs));
  }
}

unique_ptr<StyioAST>
parse_iterable(shared_ptr<StyioContext> context) {
  unique_ptr<StyioAST> output = make_unique<EmptyAST>();

  if (isalpha(context->get_curr_char()) || context->check('_')) {
    output = parse_id(context);

    context->drop_all_spaces_comments();

    if (context->check_binop()) {
      output = parse_binop_rhs(context, std::move(output));
    };

    return output;
  }
  else {
    switch (context->get_curr_char()) {
      case '(': {
        context->move(1);

        vector<unique_ptr<StyioAST>> exprs;
        do {
          context->drop_all_spaces_comments();

          if (context->check_drop(')')) {
            return make_unique<TupleAST>(std::move(exprs));
          }
          else {
            exprs.push_back(parse_expr(context));
            context->drop_white_spaces();
          }
        } while (context->check_drop(','));

        context->check_drop(')');

        if (exprs.size() == 0) {
          return make_unique<EmptyAST>();
        }
        else {
          return make_unique<TupleAST>(std::move(exprs));
        }
      }

      case '[': {
        context->move(1);

        vector<unique_ptr<StyioAST>> exprs;
        do {
          context->drop_all_spaces_comments();

          if (context->check_drop(']')) {
            return make_unique<ListAST>(std::move(exprs));
          }
          else {
            exprs.push_back(parse_expr(context));
            context->drop_white_spaces();
          }
        } while (context->check_drop(','));

        context->check_drop(']');

        if (exprs.size() == 0) {
          return make_unique<EmptyAST>();
        }
        else {
          return make_unique<ListAST>(std::move(exprs));
        }
      }

      case '{': {
        context->move(1);

        vector<unique_ptr<StyioAST>> exprs;
        do {
          context->drop_all_spaces_comments();

          if (context->check_drop('}')) {
            return make_unique<SetAST>(std::move(exprs));
          }
          else {
            exprs.push_back(parse_expr(context));
            context->drop_white_spaces();
          }
        } while (context->check_drop(','));

        context->check_drop('}');

        if (exprs.size() == 0) {
          return make_unique<EmptyAST>();
        }
        else {
          return make_unique<SetAST>(std::move(exprs));
        }
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

unique_ptr<SizeOfAST>
parse_size_of(shared_ptr<StyioContext> context) {
  unique_ptr<SizeOfAST> output;

  // eliminate | at the start
  context->move(1);

  if (isalpha(context->get_curr_char()) || context->check('_')) {
    unique_ptr<StyioAST> var = parse_id_or_value(context);

    // eliminate | at the end
    if (context->check('|')) {
      context->move(1);

      output = make_unique<SizeOfAST>(std::move(var));
    }
    else {
      string errmsg = string("|expr| // SizeOf: Expecting | at the end, but got .:| ") + char(context->get_curr_char()) + " |:.";
      throw StyioSyntaxError(errmsg);
    }
  }
  else {
    string errmsg = string("|expr| // SizeOf: Unexpected expression, starting with .:| ") + char(context->get_curr_char()) + " |:.";
    throw StyioParseError(errmsg);
  }

  return output;
}

/*
  Invoke / Call
*/

unique_ptr<StyioAST>
parse_call(shared_ptr<StyioContext> context) {
  return make_unique<NoneAST>();
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

unique_ptr<StyioAST>
parse_list_op(shared_ptr<StyioContext> context, unique_ptr<StyioAST> theList) {
  unique_ptr<StyioAST> output;

  /*
    Danger!
    when entering parse_list_op(),
    the context -> get_curr_char() must be [
    this line will drop the next 1 character anyway!
  */
  context->move(1);

  do {
    if (isalpha(context->get_curr_char()) || context->check('_')) {
      output = make_unique<ListOpAST>(
        StyioNodeHint::Access, std::move(theList), parse_id_or_value(context)
      );
    }
    else if (isdigit(context->get_curr_char())) {
      output = make_unique<ListOpAST>(
        StyioNodeHint::Access_By_Index, std::move(theList), parse_int(context)
      );
    }
    else {
      switch (context->get_curr_char()) {
        /*
          list["any"]
        */
        case '"': {
          output = make_unique<ListOpAST>(StyioNodeHint::Access_By_Name, std::move(theList), parse_string(context));
        }

        // You should NOT reach this line!
        break;

        /*
          list[<]
        */
        case '<': {
          context->move(1);

          while (context->check('<')) {
            context->move(1);
          }

          output = make_unique<ListOpAST>(StyioNodeHint::Get_Reversed, std::move(theList));
        }

        // You should NOT reach this line!
        break;

        // list[?= item]
        case '?': {
          context->move(1);

          if (context->check_drop('=')) {
            context->drop_all_spaces_comments();

            output = make_unique<ListOpAST>(StyioNodeHint::Get_Index_By_Value, std::move(theList), parse_expr(context));
          }
          else if (context->check_drop('^')) {
            context->drop_all_spaces_comments();

            output = make_unique<ListOpAST>(StyioNodeHint::Get_Indices_By_Many_Values, std::move(theList), parse_iterable(context));
          }
          else {
            string errmsg = string("Expecting ?= or ?^, but got ") + char(context->get_curr_char());
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
          context->move(1);

          context->drop_white_spaces();

          unique_ptr<StyioAST> index = parse_int(context);

          context->drop_white_spaces();

          /*
            list[^index <- value]
          */
          if (context->check_drop("<-")) {
            context->drop_white_spaces();

            output = make_unique<ListOpAST>(StyioNodeHint::Insert_Item_By_Index, std::move(theList), std::move(index), parse_expr(context));
          }
          // list[^index]
          else {
            output = make_unique<ListOpAST>(StyioNodeHint::Access_By_Index, std::move(theList), std::move(index));
          }
        }
        // You should NOT reach this line!
        break;

        /*
          list[+: value]
        */
        case '+': {
          context->move(1);

          context->check_drop_panic(':');

          context->drop_white_spaces();

          unique_ptr<StyioAST> expr = parse_expr(context);

          context->drop_white_spaces();

          output = make_unique<ListOpAST>(
            StyioNodeHint::Append_Value, std::move(theList), std::move(expr)
          );
        }

        // You should NOT reach this line!
        break;

        case '-': {
          context->move(1);

          context->check_drop_panic(':');

          context->drop_white_spaces();

          /*
            list[-: ^index]
          */
          if (context->check_drop('^')) {
            context->drop_white_spaces();

            if (isdigit(context->get_curr_char())) {
              output = make_unique<ListOpAST>(StyioNodeHint::Remove_Item_By_Index, std::move(theList), std::move(parse_int(context)));
            }
            else {
              /*
                list[-: ^(i0, i1, ...)]
              */
              output = make_unique<ListOpAST>(
                StyioNodeHint::Remove_Items_By_Many_Indices,
                std::move(theList),
                std::move(parse_iterable(context))
              );
            }
          }
          else if (context->check_drop('?')) {
            switch (context->get_curr_char()) {
              /*
                list[-: ?= value]
              */
              case '=': {
                context->move(1);

                context->drop_white_spaces();

                output = make_unique<ListOpAST>(StyioNodeHint::Remove_Item_By_Value, std::move(theList), parse_expr(context));
              }

              break;

              /*
                list[-: ?^ (v0, v1, ...)]
              */
              case '^': {
                context->move(1);

                context->drop_white_spaces();

                output = make_unique<ListOpAST>(
                  StyioNodeHint::Remove_Items_By_Many_Values,
                  std::move(theList),
                  parse_iterable(context)
                );
              }

              break;

              default:
                break;
            }
          }
          else {
            output = make_unique<ListOpAST>(StyioNodeHint::Remove_Item_By_Value, std::move(theList), parse_expr(context));
          }
        }

        // You should NOT reach this line!
        break;

        case ']': {
          output = std::move(theList);
        }

        // You should NOT reach this line!
        break;

        default: {
          string errmsg = string("Unexpected List[Operation], starts with ") + char(context->get_curr_char());
          throw StyioSyntaxError(errmsg);
        }

        // You should NOT reach this line!
        break;
      }
    }
  } while (context->check('['));

  context->find_drop(']');

  return output;
}

unique_ptr<StyioAST>
parse_loop_or_iter(shared_ptr<StyioContext> context, unique_ptr<StyioAST> iterOverIt) {
  context->drop_all_spaces_comments();

  if ((iterOverIt->hint()) == StyioNodeHint::Infinite) {
    return make_unique<LoopAST>(parse_forward(context, false));
  }
  else {
    return make_unique<IterAST>(std::move(iterOverIt), parse_forward(context, false));
  }
}

unique_ptr<StyioAST>
parse_list_or_loop(shared_ptr<StyioContext> context) {
  unique_ptr<StyioAST> output;

  vector<unique_ptr<StyioAST>> elements;

  unique_ptr<StyioAST> startEl = parse_expr(context);

  context->drop_white_spaces();

  if (context->check_drop('.')) {
    while (context->check('.')) {
      context->move(1);
    }

    context->drop_white_spaces();

    unique_ptr<StyioAST> endEl = parse_expr(context);

    context->drop_white_spaces();

    context->check_drop_panic(']');

    if (startEl->hint() == StyioNodeHint::Int && endEl->hint() == StyioNodeHint::Id) {
      output = make_unique<InfiniteAST>(std::move(startEl), std::move(endEl));
    }
    else if (startEl->hint() == StyioNodeHint::Int && endEl->hint() == StyioNodeHint::Int) {
      output = make_unique<RangeAST>(
        std::move(startEl), std::move(endEl), make_unique<IntAST>("1")
      );
    }
    else {
      string errmsg = string("Unexpected Range / List / Loop: ") + "starts with: " + std::to_string(type_to_int(startEl->hint())) + ", " + "ends with: " + std::to_string(type_to_int(endEl->hint())) + ".";
      throw StyioSyntaxError(errmsg);
    }
  }
  else if (context->check_drop(',')) {
    elements.push_back(std::move(startEl));

    do {
      context->drop_all_spaces_comments();

      if (context->check_drop(']')) {
        return make_unique<ListAST>(std::move(elements));
      }
      else {
        elements.push_back(parse_expr(context));
      }
    } while (context->check_drop(','));

    context->find_drop_panic(']');

    output = make_unique<ListAST>(std::move(elements));
  }
  else {
    elements.push_back(std::move(startEl));

    context->find_drop_panic(']');

    output = make_unique<ListAST>(std::move(elements));
  }

  while (context->check('[')) {
    output = parse_list_op(context, std::move(output));
  }

  context->drop_all_spaces();

  if (context->check_drop(">>")) {
    output = parse_loop_or_iter(context, std::move(output));
  }

  return output;
}

unique_ptr<StyioAST>
parse_loop(shared_ptr<StyioContext> context) {
  unique_ptr<StyioAST> output;

  while (context->check('.')) {
    context->move(1);
  }

  context->find_drop_panic(']');

  context->drop_all_spaces();

  if (context->check_drop(">>")) {
    context->drop_all_spaces();

    return make_unique<LoopAST>(parse_forward(context, false));
  }

  return make_unique<InfiniteAST>();
}

unique_ptr<BinOpAST>
parse_binop_rhs(shared_ptr<StyioContext> context, shared_ptr<StyioAST> lhs_ast) {
  unique_ptr<BinOpAST> output;

  switch (context->get_curr_char()) {
    // Bin_Add := <ID> "+" <EXPR>
    case '+': {
      context->move(1);
      context->drop_all_spaces_comments();

      if (context->check_drop('=')) {
        context->drop_all_spaces();

        output = make_unique<BinOpAST>(StyioNodeHint::Inc_Add, std::move(lhs_ast), std::move(parse_item_for_binop(context)));

        return output;
      }
      else {
        output = make_unique<BinOpAST>(StyioNodeHint::Bin_Add, std::move(lhs_ast), std::move(parse_item_for_binop(context)));
      }
    };

      // You should NOT reach this line!
      break;

    // Bin_Sub := <ID> "-" <EXPR>
    case '-': {
      context->move(1);
      context->drop_all_spaces_comments();

      if (context->check_drop('=')) {
        context->drop_all_spaces();

        output = make_unique<BinOpAST>(StyioNodeHint::Inc_Sub, std::move(lhs_ast), std::move(parse_item_for_binop(context)));

        return output;
      }
      else {
        output = make_unique<BinOpAST>(StyioNodeHint::Bin_Sub, std::move(lhs_ast), std::move(parse_item_for_binop(context)));
      }
    };

      // You should NOT reach this line!
      break;

    // Bin_Mul | Bin_Pow
    case '*': {
      context->move(1);
      // Bin_Pow := <ID> "**" <EXPR>
      if (context->check_drop('*')) {
        context->move(1);
        context->drop_all_spaces_comments();

        // <ID> "**" |--
        output = make_unique<BinOpAST>(StyioNodeHint::Bin_Pow, std::move(lhs_ast), std::move(parse_item_for_binop(context)));
      }
      else if (context->check_drop('=')) {
        context->drop_all_spaces();

        output = make_unique<BinOpAST>(StyioNodeHint::Inc_Mul, std::move(lhs_ast), std::move(parse_item_for_binop(context)));

        return output;
      }
      // Bin_Mul := <ID> "*" <EXPR>
      else {
        context->drop_all_spaces();

        // <ID> "*" |--
        output = make_unique<BinOpAST>(StyioNodeHint::Bin_Mul, std::move(lhs_ast), std::move(parse_item_for_binop(context)));
      }
    };
      // You should NOT reach this line!
      break;

    // Bin_Div := <ID> "/" <EXPR>
    case '/': {
      context->move(1);
      context->drop_all_spaces_comments();

      if (context->check_drop('=')) {
        context->drop_all_spaces();

        output = make_unique<BinOpAST>(StyioNodeHint::Inc_Div, std::move(lhs_ast), std::move(parse_item_for_binop(context)));

        return output;
      }
      else {
        output = make_unique<BinOpAST>(StyioNodeHint::Bin_Div, std::move(lhs_ast), std::move(parse_item_for_binop(context)));
      }
    };

      // You should NOT reach this line!
      break;

    // Bin_Mod := <ID> "%" <EXPR>
    case '%': {
      context->move(1);
      context->drop_all_spaces_comments();

      // <ID> "%" |--
      output = make_unique<BinOpAST>(StyioNodeHint::Bin_Mod, std::move(lhs_ast), std::move(parse_item_for_binop(context)));
    };

      // You should NOT reach this line!
      break;

    default:
      string errmsg = string("Unexpected BinOp.Operator: `") + char(context->get_curr_char()) + "`.";
      throw StyioSyntaxError(errmsg);

      // You should NOT reach this line!
      break;
  }

  context->drop_all_spaces_comments();

  while (context->check_binop()) {
    context->drop_all_spaces();

    output = parse_binop_rhs(context, std::move(output));
  }

  return output;
}

unique_ptr<CondAST>
parse_cond_rhs(shared_ptr<StyioContext> context, unique_ptr<StyioAST> lhsExpr) {
  unique_ptr<CondAST> condExpr;

  context->drop_all_spaces();

  switch (context->get_curr_char()) {
    case '&': {
      context->move(1);

      context->check_drop('&');

      /*
        support:
          expr && \n
          expression
      */

      context->drop_all_spaces();

      condExpr = make_unique<CondAST>(
        LogicType::AND, std::move(lhsExpr), parse_cond(context)
      );
    }

    break;

    case '|': {
      context->move(1);

      if (context->check('|')) {
        context->move(1);
      };

      /*
        support:
          expr || \n
          expression
      */

      context->drop_all_spaces();

      condExpr = make_unique<CondAST>(
        LogicType::OR, std::move(lhsExpr), parse_cond(context)
      );
    }

    break;

    case '^': {
      context->move(1);

      /*
        support:
          expr ^ \n
          expression
      */

      context->drop_all_spaces();

      condExpr = make_unique<CondAST>(
        LogicType::OR, std::move(lhsExpr), parse_cond(context)
      );
    }

    break;

    case '!': {
      context->move(1);

      if (context->check('(')) {
        context->move(1);

        /*
          support:
            !( \n
              expr
            )
        */
        context->drop_all_spaces();

        condExpr = make_unique<CondAST>(LogicType::NOT, parse_cond(context));

        context->find_drop_panic(')');
      }
    }

    break;

    default:
      break;
  }

  context->drop_all_spaces();

  while (!(context->check(')'))) {
    condExpr = std::move(parse_cond_rhs(context, std::move(condExpr)));
  }

  return condExpr;
}

unique_ptr<CondAST>
parse_cond(shared_ptr<StyioContext> context) {
  unique_ptr<StyioAST> lhsExpr;

  if (context->check('(')) {
    context->move(1);

    lhsExpr = std::move(parse_cond(context));

    context->find_drop_panic(')');
  }
  else if (context->check('!')) {
    context->move(1);

    if (context->check('(')) {
      context->move(1);

      /*
        support:
          !( \n
            expr
          )
      */
      context->drop_all_spaces();

      lhsExpr = std::move(parse_cond(context));

      context->drop_all_spaces();

      return make_unique<CondAST>(LogicType::NOT, std::move(lhsExpr));
    }
    else {
      string errmsg = string("!(expr) // Expecting ( after !, but got ") + char(context->get_curr_char());
      throw StyioSyntaxError(errmsg);
    };
  }
  else {
    lhsExpr = std::move(parse_item_for_cond(context));
  };

  // drop all spaces after first value
  context->drop_all_spaces();

  if (context->check('&') || context->check('|')) {
    return parse_cond_rhs(context, std::move(lhsExpr));
  }
  else {
    return make_unique<CondAST>(LogicType::RAW, std::move(lhsExpr));
  }

  string errmsg = string("parse_cond() : You should not reach this line!") + char(context->get_curr_char());
  throw StyioParseError(errmsg);
}

unique_ptr<StyioAST>
parse_cond_flow(shared_ptr<StyioContext> context) {
  /*
    Danger!
    when entering parse_cond_flow(),
    the context -> get_curr_char() must be ?
    this line will drop the next 1 character anyway!
  */
  context->move(1);

  context->drop_white_spaces();

  if (context->check_drop('(')) {
    unique_ptr<CondAST> condition = parse_cond(context);

    context->find_drop_panic(')');

    /*
      support:
        ?() \n
        \t\

        ?() \n
        \f\
    */
    context->drop_all_spaces_comments();

    if (context->check_drop('\\')) {
      unique_ptr<StyioAST> block;

      if (context->check_drop('t')) {
        context->check_drop('\\');

        /*
          support:
            \t\ \n
            {}
        */

        context->drop_all_spaces_comments();

        block = parse_block(context);

        /*
          support:
            \t\ {} \n
            \f\
        */
        context->drop_all_spaces_comments();

        if (context->check_drop('\\')) {
          context->check_drop_panic('f');

          context->check_drop('\\');

          /*
            support:
              \f\ \n
              {}
          */
          context->drop_all_spaces_comments();

          unique_ptr<StyioAST> blockElse = parse_block(context);

          return make_unique<CondFlowAST>(StyioNodeHint::CondFlow_Both, std::move(condition), std::move(block), std::move(blockElse));
        }
        else {
          return make_unique<CondFlowAST>(StyioNodeHint::CondFlow_True, std::move(condition), std::move(block));
        }
      }
      else if (context->check_drop('f')) {
        context->check_drop('\\');

        /*
          support:
            \f\ \n
            {}
        */
        context->drop_all_spaces_comments();

        block = parse_block(context);

        return make_unique<CondFlowAST>(StyioNodeHint::CondFlow_False, std::move(condition), std::move(block));
      }
      else {
        string errmsg = string("parse_cond_flow() // Unexpected character ") + context->get_curr_char();
        throw StyioSyntaxError(errmsg);
      }
    }
  }
  else {
    string errmsg = string("Missing （ for ?(`expr`).");
    throw StyioSyntaxError(errmsg);
  }
  
  throw StyioSyntaxError(context->label_cur_line(), string("Invalid Syntax"));
}

unique_ptr<StyioAST>
parse_func(shared_ptr<StyioContext> context) {
  /* this line drops cur_char without checking */
  context->move(1);
  context->drop_white_spaces();

  if (context->check_isal_()) {
    auto name = parse_id(context);

    context->drop_all_spaces_comments();

    if (context->check_drop(':')) {
      if (context->check_drop('=')) {
        context->drop_all_spaces();

        return make_unique<FuncAST>(
          std::move(name), parse_forward(context, true), true
        );
      }
      else {
        context->drop_all_spaces_comments();

        auto dtype = parse_dtype(context);

        context->drop_all_spaces_comments();

        if (context->check_drop(':')) {
          if (context->check_drop('=')) {
            context->drop_all_spaces_comments();

            return make_unique<FuncAST>(std::move(name), std::move(dtype), parse_forward(context, true), true);
          }
        }
        else if (context->check_drop('=')) {
          context->drop_all_spaces_comments();

          return make_unique<FuncAST>(std::move(name), std::move(dtype), parse_forward(context, true), false);
        }
      }

      string errmsg = string("parse_pipeline() // Inheritance, Type Hint.");
      throw StyioNotImplemented(errmsg);
    }
    else if (context->check_drop('=')) {
      if (context->check('>')) {
        context->move(-1);

        return make_unique<FuncAST>(
          std::move(name), parse_forward(context, true), false
        );
      }
      else {
        context->drop_all_spaces();

        return make_unique<FuncAST>(
          std::move(name), parse_forward(context, true), false
        );
      }
    }
  }

  context->drop_all_spaces();
  return parse_forward(context, true);
}

/*
  Return:
    [?] AnonyFunc
    [?] MatchCases
*/
unique_ptr<ForwardAST>
parse_forward(shared_ptr<StyioContext> context, bool is_func) {
  unique_ptr<ForwardAST> output;

  shared_ptr<VarTupleAST> args;
  bool has_args = false;

  if (is_func) {
    if (context->check('(')) {
      args = parse_var_tuple(context);
      has_args = true;
    }
  }
  else if (context->check_drop('#')) {
    context->drop_white_spaces();
    if (context->check('(')) {
      args = parse_var_tuple(context);
      has_args = true;
    }
    else {
      string errmsg = string("parse_forward() // Expecting ( after #, but got ") + char(context->get_curr_char());
      throw StyioSyntaxError(errmsg);
    }
  }

  context->drop_all_spaces();

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
  switch (context->get_curr_char()) {
    case '?': {
      context->move(1);

      switch (context->get_curr_char()) {
        /*
          ?= value
        */
        case '=': {
          context->move(1);
          context->drop_white_spaces();

          auto cases = parse_cases(context);

          /* #(args) ?= cases */
          if (context->check('{')) {
            if (has_args) {
              output = make_unique<ForwardAST>(std::move(cases));
            }
            else {
              output = make_unique<ForwardAST>(std::move(cases));
            }

            return output;
          }
          /* #(args) ?= value => then */
          else {
            unique_ptr<CheckEqAST> extra_check;
            unique_ptr<StyioAST> then;

            extra_check = make_unique<CheckEqAST>(parse_expr(context));

            context->drop_all_spaces();

            if (context->check_drop("=>")) {
              context->drop_all_spaces();

              if (context->check('{')) {
                then = parse_block(context);
              }
              else {
                then = parse_expr(context);
              }

              if (has_args) {
                output = make_unique<ForwardAST>(
                  std::move(args), std::move(extra_check), std::move(then)
                );
              }
              else {
                output = make_unique<ForwardAST>(std::move(extra_check), std::move(then));
              }
            }
            else {
              string errmsg = string(
                                "parse_forward() // Expecting `=>` after `?= "
                                "value`, but got "
                              )
                              + char(context->get_curr_char());
              throw StyioSyntaxError(errmsg);
            }
          }
        }

        break;
        /*
          ?^ [Iterable]
        */
        case '^': {
          context->move(1);

          unique_ptr<StyioAST> nextExpr;

          unique_ptr<StyioAST> iterable;

          context->drop_white_spaces();

          switch (context->get_curr_char()) {
            case '(': {
              context->move(1);

              vector<unique_ptr<StyioAST>> exprs;
              do {
                context->drop_all_spaces_comments();

                if (context->check(')')) {
                  break;
                }
                else {
                  exprs.push_back(parse_expr(context));
                }
              } while (context->check_drop(','));

              context->find_drop(')');

              iterable = make_unique<TupleAST>(std::move(exprs));
            } break;

            case '[': {
              context->move(1);

              vector<unique_ptr<StyioAST>> exprs;
              do {
                context->drop_all_spaces_comments();

                if (context->check(']')) {
                  break;
                }
                else {
                  exprs.push_back(parse_expr(context));
                }
              } while (context->check_drop(','));

              context->find_drop(']');

              iterable = make_unique<ListAST>(std::move(exprs));
            }

            break;

            case '{': {
              context->move(1);

              vector<unique_ptr<StyioAST>> exprs;
              do {
                context->drop_all_spaces_comments();

                if (context->check('}')) {
                  break;
                }
                else {
                  exprs.push_back(parse_expr(context));
                }
              } while (context->check_drop(','));

              context->find_drop('}');

              iterable = make_unique<SetAST>(std::move(exprs));
            } break;

            default: {
              if (isalpha(context->get_curr_char()) || context->check('_')) {
                iterable = parse_id_or_value(context);
              }
              else {
                string errmsg = string(
                                  "parse_forward() // Unexpected collection, "
                                  "starting with "
                                )
                                + char(context->get_curr_char());
                throw StyioSyntaxError(errmsg);
              }
            } break;
          }

          context->drop_all_spaces_comments();

          if (context->check_drop("=>")) {
            context->drop_all_spaces();

            if (context->check('{')) {
              nextExpr = parse_block(context);
            }
            else {
              nextExpr = parse_expr(context);
            }

            if (has_args) {
              output = make_unique<ForwardAST>(
                std::move(args),
                make_unique<CheckIsInAST>(std::move(iterable)),
                std::move(nextExpr)
              );
            }
            else {
              output = make_unique<ForwardAST>(
                make_unique<CheckIsInAST>(std::move(iterable)),
                std::move(nextExpr)
              );
            }
          }
          else {
            string errmsg = string(
                              "parse_forward() // Expecting `=>` after `?^ "
                              "iterable`, but got "
                            )
                            + char(context->get_curr_char());
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
          context->move(-1);

          if (has_args) {
            output = make_unique<ForwardAST>(std::move(args), parse_cond_flow(context));
          }
          else {
            output = make_unique<ForwardAST>(parse_cond_flow(context));
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
      context->move(1);

      context->check_drop_panic('>');

      context->drop_all_spaces();

      if (context->check('{')) {
        if (has_args) {
          output = make_unique<ForwardAST>(std::move(args), parse_block(context));
        }
        else {
          output = make_unique<ForwardAST>(parse_block(context));
        }
      }
      else {
        if (has_args) {
          output = make_unique<ForwardAST>(std::move(args), parse_expr(context));
        }
        else {
          output = make_unique<ForwardAST>(parse_expr(context));
        }
      }
    } break;

    /*
      support:

      { }
    */
    case '{': {
      if (has_args) {
        output = make_unique<ForwardAST>(std::move(args), parse_block(context));
      }
      else {
        output = make_unique<ForwardAST>(parse_block(context));
      }
    } break;

    default:
      string errmsg = string("parse_forward() // Unexpected character ") + char(context->get_curr_char());
      throw StyioSyntaxError(errmsg);

      break;
  }

  return output;
}

unique_ptr<StyioAST>
parse_read_file(shared_ptr<StyioContext> context, unique_ptr<IdAST> id_ast) {
  if (context->check_drop('@')) {
    context->check_drop_panic('(');

    if (context->check('"')) {
      auto path = parse_path(context);

      context->find_drop_panic(')');

      return make_unique<ReadFileAST>(std::move(id_ast), std::move(path));
    }
    else {
      string errmsg = string("Expecting id or string, but got ` ") + char(context->get_curr_char()) + " `";
      throw StyioSyntaxError(errmsg);
    }
  }
  else {
    string errmsg = string("parse_read_file() // Expecting @ as first character but got ` ") + char(context->get_curr_char()) + " `";
    throw StyioSyntaxError(errmsg);
  }
}

unique_ptr<StyioAST>
parse_print(shared_ptr<StyioContext> context) {
  unique_ptr<StyioAST> output;

  vector<unique_ptr<StyioAST>> exprs;

  /* Expecting >_ ! Move Without Check !*/
  context->move(2);

  /* Expecting ( ! Must Have ! */
  context->check_drop_panic('(');

  do {
    context->drop_all_spaces_comments();

    if (context->check_drop(')')) {
      return make_unique<PrintAST>(std::move(exprs));
    }
    else {
      exprs.push_back(parse_expr(context));
    }
  } while (context->check_drop(','));

  context->find_drop_panic(')');
  return make_unique<PrintAST>(std::move(exprs));
}

// unique_ptr<StyioAST> parse_panic (
//   shared_ptr<StyioContext> context) {
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

unique_ptr<StyioAST>
parse_stmt(
  shared_ptr<StyioContext> context
) {
  context->drop_all_spaces_comments();

  if (isalpha(context->get_curr_char()) || context->check('_')) {
    unique_ptr<IdAST> id_ast = parse_id(context);

    if (context->check('[')) {
      return parse_list_op(context, std::move(id_ast));
    }

    context->drop_all_spaces_comments();

    if (context->check_binop()) {
      return parse_binop_rhs(context, std::move(id_ast));
    }

    switch (context->get_curr_char()) {
      case '=': {
        context->move(1);

        context->drop_all_spaces_comments();

        return make_unique<FlexBindAST>(std::move(id_ast), std::move(parse_expr(context)));
      };

        // You should NOT reach this line!
        break;

      case ':': {
        context->move(1);

        if (context->check_drop('=')) {
          context->drop_all_spaces_comments();

          return make_unique<FinalBindAST>(std::move(id_ast), std::move(parse_expr(context)));
        }
        else {
          context->drop_white_spaces();

          auto type = parse_dtype(context);

          context->drop_white_spaces();

          if (context->check_drop(':')) {
            if (context->check_drop('=')) {
              context->drop_white_spaces();

              return make_unique<FinalBindAST>(std::move(id_ast), std::move(parse_expr(context)));
            }
          }
          else if (context->check_drop('=')) {
            context->drop_white_spaces();

            return make_unique<FlexBindAST>(std::move(id_ast), std::move(parse_expr(context)));
          }
          else {
            string errmsg = string("parse_stmt() // Expecting = or := after type, but got ") + context->get_curr_char();
            throw StyioSyntaxError(errmsg);
          }
        }
      };

        // You should NOT reach this line!
        break;

      case '<': {
        context->move(1);

        if (context->check_drop('-')) {
          context->drop_all_spaces();

          return parse_read_file(context, std::move(id_ast));
        }
        else {
          string errmsg = string("Expecting `-` after `<`, but found `") + char(context->get_curr_char()) + "`.";
          throw StyioSyntaxError(errmsg);
        }
      }

      // You should NOT reach this line!
      break;

      case '>': {
        context->move(1);

        if (context->check_drop('>')) {
          context->drop_all_spaces();

          return parse_loop_or_iter(context, std::move(id_ast));
        }
      }
      // You should NOT reach this line!
      break;

      default:
        break;
    }
  }
  // Int / Float
  else if (isdigit(context->get_curr_char())) {
    unique_ptr<StyioAST> numAST = parse_int_or_float(context);

    context->drop_all_spaces_comments();

    if (context->check_binop()) {
      return parse_binop_rhs(context, std::move(numAST));
    }
    else {
      return numAST;
    }
  }
  // Print
  else if (context->check(">_")) {
    return parse_print(context);
  }

  switch (context->get_curr_char()) {
    case EOF:
      return make_unique<EOFAST>();

      // You should NOT reach this line!
      break;

    case '\"':
      return parse_string(context);

      // You should NOT reach this line!
      break;

    case '?':
      return parse_cond_flow(context);

      // You should NOT reach this line!
      break;

    case '!':
      // return parse_panic(context);

      // You should NOT reach this line!
      break;

    case '#':
      return parse_func(context);

      // You should NOT reach this line!
      break;

    case '.': {
      context->move(1);
      while (context->check('.')) {
        context->move(1);
      }
      return make_unique<PassAST>();
    }

    // You should NOT reach this line!
    break;

    case '^': {
      context->move(1);

      while (context->check('^')) {
        context->move(1);
      }

      return make_unique<BreakAST>();
    }

    // You should NOT reach this line!
    break;

    case '@': {
      unique_ptr<ResourceAST> resources = parse_resources(context);

      context->drop_all_spaces_comments();

      if (context->check_drop("->")) {
        context->drop_all_spaces_comments();

        return make_unique<FromToAST>(std::move(resources), parse_block(context));
      }
      else {
        return resources;
      }
    }

    // You should NOT reach this line!
    break;

    case '[': {
      context->move(1);

      context->drop_all_spaces_comments();

      if (context->check_drop('.')) {
        return parse_loop(context);
      }
      else {
        return parse_list_or_loop(context);
      }
    }

    // You should NOT reach this line!
    break;

    case '=': {
      context->move(1);

      if (context->check_drop('>')) {
        context->drop_white_spaces();

        return make_unique<ReturnAST>(parse_expr(context));
      }
      else {
        string errmsg = string("parse_stmt() // =") + context->get_curr_char();
        throw StyioSyntaxError(errmsg);
      }
    }

    // You should NOT reach this line!
    break;

    default:
      break;
  }

  string errmsg = string("Unrecognized statement, starting with `") + char(context->get_curr_char()) + "`";
  throw StyioSyntaxError(errmsg);
}

string
parse_ext_elem(shared_ptr<StyioContext> context) {
  string itemStr;

  if (context->check('\"')) {
    // eliminate double quote symbol " at the start of dependency item
    context->move(1);

    while (context->get_curr_char() != '\"') {
      if (context->check(',')) {
        string errmsg = string("An \" was expected after") + itemStr + "however, a delimeter `,` was detected. ";
        throw StyioSyntaxError(errmsg);
      }

      itemStr += context->get_curr_char();

      context->move(1);
    };

    // eliminate double quote symbol " at the end of dependency item
    context->move(1);

    return itemStr;
  }
  else {
    string errmsg = string(
                      "Dependencies should be wrapped with double quote like "
                      "\"abc/xyz\", rather than starting with the character `"
                    )
                    + char(context->get_curr_char()) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

unique_ptr<ExtPackAST>
parse_ext_pack(shared_ptr<StyioContext> context) {
  // eliminate left square (box) bracket [
  context->move(1);

  vector<string> dependencies;

  context->drop_all_spaces();

  // add the first dependency path to the list
  dependencies.push_back(std::move(parse_ext_elem(context)));

  string pathStr = "";

  while (context->check(',')) {
    // eliminate comma ","
    context->move(1);

    // reset pathStr to empty ""
    pathStr = "";

    context->drop_all_spaces();

    // add the next dependency path to the list
    dependencies.push_back(std::move(parse_ext_elem(context)));
  };

  if (context->check(']')) {
    // eliminate right square bracket `]` after dependency list
    context->move(1);
  };

  unique_ptr<ExtPackAST> output = make_unique<ExtPackAST>(dependencies);

  return output;
}

unique_ptr<CasesAST>
parse_cases(shared_ptr<StyioContext> context) {
  vector<std::tuple<unique_ptr<StyioAST>, unique_ptr<StyioAST>>> pairs;
  unique_ptr<StyioAST> _default_stmt;

  /*
    Danger!
    the context -> get_curr_char() must be {
    this line will drop the next 1 character anyway!
  */
  context->move(1);

  while (true) {
    context->drop_all_spaces_comments();
    if (context->check_drop('_')) {
      context->find_drop("=>");

      context->drop_all_spaces_comments();

      if (context->check('{')) {
        _default_stmt = parse_block(context);
      }
      else {
        _default_stmt = parse_stmt(context);
      }

      break;
    }

    unique_ptr<StyioAST> left = parse_expr(context);

    context->find_drop("=>");

    context->drop_all_spaces_comments();

    unique_ptr<StyioAST> right;
    if (context->check('{')) {
      right = parse_block(context);
    }
    else {
      right = parse_stmt(context);
    }

    pairs.push_back(std::make_tuple(std::move(left), std::move(right)));
  }

  context->find_drop_panic('}');

  if (pairs.size() == 0) {
    return make_unique<CasesAST>(std::move(_default_stmt));
  }
  else {
    return make_unique<CasesAST>(std::move(pairs), std::move(_default_stmt));
  }
}

unique_ptr<StyioAST>
parse_block(shared_ptr<StyioContext> context) {
  vector<unique_ptr<StyioAST>> stmtBuffer;

  /*
    Danger!
    when entering parse_block(),
    the context -> get_curr_char() must be {
    this line will drop the next 1 character anyway!
  */
  context->move(1);

  while (true) {
    context->drop_all_spaces_comments();

    if (context->check_drop('}')) {
      break;
    }
    else {
      stmtBuffer.push_back(std::move(parse_stmt(context)));
    };
  };

  if (stmtBuffer.size() == 0) {
    return make_unique<EmptyBlockAST>();
  }
  else {
    return make_unique<SideBlockAST>(std::move(stmtBuffer));
  };
}

shared_ptr<MainBlockAST>
parse_main_block(shared_ptr<StyioContext> context) {
  vector<unique_ptr<StyioAST>> stmtBuffer;
  while (true) {
    unique_ptr<StyioAST> stmt = parse_stmt(context);

    if ((stmt->hint()) == StyioNodeHint::End) {
      break;
    }
    else if ((stmt->hint()) == StyioNodeHint::Comment) {
      continue;
    }
    else {
      stmtBuffer.push_back(std::move(stmt));
    }
  }

  return make_shared<MainBlockAST>(std::move(stmtBuffer));
}