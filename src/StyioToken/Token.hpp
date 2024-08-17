#pragma once
#ifndef STYIO_TOKEN_H_
#define STYIO_TOKEN_H_

#include <string>
#include <unordered_map>

enum class StyioDataTypeOption
{
  Undefined,
  Defined,

  Bool,  // Boolean
  Integer,
  Float,
  Decimal,

  Char,  // Character
  String,

  Tuple,
  List,

  Struct,

  Func,  // Function
};

struct StyioDataType
{
  StyioDataTypeOption option;
  std::string name;
  size_t num_of_bit = 0;

  bool isUndefined() {
    return option == StyioDataTypeOption::Undefined;
  }

  bool isInteger() {
    return option == StyioDataTypeOption::Integer;
  }

  bool isFloat() {
    return option == StyioDataTypeOption::Float;
  }

  bool equals(const StyioDataType other) const {
    return option == other.option and name == other.name;
  }
};

static std::unordered_map<std::string, StyioDataType> const DType_Table = {
  {"bool", StyioDataType{StyioDataTypeOption::Bool, "bool", 1}},

  {"int", StyioDataType{StyioDataTypeOption::Integer, "i32", 32}},
  {"long", StyioDataType{StyioDataTypeOption::Integer, "i64", 64}},

  {"i1", StyioDataType{StyioDataTypeOption::Integer, "i1", 1}},
  {"i8", StyioDataType{StyioDataTypeOption::Integer, "i8", 8}},
  {"i16", StyioDataType{StyioDataTypeOption::Integer, "i16", 16}},
  {"i32", StyioDataType{StyioDataTypeOption::Integer, "i32", 32}},
  {"i64", StyioDataType{StyioDataTypeOption::Integer, "i64", 64}},
  {"i128", StyioDataType{StyioDataTypeOption::Integer, "i128", 128}},

  {"float", StyioDataType{StyioDataTypeOption::Float, "f32", 32}},
  {"double", StyioDataType{StyioDataTypeOption::Float, "f64", 64}},

  {"f32", StyioDataType{StyioDataTypeOption::Float, "f64", 32}},
  {"f64", StyioDataType{StyioDataTypeOption::Float, "f64", 64}},

  {"char", StyioDataType{StyioDataTypeOption::Char, "char", 0}},

  {"string", StyioDataType{StyioDataTypeOption::String, "string", 0}},
  {"str", StyioDataType{StyioDataTypeOption::String, "string", 0}},
};

StyioDataType getMaxType(StyioDataType T1, StyioDataType T2);

std::string reprDataTypeOption(StyioDataTypeOption option);

enum class TokenKind
{
  Undefined,            // Undefined
  End_Of_File,          // EOF
  Unary_Positive,       // + a
  Unary_Negative,       // - a
  Binary_Add,           // a + b
  Binary_Sub,           // a - b
  Binary_Mul,           // a * b
  Binary_Div,           // a / b
  Binary_Pow,           // a ** b
  Binary_Mod,           // a % b
  Greater_Than,         // a > b
  Less_Than,            // a < b
  Greater_Than_Equal,   // a >= b
  Less_Than_Equal,      // a <= b
  Self_Add_Assign,      // a += b
  Self_Sub_Assign,      // a -= b
  Self_Mul_Assign,      // a *= b
  Self_Div_Assign,      // a /= b
  Bitwise_NOT,          // ~ a
  Bitwise_AND,          // a & b
  Bitwise_OR,           // a | b
  Bitwise_XOR,          // a ^ b
  Bitwise_Left_Shift,   // shl(x, y)
  Bitwise_Right_Shift,  // shr(x, y)
  Logic_NOT,            // ! a
  Logic_AND,            // a && b
  Logic_XOR,            // a ⊕ b
  Logic_OR,             // a || b
  Equal,                // a == b
  Not_Equal,            // a != b
  If_Else_Flow,         // ?() => a : b

  Iterate,  // a >> b
  Extract,  // a << b
  Next,     // a => b

  Comment_SingleLine,  // // Like This
  Comment_MultiLine,   // /* Like This */
};

