#pragma once
#ifndef STYIO_AST_H_
#define STYIO_AST_H_

// // [C++ STL]
// #include <string>
// #include <memory>
// #include <vector>

// // [Styio]
#include "../StyioToken/Token.hpp"

// [LLVM]
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"

/*
  Styio generator (Forward Declaration)
*/
class StyioToLLVM;

/*
  StyioAST: Styio Base AST
*/
class StyioAST {
  public:
    virtual ~StyioAST() {}

    virtual NodeHint hint() = 0;

    virtual std::string toString(int indent = 0, bool colorful = false) = 0;

    virtual std::string toStringInline(int indent = 0, bool colorful = false) = 0;

    virtual void accept(StyioToLLVM* generator) = 0;
};

/*
  - CasesAST
  - SideBlockAST
  - MainBlockAST
*/

class CasesAST : public StyioAST {
  std::vector<std::tuple<std::unique_ptr<StyioAST>, std::unique_ptr<StyioAST>>> Cases;
  std::unique_ptr<StyioAST> LastExpr;

  public:
    CasesAST(
      std::unique_ptr<StyioAST> expr): 
      LastExpr(std::move(expr)
    ) {

    }

    CasesAST(
      std::vector<std::tuple<std::unique_ptr<StyioAST>, std::unique_ptr<StyioAST>>> cases,
      std::unique_ptr<StyioAST> expr): 
      Cases(std::move(cases)),
      LastExpr(std::move(expr)
    ) {

    }

