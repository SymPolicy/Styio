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
      return "<ID: " + Id + ">";
    }
};

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
      return std::string("Path(File) { ") + Path + " }";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Path(File) { ") + Path + " }";
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
ListAST
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
MutAssignAST
*/
class MutAssignAST : public StyioAST {
  IdAST* varId;
  StyioAST* valExpr;

  public:
    MutAssignAST(IdAST* var, StyioAST* val) : varId(var), valExpr(val) {}

    StyioType hint() {
      return StyioType::MutAssign;
    }

    std::string toString(int indent = 0) {
      return std::string("Assign (Mutable) {\n") 
        + std::string(2, ' ') + "| Var: " 
        + varId -> toString(indent) 
        + "\n"
        + std::string(2, ' ') + "| Val: " 
        + valExpr -> toString(indent) 
        + "\n"
        + "}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Assign (Mutable) {\n") 
        + std::string(2, ' ') + "| Var: " 
        + varId -> toStringInline(indent) 
        + "; "
        + std::string(2, ' ') + "| Val: " 
        + valExpr -> toStringInline(indent) 
        + " }";
    }
};

/*
FixAssignAST
*/
class FixAssignAST : public StyioAST {
  IdAST* varId;
  StyioAST* valExpr;

  public:
    FixAssignAST(IdAST* var, StyioAST* val) : varId(var), valExpr(val) {}

    StyioType hint() {
      return StyioType::FixAssign;
    }

    std::string toString(int indent = 0) {
      return std::string("Assign (Final) {\n") 
        + std::string(2, ' ') + "| Var: " 
        + varId -> toString(indent) 
        + "\n"
        + std::string(2, ' ') + "| Val: " 
        + valExpr -> toString(indent) 
        + "\n"
        + "}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Assign (Final) { ") 
        + std::string(2, ' ') + "| Var: " 
        + varId -> toString(indent) 
        + "; "
        + std::string(2, ' ') + "| Val: " 
        + valExpr -> toString(indent) 
        + " }";
    }
};

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
BinOpAST
*/
class BinOpAST : public StyioAST {
  BinTok Op;
  StyioAST *LHS;
  StyioAST *RHS;

  public:
    BinOpAST(BinTok op, StyioAST* lhs, StyioAST* rhs): Op(op), LHS(lhs), RHS(rhs) {}

    StyioType hint() {
      return StyioType::BinOp;
    }

    std::string toString(int indent = 0) {
      return std::string("BinOp {\n") 
        + std::string(2, ' ') + "| LHS: "
        + LHS -> toString(indent) 
        + "\n"
        + std::string(2, ' ') + "| Op:  "
        + reprToken(Op)
        + "\n"
        + std::string(2, ' ') + "| RHS: "
        + RHS -> toString(indent)  
        + "\n} ";
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

/*
VarDefAST
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
BlockAST: Block
*/
class BlockAST : public StyioAST {
  std::vector<StyioAST*> Stmts;
  StyioAST* Expr;

  public:
    BlockAST() {}

    BlockAST(StyioAST* expr): Expr(expr) {}

    BlockAST(std::vector<StyioAST*> stmts, StyioAST* expr): Stmts(stmts), Expr(expr) {}

    StyioType hint() {
      return StyioType::Block;
    }

    std::string toString(int indent = 0) {
      std::string stmtStr;

      for (std::vector<StyioAST*>::iterator it = Stmts.begin(); 
        it != Stmts.end(); 
        ++it
      ) {
        stmtStr += (*it) -> toStringInline();
        stmtStr += "\n";
      };

      return std::string("Block {\n")
        + std::string(2, ' ') + "| Stmts: "
        + stmtStr
        + "\n"
        + std::string(2, ' ') + "| Expr:  "
        + Expr -> toString()  
        + "\n} ";
    }

    std::string toStringInline(int indent = 0) {
      std::string stmtStr;

      for (std::vector<StyioAST*>::iterator it = Stmts.begin(); 
        it != Stmts.end(); 
        ++it
      ) {
        stmtStr += (*it) -> toStringInline();
        stmtStr += " ;";
      };

      return std::string("Block { ")
        + std::string(2, ' ') + "| Stmts: "
        + stmtStr
        + " |"
        + std::string(2, ' ') + "| Expr:  "
        + Expr -> toString()  
        + " } ";
    }
};

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

      return std::string("Ext Pac {\n")
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

      return std::string("Dependencies { ")
        + pacPathStr
        + " } ";
    }
};

/*
WriteStdOutAST: Standard Output
*/
class WriteStdOutAST : public StyioAST {
  StringAST* Output;

  public:
    WriteStdOutAST(StringAST* output): Output(output) {}

    StyioType hint() {
      return StyioType::WriteStdOut;
    }

    std::string toString(int indent = 0) {
      return std::string("Write (StdOut) {\n")
        + std::string(2, ' ') + "| "
        + Output -> toString()
        + "\n}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("stdout { ")
        + std::string(2, ' ') + "| "
        + Output -> toString()
        + " }";
    }
};

/*
InfLoop: Infinite Loop
  incEl Increment Element
*/
class InfLoop : public StyioAST {
  StyioAST* Start;
  StyioAST* IncEl;

  public:
    InfLoop() {}

    InfLoop(StyioAST* start, StyioAST* incEl): Start(start), IncEl(incEl) {}

    StyioType hint() {
      return StyioType::InfLoop;
    }

    std::string toString(int indent = 0) {
      return std::string("InfLoop { }");
    }

    std::string toStringInline(int indent = 0) {
      return std::string("InfLoop { }");
    }
};

/*
RangeAST: Loop
*/
class RangeAST : public StyioAST {
  StyioAST* StartVal;
  StyioAST* EndVal;
  StyioAST* StepVal;

  public:
    RangeAST(StyioAST* start, StyioAST* end, StyioAST* step): StartVal(start), EndVal(end), StepVal(step) {}

    StyioType hint() {
      return StyioType::Range;
    }

    std::string toString(int indent = 0) {
      return std::string("Range {\n")
        + std::string(2, ' ') + "| Start: " + StartVal -> toString() + "\n"
        + std::string(2, ' ') + "| End: " + EndVal -> toString() + "\n"
        + std::string(2, ' ') + "| Step: " + StepVal -> toString() + "\n"
        + "\n}";
    }

    std::string toStringInline(int indent = 0) {
      return std::string("Loop {\n")
        + std::string(2, ' ') + "| Start: " + StartVal -> toString() + "\n"
        + std::string(2, ' ') + "| End: " + EndVal -> toString() + "\n"
        + std::string(2, ' ') + "| Step: " + StepVal -> toString() + "\n"
        + "\n}";
    }
};

/*

*/
class SizeOfAST : public StyioAST {
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

#endif