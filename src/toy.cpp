#include <cstdio>
#include <cstdlib>
#include <string>
#include <map>
#include <vector>

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
  TOK_EOF = -1,

  TOK_VARDEF,

  TOK_ID, 
  TOK_NUM,
};

static std::string IdStr;  // Filled in if TOK_ID
static double NumVal;              // Filled in if tok_number

/// getToken - Return the next token from standard input.
static int getToken() {
  static int nextChar = ' ';

  // ignore white spaces
  while (isspace(nextChar))
  {
    nextChar = getchar();
  }

  if (nextChar == '@')
  {
    return TOK_VARDEF;
  }

  if (isalpha(nextChar)) {
    IdStr = nextChar;

    // [a-zA-Z][a-zA-Z0-9]*
    while (isalnum((nextChar = getchar())) || nextChar == '_') 
    {
        IdStr += nextChar;
    }

    return TOK_ID;
  }

  if (isdigit(nextChar) || nextChar == '.') {   // Number: [0-9.]+
    std::string NumStr;
    do {
      NumStr += nextChar;
      nextChar = getchar();
    } while (isdigit(nextChar) || nextChar == '.');

    NumVal = strtod(NumStr.c_str(), 0);
    return TOK_NUM;
  }

  // Check for end of file.  Don't eat the EOF.
  if (nextChar == EOF)
    return TOK_EOF;

  // Otherwise, just return the character as its ascii value.
  int ThisChar = nextChar;
  nextChar = getchar();
  return ThisChar;
}

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//

/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() {}
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;
public:
  NumberExprAST(double val) : Val(val) {}
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name;
public:
  VariableExprAST(const std::string &name) : Name(name) {}
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
  char Op;
  ExprAST *LHS, *RHS;
public:
  BinaryExprAST(char op, ExprAST *lhs, ExprAST *rhs)
    : Op(op), LHS(lhs), RHS(rhs) {}
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<ExprAST*> Args;
public:
  CallExprAST(const std::string &callee, std::vector<ExprAST*> &args)
    : Callee(callee), Args(args) {}
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;
public:
  PrototypeAST(const std::string &name, const std::vector<std::string> &args)
    : Name(name), Args(args) {}

};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
  PrototypeAST *Proto;
  ExprAST *Body;
public:
  FunctionAST(PrototypeAST *proto, ExprAST *body)
    : Proto(proto), Body(body) {}

};

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

/// thisToken/getNextToken - Provide a simple token buffer.  thisToken is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates thisToken with its results.
static int thisToken;
static int getNextToken() {
  return thisToken = getToken();
}

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
static std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (!isascii(thisToken))
    return -1;

  // Make sure it's a declared binop.
  int TokPrec = BinopPrecedence[thisToken];
  if (TokPrec <= 0) return -1;
  return TokPrec;
}

/// Error* - These are little helper functions for error handling.
ExprAST *Error(const char *Str) { fprintf(stderr, "Error: %s\n", Str);return 0;}
PrototypeAST *ErrorP(const char *Str) { Error(Str); return 0; }
FunctionAST *ErrorF(const char *Str) { Error(Str); return 0; }

