#pragma once
#ifndef STYIO_TOKEN_H_
#define STYIO_TOKEN_H_

#include <cstdint>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#include "../StyioSession/SessionAllocation.hpp"

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
  Dict,

  Struct,

  Func,  // Function
};

enum class StyioHandleFamily : std::uint8_t
{
  None = 0,
  List,
  Dict,
  Range,
  File,
  Stream,
};

enum class StyioTypeState : std::uint8_t
{
  None = 0,
  Open,
  Materialized,
  Closed,
};

enum class StyioValueFamily : std::uint8_t
{
  Unknown = 0,
  Bool,
  Integer,
  Float,
  String,
  ListHandle,
  DictHandle,
  RangeHandle,
  FileHandle,
  StreamHandle,
  UserDefined,
};

enum class StyioTypeCapability : std::uint32_t
{
  None = 0,
  Iterable = 1u << 0,
  Indexable = 1u << 1,
  Sized = 1u << 2,
  Readable = 1u << 3,
  Writable = 1u << 4,
  Cloneable = 1u << 5,
  Collectable = 1u << 6,
};

inline constexpr std::uint32_t
styio_caps(StyioTypeCapability cap) {
  return static_cast<std::uint32_t>(cap);
}

inline constexpr std::uint32_t
styio_caps(StyioTypeCapability lhs, StyioTypeCapability rhs) {
  return styio_caps(lhs) | styio_caps(rhs);
}

struct StyioDataType
{
  StyioDataTypeOption option;
  std::string name;
  size_t num_of_bit = 0;
  StyioHandleFamily handle_family = StyioHandleFamily::None;
  StyioTypeState state = StyioTypeState::None;
  std::uint32_t capabilities = 0;
  std::string item_type_name;
  std::string key_type_name;
  bool has_std_stream_kind = false;
  int std_stream_kind = -1;
  StyioValueFamily value_family = StyioValueFamily::Unknown;
  StyioValueFamily item_value_family = StyioValueFamily::Unknown;
  StyioValueFamily key_value_family = StyioValueFamily::Unknown;

  bool isUndefined() const {
    return option == StyioDataTypeOption::Undefined;
  }

  bool isInteger() const {
    return option == StyioDataTypeOption::Integer;
  }

  bool isFloat() const {
    return option == StyioDataTypeOption::Float;
  }

  bool equals(const StyioDataType other) const {
    return option == other.option
      && name == other.name
      && num_of_bit == other.num_of_bit
      && handle_family == other.handle_family
      && state == other.state
      && capabilities == other.capabilities
      && item_type_name == other.item_type_name
      && key_type_name == other.key_type_name
      && has_std_stream_kind == other.has_std_stream_kind
      && std_stream_kind == other.std_stream_kind
      && value_family == other.value_family
      && item_value_family == other.item_value_family
      && key_value_family == other.key_value_family;
  }
};

inline StyioDataType
styio_make_list_type(const std::string& elem_name) {
  return StyioDataType{
    StyioDataTypeOption::List,
    std::string("list[") + elem_name + "]",
    0,
    StyioHandleFamily::List,
    StyioTypeState::Materialized,
    styio_caps(StyioTypeCapability::Iterable)
      | styio_caps(StyioTypeCapability::Indexable)
      | styio_caps(StyioTypeCapability::Sized)
      | styio_caps(StyioTypeCapability::Cloneable)
      | styio_caps(StyioTypeCapability::Collectable),
    elem_name,
    "",
    false,
    -1,
    StyioValueFamily::ListHandle};
}

inline bool
styio_is_list_type(const StyioDataType& type) {
  return type.option == StyioDataTypeOption::List
    || type.handle_family == StyioHandleFamily::List
    || type.name.rfind("list[", 0) == 0;
}

