/*
  Type Inference Implementation

  - Label Types
  - Find Recursive Type
*/

// [C++ STL]
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioNative/NativeInterop.hpp"
#include "../StyioToken/Token.hpp"

static std::vector<ParamAST*>
params_of_func_def(StyioAST* def) {
  if (auto* f = dynamic_cast<FunctionAST*>(def)) {
    return f->params;
  }
  if (auto* s = dynamic_cast<SimpleFuncAST*>(def)) {
    return s->params;
  }
  return {};
}

namespace {

StyioDataType const kBoolType{
  StyioDataTypeOption::Bool, "bool", 1};

StyioDataType const kI64Type{
  StyioDataTypeOption::Integer, "i64", 64};

StyioDataType const kF64Type{
  StyioDataTypeOption::Float, "f64", 64};

StyioDataType const kStringType{
  StyioDataTypeOption::String, "string", 0};

StyioDataType
infer_expr_type(StyioSemaContext* an, StyioAST* expr);

StyioDataType
infer_list_literal_type(StyioSemaContext* an, ListAST* list) {
  auto const& els = list->getElements();
  if (els.empty()) {
    return styio_make_list_type("i64");
  }

  StyioDataType elem_type = infer_expr_type(an, els[0]);
  if (elem_type.isUndefined()) {
    elem_type = kI64Type;
  }

  for (size_t i = 1; i < els.size(); ++i) {
    StyioDataType next_type = infer_expr_type(an, els[i]);
    if (next_type.isUndefined()) {
      continue;
    }
    if (!next_type.equals(elem_type)) {
      elem_type = kI64Type;
      break;
    }
  }

  return styio_make_list_type(elem_type.name);
}

struct MatrixLiteralInfo
{
  StyioDataType elem_type{StyioDataTypeOption::Integer, "i64", 64};
  size_t rows = 0;
  size_t cols = 0;
};

bool
type_is_numeric_family(const StyioDataType& type) {
  StyioValueFamily family = styio_value_family_for_type(type);
  return family == StyioValueFamily::Integer
    || family == StyioValueFamily::Float;
}

StyioDataType
merge_matrix_elem_types(StyioDataType current, StyioDataType next) {
  if (current.isUndefined()) {
    return next;
  }
  if (next.isUndefined()) {
    return current;
  }
  if (!type_is_numeric_family(current) || !type_is_numeric_family(next)) {
    throw StyioTypeError("matrix elements must be numeric scalar values");
  }
  if (current.equals(next)) {
    return current;
  }
  if (current.option == StyioDataTypeOption::Float
      || next.option == StyioDataTypeOption::Float) {
    return kF64Type;
  }
  return kI64Type;
}

MatrixLiteralInfo
infer_matrix_literal_info(StyioSemaContext* an, StyioAST* expr) {
  auto* outer = dynamic_cast<ListAST*>(expr);
  if (outer == nullptr) {
    throw StyioTypeError("matrix binding requires a nested list literal");
  }

  auto const& rows = outer->getElements();
  if (rows.empty()) {
    throw StyioTypeError("matrix literal requires at least one row");
  }

  MatrixLiteralInfo info;
  info.rows = rows.size();
  StyioDataType elem_type{StyioDataTypeOption::Undefined, "undefined", 0};

  for (size_t row_index = 0; row_index < rows.size(); ++row_index) {
    auto* row = dynamic_cast<ListAST*>(rows[row_index]);
    if (row == nullptr) {
      throw StyioTypeError("matrix rows must be list literals");
    }
    auto const& cells = row->getElements();
    if (cells.empty()) {
      throw StyioTypeError("matrix rows must not be empty");
    }
    if (row_index == 0) {
      info.cols = cells.size();
    }
    else if (cells.size() != info.cols) {
      throw StyioTypeError("matrix rows must have consistent length");
    }

    for (auto* cell : cells) {
      StyioDataType cell_type = infer_expr_type(an, cell);
      if (cell_type.isUndefined()) {
        cell_type = kI64Type;
      }
      elem_type = merge_matrix_elem_types(elem_type, cell_type);
    }
  }

  if (elem_type.isUndefined()) {
    elem_type = kI64Type;
  }
  info.elem_type = elem_type;
  return info;
}

bool
container_value_assignable(const StyioDataType& target, const StyioDataType& actual);

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

std::optional<int64_t>
static_i64_literal(StyioAST* ast) {
  auto* i = dynamic_cast<IntAST*>(ast);
  if (i == nullptr) {
    return std::nullopt;
  }
  try {
    return std::stoll(i->getValue());
  }
  catch (...) {
    return std::nullopt;
  }
}

StyioDataType
matrix_elem_type(const StyioDataType& matrix_type) {
  return styio_data_type_from_name(styio_matrix_elem_type_name(matrix_type));
}

StyioDataType
merge_numeric_elem_type(const StyioDataType& lhs, const StyioDataType& rhs) {
  if (!type_is_numeric_family(lhs) || !type_is_numeric_family(rhs)) {
    throw StyioTypeError("matrix operations require numeric scalar element types");
  }
  if (lhs.option == StyioDataTypeOption::Float || rhs.option == StyioDataTypeOption::Float) {
    return kF64Type;
  }
  return kI64Type;
}

void
require_matrix_arg(const std::string& name, const StyioDataType& type) {
  if (!styio_is_matrix_type(type)) {
    throw StyioTypeError("matrix intrinsic `" + name + "` requires matrix argument(s)");
  }
}

void
require_integer_arg(const std::string& name, const StyioDataType& type) {
  if (type.option != StyioDataTypeOption::Integer) {
    throw StyioTypeError("matrix intrinsic `" + name + "` requires integer dimension/index argument(s)");
  }
}

void
require_same_matrix_shape(const StyioDataType& lhs, const StyioDataType& rhs) {
  size_t lr = styio_matrix_row_count(lhs);
  size_t lc = styio_matrix_col_count(lhs);
  size_t rr = styio_matrix_row_count(rhs);
  size_t rc = styio_matrix_col_count(rhs);
  if (lr != 0 && lc != 0 && rr != 0 && rc != 0 && (lr != rr || lc != rc)) {
    throw StyioTypeError("matrix shapes must match");
  }
}

void
require_matmul_compatible(const StyioDataType& lhs, const StyioDataType& rhs) {
  size_t lc = styio_matrix_col_count(lhs);
  size_t rr = styio_matrix_row_count(rhs);
  if (lc != 0 && rr != 0 && lc != rr) {
    throw StyioTypeError("matrix multiplication requires lhs columns to equal rhs rows");
  }
}

StyioDataType
matrix_binary_result(
  const StyioDataType& lhs,
  const StyioDataType& rhs,
  StyioOpType op
) {
  const bool lhs_matrix = styio_is_matrix_type(lhs);
  const bool rhs_matrix = styio_is_matrix_type(rhs);
  if (!lhs_matrix && !rhs_matrix) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  if (op == StyioOpType::Binary_Mul && lhs_matrix != rhs_matrix) {
    const StyioDataType& matrix_type = lhs_matrix ? lhs : rhs;
    const StyioDataType& scalar_type = lhs_matrix ? rhs : lhs;
    if (!type_is_numeric_family(scalar_type)) {
      throw StyioTypeError("matrix scalar multiplication requires a numeric scalar");
    }
    StyioDataType elem = merge_numeric_elem_type(matrix_elem_type(matrix_type), scalar_type);
    return styio_make_matrix_type(
      elem.name,
      styio_matrix_row_count(matrix_type),
      styio_matrix_col_count(matrix_type));
  }
  if (!lhs_matrix || !rhs_matrix) {
    throw StyioTypeError("matrix addition/subtraction require matrix operands");
  }
  StyioDataType elem = merge_numeric_elem_type(matrix_elem_type(lhs), matrix_elem_type(rhs));
  if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub) {
    require_same_matrix_shape(lhs, rhs);
    return styio_make_matrix_type(
      elem.name,
      styio_matrix_row_count(lhs),
      styio_matrix_col_count(lhs));
  }
  if (op == StyioOpType::Binary_Mul) {
    require_matmul_compatible(lhs, rhs);
    return styio_make_matrix_type(
      elem.name,
      styio_matrix_row_count(lhs),
      styio_matrix_col_count(rhs));
  }
  throw StyioTypeError("unsupported matrix operator");
}

