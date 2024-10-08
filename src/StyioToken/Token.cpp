// [C++ STL]
#include <string>

// [Styio]
#include "Token.hpp"

std::string
reprASTType(StyioNodeType type, std::string extra) {
  std::string output = "styio.ast.";

  switch (type) {
    case StyioNodeType::True: {
      output += std::string("true");
    } break;

    case StyioNodeType::False: {
      output += std::string("false");
    } break;

    case StyioNodeType::None: {
      output += std::string("none");
    } break;

    case StyioNodeType::Empty: {
      output += std::string("empty");
    } break;

    case StyioNodeType::Id: {
      output += std::string("name");
    } break;

    case StyioNodeType::DType: {
      output += std::string("type");
    } break;

    case StyioNodeType::TypeTuple: {
      output += std::string("types");
    } break;

    case StyioNodeType::Variable: {
      output += std::string("var");
    } break;

    case StyioNodeType::Param: {
      output += std::string("arg");
    } break;

    case StyioNodeType::Integer: {
      auto name = std::string("styio.ast.int");

      output += std::string("");
    } break;

    case StyioNodeType::Float: {
      output += std::string("float");
    } break;

    case StyioNodeType::Char: {
      output += std::string("char");
    } break;

    case StyioNodeType::String: {
      output += std::string("string");
    } break;

    case StyioNodeType::NumConvert: {
      output += std::string("convert");
    } break;

    case StyioNodeType::FmtStr: {
      output += std::string("fmtstr");
    } break;

    case StyioNodeType::LocalPath: {
      output += std::string("path");
    } break;

    case StyioNodeType::RemotePath: {
      output += std::string("addr");
    } break;

    case StyioNodeType::WebUrl: {
      output += std::string("url");
    } break;

    case StyioNodeType::DBUrl: {
      output += std::string("url.database");
    } break;

    case StyioNodeType::ExtPack: {
      output += std::string("package");
    } break;

    case StyioNodeType::Parameters: {
      output += std::string("vars");
    } break;

    case StyioNodeType::Condition: {
      output += std::string("cond");
    } break;

    case StyioNodeType::SizeOf: {
      output += std::string("sizeof");
    } break;

    case StyioNodeType::BinOp: {
      output += std::string("binop");
    } break;

    case StyioNodeType::Print: {
      output += std::string("print");
    } break;

    case StyioNodeType::ReadFile: {
      output += std::string("read.file");
    } break;

    case StyioNodeType::Call: {
      output += std::string("call");
    } break;

    case StyioNodeType::Attribute: {
      output += std::string("attr");
    } break;

    case StyioNodeType::Access: {
      output += std::string("access");
    } break;

    case StyioNodeType::Access_By_Name: {
      output += std::string("access.by_name");
    } break;

    case StyioNodeType::Access_By_Index: {
      output += std::string("access.by_index");
    } break;

    case StyioNodeType::Get_Index_By_Value: {
      output += std::string("get_index.by_value");
    } break;

    case StyioNodeType::Get_Indices_By_Many_Values: {
      output += std::string("get_indices.by_values");
    } break;

    case StyioNodeType::Append_Value: {
      output += std::string("append");
    } break;

    case StyioNodeType::Insert_Item_By_Index: {
      output += std::string("insert.by_index");
    } break;

    case StyioNodeType::Remove_Item_By_Index: {
      output += std::string("remove.by_index");
    } break;

    case StyioNodeType::Remove_Items_By_Many_Indices: {
      output += std::string("remove.by_indices");
    } break;

    case StyioNodeType::Remove_Item_By_Value: {
      output += std::string("remove.by_value");
    } break;

    case StyioNodeType::Remove_Items_By_Many_Values: {
      output += std::string("remove.by_values");
    } break;

    case StyioNodeType::Get_Reversed: {
      output += std::string("reversed");
    } break;

    case StyioNodeType::Get_Index_By_Item_From_Right: {
      output += std::string("get_index.by_item_backward");
    } break;

    case StyioNodeType::Return: {
      output += std::string("return");
    } break;

    case StyioNodeType::Range: {
      output += std::string("range");
    } break;

    case StyioNodeType::Tuple: {
      output += std::string("tuple");
    } break;

    case StyioNodeType::List: {
      output += std::string("list");
    } break;

    case StyioNodeType::Set: {
      output += std::string("set");
    } break;

    case StyioNodeType::Resources: {
      output += std::string("resources");
    } break;

    case StyioNodeType::MutBind: {
      output += std::string("bind.flex");
    } break;

    case StyioNodeType::FinalBind: {
      output += std::string("styio.ast.bind.final");
    } break;

    case StyioNodeType::Block: {
      output += std::string("block");
    } break;

    case StyioNodeType::Cases: {
      output += std::string("cases");
    } break;

    case StyioNodeType::Func: {
      output += std::string("func");
    } break;

    case StyioNodeType::SimpleFunc: {
      output += std::string("func.simple");
    } break;

    case StyioNodeType::Struct: {
      output += std::string("struct");
    } break;

    case StyioNodeType::Loop: {
      output += std::string("loop");
    } break;

    case StyioNodeType::Iterator: {
      output += std::string("iterator");
    } break;

    case StyioNodeType::IterSeq: {
      output += std::string("iterator.sequence");
    } break;


    case StyioNodeType::CheckEq: {
      output += std::string("check.equal");
    } break;

    case StyioNodeType::CheckIsin: {
      output += std::string("check.isin");
    } break;

    case StyioNodeType::HashTagName: {
      output += std::string("hash_tag");
    } break;

    case StyioNodeType::TupleOperation: {
      output += std::string("tuple.op");
    } break;

    case StyioNodeType::Forward: {
      output += std::string("forward.run");
    } break;

    case StyioNodeType::If_Equal_To_Forward: {
      output += std::string("forward.check.equal");
    } break;

    case StyioNodeType::If_Is_In_Forward: {
      output += std::string("forward.check.isin");
    } break;

    case StyioNodeType::Cases_Forward: {
      output += std::string("forward.cases");
    } break;

    case StyioNodeType::If_True_Forward: {
      output += std::string("forward.only_true");
    } break;

    case StyioNodeType::If_False_Forward: {
      output += std::string("forward.only_false");
    } break;

    case StyioNodeType::Fill_Forward: {
      output += std::string("forward.fill.run");
    } break;

    case StyioNodeType::Fill_If_Equal_To_Forward: {
      output += std::string("fill.check.equal");
    } break;

    case StyioNodeType::Fill_If_Is_in_Forward: {
      output += std::string("fill.check.isin");
    } break;

    case StyioNodeType::Fill_Cases_Forward: {
      output += std::string("fill.cases");
    } break;

    case StyioNodeType::Fill_If_True_Forward: {
      output += std::string("fill.only_true");
    } break;

    case StyioNodeType::Fill_If_False_Forward: {
      output += std::string("fill.only_false");
    } break;

    case StyioNodeType::Backward: {
      output += std::string("backward");
    } break;

    case StyioNodeType::Chain_Of_Data_Processing: {
      output += std::string("chain_of_data_processing");
    } break;

    

    case StyioNodeType::TypedVar: {
      output += std::string("var");
    } break;

    case StyioNodeType::Pass: {
      output += std::string("do_nothing");
    } break;

    case StyioNodeType::Break: {
      output += std::string("break");
    } break;

    case StyioNodeType::CondFlow_True: {
      output += std::string("only_true");
    } break;

    case StyioNodeType::CondFlow_False: {
      output += std::string("only_false");
    } break;

    case StyioNodeType::CondFlow_Both: {
      output += std::string("if_else");
    } break;

    case StyioNodeType::MainBlock: {
      output += std::string("main");
    } break;

    default: {
      output += std::string("unknown");
    } break;
  }

  return output + extra;
}

