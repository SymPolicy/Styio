#ifndef STYIO_TOKEN_H_
#define STYIO_TOKEN_H_

enum class StyioType {
  /*
   * None, Null, Empty
   */

  None,
  EmptyList,

  // -----------------

  /*
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

  /*
   * External Resource Identifier
   */

  // File Path
  ExtPath,
  // Web Link
  ExtLink,
  // Package
  ExtPack,

  // -----------------
  
  /*
   * Basic Operation
   */

  // Binary Operation
  BinOp,

  // -----------------

  /*
   * Collection
   */

  // "   "
  String,
  // [a0, a1, ..., an]
  List,
  // [start .. end]
  Range,

  // -----------------

  /*
   * Basic Util
   */
  
  // Get the Size / Length / .. of A Collection
  SizeOf,

  // -----------------
  
  /*
   * Variable Definition
   */

  // @
  VarDef,

  // -----------------

  /*
   * Variable Assignment 
   */

  // =
  MutAssign,
  // := 
  FixAssign,

  // -----------------

  /*
   * Control Flow: Loop
   */
  InfLoop,

  // -----------------

  /*
   * Read
   */

  ReadFile,

  // -----------------

  /*
   * Write
   */

  WriteStdOut,

  // -----------------

  /*
   * Code Block
   */

  Block,

  // -----------------
};

enum class BinTok {
  BIN_ADD, // +
  BIN_SUB, // -
  BIN_MUL, // *
  BIN_DIV, // /
  BIN_POW, // **
  BIN_MOD, // %
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

static std::string reprToken(BinTok token) {
  switch (token)
  {
    case BinTok::BIN_ADD:
      return "<ADD>";

    case BinTok::BIN_SUB:
      return "<SUB>";

    case BinTok::BIN_MUL:
      return "<MUL>";

    case BinTok::BIN_DIV:
      return "<DIV>";

    case BinTok::BIN_POW:
      return "<POW>";

    case BinTok::BIN_MOD:
      return "<DIV>";

    default:
      return "<NULL>";

      break;
  }
}

static std::string reprToken(StyioToken token) {
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

#endif