    NodeHint hint() {
      return NodeHint::Cases;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

class SideBlockAST : public StyioAST {
  std::unique_ptr<StyioAST> Resources;
  std::vector<std::unique_ptr<StyioAST>> Stmts;

  public:
    SideBlockAST(
      std::unique_ptr<StyioAST> resources,
      std::vector<std::unique_ptr<StyioAST>> stmts): 
      Resources(std::move(resources)),
      Stmts(std::move(stmts)
    ) {

    }

    SideBlockAST(
      std::vector<std::unique_ptr<StyioAST>> stmts): 
      Stmts(std::move(stmts)
    ) {
        
    }

    NodeHint hint() {
      return NodeHint::Block;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

class MainBlockAST : public StyioAST {
  std::unique_ptr<StyioAST> Resources;
  std::vector<std::unique_ptr<StyioAST>> Stmts;

  public:
    MainBlockAST(
      std::unique_ptr<StyioAST> resources,
      std::vector<std::unique_ptr<StyioAST>> stmts): 
      Resources(std::move(resources)),
      Stmts(std::move(stmts)
    ) {

    }

    MainBlockAST(
      std::vector<std::unique_ptr<StyioAST>> stmts): 
      Stmts(std::move(stmts)
    ) {
        
    }

    NodeHint hint() {
      return NodeHint::MainBlock;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  =================
    None / Empty
  =================
*/

class TrueAST : public StyioAST {

  public:
    TrueAST () {}

    NodeHint hint() {
      return NodeHint::True;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioToLLVM* generator);
};

class FalseAST : public StyioAST {

  public:
    FalseAST () {}

    NodeHint hint() {
      return NodeHint::False;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioToLLVM* generator);
};

/*
  NoneAST: None / Null / Nil
*/
class NoneAST : public StyioAST {

  public:
    NoneAST () {}

    NodeHint hint() {
      return NodeHint::None;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioToLLVM* generator);
};

class EndAST : public StyioAST {

  public:
    EndAST () {}

    NodeHint hint() {
      return NodeHint::End;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioToLLVM* generator);
};

/*
  EmptyAST: Empty Tuple / List / Set
*/
class EmptyAST : public StyioAST {
  public:
    EmptyAST() {}

    NodeHint hint() {
      return NodeHint::Empty;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioToLLVM* generator);
};

/*
  EmptyBlockAST: Block
*/
class EmptyBlockAST : public StyioAST {
  public:
    EmptyBlockAST() {}

    NodeHint hint() {
      return NodeHint::EmptyBlock;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioToLLVM* generator);
};

class BreakAST : public StyioAST {

  public:
    BreakAST () {}

    NodeHint hint() {
      return NodeHint::Break;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioToLLVM* generator);
};

class PassAST : public StyioAST {

  public:
    PassAST () {}

    NodeHint hint() {
      return NodeHint::Pass;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioToLLVM* generator);
};

class ReturnAST : public StyioAST {
  std::unique_ptr<StyioAST> Expr;

  public:
    ReturnAST (
      std::unique_ptr<StyioAST> expr): 
      Expr(std::move(expr)) 
    {

    }

    NodeHint hint() {
      return NodeHint::Return;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioToLLVM* generator);
};

class CommentAST : public StyioAST {
  std::string Text;

  public:
    CommentAST (std::string text): Text(text) {}

    NodeHint hint() {
      return NodeHint::Comment;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioToLLVM* generator);
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

    NodeHint hint() {
      return NodeHint::Id;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return std::string("id { ") + Id + " }";
    }

    void accept(StyioToLLVM* generator);
};

class ArgAST : public StyioAST {
  std::unique_ptr<IdAST> Id;

  public:
    ArgAST(
      std::unique_ptr<IdAST> id): 
      Id(std::move(id)) 
      {

      }

    NodeHint hint() {
      return NodeHint::Arg;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioToLLVM* generator);
};

class KwArgAST : public StyioAST {
  std::unique_ptr<IdAST> Id;

  public:
    KwArgAST(
      std::unique_ptr<IdAST> id): 
      Id(std::move(id)) 
      {

      }

    NodeHint hint() {
      return NodeHint::KwArg;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioToLLVM* generator);
};

class VarsTupleAST : public StyioAST {
  std::vector<std::unique_ptr<StyioAST>> Vars;

  public:
    VarsTupleAST(
      std::vector<std::unique_ptr<StyioAST>> vars): 
      Vars(std::move(vars)) { }

    NodeHint hint() {
      return NodeHint::Fill;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

class TypeAST : public StyioAST {
  std::string Type;

  public:
    TypeAST(
      const std::string& type): 
      Type(type) {}

    NodeHint hint() {
      return NodeHint::Type;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

class TypedVarAST : public StyioAST {
  std::unique_ptr<IdAST> Id;
  std::unique_ptr<TypeAST> Type;

  public:
    TypedVarAST(
      std::unique_ptr<IdAST> id, 
      std::unique_ptr<TypeAST> type) : 
      Id(std::move(id)),
      Type(std::move(type)) 
      {

      }

    NodeHint hint() {
      return NodeHint::TypedVar;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
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
  std::string Value;

  public:
    IntAST(std::string val) : Value(val) {}

    NodeHint hint() {
      return NodeHint::Int;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + " { " + Value + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  FloatAST: Float
*/
class FloatAST : public StyioAST {
  std::string Value;

  public:
    FloatAST(std::string val) : Value(val) {}

    NodeHint hint() {
      return NodeHint::Float;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + " { " + Value + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  CharAST: Character
*/
class CharAST : public StyioAST {
  std::string Value;

  public:
    CharAST(std::string val) : Value(val) {}

    NodeHint hint() {
      return NodeHint::Char;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + " { \'" + Value + "\' }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  StringAST: String
*/
class StringAST : public StyioAST {
  std::string Value;

  public:
    StringAST(std::string val) : Value(val) {}

    NodeHint hint() {
      return NodeHint::String;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return "\"" + Value + "\"";
    }

    void accept(StyioToLLVM* generator);
};

/*
  FmtStrAST: String
*/
class FmtStrAST : public StyioAST {
  std::vector<std::string> Fragments;
  std::vector<std::unique_ptr<StyioAST>> Exprs;

  public:
    FmtStrAST(
      std::vector<std::string> fragments,
      std::vector<std::unique_ptr<StyioAST>> expressions) : 
      Fragments(fragments), 
      Exprs(std::move(expressions)) {}

    NodeHint hint() {
      return NodeHint::FmtStr;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + " { \"" + "\" }";
    }

    void accept(StyioToLLVM* generator);
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
  std::unique_ptr<StringAST> Path;

  public:
    ExtPathAST (
      std::unique_ptr<StringAST> path): 
      Path(std::move(path)) 
      {

      }

    NodeHint hint() {
      return NodeHint::ExtPath;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") + Path -> toStringInline(indent + 1, colorful) + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  ExtLinkAST: (Web) Link
*/
class ExtLinkAST : public StyioAST {
  std::string Link;

  public:
    ExtLinkAST (std::string link): Link(link) {}

    NodeHint hint() {
      return NodeHint::ExtLink;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  =================
    Collection
      Tuple
      List
      Set
  =================
*/

/*
  ListAST: List (Extendable)
*/
class ListAST : public StyioAST {
  std::vector<std::unique_ptr<StyioAST>> Elems;

  public:
    ListAST(
      std::vector<std::unique_ptr<StyioAST>> elems): 
      Elems(std::move(elems)) 
      {

      }

    NodeHint hint() {
      return NodeHint::List;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

class TupleAST : public StyioAST {
  std::vector<std::unique_ptr<StyioAST>> Elems;

  public:
    TupleAST(
      std::vector<std::unique_ptr<StyioAST>> elems): 
      Elems(std::move(elems)) 
      {

      }

    NodeHint hint() {
      return NodeHint::Tuple;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

class SetAST : public StyioAST {
  std::vector<std::unique_ptr<StyioAST>> Elems;

  public:
    SetAST(
      std::vector<std::unique_ptr<StyioAST>> elems): 
      Elems(std::move(elems)) 
      {

      }

    NodeHint hint() {
      return NodeHint::Set;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  RangeAST: Loop
*/
class RangeAST : public StyioAST 
{
  std::unique_ptr<StyioAST> StartVal;
  std::unique_ptr<StyioAST> EndVal;
  std::unique_ptr<StyioAST> StepVal;

  public:
    RangeAST(
      std::unique_ptr<StyioAST> start, 
      std::unique_ptr<StyioAST> end, 
      std::unique_ptr<StyioAST> step): 
      StartVal(std::move(start)), 
      EndVal(std::move(end)), 
      StepVal(std::move(step)) 
      {

      }

    NodeHint hint() {
      return NodeHint::Range;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
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
  std::unique_ptr<StyioAST> Value;

  public:
    SizeOfAST(
      std::unique_ptr<StyioAST> value): 
      Value(std::move(value)) 
      {
        
      }

    NodeHint hint() {
      return NodeHint::SizeOf;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
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
  NodeHint Op;
  std::unique_ptr<StyioAST> LHS;
  std::unique_ptr<StyioAST> RHS;

  public:
    BinOpAST(
      NodeHint op, 
      std::unique_ptr<StyioAST> lhs, 
      std::unique_ptr<StyioAST> rhs): 
      Op(op),
      LHS(std::move(lhs)), 
      RHS(std::move(rhs)) 
      {

      }

    NodeHint hint() {
      return Op;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" {") 
        + " LHS: " + LHS -> toStringInline(indent) 
        + " | RHS: " + RHS -> toStringInline(indent)  
        + " }";
    }

    void accept(StyioToLLVM* generator);
};

class BinCompAST: public StyioAST 
{
  CompType CompSign;
  std::unique_ptr<StyioAST> LhsExpr;
  std::unique_ptr<StyioAST> RhsExpr;

  public:
    BinCompAST(
      CompType sign, 
      std::unique_ptr<StyioAST> lhs, 
      std::unique_ptr<StyioAST> rhs): 
      CompSign(sign), 
      LhsExpr(std::move(lhs)), 
      RhsExpr(std::move(rhs)) 
      {

      }

    NodeHint hint() {
      return NodeHint::Compare;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

class CondAST: public StyioAST 
{
  LogicType LogicOp;
  
  /*
    RAW: expr
    NOT: !(expr)
  */
  std::unique_ptr<StyioAST> ValExpr;

  /*
    AND: expr && expr
    OR : expr || expr
  */
  std::unique_ptr<StyioAST> LhsExpr;
  std::unique_ptr<StyioAST> RhsExpr;

  public:
    CondAST(
      LogicType op, 
      std::unique_ptr<StyioAST> val): 
      LogicOp(op), 
      ValExpr(std::move(val))
      {

      }
    
    CondAST(
      LogicType op, 
      std::unique_ptr<StyioAST> lhs, 
      std::unique_ptr<StyioAST> rhs): 
      LogicOp(op), 
      LhsExpr(std::move(lhs)), 
      RhsExpr(std::move(rhs)) 
      {

      }

    NodeHint hint() {
      return NodeHint::Condition;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

class CallAST : public StyioAST {
  std::unique_ptr<StyioAST> Func;

  public:
    CallAST(
      std::unique_ptr<StyioAST> func) : 
      Func(std::move(func)) {}

    NodeHint hint() {
      return NodeHint::Call;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

class ListOpAST : public StyioAST 
{
  NodeHint OpType;
  std::unique_ptr<StyioAST> TheList;

  std::unique_ptr<StyioAST> Slot1;
  std::unique_ptr<StyioAST> Slot2;

  public:
    /*
      Get_Reversed
        [<]
    */
    ListOpAST(
      NodeHint opType,
      std::unique_ptr<StyioAST> theList): 
      OpType(opType),
      TheList(std::move(theList)) 
      {

      }

    /*
      Access_By_Index
        [index]

      Access_By_Name
        ["name"]

      Append_Value
        [+: value]

      Remove_Item_By_Index
        [-: index] 

      Get_Index_By_Value
        [?= value]

      Get_Indices_By_Many_Values
        [?^ values]

      Remove_Item_By_Value
        [-: ?= value]

      Remove_Items_By_Many_Indices
        [-: (i0, i1, ...)]

      Remove_Items_By_Many_Values
        [-: ?^ (v0, v1, ...)]

      Get_Index_By_Item_From_Right
        [[<] ?= value]

      Remove_Item_By_Value_From_Right
        [[<] -: ?= value]

      Remove_Items_By_Many_Values_From_Right
        [[<] -: ?^ (v0, v1, ...)]
    */
    ListOpAST(
      NodeHint opType, 
      std::unique_ptr<StyioAST> theList, 
      std::unique_ptr<StyioAST> item): 
      OpType(opType), 
      TheList(std::move(theList)), 
      Slot1(std::move(item)) 
      {

      }

    /*
      Insert_Item_By_Index
        [+: index <- value]
    */
    ListOpAST(
      NodeHint opType, 
      std::unique_ptr<StyioAST> theList, 
      std::unique_ptr<StyioAST> index, 
      std::unique_ptr<StyioAST> value): 
      OpType(opType), 
      TheList(std::move(theList)), 
      Slot1(std::move(index)), 
      Slot2(std::move(value)) 
      {

      }

    NodeHint hint() {
      return OpType;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  =================
    Statement: Resources
  =================
*/

/*
  ResourceAST: Resources

  Definition = 
    Declaration (Neccessary)
    + | Assignment (Optional)
      | Import (Optional)
*/
class ResourceAST : public StyioAST {
  std::vector<std::unique_ptr<StyioAST>> Resources;

  public:
    ResourceAST(
      std::vector<std::unique_ptr<StyioAST>> resources): 
      Resources(std::move(resources)) 
      {

      }

    NodeHint hint() {
      return NodeHint::Resources;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
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
  std::unique_ptr<IdAST> varId;
  std::unique_ptr<StyioAST> valExpr;

  public:
    FlexBindAST(
      std::unique_ptr<IdAST> var, 
      std::unique_ptr<StyioAST> val) : 
      varId(std::move(var)), 
      valExpr(std::move(val)) {}

    NodeHint hint() {
      return NodeHint::MutBind;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  FinalBindAST: Immutable Assignment (Final Binding)
*/
class FinalBindAST : public StyioAST {
  std::unique_ptr<IdAST> varId;
  std::unique_ptr<StyioAST> valExpr;

  public:
    FinalBindAST(
      std::unique_ptr<IdAST> var, 
      std::unique_ptr<StyioAST> val) : 
      varId(std::move(var)), 
      valExpr(std::move(val)) 
      {

      }

    NodeHint hint() {
      return NodeHint::FixBind;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
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
  StructAST: Structure
*/
class StructAST : public StyioAST {
  std::unique_ptr<IdAST> FName;
  std::unique_ptr<VarsTupleAST> FVars;
  std::unique_ptr<StyioAST> FBlock;

  public:
    StructAST(
      std::unique_ptr<IdAST> name, 
      std::unique_ptr<VarsTupleAST> vars,
      std::unique_ptr<StyioAST> block) : 
      FName(std::move(name)), 
      FVars(std::move(vars)),
      FBlock(std::move(block))
      {

      }

    NodeHint hint() {
      return NodeHint::Struct;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
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
  std::unique_ptr<IdAST> varId;
  std::unique_ptr<StyioAST> valExpr;

  public:
    ReadFileAST(
      std::unique_ptr<IdAST> var, 
      std::unique_ptr<StyioAST> val) : 
      varId(std::move(var)), 
      valExpr(std::move(val)) 
      {

      }

    NodeHint hint() {
      return NodeHint::ReadFile;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  PrintAST: Write to Standard Output (Print)
*/
class PrintAST : public StyioAST {
  std::vector<std::unique_ptr<StyioAST>> Exprs;

  public:
    PrintAST(
      std::vector<std::unique_ptr<StyioAST>> exprs): 
      Exprs(std::move(exprs)) {

      }

    NodeHint hint() {
      return NodeHint::Print;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
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
    ExtPackAST(
      std::vector<std::string> paths): 
      PackPaths(paths) 
      {

      }

    NodeHint hint() {
      return NodeHint::ExtPack;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  =================
    Abstract Level: Block 
  =================
*/

class CondFlowAST : public StyioAST {
  std::unique_ptr<CondAST> CondExpr;
  std::unique_ptr<StyioAST> ThenBlock;
  std::unique_ptr<StyioAST> ElseBlock;

  public:
    NodeHint WhatFlow;

    CondFlowAST(
      NodeHint whatFlow,
      std::unique_ptr<CondAST> condition,
      std::unique_ptr<StyioAST> block): 
      WhatFlow(whatFlow),
      CondExpr(std::move(condition)),
      ThenBlock(std::move(block))
      {

      }

    CondFlowAST(
      NodeHint whatFlow,
      std::unique_ptr<CondAST> condition,
      std::unique_ptr<StyioAST> blockThen,
      std::unique_ptr<StyioAST> blockElse): 
      WhatFlow(whatFlow),
      CondExpr(std::move(condition)),
      ThenBlock(std::move(blockThen)),
      ElseBlock(std::move(blockElse))
      {

      }

    NodeHint hint() {
      return WhatFlow;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  =================
    Abstract Level: Layers
  =================
*/

/*
  ICBSLayerAST: Intermediate Connection Between Scopes

  Run: Block
    => {

    }

  Fill: Fill + Block
    >> () => {}

  MatchValue: Fill + CheckEq + Block
    >> Element(Single) ?= ValueExpr(Single) => {
      ...
    }
    
    For each step of iteration, check if the element match the value expression, 
    if match case is true, then execute the branch. 

  MatchCases: Fill + Cases
    >> Element(Single) ?= {
      v0 => {}
      v1 => {}
      _  => {}
    }
    
    For each step of iteration, check if the element match any value expression, 
    if match case is true, then execute the branch. 

  ExtraIsin: Fill + CheckIsIn
    >> Element(Single) ?^ IterableExpr(Collection) => {
      ...
    }

    For each step of iteration, check if the element is in the following collection,
    if match case is true, then execute the branch. 

  ExtraCond: Fill + CondFlow
    >> Elements ? (Condition) \t\ {
      ...
    }
    
    For each step of iteration, check the given condition, 
    if condition is true, then execute the branch. 

    >> Elements ? (Condition) \f\ {
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
  std::unique_ptr<StyioAST> Value;

  public:
    CheckEqAST(
      std::unique_ptr<StyioAST> value): 
      Value(std::move(value))
      {

      }

    NodeHint hint() {
      return NodeHint::CheckEq;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

class CheckIsInAST : public StyioAST {
  std::unique_ptr<StyioAST> Iterable;

  public:
    CheckIsInAST(
      std::unique_ptr<StyioAST> value): 
      Iterable(std::move(value))
      {

      }

    NodeHint hint() {
      return NodeHint::CheckIsin;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  FromToAST
*/
class FromToAST : public StyioAST {
  std::unique_ptr<StyioAST> FromWhat;
  std::unique_ptr<StyioAST> ToWhat;

  public:
    FromToAST(
      std::unique_ptr<StyioAST> from_expr,
      std::unique_ptr<StyioAST> to_expr) : 
      FromWhat(std::move(from_expr)), 
      ToWhat(std::move(to_expr)) {}

    NodeHint hint() {
      return NodeHint::FromTo;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  ExtraEq:
    ?=

  ExtraIsIn:
    ?^
  
  ExtraCond:
    ?()
*/

class ForwardAST : public StyioAST {
  std::unique_ptr<VarsTupleAST> TmpVars;
  std::unique_ptr<CheckEqAST> ExtraEq;
  std::unique_ptr<CheckIsInAST> ExtraIsin;
  std::unique_ptr<CondFlowAST> ExtraCond;
  std::unique_ptr<StyioAST> ThenExpr;

  private:
    NodeHint Type = NodeHint::Forward;

  public:
    ForwardAST(
      std::unique_ptr<StyioAST> expr):
      ThenExpr(std::move(expr))
      { Type = NodeHint::Forward; }

    ForwardAST(
      std::unique_ptr<CheckEqAST> value,
      std::unique_ptr<StyioAST> whatnext):
      ExtraEq(std::move(value)),
      ThenExpr(std::move(whatnext))
      { Type = NodeHint::If_Equal_To_Forward; }

    ForwardAST(
      std::unique_ptr<CheckIsInAST> isin,
      std::unique_ptr<StyioAST> whatnext): 
      ExtraIsin(std::move(isin)),
      ThenExpr(std::move(whatnext))
      { Type = NodeHint::If_Is_In_Forward; }

    ForwardAST(
      std::unique_ptr<CasesAST> cases): 
      ThenExpr(std::move(cases))
      { Type = NodeHint::Cases_Forward; }

    ForwardAST(
      std::unique_ptr<CondFlowAST> condflow): 
      ExtraCond(std::move(condflow))
      {
        if ((condflow -> WhatFlow) == NodeHint::CondFlow_True) {
          Type = NodeHint::If_True_Forward; }
        else if ((condflow -> WhatFlow) == NodeHint::CondFlow_False) {
          Type = NodeHint::If_False_Forward; }
        else {
          Type = NodeHint::If_Both_Forward; }
      }

    ForwardAST(
      std::unique_ptr<VarsTupleAST> vars,
      std::unique_ptr<StyioAST> whatnext): 
      TmpVars(std::move(vars)),
      ThenExpr(std::move(whatnext))
      { Type = NodeHint::Fill_Forward; }

    ForwardAST(
      std::unique_ptr<VarsTupleAST> vars,
      std::unique_ptr<CheckEqAST> value,
      std::unique_ptr<StyioAST> whatnext): 
      TmpVars(std::move(vars)),
      ExtraEq(std::move(value)),
      ThenExpr(std::move(whatnext))
      { Type = NodeHint::Fill_If_Equal_To_Forward; }

    ForwardAST(
      std::unique_ptr<VarsTupleAST> vars,
      std::unique_ptr<CheckIsInAST> isin,
      std::unique_ptr<StyioAST> whatnext): 
      TmpVars(std::move(vars)),
      ExtraIsin(std::move(isin)),
      ThenExpr(std::move(whatnext))
      { Type = NodeHint::Fill_If_Is_in_Forward; }

    
    ForwardAST(
      std::unique_ptr<VarsTupleAST> vars,
      std::unique_ptr<CasesAST> cases): 
      TmpVars(std::move(vars)),
      ThenExpr(std::move(cases))
      { Type = NodeHint::Fill_Cases_Forward; }

    ForwardAST(
      std::unique_ptr<VarsTupleAST> vars,
      std::unique_ptr<CondFlowAST> condflow): 
      TmpVars(std::move(vars)),
      ExtraCond(std::move(condflow))
      {
        if ((condflow -> WhatFlow) == NodeHint::CondFlow_True) {
          Type = NodeHint::Fill_If_True_Forward; }
        else if ((condflow -> WhatFlow) == NodeHint::CondFlow_False) {
          Type = NodeHint::Fill_If_False_Forward; }
        else {
          Type = NodeHint::Fill_If_Both_Forward; }
      }

    NodeHint hint() {
      return Type;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
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
  std::unique_ptr<StyioAST> Start;
  std::unique_ptr<StyioAST> IncEl;

  public:
    InfiniteAST() 
    {
      WhatType = InfiniteType::Original;
    }

    InfiniteAST(
      std::unique_ptr<StyioAST> start, 
      std::unique_ptr<StyioAST> incEl): 
      Start(std::move(start)), 
      IncEl(std::move(incEl)) 
      {
        WhatType = InfiniteType::Incremental;
      }

    NodeHint hint() {
      return NodeHint::Infinite;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  FuncAST: Function
*/
class FuncAST : public StyioAST {
  std::unique_ptr<IdAST> FName;
  std::unique_ptr<StyioAST> Forward;

  bool FwithName;
  bool FisFinal;

  public:
    FuncAST(
      std::unique_ptr<StyioAST> forward,
      bool isFinal) :  
      Forward(std::move(forward)),
      FisFinal(isFinal)
      {
        FwithName = false;
      }

    FuncAST(
      std::unique_ptr<IdAST> name, 
      std::unique_ptr<StyioAST> forward,
      bool isFinal) : 
      FName(std::move(name)), 
      Forward(std::move(forward)),
      FisFinal(isFinal)
      {
        FwithName = true;
      }

    NodeHint hint() {
      return NodeHint::Func;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  =================
    Iterator
  =================
*/

/*
  IterInfinite: [...] >> {}
*/
class LoopAST : public StyioAST {
  std::unique_ptr<StyioAST> Forward;

  public:
    LoopAST(
      std::unique_ptr<StyioAST> expr):
      Forward(std::move(expr))
      {
        
      }

    NodeHint hint() {
      return NodeHint::Loop;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

/*
  IterBounded: <List/Range> >> {}
*/
class IterAST : public StyioAST {
  std::unique_ptr<StyioAST> Collection;
  std::unique_ptr<StyioAST> Forward;

  public:
    IterAST(
      std::unique_ptr<StyioAST> collection,
      std::unique_ptr<StyioAST> forward): 
      Collection(std::move(collection)),
      Forward(std::move(forward))
      {

      }

    NodeHint hint() {
      return NodeHint::Iterator;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioToLLVM* generator);
};

class Styiogenerator {
  public:
    Styiogenerator () {}

    void visit_true(TrueAST* ast);

    void visit_false(FalseAST* ast);

    void visit_none(NoneAST* ast);

    void visit_end(EndAST* ast);

    void visit_empty(EmptyAST* ast);

    void visit_empty_block(EmptyBlockAST* ast);

    void visit_pass(PassAST* ast);

    void visit_break(BreakAST* ast);

    void visit_return(ReturnAST* ast);

    void visit_comment(CommentAST* ast);

    void visit_id(IdAST* ast);

    void visit_arg(ArgAST* ast);

    void visit_kwarg(KwArgAST* ast);

    void visit_vars_tuple(VarsTupleAST* ast);

    void visit_type(TypeAST* ast);

    void visit_typed_var(TypedVarAST* ast);

    void visit_int(IntAST* ast);

    void visit_float(FloatAST* ast);

    void visit_char(CharAST* ast);

    void visit_string(StringAST* ast);

    void visit_fmt_str(FmtStrAST* ast);

    void visit_ext_path(ExtPathAST* ast);

    void visit_ext_link(ExtLinkAST* ast);

    void visit_list(ListAST* ast);

    void visit_tuple(TupleAST* ast);

    void visit_set(SetAST* ast);

    void visit_range(RangeAST* ast);

    void visit_size_of(SizeOfAST* ast);

    void visit_bin_op(BinOpAST* ast);

    void visit_bin_comp(BinCompAST* ast);

    void visit_cond(CondAST* ast);

    void visit_call(CallAST* ast);

    void visit_list_op(ListOpAST* ast);

    void visit_resources(ResourceAST* ast);

    void visit_flex_bind(FlexBindAST* ast);

    void visit_final_bind(FinalBindAST* ast);

    void visit_struct(StructAST* ast);

    void visit_read_file(ReadFileAST* ast);

    void visit_print(PrintAST* ast);

    void visit_ext_pack(ExtPackAST* ast);

    void visit_block(SideBlockAST* ast);

    void visit_cases(CasesAST* ast);

    void visit_cond_flow(CondFlowAST* ast);

    void visit_check_equal(CheckEqAST* ast);

    void visit_check_isin(CheckIsInAST* ast);

    void visit_from_to(FromToAST* ast);

    void visit_forward(ForwardAST* ast);

    void visit_infinite(InfiniteAST* ast);

    void visit_function(FuncAST* ast);

    void visit_loop(LoopAST* ast);

    void visit_iterator(IterAST* ast);

    void visit_main_block(MainBlockAST* ast);
};

class StyioToLLVM {
  std::unique_ptr<llvm::LLVMContext> llvm_ctx;
  std::unique_ptr<llvm::Module> llvm_mod;

  public:
    StyioToLLVM () {
      llvm_ctx = std::make_unique<llvm::LLVMContext>();
      llvm_mod = std::make_unique<llvm::Module>("styio", *llvm_ctx);
    }

    llvm::Value* gen_true(TrueAST* ast);

    llvm::Value* gen_false(FalseAST* ast);

    llvm::Value* gen_none(NoneAST* ast);

    llvm::Value* gen_end(EndAST* ast);

    llvm::Value* gen_empty(EmptyAST* ast);

    llvm::Value* gen_empty_block(EmptyBlockAST* ast);

    llvm::Value* gen_pass(PassAST* ast);

    llvm::Value* gen_break(BreakAST* ast);

    llvm::Value* gen_return(ReturnAST* ast);

    llvm::Value* gen_comment(CommentAST* ast);

    llvm::Value* gen_id(IdAST* ast);

    llvm::Value* gen_arg(ArgAST* ast);

    llvm::Value* gen_kwarg(KwArgAST* ast);

    llvm::Value* gen_vars_tuple(VarsTupleAST* ast);

    llvm::Value* gen_type(TypeAST* ast);

    llvm::Value* gen_typed_var(TypedVarAST* ast);

    llvm::Value* gen_int(IntAST* ast);

    llvm::Value* gen_float(FloatAST* ast);

    llvm::Value* gen_char(CharAST* ast);

    llvm::Value* gen_string(StringAST* ast);

    llvm::Value* gen_fmt_str(FmtStrAST* ast);

    llvm::Value* gen_ext_path(ExtPathAST* ast);

    llvm::Value* gen_ext_link(ExtLinkAST* ast);

    llvm::Value* gen_list(ListAST* ast);

    llvm::Value* gen_tuple(TupleAST* ast);

    llvm::Value* gen_set(SetAST* ast);

    llvm::Value* gen_range(RangeAST* ast);

    llvm::Value* gen_size_of(SizeOfAST* ast);

    llvm::Value* gen_bin_op(BinOpAST* ast);

    llvm::Value* gen_bin_comp(BinCompAST* ast);

    llvm::Value* gen_cond(CondAST* ast);

    llvm::Value* gen_call(CallAST* ast);

    llvm::Value* gen_list_op(ListOpAST* ast);

    llvm::Value* gen_resources(ResourceAST* ast);

    llvm::Value* gen_flex_bind(FlexBindAST* ast);

    llvm::Value* gen_final_bind(FinalBindAST* ast);

    llvm::Value* gen_struct(StructAST* ast);

    llvm::Value* gen_read_file(ReadFileAST* ast);

    llvm::Value* gen_print(PrintAST* ast);

    llvm::Value* gen_ext_pack(ExtPackAST* ast);

    llvm::Value* gen_block(SideBlockAST* ast);

    llvm::Value* gen_cases(CasesAST* ast);

    llvm::Value* gen_cond_flow(CondFlowAST* ast);

    llvm::Value* gen_check_equal(CheckEqAST* ast);

    llvm::Value* gen_check_isin(CheckIsInAST* ast);

    llvm::Value* gen_from_to(FromToAST* ast);

    llvm::Value* gen_forward(ForwardAST* ast);

    llvm::Value* gen_infinite(InfiniteAST* ast);

    llvm::Value* gen_function(FuncAST* ast);

    llvm::Value* gen_loop(LoopAST* ast);

    llvm::Value* gen_iterator(IterAST* ast);

    llvm::Value* gen_main_block(MainBlockAST* ast);

    llvm::Value* show();
};

#endif