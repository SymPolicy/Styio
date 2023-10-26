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

class StyioVisitor;

/*
  StyioAST
*/
class StyioAST {
  public:
    virtual ~StyioAST() {}

    virtual StyioType hint() = 0;

    virtual std::string toString(int indent = 0, bool colorful = false) = 0;

    virtual std::string toStringInline(int indent = 0, bool colorful = false) = 0;

    virtual void accept(StyioVisitor* visitor) = 0;
};

/*
  =================
    None / Empty
  =================
*/

class TrueAST : public StyioAST {

  public:
    TrueAST () {}

    StyioType hint() {
      return StyioType::True;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioVisitor* visitor);
};

class FalseAST : public StyioAST {

  public:
    FalseAST () {}

    StyioType hint() {
      return StyioType::False;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioVisitor* visitor);
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

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioVisitor* visitor);
};

class EndAST : public StyioAST {

  public:
    EndAST () {}

    StyioType hint() {
      return StyioType::End;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioVisitor* visitor);
};

/*
  EmptyAST: Empty Tuple / List / Set
*/
class EmptyAST : public StyioAST {
  public:
    EmptyAST() {}

    StyioType hint() {
      return StyioType::Empty;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioVisitor* visitor);
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

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioVisitor* visitor);
};

class BreakAST : public StyioAST {

  public:
    BreakAST () {}

    StyioType hint() {
      return StyioType::Break;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioVisitor* visitor);
};

class PassAST : public StyioAST {

  public:
    PassAST () {}

    StyioType hint() {
      return StyioType::Pass;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioVisitor* visitor);
};

class ReturnAST : public StyioAST {
  std::unique_ptr<StyioAST> Expr;

  public:
    ReturnAST (
      std::unique_ptr<StyioAST> expr): 
      Expr(std::move(expr)) 
    {

    }

    StyioType hint() {
      return StyioType::Return;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioVisitor* visitor);
};

class CommentAST : public StyioAST {
  std::string Text;

  public:
    CommentAST (std::string text): Text(text) {}

    StyioType hint() {
      return StyioType::Comment;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioVisitor* visitor);
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

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return std::string("id { ") + Id + " }";
    }

    void accept(StyioVisitor* visitor);
};

class ArgAST : public StyioAST {
  std::unique_ptr<IdAST> Id;

  public:
    ArgAST(
      std::unique_ptr<IdAST> id): 
      Id(std::move(id)) 
      {

      }

    StyioType hint() {
      return StyioType::Arg;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioVisitor* visitor);
};

class KwArgAST : public StyioAST {
  std::unique_ptr<IdAST> Id;

  public:
    KwArgAST(
      std::unique_ptr<IdAST> id): 
      Id(std::move(id)) 
      {

      }

    StyioType hint() {
      return StyioType::KwArg;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { }");
    }

    void accept(StyioVisitor* visitor);
};

class VarsTupleAST : public StyioAST {
  std::vector<std::unique_ptr<StyioAST>> Vars;

  public:
    VarsTupleAST(
      std::vector<std::unique_ptr<StyioAST>> vars): 
      Vars(std::move(vars)) { }

    StyioType hint() {
      return StyioType::Fill;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
};

class TypeAST : public StyioAST {
  std::string Type;

  public:
    TypeAST(
      const std::string& type): 
      Type(type) {}

    StyioType hint() {
      return StyioType::Type;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::TypedVar;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::Int;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + " { " + Value + " }";
    }

    void accept(StyioVisitor* visitor);
};

/*
  FloatAST: Float
*/
class FloatAST : public StyioAST {
  std::string Value;

  public:
    FloatAST(std::string val) : Value(val) {}

    StyioType hint() {
      return StyioType::Float;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + " { " + Value + " }";
    }

    void accept(StyioVisitor* visitor);
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

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + " { \'" + Value + "\' }";
    }

    void accept(StyioVisitor* visitor);
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

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return "\"" + Value + "\"";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::FmtStr;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + " { \"" + "\" }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::ExtPath;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") + Path -> toStringInline(indent + 1, colorful) + " }";
    }

    void accept(StyioVisitor* visitor);
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

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::List;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
};

