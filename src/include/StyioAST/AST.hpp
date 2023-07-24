#ifndef STYIO_AST_H_
#define STYIO_AST_H_

/*
  StyioAST
*/
class StyioAST {
  public:
    virtual ~StyioAST() {}

    virtual StyioType hint() = 0;

    virtual std::string toString(int indent = 0) = 0;

    virtual std::string toStringInline(int indent = 0) = 0;
};

/*
  =================
    None / Empty
  =================
*/

/*
  NoneAST: None / Null / Nil
*/
class NoneAST : public StyioAST {

  public:
    NoneAST () {}

    StyioType hint() {
      return StyioType::None;
    }

    std::string toString(int indent = 0) {
      return std::string("None { }");
    }

    std::string toStringInline(int indent = 0) {
      return std::string("None { }");
    }
};

class EndAST : public StyioAST {

  public:
    EndAST () {}

    StyioType hint() {
      return StyioType::End;
    }

    std::string toString(int indent = 0) {
      return std::string("EOF { }");
    }

    std::string toStringInline(int indent = 0) {
      return std::string("EOF { }");
    }
};

/*
  EmptyListAST: Empty List
*/
class EmptyListAST : public StyioAST {
  public:
    EmptyListAST() {}

    StyioType hint() {
      return StyioType::EmptyList;
    }

    std::string toString(int indent = 0) {
      return std::string("List(Empty) [ ]");
    }

    std::string toStringInline(int indent = 0) {
      return std::string("List(Empty) [ ]");
    }
};

/*
  EmptyBlockAST: Block
*/
class EmptyBlockAST : public StyioAST {
  public:
    EmptyBlockAST() {}

    StyioType hint() {
      return StyioType::EmptyBlock;
    }

    std::string toString(int indent = 0) {
      return std::string("Block (Empty) { ")
        + " } ";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Block (Empty) { ")
        + " } ";
    }
};

class PassAST : public StyioAST {

  public:
    PassAST () {}

    StyioType hint() {
      return StyioType::Pass;
    }

    std::string toString(int indent = 0) {
      return std::string("Pass { }");
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Pass { }");
    }
};

class ReturnAST : public StyioAST {
  StyioAST* Expr;

  public:
    ReturnAST (
      StyioAST* expr) : 
      Expr(expr) 
    {

    }

    StyioType hint() {
      return StyioType::Return;
    }

    std::string toString(int indent = 0) {
      return std::string("\033[1;36mReturn\033[0m { ") 
        + Expr -> toStringInline(indent + 1)
        + " }";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("\033[1;36mReturn\033[0m { ") 
        + Expr -> toStringInline(indent + 1)
        + " }";
    }
};

/*
  =================
    Variable
  =================
*/

/*
  IdAST: Identifier
*/
class IdAST : public StyioAST {
  std::string Id;

  public:
    IdAST(const std::string &id) : Id(id) {}

    StyioType hint() {
      return StyioType::Id;
    }

    std::string toString(int indent = 0) {
      return std::string("ID { ") + Id + " }";
    }

    std::string toStringInline(int indent = 0) {
      return Id;
    }
};

/*
  =================
    Scalar Value
  =================
*/

/*
  IntAST: Integer
*/
class IntAST : public StyioAST {
  int Value;

  public:
    IntAST(int val) : Value(val) {}

    StyioType hint() {
      return StyioType::Int;
    }

    std::string toString(int indent = 0) {
      return "Int { " + std::to_string(Value) + " }";
    }

    std::string toStringInline(int indent = 0) {
      return std::to_string(Value);
    }
};

/*
  FloatAST: Float
*/
class FloatAST : public StyioAST {
  double Value;

  public:
    FloatAST(double val) : Value(val) {}

    StyioType hint() {
      return StyioType::Float;
    }

    std::string toString(int indent = 0) {
      return "Float { " + std::to_string(Value) + " }";
    }

    std::string toStringInline(int indent = 0) {
      return std::to_string(Value);
    }
};

/*
  CharAST: Character
*/
class CharAST : public StyioAST {
  std::string Value;

  public:
    CharAST(std::string val) : Value(val) {}

    StyioType hint() {
      return StyioType::Char;
    }

    std::string toString(int indent = 0) {
      return "Character { \"" + Value + "\" }";
    }

    std::string toStringInline(int indent = 0) {
      return "<Character: \"" + Value + "\">";
    }
};

/*
  =================
    Data Resource Identifier
  =================
*/

/*
  ExtPathAST: (File) Path
*/
class ExtPathAST : public StyioAST {
  std::string Path;

  public:
    ExtPathAST (std::string path): Path(path) {}

    StyioType hint() {
      return StyioType::ExtPath;
    }

