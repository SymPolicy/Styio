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
#include "NewParserExpr.hpp"
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

static StyioAST* parse_token_index_suffix(StyioContext& context, StyioAST* base);
static StyioAST* parse_resource_file_atom(StyioContext& context);
static StyioAST* parse_state_decl_after_at(StyioContext& context);
static StyioAST* parse_state_ref_suffix(StyioContext& context, StateRefAST* sr);
static TypeAST* parse_styio_type(StyioContext& context);

StyioAST*
parse_name_and_following_unsafe(StyioContext& context) {
  auto name = NameAST::Create(context.cur_tok()->original);
  context.move_forward(1);

  StyioAST* output = name;

  context.skip_spaces_no_linebreak();
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

    /* M4: x[?, c], x[?=, v], x[i] */
    case StyioTokenType::TOK_LBOXBRAC: {
      output = parse_token_index_suffix(context, name);
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
  Extensible type parser: scalar names (f64, i64, …) + Topology v2 [|n|].
  Future: tuple types, @resource refs, element type parameters — branch here, not in stmt parsing.
*/
static TypeAST*
parse_styio_type(StyioContext& context) {
  context.skip();
  if (context.check(StyioTokenType::BOUNDED_BUFFER_OPEN)) {
    context.move_forward(1, "parse_styio_type[|");
    context.skip();
    if (context.cur_tok_type() != StyioTokenType::INTEGER) {
      throw StyioSyntaxError(
        context.mark_cur_tok("expected integer capacity in [|n|] type"));
    }
    std::string cap = context.cur_tok()->original;
    context.move_forward(1, "parse_styio_type cap");
    context.skip();
    context.try_match_panic(StyioTokenType::BOUNDED_BUFFER_CLOSE);
    return TypeAST::CreateBoundedRingBuffer(cap);
  }
  return TypeAST::Create(parse_name_as_str_unsafe(context));
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

    data_type = parse_styio_type(context);

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

static ResourceAST*
parse_resources_list_in_parens(StyioContext& context) {
  std::vector<std::pair<StyioAST*, std::string>> res_list;

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

ResourceAST*
parse_resources_after_at(StyioContext& context) {
  context.skip();
  if (context.match_panic(StyioTokenType::TOK_LPAREN) /* ( */) {
    return parse_resources_list_in_parens(context);
  }
  return ResourceAST::Create(std::vector<std::pair<StyioAST*, std::string>>());
}

ResourceAST*
parse_resources(
  StyioContext& context
) {
  context.match_panic(StyioTokenType::TOK_AT); /* @ */
  return parse_resources_after_at(context);
}

static StyioAST*
parse_braced_string_path(StyioContext& context) {
  context.try_match_panic(StyioTokenType::TOK_LCURBRAC);
  context.skip();
  if (not context.check(StyioTokenType::STRING)) {
    throw StyioSyntaxError(context.mark_cur_tok("expected string path in {...}"));
  }
  StyioAST* p = parse_string(context);
  context.skip();
  context.try_match_panic(StyioTokenType::TOK_RCURBRAC);
  return p;
}

static StyioAST*
parse_after_at_common(StyioContext& context, bool file_only_resource) {
  context.skip();
  if (context.check(StyioTokenType::NAME) && context.cur_tok()->original == "file") {
    context.move_forward(1, "@file");
    context.skip();
    StyioAST* path = parse_braced_string_path(context);
    return FileResourceAST::Create(path, false);
  }
  if (context.check(StyioTokenType::TOK_LCURBRAC)) {
    StyioAST* path = parse_braced_string_path(context);
    return FileResourceAST::Create(path, true);
  }
  if (not file_only_resource && context.check(StyioTokenType::TOK_LPAREN)) {
    return parse_resources_after_at(context);
  }
  return UndefinedLitAST::Create();
}

/* M6 state syntax: @[n](name = expr) / @[acc = i](name = expr) / @[name] << @file{...}
 * Target design (docs/Styio-Resource-Topology.md): @name : [|n|] := { driver } at top level,
 * expr -> $name for writes. Not implemented here — requires new tokens [| |] and grammar. */
static StyioAST*
parse_state_decl_after_at(StyioContext& context) {
  context.try_match_panic(StyioTokenType::TOK_LBOXBRAC);
  context.skip();
  if (context.check(StyioTokenType::INTEGER)) {
    IntAST* win = parse_int(context);
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_LPAREN);
    context.skip();
    if (not context.check(StyioTokenType::NAME)) {
      throw StyioSyntaxError(context.mark_cur_tok("expected (export = expr) in state decl"));
    }
    NameAST* expn = parse_name_unsafe(context);
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_EQUAL);
    context.skip();
    StyioAST* rhs = parse_expr(context);
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RPAREN);
    return StateDeclAST::Create(win, nullptr, nullptr, VarAST::Create(expn), rhs);
  }
  if (not context.check(StyioTokenType::NAME)) {
    throw StyioSyntaxError(context.mark_cur_tok("expected window size or acc = init in @[...]"));
  }
  NameAST* n0 = parse_name_unsafe(context);
  context.skip();
  if (context.try_match(StyioTokenType::TOK_EQUAL)) {
    context.skip();
    StyioAST* acc_init = parse_expr(context);
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_LPAREN);
    context.skip();
    if (not context.check(StyioTokenType::NAME)) {
      throw StyioSyntaxError(context.mark_cur_tok("expected (export = expr) in state decl"));
    }
    NameAST* expn = parse_name_unsafe(context);
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_EQUAL);
    context.skip();
    StyioAST* rhs = parse_expr(context);
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RPAREN);
    return StateDeclAST::Create(nullptr, n0, acc_init, VarAST::Create(expn), rhs);
  }
  if (context.try_match(StyioTokenType::TOK_RBOXBRAC)) {
    context.skip();
    context.try_match_panic(StyioTokenType::EXTRACTOR);
    context.skip();
    StyioAST* ratom = parse_resource_file_atom(context);
    auto* fr = dynamic_cast<FileResourceAST*>(ratom);
    if (fr == nullptr) {
      throw StyioSyntaxError(context.mark_cur_tok("snapshot pull needs @file{...} or @{...}"));
    }
    return SnapshotDeclAST::Create(n0, fr);
  }
  throw StyioSyntaxError(context.mark_cur_tok("expected = or ] after name in @[...]"));
}