class TupleAST : public StyioAST {
  std::vector<std::unique_ptr<StyioAST>> Elems;

  public:
    TupleAST(
      std::vector<std::unique_ptr<StyioAST>> elems): 
      Elems(std::move(elems)) 
      {

      }

    StyioType hint() {
      return StyioType::Tuple;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
};

class SetAST : public StyioAST {
  std::vector<std::unique_ptr<StyioAST>> Elems;

  public:
    SetAST(
      std::vector<std::unique_ptr<StyioAST>> elems): 
      Elems(std::move(elems)) 
      {

      }

    StyioType hint() {
      return StyioType::Set;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::Range;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::SizeOf;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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
  StyioType Op;
  std::unique_ptr<StyioAST> LHS;
  std::unique_ptr<StyioAST> RHS;

  public:
    BinOpAST(
      StyioType op, 
      std::unique_ptr<StyioAST> lhs, 
      std::unique_ptr<StyioAST> rhs): 
      Op(op),
      LHS(std::move(lhs)), 
      RHS(std::move(rhs)) 
      {

      }

    StyioType hint() {
      return Op;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" {") 
        + " LHS: " + LHS -> toStringInline(indent) 
        + " | RHS: " + RHS -> toStringInline(indent)  
        + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::Compare;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::Condition;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
};

class CallAST : public StyioAST {
  std::unique_ptr<StyioAST> Func;

  public:
    CallAST(
      std::unique_ptr<StyioAST> func) : 
      Func(std::move(func)) {}

    StyioType hint() {
      return StyioType::Call;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
};

class ListOpAST : public StyioAST 
{
  StyioType OpType;
  std::unique_ptr<StyioAST> TheList;

  std::unique_ptr<StyioAST> Slot1;
  std::unique_ptr<StyioAST> Slot2;

  public:
    /*
      Get_Reversed
        [<]
    */
    ListOpAST(
      StyioType opType,
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
      StyioType opType, 
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
      StyioType opType, 
      std::unique_ptr<StyioAST> theList, 
      std::unique_ptr<StyioAST> index, 
      std::unique_ptr<StyioAST> value): 
      OpType(opType), 
      TheList(std::move(theList)), 
      Slot1(std::move(index)), 
      Slot2(std::move(value)) 
      {

      }

