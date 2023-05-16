#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
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

enum StyioToken {
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
  TOK_STRING,

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


static std::string reprToken(int token) {
  switch (token)
  {
    case StyioToken::TOK_SPACE:
      return " ";

    case StyioToken::TOK_CR:
      return "<CR>";

    case StyioToken::TOK_LF:
      return "<LF>";

    case StyioToken::TOK_EOF:
      return "<EOF>";

    case StyioToken::TOK_ID:
      return "<ID>";

    case StyioToken::TOK_INT:
      return "<INT>";

    case StyioToken::TOK_FLOAT:
      return "<FLOAT>";

    case StyioToken::TOK_STRING:
      return "<STRING>";

    case StyioToken::TOK_COMMA:
      return ",";

    case StyioToken::TOK_DOT:
      return ".";

    case StyioToken::TOK_COLON:
      return ":";

    case StyioToken::TOK_TILDE:
      return "~";

    case StyioToken::TOK_EXCLAM:
      return "!";

    case StyioToken::TOK_AT:
      return "@";

    case StyioToken::TOK_HASH:
      return "#";

    case StyioToken::TOK_DOLLAR:
      return "$";

    case StyioToken::TOK_PERCENT:
      return "%";

    case StyioToken::TOK_HAT:
      return "^";

    case StyioToken::TOK_CHECK:
      return "?";

    case StyioToken::TOK_SLASH:
      return "/";

    case StyioToken::TOK_BSLASH:
      return "\\";

    case StyioToken::TOK_PIPE:
      return "|";

    case StyioToken::TOK_ELLIPSIS:
      return "...";

    case StyioToken::TOK_SQUOTE:
      return "'";

    case StyioToken::TOK_DQUOTE:
      return "\"";

    case StyioToken::TOK_BQUOTE:
      return "`";

    case StyioToken::TOK_LPAREN:
      return "(";

    case StyioToken::TOK_RPAREN:
      return ")";

    case StyioToken::TOK_LBOXBRAC:
      return "[";

    case StyioToken::TOK_RBOXBRAC:
      return "]";

    case StyioToken::TOK_LANGBRAC:
      return "<";

    case StyioToken::TOK_RANGBRAC:
      return ">";

    case StyioToken::TOK_NOT:
      return "<NOT>";

    case StyioToken::TOK_AND:
      return "<AND>";

    case StyioToken::TOK_OR:
      return "<OR>";

    case StyioToken::TOK_XOR:
      return "<XOR>";

    case StyioToken::TOK_BITAND:
      return "<BIT_AND>";

    case StyioToken::TOK_BITOR:
      return "<BIT_OR>";

    case StyioToken::TOK_BITXOR:
      return "<BIT_XOR>";

    case StyioToken::TOK_LSHIFT:
      return "<<";

    case StyioToken::TOK_RSHIFT:
      return ">>";
    
    case StyioToken::TOK_NEG:
      return "<NEG>";

    case StyioToken::TOK_ADD:
      return "<ADD>";

    case StyioToken::TOK_SUB:
      return "<SUB>";

    case StyioToken::TOK_MUL:
      return "<MUL>";

    case StyioToken::TOK_DIV:
      return "<DIV>";

    case StyioToken::TOK_MOD:
      return "<MOD>";

    case StyioToken::TOK_POW:
      return "<POW>";

    case StyioToken::TOK_GT:
      return "<GT>";

    case StyioToken::TOK_GE:
      return "<GE>";

    case StyioToken::TOK_LT:
      return "<LT>";

    case StyioToken::TOK_LE:
      return "<LE>";

    case StyioToken::TOK_EQ:
      return "<EQ>";

    case StyioToken::TOK_NE:
      return "<NE>";

    case StyioToken::TOK_MATCH:
      return "?=";

    case StyioToken::TOK_WALRUS:
      return ":=";

    case StyioToken::TOK_INFINITE_LIST:
      return "[...]";
    
    default:
      return "<UNKNOWN>";
  }
};

// ?(a > 1) [true % false]

/*
StyioAST
*/
class StyioAST {
  public:
    virtual ~StyioAST() {}