static StyioAST*
parse_resource_file_atom(StyioContext& context) {
  context.skip();
  context.match_panic(StyioTokenType::TOK_AT);
  context.skip();
  StyioAST* r = parse_after_at_common(context, true);
  if (r->getNodeType() != StyioNodeType::FileResource) {
    throw StyioSyntaxError(context.mark_cur_tok("expected @file{...} or @{...}"));
  }
  return r;
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

  if (StyioUnicode::is_identifier_start(context.get_curr_char())) {
    return parse_var_name_or_value_expr(context);
  }
  else if (StyioUnicode::is_digit(context.get_curr_char())) {
    return parse_int_or_float(context);
  }
  else if (context.check_next('|')) {
    return parse_size_of(context);
  }

  string errmsg = string("parse_value() // Unexpected value expression, starting with ") + char(context.get_curr_char());
  throw StyioParseError(errmsg);
}

StyioAST* parse_tuple_exprs(StyioContext& context);
StyioAST* parse_list_exprs(StyioContext& context);

static StyioAST* parse_or_expr(StyioContext& context);

static StyioAST* parse_fallback_expr(StyioContext& context);

static StyioAST* parse_wave_dispatch_arm(StyioContext& context);

static StyioAST*
parse_arithmetic_tail_from_atom(StyioContext& context, StyioAST* output) {
  context.skip();
  switch (context.cur_tok_type()) {
    case StyioTokenType::TOK_PLUS: {
      context.move_forward(1, "arith_tail(+)");
      output = parse_binop_rhs(context, output, StyioOpType::Binary_Add);
    } break;

    case StyioTokenType::TOK_MINUS: {
      context.move_forward(1, "arith_tail(-)");
      output = parse_binop_rhs(context, output, StyioOpType::Binary_Sub);
    } break;

    case StyioTokenType::TOK_STAR: {
      context.move_forward(1, "arith_tail(*)");
      output = parse_binop_rhs(context, output, StyioOpType::Binary_Mul);
    } break;

    case StyioTokenType::BINOP_POW: {
      context.move_forward(1, "arith_tail(POW)");
      output = parse_binop_rhs(context, output, StyioOpType::Binary_Pow);
    } break;

    case StyioTokenType::TOK_SLASH: {
      context.move_forward(1, "arith_tail(/)");
      output = parse_binop_rhs(context, output, StyioOpType::Binary_Div);
    } break;

    case StyioTokenType::TOK_PERCENT: {
      context.move_forward(1, "arith_tail(%)");
      output = parse_binop_rhs(context, output, StyioOpType::Binary_Mod);
    } break;

    case StyioTokenType::WAVE_LEFT: {
      context.move_forward(1, "arith_tail(<~)");
      context.skip();
      StyioAST* true_val = parse_or_expr(context);
      context.skip();
      if (not context.try_match(StyioTokenType::TOK_PIPE)) {
        throw StyioSyntaxError(context.mark_cur_tok("Expected | after <~ true arm"));
      }
      context.skip();
      StyioAST* false_val = parse_or_expr(context);
      output = WaveMergeAST::Create(output, true_val, false_val);
      return parse_arithmetic_tail_from_atom(context, output);
    } break;

    case StyioTokenType::WAVE_RIGHT: {
      context.move_forward(1, "arith_tail(~>)");
      context.skip();
      StyioAST* true_arm = parse_wave_dispatch_arm(context);
      context.skip();
      if (not context.try_match(StyioTokenType::TOK_PIPE)) {
        throw StyioSyntaxError(context.mark_cur_tok("Expected | after ~> true arm"));
      }
      context.skip();
      StyioAST* false_arm = parse_wave_dispatch_arm(context);
      output = WaveDispatchAST::Create(output, true_arm, false_arm);
      return parse_arithmetic_tail_from_atom(context, output);
    } break;

    case StyioTokenType::TOK_LBOXBRAC: {
      /* Do not treat `[` after a bare literal / bool as indexing — that would glue
         `result = true` to the next line's `[1,2,3] >> ...` and break parsing. */
      if (output && (output->getNodeType() == StyioNodeType::Integer
                     || output->getNodeType() == StyioNodeType::Float
                     || output->getNodeType() == StyioNodeType::Bool
                     || output->getNodeType() == StyioNodeType::String)) {
        break;
      }
      output = parse_token_index_suffix(context, output);
      return parse_arithmetic_tail_from_atom(context, output);
    } break;

    default:
      break;
  }

  return output;
}