std::string
reprToken(StyioOpType token) {
  switch (token) {
    case StyioOpType::Binary_Add:
      return "<Add>";

    case StyioOpType::Binary_Sub:
      return "<Sub>";

    case StyioOpType::Binary_Mul:
      return "<Mul>";

    case StyioOpType::Binary_Div:
      return "<Div>";

    case StyioOpType::Binary_Pow:
      return "<Pow>";

    case StyioOpType::Binary_Mod:
      return "<Mod>";

    case StyioOpType::Self_Add_Assign:
      return "+=";

    case StyioOpType::Self_Sub_Assign:
      return "-=";

    case StyioOpType::Self_Mul_Assign:
      return "*=";

    case StyioOpType::Self_Div_Assign:
      return "/=";

    default:
      return "<Undefined>";
      break;
  }
}

std::string
reprToken(LogicType token) {
  switch (token) {
    case LogicType::NOT:
      return "<NOT>";

    case LogicType::AND:
      return "<AND>";

    case LogicType::OR:
      return "<OR>";

    case LogicType::XOR:
      return "<OR>";

    default:
      return "<NULL>";

      break;
  }
}

std::string
reprToken(CompType token) {
  switch (token) {
    case CompType::EQ:
      return "<EQ>";

    case CompType::NE:
      return "<NE>";

    case CompType::GT:
      return "<GT>";

    case CompType::GE:
      return "<GE>";

    case CompType::LT:
      return "<LT>";

    case CompType::LE:
      return "<LE>";

    default:
      return "<NULL>";
      break;
  }
}

StyioDataType
getMaxType(StyioDataType T1, StyioDataType T2) {
  if (T1.option == T2.option) {
    return T1;
  }

  return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
}