    virtual std::string toString() {
      return "Styio.Base {}";
    }

    virtual std::string toStringInline() {
      return "StyioAST";
    }
};

/*
IdAST
*/
class IdAST : public StyioAST {
  std::string Id;

  public:
    IdAST(const std::string &id) : Id(id) {}

    std::string toString() {
      return std::string("ID { ") + Id + " }";
    }

    std::string toStringInline() {
      return "<ID: \"" + Id + "\">";
    }
};

/*
IntAST
*/
class IntAST : public StyioAST {
  int Value;

  public:
    IntAST(int val) : Value(val) {}

    std::string toString() {
      return "Int { " + std::to_string(Value) + " }";
    }

    std::string toStringInline() {
      return std::to_string(Value);
    }
};

/*
FloatAST
*/
class FloatAST : public StyioAST {
  double Value;

  public:
    FloatAST(double val) : Value(val) {}

    std::string toString() {
      return "Float { " + std::to_string(Value) + " }";
    }

    std::string toStringInline() {
      return std::to_string(Value);
    }
};

/*
StringAST
*/
class StringAST : public StyioAST {
  std::string Value;

  public:
    StringAST(std::string val) : Value(val) {}

    std::string toString() {
      return "String { \"" + Value + "\" }";
    }

    std::string toStringInline() {
      return "\"" + Value + "\"";
    }
};

/*
VarDefAST
*/
class VarDefAST : public StyioAST {
  IdAST *Id;

  public:
    VarDefAST(IdAST *id) : Id(id) {}

    std::string toString() {
      return "| Variable : " + Id -> toStringInline() + " |";
    }

    std::string toStringInline() {
      return "Variable : " + Id -> toStringInline();
    }
};

/*
AssignAST
*/
class AssignAST : public StyioAST {
  VarDefAST *varDef;
  StyioAST *valExpr;

  public:
    AssignAST(VarDefAST *var, StyioAST *val) : varDef(var), valExpr(val) {}

    std::string toString() {
      return "| { " + varDef -> toStringInline() + " } := { " + valExpr -> toString() + " } |";
    }
};

/*
BinOpAST
*/
class BinOpAST : public StyioAST {
  StyioToken Op;
  StyioAST *LHS;
  StyioAST *RHS;

  public:
    BinOpAST(StyioToken op, StyioAST *lhs, StyioAST *rhs): Op(op), LHS(lhs), RHS(rhs) {}

    std::string toString() {
      return std::string("BinOp {\n") 
        + "  | "
        + reprToken(Op) 
        + "\n"
        + "  | "
        + LHS -> toString() 
        + "\n"
        + "  | "
        + RHS -> toString()  
        + "\n} ";
    }
};

static std::vector<int> inputBuffer;

static int readInputChar() 
{
  int tmpChar = getchar();

  inputBuffer.push_back(tmpChar);

  return tmpChar;
}

static void dropAllGaps(int& nextChar) 
{
  while (isspace(nextChar)) {
    nextChar = readInputChar();
  };
}


static void dropSpaces(int& nextChar) 
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
      StyioToken::TOK_SPACE
    );
  };
}

static void parseLF(std::vector<int>& tokenBuffer, int& nextChar) 
{
  // nextChar = readInputChar();
  tokenBuffer.push_back(
    StyioToken::TOK_LF
  );
}

static void parseCR(std::vector<int>& tokenBuffer, int& nextChar) 
{
  // nextChar = readInputChar();
  tokenBuffer.push_back(
    StyioToken::TOK_CR
  );
}

static void parseEOF(std::vector<int>& tokenBuffer, int& nextChar) 
{
  // nextChar = readInputChar();
  tokenBuffer.push_back(
    StyioToken::TOK_EOF
  );
}

static IdAST* parseId(std::vector<int>& tokenBuffer, int& nextChar) 
{
  std::string idStr = "";
  idStr += nextChar;

  // [a-zA-Z][a-zA-Z0-9_]*
  while (isalnum((nextChar = readInputChar())) || nextChar == '_') 
  {
    idStr += nextChar;
  }

  tokenBuffer.push_back(
    StyioToken::TOK_ID
  );

  IdAST* result = new IdAST(idStr);

  std::cout << result -> toString() << std::endl;

  return result;
}

