#pragma once
#ifndef STYIO_AST_ANALYZER_VISITOR_H_
#define STYIO_AST_ANALYZER_VISITOR_H_

// [STL]
#include <iostream>
#include <string>
#include <unordered_map>

using std::string;
using std::unordered_map;

// [Styio]
#include "../StyioAST/ASTDecl.hpp"

// Generic Visitor
template <typename... Types>
class AnalyzerVisitor;

template <typename T>
class AnalyzerVisitor<T>
{
public:
  virtual std::string toString(T* t, int indent = 0) = 0;

  virtual void typeInfer(T* t) = 0;
};

template <typename T, typename... Types>
class AnalyzerVisitor<T, Types...> : public AnalyzerVisitor<Types...>
{
public:
  using AnalyzerVisitor<Types...>::toString;
  virtual std::string toString(T* t, int indent = 0) = 0;

  using AnalyzerVisitor<Types...>::typeInfer;
  virtual void typeInfer(T* t) = 0;
};

using StyioAnalyzerVisitor = AnalyzerVisitor<
  class CommentAST,

  class NoneAST,
  class EmptyAST,

  class BoolAST,
  class IntAST,
  class FloatAST,
  class CharAST,

  class StringAST,
  class SetAST,
  class ListAST,

  class StructAST,
  class TupleAST,

  class NameAST,
  class DTypeAST,

  class VarAST,
  class ArgAST,
  class OptArgAST,
  class OptKwArgAST,

  class FlexBindAST,
  class FinalBindAST,

  class BinCompAST,
  class CondAST,
  class BinOpAST,

  class AnonyFuncAST,
  class FuncAST,

  class CallAST,

  class SizeOfAST,
  class TypeConvertAST,
  class ListOpAST,
  class RangeAST,

  class IterAST,
  class LoopAST,

  class CondFlowAST,

  class EOFAST,
  class PassAST,
  class BreakAST,
  class ReturnAST,

  class CasesAST,
  class MatchCasesAST,

  class BlockAST,
  class MainBlockAST,

  class ExtPackAST,

  class InfiniteAST,

  class VarTupleAST,

  class ForwardAST,
  class CheckEqAST,
  class CheckIsinAST,
  class FromToAST,

  class FmtStrAST,

  class ResourceAST,

  class LocalPathAST,
  class RemotePathAST,
  class WebUrlAST,
  class DBUrlAST,

  class PrintAST,
  class ReadFileAST>;

class StyioAnalyzer : public StyioAnalyzerVisitor
{
  unordered_map<string, FuncAST*> func_defs;

public:
  StyioAnalyzer() {}

  ~StyioAnalyzer() {}

  /* Styio AST To String */

  std::string toString(BoolAST* ast, int indent = 0);

  std::string toString(NoneAST* ast, int indent = 0);

  std::string toString(EOFAST* ast, int indent = 0);

  std::string toString(EmptyAST* ast, int indent = 0);

  std::string toString(PassAST* ast, int indent = 0);

  std::string toString(BreakAST* ast, int indent = 0);

  std::string toString(ReturnAST* ast, int indent = 0);

  std::string toString(CommentAST* ast, int indent = 0);

  std::string toString(NameAST* ast, int indent = 0);

  std::string toString(VarAST* ast, int indent = 0);

  std::string toString(ArgAST* ast, int indent = 0);

  std::string toString(OptArgAST* ast, int indent = 0);

  std::string toString(OptKwArgAST* ast, int indent = 0);

  std::string toString(VarTupleAST* ast, int indent = 0);

  std::string toString(DTypeAST* ast, int indent = 0);

  std::string toString(IntAST* ast, int indent = 0);

  std::string toString(FloatAST* ast, int indent = 0);

  std::string toString(CharAST* ast, int indent = 0);

  std::string toString(StringAST* ast, int indent = 0);

  std::string toString(TypeConvertAST* ast, int indent = 0);

  std::string toString(FmtStrAST* ast, int indent = 0);

  std::string toString(LocalPathAST* ast, int indent = 0);

  std::string toString(RemotePathAST* ast, int indent = 0);

  std::string toString(WebUrlAST* ast, int indent = 0);

  std::string toString(DBUrlAST* ast, int indent = 0);

  std::string toString(ListAST* ast, int indent = 0);

  std::string toString(TupleAST* ast, int indent = 0);

  std::string toString(SetAST* ast, int indent = 0);

  std::string toString(RangeAST* ast, int indent = 0);

