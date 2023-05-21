#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <list>
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

  TOK_RARROW, // ->
  TOK_LARROW, // <-

  TOK_WALRUS, // :=
  TOK_MATCH, // ?=

  TOK_ELLIPSIS, // ...

  TOK_INFINITE_LIST, // [...]
};


static std::string reprToken(int token) {
  switch (token)
  {
    case StyioToken::TOK_NULL:
      return "<NULL>";

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

    case StyioToken::TOK_LCURBRAC:
      return "{";

    case StyioToken::TOK_RCURBRAC:
      return "}";

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

    case StyioToken::TOK_RARROW:
      return "->";

    case StyioToken::TOK_LARROW:
      return "<-";

    case StyioToken::TOK_WALRUS:
      return ":=";

    case StyioToken::TOK_MATCH:
      return "?=";

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
      return "<ID: " + Id + ">";
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
      return "<Int: " + std::to_string(Value) + ">";
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
      return "<Float: " + std::to_string(Value) + ">";
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
      return "<String: \"" + Value + "\">";
    }
};

/*
AssignAST
*/
class AssignAST : public StyioAST {
  IdAST* varId;
  StyioAST* valExpr;

  public:
    AssignAST(IdAST* var, StyioAST* val) : varId(var), valExpr(val) {}

    std::string toString(int indent = 2) {
      return std::string("Assign {\n") 
        + std::string(indent, ' ') + "| Var: " 
        + varId -> toString() 
        + "\n"
        + std::string(indent, ' ') + "| Op:  " 
        + ":="
        + "\n"
        + std::string(indent, ' ') + "| Val: " 
        + valExpr -> toString() 
        + "\n"
        + "}";
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
    BinOpAST(StyioToken op, StyioAST* lhs, StyioAST* rhs): Op(op), LHS(lhs), RHS(rhs) {}

    std::string toString(int indent = 2) {
      return std::string("BinOp {\n") 
        + std::string(indent, ' ') + "| LHS: "
        + LHS -> toString() 
        + "\n"
        + std::string(indent, ' ') + "| Op:  "
        + reprToken(Op)
        + "\n"
        + std::string(indent, ' ') + "| RHS: "
        + RHS -> toString()  
        + "\n} ";
    }
};

/*
VarDefAST
*/
class VarDefAST : public StyioAST {
  std::vector<IdAST*> Vars;

  public:
    VarDefAST(std::vector<IdAST*> vars): Vars(vars) {}

    std::string toString(int indent = 2) {
      std::string varStr;

      for (std::vector<IdAST*>::iterator it = Vars.begin(); 
        it != Vars.end(); 
        ++it
      ) {
        varStr += std::string(indent, ' ') + "| ";
        varStr += (*it) -> toStringInline();
        varStr += "\n";
      };

      return std::string("Variable Definition {\n")
        + varStr
        + "\n} ";
    }
};

/*
BlockAST
*/
class BlockAST : public StyioAST {
  std::vector<StyioAST*> Stmts;
  StyioAST* Expr;

  public:
    BlockAST() {}

    BlockAST(StyioAST* expr): Expr(expr) {}

    BlockAST(std::vector<StyioAST*> stmts, StyioAST* expr): Stmts(stmts), Expr(expr) {}

    std::string toString(int indent = 2) {
      std::string stmtStr;

      for (std::vector<StyioAST*>::iterator it = Stmts.begin(); 
        it != Stmts.end(); 
        ++it
      ) {
        stmtStr += (*it) -> toStringInline();
        stmtStr += "\n";
      };

      return std::string("Block {\n")
        
        + std::string(indent, ' ') + "| Stmts: "
        + stmtStr
        + "\n"
        + std::string(indent, ' ') + "| Expr:  "
        + Expr -> toString()  
        + "\n} ";
    }
};

/*
DependAST
*/
class DependencyAST : public StyioAST {
  std::vector<std::string> DependencyPaths;

  public:
    DependencyAST(std::vector<std::string> paths): DependencyPaths(paths) {}

