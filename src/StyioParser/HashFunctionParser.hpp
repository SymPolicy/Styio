#pragma once
#ifndef STYIO_HASH_FUNCTION_PARSER_H_
#define STYIO_HASH_FUNCTION_PARSER_H_

#include <string>
#include <variant>
#include <vector>

#include "Parser.hpp"

struct StyioHashFunctionParserOps
{
  bool require_name = false;
  const char* missing_name_message = "expected function name after #";
  const char* ret_tuple_label = "hash_ret_tuple_open";
  const char* ret_colon_label = "hash_ret_colon";
  const char* assign_walrus_label = "hash_walrus";
  const char* assign_equal_label = "hash_equal";
  const char* arrow_label = "hash_arrow";
  const char* match_label = "hash_match";
  const char* assign_match_label = "hash_assign_match";

  StyioAST* (*parse_expression)(StyioContext& context) = nullptr;
  StyioAST* (*parse_statement)(StyioContext& context) = nullptr;
  StyioAST* (*parse_block)(StyioContext& context) = nullptr;
  CasesAST* (*parse_cases)(StyioContext& context) = nullptr;
  StyioAST* (*parse_iterator)(StyioContext& context, StyioAST* collection) = nullptr;
};

inline std::variant<TypeAST*, TypeTupleAST*>
parse_hash_return_type_common_latest(
  StyioContext& context,
  const StyioHashFunctionParserOps& ops
) {
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_LPAREN) {
    context.move_forward(1, ops.ret_tuple_label);
    std::vector<TypeAST*> types;
    context.skip();
    while (context.cur_tok_type() != StyioTokenType::TOK_RPAREN) {
      types.push_back(parse_styio_type(context));
      context.skip();
      if (context.cur_tok_type() == StyioTokenType::TOK_RPAREN) {
        break;
      }
      context.try_match_panic(StyioTokenType::TOK_COMMA);
      context.skip();
    }
    context.try_match_panic(StyioTokenType::TOK_RPAREN);
    return TypeTupleAST::Create(types);
  }
  return parse_styio_type(context);
}

inline StyioAST*
parse_hash_function_common_latest(
  StyioContext& context,
  const StyioHashFunctionParserOps& ops
) {
  context.match_panic(StyioTokenType::TOK_HASH);
  context.skip();

  NameAST* tag_name = nullptr;
  if (context.cur_tok_type() == StyioTokenType::NAME) {
    tag_name = parse_name_unsafe(context);
  }
  else if (ops.require_name) {
    throw StyioSyntaxError(ops.missing_name_message);
  }

  std::vector<ParamAST*> params;
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_LPAREN) {
    params = parse_params(context);
  }

  std::variant<TypeAST*, TypeTupleAST*> ret_type = static_cast<TypeAST*>(nullptr);
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_COLON) {
    context.move_forward(1, ops.ret_colon_label);
    ret_type = parse_hash_return_type_common_latest(context, ops);
  }

  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_LCURBRAC) {
    return FunctionAST::Create(tag_name, false, params, ret_type, ops.parse_block(context));
  }

  if (context.cur_tok_type() == StyioTokenType::MATCH) {
    context.move_forward(1, ops.match_label);
    context.skip();
    if (context.cur_tok_type() == StyioTokenType::TOK_LCURBRAC) {
      return FunctionAST::Create(tag_name, true, params, ret_type, ops.parse_cases(context));
    }

    std::vector<StyioAST*> rvals;
    do {
      rvals.push_back(ops.parse_expression(context));
      context.skip();
    } while (context.try_match(StyioTokenType::TOK_COMMA));
    return FunctionAST::Create(tag_name, true, params, ret_type, CheckEqualAST::Create(rvals));
  }

  context.skip();
  if (context.cur_tok_type() == StyioTokenType::ITERATOR) {
    if (params.size() != 1) {
      throw StyioSyntaxError(
        context.mark_cur_tok("Confusing: The iterator (>>) can not be applied to multiple objects.")
      );
    }
    NameAST* iter_collection = NameAST::Create(params[0]->getName());
    return FunctionAST::Create(
      tag_name,
      true,
      params,
      ret_type,
      ops.parse_iterator(context, iter_collection));
  }

  context.skip();
  bool is_unique = false;
  bool saw_assign = false;
  if (context.cur_tok_type() == StyioTokenType::WALRUS) {
    is_unique = true;
    saw_assign = true;
    context.move_forward(1, ops.assign_walrus_label);
  }
  else if (context.cur_tok_type() == StyioTokenType::TOK_EQUAL) {
    saw_assign = true;
    context.move_forward(1, ops.assign_equal_label);
  }
  else if (context.cur_tok_type() != StyioTokenType::ARROW_DOUBLE_RIGHT) {
    throw StyioSyntaxError(context.mark_cur_tok("expected :=, =, or => in hash function"));
  }

  context.skip();
  if (saw_assign && context.cur_tok_type() == StyioTokenType::TOK_LPAREN) {
    params = parse_params(context);
  }

  context.skip();
  if (saw_assign && context.cur_tok_type() == StyioTokenType::MATCH) {
    if (params.size() != 1) {
      throw StyioSyntaxError(
        context.mark_cur_tok("function match sugar requires exactly one parameter")
      );
    }
    const std::string scrutinee_name = params[0]->getName();
    context.move_forward(1, ops.assign_match_label);
    context.skip();
    if (context.cur_tok_type() != StyioTokenType::TOK_LCURBRAC) {
      throw StyioSyntaxError(
        context.mark_cur_tok("expected case block after function match sugar ?=")
      );
    }
    return FunctionAST::Create(
      tag_name,
      is_unique,
      params,
      ret_type,
      MatchCasesAST::make(NameAST::Create(scrutinee_name), ops.parse_cases(context))
    );
  }

  context.skip();
  if (context.cur_tok_type() == StyioTokenType::ARROW_DOUBLE_RIGHT) {
    context.move_forward(1, ops.arrow_label);
    context.skip();
    if (context.cur_tok_type() == StyioTokenType::TOK_LCURBRAC) {
      return FunctionAST::Create(tag_name, is_unique, params, ret_type, ops.parse_block(context));
    }
    return SimpleFuncAST::Create(tag_name, is_unique, params, ret_type, ops.parse_statement(context));
  }

  if (saw_assign) {
    context.skip();
    return SimpleFuncAST::Create(tag_name, is_unique, params, ret_type, ops.parse_expression(context));
  }

  throw StyioSyntaxError(context.mark_cur_tok("expected => or expression body in hash function"));
}

#endif
