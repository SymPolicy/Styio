// [C++ STL]
#include <string>

// [Styio]
#include "Token.hpp"

std::string
reprASTType(StyioASTType type, std::string extra) {
  std::string output = "";

  switch (type) {
    case StyioASTType::True: {
      auto name = std::string("True");

      output = std::string(name);
    }

    break;

    case StyioASTType::False: {
      auto name = std::string("False");

      output = std::string(name);
    }

    break;

    case StyioASTType::None: {
      auto name = std::string("None");

      output = std::string(name);
    }

    break;

    case StyioASTType::Empty: {
      auto name = std::string("Empty");

      output = std::string(name);
    }

    break;

    case StyioASTType::Id: {
      auto name = std::string("id");

      output = std::string(name);
    }

    break;

    case StyioASTType::Variable: {
      auto name = std::string("var");

      output = std::string(name);
    }

    break;

    case StyioASTType::Arg: {
      auto name = std::string("Arg");

      output = std::string(name);
    }

    break;

    case StyioASTType::Integer: {
      auto name = std::string("styio.ast.int");

      output = std::string(name);
    }

    break;

    case StyioASTType::Float: {
      auto name = std::string("float");

      output = std::string(name);
    }

    break;

    case StyioASTType::Char: {
      auto name = std::string("char");

      output = std::string(name);
    }

    break;

    case StyioASTType::String: {
      auto name = std::string("String");

      output = std::string(name);
    }

    break;

    case StyioASTType::NumConvert: {
      auto name = std::string("Convert");

      output = std::string(name);
    }

    break;

    case StyioASTType::FmtStr: {
      auto name = std::string("FmtStr");

      output = std::string(name);
    }

    break;

    case StyioASTType::LocalPath: {
      auto name = std::string("styio.ast.path");

      output = std::string(name);
    } break;

    case StyioASTType::RemotePath: {
      auto name = std::string("Addr");

      output = std::string(name);
    } break;

    case StyioASTType::WebUrl: {
      auto name = std::string("URL");

      output = std::string(name);
    } break;

    case StyioASTType::DBUrl: {
      auto name = std::string("URL (Database)");

      output = std::string(name);
    } break;

    case StyioASTType::ExtPack: {
      auto name = std::string("Package");

      output = std::string(name);
    }

    break;

    case StyioASTType::VarTuple: {
      auto name = std::string("Fill");

      output = std::string(name);
    }

    break;

    case StyioASTType::Condition: {
      auto name = std::string("styio.ast.cond");

      output = std::string(name);
    }

    break;

    case StyioASTType::SizeOf: {
      auto name = std::string("SizeOf");

      output = std::string(name);
    } break;

    case StyioASTType::BinOp: {
      auto name = std::string("BinOp");

      output = std::string(name);
    } break;

    case StyioASTType::Print: {
      auto name = std::string("Print");

      output = std::string(name);
    } break;

    case StyioASTType::ReadFile: {
      auto name = std::string("Read File");

      output = std::string(name);
    } break;

    case StyioASTType::Call: {
      auto name = std::string("styio.ast.call");

      output = std::string(name);
    } break;

    case StyioASTType::Attribute: {
      auto name = std::string("styio.ast.attr");

      output = std::string(name);
    } break;

    case StyioASTType::Access: {
      auto name = std::string("Access");

      output = std::string(name);
    } break;

    case StyioASTType::Access_By_Name: {
      auto name = std::string("Access by Name");

      output = std::string(name);
    } break;

    case StyioASTType::Access_By_Index: {
      auto name = std::string("Access by Index");

      output = std::string(name);
    } break;

    case StyioASTType::Get_Index_By_Value: {
      auto name = std::string("Get Index by Value");

      output = std::string(name);
    } break;

    case StyioASTType::Get_Indices_By_Many_Values: {
      auto name = std::string("Get Indices by Many Value");

      output = std::string(name);
    } break;

    case StyioASTType::Append_Value: {
      auto name = std::string("Append");

      output = std::string(name);
    }

    break;

    case StyioASTType::Insert_Item_By_Index: {
      auto name = std::string("Insert by Index");

      output = std::string(name);
    }

    break;

    case StyioASTType::Remove_Item_By_Index: {
      auto name = std::string("Remove by Index");

      output = std::string(name);
    }

    break;

    case StyioASTType::Remove_Items_By_Many_Indices: {
      auto name = std::string("Remove by Many Indices");

      output = std::string(name);
    }

    break;

    case StyioASTType::Remove_Item_By_Value: {
      auto name = std::string("Remove by Value");

      output = std::string(name);
    }

    break;

    case StyioASTType::Remove_Items_By_Many_Values: {
      auto name = std::string("Remove by Many Values");

      output = std::string(name);
    }

    break;

    case StyioASTType::Get_Reversed: {
      auto name = std::string("Reversed");

      output = std::string(name);
    }

    break;

    case StyioASTType::Get_Index_By_Item_From_Right: {
      auto name = std::string("Get Index by Item (From Right)");

      output = std::string(name);
    }

    break;

    case StyioASTType::Return: {
      auto name = std::string("Return");

      output = std::string(name);
    }

    break;

    case StyioASTType::Range: {
      auto name = std::string("Range");

      output = std::string(name);
    }

    break;

    case StyioASTType::Tuple: {
      auto name = std::string("tuple");

      output = std::string(name);
    }

    break;

    case StyioASTType::List: {
      auto name = std::string("list");

      output = std::string(name);
    }

    break;

    case StyioASTType::Set: {
      auto name = std::string("hashset");

      output = std::string(name);
    }

    break;

    case StyioASTType::Resources: {
      auto name = std::string("styio.ast.resources");

      output = std::string(name);
    }

    break;

    case StyioASTType::MutBind: {
      output = std::string("Binding (Flexible)");
    }

    break;

    case StyioASTType::FinalBind: {
      output = std::string("Binding (Final)");
    }

    break;

    case StyioASTType::Block: {
      auto name = std::string("Block");

      output = std::string(name);
    }

    break;

    case StyioASTType::Cases: {
      auto name = std::string("Cases");

      output = std::string(name);
    }

    break;

    case StyioASTType::Func: {
      auto name = std::string("Function");

      output = std::string(name);
    }

    break;

    case StyioASTType::Struct: {
      auto name = std::string("Struct");

      output = std::string(name);
    }

    break;

    case StyioASTType::Loop: {
      auto name = std::string("Loop");

      output = std::string(name);
    }

    break;

    case StyioASTType::Iterator: {
      auto name = std::string("styio.ast.iterator");

      output = std::string(name);
    }

    break;

    case StyioASTType::CheckEq: {
      auto name = std::string("Equal To?");

      output = std::string(name);
    }

    break;

    case StyioASTType::CheckIsin: {
      auto name = std::string("Is In?");

      output = std::string(name);
    }

    break;

    case StyioASTType::FromTo: {
      auto name = std::string("Transfer");

      output = std::string(name);
    }

    break;

    case StyioASTType::Forward: {
      auto name = std::string("styio.ast.forward.run");

      output = std::string(name);
    }

    break;

    case StyioASTType::If_Equal_To_Forward: {
      auto name = std::string("Forward (If Equal -> Run)");

      output = std::string(name);
    }

    break;

    case StyioASTType::If_Is_In_Forward: {
      auto name = std::string("Forward (If Is In -> Run)");

      output = std::string(name);
    }

    break;

    case StyioASTType::Cases_Forward: {
      auto name = std::string("Forward (Cases)");

      output = std::string(name);
    }

    break;

    case StyioASTType::If_True_Forward: {
      auto name = std::string("Forward (If True -> Run)");

      output = std::string(name);
    }

    break;

    case StyioASTType::If_False_Forward: {
      auto name = std::string("Forward (If False -> Run)");

      output = std::string(name);
    }

    break;

    case StyioASTType::Fill_Forward: {
      auto name = std::string("Forward (Fill -> Run)");

      output = std::string(name);
    }

    break;

    case StyioASTType::Fill_If_Equal_To_Forward: {
      auto name = std::string("Forward (Fill -> If Equal -> Run)");

      output = std::string(name);
    }

    break;

    case StyioASTType::Fill_If_Is_in_Forward: {
      auto name = std::string("Forward (Fill -> If Is In -> Run)");

      output = std::string(name);
    }

    break;

    case StyioASTType::Fill_Cases_Forward: {
      auto name = std::string("Forward (Fill -> Cases)");

      output = std::string(name);
    }

    break;

    case StyioASTType::Fill_If_True_Forward: {
      auto name = std::string("Forward (Fill -> If True -> Run)");

      output = std::string(name);
    }

    break;

    case StyioASTType::Fill_If_False_Forward: {
      auto name = std::string("Forward (Fill -> If False -> Run)");

      output = std::string(name);
    } break;

    case StyioASTType::Chain_Of_Data_Processing: {
      auto name = std::string("styio.ast.chain_of_data_processing");

      output = std::string(name);
    } break;

    case StyioASTType::DType: {
      auto name = std::string("type");

      output = std::string(name);
    } break;

    case StyioASTType::TypedVar: {
      auto name = std::string("Var");

      output = std::string(name);
    }

    break;

    case StyioASTType::Pass: {
      auto name = std::string("Do Nothing");

      output = std::string(name);
    }

    break;

    case StyioASTType::Break: {
      auto name = std::string("Break");

      output = std::string(name);
    }

    break;

    case StyioASTType::CondFlow_True: {
      auto name = std::string("Conditionals (Only True)");

      output = std::string(name);
    }

    break;

    case StyioASTType::CondFlow_False: {
      auto name = std::string("Conditionals (Only False)");

      output = std::string(name);
    }

    break;

    case StyioASTType::CondFlow_Both: {
      auto name = std::string("Conditionals (True & False)");

      output = std::string(name);
    }

    break;

    case StyioASTType::MainBlock: {
      auto name = std::string("Main");

      output = std::string(name);
    }

    break;

    default:
      output = std::string("!{UnknownAST}");

      break;
  }

  return output + extra;
}

std::string
reprToken(TokenKind token) {
  switch (token) {
    case TokenKind::Binary_Add:
      return "<Add>";

    case TokenKind::Binary_Sub:
      return "<Sub>";

    case TokenKind::Binary_Mul:
      return "<Mul>";

    case TokenKind::Binary_Div:
      return "<Div>";

    case TokenKind::Binary_Pow:
      return "<Pow>";

    case TokenKind::Binary_Mod:
      return "<Mod>";

    case TokenKind::Self_Add_Assign:
      return "+=";

    case TokenKind::Self_Sub_Assign:
      return "-=";

    case TokenKind::Self_Mul_Assign:
      return "*=";

    case TokenKind::Self_Div_Assign:
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

std::string
reprToken(StyioToken token) {
  switch (token) {
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

StyioDataType
getMaxType(StyioDataType T1, StyioDataType T2) {
  if (T1.option == T2.option) {
    return T1;
  }
  
  return StyioDataType{StyioDataTypeOption::Undefined, "Undefined", 0};
}