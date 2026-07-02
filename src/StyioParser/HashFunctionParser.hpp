#pragma once
#ifndef STYIO_HASH_FUNCTION_PARSER_H_
#define STYIO_HASH_FUNCTION_PARSER_H_

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "Parser.hpp"

namespace styio_hash_parser_detail
{

class HashFunctionOwnedState
{
private:
  std::unique_ptr<NameAST> tag_name_;
  std::vector<std::unique_ptr<ParamAST>> params_;
  std::unique_ptr<TypeAST> ret_type_;
  std::unique_ptr<TypeTupleAST> ret_tuple_;
  bool ret_is_tuple_ = false;

public:
  void set_name(NameAST* name) {
    tag_name_.reset(name);
  }

  NameAST*
  release_name() {
    return tag_name_.release();
  }

  void set_params(std::vector<std::unique_ptr<ParamAST>> params) {
    params_ = std::move(params);
  }

  std::size_t
  params_size() const {
    return params_.size();
  }

  ParamAST*
  param_at(std::size_t index) const {
    return params_.at(index).get();
  }

  std::vector<ParamAST*>
  release_params() {
    std::vector<ParamAST*> params;
    params.reserve(params_.size());
    for (auto& param : params_) {
      params.push_back(param.release());
    }
    params_.clear();
    return params;
  }

  void set_ret_type(std::variant<TypeAST*, TypeTupleAST*> ret_type) {
    ret_type_.reset();
    ret_tuple_.reset();
    ret_is_tuple_ = std::holds_alternative<TypeTupleAST*>(ret_type);
    if (ret_is_tuple_) {
      ret_tuple_.reset(std::get<TypeTupleAST*>(ret_type));
      return;
    }
    ret_type_.reset(std::get<TypeAST*>(ret_type));
  }

  std::variant<TypeAST*, TypeTupleAST*>
  release_ret_type() {
    if (ret_is_tuple_) {
      ret_is_tuple_ = false;
      return ret_tuple_.release();
    }
    return ret_type_.release();
  }
};

} // namespace styio_hash_parser_detail

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
    std::vector<std::unique_ptr<TypeAST>> type_owners;
    context.skip();
    while (context.cur_tok_type() != StyioTokenType::TOK_RPAREN) {
      type_owners.emplace_back(parse_styio_type(context));
      context.skip();
      if (context.cur_tok_type() == StyioTokenType::TOK_RPAREN) {
        break;
      }
      context.try_match_panic(StyioTokenType::TOK_COMMA);
      context.skip();
    }
    context.try_match_panic(StyioTokenType::TOK_RPAREN);
    std::vector<TypeAST*> types;
    types.reserve(type_owners.size());
    for (auto& type : type_owners) {
      types.push_back(type.release());
    }
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

  styio_hash_parser_detail::HashFunctionOwnedState state;
  if (context.cur_tok_type() == StyioTokenType::NAME) {
    state.set_name(parse_name_unsafe(context));
  }
  else if (ops.require_name) {
    throw StyioSyntaxError(ops.missing_name_message);
  }

  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_LPAREN) {
    state.set_params(parse_params(context));
  }

  state.set_ret_type(static_cast<TypeAST*>(nullptr));
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_COLON) {
    context.move_forward(1, ops.ret_colon_label);
    state.set_ret_type(parse_hash_return_type_common_latest(context, ops));
  }

  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_LCURBRAC) {
    std::unique_ptr<StyioAST> body(ops.parse_block(context));
    return FunctionAST::Create(
      state.release_name(),
      false,
      state.release_params(),
      state.release_ret_type(),
      body.release());
  }

  if (context.cur_tok_type() == StyioTokenType::MATCH) {
    context.move_forward(1, ops.match_label);
    context.skip();
    if (context.cur_tok_type() == StyioTokenType::TOK_LCURBRAC) {
      std::unique_ptr<StyioAST> body(ops.parse_cases(context));
      return FunctionAST::Create(
        state.release_name(),
        true,
        state.release_params(),
        state.release_ret_type(),
        body.release());
    }

    std::vector<StyioAST*> rvals;
    std::vector<std::unique_ptr<StyioAST>> rval_owners;
    do {
      rval_owners.emplace_back(ops.parse_expression(context));
      context.skip();
    } while (context.try_match(StyioTokenType::TOK_COMMA));
    rvals.reserve(rval_owners.size());
    for (auto& rval : rval_owners) {
      rvals.push_back(rval.release());
    }
    std::unique_ptr<StyioAST> body(CheckEqualAST::Create(rvals));
    return FunctionAST::Create(
      state.release_name(),
      true,
      state.release_params(),
      state.release_ret_type(),
      body.release());
  }

  context.skip();
  if (context.cur_tok_type() == StyioTokenType::ITERATOR) {
    if (state.params_size() != 1) {
      throw StyioSyntaxError(
        context.mark_cur_tok("Confusing: The iterator (>>) can not be applied to multiple objects.")
      );
    }
    std::unique_ptr<NameAST> iter_collection(NameAST::Create(state.param_at(0)->getName()));
    std::unique_ptr<StyioAST> body(ops.parse_iterator(context, iter_collection.release()));
    return FunctionAST::Create(
      state.release_name(),
      true,
      state.release_params(),
      state.release_ret_type(),
      body.release());
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
    state.set_params(parse_params(context));
  }

  context.skip();
  if (saw_assign && context.cur_tok_type() == StyioTokenType::MATCH) {
    if (state.params_size() != 1) {
      throw StyioSyntaxError(
        context.mark_cur_tok("function match sugar requires exactly one parameter")
      );
    }
    const std::string scrutinee_name = state.param_at(0)->getName();
    context.move_forward(1, ops.assign_match_label);
    context.skip();
    if (context.cur_tok_type() != StyioTokenType::TOK_LCURBRAC) {
      throw StyioSyntaxError(
        context.mark_cur_tok("expected case block after function match sugar ?=")
      );
    }
    std::unique_ptr<NameAST> scrutinee(NameAST::Create(scrutinee_name));
    std::unique_ptr<CasesAST> cases(ops.parse_cases(context));
    std::unique_ptr<StyioAST> body(MatchCasesAST::make(scrutinee.get(), cases.get()));
    scrutinee.release();
    cases.release();
    return FunctionAST::Create(
      state.release_name(),
      is_unique,
      state.release_params(),
      state.release_ret_type(),
      body.release());
  }

  context.skip();
  if (context.cur_tok_type() == StyioTokenType::ARROW_DOUBLE_RIGHT) {
    context.move_forward(1, ops.arrow_label);
    context.skip();
    if (context.cur_tok_type() == StyioTokenType::TOK_LCURBRAC) {
      std::unique_ptr<StyioAST> body(ops.parse_block(context));
      return FunctionAST::Create(
        state.release_name(),
        is_unique,
        state.release_params(),
        state.release_ret_type(),
        body.release());
    }
    std::unique_ptr<StyioAST> statement(ops.parse_statement(context));
    return SimpleFuncAST::Create(
      state.release_name(),
      is_unique,
      state.release_params(),
      state.release_ret_type(),
      statement.release());
  }

  if (saw_assign) {
    context.skip();
    std::unique_ptr<StyioAST> expression(ops.parse_expression(context));
    return SimpleFuncAST::Create(
      state.release_name(),
      is_unique,
      state.release_params(),
      state.release_ret_type(),
      expression.release());
  }

  throw StyioSyntaxError(context.mark_cur_tok("expected => or expression body in hash function"));
}

#endif