static StyioAST*
parse_state_ref_suffix(StyioContext& context, StateRefAST* sr) {
  context.skip();
  if (context.check(StyioTokenType::TOK_LBOXBRAC)) {
    return parse_token_index_suffix(context, sr);
  }
  return sr;
}

static StyioAST*
parse_arithmetic_expr(StyioContext& context) {
  context.skip();
  switch (context.cur_tok_type()) {
    case StyioTokenType::TOK_MINUS: {
      context.move_forward(1, "arith_unary-");
      context.skip();
      StyioAST* inner = parse_arithmetic_expr(context);
      return parse_arithmetic_tail_from_atom(
        context,
        BinOpAST::Create(StyioOpType::Binary_Sub, IntAST::Create("0"), inner));
    } break;

    case StyioTokenType::TOK_PLUS: {
      context.move_forward(1, "arith_unary+");
      context.skip();
      return parse_arithmetic_tail_from_atom(context, parse_arithmetic_expr(context));
    } break;

    case StyioTokenType::TOK_DOLLAR: {
      context.move_forward(1, "arith_$");
      context.skip();
      if (not context.check(StyioTokenType::NAME)) {
        throw StyioSyntaxError(context.mark_cur_tok("expected name after $"));
      }
      auto* sr = StateRefAST::Create(parse_name_unsafe(context));
      return parse_arithmetic_tail_from_atom(context, parse_state_ref_suffix(context, sr));
    } break;

    case StyioTokenType::NAME: {
      const std::string& id = context.cur_tok()->original;
      if (id == "true" || id == "false") {
        context.move_forward(1, "bool_lit");
        return BoolAST::Create(id == "true");
      }
      return parse_name_and_following_unsafe(context);
    } break;

    case StyioTokenType::INTEGER: {
      StyioAST* output = parse_int(context);
      return parse_arithmetic_tail_from_atom(context, output);
    } break;

    case StyioTokenType::DECIMAL: {
      StyioAST* output = parse_float(context);
      return parse_arithmetic_tail_from_atom(context, output);
    } break;

    case StyioTokenType::STRING: {
      StyioAST* output = parse_string(context);
      return parse_arithmetic_tail_from_atom(context, output);
    } break;

    /* Optional spellings: ?(cond) <~ t | f — same AST as (cond) <~ t | f (highlights wave condition). */
    case StyioTokenType::TOK_QUEST: {
      context.move_forward(1, "paren_cond?");
      context.skip();
      if (not context.check(StyioTokenType::TOK_LPAREN)) {
        throw StyioSyntaxError(context.mark_cur_tok(
          "Expected ( after ? — use ?(expr) for a parenthesized subexpression (e.g. wave merge condition)"));
      }
      return parse_tuple_exprs(context);
    } break;

    case StyioTokenType::TOK_LPAREN: {
      return parse_tuple_exprs(context);
    } break;

    case StyioTokenType::TOK_LBOXBRAC: {
      return parse_list_exprs(context);
    } break;

    case StyioTokenType::TOK_AT: {
      context.move_forward(1, "@expr");
      context.skip();
      return parse_arithmetic_tail_from_atom(
        context,
        parse_after_at_common(context, false));
    } break;

    case StyioTokenType::YIELD_PIPE: {
      context.move_forward(1, "yield_pipe_expr");
      context.skip();
      return ReturnAST::Create(parse_fallback_expr(context));
    } break;

    default: {
      throw StyioNotImplemented(context.mark_cur_tok("Unknown Expression"));
    } break;
  }
}

