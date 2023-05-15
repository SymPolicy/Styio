#include <cstdio>
#include <cstdlib>
#include <string>
#include <map>
#include <vector>
#include <iostream>

enum TokenType {
  TOK_UNKNOWN,

  TOK_SPACE,
  
  TOK_EOL, // \n
  TOK_EOF, // EOF

  TOK_ID,
  TOK_INT,
  TOK_FLOAT,

  TOK_DOT, // .
  TOK_COMMA, // ,
  TOK_COLON, // :
  TOK_SEMICOLON, // ;

  TOK_TILDE, // ~
  TOK_EXCLAM, // !
  TOK_AT, // @
  TOK_HASH, // #
  TOK_DOLLAR, // $
  TOK_PERCENT, // %
  TOK_HAT, // ^
  TOK_CHECK, // ?
  TOK_SLASH, // /
  TOK_BSLASH, // Back Slash
  TOK_PIPE, // |
  
  TOK_ELLIPSIS, // ...

  TOK_SQUOTE, // '
  TOK_DQUOTE, // "
  TOK_BQUOTE, // `

  TOK_LPAREN, // (
  TOK_RPAREN, // )

  TOK_LCURBRAC, // {
  TOK_RCURBRAC, // }

  TOK_LBOXBRAC, // [
  TOK_RBOXBRAC, // ]

  TOK_LANGBRAC, // <
  TOK_RANGBRAC, // >

  TOK_AND, // &&
  TOK_OR, // ||
  TOK_XOR, // ^

  TOK_BITAND, // &
  TOK_BITOR, // |
  TOK_BITXOR, // ^

  TOK_LSHIFT, // <<
  TOK_RSHIFT, // >>

  TOK_NOT, // !

  TOK_NEG, // -

  TOK_ADD, // +
  TOK_SUB, // -
  TOK_MUL, // *
  TOK_DIV, // /

  TOK_GT, // >
  TOK_GE, // >=
  TOK_LT, // <
  TOK_LE, // <=
  TOK_EQ, // ==
  TOK_NE, // !=

  TOK_MATCH, // ?=

  TOK_INFINITE_LIST, // [...]
};

// ?(a > 1) [true % false]

static std::vector<int> Tokenize() {
  std::vector<int> tokenBuffer;

  static int thisChar = ' ';

  thisChar = getchar();

  // ignore white spaces and read next character
  while (isspace(thisChar))
  {
    thisChar = getchar();
  }

  if (isalpha(thisChar) || thisChar == '_') {
    std::string IdStr = "";
    IdStr += thisChar;

    // [a-zA-Z][a-zA-Z0-9_]*
    while (isalnum((thisChar = getchar())) || thisChar == '_') 
    {
      IdStr += thisChar;
    }

    std::cout << "[INFO]: ID `" << IdStr << "`" << std::endl;

    tokenBuffer.push_back(
      TokenType::TOK_ID
    );
  }

  if (isdigit(thisChar)) {
    std::string numStr = "";
    numStr += thisChar;
    thisChar = getchar();

    // [0-9]*
      while (isdigit(thisChar))
      {
        numStr += thisChar;
        thisChar = getchar();
      };

    if (thisChar == '.') {
      numStr += thisChar;
      thisChar = getchar();

      while (isdigit(thisChar))
      {
        numStr += thisChar;
        thisChar = getchar();
      };

      if (!isspace(thisChar)) {
        std::cout << "[Error]: Float `" << numStr << "` ends with unexpected char `" << char(thisChar) << "`" << std::endl;
      }

      std::cout << "[INFO]: Float `" << numStr << "`" << std::endl;

      tokenBuffer.push_back(
        TokenType::TOK_FLOAT
      );
    } 
    else 
    {
      if (!isspace(thisChar)) {
        std::cout << "[Error]: Integer `" << numStr << "` ends with unexpected char `" << char(thisChar) << "`" << std::endl;
      }

      std::cout << "[INFO]: Integer `" << numStr << "`" << std::endl;

      tokenBuffer.push_back(
        TokenType::TOK_INT
      );
    }
  }

  switch (thisChar)
  {
    case EOF:
      tokenBuffer.push_back(
        TokenType::TOK_EOF
      );
      break;

    case ',':
      tokenBuffer.push_back(
        TokenType::TOK_COMMA
      );
      break;
      
    case '.':
      tokenBuffer.push_back(
        TokenType::TOK_DOT
      );
      break;

    case ';':
      tokenBuffer.push_back(
        TokenType::TOK_SEMICOLON
      );
      break;

    case ':':
      tokenBuffer.push_back(
        TokenType::TOK_COLON
      );
      break;

    case '!':
      tokenBuffer.push_back(
        TokenType::TOK_EXCLAM
      );
      break;

    case '(':
      tokenBuffer.push_back(
        TokenType::TOK_LPAREN
      );
      break;

    case ')':
      tokenBuffer.push_back(
        TokenType::TOK_RPAREN
      );
      break;

    case '[':
      tokenBuffer.push_back(
        TokenType::TOK_LBOXBRAC
      );
      break;

    case ']':
      tokenBuffer.push_back(
        TokenType::TOK_RBOXBRAC
      );
      break;

    case '{':
      tokenBuffer.push_back(
        TokenType::TOK_LCURBRAC
      );
      break;

    case '}':
      tokenBuffer.push_back(
        TokenType::TOK_RCURBRAC
      );
      break;

    case '@':
      thisChar = getchar();

      if (thisChar != '\n' || thisChar != EOF) {
        while (isspace(thisChar))
        {
          thisChar = getchar();
        };
      }

      // VAR_DEF := "@" "(" [<ID> ["," <ID>]*]? ")"
      if (thisChar == '(') {
        tokenBuffer.push_back(
          TokenType::TOK_AT
        );
        break;
      }

    default:
      break;
  }

  return tokenBuffer;
}

int main() {

  while (1) 
  {
    fprintf(stderr, "</styio/> ");

    for (int tok: Tokenize()) {
      std::cout << tok << ' ';
    };

    std::cout << std::endl;

    // std::cout << "[INFO]: Token <" << thisToken << ">" << std::endl;
  };

  return 0;
}