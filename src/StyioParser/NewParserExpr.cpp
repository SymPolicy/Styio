#include "NewParserExpr.hpp"

#include <algorithm>
#include <memory>

#include "ParserLookahead.hpp"

namespace {

BlockAST*
parse_block_only_subset_nightly(StyioContext& context);

PrintAST*
parse_print_nightly(StyioContext& context);

StyioAST*
parse_hash_stmt_nightly(StyioContext& context);

StyioAST*
parse_expr_subset_allowing_follow_latest(
  StyioContext& context,
  std::initializer_list<StyioTokenType> allowed_follow);

std::string
nightly_recovery_message_latest() {
  try {
    throw;
  } catch (const StyioBaseException& ex) {
    return ex.what();
  } catch (const std::exception& ex) {
    return ex.what();
  } catch (...) {
    return "unknown nightly parser failure";
  }
}

bool
nightly_handle_recovery_latest(
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

struct TokenProbeLatest
{
  const std::vector<StyioToken*>& tokens;
  size_t cursor = 0;

  TokenProbeLatest(const std::vector<StyioToken*>& tokens, size_t start) :
      tokens(tokens),
      cursor(styio_skip_trivia_tokens(tokens, start)) {
  }

  explicit TokenProbeLatest(const StyioContext& context) :
      TokenProbeLatest(context.get_tokens(), context.get_token_index()) {
  }

  void skip() {
    cursor = styio_skip_trivia_tokens(tokens, cursor);
  }

  StyioTokenType type() {
    skip();
    if (cursor >= tokens.size()) {
      return StyioTokenType::TOK_EOF;
    }
    return tokens[cursor]->type;
  }

  bool take(StyioTokenType target) {
    skip();
    if (cursor >= tokens.size() || tokens[cursor]->type != target) {
      return false;
    }
    cursor += 1;
    return true;
  }

  bool advance() {
    skip();
    if (cursor >= tokens.size()) {
      return false;
    }
    cursor += 1;
    return true;
  }
};

template <typename AllowedFn, typename StopFn>
bool
scan_subset_route_tokens_latest(
  const std::vector<StyioToken*>& tokens,
  size_t start,
  AllowedFn allowed,
  StopFn should_stop) {
  size_t cursor = styio_skip_trivia_tokens(tokens, start);
  int paren_depth = 0;
  int box_depth = 0;
  int brace_depth = 0;
  int pending_wave_pipe_count = 0;
  bool saw_non_trivia = false;

  while (cursor < tokens.size()) {
    const StyioTokenType type = tokens[cursor]->type;
    if (should_stop(type, paren_depth, box_depth, brace_depth, saw_non_trivia)) {
      return saw_non_trivia;
    }
    if (styio_is_trivia_token(type)) {
      cursor += 1;
      continue;
    }
    if (type == StyioTokenType::TOK_PIPE) {
      if (pending_wave_pipe_count == 0) {
        return false;
      }
      pending_wave_pipe_count -= 1;
    }
    if (!allowed(type)) {
      return false;
    }
    saw_non_trivia = true;

    switch (type) {
      case StyioTokenType::TOK_LPAREN:
        paren_depth += 1;
        break;
      case StyioTokenType::TOK_RPAREN:
        if (paren_depth == 0) {
          return false;
        }
        paren_depth -= 1;
        break;
      case StyioTokenType::TOK_LBOXBRAC:
      case StyioTokenType::BOUNDED_BUFFER_OPEN:
        box_depth += 1;
        break;
      case StyioTokenType::TOK_RBOXBRAC:
      case StyioTokenType::BOUNDED_BUFFER_CLOSE:
        if (box_depth == 0) {
          return false;
        }
        box_depth -= 1;
        break;
      case StyioTokenType::TOK_LCURBRAC:
        brace_depth += 1;
        break;
      case StyioTokenType::TOK_RCURBRAC:
        if (brace_depth == 0) {
          return false;
        }
        brace_depth -= 1;
        break;
      case StyioTokenType::WAVE_RIGHT:
        pending_wave_pipe_count += 1;
        break;
      default:
        break;
    }

    cursor += 1;
  }

  return saw_non_trivia && paren_depth == 0 && box_depth == 0 && brace_depth == 0;
}

bool
consume_balanced_group_latest(
  TokenProbeLatest& probe,
  StyioTokenType open,
  StyioTokenType close) {
  if (!probe.take(open)) {
    return false;
  }

  int depth = 1;
  while (true) {
    const StyioTokenType type = probe.type();
    if (type == StyioTokenType::TOK_EOF) {
      return false;
    }
    probe.advance();
    if (type == open) {
      depth += 1;
      continue;
    }
    if (type == close) {
      depth -= 1;
      if (depth == 0) {
        return true;
      }
    }
  }
}

bool
consume_hash_return_type_latest(TokenProbeLatest& probe) {
  if (probe.type() == StyioTokenType::TOK_LPAREN) {
    return consume_balanced_group_latest(probe, StyioTokenType::TOK_LPAREN, StyioTokenType::TOK_RPAREN);
  }
  if (probe.type() == StyioTokenType::BOUNDED_BUFFER_OPEN) {
    return consume_balanced_group_latest(probe, StyioTokenType::BOUNDED_BUFFER_OPEN, StyioTokenType::BOUNDED_BUFFER_CLOSE);
  }
  if (probe.type() != StyioTokenType::NAME) {
    return false;
  }
  probe.advance();
  return true;
}

bool
can_route_hash_stmt_nightly_latest(const StyioContext& context) {
  TokenProbeLatest probe(context);
  if (!probe.take(StyioTokenType::TOK_HASH)) {
    return false;
  }
  if (probe.type() != StyioTokenType::NAME) {
    return false;
  }
  probe.advance();

  if (probe.type() == StyioTokenType::TOK_LPAREN
      && !consume_balanced_group_latest(probe, StyioTokenType::TOK_LPAREN, StyioTokenType::TOK_RPAREN)) {
    return false;
  }

  if (probe.take(StyioTokenType::TOK_COLON)
      && !consume_hash_return_type_latest(probe)) {
    return false;
  }

  bool route_supported = false;
  switch (probe.type()) {
    case StyioTokenType::MATCH:
    case StyioTokenType::ITERATOR:
    case StyioTokenType::WALRUS:
    case StyioTokenType::TOK_EQUAL:
    case StyioTokenType::ARROW_DOUBLE_RIGHT:
      route_supported = true;
      break;
    default:
      break;
  }
  if (!route_supported) {
    return false;
  }

  return scan_subset_route_tokens_latest(
    context.get_tokens(),
    context.get_token_index(),
    [](StyioTokenType type) {
      return styio_parser_stmt_subset_token_nightly(type);
    },
    [](StyioTokenType type, int paren_depth, int box_depth, int brace_depth, bool saw_non_trivia) {
      if (!saw_non_trivia) {
        return false;
      }
      if (paren_depth != 0 || box_depth != 0 || brace_depth != 0) {
        return false;
      }
      return type == StyioTokenType::TOK_EOF
             || type == StyioTokenType::TOK_LF
             || type == StyioTokenType::TOK_CR
             || type == StyioTokenType::TOK_RCURBRAC;
    });
}

bool
stmt_subset_route_supported_latest(const StyioContext& context) {
  const auto& tokens = context.get_tokens();
  const size_t start = styio_skip_trivia_tokens(tokens, context.get_token_index());
  if (start >= tokens.size()) {
    return false;
  }

  const StyioTokenType start_type = tokens[start]->type;
  if (!styio_parser_stmt_subset_start_nightly(start_type)) {
    return false;
  }
  if (start_type == StyioTokenType::TOK_HASH) {
    return can_route_hash_stmt_nightly_latest(context);
  }

  return scan_subset_route_tokens_latest(
    tokens,
    start,
    [](StyioTokenType type) {
      return styio_parser_stmt_subset_token_nightly(type);
    },
    [](StyioTokenType type, int paren_depth, int box_depth, int brace_depth, bool saw_non_trivia) {
      if (!saw_non_trivia) {
        return false;
      }
      if (paren_depth != 0 || box_depth != 0 || brace_depth != 0) {
        return false;
      }
      return type == StyioTokenType::TOK_EOF
             || type == StyioTokenType::TOK_LF
             || type == StyioTokenType::TOK_CR
             || type == StyioTokenType::TOK_RCURBRAC;
    });
}

bool
expr_subset_route_supported_until_latest(
  const StyioContext& context,
  std::initializer_list<StyioTokenType> delimiters) {
  const auto& tokens = context.get_tokens();
  const size_t start = styio_skip_trivia_tokens(tokens, context.get_token_index());
  if (start >= tokens.size()) {
    return false;
  }
  if (!styio_parser_expr_subset_start_nightly(tokens[start]->type)) {
    return false;
  }

  return scan_subset_route_tokens_latest(
    tokens,
    start,
    [](StyioTokenType type) {
      return styio_parser_stmt_subset_token_nightly(type);
    },
    [delimiters](StyioTokenType type, int paren_depth, int box_depth, int brace_depth, bool saw_non_trivia) {
      if (!saw_non_trivia) {
        return false;
      }
      if (paren_depth != 0 || box_depth != 0 || brace_depth != 0) {
        return false;
      }
      if (type == StyioTokenType::TOK_EOF) {
        return true;
      }
      for (auto delim : delimiters) {
        if (type == delim) {
          return true;
        }
      }
      return false;
    });
}

ParseAttempt<StyioAST>
try_parse_expr_subset_until_latest(
  StyioContext& context,
  std::initializer_list<StyioTokenType> delimiters) {
  const auto saved = context.save_cursor();
  context.skip();
  if (!expr_subset_route_supported_until_latest(context, delimiters)) {
    context.restore_cursor(saved);
    return ParseAttempt<StyioAST>::declined();
  }
  try {
    return ParseAttempt<StyioAST>::parsed(parse_expr_subset_allowing_follow_latest(context, delimiters));
  } catch (...) {
    context.restore_cursor(saved);
    return ParseAttempt<StyioAST>::fatal(std::current_exception());
  }
}

ParseAttempt<BlockAST>
try_parse_block_only_subset_nightly(StyioContext& context) {
  const auto saved = context.save_cursor();
  context.skip();
  if (context.cur_tok_type() != StyioTokenType::TOK_LCURBRAC) {
    context.restore_cursor(saved);
    return ParseAttempt<BlockAST>::declined();
  }
  if (!stmt_subset_route_supported_latest(context)) {
    context.restore_cursor(saved);
    return ParseAttempt<BlockAST>::declined();
  }
  try {
    return ParseAttempt<BlockAST>::parsed(parse_block_only_subset_nightly(context));
  } catch (...) {
    context.restore_cursor(saved);
    return ParseAttempt<BlockAST>::fatal(std::current_exception());
  }
}

ParseAttempt<StyioAST>
try_parse_hash_stmt_nightly_latest(StyioContext& context) {
  const auto saved = context.save_cursor();
  context.skip();
  if (!can_route_hash_stmt_nightly_latest(context)) {
    context.restore_cursor(saved);
    return ParseAttempt<StyioAST>::declined();
  }
  try {
    return ParseAttempt<StyioAST>::parsed(parse_hash_stmt_nightly(context));
  } catch (...) {
    context.restore_cursor(saved);
    return ParseAttempt<StyioAST>::fatal(std::current_exception());
  }
}

BlockAST*
parse_block_only_subset_nightly(StyioContext& context);

StyioAST*
parse_stmt_subset_with_legacy_fallback_latest_draft(StyioContext& context);

BlockAST*
parse_block_only_subset_with_legacy_fallback_latest_draft(StyioContext& context);

CasesAST*
parse_cases_only_nightly_draft(StyioContext& context);

std::vector<StyioAST*>
parse_forward_as_list_nightly_draft(StyioContext& context);

StyioAST*
parse_iterator_only_nightly_draft(StyioContext& context, StyioAST* collection);

StyioAST*
parse_list_expr_or_iterator_nightly_draft(StyioContext& context) {
  StyioAST* collection = parse_list_exprs_latest_draft(context);
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::ITERATOR) {
    return parse_iterator_only_nightly_draft(context, collection);
  }
  return collection;
}

DictAST*
parse_dict_literal_nightly_draft(StyioContext& context) {
  std::vector<std::pair<StyioAST*, StyioAST*>> entries;

  auto parse_entry_expr = [&]() -> StyioAST* {
    auto attempt = try_parse_expr_subset_until_latest(
      context,
      {StyioTokenType::TOK_COLON, StyioTokenType::TOK_COMMA, StyioTokenType::TOK_RCURBRAC});
    if (attempt.status == ParseAttemptStatus::Parsed) {
      return attempt.node;
    }
    if (attempt.status == ParseAttemptStatus::Fatal) {
      std::rethrow_exception(attempt.error);
    }
    context.note_nightly_internal_legacy_bridge_latest();
    return parse_expr(context);
  };

  context.try_match_panic(StyioTokenType::TOK_LCURBRAC);
  context.skip();
  if (context.match(StyioTokenType::TOK_RCURBRAC)) {
    return DictAST::Create(std::move(entries));
  }

  while (true) {
    StyioAST* key = parse_entry_expr();
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_COLON);
    context.skip();
    StyioAST* value = parse_entry_expr();
    entries.push_back({key, value});
    context.skip();
    if (context.match(StyioTokenType::TOK_RCURBRAC)) {
      return DictAST::Create(std::move(entries));
    }
    context.try_match_panic(StyioTokenType::TOK_COMMA);
    context.skip();
  }
}