static ExprAST *ParseExpression();

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static ExprAST *ParseIdentifierExpr() {
  std::string IdName = IdStr;

  getNextToken();  // eat identifier.

  if (thisToken != '(') // Simple variable ref.
    return new VariableExprAST(IdName);

  // Call.
  getNextToken();  // eat (
  std::vector<ExprAST*> Args;
  if (thisToken != ')') {
    while (1) {
      ExprAST *Arg = ParseExpression();
      if (!Arg) return 0;
      Args.push_back(Arg);

      if (thisToken == ')') break;

      if (thisToken != ',')
        return Error("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }

  // Eat the ')'.
  getNextToken();

  return new CallExprAST(IdName, Args);
}

/// numberexpr ::= number
static ExprAST *ParseNumberExpr() {
  ExprAST *Result = new NumberExprAST(NumVal);
  getNextToken(); // consume the number
  return Result;
}

/// parenexpr ::= '(' expression ')'
static ExprAST *ParseParenExpr() {
  getNextToken();  // eat (.
  ExprAST *V = ParseExpression();
  if (!V) return 0;

  if (thisToken != ')')
    return Error("expected ')'");
  getNextToken();  // eat ).
  return V;
}

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
static ExprAST *ParsePrimary() {
  switch (thisToken) {
  default: return Error("unknown token when expecting an expression");
  case TOK_ID: return ParseIdentifierExpr();
  case TOK_NUM:     return ParseNumberExpr();
  case '(':            return ParseParenExpr();
  }
}

/// binoprhs
///   ::= ('+' primary)*
static ExprAST *ParseBinOpRHS(int ExprPrec, ExprAST *LHS) {
  // If this is a binop, find its precedence.
  while (1) {
    int TokPrec = GetTokPrecedence();

    // If this is a binop that binds at least as tightly as the current binop,
    // consume it, otherwise we are done.
    if (TokPrec < ExprPrec)
      return LHS;

    // Okay, we know this is a binop.
    int BinOp = thisToken;
    getNextToken();  // eat binop

    // Parse the primary expression after the binary operator.
    ExprAST *RHS = ParsePrimary();
    if (!RHS) return 0;

    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec+1, RHS);
      if (RHS == 0) return 0;
    }

    // Merge LHS/RHS.
    LHS = new BinaryExprAST(BinOp, LHS, RHS);
  }
}

/// expression
///   ::= primary binoprhs
///
static ExprAST *ParseExpression() {
  ExprAST *LHS = ParsePrimary();
  if (!LHS) return 0;

  return ParseBinOpRHS(0, LHS);
}

/// prototype
///   ::= id '(' id* ')'
static PrototypeAST *ParsePrototype() {
  if (thisToken != TOK_ID)
    return ErrorP("Expected function name in prototype");

  std::string FnName = IdStr;
  getNextToken();

  if (thisToken != '(')
    return ErrorP("Expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == TOK_ID)
    ArgNames.push_back(IdStr);
  if (thisToken != ')')
    return ErrorP("Expected ')' in prototype");

  // success.
  getNextToken();  // eat ')'.

  return new PrototypeAST(FnName, ArgNames);
}

/// definition ::= 'def' prototype expression
static FunctionAST *parseVarDef() {
  getNextToken();  // eat def.
  PrototypeAST *Proto = ParsePrototype();
  if (Proto == 0) return 0;

  if (ExprAST *E = ParseExpression())
    return new FunctionAST(Proto, E);
  return 0;
}

/// toplevelexpr ::= expression
static FunctionAST *ParseTopLevelExpr() {
  if (ExprAST *E = ParseExpression()) {
    // Make an anonymous proto.
    PrototypeAST *Proto = new PrototypeAST("", std::vector<std::string>());
    return new FunctionAST(Proto, E);
  }
  return 0;
}

/// external ::= 'extern' prototype
static PrototypeAST *ParseExtern() {
  getNextToken();  // eat extern.
  return ParsePrototype();
}

//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//

static void handleVarDef() {
  // if (parseVarDef()) {
  //   fprintf(stderr, "Parsed a function definition.\n");
  // } else {
  //   // Skip token for error recovery.
  //   getNextToken();
  // }

  getNextToken();

  getNextToken();
}

static void handleDefault() {
  // // Evaluate a top-level expression into an anonymous function.
  // if (ParseTopLevelExpr()) {
  //   fprintf(stderr, "Parsed a top-level expr\n");
  // } else {
  //   // Skip token for error recovery.
  //   getNextToken();
  // }

  getNextToken();

  getNextToken();
}

/// top ::= definition | external | expression | ';'
static void interpret() {
  while (1) {
    fprintf(stderr, "[Styio]/> ");
    switch (thisToken) {
    case TOK_EOF:    return;
    case ';':        getNextToken(); break;  // ignore top-level semicolons.
    case TOK_VARDEF:    handleVarDef(); break;
    default:         handleDefault(); break;
    }
  }
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main() {
  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;  // highest.

  // Prime the first token.
  fprintf(stderr, "[Styio]/> ");
  getNextToken();

  // Run the main "interpreter loop" now.
  interpret();

  return 0;
}