/* Token Precedence Map */
static std::unordered_map<TokenKind, int> const TokenPrecedenceMap = {
  {TokenKind::Unary_Positive, 999},  // + a
  {TokenKind::Unary_Negative, 999},  // - a
  {TokenKind::Bitwise_NOT, 999},     // ~ a
  {TokenKind::Logic_NOT, 999},       // ! a

  {TokenKind::Binary_Pow, 704},  // a ** b

  {TokenKind::Binary_Mul, 703},  // a * b
  {TokenKind::Binary_Div, 703},  // a / b
  {TokenKind::Binary_Mod, 703},  // a % b

  {TokenKind::Binary_Add, 702},  // a + b
  {TokenKind::Binary_Sub, 702},  // a - b

  {TokenKind::Bitwise_Left_Shift, 701},   // shl(x, y)
  {TokenKind::Bitwise_Right_Shift, 701},  // shr(x, y)

  {TokenKind::Greater_Than, 502},        // a > b
  {TokenKind::Less_Than, 502},           // a < b
  {TokenKind::Greater_Than_Equal, 502},  // a >= b
  {TokenKind::Less_Than_Equal, 502},     // a <= b

  {TokenKind::Equal, 501},      // a == b
  {TokenKind::Not_Equal, 501},  // a != b

  {TokenKind::Bitwise_AND, 303},  // a & b
  {TokenKind::Bitwise_XOR, 302},  // a ^ b
  {TokenKind::Bitwise_OR, 301},   // a | b

  {TokenKind::Logic_AND, 203},  // a && b
  {TokenKind::Logic_XOR, 202},  // a ⊕ b
  {TokenKind::Logic_OR, 201},   // a || b

  {TokenKind::If_Else_Flow, 101},  // ?() => a : b

  {TokenKind::Self_Add_Assign, 1},  // a += b
  {TokenKind::Self_Sub_Assign, 1},  // a -= b
  {TokenKind::Self_Mul_Assign, 1},  // a *= b
  {TokenKind::Self_Div_Assign, 1},  // a /= b

  {TokenKind::Undefined, 0},    // Undefined
  {TokenKind::End_Of_File, 0},  // Undefined
};

static std::unordered_map<TokenKind, std::string> const TokenStrMap = {
  {TokenKind::Undefined, "undefined"},  // undefined
  {TokenKind::End_Of_File, "EOF"},      // EOF

  {TokenKind::Binary_Pow, "**"},  // a ** b

  {TokenKind::Binary_Mul, "*"},  // a * b
  {TokenKind::Binary_Div, "/"},  // a / b
  {TokenKind::Binary_Mod, "%"},  // a % b

  {TokenKind::Binary_Add, "+"},  // a + b
  {TokenKind::Binary_Sub, "-"},  // a - b

  {TokenKind::Greater_Than, ">"},         // a > b
  {TokenKind::Less_Than, "<"},            // a < b
  {TokenKind::Greater_Than_Equal, ">="},  // a >= b
  {TokenKind::Less_Than_Equal, "<="},     // a <= b

  {TokenKind::Equal, "=="},      // a == b
  {TokenKind::Not_Equal, "!="},  // a != b

  {TokenKind::Bitwise_AND, "&"},  // a & b
  {TokenKind::Bitwise_XOR, "^"},  // a ^ b
  {TokenKind::Bitwise_OR, "|"},   // a | b

  {TokenKind::Logic_AND, "&&"},  // a && b
  {TokenKind::Logic_XOR, "⊕"},   // a ⊕ b
  {TokenKind::Logic_OR, "||"},   // a || b

  {TokenKind::Self_Add_Assign, "+="},  // a += b
  {TokenKind::Self_Sub_Assign, "-="},  // a -= b
  {TokenKind::Self_Mul_Assign, "*="},  // a *= b
  {TokenKind::Self_Div_Assign, "/="},  // a /= b
};