StyioAST*
parse_stmt_subset_with_legacy_fallback_latest_draft(StyioContext& context) {
  auto attempt = try_parse_stmt_subset_nightly(context);
  if (attempt.status == ParseAttemptStatus::Parsed) {
    return attempt.node;
  }
  if (attempt.status == ParseAttemptStatus::Fatal) {
    std::rethrow_exception(attempt.error);
  }
  context.note_nightly_internal_legacy_bridge_latest();
  return parse_stmt_or_expr_legacy(context);
}

BlockAST*
parse_block_only_subset_with_legacy_fallback_latest_draft(StyioContext& context) {
  auto attempt = try_parse_block_only_subset_nightly(context);
  if (attempt.status == ParseAttemptStatus::Parsed) {
    return attempt.node;
  }
  if (attempt.status == ParseAttemptStatus::Fatal) {
    std::rethrow_exception(attempt.error);
  }
  context.note_nightly_internal_legacy_bridge_latest();
  return parse_block_only(context);
}

StyioAST*
parse_iterator_body_nightly_fallback_latest_draft(StyioContext& context) {
  context.skip();
  if (context.check(StyioTokenType::TOK_LCURBRAC)) {
    return parse_block_only_subset_with_legacy_fallback_latest_draft(context);
  }
  return parse_stmt_subset_with_legacy_fallback_latest_draft(context);
}

StyioAST*
parse_iterator_collection_rhs_nightly_draft(StyioContext& context) {
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_LBOXBRAC) {
    return parse_list_exprs_latest_draft(context);
  }
  auto attempt = try_parse_expr_subset_until_latest(context, {StyioTokenType::ITERATOR});
  if (attempt.status == ParseAttemptStatus::Parsed) {
    return attempt.node;
  }
  if (attempt.status == ParseAttemptStatus::Fatal) {
    std::rethrow_exception(attempt.error);
  }
  context.note_nightly_internal_legacy_bridge_latest();
  return parse_expr(context);
}

