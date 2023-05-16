#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

class StyioBaseException : public std::exception
{
  private:
    std::string message;

  public:
    StyioBaseException() : message("|Styio.BaseException|"){}

    StyioBaseException(std::string msg) : message("|Styio.BaseException| " + msg) {}

    ~StyioBaseException() throw () {}

    virtual const char* what() const throw () {
      return message.c_str();
    }
};

class StyioSyntaxError : public StyioBaseException 
{
  private:
    std::string message;

  public:
    StyioSyntaxError() : message("|Styio.SyntaxError|"){}

    StyioSyntaxError(std::string msg) : message("|Styio.SyntaxError| " + msg) {}

    ~StyioSyntaxError() throw () {}

    virtual const char* what() const throw () {
      return message.c_str();
    }
};

enum TokenType {
  TOK_EOF = -1, // EOF
  TOK_NULL = 0, // ASCII 0 NUL
  TOK_LF = 10, // ASCII 10 LF
  TOK_CR = 13, // ASCII 13 CR
  TOK_SPACE = 32, // ASCII 32 SPACE
  TOK_EXCLAM = 33, // ASCII 33 !
  TOK_DQUOTE = 34, // ASCII 24 "
  TOK_HASH = 35, // ASCII 35 #
  TOK_DOLLAR = 36, // ASCII 36 $
  TOK_PERCENT = 37, // ASCII 37 %
  TOK_AMP = 38, // ASCII 38 &
  TOK_SQUOTE = 39, // ASCII 39 '
  TOK_LPAREN = 40, // ASCII 40 (
  TOK_RPAREN = 41, // ASCII 41 )
  TOK_STAR = 42, // ASCII 42 *
  TOK_PLUS = 43, // ASCII 43 +
  TOK_COMMA = 44, // ASCII 44 ,
  TOK_MINUS = 45, // ASCII 45 -
  TOK_DOT = 46, // ASCII 46 .
  TOK_SLASH = 47, // ASCII 47 / (slash)
  TOK_COLON = 58, // ASCII 58 :
  TOK_SEMICOLON = 59, // ASCII 59 ;
  TOK_LANGBRAC = 60, // ASCII 60 <
  TOK_RANGBRAC = 62, // ASCII 62 >
  TOK_CHECK = 63, // ASCII 63 ?
  TOK_AT = 64, // ASCII 64 @
  TOK_LBOXBRAC = 91, // [
  TOK_BSLASH = 92, // ASCII 92 \ (backslash)
  TOK_RBOXBRAC = 93, // ]
  TOK_HAT = 94, // ASCII 94 ^
  TOK_UNDLINE = 95, // ASCII 95 _
  TOK_BQUOTE = 96, // ASCII 96 `
  TOK_LCURBRAC = 123, // ASCII 123 {
  TOK_PIPE = 124, // ASCII 124 |
  TOK_RCURBRAC = 125, // ASCII 125 }
  TOK_TILDE = 126, // ASCII 126 ~
  TOK_DEL = 127, // ASCII 127 DEL

  TOK_ID,
  TOK_INT,
  TOK_FLOAT,

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
  TOK_MOD, // %
  TOK_POW, // **

  TOK_GT, // >
  TOK_GE, // >=
  TOK_LT, // <
  TOK_LE, // <=
  TOK_EQ, // ==
  TOK_NE, // !=

  TOK_WALRUS, // :=

  TOK_MATCH, // ?=

  TOK_ELLIPSIS, // ...

  TOK_INFINITE_LIST, // [...]
};

// ?(a > 1) [true % false]

static std::vector<int> inputBuffer;

static int readInputChar() 
{
  int tmpChar = getchar();

  inputBuffer.push_back(tmpChar);

  return tmpChar;
}

static void ignoreBlank(int& nextChar) 
{
  while (isspace(nextChar)) {
    nextChar = readInputChar();
  };
}


