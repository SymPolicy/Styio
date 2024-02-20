#pragma once
#ifndef STYIO_TOKEN_H_
#define STYIO_TOKEN_H_

#include <unordered_map>
#include <string>

enum class StyioDataType
{
  undefined,
  i1,
  i8,
  i16,
  i32,
  i64,
  i128,
  f32,
  f64
};

static std::unordered_map<std::string, StyioDataType> const DType_Table = {
  {"i32", StyioDataType::i32},
  {"i64", StyioDataType::i64},
  {"f64", StyioDataType::f64},
  {"i1", StyioDataType::i1},
  {"i8", StyioDataType::i8},
  {"i16", StyioDataType::i16},
  {"i128", StyioDataType::i128},
  {"f32", StyioDataType::f32}
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



enum class StyioNodeHint
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
  Var,
  Arg,

  Bool,
  // Integer (General)
  Int,
  // Float (General)
  Float,
  // Character: '<Any Single Character>'
  Char,

  NumConvert,

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

  // Binary Operation
  BinOp,
  Bin_Add,  // +
  Bin_Sub,  // -
  Bin_Mul,  // *
  Bin_Div,  // /
  Bin_Pow,  // **
  Bin_Mod,  // %
  Inc_Add,  // +=
  Inc_Sub,  // -=
  Inc_Mul,  // *=
  Inc_Div,  // /=

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
  FixBind,
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

inline std::string
make_colorful(std::string text, int color) {
  /*
    Red: 91
    Green: 92
    Orange: 93
    Blue: 94
    Magenta: 95
    Cyan: 96
  */
  return std::string("\033[1;") + std::to_string(color) + "m" + text + "\033[0m";
};

std::string
reprDataType(StyioDataType dtype);

std::string
reprNodeType(StyioNodeHint type, bool colorful = false, std::string extra = "");

std::string
reprToken(CompType token);

std::string
reprToken(LogicType token);

std::string
reprToken(StyioToken token);

#endif