    std::string toString(int indent = 0) {
      return std::string("Path { ") + Path + " }";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Path { ") + Path + " }";
    }
};

/*
  ExtLinkAST: (Web) Link
*/
class ExtLinkAST : public StyioAST {
  std::string Link;

  public:
    ExtLinkAST (std::string link): Link(link) {}

    StyioType hint() {
      return StyioType::ExtLink;
    }

    std::string toString(int indent = 0) {
      return std::string("Link(Web) { }");
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Link(Web) { }");
    }
};

/*
  =================
    Collection
  =================
*/

/*
  StringAST: String
*/
class StringAST : public StyioAST {
  std::string Value;

  public:
    StringAST(std::string val) : Value(val) {}

    StyioType hint() {
      return StyioType::String;
    }

    std::string toString(int indent = 0) {
      return "String { \"" + Value + "\" }";
    }

    std::string toStringInline(int indent = 0) {
      return "<String: \"" + Value + "\">";
    }
};

/*
  ListAST: List (Extendable)
*/
class ListAST : public StyioAST {
  std::vector<StyioAST*> Elems;

  public:
    ListAST(std::vector<StyioAST*> elems): Elems(elems) {}

    StyioType hint() {
      return StyioType::List;
    }

    std::string toString(int indent = 0) {
      std::string ElemStr;

      for(int i=0; i < Elems.size(); i++) {
        ElemStr += std::string(2, ' ') + "| ";
        ElemStr += Elems[i] -> toString(indent);
        ElemStr += "\n";
      };

      return std::string("List [\n")
        + ElemStr
        + "]";
    }

    std::string toStringInline(int indent = 0) {
      std::string ElemStr;

      for(int i = 0; i < Elems.size(); i++) {
        ElemStr += Elems[i] -> toStringInline(indent);
        ElemStr += ", ";
      };

      return std::string("List [ ")
        + ElemStr
        + "]";
    }
};

/*
  RangeAST: Loop
*/
class RangeAST : public StyioAST 
{
  StyioAST* StartVal;
  StyioAST* EndVal;
  StyioAST* StepVal;

  public:
    RangeAST(StyioAST* start, StyioAST* end, StyioAST* step): StartVal(start), EndVal(end), StepVal(step) {}

    StyioType hint() {
      return StyioType::Range;
    }

    std::string toString(int indent = 0) {
      return std::string("Collection.Range {\n")
        + std::string(2, ' ') + "| Start: " + StartVal -> toString() + "\n"
        + std::string(2, ' ') + "| End: " + EndVal -> toString() + "\n"
        + std::string(2, ' ') + "| Step: " + StepVal -> toString() + "\n"
        + "\n}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Collection.Range {\n")
        + std::string(2, ' ') + "| Start: " + StartVal -> toString() + "\n"
        + std::string(2, ' ') + "| End: " + EndVal -> toString() + "\n"
        + std::string(2, ' ') + "| Step: " + StepVal -> toString() + "\n"
        + "\n}";
    }
};

/*
  =================
    Basic Operation
  =================
*/

/*
  SizeOfAST: Get Size(Length) Of A Collection
*/
class SizeOfAST : public StyioAST 
{
  StyioAST* Value;

  public:
    SizeOfAST(StyioAST* value): Value(value) {}

    StyioType hint() {
      return StyioType::SizeOf;
    }

    std::string toString(int indent = 0) {
      return std::string("SizeOf { ") 
      + Value -> toString()
      + " }";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("SizeOf { ") 
      + Value -> toStringInline()
      + " }";
    }
};

/*
  BinOpAST: Binary Operation

  | Variable
  | Scalar Value
    - Int
      {
        // General Operation
        Int (+, -, *, **, /, %) Int => Int
        
        // Bitwise Operation
        Int (&, |, >>, <<, ^) Int => Int
      }
    - Float
      {
        // General Operation
        Float (+, -, *, **, /, %) Int => Float
        Float (+, -, *, **, /, %) Float => Float
      }
    - Char
      {
        // Only Support Concatenation
        Char + Char => String
      }
  | Collection
    - Empty
      | Empty Tuple
      | Empty List (Extendable)
    - Tuple <Any>
      {
        // Only Support Concatenation
        Tuple<Any> + Tuple<Any> => Tuple
      }
    - Array <Scalar Value> // Immutable, Fixed Length
      {
        // (Only Same Length) Elementwise Operation
        Array<Any>[Length] (+, -, *, **, /, %) Array<Any>[Length] => Array<Any>

        // General Operation
        Array<Int> (+, -, *, **, /, %) Int => Array<Int>
        Array<Float> (+, -, *, **, /, %) Int => Array<Float>

        Array<Int> (+, -, *, **, /, %) Float => Array<Float>
        Array<Float> (+, -, *, **, /, %) Float => Array<Float>
      }
    - List
      {
        // Only Support Concatenation
        List<Any> + List<Any> => List<Any>
      }
    - String
      {
        // Only Support Concatenation
        String + String => String
      }
  | Blcok (With Return Value)
*/
class BinOpAST : public StyioAST 
{
  BinOpType Op;
  StyioAST *LHS;
  StyioAST *RHS;

