#ifndef STYIO_AST_H_
#define STYIO_AST_H_

/*
StyioAST
*/
class StyioAST {
  public:
    virtual ~StyioAST() {}

    virtual std::string toString() {
      return "Styio.Base {}";
    }

    virtual std::string toStringInline() {
      return "Styio.Base {}";
    }
};

/*
IdAST
*/
class IdAST : public StyioAST {
  std::string Id;

  public:
    IdAST(const std::string &id) : Id(id) {}

    std::string toString() {
      return std::string("ID { ") + Id + " }";
    }

    std::string toStringInline() {
      return "<ID: " + Id + ">";
    }
};

/*
IntAST
*/
class IntAST : public StyioAST {
  int Value;

  public:
    IntAST(int val) : Value(val) {}

    std::string toString() {
      return "Int { " + std::to_string(Value) + " }";
    }

    std::string toStringInline() {
      return std::to_string(Value);
    }
};

/*
FloatAST
*/
class FloatAST : public StyioAST {
  double Value;

  public:
    FloatAST(double val) : Value(val) {}

    std::string toString() {
      return "Float { " + std::to_string(Value) + " }";
    }

    std::string toStringInline() {
      return std::to_string(Value);
    }
};

/*
StringAST
*/
class StringAST : public StyioAST {
  std::string Value;

  public:
    StringAST(std::string val) : Value(val) {}

    std::string toString() {
      return "String { \"" + Value + "\" }";
    }

    std::string toStringInline() {
      return "<String: \"" + Value + "\">";
    }
};

/*
AssignAST
*/
class AssignAST : public StyioAST {
  IdAST* varId;
  StyioAST* valExpr;

  public:
    AssignAST(IdAST* var, StyioAST* val) : varId(var), valExpr(val) {}

    std::string toString(int indent = 0) {
      return std::string("Assign (Mutable) {\n") 
        + std::string(2, ' ') + "| Var: " 
        + varId -> toString() 
        + "\n"
        + std::string(2, ' ') + "| Op:  " 
        + "="
        + "\n"
        + std::string(2, ' ') + "| Val: " 
        + valExpr -> toString() 
        + "\n"
        + "}";
    }
};

/*
FinalAssignAST
*/
class FinalAssignAST : public StyioAST {
  IdAST* varId;
  StyioAST* valExpr;

  public:
    FinalAssignAST(IdAST* var, StyioAST* val) : varId(var), valExpr(val) {}

    std::string toString(int indent = 0) {
      return std::string("Assign (Final) {\n") 
        + std::string(2, ' ') + "| Var: " 
        + varId -> toString() 
        + "\n"
        + std::string(2, ' ') + "| Op:  " 
        + ":="
        + "\n"
        + std::string(2, ' ') + "| Val: " 
        + valExpr -> toString() 
        + "\n"
        + "}";
    }
};

/*
BinOpAST
*/
class BinOpAST : public StyioAST {
  StyioToken Op;
  StyioAST *LHS;
  StyioAST *RHS;

  public:
    BinOpAST(StyioToken op, StyioAST* lhs, StyioAST* rhs): Op(op), LHS(lhs), RHS(rhs) {}

    std::string toString(int indent = 0) {
      return std::string("BinOp {\n") 
        + std::string(2, ' ') + "| LHS: "
        + LHS -> toString() 
        + "\n"
        + std::string(2, ' ') + "| Op:  "
        + reprToken(Op)
        + "\n"
        + std::string(2, ' ') + "| RHS: "
        + RHS -> toString()  
        + "\n} ";
    }
};

/*
VarDefAST
*/
class VarDefAST : public StyioAST {
  std::vector<IdAST*> Vars;

  public:
    VarDefAST(std::vector<IdAST*> vars): Vars(vars) {}

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
};

/*
ValExprAST
*/
class ValExprAST : public StyioAST {
  StyioAST* Value;

  public:
    ValExprAST(StyioAST* value): Value(value) {}

    std::string toString(int indent = 0) {
      return std::string("ValExpr { ") + Value -> toString() + " } ";
    }
};

/*
BlockAST
*/
class BlockAST : public StyioAST {
  std::vector<StyioAST*> Stmts;
  StyioAST* Expr;

  public:
    BlockAST() {}

    BlockAST(StyioAST* expr): Expr(expr) {}

    BlockAST(std::vector<StyioAST*> stmts, StyioAST* expr): Stmts(stmts), Expr(expr) {}

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
};

/*
DependencyAST
*/
class DependencyAST : public StyioAST {
  std::vector<std::string> DependencyPaths;

  public:
    DependencyAST(std::vector<std::string> paths): DependencyPaths(paths) {}

    std::string toString(int indent = 0) {
      std::string dependencyPathsStr;

      for(int i=0; i < DependencyPaths.size(); i++) {
        dependencyPathsStr += std::string(2, ' ') + "| ";
        dependencyPathsStr += DependencyPaths[i];
        dependencyPathsStr += "\n";
      };

      return std::string("Dependencies {\n")
        + dependencyPathsStr
        + "\n} ";
    }
};

/*
StdOutAST
*/
class StdOutAST : public StyioAST {
  StringAST* Output;

  public:
    StdOutAST(StringAST* output): Output(output) {}

    std::string toString(int indent = 0) {
      return std::string("stdout {\n")
        + std::string(2, ' ') + "| "
        + Output -> toString()
        + "\n}";
    }
};

/*
LoopAST
*/
class LoopAST : public StyioAST {
  StyioAST* Start;
  StyioAST* End;
  StyioAST* Step;
  BlockAST* Block;

  public:
    LoopAST(IntAST* start, IntAST* step, BlockAST* block): Start(start), Step(step), Block(block) {}

    std::string toString(int indent = 0) {
      return std::string("Loop {\n")
        + std::string(2, ' ') + "| Start: " + Start -> toString() + "\n"
        + std::string(2, ' ') + "| End: " + End -> toString() + "\n"
        + std::string(2, ' ') + "| Step: " + Step -> toString() + "\n"
        + std::string(2, ' ') + "| Block: " + Block -> toString() + "\n"
        + "\n}";
    }
};

/*
ListAST
*/
class ListAST : public StyioAST {
  std::vector<StyioAST*> Elems;

  public:
    ListAST(std::vector<StyioAST*> elems): Elems(elems) {}

    std::string toString(int indent = 0) {
      std::string ElemStr;

      for(int i=0; i < Elems.size(); i++) {
        ElemStr += std::string(2, ' ') + "| ";
        ElemStr += Elems[i] -> toString();
        ElemStr += "\n";
      };

      return std::string("List {\n")
        + ElemStr
        + "}";
    }
};

/*
InfiniteAST
  incEl Increment Element
*/
class InfiniteAST : public StyioAST {
  IdAST* IncEl;

  public:
    InfiniteAST() {}

    std::string toString(int indent = 0) {
      return std::string("Infinite { }");
    }
};

/*
EmptyListAST
*/
class EmptyListAST : public StyioAST {
  public:
    EmptyListAST() {}

    std::string toString(int indent = 0) {
      return std::string("List(Empty) { }");
    }

    std::string toStringInline(int indent = 0) {
      return std::string("List(Empty) { }");
    }
};

#endif