BlockAST*
parse_infinite_loop_body_nightly_draft(StyioContext& context) {
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_LCURBRAC) {
    return parse_block_only_subset_with_legacy_fallback_latest_draft(context);
  }

  std::vector<StyioAST*> one;
  one.push_back(parse_stmt_subset_with_legacy_fallback_latest_draft(context));
  return BlockAST::Create(std::move(one));
}

CasesAST*
parse_cases_only_nightly_draft(StyioContext& context) {
  vector<std::pair<StyioAST*, StyioAST*>> case_pairs;
  StyioAST* default_stmt = nullptr;

  context.try_match_panic(StyioTokenType::TOK_LCURBRAC);

  while (not context.match(StyioTokenType::TOK_RCURBRAC)) {
    context.skip();
    if (context.match(StyioTokenType::TOK_UNDLINE)) {
      context.skip();
      if (context.match(StyioTokenType::ARROW_DOUBLE_RIGHT)) {
        context.skip();
        default_stmt = parse_iterator_body_nightly_fallback_latest_draft(context);
      }
      else {
        throw StyioSyntaxError("=> not found for default case");
      }
    }
    else {
      auto left_attempt = try_parse_expr_subset_until_latest(context, {StyioTokenType::ARROW_DOUBLE_RIGHT});
      StyioAST* left = nullptr;
      if (left_attempt.status == ParseAttemptStatus::Parsed) {
        left = left_attempt.node;
      }
      else if (left_attempt.status == ParseAttemptStatus::Fatal) {
        std::rethrow_exception(left_attempt.error);
      }
      else {
        context.note_nightly_internal_legacy_bridge_latest();
        left = parse_expr(context);
      }

      context.skip();
      if (context.match(StyioTokenType::ARROW_DOUBLE_RIGHT)) {
        context.skip();
        StyioAST* right = parse_iterator_body_nightly_fallback_latest_draft(context);
        case_pairs.push_back(std::make_pair(left, right));
      }
      else {
        throw StyioSyntaxError(context.mark_cur_tok("`=>` not found"));
      }
    }

    context.skip();
  }

  if (case_pairs.empty()) {
    return CasesAST::Create(default_stmt);
  }
  return CasesAST::Create(case_pairs, default_stmt);
}

std::vector<StyioAST*>
parse_forward_as_list_nightly_draft(StyioContext& context) {
  std::vector<StyioAST*> following_exprs;

  while (true) {
    context.skip();
    switch (context.cur_tok_type()) {
      case StyioTokenType::ARROW_DOUBLE_RIGHT: {
        context.move_forward(1);
        following_exprs.push_back(parse_iterator_body_nightly_fallback_latest_draft(context));
      } break;

      case StyioTokenType::TOK_QUEST: {
        throw StyioParseError("parse_forward(Conditionals)");
      } break;

      case StyioTokenType::MATCH: {
        context.move_forward(1);
        context.skip();
        if (context.check(StyioTokenType::TOK_LCURBRAC)) {
          following_exprs.push_back(parse_cases_only_nightly_draft(context));
        }
        else {
          std::vector<StyioAST*> rvals;
          do {
            auto attempt = try_parse_expr_subset_until_latest(context, {StyioTokenType::TOK_COMMA});
            if (attempt.status == ParseAttemptStatus::Parsed) {
              rvals.push_back(attempt.node);
            }
            else if (attempt.status == ParseAttemptStatus::Fatal) {
              std::rethrow_exception(attempt.error);
            }
            else {
              context.note_nightly_internal_legacy_bridge_latest();
              rvals.push_back(parse_expr(context));
            }
          } while (context.try_match(StyioTokenType::TOK_COMMA));
          following_exprs.push_back(CheckEqualAST::Create(rvals));
        }
      } break;

      case StyioTokenType::ITERATOR: {
      } break;

      default: {
        return following_exprs;
      } break;
    }
  }

  return following_exprs;
}

StyioAST*
parse_iterator_tail_nightly_draft(StyioContext& context, StyioAST* collection) {
  std::vector<ParamAST*> params;

  context.skip();

  if (context.match(StyioTokenType::TOK_HASH)) {
    context.skip();
    if (context.check(StyioTokenType::TOK_LPAREN)) {
      params = parse_params(context);
    }
    else if (context.check(StyioTokenType::NAME)) {
      std::vector<HashTagNameAST*> hash_tags;

      hash_tags.push_back(HashTagNameAST::Create(parse_name_with_spaces_unsafe(context)));

      while (context.try_match(StyioTokenType::TOK_RANGBRAC)) {
        if (context.try_match(StyioTokenType::TOK_HASH)) {
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
    || context.cur_tok_type() == StyioTokenType::NAME) {
    params = parse_params(context);
  }

  context.skip();

  if (context.try_match(StyioTokenType::TOK_AMP)) {
    context.skip();
    StyioAST* collection_b = parse_iterator_collection_rhs_nightly_draft(context);
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
    StyioAST* body_ast = parse_iterator_body_nightly_fallback_latest_draft(context);
    return StreamZipAST::Create(collection, params, collection_b, params_b, body_ast);
  }

  if (context.try_match(StyioTokenType::ARROW_DOUBLE_RIGHT)) {
    context.skip();
    return IteratorAST::Create(collection, params, parse_iterator_body_nightly_fallback_latest_draft(context));
  }
  context.skip();
  if (context.check(StyioTokenType::TOK_LCURBRAC)) {
    return IteratorAST::Create(collection, params, parse_block_only_subset_with_legacy_fallback_latest_draft(context));
  }
  else if (context.try_match(StyioTokenType::TOK_RANGBRAC)) {
    std::vector<HashTagNameAST*> hash_tags;

    do {
      if (context.try_match(StyioTokenType::TOK_HASH)) {
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
    } while (context.try_match(StyioTokenType::TOK_RANGBRAC));

    return IterSeqAST::Create(collection, params, hash_tags);
  }

  return IteratorAST::Create(collection, params);
}

StyioAST*
parse_iterator_only_nightly_draft(StyioContext& context, StyioAST* collection) {
  context.try_match_panic(StyioTokenType::ITERATOR);
  return parse_iterator_tail_nightly_draft(context, collection);
}

int
expr_prec_of(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::LOGIC_OR:
      return 20;
    case StyioTokenType::LOGIC_AND:
      return 30;
    case StyioTokenType::BINOP_EQ:
    case StyioTokenType::BINOP_NE:
      return 35;
    case StyioTokenType::BINOP_GT:
    case StyioTokenType::BINOP_GE:
    case StyioTokenType::BINOP_LT:
    case StyioTokenType::BINOP_LE:
    case StyioTokenType::TOK_RANGBRAC:
    case StyioTokenType::TOK_LANGBRAC:
      return 40;
    case StyioTokenType::TOK_PLUS:
    case StyioTokenType::TOK_MINUS:
      return 50;
    case StyioTokenType::TOK_STAR:
    case StyioTokenType::TOK_SLASH:
    case StyioTokenType::TOK_PERCENT:
      return 60;
    case StyioTokenType::BINOP_POW:
      return 70;
    default:
      return -1;
  }
}

bool
expr_is_right_assoc(StyioTokenType type) {
  return type == StyioTokenType::BINOP_POW;
}

StyioOpType
expr_map_binop(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::TOK_PLUS:
      return StyioOpType::Binary_Add;
    case StyioTokenType::TOK_MINUS:
      return StyioOpType::Binary_Sub;
    case StyioTokenType::TOK_STAR:
      return StyioOpType::Binary_Mul;
    case StyioTokenType::TOK_SLASH:
      return StyioOpType::Binary_Div;
    case StyioTokenType::TOK_PERCENT:
      return StyioOpType::Binary_Mod;
    case StyioTokenType::BINOP_POW:
      return StyioOpType::Binary_Pow;
    default:
      return StyioOpType::Undefined;
  }
}

bool
expr_is_comp(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::BINOP_GT:
    case StyioTokenType::TOK_RANGBRAC:
    case StyioTokenType::BINOP_GE:
    case StyioTokenType::BINOP_LT:
    case StyioTokenType::TOK_LANGBRAC:
    case StyioTokenType::BINOP_LE:
    case StyioTokenType::BINOP_EQ:
    case StyioTokenType::BINOP_NE:
      return true;
    default:
      return false;
  }
}

CompType
expr_map_comp(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::BINOP_GT:
    case StyioTokenType::TOK_RANGBRAC:
      return CompType::GT;
    case StyioTokenType::BINOP_GE:
      return CompType::GE;
    case StyioTokenType::BINOP_LT:
    case StyioTokenType::TOK_LANGBRAC:
      return CompType::LT;
    case StyioTokenType::BINOP_LE:
      return CompType::LE;
    case StyioTokenType::BINOP_EQ:
      return CompType::EQ;
    case StyioTokenType::BINOP_NE:
      return CompType::NE;
    default:
      return CompType::EQ;
  }
}