  public:
    BinOpAST(
      BinOpType op, 
      StyioAST* lhs, 
      StyioAST* rhs): 
      Op(op),
      LHS(lhs), 
      RHS(rhs) 
      {

      }

    StyioType hint() {
      return StyioType::BinOp;
    }

    std::string toString(int indent = 0) {
      return std::string("\033[1;36mBinary Operation\033[0m {\n")
        + "|" + std::string(2 * indent, '-') + "| LHS: "
        + LHS -> toString(indent + 1) 
        + "\n"
        + "|" + std::string(2 * indent, '-') + "| OP : " + reprToken(Op)
        + "\n"
        + "|" + std::string(2 * indent, '-') + "| RHS: "
        + RHS -> toString(indent + 1) 
        + "}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("BinOp {") 
        + " LHS: "
        + LHS -> toStringInline(indent) 
        + " | Op: "
        + reprToken(Op)
        + " | RHS: "
        + RHS -> toStringInline(indent)  
        + " }";
    }
};

class BinCompAST: public StyioAST 
{
  CompType CompSign;
  StyioAST* LhsExpr;
  StyioAST* RhsExpr;

  public:
    BinCompAST(
      CompType sign, 
      StyioAST* lhs, 
      StyioAST* rhs): 
      CompSign(sign), 
      LhsExpr(lhs), 
      RhsExpr(rhs) 
      {

      }

    StyioType hint() {
      return StyioType::Compare;
    }

    std::string toString(int indent = 0) {
      return reprToken(CompSign) + std::string(" {\n")
        + "|" + std::string(2 * indent, '-') + "| LHS: " + LhsExpr -> toString() 
        + "\n"
        + "|" + std::string(2 * indent, '-') + "| RHS: " + RhsExpr -> toString()
        + "}";
    }

    std::string toStringInline(int indent = 0) {
      return reprToken(CompSign) + std::string(" {\n")
        + std::string(2, ' ') + "| LHS: " + LhsExpr -> toString() 
        + "\n"
        + std::string(2, ' ') + "| RHS: " + RhsExpr -> toString()
        + "}";
    }
};

class CondAST: public StyioAST 
{
  LogicType LogicOp;
  
  /*
    RAW: expr
    NOT: !(expr)
  */
  StyioAST* ValExpr;

  /*
    AND: expr && expr
    OR : expr || expr
  */
  StyioAST* LhsExpr;
  StyioAST* RhsExpr;

  public:
    CondAST(
      LogicType op, 
      StyioAST* val): 
      LogicOp(op), 
      ValExpr(val)
      {

      }
    
    CondAST(
      LogicType op, 
      StyioAST* lhs, 
      StyioAST* rhs): 
      LogicOp(op), 
      LhsExpr(lhs), 
      RhsExpr(rhs) 
      {

      }

    StyioType hint() {
      return StyioType::Condition;
    }

    std::string toString(int indent = 0) {
      if (LogicOp == LogicType::AND
          || LogicOp == LogicType::OR
          || LogicOp == LogicType::XOR)
      {
        return std::string("Condition {\n")
        + "|" + std::string(2 * indent, '-') + "| Op: " + reprToken(LogicOp) 
        + "\n"
        + "|" + std::string(2 * indent, '-') + "| LHS: " + LhsExpr -> toString(indent + 1) 
        + "\n"
        + "|" + std::string(2 * indent, '-') + "| RHS: " + RhsExpr -> toString(indent + 1)
        + "}";
      }
      else
      if (LogicOp == LogicType::NOT)
      {
        return std::string("Condition {\n")
        + "|" + std::string(2 * indent, '-') + "| Op: " + reprToken(LogicOp) 
        + "\n"
        + "|" + std::string(2 * indent, '-') + "| Value: " + ValExpr -> toString(indent + 1)
        + "}";
      }
      else
      if (LogicOp == LogicType::RAW)
      {
        return std::string("Condition {\n")
        + "|" + std::string(2 * indent, '-') + "| " + ValExpr -> toString(indent + 1)
        + "}";
      }
      else
      {
        return "Condition { Undefined! }";
      }
    }