  std::string toString(SizeOfAST* ast, int indent = 0);

  std::string toString(BinOpAST* ast, int indent = 0);

  std::string toString(BinCompAST* ast, int indent = 0);

  std::string toString(CondAST* ast, int indent = 0);

  std::string toString(CallAST* ast, int indent = 0);

  std::string toString(ListOpAST* ast, int indent = 0);

  std::string toString(ResourceAST* ast, int indent = 0);

  std::string toString(FlexBindAST* ast, int indent = 0);

  std::string toString(FinalBindAST* ast, int indent = 0);

  std::string toString(StructAST* ast, int indent = 0);

  std::string toString(ReadFileAST* ast, int indent = 0);

  std::string toString(PrintAST* ast, int indent = 0);

  std::string toString(ExtPackAST* ast, int indent = 0);

  std::string toString(BlockAST* ast, int indent = 0);

  std::string toString(CasesAST* ast, int indent = 0);

  std::string toString(CondFlowAST* ast, int indent = 0);

  std::string toString(CheckEqAST* ast, int indent = 0);

  std::string toString(CheckIsinAST* ast, int indent = 0);

  std::string toString(FromToAST* ast, int indent = 0);

  std::string toString(ForwardAST* ast, int indent = 0);

  std::string toString(InfiniteAST* ast, int indent = 0);

  std::string toString(AnonyFuncAST* ast, int indent = 0);

  std::string toString(FuncAST* ast, int indent = 0);

  std::string toString(LoopAST* ast, int indent = 0);

  std::string toString(IterAST* ast, int indent = 0);

  std::string toString(MatchCasesAST* ast, int indent = 0);

  std::string toString(MainBlockAST* ast, int indent = 0);

  /* Styio AST Type Inference */

  void typeInfer(BoolAST* ast);

  void typeInfer(NoneAST* ast);

  void typeInfer(EOFAST* ast);

  void typeInfer(EmptyAST* ast);

  void typeInfer(PassAST* ast);

  void typeInfer(BreakAST* ast);

  void typeInfer(ReturnAST* ast);

  void typeInfer(CommentAST* ast);

  void typeInfer(NameAST* ast);

  void typeInfer(VarAST* ast);

  void typeInfer(ArgAST* ast);

  void typeInfer(OptArgAST* ast);

  void typeInfer(OptKwArgAST* ast);

  void typeInfer(VarTupleAST* ast);

  void typeInfer(DTypeAST* ast);

  void typeInfer(IntAST* ast);

  void typeInfer(FloatAST* ast);

  void typeInfer(CharAST* ast);

  void typeInfer(StringAST* ast);

  void typeInfer(TypeConvertAST* ast);

  void typeInfer(FmtStrAST* ast);

  void typeInfer(LocalPathAST* ast);

  void typeInfer(RemotePathAST* ast);

  void typeInfer(WebUrlAST* ast);

  void typeInfer(DBUrlAST* ast);

  void typeInfer(ListAST* ast);

  void typeInfer(TupleAST* ast);

  void typeInfer(SetAST* ast);

  void typeInfer(RangeAST* ast);

  void typeInfer(SizeOfAST* ast);

  void typeInfer(BinOpAST* ast);

  void typeInfer(BinCompAST* ast);

  void typeInfer(CondAST* ast);

  void typeInfer(CallAST* ast);

  void typeInfer(ListOpAST* ast);

  void typeInfer(ResourceAST* ast);

  void typeInfer(FlexBindAST* ast);

  void typeInfer(FinalBindAST* ast);

  void typeInfer(StructAST* ast);

  void typeInfer(ReadFileAST* ast);

  void typeInfer(PrintAST* ast);

  void typeInfer(ExtPackAST* ast);

  void typeInfer(BlockAST* ast);

  void typeInfer(CasesAST* ast);

  void typeInfer(CondFlowAST* ast);

  void typeInfer(CheckEqAST* ast);

  void typeInfer(CheckIsinAST* ast);

  void typeInfer(FromToAST* ast);

  void typeInfer(ForwardAST* ast);

  void typeInfer(InfiniteAST* ast);

  void typeInfer(AnonyFuncAST* ast);

  void typeInfer(FuncAST* ast);

  void typeInfer(LoopAST* ast);

  void typeInfer(IterAST* ast);

  void typeInfer(MatchCasesAST* ast);

  void typeInfer(MainBlockAST* ast);
};

#endif