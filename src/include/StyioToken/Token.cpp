// [C++ STL]
#include <string>

// [Styio]
#include "../StyioToken/Token.hpp"

std::string
reprDataType(StyioDataType dtype) {
  switch (dtype) {
    case StyioDataType::undefined:
      return std::string("undefined");

    case StyioDataType::i32:
      return std::string("i32");

    case StyioDataType::i64:
      return std::string("i64");

    case StyioDataType::f64:
      return std::string("f64");

    default:
      return std::string("default data type");
  }
}

std::string
reprNodeType(StyioNodeHint type, std::string extra) {
  std::string output = "";

  switch (type) {
    case StyioNodeHint::True: {
      auto name = std::string("True");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::False: {
      auto name = std::string("False");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::None: {
      auto name = std::string("None");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Empty: {
      auto name = std::string("Empty");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Id: {
      auto name = std::string("id");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Var: {
      auto name = std::string("var");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Arg: {
      auto name = std::string("Arg");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Int: {
      auto name = std::string("int");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Float: {
      auto name = std::string("float");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Char: {
      auto name = std::string("char");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::String: {
      auto name = std::string("String");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::NumConvert: {
      auto name = std::string("Convert");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::FmtStr: {
      auto name = std::string("FmtStr");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::LocalPath: {
      auto name = std::string("Path");

      output = std::string(name);
    } break;

    case StyioNodeHint::RemotePath: {
      auto name = std::string("Addr");

      output = std::string(name);
    } break;

    case StyioNodeHint::WebUrl: {
      auto name = std::string("URL");

      output = std::string(name);
    } break;

    case StyioNodeHint::DBUrl: {
      auto name = std::string("URL (Database)");

      output = std::string(name);
    } break;

    case StyioNodeHint::ExtPack: {
      auto name = std::string("Package");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::VarTuple: {
      auto name = std::string("Fill");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Condition: {
      auto name = std::string("Condition");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::SizeOf: {
      auto name = std::string("SizeOf");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Bin_Add: {
      auto name = std::string("Add");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Bin_Sub: {
      auto name = std::string("Subtract");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Bin_Mul: {
      auto name = std::string("Multiply");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Bin_Div: {
      auto name = std::string("Divide");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Bin_Pow: {
      auto name = std::string("Power");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Bin_Mod: {
      auto name = std::string("Modulo");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Inc_Add: {
      auto name = std::string("Add (Inc.)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Inc_Sub: {
      auto name = std::string("Subtract (Inc.)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Inc_Mul: {
      auto name = std::string("Multiply (Inc.)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Inc_Div: {
      auto name = std::string("Divide (Inc.)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Print: {
      auto name = std::string("Print");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::ReadFile: {
      auto name = std::string("Read File");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Call: {
      auto name = std::string("Call");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Access: {
      auto name = std::string("Access");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Access_By_Name: {
      auto name = std::string("Access by Name");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Access_By_Index: {
      auto name = std::string("Access by Index");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Get_Index_By_Value: {
      auto name = std::string("Get Index by Value");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Get_Indices_By_Many_Values: {
      auto name = std::string("Get Indices by Many Value");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Append_Value: {
      auto name = std::string("Append");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Insert_Item_By_Index: {
      auto name = std::string("Insert by Index");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Remove_Item_By_Index: {
      auto name = std::string("Remove by Index");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Remove_Items_By_Many_Indices: {
      auto name = std::string("Remove by Many Indices");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Remove_Item_By_Value: {
      auto name = std::string("Remove by Value");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Remove_Items_By_Many_Values: {
      auto name = std::string("Remove by Many Values");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Get_Reversed: {
      auto name = std::string("Reversed");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Get_Index_By_Item_From_Right: {
      auto name = std::string("Get Index by Item (From Right)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Return: {
      auto name = std::string("Return");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Range: {
      auto name = std::string("Range");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Tuple: {
      auto name = std::string("Tuple");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::List: {
      auto name = std::string("List");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Set: {
      auto name = std::string("Set");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Resources: {
      auto name = std::string("Resources");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::MutBind: {
      output = std::string("Binding (Flexible)");
    }

    break;

    case StyioNodeHint::FixBind: {
      output = std::string("Binding (Final)");
    }

    break;

    case StyioNodeHint::Block: {
      auto name = std::string("Block");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Cases: {
      auto name = std::string("Cases");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Func: {
      auto name = std::string("Function");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Struct: {
      auto name = std::string("Struct");
      
      output = std::string(name);
    }

    break;

    case StyioNodeHint::Loop: {
      auto name = std::string("Loop");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Iterator: {
      auto name = std::string("Iterator");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::CheckEq: {
      auto name = std::string("Equal To?");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::CheckIsin: {
      auto name = std::string("Is In?");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::FromTo: {
      auto name = std::string("Transfer");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Forward: {
      auto name = std::string("Forward (Run)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::If_Equal_To_Forward: {
      auto name = std::string("Forward (If Equal -> Run)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::If_Is_In_Forward: {
      auto name = std::string("Forward (If Is In -> Run)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Cases_Forward: {
      auto name = std::string("Forward (Cases)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::If_True_Forward: {
      auto name = std::string("Forward (If True -> Run)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::If_False_Forward: {
      auto name = std::string("Forward (If False -> Run)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Fill_Forward: {
      auto name = std::string("Forward (Fill -> Run)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Fill_If_Equal_To_Forward: {
      auto name = std::string("Forward (Fill -> If Equal -> Run)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Fill_If_Is_in_Forward: {
      auto name = std::string("Forward (Fill -> If Is In -> Run)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Fill_Cases_Forward: {
      auto name = std::string("Forward (Fill -> Cases)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Fill_If_True_Forward: {
      auto name = std::string("Forward (Fill -> If True -> Run)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Fill_If_False_Forward: {
      auto name = std::string("Forward (Fill -> If False -> Run)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::DType: {
      auto name = std::string("type");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::TypedVar: {
      auto name = std::string("Var");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Pass: {
      auto name = std::string("Do Nothing");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::Break: {
      auto name = std::string("Break");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::CondFlow_True: {
      auto name = std::string("Conditionals (Only True)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::CondFlow_False: {
      auto name = std::string("Conditionals (Only False)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::CondFlow_Both: {
      auto name = std::string("Conditionals (True & False)");

      output = std::string(name);
    }

    break;

    case StyioNodeHint::MainBlock: {
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