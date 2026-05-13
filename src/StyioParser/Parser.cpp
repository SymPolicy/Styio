// [C++ STL]
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <initializer_list>
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
#include "../StyioUtil/ResourceNames.hpp"
#include "../StyioUtil/Util.hpp"
#include "BinExprMapper.hpp"
#include "HashFunctionParser.hpp"
#include "NewParserExpr.hpp"
#include "Parser.hpp"
#include "Tokenizer.hpp"

using std::string;
using std::vector;

ParseAttempt<StyioAST>
try_parse_expr_subset_until_latest(
  StyioContext& context,
  std::initializer_list<StyioTokenType> delimiters
);

namespace
{

constexpr int kMaxParserDelimiterNestingLatest = 64;
constexpr size_t kMaxInternalLegacyBridgesPerTokenLatest = 4;

void
enforce_parser_delimiter_budget_latest(StyioContext& context, const char* construct) {
  if (context.delimiter_nesting_before_current_token() < kMaxParserDelimiterNestingLatest) {
    return;
  }
  throw StyioParserResourceLimitError(
    context.mark_cur_tok(
      std::string(construct) + " exceeds parser delimiter nesting limit of "
      + std::to_string(kMaxParserDelimiterNestingLatest)
    )
  );
}

void
enforce_nightly_internal_legacy_bridge_budget_latest(StyioContext& context, const char* construct) {
  const size_t bridge_count = context.note_nightly_internal_legacy_bridge_latest();
  if (bridge_count <= kMaxInternalLegacyBridgesPerTokenLatest) {
    return;
  }
  throw StyioParserResourceLimitError(
    context.mark_cur_tok(
      std::string(construct) + " exceeded nightly-to-legacy bridge limit of "
      + std::to_string(kMaxInternalLegacyBridgesPerTokenLatest)
      + " at one token"
    )
  );
}

struct ParserRouteStatsScopeLatestDraft
{
  StyioContext& context;
  StyioParserRouteStats* previous = nullptr;

  ParserRouteStatsScopeLatestDraft(StyioContext& context, StyioParserRouteStats* current) :
      context(context),
      previous(context.parser_route_stats_latest()) {
    context.set_parser_route_stats_latest(current);
  }

  ~ParserRouteStatsScopeLatestDraft() {
    context.set_parser_route_stats_latest(previous);
  }
};

struct ParseModeScopeLatestDraft
{
  StyioContext& context;
  StyioParseMode previous;

  ParseModeScopeLatestDraft(StyioContext& context, StyioParseMode current) :
      context(context),
      previous(context.parse_mode()) {
    context.set_parse_mode(current);
    context.clear_parse_diagnostics();
  }

  ~ParseModeScopeLatestDraft() {
    context.set_parse_mode(previous);
  }
};

std::string
parser_recovery_message_latest() {
  try {
    throw;
  }
  catch (const StyioBaseException& ex) {
    return ex.what();
  }
  catch (const std::exception& ex) {
    return ex.what();
  }
  catch (...) {
    return "unknown parser failure";
  }
}

bool
parser_handle_recovery_latest(
  StyioContext& context,
  std::pair<size_t, size_t> statement_start,
  const std::string& message
) {
  if (!context.is_recovery_mode()) {
    return false;
  }

  const size_t end = std::max(statement_start.second + 1, context.current_token_end_pos());
  context.record_parse_diagnostic(statement_start.second, end, message);
  context.recover_to_statement_boundary(statement_start.first);
  return true;
}

bool
is_all_underscore_identifier_latest(const std::string& text) {
  return !text.empty()
         && std::all_of(text.begin(), text.end(), [](unsigned char ch)
                        {
                          return ch == '_';
                        });
}

bool
is_default_case_wildcard_latest(StyioContext& context) {
  if (context.cur_tok_type() == StyioTokenType::TOK_UNDLINE) {
    return true;
  }
  if (context.cur_tok_type() != StyioTokenType::NAME) {
    return false;
  }
  return is_all_underscore_identifier_latest(context.cur_tok()->original);
}

}  // namespace

bool
styio_is_bool_literal_name_latest(const std::string& name) {
  return name == "true" || name == "false";
}

StyioOpType
styio_compound_assign_op_latest(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::COMPOUND_ADD:
      return StyioOpType::Self_Add_Assign;
    case StyioTokenType::COMPOUND_SUB:
      return StyioOpType::Self_Sub_Assign;
    case StyioTokenType::COMPOUND_MUL:
      return StyioOpType::Self_Mul_Assign;
    case StyioTokenType::COMPOUND_DIV:
      return StyioOpType::Self_Div_Assign;
    case StyioTokenType::COMPOUND_MOD:
      return StyioOpType::Self_Mod_Assign;
    default:
      return StyioOpType::Undefined;
  }
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

static bool
is_import_list_separator_latest(StyioTokenType type) {
  return type == StyioTokenType::TOK_COMMA
         || type == StyioTokenType::TOK_SEMICOLON;
}

static bool
is_statement_separator_latest(StyioTokenType type) {
  return type == StyioTokenType::TOK_SEMICOLON
         || type == StyioTokenType::PIPE_SEMICOLON;
}

static void
consume_statement_separators_latest(StyioContext& context) {
  while (true) {
    context.skip();
    if (!is_statement_separator_latest(context.cur_tok_type())) {
      return;
    }
    context.move_forward(1, "stmt_separator");
  }
}

static bool
has_linebreak_before_current_token_latest(const StyioContext& context) {
  const auto& tokens = context.get_tokens();
  size_t idx = context.get_token_index();
  while (idx > 0) {
    const StyioTokenType prev = tokens[idx - 1]->type;
    if (prev == StyioTokenType::TOK_SPACE || prev == StyioTokenType::TOK_CR) {
      idx -= 1;
      continue;
    }
    return prev == StyioTokenType::TOK_LF;
  }
  return false;
}

static bool
is_import_path_separator_latest(StyioTokenType type) {
  return type == StyioTokenType::TOK_SLASH
         || type == StyioTokenType::TOK_DOT;
}

static bool
matches_legacy_string_list_import_latest(StyioContext& context) {
  const auto& tokens = context.get_tokens();
  std::size_t cursor = context.get_token_index();
  if (cursor >= tokens.size() || tokens[cursor]->type != StyioTokenType::TOK_LBOXBRAC) {
    return false;
  }

  cursor += 1;
  while (cursor < tokens.size() && styio_is_trivia_token(tokens[cursor]->type)) {
    cursor += 1;
  }
  if (cursor >= tokens.size() || tokens[cursor]->type != StyioTokenType::STRING) {
    return false;
  }

  while (cursor < tokens.size()) {
    cursor += 1;
    while (cursor < tokens.size() && styio_is_trivia_token(tokens[cursor]->type)) {
      cursor += 1;
    }
    if (cursor >= tokens.size()) {
      return false;
    }
    if (tokens[cursor]->type == StyioTokenType::TOK_RBOXBRAC) {
      return true;
    }
    if (tokens[cursor]->type != StyioTokenType::TOK_COMMA) {
      return false;
    }

    cursor += 1;
    while (cursor < tokens.size() && styio_is_trivia_token(tokens[cursor]->type)) {
      cursor += 1;
    }
    if (cursor >= tokens.size() || tokens[cursor]->type != StyioTokenType::STRING) {
      return false;
    }
  }

  return false;
}

static std::string
parse_import_path_item_latest(StyioContext& context) {
  context.skip();
  if (context.cur_tok_type() != StyioTokenType::NAME) {
    throw StyioSyntaxError(context.mark_cur_tok("expected package path segment in @import"));
  }

  std::string canonical = context.cur_tok()->original;
  context.move_forward(1, "@import path segment");
  context.skip();

  std::optional<StyioTokenType> separator_type;
  while (is_import_path_separator_latest(context.cur_tok_type())) {
    const StyioTokenType current_separator = context.cur_tok_type();
    if (separator_type.has_value() && *separator_type != current_separator) {
      throw StyioSyntaxError(context.mark_cur_tok("cannot mix / and . inside one @import item"));
    }
    separator_type = current_separator;
    context.move_forward(1, "@import path separator");
    context.skip();

    if (context.cur_tok_type() != StyioTokenType::NAME) {
      throw StyioSyntaxError(context.mark_cur_tok("expected package path segment after separator in @import"));
    }

    canonical += "/";
    canonical += context.cur_tok()->original;
    context.move_forward(1, "@import path segment");
    context.skip();
  }

  return canonical;
}

static ExtPackAST*
parse_import_decl_after_at_latest(StyioContext& context) {
  if (!context.is_root_statement_position()) {
    throw StyioSyntaxError(context.mark_cur_tok("@import is only allowed at file top level"));
  }

  if (context.cur_tok_type() != StyioTokenType::NAME || context.cur_tok()->original != "import") {
    throw StyioSyntaxError(context.mark_cur_tok("expected import after @"));
  }

  context.move_forward(1, "@import");
  context.skip();
  context.try_match_panic(StyioTokenType::TOK_LCURBRAC);
  context.skip();

  if (context.cur_tok_type() == StyioTokenType::TOK_RCURBRAC) {
    throw StyioSyntaxError(context.mark_cur_tok("@import requires at least one package path"));
  }

  std::vector<std::string> paths;
  while (true) {
    paths.push_back(parse_import_path_item_latest(context));
    if (context.cur_tok_type() == StyioTokenType::TOK_RCURBRAC) {
      break;
    }
    if (!is_import_list_separator_latest(context.cur_tok_type())) {
      throw StyioSyntaxError(context.mark_cur_tok("expected , or ; between @import items"));
    }

    context.move_forward(1, "@import list separator");
    context.skip();
    if (context.cur_tok_type() == StyioTokenType::TOK_RCURBRAC) {
      throw StyioSyntaxError(context.mark_cur_tok("expected package path after @import separator"));
    }
  }

  context.try_match_panic(StyioTokenType::TOK_RCURBRAC);
  return new ExtPackAST(paths);
}

static ExportDeclAST*
parse_export_decl_after_at_latest(StyioContext& context) {
  if (!context.is_root_statement_position()) {
    throw StyioSyntaxError(context.mark_cur_tok("@export is only allowed at file top level"));
  }

  if (context.cur_tok_type() != StyioTokenType::NAME || context.cur_tok()->original != "export") {
    throw StyioSyntaxError(context.mark_cur_tok("expected export after @"));
  }

  context.move_forward(1, "@export");
  context.skip();
  context.try_match_panic(StyioTokenType::TOK_LCURBRAC);
  context.skip();

  if (context.cur_tok_type() == StyioTokenType::TOK_RCURBRAC) {
    throw StyioSyntaxError(context.mark_cur_tok("@export requires at least one symbol"));
  }

  std::vector<std::string> symbols;
  while (true) {
    symbols.push_back(parse_import_path_item_latest(context));
    if (context.cur_tok_type() == StyioTokenType::TOK_RCURBRAC) {
      break;
    }
    if (!is_import_list_separator_latest(context.cur_tok_type())) {
      throw StyioSyntaxError(context.mark_cur_tok("expected , or ; between @export symbols"));
    }

    context.move_forward(1, "@export list separator");
    context.skip();
    if (context.cur_tok_type() == StyioTokenType::TOK_RCURBRAC) {
      throw StyioSyntaxError(context.mark_cur_tok("expected symbol after @export separator"));
    }
  }

  context.try_match_panic(StyioTokenType::TOK_RCURBRAC);
  return ExportDeclAST::Create(std::move(symbols));
}

static std::string
parse_raw_braced_body_latest(StyioContext& context, const char* context_label) {
  context.skip();
  context.try_match_panic(StyioTokenType::TOK_LCURBRAC);

  if (context.cur_tok_type() == StyioTokenType::NATIVE_EXTERN_BODY) {
    std::string body = context.cur_tok()->original;
    context.move_forward(1, context_label);
    context.try_match_panic(StyioTokenType::TOK_RCURBRAC);
    return body;
  }

  int brace_depth = 1;
  std::string body;
  while (brace_depth > 0) {
    if (context.cur_tok_type() == StyioTokenType::TOK_EOF) {
      throw StyioSyntaxError(context.mark_cur_tok(std::string(context_label) + " body is missing closing }"));
    }

    const StyioTokenType type = context.cur_tok_type();
    if (type == StyioTokenType::TOK_LCURBRAC) {
      brace_depth += 1;
    }
    else if (type == StyioTokenType::TOK_RCURBRAC) {
      brace_depth -= 1;
      if (brace_depth == 0) {
        context.move_forward(1, context_label);
        break;
      }
    }

    body += context.cur_tok()->original;
    context.move_forward(1, context_label);
  }
  return body;
}

static ExternBlockAST*
parse_extern_decl_after_at_latest(StyioContext& context) {
  if (!context.is_root_statement_position()) {
    throw StyioSyntaxError(context.mark_cur_tok("@extern is only allowed at file top level"));
  }

  if (context.cur_tok_type() != StyioTokenType::NAME || context.cur_tok()->original != "extern") {
    throw StyioSyntaxError(context.mark_cur_tok("expected extern after @"));
  }

  context.move_forward(1, "@extern");
  context.skip();
  context.try_match_panic(StyioTokenType::TOK_LPAREN);
  context.skip();
  if (context.cur_tok_type() != StyioTokenType::NAME) {
    throw StyioSyntaxError(context.mark_cur_tok("@extern requires an ABI name"));
  }

  std::string abi_head = context.cur_tok()->original;
  std::transform(
    abi_head.begin(),
    abi_head.end(),
    abi_head.begin(),
    [](unsigned char ch)
    {
      return static_cast<char>(std::tolower(ch));
    }
  );
  if (abi_head != "c") {
    throw StyioSyntaxError(context.mark_cur_tok("supported @extern ABI names are c and c++"));
  }
  context.move_forward(1, "@extern abi");
  context.skip();

  std::string abi = "c";
  if (context.cur_tok_type() == StyioTokenType::TOK_PLUS) {
    context.move_forward(1, "@extern c++ abi");
    context.skip();
    if (context.cur_tok_type() != StyioTokenType::TOK_PLUS) {
      throw StyioSyntaxError(context.mark_cur_tok("@extern(c++) requires two + tokens"));
    }
    context.move_forward(1, "@extern c++ abi");
    context.skip();
    abi = "c++";
  }

  context.try_match_panic(StyioTokenType::TOK_RPAREN);
  context.skip();
  context.try_match_panic(StyioTokenType::ARROW_DOUBLE_RIGHT);
  context.skip();

  return ExternBlockAST::Create(abi, parse_raw_braced_body_latest(context, "@extern"));
}