bool
expr_is_logic(StyioTokenType type) {
  return type == StyioTokenType::LOGIC_AND || type == StyioTokenType::LOGIC_OR;
}

LogicType
expr_map_logic(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::LOGIC_AND:
      return LogicType::AND;
    case StyioTokenType::LOGIC_OR:
      return LogicType::OR;
    default:
      return LogicType::RAW;
  }
}

class StyioExprSubsetParser
{
private:
  StyioContext& context_;

  StyioAST* parse_wave_dispatch_arm_with_allowed_follow(
    std::initializer_list<StyioTokenType> allowed_follow) {
    context_.skip();
    switch (context_.cur_tok_type()) {
      case StyioTokenType::TOK_AT:
        context_.move_forward(1, "new_expr:dispatch_arm@");
        return UndefinedLitAST::Create();
      case StyioTokenType::PRINT:
        return parse_print_nightly(context_);
      case StyioTokenType::TOK_LCURBRAC:
        return parse_block_only_subset_nightly(context_);
      default: {
        StyioExprSubsetParser nested(context_);
        return nested.parse_with_allowed_follow(allowed_follow);
      }
    }
  }

  bool has_linebreak_before_current_latest_draft() const {
    const auto& tokens = context_.get_tokens();
    size_t idx = context_.get_token_index();
    while (idx > 0) {
      const StyioTokenType prev = tokens[idx - 1]->type;
      if (prev == StyioTokenType::TOK_SPACE) {
        idx -= 1;
        continue;
      }
      return prev == StyioTokenType::TOK_LF;
    }
    return false;
  }

  bool has_unsupported_continuation_latest_draft(StyioTokenType type) const {
    switch (type) {
      case StyioTokenType::TOK_LBOXBRAC:
        return !has_linebreak_before_current_latest_draft();
      case StyioTokenType::WAVE_LEFT:
      case StyioTokenType::WAVE_RIGHT:
      case StyioTokenType::TOK_PIPE:
      case StyioTokenType::TOK_EQUAL:
      case StyioTokenType::WALRUS:
      case StyioTokenType::TOK_COLON:
      case StyioTokenType::ARROW_SINGLE_LEFT:
      case StyioTokenType::ARROW_SINGLE_RIGHT:
      case StyioTokenType::ITERATOR:
      case StyioTokenType::EXTRACTOR:
      case StyioTokenType::COMPOUND_ADD:
      case StyioTokenType::COMPOUND_SUB:
      case StyioTokenType::COMPOUND_MUL:
      case StyioTokenType::COMPOUND_DIV:
      case StyioTokenType::COMPOUND_MOD:
        return true;
      default:
        return false;
    }
  }

  StyioAST* parse_postfix(StyioAST* lhs, bool allow_extended_continuations = true) {
    std::unique_ptr<StyioAST> owner(lhs);
    while (true) {
      context_.skip_spaces_no_linebreak();
      if (context_.cur_tok_type() == StyioTokenType::TOK_DOT) {
        if (dynamic_cast<FuncCallAST*>(owner.get()) != nullptr) {
          throw StyioSyntaxError("dot-chain after call remains unsupported in nightly parser subset");
        }
        context_.move_forward(1, "new_expr:dot");
        context_.skip_spaces_no_linebreak();
        if (context_.cur_tok_type() != StyioTokenType::NAME) {
          throw StyioSyntaxError("expected name after '.' in nightly parser subset");
        }
        const std::string member_name = context_.cur_tok()->original;
        context_.move_forward(1, "new_expr:dot_name");
        context_.skip_spaces_no_linebreak();
        if (context_.cur_tok_type() == StyioTokenType::TOK_LPAREN) {
          owner.reset(FuncCallAST::Create(owner.release(), NameAST::Create(member_name), parse_call_args()));
          continue;
        }
        if (member_name != "length"
            && member_name != "size"
            && member_name != "keys"
            && member_name != "values") {
          throw StyioSyntaxError("only .length, .size, .keys, and .values are supported as nightly attributes");
        }
        owner.reset(AttrAST::Create(owner.release(), NameAST::Create(member_name)));
        continue;
      }
      if (context_.cur_tok_type() == StyioTokenType::TOK_LBOXBRAC) {
        context_.move_forward(1, "new_expr:index_open");
        context_.skip();
        StyioAST* idx = parse_full_expression();
        context_.skip();
        context_.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
        owner.reset(new ListOpAST(StyioNodeType::Access_By_Index, owner.release(), idx));
        continue;
      }
      if (allow_extended_continuations && context_.match(StyioTokenType::MATCH)) {
        context_.skip();
        if (context_.cur_tok_type() != StyioTokenType::TOK_LCURBRAC) {
          throw StyioSyntaxError(context_.mark_cur_tok("?= must be followed by {"));
        }
        owner.reset(MatchCasesAST::make(owner.release(), parse_cases_only_nightly_draft(context_)));
        continue;
      }
      if (allow_extended_continuations && context_.match(StyioTokenType::ITERATOR)) {
        context_.skip();
        if (context_.cur_tok_type() != StyioTokenType::TOK_AT) {
          throw StyioSyntaxError("unsupported '>>' continuation in nightly parser subset");
        }
        /* Nightly subset accepts the same resource-write shorthand as legacy:
           `expr >> @file{...}` and `expr >> @stdout/@stderr`. */
        owner.reset(ResourceWriteAST::Create(owner.release(), parse_resource_file_atom_latest(context_)));
        continue;
      }
      if (allow_extended_continuations && context_.match(StyioTokenType::ARROW_SINGLE_RIGHT)) {
        context_.skip();
        owner.reset(ResourceRedirectAST::Create(owner.release(), parse_resource_file_atom_latest(context_)));
        continue;
      }
      if (allow_extended_continuations && context_.match(StyioTokenType::WAVE_RIGHT)) {
        context_.skip();
        StyioAST* true_arm =
          parse_wave_dispatch_arm_with_allowed_follow({StyioTokenType::TOK_PIPE});
        context_.skip();
        if (!context_.match(StyioTokenType::TOK_PIPE)) {
          throw StyioSyntaxError(context_.mark_cur_tok("Expected | after ~> true arm"));
        }
        context_.skip();
        StyioAST* false_arm = parse_wave_dispatch_arm_with_allowed_follow({});
        owner.reset(WaveDispatchAST::Create(owner.release(), true_arm, false_arm));
        continue;
      }
      if (allow_extended_continuations && owner && owner->getNodeType() == StyioNodeType::Infinite) {
        if (context_.match(StyioTokenType::TOK_QUEST)) {
          context_.skip();
          context_.try_match_panic(StyioTokenType::TOK_LPAREN);
          context_.skip();
          StyioAST* cond = parse_full_expression();
          context_.skip();
          context_.try_match_panic(StyioTokenType::TOK_RPAREN);
          context_.skip();
          context_.try_match_panic(StyioTokenType::ITERATOR);
          context_.skip();
          owner.reset(InfiniteLoopAST::CreateWhile(cond, parse_infinite_loop_body_nightly_draft(context_)));
          continue;
        }
        if (context_.match(StyioTokenType::ARROW_DOUBLE_RIGHT)) {
          context_.skip();
          owner.reset(InfiniteLoopAST::CreateInfinite(parse_infinite_loop_body_nightly_draft(context_)));
          continue;
        }
      }
      break;
    }
    return owner.release();
  }

