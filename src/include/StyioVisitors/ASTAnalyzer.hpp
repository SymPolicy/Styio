#pragma once
#ifndef STYIO_AST_ANALYZER_VISITOR_H_
#define STYIO_AST_ANALYZER_VISITOR_H_

// [STL]
#include <string>
#include <unordered_map>
#include <iostream>

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
  /* Type Inference */
  virtual void typeInfer(T* t) = 0;
};

template <typename T, typename... Types>
class AnalyzerVisitor<T, Types...> : public AnalyzerVisitor<Types...>
{
public:
  using AnalyzerVisitor<Types...>::typeInfer;

  /* Type Inference */
  virtual void typeInfer(T* t) = 0;
};

using StyioASTVisitor = AnalyzerVisitor<
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

  class IdAST,
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

class StyioASTAnalyzer : public StyioASTVisitor
{
  unordered_map<string, FuncAST*> func_defs;

public:
  StyioASTAnalyzer() {}

  ~StyioASTAnalyzer() {}

  /* Styio AST Type typeInferer*/

  void typeInfer(BoolAST* ast);

  void typeInfer(NoneAST* ast);

  void typeInfer(EOFAST* ast);

  void typeInfer(EmptyAST* ast);

  void typeInfer(PassAST* ast);

  void typeInfer(BreakAST* ast);

  void typeInfer(ReturnAST* ast);

  void typeInfer(CommentAST* ast);

  void typeInfer(IdAST* ast);

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