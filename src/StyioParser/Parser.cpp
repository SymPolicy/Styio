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
#include "BinExprMapper.hpp"
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
parse_name_as_str(StyioContext& context) {
  if (context.cur_tok_type() != StyioTokenType::NAME) {
    string errmsg = string("parse_name_as_str(): False Invoke");
    throw StyioParseError(errmsg);
  }

  auto name = context.cur_tok()->original;
  context.move_forward(1, "parse_name_as_str");
  return name;
}

std::string
parse_name_as_str_unsafe(StyioContext& context) {
  auto name = context.cur_tok()->original;
  context.move_forward(1, "parse_name_as_str_unsafe");
  return name;
}

std::vector<std::string>
parse_name_with_spaces_unsafe(StyioContext& context) {
  std::vector<std::string> name_seps;
  do {
    name_seps.push_back(context.cur_tok()->original);
    context.move_forward(1);
    while (context.check(StyioTokenType::TOK_SPACE) /* White Space */) {
      context.move_forward(1);
    }
  } while (context.check(StyioTokenType::NAME) || context.check(StyioTokenType::INTEGER));

  return name_seps;
}

HashTagNameAST*
parse_name_for_hash_tag(StyioContext& context) {
  std::vector<std::string> name_seps;
  do {
    name_seps.push_back(context.cur_tok()->original);
    context.move_forward(1);
    while (context.check(StyioTokenType::TOK_SPACE) /* White Space */) {
      context.move_forward(1);
    }
  } while (context.check(StyioTokenType::NAME) || context.check(StyioTokenType::INTEGER));

  return HashTagNameAST::Create(name_seps);
}

NameAST*
parse_name(StyioContext& context) {
  if (context.cur_tok_type() != StyioTokenType::NAME) {
    string errmsg = string("parse_name(): False Invoke");
    throw StyioParseError(errmsg);
  }

  auto ret_val = NameAST::Create(context.cur_tok()->original);
  context.move_forward(1, "parse_name");
  return ret_val;
}

NameAST*
parse_name_unsafe(StyioContext& context) {
  auto ret_val = NameAST::Create(context.cur_tok()->original);
  context.move_forward(1, "parse_name_unsafe");
  return ret_val;
}

StyioAST*
parse_name_and_following_unsafe(StyioContext& context) {
  auto name = NameAST::Create(context.cur_tok()->original);
  context.move_forward(1);

  StyioAST* output = name;

  context.skip();
  switch (context.cur_tok_type()) {
    /* + */
    case StyioTokenType::TOK_PLUS: {
      context.move_forward(1, "parse_name_and_following(TOK_PLUS)");
      output = parse_binop_rhs(context, name, StyioOpType::Binary_Add);
    } break;

    /* - */
    case StyioTokenType::TOK_MINUS: {
      context.move_forward(1, "parse_name_and_following(TOK_MINUS)");
      output = parse_binop_rhs(context, name, StyioOpType::Binary_Sub);
    } break;

    /* * */
    case StyioTokenType::TOK_STAR: {
      context.move_forward(1, "parse_name_and_following(TOK_STAR)");
      output = parse_binop_rhs(context, name, StyioOpType::Binary_Mul);
    } break;

    /* ** */
    case StyioTokenType::BINOP_POW: {
      context.move_forward(1, "parse_name_and_following(BINOP_POW)");
      output = parse_binop_rhs(context, name, StyioOpType::Binary_Pow);
    } break;

    /* / */
    case StyioTokenType::TOK_SLASH: {
      context.move_forward(1, "parse_name_and_following(TOK_SLASH)");
      output = parse_binop_rhs(context, name, StyioOpType::Binary_Div);
    } break;

    /* % */
    case StyioTokenType::TOK_PERCENT: {
      context.move_forward(1, "parse_name_and_following(TOK_PERCENT)");
      output = parse_binop_rhs(context, name, StyioOpType::Binary_Mod);
    } break;

    /* ( */
    case StyioTokenType::TOK_LPAREN: {
      output = parse_call(context, name);
    } break;

    /* . */
    case StyioTokenType::TOK_DOT: {
      context.move_forward(1, "parse_name_and_following(TOK_DOT)");
      context.skip();
      if (context.check(StyioTokenType::NAME)) {
        auto func_name = parse_name_unsafe(context);
        output = parse_call(context, func_name, name);
      }
      else {
        throw StyioSyntaxError("parse_name_and_following: There should be a name after dot!");
      }
    } break;

    /* >> */
    case StyioTokenType::ITERATOR: {
      return parse_iterator_only(context, name);
    } break;

    default: {
    } break;
  }

  if (not output) {
    throw StyioParseError("Null Return of parse_name_and_following()!");
  }

  return output;
}

TypeAST*
parse_name_as_type_unsafe(StyioContext& context) {
  auto name = context.cur_tok()->original;
  context.move_forward(1, "parse_name_as_type_unsafe");
  return TypeAST::Create(name);
}

IntAST*
parse_int(StyioContext& context) {
  if (context.cur_tok_type() != StyioTokenType::INTEGER) {
    string errmsg = string("parse_int(): False Invoke");
    throw StyioParseError(errmsg);
  }

  auto ret_val = IntAST::Create(context.cur_tok()->original);
  context.move_forward(1, "parse_int");
  return ret_val;
}

FloatAST*
parse_float(StyioContext& context) {
  if (context.cur_tok_type() != StyioTokenType::DECIMAL) {
    string errmsg = string("parse_float(): False Invoke");
    throw StyioParseError(errmsg);
  }

  auto ret_val = FloatAST::Create(context.cur_tok()->original);
  context.move_forward(1, "parse_float");
  return ret_val;
}