static std::unordered_map<std::string, TokenKind> const StrTokenMap = {
  {"", TokenKind::Undefined},       // Undefined
  {"EOF", TokenKind::End_Of_File},  // EOF

  {"**", TokenKind::Binary_Pow},  // a ** b

  {"*", TokenKind::Binary_Mul},  // a * b
  {"/", TokenKind::Binary_Div},  // a / b
  {"%", TokenKind::Binary_Mod},  // a % b

  {"+", TokenKind::Binary_Add},  // a + b
  {"-", TokenKind::Binary_Sub},  // a - b

  {">", TokenKind::Greater_Than},         // a > b
  {"<", TokenKind::Less_Than},            // a < b
  {">=", TokenKind::Greater_Than_Equal},  // a >= b
  {"<=", TokenKind::Less_Than_Equal},     // a <= b

  {"==", TokenKind::Equal},      // a == b
  {"!=", TokenKind::Not_Equal},  // a != b

  {"&", TokenKind::Bitwise_AND},  // a & b
  {"^", TokenKind::Bitwise_XOR},  // a ^ b
  {"|", TokenKind::Bitwise_OR},   // a | b

  {"&&", TokenKind::Logic_AND},  // a && b
  {"⊕", TokenKind::Logic_XOR},   // a ⊕ b
  {"||", TokenKind::Logic_OR},   // a || b

  {"+=", TokenKind::Self_Add_Assign},  // a += b
  {"-=", TokenKind::Self_Sub_Assign},  // a -= b
  {"*=", TokenKind::Self_Mul_Assign},  // a *= b
  {"/=", TokenKind::Self_Div_Assign},  // a /= b
};

enum class StyioContextType
{
};

enum class StyioPathType
{
  local_absolute_unix_like,
  local_absolute_windows,
  local_relevant_any,

  ipv4_addr,
  ipv6_addr,

  url_localhost,
  url_http,
  url_https,
  url_ftp,

  db_mysql,
  db_postgresql,
  db_mongo,

  remote_windows
};

enum class StyioASTType
{
  End,
  Pass,
  Break,
  Return,
  Comment,

  Naive,

  True,
  False,

  /* -----------------
   * None, Null, Empty
   */
  None,
  Empty,
  EmptyBlock,

  // -----------------

  /* -----------------
   * Basic Type
   */

  // Identifier: [a-zA-Z0-9_]
  Id,
  DType,
  TypedVar,
  OptArg,
  OptKwArg,
  Variable,
  Arg,

  Bool,
  // Integer (General)
  Integer,
  // Float (General)
  Float,
  // Character: '<Any Single Character>'
  Char,

  NumConvert,

  // Binary Operation
  BinOp,

  // -----------------

  /* -----------------
   * External Resource Identifier
   */

  // File Path
  LocalPath,
  RemotePath,
  WebUrl,
  DBUrl,

  // Package
  ExtPack,

  // -----------------

  /* -----------------
   * Collection
   */

  // ""
  String,
  // $""
  FmtStr,
  // [a0, a1, ..., an]
  List,
  Tuple,
  Set,
  // [start .. end]
  Range,

  // -----------------

  /* -----------------
   * Basic Operation
   */

  // Not
  Not,

  // Compare
  Compare,

  // Condition
  Condition,

  // Call
  Call,

  // Attribute
  Attribute,

  // Conditionals
  CondFlow_True,
  CondFlow_False,
  CondFlow_Both,

  // List Operation
  Access,           // [id]
  Access_By_Index,  // [index]
  Access_By_Name,   // ["name"]

  Get_Index_By_Value,          // [?= value]
  Get_Indices_By_Many_Values,  // [?^ values]

  Append_Value,          // [+: value]
  Insert_Item_By_Index,  // [+: index <- value]

  Remove_Last_Item,              // [-: ^-1]
  Remove_Item_By_Index,          // [-: index]
  Remove_Items_By_Many_Indices,  // [-: (i0, i1, ...)]
  Remove_Item_By_Value,          // [-: ?= value]
  Remove_Items_By_Many_Values,   // [-: ?^ (v0, v1, ...)]

  Get_Reversed,                  // [<]
  Get_Index_By_Item_From_Right,  // [[<] ?= value]
  // -----------------

  /* -----------------
   * Basic Util
   */

  // Get the Size / Length / .. of A Collection
  SizeOf,
  // -----------------

  /* -----------------
   * Variable Definition
   */

  // @
  Resources,
  // -----------------

  /* -----------------
   * Variable Assignment
   */

  // =
  MutBind,
  // :=
  FinalBind,
  // -----------------

  /* -----------------
   * Pipeline
   */

  Func,
  AnonyFunc,
  MatchCases,
  Struct,
  Eval,
  // -----------------