    std::string toString(int indent = 2) {
      std::string dependencyPathsStr;

      for(int i=0; i < DependencyPaths.size(); i++) {
        dependencyPathsStr += std::string(indent, ' ') + "| ";
        dependencyPathsStr += DependencyPaths[i];
        dependencyPathsStr += "\n";
      };

      return std::string("Dependencies {\n")
        + dependencyPathsStr
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

static void dropAllSpaces (int& nextChar) 
{
  while (isspace(nextChar)) {
    nextChar = readInputChar();
  };
}


static void dropWhiteSpace (int& nextChar) 
{
  while (nextChar == ' ') {
    nextChar = readInputChar();
  };
}

static void parseSpace (std::vector<int>& tokenBuffer, int& nextChar) 
{
  while (nextChar == ' ') {
    nextChar = readInputChar();
    tokenBuffer.push_back(
      StyioToken::TOK_SPACE
    );
  };
}

static void parseLF (std::vector<int>& tokenBuffer, int& nextChar) 
{
  // nextChar = readInputChar();
  tokenBuffer.push_back(
    StyioToken::TOK_LF
  );
}

static void parseCR (std::vector<int>& tokenBuffer, int& nextChar) 
{
  // nextChar = readInputChar();
  tokenBuffer.push_back(
    StyioToken::TOK_CR
  );
}

static void parseEOF (std::vector<int>& tokenBuffer, int& nextChar) 
{
  // nextChar = readInputChar();
  tokenBuffer.push_back(
    StyioToken::TOK_EOF
  );
}

static IdAST* parseId (std::vector<int>& tokenBuffer, int& nextChar) 
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

static StyioAST* parseNum (std::vector<int>& tokenBuffer, int& nextChar)
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
    //   std::string errmsg = "Int `" + numStr + "` ends with unexpected char `" + char(nextChar) + "`";
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

static StringAST* parseString (std::vector<int>& tokenBuffer, int& nextChar) 
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

static AssignAST* parseAssign (
  std::vector<int>& tokenBuffer, 
  int& nextChar, 
  IdAST* idAST
) 
{
  if (isalpha(nextChar) || nextChar == '_') 
  {
    StyioAST* value = parseId(tokenBuffer, nextChar);
    
    AssignAST* result = new AssignAST(idAST, value);

    std::cout << result -> toString() << std::endl;

    return result;
  }
  else
  if (isdigit(nextChar))
  {
    StyioAST* value = parseNum(tokenBuffer, nextChar);
    
    AssignAST* result = new AssignAST(idAST, value);

    std::cout << result -> toString() << std::endl;

    return result;
  }
  else
  {
    std::string errmsg = std::string("Unexpected Assign.Value, starts with character `") + char(nextChar) + "`";
    throw StyioSyntaxError(errmsg);
  };

  std::cout << "|NotImplemented| VAR_ASSIGN" << std::endl;
}

static BinOpAST* parseBinOp (
  std::vector<int>& tokenBuffer, 
  int& nextChar, 
  StyioToken signToken,
  StyioAST* lhsAST
) 
{
  dropWhiteSpace(nextChar);

  if (isalpha(nextChar) || nextChar == '_') {
    BinOpAST* result = new BinOpAST(signToken, lhsAST, parseId(tokenBuffer, nextChar));
    std::cout << result -> toString() << std::endl;
    return result;
  }
  else
  if (isdigit(nextChar)) {
    BinOpAST* result = new BinOpAST(signToken, lhsAST, parseNum(tokenBuffer, nextChar));
    std::cout << result -> toString() << std::endl;
    return result;
  }
  else
  {
    std::string errmsg = std::string("Unexpected BinOp.RHS, starts with character `") + char(nextChar) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

static StyioAST* parseExpr (std::vector<int>& tokenBuffer, int& nextChar) 
{
  while (nextChar != ';')
  {
    dropAllSpaces(nextChar);

    // <ID>
    if (isalpha(nextChar) || nextChar == '_') 
    {
      // parse id
      IdAST* idAST = parseId(tokenBuffer, nextChar);
      
      // ignore white spaces after id
      dropWhiteSpace(nextChar);

      // check next character
      switch (nextChar)
      {
        // <LF>
        case '\n':
          // simply eliminate LF
          nextChar = readInputChar();
          parseLF(tokenBuffer, nextChar);
          return idAST;

          // You should NOT reach this line.
          break;
        
        // <ID> := <EXPR>
        case ':':
          nextChar = readInputChar();
          if (nextChar == '=')
          {
            nextChar = readInputChar();

            tokenBuffer.push_back(
              StyioToken::TOK_WALRUS
            );

            dropWhiteSpace(nextChar);
            
            // <ID> := | ->
            AssignAST* assignAST = parseAssign(tokenBuffer, nextChar, idAST);
            
            return assignAST;
          }
          else
          {
            std::string errmsg = std::string("Unexpected `:`");
            throw StyioSyntaxError(errmsg);
          }

          // You should NOT reach this line.
          break;

        // BIN_ADD := <ID> "+" <EXPR>
        case '+':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_ADD);

          // <ID> "+" | -> 
          BinOpAST* binOpAST = parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_ADD, idAST);
          return binOpAST;

          // You should NOT reach this line.
          break;

        // BIN_SUB := <ID> "-" <EXPR>
        case '-':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_SUB);

          // <ID> "-" | ->
          BinOpAST* binOpAST = parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_SUB, idAST);
          return binOpAST;

          // You should NOT reach this line.
          break;

        // BIN_MUL | BIN_POW
        case '*':
          nextChar = readInputChar();
          // BIN_POW := <ID> "**" <EXPR>
          if (nextChar == '*')
          {
            nextChar = readInputChar();
            tokenBuffer.push_back(StyioToken::TOK_POW);

            // <ID> "**" | ->
            BinOpAST* binOpAST = parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_POW, idAST);
            return binOpAST;
          } 
          // BIN_MUL := <ID> "*" <EXPR>
          else 
          {
            tokenBuffer.push_back(StyioToken::TOK_MUL);

            // <ID> "*" | ->
            BinOpAST* binOpAST = parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_MUL, idAST);
            return binOpAST;
          }

          // You should NOT reach this line.
          break;
          
        // BIN_DIV := <ID> "/" <EXPR>
        case '/':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_DIV);