StyioDataType
infer_matrix_intrinsic_type(StyioSemaContext* an, FuncCallAST* call) {
  if (call == nullptr || call->func_callee != nullptr) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  const std::string name = call->getNameAsStr();
  if (!is_matrix_intrinsic_name(name)) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }

  std::vector<StyioDataType> args;
  for (auto* arg : call->getArgList()) {
    args.push_back(infer_expr_type(an, arg));
  }

  auto require_count = [&](size_t n) {
    if (args.size() != n) {
      throw StyioTypeError(
        "matrix intrinsic `" + name + "` expects " + std::to_string(n)
        + " argument(s), got " + std::to_string(args.size()));
    }
  };

  if (name == "mat_zeros" || name == "mat_zeros_i64") {
    require_count(2);
    require_integer_arg(name, args[0]);
    require_integer_arg(name, args[1]);
    size_t rows = 0;
    size_t cols = 0;
    if (auto r = static_i64_literal(call->getArgList()[0]); r.has_value() && *r > 0) {
      rows = static_cast<size_t>(*r);
    }
    if (auto c = static_i64_literal(call->getArgList()[1]); c.has_value() && *c > 0) {
      cols = static_cast<size_t>(*c);
    }
    return styio_make_matrix_type(name == "mat_zeros_i64" ? "i64" : "f64", rows, cols);
  }

  if (name == "mat_identity" || name == "mat_identity_i64") {
    require_count(1);
    require_integer_arg(name, args[0]);
    size_t n = 0;
    if (auto v = static_i64_literal(call->getArgList()[0]); v.has_value() && *v > 0) {
      n = static_cast<size_t>(*v);
    }
    return styio_make_matrix_type(name == "mat_identity_i64" ? "i64" : "f64", n, n);
  }

  if (name == "mat_rows" || name == "mat_cols") {
    require_count(1);
    require_matrix_arg(name, args[0]);
    return kI64Type;
  }
  if (name == "mat_shape") {
    require_count(1);
    require_matrix_arg(name, args[0]);
    return styio_make_list_type("i64");
  }
  if (name == "mat_get") {
    require_count(3);
    require_matrix_arg(name, args[0]);
    require_integer_arg(name, args[1]);
    require_integer_arg(name, args[2]);
    return matrix_elem_type(args[0]);
  }
  if (name == "mat_set") {
    require_count(4);
    require_matrix_arg(name, args[0]);
    require_integer_arg(name, args[1]);
    require_integer_arg(name, args[2]);
    if (!container_value_assignable(matrix_elem_type(args[0]), args[3])) {
      throw StyioTypeError("mat_set value does not match matrix element type");
    }
    return kI64Type;
  }
  if (name == "mat_clone" || name == "transpose") {
    require_count(1);
    require_matrix_arg(name, args[0]);
    if (name == "transpose") {
      return styio_make_matrix_type(
        styio_matrix_elem_type_name(args[0]),
        styio_matrix_col_count(args[0]),
        styio_matrix_row_count(args[0]));
    }
    return args[0];
  }
  if (name == "mat_add" || name == "mat_sub" || name == "mat_hadamard") {
    require_count(2);
    require_matrix_arg(name, args[0]);
    require_matrix_arg(name, args[1]);
    require_same_matrix_shape(args[0], args[1]);
    StyioDataType elem = merge_numeric_elem_type(matrix_elem_type(args[0]), matrix_elem_type(args[1]));
    return styio_make_matrix_type(
      elem.name,
      styio_matrix_row_count(args[0]),
      styio_matrix_col_count(args[0]));
  }
  if (name == "mat_scale") {
    require_count(2);
    require_matrix_arg(name, args[0]);
    if (!type_is_numeric_family(args[1])) {
      throw StyioTypeError("mat_scale requires a numeric scalar");
    }
    StyioDataType elem = merge_numeric_elem_type(matrix_elem_type(args[0]), args[1]);
    return styio_make_matrix_type(
      elem.name,
      styio_matrix_row_count(args[0]),
      styio_matrix_col_count(args[0]));
  }
  if (name == "matmul") {
    require_count(2);
    require_matrix_arg(name, args[0]);
    require_matrix_arg(name, args[1]);
    require_matmul_compatible(args[0], args[1]);
    StyioDataType elem = merge_numeric_elem_type(matrix_elem_type(args[0]), matrix_elem_type(args[1]));
    return styio_make_matrix_type(
      elem.name,
      styio_matrix_row_count(args[0]),
      styio_matrix_col_count(args[1]));
  }
  if (name == "dot") {
    require_count(2);
    require_matrix_arg(name, args[0]);
    require_matrix_arg(name, args[1]);
    require_same_matrix_shape(args[0], args[1]);
    return merge_numeric_elem_type(matrix_elem_type(args[0]), matrix_elem_type(args[1]));
  }
  if (name == "mat_sum") {
    require_count(1);
    require_matrix_arg(name, args[0]);
    return matrix_elem_type(args[0]);
  }
  if (name == "norm") {
    require_count(1);
    require_matrix_arg(name, args[0]);
    return kF64Type;
  }

  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

StyioDataType
merge_cond_flow_branch_type(
  const StyioDataType& then_type,
  const StyioDataType& else_type,
  const StyioDataType& saved_type) {
  if (then_type.isUndefined() && else_type.isUndefined()) {
    return saved_type.isUndefined()
      ? StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0}
      : saved_type;
  }
  if (then_type.isUndefined()) {
    return else_type;
  }
  if (else_type.isUndefined()) {
    return then_type;
  }
  if (then_type.equals(else_type)) {
    return then_type;
  }
  if (type_is_numeric_family(then_type) && type_is_numeric_family(else_type)) {
    return getMaxType(then_type, else_type);
  }
  if (!saved_type.isUndefined()) {
    if ((then_type.equals(saved_type) || type_is_numeric_family(then_type))
        && (else_type.equals(saved_type) || type_is_numeric_family(else_type))
        && type_is_numeric_family(saved_type)) {
      return getMaxType(getMaxType(saved_type, then_type), else_type);
    }
  }
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

bool
type_is_runtime_dict_value(const StyioDataType& type) {
  return styio_type_supports_runtime_dict_value(type);
}

StyioDataType
merge_dict_value_types(StyioDataType current, StyioDataType next) {
  if (current.isUndefined()) {
    return next;
  }
  if (next.isUndefined()) {
    return current;
  }
  if (current.equals(next)) {
    return current;
  }

  StyioValueFamily current_family = styio_value_family_for_type(current);
  StyioValueFamily next_family = styio_value_family_for_type(next);
  if (current_family == StyioValueFamily::Integer
      && next_family == StyioValueFamily::Integer) {
    return kI64Type;
  }
  if (type_is_numeric_family(current) && type_is_numeric_family(next)) {
    return kF64Type;
  }

  throw StyioTypeError(
    "dict values must use one consistent runtime scalar/string family in this slice");
}

bool
container_value_assignable(const StyioDataType& target, const StyioDataType& actual) {
  if (actual.isUndefined()) {
    return true;
  }
  StyioValueFamily target_family = styio_value_family_for_type(target);
  StyioValueFamily actual_family = styio_value_family_for_type(actual);
  if (target_family == StyioValueFamily::Float) {
    return actual_family == StyioValueFamily::Float
      || actual_family == StyioValueFamily::Integer;
  }
  if (target_family == StyioValueFamily::Integer) {
    return actual_family == StyioValueFamily::Integer;
  }
  return target_family == actual_family;
}

bool
is_predefined_list_operation_name(const std::string& name) {
  return name == "push" || name == "insert" || name == "pop";
}

bool
is_predefined_string_operation_name(const std::string& name) {
  return name == "lines";
}

StyioDataType
infer_predefined_list_operation_type(StyioSemaContext* an, FuncCallAST* call) {
  if (call == nullptr || call->func_callee == nullptr) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  if (!is_predefined_list_operation_name(call->getNameAsStr())) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  StyioDataType callee_type = infer_expr_type(an, call->func_callee);
  if (!styio_is_list_type(callee_type)) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  return kI64Type;
}

StyioDataType
infer_predefined_string_operation_type(StyioSemaContext* an, FuncCallAST* call) {
  if (call == nullptr || call->func_callee == nullptr) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  if (!is_predefined_string_operation_name(call->getNameAsStr())) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  StyioDataType callee_type = infer_expr_type(an, call->func_callee);
  if (callee_type.option != StyioDataTypeOption::String) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  return styio_make_list_type("string");
}

bool
func_param_accepts_arg(const StyioDataType& param_type, const StyioDataType& arg_type) {
  if (param_type.isUndefined() || arg_type.isUndefined()) {
    return true;
  }
  if (param_type.equals(arg_type)) {
    return true;
  }

  StyioValueFamily param_family = styio_value_family_for_type(param_type);
  StyioValueFamily arg_family = styio_value_family_for_type(arg_type);
  if ((param_family == StyioValueFamily::Integer || param_family == StyioValueFamily::Float)
      && (arg_family == StyioValueFamily::Integer || arg_family == StyioValueFamily::Float)) {
    return true;
  }
  if ((param_family == StyioValueFamily::Integer || param_family == StyioValueFamily::Float)
      && arg_family == StyioValueFamily::String) {
    return true;
  }
  return param_family == arg_family;
}