  /* -----------------
   * Control Flow: Loop
   */
  Infinite,
  // -----------------

  /* -----------------
   * Iterator
   */
  Loop,
  Iterator,
  // -----------------

  /* -----------------
   * Combination
   */
  IterWithMatch,
  // -----------------

  /* -----------------
   * Read
   */

  ReadFile,
  // -----------------

  /* -----------------
   * Write
   */

  Print,
  // -----------------

  /* -----------------
   * Layers
   */
  // (x, y, ...)
  VarTuple,
  // ?=
  CheckEq,
  // ?^
  CheckIsin,
  // ?()
  CheckCond,

  // Intermediate Connection Between Scopes
  Forward,
  If_Equal_To_Forward,
  If_Is_In_Forward,
  Cases_Forward,
  If_True_Forward,
  If_False_Forward,
  If_Both_Forward,

  Fill_Forward,
  Fill_If_Equal_To_Forward,
  Fill_If_Is_in_Forward,
  Fill_Cases_Forward,
  Fill_If_True_Forward,
  Fill_If_False_Forward,
  Fill_If_Both_Forward,
  // -----------------

  /* -----------------
   * Backward
   */

  Backward,

  // -----------------

  /* -----------------
   * Tuple Operations
   */

  TupleOperation,

  // -----------------

  /* -----------------
   * Chain of Data Processing
   */

  Chain_Of_Data_Processing,

  // -----------------

  /* -----------------
   * Code Block
   */

  MainBlock,
  Block,
  Cases,
  // -----------------

  Connection,
  FromTo
};

enum class InfiniteType
{
  Original,
  Incremental,
};

enum class IteratorType
{
  Original,
  WithLayer,
};

enum class LogicType
{
  RAW,
  NOT,
  AND,
  OR,
  XOR,
};

enum class CompType
{
  EQ,  // == Equal
  GT,  // >  Greater Than
  GE,  // >= Greater Than and Equal
  LT,  // <  Less Than
  LE,  // <= Less Than and Equal
  NE,  // != Not Equal
};

enum class IterOverWhat
{
  /*
   * Accept: 0 [No Variable]
   */
  InfLoop,  // [...]

  /*
   * Accept: 1 [Only One Variable]
   */
  List,   // [a0, a1, ..., an]
  Range,  // [a0...an]

  /*
   * Accept: 2 [Two Variables]
   */
  Dict,  // {k0: v0, k1: v1, kn: vn}

  /*
   * Accept: n [Any]
   */
  ListOfTuples,   // [(a0, b0, ...), (a1, b1, ...), ..., (an, bn, ...)]
  ListOfStructs,  // [s0, s1, ..., sn]
};

enum class NumPromoTy
{
  Bool_To_Int,
  Int_To_Float,
};

std::string
reprASTType(StyioASTType type, std::string extra = "");

std::string
reprToken(CompType token);

std::string
reprToken(TokenKind token);

std::string
reprToken(LogicType token);

enum class StyioTokenType
{
  TOK_EOF = -1,        // EOF
  TOK_NULL = 0,        // ASCII 0 NUL
  TOK_LF = 10,         // ASCII 10 LF
  TOK_CR = 13,         // ASCII 13 CR
  TOK_SPACE = 32,      // ASCII 32 SPACE
  TOK_EXCLAM = 33,     // ASCII 33 !
  TOK_DQUOTE = 34,     // ASCII 34 "
  TOK_HASH = 35,       // ASCII 35 #
  TOK_DOLLAR = 36,     // ASCII 36 $
  TOK_PERCENT = 37,    // ASCII 37 %
  TOK_AMP = 38,        // ASCII 38 &
  TOK_SQUOTE = 39,     // ASCII 39 '
  TOK_LPAREN = 40,     // ASCII 40 (
  TOK_RPAREN = 41,     // ASCII 41 )
  TOK_STAR = 42,       // ASCII 42 *
  TOK_PLUS = 43,       // ASCII 43 +
  TOK_COMMA = 44,      // ASCII 44 ,
  TOK_MINUS = 45,      // ASCII 45 -
  TOK_DOT = 46,        // ASCII 46 .
  TOK_SLASH = 47,      // ASCII 47 / (slash)
  TOK_COLON = 58,      // ASCII 58 :
  TOK_SEMICOLON = 59,  // ASCII 59 ;
  TOK_LANGBRAC = 60,   // ASCII 60 <
  TOK_EQUAL = 61,      // ASCII 61 =
  TOK_RANGBRAC = 62,   // ASCII 62 >
  TOK_QUESTION = 63,   // ASCII 63 ?
  TOK_AT = 64,         // ASCII 64 @
  TOK_LBOXBRAC = 91,   // [
  TOK_BACKSLASH = 92,  // ASCII 92 \ (backslash)
  TOK_RBOXBRAC = 93,   // ]
  TOK_HAT = 94,        // ASCII 94 ^
  TOK_UNDLINE = 95,    // ASCII 95 _
  TOK_BQUOTE = 96,     // ASCII 96 `
  TOK_LCURBRAC = 123,  // ASCII 123 {
  TOK_PIPE = 124,      // ASCII 124 |
  TOK_RCURBRAC = 125,  // ASCII 125 }
  TOK_TILDE = 126,     // ASCII 126 ~
  TOK_DEL = 127,       // ASCII 127 DEL