static StringAST* parseString(std::vector<int>& tokenBuffer, int& nextChar) 
{
  // eliminate the first(start) double quote
  nextChar = readInputChar();

  std::string textStr = "";
  
  while (nextChar != '\"')
  {
    textStr += nextChar;
    nextChar = readInputChar();
  };

  // eliminate the second(end) double quote
  nextChar = readInputChar();

  tokenBuffer.push_back(
    StyioToken::TOK_STRING
  );

  StringAST* result = new StringAST(textStr);

  std::cout << result -> toString() << std::endl;

  return result;
}

static StyioAST* parseNum(std::vector<int>& tokenBuffer, int& nextChar) 
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

    // if (!isspace(nextChar)) 
    // {
    //   std::string errmsg = "Float `" + numStr + "` ends with unexpected char `" + char(nextChar) + "`";
    //   throw StyioSyntaxError(errmsg);
    // }

    tokenBuffer.push_back(
      StyioToken::TOK_FLOAT
    );

    FloatAST* result = new FloatAST(std::stod(numStr));

    std::cout << result -> toString() << std::endl;

    return result;
  } 
  else 
  {
    // if (!isspace(nextChar)) 
    // {
    //   std::string errmsg = "Integer `" + numStr + "` ends with unexpected char `" + char(nextChar) + "`";
    //   throw StyioSyntaxError(errmsg);
    // }

    tokenBuffer.push_back(
      StyioToken::TOK_INT
    );

    IntAST* result = new IntAST(std::stoi(numStr));

    std::cout << result -> toString() << std::endl;

    return result;
  }
}

static BinOpAST* parseBinOp(
  std::vector<int>& tokenBuffer, 
  int& nextChar, 
  StyioToken signToken,
  StyioAST* lhsAST
) 
{
  nextChar = readInputChar();

  tokenBuffer.push_back(signToken);

  dropSpaces(nextChar);

  if (isalpha(nextChar) || nextChar == '_') {
    BinOpAST* result = new BinOpAST(signToken, lhsAST, parseId(tokenBuffer, nextChar));
    std::cout << result -> toString() << std::endl;
    return result;
  }

  if (isdigit(nextChar)) {
    BinOpAST* result = new BinOpAST(signToken, lhsAST, parseNum(tokenBuffer, nextChar));
    std::cout << result -> toString() << std::endl;
    return result;
  }
}