    std::string toStringInline(int indent = 0) {
      if (LogicOp == LogicType::AND
          || LogicOp == LogicType::OR
          || LogicOp == LogicType::XOR)
      {
        return std::string("Condition {\n")
        + std::string(2, ' ') + "| Op: " + reprToken(LogicOp) + "\n"
        + std::string(2, ' ') + "| LHS: " + LhsExpr -> toString() + "\n"
        + std::string(2, ' ') + "| RHS: " + RhsExpr -> toString() + "\n"
        + "}";
      }
      else
      if (LogicOp == LogicType::NOT)
      {
        return std::string("Condition {\n")
        + std::string(2, ' ') + "| Op: " + reprToken(LogicOp) + "\n"
        + std::string(2, ' ') + "| Value: " + ValExpr -> toString() + "\n"
        + "}";
      }
      else
      if (LogicOp == LogicType::RAW)
      {
        return std::string("Condition {\n")
        + std::string(2, ' ') + ValExpr -> toString() + "\n"
        + "}";
      }
      else
      {
        return "Condition { Undefined! }";
      }
    }
};

/*

*/
class ListOpAST : public StyioAST 
{
  StyioAST* TheList;
  ListOpType OpType;

  IntAST* Index;
  StyioAST* Value;

  std::vector<IntAST*> IndexList;
  std::vector<StyioAST*> ValueList;

  public:
    /*
      Get_Index_By_Item
        [?= value]

      Remove_Item_By_Value
        [-: ?= value]

      Get_Index_By_Item_From_Right
        [[<] ?= value]

      Remove_Item_By_Value_From_Right
        [[<] -: ?= value]
    */
    ListOpAST(
      StyioAST* theList, 
      ListOpType opType, 
      StyioAST* item): 
      TheList(theList), 
      OpType(opType), 
      Value(item) {}

    /*
      Insert_Item_By_Index
        [+: index <- value]
    */
    ListOpAST(
      StyioAST* theList, 
      ListOpType opType, 
      IntAST* index, 
      StyioAST* item): 
      TheList(theList), 
      OpType(opType), 
      Index(index), 
      Value(item) {}

    /*
      Remove_Item_By_Index
        [-: index] 
    */
    ListOpAST(
      StyioAST* theList, 
      ListOpType opType, 
      IntAST* index): 
      TheList(theList), 
      OpType(opType), 
      Index(index) {}

    /*
      Remove_Many_Items_By_Indices
        [-: (i0, i1, ...)]
    */
    ListOpAST(
      StyioAST* theList, 
      ListOpType opType, 
      std::vector<IntAST*> indexList): 
      TheList(theList), 
      OpType(opType), 
      IndexList(indexList) {}

    /*
      Remove_Many_Items_By_Values
        [-: ?^ (v0, v1, ...)]

      Remove_Many_Items_By_Values_From_Right
        [[<] -: ?^ (v0, v1, ...)]
    */
    ListOpAST(
      StyioAST* theList, 
      ListOpType opType, 
      std::vector<StyioAST*> valueList): 
      TheList(theList), 
      OpType(opType), 
      ValueList(valueList) {}

    /*
      Get_Reversed
        [<]
    */
    ListOpAST(
      StyioAST* theList, 
      ListOpType opType
      ): 
      TheList(theList), 
      OpType(opType) {}

    StyioType hint() {
      return StyioType::ListOp;
    }

    std::string toString(int indent = 0) {
      return std::string("List[Operation] { \n") 
      + std::string(2, ' ') + "Type { " + reprListOp(OpType) + " }\n"
      + std::string(2, ' ') + TheList -> toString() + "\n"
      + "}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("List[Operation] { ") 
      + reprListOp(OpType) 
      + TheList -> toStringInline()
      + " }";
    }
};

/*
  =================
    Statement: Variable Definition
  =================
*/

/*
  VarDefAST: Variable Definition

  Definition = 
    Declaration (Neccessary)
    + | Assignment (Optional)
      | Import (Optional)
*/
class VarDefAST : public StyioAST {
  std::vector<IdAST*> Vars;

  public:
    VarDefAST(std::vector<IdAST*> vars): Vars(vars) {}

    StyioType hint() {
      return StyioType::VarDef;
    }

    std::string toString(int indent = 0) {
      std::string varStr;

      for (std::vector<IdAST*>::iterator it = Vars.begin(); 
        it != Vars.end(); 
        ++it
      ) {
        varStr += std::string(2, ' ') + "| ";
        varStr += (*it) -> toStringInline();
        varStr += "\n";
      };

      return std::string("Variable Definition {\n")
        + varStr
        + "} ";
    }

    std::string toStringInline(int indent = 0) {
      std::string varStr;

      for (std::vector<IdAST*>::iterator it = Vars.begin(); 
        it != Vars.end(); 
        ++it
      ) {
        varStr += std::string(2, ' ') + "| ";
        varStr += (*it) -> toStringInline();
        varStr += " ;";
      };

      return std::string("Var Def { ")
        + varStr
        + " } ";
    }
};

/*
  =================
    Statement: Variable Assignment (Variable-Value Binding)
  =================
*/

/*
  FlexBindAST: Mutable Assignment (Flexible Binding)
*/
class FlexBindAST : public StyioAST {
  IdAST* varId;
  StyioAST* valExpr;

