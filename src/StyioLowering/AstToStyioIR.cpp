/*
  AST to StyioIR Lowering Implementation
*/

// [C++ STL]
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioIR/GenIR/GenIR.hpp"
#include "../StyioResourceTopology/ResourceTopology.hpp"
#include "../StyioToken/Token.hpp"
#include "../StyioUtil/BoundedType.hpp"
#include "../StyioUtil/BuiltinMethods.hpp"
#include "../StyioUtil/ResourceNames.hpp"
#include "StyioIROptimizer.hpp"

namespace
{

int
alloc_pulse_region_id() {
  static int n = 0;
  return n++;
}

std::string
alloc_generated_pipe_item_name() {
  static int n = 0;
  return "__styio_pipe_item_" + std::to_string(n++);
}

StyioDataType
lowering_bool_type() {
  return StyioDataType{StyioDataTypeOption::Bool, "bool", 1};
}

StyioDataType
lowering_i64_type() {
  return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
}

StyioDataType
lowering_f64_type() {
  return StyioDataType{StyioDataTypeOption::Float, "f64", 64};
}

StyioDataType
lowering_type_convert_target_type(NumPromoTy promo_type) {
  switch (promo_type) {
    case NumPromoTy::Bool_To_Int:
      return lowering_i64_type();
    case NumPromoTy::Int_To_Float:
      return lowering_f64_type();
  }
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

StyioDataType
lowering_type_convert_source_fallback_type(NumPromoTy promo_type) {
  switch (promo_type) {
    case NumPromoTy::Bool_To_Int:
      return lowering_bool_type();
    case NumPromoTy::Int_To_Float:
      return lowering_i64_type();
  }
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

StyioDataType
lowering_string_type() {
  return StyioDataType{StyioDataTypeOption::String, "string", 0};
}

StyioIR*
unsupported_ast_lowering(const char* ast_name, const char* reason = nullptr) {
  std::string msg = std::string("unsupported AST lowering: ") + ast_name;
  if (reason != nullptr && reason[0] != '\0') {
    msg += std::string(" (") + reason + ")";
  }
  throw StyioTypeError(msg);
}

std::string
alloc_lowering_tmp_name(const char* prefix) {
  static int n = 0;
  return std::string(prefix) + std::to_string(n++);
}

std::vector<ParamAST*>
params_of_func_def(StyioAST* def) {
  if (auto* f = dynamic_cast<FunctionAST*>(def)) {
    return f->params;
  }
  if (auto* s = dynamic_cast<SimpleFuncAST*>(def)) {
    return s->params;
  }
  return {};
}

SGType*
func_ret_to_sgtype(
  const std::variant<TypeAST*, TypeTupleAST*>& ret_type,
  AstToStyioIRLowerer* an
) {
  if (ret_type.valueless_by_exception()) {
    return SGType::Create(lowering_i64_type());
  }
  if (std::holds_alternative<TypeTupleAST*>(ret_type)) {
    throw StyioTypeError(
      "tuple function return annotations require tuple value IR; tuple returns are not implemented"
    );
  }
  TypeAST* t = std::get<TypeAST*>(ret_type);
  if (!t || t->getDataType().option == StyioDataTypeOption::Undefined) {
    return SGType::Create(lowering_i64_type());
  }
  return static_cast<SGType*>(t->toStyioIR(an));
}

bool
func_ret_is_unspecified(const std::variant<TypeAST*, TypeTupleAST*>& ret_type) {
  if (ret_type.valueless_by_exception() || std::holds_alternative<TypeTupleAST*>(ret_type)) {
    return false;
  }
  TypeAST* t = std::get<TypeAST*>(ret_type);
  return t == nullptr || t->getDataType().option == StyioDataTypeOption::Undefined;
}

StyioDataType
param_data_type(
  ParamAST* p,
  AstToStyioIRLowerer* an,
  std::string_view callable_name,
  std::size_t param_index
) {
  if (an != nullptr) {
    if (const auto* specialization =
          an->active_callable_specialization(callable_name)) {
      if (param_index >= specialization->param_types.size()) {
        throw StyioTypeError(
          "generic specialization parameter index is out of range"
        );
      }
      return specialization->param_types[param_index];
    }
  }
  if (!p || !p->var_type || p->var_type->getDataType().option == StyioDataTypeOption::Undefined) {
    return lowering_i64_type();
  }
  return p->var_type->getDataType();
}

SGFuncArg*
param_to_sgarg(
  ParamAST* p,
  AstToStyioIRLowerer* an,
  std::string_view callable_name,
  std::size_t param_index
) {
  StyioDataType type =
    param_data_type(p, an, callable_name, param_index);
  SGType* ty = SGType::Create(type);
  return SGFuncArg::Create(p->getName(), ty);
}

bool ast_has_tail_value(StyioAST* ast);

bool
ast_is_statement_only_tail(StyioAST* ast) {
  if (!ast) {
    return true;
  }
  switch (ast->getNodeType()) {
    case StyioNodeType::Pass:
    case StyioNodeType::Break:
    case StyioNodeType::Continue:
    case StyioNodeType::Comment:
    case StyioNodeType::Empty:
    case StyioNodeType::EmptyBlock:
    case StyioNodeType::Resources:
    case StyioNodeType::MutBind:
    case StyioNodeType::FinalBind:
    case StyioNodeType::ParallelAssign:
    case StyioNodeType::Func:
    case StyioNodeType::SimpleFunc:
    case StyioNodeType::Struct:
    case StyioNodeType::Infinite:
    case StyioNodeType::Loop:
    case StyioNodeType::Iterator:
    case StyioNodeType::IterSeq:
    case StyioNodeType::HandleAcquire:
    case StyioNodeType::ResourceWrite:
    case StyioNodeType::ResourceRedirect:
    case StyioNodeType::ResourceEffect:
    case StyioNodeType::FlowBind:
    case StyioNodeType::StateDecl:
    case StyioNodeType::StreamZip:
    case StyioNodeType::SnapshotDecl:
    case StyioNodeType::Print:
    case StyioNodeType::ReadFile:
    case StyioNodeType::Forward:
    case StyioNodeType::Cases:
    case StyioNodeType::MainBlock:
      return true;
    default:
      return false;
  }
}

bool
ast_can_be_implicit_tail_value(StyioAST* ast) {
  if (!ast || ast->getNodeType() == StyioNodeType::Return) {
    return false;
  }
  if (auto* blk = dynamic_cast<BlockAST*>(ast)) {
    return blk->followings.empty() && !blk->stmts.empty() && ast_has_tail_value(blk->stmts.back());
  }
  if (auto* m = dynamic_cast<MatchCasesAST*>(ast)) {
    CasesAST* c = m->getCases();
    for (auto const& pr : c->case_list) {
      if (ast_has_tail_value(pr.second)) {
        return true;
      }
    }
    return ast_has_tail_value(c->case_default);
  }
  return !ast_is_statement_only_tail(ast);
}

bool
ast_has_tail_value(StyioAST* ast) {
  if (!ast) {
    return false;
  }
  if (ast->getNodeType() == StyioNodeType::Return) {
    return true;
  }
  return ast_can_be_implicit_tail_value(ast);
}

static constexpr const char* kMissingFunctionTailMessage =
  "function body requires a return value; add <| expr or a final value expression";

styio::lowering::StyioIRPassPipelineOptions
intermediate_sg_block_pipeline_options() {
  styio::lowering::StyioIRPassPipelineOptions options;
  options.verifier_options.defer_unresolved_loop_control = true;
  return options;
}

SGBlock*
lower_func_body(AstToStyioIRLowerer* an, StyioAST* body, bool implicit_tail_value = false);

StyioIR*
lower_tail_stmt(AstToStyioIRLowerer* an, StyioAST* stmt) {
  if (!stmt) {
    throw StyioTypeError(kMissingFunctionTailMessage);
  }
  if (stmt->getNodeType() == StyioNodeType::Return) {
    return stmt->toStyioIR(an);
  }
  if (auto* blk = dynamic_cast<BlockAST*>(stmt)) {
    return lower_func_body(an, blk, true);
  }
  if (ast_can_be_implicit_tail_value(stmt)) {
    return SGReturn::Create(stmt->toStyioIR(an));
  }
  return stmt->toStyioIR(an);
}

SGBlock*
lower_func_body(AstToStyioIRLowerer* an, StyioAST* body, bool implicit_tail_value) {
  if (!body) {
    if (implicit_tail_value) {
      throw StyioTypeError(kMissingFunctionTailMessage);
    }
    return SGBlock::Create({});
  }
  if (auto* blk = dynamic_cast<BlockAST*>(body)) {
    if (implicit_tail_value
        && (blk->stmts.empty() || !blk->followings.empty() || !ast_has_tail_value(blk->stmts.back()))) {
      throw StyioTypeError(kMissingFunctionTailMessage);
    }
    std::vector<StyioIR*> stmts;
    stmts.reserve(blk->stmts.size() + blk->followings.size());
    for (size_t i = 0; i < blk->stmts.size(); ++i) {
      const bool tail = implicit_tail_value && i + 1 == blk->stmts.size() && blk->followings.empty();
      stmts.push_back(tail ? lower_tail_stmt(an, blk->stmts[i]) : blk->stmts[i]->toStyioIR(an));
    }
    for (auto* following : blk->followings) {
      stmts.push_back(following->toStyioIR(an));
    }
    return static_cast<SGBlock*>(
      styio::lowering::require_default_styio_ir_pass_pipeline(
        SGBlock::Create(std::move(stmts)),
        intermediate_sg_block_pipeline_options()));
  }
  std::vector<StyioIR*> one;
  one.push_back(implicit_tail_value ? lower_tail_stmt(an, body) : body->toStyioIR(an));
  return SGBlock::Create(std::move(one));
}

void
register_direct_local_function_defs(AstToStyioIRLowerer* an, StyioAST* body) {
  auto* blk = dynamic_cast<BlockAST*>(body);
  if (blk == nullptr) {
    return;
  }
  for (auto* stmt : blk->stmts) {
    if (auto* f = dynamic_cast<FunctionAST*>(stmt)) {
      an->record_function_def(f->getNameAsStr(), f->func_name->getSymbolId(), f);
      continue;
    }
    if (auto* sf = dynamic_cast<SimpleFuncAST*>(stmt)) {
      an->record_function_def(sf->func_name->getAsStr(), sf->func_name->getSymbolId(), sf);
    }
  }
}

SGBlock*
lower_func_body_with_local_defs(AstToStyioIRLowerer* an, StyioAST* body, bool implicit_tail_value = false) {
  auto saved_defs = an->func_defs;
  auto saved_defs_by_sid = an->func_defs_by_sid;
  register_direct_local_function_defs(an, body);
  SGBlock* lowered = lower_func_body(an, body, implicit_tail_value);
  an->func_defs = std::move(saved_defs);
  an->func_defs_by_sid = std::move(saved_defs_by_sid);
  return lowered;
}

bool
stmt_has_return_tree(StyioAST* ast) {
  if (!ast) {
    return false;
  }
  if (ast->getNodeType() == StyioNodeType::Return) {
    return true;
  }
  if (ast_can_be_implicit_tail_value(ast)) {
    return true;
  }
  if (ast->getNodeType() == StyioNodeType::MatchCases) {
    auto* m = static_cast<MatchCasesAST*>(ast);
    CasesAST* c = m->getCases();
    for (auto const& pr : c->case_list) {
      if (stmt_has_return_tree(pr.second)) {
        return true;
      }
    }
    return stmt_has_return_tree(c->case_default);
  }
  if (auto* b = dynamic_cast<BlockAST*>(ast)) {
    for (auto* s : b->stmts) {
      if (stmt_has_return_tree(s)) {
        return true;
      }
    }
  }
  return false;
}

bool
try_parse_int_literal_value(StyioAST* ast, std::int64_t& out) {
  auto* lit = dynamic_cast<IntAST*>(ast);
  if (!lit) {
    return false;
  }

  try {
    out = std::stoll(lit->getValue());
    return true;
  }
  catch (const std::exception&) {
    return false;
  }
}

bool
is_name_ast(StyioAST* ast, const std::string& name) {
  auto* nm = dynamic_cast<NameAST*>(ast);
  return nm != nullptr && nm->getAsStr() == name;
}

std::optional<std::int64_t>
match_case_pattern_value_for_name(StyioAST* pattern, const std::string* scrutinee_name) {
  std::int64_t literal = 0;
  if (try_parse_int_literal_value(pattern, literal)) {
    return literal;
  }

  auto* cmp = dynamic_cast<BinCompAST*>(pattern);
  if (scrutinee_name == nullptr || cmp == nullptr || cmp->getSign() != CompType::EQ) {
    return std::nullopt;
  }

  if (is_name_ast(cmp->getLHS(), *scrutinee_name)
      && try_parse_int_literal_value(cmp->getRHS(), literal)) {
    return literal;
  }
  if (is_name_ast(cmp->getRHS(), *scrutinee_name)
      && try_parse_int_literal_value(cmp->getLHS(), literal)) {
    return literal;
  }

  return std::nullopt;
}

std::optional<std::int64_t>
match_case_pattern_value(StyioAST* pattern, StyioAST* scrutinee) {
  auto* scrutinee_name = dynamic_cast<NameAST*>(scrutinee);
  const std::string* name = scrutinee_name != nullptr ? &scrutinee_name->getAsStr() : nullptr;
  return match_case_pattern_value_for_name(pattern, name);
}

std::optional<StdStreamKind>
std_stream_kind_of(const StyioDataType& type) {
  if (type.handle_family != StyioHandleFamily::Stream || !type.has_std_stream_kind) {
    return std::nullopt;
  }
  return static_cast<StdStreamKind>(type.std_stream_kind);
}

std::optional<StyioDataType>
bound_type_of(AstToStyioIRLowerer* an, StyioAST* expr) {
  auto* nm = dynamic_cast<NameAST*>(expr);
  if (nm == nullptr) {
    return std::nullopt;
  }
  const StyioDataType* bound_type =
    an->find_local_binding_type(nm->getSymbolId(), nm->getAsStr());
  if (bound_type == nullptr) {
    return std::nullopt;
  }
  return *bound_type;
}

StyioDataType
matrix_intrinsic_lowered_type(AstToStyioIRLowerer* an, FuncCallAST* call);

std::string
resource_family_for_lowering_type(const StyioDataType& type);

StyioDataType
expr_lowered_type(AstToStyioIRLowerer* an, StyioAST* expr) {
  if (auto bound = bound_type_of(an, expr)) {
    return *bound;
  }
  if (dynamic_cast<FmtStrAST*>(expr) != nullptr) {
    return lowering_string_type();
  }
  if (auto* attr = dynamic_cast<AttrAST*>(expr)) {
    auto* attr_name = dynamic_cast<NameAST*>(attr->attr);
    StyioDataType body_type = expr_lowered_type(an, attr->body);
    if (attr_name != nullptr) {
      const std::string attr_str = attr_name->getAsStr();
      if (attr_str == "keys" && styio_is_dict_type(body_type)) {
        return styio_make_list_type(styio_dict_key_type_name(body_type));
      }
      if (attr_str == "values" && styio_is_dict_type(body_type)) {
        return styio_make_list_type(styio_dict_value_type_name(body_type));
      }
      const std::string family = resource_family_for_lowering_type(body_type);
      const StyioSemaContext::ResourceMethodInfo* method =
        an->find_resource_method(family, attr_str);
      const StyioBuiltinMethodKind builtin_method = styio_builtin_method_kind(attr_str);
      if (method != nullptr
          && method->property
          && styio_is_resource_property_method_kind(builtin_method)) {
        return lowering_string_type();
      }
    }
  }
  if (auto* call = dynamic_cast<FuncCallAST*>(expr)) {
    if (call->func_callee != nullptr
        && styio_builtin_method_kind(call->getNameAsStr()) == StyioBuiltinMethodKind::StringLines) {
      StyioDataType callee_type = expr_lowered_type(an, call->func_callee);
      if (callee_type.option == StyioDataTypeOption::String) {
        return styio_make_list_type("string");
      }
    }
    if (call->func_callee != nullptr) {
      StyioDataType receiver_type = expr_lowered_type(an, call->func_callee);
      const std::string family = resource_family_for_lowering_type(receiver_type);
      const StyioSemaContext::ResourceMethodInfo* method =
        an->find_resource_method(family, call->getNameAsStr());
      if (method != nullptr && !method->property && !method->result_type.isUndefined()) {
        return method->result_type;
      }
    }
    StyioDataType matrix_type = matrix_intrinsic_lowered_type(an, call);
    if (!matrix_type.isUndefined()) {
      return matrix_type;
    }
  }
  if (auto* bin = dynamic_cast<BinOpAST*>(expr)) {
    StyioDataType t = bin->getType();
    if (!t.isUndefined()) {
      return t;
    }
    StyioDataType lt = expr_lowered_type(an, bin->getLHS());
    StyioDataType rt = expr_lowered_type(an, bin->getRHS());
    if (lt.option == StyioDataTypeOption::String || rt.option == StyioDataTypeOption::String) {
      if (bin->getOp() == StyioOpType::Binary_Add) {
        return lowering_string_type();
      }
    }
    if (lt.option == StyioDataTypeOption::Float || rt.option == StyioDataTypeOption::Float) {
      return lowering_f64_type();
    }
    if (lt.option == StyioDataTypeOption::Integer || rt.option == StyioDataTypeOption::Integer) {
      return lowering_i64_type();
    }
    if (lt.option == StyioDataTypeOption::Bool && rt.option == StyioDataTypeOption::Bool) {
      return lowering_bool_type();
    }
  }
  if (auto* access = dynamic_cast<ListOpAST*>(expr)) {
    if (access->getOp() == StyioNodeType::Access_By_Index) {
      if (auto* row_access = dynamic_cast<ListOpAST*>(access->getList())) {
        if (row_access->getOp() == StyioNodeType::Access_By_Index) {
          StyioDataType matrix_type = expr_lowered_type(an, row_access->getList());
          if (styio_is_matrix_type(matrix_type)) {
            return styio_data_type_from_name(styio_matrix_elem_type_name(matrix_type));
          }
        }
      }
    }
    StyioDataType base_type = expr_lowered_type(an, access->getList());
    if (access->getOp() == StyioNodeType::Access_By_Index && styio_is_matrix_type(base_type)) {
      return styio_make_list_type(styio_matrix_elem_type_name(base_type));
    }
    if (access->getOp() == StyioNodeType::Access_By_Slice && styio_is_list_type(base_type)) {
      return base_type;
    }
    if (access->getOp() == StyioNodeType::Access_By_Slice && styio_is_matrix_type(base_type)) {
      return styio_make_list_type(styio_type_item_type_name(base_type));
    }
    if (access->getOp() == StyioNodeType::Access_By_Slice && styio_is_dict_type(base_type)) {
      return styio_make_list_type(styio_dict_value_type_name(base_type));
    }
    if (styio_is_dict_type(base_type)) {
      return styio_data_type_from_name(styio_dict_value_type_name(base_type));
    }
    if (styio_type_is_indexable(base_type)) {
      return styio_data_type_from_name(styio_type_item_type_name(base_type));
    }
  }
  return expr->getDataType();
}

StyioDataType
merge_tail_value_type(const StyioDataType& a, const StyioDataType& b) {
  if (a.isUndefined()) {
    return b;
  }
  if (b.isUndefined()) {
    return a;
  }
  if (a.option == StyioDataTypeOption::String || b.option == StyioDataTypeOption::String) {
    return lowering_string_type();
  }
  if (a.option == StyioDataTypeOption::Float || b.option == StyioDataTypeOption::Float) {
    return lowering_f64_type();
  }
  if (a.option == StyioDataTypeOption::Integer || b.option == StyioDataTypeOption::Integer) {
    return lowering_i64_type();
  }
  if (a.option == StyioDataTypeOption::Bool && b.option == StyioDataTypeOption::Bool) {
    return lowering_bool_type();
  }
  if (a.option == StyioDataTypeOption::Char && b.option == StyioDataTypeOption::Char) {
    return styio_data_type_from_name("char");
  }
  if ((a.option == StyioDataTypeOption::Bool || a.option == StyioDataTypeOption::Char)
      && (b.option == StyioDataTypeOption::Bool || b.option == StyioDataTypeOption::Char)) {
    return lowering_i64_type();
  }
  return a;
}

StyioDataType
infer_tail_value_type(AstToStyioIRLowerer* an, StyioAST* ast) {
  if (!ast) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  if (auto* ret = dynamic_cast<ReturnAST*>(ast)) {
    return expr_lowered_type(an, ret->getExpr());
  }
  if (auto* blk = dynamic_cast<BlockAST*>(ast)) {
    if (blk->stmts.empty()) {
      return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
    }
    return infer_tail_value_type(an, blk->stmts.back());
  }
  if (auto* c = dynamic_cast<CasesAST*>(ast)) {
    StyioDataType merged{StyioDataTypeOption::Undefined, "undefined", 0};
    for (auto const& pr : c->case_list) {
      merged = merge_tail_value_type(merged, infer_tail_value_type(an, pr.second));
    }
    merged = merge_tail_value_type(merged, infer_tail_value_type(an, c->case_default));
    return merged;
  }
  if (auto* m = dynamic_cast<MatchCasesAST*>(ast)) {
    CasesAST* c = m->getCases();
    StyioDataType merged{StyioDataTypeOption::Undefined, "undefined", 0};
    for (auto const& pr : c->case_list) {
      merged = merge_tail_value_type(merged, infer_tail_value_type(an, pr.second));
    }
    merged = merge_tail_value_type(merged, infer_tail_value_type(an, c->case_default));
    return merged;
  }
  if (ast_can_be_implicit_tail_value(ast)) {
    return expr_lowered_type(an, ast);
  }
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

bool
expr_is_list_like(AstToStyioIRLowerer* an, StyioAST* expr) {
  if (expr->getNodeType() == StyioNodeType::List
      || expr->getNodeType() == StyioNodeType::Range
      || styio_is_list_type(expr_lowered_type(an, expr))) {
    return true;
  }
  return false;
}

bool
expr_is_dict_like(AstToStyioIRLowerer* an, StyioAST* expr) {
  if (expr->getNodeType() == StyioNodeType::Dict || styio_is_dict_type(expr_lowered_type(an, expr))) {
    return true;
  }
  return false;
}

bool
expr_is_matrix_like(AstToStyioIRLowerer* an, StyioAST* expr) {
  return styio_is_matrix_type(expr_lowered_type(an, expr));
}

std::string
expr_iterable_item_type_name(AstToStyioIRLowerer* an, StyioAST* expr) {
  StyioDataType type = expr_lowered_type(an, expr);
  std::string elem_type = styio_type_item_type_name(type);
  return elem_type.empty() ? std::string("i64") : elem_type;
}

StyioIR*
std_stream_item_expr_for_type(const std::string& item_name, const std::string& elem_type) {
  StyioIR* item = SGResId::Create(item_name);
  switch (styio_value_family_from_type_name(elem_type)) {
    case StyioValueFamily::ListHandle:
      return SCListToString::Create(item);
    case StyioValueFamily::DictHandle:
      return SCDictToString::Create(item);
    case StyioValueFamily::MatrixHandle:
      return SCMatrixToString::Create(item);
    default:
      return item;
  }
}

StyioIR*
lower_text_iterable_std_stream_write(
  AstToStyioIRLowerer* an,
  StyioAST* data,
  StyioIR* data_ir,
  SIOStdStreamWrite::Stream stream
) {
  std::string elem_type = expr_iterable_item_type_name(an, data);
  StyioIR* iterable_ir = data_ir;
  if (expr_is_dict_like(an, data)) {
    iterable_ir = SCDictValues::Create(iterable_ir, elem_type);
  }

  std::string item_name = alloc_generated_pipe_item_name();
  StyioIR* item_expr = std_stream_item_expr_for_type(item_name, elem_type);
  return SGForEach::Create(
    iterable_ir,
    std::move(item_name),
    std::move(elem_type),
    SGBlock::Create({SIOStdStreamWrite::Create(stream, {item_expr})})
  );
}

StyioIR*
lower_text_iterable_file_write(
  AstToStyioIRLowerer* an,
  StyioAST* data,
  StyioIR* data_ir,
  StyioIR* path_ir,
  bool is_auto_path
) {
  std::string elem_type = expr_iterable_item_type_name(an, data);
  StyioIR* iterable_ir = data_ir;
  if (expr_is_dict_like(an, data)) {
    iterable_ir = SCDictValues::Create(iterable_ir, elem_type);
  }

  const StyioValueFamily elem_family = styio_value_family_from_type_name(elem_type);
  const bool item_is_string = elem_family == StyioValueFamily::String;
  const bool promote = !item_is_string;
  const bool append_newline = promote;
  std::string item_name = alloc_generated_pipe_item_name();
  StyioIR* item_expr = std_stream_item_expr_for_type(item_name, elem_type);
  return SGForEach::Create(
    iterable_ir,
    std::move(item_name),
    std::move(elem_type),
    SGBlock::Create({
      SIOResourceWriteToFile::Create(item_expr, path_ir, is_auto_path, promote, append_newline)
    })
  );
}

bool
is_matrix_intrinsic_name(const std::string& name) {
  return name == "mat_zeros"
         || name == "mat_zeros_i64"
         || name == "mat_identity"
         || name == "mat_identity_i64"
         || name == "mat_shape"
         || name == "mat_rows"
         || name == "mat_cols"
         || name == "mat_get"
         || name == "mat_set"
         || name == "mat_clone"
         || name == "mat_add"
         || name == "mat_sub"
         || name == "mat_scale"
         || name == "mat_hadamard"
         || name == "matmul"
         || name == "transpose"
         || name == "dot"
         || name == "mat_sum"
         || name == "norm";
}

StyioDataType
matrix_intrinsic_lowered_type(AstToStyioIRLowerer* an, FuncCallAST* call) {
  if (call == nullptr || call->func_callee != nullptr || !is_matrix_intrinsic_name(call->getNameAsStr())) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  const std::string name = call->getNameAsStr();
  auto arg_type = [&](size_t i)
  {
    return i < call->getArgList().size()
             ? expr_lowered_type(an, call->getArgList()[i])
             : StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  };
  auto elem = [&](const StyioDataType& type)
  {
    return styio_data_type_from_name(styio_matrix_elem_type_name(type));
  };
  auto merge_elem = [&](const StyioDataType& a, const StyioDataType& b)
  {
    return (a.option == StyioDataTypeOption::Float || b.option == StyioDataTypeOption::Float)
             ? std::string("f64")
             : std::string("i64");
  };
  auto int_lit = [&](size_t i) -> size_t
  {
    if (i >= call->getArgList().size()) {
      return 0;
    }
    auto* lit = dynamic_cast<IntAST*>(call->getArgList()[i]);
    if (lit == nullptr) {
      return 0;
    }
    try {
      long long v = std::stoll(lit->getValue());
      return v > 0 ? static_cast<size_t>(v) : 0;
    }
    catch (...) {
      return 0;
    }
  };

  if (name == "mat_zeros" || name == "mat_zeros_i64") {
    return styio_make_matrix_type(name == "mat_zeros_i64" ? "i64" : "f64", int_lit(0), int_lit(1));
  }
  if (name == "mat_identity" || name == "mat_identity_i64") {
    size_t n = int_lit(0);
    return styio_make_matrix_type(name == "mat_identity_i64" ? "i64" : "f64", n, n);
  }
  if (name == "mat_shape") {
    return styio_make_list_type("i64");
  }
  if (name == "mat_rows" || name == "mat_cols" || name == "mat_set") {
    return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  }
  if (name == "mat_get") {
    return elem(arg_type(0));
  }
  if (name == "mat_clone") {
    return arg_type(0);
  }
  if (name == "transpose") {
    StyioDataType m = arg_type(0);
    return styio_make_matrix_type(
      styio_matrix_elem_type_name(m),
      styio_matrix_col_count(m),
      styio_matrix_row_count(m)
    );
  }
  if (name == "mat_add" || name == "mat_sub" || name == "mat_hadamard") {
    StyioDataType a = arg_type(0);
    StyioDataType b = arg_type(1);
    return styio_make_matrix_type(
      merge_elem(elem(a), elem(b)),
      styio_matrix_row_count(a),
      styio_matrix_col_count(a)
    );
  }
  if (name == "mat_scale") {
    StyioDataType m = arg_type(0);
    return styio_make_matrix_type(
      merge_elem(elem(m), arg_type(1)),
      styio_matrix_row_count(m),
      styio_matrix_col_count(m)
    );
  }
  if (name == "matmul") {
    StyioDataType a = arg_type(0);
    StyioDataType b = arg_type(1);
    return styio_make_matrix_type(
      merge_elem(elem(a), elem(b)),
      styio_matrix_row_count(a),
      styio_matrix_col_count(b)
    );
  }
  if (name == "dot" || name == "mat_sum") {
    StyioDataType t = name == "dot"
                        ? styio_data_type_from_name(merge_elem(elem(arg_type(0)), elem(arg_type(1))))
                        : elem(arg_type(0));
    return t;
  }
  if (name == "norm") {
    return StyioDataType{StyioDataTypeOption::Float, "f64", 64};
  }
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

std::string
matrix_suffix_for_type(const StyioDataType& type) {
  return styio_value_family_for_type(styio_data_type_from_name(styio_matrix_elem_type_name(type)))
             == StyioValueFamily::Float
           ? "f64"
           : "i64";
}

std::string
matrix_suffix_for_scalar_type(const StyioDataType& type) {
  return styio_value_family_for_type(type) == StyioValueFamily::Float ? "f64" : "i64";
}

std::string
matrix_intrinsic_runtime_name(AstToStyioIRLowerer* an, FuncCallAST* call) {
  const std::string name = call->getNameAsStr();
  StyioDataType result_type = expr_lowered_type(an, call);
  auto arg_type = [&](size_t i)
  {
    return i < call->getArgList().size()
             ? expr_lowered_type(an, call->getArgList()[i])
             : StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  };

  if (name == "mat_zeros") {
    return "__styio_matrix_new_f64";
  }
  if (name == "mat_zeros_i64") {
    return "__styio_matrix_new_i64";
  }
  if (name == "mat_identity") {
    return "__styio_matrix_identity_f64";
  }
  if (name == "mat_identity_i64") {
    return "__styio_matrix_identity_i64";
  }
  if (name == "mat_shape" || name == "mat_rows" || name == "mat_cols") {
    return "__styio_matrix_" + name.substr(4);
  }
  if (name == "norm") {
    return "__styio_matrix_norm";
  }
  if (name == "mat_get" || name == "mat_set") {
    return "__styio_matrix_" + name.substr(4) + "_" + matrix_suffix_for_type(arg_type(0));
  }
  if (name == "mat_clone") {
    return "__styio_matrix_clone_" + matrix_suffix_for_type(result_type);
  }
  if (name == "mat_add" || name == "mat_sub" || name == "mat_scale" || name == "mat_hadamard") {
    return "__styio_matrix_" + name.substr(4) + "_" + matrix_suffix_for_type(result_type);
  }
  if (name == "matmul") {
    return "__styio_matrix_matmul_" + matrix_suffix_for_type(result_type);
  }
  if (name == "transpose") {
    return "__styio_matrix_transpose_" + matrix_suffix_for_type(result_type);
  }
  if (name == "dot") {
    return "__styio_matrix_dot_" + matrix_suffix_for_scalar_type(result_type);
  }
  if (name == "mat_sum") {
    return "__styio_matrix_sum_" + matrix_suffix_for_scalar_type(result_type);
  }
  return name;
}

bool
collection_elem_is_string(AstToStyioIRLowerer* an, StyioAST* coll) {
  if (auto bound = bound_type_of(an, coll)) {
    return styio_type_item_type_name(*bound) == "string";
  }
  if (styio_type_item_type_name(coll->getDataType()) == "string") {
    return true;
  }
  auto* L = dynamic_cast<ListAST*>(coll);
  if (!L || L->getElements().empty()) {
    return false;
  }
  return L->getElements()[0]->getNodeType() == StyioNodeType::String;
}

std::string
resource_family_for_lowering_type(const StyioDataType& type) {
  if (type.handle_family == StyioHandleFamily::File) {
    return "file";
  }
  if (type.handle_family == StyioHandleFamily::Stream) {
    if (type.has_std_stream_kind) {
      return styio_std_stream_family_name(static_cast<StdStreamKind>(type.std_stream_kind));
    }
    return "stream";
  }
  if (styio_is_topology_resource_type(type)) {
    return "resource";
  }
  return "";
}

std::string
resource_family_for_lowering_expr(AstToStyioIRLowerer* an, StyioAST* expr) {
  if (expr == nullptr) {
    return "";
  }
  if (dynamic_cast<FileResourceAST*>(expr) != nullptr) {
    return "file";
  }
  if (auto* stream = dynamic_cast<StdStreamAST*>(expr)) {
    return styio_std_stream_family_name(stream->getStreamKind());
  }
  if (auto* receiver = dynamic_cast<ResourceReceiverAST*>(expr)) {
    return receiver->getFamilyName();
  }
  if (dynamic_cast<ResourceRefAST*>(expr) != nullptr) {
    return "resource";
  }
  return resource_family_for_lowering_type(expr_lowered_type(an, expr));
}

std::string
predefined_list_operation_runtime_name(const std::string& method, const StyioDataType& list_type) {
  if (styio_builtin_method_kind(method) == StyioBuiltinMethodKind::ListPop) {
    return "__styio_list_pop";
  }

  const std::string elem_name = styio_type_item_type_name(list_type);
  const char* suffix = "i64";
  switch (styio_value_family_from_type_name(elem_name)) {
    case StyioValueFamily::Bool:
      suffix = "bool";
      break;
    case StyioValueFamily::Char:
      suffix = "char";
      break;
    case StyioValueFamily::Float:
      suffix = "f64";
      break;
    case StyioValueFamily::String:
      suffix = "cstr";
      break;
    case StyioValueFamily::ListHandle:
      suffix = "list";
      break;
    case StyioValueFamily::DictHandle:
      suffix = "dict";
      break;
    case StyioValueFamily::MatrixHandle:
      suffix = "matrix";
      break;
    case StyioValueFamily::Integer:
    default:
      break;
  }
  return std::string("__styio_list_") + method + "_" + suffix;
}

bool
ast_value_mentions_float(StyioAST* ast) {
  if (!ast) {
    return false;
  }
  if (ast->getNodeType() == StyioNodeType::Float) {
    return true;
  }
  if (auto* bin = dynamic_cast<BinOpAST*>(ast)) {
    StyioDataType t = bin->getType();
    return t.option == StyioDataTypeOption::Float
           || ast_value_mentions_float(bin->getLHS())
           || ast_value_mentions_float(bin->getRHS());
  }
  return false;
}

void
scan_returns_for_value_kinds(StyioAST* ast, bool& has_str, bool& has_int, bool& has_float) {
  if (!ast) {
    return;
  }
  auto scan_value = [&](StyioAST* e)
  {
    if (!e) {
      return;
    }
    if (e->getNodeType() == StyioNodeType::String) {
      has_str = true;
    }
    else if (ast_value_mentions_float(e)) {
      has_float = true;
    }
    else if (e->getNodeType() == StyioNodeType::Integer) {
      has_int = true;
    }
    else {
      has_int = true;
    }
  };
  if (ast->getNodeType() == StyioNodeType::Return) {
    scan_value(static_cast<ReturnAST*>(ast)->getExpr());
    return;
  }
  if (ast->getNodeType() == StyioNodeType::MatchCases) {
    auto* m = static_cast<MatchCasesAST*>(ast);
    CasesAST* c = m->getCases();
    for (auto const& pr : c->case_list) {
      scan_returns_for_value_kinds(pr.second, has_str, has_int, has_float);
    }
    scan_returns_for_value_kinds(c->case_default, has_str, has_int, has_float);
    return;
  }
  if (auto* b = dynamic_cast<BlockAST*>(ast)) {
    for (auto* s : b->stmts) {
      scan_returns_for_value_kinds(s, has_str, has_int, has_float);
    }
    if (!b->stmts.empty()) {
      StyioAST* tail = b->stmts.back();
      if (tail->getNodeType() != StyioNodeType::Return && ast_can_be_implicit_tail_value(tail)) {
        scan_returns_for_value_kinds(tail, has_str, has_int, has_float);
      }
    }
    return;
  }
  if (ast_can_be_implicit_tail_value(ast)) {
    scan_value(ast);
  }
}

std::optional<SGMatchReprKind>
match_repr_kind_for_type(const StyioDataType& type) {
  if (type.isUndefined()) {
    return std::nullopt;
  }
  StyioValueFamily family = styio_value_family_for_type(type);
  if (family == StyioValueFamily::String) {
    return SGMatchReprKind::ExprMixed;
  }
  if (family == StyioValueFamily::Float) {
    return SGMatchReprKind::ExprFloat;
  }
  if (family == StyioValueFamily::Bool) {
    return SGMatchReprKind::ExprBool;
  }
  if (family == StyioValueFamily::Char) {
    return SGMatchReprKind::ExprChar;
  }
  if (family == StyioValueFamily::Integer) {
    return SGMatchReprKind::ExprInt;
  }
  return std::nullopt;
}

SGMatchReprKind
classify_cases(CasesAST* c, const StyioDataType* inferred_type = nullptr) {
  bool any = false;
  for (auto const& pr : c->case_list) {
    if (stmt_has_return_tree(pr.second) || ast_has_tail_value(pr.second)) {
      any = true;
    }
  }
  if (stmt_has_return_tree(c->case_default) || ast_has_tail_value(c->case_default)) {
    any = true;
  }
  if (!any) {
    return SGMatchReprKind::Stmt;
  }
  if (inferred_type != nullptr) {
    if (std::optional<SGMatchReprKind> kind = match_repr_kind_for_type(*inferred_type)) {
      return *kind;
    }
  }
  bool hs = false;
  bool hi = false;
  bool hf = false;
  for (auto const& pr : c->case_list) {
    scan_returns_for_value_kinds(pr.second, hs, hi, hf);
  }
  scan_returns_for_value_kinds(c->case_default, hs, hi, hf);
  if (hs && (hi || hf)) {
    return SGMatchReprKind::ExprMixed;
  }
  if (hs) {
    /* All arms yield strings (or only strings detected): phi must be i8* */
    return SGMatchReprKind::ExprMixed;
  }
  if (hf) {
    return SGMatchReprKind::ExprFloat;
  }
  return SGMatchReprKind::ExprInt;
}

SGMatch*
lower_cases_with_scrutinee(
  AstToStyioIRLowerer* an,
  CasesAST* c,
  StyioIR* scrutinee_ir,
  const std::string* scrutinee_name,
  const StyioDataType* inferred_type = nullptr
) {
  SGMatchReprKind rk = classify_cases(c, inferred_type);
  const bool implicit_tail_value = rk != SGMatchReprKind::Stmt;
  std::vector<std::pair<std::int64_t, SGBlock*>> arms;
  for (auto const& pr : c->case_list) {
    std::optional<std::int64_t> arm_value =
      match_case_pattern_value_for_name(pr.first, scrutinee_name);
    if (!arm_value.has_value()) {
      throw StyioTypeError("match arms need integer literal patterns in this language feature");
    }
    arms.push_back({*arm_value, lower_func_body(an, pr.second, implicit_tail_value)});
  }
  SGBlock* def = nullptr;
  if (c->case_default) {
    def = lower_func_body(an, c->case_default, implicit_tail_value);
  }
  return SGMatch::Create(scrutinee_ir, std::move(arms), def, rk);
}

bool
body_returns_single_state_decl(StyioAST* body, StateDeclAST*& out_sd) {
  out_sd = nullptr;
  if (body == nullptr) {
    return false;
  }

  if (auto* sd = dynamic_cast<StateDeclAST*>(body)) {
    out_sd = sd;
    return true;
  }

  auto* blk = dynamic_cast<BlockAST*>(body);
  if (blk == nullptr || blk->stmts.size() != 1) {
    return false;
  }

  out_sd = dynamic_cast<StateDeclAST*>(blk->stmts[0]);
  return out_sd != nullptr;
}

bool
simple_func_returns_single_state_decl(SimpleFuncAST* sf, StateDeclAST*& out_sd) {
  if (sf == nullptr) {
    out_sd = nullptr;
    return false;
  }
  return body_returns_single_state_decl(sf->ret_expr, out_sd);
}

bool
function_ast_returns_single_state_decl(FunctionAST* fn, StateDeclAST*& out_sd) {
  if (fn == nullptr) {
    out_sd = nullptr;
    return false;
  }
  return body_returns_single_state_decl(fn->func_body, out_sd);
}

bool
stmt_may_contain_pulse_state(AstToStyioIRLowerer* an, StyioAST* s) {
  if (dynamic_cast<StateDeclAST*>(s)) {
    return true;
  }
  if (auto* fc = dynamic_cast<FuncCallAST*>(s)) {
    StyioAST* def = an->find_function_def(
      fc->func_name->getSymbolId(),
      fc->getNameAsStr()
    );
    if (def == nullptr) {
      return false;
    }
    if (auto* sf = dynamic_cast<SimpleFuncAST*>(def)) {
      StateDeclAST* sd = nullptr;
      if (simple_func_returns_single_state_decl(sf, sd)) {
        return true;
      }
    }
    if (auto* fn = dynamic_cast<FunctionAST*>(def)) {
      StateDeclAST* sd = nullptr;
      if (function_ast_returns_single_state_decl(fn, sd)) {
        return true;
      }
    }
  }
  return false;
}

bool
pulse_block_has_state(AstToStyioIRLowerer* an, BlockAST* blk) {
  if (!blk) {
    return false;
  }
  for (auto* s : blk->stmts) {
    if (stmt_may_contain_pulse_state(an, s)) {
      return true;
    }
  }
  return false;
}

SeriesIntrinsicAST*
series_intrinsic_helper_body(AstToStyioIRLowerer* an, FuncCallAST* fc) {
  if (!an || !fc || fc->getArgList().empty()) {
    return nullptr;
  }
  StyioAST* def = an->find_function_def(
    fc->func_name->getSymbolId(),
    fc->getNameAsStr()
  );
  if (def == nullptr) {
    return nullptr;
  }
  auto* sf = dynamic_cast<SimpleFuncAST*>(def);
  if (!sf || sf->params.size() != fc->getArgList().size()) {
    return nullptr;
  }
  auto* body = dynamic_cast<SeriesIntrinsicAST*>(sf->ret_expr);
  if (!body) {
    return nullptr;
  }
  auto* base_name = dynamic_cast<NameAST*>(body->getBase());
  if (!base_name || base_name->getAsStr() != sf->params[0]->getName()) {
    return nullptr;
  }
  return body;
}

SeriesIntrinsicAST*
find_series_intrinsic(AstToStyioIRLowerer* an, StyioAST* e) {
  if (!e) {
    return nullptr;
  }
  if (auto* s = dynamic_cast<SeriesIntrinsicAST*>(e)) {
    return s;
  }
  if (auto* fc = dynamic_cast<FuncCallAST*>(e)) {
    return series_intrinsic_helper_body(an, fc);
  }
  if (auto* b = dynamic_cast<BinOpAST*>(e)) {
    auto* L = find_series_intrinsic(an, b->LHS);
    if (L) {
      return L;
    }
    return find_series_intrinsic(an, b->RHS);
  }
  if (auto* w = dynamic_cast<WaveMergeAST*>(e)) {
    auto* L = find_series_intrinsic(an, w->getCond());
    if (L) {
      return L;
    }
    L = find_series_intrinsic(an, w->getTrueVal());
    if (L) {
      return L;
    }
    return find_series_intrinsic(an, w->getFalseVal());
  }
  return nullptr;
}

int
window_n_from_ast(StyioAST* w) {
  auto* li = dynamic_cast<IntAST*>(w);
  if (!li) {
    throw StyioTypeError("window size for series intrinsic must be integer literal");
  }
  return static_cast<int>(std::stoll(li->value));
}

int
slot_byte_size(const SGStateSlotDesc& d) {
  const int n = d.win_n;
  switch (d.kind) {
    case SGStateSlotKind::Acc:
      return 8;
    case SGStateSlotKind::Track:
      return 8 + 8 * n + 8;
    case SGStateSlotKind::WinAvg:
    case SGStateSlotKind::WinMax:
      return n * 8 + 32 + n * 8 + 8;
    default:
      return 8;
  }
}

void
classify_state_slot(AstToStyioIRLowerer* an, StateDeclAST* sd, SGStateSlotDesc& d) {
  if (sd->getAccName()) {
    d.kind = SGStateSlotKind::Acc;
    d.win_n = 0;
    return;
  }
  auto* si = find_series_intrinsic(an, sd->getUpdateExpr());
  if (si && si->getOp() == SeriesIntrinsicOp::Avg) {
    d.kind = SGStateSlotKind::WinAvg;
    d.win_n = window_n_from_ast(si->getWindow());
    return;
  }
  if (si && si->getOp() == SeriesIntrinsicOp::Max) {
    d.kind = SGStateSlotKind::WinMax;
    d.win_n = window_n_from_ast(si->getWindow());
    return;
  }
  d.kind = SGStateSlotKind::Track;
  if (!sd->getWindowHeader()) {
    throw StyioTypeError("retired state window header required for non-accum internal state");
  }
  d.win_n = static_cast<int>(std::stoll(sd->getWindowHeader()->value));
}

StyioAST*
clone_state_expr_with_subst(StyioAST* e, const std::string& pname, StyioAST* repl);

StyioAST*
clone_state_expr(StyioAST* e) {
  return clone_state_expr_with_subst(e, std::string{}, nullptr);
}

TypeAST*
clone_type_for_var(TypeAST* t) {
  if (!t) {
    return TypeAST::Create();
  }
  return TypeAST::Create(t->getDataType());
}

VarAST*
clone_var_ast(VarAST* v) {
  if (!v) {
    return nullptr;
  }
  auto* name = NameAST::Clone(v->getName());
  auto* dtype = clone_type_for_var(v->var_type);
  if (!v->val_init) {
    return VarAST::Create(name, dtype);
  }
  return new VarAST(name, dtype, clone_state_expr(v->val_init));
}

ParamAST*
clone_param_ast(ParamAST* p) {
  if (!p) {
    return nullptr;
  }
  auto* name = NameAST::Create(p->getNameAsStr());
  auto* dtype = clone_type_for_var(p->getDType());
  if (!p->val_init) {
    return ParamAST::Create(name, dtype);
  }
  return ParamAST::Create(name, dtype, clone_state_expr(p->val_init));
}

IntAST*
clone_int_ast(IntAST* i) {
  if (!i) {
    return nullptr;
  }
  return IntAST::Create(i->value, i->num_of_bit);
}

NameAST*
clone_name_ast(NameAST* n) {
  if (!n) {
    return nullptr;
  }
  return NameAST::Clone(n);
}

class StateExprCloneVisitor
{
  std::string pname_;
  StyioAST* repl_;
  std::unordered_map<std::string, StyioAST*> named_repls_;
  std::unordered_set<std::string> local_repl_names_;
  std::string receiver_family_;
  StyioAST* receiver_repl_ = nullptr;
  std::vector<std::unique_ptr<StyioAST>> generated_repl_owners_;

  template <typename Container>
  std::vector<StyioAST*> clone_child_list(const Container& items) {
    std::vector<StyioAST*> cloned;
    cloned.reserve(items.size());
    for (auto* item : items) {
      cloned.push_back(clone(item));
    }
    return cloned;
  }

  StyioAST* clone_without_subst(StyioAST* expr) {
    StateExprCloneVisitor plain("", nullptr);
    return plain.clone(expr);
  }

  StyioAST* clone(NameAST* expr) {
    auto named = named_repls_.find(expr->getAsStr());
    if (named != named_repls_.end() && named->second != nullptr) {
      return clone_without_subst(named->second);
    }
    return NameAST::Clone(expr);
  }

  StyioAST* clone_assignment_target(StyioAST* expr) {
    if (auto* name = dynamic_cast<NameAST*>(expr)) {
      auto named = named_repls_.find(name->getAsStr());
      if (local_repl_names_.count(name->getAsStr()) != 0
          && named != named_repls_.end()
          && named->second != nullptr) {
        return clone_without_subst(named->second);
      }
      return NameAST::Clone(name);
    }
    if (auto* index = dynamic_cast<ListOpAST*>(expr)) {
      if (index->getSlot1() != nullptr && index->getSlot2() != nullptr) {
        return new ListOpAST(
          index->getOp(),
          clone_assignment_target(index->getList()),
          clone(index->getSlot1()),
          clone(index->getSlot2()));
      }
      if (index->getSlot1() != nullptr) {
        return new ListOpAST(
          index->getOp(),
          clone_assignment_target(index->getList()),
          clone(index->getSlot1()));
      }
      return new ListOpAST(index->getOp(), clone_assignment_target(index->getList()));
    }
    return clone(expr);
  }

  StyioAST* clone(CommentAST* expr) {
    return CommentAST::Create(expr->getText());
  }

  StyioAST* clone(EmptyAST*) {
    return EmptyAST::Create();
  }

  StyioAST* clone(IntAST* expr) {
    return IntAST::Create(expr->value, expr->num_of_bit);
  }

  StyioAST* clone(FloatAST* expr) {
    return FloatAST::Create(expr->getValue());
  }

  StyioAST* clone(BoolAST* expr) {
    return BoolAST::Create(expr->getValue());
  }

  StyioAST* clone(StringAST* expr) {
    return StringAST::Create(expr->getValue());
  }

  StyioAST* clone(CharAST* expr) {
    return CharAST::Create(expr->getValue());
  }

  StyioAST* clone(TupleAST* expr) {
    return TupleAST::Create(clone_child_list(expr->getElements()));
  }

  StyioAST* clone(ListAST* expr) {
    ListAST* out = ListAST::Create(clone_child_list(expr->getElements()));
    out->setDataType(expr->getDataType());
    return out;
  }

  StyioAST* clone(DictAST* expr) {
    std::vector<std::pair<StyioAST*, StyioAST*>> entries;
    entries.reserve(expr->getEntries().size());
    for (const auto& entry : expr->getEntries()) {
      entries.emplace_back(clone(entry.key), clone(entry.value));
    }
    DictAST* out = DictAST::Create(std::move(entries));
    out->setDataType(expr->getDataType());
    return out;
  }

  StyioAST* clone(SetAST* expr) {
    return SetAST::Create(clone_child_list(expr->getElements()));
  }

  StyioAST* clone(BinOpAST* expr) {
    return BinOpAST::Create(expr->operand, clone(expr->LHS), clone(expr->RHS));
  }

  StyioAST* clone(BinCompAST* expr) {
    return new BinCompAST(expr->getSign(), clone(expr->getLHS()), clone(expr->getRHS()));
  }

  StyioAST* clone(CondAST* expr) {
    if (expr->getLHS() != nullptr && expr->getRHS() != nullptr) {
      return CondAST::Create(expr->getSign(), clone(expr->getLHS()), clone(expr->getRHS()));
    }
    return CondAST::Create(expr->getSign(), clone(expr->getValue()));
  }

  StyioAST* clone(CondFlowAST* expr) {
    if (expr->getElse() != nullptr) {
      return new CondFlowAST(
        expr->getNodeType(),
        static_cast<CondAST*>(clone(expr->getCond())),
        clone(expr->getThen()),
        clone(expr->getElse())
      );
    }
    return new CondFlowAST(
      expr->getNodeType(),
      static_cast<CondAST*>(clone(expr->getCond())),
      clone(expr->getThen())
    );
  }

  StyioAST* clone(WaveMergeAST* expr) {
    return WaveMergeAST::Create(
      clone(expr->getCond()),
      clone(expr->getTrueVal()),
      clone(expr->getFalseVal())
    );
  }

  StyioAST* clone(WaveDispatchAST* expr) {
    return WaveDispatchAST::Create(
      clone(expr->getCond()),
      clone(expr->getTrueArm()),
      clone(expr->getFalseArm())
    );
  }

  StyioAST* clone(FallbackAST* expr) {
    return FallbackAST::Create(clone(expr->getPrimary()), clone(expr->getAlternate()));
  }

  StyioAST* clone(FmtStrAST* expr) {
    return FmtStrAST::Create(expr->getFragments(), clone_child_list(expr->getExprs()));
  }

  StyioAST* clone(GuardSelectorAST* expr) {
    return GuardSelectorAST::Create(clone(expr->getBase()), clone(expr->getCond()));
  }

  StyioAST* clone(EqProbeAST* expr) {
    return EqProbeAST::Create(clone(expr->getBase()), clone(expr->getProbeValue()));
  }

  StyioAST* clone(RangeAST* expr) {
    return new RangeAST(clone(expr->getStart()), clone(expr->getEnd()), clone(expr->getStep()));
  }

  StyioAST* clone(SizeOfAST* expr) {
    auto* cloned = new SizeOfAST(clone(expr->getValue()));
    cloned->setDataType(expr->getDataType());
    return cloned;
  }

  StyioAST* clone(TypeConvertAST* expr) {
    return TypeConvertAST::Create(clone(expr->getValue()), expr->getPromoTy());
  }

  TypeAST* clone_type_ast(TypeAST* expr) {
    if (expr == nullptr) {
      return TypeAST::Create();
    }
    return TypeAST::Create(expr->getDataType());
  }

  TypeTupleAST* clone_type_tuple_ast(TypeTupleAST* expr) {
    if (expr == nullptr) {
      return TypeTupleAST::Create();
    }
    std::vector<TypeAST*> cloned_types;
    cloned_types.reserve(expr->type_list.size());
    for (auto* type : expr->type_list) {
      cloned_types.push_back(clone_type_ast(type));
    }
    return TypeTupleAST::Create(std::move(cloned_types));
  }

  std::variant<TypeAST*, TypeTupleAST*> clone_ret_type(
    const std::variant<TypeAST*, TypeTupleAST*>& ret_type
  ) {
    if (ret_type.valueless_by_exception()) {
      return TypeAST::Create();
    }
    if (std::holds_alternative<TypeAST*>(ret_type)) {
      return clone_type_ast(std::get<TypeAST*>(ret_type));
    }
    return clone_type_tuple_ast(std::get<TypeTupleAST*>(ret_type));
  }

  StyioAST* clone(HandleAcquireAST* expr) {
    StyioAST* resource = clone(expr->getResource());
    const std::string original_name = expr->getVar()->getNameAsStr();
    const std::string local_name =
      alloc_lowering_tmp_name("__styio_resource_method_local_");
    auto* local_var = VarAST::Create(
      NameAST::Create(local_name),
      clone_type_for_var(expr->getVar()->getDType())
    );
    auto* repl_name = NameAST::Create(local_name);
    generated_repl_owners_.emplace_back(repl_name);
    named_repls_[original_name] = repl_name;
    local_repl_names_.insert(original_name);
    return HandleAcquireAST::Create(local_var, resource, expr->getBindMode());
  }

  StyioAST* clone(ListOpAST* expr) {
    if (expr->getSlot1() != nullptr && expr->getSlot2() != nullptr) {
      return new ListOpAST(expr->getOp(), clone(expr->getList()), clone(expr->getSlot1()), clone(expr->getSlot2()));
    }
    if (expr->getSlot1() != nullptr) {
      return new ListOpAST(expr->getOp(), clone(expr->getList()), clone(expr->getSlot1()));
    }
    return new ListOpAST(expr->getOp(), clone(expr->getList()));
  }

  StyioAST* clone(FuncCallAST* expr) {
    std::vector<StyioAST*> args = clone_child_list(expr->getArgList());
    FuncCallAST* cloned = nullptr;
    if (expr->func_callee != nullptr) {
      cloned = FuncCallAST::Create(
        clone(expr->func_callee),
        NameAST::Clone(expr->func_name),
        std::move(args));
    }
    else {
      cloned = FuncCallAST::Create(
        NameAST::Clone(expr->func_name),
        std::move(args));
    }
    cloned->copyInferenceMetadataFrom(*expr);
    return cloned;
  }

  StyioAST* clone(AttrAST* expr) {
    return AttrAST::Create(clone(expr->body), clone(expr->attr));
  }

  StyioAST* clone(FileResourceAST* expr) {
    return FileResourceAST::Create(clone(expr->getPath()), expr->isAutoDetect());
  }

  StyioAST* clone(StdStreamAST* expr) {
    if (expr->isTerminalHandle()) {
      return StdStreamAST::CreateTerminalHandle(expr->getStreamKind());
    }
    return StdStreamAST::Create(expr->getStreamKind());
  }

  StyioAST* clone(EmptyResourceAST*) {
    return EmptyResourceAST::Create();
  }

  StyioAST* clone(ResourceReceiverAST* expr) {
    if (receiver_repl_ != nullptr && expr->getFamilyName() == receiver_family_) {
      return clone_without_subst(receiver_repl_);
    }
    return ResourceReceiverAST::Create(expr->getFamilyName());
  }

  StyioAST* clone(ResourceRefAST* expr) {
    if (expr->isWholeResource()) {
      return ResourceRefAST::Create(NameAST::Clone(expr->getName()));
    }
    return ResourceRefAST::CreateSelector(
      NameAST::Clone(expr->getName()),
      expr->getSelectorKind(),
      expr->getSelectorOffset()
    );
  }

  StyioAST* clone(ResourceWriteAST* expr) {
    return ResourceWriteAST::Create(clone(expr->getData()), clone(expr->getResource()));
  }

  StyioAST* clone(ResourceRedirectAST* expr) {
    return ResourceRedirectAST::Create(clone(expr->getData()), clone(expr->getResource()));
  }

  StyioAST* clone(ResourceEffectAST* expr) {
    std::vector<ResourceEffectAST::Handler> handlers;
    handlers.reserve(expr->getHandlers().size());
    for (const auto& handler : expr->getHandlers()) {
      handlers.emplace_back(handler.effect_name, clone(handler.body));
    }
    auto* cloned = ResourceEffectAST::Create(
      clone(expr->getOperation()),
      expr->hasFallback() ? clone(expr->getFallback()) : nullptr,
      expr->isDiscard(),
      std::move(handlers),
      expr->isValueRequired()
    );
    cloned->setResultType(expr->getDataType());
    return cloned;
  }

  StyioAST* clone(InstantPullAST* expr) {
    return InstantPullAST::Create(clone(expr->getResource()), expr->getDataType());
  }

  StyioAST* clone(VarAST* expr) {
    if (expr == nullptr) {
      return nullptr;
    }
    auto* name = NameAST::Clone(expr->getName());
    auto* dtype = clone_type_for_var(expr->getDType());
    if (expr->val_init == nullptr) {
      return VarAST::Create(name, dtype);
    }
    return new VarAST(name, dtype, clone(expr->val_init));
  }

  StyioAST* clone(HistoryProbeAST* expr) {
    StyioAST* cloned_target = clone(expr->getTarget());
    if (cloned_target == nullptr || cloned_target->getNodeType() != StyioNodeType::StateRef) {
      throw StyioTypeError("history probe target must remain state reference");
    }
    return HistoryProbeAST::Create(static_cast<StateRefAST*>(cloned_target), clone(expr->getDepth()));
  }

  StyioAST* clone(StateRefAST* expr) {
    return StateRefAST::Create(NameAST::Clone(expr->getName()));
  }

  StyioAST* clone(SeriesIntrinsicAST* expr) {
    return SeriesIntrinsicAST::Create(clone(expr->getBase()), expr->getOp(), clone(expr->getWindow()));
  }

  StyioAST* clone(FunctionAST* expr) {
    std::vector<ParamAST*> params;
    params.reserve(expr->params.size());
    for (auto* param : expr->params) {
      params.push_back(clone_param_ast(param));
    }

    auto saved_repls = named_repls_;
    auto saved_local_repls = local_repl_names_;
    if (expr->func_name != nullptr) {
      named_repls_.erase(expr->func_name->getAsStr());
      local_repl_names_.erase(expr->func_name->getAsStr());
    }
    for (auto* param : params) {
      if (param != nullptr) {
        named_repls_.erase(param->getNameAsStr());
        local_repl_names_.erase(param->getNameAsStr());
      }
    }

    StyioAST* body = clone(expr->func_body);
    named_repls_ = std::move(saved_repls);
    local_repl_names_ = std::move(saved_local_repls);

    auto* cloned = FunctionAST::Create(
      NameAST::Clone(expr->func_name),
      expr->is_unique,
      std::move(params),
      clone_ret_type(expr->ret_type),
      body
    );
    std::vector<NameAST*> captures;
    captures.reserve(expr->getCaptureNames().size());
    for (auto* capture : expr->getCaptureNames()) {
      captures.push_back(NameAST::Clone(capture));
    }
    cloned->setCaptureNames(std::move(captures));
    cloned->is_self_completed = expr->is_self_completed;
    return cloned;
  }

  StyioAST* clone(SimpleFuncAST* expr) {
    std::vector<ParamAST*> params;
    params.reserve(expr->params.size());
    for (auto* param : expr->params) {
      params.push_back(clone_param_ast(param));
    }

    auto saved_repls = named_repls_;
    auto saved_local_repls = local_repl_names_;
    if (expr->func_name != nullptr) {
      named_repls_.erase(expr->func_name->getAsStr());
      local_repl_names_.erase(expr->func_name->getAsStr());
    }
    for (auto* param : params) {
      if (param != nullptr) {
        named_repls_.erase(param->getNameAsStr());
        local_repl_names_.erase(param->getNameAsStr());
      }
    }

    StyioAST* ret_expr = clone(expr->ret_expr);
    named_repls_ = std::move(saved_repls);
    local_repl_names_ = std::move(saved_local_repls);

    auto* cloned = SimpleFuncAST::Create(
      NameAST::Clone(expr->func_name),
      expr->is_unique,
      std::move(params),
      clone_ret_type(expr->ret_type),
      ret_expr
    );
    std::vector<NameAST*> captures;
    captures.reserve(expr->getCaptureNames().size());
    for (auto* capture : expr->getCaptureNames()) {
      captures.push_back(NameAST::Clone(capture));
    }
    cloned->setCaptureNames(std::move(captures));
    return cloned;
  }

  StyioAST* clone(ReturnAST* expr) {
    return ReturnAST::Create(clone(expr->getExpr()));
  }

  StyioAST* clone(FlexBindAST* expr) {
    StyioAST* value = clone(expr->getValue());
    const std::string original_name = expr->getNameAsStr();
    const std::string local_name =
      alloc_lowering_tmp_name("__styio_resource_method_local_");
    auto* local_var = VarAST::Create(
      NameAST::Create(local_name),
      clone_type_for_var(expr->getVar()->getDType())
    );
    auto* repl_name = NameAST::Create(local_name);
    generated_repl_owners_.emplace_back(repl_name);
    named_repls_[original_name] = repl_name;
    local_repl_names_.insert(original_name);
    return FlexBindAST::Create(local_var, value);
  }

  StyioAST* clone(FinalBindAST* expr) {
    StyioAST* value = clone(expr->getValue());
    const std::string original_name = expr->getName();
    const std::string local_name =
      alloc_lowering_tmp_name("__styio_resource_method_local_");
    auto* local_var = VarAST::Create(
      NameAST::Create(local_name),
      clone_type_for_var(expr->getVar()->getDType())
    );
    auto* repl_name = NameAST::Create(local_name);
    generated_repl_owners_.emplace_back(repl_name);
    named_repls_[original_name] = repl_name;
    local_repl_names_.insert(original_name);
    return FinalBindAST::Create(local_var, value);
  }

  StyioAST* clone(ParallelAssignAST* expr) {
    std::vector<StyioAST*> rhs = clone_child_list(expr->getRHS());

    std::vector<StyioAST*> lhs;
    lhs.reserve(expr->getLHS().size());
    for (auto* target : expr->getLHS()) {
      lhs.push_back(clone_assignment_target(target));
    }
    return ParallelAssignAST::Create(std::move(lhs), std::move(rhs));
  }

  StyioAST* clone(PrintAST* expr) {
    return PrintAST::Create(clone_child_list(expr->exprs));
  }

  StyioAST* clone(IteratorAST* expr) {
    StyioAST* collection = clone(expr->collection);
    std::vector<ParamAST*> params;
    params.reserve(expr->params.size());
    for (auto* param : expr->params) {
      params.push_back(clone_param_ast(param));
    }

    auto saved_repls = named_repls_;
    auto saved_local_repls = local_repl_names_;
    for (auto* param : params) {
      if (param != nullptr) {
        named_repls_.erase(param->getNameAsStr());
        local_repl_names_.erase(param->getNameAsStr());
      }
    }
    std::vector<StyioAST*> following = clone_child_list(expr->following);
    named_repls_ = std::move(saved_repls);
    local_repl_names_ = std::move(saved_local_repls);
    return IteratorAST::Create(collection, std::move(params), std::move(following));
  }

  StyioAST* clone(StreamZipAST* expr) {
    StyioAST* collection_a = clone(expr->getCollectionA());
    StyioAST* collection_b = clone(expr->getCollectionB());
    std::vector<ParamAST*> params_a;
    std::vector<ParamAST*> params_b;
    params_a.reserve(expr->getParamsA().size());
    params_b.reserve(expr->getParamsB().size());
    for (auto* param : expr->getParamsA()) {
      params_a.push_back(clone_param_ast(param));
    }
    for (auto* param : expr->getParamsB()) {
      params_b.push_back(clone_param_ast(param));
    }

    auto saved_repls = named_repls_;
    auto saved_local_repls = local_repl_names_;
    for (auto* param : params_a) {
      if (param != nullptr) {
        named_repls_.erase(param->getNameAsStr());
        local_repl_names_.erase(param->getNameAsStr());
      }
    }
    for (auto* param : params_b) {
      if (param != nullptr) {
        named_repls_.erase(param->getNameAsStr());
        local_repl_names_.erase(param->getNameAsStr());
      }
    }

    StyioAST* body = PassAST::Create();
    if (expr->getFollowing().size() == 1) {
      body = clone(expr->getFollowing()[0]);
    }
    else if (expr->getFollowing().size() > 1) {
      body = BlockAST::Create(clone_child_list(expr->getFollowing()));
    }
    named_repls_ = std::move(saved_repls);
    local_repl_names_ = std::move(saved_local_repls);
    return StreamZipAST::Create(
      collection_a,
      std::move(params_a),
      collection_b,
      std::move(params_b),
      body);
  }

  StyioAST* clone(CasesAST* expr) {
    std::vector<std::pair<StyioAST*, StyioAST*>> cloned_cases;
    cloned_cases.reserve(expr->getCases().size());
    for (const auto& entry : expr->getCases()) {
      cloned_cases.emplace_back(clone(entry.first), clone(entry.second));
    }
    return CasesAST::Create(std::move(cloned_cases), clone(expr->case_default));
  }

  StyioAST* clone(MatchCasesAST* expr) {
    StyioAST* cloned_cases = clone(expr->getCases());
    if (cloned_cases == nullptr || cloned_cases->getNodeType() != StyioNodeType::Cases) {
      throw StyioTypeError("match cases clone requires CasesAST");
    }
    auto* cloned = MatchCasesAST::make(clone(expr->getScrutinee()), static_cast<CasesAST*>(cloned_cases));
    cloned->setDataType(expr->getDataType());
    return cloned;
  }

  StyioAST* clone(InfiniteAST* expr) {
    if (expr->getType() == InfiniteType::Incremental) {
      return new InfiniteAST(clone(expr->getStart()), clone(expr->getIncEl()));
    }
    return new InfiniteAST();
  }

  StyioAST* clone(PassAST*) {
    return PassAST::Create();
  }

  StyioAST* clone(BreakAST* expr) {
    (void)expr;
    return BreakAST::Create(1u);
  }

  StyioAST* clone(ContinueAST* expr) {
    (void)expr;
    return ContinueAST::Create();
  }

  StyioAST* clone(BlockAST* expr) {
    auto saved_repls = named_repls_;
    auto saved_local_repls = local_repl_names_;
    std::vector<StyioAST*> stmts;
    stmts.reserve(expr->stmts.size());
    for (auto* stmt : expr->stmts) {
      stmts.push_back(clone(stmt));
    }
    std::vector<StyioAST*> followings = clone_child_list(expr->followings);
    named_repls_ = std::move(saved_repls);
    local_repl_names_ = std::move(saved_local_repls);
    auto* cloned_blk = BlockAST::Create(std::move(stmts));
    cloned_blk->set_followings(std::move(followings));
    return cloned_blk;
  }

  StyioAST* clone(UndefinedLitAST*) {
    return UndefinedLitAST::Create();
  }

public:
  StateExprCloneVisitor(const std::string& pname, StyioAST* repl) :
      pname_(pname),
      repl_(repl) {
    if (repl_ != nullptr && !pname_.empty()) {
      named_repls_[pname_] = repl_;
    }
  }

  StateExprCloneVisitor(
    std::unordered_map<std::string, StyioAST*> named_repls,
    std::string receiver_family,
    StyioAST* receiver_repl
  ) :
      pname_(),
      repl_(nullptr),
      named_repls_(std::move(named_repls)),
      receiver_family_(std::move(receiver_family)),
      receiver_repl_(receiver_repl) {
  }

  StyioAST* clone(StyioAST* expr) {
    if (expr == nullptr) {
      return nullptr;
    }

    switch (expr->getNodeType()) {
      case StyioNodeType::Comment:
        return clone(static_cast<CommentAST*>(expr));
      case StyioNodeType::Empty:
        return clone(static_cast<EmptyAST*>(expr));
      case StyioNodeType::Id:
        return clone(static_cast<NameAST*>(expr));
      case StyioNodeType::Integer:
        return clone(static_cast<IntAST*>(expr));
      case StyioNodeType::Float:
        return clone(static_cast<FloatAST*>(expr));
      case StyioNodeType::Bool:
        return clone(static_cast<BoolAST*>(expr));
      case StyioNodeType::String:
        return clone(static_cast<StringAST*>(expr));
      case StyioNodeType::Char:
        return clone(static_cast<CharAST*>(expr));
      case StyioNodeType::Tuple:
        return clone(static_cast<TupleAST*>(expr));
      case StyioNodeType::List:
        return clone(static_cast<ListAST*>(expr));
      case StyioNodeType::Dict:
        return clone(static_cast<DictAST*>(expr));
      case StyioNodeType::Set:
        return clone(static_cast<SetAST*>(expr));
      case StyioNodeType::BinOp:
        return clone(static_cast<BinOpAST*>(expr));
      case StyioNodeType::Compare:
        return clone(static_cast<BinCompAST*>(expr));
      case StyioNodeType::Condition:
        return clone(static_cast<CondAST*>(expr));
      case StyioNodeType::WaveMerge:
        return clone(static_cast<WaveMergeAST*>(expr));
      case StyioNodeType::WaveDispatch:
        return clone(static_cast<WaveDispatchAST*>(expr));
      case StyioNodeType::CondFlow_True:
      case StyioNodeType::CondFlow_False:
      case StyioNodeType::CondFlow_Both:
        return clone(static_cast<CondFlowAST*>(expr));
      case StyioNodeType::Fallback:
        return clone(static_cast<FallbackAST*>(expr));
      case StyioNodeType::FmtStr:
        return clone(static_cast<FmtStrAST*>(expr));
      case StyioNodeType::GuardSelector:
        return clone(static_cast<GuardSelectorAST*>(expr));
      case StyioNodeType::EqProbeSelector:
        return clone(static_cast<EqProbeAST*>(expr));
      case StyioNodeType::Range:
        return clone(static_cast<RangeAST*>(expr));
      case StyioNodeType::SizeOf:
        return clone(static_cast<SizeOfAST*>(expr));
      case StyioNodeType::NumConvert:
        return clone(static_cast<TypeConvertAST*>(expr));
      case StyioNodeType::HandleAcquire:
        return clone(static_cast<HandleAcquireAST*>(expr));
      case StyioNodeType::Access:
      case StyioNodeType::Access_By_Index:
      case StyioNodeType::Access_By_Slice:
      case StyioNodeType::Access_By_Name:
      case StyioNodeType::Get_Index_By_Value:
      case StyioNodeType::Get_Indices_By_Many_Values:
      case StyioNodeType::Append_Value:
      case StyioNodeType::Insert_Item_By_Index:
      case StyioNodeType::Remove_Last_Item:
      case StyioNodeType::Remove_Item_By_Index:
      case StyioNodeType::Remove_Items_By_Many_Indices:
      case StyioNodeType::Remove_Item_By_Value:
      case StyioNodeType::Remove_Items_By_Many_Values:
      case StyioNodeType::Get_Reversed:
      case StyioNodeType::Get_Index_By_Item_From_Right:
        return clone(static_cast<ListOpAST*>(expr));
      case StyioNodeType::Call:
        return clone(static_cast<FuncCallAST*>(expr));
      case StyioNodeType::Attribute:
        return clone(static_cast<AttrAST*>(expr));
      case StyioNodeType::FileResource:
        return clone(static_cast<FileResourceAST*>(expr));
      case StyioNodeType::StdinResource:
      case StyioNodeType::StdoutResource:
      case StyioNodeType::StderrResource:
        return clone(static_cast<StdStreamAST*>(expr));
      case StyioNodeType::EmptyResource:
        return clone(static_cast<EmptyResourceAST*>(expr));
      case StyioNodeType::ResourceReceiver:
        return clone(static_cast<ResourceReceiverAST*>(expr));
      case StyioNodeType::ResourceRef:
        return clone(static_cast<ResourceRefAST*>(expr));
      case StyioNodeType::ResourceWrite:
        return clone(static_cast<ResourceWriteAST*>(expr));
      case StyioNodeType::ResourceRedirect:
        return clone(static_cast<ResourceRedirectAST*>(expr));
      case StyioNodeType::ResourceEffect:
        return clone(static_cast<ResourceEffectAST*>(expr));
      case StyioNodeType::InstantPull:
        return clone(static_cast<InstantPullAST*>(expr));
      case StyioNodeType::Variable:
        return clone(static_cast<VarAST*>(expr));
      case StyioNodeType::HistoryProbe:
        return clone(static_cast<HistoryProbeAST*>(expr));
      case StyioNodeType::StateRef:
        return clone(static_cast<StateRefAST*>(expr));
      case StyioNodeType::SeriesIntrinsic:
        return clone(static_cast<SeriesIntrinsicAST*>(expr));
      case StyioNodeType::Func:
        return clone(static_cast<FunctionAST*>(expr));
      case StyioNodeType::SimpleFunc:
        return clone(static_cast<SimpleFuncAST*>(expr));
      case StyioNodeType::Return:
        return clone(static_cast<ReturnAST*>(expr));
      case StyioNodeType::MutBind:
        return clone(static_cast<FlexBindAST*>(expr));
      case StyioNodeType::FinalBind:
        return clone(static_cast<FinalBindAST*>(expr));
      case StyioNodeType::ParallelAssign:
        return clone(static_cast<ParallelAssignAST*>(expr));
      case StyioNodeType::Print:
        return clone(static_cast<PrintAST*>(expr));
      case StyioNodeType::Iterator:
        return clone(static_cast<IteratorAST*>(expr));
      case StyioNodeType::StreamZip:
        return clone(static_cast<StreamZipAST*>(expr));
      case StyioNodeType::Cases:
        return clone(static_cast<CasesAST*>(expr));
      case StyioNodeType::MatchCases:
        return clone(static_cast<MatchCasesAST*>(expr));
      case StyioNodeType::Infinite:
        return clone(static_cast<InfiniteAST*>(expr));
      case StyioNodeType::Pass:
        return clone(static_cast<PassAST*>(expr));
      case StyioNodeType::Break:
        return clone(static_cast<BreakAST*>(expr));
      case StyioNodeType::Continue:
        return clone(static_cast<ContinueAST*>(expr));
      case StyioNodeType::Block:
        return clone(static_cast<BlockAST*>(expr));
      case StyioNodeType::UndefLiteral:
        return clone(static_cast<UndefinedLitAST*>(expr));
      default:
        break;
    }

    throw StyioTypeError(
      std::string("unsupported AST node in inlined state expression clone: ")
      + std::to_string(static_cast<int>(expr->getNodeType()))
    );
  }
};

StyioAST*
clone_state_expr_with_subst(StyioAST* e, const std::string& pname, StyioAST* repl) {
  return StateExprCloneVisitor(pname, repl).clone(e);
}

StyioAST*
subst_param_in_expr(StyioAST* e, const std::string& pname, StyioAST* repl) {
  return clone_state_expr_with_subst(e, pname, repl);
}

StyioAST*
clone_resource_method_body_latest(
  ResourceMethodDefAST* method,
  StyioAST* receiver,
  const std::vector<StyioAST*>& args
) {
  if (method == nullptr || method->getBody() == nullptr) {
    return nullptr;
  }
  std::unordered_map<std::string, StyioAST*> named_repls;
  const auto& params = method->getParams();
  const std::size_t n = std::min(params.size(), args.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (params[i] != nullptr) {
      named_repls[params[i]->getNameAsStr()] = args[i];
    }
  }
  return StateExprCloneVisitor(
           std::move(named_repls),
           method->getFamilyName(),
           receiver
  )
    .clone(method->getBody());
}

StyioIR*
flatten_single_stmt_block_latest(StyioIR* ir) {
  if (auto* block = dynamic_cast<SGBlock*>(ir)) {
    if (block->stmts.size() == 1) {
      return block->stmts.front();
    }
  }
  return ir;
}

bool
resource_method_scalar_value_type_supported_latest(const StyioDataType& type) {
  return type.option == StyioDataTypeOption::Bool
         || type.option == StyioDataTypeOption::Integer
         || type.option == StyioDataTypeOption::Float
         || type.option == StyioDataTypeOption::Char
         || type.option == StyioDataTypeOption::String;
}

bool
resource_method_local_container_type_supported_latest(const StyioDataType& type) {
  return styio_is_list_type(type)
         || styio_is_dict_type(type)
         || styio_is_matrix_type(type);
}

bool
resource_method_local_value_type_supported_latest(const StyioDataType& type) {
  return resource_method_scalar_value_type_supported_latest(type)
         || resource_method_local_container_type_supported_latest(type);
}

StyioDataType
resource_method_preface_bind_type_latest(AstToStyioIRLowerer* an, StyioAST* stmt) {
  VarAST* var = nullptr;
  StyioAST* value = nullptr;
  if (auto* bind = dynamic_cast<FlexBindAST*>(stmt)) {
    var = bind->getVar();
    value = bind->getValue();
  }
  else if (auto* bind = dynamic_cast<FinalBindAST*>(stmt)) {
    var = bind->getVar();
    value = bind->getValue();
  }
  if (var == nullptr || value == nullptr) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  StyioDataType type = var->getDType()->getDataType();
  if (type.isUndefined()) {
    type = expr_lowered_type(an, value);
  }
  return type;
}

StyioDataType
bind_slot_type_latest(AstToStyioIRLowerer* an, const std::string& name, VarAST* var) {
  const auto sid = var != nullptr && var->getName() != nullptr
                     ? var->getName()->getSymbolId()
                     : styio::session::kInvalidSymbolId;
  const StyioDataType* local_type =
    an->find_resource_method_dynamic_local_binding_type(sid, name);
  if (local_type != nullptr) {
    return *local_type;
  }
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

bool
resource_method_value_preface_supported_latest(AstToStyioIRLowerer* an, StyioAST* stmt) {
  if (stmt == nullptr) {
    return false;
  }
  if (dynamic_cast<CommentAST*>(stmt) != nullptr
      || dynamic_cast<EmptyAST*>(stmt) != nullptr
      || dynamic_cast<PassAST*>(stmt) != nullptr
      || dynamic_cast<PrintAST*>(stmt) != nullptr
      || dynamic_cast<ResourceWriteAST*>(stmt) != nullptr
      || dynamic_cast<ResourceRedirectAST*>(stmt) != nullptr
      || dynamic_cast<ResourceEffectAST*>(stmt) != nullptr) {
    return true;
  }
  if (auto* bind = dynamic_cast<FlexBindAST*>(stmt)) {
    return resource_method_local_value_type_supported_latest(
      resource_method_preface_bind_type_latest(an, bind)
    );
  }
  if (auto* bind = dynamic_cast<FinalBindAST*>(stmt)) {
    return resource_method_local_value_type_supported_latest(
      resource_method_preface_bind_type_latest(an, bind)
    );
  }
  return false;
}

void
bind_resource_method_preface_local_latest(AstToStyioIRLowerer* an, StyioAST* stmt) {
  VarAST* var = nullptr;
  if (auto* bind = dynamic_cast<FlexBindAST*>(stmt)) {
    var = bind->getVar();
  }
  else if (auto* bind = dynamic_cast<FinalBindAST*>(stmt)) {
    var = bind->getVar();
  }
  if (var == nullptr) {
    return;
  }
  StyioDataType type = resource_method_preface_bind_type_latest(an, stmt);
  if (type.isUndefined()) {
    return;
  }
  an->record_local_binding_type(var->getNameAsStr(), var->getName()->getSymbolId(), type);
  if (resource_method_local_container_type_supported_latest(type)) {
    an->record_resource_method_dynamic_local_binding_type(
      var->getNameAsStr(),
      var->getName()->getSymbolId(),
      type
    );
  }
}

StyioIR*
lower_resource_method_value_body_latest(AstToStyioIRLowerer* an, StyioAST* body) {
  if (auto* ret = dynamic_cast<ReturnAST*>(body)) {
    return ret->getExpr() != nullptr ? ret->getExpr()->toStyioIR(an) : nullptr;
  }
  auto* block = dynamic_cast<BlockAST*>(body);
  if (block == nullptr || !block->followings.empty() || block->stmts.empty()) {
    return nullptr;
  }
  auto* tail = dynamic_cast<ReturnAST*>(block->stmts.back());
  if (tail == nullptr || tail->getExpr() == nullptr) {
    return nullptr;
  }
  if (block->stmts.size() == 1) {
    return tail->getExpr()->toStyioIR(an);
  }

  auto saved_local_types = an->local_binding_types;
  auto saved_local_types_by_sid = an->local_binding_types_by_sid;
  auto saved_dynamic_local_types = an->resource_method_dynamic_local_binding_types;
  auto saved_dynamic_local_types_by_sid =
    an->resource_method_dynamic_local_binding_types_by_sid;
  auto restore_local_types = [&]()
  {
    an->local_binding_types = saved_local_types;
    an->local_binding_types_by_sid = saved_local_types_by_sid;
    an->resource_method_dynamic_local_binding_types = saved_dynamic_local_types;
    an->resource_method_dynamic_local_binding_types_by_sid =
      saved_dynamic_local_types_by_sid;
  };
  std::vector<StyioIR*> stmts;
  stmts.reserve(block->stmts.size());
  try {
    for (std::size_t i = 0; i + 1 < block->stmts.size(); ++i) {
      if (!resource_method_value_preface_supported_latest(an, block->stmts[i])) {
        restore_local_types();
        return nullptr;
      }
      bind_resource_method_preface_local_latest(an, block->stmts[i]);
      stmts.push_back(block->stmts[i]->toStyioIR(an));
    }
    stmts.push_back(tail->getExpr()->toStyioIR(an));
  }
  catch (...) {
    restore_local_types();
    throw;
  }
  restore_local_types();
  return styio::lowering::require_default_styio_ir_pass_pipeline(
    SGBlock::Create(std::move(stmts)),
    intermediate_sg_block_pipeline_options());
}

struct PulseScratch
{
  std::vector<std::unique_ptr<StateDeclAST>> heap_decls;
};

StateDeclAST*
resolve_state_decl_impl(AstToStyioIRLowerer* an, StyioAST* stmt, PulseScratch* scratch) {
  if (auto* sd = dynamic_cast<StateDeclAST*>(stmt)) {
    return sd;
  }
  auto* fc = dynamic_cast<FuncCallAST*>(stmt);
  if (!fc) {
    return nullptr;
  }
  StyioAST* def = an->find_function_def(
    fc->func_name->getSymbolId(),
    fc->getNameAsStr()
  );
  if (def == nullptr) {
    throw StyioTypeError("unknown function in pulse body");
  }

  const std::vector<ParamAST*>* params = nullptr;
  StyioAST* body = nullptr;

  if (auto* sf = dynamic_cast<SimpleFuncAST*>(def)) {
    params = &sf->params;
    body = sf->ret_expr;
  }
  else if (auto* fn = dynamic_cast<FunctionAST*>(def)) {
    params = &fn->params;
    body = fn->func_body;
  }
  else {
    throw StyioTypeError("only single-arg function->state inlining supported");
  }

  if (params == nullptr || params->size() != 1 || fc->getArgList().size() != 1) {
    throw StyioTypeError("only single-arg function->state inlining supported");
  }

  StateDeclAST* sd = nullptr;
  if (!body_returns_single_state_decl(body, sd)) {
    throw StyioTypeError("inlined state func must return a single state declaration");
  }
  if (!sd) {
    throw StyioTypeError("inlined func body must be a state decl");
  }
  const std::string& pn = (*params)[0]->getName();
  StyioAST* rep = fc->getArgList()[0];
  StyioAST* new_rhs = subst_param_in_expr(sd->getUpdateExpr(), pn, rep);
  auto* created = StateDeclAST::Create(
    clone_int_ast(sd->getWindowHeader()),
    clone_name_ast(sd->getAccName()),
    clone_state_expr(sd->getAccInit()),
    clone_var_ast(sd->getExportVar()),
    new_rhs
  );
  scratch->heap_decls.emplace_back(created);
  return created;
}

StateDeclAST*
resolve_state_decl_cached(
  AstToStyioIRLowerer* an,
  StyioAST* stmt,
  PulseScratch* scratch,
  std::unordered_map<StyioAST*, StateDeclAST*>& cache
) {
  auto itc = cache.find(stmt);
  if (itc != cache.end()) {
    return itc->second;
  }
  StateDeclAST* sd = resolve_state_decl_impl(an, stmt, scratch);
  cache[stmt] = sd;
  return sd;
}

std::unique_ptr<SGPulsePlan>
build_pulse_plan(
  AstToStyioIRLowerer* an,
  BlockAST* blk,
  PulseScratch* scratch,
  std::unordered_map<StyioAST*, StateDeclAST*>& cache
) {
  auto plan = std::make_unique<SGPulsePlan>();
  int off = 0;
  int id = 0;
  for (auto* stmt : blk->stmts) {
    StateDeclAST* sd = resolve_state_decl_cached(an, stmt, scratch, cache);
    if (!sd) {
      continue;
    }
    SGStateSlotDesc d{};
    d.id = id;
    classify_state_slot(an, sd, d);
    d.offset = off;
    d.size = slot_byte_size(d);
    off += d.size;
    d.acc_name = sd->getAccName() ? sd->getAccName()->getAsStr() : "";
    d.export_name = sd->getExportVar()->getNameAsStr();
    plan->slots.push_back(d);
    plan->commits.push_back({id, d.export_name});
    if (not d.acc_name.empty()) {
      plan->ref_to_slot[d.acc_name] = id;
    }
    plan->ref_to_slot[d.export_name] = id;
    id += 1;
  }
  plan->total_bytes = off;
  return plan;
}

StyioIR*
lower_state_rhs(AstToStyioIRLowerer* an, StyioAST* rhs, int slot_id) {
  if (auto* fc = dynamic_cast<FuncCallAST*>(rhs)) {
    if (auto* body = series_intrinsic_helper_body(an, fc)) {
      an->set_active_series_slot(slot_id);
      StyioIR* xi = fc->getArgList()[0]->toStyioIR(an);
      an->set_active_series_slot(-1);
      if (body->getOp() == SeriesIntrinsicOp::Avg) {
        return SGSeriesAvgStep::Create(slot_id, xi);
      }
      return SGSeriesMaxStep::Create(slot_id, xi);
    }
  }
  an->set_active_series_slot(slot_id);
  StyioIR* r = rhs->toStyioIR(an);
  an->set_active_series_slot(-1);
  return r;
}

SGFlexBind*
lower_state_decl_to_flexbind(AstToStyioIRLowerer* an, StateDeclAST* sd, SGPulsePlan* plan) {
  int sid = -1;
  for (size_t i = 0; i < plan->slots.size(); ++i) {
    if (plan->slots[i].export_name == sd->getExportVar()->getNameAsStr()) {
      sid = static_cast<int>(i);
      break;
    }
  }
  if (sid < 0) {
    throw StyioTypeError("state slot not in plan");
  }
  StyioIR* rhs = lower_state_rhs(an, sd->getUpdateExpr(), sid);
  return SGFlexBind::Create(
    static_cast<SGVar*>(sd->getExportVar()->toStyioIR(an)),
    rhs
  );
}

SGBlock*
lower_pulse_body(
  AstToStyioIRLowerer* an,
  BlockAST* blk,
  SGPulsePlan* plan,
  PulseScratch* scratch,
  std::unordered_map<StyioAST*, StateDeclAST*>& cache
) {
  an->set_cur_pulse_plan(plan);
  std::vector<StyioIR*> stmts;
  for (auto* s : blk->stmts) {
    StateDeclAST* sd = resolve_state_decl_cached(an, s, scratch, cache);
    if (sd) {
      stmts.push_back(lower_state_decl_to_flexbind(an, sd, plan));
    }
    else {
      stmts.push_back(s->toStyioIR(an));
    }
  }
  an->set_cur_pulse_plan(nullptr);
  return SGBlock::Create(std::move(stmts));
}

}  // namespace

static StyioOpType
comp_type_to_op(CompType ct) {
  switch (ct) {
    case CompType::EQ:
      return StyioOpType::Equal;
    case CompType::NE:
      return StyioOpType::Not_Equal;
    case CompType::GT:
      return StyioOpType::Greater_Than;
    case CompType::GE:
      return StyioOpType::Greater_Than_Equal;
    case CompType::LT:
      return StyioOpType::Less_Than;
    case CompType::LE:
      return StyioOpType::Less_Than_Equal;
    default:
      throw StyioTypeError("unsupported comparison operator in lowering");
  }
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(CommentAST* ast) {
  (void)ast;
  return SGNoOp::Create();
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(NoneAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("NoneAST", "none/null value semantics are not defined in StyioIR");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(EmptyAST* ast) {
  (void)ast;
  return SGNoOp::Create();
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(NameAST* ast) {
  if (!ast->getLoweredCallableName().empty()) {
    return SGResId::CreateFunctionRef(
      ast->getLoweredCallableName());
  }
  const BindingInfo* binding = find_binding_info(ast->getSymbolId(), ast->getAsStr());
  if (binding != nullptr
      && (binding->dynamic_slot || binding->value_kind == BindingValueKind::ListHandle || binding->value_kind == BindingValueKind::DictHandle || binding->value_kind == BindingValueKind::MatrixHandle || binding->value_kind == BindingValueKind::TaskHandle)) {
    switch (binding->value_kind) {
      case BindingValueKind::Bool:
        return SGDynLoad::Create(ast->getAsStr(), SGDynLoadKind::Bool);
      case BindingValueKind::I64:
        return SGDynLoad::Create(ast->getAsStr(), SGDynLoadKind::I64);
      case BindingValueKind::F64:
        return SGDynLoad::Create(ast->getAsStr(), SGDynLoadKind::F64);
      case BindingValueKind::String:
        return SGDynLoad::Create(ast->getAsStr(), SGDynLoadKind::CString);
      case BindingValueKind::ListHandle:
        return SGDynLoad::Create(ast->getAsStr(), SGDynLoadKind::ListHandle);
      case BindingValueKind::DictHandle:
        return SGDynLoad::Create(ast->getAsStr(), SGDynLoadKind::DictHandle);
      case BindingValueKind::MatrixHandle:
        return SGDynLoad::Create(ast->getAsStr(), SGDynLoadKind::MatrixHandle);
      case BindingValueKind::TaskHandle:
        return SGDynLoad::Create(ast->getAsStr(), SGDynLoadKind::TaskHandle);
      default:
        throw StyioTypeError("cannot lower dynamic slot `" + ast->getAsStr() + "` with unknown runtime kind");
    }
  }
  const StyioDataType* local_type =
    find_resource_method_dynamic_local_binding_type(ast->getSymbolId(), ast->getAsStr());
  if (local_type != nullptr) {
    if (styio_is_list_type(*local_type)) {
      return SGDynLoad::Create(ast->getAsStr(), SGDynLoadKind::ListHandle);
    }
    if (styio_is_dict_type(*local_type)) {
      return SGDynLoad::Create(ast->getAsStr(), SGDynLoadKind::DictHandle);
    }
    if (styio_is_matrix_type(*local_type)) {
      return SGDynLoad::Create(ast->getAsStr(), SGDynLoadKind::MatrixHandle);
    }
  }
  return SGResId::Create(ast->getAsStr());
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(TypeAST* ast) {
  return SGType::Create(ast->type);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(TypeTupleAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("TypeTupleAST", "type tuples are declaration metadata, not runtime values");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(BoolAST* ast) {
  return SGConstBool::Create(ast->getValue());
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(IntAST* ast) {
  return SGConstInt::Create(ast->value, ast->num_of_bit);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(FloatAST* ast) {
  return SGConstFloat::Create(ast->value);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(CharAST* ast) {
  const std::string& value = ast->getValue();
  if (value.size() != 1) {
    throw StyioTypeError("char literal lowering expects exactly one byte");
  }
  return SGConstChar::Create(value.front());
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(StringAST* ast) {
  const std::string& raw = ast->getValue();
  std::string inner = raw;
  if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
    inner = raw.substr(1, raw.size() - 2);
  }
  return SGConstString::Create(inner);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(TypeConvertAST* ast) {
  StyioDataType from_type = expr_lowered_type(this, ast->getValue());
  if (from_type.isUndefined()) {
    from_type = lowering_type_convert_source_fallback_type(ast->getPromoTy());
  }
  StyioDataType to_type = ast->getDataType();
  if (to_type.isUndefined()) {
    to_type = lowering_type_convert_target_type(ast->getPromoTy());
  }
  return SGCast::Create(
    ast->getValue()->toStyioIR(this),
    SGType::Create(from_type),
    SGType::Create(to_type)
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(VarAST* ast) {
  if (ast->val_init) {
    return SGVar::Create(
      SGResId::Create(ast->var_name->getAsStr()),
      static_cast<SGType*>(ast->var_type->toStyioIR(this)),
      ast->val_init->toStyioIR(this)
    );
  }
  else {
    return SGVar::Create(
      SGResId::Create(ast->var_name->getAsStr()),
      static_cast<SGType*>(ast->var_type->toStyioIR(this))
    );
  }
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ParamAST* ast) {
  if (ast->val_init) {
    return SGVar::Create(
      SGResId::Create(ast->var_name->getAsStr()),
      static_cast<SGType*>(ast->var_type->toStyioIR(this)),
      ast->val_init->toStyioIR(this)
    );
  }
  else {
    return SGVar::Create(
      SGResId::Create(ast->var_name->getAsStr()),
      static_cast<SGType*>(ast->var_type->toStyioIR(this))
    );
  }
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(OptArgAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("OptArgAST", "optional positional arguments are declaration-only syntax");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(OptKwArgAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("OptKwArgAST", "optional keyword arguments are declaration-only syntax");
}

/*
  The declared type is always the *top* priority
  because the programmer wrote in that way!
*/
StyioIR*
AstToStyioIRLowerer::toStyioIR(FlexBindAST* ast) {
  if (auto* fr = dynamic_cast<FileResourceAST*>(ast->getValue())) {
    file_resource_bindings_[ast->getNameAsStr()] = fr;
    return SIOHandleAcquire::Create(
      ast->getNameAsStr(),
      fr->getPath()->toStyioIR(this),
      fr->isAutoDetect()
    );
  }
  if (dynamic_cast<StdStreamAST*>(ast->getValue()) != nullptr) {
    return SGNoOp::Create();
  }
  auto* var = static_cast<SGVar*>(ast->getVar()->toStyioIR(this));
  const BindingInfo* binding = find_binding_info(
    ast->getVar()->getName()->getSymbolId(),
    ast->getNameAsStr()
  );
  if (binding != nullptr) {
    var->is_dynamic_slot = binding->dynamic_slot
                           || binding->value_kind == BindingValueKind::ListHandle
                           || binding->value_kind == BindingValueKind::DictHandle
                           || binding->value_kind == BindingValueKind::MatrixHandle
                           || binding->value_kind == BindingValueKind::TaskHandle;
    var->is_list_slot = !binding->dynamic_slot
                        && binding->value_kind == BindingValueKind::ListHandle;
  }
  StyioDataType var_type = bind_slot_type_latest(this, ast->getNameAsStr(), ast->getVar());
  if (styio_is_list_type(var_type)
      || styio_is_dict_type(var_type)
      || styio_is_matrix_type(var_type)) {
    var->is_dynamic_slot = true;
  }
  return SGFlexBind::Create(var, ast->getValue()->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(FinalBindAST* ast) {
  if (auto* fr = dynamic_cast<FileResourceAST*>(ast->getValue())) {
    file_resource_bindings_[ast->getName()] = fr;
    return SIOHandleAcquire::Create(
      ast->getName(),
      fr->getPath()->toStyioIR(this),
      fr->isAutoDetect()
    );
  }
  if (dynamic_cast<StdStreamAST*>(ast->getValue()) != nullptr) {
    return SGNoOp::Create();
  }
  auto* var = static_cast<SGVar*>(ast->getVar()->toStyioIR(this));
  const BindingInfo* binding = find_binding_info(
    ast->getVar()->getName()->getSymbolId(),
    ast->getVar()->getNameAsStr()
  );
  if (binding != nullptr) {
    var->is_dynamic_slot = binding->dynamic_slot
                           || binding->value_kind == BindingValueKind::ListHandle
                           || binding->value_kind == BindingValueKind::DictHandle
                           || binding->value_kind == BindingValueKind::MatrixHandle
                           || binding->value_kind == BindingValueKind::TaskHandle;
    var->is_list_slot = !binding->dynamic_slot
                        && binding->value_kind == BindingValueKind::ListHandle;
  }
  StyioDataType var_type = bind_slot_type_latest(this, ast->getName(), ast->getVar());
  if (styio_is_list_type(var_type)
      || styio_is_dict_type(var_type)
      || styio_is_matrix_type(var_type)) {
    var->is_dynamic_slot = true;
  }
  return SGFinalBind::Create(var, ast->getValue()->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ParallelAssignAST* ast) {
  std::vector<StyioIR*> stmts;
  std::vector<std::string> tmp_names;
  tmp_names.reserve(ast->getRHS().size());

  for (auto* rhs : ast->getRHS()) {
    std::string tmp_name = alloc_lowering_tmp_name("__styio_parallel_tmp_");
    tmp_names.push_back(tmp_name);
    std::unique_ptr<VarAST> tmp_var(VarAST::Create(NameAST::Create(tmp_name)));
    auto* sg_var = static_cast<SGVar*>(tmp_var->toStyioIR(this));
    stmts.push_back(SGFinalBind::Create(sg_var, rhs->toStyioIR(this)));
  }

  for (size_t i = 0; i < ast->getLHS().size(); ++i) {
    StyioIR* rhs_val = SGResId::Create(tmp_names[i]);
    if (auto* nm = dynamic_cast<NameAST*>(ast->getLHS()[i])) {
      std::unique_ptr<VarAST> lhs_var(VarAST::Create(NameAST::Clone(nm)));
      auto* sg_var = static_cast<SGVar*>(lhs_var->toStyioIR(this));
      const BindingInfo* binding = find_binding_info(nm->getSymbolId(), nm->getAsStr());
      if (binding != nullptr) {
        sg_var->is_dynamic_slot = binding->dynamic_slot
                                  || binding->value_kind == BindingValueKind::ListHandle
                                  || binding->value_kind == BindingValueKind::DictHandle
                                  || binding->value_kind == BindingValueKind::MatrixHandle
                                  || binding->value_kind == BindingValueKind::TaskHandle;
        sg_var->is_list_slot = !binding->dynamic_slot
                               && binding->value_kind == BindingValueKind::ListHandle;
      }
      stmts.push_back(SGFlexBind::Create(sg_var, rhs_val));
      continue;
    }

    auto* idx = static_cast<ListOpAST*>(ast->getLHS()[i]);
    StyioDataType base_type = idx->getList()->getDataType();
    if (auto bound = bound_type_of(this, idx->getList())) {
      base_type = *bound;
    }
    if (styio_is_dict_type(base_type)) {
      stmts.push_back(SCDictSet::Create(
        idx->getList()->toStyioIR(this),
        idx->getSlot1()->toStyioIR(this),
        rhs_val,
        styio_dict_value_type_name(base_type)
      ));
    }
    else {
      stmts.push_back(SCListSet::Create(
        idx->getList()->toStyioIR(this),
        idx->getSlot1()->toStyioIR(this),
        rhs_val,
        styio_type_item_type_name(base_type)
      ));
    }
  }

  return SGBlock::Create(std::move(stmts));
}

// MIGRATION-NEEDED: M-SEMA-01 (docs/rollups/MIGRATION-LEDGER.md)
// Either restore lowering for InfiniteAST or delete the AST node entirely.
StyioIR*
AstToStyioIRLowerer::toStyioIR(InfiniteAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("InfiniteAST", "legacy infinite sequence syntax has no active StyioIR lowering");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(StructAST* ast) {
  std::vector<SGVar*> elems;

  for (auto arg : ast->args) {
    elems.push_back(static_cast<SGVar*>(arg->toStyioIR(this)));
  }

  return SGStruct::Create(SGResId::Create(ast->name->getAsStr()), elems);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(TupleAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("TupleAST", "tuple value IR is not implemented");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(VarTupleAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("VarTupleAST", "tuple parameter groups are declaration-only syntax");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ExtractorAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("ExtractorAST", "tuple extractor IR is not implemented");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(RangeAST* ast) {
  std::int64_t start = 0;
  std::int64_t end = 0;
  std::int64_t step = 0;

  const bool constant_range =
    try_parse_int_literal_value(ast->getStart(), start)
    && try_parse_int_literal_value(ast->getEnd(), end)
    && try_parse_int_literal_value(ast->getStep(), step);

  if (!constant_range) {
    return SGCall::Create(
      SGResId::Create("__styio_list_range_i64"),
      {ast->getStart()->toStyioIR(this), ast->getEnd()->toStyioIR(this), ast->getStep()->toStyioIR(this)}
    );
  }

  if (step == 0) {
    throw StyioTypeError("range step cannot be 0");
  }

  std::vector<StyioIR*> el;

  if (step > 0) {
    for (std::int64_t cur = start; cur <= end;) {
      el.push_back(SGConstInt::Create(std::to_string(cur)));
      if (cur == end) {
        break;
      }
      if (cur > std::numeric_limits<std::int64_t>::max() - step) {
        throw StyioTypeError("range literal overflow");
      }
      cur += step;
    }
  }
  else {
    for (std::int64_t cur = start; cur >= end;) {
      el.push_back(SGConstInt::Create(std::to_string(cur)));
      if (cur == end) {
        break;
      }
      if (cur < std::numeric_limits<std::int64_t>::min() - step) {
        throw StyioTypeError("range literal overflow");
      }
      cur += step;
    }
  }

  return SCListLiteral::Create(std::move(el), "i64");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(SetAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("SetAST", "set literal IR is not implemented");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ListAST* ast) {
  StyioDataType list_type = expr_lowered_type(this, ast);
  if (styio_is_matrix_type(list_type)) {
    std::vector<StyioIR*> flat;
    for (auto* row_expr : ast->getElements()) {
      auto* row = dynamic_cast<ListAST*>(row_expr);
      if (row == nullptr) {
        throw StyioTypeError("matrix literal rows must be list literals");
      }
      for (auto* cell : row->getElements()) {
        flat.push_back(cell->toStyioIR(this));
      }
    }
    return SCMatrixLiteral::Create(
      std::move(flat),
      styio_matrix_elem_type_name(list_type),
      styio_matrix_row_count(list_type),
      styio_matrix_col_count(list_type)
    );
  }
  std::vector<StyioIR*> el;
  for (auto* e : ast->getElements()) {
    el.push_back(e->toStyioIR(this));
  }
  return SCListLiteral::Create(std::move(el), styio_type_item_type_name(list_type));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(DictAST* ast) {
  std::vector<SCDictLiteral::Entry> entries;
  for (auto const& entry : ast->getEntries()) {
    entries.push_back(SCDictLiteral::Entry{
      entry.key->toStyioIR(this),
      entry.value->toStyioIR(this)
    });
  }
  StyioDataType dict_type = expr_lowered_type(this, ast);
  return SCDictLiteral::Create(
    std::move(entries),
    styio_dict_value_type_name(dict_type)
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(SizeOfAST* ast) {
  if (ast == nullptr || ast->getValue() == nullptr) {
    throw StyioTypeError("size-of expects an expression");
  }

  StyioDataType value_type = expr_lowered_type(this, ast->getValue());
  StyioIR* value_ir = ast->getValue()->toStyioIR(this);
  if (styio_is_dict_type(value_type)) {
    return SCDictLen::Create(value_ir);
  }
  if (styio_is_list_type(value_type)) {
    return SCListLen::Create(value_ir);
  }

  throw StyioTypeError("size-of expects a list or dict value");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ListOpAST* ast) {
  if (ast->getOp() == StyioNodeType::Access_By_Slice) {
    StyioDataType base_type = expr_lowered_type(this, ast->getList());
    if (styio_is_matrix_type(base_type)) {
      return SCMatrixRowsSlice::Create(
        ast->getList()->toStyioIR(this),
        ast->getSlot1()->toStyioIR(this),
        ast->getSlot2() != nullptr ? ast->getSlot2()->toStyioIR(this) : nullptr,
        styio_matrix_elem_type_name(base_type)
      );
    }
    if (styio_is_dict_type(base_type)) {
      const std::string value_type = styio_dict_value_type_name(base_type);
      return SCListSlice::Create(
        SCDictValues::Create(ast->getList()->toStyioIR(this), value_type),
        ast->getSlot1()->toStyioIR(this),
        ast->getSlot2() != nullptr ? ast->getSlot2()->toStyioIR(this) : nullptr,
        value_type
      );
    }
    if (!styio_is_list_type(base_type)) {
      throw StyioTypeError("list slice lowering requires a list value");
    }
    return SCListSlice::Create(
      ast->getList()->toStyioIR(this),
      ast->getSlot1()->toStyioIR(this),
      ast->getSlot2() != nullptr ? ast->getSlot2()->toStyioIR(this) : nullptr,
      styio_type_item_type_name(base_type)
    );
  }
  if (ast->getOp() == StyioNodeType::Access_By_Index) {
    if (auto* row_access = dynamic_cast<ListOpAST*>(ast->getList())) {
      if (row_access->getOp() == StyioNodeType::Access_By_Index) {
        StyioDataType matrix_type = expr_lowered_type(this, row_access->getList());
        if (styio_is_matrix_type(matrix_type)) {
          return SCMatrixGet::Create(
            row_access->getList()->toStyioIR(this),
            row_access->getSlot1()->toStyioIR(this),
            ast->getSlot1()->toStyioIR(this),
            styio_matrix_elem_type_name(matrix_type)
          );
        }
      }
    }
  }
  StyioDataType base_type = expr_lowered_type(this, ast->getList());
  if (styio_is_matrix_type(base_type) && ast->getOp() == StyioNodeType::Access_By_Index) {
    return SCMatrixRow::Create(
      ast->getList()->toStyioIR(this),
      ast->getSlot1()->toStyioIR(this),
      styio_matrix_elem_type_name(base_type)
    );
  }
  if (styio_is_dict_type(base_type)
      && (ast->getOp() == StyioNodeType::Access_By_Index || ast->getOp() == StyioNodeType::Access_By_Name)) {
    return SCDictGet::Create(
      ast->getList()->toStyioIR(this),
      ast->getSlot1()->toStyioIR(this),
      styio_dict_value_type_name(base_type)
    );
  }
  if (ast->getOp() == StyioNodeType::Access_By_Index) {
    return SCListGet::Create(
      ast->getList()->toStyioIR(this),
      ast->getSlot1()->toStyioIR(this),
      styio_type_item_type_name(base_type)
    );
  }
  throw StyioTypeError("unsupported list operation in lowering");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(BinCompAST* ast) {
  StyioOpType op = comp_type_to_op(ast->getSign());
  return SGBinOp::Create(
    ast->getLHS()->toStyioIR(this),
    ast->getRHS()->toStyioIR(this),
    op,
    SGType::Create(StyioDataType{StyioDataTypeOption::Bool, "bool", 1}),
    expr_lowered_type(this, ast->getLHS()),
    expr_lowered_type(this, ast->getRHS())
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(CondAST* ast) {
  switch (ast->getSign()) {
    case LogicType::NOT:
      return SGCond::Create(
        ast->getValue()->toStyioIR(this),
        SGConstBool::Create(false),
        StyioOpType::Logic_NOT
      );
    case LogicType::AND:
      return SGCond::Create(
        ast->getLHS()->toStyioIR(this),
        ast->getRHS()->toStyioIR(this),
        StyioOpType::Logic_AND
      );
    case LogicType::OR:
      return SGCond::Create(
        ast->getLHS()->toStyioIR(this),
        ast->getRHS()->toStyioIR(this),
        StyioOpType::Logic_OR
      );
    case LogicType::XOR:
      return SGCond::Create(
        ast->getLHS()->toStyioIR(this),
        ast->getRHS()->toStyioIR(this),
        StyioOpType::Logic_XOR
      );
    case LogicType::RAW:
      return ast->getValue()->toStyioIR(this);
    default:
      throw StyioTypeError("unsupported logical condition operator in lowering");
  }
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(UndefinedLitAST* ast) {
  (void)ast;
  return SGUndef::Create();
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(WaveMergeAST* ast) {
  return SGWaveMerge::Create(
    ast->getCond()->toStyioIR(this),
    ast->getTrueVal()->toStyioIR(this),
    ast->getFalseVal()->toStyioIR(this)
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(WaveDispatchAST* ast) {
  return SGWaveDispatch::Create(
    ast->getCond()->toStyioIR(this),
    ast->getTrueArm()->toStyioIR(this),
    ast->getFalseArm()->toStyioIR(this)
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(FallbackAST* ast) {
  return SGFallback::Create(
    ast->getPrimary()->toStyioIR(this),
    ast->getAlternate()->toStyioIR(this)
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(GuardSelectorAST* ast) {
  return SGGuardSelect::Create(
    ast->getBase()->toStyioIR(this),
    ast->getCond()->toStyioIR(this)
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(EqProbeAST* ast) {
  return SGEqProbe::Create(
    ast->getBase()->toStyioIR(this),
    ast->getProbeValue()->toStyioIR(this)
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(FileResourceAST* ast) {
  return ast->getPath()->toStyioIR(this);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(StdStreamAST* ast) {
  (void)ast;
  /* StdStreamAST is not lowered to its own IR node;
     it is consumed by the parent node (ResourceWriteAST, IteratorAST, etc.). */
  return unsupported_ast_lowering("StdStreamAST", "standard streams must be consumed by a parent resource operation");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(HandleAcquireAST* ast) {
  auto binding_value_kind_for_type_latest = [](const StyioDataType& type) {
    if (styio_is_list_type(type)) {
      return BindingValueKind::ListHandle;
    }
    if (styio_is_dict_type(type)) {
      return BindingValueKind::DictHandle;
    }
    if (styio_is_matrix_type(type)) {
      return BindingValueKind::MatrixHandle;
    }
    if (type.handle_family == StyioHandleFamily::Task) {
      return BindingValueKind::TaskHandle;
    }
    if (type.option == StyioDataTypeOption::String) {
      return BindingValueKind::String;
    }
    if (type.option == StyioDataTypeOption::Float) {
      return BindingValueKind::F64;
    }
    if (type.option == StyioDataTypeOption::Bool) {
      return BindingValueKind::Bool;
    }
    if (type.option == StyioDataTypeOption::Integer) {
      return BindingValueKind::I64;
    }
    return BindingValueKind::Unknown;
  };
  auto register_bound_name = [&](const std::string& name,
                                 const StyioDataType& type,
                                 bool resource_value,
                                 BindingValueKind kind) {
    if (type.isUndefined()) {
      return;
    }
    const auto sid = ast->getVar()->getName()->getSymbolId();
    record_local_binding_type(name, sid, type);
    BindingInfo info;
    info.final_slot = !ast->isFlexBind();
    info.dynamic_slot = ast->isFlexBind();
    info.resource_value = resource_value;
    info.value_kind = kind;
    info.declared_type = type;
    record_binding_info(name, sid, info);
  };
  const std::string target_name = ast->getVar()->getNameAsStr();
  const auto target_sid = ast->getVar()->getName()->getSymbolId();
  if (auto* task_name = dynamic_cast<NameAST*>(ast->getResource())) {
    auto source_type = bound_type_of(this, task_name);
    if (source_type.has_value() && source_type->handle_family == StyioHandleFamily::Task) {
      StyioDataType result_type = styio_data_type_from_name(styio_task_result_type_name(*source_type));
      if (result_type.name == "unit") {
        result_type = StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
      }
      register_bound_name(
        target_name,
        result_type,
        false,
        binding_value_kind_for_type_latest(result_type)
      );
      return SIOFlowBind::Create(
        task_name->toStyioIR(this),
        target_name,
        result_type,
        true
      );
    }
  }
  if (collect_bind_handle_acquires_.count(ast) != 0) {
    StyioDataType collected_type = styio_make_list_type("string");
    auto type_it = collect_bind_handle_acquire_types_.find(ast);
    if (type_it != collect_bind_handle_acquire_types_.end()) {
      collected_type = type_it->second;
    }
    register_bound_name(
      target_name,
      collected_type,
      true,
      BindingValueKind::ListHandle
    );
    auto* var = SGVar::Create(
      SGResId::Create(target_name),
      SGType::Create(collected_type)
    );
    const BindingInfo* binding = find_binding_info(target_sid, target_name);
    if (binding != nullptr) {
      var->is_dynamic_slot = binding->dynamic_slot
                             || binding->value_kind == BindingValueKind::ListHandle;
      var->is_list_slot = !binding->dynamic_slot
                          && binding->value_kind == BindingValueKind::ListHandle;
    }
    return SGFlexBind::Create(
      var,
      SIOListReadStdin::Create(styio_type_item_type_name(collected_type))
    );
  }
  if (dynamic_cast<NameAST*>(ast->getResource())) {
    auto* var = static_cast<SGVar*>(ast->getVar()->toStyioIR(this));
    const BindingInfo* binding = find_binding_info(target_sid, target_name);
    if (binding != nullptr) {
      var->is_dynamic_slot = binding->dynamic_slot
                             || binding->value_kind == BindingValueKind::ListHandle
                             || binding->value_kind == BindingValueKind::DictHandle
                             || binding->value_kind == BindingValueKind::MatrixHandle;
      var->is_list_slot = !binding->dynamic_slot
                          && binding->value_kind == BindingValueKind::ListHandle;
    }

    StyioIR* rhs = nullptr;
    auto src_type = bound_type_of(this, ast->getResource());
    if (src_type.has_value()
        && (src_type->handle_family == StyioHandleFamily::File
            || src_type->handle_family == StyioHandleFamily::Stream)) {
      throw StyioTypeError(
        "resource clone source has no implemented `<<` clone lowering for file/stream handles"
      );
    }
    if (src_type.has_value() && styio_is_topology_resource_type(*src_type)) {
      throw StyioTypeError(
        "resource clone source has no implemented `<<` clone lowering for topology resources"
      );
    }
    if (src_type.has_value() && styio_is_dict_type(*src_type)) {
      rhs = SCDictClone::Create(ast->getResource()->toStyioIR(this));
    }
    else if (src_type.has_value() && styio_is_list_type(*src_type)) {
      rhs = SCListClone::Create(ast->getResource()->toStyioIR(this));
    }
    else if (src_type.has_value() && styio_is_matrix_type(*src_type)) {
      rhs = SCMatrixClone::Create(
        ast->getResource()->toStyioIR(this),
        styio_matrix_elem_type_name(*src_type)
      );
    }
    else {
      throw StyioTypeError("resource clone source has no implemented `<<` clone lowering");
    }

    if (src_type.has_value()) {
      register_bound_name(
        target_name,
        *src_type,
        binding != nullptr ? binding->resource_value : styio_type_is_resource_handle(*src_type),
        binding != nullptr ? binding->value_kind : binding_value_kind_for_type_latest(*src_type)
      );
    }

    if (ast->isFlexBind()) {
      return SGFlexBind::Create(var, rhs);
    }
    return SGFinalBind::Create(var, rhs);
  }

  if (dynamic_cast<StdStreamAST*>(ast->getResource())) {
    /* Standard stream aliases are compile-time handles; lowering happens at the use site. */
    register_bound_name(
      target_name,
      expr_lowered_type(this, ast->getResource()),
      true,
      BindingValueKind::Unknown
    );
    return SGNoOp::Create();
  }
  auto* fr = dynamic_cast<FileResourceAST*>(ast->getResource());
  if (!fr) {
    throw StyioTypeError("handle acquire needs @file(...) or @{...}");
  }
  StyioDataType file_type = ast->getVar()->getDType()->getDataType();
  if (file_type.isUndefined()) {
    file_type = expr_lowered_type(this, ast->getResource());
  }
  register_bound_name(
    target_name,
    file_type,
    true,
    binding_value_kind_for_type_latest(file_type)
  );
  file_resource_bindings_[target_name] = fr;
  return SIOHandleAcquire::Create(
    target_name,
    fr->getPath()->toStyioIR(this),
    fr->isAutoDetect()
  );
}

static StyioDataType
resource_storage_type_latest(const StyioDataType& resource_type);

static StyioIR*
zero_value_for_type_latest(const StyioDataType& type);

StyioIR*
AstToStyioIRLowerer::lowerResourceSinkWriteLatest(
  StyioAST* data,
  StyioAST* resource,
  bool redirect_mode
) {
  data = resolveResourceReceiverExprLatest(data);
  resource = resolveResourceReceiverExprLatest(resource);

  if (auto* logical = dynamic_cast<ResourceRefAST*>(resource)) {
    StyioIR* data_ir = data->toStyioIR(this);
    const auto* resource_type_ptr = find_resource_binding_type(
      logical->getName()->getSymbolId(),
      logical->getNameStr());
    if (resource_type_ptr == nullptr) {
      throw StyioTypeError("unknown resource `" + logical->getNameStr() + "`");
    }
    StyioDataType resource_type = *resource_type_ptr;
    StyioDataType storage_type = resource_storage_type_latest(resource_type);
    return SGFlexBind::Create(
      SGVar::Create(SGResId::Create(logical->getNameStr()), SGType::Create(storage_type)),
      data_ir,
      true
    );
  }

  StyioIR* data_ir = data->toStyioIR(this);
  if (auto* ss = dynamic_cast<StdStreamAST*>(resource)) {
    if (ss->getStreamKind() == StdStreamKind::Stdin) {
      const char* action = redirect_mode ? "redirect to" : "write to";
      throw StyioTypeError(std::string("@stdin is a read-only stream; cannot ") + action + " it");
    }
    auto stream = (ss->getStreamKind() == StdStreamKind::Stdout)
                    ? SIOStdStreamWrite::Stream::Stdout
                    : SIOStdStreamWrite::Stream::Stderr;
    if (!redirect_mode && (expr_is_list_like(this, data) || expr_is_dict_like(this, data))) {
      return lower_text_iterable_std_stream_write(this, data, data_ir, stream);
    }
    if (expr_is_list_like(this, data)) {
      data_ir = SCListToString::Create(data_ir);
    }
    else if (expr_is_dict_like(this, data)) {
      data_ir = SCDictToString::Create(data_ir);
    }
    return SIOStdStreamWrite::Create(stream, {data_ir});
  }

  auto* fr = dynamic_cast<FileResourceAST*>(resource);
  if (!fr) {
    const char* op = redirect_mode ? "->" : "<<";
    throw StyioTypeError(std::string(op) + " target must be a file or standard stream resource");
  }

  if (!redirect_mode && (expr_is_list_like(this, data) || expr_is_dict_like(this, data))) {
    return lower_text_iterable_file_write(
      this,
      data,
      data_ir,
      fr->getPath()->toStyioIR(this),
      fr->isAutoDetect()
    );
  }

  if (expr_is_list_like(this, data)) {
    data_ir = SCListToString::Create(data_ir);
  }
  else if (expr_is_dict_like(this, data)) {
    data_ir = SCDictToString::Create(data_ir);
  }

  bool promote = true;
  bool append_newline = false;
  if (!redirect_mode) {
    StyioDataType dt = data->getDataType();
    bool is_str = dt.option == StyioDataTypeOption::String
                  || data->getNodeType() == StyioNodeType::String;
    promote = !is_str;
    append_newline = promote;
  }

  return SIOResourceWriteToFile::Create(
    data_ir,
    fr->getPath()->toStyioIR(this),
    fr->isAutoDetect(),
    promote,
    append_newline
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ResourceWriteAST* ast) {
  if (collect_bind_resource_writes_.count(ast) != 0) {
    auto* target_name = static_cast<NameAST*>(ast->getData());
    StyioDataType collected_type = styio_make_list_type("string");
    auto type_it = collect_bind_resource_write_types_.find(ast);
    if (type_it != collect_bind_resource_write_types_.end()) {
      collected_type = type_it->second;
    }
    auto* var = SGVar::Create(
      SGResId::Create(target_name->getAsStr()),
      SGType::Create(collected_type)
    );
    const BindingInfo* binding = find_binding_info(
      target_name->getSymbolId(),
      target_name->getAsStr()
    );
    if (binding != nullptr) {
      var->is_dynamic_slot = binding->dynamic_slot
                             || binding->value_kind == BindingValueKind::ListHandle;
      var->is_list_slot = !binding->dynamic_slot
                          && binding->value_kind == BindingValueKind::ListHandle;
    }
    return SGFlexBind::Create(
      var,
      SIOListReadStdin::Create(styio_type_item_type_name(collected_type))
    );
  }
  return lowerResourceSinkWriteLatest(
    ast->getData(),
    ast->getResource(),
    false
  );
}

static StyioIR*
lower_file_release_latest(AstToStyioIRLowerer* an, StyioAST* expr) {
  if (auto* name = dynamic_cast<NameAST*>(expr)) {
    return SIOHandleRelease::CreateFromVar(name->getAsStr());
  }
  if (auto* fr = dynamic_cast<FileResourceAST*>(expr)) {
    return SIOHandleRelease::CreateFromPath(
      fr->getPath()->toStyioIR(an),
      fr->isAutoDetect()
    );
  }
  return unsupported_ast_lowering("ResourceRedirectAST", "file release requires a file handle name or file resource");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ResourceRedirectAST* ast) {
  if (dynamic_cast<EmptyResourceAST*>(ast->getResource()) != nullptr) {
    return lower_file_release_latest(this, resolveResourceReceiverExprLatest(ast->getData()));
  }
  return lowerResourceSinkWriteLatest(
    ast->getData(),
    ast->getResource(),
    true
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ResourceEffectAST* ast) {
  StyioIR* operation = ast->getOperation()->toStyioIR(this);
  StyioIR* fallback = ast->hasFallback() ? ast->getFallback()->toStyioIR(this) : nullptr;
  std::vector<SIOResourceEffect::Handler> handlers;
  handlers.reserve(ast->getHandlers().size());
  for (const auto& handler : ast->getHandlers()) {
    handlers.emplace_back(handler.effect_name, handler.body->toStyioIR(this));
  }
  return SIOResourceEffect::Create(
    operation,
    fallback,
    ast->isDiscard(),
    ast->getDataType(),
    std::move(handlers),
    ast->isValueRequired()
  );
}

/*
  Int -> Int => Pass
  Int -> Float => Pass
*/
StyioIR*
AstToStyioIRLowerer::toStyioIR(BinOpAST* ast) {
  return SGBinOp::Create(
    ast->LHS->toStyioIR(this),
    ast->RHS->toStyioIR(this),
    ast->operand,
    static_cast<SGType*>(ast->data_type->toStyioIR(this)),
    expr_lowered_type(this, ast->LHS),
    expr_lowered_type(this, ast->RHS)
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(FmtStrAST* ast) {
  const StyioDataType string_type = lowering_string_type();
  StyioIR* out = SGConstString::Create("");

  auto append = [&](StyioIR* rhs, StyioDataType rhs_type)
  {
    out = SGBinOp::Create(
      out,
      rhs,
      StyioOpType::Binary_Add,
      SGType::Create(string_type),
      string_type,
      rhs_type
    );
  };

  const auto& fragments = ast->getFragments();
  const auto& exprs = ast->getExprs();
  for (size_t i = 0; i < fragments.size(); ++i) {
    if (!fragments[i].empty()) {
      append(SGConstString::Create(fragments[i]), string_type);
    }
    if (i < exprs.size()) {
      append(exprs[i]->toStyioIR(this), expr_lowered_type(this, exprs[i]));
    }
  }

  return out;
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ResourceAST* ast) {
  (void)ast;
  return SGNoOp::Create();
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(EmptyResourceAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("EmptyResourceAST", "empty resource is a redirect/release sentinel only");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ResourceReceiverAST* ast) {
  StyioAST* receiver = resolveResourceReceiverExprLatest(ast);
  if (receiver != ast) {
    return receiver->toStyioIR(this);
  }
  return SGResId::Create(ast->getFamilyName());
}

StyioAST*
AstToStyioIRLowerer::resolveResourceReceiverExprLatest(StyioAST* expr) const {
  auto* receiver = dynamic_cast<ResourceReceiverAST*>(expr);
  if (receiver == nullptr) {
    return expr;
  }
  auto it = resource_receiver_expr_bindings_.find(receiver->getFamilyName());
  if (it == resource_receiver_expr_bindings_.end()) {
    return expr;
  }
  return it->second;
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ResourceMethodDefAST* ast) {
  (void)ast;
  return SGNoOp::Create();
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ResourceOrderAST* ast) {
  (void)ast;
  return SGNoOp::Create();
}

static StyioDataType
resource_storage_type_latest(const StyioDataType& resource_type) {
  StyioDataType value_type = styio_topology_resource_value_type(resource_type);
  StyioValueFamily value_family = styio_value_family_for_type(value_type);
  if ((value_family == StyioValueFamily::Integer
       || value_family == StyioValueFamily::Float
       || value_family == StyioValueFamily::Bool
       || value_family == StyioValueFamily::Char
       || value_family == StyioValueFamily::String
       || value_family == StyioValueFamily::ListHandle
       || value_family == StyioValueFamily::DictHandle
       || value_family == StyioValueFamily::MatrixHandle)
      && resource_type.resource_shape_bound > 0
      && (resource_type.resource_shape == StyioResourceShapeKind::Fixed || resource_type.resource_shape == StyioResourceShapeKind::Recent)) {
    std::string ring_name = std::string(kStyioBoundedRingPrefix);
    if (value_family == StyioValueFamily::Float
        || value_family == StyioValueFamily::Bool
        || value_family == StyioValueFamily::Char
        || value_family == StyioValueFamily::String
        || value_family == StyioValueFamily::ListHandle
        || value_family == StyioValueFamily::DictHandle
        || value_family == StyioValueFamily::MatrixHandle) {
      ring_name += value_type.name + ":";
    }
    ring_name += std::to_string(resource_type.resource_shape_bound);
    return StyioDataType{
      StyioDataTypeOption::Defined,
      ring_name,
      0
    };
  }
  return value_type;
}

static StyioIR*
zero_value_for_type_latest(const StyioDataType& type) {
  if (auto ring_type = styio_bounded_ring_value_type_name(type)) {
    if (*ring_type == "f64") {
      return SGConstFloat::Create("0.0");
    }
    if (*ring_type == "bool") {
      return SGConstBool::Create(false);
    }
    if (*ring_type == "string") {
      return SGConstString::Create("");
    }
    return SGConstInt::Create(0);
  }
  if (type.option == StyioDataTypeOption::Bool) {
    return SGConstBool::Create(false);
  }
  if (type.option == StyioDataTypeOption::Float) {
    return SGConstFloat::Create("0.0");
  }
  if (type.option == StyioDataTypeOption::String) {
    return SGConstString::Create("");
  }
  return SGConstInt::Create(0);
}

static int
resource_selector_snapshot_depth_latest(ResourceRefAST* ast, const StyioDataType& resource_type) {
  if (ast->getSelectorKind() == ResourceSelectorKind::SnapshotAll) {
    if (resource_type.resource_shape_bound > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      throw StyioTypeError("resource selector history bound exceeds supported selector depth");
    }
    return static_cast<int>(resource_type.resource_shape_bound);
  }
  if (ast->getSelectorKind() == ResourceSelectorKind::SliceFrom) {
    if (ast->getSelectorOffset() == std::numeric_limits<int>::min()) {
      throw StyioTypeError("resource slice selector depth exceeds supported selector depth");
    }
    return -ast->getSelectorOffset();
  }
  return 0;
}

static StyioIR*
lower_resource_selector_snapshot_latest(ResourceRefAST* ast, const StyioDataType& resource_type) {
  StyioDataType value_type = styio_topology_resource_value_type(resource_type);
  const int depth = resource_selector_snapshot_depth_latest(ast, resource_type);
  if (depth <= 0) {
    throw StyioTypeError("resource slice/snapshot lowering requires a bounded selector depth");
  }
  std::vector<StyioIR*> elems;
  elems.reserve(static_cast<std::size_t>(depth));
  for (int d = depth; d >= 1; --d) {
    elems.push_back(SGResId::CreateHistory(ast->getNameStr(), -d));
  }
  return SCListLiteral::Create(std::move(elems), value_type.name);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ResourceDeclAST* ast) {
  std::vector<StyioIR*> stmts;
  for (const auto& slot : ast->getSlots()) {
    const std::string name = slot.name->getAsStr();
    StyioDataType resource_type = styio_normalize_resource_decl_type(slot.type->getDataType());
    const auto* recorded_resource_type = find_resource_binding_type(slot.name->getSymbolId(), name);
    if (recorded_resource_type != nullptr) {
      resource_type = *recorded_resource_type;
    }
    StyioDataType storage_type = resource_storage_type_latest(resource_type);
    auto* var = SGVar::Create(SGResId::Create(name), SGType::Create(storage_type));
    stmts.push_back(SGFinalBind::Create(var, zero_value_for_type_latest(storage_type)));
  }
  if (ast->getDriver() != nullptr) {
    stmts.push_back(ast->getDriver()->toStyioIR(this));
  }
  return SGBlock::Create(std::move(stmts));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ResourceRefAST* ast) {
  if (ast->getSelectorKind() == ResourceSelectorKind::Offset) {
    return SGResId::CreateHistory(ast->getNameStr(), ast->getSelectorOffset());
  }
  if (ast->getSelectorKind() == ResourceSelectorKind::SliceFrom
      || ast->getSelectorKind() == ResourceSelectorKind::SnapshotAll) {
    const auto* resource_type = find_resource_binding_type(
      ast->getName()->getSymbolId(),
      ast->getNameStr());
    if (resource_type == nullptr) {
      throw StyioTypeError("unknown resource `" + ast->getNameStr() + "`");
    }
    return lower_resource_selector_snapshot_latest(ast, *resource_type);
  }
  return SGResId::Create(ast->getNameStr());
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ResPathAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("ResPathAST", "resource path values are not implemented as runtime IR");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(RemotePathAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("RemotePathAST", "remote path values are not implemented as runtime IR");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(WebUrlAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("WebUrlAST", "web URL values are not implemented as runtime IR");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(DBUrlAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("DBUrlAST", "database URL values are not implemented as runtime IR");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ExtPackAST* ast) {
  (void)ast;
  return SGNoOp::Create();
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ExportDeclAST* ast) {
  return SGExportDecl::Create(ast->getSymbols());
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ExternBlockAST* ast) {
  return SGExternBlock::Create(
    ast->getAbi(),
    ast->getBody(),
    ast->getSourcePaths(),
    ast->getExportedSymbols());
}

// MIGRATION-NEEDED: M-SEMA-01 (docs/rollups/MIGRATION-LEDGER.md)
// Either restore lowering for ReadFileAST or delete the AST node entirely.
StyioIR*
AstToStyioIRLowerer::toStyioIR(ReadFileAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("ReadFileAST", "legacy read-file syntax is superseded by file resources");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(EOFAST* ast) {
  (void)ast;
  return SGNoOp::Create();
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(BreakAST* ast) {
  (void)ast;
  return SGBreak::Create(1u);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ContinueAST* ast) {
  (void)ast;
  return SGContinue::Create();
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(PassAST* ast) {
  (void)ast;
  return SGNoOp::Create();
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ReturnAST* ast) {
  return SGReturn::Create(ast->getExpr()->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(FuncCallAST* ast) {
  const StyioBuiltinMethodKind builtin_method = styio_builtin_method_kind(ast->getNameAsStr());
  if (ast->func_callee != nullptr && styio_is_predefined_list_operation_kind(builtin_method)) {
    std::vector<StyioIR*> args;
    args.reserve(ast->getArgList().size() + 1);
    args.push_back(ast->func_callee->toStyioIR(this));
    for (auto* a : ast->getArgList()) {
      args.push_back(a->toStyioIR(this));
    }
    return SGCall::Create(
      SGResId::Create(predefined_list_operation_runtime_name(
        ast->getNameAsStr(),
        expr_lowered_type(this, ast->func_callee)
      )),
      std::move(args)
    );
  }

  if (ast->func_callee != nullptr && styio_is_predefined_string_operation_kind(builtin_method)) {
    if (!ast->getArgList().empty()) {
      throw StyioTypeError("string.lines() does not take arguments");
    }
    return SGCall::Create(
      SGResId::Create("__styio_string_lines"),
      {ast->func_callee->toStyioIR(this)}
    );
  }

  if (ast->isCallableApply()) {
    throw StyioTypeError(
      "one-shot continuation resume `<|` requires continuation lowering; "
      "captured continuations must be resumed or discontinued exactly once"
    );
  }

  if (ast->isIndirectCallableCall()) {
    std::vector<StyioIR*> args;
    args.reserve(ast->getArgList().size());
    for (auto* arg : ast->getArgList()) {
      args.push_back(arg->toStyioIR(this));
    }
    return SGCall::CreateIndirect(
      SGResId::Create(ast->getNameAsStr()),
      ast->getIndirectCallableType(),
      std::move(args));
  }

  if (ast->func_callee == nullptr && is_matrix_intrinsic_name(ast->getNameAsStr())) {
    std::vector<StyioIR*> args;
    for (auto* a : ast->getArgList()) {
      args.push_back(a->toStyioIR(this));
    }
    return SGCall::Create(
      SGResId::Create(matrix_intrinsic_runtime_name(this, ast)),
      std::move(args)
    );
  }

  if (ast->func_callee != nullptr) {
    StyioDataType receiver_type = expr_lowered_type(this, ast->func_callee);
    if (receiver_type.handle_family == StyioHandleFamily::File
        || receiver_type.handle_family == StyioHandleFamily::Stream
        || styio_is_topology_resource_type(receiver_type)) {
      const std::string family = resource_family_for_lowering_expr(this, ast->func_callee);
      const ResourceMethodInfo* method = find_resource_method(family, ast->getNameAsStr());
      auto family_def_it = resource_method_body_defs_.find(family);
      if (family_def_it != resource_method_body_defs_.end()) {
        auto method_def_it = family_def_it->second.find(ast->getNameAsStr());
        if (method_def_it != family_def_it->second.end()) {
          auto saved_receiver = resource_receiver_expr_bindings_.find(family);
          StyioAST* saved_receiver_expr = saved_receiver == resource_receiver_expr_bindings_.end()
                                            ? nullptr
                                            : saved_receiver->second;
          auto restore_receiver = [&]()
          {
            if (saved_receiver == resource_receiver_expr_bindings_.end()) {
              resource_receiver_expr_bindings_.erase(family);
            }
            else {
              resource_receiver_expr_bindings_[family] = saved_receiver_expr;
            }
          };
          resource_receiver_expr_bindings_[family] = ast->func_callee;
          StyioIR* lowered = nullptr;
          try {
            StyioAST* inlined_body = clone_resource_method_body_latest(
              method_def_it->second,
              ast->func_callee,
              ast->getArgList()
            );
            if (inlined_body == nullptr) {
              throw StyioTypeError("resource method lowering produced no body");
            }
            if (StyioIR* value_body = lower_resource_method_value_body_latest(this, inlined_body)) {
              lowered = value_body;
            }
            else {
              lowered = flatten_single_stmt_block_latest(inlined_body->toStyioIR(this));
            }
          }
          catch (...) {
            restore_receiver();
            throw;
          }
          restore_receiver();
          return lowered;
        }
      }
      const bool consuming_method = method != nullptr && method->consuming;
      if (receiver_type.handle_family == StyioHandleFamily::File) {
        if (consuming_method) {
          return lower_file_release_latest(this, resolveResourceReceiverExprLatest(ast->func_callee));
        }
        if (method != nullptr
            && styio_is_resource_write_method_kind(builtin_method)
            && ast->getArgList().size() == 1) {
          StyioAST* resolved_receiver = resolveResourceReceiverExprLatest(ast->func_callee);
          FileResourceAST* fr = dynamic_cast<FileResourceAST*>(resolved_receiver);
          std::string required_handle_var;
          if (fr == nullptr) {
            if (auto* name = dynamic_cast<NameAST*>(resolved_receiver)) {
              auto fit = file_resource_bindings_.find(name->getAsStr());
              if (fit != file_resource_bindings_.end()) {
                fr = fit->second;
                required_handle_var = name->getAsStr();
              }
            }
          }
          if (fr != nullptr) {
            StyioAST* data = ast->getArgList().front();
            StyioIR* data_ir = data->toStyioIR(this);
            StyioDataType dt = data->getDataType();
            bool is_str = dt.option == StyioDataTypeOption::String
                          || data->getNodeType() == StyioNodeType::String;
            return SIOResourceWriteToFile::Create(
              data_ir,
              fr->getPath()->toStyioIR(this),
              fr->isAutoDetect(),
              !is_str,
              false,
              required_handle_var
            );
          }
        }
      }
      return unsupported_ast_lowering("FuncCallAST", "unsupported resource method call");
    }
  }

  StyioAST* func_def = find_function_def(
    ast->func_name->getSymbolId(),
    ast->getNameAsStr()
  );
  if (func_def == nullptr) {
    if (find_native_function_def(ast->func_name->getSymbolId(), ast->getNameAsStr()) != nullptr) {
      std::vector<StyioIR*> args;
      for (auto* a : ast->getArgList()) {
        args.push_back(a->toStyioIR(this));
      }
      return SGCall::Create(
        SGResId::Create(ast->getNameAsStr()),
        std::move(args)
      );
    }
    throw StyioTypeError("unknown function `" + ast->getNameAsStr() + "`");
  }
  auto params = params_of_func_def(func_def);
  if (params.size() != ast->getArgList().size()) {
    throw StyioTypeError(
      "function `" + ast->getNameAsStr() + "` expects "
      + std::to_string(params.size()) + " argument(s), got "
      + std::to_string(ast->getArgList().size())
    );
  }

  std::vector<StyioIR*> args;
  for (auto* a : ast->getArgList()) {
    args.push_back(a->toStyioIR(this));
  }
  return SGCall::Create(
    SGResId::Create(
      ast->getLoweredCalleeName().empty()
        ? ast->getNameAsStr()
        : ast->getLoweredCalleeName()),
    std::move(args)
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(AttrAST* ast) {
  auto* attr_name = dynamic_cast<NameAST*>(ast->attr);
  if (attr_name == nullptr) {
    return unsupported_ast_lowering("AttrAST", "attribute name must be a simple identifier");
  }
  const std::string attr_str = attr_name->getAsStr();
  const StyioBuiltinMethodKind builtin_method = styio_builtin_method_kind(attr_str);
  StyioDataType body_type = ast->body->getDataType();
  body_type = expr_lowered_type(this, ast->body);
  if (attr_str == "keys") {
    return SCDictKeys::Create(ast->body->toStyioIR(this));
  }
  if (attr_str == "values") {
    return SCDictValues::Create(
      ast->body->toStyioIR(this),
      styio_dict_value_type_name(body_type)
    );
  }
  const std::string family = resource_family_for_lowering_type(body_type);
  const ResourceMethodInfo* method = find_resource_method(family, attr_str);
  if (method != nullptr && method->property) {
    auto family_def_it = resource_method_body_defs_.find(family);
    if (family_def_it != resource_method_body_defs_.end()) {
      auto method_def_it = family_def_it->second.find(attr_str);
      if (method_def_it != family_def_it->second.end()
          && method_def_it->second->isProperty()) {
        auto saved_receiver = resource_receiver_expr_bindings_.find(family);
        StyioAST* saved_receiver_expr = saved_receiver == resource_receiver_expr_bindings_.end()
                                          ? nullptr
                                          : saved_receiver->second;
        auto restore_receiver = [&]()
        {
          if (saved_receiver == resource_receiver_expr_bindings_.end()) {
            resource_receiver_expr_bindings_.erase(family);
          }
          else {
            resource_receiver_expr_bindings_[family] = saved_receiver_expr;
          }
        };
        resource_receiver_expr_bindings_[family] = ast->body;
        StyioIR* lowered = nullptr;
        try {
          StyioAST* inlined_body = clone_resource_method_body_latest(
            method_def_it->second,
            ast->body,
            {}
          );
          if (inlined_body == nullptr) {
            throw StyioTypeError("resource property lowering produced no body");
          }
          if (StyioIR* value_body = lower_resource_method_value_body_latest(this, inlined_body)) {
            lowered = value_body;
          }
          else {
            lowered = flatten_single_stmt_block_latest(inlined_body->toStyioIR(this));
          }
        }
        catch (...) {
          restore_receiver();
          throw;
        }
        restore_receiver();
        return lowered;
      }
    }
    if (body_type.handle_family == StyioHandleFamily::File
        && styio_is_resource_property_method_kind(builtin_method)) {
      StyioAST* resolved_body = resolveResourceReceiverExprLatest(ast->body);
      if (auto* fr = dynamic_cast<FileResourceAST*>(resolved_body)) {
        return fr->getPath()->toStyioIR(this);
      }
      if (auto* name = dynamic_cast<NameAST*>(resolved_body)) {
        auto fit = file_resource_bindings_.find(name->getAsStr());
        if (fit != file_resource_bindings_.end()) {
          return fit->second->getPath()->toStyioIR(this);
        }
      }
      return SGConstString::Create("");
    }
    return unsupported_ast_lowering("AttrAST", "unsupported resource property access");
  }
  if (styio_is_dict_type(body_type)) {
    return SCDictLen::Create(ast->body->toStyioIR(this));
  }
  return SCListLen::Create(ast->body->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(PrintAST* ast) {
  /* Standard streams: unify >_() to SIOStdStreamWrite(Stdout). */
  std::vector<StyioIR*> parts;
  for (auto* e : ast->exprs) {
    StyioIR* lowered = e->toStyioIR(this);
    parts.push_back(
      expr_is_matrix_like(this, e)
        ? static_cast<StyioIR*>(SCMatrixToString::Create(lowered))
        : (expr_is_list_like(this, e)
             ? static_cast<StyioIR*>(SCListToString::Create(lowered))
             : (expr_is_dict_like(this, e)
                  ? static_cast<StyioIR*>(SCDictToString::Create(lowered))
                  : lowered))
    );
  }
  return SIOStdStreamWrite::Create(SIOStdStreamWrite::Stream::Stdout, parts);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ForwardAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("ForwardAST", "forward flow syntax has no active StyioIR lowering");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(BackwardAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("BackwardAST", "backward flow syntax has no active StyioIR lowering");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(CODPAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("CODPAST", "chain-of-data-processing syntax has no active StyioIR lowering");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(CheckEqualAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("CheckEqualAST", "match guards must be lowered through MatchCasesAST");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(CheckIsinAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("CheckIsinAST", "match membership guards are not implemented as standalone IR");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(HashTagNameAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("HashTagNameAST", "hash-tag names are parser metadata, not runtime values");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(CondFlowAST* ast) {
  SGBlock* then_block = lower_func_body(this, ast->getThen());
  SGBlock* else_block = ast->getElse() ? lower_func_body(this, ast->getElse()) : nullptr;
  return SGIf::Create(ast->getCond()->toStyioIR(this), then_block, else_block);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(AnonyFuncAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("AnonyFuncAST", "anonymous function closures are not implemented in StyioIR");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(FunctionAST* ast) {
  int saved_hist_r = post_pulse_hist_region_;
  SGPulsePlan* saved_hist_p = post_pulse_hist_plan_;
  set_post_pulse_hist_context(-1, nullptr);
  auto saved_local_types = local_binding_types;
  auto saved_local_types_by_sid = local_binding_types_by_sid;
  std::vector<SGFuncArg*> fargs;
  for (std::size_t i = 0; i < ast->params.size(); ++i) {
    auto* p = ast->params[i];
    record_local_binding_type(
      p->getName(),
      p->var_name->getSymbolId(),
      param_data_type(p, this, ast->getNameAsStr(), i));
    fargs.push_back(
      param_to_sgarg(p, this, ast->getNameAsStr(), i));
  }
  SGType* rt = func_ret_to_sgtype(ast->ret_type, this);
  const auto* specialization =
    active_callable_specialization(ast->getNameAsStr());
  if (specialization != nullptr) {
    delete rt;
    rt = SGType::Create(specialization->result_type);
  }
  else if (func_ret_is_unspecified(ast->ret_type)) {
    StyioDataType inferred_ret = infer_tail_value_type(this, ast->func_body);
    if (!inferred_ret.isUndefined()) {
      delete rt;
      rt = SGType::Create(inferred_ret);
    }
  }
  if (auto* blk = dynamic_cast<BlockAST*>(ast->func_body)) {
    if (blk->stmts.size() == 1 && blk->stmts[0]->getNodeType() == StyioNodeType::MatchCases) {
      auto* mc = static_cast<MatchCasesAST*>(blk->stmts[0]);
      CasesAST* c = mc->getCases();
      bool hs = false;
      bool hi = false;
      bool hf = false;
      for (auto const& pr : c->case_list) {
        scan_returns_for_value_kinds(pr.second, hs, hi, hf);
      }
      scan_returns_for_value_kinds(c->case_default, hs, hi, hf);
      if (hs) {
        delete rt;
        rt = SGType::Create(StyioDataType{StyioDataTypeOption::String, "string", 0});
      }
    }
  }
  SGBlock* body = nullptr;
  try {
    if (auto* direct_cases = dynamic_cast<CasesAST*>(ast->func_body)) {
      if (ast->params.size() != 1) {
        throw StyioTypeError("function match sugar requires exactly one parameter");
      }
      const std::string scrutinee_name = ast->params[0]->getName();
      body = SGBlock::Create({
        SGReturn::Create(lower_cases_with_scrutinee(
          this,
          direct_cases,
          SGResId::Create(scrutinee_name),
          &scrutinee_name,
          &rt->data_type
        ))
      });
    }
    else {
      body = lower_func_body_with_local_defs(this, ast->func_body, true);
    }
  }
  catch (...) {
    local_binding_types = std::move(saved_local_types);
    local_binding_types_by_sid = std::move(saved_local_types_by_sid);
    set_post_pulse_hist_context(saved_hist_r, saved_hist_p);
    throw;
  }
  local_binding_types = std::move(saved_local_types);
  local_binding_types_by_sid = std::move(saved_local_types_by_sid);
  const auto* effect_facts =
    find_callable_effect_row(ast->getNameAsStr());
  std::vector<std::string> capture_names;
  capture_names.reserve(ast->getCaptureNames().size());
  for (auto* capture : ast->getCaptureNames()) {
    capture_names.push_back(capture->getAsStr());
  }
  SGFunc* fn = SGFunc::Create(
    rt,
    SGResId::Create(
      specialization == nullptr
        ? ast->getNameAsStr()
        : specialization->lowered_name),
    std::move(fargs),
    body,
    effect_facts == nullptr
      ? styio::sema::CallableEffectRow::unknown()
      : effect_facts->row,
    std::move(capture_names),
    specialization == nullptr
      ? std::string()
      : specialization->content_digest
  );
  set_post_pulse_hist_context(saved_hist_r, saved_hist_p);
  return fn;
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(SimpleFuncAST* ast) {
  int saved_hist_r = post_pulse_hist_region_;
  SGPulsePlan* saved_hist_p = post_pulse_hist_plan_;
  set_post_pulse_hist_context(-1, nullptr);
  auto saved_local_types = local_binding_types;
  auto saved_local_types_by_sid = local_binding_types_by_sid;
  std::vector<SGFuncArg*> fargs;
  for (std::size_t i = 0; i < ast->params.size(); ++i) {
    auto* p = ast->params[i];
    record_local_binding_type(
      p->getName(),
      p->var_name->getSymbolId(),
      param_data_type(p, this, ast->func_name->getAsStr(), i));
    fargs.push_back(
      param_to_sgarg(
        p,
        this,
        ast->func_name->getAsStr(),
        i));
  }
  SGType* rt = func_ret_to_sgtype(ast->ret_type, this);
  const auto* specialization =
    active_callable_specialization(ast->func_name->getAsStr());
  if (specialization != nullptr) {
    delete rt;
    rt = SGType::Create(specialization->result_type);
  }
  else if (func_ret_is_unspecified(ast->ret_type)) {
    StyioDataType inferred_ret = infer_tail_value_type(this, ast->ret_expr);
    if (!inferred_ret.isUndefined()) {
      delete rt;
      rt = SGType::Create(inferred_ret);
    }
  }
  if (auto* blk = dynamic_cast<BlockAST*>(ast->ret_expr)) {
    if (blk->stmts.size() == 1 && blk->stmts[0]->getNodeType() == StyioNodeType::MatchCases) {
      auto* mc = static_cast<MatchCasesAST*>(blk->stmts[0]);
      CasesAST* c = mc->getCases();
      bool hs = false;
      bool hi = false;
      bool hf = false;
      for (auto const& pr : c->case_list) {
        scan_returns_for_value_kinds(pr.second, hs, hi, hf);
      }
      scan_returns_for_value_kinds(c->case_default, hs, hi, hf);
      if (hs) {
        /* Any <| "..." arm: LLVM return must be i8* (and may mix with snprintf ints). */
        delete rt;
        rt = SGType::Create(StyioDataType{StyioDataTypeOption::String, "string", 0});
      }
    }
  }
  SGBlock* body = nullptr;
  try {
    body = lower_func_body_with_local_defs(this, ast->ret_expr, true);
  }
  catch (...) {
    local_binding_types = std::move(saved_local_types);
    local_binding_types_by_sid = std::move(saved_local_types_by_sid);
    set_post_pulse_hist_context(saved_hist_r, saved_hist_p);
    throw;
  }
  local_binding_types = std::move(saved_local_types);
  local_binding_types_by_sid = std::move(saved_local_types_by_sid);
  const auto* effect_facts =
    find_callable_effect_row(ast->func_name->getAsStr());
  std::vector<std::string> capture_names;
  capture_names.reserve(ast->getCaptureNames().size());
  for (auto* capture : ast->getCaptureNames()) {
    capture_names.push_back(capture->getAsStr());
  }
  SGFunc* fn = SGFunc::Create(
    rt,
    SGResId::Create(
      specialization == nullptr
        ? ast->func_name->getAsStr()
        : specialization->lowered_name),
    std::move(fargs),
    body,
    effect_facts == nullptr
      ? styio::sema::CallableEffectRow::unknown()
      : effect_facts->row,
    std::move(capture_names),
    specialization == nullptr
      ? std::string()
      : specialization->content_digest
  );
  set_post_pulse_hist_context(saved_hist_r, saved_hist_p);
  return fn;
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(IteratorAST* ast) {
  if (ast->getNodeType() == StyioNodeType::IterSeq) {
    throw StyioTypeError(
      "iterator sequence hash-tag routing is not implemented; use #(param) => { ... } iterator bodies"
    );
  }

  std::string vname = "it";
  if (!ast->params.empty()) {
    vname = ast->params[0]->getName();
  }
  auto bind_iter_param = [&](const std::string& name, const StyioDataType& type)
  {
    record_local_binding_type(name, ast->params[0]->var_name->getSymbolId(), type);
  };
  auto saved_locals = local_binding_types;
  auto saved_locals_by_sid = local_binding_types_by_sid;
  auto saved_bind = binding_info_;
  auto saved_bind_by_sid = binding_info_by_sid_;
  if (!ast->params.empty()) {
    bind_iter_param(
      vname,
      styio_data_type_from_name(
        styio_type_item_type_name(expr_lowered_type(this, ast->collection))
      )
    );
  }
  std::unique_ptr<SGBlock> body(SGBlock::Create({}));
  std::unique_ptr<SGPulsePlan> pplan;
  if (!ast->following.empty()) {
    auto* abody = dynamic_cast<BlockAST*>(ast->following[0]);
    if (abody && pulse_block_has_state(this, abody)) {
      PulseScratch scratch;
      std::unordered_map<StyioAST*, StateDeclAST*> cache;
      pplan = build_pulse_plan(this, abody, &scratch, cache);
      body.reset(lower_pulse_body(this, abody, pplan.get(), &scratch, cache));
    }
    else {
      body.reset(lower_func_body(this, ast->following[0]));
    }
  }
  local_binding_types = std::move(saved_locals);
  local_binding_types_by_sid = std::move(saved_locals_by_sid);
  binding_info_ = std::move(saved_bind);
  binding_info_by_sid_ = std::move(saved_bind_by_sid);
  if (ast->collection->getNodeType() == StyioNodeType::Range) {
    auto* rg = static_cast<RangeAST*>(ast->collection);
    return SGRangeFor::Create(
      rg->getStart()->toStyioIR(this),
      rg->getEnd()->toStyioIR(this),
      rg->getStep()->toStyioIR(this),
      std::move(vname),
      body.release()
    );
  }
  /* Stdio input: stdin line iteration. */
  auto col_nt = ast->collection->getNodeType();
  if (col_nt == StyioNodeType::StdinResource
      || col_nt == StyioNodeType::StdoutResource
      || col_nt == StyioNodeType::StderrResource) {
    auto* ss = static_cast<StdStreamAST*>(ast->collection);
    if (ss->getStreamKind() == StdStreamKind::Stdout) {
      throw StyioTypeError("@stdout is a write-only stream; cannot iterate over it");
    }
    if (ss->getStreamKind() == StdStreamKind::Stderr) {
      throw StyioTypeError("@stderr is a write-only stream; cannot iterate over it");
    }
    auto* sl = SIOStdStreamLineIter::Create(std::move(vname), body.get());
    body.release();
    if (pplan) {
      sl->set_pulse_plan(std::move(pplan));
      if (sl->pulse_plan && sl->pulse_plan->total_bytes > 0) {
        sl->pulse_region_id = alloc_pulse_region_id();
      }
    }
    return sl;
  }
  if (ast->collection->getNodeType() == StyioNodeType::FileResource) {
    auto* fr = static_cast<FileResourceAST*>(ast->collection);
    auto* fl = SIOFileLineIter::CreateFromPath(
      fr->getPath()->toStyioIR(this),
      std::move(vname),
      body.get()
    );
    body.release();
    if (pplan) {
      fl->set_pulse_plan(std::move(pplan));
      if (fl->pulse_plan && fl->pulse_plan->total_bytes > 0) {
        fl->pulse_region_id = alloc_pulse_region_id();
      }
    }
    return fl;
  }
  if (ast->collection->getNodeType() == StyioNodeType::Id) {
    auto* nm = static_cast<NameAST*>(ast->collection);
    const StyioDataType* collection_type =
      find_local_binding_type(nm->getSymbolId(), nm->getAsStr());
    if (collection_type != nullptr) {
      if (auto kind = std_stream_kind_of(*collection_type)) {
        if (*kind == StdStreamKind::Stdout) {
          throw StyioTypeError("@stdout is a write-only stream; cannot iterate over it");
        }
        if (*kind == StdStreamKind::Stderr) {
          throw StyioTypeError("@stderr is a write-only stream; cannot iterate over it");
        }
        auto* sl = SIOStdStreamLineIter::Create(std::move(vname), body.get());
        body.release();
        if (pplan) {
          sl->set_pulse_plan(std::move(pplan));
          if (sl->pulse_plan && sl->pulse_plan->total_bytes > 0) {
            sl->pulse_region_id = alloc_pulse_region_id();
          }
        }
        return sl;
      }
      if (collection_type->handle_family == StyioHandleFamily::File) {
        auto* fl = SIOFileLineIter::CreateFromHandle(
          nm->getAsStr(),
          std::move(vname),
          body.get()
        );
        body.release();
        if (pplan) {
          fl->set_pulse_plan(std::move(pplan));
          if (fl->pulse_plan && fl->pulse_plan->total_bytes > 0) {
            fl->pulse_region_id = alloc_pulse_region_id();
          }
        }
        return fl;
      }
    }
  }
  auto* fe = SGForEach::Create(
    ast->collection->toStyioIR(this),
    std::move(vname),
    styio_type_item_type_name(expr_lowered_type(this, ast->collection)),
    body.get()
  );
  body.release();
  if (pplan) {
    fe->set_pulse_plan(std::move(pplan));
    if (fe->pulse_plan && fe->pulse_plan->total_bytes > 0) {
      fe->pulse_region_id = alloc_pulse_region_id();
    }
  }
  return fe;
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(StreamZipAST* ast) {
  std::string va = "a";
  std::string vb = "b";
  if (!ast->getParamsA().empty()) {
    va = ast->getParamsA()[0]->getNameAsStr();
  }
  if (!ast->getParamsB().empty()) {
    vb = ast->getParamsB()[0]->getNameAsStr();
  }
  auto bind_zip_param = [&](const std::string& name, const StyioDataType& type)
  {
    record_local_binding_type(name, styio::session::kInvalidSymbolId, type);
  };
  auto saved_locals = local_binding_types;
  auto saved_locals_by_sid = local_binding_types_by_sid;
  auto saved_bind = binding_info_;
  if (!ast->getParamsA().empty()) {
    bind_zip_param(
      va,
      styio_data_type_from_name(
        styio_type_item_type_name(expr_lowered_type(this, ast->getCollectionA()))
      )
    );
  }
  if (!ast->getParamsB().empty()) {
    bind_zip_param(
      vb,
      styio_data_type_from_name(
        styio_type_item_type_name(expr_lowered_type(this, ast->getCollectionB()))
      )
    );
  }
  std::unique_ptr<SGBlock> body(SGBlock::Create({}));
  std::unique_ptr<SGPulsePlan> pplan;
  if (!ast->getFollowing().empty()) {
    auto* abody = dynamic_cast<BlockAST*>(ast->getFollowing()[0]);
    if (abody && pulse_block_has_state(this, abody)) {
      PulseScratch scratch;
      std::unordered_map<StyioAST*, StateDeclAST*> cache;
      pplan = build_pulse_plan(this, abody, &scratch, cache);
      body.reset(lower_pulse_body(this, abody, pplan.get(), &scratch, cache));
    }
    else {
      body.reset(lower_func_body(this, ast->getFollowing()[0]));
    }
  }
  local_binding_types = std::move(saved_locals);
  local_binding_types_by_sid = std::move(saved_locals_by_sid);
  binding_info_ = std::move(saved_bind);
  StyioAST* ca = ast->getCollectionA();
  StyioAST* cb = ast->getCollectionB();
  bool fa = false;
  bool fb = false;
  bool sa = false;
  bool sb = false;
  StyioIR* ia = nullptr;
  StyioIR* ib = nullptr;
  if (ca->getNodeType() == StyioNodeType::FileResource) {
    fa = true;
    ia = static_cast<FileResourceAST*>(ca)->getPath()->toStyioIR(this);
  }
  else if (auto* stream = dynamic_cast<StdStreamAST*>(ca)) {
    if (stream->getStreamKind() != StdStreamKind::Stdin) {
      throw StyioTypeError("stream zip only supports @stdin among standard streams");
    }
    sa = true;
    ia = SGNoOp::Create();
  }
  else {
    ia = ca->toStyioIR(this);
  }
  if (cb->getNodeType() == StyioNodeType::FileResource) {
    fb = true;
    ib = static_cast<FileResourceAST*>(cb)->getPath()->toStyioIR(this);
  }
  else if (auto* stream = dynamic_cast<StdStreamAST*>(cb)) {
    if (stream->getStreamKind() != StdStreamKind::Stdin) {
      throw StyioTypeError("stream zip only supports @stdin among standard streams");
    }
    sb = true;
    ib = SGNoOp::Create();
  }
  else {
    ib = cb->toStyioIR(this);
  }
  if (sa && sb) {
    throw StyioTypeError("zip over @stdin on both sides requires a distinct stream-driver decision");
  }
  bool astr = collection_elem_is_string(this, ca);
  bool bstr = collection_elem_is_string(this, cb);
  auto zip_elem_type = [&](StyioAST* collection)
  {
    std::string elem_type = styio_type_item_type_name(expr_lowered_type(this, collection));
    return elem_type.empty() ? std::string("i64") : elem_type;
  };
  std::string a_elem_type = zip_elem_type(ca);
  std::string b_elem_type = zip_elem_type(cb);
  auto* z = SIOStreamZip::Create(
    ia,
    fa,
    sa,
    std::move(va),
    ib,
    fb,
    sb,
    std::move(vb),
    astr,
    bstr,
    std::move(a_elem_type),
    std::move(b_elem_type),
    body.get());
  body.release();
  if (pplan) {
    z->set_pulse_plan(std::move(pplan));
    if (z->pulse_plan && z->pulse_plan->total_bytes > 0) {
      z->pulse_region_id = alloc_pulse_region_id();
    }
  }
  return z;
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(SnapshotDeclAST* ast) {
  return SGSnapshotDecl::Create(ast->getVar()->getAsStr(), ast->getResource()->getPath()->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(InstantPullAST* ast) {
  /* Stdio input: stdin instant pull. */
  auto* ss = dynamic_cast<StdStreamAST*>(ast->getResource());
  if (ss) {
    if (ss->getStreamKind() == StdStreamKind::Stdout) {
      throw StyioTypeError("@stdout is a write-only stream; cannot read from it");
    }
    if (ss->getStreamKind() == StdStreamKind::Stderr) {
      throw StyioTypeError("@stderr is a write-only stream; cannot read from it");
    }
    return SIOStdStreamPull::Create(ast->getDataType());
  }
  auto* fr = dynamic_cast<FileResourceAST*>(ast->getResource());
  if (!fr) {
    if (auto* name = dynamic_cast<NameAST*>(ast->getResource())) {
      auto source_type = bound_type_of(this, name);
      if (!source_type.has_value()
          || source_type->handle_family != StyioHandleFamily::File) {
        throw StyioTypeError("instant pull handle source must be an acquired file handle");
      }
      return SIOInstantPull::CreateFromHandle(name->getAsStr());
    }
    throw StyioTypeError("instant pull needs @file(...), @{...}, @stdin, or an acquired file handle");
  }
  return SIOInstantPull::Create(fr->getPath()->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(TaskBlockAST* ast) {
  auto* body = static_cast<SGBlock*>(ast->getBody()->toStyioIR(this));
  StyioDataType result_type = ast->getResultType();
  if (result_type.isUndefined()) {
    result_type = StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  }
  return SIOTaskCreate::Create(body, result_type);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(TaskGroupLaunchAST* ast) {
  std::vector<StyioIR*> stmts;
  stmts.reserve(ast->getEntries().size());
  for (auto* entry : ast->getEntries()) {
    stmts.push_back(entry->toStyioIR(this));
  }
  return SGEntry::Create(std::move(stmts));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(FlowBindAST* ast) {
  if (ast->getSource() == nullptr) {
    throw StyioTypeError(
      "bare continuation freeze `?| ->` requires continuation lowering; "
      "captured continuations must be resumed or discontinued exactly once"
    );
  }
  StyioDataType source_type = expr_lowered_type(this, ast->getSource());
  StyioDataType result_type = ast->getResultType();
  bool source_is_task = source_type.handle_family == StyioHandleFamily::Task;
  if (source_is_task && result_type.isUndefined()) {
    result_type = styio_data_type_from_name(styio_task_result_type_name(source_type));
  }
  if (result_type.isUndefined()) {
    result_type = source_type;
  }
  StyioIR* fallback = ast->hasFallback() ? ast->getFallback()->toStyioIR(this) : nullptr;
  return SIOFlowBind::Create(
    ast->getSource()->toStyioIR(this),
    ast->getTargetNameAsStr(),
    result_type,
    source_is_task,
    fallback,
    ast->isAwaitBind()
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(IterSeqAST* ast) {
  (void)ast;
  throw StyioTypeError(
    "iterator sequence hash-tag routing is not implemented; use #(param) => { ... } iterator bodies"
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(InfiniteLoopAST* ast) {
  SGBlock* b = lower_func_body(this, ast->getBody());
  if (ast->getWhileCond()) {
    return SGLoop::CreateWhile(ast->getWhileCond()->toStyioIR(this), b);
  }
  return SGLoop::CreateInfinite(b);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(CasesAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("CasesAST", "cases must be lowered through MatchCasesAST");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(StateDeclAST* ast) {
  (void)ast;
  return unsupported_ast_lowering("StateDeclAST", "state declarations must be lowered through pulse topology planning");
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(StateRefAST* ast) {
  if (is_snapshot_var(ast->getNameStr())) {
    return SGSnapshotShadowLoad::Create(ast->getNameStr());
  }
  auto* pl = cur_pulse_plan();
  if (!pl) {
    throw StyioTypeError("retired state reference only valid inside pulse body");
  }
  auto it = pl->ref_to_slot.find(ast->getNameStr());
  if (it == pl->ref_to_slot.end()) {
    throw StyioTypeError("unknown state reference");
  }
  return SGStateSnapLoad::Create(it->second);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(HistoryProbeAST* ast) {
  auto* pl = cur_pulse_plan();
  int ledger_region = -1;
  if (!pl) {
    pl = post_pulse_hist_plan_;
    ledger_region = post_pulse_hist_region_;
    if (!pl || ledger_region < 0) {
      throw StyioTypeError(
        "history probe only valid inside pulse body or after foreach/file line iter with pulse"
      );
    }
  }
  std::string nm = ast->getTarget()->getNameStr();
  auto it = pl->ref_to_slot.find(nm);
  if (it == pl->ref_to_slot.end()) {
    throw StyioTypeError("unknown state in history probe");
  }
  int dep = window_n_from_ast(ast->getDepth());
  return SGStateHistLoad::Create(it->second, dep, ledger_region);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(SeriesIntrinsicAST* ast) {
  int sid = active_series_slot();
  if (sid < 0) {
    throw StyioTypeError("series intrinsic needs enclosing state slot");
  }
  StyioIR* bx = ast->getBase()->toStyioIR(this);
  if (ast->getOp() == SeriesIntrinsicOp::Avg) {
    return SGSeriesAvgStep::Create(sid, bx);
  }
  return SGSeriesMaxStep::Create(sid, bx);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(MatchCasesAST* ast) {
  CasesAST* c = ast->getCases();
  auto* scrutinee_name_ast = dynamic_cast<NameAST*>(ast->getScrutinee());
  const std::string* scrutinee_name =
    scrutinee_name_ast != nullptr ? &scrutinee_name_ast->getAsStr() : nullptr;
  StyioDataType inferred_type = ast->getDataType();
  return lower_cases_with_scrutinee(
    this,
    c,
    ast->getScrutinee()->toStyioIR(this),
    scrutinee_name,
    &inferred_type
  );
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(BlockAST* ast) {
  std::vector<StyioIR*> ir_stmts;
  ir_stmts.reserve(ast->stmts.size() + ast->followings.size());
  for (auto* s : ast->stmts) {
    ir_stmts.push_back(s->toStyioIR(this));
  }
  for (auto* following : ast->followings) {
    ir_stmts.push_back(following->toStyioIR(this));
  }
  return styio::lowering::require_default_styio_ir_pass_pipeline(
    SGBlock::Create(std::move(ir_stmts)),
    intermediate_sg_block_pipeline_options());
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(MainBlockAST* ast) {
  styio::resource_topology::validate_or_throw(ast, "lowering-resource-topology");

  std::vector<StyioIR*> ir_stmts;
  int pending_region = -1;
  SGPulsePlan* pending_plan = nullptr;

  func_defs.clear();
  func_defs_by_sid.clear();
  file_resource_bindings_.clear();
  resource_method_body_defs_.clear();
  resource_receiver_expr_bindings_.clear();
  register_imported_callable_definitions();
  for (auto* stmt : ast->getStmts()) {
    if (auto* f = dynamic_cast<FunctionAST*>(stmt)) {
      record_function_def(f->getNameAsStr(), f->func_name->getSymbolId(), f);
      continue;
    }
    if (auto* sf = dynamic_cast<SimpleFuncAST*>(stmt)) {
      record_function_def(sf->func_name->getAsStr(), sf->func_name->getSymbolId(), sf);
      continue;
    }
    if (auto* method = dynamic_cast<ResourceMethodDefAST*>(stmt)) {
      resource_method_body_defs_[method->getFamilyName()][method->getMethodName()] = method;
    }
  }

  for (auto stmt : ast->getStmts()) {
    set_post_pulse_hist_context(pending_region, pending_plan);
    std::string generic_name;
    if (auto* f = dynamic_cast<FunctionAST*>(stmt)) {
      generic_name = f->getNameAsStr();
    }
    else if (auto* sf = dynamic_cast<SimpleFuncAST*>(stmt)) {
      generic_name = sf->func_name->getAsStr();
    }
    if (!generic_name.empty()
        && callable_has_runtime_specializations(generic_name)) {
      const auto specializations = callable_specializations(generic_name);
      for (const auto& specialization : specializations) {
        activate_callable_specialization(specialization);
        try {
          prepare_callable_specialization_body(stmt, specialization);
          ir_stmts.push_back(stmt->toStyioIR(this));
        }
        catch (...) {
          clear_active_callable_specialization();
          throw;
        }
        clear_active_callable_specialization();
      }
      continue;
    }
    if (auto* f = dynamic_cast<FunctionAST*>(stmt)) {
      if (find_function_def(f->func_name->getSymbolId(), f->getNameAsStr()) != f) {
        continue;
      }
    }
    if (auto* sf = dynamic_cast<SimpleFuncAST*>(stmt)) {
      if (find_function_def(sf->func_name->getSymbolId(), sf->func_name->getAsStr()) != sf) {
        continue;
      }
    }
    StyioIR* ir = stmt->toStyioIR(this);
    if (auto* fe = dynamic_cast<SGForEach*>(ir)) {
      if (fe->pulse_plan && fe->pulse_plan->total_bytes > 0) {
        pending_region = fe->pulse_region_id;
        pending_plan = fe->pulse_plan.get();
      }
    }
    else if (auto* fl = dynamic_cast<SIOFileLineIter*>(ir)) {
      if (fl->pulse_plan && fl->pulse_plan->total_bytes > 0) {
        pending_region = fl->pulse_region_id;
        pending_plan = fl->pulse_plan.get();
      }
    }
    else if (auto* sz = dynamic_cast<SIOStreamZip*>(ir)) {
      if (sz->pulse_plan && sz->pulse_plan->total_bytes > 0) {
        pending_region = sz->pulse_region_id;
        pending_plan = sz->pulse_plan.get();
      }
    }
    ir_stmts.push_back(ir);
  }

  std::vector<const ImportedCallableDefinition*> imported_definitions;
  imported_definitions.reserve(imported_callable_definitions().size());
  for (const auto& imported : imported_callable_definitions()) {
    imported_definitions.push_back(&imported);
  }
  std::sort(
    imported_definitions.begin(),
    imported_definitions.end(),
    [](const auto* lhs, const auto* rhs)
    {
      if (lhs->module_id != rhs->module_id) {
        return lhs->module_id < rhs->module_id;
      }
      std::string lhs_name;
      std::string rhs_name;
      if (auto* function =
            dynamic_cast<FunctionAST*>(lhs->definition)) {
        lhs_name = function->getNameAsStr();
      }
      else if (auto* function =
                 dynamic_cast<SimpleFuncAST*>(lhs->definition)) {
        lhs_name = function->func_name->getAsStr();
      }
      if (auto* function =
            dynamic_cast<FunctionAST*>(rhs->definition)) {
        rhs_name = function->getNameAsStr();
      }
      else if (auto* function =
                 dynamic_cast<SimpleFuncAST*>(rhs->definition)) {
        rhs_name = function->func_name->getAsStr();
      }
      return lhs_name < rhs_name;
    });
  for (const auto* imported : imported_definitions) {
    StyioAST* definition = imported->definition;
    std::string name;
    if (auto* function = dynamic_cast<FunctionAST*>(definition)) {
      name = function->getNameAsStr();
    }
    else if (auto* function =
               dynamic_cast<SimpleFuncAST*>(definition)) {
      name = function->func_name->getAsStr();
    }
    if (name.empty()) {
      throw StyioTypeError(
        "imported callable interface contains a non-callable body"
      );
    }
    if (imported->has_scheme) {
      const auto specializations = callable_specializations(name);
      for (const auto& specialization : specializations) {
        activate_callable_specialization(specialization);
        try {
          prepare_callable_specialization_body(
            definition,
            specialization);
          ir_stmts.push_back(definition->toStyioIR(this));
        }
        catch (...) {
          clear_active_callable_specialization();
          throw;
        }
        clear_active_callable_specialization();
      }
      continue;
    }
    if (!imported_concrete_callable_is_reachable(name)) {
      continue;
    }
    ir_stmts.push_back(definition->toStyioIR(this));
  }
  set_post_pulse_hist_context(-1, nullptr);

  return styio::lowering::require_default_styio_ir_pass_pipeline(SGMainEntry::Create(std::move(ir_stmts)));
}
