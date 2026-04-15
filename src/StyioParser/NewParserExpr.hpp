#pragma once
#ifndef STYIO_NEW_PARSER_EXPR_H_
#define STYIO_NEW_PARSER_EXPR_H_

#include <exception>

#include "../StyioException/Exception.hpp"
#include "Parser.hpp"

enum class ParseAttemptStatus
{
  Parsed,
  Declined,
  Fatal,
};

template <typename T>
struct ParseAttempt
{
  ParseAttemptStatus status = ParseAttemptStatus::Declined;
  T* node = nullptr;
  std::exception_ptr error;

  static ParseAttempt parsed(T* value) {
    ParseAttempt out;
    out.status = ParseAttemptStatus::Parsed;
    out.node = value;
    return out;
  }

  static ParseAttempt declined() {
    return ParseAttempt {};
  }

  static ParseAttempt fatal(std::exception_ptr ex) {
    ParseAttempt out;
    out.status = ParseAttemptStatus::Fatal;
    out.error = ex;
    return out;
  }
};

bool
styio_parser_expr_subset_token_nightly(StyioTokenType type);

bool
styio_parser_expr_subset_start_nightly(StyioTokenType type);

bool
styio_parser_stmt_subset_token_nightly(StyioTokenType type);

bool
styio_parser_stmt_subset_start_nightly(StyioTokenType type);

StyioAST*
parse_expr_subset_nightly(StyioContext& context);

StyioAST*
parse_stmt_subset_nightly(StyioContext& context);

ParseAttempt<StyioAST>
try_parse_stmt_subset_nightly(StyioContext& context);

MainBlockAST*
parse_main_block_subset_nightly(StyioContext& context);

#endif