static StyioAST*
parse_relational_expr(StyioContext& context) {
  StyioAST* lhs = parse_arithmetic_expr(context);

  while (true) {
    context.skip();
    CompType ct = CompType::EQ;
    bool have = false;

    switch (context.cur_tok_type()) {
      case StyioTokenType::BINOP_EQ: {
        ct = CompType::EQ;
        have = true;
      } break;
      case StyioTokenType::BINOP_NE: {
        ct = CompType::NE;
        have = true;
      } break;
      case StyioTokenType::TOK_RANGBRAC: {
        ct = CompType::GT;
        have = true;
      } break;
      case StyioTokenType::BINOP_GE: {
        ct = CompType::GE;
        have = true;
      } break;
      case StyioTokenType::TOK_LANGBRAC: {
        ct = CompType::LT;
        have = true;
      } break;
      case StyioTokenType::BINOP_LE: {
        ct = CompType::LE;
        have = true;
      } break;
      default:
        break;
    }

    if (not have) {
      return lhs;
    }

    context.move_forward(1, "rel_op");
    StyioAST* rhs = parse_arithmetic_expr(context);
    lhs = new BinCompAST(ct, lhs, rhs);
  }
}

static StyioAST*
parse_and_expr(StyioContext& context) {
  StyioAST* lhs = parse_relational_expr(context);

  while (true) {
    context.skip();
    if (not context.check(StyioTokenType::LOGIC_AND)) {
      return lhs;
    }
    context.move_forward(1, "&&");
    StyioAST* rhs = parse_relational_expr(context);
    lhs = CondAST::Create(LogicType::AND, lhs, rhs);
  }
}

static StyioAST*
parse_or_expr(StyioContext& context) {
  StyioAST* lhs = parse_and_expr(context);

  while (true) {
    context.skip();
    if (not context.check(StyioTokenType::LOGIC_OR)) {
      return lhs;
    }
    context.move_forward(1, "||");
    StyioAST* rhs = parse_and_expr(context);
    lhs = CondAST::Create(LogicType::OR, lhs, rhs);
  }
}

static StyioAST*
parse_fallback_expr(StyioContext& context) {
  StyioAST* lhs = parse_or_expr(context);
  while (true) {
    context.skip();
    if (not context.check(StyioTokenType::TOK_PIPE)) {
      return lhs;
    }
    context.move_forward(1, "fallback|");
    context.skip();
    StyioAST* rhs = parse_or_expr(context);
    lhs = FallbackAST::Create(lhs, rhs);
  }
}

static StyioAST*
parse_wave_dispatch_arm(StyioContext& context) {
  context.skip();
  switch (context.cur_tok_type()) {
    case StyioTokenType::TOK_AT: {
      context.move_forward(1, "dispatch_arm@");
      return UndefinedLitAST::Create();
    }
    case StyioTokenType::PRINT: {
      return parse_print(context);
    }
    case StyioTokenType::NAME: {
      return parse_name_and_following_unsafe(context);
    }
    case StyioTokenType::TOK_LPAREN: {
      return parse_tuple_exprs(context);
    }
    case StyioTokenType::TOK_LCURBRAC: {
      return parse_block_only(context);
    }
    default: {
      throw StyioSyntaxError(
        context.mark_cur_tok("Expected @, >_(...), name, (expr), or { block } after ~>"));
    }
  }
}

static StyioAST*
parse_token_index_suffix(StyioContext& context, StyioAST* base) {
  context.try_match_panic(StyioTokenType::TOK_LBOXBRAC);
  context.skip();

  if (auto* sr = dynamic_cast<StateRefAST*>(base)) {
    if (context.check(StyioTokenType::EXTRACTOR)) {
      context.move_forward(1, "state_hist[<<");
      context.skip();
      context.try_match_panic(StyioTokenType::TOK_COMMA);
      context.skip();
      StyioAST* dep = parse_fallback_expr(context);
      context.skip();
      context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
      return HistoryProbeAST::Create(sr, dep);
    }
  }

  if (context.check(StyioTokenType::NAME)) {
    const std::string& idxsym = context.cur_tok()->original;
    if (idxsym == "avg" || idxsym == "max") {
      SeriesIntrinsicOp op =
        (idxsym == "avg") ? SeriesIntrinsicOp::Avg : SeriesIntrinsicOp::Max;
      context.move_forward(1, "series_intrinsic");
      context.skip();
      context.try_match_panic(StyioTokenType::TOK_COMMA);
      context.skip();
      StyioAST* win = parse_fallback_expr(context);
      context.skip();
      context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
      return SeriesIntrinsicAST::Create(base, op, win);
    }
  }

  if (context.check(StyioTokenType::TOK_QUEST)) {
    context.move_forward(1, "[?");
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_COMMA);
    context.skip();
    StyioAST* cond = parse_or_expr(context);
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
    return GuardSelectorAST::Create(base, cond);
  }

  if (context.check(StyioTokenType::MATCH)) {
    context.move_forward(1, "[?=");
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_COMMA);
    context.skip();
    StyioAST* val = parse_fallback_expr(context);
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
    return EqProbeAST::Create(base, val);
  }

  StyioAST* idx = parse_fallback_expr(context);
  context.skip();
  context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
  return new ListOpAST(StyioNodeType::Access_By_Index, base, idx);
}

