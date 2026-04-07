#pragma once
#ifndef STYIO_NEW_PARSER_EXPR_H_
#define STYIO_NEW_PARSER_EXPR_H_

#include "../StyioException/Exception.hpp"
#include "Parser.hpp"

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

MainBlockAST*
parse_main_block_subset_nightly(StyioContext& context);

#endif
