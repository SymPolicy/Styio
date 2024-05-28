#pragma once
#ifndef STYIO_TOKEN_H_
#define STYIO_TOKEN_H_

#include <string>
#include <unordered_map>

enum class StyioDataTypeOption {
  Undefined,
  Defined,

  Bool, // Boolean
  Integer,
  Float,
  Decimal,

  Char, // Character
  String,

  Tuple,
  List,
  
  Struct,

  Func, // Function
};

struct StyioDataType {
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
  {"bool", StyioDataType{StyioDataTypeOption::Bool, "bool", 1} },
  
  {"int", StyioDataType{StyioDataTypeOption::Integer, "i32", 32} },
  {"long", StyioDataType{StyioDataTypeOption::Integer, "i64", 64} },

  {"i1", StyioDataType{StyioDataTypeOption::Integer, "i1", 1} },
  {"i8", StyioDataType{StyioDataTypeOption::Integer, "i8", 8} },
  {"i16", StyioDataType{StyioDataTypeOption::Integer, "i16", 16} },
  {"i32", StyioDataType{StyioDataTypeOption::Integer, "i32", 32} },
  {"i64", StyioDataType{StyioDataTypeOption::Integer, "i64", 64} },
  {"i128", StyioDataType{StyioDataTypeOption::Integer, "i128", 128} },

  {"float", StyioDataType{StyioDataTypeOption::Float, "f32", 32} },
  {"double", StyioDataType{StyioDataTypeOption::Float, "f64", 64} },

  {"f32", StyioDataType{StyioDataTypeOption::Float, "f64", 32} },
  {"f64", StyioDataType{StyioDataTypeOption::Float, "f64", 64} },

  {"char", StyioDataType{StyioDataTypeOption::Char, "char", 0} },

  {"string", StyioDataType{StyioDataTypeOption::String, "string", 0} },
  {"str", StyioDataType{StyioDataTypeOption::String, "string", 0} },
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

  Comment_SingleLine, // Like This
  Comment_MultiLine, /* Like This */
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

  {TokenKind::Undefined, 0},  // Undefined
  {TokenKind::End_Of_File, 0},  // Undefined
};

static std::unordered_map<TokenKind, std::string> const TokenStrMap = {
  {TokenKind::Undefined, "undefined"},  // undefined
  {TokenKind::End_Of_File, "EOF"},  // EOF

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
  {"", TokenKind::Undefined},  // Undefined
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

enum class StyioToken
{
  TOK_EOF = -1,        // EOF
  TOK_NULL = 0,        // ASCII 0 NUL
  TOK_LF = 10,         // ASCII 10 LF
  TOK_CR = 13,         // ASCII 13 CR
  TOK_SPACE = 32,      // ASCII 32 SPACE
  TOK_EXCLAM = 33,     // ASCII 33 !
  TOK_DQUOTE = 34,     // ASCII 24 "
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
  TOK_RANGBRAC = 62,   // ASCII 62 >
  TOK_CHECK = 63,      // ASCII 63 ?
  TOK_AT = 64,         // ASCII 64 @
  TOK_LBOXBRAC = 91,   // [
  TOK_BSLASH = 92,     // ASCII 92 \ (backslash)
  TOK_RBOXBRAC = 93,   // ]
  TOK_HAT = 94,        // ASCII 94 ^
  TOK_UNDLINE = 95,    // ASCII 95 _
  TOK_BQUOTE = 96,     // ASCII 96 `
  TOK_LCURBRAC = 123,  // ASCII 123 {
  TOK_PIPE = 124,      // ASCII 124 |
  TOK_RCURBRAC = 125,  // ASCII 125 }
  TOK_TILDE = 126,     // ASCII 126 ~
  TOK_DEL = 127,       // ASCII 127 DEL

  TOK_ID,
  TOK_INT,
  TOK_FLOAT,
  TOK_STRING,

  TOK_AND,  // &&
  TOK_OR,   // ||
  TOK_XOR,  // ^

  TOK_BITAND,  // &
  TOK_BITOR,   // |
  TOK_BITXOR,  // ^

  TOK_LSHIFT,  // <<
  TOK_RSHIFT,  // >>

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

  TOK_STDOUT,  // >_
  TOK_WALRUS,  // :=
  TOK_MATCH,   // ?=

  TOK_ELLIPSIS,  // ...

  TOK_INFINITE_LIST,  // [...]
};

std::string
reprASTType(StyioASTType type, std::string extra = "");

std::string
reprToken(CompType token);

std::string
reprToken(TokenKind token);

std::string
reprToken(LogicType token);

std::string
reprToken(StyioToken token);

#endif