  TOK_NAME,
  TOK_INT,
  TOK_FLOAT,
  TOK_STRING,
  TOK_LINE_COMMENT,
  TOK_CLOSED_COMMENT,

  TOK_AND,  // &&
  TOK_OR,   // ||
  TOK_XOR,  // ^

  TOK_BITAND,  // &
  TOK_BITOR,   // |
  TOK_BITXOR,  // ^

  TOK_BACKWARD,  // <<
  TOK_FORWARD,   // >>

  TOK_NOT,  // !

  TOK_NEG,  // -

  TOK_ADD,  // +
  TOK_SUB,  // -
  TOK_MUL,  // *
  TOK_DIV,  // /
  TOK_MOD,  // %
  TOK_POW,  // **

  TOK_GT,  // >
  TOK_GE,  // >=
  TOK_LT,  // <
  TOK_LE,  // <=
  TOK_EQ,  // ==
  TOK_NE,  // !=

  TOK_RARROW,  // ->
  TOK_LARROW,  // <-

  TOK_TERMINAL,  // >_
  TOK_WALRUS,    // :=
  TOK_MATCH,     // ?=

  TOK_ELLIPSIS,  // ...

  TOK_INFINITE_LIST,  // [...]

  TOK_UNKNOWN,
};

class StyioToken
{
private:
  StyioToken(StyioTokenType token_type, std::string token_literal) :
      type(token_type), literal(token_literal) {
  }

public:
  StyioTokenType type;
  std::string literal;

  static StyioToken* Create(StyioTokenType token_type) {
    return new StyioToken(token_type, "");
  }

  static StyioToken* Create(StyioTokenType token_type, std::string token_literal) {
    return new StyioToken(token_type, token_literal);
  }