          // <ID> "/" | -> 
          BinOpAST* binOpAST = parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_DIV, idAST);
          return binOpAST;

          // You should NOT reach this line.
          break;

        // BIN_MOD := <ID> "%" <EXPR> 
        case '%':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_MOD);

          // <ID> "%" | -> 
          BinOpAST* binOpAST = parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_MOD, idAST);
          return binOpAST;

          // You should NOT reach this line.
          break;
        
        default:
          break;
      }
    }

    if (isdigit(nextChar)) {
      StyioAST* numAST = parseNum(tokenBuffer, nextChar);

      dropWhiteSpace(nextChar);

      switch (nextChar)
      {
        // <LF>
        case '\n':
          // simply eliminate LF
          nextChar = readInputChar();
          parseLF(tokenBuffer, nextChar);

          return numAST;

          // You should NOT reach this line.
          break;

        // BIN_ADD := [<Int>|<Float>] "+" <EXPR>
        case '+':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_ADD);

          // [<Int>|<Float>] "+" | ->
          BinOpAST* binOpAST = parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_ADD, numAST);
          return binOpAST;

          // You should NOT reach this line.
          break;

        // BIN_SUB := [<Int>|<Float>] "-" <EXPR>
        case '-':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_SUB);

          // [<Int>|<Float>] "-" | ->
          BinOpAST* binOpAST = parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_SUB, numAST);
          return binOpAST;

          // You should NOT reach this line.
          break;

        // BIN_MUL | BIN_POW
        case '*':
          nextChar = readInputChar();
          // BIN_POW := [<Int>|<Float>] "**" <EXPR>
          if (nextChar == '*')
          {
            nextChar = readInputChar();
            tokenBuffer.push_back(StyioToken::TOK_POW);

            // [<Int>|<Float>] "**" | ->
            BinOpAST* binOpAST = parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_POW, numAST);
            return binOpAST;
          } 
          // BIN_MUL := [<Int>|<Float>] "*" <EXPR>
          else 
          {
            tokenBuffer.push_back(StyioToken::TOK_MUL);

            // [<Int>|<Float>] "*" | ->
            BinOpAST* binOpAST = parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_MUL, numAST);
            return binOpAST;
          }

          // You should NOT reach this line.
          break;
        // BIN_DIV := [<Int>|<Float>] "/" <EXPR>
        case '/':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_DIV);
          
          // [<Int>|<Float>] "/" | ->
          BinOpAST* binOpAST = parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_DIV, numAST);
          return binOpAST;

          // You should NOT reach this line.
          break;
        // BIN_MOD := [<Int>|<Float>] "%" <EXPR>
        case '%':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_MOD);
          
          // [<Int>|<Float>] "%" | ->
          BinOpAST* binOpAST = parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_MOD, numAST);
          return binOpAST;

          // You should NOT reach this line.
          break;

        default:
          break;
      }
    }

    switch (nextChar)
    {
      // VAR_DEF := "@" "(" [<ID> ["," <ID>]*]? ")"
      case '@':
        nextChar = readInputChar();

        tokenBuffer.push_back(
          StyioToken::TOK_AT
        );
        
        dropWhiteSpace(nextChar);

        if (nextChar == '(') 
        {
          nextChar = readInputChar();

          tokenBuffer.push_back(
            StyioToken::TOK_LPAREN
          );
        };
        
        std::vector<IdAST*> varBuffer;

        if (isalpha(nextChar) || nextChar == '_') {
          // "@" "(" | ->
          IdAST* idAST = parseId(tokenBuffer, nextChar);
      
          varBuffer.push_back(idAST);
        };

        dropWhiteSpace(nextChar);

        // "@" "(" [<ID> | ->
        while (nextChar == ',')
        {
          nextChar = readInputChar();

          dropWhiteSpace(nextChar);

          if (isalpha(nextChar) || nextChar == '_') {
            tokenBuffer.push_back(
              StyioToken::TOK_COMMA
            );
            
            IdAST* idAST = parseId(tokenBuffer, nextChar);
      
            varBuffer.push_back(idAST);

            dropWhiteSpace(nextChar);
          };
        };
        
        if (nextChar == ')') 
        {
          nextChar = readInputChar();

          tokenBuffer.push_back(
            StyioToken::TOK_RPAREN
          );
        };

        VarDefAST* result = new VarDefAST(varBuffer);

        std::cout << result -> toString() << std::endl;

        return result;
        
        // You should NOT reach this line.
        break; 

      case ':':
        nextChar = readInputChar();

        if (nextChar == '=') {
          nextChar = readInputChar();

          tokenBuffer.push_back(
            StyioToken::TOK_WALRUS
          );
        } 
        else
        {
          tokenBuffer.push_back(
            StyioToken::TOK_COLON
          );
        };
        
        break;

      case '-':
        nextChar = readInputChar();

        if (nextChar == '>') {
          nextChar = readInputChar();

          tokenBuffer.push_back(
            StyioToken::TOK_RARROW
          );
        } else {
          tokenBuffer.push_back(
            StyioToken::TOK_MINUS
          );
        };
        
        break;

      case '?':
        nextChar = readInputChar();
        
        if (nextChar == '=') {
          nextChar = readInputChar();

          tokenBuffer.push_back(
            StyioToken::TOK_MATCH
          );
        } 
        else 
        {
          tokenBuffer.push_back(
            StyioToken::TOK_CHECK
          );
        };
        
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
        parseDependency(tokenBuffer, nextChar);

        break;

      case ']':
        tokenBuffer.push_back(
          StyioToken::TOK_RBOXBRAC
        );
        nextChar = readInputChar();
        
        break;

      case '{':
        nextChar = readInputChar();
        
        tokenBuffer.push_back(
          StyioToken::TOK_LCURBRAC
        );
        
        // "{" | ->
        parseBlock(tokenBuffer, nextChar);

        break;

      case '}':
        nextChar = readInputChar();
        
        tokenBuffer.push_back(
          StyioToken::TOK_RCURBRAC
        );

        break;

      case '<':
        nextChar = readInputChar();
        tokenBuffer.push_back(
          StyioToken::TOK_LANGBRAC
        );
        break;

      case '>':
        nextChar = readInputChar();
        tokenBuffer.push_back(
          StyioToken::TOK_RANGBRAC
        );
        break;
        
      default:
        break;
    }

    std::cout << "Next: " << char(nextChar) << std::endl;
  };
}

