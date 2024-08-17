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

std::vector<StyioToken *>
StyioTokenizer::tokenize(std::string code) {
  std::vector<StyioToken *> tokens;
  unsigned long long loc = 0; /* local position */

  while (loc < code.length() - 1) {
    /* Spaces and Comments */
    switch (code.at(loc)) {
      case ' ': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_SPACE, ""));
        loc += 1;
      } break;

      /* LF */
      case '\n': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LF, ""));
        loc += 1;
      } break;

      /* CR */
      case '\r': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_CR, ""));
        loc += 1;
      } break;

      default: {
      } break;
    }

    // commments
    if (code.compare(loc, 2, "//") == 0) {
      std::string literal;
      loc += 2;

      while (code.at(loc) != '\n'
             && code.at(loc) != '\r'
             && code.at(loc) != EOF) {
        literal += code.at(loc);
        loc += 1;
      }

      tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LINE_COMMENT, literal));
    }
    /* comments */
    else if (code.compare(loc, 2, "/*") == 0) {
      std::string literal;
      loc += 2;

      while (not (code.compare(loc, 2, "*/") == 0)) {
        literal += code.at(loc);
        loc += 1;
      }

      loc += 2;

      tokens.push_back(StyioToken::Create(StyioTokenType::TOK_CLOSED_COMMENT, literal));
    }

    /* varname / typename */
    if (isalpha(code.at(loc)) || (code.at(loc) == '_')) {
      std::string literal;

      do {
        literal += code.at(loc);
        loc += 1;
      } while (isalnum(code.at(loc)));

      tokens.push_back(StyioToken::Create(StyioTokenType::TOK_NAME, literal));
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

        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_INT, literal));
      }
      else {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_FLOAT, literal));
      }
    }

    switch (code.at(loc)) {
      // -1
      case EOF: {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_EOF, ""));
        return tokens;
      } break;

      // 33
      case '!': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_EXCLAM, ""));
        loc += 1;
      } break;

      // 34
      case '\"': {
        loc += 1;

        std::string literal;
        while (code.at(loc) != '\"') {
          literal += code.at(loc);
          loc += 1;
        }

        loc += 1;

        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_STRING, literal));
      } break;

      // 35
      case '#': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_HASH, ""));
        loc += 1;
      } break;

      // 36
      case '$': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_DOLLAR, ""));
        loc += 1;
      } break;

      // 37
      case '%': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_PERCENT, ""));
        loc += 1;
      } break;

      // 38
      case '&': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_AMP, ""));
        loc += 1;
      } break;

      // 39
      case '\'': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_SQUOTE, ""));
        loc += 1;
      } break;

      // 40
      case '(': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LPAREN, ""));
        loc += 1;
      } break;

      // 41
      case ')': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_RPAREN, ""));
        loc += 1;
      } break;

      // 42
      case '*': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_STAR, ""));
        loc += 1;
      } break;

      // 43
      case '+': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_PLUS, ""));
        loc += 1;
      } break;

      // 44
      case ',': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_COMMA, ""));
        loc += 1;
      } break;

      // 45
      case '-': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_MINUS, ""));
        loc += 1;
      } break;

      // 46
      case '.': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_DOT, ""));
        loc += 1;
      } break;

      // 47
      case '/': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_SLASH, ""));
        loc += 1;
      } break;

      // 58
      case ':': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_COLON, ""));
        loc += 1;
      } break;

      // 59
      case ';': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_SEMICOLON, ""));
        loc += 1;
      } break;

      // 60
      case '<': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LANGBRAC, ""));
        loc += 1;
      } break;

      // 61
      case '=': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_EQUAL, ""));
        loc += 1;
      } break;

      // 62
      case '>': {
        loc += 1;

        if (code.at(loc) == '>') {
          loc += 1;
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_FORWARD));
        }
        if (code.at(loc) == '_') {
          loc += 1;
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_TERMINAL));
        }
        else {
          tokens.push_back(StyioToken::Create(StyioTokenType::TOK_RANGBRAC));
        }

      } break;

      // 63
      case '?': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_QUESTION, ""));
        loc += 1;
      } break;

      // 64
      case '@': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_AT, ""));
        loc += 1;
      } break;

      // 91
      case '[': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LBOXBRAC, ""));
        loc += 1;
      } break;

      // 92
      case '\\': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_BACKSLASH, ""));
        loc += 1;
      } break;

      // 93
      case ']': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_RBOXBRAC, ""));
        loc += 1;
      } break;

      // 94
      case '^': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_HAT, ""));
        loc += 1;
      } break;

      // 95
      case '_': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_UNDLINE, ""));
        loc += 1;
      } break;

      // 96
      case '`': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_BQUOTE, ""));
        loc += 1;
      } break;

      // 123
      case '{': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_LCURBRAC, ""));
        loc += 1;
      } break;

      // 124
      case '|': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_PIPE, ""));
        loc += 1;
      } break;

      // 125
      case '}': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_RCURBRAC, ""));
        loc += 1;
      } break;

      // 126
      case '~': {
        tokens.push_back(StyioToken::Create(StyioTokenType::TOK_TILDE, ""));
        loc += 1;
      } break;

      default:
        break;
    }
  }

  return tokens;
}