static void ignoreSpaces(int& nextChar) 
{
  while (nextChar == ' ') {
    nextChar = readInputChar();
  };
}

static void parseSpace(std::vector<int>& tokenBuffer, int& nextChar) 
{
  while (nextChar == ' ') {
    nextChar = readInputChar();
    tokenBuffer.push_back(
      TokenType::TOK_SPACE
    );
  };
}

static void parseLF(std::vector<int>& tokenBuffer, int& nextChar) 
{
  // nextChar = readInputChar();
  tokenBuffer.push_back(
    TokenType::TOK_LF
  );
}

static void parseCR(std::vector<int>& tokenBuffer, int& nextChar) 
{
  // nextChar = readInputChar();
  tokenBuffer.push_back(
    TokenType::TOK_CR
  );
}

static void parseEOF(std::vector<int>& tokenBuffer, int& nextChar) 
{
  // nextChar = readInputChar();
  tokenBuffer.push_back(
    TokenType::TOK_EOF
  );
}

static void parseId(std::vector<int>& tokenBuffer, int& nextChar) 
{
  std::string idStr = "";
  idStr += nextChar;

  // [a-zA-Z][a-zA-Z0-9_]*
  while (isalnum((nextChar = readInputChar())) || nextChar == '_') 
  {
    idStr += nextChar;
  }

  std::cout << "|Info| ID `" << idStr << "`" << std::endl;

  tokenBuffer.push_back(
    TokenType::TOK_ID
  );
}

static void parseNum(std::vector<int>& tokenBuffer, int& nextChar) 
{
  std::string numStr = "";
  numStr += nextChar;
  nextChar = readInputChar();

  // [0-9]*
  while (isdigit(nextChar))
  {
    numStr += nextChar;
    nextChar = readInputChar();
  };

  if (nextChar == '.') 
  {
    numStr += nextChar;
    nextChar = readInputChar();

    while (isdigit(nextChar))
    {
      numStr += nextChar;
      nextChar = readInputChar();
    };

    if (!isspace(nextChar)) 
    {
      std::string errmsg = "Float `" + numStr + "` ends with unexpected char `" + char(nextChar) + "`";
      throw StyioSyntaxError(errmsg);
    } else {
      std::cout << "|Info| Float `" << numStr << "`" << std::endl;
    }

    tokenBuffer.push_back(
      TokenType::TOK_FLOAT
    );

    return;
  } 
  else 
  {
    if (!isspace(nextChar)) 
    {
      std::string errmsg = "Integer `" + numStr + "` ends with unexpected char `" + char(nextChar) + "`";
      throw StyioSyntaxError(errmsg);
    } else {
      std::cout << "|Info| Integer `" << numStr << "`" << std::endl;
    }

    tokenBuffer.push_back(
      TokenType::TOK_INT
    );

    return;
  }
}

static void parseBinAdd(std::vector<int>& tokenBuffer, int& nextChar) 
{
  nextChar = readInputChar();

  tokenBuffer.push_back(
    TokenType::TOK_ADD
  );

  ignoreSpaces(nextChar);

  if (isalpha(nextChar) || nextChar == '_') {
    parseId(tokenBuffer, nextChar);
  }

  if (isdigit(nextChar)) {
    parseNum(tokenBuffer, nextChar);
  }
  
  // std::cout << "|NotImplemented| BIN_ADD" << std::endl;
}

static void parseBinSub(std::vector<int>& tokenBuffer, int& nextChar) 
{
  nextChar = readInputChar();

  tokenBuffer.push_back(
    TokenType::TOK_SUB
  );

  ignoreSpaces(nextChar);

  if (isalpha(nextChar) || nextChar == '_') {
    parseId(tokenBuffer, nextChar);
  }

  if (isdigit(nextChar)) {
    parseNum(tokenBuffer, nextChar);
  }
  
  // std::cout << "|NotImplemented| BIN_SUB" << std::endl;
}

