/*
  AST to StyioIR Lowering Implementation
*/

// [C++ STL]
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioIR/GenIR/GenIR.hpp"
#include "../StyioIR/IOIR/IOIR.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"

namespace {

int
alloc_pulse_region_id() {
  static int n = 0;
  return n++;
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
    return SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
  }
  if (std::holds_alternative<TypeTupleAST*>(ret_type)) {
    return SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
  }
  TypeAST* t = std::get<TypeAST*>(ret_type);
  if (!t || t->getDataType().option == StyioDataTypeOption::Undefined) {
    return SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
  }
  return static_cast<SGType*>(t->toStyioIR(an));
}

SGFuncArg*
param_to_sgarg(ParamAST* p, AstToStyioIRLowerer* an) {
  StyioDataTypeOption opty = p->var_type->getDataType().option;
  SGType* ty = (opty == StyioDataTypeOption::Undefined)
    ? SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64})
    : static_cast<SGType*>(p->var_type->toStyioIR(an));
  return SGFuncArg::Create(p->getName(), ty);
}

SGBlock*
lower_func_body(AstToStyioIRLowerer* an, StyioAST* body) {
  if (!body) {
    return SGBlock::Create({});
  }
  if (auto* blk = dynamic_cast<BlockAST*>(body)) {
    return static_cast<SGBlock*>(blk->toStyioIR(an));
  }
  std::vector<StyioIR*> one;
  one.push_back(body->toStyioIR(an));
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
      an->func_defs[f->getNameAsStr()] = f;
      continue;
    }
    if (auto* sf = dynamic_cast<SimpleFuncAST*>(stmt)) {
      an->func_defs[sf->func_name->getAsStr()] = sf;
    }
  }
}

SGBlock*
lower_func_body_with_local_defs(AstToStyioIRLowerer* an, StyioAST* body) {
  auto saved_defs = an->func_defs;
  register_direct_local_function_defs(an, body);
  SGBlock* lowered = lower_func_body(an, body);
  an->func_defs = std::move(saved_defs);
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
  } catch (const std::exception&) {
    return false;
  }
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
  auto it = an->local_binding_types.find(nm->getAsStr());
  if (it == an->local_binding_types.end()) {
    return std::nullopt;
  }
  return it->second;
}

StyioDataType
matrix_intrinsic_lowered_type(AstToStyioIRLowerer* an, FuncCallAST* call);