StyioDataType
func_ret_type_of_def(StyioSemaContext* an, StyioAST* def) {
  if (auto* f = dynamic_cast<FunctionAST*>(def)) {
    if (f->ret_type.valueless_by_exception()) {
      return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
    }
    if (std::holds_alternative<TypeAST*>(f->ret_type)) {
      auto* ty = std::get<TypeAST*>(f->ret_type);
      if (ty != nullptr) {
        StyioDataType dt = ty->getDataType();
        if (!dt.isUndefined()) {
          return dt;
        }
      }
    }
    return infer_expr_type(an, f->func_body);
  }

  if (auto* f = dynamic_cast<SimpleFuncAST*>(def)) {
    if (f->ret_type.valueless_by_exception()) {
      return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
    }
    if (std::holds_alternative<TypeAST*>(f->ret_type)) {
      auto* ty = std::get<TypeAST*>(f->ret_type);
      if (ty != nullptr) {
        StyioDataType dt = ty->getDataType();
        if (!dt.isUndefined()) {
          return dt;
        }
      }
    }
    return infer_expr_type(an, f->ret_expr);
  }

  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

StyioDataType
infer_dict_literal_type(StyioSemaContext* an, DictAST* dict) {
  auto const& entries = dict->getEntries();
  if (entries.empty()) {
    return styio_make_dict_type("string", "i64");
  }

  for (auto const& entry : entries) {
    StyioDataType key_type = infer_expr_type(an, entry.key);
    if (key_type.option != StyioDataTypeOption::String) {
      throw StyioTypeError("dict keys must have type string in this slice");
    }
  }

  StyioDataType value_type = infer_expr_type(an, entries[0].value);
  if (value_type.isUndefined()) {
    value_type = kI64Type;
  }
  if (!type_is_runtime_dict_value(value_type)) {
    throw StyioTypeError(
      "dict values must have a runtime scalar or string type in this slice");
  }

  for (size_t i = 1; i < entries.size(); ++i) {
    StyioDataType next_type = infer_expr_type(an, entries[i].value);
    if (next_type.isUndefined()) {
      continue;
    }
    if (!type_is_runtime_dict_value(next_type)) {
      throw StyioTypeError(
        "dict values must have a runtime scalar or string type in this slice");
    }
    value_type = merge_dict_value_types(value_type, next_type);
  }

  return styio_make_dict_type("string", value_type.name);
}

StyioDataType
infer_expr_type(StyioSemaContext* an, StyioAST* expr) {
  if (expr == nullptr) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }

  switch (expr->getNodeType()) {
    case StyioNodeType::Bool:
    case StyioNodeType::Condition:
    case StyioNodeType::Compare:
      return kBoolType;
    case StyioNodeType::Integer:
      return static_cast<IntAST*>(expr)->getDataType();
    case StyioNodeType::Float:
      return static_cast<FloatAST*>(expr)->getDataType();
    case StyioNodeType::String:
      return kStringType;
    case StyioNodeType::List:
      return infer_list_literal_type(an, static_cast<ListAST*>(expr));
    case StyioNodeType::Dict:
      return infer_dict_literal_type(an, static_cast<DictAST*>(expr));
    case StyioNodeType::Range:
    case StyioNodeType::TypedStdinList:
    case StyioNodeType::StdinResource:
    case StyioNodeType::StdoutResource:
    case StyioNodeType::StderrResource:
    case StyioNodeType::FileResource:
    case StyioNodeType::InstantPull:
      return expr->getDataType();
    case StyioNodeType::Attribute: {
      auto* attr = static_cast<AttrAST*>(expr);
      auto* attr_name = dynamic_cast<NameAST*>(attr->attr);
      if (attr_name == nullptr) {
        return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
      }
      StyioDataType base_type = infer_expr_type(an, attr->body);
      if (attr_name->getAsStr() == "keys" && styio_is_dict_type(base_type)) {
        return styio_make_list_type(styio_dict_key_type_name(base_type));
      }
      if (attr_name->getAsStr() == "values" && styio_is_dict_type(base_type)) {
        return styio_make_list_type(styio_dict_value_type_name(base_type));
      }
      return kI64Type;
    }
    case StyioNodeType::Access_By_Index: {
      auto* access = static_cast<ListOpAST*>(expr);
      StyioDataType base_type = infer_expr_type(an, access->getList());
      if (!styio_type_is_indexable(base_type)) {
        return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
      }
      return styio_data_type_from_name(styio_type_item_type_name(base_type));
    }
    case StyioNodeType::Access_By_Name: {
      auto* access = static_cast<ListOpAST*>(expr);
      StyioDataType base_type = infer_expr_type(an, access->getList());
      if (!styio_is_dict_type(base_type)) {
        return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
      }
      return styio_data_type_from_name(styio_dict_value_type_name(base_type));
    }
    case StyioNodeType::BinOp: {
      StyioDataType t = static_cast<BinOpAST*>(expr)->getType();
      return t.isUndefined() ? expr->getDataType() : t;
    }
    case StyioNodeType::WaveMerge: {
      auto* wave = static_cast<WaveMergeAST*>(expr);
      return merge_cond_flow_branch_type(
        infer_expr_type(an, wave->getTrueVal()),
        infer_expr_type(an, wave->getFalseVal()),
        StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0});
    }
    case StyioNodeType::Id: {
      auto* nm = static_cast<NameAST*>(expr);
      auto it = an->local_binding_types.find(nm->getAsStr());
      if (it != an->local_binding_types.end()) {
        return it->second;
      }
      return expr->getDataType();
    }
    case StyioNodeType::Call: {
      auto* call = static_cast<FuncCallAST*>(expr);
      StyioDataType builtin_type = infer_predefined_list_operation_type(an, call);
      if (!builtin_type.isUndefined()) {
        return builtin_type;
      }
      builtin_type = infer_predefined_string_operation_type(an, call);
      if (!builtin_type.isUndefined()) {
        return builtin_type;
      }
      builtin_type = infer_matrix_intrinsic_type(an, call);
      if (!builtin_type.isUndefined()) {
        return builtin_type;
      }
      auto it = an->func_defs.find(call->getNameAsStr());
      if (it != an->func_defs.end()) {
        return func_ret_type_of_def(an, it->second);
      }
      auto native_it = an->native_func_defs.find(call->getNameAsStr());
      if (native_it != an->native_func_defs.end()) {
        return native_it->second.return_type;
      }
      return expr->getDataType();
    }
    default:
      return expr->getDataType();
  }
}

StyioDataType
infer_collection_elem_type(StyioSemaContext* an, StyioAST* coll) {
  StyioDataType collection_type = infer_expr_type(an, coll);
  if (styio_type_is_iterable(collection_type)) {
    return styio_data_type_from_name(styio_type_item_type_name(collection_type));
  }
  if (auto* L = dynamic_cast<ListAST*>(coll)) {
    return styio_data_type_from_name(styio_type_item_type_name(infer_list_literal_type(an, L)));
  }
  return kI64Type;
}

bool
type_is_string(StyioDataType const& t) {
  return t.option == StyioDataTypeOption::String;
}

bool
type_is_intish(StyioDataType const& t) {
  return t.option == StyioDataTypeOption::Integer
    || t.option == StyioDataTypeOption::Float;
}

bool
type_is_bool(StyioDataType const& t) {
  return styio_value_family_for_type(t) == StyioValueFamily::Bool;
}

bool
type_is_text_serializable_iterable(StyioDataType const& t) {
  if (styio_is_list_type(t)) {
    return styio_type_supports_runtime_list_elem(
      styio_data_type_from_name(styio_type_item_type_name(t)));
  }
  if (styio_is_dict_type(t)) {
    return styio_dict_key_type_name(t) == "string"
      && styio_type_supports_runtime_dict_value(
        styio_data_type_from_name(styio_dict_value_type_name(t)));
  }
  if (t.handle_family == StyioHandleFamily::Range) {
    return styio_value_family_is_runtime_scalar(styio_type_item_value_family(t));
  }
  return false;
}

std::optional<bool>
expr_is_string_hint(StyioSemaContext* an, StyioAST* x) {
  StyioDataType t = infer_expr_type(an, x);
  if (t.isUndefined()) {
    return std::nullopt;
  }
  return type_is_string(t);
}

std::optional<bool>
expr_is_intish_hint(StyioSemaContext* an, StyioAST* x) {
  StyioDataType t = infer_expr_type(an, x);
  if (t.isUndefined()) {
    return std::nullopt;
  }
  return type_is_intish(t);
}

StyioSemaContext::BindingValueKind
binding_value_kind_for_type(const StyioDataType& type) {
  switch (styio_value_family_for_type(type)) {
    case StyioValueFamily::ListHandle:
      return StyioSemaContext::BindingValueKind::ListHandle;
    case StyioValueFamily::DictHandle:
      return StyioSemaContext::BindingValueKind::DictHandle;
    case StyioValueFamily::MatrixHandle:
      return StyioSemaContext::BindingValueKind::MatrixHandle;
    case StyioValueFamily::String:
      return StyioSemaContext::BindingValueKind::String;
    case StyioValueFamily::Float:
      return StyioSemaContext::BindingValueKind::F64;
    case StyioValueFamily::Bool:
      return StyioSemaContext::BindingValueKind::Bool;
    case StyioValueFamily::Integer:
      return StyioSemaContext::BindingValueKind::I64;
    default:
      break;
  }
  return StyioSemaContext::BindingValueKind::Unknown;
}

bool
infer_concat_string_add(StyioSemaContext* an, BinOpAST* ast, StyioAST* lhs, StyioAST* rhs) {
  std::optional<bool> ls = expr_is_string_hint(an, lhs);
  std::optional<bool> rs = expr_is_string_hint(an, rhs);
  std::optional<bool> li = expr_is_intish_hint(an, lhs);
  std::optional<bool> ri = expr_is_intish_hint(an, rhs);
  if ((ls && *ls) || (rs && *rs)) {
    if ((ls && *ls && rs && *rs) || (ls && *ls && ri && *ri) || (rs && *rs && li && *li)) {
      ast->setDType(kStringType);
      return true;
    }
  }
  return false;
}

bool
infer_numeric_string_coercion(StyioSemaContext* an, BinOpAST* ast, StyioAST* lhs, StyioAST* rhs) {
  StyioDataType lhs_type = infer_expr_type(an, lhs);
  StyioDataType rhs_type = infer_expr_type(an, rhs);
  const bool lhs_string = type_is_string(lhs_type);
  const bool rhs_string = type_is_string(rhs_type);
  const bool lhs_numeric = type_is_intish(lhs_type);
  const bool rhs_numeric = type_is_intish(rhs_type);
  if (!lhs_string && !rhs_string) {
    return false;
  }
  if (!lhs_numeric && !rhs_numeric && !(lhs_string && rhs_string)) {
    return false;
  }
  ast->setDType(
    lhs_type.isFloat() || rhs_type.isFloat()
      ? kF64Type
      : kI64Type);
  return true;
}

}  // namespace

void
StyioSemaContext::typeInfer(CommentAST* ast) {
}

void
StyioSemaContext::typeInfer(NoneAST* ast) {
}

void
StyioSemaContext::typeInfer(EmptyAST* ast) {
}

void
StyioSemaContext::typeInfer(NameAST* ast) {
}

void
StyioSemaContext::typeInfer(TypeAST* ast) {
}