static void parseBinMul(std::vector<int>& tokenBuffer, int& nextChar) 
{
  nextChar = readInputChar();
  
  tokenBuffer.push_back(
    TokenType::TOK_MUL
  );

  ignoreSpaces(nextChar);

  if (isalpha(nextChar) || nextChar == '_') {
    parseId(tokenBuffer, nextChar);
  }

  if (isdigit(nextChar)) {
    parseNum(tokenBuffer, nextChar);
  }

  // std::cout << "|NotImplemented| BIN_MUL" << std::endl;
}

static void parseBinDiv(std::vector<int>& tokenBuffer, int& nextChar) 
{
  nextChar = readInputChar();
  
  tokenBuffer.push_back(
    TokenType::TOK_DIV
  );

  ignoreSpaces(nextChar);

  if (isalpha(nextChar) || nextChar == '_') {
    parseId(tokenBuffer, nextChar);
  }

  if (isdigit(nextChar)) {
    parseNum(tokenBuffer, nextChar);
  }

  // std::cout << "|NotImplemented| BIN_DIV" << std::endl;
}

static void parseBinPow(std::vector<int>& tokenBuffer, int& nextChar) 
{
  nextChar = readInputChar();
  
  tokenBuffer.push_back(
    TokenType::TOK_POW
  );

  ignoreSpaces(nextChar);

  if (isalpha(nextChar) || nextChar == '_') {
    parseId(tokenBuffer, nextChar);
  }

  if (isdigit(nextChar)) {
    parseNum(tokenBuffer, nextChar);
  }

  // std::cout << "|NotImplemented| BIN_POW" << std::endl;
}

static void parseBinMod(std::vector<int>& tokenBuffer, int& nextChar) 
{
  nextChar = readInputChar();

  tokenBuffer.push_back(
    TokenType::TOK_MOD
  );

  ignoreSpaces(nextChar);

  if (isalpha(nextChar) || nextChar == '_') {
    parseId(tokenBuffer, nextChar);
  }

  if (isdigit(nextChar)) {
    parseNum(tokenBuffer, nextChar);
  }

  // std::cout << "|NotImplemented| BIN_MOD" << std::endl;
}