inline StyioDataType
styio_make_dict_type(const std::string& key_name, const std::string& value_name) {
  return StyioDataType{
    StyioDataTypeOption::Dict,
    std::string("dict[") + key_name + "," + value_name + "]",
    0,
    StyioHandleFamily::Dict,
    StyioTypeState::Materialized,
    styio_caps(StyioTypeCapability::Indexable)
      | styio_caps(StyioTypeCapability::Sized)
      | styio_caps(StyioTypeCapability::Cloneable),
    value_name,
    key_name,
    false,
    -1,
    StyioValueFamily::DictHandle};
}

inline bool
styio_is_dict_type(const StyioDataType& type) {
  return type.option == StyioDataTypeOption::Dict
    || type.handle_family == StyioHandleFamily::Dict
    || type.name.rfind("dict[", 0) == 0;
}

inline std::string
styio_list_elem_type_name(const StyioDataType& type) {
  const std::string& name = type.name;
  if (name.rfind("list[", 0) == 0 && !name.empty() && name.back() == ']') {
    return name.substr(5, name.size() - 6);
  }
  return "i64";
}

inline std::string
styio_dict_key_type_name(const StyioDataType& type) {
  if (!type.key_type_name.empty()) {
    return type.key_type_name;
  }
  const std::string& name = type.name;
  if (name.rfind("dict[", 0) == 0 && !name.empty() && name.back() == ']') {
    const std::string inner = name.substr(5, name.size() - 6);
    const size_t comma = inner.find(',');
    if (comma != std::string::npos) {
      return inner.substr(0, comma);
    }
  }
  return "string";
}

inline std::string
styio_dict_value_type_name(const StyioDataType& type) {
  if (!type.item_type_name.empty()) {
    return type.item_type_name;
  }
  const std::string& name = type.name;
  if (name.rfind("dict[", 0) == 0 && !name.empty() && name.back() == ']') {
    const std::string inner = name.substr(5, name.size() - 6);
    const size_t comma = inner.find(',');
    if (comma != std::string::npos && comma + 1 < inner.size()) {
      return inner.substr(comma + 1);
    }
  }
  return "i64";
}

inline bool
styio_type_has_capability(
  const StyioDataType& type,
  StyioTypeCapability capability
) {
  return (type.capabilities & styio_caps(capability)) != 0;
}

inline bool
styio_type_is_iterable(const StyioDataType& type) {
  return styio_type_has_capability(type, StyioTypeCapability::Iterable);
}

inline bool
styio_type_is_indexable(const StyioDataType& type) {
  return styio_type_has_capability(type, StyioTypeCapability::Indexable);
}

inline bool
styio_type_is_sized(const StyioDataType& type) {
  return styio_type_has_capability(type, StyioTypeCapability::Sized);
}

inline bool
styio_type_is_readable(const StyioDataType& type) {
  return styio_type_has_capability(type, StyioTypeCapability::Readable);
}

inline bool
styio_type_is_writable(const StyioDataType& type) {
  return styio_type_has_capability(type, StyioTypeCapability::Writable);
}

inline bool
styio_type_is_cloneable(const StyioDataType& type) {
  return styio_type_has_capability(type, StyioTypeCapability::Cloneable);
}

inline bool
styio_type_is_resource_handle(const StyioDataType& type) {
  return type.handle_family != StyioHandleFamily::None
    || styio_is_list_type(type)
    || styio_is_dict_type(type);
}