void
StyioSemaContext::typeInfer(TypeTupleAST* ast) {
}

void
StyioSemaContext::typeInfer(BoolAST* ast) {
}

void
StyioSemaContext::typeInfer(IntAST* ast) {
}

void
StyioSemaContext::typeInfer(FloatAST* ast) {
}

void
StyioSemaContext::typeInfer(CharAST* ast) {
}

void
StyioSemaContext::typeInfer(StringAST* ast) {
}

void
StyioSemaContext::typeInfer(TypeConvertAST*) {
}

void
StyioSemaContext::typeInfer(VarAST* ast) {
}

void
StyioSemaContext::typeInfer(ParamAST* ast) {
}

void
StyioSemaContext::typeInfer(OptArgAST* ast) {
}

void
StyioSemaContext::typeInfer(OptKwArgAST* ast) {
}

/*
  The declared type is always the *top* priority
  because the programmer wrote in that way!
*/
void
StyioSemaContext::typeInfer(FlexBindAST* ast) {
  const std::string& bound_name = ast->getNameAsStr();
  if (fixed_assignment_names_.count(bound_name) != 0) {
    throw StyioSyntaxError(
      "variable `" + bound_name + "` was defined with `:=` (fixed assignment); "
      "cannot reassign with `=` (flex bind). Use a different name.");
  }

  auto reject_plain_resource_copy = [&](StyioAST* expr) {
    auto* src = dynamic_cast<NameAST*>(expr);
    if (src == nullptr) {
      return;
    }
    auto it = binding_info_.find(src->getAsStr());
    if (it != binding_info_.end() && it->second.resource_value) {
      throw StyioTypeError(
        "resource `" + src->getAsStr()
        + "` cannot be copied with `=`; use `<-` or `<<` to clone it");
    }
  };

  auto expr_value_kind = [&](StyioAST* expr) -> BindingValueKind {
    if (expr->getNodeType() == StyioNodeType::TypedStdinList) {
      return BindingValueKind::ListHandle;
    }
    if (expr->getNodeType() == StyioNodeType::List) {
      return BindingValueKind::ListHandle;
    }
    if (expr->getNodeType() == StyioNodeType::Dict) {
      return BindingValueKind::DictHandle;
    }
    if (auto* nm = dynamic_cast<NameAST*>(expr)) {
      auto bit = binding_info_.find(nm->getAsStr());
      if (bit != binding_info_.end()) {
        return bit->second.value_kind;
      }
    }

    StyioDataType ty = infer_expr_type(this, expr);
    switch (expr->getNodeType()) {
      case StyioNodeType::Bool:
      case StyioNodeType::Condition:
      case StyioNodeType::Compare:
        return BindingValueKind::Bool;
      case StyioNodeType::Integer:
        return BindingValueKind::I64;
      case StyioNodeType::Float:
        return BindingValueKind::F64;
      case StyioNodeType::String:
        return BindingValueKind::String;
      default:
        return binding_value_kind_for_type(ty);
    }
  };

  auto var_type = ast->getVar()->getDType()->type;

  if (var_type.option != StyioDataTypeOption::Undefined) {
    if (ast->getValue()->getNodeType() == StyioNodeType::BinOp) {
      if (!styio_is_matrix_type(var_type)) {
        static_cast<BinOpAST*>(ast->getValue())->setDType(var_type);
      }
    }
  }

  ast->getValue()->typeInfer(this);

  if (styio_is_matrix_type(var_type)) {
    if (dynamic_cast<ListAST*>(ast->getValue()) != nullptr) {
      MatrixLiteralInfo matrix = infer_matrix_literal_info(this, ast->getValue());
      var_type = styio_make_matrix_type(matrix.elem_type.name, matrix.rows, matrix.cols);
      static_cast<ListAST*>(ast->getValue())->setDataType(var_type);
    }
    else {
      StyioDataType rhs_type = infer_expr_type(this, ast->getValue());
      require_matrix_arg("matrix binding", rhs_type);
      if (styio_matrix_row_count(var_type) == 0 && styio_matrix_col_count(var_type) == 0) {
        var_type = rhs_type;
      }
      else {
        require_same_matrix_shape(var_type, rhs_type);
      }
    }
    ast->getVar()->setDataType(var_type);
  }

  if (var_type.option == StyioDataTypeOption::Undefined) {
    switch (ast->getValue()->getNodeType()) {
      case StyioNodeType::Integer: {
        ast->getVar()->setDataType(static_cast<IntAST*>(ast->getValue())->getDataType());
      } break;

      case StyioNodeType::Float: {
        ast->getVar()->setDataType(static_cast<FloatAST*>(ast->getValue())->getDataType());
      } break;

      case StyioNodeType::BinOp: {
        ast->getVar()->setDataType(static_cast<BinOpAST*>(ast->getValue())->getType());
      } break;

      case StyioNodeType::Bool:
      case StyioNodeType::Condition:
      case StyioNodeType::Compare: {
        ast->getVar()->setDataType(StyioDataType{StyioDataTypeOption::Bool, "bool", 1});
      } break;

      case StyioNodeType::String: {
        ast->getVar()->setDataType(kStringType);
      } break;

      case StyioNodeType::Tuple: {
        ast->getVar()->setDataType(ast->getValue()->getDataType());
      } break;

      default:
        break;
    }
  }

  reject_plain_resource_copy(ast->getValue());
  StyioDataType inferred_rhs_type = infer_expr_type(this, ast->getValue());
  if (inferred_rhs_type.handle_family == StyioHandleFamily::File
      || inferred_rhs_type.handle_family == StyioHandleFamily::Stream) {
    throw StyioTypeError(
      "resource handles must be bound with `<-`; use `<- @...` for files and standard streams");
  }

  StyioDataType concrete_type = inferred_rhs_type;
  if (var_type.option != StyioDataTypeOption::Undefined) {
    concrete_type = var_type;
  }
  BindingValueKind kind = expr_value_kind(ast->getValue());
  if (styio_is_matrix_type(concrete_type)) {
    kind = BindingValueKind::MatrixHandle;
  }

  local_binding_types[ast->getNameAsStr()] = concrete_type;

  BindingInfo info;
  auto prev = binding_info_.find(bound_name);
  if (prev != binding_info_.end()) {
    info = prev->second;
  }
  info.final_slot = false;
  info.dynamic_slot = info.dynamic_slot
    || ast->getValue()->getNodeType() == StyioNodeType::TypedStdinList
    || kind == BindingValueKind::ListHandle
    || kind == BindingValueKind::DictHandle;
  if (kind == BindingValueKind::MatrixHandle) {
    info.dynamic_slot = false;
  }
  info.resource_value = kind == BindingValueKind::ListHandle
    || kind == BindingValueKind::DictHandle
    || kind == BindingValueKind::MatrixHandle;
  info.value_kind = kind;
  info.declared_type = info.dynamic_slot
    ? StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0}
    : concrete_type;
  binding_info_[bound_name] = info;
}

void
StyioSemaContext::typeInfer(FinalBindAST* ast) {
  auto* rhs_name = dynamic_cast<NameAST*>(ast->getValue());
  if (rhs_name != nullptr) {
    auto it = binding_info_.find(rhs_name->getAsStr());
    if (it != binding_info_.end() && it->second.resource_value) {
      throw StyioTypeError(
        "resource `" + rhs_name->getAsStr()
        + "` cannot be copied with `:=`; use `<-` or `<<` to clone it");
    }
  }
  ast->getValue()->typeInfer(this);
  auto vt = ast->getVar()->getDType()->type;
  if (styio_is_matrix_type(vt)) {
    if (dynamic_cast<ListAST*>(ast->getValue()) != nullptr) {
      MatrixLiteralInfo matrix = infer_matrix_literal_info(this, ast->getValue());
      vt = styio_make_matrix_type(matrix.elem_type.name, matrix.rows, matrix.cols);
      static_cast<ListAST*>(ast->getValue())->setDataType(vt);
    }
    else {
      StyioDataType rhs_type = infer_expr_type(this, ast->getValue());
      require_matrix_arg("matrix binding", rhs_type);
      if (styio_matrix_row_count(vt) == 0 && styio_matrix_col_count(vt) == 0) {
        vt = rhs_type;
      }
      else {
        require_same_matrix_shape(vt, rhs_type);
      }
    }
    ast->getVar()->setDataType(vt);
  }
  if (ast->getValue()->getNodeType() == StyioNodeType::BinOp) {
    if (!styio_is_matrix_type(vt)) {
      static_cast<BinOpAST*>(ast->getValue())->setDType(vt);
      ast->getValue()->typeInfer(this);
    }
  }
  local_binding_types[ast->getVar()->getNameAsStr()] = vt;
  fixed_assignment_names_.insert(ast->getVar()->getNameAsStr());

  auto rhs_info = rhs_name == nullptr
    ? binding_info_.end()
    : binding_info_.find(rhs_name->getAsStr());
  BindingValueKind rhs_kind = binding_value_kind_for_type(infer_expr_type(this, ast->getValue()));
  BindingInfo info;
  info.final_slot = true;
  info.dynamic_slot = false;
  const bool runtime_resource =
    ast->getValue()->getNodeType() == StyioNodeType::TypedStdinList
    || ast->getValue()->getNodeType() == StyioNodeType::Dict
    || rhs_kind == BindingValueKind::ListHandle
    || rhs_kind == BindingValueKind::DictHandle
    || rhs_kind == BindingValueKind::MatrixHandle
    || (rhs_info != binding_info_.end()
        && (rhs_info->second.value_kind == BindingValueKind::ListHandle
            || rhs_info->second.value_kind == BindingValueKind::DictHandle
            || rhs_info->second.value_kind == BindingValueKind::MatrixHandle));
  info.resource_value = runtime_resource;
  if (ast->getValue()->getNodeType() == StyioNodeType::Dict) {
    info.value_kind = BindingValueKind::DictHandle;
  }
  else if (runtime_resource) {
    info.value_kind =
      ast->getValue()->getNodeType() == StyioNodeType::TypedStdinList
        ? BindingValueKind::ListHandle
        : (rhs_info != binding_info_.end() ? rhs_info->second.value_kind : rhs_kind);
    if (styio_is_matrix_type(vt)) {
      info.value_kind = BindingValueKind::MatrixHandle;
    }
  }
  else if (vt.option == StyioDataTypeOption::String) {
    info.value_kind = BindingValueKind::String;
  }
  else if (vt.option == StyioDataTypeOption::Float) {
    info.value_kind = BindingValueKind::F64;
  }
  else if (vt.option == StyioDataTypeOption::Bool) {
    info.value_kind = BindingValueKind::Bool;
  }
  else if (vt.option == StyioDataTypeOption::Integer) {
    info.value_kind = BindingValueKind::I64;
  }
  else {
    info.value_kind = BindingValueKind::Unknown;
  }
  info.declared_type = vt;
  binding_info_[ast->getVar()->getNameAsStr()] = info;
}

