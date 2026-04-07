#pragma once
#ifndef STYIO_NEW_PARSER_EXPR_H_
#define STYIO_NEW_PARSER_EXPR_H_

#include "../StyioException/Exception.hpp"
#include "Parser.hpp"

bool
styio_new_parser_is_expr_subset_token(StyioTokenType type);

bool
styio_new_parser_is_expr_subset_start(StyioTokenType type);

bool
styio_new_parser_is_stmt_subset_token(StyioTokenType type);

bool
styio_new_parser_is_stmt_subset_start(StyioTokenType type);

StyioAST*
parse_expr_new_subset(StyioContext& context);

StyioAST*
parse_stmt_new_subset(StyioContext& context);

MainBlockAST*
parse_main_block_new_subset(StyioContext& context);

#endif