static StyioAST* parse_token_index_suffix(StyioContext& context, StyioAST* base);
static StyioAST* parse_state_ref_suffix(StyioContext& context, StateRefAST* sr);
TypeAST* parse_styio_type(StyioContext& context);
static StyioAST* parse_expr_postfix(StyioContext& context, StyioAST* lhs);
static ResourceRefAST* parse_resource_ref_after_at_latest(StyioContext& context);
static InfiniteLoopAST* parse_infinite_conditional_loop_after_iterator(StyioContext& context);
static StyioAST* parse_iterator_tail(StyioContext& context, StyioAST* collection);

static thread_local std::string g_resource_method_receiver_family_latest;

struct ResourceMethodReceiverScopeLatest
{
  std::string previous;

  explicit ResourceMethodReceiverScopeLatest(std::string family) :
      previous(std::move(g_resource_method_receiver_family_latest)) {
    g_resource_method_receiver_family_latest = std::move(family);
  }

  ~ResourceMethodReceiverScopeLatest() {
    g_resource_method_receiver_family_latest = std::move(previous);
  }
};

static StyioAST*
reassociate_add_into_resource_sink_latest_draft(
  StyioOpType op,
  StyioAST* lhs,
  StyioAST* rhs
) {
  if (op != StyioOpType::Binary_Add) {
    return BinOpAST::Create(op, lhs, rhs);
  }

  if (auto* write = dynamic_cast<ResourceWriteAST*>(rhs)) {
    StyioAST* data = write->release_data_latest();
    StyioAST* resource = write->release_resource_latest();
    delete write;
    return ResourceWriteAST::Create(
      BinOpAST::Create(StyioOpType::Binary_Add, lhs, data),
      resource
    );
  }

  if (auto* redirect = dynamic_cast<ResourceRedirectAST*>(rhs)) {
    StyioAST* data = redirect->release_data_latest();
    StyioAST* resource = redirect->release_resource_latest();
    delete redirect;
    return ResourceRedirectAST::Create(
      BinOpAST::Create(StyioOpType::Binary_Add, lhs, data),
      resource
    );
  }

  return BinOpAST::Create(op, lhs, rhs);
}

StyioAST*
parse_name_and_following_unsafe(StyioContext& context) {
  std::unique_ptr<NameAST> name(NameAST::Create(context.cur_tok()->original));
  context.move_forward(1);

  StyioAST* output = name.get();

  context.skip_spaces_no_linebreak();
  switch (context.cur_tok_type()) {
    /* + */
    case StyioTokenType::TOK_PLUS: {
      context.move_forward(1, "parse_name_and_following(TOK_PLUS)");
      output = parse_binop_rhs(context, name.release(), StyioOpType::Binary_Add);
    } break;

    /* - */
    case StyioTokenType::TOK_MINUS: {
      context.move_forward(1, "parse_name_and_following(TOK_MINUS)");
      output = parse_binop_rhs(context, name.release(), StyioOpType::Binary_Sub);
    } break;

    /* * */
    case StyioTokenType::TOK_STAR: {
      context.move_forward(1, "parse_name_and_following(TOK_STAR)");
      output = parse_binop_rhs(context, name.release(), StyioOpType::Binary_Mul);
    } break;

    /* ** */
    case StyioTokenType::BINOP_POW: {
      context.move_forward(1, "parse_name_and_following(BINOP_POW)");
      output = parse_binop_rhs(context, name.release(), StyioOpType::Binary_Pow);
    } break;

    /* / */
    case StyioTokenType::TOK_SLASH: {
      context.move_forward(1, "parse_name_and_following(TOK_SLASH)");
      output = parse_binop_rhs(context, name.release(), StyioOpType::Binary_Div);
    } break;

    /* % */
    case StyioTokenType::TOK_PERCENT: {
      context.move_forward(1, "parse_name_and_following(TOK_PERCENT)");
      output = parse_binop_rhs(context, name.release(), StyioOpType::Binary_Mod);
    } break;

    /* ( */
    case StyioTokenType::TOK_LPAREN: {
      output = parse_call(context, name.release());
    } break;

    /* . */
    case StyioTokenType::TOK_DOT: {
      context.move_forward(1, "parse_name_and_following(TOK_DOT)");
      context.skip();
      if (context.check(StyioTokenType::NAME)) {
        auto func_name = parse_name_unsafe(context);
        output = parse_call(context, func_name, name.release());
      }
      else {
        throw StyioSyntaxError("parse_name_and_following: There should be a name after dot!");
      }
    } break;

    /* >> */
    case StyioTokenType::ITERATOR: {
      const StyioDoubleRightContinuationOps ops{
        nullptr,
        parse_iterator_tail,
        "unsupported '>>' continuation after name",
      };
      return parse_double_right_continuation_latest(context, name.release(), ops);
    } break;

    /* Wave dispatch: x[?, c], x[?=, v], x[i] */
    case StyioTokenType::TOK_LBOXBRAC: {
      output = parse_token_index_suffix(context, name.release());
    } break;

    default: {
    } break;
  }

  if (not output) {
    throw StyioParseError("Null Return of parse_name_and_following()!");
  }

  if (output == name.get()) {
    name.release();
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

static StyioAST*
parse_negative_numeric_literal_latest(StyioContext& context) {
  switch (context.cur_tok_type()) {
    case StyioTokenType::INTEGER: {
      auto ret_val = IntAST::Create("-" + context.cur_tok()->original);
      context.move_forward(1, "parse_negative_int");
      return ret_val;
    }

    case StyioTokenType::DECIMAL: {
      auto ret_val = FloatAST::Create("-" + context.cur_tok()->original);
      context.move_forward(1, "parse_negative_float");
      return ret_val;
    }

    default:
      return nullptr;
  }
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

static std::vector<std::pair<size_t, size_t>>
build_line_seps_for_embedded_expr_latest(const std::string& src) {
  std::vector<std::pair<size_t, size_t>> seps;
  if (src.empty()) {
    seps.push_back({0, 0});
    return seps;
  }

  size_t start = 0;
  for (size_t i = 0; i < src.size(); ++i) {
    if (src[i] == '\n') {
      seps.push_back({start, i - start});
      start = i + 1;
    }
  }
  if (start <= src.size()) {
    seps.push_back({start, src.size() - start});
  }
  return seps;
}

static std::string
strip_string_token_quotes_latest(const std::string& raw) {
  if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
    return raw.substr(1, raw.size() - 2);
  }
  return raw;
}

static bool
format_expr_text_is_empty_latest(const std::string& text) {
  return std::all_of(text.begin(), text.end(), [](unsigned char ch)
                     {
                       return std::isspace(ch) != 0;
                     });
}

static size_t
find_format_expr_end_latest(const std::string& text, size_t expr_start) {
  size_t brace_depth = 0;
  for (size_t i = expr_start; i < text.size(); ++i) {
    if (text[i] == '{') {
      brace_depth += 1;
      continue;
    }
    if (text[i] == '}') {
      if (brace_depth == 0) {
        return i;
      }
      brace_depth -= 1;
    }
  }
  throw StyioSyntaxError("unterminated format string expression");
}

static StyioAST*
parse_format_expr_text_latest(const std::string& expr_text, StyioParserEngine engine) {
  if (format_expr_text_is_empty_latest(expr_text)) {
    throw StyioSyntaxError("empty format string expression");
  }

  std::vector<StyioToken*> tokens = StyioTokenizer::tokenize(expr_text);
  StyioContext expr_context(
    "<format-string>",
    expr_text,
    build_line_seps_for_embedded_expr_latest(expr_text),
    tokens,
    false
  );

  StyioAST* expr = (engine == StyioParserEngine::Nightly)
                     ? parse_expr_subset_nightly(expr_context)
                     : parse_expr(expr_context);
  expr_context.skip();
  if (expr_context.cur_tok_type() != StyioTokenType::TOK_EOF) {
    throw StyioSyntaxError(
      expr_context.mark_cur_tok("unexpected token in format string expression")
    );
  }

  return expr;
}

FmtStrAST*
parse_fmt_str_token_latest(StyioContext& context, StyioParserEngine engine) {
  if (context.cur_tok_type() != StyioTokenType::TOK_DOLLAR) {
    throw StyioParseError("parse_fmt_str_token_latest(): expected $");
  }

  context.move_forward(1, "fmt:$");
  if (context.cur_tok_type() != StyioTokenType::STRING) {
    throw StyioSyntaxError(context.mark_cur_tok("expected string literal after $ for format string"));
  }

  const std::string text = strip_string_token_quotes_latest(context.cur_tok()->original);
  context.move_forward(1, "fmt:string");

  std::vector<std::string> fragments;
  std::vector<StyioAST*> exprs;
  std::string fragment;

  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '{') {
      if (i + 1 < text.size() && text[i + 1] == '{') {
        fragment += '{';
        i += 1;
        continue;
      }

      const size_t expr_end = find_format_expr_end_latest(text, i + 1);
      fragments.push_back(fragment);
      fragment.clear();
      exprs.push_back(parse_format_expr_text_latest(text.substr(i + 1, expr_end - i - 1), engine));
      i = expr_end;
      continue;
    }

    if (text[i] == '}') {
      if (i + 1 < text.size() && text[i + 1] == '}') {
        fragment += '}';
        i += 1;
        continue;
      }
      throw StyioSyntaxError("unmatched } in format string");
    }

    fragment += text[i];
  }

  fragments.push_back(fragment);
  return FmtStrAST::Create(fragments, exprs);
}

StyioAST*
parse_path(StyioContext& context) {
  context.move(1);

  string text = "";

  while (not context.check_next('"')) {
    if (context.get_curr_pos() >= context.get_code().size()) {
      throw StyioSyntaxError(
        context.label_cur_line(
          static_cast<int>(context.get_curr_pos()),
          "unterminated path literal: missing closing '\"'"
        )
      );
    }
    text += context.get_curr_char();
    context.move(1);
  }

  // drop " at the end
  context.move(1);

  if (text.starts_with("/")) {
    return ResPathAST::Create(StyioPathType::local_absolute_unix_like, text);
  }
  else if (text.size() >= 2
           && std::isupper(static_cast<unsigned char>(text[0]))
           && text[1] == ':') {
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

static StyioDataType
resource_suffix_value_type_latest(const StyioDataType& raw) {
  if (styio_is_list_type(raw)) {
    return styio_data_type_from_name(styio_list_elem_type_name(raw));
  }
  return raw;
}

static TypeAST*
parse_styio_type_atom_latest(StyioContext& context) {
  context.skip();
  if (context.check(StyioTokenType::BOUNDED_BUFFER_OPEN)) {
    context.move_forward(1, "parse_styio_type[|");
    context.skip();
    if (context.cur_tok_type() != StyioTokenType::INTEGER) {
      throw StyioSyntaxError(
        context.mark_cur_tok("expected integer capacity in [|n|] type")
      );
    }
    std::string cap = context.cur_tok()->original;
    context.move_forward(1, "parse_styio_type cap");
    context.skip();
    context.try_match_panic(StyioTokenType::BOUNDED_BUFFER_CLOSE);
    return TypeAST::CreateBoundedRingBuffer(cap);
  }
  if (context.try_match(StyioTokenType::TOK_LPAREN)) {
    std::vector<std::string> elems;
    context.skip();
    while (!context.check(StyioTokenType::TOK_RPAREN)) {
      std::unique_ptr<TypeAST> elem(parse_styio_type(context));
      elems.push_back(elem->getTypeName());
      context.skip();
      if (!context.try_match(StyioTokenType::TOK_COMMA)) {
        break;
      }
      context.skip();
    }
    context.try_match_panic(StyioTokenType::TOK_RPAREN);
    std::string name = "(";
    for (std::size_t i = 0; i < elems.size(); ++i) {
      if (i != 0) {
        name += ",";
      }
      name += elems[i];
    }
    name += ")";
    return TypeAST::Create(StyioDataType{StyioDataTypeOption::Tuple, name, 0});
  }
  const std::string type_name = parse_name_as_str_unsafe(context);
  if (type_name == "list") {
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_LBOXBRAC);
    std::unique_ptr<TypeAST> elem(parse_styio_type(context));
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
    const std::string elem_name = elem->getTypeName();
    return TypeAST::Create(styio_make_list_type(elem_name));
  }
  if (type_name == "dict") {
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_LBOXBRAC);
    std::unique_ptr<TypeAST> key(parse_styio_type(context));
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_COMMA);
    std::unique_ptr<TypeAST> value(parse_styio_type(context));
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
    const std::string key_name = key->getTypeName();
    const std::string value_name = value->getTypeName();
    return TypeAST::Create(styio_make_dict_type(key_name, value_name));
  }
  return TypeAST::Create(type_name);
}