  static std::string getTokName(StyioTokenType type) {
    switch (type) {
      case StyioTokenType::TOK_SPACE:
        return " ";

      case StyioTokenType::TOK_CR:
        return "<CR>";

      case StyioTokenType::TOK_LF:
        return "<LF>";

      case StyioTokenType::TOK_EOF:
        return "<EOF>";

      case StyioTokenType::TOK_NAME:
        return "<ID>";

      case StyioTokenType::TOK_INT:
        return "<INT>";

      case StyioTokenType::TOK_FLOAT:
        return "<FLOAT>";

      case StyioTokenType::TOK_STRING:
        return "<STRING>";

      case StyioTokenType::TOK_LINE_COMMENT:
        return "// COMMENT";

      case StyioTokenType::TOK_CLOSED_COMMENT:
        return "/* COMMENT */";

      case StyioTokenType::TOK_COMMA:
        return ",";

      case StyioTokenType::TOK_DOT:
        return ".";

      case StyioTokenType::TOK_COLON:
        return ":";

      case StyioTokenType::TOK_TILDE:
        return "~";

      case StyioTokenType::TOK_EXCLAM:
        return "!";

      case StyioTokenType::TOK_AT:
        return "@";

      case StyioTokenType::TOK_HASH:
        return "#";

      case StyioTokenType::TOK_DOLLAR:
        return "$";

      case StyioTokenType::TOK_PERCENT:
        return "%";

      case StyioTokenType::TOK_HAT:
        return "^";

      case StyioTokenType::TOK_QUESTION:
        return "?";

      case StyioTokenType::TOK_SLASH:
        return "/";

      case StyioTokenType::TOK_BACKSLASH:
        return "\\";

      case StyioTokenType::TOK_PIPE:
        return "|";

      case StyioTokenType::TOK_ELLIPSIS:
        return "...";

      case StyioTokenType::TOK_SQUOTE:
        return "'";

      case StyioTokenType::TOK_DQUOTE:
        return "\"";

      case StyioTokenType::TOK_BQUOTE:
        return "`";

      case StyioTokenType::TOK_LPAREN:
        return "(";

      case StyioTokenType::TOK_RPAREN:
        return ")";

      case StyioTokenType::TOK_LBOXBRAC:
        return "[";

      case StyioTokenType::TOK_RBOXBRAC:
        return "]";

      case StyioTokenType::TOK_LCURBRAC:
        return "{";

      case StyioTokenType::TOK_RCURBRAC:
        return "}";

      case StyioTokenType::TOK_LANGBRAC:
        return "<";

      case StyioTokenType::TOK_RANGBRAC:
        return ">";

      case StyioTokenType::TOK_NOT:
        return "<NOT>";

      case StyioTokenType::TOK_AND:
        return "<AND>";

      case StyioTokenType::TOK_OR:
        return "<OR>";

      case StyioTokenType::TOK_XOR:
        return "<XOR>";

      case StyioTokenType::TOK_BITAND:
        return "<BIT_AND>";

      case StyioTokenType::TOK_BITOR:
        return "<BIT_OR>";

      case StyioTokenType::TOK_BITXOR:
        return "<BIT_XOR>";

      case StyioTokenType::TOK_BACKWARD:
        return "<<";

      case StyioTokenType::TOK_FORWARD:
        return ">>";

      case StyioTokenType::TOK_TERMINAL:
        return ">_";

      case StyioTokenType::TOK_NEG:
        return "<NEG>";

      case StyioTokenType::TOK_ADD:
        return "<ADD>";

      case StyioTokenType::TOK_SUB:
        return "<SUB>";

      case StyioTokenType::TOK_MUL:
        return "<MUL>";

      case StyioTokenType::TOK_DIV:
        return "<DIV>";

      case StyioTokenType::TOK_MOD:
        return "<MOD>";

      case StyioTokenType::TOK_POW:
        return "<POW>";

      case StyioTokenType::TOK_GT:
        return "<GT>";

      case StyioTokenType::TOK_GE:
        return "<GE>";

      case StyioTokenType::TOK_LT:
        return "<LT>";

      case StyioTokenType::TOK_LE:
        return "<LE>";

      case StyioTokenType::TOK_EQ:
        return "<EQ>";

      case StyioTokenType::TOK_NE:
        return "<NE>";

      case StyioTokenType::TOK_RARROW:
        return "->";

      case StyioTokenType::TOK_LARROW:
        return "<-";

      case StyioTokenType::TOK_WALRUS:
        return ":=";

      case StyioTokenType::TOK_MATCH:
        return "?=";

      case StyioTokenType::TOK_INFINITE_LIST:
        return "[...]";

      default:
        return "<UNKNOWN>";
    }
  };