StringAST*
parse_string(StyioContext& context) {
  if (context.cur_tok_type() != StyioTokenType::STRING) {
    string errmsg = string("parse_string(): False Invoke");
    throw StyioParseError(errmsg);
  }

  auto ret_val = StringAST::Create(context.cur_tok()->original);
  context.move_forward(1, "parse_string");
  return ret_val;
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
  if (context.check_next('.')) {
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

  while (not context.check_next('\'')) {
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

  while (not context.check_next('\"')) {
    if (context.check_next('{')) {
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
    else if (context.check_next('}')) {
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

  while (not context.check_next('"')) {
    text += context.get_curr_char();
    context.move(1);
  }

  // drop " at the end
  context.move(1);

  if (text.starts_with("/")) {
    return ResPathAST::Create(StyioPathType::local_absolute_unix_like, text);
  }
  else if (std::isupper(text.at(0)) && text.at(1) == ':') {
    return ResPathAST::Create(StyioPathType::local_absolute_windows, text);
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

  return ResPathAST::Create(StyioPathType::local_relevant_any, text);
}

TypeAST*
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

  return TypeAST::Create(text);
}

/*
  Basic Collection
  - typed_var
  - Fill (Variable Tuple)
  - Resources
*/

ParamAST*
parse_argument(StyioContext& context) {
  string namestr = "";
  /* it includes cur_char in the name without checking */
  do {
    namestr += context.get_curr_char();
    context.move(1);
  } while (context.check_isalnum_());

  NameAST* name = NameAST::Create(namestr);
  TypeAST* data_type;
  StyioAST* default_value;

  context.drop_white_spaces();

  if (context.check_drop(':')) {
    context.drop_white_spaces();

    data_type = parse_dtype(context);

    context.drop_white_spaces();

    if (context.check_drop('=')) {
      context.drop_white_spaces();

      default_value = parse_expr(context);

      return ParamAST::Create(name, data_type, default_value);
    }
    else {
      return ParamAST::Create(name, data_type);
    }
  }
  else {
    return ParamAST::Create(name);
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
  std::vector<std::pair<StyioAST*, std::string>> res_list;

  context.match_panic(StyioTokenType::TOK_AT); /* @ */

  if (context.match_panic(StyioTokenType::TOK_LPAREN) /* ( */) {
    do {
      context.skip();
      switch (context.cur_tok_type()) {
        /* ( */
        case StyioTokenType::TOK_RPAREN: {
          context.move_forward(1, "parse_resources");
          return ResourceAST::Create(res_list);
        } break;

        case StyioTokenType::STRING: {
          auto the_str = parse_string(context);

          context.skip();
          if (context.match(StyioTokenType::TOK_COLON) /* : */) {
            context.skip();
            if (context.check(StyioTokenType::NAME) /* check! */) {
              auto the_type_name = parse_name_as_str(context);
              res_list.push_back(
                std::make_pair(the_str, the_type_name)
              );
            }
          }
          else {
            res_list.push_back(
              std::make_pair(the_str, std::string(""))
            );
          }
        } break;

        case StyioTokenType::NAME: {
          auto the_name = parse_name(context);

          context.skip();
          if (context.match(StyioTokenType::ARROW_SINGLE_LEFT)) {
            context.skip();
            if (context.check(StyioTokenType::NAME)) {
              auto the_expr = parse_var_name_or_value_expr(context);

              res_list.push_back(
                std::make_pair(
                  FinalBindAST::Create(
                    VarAST::Create(the_name),
                    the_expr
                  ),
                  std::string("")
                )
              );
            }
          }
          else {
          }

        } break;

        default:
          break;
      }
    } while (context.match(StyioTokenType::TOK_COMMA) /* , */
             || context.match(StyioTokenType::TOK_SEMICOLON) /* ; */);

    context.try_match_panic(StyioTokenType::TOK_RPAREN);

    return ResourceAST::Create(res_list);
  }

  return ResourceAST::Create(res_list);
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

  output = parse_value_expr(context);

  context.drop_all_spaces();

  switch (context.get_curr_char()) {
    case '=': {
      context.move(1);

      if (context.check_next('=')) {
        context.move(1);

        /*
          Equal
            expr == expr
        */

        // drop all spaces after ==
        context.drop_all_spaces();

        output = new BinCompAST(
          CompType::EQ, (output), parse_value_expr(context)
        );
      };
    }

    break;

    case '!': {
      context.move(1);

      if (context.check_next('=')) {
        context.move(1);

        /*
          Not Equal
            expr != expr
        */

        // drop all spaces after !=
        context.drop_all_spaces();

        output = new BinCompAST(
          CompType::NE, (output), parse_value_expr(context)
        );
      };
    }

    break;

    case '>': {
      context.move(1);

      if (context.check_next('=')) {
        context.move(1);

        /*
          Greater Than and Equal
            expr >= expr
        */

        // drop all spaces after >=
        context.drop_all_spaces();

        output = new BinCompAST(
          CompType::GE, (output), parse_value_expr(context)
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
          CompType::GT, (output), parse_value_expr(context)
        );
      };
    }

    break;

    case '<': {
      context.move(1);

      if (context.check_next('=')) {
        context.move(1);

        /*
          Less Than and Equal
            expr <= expr
        */

        // drop all spaces after <=
        context.drop_all_spaces();

        output = new BinCompAST(
          CompType::LE, (output), parse_value_expr(context)
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
          CompType::LT, (output), parse_value_expr(context)
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
parse_var_name_or_value_expr(StyioContext& context) {
  StyioAST* output;

  if (context.check(StyioTokenType::NAME)) {
    auto varname = parse_name(context);
    if (context.check(StyioTokenType::TOK_LBOXBRAC) /* [ */) {
      output = parse_index_op(context, varname);
    }
    else if (context.check(StyioTokenType::TOK_LPAREN) /* ( */) {
      output = parse_index_op(context, varname);
    }
    else {
      output = varname;
    }
  }
  else if (context.check(StyioTokenType::INTEGER)) {
    output = parse_int(context);
  }
  else if (context.check(StyioTokenType::DECIMAL)) {
    output = parse_float(context);
  }
  else if (context.check(StyioTokenType::STRING)) {
    output = parse_string(context);
  }

  return output;
}

StyioAST*
parse_value_expr(StyioContext& context) {
  StyioAST* output;

  if (isalpha(context.get_curr_char()) || context.check_next('_')) {
    return parse_var_name_or_value_expr(context);
  }
  else if (isdigit(context.get_curr_char())) {
    return parse_int_or_float(context);
  }
  else if (context.check_next('|')) {
    return parse_size_of(context);
  }

  string errmsg = string("parse_value() // Unexpected value expression, starting with ") + char(context.get_curr_char());
  throw StyioParseError(errmsg);
}

StyioAST*
parse_binop_item(StyioContext& context) {
  StyioAST* output;

  context.skip();
  switch (context.cur_tok_type()) {
    /* name */
    case StyioTokenType::NAME: {
      output = parse_name(context);
    } break;

    /* 0 */
    case StyioTokenType::INTEGER: {
      output = parse_int(context);
    } break;

    /* "string" */
    case StyioTokenType::STRING: {
      output = parse_string(context);
    } break;

    default: {
    } break;
  }

  return output;
}

StyioAST*
parse_tuple_exprs(StyioContext& context) {
  TupleAST* the_tuple;
  vector<StyioAST*> elems; /* elements */

  context.try_match_panic(StyioTokenType::TOK_LPAREN);

  do {
    context.skip();
    if (context.check(StyioTokenType::TOK_RPAREN) /* ) */) {
      break; /* early stop */
    }

    elems.push_back(parse_expr(context));
  } while (context.try_match(StyioTokenType::TOK_COMMA) /* , */);

  context.try_match_panic(StyioTokenType::TOK_RPAREN); /* ) */

  the_tuple = TupleAST::Create(elems);

  context.skip();

  switch (context.cur_tok_type()) {
    case StyioTokenType::ITERATOR: {
      return parse_iterator_only(context, the_tuple);
    } break;

    default:
      break;
  }

  return the_tuple;
}

StyioAST*
parse_expr(StyioContext& context) {
  StyioAST* output;

  context.skip();
  switch (context.cur_tok_type()) {
    /* name */
    case StyioTokenType::NAME: {
      output = parse_name_and_following_unsafe(context);
    } break;

    /* 0 */
    case StyioTokenType::INTEGER: {
      output = parse_int(context);

      switch (context.cur_tok_type()) {
        case StyioTokenType::TOK_PLUS: {
          context.move_forward(1);
          output = parse_binop_rhs(context, output, StyioOpType::Binary_Add);
        } break;

        case StyioTokenType::TOK_MINUS: {
          context.move_forward(1);
          output = parse_binop_rhs(context, output, StyioOpType::Binary_Sub);
        } break;

        case StyioTokenType::TOK_STAR: {
          context.move_forward(1);
          if (context.check_drop('*')) {
            output = parse_binop_rhs(context, output, StyioOpType::Binary_Pow);
          }
          else {
            output = parse_binop_rhs(context, output, StyioOpType::Binary_Mul);
          }
        } break;

        case StyioTokenType::TOK_SLASH: {
          context.move_forward(1);
          output = parse_binop_rhs(context, output, StyioOpType::Binary_Div);
        } break;

        case StyioTokenType::TOK_PERCENT: {
          context.move_forward(1);
          output = parse_binop_rhs(context, output, StyioOpType::Binary_Mod);
        } break;

        default:
          break;
      }
    } break;

    /* 0.0 */
    case StyioTokenType::DECIMAL: {
      output = parse_float(context);

      switch (context.cur_tok_type()) {
        case StyioTokenType::TOK_PLUS: {
          context.move_forward(1);
          output = parse_binop_rhs(context, output, StyioOpType::Binary_Add);
        } break;

        case StyioTokenType::TOK_MINUS: {
          context.move_forward(1);
          output = parse_binop_rhs(context, output, StyioOpType::Binary_Sub);
        } break;

        case StyioTokenType::TOK_STAR: {
          context.move_forward(1);
          if (context.check_drop('*')) {
            output = parse_binop_rhs(context, output, StyioOpType::Binary_Pow);
          }
          else {
            output = parse_binop_rhs(context, output, StyioOpType::Binary_Mul);
          }
        } break;

        case StyioTokenType::TOK_SLASH: {
          context.move_forward(1);
          output = parse_binop_rhs(context, output, StyioOpType::Binary_Div);
        } break;

        case StyioTokenType::TOK_PERCENT: {
          context.move_forward(1);
          output = parse_binop_rhs(context, output, StyioOpType::Binary_Mod);
        } break;

        default:
          break;
      }
    } break;

    /* "string" */
    case StyioTokenType::STRING: {
      output = parse_string(context);

      switch (context.cur_tok_type()) {
        case StyioTokenType::TOK_PLUS: {
          context.move_forward(1);
          output = parse_binop_rhs(context, output, StyioOpType::Binary_Add);
        } break;

        case StyioTokenType::TOK_MINUS: {
          context.move_forward(1);
          output = parse_binop_rhs(context, output, StyioOpType::Binary_Sub);
        } break;

        case StyioTokenType::TOK_STAR: {
          context.move_forward(1);
          if (context.check_drop('*')) {
            output = parse_binop_rhs(context, output, StyioOpType::Binary_Pow);
          }
          else {
            output = parse_binop_rhs(context, output, StyioOpType::Binary_Mul);
          }
        } break;

        case StyioTokenType::TOK_SLASH: {
          context.move_forward(1);
          output = parse_binop_rhs(context, output, StyioOpType::Binary_Div);
        } break;

        case StyioTokenType::TOK_PERCENT: {
          context.move_forward(1);
          output = parse_binop_rhs(context, output, StyioOpType::Binary_Mod);
        } break;

        default:
          break;
      }
    } break;

    /* ( */
    case StyioTokenType::TOK_LPAREN: {
      return parse_tuple_exprs(context);
    } break;

    /* [ */
    case StyioTokenType::TOK_LBOXBRAC: {
      return parse_list_exprs(context);
    } break;

    default: {
      throw StyioNotImplemented(context.mark_cur_tok("Unknown Expression"));
    } break;
  }

  return output;
}

ReturnAST*
parse_return(
  StyioContext& context
) {
  context.match_panic(StyioTokenType::EXTRACTOR);  // <<
  return ReturnAST::Create(parse_expr(context));
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
parse_list_exprs(StyioContext& context) {
  vector<StyioAST*> exprs;

  context.move_forward(1);

  do {
    context.skip();

    if (context.match(StyioTokenType::TOK_RBOXBRAC) /* ] */) {
      return ListAST::Create(exprs);
    }
    else {
      exprs.push_back(parse_expr(context));
      context.skip();
    }
  } while (context.try_match(StyioTokenType::TOK_COMMA) /* , */);

  context.try_match_panic(StyioTokenType::TOK_RBOXBRAC); /* ] */

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
  vector<ParamAST*> elems;

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

  if (isalpha(context.get_curr_char()) || context.check_next('_')) {
    output = parse_name(context);

    context.drop_all_spaces_comments();

    switch (context.get_curr_char()) {
      case '+': {
        context.move(1);
        output = parse_binop_rhs(context, output, StyioOpType::Binary_Add);
      } break;

      case '-': {
        context.move(1);
        output = parse_binop_rhs(context, output, StyioOpType::Binary_Sub);
      } break;

      case '*': {
        context.move(1);
        if (context.check_drop('*')) {
          output = parse_binop_rhs(context, output, StyioOpType::Binary_Pow);
        }
        else {
          output = parse_binop_rhs(context, output, StyioOpType::Binary_Mul);
        }
      } break;

      case '/': {
        context.move(1);
        output = parse_binop_rhs(context, output, StyioOpType::Binary_Div);
      } break;

      case '%': {
        context.move(1);
        output = parse_binop_rhs(context, output, StyioOpType::Binary_Mod);
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

  if (isalpha(context.get_curr_char()) || context.check_next('_')) {
    StyioAST* var = parse_var_name_or_value_expr(context);

    // eliminate | at the end
    if (context.check_next('|')) {
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

FuncCallAST*
parse_call(
  StyioContext& context,
  NameAST* func_name,
  StyioAST* callee
) {
  context.try_match_panic(StyioTokenType::TOK_LPAREN); /* ( */

  vector<StyioAST*> args;
  while (not context.check(StyioTokenType::TOK_RPAREN) /* ) */) {
    args.push_back(parse_expr(context));
    context.try_match(StyioTokenType::TOK_COMMA); /* , */
    context.skip();
  }

  context.try_match_panic(StyioTokenType::TOK_RPAREN); /* ) */

  if (callee) {
    return FuncCallAST::Create(callee, func_name, args);
  }
  else {
    return FuncCallAST::Create(func_name, args);
  }
}

AttrAST*
parse_attr(
  StyioContext& context
) {
  auto main_name = NameAST::Create(parse_name_as_str(context));

  StyioAST* attr_name;
  if (context.find_drop('.')) {
    attr_name = NameAST::Create(parse_name_as_str(context));
  }
  else if (context.find_drop('[')) {
    /* Object["name"] */
    if (context.check_next('"')) {
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
    std::string curr_token = parse_name_as_str(context);
    context.drop_all_spaces_comments();

    if (context.check_drop('.')) {
      AttrAST* temp = AttrAST::Create(callee, NameAST::Create(curr_token));
      return parse_chain_of_call(context, temp);
    }
    else if (context.check_next('(')) {
      FuncCallAST* temp = parse_call(context, NameAST::Create(curr_token));

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
parse_index_op(StyioContext& context, StyioAST* theList) {
  StyioAST* output;

  /*
    the current character must be [
    this line will drop the next 1 character anyway!
  */
  context.move(1);

  do {
    if (isalpha(context.get_curr_char()) || context.check_next('_')) {
      output = new ListOpAST(
        StyioNodeType::Access, (theList), parse_var_name_or_value_expr(context)
      );
    }
    else if (isdigit(context.get_curr_char())) {
      output = new ListOpAST(
        StyioNodeType::Access_By_Index, (theList), parse_int(context)
      );
    }
    else {
      switch (context.get_curr_char()) {
        /*
          list["any"]
        */
        case '"': {
          output = new ListOpAST(StyioNodeType::Access_By_Name, (theList), parse_string(context));
        }

        // You should NOT reach this line!
        break;

        /*
          list[<]
        */
        case '<': {
          context.move(1);

          while (context.check_next('<')) {
            context.move(1);
          }

          output = new ListOpAST(StyioNodeType::Get_Reversed, (theList));
        }

        // You should NOT reach this line!
        break;

        // list[?= item]
        case '?': {
          context.move(1);

          if (context.check_drop('=')) {
            context.drop_all_spaces_comments();

            output = new ListOpAST(StyioNodeType::Get_Index_By_Value, (theList), parse_expr(context));
          }
          else if (context.check_drop('^')) {
            context.drop_all_spaces_comments();

            output = new ListOpAST(StyioNodeType::Get_Indices_By_Many_Values, (theList), parse_iterable(context));
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

            output = new ListOpAST(StyioNodeType::Insert_Item_By_Index, (theList), (index), parse_expr(context));
          }
          // list[^index]
          else {
            output = new ListOpAST(StyioNodeType::Access_By_Index, (theList), (index));
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
            StyioNodeType::Append_Value, (theList), (expr)
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
              output = new ListOpAST(StyioNodeType::Remove_Item_By_Index, (theList), (parse_int(context)));
            }
            else {
              /*
                list[-: ^(i0, i1, ...)]
              */
              output = new ListOpAST(
                StyioNodeType::Remove_Items_By_Many_Indices,
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

                output = new ListOpAST(StyioNodeType::Remove_Item_By_Value, (theList), parse_expr(context));
              }

              break;

              /*
                list[-: ?^ (v0, v1, ...)]
              */
              case '^': {
                context.move(1);

                context.drop_white_spaces();

                output = new ListOpAST(
                  StyioNodeType::Remove_Items_By_Many_Values,
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
            output = new ListOpAST(StyioNodeType::Remove_Item_By_Value, (theList), parse_expr(context));
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

  if ((iterOverIt->getNodeType()) == StyioNodeType::Infinite) {
    return InfiniteLoopAST::Create();
  }
  else {
    // return new IteratorAST(iterOverIt, parse_forward(context, false));
  }
}

StyioAST*
parse_list_or_loop(StyioContext& context) {
  StyioAST* output;

  vector<StyioAST*> elements;

  StyioAST* startEl = parse_expr(context);

  context.drop_white_spaces();

  if (context.check_drop('.')) {
    while (context.check_next('.')) {
      context.move(1);
    }

    context.drop_white_spaces();

    StyioAST* endEl = parse_expr(context);

    context.drop_white_spaces();

    context.check_drop_panic(']');

    if (startEl->getNodeType() == StyioNodeType::Integer && endEl->getNodeType() == StyioNodeType::Id) {
      output = new InfiniteAST((startEl), (endEl));
    }
    else if (startEl->getNodeType() == StyioNodeType::Integer && endEl->getNodeType() == StyioNodeType::Integer) {
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

  while (context.check_next('[')) {
    output = parse_index_op(context, (output));
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

  while (context.check_next('.')) {
    context.move(1);
  }

  context.find_drop_panic(']');

  context.drop_all_spaces();

  if (context.check_drop(">>")) {
    context.drop_all_spaces();

    return InfiniteLoopAST::Create();
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
  StyioOpType curr_token
) {
  BinOpAST* output;

  context.skip();
  StyioAST* rhs_ast = parse_binop_item(context);

  context.drop_all_spaces_comments();

  StyioOpType next_token;
  switch (context.cur_tok_type()) {
    /* + */
    case StyioTokenType::TOK_PLUS: {
      context.move_forward(1, "parse_binop_rhs(TOK_PLUS)");
      next_token = StyioOpType::Binary_Add;
    } break;

    /* - */
    case StyioTokenType::TOK_MINUS: {
      context.move_forward(1, "parse_binop_rhs(TOK_MINUS)");
      next_token = StyioOpType::Binary_Sub;
    } break;

    /* * */
    case StyioTokenType::TOK_STAR: {
      context.move_forward(1, "parse_binop_rhs(TOK_STAR)");
      next_token = StyioOpType::Binary_Mul;
    } break;

    /* ** */
    case StyioTokenType::BINOP_POW: {
      context.move_forward(1, "parse_binop_rhs(BINOP_POW)");
      next_token = StyioOpType::Binary_Pow;
    } break;

    /* / */
    case StyioTokenType::TOK_SLASH: {
      context.move_forward(1, "parse_binop_rhs(TOK_SLASH)");
      next_token = StyioOpType::Binary_Div;
    } break;

    /* % */
    case StyioTokenType::TOK_PERCENT: {
      context.move_forward(1, "parse_binop_rhs(TOK_PERCENT)");
      next_token = StyioOpType::Binary_Mod;
    } break;

    default: {
      return BinOpAST::Create(curr_token, lhs_ast, rhs_ast);
    } break;
  }

  if (next_token > curr_token) {
    output = BinOpAST::Create(
      curr_token,
      lhs_ast,
      parse_binop_rhs(
        context,
        rhs_ast,
        next_token
      )
    );
  }
  else {
    output = parse_binop_rhs(
      context,
      BinOpAST::Create(
        curr_token,
        lhs_ast,
        rhs_ast
      ),
      next_token
    );
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
      context.check_drop_panic('&');

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
      context.check_drop_panic('|');

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
        LogicType::XOR, (lhsExpr), parse_cond(context)
      );
    }

    break;

    case '!': {
      context.move(1);

      if (context.check_next('(')) {
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

  while (!(context.check_next(')'))) {
    condExpr = (parse_cond_rhs(context, (condExpr)));
  }

  return condExpr;
}

CondAST*
parse_cond(StyioContext& context) {
  StyioAST* lhsExpr;

  context.drop_all_spaces_comments();

  if (context.check_drop('(')) {
    lhsExpr = parse_cond(context);
    context.find_drop_panic(')');
  }
  else if (context.check_drop('!')) {
    context.drop_all_spaces_comments();
    if (context.check_drop('(')) {
      /*
        support:
          !( \n
            expr
          )
      */
      context.drop_all_spaces();

      lhsExpr = parse_cond(context);

      context.drop_all_spaces();

      return new CondAST(LogicType::NOT, lhsExpr);
    }
    else {
      string errmsg = string("!(expr) // Expecting ( after !, but got ") + char(context.get_curr_char());
      throw StyioSyntaxError(errmsg);
    }
  }
  else {
    lhsExpr = parse_cond_item(context);
  }

  // drop all spaces after first value
  context.drop_all_spaces();

  if (context.check_next("&&") || context.check_next("||")) {
    return parse_cond_rhs(context, lhsExpr);
  }
  else {
    return new CondAST(LogicType::RAW, lhsExpr);
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

        block = parse_block_only(context);

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

          StyioAST* blockElse = parse_block_only(context);

          return new CondFlowAST(StyioNodeType::CondFlow_Both, (condition), (block), (blockElse));
        }
        else {
          return new CondFlowAST(StyioNodeType::CondFlow_True, (condition), (block));
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

        block = parse_block_only(context);

        return new CondFlowAST(StyioNodeType::CondFlow_False, (condition), (block));
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
parse_hash_tag(StyioContext& context) {
  context.match_panic(StyioTokenType::TOK_HASH); /* # */

  NameAST* tag_name = nullptr;
  std::vector<ParamAST*> params;
  std::variant<TypeAST*, TypeTupleAST*> ret_type;
  StyioAST* ret_expr;

  /* TAG NAME */
  context.skip();
  if (context.check(StyioTokenType::NAME)) {
    tag_name = parse_name_unsafe(context);
  }

  params = parse_params(context);

  context.skip();
  if (context.match(StyioTokenType::TOK_COLON) /* : */) {
    context.skip();
    if (context.check(StyioTokenType::NAME)) {
      auto type_name = parse_name_as_str_unsafe(context);
      ret_type = TypeAST::Create(type_name);
    }
    else if (context.match(StyioTokenType::TOK_LPAREN) /* ( */) {
      std::vector<TypeAST*> types;
      do {
        context.skip();
        if (context.check(StyioTokenType::NAME)) {
          TypeAST* type_name = parse_name_as_type_unsafe(context);
          types.push_back(type_name);
        }
      } while (context.try_match(StyioTokenType::TOK_COMMA) /* , */);

      context.try_match_panic(StyioTokenType::TOK_RPAREN); /* ) */

      ret_type = TypeTupleAST::Create(types);
    }
  }

  context.skip();
  /* Block */
  if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
    ret_expr = parse_block_with_forward(context);
    return FunctionAST::Create(tag_name, false, params, ret_type, ret_expr);
  }
  /* Block or Statement */
  else if (context.match(StyioTokenType::ARROW_DOUBLE_RIGHT) /* => */) {
    context.skip();
    /* Block */
    if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
      ret_expr = parse_block_with_forward(context);
      return FunctionAST::Create(tag_name, false, params, ret_type, ret_expr);
    }
    /* Statement */
    else {
      ret_expr = parse_stmt_or_expr(context);
      return SimpleFuncAST::Create(tag_name, params, ret_type, ret_expr);
    }
  }
  /* SimpleFunc */
  else if (context.match(StyioTokenType::TOK_EQUAL) /* = */) {
    context.skip();
    ret_expr = parse_expr(context);

    return SimpleFuncAST::Create(tag_name, false, params, ret_type, ret_expr);
  }
  /* SimpleFunc (Unique) */
  else if (context.match(StyioTokenType::WALRUS) /* := */) {
    context.skip();
    ret_expr = parse_expr(context);

    return SimpleFuncAST::Create(tag_name, true, params, ret_expr);
  }
  /* Match Cases */
  else if (context.match(StyioTokenType::MATCH) /* ?= */) {
    if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
      return FunctionAST::Create(tag_name, true, params, ret_type, parse_cases_only(context));
    }
    else {
      std::vector<StyioAST*> rvals;

      do {
        rvals.push_back(parse_expr(context));
      } while (context.try_match(StyioTokenType::TOK_COMMA) /* , */);

      return FunctionAST::Create(tag_name, true, params, ret_type, CheckEqualAST::Create(rvals));
    }
  }
  /* Iterator */
  else if (context.check(StyioTokenType::ITERATOR) /* >> */) {
    if (params.size() != 1) {
      throw StyioSyntaxError(context.mark_cur_tok("Confusing: The iterator (>>) can not be applied to multiple objects."));
    }

    ret_expr = parse_iterator_with_forward(context, params[0]);

    return FunctionAST::Create(tag_name, true, params, ret_type, ret_expr);
  }

  throw StyioParseError(context.mark_cur_tok("Reached the End of parse_hash_tag."));
}

std::vector<ParamAST*>
parse_params(StyioContext& context) {
  std::vector<ParamAST*> params;

  context.try_match(StyioTokenType::TOK_HASH); /* # */

  context.try_match(StyioTokenType::TOK_LPAREN); /* ( */

  do {
    context.skip();
    if (context.check(StyioTokenType::NAME)) {
      NameAST* var_name = parse_name(context);

      context.skip();
      if (context.match(StyioTokenType::TOK_COLON) /* : */) {
        context.skip();
        auto var_type = parse_name_as_str(context);

        params.push_back(ParamAST::Create(
          var_name,
          TypeAST::Create(var_type)
        ));
      }
      else {
        params.push_back(ParamAST::Create(var_name));
      }
    }
  } while (context.try_match(StyioTokenType::TOK_COMMA) /* , */);

  /* ) */
  context.try_match(StyioTokenType::TOK_RPAREN);

  return params;
}

std::vector<StyioAST*>
parse_forward_as_list(
  StyioContext& context
) {
  std::vector<StyioAST*> following_exprs;

  while (true) {
    context.skip();
    switch (context.cur_tok_type()) {
      /* => Block or Statement */
      case StyioTokenType::ARROW_DOUBLE_RIGHT: {
        context.move_forward(1);

        context.skip();
        if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
          following_exprs.push_back(parse_block_only(context));
        }
        else {
          following_exprs.push_back(parse_stmt_or_expr(context));
        }
      } break;

      /* ? Conditionals */
      case StyioTokenType::TOK_QUEST: {
        throw StyioNotImplemented("parse_forward(Conditionals)");
      } break;

      /* ?= Match Cases */
      case StyioTokenType::MATCH: {
        context.move_forward(1);

        context.skip();
        /* { _ => ... } Cases */
        if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
          following_exprs.push_back(parse_cases_only(context));
        }
        else {
          std::vector<StyioAST*> rvals;

          do {
            rvals.push_back(parse_expr(context));
          } while (context.try_match(StyioTokenType::TOK_COMMA) /* , */);

          following_exprs.push_back(CheckEqualAST::Create(rvals));
        }
      } break;

      /* >> Iterator */
      case StyioTokenType::ITERATOR: {
      } break;

      default: {
        return following_exprs;
      } break;
    }
  }

  return following_exprs;
}

BlockAST*
parse_block_with_forward(StyioContext& context) {
  BlockAST* block = parse_block_only(context);

  block->followings = parse_forward_as_list(context);

  return block;
}

CasesAST*
parse_cases_only(StyioContext& context) {
  vector<std::pair<StyioAST*, StyioAST*>> case_pairs;
  StyioAST* default_stmt;

  context.try_match_panic(StyioTokenType::TOK_LCURBRAC); /* { */

  while (not context.match(StyioTokenType::TOK_RCURBRAC) /* } */) {
    context.skip();
    if (context.match(StyioTokenType::TOK_UNDLINE) /* _ */) {
      context.skip();
      if (context.match(StyioTokenType::ARROW_DOUBLE_RIGHT) /* => */) {
        context.skip();
        if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
          default_stmt = parse_block_only(context);
        }
        else {
          default_stmt = parse_stmt_or_expr(context);
        }
      }
      else {
        // SyntaxError
        throw StyioSyntaxError("=> not found for default case");
      }
    }
    else {
      // StyioAST* left = parse_cond(context);
      StyioAST* left = parse_expr(context);

      context.skip();
      if (context.match(StyioTokenType::ARROW_DOUBLE_RIGHT) /* => */) {
        StyioAST* right;

        context.skip();
        if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
          right = parse_block_only(context);
        }
        else {
          right = parse_stmt_or_expr(context);
        }

        case_pairs.push_back(std::make_pair(left, right));
      }
      else {
        // SyntaxError
        throw StyioSyntaxError(context.mark_cur_tok("`=>` not found"));
      }
    }

    context.skip();
  }

  // for (size_t i = 0; i < case_pairs.size(); i++) {
  //   std::cout << "First: " << std::endl;
  //   context.show_ast(case_pairs[i].first);
  //   std::cout << "Second" << std::endl;
  //   context.show_ast(case_pairs[i].second);
  // }

  if (case_pairs.size() == 0) {
    return CasesAST::Create(default_stmt);
  }
  else {
    return CasesAST::Create(case_pairs, default_stmt);
  }
}

IteratorAST*
parse_iterator_with_forward(
  StyioContext& context,
  StyioAST* collection
) {
  IteratorAST* output = parse_iterator_only(context, collection);

  auto forward_following = parse_forward_as_list(context);

  output->following.insert(
    output->following.end(),
    std::make_move_iterator(forward_following.begin()),
    std::make_move_iterator(forward_following.end())
  );

  return output;
}

IteratorAST*
parse_iterator_only(
  StyioContext& context,
  StyioAST* collection
) {
  // Guard: Check >>
  context.try_match_panic(StyioTokenType::ITERATOR);

  std::vector<ParamAST*> params;
  BlockAST* block;

  context.skip();

  /* Iterator Sequence (Early Stop) */
  if (context.match(StyioTokenType::TOK_HASH) /* # */) {
    std::vector<HashTagNameAST*> hash_tags;

    context.skip();
    if (context.check(StyioTokenType::NAME)) {
      hash_tags.push_back(HashTagNameAST::Create(parse_name_with_spaces_unsafe(context)));
    }

    while (context.try_match(StyioTokenType::TOK_RANGBRAC) /* > */) {
      if (context.try_match(StyioTokenType::TOK_HASH) /* # */) {
        context.skip();
        if (context.check(StyioTokenType::NAME)) {
          hash_tags.push_back(HashTagNameAST::Create(parse_name_with_spaces_unsafe(context)));
        }
        else {
          throw StyioSyntaxError(context.mark_cur_tok("What the hell after this hash tag?"));
        }
      }
      else {
        throw StyioSyntaxError(context.mark_cur_tok("Iterator sequence only support hash tags."));
      }
    }

    return IterSeqAST::Create(collection, hash_tags);
  }
  else if (context.cur_tok_type() == StyioTokenType::TOK_LPAREN /* ( */
           || context.cur_tok_type() == StyioTokenType::NAME /* param */) {
    params = parse_params(context);
  }

  context.skip();

  if (context.try_match(StyioTokenType::ARROW_DOUBLE_RIGHT) /* => */) {
    if (context.try_match(StyioTokenType::TOK_LCURBRAC) /* { */) {
      return IteratorAST::Create(collection, params, parse_block_only(context));
    }
    else {
      return IteratorAST::Create(collection, params, parse_stmt_or_expr(context));
    }
  }
  else if (context.try_match(StyioTokenType::TOK_LCURBRAC) /* { */) {
    return IteratorAST::Create(collection, params, parse_block_only(context));
  }
  else if (context.try_match(StyioTokenType::TOK_RANGBRAC) /* > */) {
    std::vector<HashTagNameAST*> hash_tags;

    do {
      if (context.try_match(StyioTokenType::TOK_HASH) /* # */) {
        context.skip();
        if (context.check(StyioTokenType::NAME)) {
          hash_tags.push_back(HashTagNameAST::Create(parse_name_with_spaces_unsafe(context)));
        }
        else {
          throw StyioSyntaxError(context.mark_cur_tok("What the hell after this hash tag?"));
        }
      }
      else {
        throw StyioSyntaxError(context.mark_cur_tok("Iterator sequence only support hash tags."));
      }
    } while (context.try_match(StyioTokenType::TOK_RANGBRAC) /* > */);

    return IterSeqAST::Create(collection, params, hash_tags);
  }

  return IteratorAST::Create(collection, params);
}

/*
  BackwardAST
  - Filling: (a, b, ...) << EXPR
    EXPR should return an special type (not decided yet),
    where
      1. the returned values and the tuple should be the same length (
          probably, the returned values can be
        )
      2. the type (if declared) should be the same
  - Import:
    a, b <- @("./ra.txt"), @("./rb.txt")
*/
BackwardAST*
parse_backward(StyioContext& context, bool is_func) {
  BackwardAST* output;

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
    // Exception: Tuple Operation Not Found (unacceptable in this function.)
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

  string name = parse_name_as_str(context);

  context.find_drop_panic('{');

  vector<StyioAST*> op_args;

  if (name == "filter") {
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

    if (context.check_next('"')) {
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
  vector<StyioAST*> exprs;

  context.match_panic(StyioTokenType::PRINT);  // >_

  context.try_match_panic(StyioTokenType::TOK_LPAREN);  // (

  do {
    context.skip();

    if (context.match(StyioTokenType::TOK_RPAREN) /* ) */) {
      return PrintAST::Create(exprs);
    }
    else {
      exprs.push_back(parse_expr(context));
    }
  } while (context.try_match(StyioTokenType::TOK_COMMA) /* , */);

  context.try_match_panic(StyioTokenType::TOK_RPAREN); /* ) */

  return PrintAST::Create(exprs);
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
parse_stmt_or_expr(
  StyioContext& context
) {
  context.skip();

  switch (context.cur_tok_type()) {
    /* var_name / func_name */
    case StyioTokenType::NAME: {
      return parse_name_and_following_unsafe(context);
    } break;

    /* int */
    case StyioTokenType::INTEGER: {
      return parse_int(context);
    } break;

    /* float */
    case StyioTokenType::DECIMAL: {
      return parse_float(context);
    } break;

    /* @ */
    case StyioTokenType::TOK_AT: {
      return parse_resources(context);
    } break;

    /* # */
    case StyioTokenType::TOK_HASH: {
      return parse_hash_tag(context);
    } break;

    /* >_ */
    case StyioTokenType::PRINT: {
      return parse_print(context);
    } break;

    /* ( */
    case StyioTokenType::TOK_LPAREN: {
      return parse_tuple_exprs(context);
    } break;

    /* ... */
    case StyioTokenType::ELLIPSIS: {
      context.move_forward(1, "parse_stmt_or_expr");
      return PassAST::Create();
    } break;

    /* << */
    case StyioTokenType::EXTRACTOR: {
      return parse_return(context);
    } break;

    case StyioTokenType::TOK_EOF: {
      return EOFAST::Create();
    } break;

    default: {
      throw StyioSyntaxError(context.mark_cur_tok("No Statement Starts With This"));
    } break;
  }

  throw StyioParseError(context.mark_cur_tok("Reached The End of `parse_stmt_or_expr()`"));
}

BlockAST*
parse_block_only(StyioContext& context) {
  vector<StyioAST*> stmts;

  context.match_panic(StyioTokenType::TOK_LCURBRAC); /* { */

  while (
    context.cur_tok_type() != StyioTokenType::TOK_EOF
  ) {
    context.skip();
    if (context.match(StyioTokenType::TOK_RCURBRAC) /* } */) {
      return BlockAST::Create(std::move(stmts));
    }
    else {
      stmts.push_back(parse_stmt_or_expr(context));
    }
  }

  context.try_match_panic(StyioTokenType::TOK_RCURBRAC); /* } */

  return BlockAST::Create(std::move(stmts));
}

MainBlockAST*
parse_main_block(StyioContext& context) {
  vector<StyioAST*> statements;

  while (true) {
    StyioAST* stmt = parse_stmt_or_expr(context);

    if ((stmt->getNodeType()) == StyioNodeType::End) {
      break;
    }
    else if ((stmt->getNodeType()) == StyioNodeType::Comment) {
      continue;
    }
    else {
      statements.push_back(stmt);
    }
  }

  return MainBlockAST::Create(statements);
}