static BlockAST* parseBlock (std::vector<int>& tokenBuffer, int& nextChar) 
{
  // the last expression will be the return expression
  // a block must have a return value
  // either an expression
  // or null

  std::vector<int> tokenBuffer;
  std::vector<StyioAST*> stmtBuffer;

  static int nextChar = ' ';

  nextChar = readInputChar();

  if (nextChar != '}') {
    StyioAST* exprAST = parseExpr(tokenBuffer, nextChar);
    stmtBuffer.push_back(exprAST);

    while (nextChar == ';')
    {
      // eliminate ;
      nextChar = readInputChar();

      StyioAST* exprAST = parseExpr(tokenBuffer, nextChar);
      stmtBuffer.push_back(exprAST);
    }

    BlockAST* result = new BlockAST(stmtBuffer, exprAST);

    std::cout << result -> toString() << std::endl;

    return result;
  };
  
}


static std::string parseDependencyItem(std::vector<int>& tokenBuffer, int& nextChar)
{
  std::string itemStr;

  if (nextChar == '\"')
  {
    // eliminate double quote symbol " at the start of dependency item
    nextChar = readInputChar();

    while (nextChar != '\"') 
    {
      if (nextChar == ',') 
      {
        std::string errmsg = std::string("An \" was expected after") + itemStr + "however, a delimeter `,` was detected. ";
        throw StyioSyntaxError(errmsg);
      }

      itemStr += nextChar;

      nextChar = readInputChar();
    };

    // eliminate double quote symbol " at the end of dependency item
    nextChar = readInputChar();

    return itemStr;
  }
  else
  {
    std::string errmsg = std::string("Dependencies should be wrapped with double quote like \"abc/xyz\", rather than starting with the character `") + char(nextChar) + "`";
    throw StyioSyntaxError(errmsg);
  };
}