/*
  Extensible type parser: scalar names (f64, i64, …), Topology v2
  `T|n|` / `T|..n|` / `T..`, and container aliases.
*/
TypeAST*
parse_styio_type(StyioContext& context) {
  std::unique_ptr<TypeAST> base(parse_styio_type_atom_latest(context));
  context.skip();

  const auto suffix_saved = context.save_cursor();
  if (context.try_match(StyioTokenType::TOK_PIPE)) {
    context.skip();
    const bool recent = context.try_match(StyioTokenType::ELLIPSIS);
    context.skip();
    if (context.cur_tok_type() != StyioTokenType::INTEGER) {
      context.restore_cursor(suffix_saved);
      return base.release();
    }
    std::size_t bound = 0;
    try {
      bound = static_cast<std::size_t>(std::stoull(context.cur_tok()->original));
    }
    catch (...) {
      throw StyioSyntaxError(context.mark_cur_tok("invalid topology resource length"));
    }
    context.move_forward(1, "parse_styio_type resource_bound");
    context.skip();
    if (!context.try_match(StyioTokenType::TOK_PIPE)) {
      context.restore_cursor(suffix_saved);
      return base.release();
    }
    StyioDataType value_type = resource_suffix_value_type_latest(base->getDataType());
    return TypeAST::Create(styio_make_topology_resource_type(
      value_type,
      recent ? StyioResourceShapeKind::Recent : StyioResourceShapeKind::Fixed,
      bound
    ));
  }

  if (context.try_match(StyioTokenType::ELLIPSIS)) {
    StyioDataType value_type = resource_suffix_value_type_latest(base->getDataType());
    return TypeAST::Create(styio_make_topology_sequence_type(value_type.name));
  }

  return base.release();
}

StyioAST*
try_parse_typed_stdin_pull_bind_latest(
  StyioContext& context,
  std::vector<std::string> target_names
) {
  const auto saved = context.save_cursor();
  context.skip();
  if (!context.try_match(StyioTokenType::TOK_AT)) {
    context.restore_cursor(saved);
    return nullptr;
  }
  context.skip();
  if (!context.check(StyioTokenType::NAME)
      || !styio_is_stdin_resource_family_name(context.cur_tok()->original)) {
    context.restore_cursor(saved);
    return nullptr;
  }
  context.move_forward(1, "typed_stdin_pull:@stdin");
  context.skip();
  if (!context.try_match(StyioTokenType::TOK_COLON)) {
    context.restore_cursor(saved);
    return nullptr;
  }
  context.skip();

  std::vector<StyioDataType> pull_types;
  if (context.try_match(StyioTokenType::TOK_LPAREN)) {
    context.skip();
    while (!context.check(StyioTokenType::TOK_RPAREN)) {
      std::unique_ptr<TypeAST> elem(parse_styio_type(context));
      pull_types.push_back(elem->getDataType());
      context.skip();
      if (!context.try_match(StyioTokenType::TOK_COMMA)) {
        break;
      }
      context.skip();
    }
    context.try_match_panic(StyioTokenType::TOK_RPAREN);
  }
  else {
    std::unique_ptr<TypeAST> elem(parse_styio_type(context));
    pull_types.push_back(elem->getDataType());
  }

  if (target_names.size() != pull_types.size()) {
    throw StyioSyntaxError("stdin pull target count must match type count");
  }

  std::vector<StyioAST*> stmts;
  stmts.reserve(target_names.size());
  for (std::size_t i = 0; i < target_names.size(); ++i) {
    const StyioDataType& ty = pull_types[i];
    if (target_names.size() > 1 && styio_is_list_type(ty)) {
      throw StyioSyntaxError("multi-target stdin pull supports scalar targets; use a single target for list[T]");
    }
    if (!styio_is_list_type(ty)
        && ty.option != StyioDataTypeOption::Integer
        && ty.option != StyioDataTypeOption::Float
        && ty.option != StyioDataTypeOption::String) {
      throw StyioSyntaxError("typed stdin pull supports i64, f64, string, or list[T] targets");
    }
    stmts.push_back(FinalBindAST::Create(
      VarAST::Create(NameAST::Create(target_names[i]), TypeAST::Create(ty)),
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), ty)
    ));
  }

  if (stmts.size() == 1) {
    return stmts.front();
  }
  return BlockAST::Create(std::move(stmts));
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

  std::unique_ptr<NameAST> name(NameAST::Create(namestr));

  context.drop_white_spaces();

  if (context.check_drop(':')) {
    context.drop_white_spaces();

    std::unique_ptr<TypeAST> data_type(parse_styio_type(context));

    context.drop_white_spaces();

    if (context.check_drop('=')) {
      context.drop_white_spaces();

      std::unique_ptr<StyioAST> default_value(parse_expr(context));

      auto* param = ParamAST::Create(name.get(), data_type.get(), default_value.get());
      name.release();
      data_type.release();
      default_value.release();
      return param;
    }
    else {
      auto* param = ParamAST::Create(name.get(), data_type.get());
      name.release();
      data_type.release();
      return param;
    }
  }
  else {
    auto* param = ParamAST::Create(name.get());
    name.release();
    return param;
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
parse_parenthesized_string_path(StyioContext& context) {
  context.try_match_panic(StyioTokenType::TOK_LPAREN);
  context.skip();
  if (not context.check(StyioTokenType::STRING)) {
    throw StyioSyntaxError(context.mark_cur_tok("expected string path in (...)"));
  }
  StyioAST* p = parse_string(context);
  context.skip();
  context.try_match_panic(StyioTokenType::TOK_RPAREN);
  return p;
}

StyioAST*
parse_after_at_common(StyioContext& context, bool file_only_resource) {
  context.skip();
  if (context.check(StyioTokenType::NAME)
      && styio_is_file_resource_family_name(context.cur_tok()->original)) {
    context.move_forward(1, "@file");
    context.skip();
    StyioAST* path = parse_parenthesized_string_path(context);
    return FileResourceAST::Create(path, false);
  }
  /* Standard stream resources: @stdout / @stderr / @stdin */
  if (context.check(StyioTokenType::NAME)
      && styio_is_stdout_resource_family_name(context.cur_tok()->original)) {
    context.move_forward(1, "@stdout");
    return StdStreamAST::Create(StdStreamKind::Stdout);
  }
  if (context.check(StyioTokenType::NAME)
      && styio_is_stderr_resource_family_name(context.cur_tok()->original)) {
    context.move_forward(1, "@stderr");
    return StdStreamAST::Create(StdStreamKind::Stderr);
  }
  if (context.check(StyioTokenType::NAME)
      && styio_is_stdin_resource_family_name(context.cur_tok()->original)) {
    context.move_forward(1, "@stdin");
    context.skip();
    if (context.try_match(StyioTokenType::TOK_COLON)) {
      context.skip();
      std::unique_ptr<TypeAST> ty(parse_styio_type(context));
      StyioDataType typed_pull = ty->getDataType();
      return InstantPullAST::Create(
        StdStreamAST::Create(StdStreamKind::Stdin),
        std::move(typed_pull)
      );
    }
    return StdStreamAST::Create(StdStreamKind::Stdin);
  }
  if (context.check(StyioTokenType::TOK_LCURBRAC)) {
    StyioAST* path = parse_braced_string_path(context);
    return FileResourceAST::Create(path, true);
  }
  if (context.check(StyioTokenType::TOK_LPAREN)) {
    const auto saved = context.save_cursor();
    context.move_forward(1, "@(");
    context.skip();
    if (context.try_match(StyioTokenType::TOK_RPAREN)) {
      return EmptyResourceAST::Create();
    }
    if (context.check(StyioTokenType::STRING)) {
      StyioAST* path = parse_string(context);
      context.skip();
      context.try_match_panic(StyioTokenType::TOK_RPAREN);
      return FileResourceAST::Create(path, true);
    }
    context.restore_cursor(saved);
    if (not file_only_resource) {
      return parse_resources_after_at(context);
    }
  }
  if (not file_only_resource && context.check(StyioTokenType::NAME)) {
    return parse_resource_ref_after_at_latest(context);
  }
  return UndefinedLitAST::Create();
}

StyioAST*
parse_state_decl_after_at_latest(StyioContext& context) {
  throw StyioSyntaxError(context.mark_cur_tok(
    "legacy state-resource @[...] syntax is retired; use top-level @name : T|..n| resources, "
    "expr -> @name writes, and @name[-1] selectors"
  ));
}

static std::vector<std::unique_ptr<ParamAST>>
parse_internal_resource_params_latest(StyioContext& context) {
  std::vector<std::unique_ptr<ParamAST>> params;
  context.try_match_panic(StyioTokenType::TOK_HASH);
  context.skip();
  context.try_match_panic(StyioTokenType::TOK_LPAREN);
  context.skip();
  if (!context.check(StyioTokenType::TOK_RPAREN)) {
    while (true) {
      if (!context.check(StyioTokenType::NAME)) {
        throw StyioSyntaxError(context.mark_cur_tok("resource declaration parameter needs a name"));
      }
      std::unique_ptr<NameAST> var_name(parse_name_unsafe(context));
      context.skip();
      if (context.try_match(StyioTokenType::TOK_COLON)) {
        context.skip();
        std::unique_ptr<TypeAST> var_type(parse_styio_type(context));
        std::unique_ptr<ParamAST> param(ParamAST::Create(var_name.get(), var_type.get()));
        var_name.release();
        var_type.release();
        params.push_back(std::move(param));
      }
      else {
        std::unique_ptr<ParamAST> param(ParamAST::Create(var_name.get()));
        var_name.release();
        params.push_back(std::move(param));
      }
      context.skip();
      if (!context.try_match(StyioTokenType::TOK_COMMA)) {
        break;
      }
      context.skip();
    }
  }
  context.try_match_panic(StyioTokenType::TOK_RPAREN);
  return params;
}

static ResourceAST*
make_internal_resource_decl_marker_latest(
  const std::string& resource_name,
  std::unique_ptr<TypeAST> resource_type
) {
  const std::string type_name = resource_type == nullptr ? std::string("") : resource_type->getTypeName();
  std::vector<std::pair<StyioAST*, std::string>> resources;
  resources.emplace_back(NameAST::Create(resource_name), type_name);
  return ResourceAST::Create(std::move(resources));
}

static bool
file_resource_decl_body_calls_file_path_latest(StyioContext& context) {
  const auto& tokens = context.get_tokens();
  const size_t start = context.get_token_index();
  int brace_depth = 0;

  for (size_t i = start; i < tokens.size(); ++i) {
    const auto type = tokens[i]->type;
    if (type == StyioTokenType::TOK_LCURBRAC) {
      ++brace_depth;
      continue;
    }
    if (type == StyioTokenType::TOK_RCURBRAC) {
      --brace_depth;
      if (brace_depth <= 0) {
        return false;
      }
      continue;
    }
    if (brace_depth <= 0 || styio_is_trivia_token(type)) {
      continue;
    }
    if (type == StyioTokenType::NAME
        && styio_is_file_resource_family_name(tokens[i]->original)) {
      size_t next = i + 1;
      while (next < tokens.size() && styio_is_trivia_token(tokens[next]->type)) {
        ++next;
      }
      if (next < tokens.size() && tokens[next]->type == StyioTokenType::TOK_LPAREN) {
        return true;
      }
    }
  }
  return false;
}

static bool
internal_resource_decl_body_is_placeholder_latest(const BlockAST* body) {
  return body != nullptr
         && body->stmts.size() == 1
         && body->stmts[0] != nullptr
         && (body->stmts[0]->getNodeType() == StyioNodeType::UndefLiteral || body->stmts[0]->getNodeType() == StyioNodeType::Pass);
}

static void
parse_internal_standard_stream_decl_body_latest(
  StyioContext& context,
  const std::string& stream_name,
  const std::vector<std::unique_ptr<ParamAST>>& params
) {
  auto parse_symbolic_terminal = [&]()
  {
    if (!parse_terminal_handle_latest(context)) {
      throw StyioSyntaxError(context.mark_cur_tok("expected terminal handle [>_] or (>_)"));
    }
  };

  context.try_match_panic(StyioTokenType::TOK_LCURBRAC);
  context.skip();
  if (styio_is_stdin_resource_family_name(stream_name)) {
    if (!params.empty()) {
      throw StyioSyntaxError(context.mark_cur_tok("@stdin declaration expects zero parameters"));
    }
    context.try_match_panic(StyioTokenType::YIELD_PIPE);
    context.skip();
    if (context.try_match(StyioTokenType::ARROW_SINGLE_LEFT)) {
      context.skip();
    }
    parse_symbolic_terminal();
  }
  else if (styio_is_stdout_resource_family_name(stream_name)) {
    if (params.size() != 1) {
      throw StyioSyntaxError(context.mark_cur_tok("@stdout declaration expects one parameter"));
    }
    if (!context.check(StyioTokenType::NAME)) {
      throw StyioSyntaxError(context.mark_cur_tok("@stdout declaration body expects its declared value name"));
    }
    if (context.cur_tok()->original != params[0]->getName()) {
      throw StyioSyntaxError(context.mark_cur_tok("@stdout declaration body uses an undeclared value name"));
    }
    context.move_forward(1, "std_stream_decl_stdout_arg");
    context.skip();
    if (!context.try_match(StyioTokenType::ITERATOR)
        && !context.try_match(StyioTokenType::ARROW_SINGLE_RIGHT)) {
      throw StyioSyntaxError(context.mark_cur_tok("@stdout declaration expects >> or ->"));
    }
    context.skip();
    parse_symbolic_terminal();
  }
  else {
    if (params.size() != 1) {
      throw StyioSyntaxError(context.mark_cur_tok("@stderr declaration expects one parameter"));
    }
    context.try_match_panic(StyioTokenType::TOK_EXCLAM);
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_LPAREN);
    context.skip();
    if (!context.check(StyioTokenType::NAME)) {
      throw StyioSyntaxError(context.mark_cur_tok("@stderr declaration body expects its declared value name"));
    }
    if (context.cur_tok()->original != params[0]->getName()) {
      throw StyioSyntaxError(context.mark_cur_tok("@stderr declaration body uses an undeclared value name"));
    }
    context.move_forward(1, "std_stream_decl_stderr_arg");
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RPAREN);
    context.skip();
    if (!context.try_match(StyioTokenType::ITERATOR)
        && !context.try_match(StyioTokenType::ARROW_SINGLE_RIGHT)) {
      throw StyioSyntaxError(context.mark_cur_tok("@stderr declaration expects >> or ->"));
    }
    context.skip();
    parse_symbolic_terminal();
  }
  context.skip();
  context.try_match_panic(StyioTokenType::TOK_RCURBRAC);
}

