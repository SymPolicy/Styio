#ifndef STYIO_TOKEN_H_
#define STYIO_TOKEN_H_

enum class StyioType {
  End,
  Pass,
  Return,
  Comment,

  /* -----------------
   * None, Null, Empty
   */
  None,
  EmptyList,
  EmptyBlock,

  // -----------------

  /* -----------------
   * Basic Type
   */

  // Identifier: [a-zA-Z0-9_]
  Id,
  // Integer (General)
  Int,
  // Float (General)
  Float,
  // Character: '<Any Single Character>'
  Char,

  // -----------------

  /* -----------------
   * External Resource Identifier
   */

  // File Path
  ExtPath,
  // Web Link
  ExtLink,
  // Package
  ExtPack,

  // -----------------

  /* -----------------
   * Collection
   */

  // "   "
  String,
  // [a0, a1, ..., an]
  List,
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

  // List Operation
  ListOp,
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
  MutAssign,
  // := 
  FixAssign,
  // -----------------

  /* -----------------
   * Pipeline
   */

  Function,
  Structure,
  Evaluation,
  // -----------------

  /* -----------------
   * Control Flow: Loop
   */
  InfLoop,
  // -----------------

  /* -----------------
   * Iterator
   */
  Loop,
  IterBounded,
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

  WriteStdOut,
  // -----------------

  /* -----------------
   * Layers
   */
  // (x, y, ...)
  Filling,
  // ?=
  CheckEq,
  // ?^
  CheckIsin,
  // ?()
  CheckCond,

  // Intermediate Connection Between Scopes
  Forward,
  Forward_Run,
  Forward_Filling,
  Forward_MatchValue,
  Forward_MatchCases,
  Forward_CheckIsin,
  Forward_CheckCond_True,
  Forward_CheckCond_False,
  // -----------------

  /* -----------------
   * Code Block
   */

  Block,
  Cases,
  // -----------------

  CondFlow,
  Connection
};

enum class InfiniteType {
  Original,
  Incremental,
};

enum class IteratorType {
  Original,
  WithLayer,
};

enum class BinOpType {
  BIN_ADD, // +
  BIN_SUB, // -
  BIN_MUL, // *
  BIN_DIV, // /
  BIN_POW, // **
  BIN_MOD, // %
};

enum class LogicType {
  RAW,
  NOT,
  AND,
  OR,
  XOR,
};

enum class CompType {
  EQ, // == Equal
  GT, // >  Greater Than
  GE, // >= Greater Than and Equal
  LT, // <  Less Than
  LE, // <= Less Than and Equal
  NE, // != Not Equal
};

enum class FlowType {
  True,
  False,
  Both,
};

enum class ListOpType {
  Access_Via_Name, // ["name"]
  Access_Via_Index, // [index]

  Get_Index_By_Item, // [?= value]

  Insert_Item_By_Index, // [+: index <- value]

  Remove_Item_By_Index, // [-: index] 
  Remove_Many_Items_By_Indices, // [-: (i0, i1, ...)]
  Remove_Item_By_Value, // [-: ?= value]
  Remove_Many_Items_By_Values, // [-: ?^ (v0, v1, ...)]

  Get_Reversed, // [<]
  Get_Index_By_Item_From_Right, // [[<] ?= value]
  Remove_Item_By_Value_From_Right, // [[<] -: ?= value]
  Remove_Many_Items_By_Values_From_Right, // [[<] -: ?^ (v0, v1, ...)]
};

enum class IterOverWhat {
  /*
   * Accept: 0 [No Variable]
   */
  InfLoop, // [...]

  /*
   * Accept: 1 [Only One Variable]
   */
  List, // [a0, a1, ..., an]
  Range, // [a0...an]

  /*
   * Accept: 2 [Two Variables]
   */
  Dict, // {k0: v0, k1: v1, kn: vn}

  /*
   * Accept: n [Any]
   */
  ListOfTuples, // [(a0, b0, ...), (a1, b1, ...), ..., (an, bn, ...)]
  ListOfStructs, // [s0, s1, ..., sn]
};

enum class StyioToken {
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

  TOK_STDOUT, // >_
  TOK_WALRUS, // :=
  TOK_MATCH, // ?=

  TOK_ELLIPSIS, // ...

  TOK_INFINITE_LIST, // [...]
};

inline std::string make_colorful(std::string text, int color)
{
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

std::string reprStyioType (
  StyioType type,
  bool colorful = false,
  std::string extra = "");

std::string reprListOp(ListOpType listOp);

std::string reprFlow (FlowType flow);

std::string reprToken(CompType token);

std::string reprToken(LogicType token);

std::string reprToken(BinOpType token);

std::string reprToken(StyioToken token);

#endif
