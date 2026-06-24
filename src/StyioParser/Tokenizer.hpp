#pragma once
#ifndef STYIO_TOKENIZER_H_
#define STYIO_TOKENIZER_H_

#include <string>
#include <string_view>
#include <vector>

#include "../StyioToken/Token.hpp"

/*
  StyioTokenizer — zero-copy span-first linear scanner.

  CreateFromSpan() tokens carry NO owned text. lexeme() returns a
  std::string_view into the source buffer. Caller must ensure the
  source outlives all token lexeme() accesses.

  Preferred API: tokenizeOwned(std::string source) returns a
  StyioTokenStream that co-owns the source buffer and tokens,
  guaranteeing stable views.

  Legacy API: tokenize(const std::string& code) copies the source
  internally once and returns tokens with stable views. Prefer
  tokenizeOwned in new code.
*/

// ---------------------------------------------------------------
// Metrics — allocation and dispatch counters
// ---------------------------------------------------------------
struct StyioTokenizerMetrics {
  size_t input_bytes = 0;
  size_t token_count = 0;

  size_t source_view_token_count = 0;   // tokens using source_data_ view only
  size_t source_span_token_count = 0;   // tokens with any source span
  size_t zero_width_span_token_count = 0; // zero-width (e.g. EOF)

  size_t owned_text_token_count = 0;     // tokens with owned text (NAME/INT/DEC)
  size_t owned_text_bytes = 0;           // total bytes of owned text
  size_t decoded_text_token_count = 0;   // tokens with decoded text (STRING)
  size_t decoded_text_bytes = 0;         // total bytes of decoded text

  size_t source_copy_bytes = 0;          // source copy for legacy tokenize()
  size_t operator_bucket_probes = 0;     // O(1) bucket lookups
  size_t string_decodes = 0;             // string escape processing count
};

// ---------------------------------------------------------------
// StyioTokenStream — co-owns source buffer + tokens
// ---------------------------------------------------------------
class StyioTokenStream {
  std::string source_;
  std::vector<StyioToken*> tokens_;

public:
  StyioTokenStream(std::string src, std::vector<StyioToken*> toks) :
      source_(std::move(src)), tokens_(std::move(toks)) {}

  StyioTokenStream(StyioTokenStream&&) noexcept = default;
  StyioTokenStream& operator=(StyioTokenStream&&) noexcept = default;

  ~StyioTokenStream() {
    for (auto* t : tokens_) delete t;
  }

  const std::vector<StyioToken*>& tokens() const { return tokens_; }
  const std::string& source() const { return source_; }

  // Release tokens without deleting (for legacy callers).
  std::vector<StyioToken*> release() {
    std::vector<StyioToken*> out;
    out.swap(tokens_);
    return out;
  }
};

// ---------------------------------------------------------------
// Tokenizer
// ---------------------------------------------------------------
class StyioTokenizer
{
public:
  /*
    Recommended: co-own source + tokens. lexeme() views are stable.
    Use this when the caller can transfer ownership of the source string.
  */
  static StyioTokenStream tokenizeOwned(std::string source);

  /*
    Legacy: borrows source. Caller must keep source alive longer than
    any lexeme() access on returned tokens. The source is NOT copied.
    For tests calling tokenize("literal"), prefer tokenizeOwned.
  */
  static std::vector<StyioToken*> tokenize(const std::string& code);

  /*
    Tokenize with allocation metrics.
  */
  static std::vector<StyioToken*>
  tokenizeWithMetrics(const std::string& code, void* metrics_out);
};

#endif
