// [C++ STL]
#include <algorithm>
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
#include "../StyioUtil/Util.hpp"
#include "Tokenizer.hpp"

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

  while (loc < code.length() - 1) {
    /* Spaces and Comments */
    switch (code.at(loc)) {
      case ' ': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_SPACE, " "));
        loc += 1;
      } break;

      /* LF */
      case '\n': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LF, "\n"));
        loc += 1;
      } break;

      /* CR */
      case '\r': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_CR, "\r"));
        loc += 1;
      } break;

      default: {
      } break;
    }

    // commments
    if (code.compare(loc, 2, "//") == 0) {
      std::string literal = "//";
      loc += 2;

      while (code.at(loc) != '\n'
             && code.at(loc) != '\r'
             && code.at(loc) != EOF) {
        literal += code.at(loc);
        loc += 1;
      }

      tokens.push_back(StyioToken::Create(StyioTokenType::COMMENT_LINE, literal));
    }
    /* comments */
    else if (code.compare(loc, 2, "/*") == 0) {
      std::string literal = "/*";
      loc += 2;

      while (not(code.compare(loc, 2, "*/") == 0)) {
        literal += code.at(loc);
        loc += 1;
      }

      literal += "*/";
      loc += 2;

      tokens.push_back(StyioToken::Create(StyioTokenType::COMMENT_CLOSED, literal));
    }

    /* varname / typename */
    if (isalpha(code.at(loc)) || (code.at(loc) == '_')) {
      std::string literal;

      do {
        literal += code.at(loc);
        loc += 1;
      } while (isalnum(code.at(loc)));

      tokens.push_back(StyioToken::Create(StyioTokenType::NAME, literal));
    }
    /* integer / float / decimal */
    else if (isdigit(code.at(loc))) {
      std::string literal;

      do {
        literal += code.at(loc);
        loc += 1;
      } while (isdigit(code.at(loc)));

      /* If Float: xxx.yyy */
      if (code.at(loc) == '.' && isdigit(code.at(loc + 1))) {
        /* include '.' */
        literal += code.at(loc);
        loc += 1;

        /* include yyy */
        do {
          literal += code.at(loc);
          loc += 1;
        } while (isdigit(code.at(loc)));

        tokens.push_back(StyioToken::Create(StyioTokenType::DECIMAL, literal));
      }
      else {
        tokens.push_back(StyioToken::Create(StyioTokenType::INTEGER, literal));
      }
    }

    switch (code.at(loc)) {
      // -1
      case EOF: {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_EOF, std::to_string(EOF)));
        return tokens;
      } break;

      // 33
      case '!': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_EXCLAM, "!"));
        loc += 1;
      } break;

      // 34
      case '\"': {
        std::string literal = "\"";
        loc += 1;

        while (code.at(loc) != '\"') {
          literal += code.at(loc);
          loc += 1;
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
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_PERCENT, "%"));
        loc += 1;
      } break;

      // 38
      case '&': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_AMP, "&"));
        loc += 1;
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
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_STAR, "*"));
        loc += 1;
      } break;

      // 43
      case '+': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_PLUS, "+"));
        loc += 1;
      } break;

      // 44
      case ',': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_COMMA, ","));
        loc += 1;
      } break;

      // 45
      case '-': {
        size_t count = 1 + count_consecutive(code, loc + 1, '-');

        if (count == 1) {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_MINUS, "-"));
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::SINGLE_SEP_LINE, std::string(count, '-')));
        }

        // anyway
        loc += count;
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
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_SLASH, "/"));
        loc += 1;
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
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LANGBRAC, "<"));
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::EXTRACTOR, std::string(count, '<')));
        }

        // anyway
        loc += count;
      } break;

      // 61
      case '=': {
        size_t count = 1 + count_consecutive(code, loc + 1, '=');

        /* = TOK_EQUAL */
        if (count == 1) {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_EQUAL, "="));
        }
        /* == BINOP_EQ */
        else if (count == 2) {
          tokens.push_back(StyioToken::Create(StyioTokenType::BINOP_EQ, "=="));
        }
        /* === DOUBLE_SEP_LINE */
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::DOUBLE_SEP_LINE, std::string(count, '=')));
        }

        // anyway
        loc += count;
      } break;

      // 62
      case '>': {
        // std::cout << ">" << std::endl;
        if (loc + 1 < code.size() - 1 && code.at(loc + 1) == '_') {
          // std::cout << ">_" << std::endl;
          tokens.push_back(StyioToken::Create(StyioTokenType::PRINT, ">_"));
          loc += 2;
        }
        else if (loc + 1 < code.size() - 1 && code.at(loc + 1) == '>') {
          // std::cout << "multi >" << std::endl;
          size_t count = 2 + count_consecutive(code, loc + 2, '>');
          tokens.push_back(StyioToken::Create(StyioTokenType::ITERATOR, std::string(count, '>')));
          loc += count;
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
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LBOXBRAC, "["));
        loc += 1;
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
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LCURBRAC, "{"));
        loc += 1;
      } break;

      // 124
      case '|': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_PIPE, "|"));
        loc += 1;
      } break;

      // 125
      case '}': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_RCURBRAC, "}"));
        loc += 1;
      } break;

      // 126
      case '~': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_TILDE, "~"));
        loc += 1;
      } break;

      default:
        break;
    }
  }

  return tokens;
}