static ResourceAST*
parse_internal_resource_decl_after_at_latest(StyioContext& context) {
  if (!context.check(StyioTokenType::NAME)) {
    throw StyioSyntaxError(context.mark_cur_tok("resource declaration needs a name after @"));
  }

  const std::string resource_name = parse_name_as_str_unsafe(context);
  context.skip();

  std::unique_ptr<TypeAST> resource_type;
  if (context.try_match(StyioTokenType::TOK_COLON)) {
    context.skip();
    resource_type.reset(parse_styio_type(context));
    context.skip();
  }

  context.try_match_panic(StyioTokenType::WALRUS);
  context.skip();
  std::vector<std::unique_ptr<ParamAST>> params = parse_internal_resource_params_latest(context);
  context.skip();
  context.try_match_panic(StyioTokenType::ARROW_DOUBLE_RIGHT);
  context.skip();

  const bool standard_stream_name = styio_is_std_stream_resource_family_name(resource_name);

  if (standard_stream_name) {
    parse_internal_standard_stream_decl_body_latest(context, resource_name, params);
    return make_internal_resource_decl_marker_latest(resource_name, std::move(resource_type));
  }

  if (styio_is_file_resource_family_name(resource_name) && resource_type == nullptr) {
    throw StyioSyntaxError(context.mark_cur_tok("@file declaration needs a resource type"));
  }
  if (styio_is_file_resource_family_name(resource_name) && file_resource_decl_body_calls_file_path_latest(context)) {
    throw StyioSyntaxError(context.mark_cur_tok("@file declaration must be written in Styio; file(path) is not an allowed primitive"));
  }

  std::unique_ptr<BlockAST> body(parse_block_only(context));
  if (styio_is_file_resource_family_name(resource_name)) {
    if (!internal_resource_decl_body_is_placeholder_latest(body.get())) {
      throw StyioSyntaxError(context.mark_cur_tok("@file declaration currently accepts only `{ ... }` until typed resource body lowering is implemented"));
    }
    return make_internal_resource_decl_marker_latest(resource_name, std::move(resource_type));
  }

  throw StyioSyntaxError(context.mark_cur_tok("unsupported internal resource declaration"));
}

static bool
parse_at_name_colon_routes_to_internal_decl_latest(StyioContext& context) {
  const auto saved = context.save_cursor();
  bool internal_decl = false;
  try {
    if (!context.check(StyioTokenType::NAME)) {
      context.restore_cursor(saved);
      return false;
    }
    context.move_forward(1, "resource_decl_route_name");
    context.skip();
    if (!context.try_match(StyioTokenType::TOK_COLON)) {
      context.restore_cursor(saved);
      return false;
    }
    context.skip();
    std::unique_ptr<TypeAST> ty(parse_styio_type(context));
    context.skip();
    if (context.try_match(StyioTokenType::WALRUS)) {
      context.skip();
      internal_decl = context.check(StyioTokenType::TOK_HASH);
    }
  }
  catch (...) {
    context.restore_cursor(saved);
    throw;
  }
  context.restore_cursor(saved);
  return internal_decl;
}

static ResourceDeclAST*
parse_resource_decl_v2_after_at_latest(StyioContext& context) {
  std::vector<std::pair<std::unique_ptr<NameAST>, std::unique_ptr<TypeAST>>> slot_owners;
  while (true) {
    if (!context.check(StyioTokenType::NAME)) {
      throw StyioSyntaxError(context.mark_cur_tok("resource declaration needs a name after @"));
    }
    std::unique_ptr<NameAST> name(parse_name_unsafe(context));
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_COLON);
    context.skip();
    std::unique_ptr<TypeAST> type(parse_styio_type(context));
    slot_owners.emplace_back(std::move(name), std::move(type));
    context.skip();
    if (!context.try_match(StyioTokenType::TOK_COMMA)) {
      break;
    }
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_AT);
    context.skip();
  }

  std::unique_ptr<BlockAST> driver;
  if (context.try_match(StyioTokenType::WALRUS)) {
    context.skip();
    if (!context.check(StyioTokenType::TOK_LCURBRAC)) {
      throw StyioSyntaxError(context.mark_cur_tok("resource declaration driver must be a block"));
    }
    driver.reset(parse_block_only(context));
  }
  std::vector<std::pair<NameAST*, TypeAST*>> slots;
  slots.reserve(slot_owners.size());
  for (const auto& slot : slot_owners) {
    slots.emplace_back(slot.first.get(), slot.second.get());
  }
  auto* decl = ResourceDeclAST::Create(std::move(slots), driver.get());
  for (auto& slot : slot_owners) {
    slot.first.release();
    slot.second.release();
  }
  driver.release();
  return decl;
}

static bool
parse_resource_method_route_after_at_latest(StyioContext& context) {
  const auto saved = context.save_cursor();
  bool routes = false;
  try {
    if (!context.check(StyioTokenType::NAME)) {
      context.restore_cursor(saved);
      return false;
    }
    context.move_forward(1, "resource_method_route_family");
    context.skip();
    bool has_separator = false;
    if (context.try_match(StyioTokenType::TOK_DOT)) {
      has_separator = true;
    }
    else if (context.try_match(StyioTokenType::TOK_COLON)) {
      context.skip();
      if (context.try_match(StyioTokenType::TOK_COLON)) {
        has_separator = true;
      }
    }
    if (!has_separator) {
      context.restore_cursor(saved);
      return false;
    }
    context.skip();
    if (!context.check(StyioTokenType::NAME)) {
      context.restore_cursor(saved);
      return false;
    }
    context.move_forward(1, "resource_method_route_name");
    context.skip();
    routes = context.check(StyioTokenType::TOK_EQUAL)
             || context.check(StyioTokenType::WALRUS);
  }
  catch (...) {
    context.restore_cursor(saved);
    throw;
  }
  context.restore_cursor(saved);
  return routes;
}

static ResourceMethodDefAST*
parse_resource_method_def_after_at_latest(StyioContext& context) {
  if (!context.check(StyioTokenType::NAME)) {
    throw StyioSyntaxError(context.mark_cur_tok("resource method definition needs a family name after @"));
  }
  const std::string family = parse_name_as_str_unsafe(context);
  context.skip();
  bool ok_separator = false;
  if (context.try_match(StyioTokenType::TOK_DOT)) {
    ok_separator = true;
  }
  else if (context.try_match(StyioTokenType::TOK_COLON)) {
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_COLON);
    ok_separator = true;
  }
  if (!ok_separator) {
    throw StyioSyntaxError(context.mark_cur_tok("resource method definition needs . or :: after resource family"));
  }
  context.skip();
  if (!context.check(StyioTokenType::NAME)) {
    throw StyioSyntaxError(context.mark_cur_tok("resource method definition needs a method name"));
  }
  const std::string method = parse_name_as_str_unsafe(context);
  context.skip();
  bool final_binding = false;
  if (context.try_match(StyioTokenType::WALRUS)) {
    final_binding = true;
  }
  else {
    context.try_match_panic(StyioTokenType::TOK_EQUAL);
  }
  context.skip();

  std::vector<ParamAST*> params;
  bool property = true;
  StyioAST* body = nullptr;
  if (context.check(StyioTokenType::TOK_LPAREN)) {
    property = false;
    params = parse_params(context);
    context.skip();
    context.try_match_panic(StyioTokenType::ARROW_DOUBLE_RIGHT);
    context.skip();
    ResourceMethodReceiverScopeLatest receiver_scope(family);
    if (context.check(StyioTokenType::TOK_LCURBRAC)) {
      body = parse_block_only(context);
    }
    else {
      body = parse_stmt_or_expr_legacy(context);
    }
  }
  else {
    ResourceMethodReceiverScopeLatest receiver_scope(family);
    body = parse_expr(context);
  }

  return ResourceMethodDefAST::Create(
    family,
    method,
    final_binding,
    property,
    std::move(params),
    body
  );
}

static int
parse_resource_selector_offset_latest(StyioContext& context) {
  int sign = 1;
  if (context.try_match(StyioTokenType::TOK_MINUS)) {
    sign = -1;
    context.skip();
  }
  if (context.cur_tok_type() != StyioTokenType::INTEGER) {
    throw StyioSyntaxError(context.mark_cur_tok("resource selector expects an integer history index"));
  }
  int value = 0;
  try {
    value = std::stoi(context.cur_tok()->original);
  }
  catch (...) {
    throw StyioSyntaxError(context.mark_cur_tok("invalid resource selector history index"));
  }
  context.move_forward(1, "resource_selector_index");
  value *= sign;
  if (value >= 0) {
    throw StyioSyntaxError(context.mark_cur_tok("resource selector expects negative history index"));
  }
  return value;
}

static ResourceRefAST*
parse_resource_ref_after_at_latest(StyioContext& context) {
  if (!context.check(StyioTokenType::NAME)) {
    throw StyioSyntaxError(context.mark_cur_tok("resource reference needs a name after @"));
  }
  NameAST* name = parse_name_unsafe(context);
  context.skip();
  if (!context.try_match(StyioTokenType::TOK_LBOXBRAC)) {
    return ResourceRefAST::Create(name);
  }
  context.skip();
  if (context.try_match(StyioTokenType::ELLIPSIS)) {
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
    return ResourceRefAST::CreateSelector(name, ResourceSelectorKind::SnapshotAll);
  }
  const int offset = parse_resource_selector_offset_latest(context);
  context.skip();
  if (context.try_match(StyioTokenType::ELLIPSIS)) {
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
    return ResourceRefAST::CreateSelector(name, ResourceSelectorKind::SliceFrom, offset);
  }
  context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
  return ResourceRefAST::CreateSelector(name, ResourceSelectorKind::Offset, offset);
}

StyioAST*
parse_at_stmt_or_expr_latest(StyioContext& context) {
  context.move_forward(1, "stmt@");
  context.skip();
  if (context.check(StyioTokenType::NAME)) {
    if (parse_resource_method_route_after_at_latest(context)) {
      return parse_resource_method_def_after_at_latest(context);
    }
    if (!g_resource_method_receiver_family_latest.empty()
        && context.cur_tok()->original == g_resource_method_receiver_family_latest) {
      const auto receiver_saved = context.save_cursor();
      const std::string family = context.cur_tok()->original;
      context.move_forward(1, "resource_method_receiver");
      context.skip();
      if (!context.check(StyioTokenType::TOK_LPAREN)
          && !context.check(StyioTokenType::TOK_COLON)) {
        return parse_expr_postfix(context, ResourceReceiverAST::Create(family));
      }
      context.restore_cursor(receiver_saved);
    }
    auto saved_resource_decl = context.save_cursor();
    const std::string probe_name = context.cur_tok()->original;
    const bool builtin_resource_name = styio_is_builtin_resource_family_name(probe_name);
    const bool internal_colon_decl =
      parse_at_name_colon_routes_to_internal_decl_latest(context);
    context.move_forward(1, "resource_decl_probe");
    context.skip();
    if (context.check(StyioTokenType::WALRUS)
        || internal_colon_decl) {
      context.restore_cursor(saved_resource_decl);
      return parse_internal_resource_decl_after_at_latest(context);
    }
    if (context.check(StyioTokenType::TOK_COLON) && !builtin_resource_name) {
      context.restore_cursor(saved_resource_decl);
      return parse_resource_decl_v2_after_at_latest(context);
    }
    context.restore_cursor(saved_resource_decl);
  }
  if (context.check(StyioTokenType::NAME) && context.cur_tok()->original == "import") {
    return parse_import_decl_after_at_latest(context);
  }
  if (context.check(StyioTokenType::NAME) && context.cur_tok()->original == "export") {
    return parse_export_decl_after_at_latest(context);
  }
  if (context.check(StyioTokenType::NAME) && context.cur_tok()->original == "extern") {
    return parse_extern_decl_after_at_latest(context);
  }
  if (context.check(StyioTokenType::TOK_LBOXBRAC)) {
    return parse_state_decl_after_at_latest(context);
  }
  if (context.check(StyioTokenType::TOK_LPAREN)) {
    return parse_expr_postfix(context, parse_after_at_common(context, false));
  }
  if (context.check(StyioTokenType::NAME)
      && !styio_is_builtin_resource_family_name(context.cur_tok()->original)) {
    return parse_resource_ref_after_at_latest(context);
  }
  return parse_expr_postfix(context, parse_after_at_common(context, false));
}

enum class ResourceOperandPurposeLatest
{
  FileAtom,
  InstantPullSource,
  SinkTarget,
};

static bool
resource_operand_accepts_latest(
  StyioNodeType node_type,
  ResourceOperandPurposeLatest purpose
) {
  switch (purpose) {
    case ResourceOperandPurposeLatest::FileAtom:
      return node_type == StyioNodeType::FileResource
             || node_type == StyioNodeType::StdinResource
             || node_type == StyioNodeType::StdoutResource
             || node_type == StyioNodeType::StderrResource
             || node_type == StyioNodeType::InstantPull;
    case ResourceOperandPurposeLatest::InstantPullSource:
      return node_type == StyioNodeType::FileResource
             || node_type == StyioNodeType::StdinResource
             || node_type == StyioNodeType::StdoutResource
             || node_type == StyioNodeType::StderrResource;
    case ResourceOperandPurposeLatest::SinkTarget:
      return node_type == StyioNodeType::FileResource
             || node_type == StyioNodeType::StdinResource
             || node_type == StyioNodeType::StdoutResource
             || node_type == StyioNodeType::StderrResource
             || node_type == StyioNodeType::EmptyResource
             || node_type == StyioNodeType::InstantPull;
  }
  return false;
}

