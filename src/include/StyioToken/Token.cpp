// [C++ STL]
#include <string>

// [Styio]
#include "../StyioToken/Token.hpp"

std::string reprStyioType (
  NodeHint type,
  bool colorful,
  std::string extra) {
  int titleColor = 96;
  int flexColor = 91;
  int fixColor = 92;

  std::string output = "";

  switch (type)
  {
  case NodeHint::True:
    {
      auto name = std::string("True");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }

    break;

  case NodeHint::False:
    {
      auto name = std::string("False");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }

    break;
    
  case NodeHint::None:
    {
      auto name = std::string("None");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }

    break;

  case NodeHint::Empty:
    {
      auto name = std::string("Empty");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }

    break;

  case NodeHint::Id:
    {
      auto name = std::string("id");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }

    break;

  case NodeHint::Int:
    {
      auto name = std::string("int");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Float:
    {
      auto name = std::string("float");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Char:
    {
      auto name = std::string("char");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::String:
    {
      auto name = std::string("String");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::FmtStr:
    {
      auto name = std::string("FmtStr");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::ExtPath:
    {
      auto name = std::string("Path");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::ExtLink:
    {
      auto name = std::string("Link");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::ExtPack:
    {
      auto name = std::string("Package");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Fill:
    {
      auto name = std::string("Fill");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Condition:
    {
      auto name = std::string("Condition");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::SizeOf:
    {
      auto name = std::string("SizeOf");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Bin_Add:
    {
      auto name = std::string("Add");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Bin_Sub:
    {
      auto name = std::string("Subtract");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Bin_Mul:
    {
      auto name = std::string("Multiply");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Bin_Div:
    {
      auto name = std::string("Divide");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Bin_Pow:
    {
      auto name = std::string("Power");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Bin_Mod:
    {
      auto name = std::string("Modulo");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Inc_Add:
    {
      auto name = std::string("Add (Inc.)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Inc_Sub:
    {
      auto name = std::string("Subtract (Inc.)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Inc_Mul:
    {
      auto name = std::string("Multiply (Inc.)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Inc_Div:
    {
      auto name = std::string("Divide (Inc.)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Print:
    {
      auto name = std::string("Print");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::ReadFile:
    {
      auto name = std::string("Read File");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Call:
    {
      auto name = std::string("Call");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Access:
    {
      auto name = std::string("Access");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Access_By_Name:
    {
      auto name = std::string("Access by Name");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Access_By_Index:
    {
      auto name = std::string("Access by Index");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Get_Index_By_Value:
    {
      auto name = std::string("Get Index by Value");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

case NodeHint::Get_Indices_By_Many_Values:
    {
      auto name = std::string("Get Indices by Many Value");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Append_Value:
    {
      auto name = std::string("Append");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Insert_Item_By_Index:
    {
      auto name = std::string("Insert by Index");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Remove_Item_By_Index:
    {
      auto name = std::string("Remove by Index");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Remove_Items_By_Many_Indices:
    {
      auto name = std::string("Remove by Many Indices");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Remove_Item_By_Value:
    {
      auto name = std::string("Remove by Value");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Remove_Items_By_Many_Values:
    {
      auto name = std::string("Remove by Many Values");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Get_Reversed:
    {
      auto name = std::string("Reversed");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Get_Index_By_Item_From_Right:
    {
      auto name = std::string("Get Index by Item (From Right)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Return:
    {
      auto name = std::string("Return");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Range:
    {
      auto name = std::string("Range");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Tuple:
    {
      auto name = std::string("Tuple");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;
  
  case NodeHint::List:
    {
      auto name = std::string("List");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Set:
    {
      auto name = std::string("Set");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Resources:
    {
      auto name = std::string("Resources");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;


    case NodeHint::MutBind:
    {
      if (colorful) {
        output = make_colorful(std::string("Binding"), titleColor) + " " + make_colorful(std::string("(Flexible)"), flexColor);
      } else {
        output = std::string("Binding (Flexible)");
      }
    }
    
    break;

  case NodeHint::FixBind:
    {
      if (colorful) {
        output = make_colorful(std::string("Binding"), titleColor) + " " + make_colorful(std::string("(Final)"), flexColor);
      } else {
        output = std::string("Binding (Final)");
      }
    }
    
    break;

  case NodeHint::Block:
    {
      auto name = std::string("Block");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Cases:
    {
      auto name = std::string("Cases");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Func:
    {
      auto name = std::string("Function");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Struct:
    {
      auto name = std::string("Struct");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Loop:
    {
      auto name = std::string("Loop");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Iterator:
    {
      auto name = std::string("Iterator");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::CheckEq:
    {
      auto name = std::string("Equal To?");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::CheckIsin:
    {
      auto name = std::string("Is In?");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::FromTo:
    {
      auto name = std::string("Transfer");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Forward:
    {
      auto name = std::string("Forward (Run)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::If_Equal_To_Forward:
    {
      auto name = std::string("Forward (If Equal -> Run)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::If_Is_In_Forward:
    {
      auto name = std::string("Forward (If Is In -> Run)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Cases_Forward:
    {
      auto name = std::string("Forward (Cases)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::If_True_Forward:
    {
      auto name = std::string("Forward (If True -> Run)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::If_False_Forward:
    {
      auto name = std::string("Forward (If False -> Run)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Fill_Forward:
    {
      auto name = std::string("Forward (Fill -> Run)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Fill_If_Equal_To_Forward:
    {
      auto name = std::string("Forward (Fill -> If Equal -> Run)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Fill_If_Is_in_Forward:
    {
      auto name = std::string("Forward (Fill -> If Is In -> Run)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Fill_Cases_Forward:
    {
      auto name = std::string("Forward (Fill -> Cases)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Fill_If_True_Forward:
    {
      auto name = std::string("Forward (Fill -> If True -> Run)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Fill_If_False_Forward:
    {
      auto name = std::string("Forward (Fill -> If False -> Run)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Type:
    {
      auto name = std::string("type");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::TypedVar:
    {
      auto name = std::string("Var");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Pass:
    {
      auto name = std::string("Do Nothing");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::Break:
    {
      auto name = std::string("Break");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::CondFlow_True:
    {
      auto name = std::string("Conditionals (Only True)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::CondFlow_False:
    {
      auto name = std::string("Conditionals (Only False)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  case NodeHint::CondFlow_Both:
    {
      auto name = std::string("Conditionals (True & False)");

      if (colorful) {
        output = make_colorful(name, titleColor);
      } else {
        output = std::string(name);
      }
    }
    
    break;

  default:
    output = std::string("Styio[Unknown]");

    break;
  }
  
  return output + extra;
}

std::string reprToken(LogicType token) {
  switch (token)
  {
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

std::string reprToken(CompType token) {
  switch (token)
  {
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

std::string reprToken(StyioToken token) {
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