inline StyioValueFamily
styio_value_family_for_type(const StyioDataType& type) {
  if (type.value_family != StyioValueFamily::Unknown) {
    return type.value_family;
  }
  if (styio_is_list_type(type)) {
    return StyioValueFamily::ListHandle;
  }
  if (styio_is_dict_type(type)) {
    return StyioValueFamily::DictHandle;
  }
  switch (type.handle_family) {
    case StyioHandleFamily::List:
      return StyioValueFamily::ListHandle;
    case StyioHandleFamily::Dict:
      return StyioValueFamily::DictHandle;
    case StyioHandleFamily::Range:
      return StyioValueFamily::RangeHandle;
    case StyioHandleFamily::File:
      return StyioValueFamily::FileHandle;
    case StyioHandleFamily::Stream:
      return StyioValueFamily::StreamHandle;
    case StyioHandleFamily::None:
      break;
  }
  switch (type.option) {
    case StyioDataTypeOption::Bool:
      return StyioValueFamily::Bool;
    case StyioDataTypeOption::Integer:
      return StyioValueFamily::Integer;
    case StyioDataTypeOption::Float:
      return StyioValueFamily::Float;
    case StyioDataTypeOption::String:
      return StyioValueFamily::String;
    case StyioDataTypeOption::Defined:
    case StyioDataTypeOption::Struct:
    case StyioDataTypeOption::Func:
      return StyioValueFamily::UserDefined;
    default:
      return StyioValueFamily::Unknown;
  }
}

inline std::string
styio_type_item_type_name(const StyioDataType& type) {
  if (!type.item_type_name.empty()) {
    return type.item_type_name;
  }
  if (styio_is_list_type(type)) {
    return styio_list_elem_type_name(type);
  }
  if (styio_is_dict_type(type)) {
    return styio_dict_value_type_name(type);
  }
  return "i64";
}