static StyioAST*
parse_resource_operand_after_at_latest(
  StyioContext& context,
  ResourceOperandPurposeLatest purpose,
  const std::string& diagnostic,
  bool allow_named_ref
) {
  context.skip();
  context.match_panic(StyioTokenType::TOK_AT);
  context.skip();
  if (allow_named_ref
      && context.check(StyioTokenType::NAME)
      && !styio_is_builtin_resource_family_name(context.cur_tok()->original)) {
    return parse_resource_ref_after_at_latest(context);
  }
  StyioAST* resource = parse_after_at_common(context, true);
  if (!resource_operand_accepts_latest(resource->getNodeType(), purpose)) {
    delete resource;
    throw StyioSyntaxError(context.mark_cur_tok(diagnostic));
  }
  return resource;
}

StyioAST*
parse_resource_file_atom_latest(StyioContext& context) {
  return parse_resource_operand_after_at_latest(
    context,
    ResourceOperandPurposeLatest::FileAtom,
    "expected @file(...), @{...}, @stdout, @stderr, or @stdin",
    false
  );
}

StyioAST*
parse_instant_pull_resource_atom_latest(StyioContext& context, const std::string& diagnostic) {
  return parse_resource_operand_after_at_latest(
    context,
    ResourceOperandPurposeLatest::InstantPullSource,
    diagnostic,
    false
  );
}

StyioAST*
parse_parenthesized_instant_pull_latest(
  StyioContext& context,
  StyioTokenType prefix,
  const std::string& diagnostic,
  const std::string& close_diagnostic
) {
  context.move_forward(1, prefix == StyioTokenType::ARROW_SINGLE_LEFT ? "instant<-" : "instant<<");
  context.skip();
  StyioAST* ratom = parse_instant_pull_resource_atom_latest(context, diagnostic);
  context.skip();
  if (!context.match(StyioTokenType::TOK_RPAREN)) {
    delete ratom;
    throw StyioSyntaxError(close_diagnostic);
  }
  return InstantPullAST::Create(ratom);
}

bool
parse_terminal_handle_latest(StyioContext& context) {
  const auto saved = context.save_cursor();
  context.skip();
  if (context.try_match(StyioTokenType::TOK_LBOXBRAC)) {
    context.skip();
    if (!context.check(StyioTokenType::PRINT)) {
      context.restore_cursor(saved);
      return false;
    }
    context.move_forward(1, "terminal_handle[>_]");
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
    return true;
  }
  if (context.try_match(StyioTokenType::TOK_LPAREN)) {
    context.skip();
    if (!context.check(StyioTokenType::PRINT)) {
      context.restore_cursor(saved);
      return false;
    }
    context.move_forward(1, "terminal_handle(>_)");
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RPAREN);
    return true;
  }
  context.restore_cursor(saved);
  return false;
}

StyioAST*
parse_resource_target_latest(StyioContext& context, StdStreamKind terminal_kind) {
  if (parse_terminal_handle_latest(context)) {
    return StdStreamAST::CreateTerminalHandle(terminal_kind);
  }
  return parse_resource_operand_after_at_latest(
    context,
    ResourceOperandPurposeLatest::SinkTarget,
    "expected @file(...), @{...}, @(\"...\"), @(), @stdout, @stderr, @stdin, or @resource",
    true
  );
}

StyioAST*
try_parse_resource_write_tail_latest(StyioContext& context, StyioAST* data) {
  const auto saved = context.save_cursor();
  context.skip();
  bool terminal_target = false;
  if (context.check(StyioTokenType::TOK_LBOXBRAC)
      || context.check(StyioTokenType::TOK_LPAREN)) {
    const auto terminal_saved = context.save_cursor();
    terminal_target = parse_terminal_handle_latest(context);
    context.restore_cursor(terminal_saved);
  }
  if (context.check(StyioTokenType::TOK_AT) || terminal_target) {
    std::unique_ptr<StyioAST> target(parse_resource_target_latest(context));
    StyioAST* write = ResourceWriteAST::Create(data, target.get());
    target.release();
    return write;
  }
  context.restore_cursor(saved);
  return nullptr;
}

StyioAST*
parse_resource_extractor_write_tail_latest(StyioContext& context, StyioAST* data) {
  std::unique_ptr<StyioAST> data_owner(data);
  context.skip();
  std::unique_ptr<StyioAST> target(parse_resource_target_latest(context));
  StyioAST* write = ResourceWriteAST::Create(data_owner.get(), target.get());
  data_owner.release();
  target.release();
  return write;
}

StyioAST*
parse_resource_redirect_tail_latest(StyioContext& context, StyioAST* data) {
  std::unique_ptr<StyioAST> data_owner(data);
  context.skip();
  std::unique_ptr<StyioAST> target(parse_resource_target_latest(context));
  StyioAST* redirect = ResourceRedirectAST::Create(data_owner.get(), target.get());
  data_owner.release();
  target.release();
  return redirect;
}

StyioAST*
parse_double_right_continuation_latest(
  StyioContext& context,
  StyioAST* lhs,
  const StyioDoubleRightContinuationOps& ops
) {
  std::unique_ptr<StyioAST> lhs_owner(lhs);
  context.try_match_panic(StyioTokenType::ITERATOR);
  if (lhs_owner != nullptr
      && lhs_owner->getNodeType() == StyioNodeType::Infinite
      && ops.parse_infinite_after_arrow != nullptr) {
    lhs_owner.reset();
    return ops.parse_infinite_after_arrow(context);
  }
  if (StyioAST* write = try_parse_resource_write_tail_latest(context, lhs_owner.get())) {
    lhs_owner.release();
    return write;
  }
  if (ops.parse_iterator_tail_after_arrow != nullptr) {
    return ops.parse_iterator_tail_after_arrow(context, lhs_owner.release());
  }
  throw StyioSyntaxError(context.mark_cur_tok(ops.unsupported_message));
}

/*
  Expression
  - Value
  - Binary Comparison
*/

StyioAST*
parse_cond_item(StyioContext& context) {
  context.drop_all_spaces();

  std::unique_ptr<StyioAST> output(parse_value_expr(context));

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

        std::unique_ptr<StyioAST> rhs(parse_value_expr(context));
        StyioAST* comp = new BinCompAST(CompType::EQ, output.get(), rhs.get());
        output.release();
        rhs.release();
        output.reset(comp);
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

        std::unique_ptr<StyioAST> rhs(parse_value_expr(context));
        StyioAST* comp = new BinCompAST(CompType::NE, output.get(), rhs.get());
        output.release();
        rhs.release();
        output.reset(comp);
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

        std::unique_ptr<StyioAST> rhs(parse_value_expr(context));
        StyioAST* comp = new BinCompAST(CompType::GE, output.get(), rhs.get());
        output.release();
        rhs.release();
        output.reset(comp);
      }
      else {
        /*
          Greater Than
            expr > expr
        */

        // drop all spaces after >
        context.drop_all_spaces();

        std::unique_ptr<StyioAST> rhs(parse_value_expr(context));
        StyioAST* comp = new BinCompAST(CompType::GT, output.get(), rhs.get());
        output.release();
        rhs.release();
        output.reset(comp);
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

        std::unique_ptr<StyioAST> rhs(parse_value_expr(context));
        StyioAST* comp = new BinCompAST(CompType::LE, output.get(), rhs.get());
        output.release();
        rhs.release();
        output.reset(comp);
      }
      else {
        /*
          Less Than
            expr < expr
        */

        // drop all spaces after <
        context.drop_all_spaces();

        std::unique_ptr<StyioAST> rhs(parse_value_expr(context));
        StyioAST* comp = new BinCompAST(CompType::LT, output.get(), rhs.get());
        output.release();
        rhs.release();
        output.reset(comp);
      };
    }

    break;

    default:
      break;
  }

  return output.release();
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
StyioAST* parse_list_exprs_latest_draft(StyioContext& context);

static StyioAST* parse_or_expr(StyioContext& context);

static StyioAST* parse_fallback_expr(StyioContext& context);

static thread_local bool g_apply_pipe_tail_enabled_latest = true;

class ApplyPipeTailDisableScopeLatest
{
private:
  bool saved_;

public:
  ApplyPipeTailDisableScopeLatest() :
      saved_(g_apply_pipe_tail_enabled_latest) {
    g_apply_pipe_tail_enabled_latest = false;
  }

  ~ApplyPipeTailDisableScopeLatest() {
    g_apply_pipe_tail_enabled_latest = saved_;
  }
};

static FuncCallAST*
make_callable_apply_latest(StyioAST* callee, StyioAST* arg) {
  std::unique_ptr<StyioAST> callee_owner(callee);
  std::unique_ptr<StyioAST> arg_owner(arg);
  vector<StyioAST*> args;
  args.push_back(arg_owner.get());

  FuncCallAST* call = nullptr;
  if (auto* name = dynamic_cast<NameAST*>(callee_owner.get())) {
    call = FuncCallAST::Create(name, args);
  }
  else {
    call = FuncCallAST::CreateCallable(callee_owner.get(), args);
  }
  callee_owner.release();
  arg_owner.release();
  return call;
}

static FuncCallAST*
parse_callable_call_suffix_latest(StyioContext& context, StyioAST* callee) {
  std::unique_ptr<StyioAST> callee_owner(callee);
  context.try_match_panic(StyioTokenType::TOK_LPAREN);

  vector<std::unique_ptr<StyioAST>> arg_owners;
  context.skip();
  while (!context.check(StyioTokenType::TOK_RPAREN)) {
    arg_owners.emplace_back(parse_expr(context));
    context.try_match(StyioTokenType::TOK_COMMA);
    context.skip();
  }
  context.try_match_panic(StyioTokenType::TOK_RPAREN);
  vector<StyioAST*> args;
  args.reserve(arg_owners.size());
  for (auto& arg : arg_owners) {
    args.push_back(arg.get());
  }
  FuncCallAST* call = FuncCallAST::CreateCallable(callee_owner.get(), args);
  callee_owner.release();
  for (auto& arg : arg_owners) {
    arg.release();
  }
  return call;
}