  StyioAST* parse_full_expression() {
    StyioAST* parsed = parse_postfix(parse_expression(0));
    context_.skip_spaces_no_linebreak();
    if (has_unsupported_continuation_latest_draft(context_.cur_tok_type())) {
      delete parsed;
      throw StyioSyntaxError("unsupported expression continuation in nightly parser subset");
    }
    return parsed;
  }

  std::vector<StyioAST*> parse_call_args() {
    context_.move_forward(1, "new_expr:call(");

    std::vector<StyioAST*> args;
    context_.skip_spaces_no_linebreak();
    if (context_.cur_tok_type() != StyioTokenType::TOK_RPAREN) {
      while (true) {
        args.push_back(parse_full_expression());
        context_.skip_spaces_no_linebreak();
        if (context_.cur_tok_type() == StyioTokenType::TOK_RPAREN) {
          break;
        }
        context_.try_match_panic(StyioTokenType::TOK_COMMA);
        context_.skip_spaces_no_linebreak();
      }
    }
    context_.try_match_panic(StyioTokenType::TOK_RPAREN);
    return args;
  }

  StyioAST* parse_call_after_name(const std::string& callee_name) {
    return FuncCallAST::Create(NameAST::Create(callee_name), parse_call_args());
  }

  StyioAST* parse_name_family(const std::string& name) {
    if (name == "true" || name == "false") {
      return BoolAST::Create(name == "true");
    }

    context_.skip_spaces_no_linebreak();
    if (context_.cur_tok_type() == StyioTokenType::TOK_LPAREN) {
      return parse_call_after_name(name);
    }

    return NameAST::Create(name);
  }

  StyioAST* parse_state_ref_nightly_draft() {
    context_.move_forward(1, "new_expr:$");
    context_.skip();
    if (context_.cur_tok_type() != StyioTokenType::NAME) {
      throw StyioSyntaxError(context_.mark_cur_tok("expected name after $ in nightly parser subset"));
    }
    NameAST* name = parse_name_unsafe(context_);
    return StateRefAST::Create(name);
  }

  StyioAST* parse_primary() {
    context_.skip();

    switch (context_.cur_tok_type()) {
      case StyioTokenType::INTEGER: {
        const std::string lit = context_.cur_tok()->original;
        context_.move_forward(1, "new_expr:int");
        return IntAST::Create(lit);
      }
      case StyioTokenType::DECIMAL: {
        const std::string lit = context_.cur_tok()->original;
        context_.move_forward(1, "new_expr:float");
        return FloatAST::Create(lit);
      }
      case StyioTokenType::STRING: {
        const std::string lit = context_.cur_tok()->original;
        context_.move_forward(1, "new_expr:string");
        return StringAST::Create(lit);
      }
      case StyioTokenType::TOK_LBOXBRAC: {
        return parse_list_expr_or_iterator_nightly_draft(context_);
      }
      case StyioTokenType::TOK_AT: {
        return parse_resource_file_atom_latest(context_);
      }
      case StyioTokenType::NAME: {
        const std::string name = context_.cur_tok()->original;
        context_.move_forward(1, "new_expr:name");
        context_.skip_spaces_no_linebreak();
        if (name == "dict" && context_.cur_tok_type() == StyioTokenType::TOK_LCURBRAC) {
          return parse_dict_literal_nightly_draft(context_);
        }
        return parse_name_family(name);
      }
      case StyioTokenType::TOK_DOLLAR: {
        return parse_state_ref_nightly_draft();
      }
      case StyioTokenType::TOK_LPAREN: {
        context_.move_forward(1, "new_expr:(");
        context_.skip();
        if (context_.cur_tok_type() == StyioTokenType::EXTRACTOR) {
          context_.move_forward(1, "new_expr:instant_pull");
          context_.skip();
          StyioAST* ratom = parse_resource_file_atom_latest(context_);
          /* Accept file resources and @stdin for instant pull (M10). */
          auto rnt = ratom->getNodeType();
          if (rnt != StyioNodeType::FileResource
              && rnt != StyioNodeType::StdinResource
              && rnt != StyioNodeType::StdoutResource
              && rnt != StyioNodeType::StderrResource) {
            delete ratom;
            throw StyioSyntaxError(context_.mark_cur_tok("instant pull needs @file{...}, @{...}, or @stdin"));
          }
          context_.skip();
          if (!context_.match(StyioTokenType::TOK_RPAREN)) {
            delete ratom;
            throw StyioSyntaxError("expected ')' after instant pull in nightly parser subset");
          }
          return InstantPullAST::Create(ratom);
        }
        StyioAST* inner = parse_full_expression();
        context_.skip();
        if (!context_.match(StyioTokenType::TOK_RPAREN)) {
          throw StyioSyntaxError("expected ')' in nightly parser expression subset");
        }
        return inner;
      }
      default:
        throw StyioSyntaxError("unexpected token in nightly parser expression subset");
    }
  }

  StyioAST* parse_unary() {
    context_.skip();

    if (context_.cur_tok_type() == StyioTokenType::TOK_PLUS) {
      context_.move_forward(1, "new_expr:unary+");
      return parse_unary();
    }
    if (context_.cur_tok_type() == StyioTokenType::TOK_MINUS) {
      context_.move_forward(1, "new_expr:unary-");
      // Align with legacy parser: unary '-' captures the remaining expression.
      return BinOpAST::Create(StyioOpType::Binary_Sub, IntAST::Create("0"), parse_expression(0));
    }

    return parse_postfix(parse_primary(), false);
  }

  StyioAST* parse_expression(int min_prec) {
    StyioAST* lhs = parse_unary();

    while (true) {
      context_.skip_spaces_no_linebreak();
      const StyioTokenType tok = context_.cur_tok_type();
      const int prec = expr_prec_of(tok);
      if (prec < min_prec) {
        break;
      }

      const bool is_comp = expr_is_comp(tok);
      const bool is_logic = expr_is_logic(tok);
      const StyioOpType op = expr_map_binop(tok);
      if (!is_comp && !is_logic && op == StyioOpType::Undefined) {
        break;
      }

      context_.move_forward(1, "new_expr:binop");
      const int next_min = expr_is_right_assoc(tok) ? prec : (prec + 1);
      StyioAST* rhs = parse_expression(next_min);
      if (is_comp) {
        lhs = new BinCompAST(expr_map_comp(tok), lhs, rhs);
      }
      else if (is_logic) {
        lhs = CondAST::Create(expr_map_logic(tok), lhs, rhs);
      }
      else {
        lhs = BinOpAST::Create(op, lhs, rhs);
      }
    }

    return lhs;
  }

public:
  explicit StyioExprSubsetParser(StyioContext& context) :
      context_(context) {
  }

  StyioAST* parse() {
    return parse_full_expression();
  }

  StyioAST* parse_with_allowed_follow(std::initializer_list<StyioTokenType> allowed_follow) {
    StyioAST* parsed = parse_postfix(parse_expression(0));
    context_.skip_spaces_no_linebreak();
    const StyioTokenType follow = context_.cur_tok_type();
    for (auto allowed : allowed_follow) {
      if (follow == allowed) {
        return parsed;
      }
    }
    if (has_unsupported_continuation_latest_draft(follow)) {
      delete parsed;
      throw StyioSyntaxError("unsupported expression continuation in nightly parser subset");
    }
    return parsed;
  }
};

StyioAST*
parse_expr_subset_allowing_follow_latest(
  StyioContext& context,
  std::initializer_list<StyioTokenType> allowed_follow) {
  StyioExprSubsetParser parser(context);
  return parser.parse_with_allowed_follow(allowed_follow);
}

} // namespace

