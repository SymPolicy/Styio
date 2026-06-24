#pragma once
#ifndef STYIO_TOKENIZER_H_
#define STYIO_TOKENIZER_H_

// [C++ STL]
#include <string>
#include <vector>

// [Styio]
#include "../StyioToken/Token.hpp"

/*
  StyioTokenizer — span-first linear scanner.

  tokenize() performs a single O(n) pass over the source buffer,
  recording source spans in each token. Tokens are constructed via
  CreateFromSpan which builds `original` from the source slice in
  a single allocation (not char-by-char).

  The caller must keep the source buffer alive for the lifetime of
  the returned tokens if lexeme() views are used. The `original`
  std::string field provides owned text for backward compatibility.
*/
class StyioTokenizer
{
public:
  /*
    Tokenize source into a vector of StyioToken pointers.

    code must outlive the returned tokens if span views (lexeme())
    are accessed. The caller takes ownership of the returned tokens.
  */
  static std::vector<StyioToken*> tokenize(const std::string& code);
};

#endif