static StyioAST*
parse_arithmetic_tail_from_atom(StyioContext& context, StyioAST* output) {
  std::unique_ptr<StyioAST> output_owner(output);
  context.skip();
  switch (context.cur_tok_type()) {
    case StyioTokenType::TOK_PLUS: {
      context.move_forward(1, "arith_tail(+)");
      output_owner.reset(parse_binop_rhs(context, output_owner.release(), StyioOpType::Binary_Add));
    } break;

    case StyioTokenType::TOK_MINUS: {
      context.move_forward(1, "arith_tail(-)");
      output_owner.reset(parse_binop_rhs(context, output_owner.release(), StyioOpType::Binary_Sub));
    } break;

    case StyioTokenType::TOK_STAR: {
      context.move_forward(1, "arith_tail(*)");
      output_owner.reset(parse_binop_rhs(context, output_owner.release(), StyioOpType::Binary_Mul));
    } break;

    case StyioTokenType::BINOP_POW: {
      context.move_forward(1, "arith_tail(POW)");
      output_owner.reset(parse_binop_rhs(context, output_owner.release(), StyioOpType::Binary_Pow));
    } break;

    case StyioTokenType::TOK_SLASH: {
      context.move_forward(1, "arith_tail(/)");
      output_owner.reset(parse_binop_rhs(context, output_owner.release(), StyioOpType::Binary_Div));
    } break;

    case StyioTokenType::TOK_PERCENT: {
      context.move_forward(1, "arith_tail(%)");
      output_owner.reset(parse_binop_rhs(context, output_owner.release(), StyioOpType::Binary_Mod));
    } break;

    case StyioTokenType::WAVE_LEFT:
    case StyioTokenType::WAVE_RIGHT:
      throw StyioSyntaxError(context.mark_cur_tok(
        "<~ and ~> are reserved; use ?(cond) => value | fallback or ?(cond) => { ... } | { ... }"
      ));

    case StyioTokenType::TOK_LBOXBRAC: {
      if (has_linebreak_before_current_token_latest(context)) {
        break;
      }
      /* Do not treat `[` after a bare literal / bool as indexing — that would glue
         `result = true` to the next line's `[1,2,3] >> ...` and break parsing. */
      if (output_owner && (output_owner->getNodeType() == StyioNodeType::Integer || output_owner->getNodeType() == StyioNodeType::Float || output_owner->getNodeType() == StyioNodeType::Bool || output_owner->getNodeType() == StyioNodeType::String)) {
        break;
      }
      output_owner.reset(parse_token_index_suffix(context, output_owner.release()));
      return parse_arithmetic_tail_from_atom(context, output_owner.release());
    } break;

    case StyioTokenType::TOK_LPAREN: {
      if (has_linebreak_before_current_token_latest(context)) {
        break;
      }
      output_owner.reset(parse_callable_call_suffix_latest(context, output_owner.release()));
      return parse_arithmetic_tail_from_atom(context, output_owner.release());
    } break;

    case StyioTokenType::YIELD_PIPE: {
      if (!g_apply_pipe_tail_enabled_latest || has_linebreak_before_current_token_latest(context)) {
        break;
      }
      context.move_forward(1, "apply_pipe(<|)");
      context.skip();
      StyioAST* arg = nullptr;
      {
        ApplyPipeTailDisableScopeLatest scope;
        arg = parse_fallback_expr(context);
      }
      output_owner.reset(make_callable_apply_latest(output_owner.release(), arg));
      return parse_arithmetic_tail_from_atom(context, output_owner.release());
    } break;

    default:
      break;
  }

  return output_owner.release();
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
parse_guard_value_expr_latest(StyioContext& context);

static StyioAST*
parse_arithmetic_expr(StyioContext& context) {
  context.skip();
  switch (context.cur_tok_type()) {
    case StyioTokenType::TOK_MINUS: {
      context.move_forward(1, "arith_unary-");
      context.skip();
      if (StyioAST* negative_literal = parse_negative_numeric_literal_latest(context)) {
        return parse_arithmetic_tail_from_atom(context, negative_literal);
      }
      std::unique_ptr<StyioAST> inner(parse_arithmetic_expr(context));
      std::unique_ptr<StyioAST> zero(IntAST::Create("0"));
      StyioAST* negative_expr = BinOpAST::Create(
        StyioOpType::Binary_Sub,
        zero.get(),
        inner.get()
      );
      zero.release();
      inner.release();
      return parse_arithmetic_tail_from_atom(
        context,
        negative_expr
      );
    } break;

    case StyioTokenType::TOK_PLUS: {
      context.move_forward(1, "arith_unary+");
      context.skip();
      return parse_arithmetic_tail_from_atom(context, parse_arithmetic_expr(context));
    } break;

    case StyioTokenType::TOK_DOLLAR: {
      const auto& tokens = context.get_tokens();
      const size_t cursor = context.get_token_index();
      if (cursor + 1 < tokens.size() && tokens[cursor + 1]->type == StyioTokenType::STRING) {
        return parse_arithmetic_tail_from_atom(
          context,
          parse_fmt_str_token_latest(context, StyioParserEngine::Legacy)
        );
      }
      throw StyioSyntaxError(context.mark_cur_tok(
        "legacy state-resource $state syntax is retired; use @name[-1] resource selectors"
      ));
    } break;

    case StyioTokenType::NAME: {
      const std::string& id = context.cur_tok()->original;
      if (styio_is_bool_literal_name_latest(id)) {
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

    case StyioTokenType::TOK_QUEST: {
      return parse_guard_value_expr_latest(context);
    } break;

    case StyioTokenType::TOK_LPAREN: {
      const auto saved = context.save_cursor();
      context.move_forward(1, "arith(");
      context.skip();
      if (context.check(StyioTokenType::EXTRACTOR)) {
        return parse_parenthesized_instant_pull_latest(
          context,
          StyioTokenType::EXTRACTOR,
          "instant pull needs @file(...), @{...}, or @stdin",
          "expected ')' after instant pull"
        );
      }
      context.restore_cursor(saved);
      return parse_tuple_exprs(context);
    } break;

    case StyioTokenType::TOK_LBOXBRAC: {
      return parse_list_exprs_latest_draft(context);
    } break;

    case StyioTokenType::TOK_AT: {
      context.move_forward(1, "@expr");
      context.skip();
      return parse_arithmetic_tail_from_atom(
        context,
        parse_after_at_common(context, false)
      );
    } break;

    case StyioTokenType::YIELD_PIPE: {
      context.move_forward(1, "yield_pipe_expr");
      context.skip();
      return ReturnAST::Create(parse_fallback_expr(context));
    } break;

    case StyioTokenType::RETURN_PIPE: {
      context.move_forward(1, "return_pipe_expr");
      context.skip();
      return ReturnAST::Create(parse_fallback_expr(context));
    } break;

    default: {
      throw StyioParseError(context.mark_cur_tok("Unknown Expression"));
    } break;
  }
}

static StyioAST*
parse_relational_expr(StyioContext& context) {
  std::unique_ptr<StyioAST> lhs(parse_arithmetic_expr(context));

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
      return lhs.release();
    }

    context.move_forward(1, "rel_op");
    std::unique_ptr<StyioAST> rhs(parse_arithmetic_expr(context));
    StyioAST* comparison = new BinCompAST(ct, lhs.get(), rhs.get());
    lhs.release();
    rhs.release();
    lhs.reset(comparison);
  }
}

static StyioAST*
parse_and_expr(StyioContext& context) {
  std::unique_ptr<StyioAST> lhs(parse_relational_expr(context));

  while (true) {
    context.skip();
    if (not context.check(StyioTokenType::LOGIC_AND)) {
      return lhs.release();
    }
    context.move_forward(1, "&&");
    std::unique_ptr<StyioAST> rhs(parse_relational_expr(context));
    StyioAST* cond = CondAST::Create(LogicType::AND, lhs.get(), rhs.get());
    lhs.release();
    rhs.release();
    lhs.reset(cond);
  }
}

static StyioAST*
parse_or_expr(StyioContext& context) {
  std::unique_ptr<StyioAST> lhs(parse_and_expr(context));

  while (true) {
    context.skip();
    if (not context.check(StyioTokenType::LOGIC_OR)) {
      return lhs.release();
    }
    context.move_forward(1, "||");
    std::unique_ptr<StyioAST> rhs(parse_and_expr(context));
    StyioAST* cond = CondAST::Create(LogicType::OR, lhs.get(), rhs.get());
    lhs.release();
    rhs.release();
    lhs.reset(cond);
  }
}

static StyioAST*
parse_fallback_expr(StyioContext& context) {
  std::unique_ptr<StyioAST> lhs(parse_or_expr(context));
  while (true) {
    context.skip();
    if (not context.check(StyioTokenType::TOK_PIPE)) {
      return lhs.release();
    }
    context.move_forward(1, "fallback|");
    context.skip();
    std::unique_ptr<StyioAST> rhs(parse_or_expr(context));
    StyioAST* fallback = FallbackAST::Create(lhs.get(), rhs.get());
    lhs.release();
    rhs.release();
    lhs.reset(fallback);
  }
}

static StyioAST*
parse_guard_value_expr_latest(StyioContext& context) {
  context.move_forward(1, "guard_expr?");
  context.skip();
  if (not context.check(StyioTokenType::TOK_LPAREN)) {
    throw StyioSyntaxError(context.mark_cur_tok(
      "Expected ( after ? — use ?(cond) => true_value | false_value"
    ));
  }
  context.move_forward(1, "guard_expr(");
  context.skip();
  std::unique_ptr<StyioAST> cond(parse_expr(context));
  context.skip();
  context.match_panic(StyioTokenType::TOK_RPAREN);
  context.skip();

  if (context.try_match(StyioTokenType::ARROW_DOUBLE_RIGHT)) {
    context.skip();
    std::unique_ptr<StyioAST> true_val(parse_or_expr(context));
    context.skip();
    if (not context.try_match(StyioTokenType::TOK_PIPE)) {
      throw StyioSyntaxError(context.mark_cur_tok("Expected | after guard true value"));
    }
    context.skip();
    std::unique_ptr<StyioAST> false_val(parse_or_expr(context));
    StyioAST* merge = WaveMergeAST::Create(cond.get(), true_val.get(), false_val.get());
    cond.release();
    true_val.release();
    false_val.release();
    return merge;
  }

  return parse_arithmetic_tail_from_atom(context, cond.release());
}

static StyioAST*
parse_token_index_suffix(StyioContext& context, StyioAST* base) {
  std::unique_ptr<StyioAST> base_owner(base);
  context.try_match_panic(StyioTokenType::TOK_LBOXBRAC);
  context.skip();

  if (auto* sr = dynamic_cast<StateRefAST*>(base_owner.get())) {
    if (context.check(StyioTokenType::EXTRACTOR)) {
      context.move_forward(1, "retired_state_history_selector");
      context.skip();
      context.try_match_panic(StyioTokenType::TOK_COMMA);
      context.skip();
      std::unique_ptr<StyioAST> dep(parse_fallback_expr(context));
      context.skip();
      context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
      StyioAST* probe = HistoryProbeAST::Create(sr, dep.get());
      base_owner.release();
      dep.release();
      return probe;
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
      std::unique_ptr<StyioAST> win(parse_fallback_expr(context));
      context.skip();
      context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
      StyioAST* intrinsic = SeriesIntrinsicAST::Create(base_owner.get(), op, win.get());
      base_owner.release();
      win.release();
      return intrinsic;
    }
  }

  if (context.check(StyioTokenType::TOK_QUEST)) {
    context.move_forward(1, "[?");
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_COMMA);
    context.skip();
    std::unique_ptr<StyioAST> cond(parse_or_expr(context));
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
    StyioAST* selector = GuardSelectorAST::Create(base_owner.get(), cond.get());
    base_owner.release();
    cond.release();
    return selector;
  }

  if (context.check(StyioTokenType::MATCH)) {
    context.move_forward(1, "[?=");
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_COMMA);
    context.skip();
    std::unique_ptr<StyioAST> val(parse_fallback_expr(context));
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
    StyioAST* probe = EqProbeAST::Create(base_owner.get(), val.get());
    base_owner.release();
    val.release();
    return probe;
  }

  std::unique_ptr<StyioAST> idx(parse_fallback_expr(context));
  context.skip();
  context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
  StyioAST* access = new ListOpAST(StyioNodeType::Access_By_Index, base_owner.get(), idx.get());
  base_owner.release();
  idx.release();
  return access;
}

static BlockAST*
parse_loop_body_clause(StyioContext& context) {
  context.skip();
  if (context.check(StyioTokenType::TOK_LCURBRAC)) {
    return parse_block_only(context);
  }
  std::vector<StyioAST*> one;
  one.push_back(parse_stmt_or_expr_legacy(context));
  return BlockAST::Create(std::move(one));
}

static InfiniteLoopAST*
parse_infinite_conditional_loop_after_iterator(StyioContext& context) {
  context.skip();
  if (!context.match(StyioTokenType::TOK_QUEST)) {
    throw StyioSyntaxError(
      context.mark_cur_tok("expected ?(condition) => after [...] >>")
    );
  }
  context.skip();
  context.try_match_panic(StyioTokenType::TOK_LPAREN);
  context.skip();
  StyioAST* cond = parse_fallback_expr(context);
  context.skip();
  context.try_match_panic(StyioTokenType::TOK_RPAREN);
  context.skip();
  context.try_match_panic(StyioTokenType::ARROW_DOUBLE_RIGHT);
  context.skip();
  return InfiniteLoopAST::CreateWhile(cond, parse_loop_body_clause(context));
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
            throw StyioSyntaxError(
              context.mark_cur_tok("expected hash tag name after # in iterator sequence")
            );
          }
        }
        else {
          throw StyioSyntaxError(
            context.mark_cur_tok("iterator sequence expects another #tag after >")
          );
        }
      }

      return IterSeqAST::Create(collection, hash_tags);
    }
    else {
      throw StyioSyntaxError(
        context.mark_cur_tok("expected #(param...) or #tag after >> in iterator")
      );
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
      body_ast = parse_stmt_or_expr_legacy(context);
    }
    return StreamZipAST::Create(collection, params, collection_b, params_b, body_ast);
  }

  if (context.try_match(StyioTokenType::ARROW_DOUBLE_RIGHT) /* => */) {
    context.skip();
    if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
      return IteratorAST::Create(collection, params, parse_block_only(context));
    }
    return IteratorAST::Create(collection, params, parse_stmt_or_expr_legacy(context));
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
          throw StyioSyntaxError(
            context.mark_cur_tok("expected hash tag name after # in iterator sequence")
          );
        }
      }
      else {
        throw StyioSyntaxError(
          context.mark_cur_tok("iterator sequence expects another #tag after >")
        );
      }
    } while (context.try_match(StyioTokenType::TOK_RANGBRAC) /* > */);

    return IterSeqAST::Create(collection, params, hash_tags);
  }

  return IteratorAST::Create(collection, params);
}

static StyioAST*
parse_expr_postfix(StyioContext& context, StyioAST* lhs) {
  std::unique_ptr<StyioAST> lhs_owner(lhs);
  while (true) {
    context.skip();
    if (context.match(StyioTokenType::MATCH)) {
      context.skip();
      if (not context.check(StyioTokenType::TOK_LCURBRAC)) {
        throw StyioSyntaxError(context.mark_cur_tok("?= must be followed by {"));
      }
      std::unique_ptr<CasesAST> cases(parse_cases_only_latest(context));
      StyioAST* match = MatchCasesAST::make(lhs_owner.get(), cases.get());
      lhs_owner.release();
      cases.release();
      lhs_owner.reset(match);
      continue;
    }
    if (context.check(StyioTokenType::ITERATOR)) {
      const StyioDoubleRightContinuationOps ops{
        parse_infinite_conditional_loop_after_iterator,
        parse_iterator_tail,
        "unsupported '>>' continuation",
      };
      lhs_owner.reset(parse_double_right_continuation_latest(context, lhs_owner.release(), ops));
      continue;
    }
    if (context.match(StyioTokenType::ARROW_SINGLE_RIGHT)) {
      lhs_owner.reset(parse_resource_redirect_tail_latest(context, lhs_owner.release()));
      continue;
    }
    if (context.match(StyioTokenType::TOK_DOT) && !has_linebreak_before_current_token_latest(context)) {
      context.skip();
      if (dynamic_cast<FuncCallAST*>(lhs_owner.get()) != nullptr
          || dynamic_cast<AttrAST*>(lhs_owner.get()) != nullptr) {
        throw StyioSyntaxError(context.mark_cur_tok("dot-chain calls are not supported"));
      }
      if (!context.check(StyioTokenType::NAME)) {
        throw StyioSyntaxError(context.mark_cur_tok("expected name after ."));
      }
      std::unique_ptr<NameAST> member(parse_name_unsafe(context));
      context.skip_spaces_no_linebreak();
      if (context.check(StyioTokenType::TOK_LPAREN)) {
        lhs_owner.reset(parse_call(context, member.release(), lhs_owner.release()));
      }
      else {
        StyioAST* attr = AttrAST::Create(lhs_owner.get(), member.get());
        lhs_owner.release();
        member.release();
        lhs_owner.reset(attr);
      }
      continue;
    }
    if (context.check(StyioTokenType::TOK_LPAREN) && !has_linebreak_before_current_token_latest(context)) {
      lhs_owner.reset(parse_callable_call_suffix_latest(context, lhs_owner.release()));
      continue;
    }
    if (lhs_owner && lhs_owner->getNodeType() == StyioNodeType::Infinite) {
      if (context.match(StyioTokenType::TOK_QUEST)) {
        throw StyioSyntaxError(
          context.mark_cur_tok("conditional loop syntax is [...] >> ?(condition) => { ... }")
        );
      }
      if (context.match(StyioTokenType::ARROW_DOUBLE_RIGHT)) {
        context.skip();
        std::unique_ptr<BlockAST> body(parse_loop_body_clause(context));
        StyioAST* loop = InfiniteLoopAST::CreateInfinite(body.get());
        lhs_owner.reset(loop);
        body.release();
        continue;
      }
    }
    break;
  }
  return lhs_owner.release();
}