  size_t length() {
    switch (type) {
      case StyioTokenType::TOK_SPACE:
        return 1;

      case StyioTokenType::TOK_CR:
        return 1;

      case StyioTokenType::TOK_LF:
        return 1;

      case StyioTokenType::TOK_EOF:
        return 1;

      case StyioTokenType::TOK_NAME:
        return literal.length();

      case StyioTokenType::TOK_INT:
        return literal.length();

      case StyioTokenType::TOK_FLOAT:
        return literal.length();

      case StyioTokenType::TOK_STRING:
        return literal.length() + 2;

      case StyioTokenType::TOK_LINE_COMMENT:
        return literal.length() + 2;

      case StyioTokenType::TOK_CLOSED_COMMENT:
        return literal.length() + 4;

      case StyioTokenType::TOK_COMMA:
        return 1;

      case StyioTokenType::TOK_DOT:
        return 1;

      case StyioTokenType::TOK_COLON:
        return 1;

      case StyioTokenType::TOK_TILDE:
        return 1;

      case StyioTokenType::TOK_EXCLAM:
        return 1;

      case StyioTokenType::TOK_AT:
        return 1;

      case StyioTokenType::TOK_HASH:
        return 1;

      case StyioTokenType::TOK_DOLLAR:
        return 1;

      case StyioTokenType::TOK_PERCENT:
        return 1;

      case StyioTokenType::TOK_HAT:
        return 1;

      case StyioTokenType::TOK_QUESTION:
        return 1;

      case StyioTokenType::TOK_SLASH:
        return 1;

      case StyioTokenType::TOK_BACKSLASH:
        return 1;

      case StyioTokenType::TOK_PIPE:
        return 1;

      case StyioTokenType::TOK_ELLIPSIS:
        return 3;

      case StyioTokenType::TOK_SQUOTE:
        return 1;

      case StyioTokenType::TOK_DQUOTE:
        return 1;

      case StyioTokenType::TOK_BQUOTE:
        return 1;

      case StyioTokenType::TOK_LPAREN:
        return 1;

      case StyioTokenType::TOK_RPAREN:
        return 1;

      case StyioTokenType::TOK_LBOXBRAC:
        return 1;

      case StyioTokenType::TOK_RBOXBRAC:
        return 1;

      case StyioTokenType::TOK_LCURBRAC:
        return 1;

      case StyioTokenType::TOK_RCURBRAC:
        return 1;

      case StyioTokenType::TOK_LANGBRAC:
        return 1;

      case StyioTokenType::TOK_RANGBRAC:
        return 1;

      case StyioTokenType::TOK_NOT:
        return 1;

      case StyioTokenType::TOK_AND:
        return 2;

      case StyioTokenType::TOK_OR:
        return 2;

      case StyioTokenType::TOK_XOR:
        return 1;

      case StyioTokenType::TOK_BITAND:
        return 1;

      case StyioTokenType::TOK_BITOR:
        return 1;

      case StyioTokenType::TOK_BITXOR:
        return 1;

      case StyioTokenType::TOK_BACKWARD:
        return 2;

      case StyioTokenType::TOK_FORWARD:
        return 2;

      case StyioTokenType::TOK_TERMINAL:
        return 2;

      case StyioTokenType::TOK_NEG:
        return 1;

      case StyioTokenType::TOK_ADD:
        return 1;

      case StyioTokenType::TOK_SUB:
        return 1;

      case StyioTokenType::TOK_MUL:
        return 1;

      case StyioTokenType::TOK_DIV:
        return 1;

      case StyioTokenType::TOK_MOD:
        return 1;

      case StyioTokenType::TOK_POW:
        return 2;

      case StyioTokenType::TOK_GT:
        return 1;

      case StyioTokenType::TOK_GE:
        return 2;

      case StyioTokenType::TOK_LT:
        return 1;

      case StyioTokenType::TOK_LE:
        return 2;

      case StyioTokenType::TOK_EQ:
        return 2;

      case StyioTokenType::TOK_NE:
        return 2;

      case StyioTokenType::TOK_RARROW:
        return 2;

      case StyioTokenType::TOK_LARROW:
        return 2;

      case StyioTokenType::TOK_WALRUS:
        return 2;

      case StyioTokenType::TOK_MATCH:
        return 2;

      case StyioTokenType::TOK_INFINITE_LIST:
        return 5;

      default:
        return 0;
    }
  }

  std::string as_str() {
    if (type == StyioTokenType::TOK_LF) {
      return std::string(" ║ ") + getTokName(this->type) + "\n";
    }
    else if (type == StyioTokenType::TOK_NAME) {
      return std::string(" ║ ") + this->literal;
    }
    else if (type == StyioTokenType::TOK_NAME
             || type == StyioTokenType::TOK_INT
             || type == StyioTokenType::TOK_FLOAT) {
      return std::string(" ║ ") + getTokName(this->type) + ": " + this->literal;
    }
    else if (type == StyioTokenType::TOK_STRING) {
      return std::string(" ║ ") + "\"" + this->literal + "\"";
    }
    else {
      return std::string(" ║ ") + getTokName(this->type);
    }
  }
};

#endif