bool
styio_parser_expr_subset_token_nightly(StyioTokenType type) {
  if (styio_is_trivia_token(type)) {
    return true;
  }

  switch (type) {
    case StyioTokenType::TOK_EOF:
    case StyioTokenType::INTEGER:
    case StyioTokenType::DECIMAL:
    case StyioTokenType::STRING:
    case StyioTokenType::NAME:
    case StyioTokenType::TOK_AT:
    case StyioTokenType::TOK_COLON:
    case StyioTokenType::TOK_DOLLAR:
    case StyioTokenType::TOK_LBOXBRAC:
    case StyioTokenType::TOK_LPAREN:
    case StyioTokenType::TOK_RPAREN:
    case StyioTokenType::TOK_RBOXBRAC:
    case StyioTokenType::TOK_COMMA:
    case StyioTokenType::TOK_DOT:
    case StyioTokenType::ELLIPSIS:
    case StyioTokenType::TOK_PLUS:
    case StyioTokenType::TOK_MINUS:
    case StyioTokenType::TOK_STAR:
    case StyioTokenType::TOK_SLASH:
    case StyioTokenType::TOK_PERCENT:
    case StyioTokenType::BINOP_POW:
    case StyioTokenType::BINOP_GT:
    case StyioTokenType::BINOP_GE:
    case StyioTokenType::BINOP_LT:
    case StyioTokenType::BINOP_LE:
    case StyioTokenType::TOK_RANGBRAC:
    case StyioTokenType::TOK_LANGBRAC:
    case StyioTokenType::BINOP_EQ:
    case StyioTokenType::BINOP_NE:
    case StyioTokenType::LOGIC_AND:
    case StyioTokenType::LOGIC_OR:
      return true;
    default:
      return false;
  }
}

bool
styio_parser_expr_subset_start_nightly(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::INTEGER:
    case StyioTokenType::DECIMAL:
    case StyioTokenType::STRING:
    case StyioTokenType::NAME:
    case StyioTokenType::TOK_AT:
    case StyioTokenType::TOK_DOLLAR:
    case StyioTokenType::TOK_LBOXBRAC:
    case StyioTokenType::TOK_LPAREN:
    case StyioTokenType::TOK_PLUS:
    case StyioTokenType::TOK_MINUS:
      return true;
    default:
      return false;
  }
}

StyioAST*
parse_expr_subset_nightly(StyioContext& context) {
  StyioExprSubsetParser parser(context);
  return parser.parse();
}

bool
styio_parser_stmt_subset_token_nightly(StyioTokenType type) {
  if (styio_parser_expr_subset_token_nightly(type)) {
    return true;
  }
  switch (type) {
    case StyioTokenType::TOK_HASH:
    case StyioTokenType::PRINT:
    case StyioTokenType::TOK_HAT:
    case StyioTokenType::TOK_COMMA:
    case StyioTokenType::TOK_AMP:
    case StyioTokenType::TOK_EQUAL:
    case StyioTokenType::TOK_AT:
    case StyioTokenType::ARROW_SINGLE_LEFT:
    case StyioTokenType::ARROW_SINGLE_RIGHT:
    case StyioTokenType::TOK_COLON:
    case StyioTokenType::WALRUS:
    case StyioTokenType::MATCH:
    case StyioTokenType::ITERATOR:
    case StyioTokenType::ARROW_DOUBLE_RIGHT:
    case StyioTokenType::WAVE_RIGHT:
    case StyioTokenType::TOK_PIPE:
    case StyioTokenType::TOK_LBOXBRAC:
    case StyioTokenType::TOK_RBOXBRAC:
    case StyioTokenType::TOK_LCURBRAC:
    case StyioTokenType::TOK_RCURBRAC:
    case StyioTokenType::TOK_UNDLINE:
    case StyioTokenType::EXTRACTOR:
    case StyioTokenType::YIELD_PIPE:
    case StyioTokenType::ELLIPSIS:
    case StyioTokenType::BOUNDED_BUFFER_OPEN:
    case StyioTokenType::BOUNDED_BUFFER_CLOSE:
    case StyioTokenType::COMPOUND_ADD:
    case StyioTokenType::COMPOUND_SUB:
    case StyioTokenType::COMPOUND_MUL:
    case StyioTokenType::COMPOUND_DIV:
    case StyioTokenType::COMPOUND_MOD:
    case StyioTokenType::TOK_QUEST:
      return true;
    default:
      return false;
  }
}

bool
styio_parser_stmt_subset_start_nightly(StyioTokenType type) {
  return type == StyioTokenType::TOK_HASH
         || type == StyioTokenType::PRINT
         || type == StyioTokenType::TOK_HAT
         || type == StyioTokenType::TOK_AT
         || type == StyioTokenType::TOK_LCURBRAC
         || type == StyioTokenType::TOK_QUEST
         || type == StyioTokenType::EXTRACTOR
         || type == StyioTokenType::YIELD_PIPE
         || type == StyioTokenType::ELLIPSIS
         || type == StyioTokenType::ITERATOR
         || styio_parser_expr_subset_start_nightly(type);
}

namespace {
StyioAST*
parse_stmt_subset_impl_nightly(StyioContext& context);

BlockAST*
parse_block_only_subset_nightly(StyioContext& context);

BlockAST*
parse_block_with_forward_subset_nightly(StyioContext& context);

PrintAST*
parse_print_nightly(StyioContext& context) {
  std::vector<StyioAST*> exprs;
  context.match_panic(StyioTokenType::PRINT);
  context.try_match_panic(StyioTokenType::TOK_LPAREN);

  while (true) {
    context.skip();
    if (context.match(StyioTokenType::TOK_RPAREN)) {
      break;
    }
    exprs.push_back(parse_expr_subset_nightly(context));
    context.skip();
    if (context.match(StyioTokenType::TOK_RPAREN)) {
      break;
    }
    context.try_match_panic(StyioTokenType::TOK_COMMA);
  }

  return PrintAST::Create(exprs);
}

ReturnAST*
parse_return_nightly(StyioContext& context) {
  context.match_panic(StyioTokenType::EXTRACTOR);
  return ReturnAST::Create(parse_expr_subset_nightly(context));
}

BreakAST*
parse_break_nightly(StyioContext& context) {
  unsigned depth = 0;
  while (context.check(StyioTokenType::TOK_HAT)) {
    depth += 1;
    context.move_forward(1, "new_stmt:break");
  }
  return BreakAST::Create(depth > 0 ? depth : 1u);
}

ContinueAST*
parse_continue_nightly(StyioContext& context) {
  size_t n = context.cur_tok()->original.size();
  unsigned depth = static_cast<unsigned>(n > 1 ? n - 1 : 1);
  context.move_forward(1, "new_stmt:continue");
  return ContinueAST::Create(depth);
}

PassAST*
parse_pass_nightly(StyioContext& context) {
  context.move_forward(1, "new_stmt:pass");
  return PassAST::Create();
}

BlockAST*
parse_block_only_subset_nightly(StyioContext& context) {
  std::vector<std::unique_ptr<StyioAST>> statements_owned;
  context.match_panic(StyioTokenType::TOK_LCURBRAC);

  while (context.cur_tok_type() != StyioTokenType::TOK_EOF) {
    context.skip();
    if (context.match(StyioTokenType::TOK_RCURBRAC)) {
      std::vector<StyioAST*> statements;
      statements.reserve(statements_owned.size());
      for (auto& owned : statements_owned) {
        statements.push_back(owned.release());
      }
      return BlockAST::Create(std::move(statements));
    }
    const auto statement_start = context.save_cursor();
    try {
      statements_owned.emplace_back(parse_stmt_subset_impl_nightly(context));
    } catch (...) {
      if (!nightly_handle_recovery_latest(context, statement_start, nightly_recovery_message_latest())) {
        throw;
      }
    }
  }

  context.try_match_panic(StyioTokenType::TOK_RCURBRAC);
  return BlockAST::Create(std::vector<StyioAST*>());
}

BlockAST*
parse_block_with_forward_subset_nightly(StyioContext& context) {
  std::unique_ptr<BlockAST> block(parse_block_only_subset_nightly(context));
  block->set_followings(parse_forward_as_list_nightly_draft(context));
  return block.release();
}

TypeAST*
parse_hash_simple_type_nightly(StyioContext& context) {
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::BOUNDED_BUFFER_OPEN) {
    context.move_forward(1, "new_stmt:hash_type_bounded_open");
    context.skip();
    if (context.cur_tok_type() != StyioTokenType::INTEGER) {
      throw StyioSyntaxError("expected integer capacity in [|n|] return type");
    }
    const std::string cap = context.cur_tok()->original;
    context.move_forward(1, "new_stmt:hash_type_bounded_cap");
    context.skip();
    context.try_match_panic(StyioTokenType::BOUNDED_BUFFER_CLOSE);
    return TypeAST::CreateBoundedRingBuffer(cap);
  }
  if (context.cur_tok_type() != StyioTokenType::NAME) {
    throw StyioSyntaxError("expected simple return type name in nightly parser subset");
  }
  const std::string type_name = context.cur_tok()->original;
  context.move_forward(1, "new_stmt:hash_type_name");
  return TypeAST::Create(type_name);
}