  public:
    FlexBindAST(IdAST* var, StyioAST* val) : varId(var), valExpr(val) {}

    StyioType hint() {
      return StyioType::MutAssign;
    }

    std::string toString(int indent = 0) {
      return std::string("\033[1;36mBinding\033[0m \033[32m(Flexible)\033[0m {\n") 
        + "|" + std::string(2 * indent, '-') + "| Var: " 
        + varId -> toString(indent + 1) 
        + "\n"
        + "|" + std::string(2 * indent, '-') + "| Val: " 
        + valExpr -> toString(indent + 1)
        + "}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("\033[1;36mBinding (Flexible)\033[0m {\n") 
        + std::string(2, ' ') + "| Var: " 
        + varId -> toStringInline(indent) 
        + "; "
        + std::string(2, ' ') + "| Val: " 
        + valExpr -> toStringInline(indent) 
        + " }";
    }
};

/*
  FinalBindAST: Immutable Assignment (Final Binding)
*/
class FinalBindAST : public StyioAST {
  IdAST* varId;
  StyioAST* valExpr;

  public:
    FinalBindAST(IdAST* var, StyioAST* val) : varId(var), valExpr(val) {}

    StyioType hint() {
      return StyioType::FixAssign;
    }

    std::string toString(int indent = 0) {
      return std::string("\033[1;36mBinding\033[0m \033[31m(Final)\033[0m {\n") 
        + std::string(2, ' ') + "| Var: " 
        + varId -> toString(indent) 
        + "\n"
        + std::string(2, ' ') + "| Val: " 
        + valExpr -> toString(indent) 
        + "\n"
        + "}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("\033[1;36mBinding (Final)\033[0m { ") 
        + std::string(2, ' ') + "| Var: " 
        + varId -> toString(indent) 
        + "; "
        + std::string(2, ' ') + "| Val: " 
        + valExpr -> toString(indent) 
        + " }";
    }
};

/*
  =================
    Pipeline
      Function
      Structure (Struct)
      Evaluation
  =================
*/

/*
  FuncAST: Function
*/
class FuncAST : public StyioAST {
  IdAST* FName;
  std::vector<IdAST*> FVars;
  StyioAST* FBlock;

  bool FwithName;
  bool FisFinal;

  public:
    FuncAST(
      std::vector<IdAST*> vars,
      StyioAST* block,
      bool isFinal) :  
      FVars(vars),
      FBlock(block),
      FisFinal(isFinal)
      {
        FwithName = false;
      }

    FuncAST(
      IdAST* name, 
      std::vector<IdAST*> vars,
      StyioAST* block,
      bool isFinal) : 
      FName(name), 
      FVars(vars),
      FBlock(block),
      FisFinal(isFinal)
      {
        FwithName = true;
      }

    StyioType hint() {
      return StyioType::Function;
    }

    std::string toString(int indent = 0) {
      std::string output = std::string("\033[1;36mFunction\033[0m ");

      if (FisFinal)
      {
        output += "\033[31m(Final)\033[0m";
      }
      else
      {
        output += "\033[32m(Flexible)\033[0m";
      };

      output += " {\n";
      output += "|" + std::string(2 * indent, '-') + "| Name: " + FName -> toString(indent + 1) + "\n";
      
      output += "|" + std::string(2 * indent, '-') + "| Vars: ";
      for (std::vector<IdAST*>::iterator it = FVars.begin(); 
        it != FVars.end(); 
        ++it
      ) {
        output += std::string(2 * indent, '-');
        output += (*it) -> toString(indent + 1);
        output += " ";
      };

      output += "\n";

      output += "|" + std::string(2 * indent, '-') + "| " + FBlock -> toString(indent + 1);

      output += "}";

      return output;
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Function {") 
        + "}";
    }
};

/*
  StructAST: Structure
*/
class StructAST : public StyioAST {
  IdAST* FName;
  std::vector<IdAST*> FVars;
  StyioAST* FBlock;

  public:
    StructAST(
      IdAST* name, 
      std::vector<IdAST*> vars,
      StyioAST* block) : 
      FName(name), 
      FVars(vars),
      FBlock(block)
      {

      }

    StyioType hint() {
      return StyioType::Structure;
    }

    std::string toString(int indent = 0) {
      return std::string("Struct {") 
        + "}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Struct {") 
        + "}";
    }
};

/*
  =================
    OS Utility: IO Stream
  =================
*/

/*
  ReadFileAST: Read (File)
*/
class ReadFileAST : public StyioAST {
  IdAST* varId;
  StyioAST* valExpr;

  public:
    ReadFileAST(IdAST* var, StyioAST* val) : varId(var), valExpr(val) {}

    StyioType hint() {
      return StyioType::ReadFile;
    }

    std::string toString(int indent = 0) {
      return std::string("Read {\n") 
        + std::string(2, ' ') + "| Var: " 
        + varId -> toString(indent) 
        + "\n"
        + std::string(2, ' ') + "| Val: " 
        + valExpr -> toStringInline(indent) 
        + "\n"
        + "}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Read {\n") 
        + std::string(2, ' ') + "| Var: " 
        + varId -> toString(indent) 
        + "; "
        + std::string(2, ' ') + "| Val: " 
        + valExpr -> toStringInline(indent) 
        + " }";
    }
};

/*
  WriteStdOutAST: Write to Standard Output (Print)
*/
class WriteStdOutAST : public StyioAST {
  StyioAST* Output;

  public:
    WriteStdOutAST(
      StyioAST* output): 
      Output(output) 
      {

      }

    StyioType hint() {
      return StyioType::WriteStdOut;
    }

    std::string toString(int indent = 0) {
      return std::string("Print {\n")
        + std::string(2, ' ') + "| "
        + Output -> toString()
        + "\n}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Print { ")
        + std::string(2, ' ') + "| "
        + Output -> toString()
        + " }";
    }
};

/*
  =================
    Abstract Level: Dependencies
  =================
*/

/*
  ExtPackAST: External Packages
*/
class ExtPackAST : public StyioAST {
  std::vector<std::string> PackPaths;

  public:
    ExtPackAST(std::vector<std::string> paths): PackPaths(paths) {}

    StyioType hint() {
      return StyioType::ExtPack;
    }

    std::string toString(int indent = 0) {
      std::string pacPathStr;

      for(int i = 0; i < PackPaths.size(); i++) {
        pacPathStr += std::string(2, ' ') + "| ";
        pacPathStr += PackPaths[i];
        pacPathStr += "\n";
      };

      return std::string("External Packages {\n")
        + pacPathStr
        + "\n} ";
    }

    std::string toStringInline(int indent = 0) {
      std::string pacPathStr;

      for(int i = 0; i < PackPaths.size(); i++) {
        pacPathStr += std::string(2, ' ') + "| ";
        pacPathStr += PackPaths[i];
        pacPathStr += " ;";
      };

      return std::string("External Packages { ")
        + pacPathStr
        + " } ";
    }
};

/*
  =================
    Abstract Level: Block 
  =================
*/

/*
  BlockAST: Block
*/
class BlockAST : public StyioAST {
  std::vector<StyioAST*> Stmts;

  public:
    BlockAST() {}

    BlockAST(
      std::vector<StyioAST*> stmts): 
      Stmts(stmts) 
      {

      }

    StyioType hint() {
      return StyioType::Block;
    }

    std::string toString(int indent = 0) {
      std::string stmtStr;

      for (std::vector<StyioAST*>::iterator it = Stmts.begin(); 
        it != Stmts.end(); 
        ++it
      ) {
        stmtStr += "|" + std::string(2 * indent, '-') + "| ";
        stmtStr += (*it) -> toString(indent + 1);
        
        if (it != (Stmts.end() - 1))
        {
          stmtStr += "\n";
        };
      };

      return std::string("\033[1;36mBlock\033[0m {\n")
        + stmtStr
        + "}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Block { }");
    }
};

/*
  CaseAST: Match Block (Cases)
*/
class CaseAST : public StyioAST {
  std::vector<std::tuple<StyioAST*, StyioAST*>> Cases;

  public:
    CaseAST() {}

    CaseAST(
      std::vector<std::tuple<StyioAST*, StyioAST*>> cases): 
      Cases(cases) 
      {

      }

    StyioType hint() {
      return StyioType::MatchBlock;
    }

    std::string toString(int indent = 0) {
      std::string stmtStr;

      for (auto [X, Y] : Cases)
      {
        stmtStr += X -> toStringInline();
        stmtStr += Y -> toStringInline();
        stmtStr += "\n";
      };

      return std::string("Match Block (Cases) {\n")
        + std::string(2, ' ') + "| Cases: "
        + stmtStr
        + "\n"
        + "\n} ";
    }

    std::string toStringInline(int indent = 0) {
      std::string stmtStr;

      for (auto [X, Y] : Cases)
      {
        stmtStr += X -> toStringInline();
        stmtStr += Y -> toStringInline();
        stmtStr += "\n";
      };

      return std::string("Block { ")
        + std::string(2, ' ') + "| Cases: "
        + stmtStr
        + " } ";
    }
};

class CondFlowAST : public StyioAST {
  FlowType WhatFlow;
  CondAST* CondExpr;
  StyioAST* ThenBlock;
  StyioAST* ElseBlock;

  public:
    CondFlowAST(
      FlowType whatFlow,
      CondAST* condition,
      StyioAST* block): 
      WhatFlow(whatFlow),
      CondExpr(condition),
      ThenBlock(block)
      {

      }

    CondFlowAST(
      FlowType whatFlow,
      CondAST* condition,
      StyioAST* blockThen,
      StyioAST* blockElse): 
      WhatFlow(whatFlow),
      CondExpr(condition),
      ThenBlock(blockThen),
      ElseBlock(blockElse)
      {

      }

    StyioType hint() {
      return StyioType::CondFlow;
    }

    std::string toString(int indent = 0) {
      return std::string("If (Flow) {") 
      + "}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("If (Flow) {") 
      + "}";
    }
};

/*
  =================
    Abstract Level: Layers
  =================
*/

/*
  ICBSLayerAST: Intermediate Connection Between Scopes

  ExtraMatchValue:
    >> Element(Single) ?= ValueExpr(Single) => {
      ...
    }
    
    For each step of iteration, check if the element match the value expression, 
    if match case is true, then execute the branch. 

  ExtraMatchCases:
    >> Element(Single) ?= {
      v0 => {}
      v1 => {}
      _  => {}
    }
    
    For each step of iteration, check if the element match any value expression, 
    if match case is true, then execute the branch. 

  ExtraIsin:
    >> Element(Single) ?^ IterableExpr(Collection) => {
      ...
    }

    For each step of iteration, check if the element is in the following collection,
    if match case is true, then execute the branch. 

  ExtraCond: 
    >> Elements ?? (Condition) :) {
      ...
    }
    
    For each step of iteration, check the given condition, 
    if condition is true, then execute the branch. 

    >> Elements ?? (Condition) :( {
      ...
    }
    
    For each step of iteration, check the given condition, 
    if condition is false, then execute the branch. 

  Rules:
    1. If: a variable NOT not exists in its outer scope
       Then: create a variable and refresh it for each step

    2. If: a value expression can be evaluated with respect to its outer scope
       And: the value expression changes along with the iteration
       Then: refresh the value expression for each step

  How to parse ICBSLayer (for each element):
    1. Is this a [variable] or a [value expression]?
       
      Variable => 2
      Value Expression => 3

      (Hint: A value expression is something can be evaluated to a value
      after performing a series operations.)

    2. Is this variable previously defined or declared?

      Yes => 4
      No => 5

    3. Is this value expression using any variable that was NOT previously defined?

      Yes => 6
      No => 7

    4. Is this variable still mutable?

      Yes => 8
      No => 9

    5. Great! This element is a [temporary variable]
      that can ONLY be used in the following block.
      
      For each step of the iteration, you should:
        - Refresh this temporary variable before the start of the block.

    6. Error! Why is it using something that does NOT exists?
      This is an illegal value expression, 
      you should throw an exception for this.

    7. Great! This element is a [changing value expression]
      that the value is changing while iteration.

      For each step of the iteration, you should:
        - Evaluate the value expression before the start of the block.

    8. Great! This element is a [changing variable]
      that the value of thisvariable is changing while iteration.

      (Note: The value of this variable should be changed by
        some statements inside the following code block.
        However, if you can NOT find such statements,
        do nothing.)

      For each step of the iteration, you should:
        - Refresh this variable before the start of the block.

    9. Error! Why are you trying to update a value that can NOT be changed?
      There must be something wrong, 
      you should throw an exception for this.
*/

class CheckEqAST : public StyioAST {
  StyioAST* Value;

  public:
    CheckEqAST(
      StyioAST* value): 
      Value(value)
      {

      }

    StyioType hint() {
      return StyioType::CheckEq;
    }

    std::string toString(int indent = 0) {
      return std::string("Check Equal {") 
      + "}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Check Equal {") 
      + "}";
    }
};

class CheckIsinAST : public StyioAST {
  StyioAST* Iterable;

  public:
    CheckIsinAST(
      StyioAST* value): 
      Iterable(value)
      {

      }

    StyioType hint() {
      return StyioType::CheckIsin;
    }

    std::string toString(int indent = 0) {
      return std::string("Check Is In {") 
      + "}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Check Is In {") 
      + "}";
    }
};

class CheckCondAST : public StyioAST {
  CondAST* Condition;

  public:
    CheckCondAST(
      CondAST* condition): 
      Condition(condition)
      {

      }

    StyioType hint() {
      return StyioType::CheckCond;
    }

    std::string toString(int indent = 0) {
      return std::string("Check Condition {") 
      + "}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Check Condition {") 
      + "}";
    }
};

/*
  ExtraEq:
    ?=

  ExtraIsIn:
    ?^
  
  ExtraCond:
    ?()
*/

class ICBSLayerAST : public StyioAST {
  std::vector<StyioAST*> TmpVars;
  CheckEqAST* ExtraEq;
  CheckIsinAST* ExtraIsin;
  CheckCondAST* ExtraCond;