StyioAST*
parse_binop_item(StyioContext& context) {
  StyioAST* output = nullptr;

  context.skip();
  switch (context.cur_tok_type()) {
    case StyioTokenType::NAME: {
      const std::string& id = context.cur_tok()->original;
      if (styio_is_bool_literal_name_latest(id)) {
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
      const auto& tokens = context.get_tokens();
      const size_t cursor = context.get_token_index();
      if (cursor + 1 < tokens.size() && tokens[cursor + 1]->type == StyioTokenType::STRING) {
        output = parse_fmt_str_token_latest(context, StyioParserEngine::Legacy);
        output = parse_arithmetic_tail_from_atom(context, output);
        break;
      }
      throw StyioSyntaxError(context.mark_cur_tok(
        "legacy state-resource $state syntax is retired; use @name[-1] resource selectors"
      ));
    } break;

    case StyioTokenType::TOK_LPAREN: {
      context.move_forward(1, "binop_item(");
      context.skip();
      if (context.check(StyioTokenType::EXTRACTOR)) {
        output = parse_parenthesized_instant_pull_latest(
          context,
          StyioTokenType::EXTRACTOR,
          "instant pull needs @file(...), @{...}, or @stdin",
          "expected ')' after instant pull"
        );
        output = parse_arithmetic_tail_from_atom(context, output);
      }
      else {
        std::unique_ptr<StyioAST> output_owner(parse_fallback_expr(context));
        context.skip();
        context.try_match_panic(StyioTokenType::TOK_RPAREN);
        output = parse_arithmetic_tail_from_atom(context, output_owner.release());
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

  std::unique_ptr<StyioAST> first(parse_fallback_expr(context));
  context.skip();

  if (context.check(StyioTokenType::TOK_RPAREN)) {
    context.move_forward(1, "paren_close");
    return parse_arithmetic_tail_from_atom(context, first.release());
  }

  vector<std::unique_ptr<StyioAST>> elem_owners;
  elem_owners.push_back(std::move(first));

  while (context.try_match(StyioTokenType::TOK_COMMA)) {
    context.skip();
    elem_owners.emplace_back(parse_fallback_expr(context));
    context.skip();
  }

  context.try_match_panic(StyioTokenType::TOK_RPAREN);

  vector<StyioAST*> elems;
  elems.reserve(elem_owners.size());
  for (auto& elem : elem_owners) {
    elems.push_back(elem.get());
  }
  std::unique_ptr<TupleAST> the_tuple(TupleAST::Create(elems));
  for (auto& elem : elem_owners) {
    elem.release();
  }

  context.skip();

  switch (context.cur_tok_type()) {
    case StyioTokenType::ITERATOR: {
      return parse_iterator_only_latest(context, the_tuple.release());
    } break;

    default:
      break;
  }

  return the_tuple.release();
}

StyioAST*
parse_expr(StyioContext& context) {
  enforce_parser_delimiter_budget_latest(context, "expression");
  /* Keep postfix tails after ||/&&; <~ and ~> are reserved at token level. */
  return parse_expr_postfix(
    context,
    parse_arithmetic_tail_from_atom(context, parse_fallback_expr(context))
  );
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
parse_list_exprs_latest_draft(StyioContext& context) {
  enforce_parser_delimiter_budget_latest(context, "list expression");
  std::vector<std::unique_ptr<StyioAST>> expr_owners;
  auto release_exprs = [&]() -> vector<StyioAST*>
  {
    vector<StyioAST*> exprs;
    exprs.reserve(expr_owners.size());
    for (auto& owner : expr_owners) {
      exprs.push_back(owner.release());
    }
    return exprs;
  };
  auto parse_list_elem_expr = [&](std::initializer_list<StyioTokenType> allowed_follow) -> StyioAST*
  {
    auto attempt = try_parse_expr_subset_until_latest(context, allowed_follow);
    if (attempt.status == ParseAttemptStatus::Parsed) {
      return attempt.node;
    }
    if (attempt.status == ParseAttemptStatus::Fatal) {
      std::rethrow_exception(attempt.error);
    }
    enforce_nightly_internal_legacy_bridge_budget_latest(context, "list expression fallback");
    return parse_expr(context);
  };

  context.move_forward(1); /* [ */

  context.skip();
  if (context.match(StyioTokenType::ELLIPSIS)) {
    /* match() already consumed the ellipsis token */
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
    return new InfiniteAST();
  }

  if (context.match(StyioTokenType::TOK_RBOXBRAC) /* ] */) {
    return ListAST::Create({});
  }

  std::unique_ptr<StyioAST> first_expr(parse_list_elem_expr(
    {StyioTokenType::ELLIPSIS, StyioTokenType::TOK_COMMA, StyioTokenType::TOK_RBOXBRAC}
  ));
  context.skip();

  if (context.match(StyioTokenType::ELLIPSIS)) {
    context.skip();
    std::unique_ptr<StyioAST> last_expr(parse_list_elem_expr({StyioTokenType::TOK_RBOXBRAC}));
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
    return new RangeAST(first_expr.release(), last_expr.release(), IntAST::Create("1"));
  }

  expr_owners.push_back(std::move(first_expr));

  while (context.try_match(StyioTokenType::TOK_COMMA) /* , */) {
    context.skip();

    if (context.match(StyioTokenType::TOK_RBOXBRAC) /* ] */) {
      return ListAST::Create(release_exprs());
    }

    expr_owners.emplace_back(parse_list_elem_expr({StyioTokenType::TOK_COMMA, StyioTokenType::TOK_RBOXBRAC}));
    context.skip();
  }

  context.try_match_panic(StyioTokenType::TOK_RBOXBRAC); /* ] */

  return ListAST::Create(release_exprs());
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
  std::unique_ptr<NameAST> func_name_owner(func_name);
  std::unique_ptr<StyioAST> callee_owner(callee);
  context.try_match_panic(StyioTokenType::TOK_LPAREN); /* ( */

  vector<std::unique_ptr<StyioAST>> arg_owners;
  while (not context.check(StyioTokenType::TOK_RPAREN) /* ) */) {
    arg_owners.emplace_back(parse_expr(context));
    context.try_match(StyioTokenType::TOK_COMMA); /* , */
    context.skip();
  }

  context.try_match_panic(StyioTokenType::TOK_RPAREN); /* ) */

  vector<StyioAST*> args;
  args.reserve(arg_owners.size());
  for (auto& arg : arg_owners) {
    args.push_back(arg.get());
  }
  FuncCallAST* call = nullptr;
  if (callee) {
    call = FuncCallAST::Create(callee_owner.get(), func_name_owner.get(), args);
  }
  else {
    call = FuncCallAST::Create(func_name_owner.get(), args);
  }
  func_name_owner.release();
  callee_owner.release();
  for (auto& arg : arg_owners) {
    arg.release();
  }
  return call;
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
  throw StyioParseError("parse_loop_or_iter: non-infinite iterator");
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
StyioAST*
parse_binop_rhs(
  StyioContext& context,
  StyioAST* lhs_ast,
  StyioOpType curr_token
) {
  std::unique_ptr<StyioAST> lhs_owner(lhs_ast);

  context.skip();
  std::unique_ptr<StyioAST> rhs_owner(parse_binop_item(context));

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
      StyioAST* output = reassociate_add_into_resource_sink_latest_draft(
        curr_token,
        lhs_owner.get(),
        rhs_owner.get()
      );
      lhs_owner.release();
      rhs_owner.release();
      return output;
    } break;
  }

  if (next_token > curr_token) {
    std::unique_ptr<StyioAST> nested_rhs(parse_binop_rhs(
      context,
      rhs_owner.release(),
      next_token
    ));
    StyioAST* output = BinOpAST::Create(
      curr_token,
      lhs_owner.get(),
      nested_rhs.get()
    );
    lhs_owner.release();
    nested_rhs.release();
    return output;
  }
  else {
    std::unique_ptr<StyioAST> combined_lhs(
      reassociate_add_into_resource_sink_latest_draft(
        curr_token,
        lhs_owner.get(),
        rhs_owner.get()
      )
    );
    lhs_owner.release();
    rhs_owner.release();
    return parse_binop_rhs(context, combined_lhs.release(), next_token);
  }
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
  const StyioHashFunctionParserOps ops{
    false,
    "expected function name after #",
    "hash_ret_tuple_open",
    "hash_ret_colon",
    "hash_walrus",
    "hash_equal",
    "hash_arrow",
    "hash_match",
    "hash_assign_match",
    parse_expr,
    parse_stmt_or_expr_legacy,
    [](StyioContext& inner) -> StyioAST*
    {
      return parse_block_with_forward(inner);
    },
    parse_cases_only_latest,
    parse_iterator_with_forward,
  };
  return parse_hash_function_common_latest(context, ops);
}

std::vector<ParamAST*>
parse_params(StyioContext& context) {
  std::vector<ParamAST*> params;

  context.try_match(StyioTokenType::TOK_HASH); /* # */

  context.try_match(StyioTokenType::TOK_LPAREN); /* ( */

  do {
    context.skip();
    if (context.check(StyioTokenType::NAME)) {
      std::unique_ptr<NameAST> var_name(parse_name(context));

      context.skip();
      if (context.match(StyioTokenType::TOK_COLON) /* : */) {
        context.skip();
        std::unique_ptr<TypeAST> var_type(parse_styio_type(context));

        std::unique_ptr<ParamAST> param(ParamAST::Create(
          var_name.get(),
          var_type.get()
        ));
        var_name.release();
        var_type.release();
        params.push_back(param.get());
        param.release();
      }
      else {
        std::unique_ptr<ParamAST> param(ParamAST::Create(var_name.get()));
        var_name.release();
        params.push_back(param.get());
        param.release();
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
          following_exprs.push_back(parse_stmt_or_expr_legacy(context));
        }
      } break;

      /* ? Conditionals */
      case StyioTokenType::TOK_QUEST: {
        throw StyioParseError("parse_forward(Conditionals)");
      } break;

      /* ?= Match Cases */
      case StyioTokenType::MATCH: {
        context.move_forward(1);

        context.skip();
        /* { _ => ... } Cases */
        if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
          following_exprs.push_back(parse_cases_only_latest(context));
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

  block->set_followings(parse_forward_as_list(context));

  return block;
}

CasesAST*
parse_cases_only_latest(StyioContext& context) {
  vector<std::pair<std::unique_ptr<StyioAST>, std::unique_ptr<StyioAST>>> case_owners;
  std::unique_ptr<StyioAST> default_owner;

  context.try_match_panic(StyioTokenType::TOK_LCURBRAC); /* { */

  while (not context.match(StyioTokenType::TOK_RCURBRAC) /* } */) {
    context.skip();
    if (is_default_case_wildcard_latest(context) /* _ */) {
      context.move_forward(1, "legacy_cases:default_wildcard");
      context.skip();
      if (context.match(StyioTokenType::ARROW_DOUBLE_RIGHT) /* => */) {
        context.skip();
        if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
          default_owner.reset(parse_block_only(context));
        }
        else {
          default_owner.reset(parse_stmt_or_expr_legacy(context));
        }
      }
      else {
        // SyntaxError
        throw StyioSyntaxError("=> not found for default case");
      }
    }
    else {
      // StyioAST* left = parse_cond(context);
      std::unique_ptr<StyioAST> left(parse_expr(context));

      context.skip();
      if (context.match(StyioTokenType::ARROW_DOUBLE_RIGHT) /* => */) {
        std::unique_ptr<StyioAST> right;

        context.skip();
        if (context.check(StyioTokenType::TOK_LCURBRAC) /* { */) {
          right.reset(parse_block_only(context));
        }
        else {
          right.reset(parse_stmt_or_expr_legacy(context));
        }

        case_owners.emplace_back(std::move(left), std::move(right));
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

  if (case_owners.size() == 0) {
    CasesAST* cases = CasesAST::Create(default_owner.get());
    default_owner.release();
    return cases;
  }
  else {
    vector<std::pair<StyioAST*, StyioAST*>> case_pairs;
    case_pairs.reserve(case_owners.size());
    for (auto& entry : case_owners) {
      case_pairs.emplace_back(entry.first.get(), entry.second.get());
    }
    CasesAST* cases = CasesAST::Create(case_pairs, default_owner.get());
    for (auto& entry : case_owners) {
      entry.first.release();
      entry.second.release();
    }
    default_owner.release();
    return cases;
  }
}

StyioAST*
parse_iterator_with_forward(
  StyioContext& context,
  StyioAST* collection
) {
  std::unique_ptr<StyioAST> output(parse_iterator_only_latest(context, collection));

  auto forward_following = parse_forward_as_list(context);
  std::vector<std::unique_ptr<StyioAST>> forward_following_owned;
  forward_following_owned.reserve(forward_following.size());
  for (auto* node : forward_following) {
    forward_following_owned.emplace_back(node);
  }
  if (!forward_following.empty()) {
    auto* iterator_output = dynamic_cast<IteratorAST*>(output.get());
    if (iterator_output == nullptr) {
      throw StyioParseError(
        context.mark_cur_tok("forward clauses are only supported for plain iterators")
      );
    }
    if (dynamic_cast<CheckEqualAST*>(forward_following.front()) != nullptr
        || dynamic_cast<CasesAST*>(forward_following.front()) != nullptr) {
      throw StyioParseError(
        context.mark_cur_tok("iterator '?=' forward clauses are not supported in function definitions")
      );
    }
    if (forward_following.size() > 1) {
      throw StyioParseError(
        context.mark_cur_tok("iterator forward chains with multiple clauses are not supported")
      );
    }
    for (auto& node : forward_following_owned) {
      node.release();
    }
    iterator_output->append_followings(std::move(forward_following));
  }

  return output.release();
}

StyioAST*
parse_iterator_only_latest(
  StyioContext& context,
  StyioAST* collection
) {
  std::unique_ptr<StyioAST> collection_owner(collection);
  context.try_match_panic(StyioTokenType::ITERATOR);
  StyioAST* iterator = parse_iterator_tail(context, collection_owner.get());
  if (dynamic_cast<IteratorAST*>(iterator) != nullptr
      || dynamic_cast<StreamZipAST*>(iterator) != nullptr) {
    collection_owner.release();
  }
  return iterator;
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
    prev_op->setNextOp(curr_op);
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
parse_stmt_or_expr_legacy(
  StyioContext& context
) {
  context.skip();

  switch (context.cur_tok_type()) {
    /* var_name / func_name */
    case StyioTokenType::NAME: {
      const std::string& id = context.cur_tok()->original;
      if (styio_is_bool_literal_name_latest(id)) {
        return parse_expr(context);
      }

      auto saved = context.save_cursor();
      context.move_forward(1, "stmt_name_probe");
      context.skip();
      StyioTokenType nt = context.cur_tok_type();

      if (nt == StyioTokenType::TOK_COMMA) {
        std::vector<std::string> target_names{id};
        while (context.cur_tok_type() == StyioTokenType::TOK_COMMA) {
          context.move_forward(1, "typed_stdin_pull:target_comma");
          context.skip();
          if (context.cur_tok_type() != StyioTokenType::NAME) {
            throw StyioSyntaxError("typed stdin pull targets must be names");
          }
          target_names.push_back(context.cur_tok()->original);
          context.move_forward(1, "typed_stdin_pull:target_name");
          context.skip();
        }
        if (context.cur_tok_type() == StyioTokenType::ARROW_SINGLE_LEFT) {
          context.move_forward(1, "typed_stdin_pull<-");
          context.skip();
          StyioAST* pull = try_parse_typed_stdin_pull_bind_latest(context, std::move(target_names));
          if (pull == nullptr) {
            throw StyioSyntaxError("multi-target stdin pull requires @stdin : (...)");
          }
          return pull;
        }
        context.restore_cursor(saved);
        return parse_expr_postfix(context, parse_name_and_following_unsafe(context));
      }

      if (nt == StyioTokenType::TOK_EQUAL) {
        context.move_forward(1, "flex_bind=");
        std::unique_ptr<VarAST> var(VarAST::Create(NameAST::Create(id)));
        std::unique_ptr<StyioAST> value(parse_expr(context));
        auto* bind = FlexBindAST::Create(var.get(), value.get());
        var.release();
        value.release();
        return bind;
      }

      if (nt == StyioTokenType::WALRUS) {
        context.move_forward(1, "final_bind:=");
        context.skip();
        std::unique_ptr<VarAST> var(VarAST::Create(NameAST::Create(id)));
        std::unique_ptr<StyioAST> value(parse_expr(context));
        auto* bind = FinalBindAST::Create(var.get(), value.get());
        var.release();
        value.release();
        return bind;
      }

      if (nt == StyioTokenType::ARROW_DOUBLE_RIGHT) {
        context.move_forward(1, "resource_order=>");
        context.skip();
        std::unique_ptr<NameAST> resource_name(NameAST::Create(id));
        std::unique_ptr<StyioAST> order_expr(parse_expr(context));
        auto* order = ResourceOrderAST::Create(resource_name.get(), order_expr.get());
        resource_name.release();
        order_expr.release();
        return order;
      }

      if (nt == StyioTokenType::ARROW_SINGLE_LEFT) {
        context.move_forward(1, "handle<-");
        context.skip();
        if (StyioAST* pull = try_parse_typed_stdin_pull_bind_latest(context, {id})) {
          return pull;
        }
        std::unique_ptr<VarAST> var(VarAST::Create(NameAST::Create(id)));
        std::unique_ptr<StyioAST> resource(parse_resource_file_atom_latest(context));
        auto* acquire = HandleAcquireAST::Create(var.get(), resource.get());
        var.release();
        resource.release();
        return acquire;
      }

      if (nt == StyioTokenType::EXTRACTOR) {
        context.move_forward(1, "data<<file");
        return parse_resource_extractor_write_tail_latest(context, NameAST::Create(id));
      }

      if (nt == StyioTokenType::TOK_COLON) {
        context.move_forward(1, "final_bind:");
        context.skip();
        std::unique_ptr<TypeAST> ty(parse_styio_type(context));
        context.skip();
        if (not context.match(StyioTokenType::WALRUS)) {
          throw StyioSyntaxError(context.mark_cur_tok("expected := after type in binding"));
        }
        context.skip();
        std::unique_ptr<VarAST> var(VarAST::Create(NameAST::Create(id), ty.get()));
        ty.release();
        std::unique_ptr<StyioAST> value(parse_expr(context));
        auto* bind = FinalBindAST::Create(var.get(), value.get());
        var.release();
        value.release();
        return bind;
      }

      StyioOpType cop = styio_compound_assign_op_latest(nt);

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

    case StyioTokenType::TOK_DOLLAR: {
      return parse_expr(context);
    } break;

    /* @ */
    case StyioTokenType::TOK_AT: {
      return parse_at_stmt_or_expr_latest(context);
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
      while (context.check(StyioTokenType::TOK_HAT)) {
        context.move_forward(1, "break^");
      }
      return BreakAST::Create(1u);
    } break;

    case StyioTokenType::TOK_QUEST: {
      context.move_forward(1, "guard?");
      context.skip();
      context.match_panic(StyioTokenType::TOK_LPAREN);
      context.skip();
      std::unique_ptr<StyioAST> cond_expr;
      auto cond_start = context.save_cursor();
      try {
        cond_expr.reset(parse_expr_subset_nightly(context));
      }
      catch (const StyioParserResourceLimitError&) {
        context.restore_cursor(cond_start);
        throw;
      }
      catch (const std::exception&) {
        context.restore_cursor(cond_start);
        cond_expr.reset(parse_expr(context));
      }
      context.skip();
      context.match_panic(StyioTokenType::TOK_RPAREN);
      context.skip();
      context.match_panic(StyioTokenType::ARROW_DOUBLE_RIGHT);
      context.skip();

      std::unique_ptr<CondAST> cond;
      if (auto* parsed_cond = dynamic_cast<CondAST*>(cond_expr.get())) {
        cond_expr.release();
        cond.reset(parsed_cond);
      }
      else {
        cond.reset(CondAST::Create(LogicType::RAW, cond_expr.get()));
        cond_expr.release();
      }

      std::unique_ptr<StyioAST> then_body(
        context.cur_tok_type() == StyioTokenType::TOK_LCURBRAC
          ? parse_stmt_subset_nightly(context)
          : parse_stmt_or_expr_legacy(context)
      );
      context.skip();
      if (context.try_match(StyioTokenType::TOK_PIPE)) {
        context.skip();
        if (context.cur_tok_type() != StyioTokenType::TOK_LCURBRAC) {
          throw StyioSyntaxError(
            context.mark_cur_tok("expected fallback block after conditional guard |")
          );
        }
        std::unique_ptr<StyioAST> else_body(parse_stmt_subset_nightly(context));
        auto* flow = new CondFlowAST(
          StyioNodeType::CondFlow_Both,
          cond.get(),
          then_body.get(),
          else_body.get()
        );
        cond.release();
        then_body.release();
        else_body.release();
        return flow;
      }

      auto* flow = new CondFlowAST(StyioNodeType::CondFlow_True, cond.get(), then_body.get());
      cond.release();
      then_body.release();
      return flow;
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

    case StyioTokenType::RETURN_PIPE: {
      context.move_forward(1, "stmt_return_pipe");
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

  throw StyioParseError(context.mark_cur_tok("Reached The End of `parse_stmt_or_expr_legacy()`"));
}

BlockAST*
parse_block_only(StyioContext& context) {
  vector<StyioAST*> stmts;

  context.match_panic(StyioTokenType::TOK_LCURBRAC); /* { */

  while (
    context.cur_tok_type() != StyioTokenType::TOK_EOF
  ) {
    consume_statement_separators_latest(context);
    if (context.cur_tok_type() == StyioTokenType::TOK_EOF) {
      break;
    }
    if (context.match(StyioTokenType::TOK_RCURBRAC) /* } */) {
      return BlockAST::Create(std::move(stmts));
    }
    else {
      const auto statement_start = context.save_cursor();
      try {
        stmts.push_back(parse_stmt_or_expr_legacy(context));
      }
      catch (...) {
        if (!parser_handle_recovery_latest(context, statement_start, parser_recovery_message_latest())) {
          throw;
        }
      }
    }
  }

  context.try_match_panic(StyioTokenType::TOK_RCURBRAC); /* } */

  return BlockAST::Create(std::move(stmts));
}

MainBlockAST*
parse_main_block_legacy(StyioContext& context) {
  std::vector<std::unique_ptr<StyioAST>> statements_owned;

  while (true) {
    consume_statement_separators_latest(context);
    const auto statement_start = context.save_cursor();
    StyioAST* stmt = nullptr;
    try {
      if (matches_legacy_string_list_import_latest(context)) {
        throw StyioSyntaxError(
          context.mark_cur_tok("legacy import syntax [\"pkg\"] is deprecated; use @import { pkg }")
        );
      }
      stmt = parse_stmt_or_expr_legacy(context);
    }
    catch (...) {
      if (parser_handle_recovery_latest(context, statement_start, parser_recovery_message_latest())) {
        continue;
      }
      throw;
    }

    if ((stmt->getNodeType()) == StyioNodeType::End) {
      delete stmt;
      break;
    }
    else if ((stmt->getNodeType()) == StyioNodeType::Comment) {
      delete stmt;
      continue;
    }
    else {
      statements_owned.emplace_back(stmt);
    }
  }

  vector<StyioAST*> statements;
  statements.reserve(statements_owned.size());
  for (auto& owned : statements_owned) {
    statements.push_back(owned.release());
  }
  return MainBlockAST::Create(statements);
}

bool
styio_parse_parser_engine_latest(const std::string& raw, StyioParserEngine& out) {
  if (raw == "legacy") {
    out = StyioParserEngine::Legacy;
    return true;
  }
  if (raw == "nightly") {
    out = StyioParserEngine::Nightly;
    return true;
  }
  if (raw == "new") {
    out = StyioParserEngine::Nightly;
    return true;
  }
  return false;
}

const char*
styio_parser_engine_name_latest(StyioParserEngine engine) {
  switch (engine) {
    case StyioParserEngine::Legacy:
      return "legacy";
    case StyioParserEngine::Nightly:
      return "nightly";
  }
  return "legacy";
}

static MainBlockAST*
parse_main_block_shadow_nightly(StyioContext& context, StyioParserRouteStats* route_stats) {
  if (route_stats != nullptr) {
    *route_stats = StyioParserRouteStats{};
  }
  ParserRouteStatsScopeLatestDraft route_stats_scope(context, route_stats);

  std::vector<std::unique_ptr<StyioAST>> statements_owned;
  while (true) {
    consume_statement_separators_latest(context);
    if (context.cur_tok_type() == StyioTokenType::TOK_EOF) {
      break;
    }

    const auto statement_start = context.save_cursor();
    StyioAST* stmt = nullptr;
    try {
      if (matches_legacy_string_list_import_latest(context)) {
        throw StyioSyntaxError(
          context.mark_cur_tok("legacy import syntax [\"pkg\"] is deprecated; use @import { pkg }")
        );
      }
      auto attempt = try_parse_stmt_subset_nightly(context);
      if (attempt.status == ParseAttemptStatus::Parsed) {
        stmt = attempt.node;
        if (route_stats != nullptr) {
          route_stats->nightly_subset_statements += 1;
        }
      }
      else if (attempt.status == ParseAttemptStatus::Fatal) {
        std::rethrow_exception(attempt.error);
      }
      else if (route_stats != nullptr && styio_parser_stmt_subset_start_nightly(context.cur_tok_type())) {
        route_stats->nightly_declined_statements += 1;
      }

      if (stmt == nullptr) {
        stmt = parse_stmt_or_expr_legacy(context);
        if (route_stats != nullptr) {
          route_stats->legacy_fallback_statements += 1;
        }
      }
    }
    catch (...) {
      if (parser_handle_recovery_latest(context, statement_start, parser_recovery_message_latest())) {
        continue;
      }
      throw;
    }

    if ((stmt->getNodeType()) == StyioNodeType::End) {
      delete stmt;
      break;
    }
    if ((stmt->getNodeType()) == StyioNodeType::Comment) {
      delete stmt;
      continue;
    }
    statements_owned.emplace_back(stmt);
  }

  std::vector<StyioAST*> statements;
  statements.reserve(statements_owned.size());
  for (auto& owned : statements_owned) {
    statements.push_back(owned.release());
  }
  return MainBlockAST::Create(statements);
}

MainBlockAST*
parse_main_block_with_engine_latest(
  StyioContext& context,
  StyioParserEngine engine,
  StyioParserRouteStats* route_stats,
  StyioParseMode mode
) {
  ParseModeScopeLatestDraft parse_mode_scope(context, mode);
  switch (engine) {
    case StyioParserEngine::Legacy:
      if (route_stats != nullptr) {
        *route_stats = StyioParserRouteStats{};
      }
      return parse_main_block_legacy(context);
    case StyioParserEngine::Nightly:
      return parse_main_block_shadow_nightly(context, route_stats);
  }
  if (route_stats != nullptr) {
    *route_stats = StyioParserRouteStats{};
  }
  return parse_main_block_legacy(context);
}