void
StyioSemaContext::typeInfer(ParallelAssignAST* ast) {
  if (ast->getLHS().size() != ast->getRHS().size()) {
    throw StyioTypeError("parallel assignment requires the same number of LHS and RHS expressions");
  }

  for (auto* rhs : ast->getRHS()) {
    if (auto* rhs_name = dynamic_cast<NameAST*>(rhs)) {
      auto it = binding_info_.find(rhs_name->getAsStr());
      if (it != binding_info_.end() && it->second.resource_value) {
        throw StyioTypeError(
          "resource `" + rhs_name->getAsStr()
          + "` cannot be copied with `=`; use `<-` or `<<` to clone it");
      }
    }
    rhs->typeInfer(this);
  }

  for (size_t i = 0; i < ast->getLHS().size(); ++i) {
    StyioAST* lhs = ast->getLHS()[i];
    if (auto* nm = dynamic_cast<NameAST*>(lhs)) {
      auto it = binding_info_.find(nm->getAsStr());
      if ((it != binding_info_.end() && it->second.final_slot)
          || fixed_assignment_names_.count(nm->getAsStr()) != 0) {
        throw StyioTypeError("parallel assignment cannot rebind final slot `" + nm->getAsStr() + "`");
      }
      if (it != binding_info_.end() && it->second.dynamic_slot) {
        if (auto* rhs_name = dynamic_cast<NameAST*>(ast->getRHS()[i])) {
          auto rit = binding_info_.find(rhs_name->getAsStr());
          if (rit != binding_info_.end()) {
            it->second.value_kind = rit->second.value_kind;
            it->second.resource_value = rit->second.resource_value;
            local_binding_types[nm->getAsStr()] = rit->second.declared_type;
            binding_info_[nm->getAsStr()] = it->second;
          }
        }
      }
      continue;
    }

    auto* idx = dynamic_cast<ListOpAST*>(lhs);
    if (idx == nullptr || idx->getOp() != StyioNodeType::Access_By_Index) {
      throw StyioTypeError("parallel assignment targets must be names or indexed list elements");
    }
    idx->typeInfer(this);
    StyioDataType base_type = infer_expr_type(this, idx->getList());
    StyioDataType rhs_type = infer_expr_type(this, ast->getRHS()[i]);
    if (styio_is_dict_type(base_type)) {
      StyioDataType target_type =
        styio_data_type_from_name(styio_dict_value_type_name(base_type));
      if (!container_value_assignable(target_type, rhs_type)) {
        throw StyioTypeError(
          "indexed assignment RHS does not match dict value type `"
          + target_type.name + "`");
      }
      continue;
    }
    if (!styio_is_list_type(base_type)) {
      throw StyioTypeError(
        "indexed assignment in this slice supports dict[string,T] or list[T] targets only");
    }
    StyioDataType elem_type = styio_data_type_from_name(styio_type_item_type_name(base_type));
    if (!styio_type_supports_runtime_list_elem(elem_type)) {
      throw StyioTypeError(
        "indexed assignment in this slice supports runtime list element families only");
    }
    if (!container_value_assignable(elem_type, rhs_type)) {
      throw StyioTypeError(
        "indexed assignment RHS does not match list element type `"
        + elem_type.name + "`");
    }
  }
}

void
StyioSemaContext::typeInfer(InfiniteAST* ast) {
}

void
StyioSemaContext::typeInfer(StructAST* ast) {
}

void
StyioSemaContext::typeInfer(TupleAST* ast) {
  /* if no element against the consistency, the tuple will have a type. */
  auto elements = ast->getElements();

  bool is_consistent = true;
  StyioDataType aggregated_type = elements[0]->getDataType();
  if (aggregated_type.isUndefined()) {
    for (size_t i = 1; i < elements.size(); i += 1) {
      if (not(elements[i]->getDataType()).equals(aggregated_type)) {
        is_consistent = false;
      }
    }
  }

  if (is_consistent) {
    ast->setConsistency(is_consistent);
    ast->setDataType(aggregated_type);
  }
}

void
StyioSemaContext::typeInfer(VarTupleAST* ast) {
}

void
StyioSemaContext::typeInfer(ExtractorAST* ast) {
}

void
StyioSemaContext::typeInfer(RangeAST* ast) {
}

void
StyioSemaContext::typeInfer(SetAST* ast) {
}

void
StyioSemaContext::typeInfer(ListAST* ast) {
  for (auto* elem : ast->getElements()) {
    elem->typeInfer(this);
  }
  ast->setConsistency(true);
  ast->setDataType(infer_list_literal_type(this, ast));
}

void
StyioSemaContext::typeInfer(DictAST* ast) {
  auto const& entries = ast->getEntries();
  for (auto const& entry : entries) {
    entry.key->typeInfer(this);
    entry.value->typeInfer(this);
  }
  StyioDataType dict_type = infer_dict_literal_type(this, ast);
  ast->setConsistency(true);
  ast->setDataType(dict_type);
}