StyioDataType
expr_lowered_type(AstToStyioIRLowerer* an, StyioAST* expr) {
  if (auto bound = bound_type_of(an, expr)) {
    return *bound;
  }
  if (auto* attr = dynamic_cast<AttrAST*>(expr)) {
    auto* attr_name = dynamic_cast<NameAST*>(attr->attr);
    StyioDataType body_type = expr_lowered_type(an, attr->body);
    if (attr_name != nullptr) {
      if (attr_name->getAsStr() == "keys" && styio_is_dict_type(body_type)) {
        return styio_make_list_type(styio_dict_key_type_name(body_type));
      }
      if (attr_name->getAsStr() == "values" && styio_is_dict_type(body_type)) {
        return styio_make_list_type(styio_dict_value_type_name(body_type));
      }
    }
  }
  if (auto* call = dynamic_cast<FuncCallAST*>(expr)) {
    if (call->func_callee != nullptr && call->getNameAsStr() == "lines") {
      StyioDataType callee_type = expr_lowered_type(an, call->func_callee);
      if (callee_type.option == StyioDataTypeOption::String) {
        return styio_make_list_type("string");
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
  }
  if (auto* access = dynamic_cast<ListOpAST*>(expr)) {
    StyioDataType base_type = expr_lowered_type(an, access->getList());
    if (styio_is_dict_type(base_type)) {
      return styio_data_type_from_name(styio_dict_value_type_name(base_type));
    }
    if (styio_type_is_indexable(base_type)) {
      return styio_data_type_from_name(styio_type_item_type_name(base_type));
    }
  }
  return expr->getDataType();
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
  auto arg_type = [&](size_t i) {
    return i < call->getArgList().size()
      ? expr_lowered_type(an, call->getArgList()[i])
      : StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  };
  auto elem = [&](const StyioDataType& type) {
    return styio_data_type_from_name(styio_matrix_elem_type_name(type));
  };
  auto merge_elem = [&](const StyioDataType& a, const StyioDataType& b) {
    return (a.option == StyioDataTypeOption::Float || b.option == StyioDataTypeOption::Float)
      ? std::string("f64")
      : std::string("i64");
  };
  auto int_lit = [&](size_t i) -> size_t {
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
      styio_matrix_row_count(m));
  }
  if (name == "mat_add" || name == "mat_sub" || name == "mat_hadamard") {
    StyioDataType a = arg_type(0);
    StyioDataType b = arg_type(1);
    return styio_make_matrix_type(
      merge_elem(elem(a), elem(b)),
      styio_matrix_row_count(a),
      styio_matrix_col_count(a));
  }
  if (name == "mat_scale") {
    StyioDataType m = arg_type(0);
    return styio_make_matrix_type(
      merge_elem(elem(m), arg_type(1)),
      styio_matrix_row_count(m),
      styio_matrix_col_count(m));
  }
  if (name == "matmul") {
    StyioDataType a = arg_type(0);
    StyioDataType b = arg_type(1);
    return styio_make_matrix_type(
      merge_elem(elem(a), elem(b)),
      styio_matrix_row_count(a),
      styio_matrix_col_count(b));
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
  auto arg_type = [&](size_t i) {
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

bool
is_predefined_list_operation_name(const std::string& name) {
  return name == "push" || name == "insert" || name == "pop";
}

bool
is_predefined_string_operation_name(const std::string& name) {
  return name == "lines";
}

std::string
predefined_list_operation_runtime_name(const std::string& method, const StyioDataType& list_type) {
  if (method == "pop") {
    return "__styio_list_pop";
  }

  const std::string elem_name = styio_type_item_type_name(list_type);
  const char* suffix = "i64";
  switch (styio_value_family_from_type_name(elem_name)) {
    case StyioValueFamily::Bool:
      suffix = "bool";
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
    case StyioValueFamily::Integer:
    default:
      break;
  }
  return std::string("__styio_list_") + method + "_" + suffix;
}

void
scan_returns_for_str_int(StyioAST* ast, bool& has_str, bool& has_int) {
  if (!ast) {
    return;
  }
  if (ast->getNodeType() == StyioNodeType::Return) {
    StyioAST* e = static_cast<ReturnAST*>(ast)->getExpr();
    if (e->getNodeType() == StyioNodeType::String) {
      has_str = true;
    }
    else if (e->getNodeType() == StyioNodeType::Integer) {
      has_int = true;
    }
    else {
      has_int = true;
    }
    return;
  }
  if (ast->getNodeType() == StyioNodeType::MatchCases) {
    auto* m = static_cast<MatchCasesAST*>(ast);
    CasesAST* c = m->getCases();
    for (auto const& pr : c->case_list) {
      scan_returns_for_str_int(pr.second, has_str, has_int);
    }
    scan_returns_for_str_int(c->case_default, has_str, has_int);
    return;
  }
  if (auto* b = dynamic_cast<BlockAST*>(ast)) {
    for (auto* s : b->stmts) {
      scan_returns_for_str_int(s, has_str, has_int);
    }
  }
}

SGMatchReprKind
classify_cases(CasesAST* c) {
  bool any = false;
  for (auto const& pr : c->case_list) {
    if (stmt_has_return_tree(pr.second)) {
      any = true;
    }
  }
  if (stmt_has_return_tree(c->case_default)) {
    any = true;
  }
  if (!any) {
    return SGMatchReprKind::Stmt;
  }
  bool hs = false;
  bool hi = false;
  for (auto const& pr : c->case_list) {
    scan_returns_for_str_int(pr.second, hs, hi);
  }
  scan_returns_for_str_int(c->case_default, hs, hi);
  if (hs && hi) {
    return SGMatchReprKind::ExprMixed;
  }
  if (hs) {
    /* All arms yield strings (or only strings detected): phi must be i8* */
    return SGMatchReprKind::ExprMixed;
  }
  return SGMatchReprKind::ExprInt;
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
    auto it = an->func_defs.find(fc->getNameAsStr());
    if (it == an->func_defs.end()) {
      return false;
    }
    if (auto* sf = dynamic_cast<SimpleFuncAST*>(it->second)) {
      StateDeclAST* sd = nullptr;
      if (simple_func_returns_single_state_decl(sf, sd)) {
        return true;
      }
    }
    if (auto* fn = dynamic_cast<FunctionAST*>(it->second)) {
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
find_series_intrinsic(StyioAST* e) {
  if (!e) {
    return nullptr;
  }
  if (auto* s = dynamic_cast<SeriesIntrinsicAST*>(e)) {
    return s;
  }
  if (auto* b = dynamic_cast<BinOpAST*>(e)) {
    auto* L = find_series_intrinsic(b->LHS);
    if (L) {
      return L;
    }
    return find_series_intrinsic(b->RHS);
  }
  if (auto* w = dynamic_cast<WaveMergeAST*>(e)) {
    auto* L = find_series_intrinsic(w->getCond());
    if (L) {
      return L;
    }
    L = find_series_intrinsic(w->getTrueVal());
    if (L) {
      return L;
    }
    return find_series_intrinsic(w->getFalseVal());
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
classify_state_slot(StateDeclAST* sd, SGStateSlotDesc& d) {
  if (sd->getAccName()) {
    d.kind = SGStateSlotKind::Acc;
    d.win_n = 0;
    return;
  }
  auto* si = find_series_intrinsic(sd->getUpdateExpr());
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
    throw StyioTypeError("@[n] header required for non-accum state");
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
  auto* name = NameAST::Create(v->getNameAsStr());
  auto* dtype = clone_type_for_var(v->var_type);
  if (!v->val_init) {
    return VarAST::Create(name, dtype);
  }
  return new VarAST(name, dtype, clone_state_expr(v->val_init));
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
  return NameAST::Create(n->getAsStr());
}

class StateExprCloneVisitor
{
  std::string pname_;
  StyioAST* repl_;

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
    if (repl_ != nullptr && expr->getAsStr() == pname_) {
      return clone_without_subst(repl_);
    }
    return NameAST::Create(expr->getAsStr());
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

  StyioAST* clone(TupleAST* expr) {
    return TupleAST::Create(clone_child_list(expr->getElements()));
  }

  StyioAST* clone(ListAST* expr) {
    return ListAST::Create(clone_child_list(expr->getElements()));
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

  StyioAST* clone(WaveMergeAST* expr) {
    return WaveMergeAST::Create(
      clone(expr->getCond()),
      clone(expr->getTrueVal()),
      clone(expr->getFalseVal()));
  }

  StyioAST* clone(WaveDispatchAST* expr) {
    return WaveDispatchAST::Create(
      clone(expr->getCond()),
      clone(expr->getTrueArm()),
      clone(expr->getFalseArm()));
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

  StyioAST* clone(TypeConvertAST* expr) {
    return TypeConvertAST::Create(clone(expr->getValue()), expr->getPromoTy());
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
    if (expr->func_callee != nullptr) {
      return FuncCallAST::Create(clone(expr->func_callee), NameAST::Create(expr->getNameAsStr()), std::move(args));
    }
    return FuncCallAST::Create(NameAST::Create(expr->getNameAsStr()), std::move(args));
  }

  StyioAST* clone(AttrAST* expr) {
    return AttrAST::Create(clone(expr->body), clone(expr->attr));
  }

  StyioAST* clone(HistoryProbeAST* expr) {
    StyioAST* cloned_target = clone(expr->getTarget());
    if (cloned_target == nullptr || cloned_target->getNodeType() != StyioNodeType::StateRef) {
      throw StyioTypeError("history probe target must remain state reference");
    }
    return HistoryProbeAST::Create(static_cast<StateRefAST*>(cloned_target), clone(expr->getDepth()));
  }

  StyioAST* clone(StateRefAST* expr) {
    return StateRefAST::Create(NameAST::Create(expr->getNameStr()));
  }

  StyioAST* clone(SeriesIntrinsicAST* expr) {
    return SeriesIntrinsicAST::Create(clone(expr->getBase()), expr->getOp(), clone(expr->getWindow()));
  }

  StyioAST* clone(ReturnAST* expr) {
    return ReturnAST::Create(clone(expr->getExpr()));
  }

  StyioAST* clone(PrintAST* expr) {
    return PrintAST::Create(clone_child_list(expr->exprs));
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
    return MatchCasesAST::make(clone(expr->getScrutinee()), static_cast<CasesAST*>(cloned_cases));
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
    return ContinueAST::Create(expr->getDepth());
  }

  StyioAST* clone(BlockAST* expr) {
    std::vector<StyioAST*> stmts = clone_child_list(expr->stmts);
    std::vector<StyioAST*> followings = clone_child_list(expr->followings);
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
  }

  StyioAST* clone(StyioAST* expr) {
    if (expr == nullptr) {
      return nullptr;
    }

    switch (expr->getNodeType()) {
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
      case StyioNodeType::Tuple:
        return clone(static_cast<TupleAST*>(expr));
      case StyioNodeType::List:
        return clone(static_cast<ListAST*>(expr));
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
      case StyioNodeType::NumConvert:
        return clone(static_cast<TypeConvertAST*>(expr));
      case StyioNodeType::Access:
      case StyioNodeType::Access_By_Index:
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
      case StyioNodeType::HistoryProbe:
        return clone(static_cast<HistoryProbeAST*>(expr));
      case StyioNodeType::StateRef:
        return clone(static_cast<StateRefAST*>(expr));
      case StyioNodeType::SeriesIntrinsic:
        return clone(static_cast<SeriesIntrinsicAST*>(expr));
      case StyioNodeType::Return:
        return clone(static_cast<ReturnAST*>(expr));
      case StyioNodeType::Print:
        return clone(static_cast<PrintAST*>(expr));
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
      + std::to_string(static_cast<int>(expr->getNodeType())));
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

struct PulseScratch {
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
  auto it = an->func_defs.find(fc->getNameAsStr());
  if (it == an->func_defs.end()) {
    throw StyioTypeError("unknown function in pulse body");
  }

  const std::vector<ParamAST*>* params = nullptr;
  StyioAST* body = nullptr;

  if (auto* sf = dynamic_cast<SimpleFuncAST*>(it->second)) {
    params = &sf->params;
    body = sf->ret_expr;
  }
  else if (auto* fn = dynamic_cast<FunctionAST*>(it->second)) {
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
    new_rhs);
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
    classify_state_slot(sd, d);
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
    if (fc->getNameAsStr() == "get_ma" && fc->getArgList().size() == 2) {
      auto it = an->func_defs.find("get_ma");
      if (it != an->func_defs.end()) {
        if (auto* sf = dynamic_cast<SimpleFuncAST*>(it->second)) {
          if (sf->params.size() == 2) {
            auto* body = dynamic_cast<SeriesIntrinsicAST*>(sf->ret_expr);
            if (body && body->getOp() == SeriesIntrinsicOp::Avg) {
              an->set_active_series_slot(slot_id);
              StyioIR* xi = fc->getArgList()[0]->toStyioIR(an);
              an->set_active_series_slot(-1);
              return SGSeriesAvgStep::Create(slot_id, xi);
            }
          }
        }
      }
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
    rhs);
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
      return StyioOpType::Equal;
  }
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(CommentAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(NoneAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(EmptyAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(NameAST* ast) {
  auto it = binding_info_.find(ast->getAsStr());
  if (it != binding_info_.end()
      && (it->second.dynamic_slot
          || it->second.value_kind == BindingValueKind::ListHandle
          || it->second.value_kind == BindingValueKind::DictHandle)) {
    switch (it->second.value_kind) {
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
      default:
        throw StyioTypeError("cannot lower dynamic slot `" + ast->getAsStr() + "` with unknown runtime kind");
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
  return SGConstInt::Create(0);
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
  return SGConstInt::Create(0);
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
AstToStyioIRLowerer::toStyioIR(TypeConvertAST*) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(VarAST* ast) {
  if (ast->val_init) {
    return SGVar::Create(
      static_cast<SGResId*>(ast->var_name->toStyioIR(this)),
      static_cast<SGType*>(ast->var_type->toStyioIR(this)),
      ast->val_init->toStyioIR(this)
    );
  }
  else {
    return SGVar::Create(
      static_cast<SGResId*>(ast->var_name->toStyioIR(this)),
      static_cast<SGType*>(ast->var_type->toStyioIR(this))
    );
  }
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ParamAST* ast) {
  if (ast->val_init) {
    return SGVar::Create(
      static_cast<SGResId*>(ast->var_name->toStyioIR(this)),
      static_cast<SGType*>(ast->var_type->toStyioIR(this)),
      ast->val_init->toStyioIR(this)
    );
  }
  else {
    return SGVar::Create(
      static_cast<SGResId*>(ast->var_name->toStyioIR(this)),
      static_cast<SGType*>(ast->var_type->toStyioIR(this))
    );
  }
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(OptArgAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(OptKwArgAST* ast) {
  return SGConstInt::Create(0);
}

/*
  The declared type is always the *top* priority
  because the programmer wrote in that way!
*/
StyioIR*
AstToStyioIRLowerer::toStyioIR(FlexBindAST* ast) {
  auto* var = static_cast<SGVar*>(ast->getVar()->toStyioIR(this));
  auto it = binding_info_.find(ast->getNameAsStr());
  if (it != binding_info_.end()) {
    var->is_dynamic_slot = it->second.dynamic_slot
      || it->second.value_kind == BindingValueKind::ListHandle
      || it->second.value_kind == BindingValueKind::DictHandle;
    var->is_list_slot = !it->second.dynamic_slot
      && it->second.value_kind == BindingValueKind::ListHandle;
  }
  return SGFlexBind::Create(var, ast->getValue()->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(FinalBindAST* ast) {
  auto* var = static_cast<SGVar*>(ast->getVar()->toStyioIR(this));
  auto it = binding_info_.find(ast->getVar()->getNameAsStr());
  if (it != binding_info_.end()) {
    var->is_dynamic_slot = it->second.dynamic_slot
      || it->second.value_kind == BindingValueKind::ListHandle
      || it->second.value_kind == BindingValueKind::DictHandle;
    var->is_list_slot = !it->second.dynamic_slot
      && it->second.value_kind == BindingValueKind::ListHandle;
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
    VarAST* tmp_var = VarAST::Create(NameAST::Create(tmp_name));
    auto* sg_var = static_cast<SGVar*>(tmp_var->toStyioIR(this));
    stmts.push_back(SGFinalBind::Create(sg_var, rhs->toStyioIR(this)));
  }

  for (size_t i = 0; i < ast->getLHS().size(); ++i) {
    StyioIR* rhs_val = SGResId::Create(tmp_names[i]);
    if (auto* nm = dynamic_cast<NameAST*>(ast->getLHS()[i])) {
      VarAST* lhs_var = VarAST::Create(NameAST::Create(nm->getAsStr()));
      auto* sg_var = static_cast<SGVar*>(lhs_var->toStyioIR(this));
      auto it = binding_info_.find(nm->getAsStr());
      if (it != binding_info_.end()) {
        sg_var->is_dynamic_slot = it->second.dynamic_slot
          || it->second.value_kind == BindingValueKind::ListHandle
          || it->second.value_kind == BindingValueKind::DictHandle;
        sg_var->is_list_slot = !it->second.dynamic_slot
          && it->second.value_kind == BindingValueKind::ListHandle;
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
        styio_dict_value_type_name(base_type)));
    }
    else {
      stmts.push_back(SCListSet::Create(
        idx->getList()->toStyioIR(this),
        idx->getSlot1()->toStyioIR(this),
        rhs_val,
        styio_type_item_type_name(base_type)));
    }
  }

  return SGBlock::Create(std::move(stmts));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(InfiniteAST* ast) {
  return SGConstInt::Create(0);
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
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(VarTupleAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ExtractorAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(RangeAST* ast) {
  std::int64_t start = 0;
  std::int64_t end = 0;
  std::int64_t step = 0;

  if (!try_parse_int_literal_value(ast->getStart(), start)
      || !try_parse_int_literal_value(ast->getEnd(), end)
      || !try_parse_int_literal_value(ast->getStep(), step)) {
    throw StyioTypeError("range literal lowering currently requires integer literal bounds");
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
  return SGConstInt::Create(0);
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
      styio_matrix_col_count(list_type));
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
      entry.value->toStyioIR(this)});
  }
  StyioDataType dict_type = expr_lowered_type(this, ast);
  return SCDictLiteral::Create(
    std::move(entries),
    styio_dict_value_type_name(dict_type));
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
  if (ast->getOp() == StyioNodeType::Access_By_Index) {
    if (auto* row_access = dynamic_cast<ListOpAST*>(ast->getList())) {
      if (row_access->getOp() == StyioNodeType::Access_By_Index) {
        StyioDataType matrix_type = expr_lowered_type(this, row_access->getList());
        if (styio_is_matrix_type(matrix_type)) {
          return SCMatrixGet::Create(
            row_access->getList()->toStyioIR(this),
            row_access->getSlot1()->toStyioIR(this),
            ast->getSlot1()->toStyioIR(this),
            styio_matrix_elem_type_name(matrix_type));
        }
      }
    }
  }
  StyioDataType base_type = expr_lowered_type(this, ast->getList());
  if (styio_is_matrix_type(base_type) && ast->getOp() == StyioNodeType::Access_By_Index) {
    return SCMatrixRow::Create(
      ast->getList()->toStyioIR(this),
      ast->getSlot1()->toStyioIR(this),
      styio_matrix_elem_type_name(base_type));
  }
  if (styio_is_dict_type(base_type)
      && (ast->getOp() == StyioNodeType::Access_By_Index
          || ast->getOp() == StyioNodeType::Access_By_Name)) {
    return SCDictGet::Create(
      ast->getList()->toStyioIR(this),
      ast->getSlot1()->toStyioIR(this),
      styio_dict_value_type_name(base_type));
  }
  if (ast->getOp() == StyioNodeType::Access_By_Index) {
    return SCListGet::Create(
      ast->getList()->toStyioIR(this),
      ast->getSlot1()->toStyioIR(this),
      styio_type_item_type_name(base_type));
  }
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(BinCompAST* ast) {
  StyioOpType op = comp_type_to_op(ast->getSign());
  return SGBinOp::Create(
    ast->getLHS()->toStyioIR(this),
    ast->getRHS()->toStyioIR(this),
    op,
    SGType::Create(StyioDataType{StyioDataTypeOption::Bool, "bool", 1}));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(CondAST* ast) {
  switch (ast->getSign()) {
    case LogicType::AND:
      return SGCond::Create(
        ast->getLHS()->toStyioIR(this),
        ast->getRHS()->toStyioIR(this),
        StyioOpType::Logic_AND);
    case LogicType::OR:
      return SGCond::Create(
        ast->getLHS()->toStyioIR(this),
        ast->getRHS()->toStyioIR(this),
        StyioOpType::Logic_OR);
    case LogicType::RAW:
      return ast->getValue()->toStyioIR(this);
    default:
      return ast->getValue() ? ast->getValue()->toStyioIR(this) : SGConstInt::Create(0);
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
    ast->getFalseVal()->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(WaveDispatchAST* ast) {
  return SGWaveDispatch::Create(
    ast->getCond()->toStyioIR(this),
    ast->getTrueArm()->toStyioIR(this),
    ast->getFalseArm()->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(FallbackAST* ast) {
  return SGFallback::Create(
    ast->getPrimary()->toStyioIR(this),
    ast->getAlternate()->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(GuardSelectorAST* ast) {
  return SGGuardSelect::Create(
    ast->getBase()->toStyioIR(this),
    ast->getCond()->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(EqProbeAST* ast) {
  return SGEqProbe::Create(
    ast->getBase()->toStyioIR(this),
    ast->getProbeValue()->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(FileResourceAST* ast) {
  return ast->getPath()->toStyioIR(this);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(StdStreamAST* ast) {
  /* StdStreamAST is not lowered to its own IR node;
     it is consumed by the parent node (ResourceWriteAST, IteratorAST, etc.). */
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(HandleAcquireAST* ast) {
  if (collect_bind_handle_acquires_.count(ast) != 0) {
    StyioDataType collected_type = styio_make_list_type("string");
    auto type_it = collect_bind_handle_acquire_types_.find(ast);
    if (type_it != collect_bind_handle_acquire_types_.end()) {
      collected_type = type_it->second;
    }
    auto* var = SGVar::Create(
      SGResId::Create(ast->getVar()->getNameAsStr()),
      SGType::Create(collected_type));
    auto bit = binding_info_.find(ast->getVar()->getNameAsStr());
    if (bit != binding_info_.end()) {
      var->is_dynamic_slot = bit->second.dynamic_slot
        || bit->second.value_kind == BindingValueKind::ListHandle;
      var->is_list_slot = !bit->second.dynamic_slot
        && bit->second.value_kind == BindingValueKind::ListHandle;
    }
    return SGFlexBind::Create(
      var,
      SIOListReadStdin::Create(styio_type_item_type_name(collected_type)));
  }
  if (dynamic_cast<TypedStdinListAST*>(ast->getResource())
      || dynamic_cast<NameAST*>(ast->getResource())) {
    auto* var = static_cast<SGVar*>(ast->getVar()->toStyioIR(this));
    auto it = binding_info_.find(ast->getVar()->getNameAsStr());
    if (it != binding_info_.end()) {
      var->is_dynamic_slot = it->second.dynamic_slot
        || it->second.value_kind == BindingValueKind::ListHandle
        || it->second.value_kind == BindingValueKind::DictHandle;
      var->is_list_slot = !it->second.dynamic_slot
        && it->second.value_kind == BindingValueKind::ListHandle;
    }

    StyioIR* rhs = nullptr;
    if (auto* typed = dynamic_cast<TypedStdinListAST*>(ast->getResource())) {
      rhs = typed->toStyioIR(this);
    }
    else {
      auto src_type = bound_type_of(this, ast->getResource());
      if (src_type.has_value() && styio_is_dict_type(*src_type)) {
        rhs = SCDictClone::Create(ast->getResource()->toStyioIR(this));
      }
      else {
        rhs = SCListClone::Create(ast->getResource()->toStyioIR(this));
      }
    }

    if (ast->isFlexBind()) {
      return SGFlexBind::Create(var, rhs);
    }
    return SGFinalBind::Create(var, rhs);
  }

  if (dynamic_cast<StdStreamAST*>(ast->getResource())) {
    /* Standard stream aliases are compile-time handles in v1; lowering happens at the use site. */
    return SGConstInt::Create(0);
  }
  auto* fr = dynamic_cast<FileResourceAST*>(ast->getResource());
  if (!fr) {
    throw StyioTypeError("handle acquire needs @file{...} or @{...}");
  }
  return SIOHandleAcquire::Create(
    ast->getVar()->getNameAsStr(),
    fr->getPath()->toStyioIR(this),
    fr->isAutoDetect());
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
      SGType::Create(collected_type));
    auto bit = binding_info_.find(target_name->getAsStr());
    if (bit != binding_info_.end()) {
      var->is_dynamic_slot = bit->second.dynamic_slot
        || bit->second.value_kind == BindingValueKind::ListHandle;
      var->is_list_slot = !bit->second.dynamic_slot
        && bit->second.value_kind == BindingValueKind::ListHandle;
    }
    return SGFlexBind::Create(
      var,
      SIOListReadStdin::Create(styio_type_item_type_name(collected_type)));
  }
  StyioIR* data_ir = ast->getData()->toStyioIR(this);
  if (expr_is_list_like(this, ast->getData())) {
    data_ir = SCListToString::Create(data_ir);
  }
  else if (expr_is_dict_like(this, ast->getData())) {
    data_ir = SCDictToString::Create(data_ir);
  }
  /* M9: check for standard stream target. */
  auto* ss = dynamic_cast<StdStreamAST*>(ast->getResource());
  if (ss) {
    /* M10: reject write to @stdin. */
    if (ss->getStreamKind() == StdStreamKind::Stdin) {
      throw StyioTypeError("@stdin is a read-only stream; cannot write to it");
    }
    auto stream = (ss->getStreamKind() == StdStreamKind::Stdout)
      ? SIOStdStreamWrite::Stream::Stdout
      : SIOStdStreamWrite::Stream::Stderr;
    return SIOStdStreamWrite::Create(stream, {data_ir});
  }
  auto* fr = dynamic_cast<FileResourceAST*>(ast->getResource());
  if (!fr) {
    throw StyioTypeError("<< target must be a file or standard stream resource");
  }
  StyioDataType dt = ast->getData()->getDataType();
  bool is_str = dt.option == StyioDataTypeOption::String
    || ast->getData()->getNodeType() == StyioNodeType::String;
  bool prom = !is_str;
  return SIOResourceWriteToFile::Create(
    data_ir,
    fr->getPath()->toStyioIR(this),
    fr->isAutoDetect(),
    prom,
    prom);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ResourceRedirectAST* ast) {
  StyioIR* data_ir = ast->getData()->toStyioIR(this);
  if (expr_is_list_like(this, ast->getData())) {
    data_ir = SCListToString::Create(data_ir);
  }
  else if (expr_is_dict_like(this, ast->getData())) {
    data_ir = SCDictToString::Create(data_ir);
  }
  /* M9: redirect to standard stream → SIOStdStreamWrite */
  auto* ss = dynamic_cast<StdStreamAST*>(ast->getResource());
  if (ss) {
    if (ss->getStreamKind() == StdStreamKind::Stdin) {
      throw StyioTypeError("@stdin is a read-only stream; cannot redirect to it");
    }
    auto stream = (ss->getStreamKind() == StdStreamKind::Stdout)
      ? SIOStdStreamWrite::Stream::Stdout
      : SIOStdStreamWrite::Stream::Stderr;
    return SIOStdStreamWrite::Create(stream, {data_ir});
  }
  auto* fr = dynamic_cast<FileResourceAST*>(ast->getResource());
  if (!fr) {
    throw StyioTypeError("-> target must be a file or standard stream resource");
  }
  return SIOResourceWriteToFile::Create(
    data_ir,
    fr->getPath()->toStyioIR(this),
    fr->isAutoDetect(),
    true,
    false);
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
    expr_lowered_type(this, ast->RHS));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(FmtStrAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ResourceAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ResPathAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(RemotePathAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(WebUrlAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(DBUrlAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ExtPackAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ExportDeclAST* ast) {
  return SGExportDecl::Create(ast->getSymbols());
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ExternBlockAST* ast) {
  return SGExternBlock::Create(ast->getAbi(), ast->getBody());
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ReadFileAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(EOFAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(BreakAST* ast) {
  (void)ast;
  return SGBreak::Create(1u);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ContinueAST* ast) {
  return SGContinue::Create(ast->getDepth());
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(PassAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ReturnAST* ast) {
  return SGReturn::Create(ast->getExpr()->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(FuncCallAST* ast) {
  if (ast->func_callee != nullptr && is_predefined_list_operation_name(ast->getNameAsStr())) {
    std::vector<StyioIR*> args;
    args.reserve(ast->getArgList().size() + 1);
    args.push_back(ast->func_callee->toStyioIR(this));
    for (auto* a : ast->getArgList()) {
      args.push_back(a->toStyioIR(this));
    }
    return SGCall::Create(
      SGResId::Create(predefined_list_operation_runtime_name(
        ast->getNameAsStr(),
        expr_lowered_type(this, ast->func_callee))),
      std::move(args));
  }

  if (ast->func_callee != nullptr && is_predefined_string_operation_name(ast->getNameAsStr())) {
    if (!ast->getArgList().empty()) {
      throw StyioTypeError("string.lines() does not take arguments");
    }
    return SGCall::Create(
      SGResId::Create("__styio_string_lines"),
      {ast->func_callee->toStyioIR(this)});
  }

  if (ast->isCallableApply()) {
    throw StyioTypeError(
      "one-shot continuation resume `<|` requires continuation lowering; "
      "captured continuations must be resumed or discontinued exactly once");
  }

  if (ast->func_callee == nullptr && is_matrix_intrinsic_name(ast->getNameAsStr())) {
    std::vector<StyioIR*> args;
    for (auto* a : ast->getArgList()) {
      args.push_back(a->toStyioIR(this));
    }
    return SGCall::Create(
      SGResId::Create(matrix_intrinsic_runtime_name(this, ast)),
      std::move(args));
  }

  auto def_it = func_defs.find(ast->getNameAsStr());
  if (def_it == func_defs.end()) {
    if (native_func_defs.find(ast->getNameAsStr()) != native_func_defs.end()) {
      std::vector<StyioIR*> args;
      for (auto* a : ast->getArgList()) {
        args.push_back(a->toStyioIR(this));
      }
      return SGCall::Create(
        SGResId::Create(ast->getNameAsStr()),
        std::move(args));
    }
    throw StyioTypeError("unknown function `" + ast->getNameAsStr() + "`");
  }
  auto params = params_of_func_def(def_it->second);
  if (params.size() != ast->getArgList().size()) {
    throw StyioTypeError(
      "function `" + ast->getNameAsStr() + "` expects "
      + std::to_string(params.size()) + " argument(s), got "
      + std::to_string(ast->getArgList().size()));
  }

  std::vector<StyioIR*> args;
  for (auto* a : ast->getArgList()) {
    args.push_back(a->toStyioIR(this));
  }
  return SGCall::Create(
    SGResId::Create(ast->getNameAsStr()),
    std::move(args));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(AttrAST* ast) {
  auto* attr_name = dynamic_cast<NameAST*>(ast->attr);
  if (attr_name == nullptr) {
    return SGConstInt::Create(0);
  }
  StyioDataType body_type = ast->body->getDataType();
  body_type = expr_lowered_type(this, ast->body);
  if (attr_name->getAsStr() == "keys") {
    return SCDictKeys::Create(ast->body->toStyioIR(this));
  }
  if (attr_name->getAsStr() == "values") {
    return SCDictValues::Create(
      ast->body->toStyioIR(this),
      styio_dict_value_type_name(body_type));
  }
  if (styio_is_dict_type(body_type)) {
    return SCDictLen::Create(ast->body->toStyioIR(this));
  }
  return SCListLen::Create(ast->body->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(PrintAST* ast) {
  /* M9 Task 9.10: unify >_() to SIOStdStreamWrite(Stdout). */
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
                : lowered)));
  }
  return SIOStdStreamWrite::Create(SIOStdStreamWrite::Stream::Stdout, parts);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(ForwardAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(BackwardAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(CODPAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(CheckEqualAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(CheckIsinAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(HashTagNameAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(CondFlowAST* ast) {
  SGBlock* then_block = lower_func_body(this, ast->getThen());
  SGBlock* else_block = ast->getElse() ? lower_func_body(this, ast->getElse()) : nullptr;
  return SGIf::Create(ast->getCond()->toStyioIR(this), then_block, else_block);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(AnonyFuncAST* ast) {
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(FunctionAST* ast) {
  int saved_hist_r = post_pulse_hist_region_;
  SGPulsePlan* saved_hist_p = post_pulse_hist_plan_;
  set_post_pulse_hist_context(-1, nullptr);
  std::vector<SGFuncArg*> fargs;
  for (auto* p : ast->params) {
    fargs.push_back(param_to_sgarg(p, this));
  }
  SGType* rt = func_ret_to_sgtype(ast->ret_type, this);
  if (auto* blk = dynamic_cast<BlockAST*>(ast->func_body)) {
    if (blk->stmts.size() == 1 && blk->stmts[0]->getNodeType() == StyioNodeType::MatchCases) {
      auto* mc = static_cast<MatchCasesAST*>(blk->stmts[0]);
      CasesAST* c = mc->getCases();
      bool hs = false;
      bool hi = false;
      for (auto const& pr : c->case_list) {
        scan_returns_for_str_int(pr.second, hs, hi);
      }
      scan_returns_for_str_int(c->case_default, hs, hi);
      if (hs) {
        rt = SGType::Create(StyioDataType{StyioDataTypeOption::String, "string", 0});
      }
    }
  }
  SGBlock* body = lower_func_body_with_local_defs(this, ast->func_body);
  SGFunc* fn = SGFunc::Create(
    rt,
    SGResId::Create(ast->getNameAsStr()),
    std::move(fargs),
    body);
  set_post_pulse_hist_context(saved_hist_r, saved_hist_p);
  return fn;
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(SimpleFuncAST* ast) {
  int saved_hist_r = post_pulse_hist_region_;
  SGPulsePlan* saved_hist_p = post_pulse_hist_plan_;
  set_post_pulse_hist_context(-1, nullptr);
  std::vector<SGFuncArg*> fargs;
  for (auto* p : ast->params) {
    fargs.push_back(param_to_sgarg(p, this));
  }
  SGType* rt = func_ret_to_sgtype(ast->ret_type, this);
  if (auto* blk = dynamic_cast<BlockAST*>(ast->ret_expr)) {
    if (blk->stmts.size() == 1 && blk->stmts[0]->getNodeType() == StyioNodeType::MatchCases) {
      auto* mc = static_cast<MatchCasesAST*>(blk->stmts[0]);
      CasesAST* c = mc->getCases();
      bool hs = false;
      bool hi = false;
      for (auto const& pr : c->case_list) {
        scan_returns_for_str_int(pr.second, hs, hi);
      }
      scan_returns_for_str_int(c->case_default, hs, hi);
      if (hs) {
        /* Any <| "..." arm: LLVM return must be i8* (and may mix with snprintf ints). */
        rt = SGType::Create(StyioDataType{StyioDataTypeOption::String, "string", 0});
      }
    }
  }
  SGBlock* body = lower_func_body_with_local_defs(this, ast->ret_expr);
  SGFunc* fn = SGFunc::Create(
    rt,
    SGResId::Create(ast->func_name->getAsStr()),
    std::move(fargs),
    body);
  set_post_pulse_hist_context(saved_hist_r, saved_hist_p);
  return fn;
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(IteratorAST* ast) {
  std::string vname = "it";
  if (!ast->params.empty()) {
    vname = ast->params[0]->getName();
  }
  auto bind_iter_param = [&](const std::string& name, const StyioDataType& type) {
    local_binding_types[name] = type;
  };
  auto saved_locals = local_binding_types;
  auto saved_bind = binding_info_;
  if (!ast->params.empty()) {
    bind_iter_param(
      vname,
      styio_data_type_from_name(
        styio_type_item_type_name(expr_lowered_type(this, ast->collection))));
  }
  SGBlock* body = SGBlock::Create({});
  std::unique_ptr<SGPulsePlan> pplan;
  if (!ast->following.empty()) {
    auto* abody = dynamic_cast<BlockAST*>(ast->following[0]);
    if (abody && pulse_block_has_state(this, abody)) {
      PulseScratch scratch;
      std::unordered_map<StyioAST*, StateDeclAST*> cache;
      pplan = build_pulse_plan(this, abody, &scratch, cache);
      body = lower_pulse_body(this, abody, pplan.get(), &scratch, cache);
    }
    else {
      body = lower_func_body(this, ast->following[0]);
    }
  }
  local_binding_types = std::move(saved_locals);
  binding_info_ = std::move(saved_bind);
  if (ast->collection->getNodeType() == StyioNodeType::Range) {
    auto* rg = static_cast<RangeAST*>(ast->collection);
    return SGRangeFor::Create(
      rg->getStart()->toStyioIR(this),
      rg->getEnd()->toStyioIR(this),
      rg->getStep()->toStyioIR(this),
      std::move(vname),
      body);
  }
  /* M10: stdin line iteration. */
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
    auto* sl = SIOStdStreamLineIter::Create(std::move(vname), body);
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
      body);
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
    auto it = local_binding_types.find(nm->getAsStr());
    if (it != local_binding_types.end()) {
      if (auto kind = std_stream_kind_of(it->second)) {
        if (*kind == StdStreamKind::Stdout) {
          throw StyioTypeError("@stdout is a write-only stream; cannot iterate over it");
        }
        if (*kind == StdStreamKind::Stderr) {
          throw StyioTypeError("@stderr is a write-only stream; cannot iterate over it");
        }
        auto* sl = SIOStdStreamLineIter::Create(std::move(vname), body);
        if (pplan) {
          sl->set_pulse_plan(std::move(pplan));
          if (sl->pulse_plan && sl->pulse_plan->total_bytes > 0) {
            sl->pulse_region_id = alloc_pulse_region_id();
          }
        }
        return sl;
      }
      if (it->second.handle_family == StyioHandleFamily::File) {
        auto* fl = SIOFileLineIter::CreateFromHandle(
          nm->getAsStr(),
          std::move(vname),
          body);
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
    body);
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
  auto bind_zip_param = [&](const std::string& name, const StyioDataType& type) {
    local_binding_types[name] = type;
  };
  auto saved_locals = local_binding_types;
  auto saved_bind = binding_info_;
  if (!ast->getParamsA().empty()) {
    bind_zip_param(
      va,
      styio_data_type_from_name(
        styio_type_item_type_name(expr_lowered_type(this, ast->getCollectionA()))));
  }
  if (!ast->getParamsB().empty()) {
    bind_zip_param(
      vb,
      styio_data_type_from_name(
        styio_type_item_type_name(expr_lowered_type(this, ast->getCollectionB()))));
  }
  SGBlock* body = SGBlock::Create({});
  std::unique_ptr<SGPulsePlan> pplan;
  if (!ast->getFollowing().empty()) {
    auto* abody = dynamic_cast<BlockAST*>(ast->getFollowing()[0]);
    if (abody && pulse_block_has_state(this, abody)) {
      PulseScratch scratch;
      std::unordered_map<StyioAST*, StateDeclAST*> cache;
      pplan = build_pulse_plan(this, abody, &scratch, cache);
      body = lower_pulse_body(this, abody, pplan.get(), &scratch, cache);
    }
    else {
      body = lower_func_body(this, ast->getFollowing()[0]);
    }
  }
  local_binding_types = std::move(saved_locals);
  binding_info_ = std::move(saved_bind);
  StyioAST* ca = ast->getCollectionA();
  StyioAST* cb = ast->getCollectionB();
  bool fa = false;
  bool fb = false;
  StyioIR* ia = nullptr;
  StyioIR* ib = nullptr;
  if (ca->getNodeType() == StyioNodeType::FileResource) {
    fa = true;
    ia = static_cast<FileResourceAST*>(ca)->getPath()->toStyioIR(this);
  }
  else {
    ia = ca->toStyioIR(this);
  }
  if (cb->getNodeType() == StyioNodeType::FileResource) {
    fb = true;
    ib = static_cast<FileResourceAST*>(cb)->getPath()->toStyioIR(this);
  }
  else {
    ib = cb->toStyioIR(this);
  }
  bool astr = collection_elem_is_string(this, ca);
  bool bstr = collection_elem_is_string(this, cb);
  auto* z = SIOStreamZip::Create(ia, fa, std::move(va), ib, fb, std::move(vb), astr, bstr, body);
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
  /* M10: stdin instant pull. */
  auto* ss = dynamic_cast<StdStreamAST*>(ast->getResource());
  if (ss) {
    if (ss->getStreamKind() == StdStreamKind::Stdout) {
      throw StyioTypeError("@stdout is a write-only stream; cannot read from it");
    }
    if (ss->getStreamKind() == StdStreamKind::Stderr) {
      throw StyioTypeError("@stderr is a write-only stream; cannot read from it");
    }
    return SIOStdStreamPull::Create();
  }
  auto* fr = dynamic_cast<FileResourceAST*>(ast->getResource());
  if (!fr) {
    throw StyioTypeError("instant pull needs @file{...}, @{...}, or @stdin");
  }
  return SIOInstantPull::Create(fr->getPath()->toStyioIR(this));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(TypedStdinListAST* ast) {
  return SIOListReadStdin::Create(styio_list_elem_type_name(ast->getDataType()));
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(IterSeqAST* ast) {
  return SGConstInt::Create(0);
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
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(StateDeclAST* ast) {
  (void)ast;
  return SGConstInt::Create(0);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(StateRefAST* ast) {
  if (is_snapshot_var(ast->getNameStr())) {
    return SGSnapshotShadowLoad::Create(ast->getNameStr());
  }
  auto* pl = cur_pulse_plan();
  if (!pl) {
    throw StyioTypeError("$state only valid inside pulse body");
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
        "history probe only valid inside pulse body or after foreach/file line iter with pulse");
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
  SGMatchReprKind rk = classify_cases(c);
  StyioIR* scr = ast->getScrutinee()->toStyioIR(this);
  std::vector<std::pair<std::int64_t, SGBlock*>> arms;
  for (auto const& pr : c->case_list) {
    auto* li = dynamic_cast<IntAST*>(pr.first);
    if (!li) {
      throw StyioTypeError("match arms need integer literal patterns in this milestone");
    }
    arms.push_back({std::stoll(li->value), lower_func_body(this, pr.second)});
  }
  SGBlock* def = nullptr;
  if (c->case_default) {
    def = lower_func_body(this, c->case_default);
  }
  return SGMatch::Create(scr, std::move(arms), def, rk);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(BlockAST* ast) {
  std::vector<StyioIR*> ir_stmts;
  for (auto* s : ast->stmts) {
    ir_stmts.push_back(s->toStyioIR(this));
  }
  return SGBlock::Create(ir_stmts);
}

StyioIR*
AstToStyioIRLowerer::toStyioIR(MainBlockAST* ast) {
  std::vector<StyioIR*> ir_stmts;
  int pending_region = -1;
  SGPulsePlan* pending_plan = nullptr;

  func_defs.clear();
  for (auto* stmt : ast->getStmts()) {
    if (auto* f = dynamic_cast<FunctionAST*>(stmt)) {
      func_defs[f->getNameAsStr()] = f;
      continue;
    }
    if (auto* sf = dynamic_cast<SimpleFuncAST*>(stmt)) {
      func_defs[sf->func_name->getAsStr()] = sf;
    }
  }

  for (auto stmt : ast->getStmts()) {
    set_post_pulse_hist_context(pending_region, pending_plan);
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
  set_post_pulse_hist_context(-1, nullptr);

  return SGMainEntry::Create(ir_stmts);
}
