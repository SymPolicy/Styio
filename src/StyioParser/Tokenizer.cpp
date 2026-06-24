// [C++ STL]
#include <algorithm>
#include <array>
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
  auto abi_view = tokens[*cur]->lexeme();
  std::string abi(abi_view);
  std::transform(abi.begin(), abi.end(), abi.begin(),
    [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (abi != "c") { return false; }

  cur = styio_prev_non_trivia_index(tokens, *cur);
  if (!cur || tokens[*cur]->type != StyioTokenType::TOK_LPAREN) { return false; }

  cur = styio_prev_non_trivia_index(tokens, *cur);
  if (!cur || tokens[*cur]->type != StyioTokenType::NAME || tokens[*cur]->lexeme() != "extern") {
    return false;
  }

  cur = styio_prev_non_trivia_index(tokens, *cur);
  return cur && tokens[*cur]->type == StyioTokenType::TOK_AT && (is_cpp_abi || abi == "c");
}

bool
styio_try_scan_raw_string_literal_end(std::string_view code, size_t start, size_t& end) {
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

  const std::string delimiter = std::string(code.substr(quote_pos + 1, open_paren - quote_pos - 1));
  const std::string close_marker = ")" + delimiter + "\"";
  const size_t close_pos = code.find(close_marker, open_paren + 1);
  if (close_pos == std::string::npos) {
    return false;
  }

  end = close_pos + close_marker.size();
  return true;
}

size_t
styio_scan_native_extern_body_end(std::string_view code, size_t body_start) {
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
// Operator dispatch — constexpr precomputed bucket index.
// O(1) lookup: kOperatorBuckets[static_cast<unsigned char>(ch)].
// Bucket entries are length-descending for longest-match.
// ---------------------------------------------------------------

struct OperatorEntry {
  const char* spelling;
  StyioTokenType type;
  uint8_t length;
};

static constexpr OperatorEntry kOperatorTable[] = {
  // '!' (33)
  {"!=", StyioTokenType::BINOP_NE, 2},
  // '%' (37)
  {"%=", StyioTokenType::COMPOUND_MOD, 2},
  // '&' (38)
  {"&&", StyioTokenType::LOGIC_AND, 2},
  // '*' (42)
  {"**", StyioTokenType::BINOP_POW, 2},
  {"*=", StyioTokenType::COMPOUND_MUL, 2},
  // '+' (43)
  {"+=", StyioTokenType::COMPOUND_ADD, 2},
  // '-' (45)
  {"->", StyioTokenType::ARROW_SINGLE_RIGHT, 2},
  {"-=", StyioTokenType::COMPOUND_SUB, 2},
  // '/' (47)
  {"/=", StyioTokenType::COMPOUND_DIV, 2},
  // ':' (58)
  {":=", StyioTokenType::WALRUS, 2},
  // '<' (60) — longest-first
  {"<=", StyioTokenType::BINOP_LE, 2},
  {"<~", StyioTokenType::WAVE_LEFT, 2},
  {"<|", StyioTokenType::YIELD_PIPE, 2},
  {"<-", StyioTokenType::ARROW_SINGLE_LEFT, 2},
  // '=' (61) — longest-first ("==" handled by consecutive-count)
  {"=>", StyioTokenType::ARROW_DOUBLE_RIGHT, 2},
  // '>' (62)
  {">=", StyioTokenType::BINOP_GE, 2},
  {">_", StyioTokenType::PRINT, 2},
  // '?' (63)
  {"?|", StyioTokenType::AWAIT_PIPE, 2},
  {"?=", StyioTokenType::MATCH, 2},
  {"??", StyioTokenType::DBQUESTION, 2},
  // '[' (91)
  {"[|", StyioTokenType::BOUNDED_BUFFER_OPEN, 2},
  // '|' (124) — longest-first
  {"|<|", StyioTokenType::RETURN_PIPE, 3},
  {"||>", StyioTokenType::TASK_LAUNCH, 3},
  {"||", StyioTokenType::LOGIC_OR, 2},
  {"|;", StyioTokenType::PIPE_SEMICOLON, 2},
  {"|]", StyioTokenType::BOUNDED_BUFFER_CLOSE, 2},
  // '~' (126)
  {"~>", StyioTokenType::WAVE_RIGHT, 2},
};
static constexpr size_t kOpTableSize = sizeof(kOperatorTable) / sizeof(kOperatorTable[0]);

struct OperatorBucket { uint8_t start; uint8_t count; };
static constexpr OperatorBucket kEmptyBucket{0, 0};

// Precomputed at compile time: index into kOperatorTable by first char (0-127).
// Each entry stores start+count in the ordered kOperatorTable.
static constexpr std::array<OperatorBucket, 128> make_op_buckets() {
  std::array<OperatorBucket, 128> buckets{};
  for (auto& b : buckets) b = kEmptyBucket;
  uint8_t run_start = 0;
  char run_char = 0;
  bool in_run = false;
  for (size_t i = 0; i < kOpTableSize; ++i) {
    char fc = kOperatorTable[i].spelling[0];
    if (fc < 0 || fc >= 128) continue;
    auto uc = static_cast<unsigned char>(fc);
    if (!in_run || fc != run_char) {
      if (in_run) { buckets[static_cast<unsigned char>(run_char)] = {run_start, static_cast<uint8_t>(i - run_start)}; }
      run_start = static_cast<uint8_t>(i);
      run_char = fc;
      in_run = true;
    }
  }
  if (in_run) { buckets[static_cast<unsigned char>(run_char)] = {run_start, static_cast<uint8_t>(kOpTableSize - run_start)}; }
  return buckets;
}
static constexpr auto kOperatorBuckets = make_op_buckets();

// O(1) longest-match: kOperatorBuckets[ch] → try entries length-descending.
inline StyioToken* try_match_op(const char* data, size_t code_len, size_t loc, unsigned char ch) {
  if (ch >= 128) return nullptr;
  auto& bucket = kOperatorBuckets[ch];
  for (uint8_t i = 0; i < bucket.count; ++i) {
    auto& e = kOperatorTable[bucket.start + i];
    if (loc + e.length <= code_len && std::string_view(data + loc, e.length) == e.spelling)
      return StyioToken::CreateFromSpan(e.type, data, loc, e.length);
  }
  return nullptr;
}

inline size_t count_consecutive_chars(const char* data, size_t code_len, size_t start, char t) {
  size_t n = 0;
  while (start + n < code_len && data[start + n] == t) ++n;
  return n;
}

// ---------------------------------------------------------------
// String literal scanner with escape processing.
// Returns raw span (including quotes) and decoded value.
// Throws StyioLexError on unterminated strings or illegal newlines.
// ---------------------------------------------------------------
struct StringScanResult {
  size_t raw_begin;
  size_t raw_length;   // includes both quotes
  std::string decoded;
};

StringScanResult scan_string_literal(const char* data, size_t code_len, size_t start) {
  if (start >= code_len || data[start] != '"')
    throw StyioLexError("string literal must start with \" at offset " + std::to_string(start));

  size_t loc = start + 1;  // skip opening "
  std::string decoded;
  decoded.reserve(32);

  while (loc < code_len) {
    char ch = data[loc];
    if (ch == '"') {
      loc += 1;  // skip closing "
      return {start, loc - start, std::move(decoded)};
    }
    if (ch == '\\' && loc + 1 < code_len) {
      loc += 1;
      char esc = data[loc];
      switch (esc) {
        case '"':  decoded += '"';  break;
        case '\\': decoded += '\\'; break;
        case 'n':  decoded += '\n'; break;
        case 't':  decoded += '\t'; break;
        case 'r':  decoded += '\r'; break;
        case '0':  decoded += '\0'; break;
        default:
          // Unknown escape: keep both chars in decoded for stability.
          decoded += '\\';
          decoded += esc;
          break;
      }
      loc += 1;
      continue;
    }
    decoded += ch;
    loc += 1;
  }
  throw StyioLexError(
    "unterminated string literal at offset " + std::to_string(start));
}

// Helper: push a span-only token (zero allocation).
inline void push_token(
  TokenAccumulator& tokens, StyioTokenizerMetrics* m,
  StyioTokenType type, const char* data, size_t begin, size_t length
) {
  tokens.push_back(StyioToken::CreateFromSpan(type, data, begin, length));
  if (m) { m->source_view_token_count++; m->source_span_token_count++; }
}

// Helper: push a STRING token. Raw text is source span (zero-copy).
// decoded value stored separately.
inline void push_string_token(
  TokenAccumulator& tokens, StyioTokenizerMetrics* m,
  const char* src, const StringScanResult& sr
) {
  if (m) { m->string_decodes++; if (!sr.decoded.empty()) { m->decoded_text_token_count++; m->decoded_text_bytes += sr.decoded.size(); } }
  tokens.push_back(StyioToken::CreateString(src, sr.raw_begin, sr.raw_length, sr.decoded));
}

}  // namespace

// ---------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------

// ---------------------------------------------------------------
// Internal: core scan loop — takes source data pointer + length.
// ---------------------------------------------------------------
static std::vector<StyioToken*>
tokenize_impl(const char* src_data, size_t code_len, StyioTokenizerMetrics* m) {
  if (m) { m->input_bytes = code_len; }

  TokenAccumulator tokens;
  size_t loc = 0;

  while (loc < code_len) {
    const char ch = src_data[loc];

    // ---- Whitespace ----
    if (ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f') {
      push_token(tokens,m, StyioTokenType::TOK_SPACE, src_data, loc, 1);
      loc += 1; continue;
    }
    if (ch == '\n') {
      push_token(tokens,m, StyioTokenType::TOK_LF, src_data, loc, 1);
      loc += 1; continue;
    }
    if (ch == '\r') {
      push_token(tokens,m, StyioTokenType::TOK_CR, src_data, loc, 1);
      loc += 1; continue;
    }

    // ---- Line comment // ----
    if (ch == '/' && loc + 1 < code_len && src_data[loc + 1] == '/') {
      size_t begin = loc; loc += 2;
      while (loc < code_len && src_data[loc] != '\n' && src_data[loc] != '\r') ++loc;
      push_token(tokens,m, StyioTokenType::COMMENT_LINE, src_data, begin, loc - begin);
      continue;
    }

    // ---- Block comment /* */ ----
    if (ch == '/' && loc + 1 < code_len && src_data[loc + 1] == '*') {
      size_t begin = loc; loc += 2;
      while (loc + 1 < code_len && !(src_data[loc] == '*' && src_data[loc + 1] == '/')) ++loc;
      if (loc + 1 >= code_len)
        throw StyioLexError("Unterminated block comment at offset " + std::to_string(begin));
      loc += 2;
      push_token(tokens,m, StyioTokenType::COMMENT_CLOSED, src_data, begin, loc - begin);
      continue;
    }

    // ---- Identifier ----
    if (StyioUnicode::is_identifier_start(ch)) {
      size_t begin = loc; loc += 1;
      while (loc < code_len && StyioUnicode::is_identifier_continue(src_data[loc])) ++loc;
      size_t len = loc - begin;
      if (len == 1 && src_data[begin] == '_')
        push_token(tokens,m, StyioTokenType::TOK_UNDLINE, src_data, begin, 1);
      else
        push_token(tokens, m, StyioTokenType::NAME, src_data, begin, len);
      continue;
    }

    // ---- Number ----
    if (StyioUnicode::is_digit(ch)) {
      size_t begin = loc; loc += 1;
      while (loc < code_len && StyioUnicode::is_digit(src_data[loc])) ++loc;
      if (loc + 1 < code_len && src_data[loc] == '.' && StyioUnicode::is_digit(src_data[loc + 1])) {
        loc += 1;
        while (loc < code_len && StyioUnicode::is_digit(src_data[loc])) ++loc;
        push_token(tokens, m, StyioTokenType::DECIMAL, src_data, begin, loc - begin);
      } else {
        push_token(tokens, m, StyioTokenType::INTEGER, src_data, begin, loc - begin);
      }
      continue;
    }

    // ---- String literal (with escape processing) ----
    if (ch == '"') {
      auto sr = scan_string_literal(src_data, code_len, loc);
      size_t end = sr.raw_begin + sr.raw_length;
      push_string_token(tokens, m, src_data, sr);
      loc = end;
      continue;
    }

    // ---- Multi-char operator (constexpr bucket lookup) ----
    {
      auto uc = static_cast<unsigned char>(ch);
      if (uc < 128 && kOperatorBuckets[uc].count > 0) {
        if (m) m->operator_bucket_probes++;
        StyioToken* matched = try_match_op(src_data, code_len, loc, uc);
        if (matched) {
          tokens.push_back(matched);
          if (m) { m->source_view_token_count++; m->source_span_token_count++; }
          loc += matched->len();
          continue;
        }
      }
    }

    // ---- Single-char and repeat-char dispatch ----
    switch (ch) {
      case '!': push_token(tokens,m,StyioTokenType::TOK_EXCLAM,src_data,loc,1); loc+=1; break;
      case '#': push_token(tokens,m,StyioTokenType::TOK_HASH,src_data,loc,1); loc+=1; break;
      case '$': push_token(tokens,m,StyioTokenType::TOK_DOLLAR,src_data,loc,1); loc+=1; break;
      case '%': push_token(tokens,m,StyioTokenType::TOK_PERCENT,src_data,loc,1); loc+=1; break;
      case '&': push_token(tokens,m,StyioTokenType::TOK_AMP,src_data,loc,1); loc+=1; break;
      case '\'': push_token(tokens,m,StyioTokenType::TOK_SQUOTE,src_data,loc,1); loc+=1; break;
      case '(': push_token(tokens,m,StyioTokenType::TOK_LPAREN,src_data,loc,1); loc+=1; break;
      case ')': push_token(tokens,m,StyioTokenType::TOK_RPAREN,src_data,loc,1); loc+=1; break;
      case '*': push_token(tokens,m,StyioTokenType::TOK_STAR,src_data,loc,1); loc+=1; break;
      case '+': push_token(tokens,m,StyioTokenType::TOK_PLUS,src_data,loc,1); loc+=1; break;
      case ',': push_token(tokens,m,StyioTokenType::TOK_COMMA,src_data,loc,1); loc+=1; break;
      case '-': {
        size_t n = 1 + count_consecutive_chars(src_data,code_len,loc+1,'-');
        if (n >= 3) { push_token(tokens,m,StyioTokenType::SINGLE_SEP_LINE,src_data,loc,n); loc+=n; }
        else { push_token(tokens,m,StyioTokenType::TOK_MINUS,src_data,loc,1); loc+=1; }
        break;
      }
      case '.': {
        size_t n = 1 + count_consecutive_chars(src_data,code_len,loc+1,'.');
        push_token(tokens,m, n==1?StyioTokenType::TOK_DOT:StyioTokenType::ELLIPSIS,src_data,loc,n);
        loc+=n; break;
      }
      case '/': push_token(tokens,m,StyioTokenType::TOK_SLASH,src_data,loc,1); loc+=1; break;
      case ':': push_token(tokens,m,StyioTokenType::TOK_COLON,src_data,loc,1); loc+=1; break;
      case ';': push_token(tokens,m,StyioTokenType::TOK_SEMICOLON,src_data,loc,1); loc+=1; break;
      case '<': {
        size_t n = 1 + count_consecutive_chars(src_data,code_len,loc+1,'<');
        push_token(tokens,m, n>=2?StyioTokenType::EXTRACTOR:StyioTokenType::TOK_LANGBRAC,src_data,loc,n);
        loc+=n; break;
      }
      case '=': {
        size_t n = 1 + count_consecutive_chars(src_data,code_len,loc+1,'=');
        if (n>=3) { push_token(tokens,m,StyioTokenType::DOUBLE_SEP_LINE,src_data,loc,n); loc+=n; }
        else if (n==2) { push_token(tokens,m,StyioTokenType::BINOP_EQ,src_data,loc,2); loc+=2; }
        else { push_token(tokens,m,StyioTokenType::TOK_EQUAL,src_data,loc,1); loc+=1; }
        break;
      }
      case '>': {
        size_t n = 1 + count_consecutive_chars(src_data,code_len,loc+1,'>');
        push_token(tokens,m, n>=2?StyioTokenType::ITERATOR:StyioTokenType::TOK_RANGBRAC,src_data,loc,n);
        loc+=n; break;
      }
      case '?': push_token(tokens,m,StyioTokenType::TOK_QUEST,src_data,loc,1); loc+=1; break;
      case '@': push_token(tokens,m,StyioTokenType::TOK_AT,src_data,loc,1); loc+=1; break;
      case '[': push_token(tokens,m,StyioTokenType::TOK_LBOXBRAC,src_data,loc,1); loc+=1; break;
      case '\\': push_token(tokens,m,StyioTokenType::TOK_BACKSLASH,src_data,loc,1); loc+=1; break;
      case ']': push_token(tokens,m,StyioTokenType::TOK_RBOXBRAC,src_data,loc,1); loc+=1; break;
      case '^': push_token(tokens,m,StyioTokenType::TOK_HAT,src_data,loc,1); loc+=1; break;
      case '_': push_token(tokens,m,StyioTokenType::TOK_UNDLINE,src_data,loc,1); loc+=1; break;
      case '`': push_token(tokens,m,StyioTokenType::TOK_BQUOTE,src_data,loc,1); loc+=1; break;
      case '{': {
        bool is_native = styio_recent_tokens_open_native_extern_body(tokens.view());
        push_token(tokens,m,StyioTokenType::TOK_LCURBRAC,src_data,loc,1); loc+=1;
        if (is_native) {
          size_t body_start = loc;
          size_t body_end = styio_scan_native_extern_body_end(
            std::string_view(src_data, code_len), body_start);
          push_token(tokens,m,StyioTokenType::NATIVE_EXTERN_BODY,src_data,body_start,body_end-body_start);
          push_token(tokens,m,StyioTokenType::TOK_RCURBRAC,src_data,body_end,1);
          loc = body_end + 1;
        }
        break;
      }
      case '|': push_token(tokens,m,StyioTokenType::TOK_PIPE,src_data,loc,1); loc+=1; break;
      case '}': push_token(tokens,m,StyioTokenType::TOK_RCURBRAC,src_data,loc,1); loc+=1; break;
      case '~': push_token(tokens,m,StyioTokenType::TOK_TILDE,src_data,loc,1); loc+=1; break;
      default:
        push_token(tokens,m,StyioTokenType::UNKNOWN,src_data,loc,1); loc+=1; break;
    }
  }

  // EOF — zero-width span at source end
  tokens.push_back(StyioToken::CreateFromSpan(StyioTokenType::TOK_EOF, src_data, code_len, 0));
  if (m) {
    m->token_count = tokens.view().size();
    m->source_span_token_count++;
    m->zero_width_span_token_count++;
  }
  return tokens.release();
}

// ---------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------

StyioTokenStream
StyioTokenizer::tokenizeOwned(std::string source) {
  auto src_len = source.size();
  auto src_data = source.data();
  auto tokens = tokenize_impl(src_data, src_len, nullptr);
  return StyioTokenStream(std::move(source), std::move(tokens));
}

// Per-call source buffer registry — keeps source alive for legacy tokenize().
static std::vector<std::unique_ptr<std::string>> g_legacy_sources;

std::vector<StyioToken*>
StyioTokenizer::tokenize(const std::string& code) {
  auto copy = std::make_unique<std::string>(code);
  auto* data = copy->data();
  auto size = copy->size();
  g_legacy_sources.push_back(std::move(copy));
  return tokenize_impl(data, size, nullptr);
}

std::vector<StyioToken*>
StyioTokenizer::tokenizeWithMetrics(const std::string& code, void* metrics_out) {
  auto* m = static_cast<StyioTokenizerMetrics*>(metrics_out);
  if (m) m->source_copy_bytes = code.size();
  auto copy = std::make_unique<std::string>(code);
  auto* data = copy->data();
  auto size = copy->size();
  g_legacy_sources.push_back(std::move(copy));
  return tokenize_impl(data, size, m);
}