void
StyioSemaContext::typeInfer(SizeOfAST* ast) {
  if (ast == nullptr || ast->getValue() == nullptr) {
    throw StyioTypeError("size-of expects an expression");
  }

  ast->getValue()->typeInfer(this);
  const StyioDataType value_type = infer_expr_type(this, ast->getValue());
  if (!styio_is_list_type(value_type) && !styio_is_dict_type(value_type)) {
    throw StyioTypeError("size-of expects a list or dict value");
  }

  ast->setDataType(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
}

void
StyioSemaContext::typeInfer(ListOpAST* ast) {
  ast->getList()->typeInfer(this);
  if (ast->getSlot1()) {
    ast->getSlot1()->typeInfer(this);
  }
  if (ast->getSlot2()) {
    ast->getSlot2()->typeInfer(this);
  }

  StyioDataType list_type = infer_expr_type(this, ast->getList());
  if (ast->getOp() == StyioNodeType::Access_By_Name) {
    if (!styio_is_dict_type(list_type)) {
      throw StyioTypeError("name-based access requires a dict value");
    }
    return;
  }
  if (ast->getOp() != StyioNodeType::Access_By_Index) {
    return;
  }

  if (!styio_type_is_indexable(list_type)) {
    throw StyioTypeError("indexed access requires an indexable value");
  }

  StyioDataType slot_type = infer_expr_type(this, ast->getSlot1());
  if (styio_is_dict_type(list_type)) {
    if (slot_type.option != StyioDataTypeOption::String) {
      throw StyioTypeError("dict index must have type string");
    }
    return;
  }

  if (slot_type.option != StyioDataTypeOption::Integer) {
    throw StyioTypeError("list index must have integer type");
  }
}

void
StyioSemaContext::typeInfer(BinCompAST* ast) {
  ast->getLHS()->typeInfer(this);
  ast->getRHS()->typeInfer(this);
}

void
StyioSemaContext::typeInfer(CondAST* ast) {
  if (ast->getValue()) {
    ast->getValue()->typeInfer(this);
  }
  if (ast->getLHS()) {
    ast->getLHS()->typeInfer(this);
  }
  if (ast->getRHS()) {
    ast->getRHS()->typeInfer(this);
  }
}

void
StyioSemaContext::typeInfer(UndefinedLitAST* ast) {
  (void)ast;
}

void
StyioSemaContext::typeInfer(WaveMergeAST* ast) {
  ast->getCond()->typeInfer(this);
  ast->getTrueVal()->typeInfer(this);
  ast->getFalseVal()->typeInfer(this);
}

void
StyioSemaContext::typeInfer(WaveDispatchAST* ast) {
  ast->getCond()->typeInfer(this);
  ast->getTrueArm()->typeInfer(this);
  ast->getFalseArm()->typeInfer(this);
}

void
StyioSemaContext::typeInfer(FallbackAST* ast) {
  ast->getPrimary()->typeInfer(this);
  ast->getAlternate()->typeInfer(this);
}

void
StyioSemaContext::typeInfer(GuardSelectorAST* ast) {
  ast->getBase()->typeInfer(this);
  ast->getCond()->typeInfer(this);
}

void
StyioSemaContext::typeInfer(EqProbeAST* ast) {
  ast->getBase()->typeInfer(this);
  ast->getProbeValue()->typeInfer(this);
}

void
StyioSemaContext::typeInfer(FileResourceAST* ast) {
  ast->getPath()->typeInfer(this);
}

void
StyioSemaContext::typeInfer(StdStreamAST* ast) {
  /* No children to infer. */
}

void
StyioSemaContext::typeInfer(HandleAcquireAST* ast) {
  const std::string name = ast->getVar()->getNameAsStr();
  if (!ast->isFlexBind() && local_binding_types.count(name) != 0) {
    throw StyioTypeError("final resource bind cannot redefine `" + name + "`");
  }
  if (ast->isFlexBind() && fixed_assignment_names_.count(name) != 0) {
    throw StyioTypeError("resource clone cannot rebind final slot `" + name + "`");
  }

  ast->getResource()->typeInfer(this);

  BindingInfo info;
  info.final_slot = !ast->isFlexBind();
  info.dynamic_slot = ast->isFlexBind();
  info.declared_type = infer_expr_type(this, ast->getResource());
  info.value_kind = BindingValueKind::I64;

  if (auto* typed = dynamic_cast<TypedStdinListAST*>(ast->getResource())) {
    info.value_kind = BindingValueKind::ListHandle;
    info.resource_value = true;
    info.declared_type = typed->getDataType();
    local_binding_types[name] = typed->getDataType();
    if (!ast->isFlexBind()) {
      ast->getVar()->setDataType(typed->getDataType());
    }
  }
  else if (auto* src = dynamic_cast<NameAST*>(ast->getResource())) {
    auto it = binding_info_.find(src->getAsStr());
    StyioDataType source_type =
      (it != binding_info_.end()) ? it->second.declared_type
                                  : StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
    if (source_type.isUndefined()) {
      auto tit = local_binding_types.find(src->getAsStr());
      if (tit != local_binding_types.end()) {
        source_type = tit->second;
      }
    }
    std::optional<StdStreamKind> stream_kind;
    if (!source_type.isUndefined()
        && source_type.handle_family == StyioHandleFamily::Stream
        && source_type.has_std_stream_kind) {
      stream_kind = static_cast<StdStreamKind>(source_type.std_stream_kind);
    }
    if (ast->isFlexBind()
        && stream_kind.has_value()
        && *stream_kind == StdStreamKind::Stdin) {
      StyioDataType collected_type = styio_make_list_type("string");
      info.dynamic_slot = true;
      info.value_kind = BindingValueKind::ListHandle;
      info.resource_value = true;
      info.declared_type = StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
      local_binding_types[name] = collected_type;
      collect_bind_handle_acquires_.insert(ast);
      collect_bind_handle_acquire_types_[ast] = collected_type;
    }
    else {
      if (it == binding_info_.end() || !it->second.resource_value
          || !styio_type_is_cloneable(source_type)) {
        throw StyioTypeError(
          "resource clone source `" + src->getAsStr() + "` is not a cloneable resource");
      }
      info.value_kind = it->second.value_kind;
      info.resource_value = it->second.resource_value;
      info.declared_type = source_type;
      local_binding_types[name] = source_type;
      if (!ast->isFlexBind()) {
        ast->getVar()->setDataType(source_type);
      }
    }
  }
  else {
    if (info.declared_type.isUndefined()) {
      throw StyioTypeError("handle acquire needs a typed resource source");
    }
    info.resource_value = styio_type_is_resource_handle(info.declared_type);
    local_binding_types[name] = info.declared_type;
    if (!ast->isFlexBind()) {
      ast->getVar()->setDataType(info.declared_type);
    }
  }

  if (!ast->isFlexBind()) {
    fixed_assignment_names_.insert(name);
  }
  binding_info_[name] = info;
}

void
StyioSemaContext::typeInfer(ResourceWriteAST* ast) {
  ast->getResource()->typeInfer(this);
  auto* target_name = dynamic_cast<NameAST*>(ast->getData());
  StyioDataType resource_type = infer_expr_type(this, ast->getResource());
  if (target_name != nullptr
      && local_binding_types.count(target_name->getAsStr()) == 0
      && binding_info_.count(target_name->getAsStr()) == 0
      && resource_type.handle_family == StyioHandleFamily::Stream
      && resource_type.has_std_stream_kind
      && static_cast<StdStreamKind>(resource_type.std_stream_kind) == StdStreamKind::Stdin) {
    BindingInfo info;
    info.final_slot = false;
    info.dynamic_slot = true;
    info.resource_value = true;
    info.value_kind = BindingValueKind::ListHandle;
    info.declared_type = StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
    StyioDataType collected_type = styio_make_list_type("string");
    local_binding_types[target_name->getAsStr()] = collected_type;
    binding_info_[target_name->getAsStr()] = info;
    collect_bind_resource_writes_.insert(ast);
    collect_bind_resource_write_types_[ast] = collected_type;
    return;
  }
  ast->getData()->typeInfer(this);
  if (!styio_type_is_writable(resource_type)) {
    throw StyioTypeError("write target must be a writable resource");
  }
  if (auto* stream = dynamic_cast<StdStreamAST*>(ast->getResource())) {
    if (stream->getStreamKind() != StdStreamKind::Stdin) {
      StyioDataType data_type = infer_expr_type(this, ast->getData());
      if (!type_is_text_serializable_iterable(data_type)) {
        throw StyioTypeError(
          "terminal/standard-stream `>>` requires an iterable text-serializable value; "
          "use `-> @stdout` or `-> [>_]` for scalar text");
      }
    }
  }
}

void
StyioSemaContext::typeInfer(ResourceRedirectAST* ast) {
  ast->getData()->typeInfer(this);
  ast->getResource()->typeInfer(this);
  StyioDataType resource_type = infer_expr_type(this, ast->getResource());
  if (!styio_type_is_writable(resource_type)) {
    throw StyioTypeError("redirect target must be a writable resource");
  }
}

/*
  Int -> Int => Pass
  Int -> Float => Pass
*/
void
StyioSemaContext::typeInfer(BinOpAST* ast) {
  auto lhs = ast->getLHS();
  auto rhs = ast->getRHS();
  auto op = ast->getOp();

  if (lhs == nullptr || rhs == nullptr) {
    if (lhs != nullptr) {
      lhs->typeInfer(this);
    }
    if (rhs != nullptr) {
      rhs->typeInfer(this);
    }
    ast->setDType(StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0});
    return;
  }

  if (op == StyioOpType::Self_Add_Assign || op == StyioOpType::Self_Sub_Assign
      || op == StyioOpType::Self_Mul_Assign || op == StyioOpType::Self_Div_Assign
      || op == StyioOpType::Self_Mod_Assign) {
    rhs->typeInfer(this);
    if (lhs->getNodeType() == StyioNodeType::Id) {
      auto* nm = static_cast<NameAST*>(lhs);
      auto it = local_binding_types.find(nm->getAsStr());
      if (it != local_binding_types.end()) {
        ast->setDType(it->second);
      }
      else {
        ast->setDType(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
      }
    } else {
      ast->setDType(StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0});
    }
    return;
  }

  if (ast->getType().isUndefined()) {
    lhs->typeInfer(this);
    rhs->typeInfer(this);
    StyioDataType lhs_type = infer_expr_type(this, lhs);
    StyioDataType rhs_type = infer_expr_type(this, rhs);
    if (styio_is_matrix_type(lhs_type) || styio_is_matrix_type(rhs_type)) {
      if (op != StyioOpType::Binary_Add
          && op != StyioOpType::Binary_Sub
          && op != StyioOpType::Binary_Mul) {
        throw StyioTypeError("unsupported matrix operator");
      }
      ast->setDType(matrix_binary_result(lhs_type, rhs_type, op));
      return;
    }
    if (op == StyioOpType::Binary_Add
        && infer_concat_string_add(this, ast, lhs, rhs)) {
      return;
    }
    if ((op == StyioOpType::Binary_Add
         || op == StyioOpType::Binary_Sub
         || op == StyioOpType::Binary_Mul
         || op == StyioOpType::Binary_Div
         || op == StyioOpType::Binary_Mod
         || op == StyioOpType::Binary_Pow)
        && infer_numeric_string_coercion(this, ast, lhs, rhs)) {
      return;
    }
    auto lhs_hint = lhs->getNodeType();
    auto rhs_hint = rhs->getNodeType();

    switch (lhs_hint) {
      case StyioNodeType::Integer: {
        switch (rhs_hint) {
          case StyioNodeType::Integer: {
            auto lhs_int = static_cast<IntAST*>(lhs);
            auto rhs_int = static_cast<IntAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_int->getDataType(), rhs_int->getDataType()));
            }
          } break;

          case StyioNodeType::Float: {
            auto lhs_int = static_cast<IntAST*>(lhs);
            auto rhs_float = static_cast<FloatAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_int->getDataType(), rhs_float->getDataType()));
            }
          } break;

          case StyioNodeType::BinOp: {
            auto lhs_expr = static_cast<IntAST*>(lhs);
            auto rhs_expr = static_cast<BinOpAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_expr->getDataType(), rhs_expr->getType()));
            }
          } break;

          default:
            break;
        }
      } break;

      case StyioNodeType::Float: {
        switch (rhs_hint) {
          case StyioNodeType::Integer: {
            auto lhs_float = static_cast<FloatAST*>(lhs);
            auto rhs_int = static_cast<IntAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_float->getDataType(), rhs_int->getDataType()));
            }
          } break;

          case StyioNodeType::Float: {
            auto lhs_float = static_cast<FloatAST*>(lhs);
            auto rhs_float = static_cast<FloatAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_float->getDataType(), rhs_float->getDataType()));
            }
          } break;

          default:
            break;
        }
      } break;

      case StyioNodeType::BinOp: {
        switch (rhs_hint) {
          case StyioNodeType::Integer: {
            auto lhs_expr = static_cast<BinOpAST*>(lhs);
            auto rhs_expr = static_cast<IntAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_expr->getType(), rhs_expr->getDataType()));
            }
          } break;

          case StyioNodeType::Float: {
            auto lhs_binop = static_cast<BinOpAST*>(lhs);
            auto rhs_float = static_cast<FloatAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_binop->getType(), rhs_float->getDataType()));
            }
          } break;

          case StyioNodeType::BinOp: {
            auto lhs_binop = static_cast<BinOpAST*>(lhs);
            auto rhs_binop = static_cast<BinOpAST*>(rhs);

            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lhs_binop->getType(), rhs_binop->getType()));
            }
          } break;

          default:
            break;
        }
      } break;

      case StyioNodeType::Id: {
        auto* lid = static_cast<NameAST*>(lhs);
        auto lt_it = local_binding_types.find(lid->getAsStr());
        if (lt_it == local_binding_types.end()) {
          break;
        }
        StyioDataType lt = lt_it->second;

        switch (rhs_hint) {
          case StyioNodeType::Integer: {
            auto* ri = static_cast<IntAST*>(rhs);
            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lt, ri->getDataType()));
            }
          } break;

          case StyioNodeType::Float: {
            auto* rf = static_cast<FloatAST*>(rhs);
            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lt, rf->getDataType()));
            }
          } break;

          case StyioNodeType::BinOp: {
            auto* rb = static_cast<BinOpAST*>(rhs);
            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lt, rb->getType()));
            }
          } break;

          case StyioNodeType::Id: {
            auto* rid = static_cast<NameAST*>(rhs);
            auto rt_it = local_binding_types.find(rid->getAsStr());
            if (rt_it == local_binding_types.end()) {
              break;
            }
            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lt, rt_it->second));
            }
          } break;

          default:
            break;
        }
      } break;

      default:
        break;
    }

    if (ast->getType().isUndefined()
        && (op == StyioOpType::Binary_Add
            || op == StyioOpType::Binary_Sub
            || op == StyioOpType::Binary_Mul
            || op == StyioOpType::Binary_Div
            || op == StyioOpType::Binary_Mod
            || op == StyioOpType::Binary_Pow)) {
      StyioDataType lhs_type = infer_expr_type(this, lhs);
      StyioDataType rhs_type = infer_expr_type(this, rhs);
      if (type_is_numeric_family(lhs_type) && type_is_numeric_family(rhs_type)) {
        ast->setDType(getMaxType(lhs_type, rhs_type));
      }
    }
  }
  else {
    /* transfer the type of this binop to the child binop */
    if (lhs->getNodeType() == StyioNodeType::BinOp) {
      auto lhs_binop = static_cast<BinOpAST*>(lhs);
      lhs_binop->setDType(ast->getType());
      lhs->typeInfer(this);
    }

    if (rhs->getNodeType() == StyioNodeType::BinOp) {
      auto rhs_binop = static_cast<BinOpAST*>(rhs);
      rhs_binop->setDType(ast->getType());
      rhs->typeInfer(this);
    }

    return;
  }
}