/*
parseDependency

Dependencies should be written like a list of paths
like this -> ["ab/c", "x/yz"]

// 1. The dependencies should be parsed before any domain (statement/expression). 
// 2. The left square bracket `[` is only eliminated after entering this function (parseDependency)
| -> "[" <PATH>+ "]"

If ? ( "the program starts with a left square bracket `[`" ),
then -> { 
  "parseDependency() starts";
  "eliminate the left square bracket `[`";
  "parse dependency paths, which take comma `,` as delimeter";
  "eliminate the right square bracket `]`";
} 
else :  { 
  "parseDependency() should NOT be invoked in this case";
  "if starts with left curly brace `{`, try parseDomain()";
  "otherwise, try parseScript()";
}

*/
static DependencyAST* parseDependency (std::vector<int>& tokenBuffer, int& nextChar) 
{ 
  // eliminate left square (box) bracket [
  nextChar = readInputChar();
  tokenBuffer.push_back(
    StyioToken::TOK_LBOXBRAC
  );

  std::vector<std::string> dependencies;

  dropAllSpaces(nextChar);

  // add the first dependency path to the list
  dependencies.push_back(parseDependencyItem(tokenBuffer, nextChar));

  std::string pathStr = "";
  
  while (nextChar == ',') {
    // eliminate comma ","
    nextChar = readInputChar();

    // reset pathStr to empty ""
    pathStr = ""; 

    dropAllSpaces(nextChar);
    
    // add the next dependency path to the list
    dependencies.push_back(parseDependencyItem(tokenBuffer, nextChar));
  };

  if (nextChar == ']') {
    // eliminate right square bracket `]` after dependency list
    nextChar = readInputChar();
  };

  DependencyAST* result = new DependencyAST(dependencies);

  std::cout << result -> toString() << std::endl;

  return result;
}

