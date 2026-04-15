#pragma once
#ifndef STYIO_PARSER_LOOKAHEAD_H_
#define STYIO_PARSER_LOOKAHEAD_H_

#include <cstddef>
#include <vector>

#include "../StyioToken/Token.hpp"

bool
styio_is_trivia_token(StyioTokenType type);

size_t
styio_skip_trivia_tokens(const std::vector<StyioToken*>& tokens, size_t index);

bool
styio_try_check_non_trivia(
  const std::vector<StyioToken*>& tokens,
  size_t index,
  StyioTokenType target);

#endif