void
StyioSemaContext::typeInfer(FmtStrAST* ast) {
}

void
StyioSemaContext::typeInfer(ResourceAST* ast) {
}

void
StyioSemaContext::typeInfer(ResPathAST* ast) {
}

void
StyioSemaContext::typeInfer(RemotePathAST* ast) {
}

void
StyioSemaContext::typeInfer(WebUrlAST* ast) {
}

void
StyioSemaContext::typeInfer(DBUrlAST* ast) {
}

void
StyioSemaContext::typeInfer(ExtPackAST* ast) {
}

void
StyioSemaContext::typeInfer(ExportDeclAST* ast) {
}

void
StyioSemaContext::typeInfer(ExternBlockAST* ast) {
}

void
StyioSemaContext::typeInfer(ReadFileAST* ast) {
}

void
StyioSemaContext::typeInfer(EOFAST* ast) {
}

void
StyioSemaContext::typeInfer(BreakAST* ast) {
}

void
StyioSemaContext::typeInfer(ContinueAST* ast) {
  (void)ast;
}

void
StyioSemaContext::typeInfer(PassAST* ast) {
}

void
StyioSemaContext::typeInfer(ReturnAST* ast) {
}

void
StyioSemaContext::typeInfer(FuncCallAST* ast) {
  if (ast->func_callee != nullptr) {
    ast->func_callee->typeInfer(this);
  }

  if (ast->func_callee != nullptr && is_predefined_list_operation_name(ast->getNameAsStr())) {
    for (auto* arg : ast->getArgList()) {
      arg->typeInfer(this);
    }

    StyioDataType callee_type = infer_expr_type(this, ast->func_callee);
    if (!styio_is_list_type(callee_type)) {
      throw StyioTypeError(
        "predefined list operation `" + ast->getNameAsStr() + "` requires a list[T] receiver");
    }

    StyioDataType elem_type = styio_data_type_from_name(styio_type_item_type_name(callee_type));
    if (!styio_type_supports_runtime_list_elem(elem_type)) {
      throw StyioTypeError(
        "predefined list operation `" + ast->getNameAsStr()
        + "` requires a runtime list element family");
    }

    if (ast->getNameAsStr() == "push") {
      if (ast->getArgList().size() != 1) {
        throw StyioTypeError("list.push(value) requires exactly one argument");
      }
      StyioDataType value_type = infer_expr_type(this, ast->getArgList()[0]);
      if (!container_value_assignable(elem_type, value_type)) {
        throw StyioTypeError(
          "list.push(value) expects `" + elem_type.name + "`, got `" + value_type.name + "`");
      }
      return;
    }

    if (ast->getNameAsStr() == "insert") {
      if (ast->getArgList().size() != 2) {
        throw StyioTypeError("list.insert(index, value) requires exactly two arguments");
      }
      StyioDataType index_type = infer_expr_type(this, ast->getArgList()[0]);
      if (index_type.option != StyioDataTypeOption::Integer) {
        throw StyioTypeError("list.insert(index, value) requires an integer index");
      }
      StyioDataType value_type = infer_expr_type(this, ast->getArgList()[1]);
      if (!container_value_assignable(elem_type, value_type)) {
        throw StyioTypeError(
          "list.insert(index, value) expects `" + elem_type.name + "`, got `"
          + value_type.name + "`");
      }
      return;
    }

    if (!ast->getArgList().empty()) {
      throw StyioTypeError("list.pop() does not take arguments");
    }
    return;
  }

  if (ast->func_callee != nullptr && is_predefined_string_operation_name(ast->getNameAsStr())) {
    for (auto* arg : ast->getArgList()) {
      arg->typeInfer(this);
    }
    StyioDataType callee_type = infer_expr_type(this, ast->func_callee);
    if (callee_type.option != StyioDataTypeOption::String) {
      throw StyioTypeError("string.lines() requires a string receiver");
    }
    if (!ast->getArgList().empty()) {
      throw StyioTypeError("string.lines() does not take arguments");
    }
    return;
  }

  if (ast->isCallableApply()) {
    throw StyioTypeError(
      "one-shot continuation resume `<|` requires continuation lowering; "
      "captured continuations must be resumed or discontinued exactly once");
  }

  if (ast->func_callee == nullptr && is_matrix_intrinsic_name(ast->getNameAsStr())) {
    for (auto* arg : ast->getArgList()) {
      arg->typeInfer(this);
    }
    (void)infer_matrix_intrinsic_type(this, ast);
    return;
  }

  auto def_it = func_defs.find(ast->getNameAsStr());
  vector<StyioDataType> arg_types;

  for (auto arg : ast->getArgList()) {
    arg->typeInfer(this);
    arg_types.push_back(infer_expr_type(this, arg));
  }

  if (def_it == func_defs.end()) {
    auto native_it = native_func_defs.find(ast->getNameAsStr());
    if (native_it == native_func_defs.end()) {
      throw StyioTypeError("unknown function `" + ast->getNameAsStr() + "`");
    }
    const auto& native = native_it->second;
    if (arg_types.size() != native.arg_types.size()) {
      throw StyioTypeError(
        "function `" + ast->getNameAsStr() + "` expects "
        + std::to_string(native.arg_types.size()) + " argument(s), got "
        + std::to_string(arg_types.size()));
    }
    for (size_t i = 0; i < native.arg_types.size(); ++i) {
      if (!func_param_accepts_arg(native.arg_types[i], arg_types[i])) {
        throw StyioTypeError(
          "function argument type mismatch for native parameter "
          + std::to_string(i) + ": expected " + native.arg_types[i].name
          + ", got " + arg_types[i].name);
      }
    }
    return;
  }

  auto func_args = params_of_func_def(def_it->second);

  if (arg_types.size() != func_args.size()) {
    throw StyioTypeError(
      "function `" + ast->getNameAsStr() + "` expects "
      + std::to_string(func_args.size()) + " argument(s), got "
      + std::to_string(arg_types.size()));
  }

  for (size_t i = 0; i < func_args.size(); i++) {
    StyioDataType declared_type = func_args[i]->getDType()->getDataType();
    if (declared_type.isUndefined()) {
      func_args[i]->setDataType(arg_types[i]);
      continue;
    }
    if (!func_param_accepts_arg(declared_type, arg_types[i])) {
      throw StyioTypeError(
        "function argument type mismatch for parameter '" + func_args[i]->getNameAsStr()
        + "': expected " + declared_type.name + ", got " + arg_types[i].name);
    }
  }
}

