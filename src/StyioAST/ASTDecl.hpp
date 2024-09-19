#pragma once
#ifndef STYIO_AST_DECLARATION_H_
#define STYIO_AST_DECLARATION_H_

/* Forward Declaration */

/* Styio AST */
// Base
class StyioAST;

// Comment
class CommentAST;

// None / Empty
class NoneAST;
class EmptyAST;

/*
  Literals:
  - Bool (Boolean)
  - Int (Integer)
  - Float
  - Char (Character)
*/
class BoolAST;
class IntAST;
class FloatAST;
class CharAST;

/*
  Collections:
  - string [homo]

  - set [homo]
  - list [heter]

  - struct [heter]
  - tuple [heter]
*/
class StringAST;
class SetAST;
class ListAST;

class StructAST;
class TupleAST;

/*
  Variable:
  - Id
  - DType (Data Type)
*/
class NameAST;
class DTypeAST;

/*
  Variable Types:
  - Var ([Global|Local] Variables)
  - Arg (Function Arguments)
  - OptArg (Optional Function Arguments)
  - OptKwArg (Optional Function Arguments)
*/
class VarAST;
class ParamAST;
class OptArgAST;
class OptKwArgAST;

/*
  Assignment
  - FlexBind
  - FinalBind
*/
class FlexBindAST;
class FinalBindAST;

/*
  Binary Tree:
  - BinComp (Comparisons)
  - Cond (Condition)
  - BinOp (Binary Operations)
*/
class BinCompAST;
class CondAST;
class BinOpAST;

/*
  Function
  - Anonymous Function
  - Function

  - Call
*/
class AnonyFuncAST;
class FunctionAST;

class CallAST;

class AttrAST;

/*
  Methods:
  - SizeOf

  - TypeOf
  - TypeConvert

  - Range
  - ListOp
*/
class SizeOfAST;

class TypeOfAST;
class TypeConvertAST;

class ListOpAST;

class RangeAST;

/*
  Iteration:
  - Iter (Iterator)
  - Loop
*/
class IteratorAST;
class InfiniteLoopAST;

/*
  Control Flow
  - CondFlow (Conditional)

  - End-Of-Line
  - `pass` ..........
  - `break` ^^^^^^^^^
  - `continue` >>>>>>
  - `return` <<<<<<<<
*/
class CondFlowAST;

class EOFAST;
class PassAST;
class BreakAST;
class ContinueAST;
class ReturnAST;

/*
  Cases
  - Cases
  - MatchCases
*/
class CasesAST;
class MatchCasesAST;

/*
  Blocks
  - SideBlock
  - MainBlock
*/
class BlockAST;
class MainBlockAST;

/*
  Package Management
  - ExtPack
*/
class ExtPackAST;

/*
  Conceptions:
  - Infinite
*/
class InfiniteAST;

/*
  Intermediate Components:
  - VarTupleAST
*/
class VarTupleAST;

class ExtractorAST;

class ForwardAST;
class BackwardAST;

class CODPAST;
class CheckEqAST;
class CheckIsinAST;
class FromToAST;

/*
  Features:
  - FmtStr (Format String)
*/
class FmtStrAST;

/*
  Resources:
  - ResourceAST

  - LocalPath
  - RemotePath
  - WebUrl
  - DBUrl
*/
class ResourceAST;

class ResPathAST;
class RemotePathAST;
class WebUrlAST;
class DBUrlAST;

/*
  Methods (I/O)
  - print
  - read file
  - write file
*/
class PrintAST;
class ReadFileAST;
class WriteFileAST;

#endif