// [C++ STL]
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "../StyioUnicode/Unicode.hpp"
#include "../StyioUtil/Util.hpp"
#include "Tokenizer.hpp"

namespace {

bool
styio_tokenizer_is_trivia(StyioTokenType type) {
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

class TokenAccumulator
{
  std::vector<StyioToken*> tokens_;

public:
  TokenAccumulator() = default;
  TokenAccumulator(const TokenAccumulator&) = delete;
  TokenAccumulator& operator=(const TokenAccumulator&) = delete;

  ~TokenAccumulator() {
    for (auto* token : tokens_) {
      delete token;
    }
  }

  void
  push_back(StyioToken* token) {
    tokens_.push_back(token);
  }

  const std::vector<StyioToken*>&
  view() const {
    return tokens_;
  }

  std::vector<StyioToken*>
  release() {
    std::vector<StyioToken*> out;
    out.swap(tokens_);
    return out;
  }
};

std::optional<size_t>
styio_prev_non_trivia_index(const std::vector<StyioToken*>& tokens, size_t before) {
  while (before > 0) {
    before -= 1;
    if (!styio_tokenizer_is_trivia(tokens[before]->type)) {
      return before;
    }
  }
  return std::nullopt;
}

bool
styio_recent_tokens_open_native_extern_body(const std::vector<StyioToken*>& tokens) {
  auto cur = styio_prev_non_trivia_index(tokens, tokens.size());
  if (!cur) {
    return false;
  }

  if (tokens[*cur]->type == StyioTokenType::ARROW_DOUBLE_RIGHT) {
    cur = styio_prev_non_trivia_index(tokens, *cur);
  }

  if (!cur || tokens[*cur]->type != StyioTokenType::TOK_RPAREN) {
    return false;
  }

  cur = styio_prev_non_trivia_index(tokens, *cur);
  if (!cur) {
    return false;
  }

  bool is_cpp_abi = false;
  if (tokens[*cur]->type == StyioTokenType::TOK_PLUS) {
    cur = styio_prev_non_trivia_index(tokens, *cur);
    if (!cur || tokens[*cur]->type != StyioTokenType::TOK_PLUS) {
      return false;
    }
    is_cpp_abi = true;
    cur = styio_prev_non_trivia_index(tokens, *cur);
  }

  if (!cur || tokens[*cur]->type != StyioTokenType::NAME) {
    return false;
  }
  std::string abi = tokens[*cur]->original;
  std::transform(
    abi.begin(),
    abi.end(),
    abi.begin(),
    [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (abi != "c") {
    return false;
  }

  cur = styio_prev_non_trivia_index(tokens, *cur);
  if (!cur || tokens[*cur]->type != StyioTokenType::TOK_LPAREN) {
    return false;
  }

  cur = styio_prev_non_trivia_index(tokens, *cur);
  if (!cur || tokens[*cur]->type != StyioTokenType::NAME || tokens[*cur]->original != "extern") {
    return false;
  }

  cur = styio_prev_non_trivia_index(tokens, *cur);
  return cur && tokens[*cur]->type == StyioTokenType::TOK_AT && (is_cpp_abi || abi == "c");
}

bool
styio_try_scan_raw_string_literal_end(const std::string& code, size_t start, size_t& end) {
  std::string prefix;
  if (code.compare(start, 3, "u8R") == 0) {
    prefix = "u8R";
  }
  else if (code.compare(start, 2, "uR") == 0
           || code.compare(start, 2, "UR") == 0
           || code.compare(start, 2, "LR") == 0) {
    prefix = code.substr(start, 2);
  }
  else if (code.compare(start, 1, "R") == 0) {
    prefix = "R";
  }
  else {
    return false;
  }

  const size_t quote_pos = start + prefix.size();
  if (quote_pos >= code.size() || code[quote_pos] != '"') {
    return false;
  }

  const size_t open_paren = code.find('(', quote_pos + 1);
  if (open_paren == std::string::npos) {
    return false;
  }

  const std::string delimiter = code.substr(quote_pos + 1, open_paren - quote_pos - 1);
  const std::string close_marker = ")" + delimiter + "\"";
  const size_t close_pos = code.find(close_marker, open_paren + 1);
  if (close_pos == std::string::npos) {
    return false;
  }

  end = close_pos + close_marker.size();
  return true;
}

size_t
styio_scan_native_extern_body_end(const std::string& code, size_t body_start) {
  enum class Mode { Normal, LineComment, BlockComment, StringLiteral, CharLiteral };
  Mode mode = Mode::Normal;
  bool escaped = false;
  int brace_depth = 1;
  size_t loc = body_start;

  while (loc < code.size()) {
    const char ch = code[loc];
    const char next = loc + 1 < code.size() ? code[loc + 1] : '\0';

    switch (mode) {
      case Mode::Normal: {
        size_t raw_end = 0;
        if (styio_try_scan_raw_string_literal_end(code, loc, raw_end)) {
          loc = raw_end;
          continue;
        }
        if (ch == '/' && next == '/') {
          mode = Mode::LineComment;
          loc += 2;
          continue;
        }
        if (ch == '/' && next == '*') {
          mode = Mode::BlockComment;
          loc += 2;
          continue;
        }
        if (ch == '"') {
          mode = Mode::StringLiteral;
          escaped = false;
          loc += 1;
          continue;
        }
        if (ch == '\'') {
          mode = Mode::CharLiteral;
          escaped = false;
          loc += 1;
          continue;
        }
        if (ch == '{') {
          brace_depth += 1;
        }
        else if (ch == '}') {
          brace_depth -= 1;
          if (brace_depth == 0) {
            return loc;
          }
        }
        loc += 1;
        break;
      }
      case Mode::LineComment:
        if (ch == '\n' || ch == '\r') {
          mode = Mode::Normal;
        }
        loc += 1;
        break;
      case Mode::BlockComment:
        if (ch == '*' && next == '/') {
          mode = Mode::Normal;
          loc += 2;
        }
        else {
          loc += 1;
        }
        break;
      case Mode::StringLiteral:
        if (escaped) {
          escaped = false;
        }
        else if (ch == '\\') {
          escaped = true;
        }
        else if (ch == '"') {
          mode = Mode::Normal;
        }
        loc += 1;
        break;
      case Mode::CharLiteral:
        if (escaped) {
          escaped = false;
        }
        else if (ch == '\\') {
          escaped = true;
        }
        else if (ch == '\'') {
          mode = Mode::Normal;
        }
        loc += 1;
        break;
    }
  }

  throw StyioLexError(
    "Unterminated native @extern block at offset " + std::to_string(body_start));
}

// ---------------------------------------------------------------
// Operator dispatch table — O(1) lookup by first-char bucket,
// longest-match via length-descending entries within each bucket.
// ---------------------------------------------------------------

struct OperatorEntry
{
  const char* spelling;
  StyioTokenType type;
  uint8_t length;        // strlen(spelling), precomputed
  uint8_t min_match;     // minimum chars to match before checking (optimization)
};

/*
  Each bucket contains entries sorted by length descending so the
  first match is always the longest (longest-match rule).

  Entries with length 1 are handled by the ASCII-mapped single-char
  dispatch and don't appear here unless they have multi-char siblings.
*/

// Operator entries organized by first character for longest-match.
// Ordered longest-first within each character group.
static constexpr OperatorEntry kOperatorTable[] = {
  // '!' (33)
  {"!=", StyioTokenType::BINOP_NE, 2, 2},

  // '%' (37)
  {"%=", StyioTokenType::COMPOUND_MOD, 2, 2},

  // '&' (38)
  {"&&", StyioTokenType::LOGIC_AND, 2, 2},

  // '*' (42)
  {"**", StyioTokenType::BINOP_POW, 2, 2},
  {"*=", StyioTokenType::COMPOUND_MUL, 2, 2},

  // '+' (43)
  {"+=", StyioTokenType::COMPOUND_ADD, 2, 2},

  // '-' (45)
  {"->", StyioTokenType::ARROW_SINGLE_RIGHT, 2, 2},
  {"-=", StyioTokenType::COMPOUND_SUB, 2, 2},

  // '/' (47)
  {"/=", StyioTokenType::COMPOUND_DIV, 2, 2},

  // ':' (58)
  {":=", StyioTokenType::WALRUS, 2, 2},

  // '<' (60) — longest-first
  {"<=", StyioTokenType::BINOP_LE, 2, 2},
  {"<~", StyioTokenType::WAVE_LEFT, 2, 2},
  {"<|", StyioTokenType::YIELD_PIPE, 2, 2},
  {"<-", StyioTokenType::ARROW_SINGLE_LEFT, 2, 2},

  // '=' (61) — longest-first
  // "==" is handled by consecutive-count logic, not the table
  {"=>", StyioTokenType::ARROW_DOUBLE_RIGHT, 2, 2},

  // '>' (62) — longest-first
  {">=", StyioTokenType::BINOP_GE, 2, 2},
  {">_", StyioTokenType::PRINT, 2, 2},

  // '?' (63)
  {"?|", StyioTokenType::AWAIT_PIPE, 2, 2},
  {"?=", StyioTokenType::MATCH, 2, 2},
  {"??", StyioTokenType::DBQUESTION, 2, 2},

  // '[' (91)
  {"[|", StyioTokenType::BOUNDED_BUFFER_OPEN, 2, 2},

  // '|' (124) — longest-first
  {"|<|", StyioTokenType::RETURN_PIPE, 3, 2},
  {"||>", StyioTokenType::TASK_LAUNCH, 3, 2},
  {"||", StyioTokenType::LOGIC_OR, 2, 2},
  {"|;", StyioTokenType::PIPE_SEMICOLON, 2, 2},
  {"|]", StyioTokenType::BOUNDED_BUFFER_CLOSE, 2, 2},

  // '~' (126)
  {"~>", StyioTokenType::WAVE_RIGHT, 2, 2},
};

/*
  Bucket index table: for each ASCII char 0-127, the start index
  into kOperatorTable. 0xFF means no multi-char operators for that char.
  kOperatorTable is already ordered by first character so buckets are
  contiguous ranges.
*/
struct OperatorBucket
{
  uint8_t start;  // index into kOperatorTable
  uint8_t count;  // number of entries for this first char
};

static constexpr size_t kOpTableSize = sizeof(kOperatorTable) / sizeof(kOperatorTable[0]);

// Build the bucket index at compile time via a helper.
// The table is ordered by first-char, so we compute run lengths.
static constexpr OperatorBucket
build_op_bucket(char c) {
  OperatorBucket bucket = {0, 0};
  bool found = false;
  for (size_t i = 0; i < kOpTableSize; ++i) {
    if (kOperatorTable[i].spelling[0] == c) {
      if (!found) {
        bucket.start = static_cast<uint8_t>(i);
        found = true;
      }
      bucket.count++;
    }
    else if (found) {
      break;  // past this char's run
    }
  }
  return bucket;
}

// Inline helper: try to match a multi-char operator starting at loc.
// Returns the matched token or nullptr if no match.
inline StyioToken*
try_match_operator(
  const char* data,
  size_t code_len,
  size_t loc,
  const OperatorBucket& bucket
) {
  for (uint8_t i = 0; i < bucket.count; ++i) {
    const auto& entry = kOperatorTable[bucket.start + i];
    if (loc + entry.length <= code_len
        && std::string_view(data + loc, entry.length) == entry.spelling) {
      return StyioToken::CreateFromSpan(entry.type, data, loc, entry.length);
    }
  }
  return nullptr;
}

// Repeat-char tokens: ---, ===, ..., <<<, >>>  etc.
// Counts consecutive repeats of character ch starting at loc.
inline size_t
count_consecutive_chars(const char* data, size_t code_len, size_t start, char target) {
  size_t count = 0;
  while (start + count < code_len && data[start + count] == target) {
    count += 1;
  }
  return count;
}

}  // namespace

// ---------------------------------------------------------------
// Public tokenizer entry point (span-first)
//
// Complexity: O(n) single-pass linear scan over the source.
// Tokens store source spans; `original` strings are constructed
// once from the span (not char-by-char).
// ---------------------------------------------------------------
std::vector<StyioToken*>
StyioTokenizer::tokenize(const std::string& code) {
  TokenAccumulator tokens;
  const char* data = code.data();
  const size_t code_len = code.size();
  size_t loc = 0;

  while (loc < code_len) {
    const char ch = data[loc];

    // ---- Whitespace fast-path ----
    switch (ch) {
      case ' ':
      case '\t':
      case '\v':
      case '\f': {
        tokens.push_back(
          StyioToken::CreateFromSpan(StyioTokenType::TOK_SPACE, data, loc, 1));
        loc += 1;
        continue;
      }
      case '\n': {
        tokens.push_back(
          StyioToken::CreateFromSpan(StyioTokenType::TOK_LF, data, loc, 1));
        loc += 1;
        continue;
      }
      case '\r': {
        tokens.push_back(
          StyioToken::CreateFromSpan(StyioTokenType::TOK_CR, data, loc, 1));
        loc += 1;
        continue;
      }
      default:
        break;
    }

    // ---- Line comment // ----
    if (ch == '/' && loc + 1 < code_len && data[loc + 1] == '/') {
      const size_t begin = loc;
      loc += 2;
      while (loc < code_len && data[loc] != '\n' && data[loc] != '\r') {
        loc += 1;
      }
      tokens.push_back(
        StyioToken::CreateFromSpan(StyioTokenType::COMMENT_LINE, data, begin, loc - begin));
      continue;
    }

    // ---- Block comment /* ... */ ----
    if (ch == '/' && loc + 1 < code_len && data[loc + 1] == '*') {
      const size_t begin = loc;
      loc += 2;
      while (loc + 1 < code_len && !(data[loc] == '*' && data[loc + 1] == '/')) {
        loc += 1;
      }
      if (loc + 1 >= code_len) {
        throw StyioLexError(
          "Unterminated block comment at offset " + std::to_string(begin));
      }
      loc += 2;  // consume */
      tokens.push_back(
        StyioToken::CreateFromSpan(StyioTokenType::COMMENT_CLOSED, data, begin, loc - begin));
      continue;
    }

    // ---- Identifier / keyword ----
    if (StyioUnicode::is_identifier_start(ch)) {
      const size_t begin = loc;
      loc += 1;
      while (loc < code_len
             && StyioUnicode::is_identifier_continue(data[loc])) {
        loc += 1;
      }
      const size_t len = loc - begin;
      if (len == 1 && data[begin] == '_') {
        tokens.push_back(
          StyioToken::CreateFromSpan(StyioTokenType::TOK_UNDLINE, data, begin, 1));
      }
      else {
        tokens.push_back(
          StyioToken::CreateFromSpan(StyioTokenType::NAME, data, begin, len));
      }
      continue;
    }

    // ---- Number (integer / float) ----
    if (StyioUnicode::is_digit(ch)) {
      const size_t begin = loc;
      loc += 1;
      while (loc < code_len && StyioUnicode::is_digit(data[loc])) {
        loc += 1;
      }
      // Float: xxx.yyy
      if (loc + 1 < code_len && data[loc] == '.'
          && StyioUnicode::is_digit(data[loc + 1])) {
        loc += 1;  // consume '.'
        while (loc < code_len && StyioUnicode::is_digit(data[loc])) {
          loc += 1;
        }
        tokens.push_back(
          StyioToken::CreateFromSpan(StyioTokenType::DECIMAL, data, begin, loc - begin));
      }
      else {
        tokens.push_back(
          StyioToken::CreateFromSpan(StyioTokenType::INTEGER, data, begin, loc - begin));
      }
      continue;
    }

    // ---- String literal ----
    if (ch == '"') {
      const size_t begin = loc;
      loc += 1;
      while (loc < code_len && data[loc] != '"') {
        loc += 1;
      }
      if (loc >= code_len) {
        throw StyioLexError(
          "Unterminated string literal at offset " + std::to_string(begin));
      }
      loc += 1;  // consume closing "
      tokens.push_back(
        StyioToken::CreateFromSpan(StyioTokenType::STRING, data, begin, loc - begin));
      continue;
    }

    // ---- Operator / symbol dispatch ----
    // Try multi-char first (longest-match), then fall back to single-char.

    // Characters that have multi-char operator entries
    if (ch == '!' || ch == '%' || ch == '&' || ch == '*' || ch == '+' || ch == '-'
        || ch == '/' || ch == ':' || ch == '<' || ch == '=' || ch == '>' || ch == '?'
        || ch == '[' || ch == '|' || ch == '~') {
      OperatorBucket bucket = build_op_bucket(ch);
      if (bucket.count > 0) {
        StyioToken* matched = try_match_operator(data, code_len, loc, bucket);
        if (matched != nullptr) {
          tokens.push_back(matched);
          loc += matched->len();
          continue;
        }
      }
    }

    // ---- Character-level dispatch for remaining cases ----
    switch (ch) {
      case '!':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_EXCLAM, data, loc, 1));
        loc += 1;
        break;

      case '#':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_HASH, data, loc, 1));
        loc += 1;
        break;

      case '$':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_DOLLAR, data, loc, 1));
        loc += 1;
        break;

      case '%':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_PERCENT, data, loc, 1));
        loc += 1;
        break;

      case '&':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_AMP, data, loc, 1));
        loc += 1;
        break;

      case '\'':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_SQUOTE, data, loc, 1));
        loc += 1;
        break;

      case '(':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_LPAREN, data, loc, 1));
        loc += 1;
        break;

      case ')':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_RPAREN, data, loc, 1));
        loc += 1;
        break;

      case '*':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_STAR, data, loc, 1));
        loc += 1;
        break;

      case '+':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_PLUS, data, loc, 1));
        loc += 1;
        break;

      case ',':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_COMMA, data, loc, 1));
        loc += 1;
        break;

      case '-': {
        // Check for --- (SINGLE_SEP_LINE) — multi-dash
        size_t dash_count = 1 + count_consecutive_chars(data, code_len, loc + 1, '-');
        if (dash_count >= 3) {
          tokens.push_back(
            StyioToken::CreateFromSpan(StyioTokenType::SINGLE_SEP_LINE, data, loc, dash_count));
          loc += dash_count;
        }
        else {
          tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_MINUS, data, loc, 1));
          loc += 1;
        }
        break;
      }

      case '.': {
        size_t dot_count = 1 + count_consecutive_chars(data, code_len, loc + 1, '.');
        if (dot_count == 1) {
          tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_DOT, data, loc, 1));
        }
        else {
          tokens.push_back(
            StyioToken::CreateFromSpan(StyioTokenType::ELLIPSIS, data, loc, dot_count));
        }
        loc += dot_count;
        break;
      }

      case '/':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_SLASH, data, loc, 1));
        loc += 1;
        break;

      case ':':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_COLON, data, loc, 1));
        loc += 1;
        break;

      case ';':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_SEMICOLON, data, loc, 1));
        loc += 1;
        break;

      case '<': {
        // <<, <<< etc. → EXTRACTOR
        size_t lt_count = 1 + count_consecutive_chars(data, code_len, loc + 1, '<');
        if (lt_count >= 2) {
          tokens.push_back(
            StyioToken::CreateFromSpan(StyioTokenType::EXTRACTOR, data, loc, lt_count));
          loc += lt_count;
        }
        else {
          tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_LANGBRAC, data, loc, 1));
          loc += 1;
        }
        break;
      }

      case '=': {
        // => already tried, now handle = / == / ===
        size_t eq_count = 1 + count_consecutive_chars(data, code_len, loc + 1, '=');
        if (eq_count >= 3) {
          tokens.push_back(
            StyioToken::CreateFromSpan(StyioTokenType::DOUBLE_SEP_LINE, data, loc, eq_count));
          loc += eq_count;
        }
        else if (eq_count == 2) {
          tokens.push_back(
            StyioToken::CreateFromSpan(StyioTokenType::BINOP_EQ, data, loc, 2));
          loc += 2;
        }
        else {
          tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_EQUAL, data, loc, 1));
          loc += 1;
        }
        break;
      }

      case '>': {
        // >>, >>> etc. → ITERATOR
        size_t gt_count = 1 + count_consecutive_chars(data, code_len, loc + 1, '>');
        if (gt_count >= 2) {
          tokens.push_back(
            StyioToken::CreateFromSpan(StyioTokenType::ITERATOR, data, loc, gt_count));
          loc += gt_count;
        }
        else {
          tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_RANGBRAC, data, loc, 1));
          loc += 1;
        }
        break;
      }

      case '?':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_QUEST, data, loc, 1));
        loc += 1;
        break;

      case '@':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_AT, data, loc, 1));
        loc += 1;
        break;

      case '[':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_LBOXBRAC, data, loc, 1));
        loc += 1;
        break;

      case '\\':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_BACKSLASH, data, loc, 1));
        loc += 1;
        break;

      case ']':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_RBOXBRAC, data, loc, 1));
        loc += 1;
        break;

      case '^':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_HAT, data, loc, 1));
        loc += 1;
        break;

      case '_':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_UNDLINE, data, loc, 1));
        loc += 1;
        break;

      case '`':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_BQUOTE, data, loc, 1));
        loc += 1;
        break;

      case '{': {
        const bool is_native_extern_body =
          styio_recent_tokens_open_native_extern_body(tokens.view());
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_LCURBRAC, data, loc, 1));
        loc += 1;
        if (is_native_extern_body) {
          const size_t body_start = loc;
          const size_t body_end = styio_scan_native_extern_body_end(code, body_start);
          tokens.push_back(
            StyioToken::CreateFromSpan(
              StyioTokenType::NATIVE_EXTERN_BODY, data, body_start, body_end - body_start));
          tokens.push_back(
            StyioToken::CreateFromSpan(StyioTokenType::TOK_RCURBRAC, data, body_end, 1));
          loc = body_end + 1;
        }
        break;
      }

      case '|':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_PIPE, data, loc, 1));
        loc += 1;
        break;

      case '}':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_RCURBRAC, data, loc, 1));
        loc += 1;
        break;

      case '~':
        tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_TILDE, data, loc, 1));
        loc += 1;
        break;

      default: {
        // Unrecognized byte (e.g. embedded NUL): advance past it.
        tokens.push_back(
          StyioToken::CreateFromSpan(StyioTokenType::UNKNOWN, data, loc, 1));
        loc += 1;
        break;
      }
    }
  }

  // EOF sentinel
  tokens.push_back(StyioToken::Create(StyioTokenType::TOK_EOF, ""));
  return tokens.release();
}