/* Pre-defined DType Table */
static std::unordered_map<std::string, StyioDataType> const DTypeTable = {
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

enum class StyioOpType
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
  Self_Mod_Assign,      // a %= b
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
static std::unordered_map<StyioOpType, int> const TokenPrecedenceMap = {
  {StyioOpType::Unary_Positive, 999},  // + a
  {StyioOpType::Unary_Negative, 999},  // - a
  {StyioOpType::Bitwise_NOT, 999},     // ~ a
  {StyioOpType::Logic_NOT, 999},       // ! a

  {StyioOpType::Binary_Pow, 704},  // a ** b

  {StyioOpType::Binary_Mul, 703},  // a * b
  {StyioOpType::Binary_Div, 703},  // a / b
  {StyioOpType::Binary_Mod, 703},  // a % b

  {StyioOpType::Binary_Add, 702},  // a + b
  {StyioOpType::Binary_Sub, 702},  // a - b

  {StyioOpType::Bitwise_Left_Shift, 701},   // shl(x, y)
  {StyioOpType::Bitwise_Right_Shift, 701},  // shr(x, y)

  {StyioOpType::Greater_Than, 502},        // a > b
  {StyioOpType::Less_Than, 502},           // a < b
  {StyioOpType::Greater_Than_Equal, 502},  // a >= b
  {StyioOpType::Less_Than_Equal, 502},     // a <= b

  {StyioOpType::Equal, 501},      // a == b
  {StyioOpType::Not_Equal, 501},  // a != b

  {StyioOpType::Bitwise_AND, 303},  // a & b
  {StyioOpType::Bitwise_XOR, 302},  // a ^ b
  {StyioOpType::Bitwise_OR, 301},   // a | b

  {StyioOpType::Logic_AND, 203},  // a && b
  {StyioOpType::Logic_XOR, 202},  // a ⊕ b
  {StyioOpType::Logic_OR, 201},   // a || b

  {StyioOpType::If_Else_Flow, 101},  // ?() => a : b

  {StyioOpType::Self_Add_Assign, 1},  // a += b
  {StyioOpType::Self_Sub_Assign, 1},  // a -= b
  {StyioOpType::Self_Mul_Assign, 1},  // a *= b
  {StyioOpType::Self_Div_Assign, 1},  // a /= b
  {StyioOpType::Self_Mod_Assign, 1},  // a %= b

  {StyioOpType::Undefined, 0},    // Undefined
  {StyioOpType::End_Of_File, 0},  // Undefined
};

static std::unordered_map<StyioOpType, std::string> const TokenStrMap = {
  {StyioOpType::Undefined, "undefined"},  // undefined
  {StyioOpType::End_Of_File, "EOF"},      // EOF

  {StyioOpType::Binary_Pow, "**"},  // a ** b

  {StyioOpType::Binary_Mul, "*"},  // a * b
  {StyioOpType::Binary_Div, "/"},  // a / b
  {StyioOpType::Binary_Mod, "%"},  // a % b

  {StyioOpType::Binary_Add, "+"},  // a + b
  {StyioOpType::Binary_Sub, "-"},  // a - b

  {StyioOpType::Greater_Than, ">"},         // a > b
  {StyioOpType::Less_Than, "<"},            // a < b
  {StyioOpType::Greater_Than_Equal, ">="},  // a >= b
  {StyioOpType::Less_Than_Equal, "<="},     // a <= b

  {StyioOpType::Equal, "=="},      // a == b
  {StyioOpType::Not_Equal, "!="},  // a != b

  {StyioOpType::Bitwise_AND, "&"},  // a & b
  {StyioOpType::Bitwise_XOR, "^"},  // a ^ b
  {StyioOpType::Bitwise_OR, "|"},   // a | b

  {StyioOpType::Logic_AND, "&&"},  // a && b
  {StyioOpType::Logic_XOR, "⊕"},   // a ⊕ b
  {StyioOpType::Logic_OR, "||"},   // a || b

  {StyioOpType::Self_Add_Assign, "+="},  // a += b
  {StyioOpType::Self_Sub_Assign, "-="},  // a -= b
  {StyioOpType::Self_Mul_Assign, "*="},  // a *= b
  {StyioOpType::Self_Div_Assign, "/="},  // a /= b
  {StyioOpType::Self_Mod_Assign, "%="},  // a %= b
};

static std::unordered_map<std::string, StyioOpType> const StrTokenMap = {
  {"", StyioOpType::Undefined},       // Undefined
  {"EOF", StyioOpType::End_Of_File},  // EOF

  {"**", StyioOpType::Binary_Pow},  // a ** b

  {"*", StyioOpType::Binary_Mul},  // a * b
  {"/", StyioOpType::Binary_Div},  // a / b
  {"%", StyioOpType::Binary_Mod},  // a % b

  {"+", StyioOpType::Binary_Add},  // a + b
  {"-", StyioOpType::Binary_Sub},  // a - b

  {">", StyioOpType::Greater_Than},         // a > b
  {"<", StyioOpType::Less_Than},            // a < b
  {">=", StyioOpType::Greater_Than_Equal},  // a >= b
  {"<=", StyioOpType::Less_Than_Equal},     // a <= b

  {"==", StyioOpType::Equal},      // a == b
  {"!=", StyioOpType::Not_Equal},  // a != b

  {"&", StyioOpType::Bitwise_AND},  // a & b
  {"^", StyioOpType::Bitwise_XOR},  // a ^ b
  {"|", StyioOpType::Bitwise_OR},   // a | b

  {"&&", StyioOpType::Logic_AND},  // a && b
  {"⊕", StyioOpType::Logic_XOR},   // a ⊕ b
  {"||", StyioOpType::Logic_OR},   // a || b

  {"+=", StyioOpType::Self_Add_Assign},  // a += b
  {"-=", StyioOpType::Self_Sub_Assign},  // a -= b
  {"*=", StyioOpType::Self_Mul_Assign},  // a *= b
  {"/=", StyioOpType::Self_Div_Assign},  // a /= b
  {"%=", StyioOpType::Self_Mod_Assign},  // a %= b
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

enum class StyioNodeType
{
  End,
  Pass,
  Break,
  Continue,
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
  TypeTuple,
  TypedVar,
  OptArg,
  OptKwArg,
  Variable,
  Param,

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
  Dict,
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

  // M4: undefined literal, wave ops, fallback, selectors
  UndefLiteral,
  WaveMerge,
  WaveDispatch,
  Fallback,
  GuardSelector,
  EqProbeSelector,

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
  ParallelAssign,
  // -----------------

  /* -----------------
   * Pipeline
   */

  Func,
  SimpleFunc,
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
  IterSeq,
  // -----------------

  /* -----------------
   * Combination
   */
  IterWithMatch,
  // -----------------

  /* -----------------
   * Read
   */

  FileResource,
  HandleAcquire,
  ResourceWrite,
  ResourceRedirect,

  /* M6: state ledger, $refs, intrinsics, history */
  StateDecl,
  StateRef,
  HistoryProbe,
  SeriesIntrinsic,

  /* M7: multi-stream */
  StreamZip,
  SnapshotDecl,
  InstantPull,
  TypedStdinList,

  /* M9-M10: standard streams */
  StdinResource,
  StdoutResource,
  StderrResource,

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
  Parameters,
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
  HashTagName
};

/* M9: standard stream direction */
enum class StdStreamKind
{
  Stdin,
  Stdout,
  Stderr,
};

inline StyioDataType
styio_data_type_from_name(const std::string& type_name) {
  auto it = DTypeTable.find(type_name);
  if (it != DTypeTable.end()) {
    return it->second;
  }
  if (type_name.rfind("list[", 0) == 0) {
    return styio_make_list_type(styio_list_elem_type_name(
      StyioDataType{StyioDataTypeOption::List, type_name, 0}));
  }
  if (type_name.rfind("dict[", 0) == 0) {
    StyioDataType temp{StyioDataTypeOption::Dict, type_name, 0};
    return styio_make_dict_type(
      styio_dict_key_type_name(temp),
      styio_dict_value_type_name(temp));
  }
  return StyioDataType{StyioDataTypeOption::Defined, type_name, 0};
}

inline StyioValueFamily
styio_value_family_from_type_name(const std::string& type_name) {
  return styio_value_family_for_type(styio_data_type_from_name(type_name));
}

inline StyioValueFamily
styio_type_item_value_family(const StyioDataType& type) {
  if (type.item_value_family != StyioValueFamily::Unknown) {
    return type.item_value_family;
  }
  return styio_value_family_from_type_name(styio_type_item_type_name(type));
}

inline StyioValueFamily
styio_dict_key_value_family(const StyioDataType& type) {
  if (type.key_value_family != StyioValueFamily::Unknown) {
    return type.key_value_family;
  }
  return styio_value_family_from_type_name(styio_dict_key_type_name(type));
}

inline bool
styio_value_family_is_runtime_scalar(StyioValueFamily family) {
  switch (family) {
    case StyioValueFamily::Bool:
    case StyioValueFamily::Integer:
    case StyioValueFamily::Float:
    case StyioValueFamily::String:
      return true;
    default:
      return false;
  }
}

inline bool
styio_value_family_is_runtime_handle(StyioValueFamily family) {
  switch (family) {
    case StyioValueFamily::ListHandle:
    case StyioValueFamily::DictHandle:
      return true;
    default:
      return false;
  }
}

inline bool
styio_type_supports_runtime_list_elem(const StyioDataType& type) {
  StyioValueFamily family = styio_value_family_for_type(type);
  return styio_value_family_is_runtime_scalar(family)
    || styio_value_family_is_runtime_handle(family);
}

inline bool
styio_type_supports_runtime_dict_value(const StyioDataType& type) {
  StyioValueFamily family = styio_value_family_for_type(type);
  return styio_value_family_is_runtime_scalar(family)
    || styio_value_family_is_runtime_handle(family);
}

inline StyioDataType
styio_make_range_type(const std::string& elem_name = "i64") {
  return StyioDataType{
    StyioDataTypeOption::Defined,
    std::string("range[") + elem_name + "]",
    0,
    StyioHandleFamily::Range,
    StyioTypeState::Materialized,
    styio_caps(StyioTypeCapability::Iterable),
    elem_name,
    "",
    false,
    -1,
    StyioValueFamily::RangeHandle};
}

inline StyioDataType
styio_make_file_handle_type(const std::string& elem_name = "i64") {
  return StyioDataType{
    StyioDataTypeOption::Defined,
    std::string("file[") + elem_name + "]",
    0,
    StyioHandleFamily::File,
    StyioTypeState::Open,
    styio_caps(StyioTypeCapability::Iterable)
      | styio_caps(StyioTypeCapability::Readable)
      | styio_caps(StyioTypeCapability::Writable),
    elem_name,
    "",
    false,
    -1,
    StyioValueFamily::FileHandle};
}

inline StyioDataType
styio_make_std_stream_type(
  StdStreamKind kind,
  const std::string& elem_name = "string"
) {
  StyioDataType type{
    StyioDataTypeOption::Defined,
    kind == StdStreamKind::Stdin
      ? std::string("stdin[") + elem_name + "]"
      : (kind == StdStreamKind::Stdout
          ? std::string("stdout[") + elem_name + "]"
          : std::string("stderr[") + elem_name + "]"),
    0,
    StyioHandleFamily::Stream,
    StyioTypeState::Open,
    kind == StdStreamKind::Stdin
      ? (styio_caps(StyioTypeCapability::Iterable)
         | styio_caps(StyioTypeCapability::Readable))
      : styio_caps(StyioTypeCapability::Writable),
    elem_name,
    "",
    false,
    -1,
    StyioValueFamily::StreamHandle};
  type.has_std_stream_kind = true;
  type.std_stream_kind = static_cast<int>(kind);
  return type;
}

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
reprASTType(StyioNodeType type, std::string extra = "");

std::string
reprToken(CompType token);

std::string
reprToken(StyioOpType token);

std::string
reprToken(LogicType token);

/*
  To distinguish
    <= (less than or equal)
    from
    <= (left double arrow),
  construct a static map:
    {
      TOK_LE: (LANGLEBRAC, EQUAL),
      TOK_ARROW_DOUBLE_LEFT: (LANGLEBRAC, EQUAL)
    }
  use a function:
    check_pattern(StyioTokenType::TOK_LE)
*/
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
  TOK_QUEST = 63,      // ASCII 63 ?
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

  NAME = 1024,           // varname, funcname
  INTEGER,        // 0
  DECIMAL,        // 0.0
  STRING,         // "string"
  COMMENT_LINE,   //
  COMMENT_CLOSED, /* */

  BINOP_BITAND,  // &
  BINOP_BITOR,   // |
  BINOP_BITXOR,  // ^

  EXTRACTOR,  // <<
  ITERATOR,   // >>

  LOGIC_NOT,  // !
  LOGIC_AND,  // &&
  LOGIC_OR,   // ||
  LOGIC_XOR,  // ^

  UNARY_NEG,  // -

  BINOP_ADD,  // +
  BINOP_SUB,  // -
  BINOP_MUL,  // *
  BINOP_DIV,  // /
  BINOP_MOD,  // %
  BINOP_POW,  // **

  BINOP_GT,  // >
  BINOP_GE,  // >=
  BINOP_LT,  // <
  BINOP_LE,  // <=
  BINOP_EQ,  // ==
  BINOP_NE,  // !=

  PRINT,   // >_
  WALRUS,  // :=
  MATCH,   // ?=

  YIELD_PIPE,  // <|

  ARROW_DOUBLE_RIGHT,  // =>
  ARROW_DOUBLE_LEFT,   // <=
  ARROW_SINGLE_RIGHT,  // ->
  ARROW_SINGLE_LEFT,   // <-

  ELLIPSIS,       // ...
  INFINITE_LIST,  // [...]

  /* Topology v2: bounded ring buffer type [| n |] — paired delimiters (distinct from [ ... ]). */
  BOUNDED_BUFFER_OPEN,   // [|
  BOUNDED_BUFFER_CLOSE,  // |]

  SINGLE_SEP_LINE,  // ---
  DOUBLE_SEP_LINE,  // ===

  COMPOUND_ADD,  // +=
  COMPOUND_SUB,  // -=
  COMPOUND_MUL,  // *=
  COMPOUND_DIV,  // /=
  COMPOUND_MOD,  // %=

  WAVE_LEFT,   // <~
  WAVE_RIGHT,  // ~>
  DBQUESTION,  // ??

  UNKNOWN,
};

class StyioToken
{
private:
  StyioToken(
    StyioTokenType token_type,
    std::string token_literal
  ) :
      type(token_type), original(token_literal) {
  }

public:
  StyioTokenType type;
  std::string original;

  static void*
  operator new(std::size_t sz) {
    return styio::session_alloc::allocate_token_object(sz);
  }

  static void
  operator delete(void* ptr) noexcept {
    styio::session_alloc::free_object(ptr);
  }

  static StyioToken* Create(StyioTokenType token_type, std::string original_string) {
    return new StyioToken(token_type, original_string);
  }

  static StyioToken* CreatePersistent(StyioTokenType token_type, std::string original_string) {
    void* mem = ::operator new(sizeof(StyioToken));
    return ::new(mem) StyioToken(token_type, original_string);
  }

  static std::string getTokName(StyioTokenType type);

  size_t length();

  std::string as_str();
};

static std::unordered_map<StyioTokenType, std::vector<StyioTokenType> > const
  StyioTokenMap = {
    // =>
    {StyioTokenType::ARROW_DOUBLE_RIGHT,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_EQUAL,
       StyioTokenType::TOK_RANGBRAC
     }
    },
    // <=
    {StyioTokenType::ARROW_DOUBLE_LEFT,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_LANGBRAC,
       StyioTokenType::TOK_EQUAL
     }
    },
    // ->
    {StyioTokenType::ARROW_SINGLE_LEFT,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_MINUS,
       StyioTokenType::TOK_RANGBRAC
     }
    },
    // <-
    {StyioTokenType::ARROW_SINGLE_LEFT,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_LANGBRAC,
       StyioTokenType::TOK_MINUS
     }
    },
    /*
      Binary Operations
    */
    // ==
    {StyioTokenType::BINOP_EQ,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_EQUAL,
       StyioTokenType::TOK_EQUAL
     }
    },
    // !=
    {StyioTokenType::BINOP_NE,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_EXCLAM,
       StyioTokenType::TOK_EQUAL
     }
    },
    // >=
    {StyioTokenType::BINOP_GE,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_RANGBRAC,
       StyioTokenType::TOK_EQUAL
     }
    },
    // >
    {StyioTokenType::BINOP_GT,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_RANGBRAC
     }
    },
    // <=
    {StyioTokenType::BINOP_LE,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_LANGBRAC,
       StyioTokenType::TOK_EQUAL
     }
    },
    // <
    {StyioTokenType::BINOP_LT,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_LANGBRAC
     }
    },

    /*
      Special
    */
    // >_
    {StyioTokenType::PRINT,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_RANGBRAC,
       StyioTokenType::TOK_UNDLINE
     }
    },
    // :=
    {StyioTokenType::WALRUS,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_COLON,
       StyioTokenType::TOK_EQUAL
     }
    },
    // ?=
    {StyioTokenType::MATCH,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_QUEST,
       StyioTokenType::TOK_EQUAL
     }
    },

    /*
      LOGIC
    */
    // ! (Alternative)
    {StyioTokenType::LOGIC_NOT,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_EXCLAM
     }
    },
    // &&
    {StyioTokenType::LOGIC_AND,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_AMP,
       StyioTokenType::TOK_AMP
     }
    },
    // ||
    {StyioTokenType::LOGIC_OR,
     std::vector<StyioTokenType>{
       StyioTokenType::TOK_PIPE,
       StyioTokenType::TOK_AMP
     }
    },
};

#endif