static std::vector<int> Tokenize() {
  std::vector<int> tokenBuffer;

  static int thisChar = ' ';

  do
  {
    ignoreBlank(thisChar);

    if (isalpha(thisChar) || thisChar == '_') 
    {
      // parse id
      parseId(tokenBuffer, thisChar);
      
      // ignore white spaces after id
      ignoreSpaces(thisChar);

      // check next character
      switch (thisChar)
      {
        // <LF>
        case '\n':
          parseLF(tokenBuffer, thisChar);

          return tokenBuffer;
        
        // <ID> := <EXPR>
        case ':':
          thisChar = readInputChar();

          if (thisChar == '=')
          {
            std::cout << "|NotImplemented| VAR_ASSIGN" << std::endl;
          }
          break;
        // BIN_ADD := <ID> "+" <ID>
        case '+':
          parseBinAdd(tokenBuffer, thisChar);
          break;
        // BIN_SUB := <ID> "-" <ID>
        case '-':
          parseBinSub(tokenBuffer, thisChar);
          break;
        // BIN_MUL | BIN_POW
        case '*':
          thisChar = readInputChar();
          // BIN_POW := <ID> "**" <ID>
          if (thisChar == '*')
          {
            parseBinPow(tokenBuffer, thisChar);
          } 
          // BIN_MUL := <ID> "*" <ID>
          else 
          {
            parseBinMul(tokenBuffer, thisChar);
          }
          break;
        // BIN_MUL := <ID> "/" <ID>
        case '/':
          parseBinDiv(tokenBuffer, thisChar);
          break;
        // BIN_MUL := <ID> "%" <ID>
        case '%':
          parseBinMod(tokenBuffer, thisChar);
          break;
        
        default:
          std::cout << "|Info| <ID> followed by unhandled char " << thisChar << std::endl;
          break;
      }
    }

    if (isdigit(thisChar)) {
      parseNum(tokenBuffer, thisChar);

      ignoreSpaces(thisChar);

      switch (thisChar)
      {
        // <LF>
        case '\n':
          parseLF(tokenBuffer, thisChar);
          return tokenBuffer;
        // BIN_ADD := [<Integer>|<Float>] "+" [<Integer>|<Float>]
        case '+':
          parseBinAdd(tokenBuffer, thisChar);
          break;
        // BIN_SUB := [<Integer>|<Float>] "-" [<Integer>|<Float>]
        case '-':
          parseBinSub(tokenBuffer, thisChar);
          break;
        // BIN_MUL | BIN_POW
        case '*':
          thisChar = readInputChar();
          // BIN_POW := [<Integer>|<Float>] "**" [<Integer>|<Float>]
          if (thisChar == '*')
          {
            parseBinPow(tokenBuffer, thisChar);
          } 
          // BIN_MUL := [<Integer>|<Float>] "*" [<Integer>|<Float>]
          else 
          {
            parseBinMul(tokenBuffer, thisChar);
          }
          break;
        // BIN_DIV := [<Integer>|<Float>] "/" [<Integer>|<Float>]
        case '/':
          parseBinDiv(tokenBuffer, thisChar);
          break;
        // BIN_MOD := [<Integer>|<Float>] "%" [<Integer>|<Float>]
        case '%':
          parseBinMod(tokenBuffer, thisChar);
          break;

        default:
          std::cout << "|Info| <Integer> followed by unhandled char " << thisChar << std::endl;
          break;
      }
    }

    switch (thisChar)
    {
      case '\n':
        parseLF(tokenBuffer, thisChar);
        break;

      case '\r':
        parseCR(tokenBuffer, thisChar);
        break;

      case EOF:
        parseEOF(tokenBuffer, thisChar);
        break;

      case '!':
        tokenBuffer.push_back(
          TokenType::TOK_EXCLAM
        );
        thisChar = readInputChar();
        break;

      case '@':
        tokenBuffer.push_back(
          TokenType::TOK_AT
        );
        thisChar = readInputChar();
        break;

      case ',':
        tokenBuffer.push_back(
          TokenType::TOK_COMMA
        );
        thisChar = readInputChar();
        break;

      case '.':
        tokenBuffer.push_back(
          TokenType::TOK_DOT
        );
        thisChar = readInputChar();
        break;

      case ';':
        tokenBuffer.push_back(
          TokenType::TOK_SEMICOLON
        );
        thisChar = readInputChar();
        break;

      case ':':
        tokenBuffer.push_back(
          TokenType::TOK_COLON
        );
        thisChar = readInputChar();
        break;

      case '(':
        tokenBuffer.push_back(
          TokenType::TOK_LPAREN
        );
        thisChar = readInputChar();
        break;

      case ')':
        tokenBuffer.push_back(
          TokenType::TOK_RPAREN
        );
        thisChar = readInputChar();
        break;

      case '[':
        tokenBuffer.push_back(
          TokenType::TOK_LBOXBRAC
        );
        thisChar = readInputChar();
        break;

      case ']':
        tokenBuffer.push_back(
          TokenType::TOK_RBOXBRAC
        );
        thisChar = readInputChar();
        break;

      case '{':
        tokenBuffer.push_back(
          TokenType::TOK_LCURBRAC
        );
        thisChar = readInputChar();
        break;

      case '}':
        tokenBuffer.push_back(
          TokenType::TOK_RCURBRAC
        );
        thisChar = readInputChar();
        break;

      default:
        std::cout << "|Unknown| " << thisChar << std::endl;
        thisChar = readInputChar();
        break;
    }

    std::cout << "|Cursor| " << thisChar << std::endl;

  } while (thisChar != '\n' && thisChar != EOF);

  return tokenBuffer;
}