static void parseDomain (std::vector<int>& tokenBuffer, int& nextChar) 
{

}

static void parseModule (std::vector<int>& tokenBuffer, int& nextChar) 
{
  if (nextChar == '[') { 
    // | -> <DEPEND>
    parseDependency(tokenBuffer, nextChar);

    dropAllSpaces(nextChar);
  };

  if (nextChar == '-') 
  {
    nextChar = readInputChar();

    if (nextChar == '>')
    {
      nextChar = readInputChar();
    }
    else
    {
      std::string errmsg = "Missed `>` for right arrow `->`";
      throw StyioSyntaxError(errmsg);
    };
  };

  // | -> "{" <STMT>* <EXPR>? "}"
  if (nextChar == '{') {
    parseDomain(tokenBuffer, nextChar);
  };

  // | -> <STMT>* <EXPR>?
  parseBlock(tokenBuffer, nextChar);
}

static std::vector<int> parseProgram () 
{
  std::vector<int> tokenBuffer;
  static int nextChar = ' ';

  return tokenBuffer;
}

static std::vector<int> Tokenize() {
  std::vector<int> tokenBuffer;

  static int nextChar = ' ';

  while (1)
  {
    dropAllSpaces(nextChar);

    // <ID>
    if (isalpha(nextChar) || nextChar == '_') 
    {
      // parse id
      IdAST* idAST = parseId(tokenBuffer, nextChar);
      
      // ignore white spaces after id
      dropWhiteSpace(nextChar);

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
            nextChar = readInputChar();

            tokenBuffer.push_back(
              StyioToken::TOK_WALRUS
            );

            dropWhiteSpace(nextChar);
            
            // <ID> := | ->
            parseAssign(tokenBuffer, nextChar, idAST);
          }
          break;

        // BIN_ADD := <ID> "+" <EXPR>
        case '+':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_ADD);

          // <ID> "+" | -> 
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_ADD, idAST);
          break;

        // BIN_SUB := <ID> "-" <EXPR>
        case '-':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_SUB);

          // <ID> "-" | ->
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_SUB, idAST);
          break;

        // BIN_MUL | BIN_POW
        case '*':
          nextChar = readInputChar();
          // BIN_POW := <ID> "**" <EXPR>
          if (nextChar == '*')
          {
            nextChar = readInputChar();
            tokenBuffer.push_back(StyioToken::TOK_POW);

            // <ID> "**" | ->
            parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_POW, idAST);
          } 
          // BIN_MUL := <ID> "*" <EXPR>
          else 
          {
            tokenBuffer.push_back(StyioToken::TOK_MUL);

            // <ID> "*" | ->
            parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_MUL, idAST);
          }
          break;
          
        // BIN_DIV := <ID> "/" <EXPR>
        case '/':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_DIV);

          // <ID> "/" | -> 
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_DIV, idAST);
          break;

        // BIN_MOD := <ID> "%" <EXPR> 
        case '%':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_MOD);

          // <ID> "%" | -> 
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_MOD, idAST);
          break;
        
        default:
          break;
      }
    }

    if (isdigit(nextChar)) {
      StyioAST* numAST = parseNum(tokenBuffer, nextChar);

      dropWhiteSpace(nextChar);

      switch (nextChar)
      {
        // <LF>
        case '\n':
          parseLF(tokenBuffer, nextChar);
          return tokenBuffer;

        // BIN_ADD := [<Int>|<Float>] "+" <EXPR>
        case '+':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_ADD);

          // [<Int>|<Float>] "+" | ->
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_ADD, numAST);
          break;

        // BIN_SUB := [<Int>|<Float>] "-" <EXPR>
        case '-':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_SUB);

          // [<Int>|<Float>] "-" | ->
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_SUB, numAST);
          break;

        // BIN_MUL | BIN_POW
        case '*':
          nextChar = readInputChar();
          // BIN_POW := [<Int>|<Float>] "**" <EXPR>
          if (nextChar == '*')
          {
            nextChar = readInputChar();
            tokenBuffer.push_back(StyioToken::TOK_POW);

            // [<Int>|<Float>] "**" | ->
            parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_POW, numAST);
          } 
          // BIN_MUL := [<Int>|<Float>] "*" <EXPR>
          else 
          {
            tokenBuffer.push_back(StyioToken::TOK_MUL);

            // [<Int>|<Float>] "*" | ->
            parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_MUL, numAST);
          }
          break;
        // BIN_DIV := [<Int>|<Float>] "/" <EXPR>
        case '/':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_DIV);
          
          // [<Int>|<Float>] "/" | ->
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_DIV, numAST);
          break;
        // BIN_MOD := [<Int>|<Float>] "%" <EXPR>
        case '%':
          nextChar = readInputChar();
          tokenBuffer.push_back(StyioToken::TOK_MOD);
          
          // [<Int>|<Float>] "%" | ->
          parseBinOp(tokenBuffer, nextChar, StyioToken::TOK_MOD, numAST);
          break;

        default:
          break;
      }
    }

    switch (nextChar)
    {
      // VAR_DEF := "@" "(" [<ID> ["," <ID>]*]? ")"
      case '@':
        nextChar = readInputChar();

        tokenBuffer.push_back(
          StyioToken::TOK_AT
        );
        
        dropWhiteSpace(nextChar);

        if (nextChar == '(') 
        {
          nextChar = readInputChar();

          tokenBuffer.push_back(
            StyioToken::TOK_LPAREN
          );
        };
         
        if (isalpha(nextChar) || nextChar == '_') {
          // "@" "(" | ->
          parseId(tokenBuffer, nextChar);
        };

        dropWhiteSpace(nextChar);

        // "@" "(" [<ID> | ->
        while (nextChar == ',')
        {
          nextChar = readInputChar();

          dropWhiteSpace(nextChar);

          if (isalpha(nextChar) || nextChar == '_') {
            tokenBuffer.push_back(
              StyioToken::TOK_COMMA
            );
            
            parseId(tokenBuffer, nextChar);

            dropWhiteSpace(nextChar);
          };
        };
        
        if (nextChar == ')') 
        {
          nextChar = readInputChar();

          tokenBuffer.push_back(
            StyioToken::TOK_RPAREN
          );
        };
        
        break; 

      case ':':
        nextChar = readInputChar();

        if (nextChar == '=') {
          nextChar = readInputChar();

          tokenBuffer.push_back(
            StyioToken::TOK_WALRUS
          );
        } 
        else
        {
          tokenBuffer.push_back(
            StyioToken::TOK_COLON
          );
        };
        
        break;

      case '-':
        nextChar = readInputChar();

        if (nextChar == '>') {
          nextChar = readInputChar();

          tokenBuffer.push_back(
            StyioToken::TOK_RARROW
          );
        } else {
          tokenBuffer.push_back(
            StyioToken::TOK_MINUS
          );
        };
        
        break;

      case '?':
        nextChar = readInputChar();
        
        if (nextChar == '=') {
          nextChar = readInputChar();

          tokenBuffer.push_back(
            StyioToken::TOK_MATCH
          );
        } 
        else 
        {
          tokenBuffer.push_back(
            StyioToken::TOK_CHECK
          );
        };
        
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
        parseDependency(tokenBuffer, nextChar);

        break;

      case ']':
        tokenBuffer.push_back(
          StyioToken::TOK_RBOXBRAC
        );
        nextChar = readInputChar();
        
        break;

      case '{':
        nextChar = readInputChar();
        
        tokenBuffer.push_back(
          StyioToken::TOK_LCURBRAC
        );
        
        // "{" | ->
        parseBlock(tokenBuffer, nextChar);

        break;

      case '}':
        nextChar = readInputChar();
        
        tokenBuffer.push_back(
          StyioToken::TOK_RCURBRAC
        );

        break;

      case '<':
        nextChar = readInputChar();
        tokenBuffer.push_back(
          StyioToken::TOK_LANGBRAC
        );
        break;

      case '>':
        nextChar = readInputChar();
        tokenBuffer.push_back(
          StyioToken::TOK_RANGBRAC
        );
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