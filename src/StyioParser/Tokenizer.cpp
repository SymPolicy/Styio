// [C++ STL]
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <regex>
#include <string>
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
  if (!cur || tokens[*cur]->type != StyioTokenType::ARROW_DOUBLE_RIGHT) {
    return false;
  }

  cur = styio_prev_non_trivia_index(tokens, *cur);
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

}  // namespace

size_t
count_consecutive(const std::string &text, size_t start, char target) {
  size_t count = 0;

  while (start + count < text.length()
         && text.at(start + count) == target) {
    count += 1;
  }

  return count;
}

std::vector<StyioToken *>
StyioTokenizer::tokenize(std::string code) {
  std::vector<StyioToken *> tokens;
  unsigned long long loc = 0; /* local position */

  while (loc < code.length()) {
    /* Single-char whitespace tokens are terminal for this loop turn. */
    switch (code.at(loc)) {
      case ' ':
      case '\t':
      case '\v':
      case '\f': {
        tokens.push_back(
          StyioToken::Create(StyioTokenType::TOK_SPACE, std::string(1, code.at(loc))));
        loc += 1;
        continue;
      }

      /* LF */
      case '\n': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LF, "\n"));
        loc += 1;
        continue;
      }

      /* CR */
      case '\r': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_CR, "\r"));
        loc += 1;
        continue;
      }

      default: {
      } break;
    }

    if (loc >= code.length()) {
      break;
    }

    // commments
    if (code.compare(loc, 2, "//") == 0) {
      std::string literal = "//";
      loc += 2;

      while (loc < code.length()
             && code.at(loc) != '\n'
             && code.at(loc) != '\r') {
        literal += code.at(loc);
        loc += 1;
      }

      tokens.push_back(StyioToken::Create(StyioTokenType::COMMENT_LINE, literal));
      continue;
    }
    /* comments */
    else if (code.compare(loc, 2, "/*") == 0) {
      std::string literal = "/*";
      const unsigned long long start = loc;
      loc += 2;

      while (loc < code.length() && not(code.compare(loc, 2, "*/") == 0)) {
        literal += code.at(loc);
        loc += 1;
      }
      if (loc >= code.length()) {
        throw StyioLexError(
          "Unterminated block comment at offset " + std::to_string(start));
      }

      literal += "*/";
      loc += 2;

      tokens.push_back(StyioToken::Create(StyioTokenType::COMMENT_CLOSED, literal));
      continue;
    }

    /* varname / typename */
    if (StyioUnicode::is_identifier_start(code.at(loc))) {
      std::string literal;

      do {
        literal += code.at(loc);
        loc += 1;
      } while (loc < code.length()
               && StyioUnicode::is_identifier_continue(code.at(loc)));

      if (literal == "_") {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_UNDLINE, "_"));
      }
      else {
        tokens.push_back(StyioToken::Create(StyioTokenType::NAME, literal));
      }
      continue;
    }
    /* integer / float / decimal */
    else if (StyioUnicode::is_digit(code.at(loc))) {
      std::string literal;

      do {
        literal += code.at(loc);
        loc += 1;
      } while (loc < code.length() && StyioUnicode::is_digit(code.at(loc)));

      /* If Float: xxx.yyy */
      if (loc + 1 < code.length() && code.at(loc) == '.' && StyioUnicode::is_digit(code.at(loc + 1))) {
        /* include '.' */
        literal += code.at(loc);
        loc += 1;

        /* include yyy */
        do {
          literal += code.at(loc);
          loc += 1;
        } while (loc < code.length() && StyioUnicode::is_digit(code.at(loc)));

        tokens.push_back(StyioToken::Create(StyioTokenType::DECIMAL, literal));
      }
      else {
        tokens.push_back(StyioToken::Create(StyioTokenType::INTEGER, literal));
      }
      continue;
    }

    if (loc >= code.length()) {
      continue;
    }

    switch (code.at(loc)) {
      // 33
      case '!': {
        if (loc + 1 < code.size() && code.at(loc + 1) == '=') {
          tokens.push_back(StyioToken::Create(StyioTokenType::BINOP_NE, "!="));
          loc += 2;
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_EXCLAM, "!"));
          loc += 1;
        }
      } break;

      // 34
      case '\"': {
        std::string literal = "\"";
        const unsigned long long start = loc;
        loc += 1;

        while (loc < code.length() && code.at(loc) != '\"') {
          literal += code.at(loc);
          loc += 1;
        }
        if (loc >= code.length()) {
          throw StyioLexError(
            "Unterminated string literal at offset " + std::to_string(start));
        }

        literal += "\"";
        loc += 1;

        tokens.push_back(StyioToken::Create(StyioTokenType::STRING, literal));
      } break;

      // 35
      case '#': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_HASH, "#"));
        loc += 1;
      } break;

      // 36
      case '$': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_DOLLAR, "$"));
        loc += 1;
      } break;

      // 37
      case '%': {
        if (loc + 1 < code.size() && code.at(loc + 1) == '=') {
          tokens.push_back(StyioToken::Create(StyioTokenType::COMPOUND_MOD, "%="));
          loc += 2;
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_PERCENT, "%"));
          loc += 1;
        }
      } break;

      // 38
      case '&': {
        if (loc + 1 < code.size() && code.at(loc + 1) == '&') {
          tokens.push_back(StyioToken::Create(StyioTokenType::LOGIC_AND, "&&"));
          loc += 2;
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_AMP, "&"));
          loc += 1;
        }
      } break;

      // 39
      case '\'': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_SQUOTE, "\'"));
        loc += 1;
      } break;

      // 40
      case '(': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LPAREN, "("));
        loc += 1;
      } break;

      // 41
      case ')': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_RPAREN, ")"));
        loc += 1;
      } break;

      // 42
      case '*': {
        if (loc + 1 < code.size() && code.at(loc + 1) == '*') {
          tokens.push_back(StyioToken::Create(StyioTokenType::BINOP_POW, "**"));
          loc += 2;
        }
        else if (loc + 1 < code.size() && code.at(loc + 1) == '=') {
          tokens.push_back(StyioToken::Create(StyioTokenType::COMPOUND_MUL, "*="));
          loc += 2;
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_STAR, "*"));
          loc += 1;
        }
      } break;

      // 43
      case '+': {
        if (loc + 1 < code.size() && code.at(loc + 1) == '=') {
          tokens.push_back(StyioToken::Create(StyioTokenType::COMPOUND_ADD, "+="));
          loc += 2;
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_PLUS, "+"));
          loc += 1;
        }
      } break;

      // 44
      case ',': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_COMMA, ","));
        loc += 1;
      } break;

      // 45
      case '-': {
        /* -> ARROW_SINGLE_RIGHT */
        if (loc + 1 < code.size() && code.at(loc + 1) == '>') {
          tokens.push_back(StyioToken::Create(StyioTokenType::ARROW_SINGLE_RIGHT, "->"));
          loc += 2;
        }
        /* -- SINGLE_SEP_LINE */
        else if (loc + 1 < code.size() && code.at(loc + 1) == '-') {
          size_t count = 2 + count_consecutive(code, loc + 2, '-');
          tokens.push_back(StyioToken::Create(StyioTokenType::SINGLE_SEP_LINE, std::string(count, '-')));
          loc += count;
        }
        else if (loc + 1 < code.size() && code.at(loc + 1) == '=') {
          tokens.push_back(StyioToken::Create(StyioTokenType::COMPOUND_SUB, "-="));
          loc += 2;
        }
        else {
          /* - TOK_MINUS */
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_MINUS, "-"));
          loc += 1;
        }
      } break;

      // 46
      case '.': {
        size_t count = 1 + count_consecutive(code, loc + 1, '.');

        if (count == 1) {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_DOT, "."));
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::ELLIPSIS, std::string(count, '.')));
        }

        // anyway
        loc += count;
      } break;

      // 47
      case '/': {
        if (loc + 1 < code.size() && code.at(loc + 1) == '=') {
          tokens.push_back(StyioToken::Create(StyioTokenType::COMPOUND_DIV, "/="));
          loc += 2;
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_SLASH, "/"));
          loc += 1;
        }
      } break;

      // 58
      case ':': {
        if (loc + 1 < code.length() && code.at(loc + 1) == '=') {
          tokens.push_back(StyioToken::Create(StyioTokenType::WALRUS, ":="));
          loc += 2;
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_COLON, ":"));
          loc += 1;
        }

      } break;

      // 59
      case ';': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_SEMICOLON, ";"));
        loc += 1;
      } break;

      // 60
      case '<': {
        size_t count = 1 + count_consecutive(code, loc + 1, '<');

        if (count == 1) {
          if (loc + 1 < code.size() && code.at(loc + 1) == '~') {
            tokens.push_back(StyioToken::Create(StyioTokenType::WAVE_LEFT, "<~"));
            loc += 2;
          }
          else if (loc + 1 < code.size() && code.at(loc + 1) == '=') {
            tokens.push_back(StyioToken::Create(StyioTokenType::BINOP_LE, "<="));
            loc += 2;
          }
          else if (loc + 1 < code.size() && code.at(loc + 1) == '|') {
            tokens.push_back(StyioToken::Create(StyioTokenType::YIELD_PIPE, "<|"));
            loc += 2;
          }
          else if (loc + 1 < code.size() && code.at(loc + 1) == '-') {
            tokens.push_back(StyioToken::Create(StyioTokenType::ARROW_SINGLE_LEFT, "<-"));
            loc += 2;
          }
          else {
            tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LANGBRAC, "<"));
            loc += 1;
          }
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::EXTRACTOR, std::string(count, '<')));
          loc += count;
        }
      } break;

      // 61
      case '=': {
        if (loc + 1 < code.size() && code.at(loc + 1) == '>') {
          tokens.push_back(StyioToken::Create(StyioTokenType::ARROW_DOUBLE_RIGHT, "=>"));
          loc += 2;
        }
        else if (loc + 1 < code.size() && code.at(loc + 1) == '=') {
          size_t count = 2 + count_consecutive(code, loc + 2, '=');

          /* == BINOP_EQ */
          if (count == 2) {
            tokens.push_back(StyioToken::Create(StyioTokenType::BINOP_EQ, "=="));
          }
          /* === DOUBLE_SEP_LINE */
          else {
            tokens.push_back(StyioToken::Create(StyioTokenType::DOUBLE_SEP_LINE, std::string(count, '=')));
          }
          loc += count;
        }
        else {
          /* = TOK_EQUAL */
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_EQUAL, "="));
          loc += 1;
        }
      } break;

      // 62
      case '>': {
        // std::cout << ">" << std::endl;
        if (loc + 1 < code.size() && code.at(loc + 1) == '_') {
          // std::cout << ">_" << std::endl;
          tokens.push_back(StyioToken::Create(StyioTokenType::PRINT, ">_"));
          loc += 2;
        }
        else if (loc + 1 < code.size() && code.at(loc + 1) == '>') {
          // std::cout << "multi >" << std::endl;
          size_t count = 2 + count_consecutive(code, loc + 2, '>');
          tokens.push_back(StyioToken::Create(StyioTokenType::ITERATOR, std::string(count, '>')));
          loc += count;
        }
        else if (loc + 1 < code.size() && code.at(loc + 1) == '=') {
          tokens.push_back(StyioToken::Create(StyioTokenType::BINOP_GE, ">="));
          loc += 2;
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_RANGBRAC, ">"));
          loc += 1;
        }
      } break;

      // 63
      case '?': {
        if (loc + 1 < code.length() && code.at(loc + 1) == '=') {
          tokens.push_back(StyioToken::Create(StyioTokenType::MATCH, "?="));
          loc += 2;
        }
        else if (loc + 1 < code.length() && code.at(loc + 1) == '?') {
          tokens.push_back(StyioToken::Create(StyioTokenType::DBQUESTION, "??"));
          loc += 2;
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_QUEST, "?"));
          loc += 1;
        }

      } break;

      // 64
      case '@': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_AT, "@"));
        loc += 1;
      } break;

      // 91
      case '[': {
        if (loc + 1 < code.size() && code.at(loc + 1) == '|') {
          tokens.push_back(StyioToken::Create(StyioTokenType::BOUNDED_BUFFER_OPEN, "[|"));
          loc += 2;
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LBOXBRAC, "["));
          loc += 1;
        }
      } break;

      // 92
      case '\\': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_BACKSLASH, "\\"));
        loc += 1;
      } break;

      // 93
      case ']': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_RBOXBRAC, "]"));
        loc += 1;
      } break;

      // 94
      case '^': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_HAT, "^"));
        loc += 1;
      } break;

      // 95
      case '_': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_UNDLINE, "_"));
        loc += 1;
      } break;

      // 96
      case '`': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_BQUOTE, "`"));
        loc += 1;
      } break;

      // 123
      case '{': {
        const bool is_native_extern_body =
          styio_recent_tokens_open_native_extern_body(tokens);
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LCURBRAC, "{"));
        loc += 1;
        if (is_native_extern_body) {
          const size_t body_start = loc;
          const size_t body_end = styio_scan_native_extern_body_end(code, body_start);
          tokens.push_back(StyioToken::Create(
            StyioTokenType::NATIVE_EXTERN_BODY,
            code.substr(body_start, body_end - body_start)));
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_RCURBRAC, "}"));
          loc = body_end + 1;
        }
      } break;

      // 124
      case '|': {
        if (loc + 2 < code.size() && code.at(loc + 1) == '<' && code.at(loc + 2) == '|') {
          tokens.push_back(StyioToken::Create(StyioTokenType::RETURN_PIPE, "|<|"));
          loc += 3;
        }
        else if (loc + 1 < code.size() && code.at(loc + 1) == ';') {
          tokens.push_back(StyioToken::Create(StyioTokenType::PIPE_SEMICOLON, "|;"));
          loc += 2;
        }
        else if (loc + 1 < code.size() && code.at(loc + 1) == ']') {
          tokens.push_back(StyioToken::Create(StyioTokenType::BOUNDED_BUFFER_CLOSE, "|]"));
          loc += 2;
        }
        else if (loc + 1 < code.size() && code.at(loc + 1) == '|') {
          tokens.push_back(StyioToken::Create(StyioTokenType::LOGIC_OR, "||"));
          loc += 2;
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_PIPE, "|"));
          loc += 1;
        }
      } break;

      // 125
      case '}': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_RCURBRAC, "}"));
        loc += 1;
      } break;

      // 126
      case '~': {
        if (loc + 1 < code.size() && code.at(loc + 1) == '>') {
          tokens.push_back(StyioToken::Create(StyioTokenType::WAVE_RIGHT, "~>"));
          loc += 2;
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_TILDE, "~"));
          loc += 1;
        }
      } break;

      default: {
        /* Single-byte not recognized above (e.g. embedded NUL): must advance. */
        tokens.push_back(
          StyioToken::Create(StyioTokenType::UNKNOWN, std::string(1, code.at(loc))));
        loc += 1;
      } break;
    }
  }

  tokens.push_back(StyioToken::Create(StyioTokenType::TOK_EOF, ""));
  return tokens;
}