static std::vector<int> Tokenize() {
  std::vector<int> tokenBuffer;

  static int nextChar = ' ';

  while (1)
  {
    dropAllGaps(nextChar);

    if (isalpha(nextChar) || nextChar == '_') 
    {
      // parse id
      IdAST* idAST = parseId(tokenBuffer, nextChar);
      
      // ignore white spaces after id
      dropSpaces(nextChar);

      // check next character
      switch (nextChar)
      {
        // <LF>
        case '\n':
          parseLF(tokenBuffer, nextChar);
          return tokenBuffer;
        
        // <ID> := <EXPR>
        case ':':
          nextChar = readInputChar();
          if (nextChar == '=')
          {
            std::cout << "|NotImplemented| VAR_ASSIGN" << std::endl;
          }
          break;
        // BIN_ADD := <ID> "+" <EXPR>
        case '+':
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_ADD, idAST);
          break;
        // BIN_SUB := <ID> "-" <EXPR>
        case '-':
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_SUB, idAST);
          break;
        // BIN_MUL | BIN_POW
        case '*':
          nextChar = readInputChar();
          // BIN_POW := <ID> "**" <EXPR>
          if (nextChar == '*')
          {
            parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_POW, idAST);
          } 
          // BIN_MUL := <ID> "*" <EXPR>
          else 
          {
            parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_MUL, idAST);
          }
          break;
        // BIN_MUL := <ID> "/" <EXPR>
        case '/':
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_DIV, idAST);
          break;
        // BIN_MUL := <ID> "%" <EXPR>
        case '%':
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_MOD, idAST);
          break;
        
        default:
          // std::cout << "|Info| <ID> followed by unhandled char " << thisChar << std::endl;
          break;
      }
    }

    if (isdigit(nextChar)) {
      StyioAST* numAST = parseNum(tokenBuffer, nextChar);

      dropSpaces(nextChar);

      switch (nextChar)
      {
        // <LF>
        case '\n':
          parseLF(tokenBuffer, nextChar);
          return tokenBuffer;
        // BIN_ADD := [<Integer>|<Float>] "+" <EXPR>
        case '+':
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_ADD, numAST);
          break;
        // BIN_SUB := [<Integer>|<Float>] "-" <EXPR>
        case '-':
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_SUB, numAST);
          break;
        // BIN_MUL | BIN_POW
        case '*':
          nextChar = readInputChar();
          // BIN_POW := [<Integer>|<Float>] "**" <EXPR>
          if (nextChar == '*')
          {
            parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_POW, numAST);
          } 
          // BIN_MUL := [<Integer>|<Float>] "*" <EXPR>
          else 
          {
            parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_MUL, numAST);
          }
          break;
        // BIN_DIV := [<Integer>|<Float>] "/" <EXPR>
        case '/':
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_DIV, numAST);
          break;
        // BIN_MOD := [<Integer>|<Float>] "%" <EXPR>
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_MOD, numAST);
          break;

        default:
          // std::cout << "|Info| <Integer> followed by unhandled char " << thisChar << std::endl;
          break;
      }
    }

    switch (nextChar)
    {
      case '@':
        tokenBuffer.push_back(
          StyioToken::TOK_AT
        );
        
        dropSpaces(nextChar);

        if (isalpha(nextChar) || ) {

        } 
        
        break; 

      case '\"':
        parseString(tokenBuffer, nextChar);
        break;

      case '!':
        tokenBuffer.push_back(
          StyioToken::TOK_EXCLAM
        );
        nextChar = readInputChar();
        break;

      case ',':
        tokenBuffer.push_back(
          StyioToken::TOK_COMMA
        );
        nextChar = readInputChar();
        break;

      case '.':
        tokenBuffer.push_back(
          StyioToken::TOK_DOT
        );
        nextChar = readInputChar();
        break;

      case ';':
        tokenBuffer.push_back(
          StyioToken::TOK_SEMICOLON
        );
        nextChar = readInputChar();
        break;

      case ':':
        tokenBuffer.push_back(
          StyioToken::TOK_COLON
        );
        nextChar = readInputChar();
        break;

      case '(':
        tokenBuffer.push_back(
          StyioToken::TOK_LPAREN
        );
        nextChar = readInputChar();
        break;

      case ')':
        tokenBuffer.push_back(
          StyioToken::TOK_RPAREN
        );
        nextChar = readInputChar();
        break;

      case '[':
        tokenBuffer.push_back(
          StyioToken::TOK_LBOXBRAC
        );
        nextChar = readInputChar();
        break;

      case ']':
        tokenBuffer.push_back(
          StyioToken::TOK_RBOXBRAC
        );
        nextChar = readInputChar();
        break;

      case '{':
        tokenBuffer.push_back(
          StyioToken::TOK_LCURBRAC
        );
        nextChar = readInputChar();
        break;

      case '}':
        tokenBuffer.push_back(
          StyioToken::TOK_RCURBRAC
        );
        nextChar = readInputChar();
        break;
      default:
        break;
    }

    switch (nextChar)
    {
      case '\n':
        parseLF(tokenBuffer, nextChar);
        return tokenBuffer;

      case EOF:
        parseEOF(tokenBuffer, nextChar);
        return tokenBuffer;
    
      default:
        std::cout << "Continue: " << char(nextChar) << std::endl;
        break;
    }
  };

  return tokenBuffer;
}


int main() {

  while (1) 
  {
    fprintf(stderr, "Styio/> ");

    for (int tok: Tokenize()) {
      std::cout << reprToken(tok) << ' ';
    };
    std::cout << std::endl;

  }; 

  return 0;
}