  public:
    ICBSLayerAST() {}

    ICBSLayerAST(
      std::vector<StyioAST*> vars): 
      TmpVars(vars)
      {

      }

    ICBSLayerAST(
      std::vector<StyioAST*> vars,
      CheckEqAST* value): 
      TmpVars(vars),
      ExtraEq(value)
      {

      }

    ICBSLayerAST(
      std::vector<StyioAST*> vars,
      CheckIsinAST* isinExpr): 
      TmpVars(vars),
      ExtraIsin(isinExpr)
      {

      }

    ICBSLayerAST(
      std::vector<StyioAST*> vars,
      CheckCondAST* condExpr): 
      TmpVars(vars),
      ExtraCond(condExpr)
      {

      }

    StyioType hint() {
      return StyioType::ICBSLayer;
    }

    std::string toString(int indent = 0) {
      return std::string("Layer (Intermediate Connection Between Scopes) { }");
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Layer (Intermediate Connection Between Scopes) { }");
    }
};

/*
  =================
    Infinite loop 
  =================
*/

/*
InfLoop: Infinite Loop
  incEl Increment Element
*/
class InfiniteAST : public StyioAST {
  InfiniteType WhatType;
  StyioAST* Start;
  StyioAST* IncEl;

  public:
    InfiniteAST() 
    {
      WhatType = InfiniteType::Original;
    }