    StyioType hint() {
      return OpType;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::Resources;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::MutBind;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::FixBind;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::Struct;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::ReadFile;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::Print;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::ExtPack;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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
  std::unique_ptr<StyioAST> Resources;
  std::vector<std::unique_ptr<StyioAST>> Stmts;

  public:
    BlockAST(
      std::unique_ptr<StyioAST> resources,
      std::vector<std::unique_ptr<StyioAST>> stmts): 
      Resources(std::move(resources)),
      Stmts(std::move(stmts)) 
      {

      }

    BlockAST(
      std::vector<std::unique_ptr<StyioAST>> stmts): 
      Stmts(std::move(stmts)) 
      {
        
      }

    StyioType hint() {
      return StyioType::Block;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
};

/*
  CasesAST: Match Cases
*/
class CasesAST : public StyioAST {
  std::vector<std::tuple<std::unique_ptr<StyioAST>, std::unique_ptr<StyioAST>>> Cases;
  std::unique_ptr<StyioAST> LastExpr;

  public:
    CasesAST(
      std::unique_ptr<StyioAST> expr): 
      LastExpr(std::move(expr)) 
      {

      }

    CasesAST(
      std::vector<std::tuple<std::unique_ptr<StyioAST>, std::unique_ptr<StyioAST>>> cases,
      std::unique_ptr<StyioAST> expr): 
      Cases(std::move(cases)),
      LastExpr(std::move(expr)) 
      {

      }

    StyioType hint() {
      return StyioType::Cases;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
};

class CondFlowAST : public StyioAST {
  std::unique_ptr<CondAST> CondExpr;
  std::unique_ptr<StyioAST> ThenBlock;
  std::unique_ptr<StyioAST> ElseBlock;

  public:
    StyioType WhatFlow;

    CondFlowAST(
      StyioType whatFlow,
      std::unique_ptr<CondAST> condition,
      std::unique_ptr<StyioAST> block): 
      WhatFlow(whatFlow),
      CondExpr(std::move(condition)),
      ThenBlock(std::move(block))
      {

      }

    CondFlowAST(
      StyioType whatFlow,
      std::unique_ptr<CondAST> condition,
      std::unique_ptr<StyioAST> blockThen,
      std::unique_ptr<StyioAST> blockElse): 
      WhatFlow(whatFlow),
      CondExpr(std::move(condition)),
      ThenBlock(std::move(blockThen)),
      ElseBlock(std::move(blockElse))
      {

      }

    StyioType hint() {
      return WhatFlow;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::CheckEq;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
};

class CheckIsInAST : public StyioAST {
  std::unique_ptr<StyioAST> Iterable;

  public:
    CheckIsInAST(
      std::unique_ptr<StyioAST> value): 
      Iterable(std::move(value))
      {

      }

    StyioType hint() {
      return StyioType::CheckIsin;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::FromTo;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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
    StyioType Type = StyioType::Forward;

  public:
    ForwardAST(
      std::unique_ptr<StyioAST> expr):
      ThenExpr(std::move(expr))
      { Type = StyioType::Forward; }

    ForwardAST(
      std::unique_ptr<CheckEqAST> value,
      std::unique_ptr<StyioAST> whatnext):
      ExtraEq(std::move(value)),
      ThenExpr(std::move(whatnext))
      { Type = StyioType::If_Equal_To_Forward; }

    ForwardAST(
      std::unique_ptr<CheckIsInAST> isin,
      std::unique_ptr<StyioAST> whatnext): 
      ExtraIsin(std::move(isin)),
      ThenExpr(std::move(whatnext))
      { Type = StyioType::If_Is_In_Forward; }

    ForwardAST(
      std::unique_ptr<CasesAST> cases): 
      ThenExpr(std::move(cases))
      { Type = StyioType::Cases_Forward; }

    ForwardAST(
      std::unique_ptr<CondFlowAST> condflow): 
      ExtraCond(std::move(condflow))
      {
        if ((condflow -> WhatFlow) == StyioType::CondFlow_True) {
          Type = StyioType::If_True_Forward; }
        else if ((condflow -> WhatFlow) == StyioType::CondFlow_False) {
          Type = StyioType::If_False_Forward; }
        else {
          Type = StyioType::If_Both_Forward; }
      }

    ForwardAST(
      std::unique_ptr<VarsTupleAST> vars,
      std::unique_ptr<StyioAST> whatnext): 
      TmpVars(std::move(vars)),
      ThenExpr(std::move(whatnext))
      { Type = StyioType::Fill_Forward; }

    ForwardAST(
      std::unique_ptr<VarsTupleAST> vars,
      std::unique_ptr<CheckEqAST> value,
      std::unique_ptr<StyioAST> whatnext): 
      TmpVars(std::move(vars)),
      ExtraEq(std::move(value)),
      ThenExpr(std::move(whatnext))
      { Type = StyioType::Fill_If_Equal_To_Forward; }

    ForwardAST(
      std::unique_ptr<VarsTupleAST> vars,
      std::unique_ptr<CheckIsInAST> isin,
      std::unique_ptr<StyioAST> whatnext): 
      TmpVars(std::move(vars)),
      ExtraIsin(std::move(isin)),
      ThenExpr(std::move(whatnext))
      { Type = StyioType::Fill_If_Is_in_Forward; }

    
    ForwardAST(
      std::unique_ptr<VarsTupleAST> vars,
      std::unique_ptr<CasesAST> cases): 
      TmpVars(std::move(vars)),
      ThenExpr(std::move(cases))
      { Type = StyioType::Fill_Cases_Forward; }

    ForwardAST(
      std::unique_ptr<VarsTupleAST> vars,
      std::unique_ptr<CondFlowAST> condflow): 
      TmpVars(std::move(vars)),
      ExtraCond(std::move(condflow))
      {
        if ((condflow -> WhatFlow) == StyioType::CondFlow_True) {
          Type = StyioType::Fill_If_True_Forward; }
        else if ((condflow -> WhatFlow) == StyioType::CondFlow_False) {
          Type = StyioType::Fill_If_False_Forward; }
        else {
          Type = StyioType::Fill_If_Both_Forward; }
      }

    StyioType hint() {
      return Type;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::Infinite;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::Func;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::Loop;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
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

    StyioType hint() {
      return StyioType::Iterator;
    }

    std::string toString(int indent = 0, bool colorful = false);

    std::string toStringInline(int indent = 0, bool colorful = false) {
      return reprStyioType(this -> hint(), colorful) + std::string(" { ") 
      + " }";
    }

    void accept(StyioVisitor* visitor);
};

class StyioVisitor {
  std::unique_ptr<llvm::LLVMContext> context;

  public:
    StyioVisitor () {}

    llvm::Value* visit_true(TrueAST* ast);

    llvm::Value* visit_false(FalseAST* ast);

    llvm::Value* visit_none(NoneAST* ast);

    llvm::Value* visit_end(EndAST* ast);

    llvm::Value* visit_empty(EmptyAST* ast);

    llvm::Value* visit_empty_block(EmptyBlockAST* ast);

    llvm::Value* visit_pass(PassAST* ast);

    llvm::Value* visit_break(BreakAST* ast);

    llvm::Value* visit_return(ReturnAST* ast);

    llvm::Value* visit_comment(CommentAST* ast);

    llvm::Value* visit_id(IdAST* ast);

    llvm::Value* visit_arg(ArgAST* ast);

    llvm::Value* visit_kwarg(KwArgAST* ast);

    llvm::Value* visit_vars_tuple(VarsTupleAST* ast);

    llvm::Value* visit_type(TypeAST* ast);

    llvm::Value* visit_typed_var(TypedVarAST* ast);

    llvm::Value* visit_int(IntAST* ast);

    llvm::Value* visit_float(FloatAST* ast);

    llvm::Value* visit_char(CharAST* ast);

    llvm::Value* visit_string(StringAST* ast);

    llvm::Value* visit_fmt_str(FmtStrAST* ast);

    llvm::Value* visit_ext_path(ExtPathAST* ast);

    llvm::Value* visit_ext_link(ExtLinkAST* ast);

    llvm::Value* visit_list(ListAST* ast);

    llvm::Value* visit_tuple(TupleAST* ast);

    llvm::Value* visit_set(SetAST* ast);

    llvm::Value* visit_range(RangeAST* ast);

    llvm::Value* visit_size_of(SizeOfAST* ast);

    llvm::Value* visit_bin_op(BinOpAST* ast);

    llvm::Value* visit_bin_comp(BinCompAST* ast);

    llvm::Value* visit_cond(CondAST* ast);

    llvm::Value* visit_call(CallAST* ast);

    llvm::Value* visit_list_op(ListOpAST* ast);

    llvm::Value* visit_resources(ResourceAST* ast);

    llvm::Value* visit_flex_bind(FlexBindAST* ast);

    llvm::Value* visit_final_bind(FinalBindAST* ast);

    llvm::Value* visit_struct(StructAST* ast);

    llvm::Value* visit_read_file(ReadFileAST* ast);

    llvm::Value* visit_print(PrintAST* ast);

    llvm::Value* visit_ext_pack(ExtPackAST* ast);

    llvm::Value* visit_block(BlockAST* ast);

    llvm::Value* visit_cases(CasesAST* ast);

    llvm::Value* visit_cond_flow(CondFlowAST* ast);

    llvm::Value* visit_check_equal(CheckEqAST* ast);

    llvm::Value* visit_check_isin(CheckIsInAST* ast);

    llvm::Value* visit_from_to(FromToAST* ast);

    llvm::Value* visit_forward(ForwardAST* ast);

    llvm::Value* visit_infinite(InfiniteAST* ast);

    llvm::Value* visit_function(FuncAST* ast);

    llvm::Value* visit_loop(LoopAST* ast);

    llvm::Value* visit_iterator(IterAST* ast);
};

#endif