std::variant<TypeAST*, TypeTupleAST*>
parse_hash_ret_type_nightly(StyioContext& context) {
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_LPAREN) {
    context.move_forward(1, "new_stmt:hash_ret_tuple_open");
    std::vector<TypeAST*> types;
    context.skip();
    if (context.cur_tok_type() != StyioTokenType::TOK_RPAREN) {
      while (true) {
        types.push_back(parse_hash_simple_type_nightly(context));
        context.skip();
        if (context.cur_tok_type() == StyioTokenType::TOK_RPAREN) {
          break;
        }
        context.try_match_panic(StyioTokenType::TOK_COMMA);
        context.skip();
      }
    }
    context.try_match_panic(StyioTokenType::TOK_RPAREN);
    return TypeTupleAST::Create(types);
  }
  return parse_hash_simple_type_nightly(context);
}

StyioAST*
parse_hash_stmt_nightly(StyioContext& context) {
  context.match_panic(StyioTokenType::TOK_HASH);
  context.skip();
  if (context.cur_tok_type() != StyioTokenType::NAME) {
    throw StyioSyntaxError("expected function name after # in nightly parser subset");
  }
  NameAST* tag_name = parse_name_unsafe(context);

  std::vector<ParamAST*> params;
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_LPAREN) {
    params = parse_params(context);
  }

  std::variant<TypeAST*, TypeTupleAST*> ret_type = static_cast<TypeAST*>(nullptr);
  context.skip();
  if (context.cur_tok_type() == StyioTokenType::TOK_COLON) {
    context.move_forward(1, "new_stmt:hash_ret_colon");
    ret_type = parse_hash_ret_type_nightly(context);
  }

  context.skip();
  if (context.cur_tok_type() == StyioTokenType::MATCH) {
    context.move_forward(1, "new_stmt:hash_match");
    if (context.cur_tok_type() == StyioTokenType::TOK_LCURBRAC) {
      return FunctionAST::Create(tag_name, true, params, ret_type, parse_cases_only_nightly_draft(context));
    }

    std::vector<StyioAST*> rvals;
    do {
      rvals.push_back(parse_expr_subset_nightly(context));
    } while (context.try_match(StyioTokenType::TOK_COMMA));
    return FunctionAST::Create(tag_name, true, params, ret_type, CheckEqualAST::Create(rvals));
  }

  context.skip();
  if (context.cur_tok_type() == StyioTokenType::ITERATOR) {
    if (params.size() != 1) {
      throw StyioSyntaxError(
        context.mark_cur_tok("Confusing: The iterator (>>) can not be applied to multiple objects."));
    }
    NameAST* iter_collection = NameAST::Create(params[0]->getName());
    return FunctionAST::Create(tag_name, true, params, ret_type, parse_iterator_with_forward(context, iter_collection));
  }

  context.skip();
  bool is_unique = false;
  bool saw_assign = false;
  if (context.cur_tok_type() == StyioTokenType::WALRUS) {
    is_unique = true;
    saw_assign = true;
    context.move_forward(1, "new_stmt:hash_walrus");
  }
  else if (context.cur_tok_type() == StyioTokenType::TOK_EQUAL) {
    saw_assign = true;
    context.move_forward(1, "new_stmt:hash_equal");
  }
  else if (context.cur_tok_type() != StyioTokenType::ARROW_DOUBLE_RIGHT) {
    throw StyioSyntaxError("expected :=, =, or => in nightly parser subset hash function");
  }

  context.skip();
  if (saw_assign && context.cur_tok_type() == StyioTokenType::TOK_LPAREN) {
    params = parse_params(context);
  }

  context.skip();
  if (context.cur_tok_type() == StyioTokenType::ARROW_DOUBLE_RIGHT) {
    context.move_forward(1, "new_stmt:hash_arrow");

    context.skip();
    if (context.cur_tok_type() == StyioTokenType::TOK_LCURBRAC) {
      return FunctionAST::Create(tag_name, is_unique, params, ret_type, parse_block_with_forward_subset_nightly(context));
    }
    StyioAST* ret_expr = parse_stmt_subset_impl_nightly(context);
    return SimpleFuncAST::Create(tag_name, is_unique, params, ret_type, ret_expr);
  }
  if (saw_assign) {
    context.skip();
    StyioAST* ret_expr = parse_expr_subset_nightly(context);
    return SimpleFuncAST::Create(tag_name, is_unique, params, ret_type, ret_expr);
  }
  throw StyioSyntaxError("expected => or expression body in nightly parser subset hash function");
}