std::string
reprDataTypeOption(StyioDataTypeOption option) {
  switch (option) {
    case StyioDataTypeOption::Undefined: {
      return "undefined";
    } break;

    case StyioDataTypeOption::Defined: {
      return "defined";
    } break;

    case StyioDataTypeOption::Bool: {
      return "bool";
    } break;

    case StyioDataTypeOption::Integer: {
      return "int";
    } break;

    case StyioDataTypeOption::Float: {
      return "float";
    } break;

    case StyioDataTypeOption::Decimal: {
      return "decimal";
    } break;

    case StyioDataTypeOption::Char: {
      return "char";
    } break;

    case StyioDataTypeOption::String: {
      return "string";
    } break;

    case StyioDataTypeOption::Tuple: {
      return "tuple";
    } break;

    case StyioDataTypeOption::List: {
      return "list";
    } break;

    case StyioDataTypeOption::Struct: {
      return "struct";
    } break;

    case StyioDataTypeOption::Func: {
      return "func";
    } break;

    default: {
      return "unknown";
    } break;
  }
};

/*
  =========================

  StyioToken Implementation

  =========================
*/

std::string
StyioToken::getTokName(StyioTokenType type) {
  switch (type) {
    case StyioTokenType::TOK_SPACE:
      return " ";

    case StyioTokenType::TOK_CR:
      return "<CR>";

    case StyioTokenType::TOK_LF:
      return "<LF>";

    case StyioTokenType::TOK_EOF:
      return "<EOF>";

    case StyioTokenType::NAME:
      return "<ID>";

    case StyioTokenType::INTEGER:
      return "<INT>";

    case StyioTokenType::DECIMAL:
      return "<FLOAT>";

    case StyioTokenType::STRING:
      return "<STRING>";

    case StyioTokenType::COMMENT_LINE:
      return "// COMMENT ";

    case StyioTokenType::COMMENT_CLOSED:
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

    case StyioTokenType::TOK_QUEST:
      return "?";

    case StyioTokenType::TOK_SLASH:
      return "/";

    case StyioTokenType::TOK_BACKSLASH:
      return "\\";

    case StyioTokenType::TOK_PIPE:
      return "|";

    case StyioTokenType::ELLIPSIS:
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

    case StyioTokenType::TOK_EQUAL:
      return "=";

    case StyioTokenType::TOK_LANGBRAC:
      return "<";

    case StyioTokenType::TOK_RANGBRAC:
      return ">";

    case StyioTokenType::LOGIC_NOT:
      return "<NOT>";

    case StyioTokenType::LOGIC_AND:
      return "<AND>";

    case StyioTokenType::LOGIC_OR:
      return "<OR>";

    case StyioTokenType::LOGIC_XOR:
      return "<XOR>";

    case StyioTokenType::BINOP_BITAND:
      return "<BIT_AND>";

    case StyioTokenType::BINOP_BITOR:
      return "<BIT_OR>";

    case StyioTokenType::BINOP_BITXOR:
      return "<BIT_XOR>";

    case StyioTokenType::EXTRACTOR:
      return "<<";

    case StyioTokenType::ITERATOR:
      return ">>";

    case StyioTokenType::PRINT:
      return ">_";

    case StyioTokenType::UNARY_NEG:
      return "<NEG>";

    case StyioTokenType::BINOP_ADD:
      return "<ADD>";

    case StyioTokenType::BINOP_SUB:
      return "<SUB>";

    case StyioTokenType::BINOP_MUL:
      return "<MUL>";

    case StyioTokenType::BINOP_DIV:
      return "<DIV>";

    case StyioTokenType::BINOP_MOD:
      return "<MOD>";

    case StyioTokenType::BINOP_POW:
      return "<POW>";

    case StyioTokenType::BINOP_GT:
      return "<GT>";

    case StyioTokenType::BINOP_GE:
      return "<GE>";

    case StyioTokenType::BINOP_LT:
      return "<LT>";

    case StyioTokenType::BINOP_LE:
      return "<LE>";

    case StyioTokenType::BINOP_EQ:
      return "<EQ>";

    case StyioTokenType::BINOP_NE:
      return "<NE>";

    case StyioTokenType::ARROW_DOUBLE_RIGHT:
      return "=>";

    case StyioTokenType::ARROW_DOUBLE_LEFT:
      return "<=";

    case StyioTokenType::ARROW_SINGLE_RIGHT:
      return "->";

    case StyioTokenType::ARROW_SINGLE_LEFT:
      return "<-";

    case StyioTokenType::WALRUS:
      return ":=";

    case StyioTokenType::MATCH:
      return "?=";

    case StyioTokenType::INFINITE_LIST:
      return "[...]";

    default:
      return "<UNKNOWN>";
  }
};

size_t
StyioToken::length() {
  return original.length();
}

std::string
StyioToken::as_str() {
  if (type == StyioTokenType::TOK_LF) {
    return "<LF>";
  }
  else if (type == StyioTokenType::TOK_SPACE) {
    return "<SPACE>";
  }
  else if (type == StyioTokenType::NAME
           || type == StyioTokenType::INTEGER
           || type == StyioTokenType::DECIMAL) {
    return getTokName(this->type) + " = " + this->original;
  }
  else if (type == StyioTokenType::STRING) {
    return "\"" + this->original + "\"";
  }
  else {
    return getTokName(this->type);
  }
}