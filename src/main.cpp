#include <cstdio>
#include <cstdlib>
#include <string>
#include <map>
#include <vector>
#include <iostream>

enum TokenType {
  TOK_UNKOWN,
  
  TOK_EOF, // end-of-line

  TOK_SPACE, // space

  TOK_ID,

  TOK_INT,
  TOK_FLOAT,

  TOK_DOT, // .
  TOK_COMMA, // ,
  TOK_COLON, // :
  TOK_SEMICOLON, // ;

  TOK_VARDEF, // @
  
  TOK_ELLIPSIS, // ...

  TOK_EXCLAM, // !

  TOK_NEG, // -

  TOK_ADD, // +
  TOK_SUB, // -
  TOK_MUL, // *
  TOK_DIV, // /

  TOK_LPAREN, // (
  TOK_RPAREN, // )

  TOK_LCURBRAC, // {
  TOK_RCURBRAC, // }

  TOK_LBOXBRAC, // [
  TOK_RBOXBRAC, // ]

  TOK_LANGBRAC, // <
  TOK_RANGBRAC, // >
};

// ?(a > 1) [true % false]

static int matchToken() {
  static int thisChar = ' ';

  thisChar = getchar();

  // ignore white spaces and read next character
  while (isspace(thisChar))
  {
    thisChar = getchar();
  }

  switch (thisChar)
  {
    case EOF:
      return TokenType::TOK_EOF;

    case '(':
      return TokenType::TOK_LPAREN;

    case '@':
      do
      {
        thisChar = getchar();
      } while (isspace(thisChar));

      // VAR_DEF := "@" "(" [<ID> ["," <ID>]*]? ")"
      if (thisChar == '(') {
        return TokenType::TOK_VARDEF;
      } else {
        return TokenType::TOK_UNKOWN;
      }

    default:
      std::cout << "[INFO]: unexpected char " << char(thisChar) << std::endl;
      break;
  }

  return TokenType::TOK_UNKOWN;
}

int main() {
  while (1) 
  {
    fprintf(stderr, "</styio/> ");

    int thisToken = matchToken();

    std::cout << "[INFO]: Found " << thisToken << std::endl;
  };

  return 0;
}