StyioAST*
parse_stmt_subset_impl_nightly(StyioContext& context) {
  context.skip();

  if (context.cur_tok_type() == StyioTokenType::NAME) {
    const auto saved = context.save_cursor();
    const std::string id = context.cur_tok()->original;
    if (id == "true" || id == "false") {
      return parse_expr_subset_nightly(context);
    }
    context.move_forward(1, "new_stmt:name_probe");
    context.skip();
    {
      auto parse_target_suffix = [&](StyioAST* base) -> StyioAST* {
        while (true) {
          context.skip();
          if (context.cur_tok_type() != StyioTokenType::TOK_LBOXBRAC) {
            return base;
          }
          context.move_forward(1, "new_stmt:target_index_open");
          context.skip();
          StyioAST* idx = parse_expr_subset_nightly(context);
          context.skip();
          context.try_match_panic(StyioTokenType::TOK_RBOXBRAC);
          base = new ListOpAST(StyioNodeType::Access_By_Index, base, idx);
        }
      };

      std::vector<std::unique_ptr<StyioAST>> lhs_owned;
      lhs_owned.emplace_back(parse_target_suffix(NameAST::Create(id)));
      context.skip();
      bool saw_multi_target = false;
      while (context.cur_tok_type() == StyioTokenType::TOK_COMMA) {
        saw_multi_target = true;
        context.move_forward(1, "new_stmt:target_comma");
        context.skip();
        if (context.cur_tok_type() != StyioTokenType::NAME) {
          throw StyioSyntaxError("parallel assignment targets must be names or indexed list elements");
        }
        const std::string next_id = context.cur_tok()->original;
        context.move_forward(1, "new_stmt:target_name");
        lhs_owned.emplace_back(parse_target_suffix(NameAST::Create(next_id)));
        context.skip();
      }
      const bool indexed_target =
        lhs_owned[0]->getNodeType() == StyioNodeType::Access_By_Index;
      if ((saw_multi_target || indexed_target) && context.cur_tok_type() == StyioTokenType::TOK_EQUAL) {
        context.move_forward(1, "new_stmt:parallel_assign_equal");
        context.skip();
        std::vector<StyioAST*> lhs;
        lhs.reserve(lhs_owned.size());
        for (auto& item : lhs_owned) {
          lhs.push_back(item.release());
        }
        std::vector<StyioAST*> rhs;
        do {
          rhs.push_back(parse_expr_subset_nightly(context));
          context.skip();
        } while (context.try_match(StyioTokenType::TOK_COMMA));
        return ParallelAssignAST::Create(std::move(lhs), std::move(rhs));
      }
    }
    context.restore_cursor(saved);
    context.move_forward(1, "new_stmt:name_probe");
    context.skip();
    if (context.cur_tok_type() == StyioTokenType::TOK_COLON) {
      context.move_forward(1, "new_stmt:final_bind_colon");
      context.skip();
      TypeAST* ty = parse_styio_type(context);
      context.skip();
      if (context.cur_tok_type() != StyioTokenType::WALRUS) {
        throw StyioSyntaxError("expected ':=' after type in nightly parser subset");
      }
      context.move_forward(1, "new_stmt:final_bind_walrus");
      context.skip();
      return FinalBindAST::Create(
        VarAST::Create(NameAST::Create(id), ty),
        parse_expr_subset_nightly(context));
    }
    if (context.cur_tok_type() == StyioTokenType::TOK_EQUAL) {
      context.move_forward(1, "new_stmt:flex_bind");
      context.skip();
      return FlexBindAST::Create(
        VarAST::Create(NameAST::Create(id)),
        parse_expr_subset_nightly(context));
    }
    if (context.cur_tok_type() == StyioTokenType::ARROW_SINGLE_LEFT) {
      context.move_forward(1, "new_stmt:handle_acquire");
      context.skip();
      if (context.cur_tok_type() == StyioTokenType::NAME) {
        const std::string src = context.cur_tok()->original;
        context.move_forward(1, "new_stmt:clone_from_name");
        return HandleAcquireAST::Create(
          VarAST::Create(NameAST::Create(id)),
          NameAST::Create(src));
      }
      return HandleAcquireAST::Create(
        VarAST::Create(NameAST::Create(id)),
        parse_resource_file_atom_latest(context));
    }
    if (context.cur_tok_type() == StyioTokenType::EXTRACTOR) {
      context.move_forward(1, "new_stmt:resource_write");
      context.skip();
      if (context.cur_tok_type() == StyioTokenType::NAME) {
        const std::string src = context.cur_tok()->original;
        context.move_forward(1, "new_stmt:clone_into_name");
        return HandleAcquireAST::Create(
          VarAST::Create(NameAST::Create(id)),
          NameAST::Create(src),
          HandleAcquireAST::BindMode::Flex);
      }
      return ResourceWriteAST::Create(
        NameAST::Create(id),
        parse_resource_file_atom_latest(context));
    }
    if (context.cur_tok_type() == StyioTokenType::ITERATOR) {
      const auto saved_iter = context.save_cursor();
      context.move_forward(1, "new_stmt:iterator_probe");
      context.skip();
      if (context.cur_tok_type() == StyioTokenType::TOK_AT) {
        return ResourceWriteAST::Create(
          NameAST::Create(id),
          parse_resource_file_atom_latest(context));
      }
      context.restore_cursor(saved_iter);
      return parse_iterator_only_nightly_draft(context, NameAST::Create(id));
    }
    StyioOpType cop = StyioOpType::Undefined;
    switch (context.cur_tok_type()) {
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
      context.move_forward(1, "new_stmt:compound_assign");
      context.skip();
      return BinOpAST::Create(cop, NameAST::Create(id), parse_expr_subset_nightly(context));
    }
    context.restore_cursor(saved);
  }

  if (context.cur_tok_type() == StyioTokenType::PRINT) {
    return parse_print_nightly(context);
  }
  if (context.cur_tok_type() == StyioTokenType::TOK_HASH) {
    auto attempt = try_parse_hash_stmt_nightly_latest(context);
    if (attempt.status == ParseAttemptStatus::Parsed) {
      return attempt.node;
    }
    if (attempt.status == ParseAttemptStatus::Fatal) {
      std::rethrow_exception(attempt.error);
    }
    context.note_nightly_internal_legacy_bridge_latest();
    return parse_hash_tag(context);
  }
  if (context.cur_tok_type() == StyioTokenType::TOK_AT) {
    return parse_at_stmt_or_expr_latest(context);
  }
  if (context.cur_tok_type() == StyioTokenType::TOK_QUEST) {
    context.move_forward(1, "new_stmt:condflow?");
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_LPAREN);
    context.skip();
    StyioAST* cond_expr = parse_expr_subset_nightly(context);
    context.skip();
    context.try_match_panic(StyioTokenType::TOK_RPAREN);
    context.skip();
    context.try_match_panic(StyioTokenType::ARROW_DOUBLE_RIGHT);
    context.skip();
    CondAST* cond = dynamic_cast<CondAST*>(cond_expr);
    if (cond == nullptr) {
      cond = CondAST::Create(LogicType::RAW, cond_expr);
    }
    return new CondFlowAST(
      StyioNodeType::CondFlow_True,
      cond,
      parse_iterator_body_nightly_fallback_latest_draft(context));
  }
  if (context.cur_tok_type() == StyioTokenType::TOK_LCURBRAC) {
    return parse_block_only_subset_nightly(context);
  }
  if (context.cur_tok_type() == StyioTokenType::EXTRACTOR) {
    return parse_return_nightly(context);
  }
  if (context.cur_tok_type() == StyioTokenType::TOK_HAT) {
    return parse_break_nightly(context);
  }
  if (context.cur_tok_type() == StyioTokenType::ITERATOR) {
    return parse_continue_nightly(context);
  }
  if (context.cur_tok_type() == StyioTokenType::YIELD_PIPE) {
    context.move_forward(1, "new_stmt:yield_pipe");
    context.skip();
    return ReturnAST::Create(parse_expr_subset_nightly(context));
  }
  if (context.cur_tok_type() == StyioTokenType::ELLIPSIS) {
    return parse_pass_nightly(context);
  }
  if (styio_parser_expr_subset_start_nightly(context.cur_tok_type())) {
    return parse_expr_subset_nightly(context);
  }
  throw StyioSyntaxError("unexpected statement token in nightly parser subset");
}

} // namespace

StyioAST*
parse_stmt_subset_nightly(StyioContext& context) {
  return parse_stmt_subset_impl_nightly(context);
}

ParseAttempt<StyioAST>
try_parse_stmt_subset_nightly(StyioContext& context) {
  const auto saved = context.save_cursor();
  context.skip();
  if (!stmt_subset_route_supported_latest(context)) {
    context.restore_cursor(saved);
    return ParseAttempt<StyioAST>::declined();
  }
  try {
    return ParseAttempt<StyioAST>::parsed(parse_stmt_subset_impl_nightly(context));
  } catch (...) {
    context.restore_cursor(saved);
    return ParseAttempt<StyioAST>::fatal(std::current_exception());
  }
}

MainBlockAST*
parse_main_block_subset_nightly(StyioContext& context) {
  std::vector<std::unique_ptr<StyioAST>> statements_owned;
  while (true) {
    context.skip();
    if (context.cur_tok_type() == StyioTokenType::TOK_EOF) {
      break;
    }
    const auto statement_start = context.save_cursor();
    try {
      statements_owned.emplace_back(parse_stmt_subset_impl_nightly(context));
    } catch (...) {
      if (!nightly_handle_recovery_latest(context, statement_start, nightly_recovery_message_latest())) {
        throw;
      }
    }
  }
  std::vector<StyioAST*> statements;
  statements.reserve(statements_owned.size());
  for (auto& owned : statements_owned) {
    statements.push_back(owned.release());
  }
  return MainBlockAST::Create(statements);
}
