#pragma once
#ifndef STYIO_TOKENIZER_H_
#define STYIO_TOKENIZER_H_

#include <string>
#include <vector>

#include "../StyioToken/Token.hpp"

/*
  StyioTokenizer — span-first linear scanner.

  tokenize() performs a single O(n) pass. Operator/space/comment/EOF
  tokens are pure span (CreateFromSpan — zero text allocation).
  NAME, INTEGER, DECIMAL tokens use CreateOwned (one allocation for
  the identifier/number spelling). STRING tokens use CreateString
  (raw quoted form in `original`, decoded value via decodedString()).

  tokenizeWithMetrics() populates an optional StyioTokenizerMetrics
  struct proving allocation counts.

  Caller must keep the source buffer alive for token lexeme() views.
*/

struct StyioTokenizerMetrics {
  size_t input_bytes = 0;
  size_t token_count = 0;
  size_t span_token_count = 0;
  size_t owned_text_token_count = 0;
  size_t owned_text_bytes = 0;
  size_t operator_bucket_probes = 0;
  size_t string_decodes = 0;
};

class StyioTokenizer
{
public:
  static std::vector<StyioToken*> tokenize(const std::string& code);

  /*
    Tokenize with allocation metrics.
    metrics_out must point to a StyioTokenizerMetrics struct.
    Pass nullptr to skip metrics collection.
  */
  static std::vector<StyioToken*>
  tokenizeWithMetrics(const std::string& code, void* metrics_out);
};

#endif