void
StyioSemaContext::typeInfer(AttrAST* ast) {
  ast->body->typeInfer(this);
  auto* attr_name = dynamic_cast<NameAST*>(ast->attr);
  if (attr_name == nullptr) {
    throw StyioTypeError("attribute access requires a simple name");
  }
  StyioDataType body_type = infer_expr_type(this, ast->body);
  if (attr_name->getAsStr() == "length" || attr_name->getAsStr() == "size") {
    if (!styio_type_is_sized(body_type)) {
      throw StyioTypeError(".length/.size require a sized value");
    }
    return;
  }
  if ((attr_name->getAsStr() == "keys" || attr_name->getAsStr() == "values")
      && styio_is_dict_type(body_type)) {
    return;
  }
  throw StyioTypeError("only .length, .size, .keys, and .values are supported");
}

void
StyioSemaContext::typeInfer(PrintAST* ast) {
  for (auto* e : ast->exprs) {
    e->typeInfer(this);
  }
}

void
StyioSemaContext::typeInfer(ForwardAST* ast) {
}

void
StyioSemaContext::typeInfer(BackwardAST* ast) {
}

void
StyioSemaContext::typeInfer(CODPAST* ast) {
}

void
StyioSemaContext::typeInfer(CheckEqualAST* ast) {
}

void
StyioSemaContext::typeInfer(CheckIsinAST* ast) {
}

void
StyioSemaContext::typeInfer(HashTagNameAST* ast) {
}

void
StyioSemaContext::typeInfer(CondFlowAST* ast) {
  ast->getCond()->typeInfer(this);
  auto saved = local_binding_types;
  auto saved_fixed = fixed_assignment_names_;
  auto saved_bind = binding_info_;

  ast->getThen()->typeInfer(this);
  auto then_types = local_binding_types;
  auto then_bind = binding_info_;

  local_binding_types = saved;
  fixed_assignment_names_ = saved_fixed;
  binding_info_ = saved_bind;

  if (ast->getElse() != nullptr) {
    ast->getElse()->typeInfer(this);
    auto else_types = local_binding_types;
    auto else_bind = binding_info_;

    local_binding_types = saved;
    fixed_assignment_names_ = saved_fixed;
    binding_info_ = saved_bind;

    for (auto const& entry : then_bind) {
      auto eit = else_bind.find(entry.first);
      if (eit == else_bind.end()) {
        continue;
      }
      BindingInfo merged = entry.second;
      if (entry.second.value_kind != eit->second.value_kind
          || entry.second.resource_value != eit->second.resource_value) {
        merged.dynamic_slot = true;
        merged.resource_value = false;
        merged.value_kind = BindingValueKind::Unknown;
        merged.declared_type = StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
      }
      binding_info_[entry.first] = merged;

      auto tit = then_types.find(entry.first);
      auto eit_ty = else_types.find(entry.first);
      auto sit = saved.find(entry.first);
      const StyioDataType then_type = tit != then_types.end()
        ? tit->second
        : StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
      const StyioDataType else_type = eit_ty != else_types.end()
        ? eit_ty->second
        : StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
      const StyioDataType saved_type = sit != saved.end()
        ? sit->second
        : StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
      local_binding_types[entry.first] =
        merge_cond_flow_branch_type(then_type, else_type, saved_type);
    }
    return;
  }

  local_binding_types = saved;
  fixed_assignment_names_ = saved_fixed;
  binding_info_ = saved_bind;
}

void
StyioSemaContext::typeInfer(AnonyFuncAST* ast) {
}

void
StyioSemaContext::typeInfer(FunctionAST* ast) {
  func_defs[ast->getNameAsStr()] = ast;
}

void
StyioSemaContext::typeInfer(SimpleFuncAST* ast) {
  func_defs[ast->func_name->getAsStr()] = ast;
}

void
StyioSemaContext::typeInfer(IteratorAST* ast) {
  auto saved = local_binding_types;
  auto saved_fixed = fixed_assignment_names_;
  auto saved_bind = binding_info_;
  ast->collection->typeInfer(this);
  StyioDataType collection_type = infer_expr_type(this, ast->collection);
  if (!styio_type_is_iterable(collection_type)) {
    throw StyioTypeError("iteration requires an iterable value");
  }
  StyioDataType et = infer_collection_elem_type(this, ast->collection);
  if (!ast->params.empty()) {
    local_binding_types[ast->params[0]->getNameAsStr()] = et;
  }
  for (auto* f : ast->following) {
    f->typeInfer(this);
  }
  local_binding_types = std::move(saved);
  fixed_assignment_names_ = std::move(saved_fixed);
  binding_info_ = std::move(saved_bind);
}

void
StyioSemaContext::typeInfer(StreamZipAST* ast) {
  auto saved = local_binding_types;
  auto saved_fixed = fixed_assignment_names_;
  auto saved_bind = binding_info_;
  ast->getCollectionA()->typeInfer(this);
  ast->getCollectionB()->typeInfer(this);
  StyioDataType ta = infer_expr_type(this, ast->getCollectionA());
  StyioDataType tb = infer_expr_type(this, ast->getCollectionB());
  if (!styio_type_is_iterable(ta) || !styio_type_is_iterable(tb)) {
    throw StyioTypeError("zip requires iterable inputs on both sides");
  }
  StyioDataType ea = infer_collection_elem_type(this, ast->getCollectionA());
  StyioDataType eb = infer_collection_elem_type(this, ast->getCollectionB());
  if (!ast->getParamsA().empty()) {
    local_binding_types[ast->getParamsA()[0]->getNameAsStr()] = ea;
  }
  if (!ast->getParamsB().empty()) {
    local_binding_types[ast->getParamsB()[0]->getNameAsStr()] = eb;
  }
  for (auto* f : ast->getFollowing()) {
    f->typeInfer(this);
  }
  local_binding_types = std::move(saved);
  fixed_assignment_names_ = std::move(saved_fixed);
  binding_info_ = std::move(saved_bind);
}

void
StyioSemaContext::typeInfer(SnapshotDeclAST* ast) {
  snapshot_var_names_.insert(ast->getVar()->getAsStr());
  local_binding_types[ast->getVar()->getAsStr()] =
    StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  ast->getResource()->typeInfer(this);
}

void
StyioSemaContext::typeInfer(InstantPullAST* ast) {
  ast->getResource()->typeInfer(this);
}

void
StyioSemaContext::typeInfer(TypedStdinListAST* ast) {
  ast->getListType()->typeInfer(this);
}

void
StyioSemaContext::typeInfer(IterSeqAST* ast) {
}


void
StyioSemaContext::typeInfer(InfiniteLoopAST* ast) {
  if (ast->getWhileCond() != nullptr) {
    ast->getWhileCond()->typeInfer(this);
    const StyioDataType cond_type = infer_expr_type(this, ast->getWhileCond());
    if (!type_is_bool(cond_type)) {
      throw StyioTypeError("conditional loop guard must have bool type");
    }
  }
  if (ast->getBody() != nullptr) {
    ast->getBody()->typeInfer(this);
  }
}

void
StyioSemaContext::typeInfer(CasesAST* ast) {
}

void
StyioSemaContext::typeInfer(MatchCasesAST* ast) {
}

void
StyioSemaContext::typeInfer(BlockAST* ast) {
  for (auto* s : ast->stmts) {
    s->typeInfer(this);
  }
}

void
StyioSemaContext::typeInfer(StateDeclAST* ast) {
  if (ast->getAccInit()) {
    ast->getAccInit()->typeInfer(this);
  }
  ast->getUpdateExpr()->typeInfer(this);
}

void
StyioSemaContext::typeInfer(StateRefAST* ast) {
  (void)ast;
}

void
StyioSemaContext::typeInfer(HistoryProbeAST* ast) {
  ast->getDepth()->typeInfer(this);
}

void
StyioSemaContext::typeInfer(SeriesIntrinsicAST* ast) {
  ast->getBase()->typeInfer(this);
  ast->getWindow()->typeInfer(this);
}

void
StyioSemaContext::typeInfer(MainBlockAST* ast) {
  snapshot_var_names_.clear();
  func_defs.clear();
  native_func_defs.clear();
  local_binding_types.clear();
  fixed_assignment_names_.clear();
  binding_info_.clear();
  collect_bind_resource_writes_.clear();
  collect_bind_handle_acquires_.clear();
  collect_bind_resource_write_types_.clear();
  collect_bind_handle_acquire_types_.clear();
  auto stmts = ast->getStmts();
  std::vector<std::string> exported_symbols;
  for (auto const& s : stmts) {
    if (auto* export_decl = dynamic_cast<ExportDeclAST*>(s)) {
      const auto& symbols = export_decl->getSymbols();
      exported_symbols.insert(exported_symbols.end(), symbols.begin(), symbols.end());
    }
  }
  const std::unordered_set<std::string> export_filter(
    exported_symbols.begin(),
    exported_symbols.end());
  for (auto const& s : stmts) {
    if (auto* f = dynamic_cast<FunctionAST*>(s)) {
      func_defs[f->getNameAsStr()] = f;
      continue;
    }
    if (auto* sf = dynamic_cast<SimpleFuncAST*>(s)) {
      func_defs[sf->func_name->getAsStr()] = sf;
      continue;
    }
    if (auto* ex = dynamic_cast<ExternBlockAST*>(s)) {
      const auto signatures = styio::native::parse_function_signatures(ex->getBody());
      for (const auto& sig : signatures) {
        if (!export_filter.empty() && export_filter.find(sig.name) == export_filter.end()) {
          continue;
        }
        NativeFunctionType native_type;
        native_type.return_type = styio::native::styio_data_type_for_c_type(sig.return_type);
        for (const auto& param : sig.params) {
          native_type.arg_types.push_back(styio::native::styio_data_type_for_c_type(param.type));
        }
        native_func_defs[sig.name] = std::move(native_type);
      }
    }
  }
  for (auto const& s : stmts) {
    s->typeInfer(this);
  }
}