std::string tokenRepr(int token) {
  switch (token)
  {
    case TokenType::TOK_SPACE:
      return " ";

    case TokenType::TOK_CR:
      return "<CR>";

    case TokenType::TOK_LF:
      return "<LF>";

    case TokenType::TOK_EOF:
      return "<EOF>";

    case TokenType::TOK_ID:
      return "<ID>";

    case TokenType::TOK_INT:
      return "<INT>";

    case TokenType::TOK_FLOAT:
      return "<FLOAT>";

    case TokenType::TOK_COMMA:
      return ",";

    case TokenType::TOK_DOT:
      return ".";

    case TokenType::TOK_COLON:
      return ":";

    case TokenType::TOK_TILDE:
      return "~";

    case TokenType::TOK_EXCLAM:
      return "!";

    case TokenType::TOK_AT:
      return "@";

    case TokenType::TOK_HASH:
      return "#";

    case TokenType::TOK_DOLLAR:
      return "$";

    case TokenType::TOK_PERCENT:
      return "%";

    case TokenType::TOK_HAT:
      return "^";

    case TokenType::TOK_CHECK:
      return "?";

    case TokenType::TOK_SLASH:
      return "/";

    case TokenType::TOK_BSLASH:
      return "\\";

    case TokenType::TOK_PIPE:
      return "|";

    case TokenType::TOK_ELLIPSIS:
      return "...";

    case TokenType::TOK_SQUOTE:
      return "'";

    case TokenType::TOK_DQUOTE:
      return "\"";

    case TokenType::TOK_BQUOTE:
      return "`";

    case TokenType::TOK_LPAREN:
      return "(";

    case TokenType::TOK_RPAREN:
      return ")";

    case TokenType::TOK_LBOXBRAC:
      return "[";

    case TokenType::TOK_RBOXBRAC:
      return "]";

    case TokenType::TOK_LANGBRAC:
      return "<";

    case TokenType::TOK_RANGBRAC:
      return ">";

    case TokenType::TOK_NOT:
      return "<NOT>";

    case TokenType::TOK_AND:
      return "<LOGIC_AND>";

    case TokenType::TOK_OR:
      return "<LOGIC_OR>";

    case TokenType::TOK_XOR:
      return "<LOGIC_XOR>";

    case TokenType::TOK_BITAND:
      return "<BIT_AND>";

    case TokenType::TOK_BITOR:
      return "<BIT_OR>";

    case TokenType::TOK_BITXOR:
      return "<BIT_XOR>";

    case TokenType::TOK_LSHIFT:
      return "<BIT_LEFT_SHIFT>";

    case TokenType::TOK_RSHIFT:
      return "<BIT_RIGHT_SHIFT>";
    
    case TokenType::TOK_NEG:
      return "<NEG_OP>";

    case TokenType::TOK_ADD:
      return "<ADD_OP>";

    case TokenType::TOK_SUB:
      return "<SUB_OP>";

    case TokenType::TOK_MUL:
      return "<MUL_OP>";

    case TokenType::TOK_DIV:
      return "<DIV_OP>";

    case TokenType::TOK_MOD:
      return "<MOD_OP>";

    case TokenType::TOK_POW:
      return "<POW_OP>";

    case TokenType::TOK_GT:
      return "<GT>";

    case TokenType::TOK_GE:
      return "<GE>";

    case TokenType::TOK_LT:
      return "<LT>";

    case TokenType::TOK_LE:
      return "<LE>";

    case TokenType::TOK_EQ:
      return "<EQ>";

    case TokenType::TOK_NE:
      return "<NE>";

    case TokenType::TOK_MATCH:
      return "?=";

    case TokenType::TOK_INFINITE_LIST:
      return "[...]";
    
    default:
      return "<UNKNOWN>";
  }
}

int main() {

  while (1) 
  {
    fprintf(stderr, "styio$: ");

    for (int tok: Tokenize()) {
      std::cout << tokenRepr(tok) << ' ';
    };
    std::cout << std::endl;

  }; 

  return 0;
}