    InfiniteAST(
      StyioAST* start, 
      StyioAST* incEl): 
      Start(start), 
      IncEl(incEl) 
      {
        WhatType = InfiniteType::Incremental;
      }

    StyioType hint() {
      return StyioType::InfLoop;
    }

    std::string toString(int indent = 0) {
      switch (WhatType)
      {
      case InfiniteType::Original:
        return std::string("Infinite { }");

        // You should NOT reach this line!
        break;
      
      case InfiniteType::Incremental:
        return std::string("Infinite {")
          + "\n" 
          + "|" + std::string(2 * indent, '-') + "| Start: "
          + Start -> toString(indent + 1) 
          + "\n"
          + "|" + std::string(2 * indent, '-') + "| Increment: "
          + IncEl -> toString(indent + 1) 
          + "\n"
          + "}";

        // You should NOT reach this line!
        break;
      
      default:
        break;
      }

      return std::string("Infinite { Undefined! }");
    }

    std::string toStringInline(int indent = 0) {
      return std::string("InfLoop { }");
    }
};

/*
  =================
    Iterator
  =================
*/

/*
  IterInfiniteAST: [...] >> {}
*/
class IterInfiniteAST : public StyioAST {
  IteratorType WhatType;
  std::vector<IdAST*> TmpVars;
  StyioAST* TheBlock;

