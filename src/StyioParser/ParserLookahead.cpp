#include "ParserLookahead.hpp"

bool
styio_is_trivia_token(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::TOK_SPACE:
    case StyioTokenType::TOK_LF:
    case StyioTokenType::TOK_CR:
    case StyioTokenType::COMMENT_LINE:
    case StyioTokenType::COMMENT_CLOSED:
      return true;
    default:
      return false;
  }
}

size_t
styio_skip_trivia_tokens(const std::vector<StyioToken*>& tokens, size_t index) {
  size_t cursor = index;
  while (cursor < tokens.size() && styio_is_trivia_token(tokens[cursor]->type)) {
    cursor += 1;
  }
  return cursor;
}

bool
styio_try_check_non_trivia(
  const std::vector<StyioToken*>& tokens,
  size_t index,
  StyioTokenType target) {
  const size_t probe = styio_skip_trivia_tokens(tokens, index);
  if (probe >= tokens.size()) {
    return false;
  }
  return tokens[probe]->type == target;
}
