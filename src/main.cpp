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

    do 
    {
      thisChar = getchar();

      // ignore white spaces and read next character
      while (isspace(thisChar))
      {
        thisChar = getchar();
      }

      switch (thisChar)
      {
        case '(':
          return TokenType::TOK_LPAREN;
          break;
        case '@':
          do
          {
            thisChar = getchar();
          } while (isspace(thisChar));

          // VAR_DEF := "@" "(" [<ID> ["," <ID>]*]? ")"
          if (thisChar == '(') {
            return TokenType::TOK_VARDEF;
          }

          break;

        default:
          break;
      }

    } while (thisChar != '\n');

    return TokenType::TOK_UNKOWN;
}

int main() {
  while (1) 
  {
    fprintf(stderr, "</styio/> ");

    int thisToken = matchToken();

    std::cout << "[INFO]: " << thisToken << std::endl;
  };

  return 0;
}