  public:
    IterInfiniteAST(
      StyioAST* block):
      TheBlock(block)
      {
        WhatType = IteratorType::Original;
      }

    IterInfiniteAST(
      std::vector<IdAST*> tmpVars,
      StyioAST* block):
      TmpVars(tmpVars),
      TheBlock(block)
      {
        WhatType = IteratorType::Original;
      }

    StyioType hint() {
      return StyioType::IterInfLoop;
    }

    std::string toString(int indent = 0) {
      return std::string("Loop (Infinite) { ") 
      + "}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Loop (Infinite) { ")
      + "}";
    }
};

/*
  IterListAST: <List> >> {}
*/
class IterListAST : public StyioAST {
  StyioAST* TheList;
  std::vector<IdAST*> TmpVars;
  StyioAST* TheBlock;

  public:
    IterListAST(
      StyioAST* theList, 
      std::vector<IdAST*> tmpVars,
      StyioAST* block): 
      TheList(theList),
      TmpVars(tmpVars),
      TheBlock(block)
      {

      }

    StyioType hint() {
      return StyioType::IterList;
    }

    std::string toString(int indent = 0) {
      return std::string("Iter (List) { ") 
      + TheList -> toString()
      + " }";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Iter (List) { ") 
      + TheList -> toStringInline()
      + " }";
    }
};

/*
  IterRangeAST: Range >> {}
*/
class IterRangeAST : public StyioAST {
  StyioAST* TheRange;
  std::vector<IdAST*> TmpVars;
  StyioAST* TheBlock;

  public:
    IterRangeAST(
      StyioAST* theRange, 
      std::vector<IdAST*> tmpVars,
      StyioAST* block): 
      TheRange(theRange),
      TmpVars(tmpVars),
      TheBlock(block)
      {

      }

    IterRangeAST(
      StyioAST* theRange, 
      std::vector<IdAST*> tmpVars): 
      TheRange(theRange),
      TmpVars(tmpVars)
      {

      }

    StyioType hint() {
      return StyioType::IterRange;
    }

    std::string toString(int indent = 0) {
      return std::string("Iter (Range) { ") 
      + TheRange -> toString()
      + " }";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Iter (Range) { ") 
      + TheRange -> toStringInline()
      + " }";
    }
};

#endif