static BlockAST*
parse_loop_body_clause(StyioContext& context) {
  context.skip();
  if (context.check(StyioTokenType::TOK_LCURBRAC)) {
    return parse_block_only(context);
  }
  std::vector<StyioAST*> one;
  one.push_back(parse_stmt_or_expr(context));
  return BlockAST::Create(std::move(one));
}

static StyioAST*
parse_iterator_tail(StyioContext& context, StyioAST* collection) {
  std::vector<ParamAST*> params;

  context.skip();

  if (context.match(StyioTokenType::TOK_HASH) /* # */) {
    context.skip();
    /* #(param, ...) => { } — same param list as functions */
    if (context.check(StyioTokenType::TOK_LPAREN)) {
      params = parse_params(context);
    }
    else if (context.check(StyioTokenType::NAME)) {
      std::vector<HashTagNameAST*> hash_tags;

      hash_tags.push_back(HashTagNameAST::Create(parse_name_with_spaces_unsafe(context)));

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
    else {
      throw StyioSyntaxError(context.mark_cur_tok("Expected ( or name after # in iterator"));
    }
  }
  else if (
    context.cur_tok_type() == StyioTokenType::TOK_LPAREN
    || context.cur_tok_type() == StyioTokenType::NAME
  ) {
    params = parse_params(context);
  }

  context.skip();

  if (context.try_match(StyioTokenType::TOK_AMP)) {
    context.skip();
    StyioAST* collection_b = parse_fallback_expr(context);
    context.skip();
    if (not context.match(StyioTokenType::ITERATOR)) {
      throw StyioSyntaxError(context.mark_cur_tok("expected >> after first stream in zip"));
    }
    context.skip();
    std::vector<ParamAST*> params_b = parse_params(context);
    context.skip();
    if (not context.try_match(StyioTokenType::ARROW_DOUBLE_RIGHT)) {
      throw StyioSyntaxError(context.mark_cur_tok("expected => after zip streams"));
    }
    context.skip();
    StyioAST* body_ast = nullptr;
    if (context.check(StyioTokenType::TOK_LCURBRAC)) {
      body_ast = parse_block_only(context);
    }
    else {
      body_ast = parse_stmt_or_expr(context);
    }
    return StreamZipAST::Create(collection, params, collection_b, params_b, body_ast);
  }

  if (context.try_match(StyioTokenType::ARROW_DOUBLE_RIGHT) /* => */) {
    context.skip();
    if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
      return IteratorAST::Create(collection, params, parse_block_only(context));
    }
    return IteratorAST::Create(collection, params, parse_stmt_or_expr(context));
  }
  context.skip();
  if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
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

static StyioAST*
parse_expr_postfix(StyioContext& context, StyioAST* lhs) {
  while (true) {
    context.skip();
    if (context.match(StyioTokenType::MATCH)) {
      context.skip();
      if (not context.check(StyioTokenType::TOK_LCURBRAC)) {
        throw StyioSyntaxError(context.mark_cur_tok("?= must be followed by {"));
      }
      lhs = MatchCasesAST::make(lhs, parse_cases_only(context));
      continue;
    }
    if (context.match(StyioTokenType::ITERATOR)) {
      context.skip();
      /* Write: data flows into the file — `expr >> @file{...}` (@{.} allowed). */
      if (context.check(StyioTokenType::TOK_AT)) {
        lhs = ResourceWriteAST::Create(lhs, parse_resource_file_atom(context));
      }
      else {
        lhs = parse_iterator_tail(context, lhs);
      }
      continue;
    }
    if (context.match(StyioTokenType::ARROW_SINGLE_RIGHT)) {
      context.skip();
      lhs = ResourceRedirectAST::Create(lhs, parse_resource_file_atom(context));
      continue;
    }
    if (lhs && lhs->getNodeType() == StyioNodeType::Infinite) {
      if (context.match(StyioTokenType::TOK_QUEST)) {
        context.skip();
        context.try_match_panic(StyioTokenType::TOK_LPAREN);
        context.skip();
        StyioAST* cond = parse_fallback_expr(context);
        context.skip();
        context.try_match_panic(StyioTokenType::TOK_RPAREN);
        context.skip();
        context.try_match_panic(StyioTokenType::ITERATOR);
        context.skip();
        BlockAST* body = parse_loop_body_clause(context);
        lhs = InfiniteLoopAST::CreateWhile(cond, body);
        continue;
      }
      if (context.match(StyioTokenType::ARROW_DOUBLE_RIGHT)) {
        context.skip();
        BlockAST* body = parse_loop_body_clause(context);
        lhs = InfiniteLoopAST::CreateInfinite(body);
        continue;
      }
    }
    break;
  }
  return lhs;
}

StyioAST*
parse_binop_item(StyioContext& context) {
  StyioAST* output = nullptr;

  context.skip();
  switch (context.cur_tok_type()) {
    case StyioTokenType::NAME: {
      const std::string& id = context.cur_tok()->original;
      if (id == "true" || id == "false") {
        context.move_forward(1, "binop_item_bool");
        output = BoolAST::Create(id == "true");
        output = parse_arithmetic_tail_from_atom(context, output);
      }
      else {
        /* RHS calls: fact(n - 1) after + - * ... need `parse_name_and_following`, not name only. */
        output = parse_name_and_following_unsafe(context);
      }
    } break;

    case StyioTokenType::INTEGER: {
      output = parse_int(context);
      output = parse_arithmetic_tail_from_atom(context, output);
    } break;

    case StyioTokenType::DECIMAL: {
      output = parse_float(context);
      output = parse_arithmetic_tail_from_atom(context, output);
    } break;

    case StyioTokenType::STRING: {
      output = parse_string(context);
      output = parse_arithmetic_tail_from_atom(context, output);
    } break;

    case StyioTokenType::TOK_AT: {
      context.move_forward(1, "binop_item@");
      context.skip();
      output = parse_after_at_common(context, false);
    } break;

    case StyioTokenType::TOK_DOLLAR: {
      context.move_forward(1, "binop_item$");
      context.skip();
      if (not context.check(StyioTokenType::NAME)) {
        throw StyioSyntaxError(context.mark_cur_tok("expected name after $ in expression"));
      }
      auto* sr = StateRefAST::Create(parse_name_unsafe(context));
      output = parse_arithmetic_tail_from_atom(context, parse_state_ref_suffix(context, sr));
    } break;

    case StyioTokenType::TOK_LPAREN: {
      context.move_forward(1, "binop_item(");
      context.skip();
      if (context.check(StyioTokenType::EXTRACTOR)) {
        context.move_forward(1, "instant<<");
        context.skip();
        StyioAST* ratom = parse_resource_file_atom(context);
        auto* fr = dynamic_cast<FileResourceAST*>(ratom);
        if (fr == nullptr) {
          throw StyioSyntaxError(context.mark_cur_tok("instant pull needs @file{...} or @{...}"));
        }
        context.skip();
        context.try_match_panic(StyioTokenType::TOK_RPAREN);
        output = InstantPullAST::Create(fr);
        output = parse_arithmetic_tail_from_atom(context, output);
      }
      else {
        output = parse_fallback_expr(context);
        context.skip();
        context.try_match_panic(StyioTokenType::TOK_RPAREN);
        output = parse_arithmetic_tail_from_atom(context, output);
      }
    } break;

    default: {
    } break;
  }

  return output;
}

StyioAST*
parse_tuple_exprs(StyioContext& context) {
  context.try_match_panic(StyioTokenType::TOK_LPAREN);

  context.skip();
  if (context.check(StyioTokenType::TOK_RPAREN)) {
    context.move_forward(1, "empty_paren");
    return TupleAST::Create(vector<StyioAST*>());
  }

  StyioAST* first = parse_fallback_expr(context);
  context.skip();

  if (context.check(StyioTokenType::TOK_RPAREN)) {
    context.move_forward(1, "paren_close");
    return parse_arithmetic_tail_from_atom(context, first);
  }

  vector<StyioAST*> elems;
  elems.push_back(first);

  while (context.try_match(StyioTokenType::TOK_COMMA)) {
    context.skip();
    elems.push_back(parse_fallback_expr(context));
    context.skip();
  }

  context.try_match_panic(StyioTokenType::TOK_RPAREN);

  TupleAST* the_tuple = TupleAST::Create(elems);

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
  /* Allow ~> / <~ / additive tails after ||/&& (e.g. `(cond) ~> arm | @`). */
  return parse_expr_postfix(
    context,
    parse_arithmetic_tail_from_atom(context, parse_fallback_expr(context)));
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

  context.move_forward(1); /* [ */

  context.skip();
  if (context.match(StyioTokenType::ELLIPSIS)) {
    /* match() already consumed the ellipsis token */
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
    return new InfiniteAST();
  }

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

  if (StyioUnicode::is_identifier_start(context.get_curr_char())) {
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

  if (StyioUnicode::is_identifier_start(context.get_curr_char())) {
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
        temp->setFuncCallee(callee);
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
    if (StyioUnicode::is_identifier_start(context.get_curr_char())) {
      output = new ListOpAST(
        StyioNodeType::Access, (theList), parse_var_name_or_value_expr(context)
      );
    }
    else if (StyioUnicode::is_digit(context.get_curr_char())) {
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

            if (StyioUnicode::is_digit(context.get_curr_char())) {
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
  throw StyioNotImplemented("parse_loop_or_iter: non-infinite iterator");
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
    if (context.check(StyioTokenType::BOUNDED_BUFFER_OPEN)) {
      ret_type = parse_styio_type(context);
    }
    else if (context.check(StyioTokenType::NAME)) {
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
        else if (context.check(StyioTokenType::BOUNDED_BUFFER_OPEN)) {
          types.push_back(parse_styio_type(context));
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
    /*
      # f : Ret = (a: T, ...) => body
      (params after `=` override the empty param list from `# name` when `: ret` was parsed first.)
    */
    if (context.check(StyioTokenType::TOK_LPAREN) /* ( */) {
      params = parse_params(context);
      context.skip();
      if (context.match(StyioTokenType::ARROW_DOUBLE_RIGHT) /* => */) {
        context.skip();
        if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
          ret_expr = parse_block_with_forward(context);
          return FunctionAST::Create(tag_name, false, params, ret_type, ret_expr);
        }
        ret_expr = parse_stmt_or_expr(context);
        return SimpleFuncAST::Create(tag_name, false, params, ret_type, ret_expr);
      }
    }

    ret_expr = parse_expr(context);

    return SimpleFuncAST::Create(tag_name, false, params, ret_type, ret_expr);
  }
  /* SimpleFunc (Unique) */
  else if (context.match(StyioTokenType::WALRUS) /* := */) {
    context.skip();
    if (context.check(StyioTokenType::TOK_LPAREN) /* ( */) {
      params = parse_params(context);
      context.skip();
      if (context.match(StyioTokenType::ARROW_DOUBLE_RIGHT) /* => */) {
        context.skip();
        if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
          ret_expr = parse_block_with_forward(context);
          return FunctionAST::Create(tag_name, true, params, ret_type, ret_expr);
        }
        ret_expr = parse_stmt_or_expr(context);
        return SimpleFuncAST::Create(tag_name, true, params, ret_type, ret_expr);
      }
    }

    ret_expr = parse_expr(context);

    return SimpleFuncAST::Create(tag_name, true, params, ret_type, ret_expr);
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
    throw StyioNotImplemented(
      context.mark_cur_tok("hash iterator definitions are temporarily disabled (runtime crash guard)"));
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
  context.try_match_panic(StyioTokenType::ITERATOR);
  return static_cast<IteratorAST*>(parse_iterator_tail(context, collection));
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
      const std::string& id = context.cur_tok()->original;
      if (id == "true" || id == "false") {
        return parse_expr(context);
      }

      auto saved = context.save_cursor();
      context.move_forward(1, "stmt_name_probe");
      context.skip();
      StyioTokenType nt = context.cur_tok_type();

      if (nt == StyioTokenType::TOK_EQUAL) {
        context.move_forward(1, "flex_bind=");
        return FlexBindAST::Create(
          VarAST::Create(NameAST::Create(id)),
          parse_expr(context));
      }

      if (nt == StyioTokenType::ARROW_SINGLE_LEFT) {
        context.move_forward(1, "handle<-");
        context.skip();
        return HandleAcquireAST::Create(
          VarAST::Create(NameAST::Create(id)),
          parse_resource_file_atom(context));
      }

      if (nt == StyioTokenType::EXTRACTOR) {
        context.move_forward(1, "data<<file");
        context.skip();
        return ResourceWriteAST::Create(
          NameAST::Create(id),
          parse_resource_file_atom(context));
      }

      if (nt == StyioTokenType::TOK_COLON) {
        context.move_forward(1, "final_bind:");
        context.skip();
        TypeAST* ty = parse_styio_type(context);
        context.skip();
        if (not context.match(StyioTokenType::WALRUS)) {
          throw StyioSyntaxError(context.mark_cur_tok("expected := after type in binding"));
        }
        context.skip();
        return FinalBindAST::Create(
          VarAST::Create(NameAST::Create(id), ty),
          parse_expr(context));
      }

      StyioOpType cop = StyioOpType::Undefined;
      switch (nt) {
        case StyioTokenType::COMPOUND_ADD:
          cop = StyioOpType::Self_Add_Assign;
          break;
        case StyioTokenType::COMPOUND_SUB:
          cop = StyioOpType::Self_Sub_Assign;
          break;
        case StyioTokenType::COMPOUND_MUL:
          cop = StyioOpType::Self_Mul_Assign;
          break;
        case StyioTokenType::COMPOUND_DIV:
          cop = StyioOpType::Self_Div_Assign;
          break;
        case StyioTokenType::COMPOUND_MOD:
          cop = StyioOpType::Self_Mod_Assign;
          break;
        default:
          break;
      }

      if (cop != StyioOpType::Undefined) {
        context.move_forward(1, "compound_assign");
        context.skip();
        return BinOpAST::Create(cop, NameAST::Create(id), parse_expr(context));
      }

      context.restore_cursor(saved);
      return parse_expr_postfix(context, parse_name_and_following_unsafe(context));
    } break;

    /* [ list / [...] infinite / loop heads */
    case StyioTokenType::TOK_LBOXBRAC: {
      return parse_expr(context);
    } break;

    /* int */
    case StyioTokenType::INTEGER: {
      return parse_expr(context);
    } break;

    /* float */
    case StyioTokenType::DECIMAL: {
      return parse_expr(context);
    } break;

    case StyioTokenType::STRING: {
      return parse_expr(context);
    } break;

    /* @ */
    case StyioTokenType::TOK_AT: {
      context.move_forward(1, "stmt@");
      context.skip();
      if (context.check(StyioTokenType::TOK_LBOXBRAC)) {
        return parse_state_decl_after_at(context);
      }
      if (context.check(StyioTokenType::TOK_LPAREN)) {
        return parse_resources_after_at(context);
      }
      return parse_expr_postfix(context, parse_after_at_common(context, false));
    } break;

    /* # */
    case StyioTokenType::TOK_HASH: {
      return parse_hash_tag(context);
    } break;

    /* >_ */
    case StyioTokenType::PRINT: {
      return parse_print(context);
    } break;

    case StyioTokenType::TOK_HAT: {
      unsigned depth = 0;
      while (context.check(StyioTokenType::TOK_HAT)) {
        depth += 1;
        context.move_forward(1, "break^");
      }
      return BreakAST::Create(depth > 0 ? depth : 1u);
    } break;

    case StyioTokenType::ITERATOR: {
      size_t n = context.cur_tok()->original.size();
      unsigned depth = static_cast<unsigned>(n > 1 ? n - 1 : 1);
      context.move_forward(1, "continue>>");
      return ContinueAST::Create(depth);
    } break;

    /* ( */
    case StyioTokenType::TOK_LPAREN: {
      return parse_expr(context);
    } break;

    case StyioTokenType::TOK_LCURBRAC: {
      return parse_block_only(context);
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

    case StyioTokenType::YIELD_PIPE: {
      context.move_forward(1, "stmt_yield_pipe");
      context.skip();
      return ReturnAST::Create(parse_expr(context));
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

bool
styio_parse_parser_engine(const std::string& raw, StyioParserEngine& out) {
  if (raw == "legacy") {
    out = StyioParserEngine::Legacy;
    return true;
  }
  if (raw == "new") {
    out = StyioParserEngine::New;
    return true;
  }
  return false;
}

const char*
styio_parser_engine_name(StyioParserEngine engine) {
  switch (engine) {
    case StyioParserEngine::Legacy:
      return "legacy";
    case StyioParserEngine::New:
      return "new";
  }
  return "legacy";
}

static MainBlockAST*
parse_main_block_new_shadow(StyioContext& context) {
  const auto& tokens = context.get_tokens();
  const size_t start = styio_skip_trivia_tokens(tokens, context.get_token_index());
  if (start >= tokens.size()) {
    return MainBlockAST::Create(vector<StyioAST*>());
  }
  if (!styio_new_parser_is_stmt_subset_start(tokens[start]->type)) {
    return parse_main_block(context);
  }
  for (size_t i = start; i < tokens.size(); ++i) {
    if (!styio_new_parser_is_stmt_subset_token(tokens[i]->type)) {
      return parse_main_block(context);
    }
    if (tokens[i]->type == StyioTokenType::TOK_EOF) {
      break;
    }
  }

  // E.4: NewParser currently owns a small statement subset
  // (`print`, expr stmt, and bind subset).
  // Non-subset inputs are routed to legacy above.
  const auto saved = context.save_cursor();
  MainBlockAST* parsed = nullptr;
  try {
    parsed = parse_main_block_new_subset(context);
  } catch (const std::exception&) {
    context.restore_cursor(saved);
    return parse_main_block(context);
  }
  if (context.cur_tok_type() != StyioTokenType::TOK_EOF) {
    // Conservative rollback on trailing unsupported syntax.
    delete parsed;
    context.restore_cursor(saved);
    return parse_main_block(context);
  }
  return parsed;
}

MainBlockAST*
parse_main_block_with_engine(StyioContext& context, StyioParserEngine engine) {
  switch (engine) {
    case StyioParserEngine::Legacy:
      return parse_main_block(context);
    case StyioParserEngine::New:
      return parse_main_block_new_shadow(context);
  }
  return parse_main_block(context);
}
