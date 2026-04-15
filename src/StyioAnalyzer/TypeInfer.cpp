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
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "Util.hpp"

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
infer_expr_type(StyioAnalyzer* an, StyioAST* expr);

StyioDataType
infer_list_literal_type(StyioAnalyzer* an, ListAST* list) {
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

bool
type_is_numeric_family(const StyioDataType& type) {
  StyioValueFamily family = styio_value_family_for_type(type);
  return family == StyioValueFamily::Integer
    || family == StyioValueFamily::Float;
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

StyioDataType
infer_predefined_list_operation_type(StyioAnalyzer* an, FuncCallAST* call) {
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
func_ret_type_of_def(StyioAnalyzer* an, StyioAST* def) {
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
infer_dict_literal_type(StyioAnalyzer* an, DictAST* dict) {
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
infer_expr_type(StyioAnalyzer* an, StyioAST* expr) {
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
      auto it = an->func_defs.find(call->getNameAsStr());
      if (it != an->func_defs.end()) {
        return func_ret_type_of_def(an, it->second);
      }
      return expr->getDataType();
    }
    default:
      return expr->getDataType();
  }
}

StyioDataType
infer_collection_elem_type(StyioAnalyzer* an, StyioAST* coll) {
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

std::optional<bool>
expr_is_string_hint(StyioAnalyzer* an, StyioAST* x) {
  StyioDataType t = infer_expr_type(an, x);
  if (t.isUndefined()) {
    return std::nullopt;
  }
  return type_is_string(t);
}

std::optional<bool>
expr_is_intish_hint(StyioAnalyzer* an, StyioAST* x) {
  StyioDataType t = infer_expr_type(an, x);
  if (t.isUndefined()) {
    return std::nullopt;
  }
  return type_is_intish(t);
}

StyioAnalyzer::BindingValueKind
binding_value_kind_for_type(const StyioDataType& type) {
  switch (styio_value_family_for_type(type)) {
    case StyioValueFamily::ListHandle:
      return StyioAnalyzer::BindingValueKind::ListHandle;
    case StyioValueFamily::DictHandle:
      return StyioAnalyzer::BindingValueKind::DictHandle;
    case StyioValueFamily::String:
      return StyioAnalyzer::BindingValueKind::String;
    case StyioValueFamily::Float:
      return StyioAnalyzer::BindingValueKind::F64;
    case StyioValueFamily::Bool:
      return StyioAnalyzer::BindingValueKind::Bool;
    case StyioValueFamily::Integer:
      return StyioAnalyzer::BindingValueKind::I64;
    default:
      break;
  }
  return StyioAnalyzer::BindingValueKind::Unknown;
}

bool
infer_concat_string_add(StyioAnalyzer* an, BinOpAST* ast, StyioAST* lhs, StyioAST* rhs) {
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
infer_numeric_string_coercion(StyioAnalyzer* an, BinOpAST* ast, StyioAST* lhs, StyioAST* rhs) {
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
StyioAnalyzer::typeInfer(CommentAST* ast) {
}

void
StyioAnalyzer::typeInfer(NoneAST* ast) {
}

void
StyioAnalyzer::typeInfer(EmptyAST* ast) {
}

void
StyioAnalyzer::typeInfer(NameAST* ast) {
}

void
StyioAnalyzer::typeInfer(TypeAST* ast) {
}

void
StyioAnalyzer::typeInfer(TypeTupleAST* ast) {
}

void
StyioAnalyzer::typeInfer(BoolAST* ast) {
}

void
StyioAnalyzer::typeInfer(IntAST* ast) {
}

void
StyioAnalyzer::typeInfer(FloatAST* ast) {
}

void
StyioAnalyzer::typeInfer(CharAST* ast) {
}

void
StyioAnalyzer::typeInfer(StringAST* ast) {
}

void
StyioAnalyzer::typeInfer(TypeConvertAST*) {
}

void
StyioAnalyzer::typeInfer(VarAST* ast) {
}

void
StyioAnalyzer::typeInfer(ParamAST* ast) {
}

void
StyioAnalyzer::typeInfer(OptArgAST* ast) {
}

void
StyioAnalyzer::typeInfer(OptKwArgAST* ast) {
}

/*
  The declared type is always the *top* priority
  because the programmer wrote in that way!
*/
void
StyioAnalyzer::typeInfer(FlexBindAST* ast) {
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
      static_cast<BinOpAST*>(ast->getValue())->setDType(var_type);
    }
  }

  ast->getValue()->typeInfer(this);

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

  BindingValueKind kind = expr_value_kind(ast->getValue());
  StyioDataType concrete_type = inferred_rhs_type;
  if (var_type.option != StyioDataTypeOption::Undefined) {
    concrete_type = var_type;
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
  info.resource_value = kind == BindingValueKind::ListHandle
    || kind == BindingValueKind::DictHandle;
  info.value_kind = kind;
  info.declared_type = info.dynamic_slot
    ? StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0}
    : concrete_type;
  binding_info_[bound_name] = info;
}

void
StyioAnalyzer::typeInfer(FinalBindAST* ast) {
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
  if (ast->getValue()->getNodeType() == StyioNodeType::BinOp) {
    static_cast<BinOpAST*>(ast->getValue())->setDType(vt);
    ast->getValue()->typeInfer(this);
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
    || (rhs_info != binding_info_.end()
        && (rhs_info->second.value_kind == BindingValueKind::ListHandle
            || rhs_info->second.value_kind == BindingValueKind::DictHandle));
  info.resource_value = runtime_resource;
  if (ast->getValue()->getNodeType() == StyioNodeType::Dict) {
    info.value_kind = BindingValueKind::DictHandle;
  }
  else if (runtime_resource) {
    info.value_kind =
      ast->getValue()->getNodeType() == StyioNodeType::TypedStdinList
        ? BindingValueKind::ListHandle
        : (rhs_info != binding_info_.end() ? rhs_info->second.value_kind : rhs_kind);
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
StyioAnalyzer::typeInfer(ParallelAssignAST* ast) {
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
StyioAnalyzer::typeInfer(InfiniteAST* ast) {
}

void
StyioAnalyzer::typeInfer(StructAST* ast) {
}

void
StyioAnalyzer::typeInfer(TupleAST* ast) {
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
StyioAnalyzer::typeInfer(VarTupleAST* ast) {
}

void
StyioAnalyzer::typeInfer(ExtractorAST* ast) {
}

void
StyioAnalyzer::typeInfer(RangeAST* ast) {
}

void
StyioAnalyzer::typeInfer(SetAST* ast) {
}

void
StyioAnalyzer::typeInfer(ListAST* ast) {
  for (auto* elem : ast->getElements()) {
    elem->typeInfer(this);
  }
  ast->setConsistency(true);
  ast->setDataType(infer_list_literal_type(this, ast));
}

void
StyioAnalyzer::typeInfer(DictAST* ast) {
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
StyioAnalyzer::typeInfer(SizeOfAST* ast) {
}

void
StyioAnalyzer::typeInfer(ListOpAST* ast) {
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
StyioAnalyzer::typeInfer(BinCompAST* ast) {
  ast->getLHS()->typeInfer(this);
  ast->getRHS()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(CondAST* ast) {
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
StyioAnalyzer::typeInfer(UndefinedLitAST* ast) {
  (void)ast;
}

void
StyioAnalyzer::typeInfer(WaveMergeAST* ast) {
  ast->getCond()->typeInfer(this);
  ast->getTrueVal()->typeInfer(this);
  ast->getFalseVal()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(WaveDispatchAST* ast) {
  ast->getCond()->typeInfer(this);
  ast->getTrueArm()->typeInfer(this);
  ast->getFalseArm()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(FallbackAST* ast) {
  ast->getPrimary()->typeInfer(this);
  ast->getAlternate()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(GuardSelectorAST* ast) {
  ast->getBase()->typeInfer(this);
  ast->getCond()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(EqProbeAST* ast) {
  ast->getBase()->typeInfer(this);
  ast->getProbeValue()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(FileResourceAST* ast) {
  ast->getPath()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(StdStreamAST* ast) {
  /* No children to infer. */
}

void
StyioAnalyzer::typeInfer(HandleAcquireAST* ast) {
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
StyioAnalyzer::typeInfer(ResourceWriteAST* ast) {
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
}

void
StyioAnalyzer::typeInfer(ResourceRedirectAST* ast) {
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
StyioAnalyzer::typeInfer(BinOpAST* ast) {
  auto lhs = ast->getLHS();
  auto rhs = ast->getRHS();
  auto op = ast->getOp();

  if (op == StyioOpType::Self_Add_Assign || op == StyioOpType::Self_Sub_Assign
      || op == StyioOpType::Self_Mul_Assign || op == StyioOpType::Self_Div_Assign
      || op == StyioOpType::Self_Mod_Assign) {
    rhs->typeInfer(this);
    auto* nm = static_cast<NameAST*>(lhs);
    auto it = local_binding_types.find(nm->getAsStr());
    if (it != local_binding_types.end()) {
      ast->setDType(it->second);
    }
    else {
      ast->setDType(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
    }
    return;
  }

  if (ast->getType().isUndefined()) {
    lhs->typeInfer(this);
    rhs->typeInfer(this);
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
StyioAnalyzer::typeInfer(FmtStrAST* ast) {
}

void
StyioAnalyzer::typeInfer(ResourceAST* ast) {
}

void
StyioAnalyzer::typeInfer(ResPathAST* ast) {
}

void
StyioAnalyzer::typeInfer(RemotePathAST* ast) {
}

void
StyioAnalyzer::typeInfer(WebUrlAST* ast) {
}

void
StyioAnalyzer::typeInfer(DBUrlAST* ast) {
}

void
StyioAnalyzer::typeInfer(ExtPackAST* ast) {
}

void
StyioAnalyzer::typeInfer(ReadFileAST* ast) {
}

void
StyioAnalyzer::typeInfer(EOFAST* ast) {
}

void
StyioAnalyzer::typeInfer(BreakAST* ast) {
}

void
StyioAnalyzer::typeInfer(ContinueAST* ast) {
  (void)ast;
}

void
StyioAnalyzer::typeInfer(PassAST* ast) {
}

void
StyioAnalyzer::typeInfer(ReturnAST* ast) {
}

void
StyioAnalyzer::typeInfer(FuncCallAST* ast) {
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

  auto def_it = func_defs.find(ast->getNameAsStr());
  if (def_it == func_defs.end()) {
    return;
  }

  vector<StyioDataType> arg_types;

  for (auto arg : ast->getArgList()) {
    arg->typeInfer(this);
    arg_types.push_back(infer_expr_type(this, arg));
  }

  auto func_args = params_of_func_def(def_it->second);

  if (arg_types.size() != func_args.size()) {
    return;
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
StyioAnalyzer::typeInfer(AttrAST* ast) {
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
StyioAnalyzer::typeInfer(PrintAST* ast) {
  for (auto* e : ast->exprs) {
    e->typeInfer(this);
  }
}

void
StyioAnalyzer::typeInfer(ForwardAST* ast) {
}

void
StyioAnalyzer::typeInfer(BackwardAST* ast) {
}

void
StyioAnalyzer::typeInfer(CODPAST* ast) {
}

void
StyioAnalyzer::typeInfer(CheckEqualAST* ast) {
}

void
StyioAnalyzer::typeInfer(CheckIsinAST* ast) {
}

void
StyioAnalyzer::typeInfer(HashTagNameAST* ast) {
}

void
StyioAnalyzer::typeInfer(CondFlowAST* ast) {
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
      if (tit != then_types.end() && eit_ty != else_types.end() && tit->second.equals(eit_ty->second)) {
        local_binding_types[entry.first] = tit->second;
      }
      else {
        local_binding_types[entry.first] =
          StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
      }
    }
    return;
  }

  local_binding_types = saved;
  fixed_assignment_names_ = saved_fixed;
  binding_info_ = saved_bind;
}

void
StyioAnalyzer::typeInfer(AnonyFuncAST* ast) {
}

void
StyioAnalyzer::typeInfer(FunctionAST* ast) {
  func_defs[ast->getNameAsStr()] = ast;
}

void
StyioAnalyzer::typeInfer(SimpleFuncAST* ast) {
  func_defs[ast->func_name->getAsStr()] = ast;
}

void
StyioAnalyzer::typeInfer(IteratorAST* ast) {
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
StyioAnalyzer::typeInfer(StreamZipAST* ast) {
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
StyioAnalyzer::typeInfer(SnapshotDeclAST* ast) {
  snapshot_var_names_.insert(ast->getVar()->getAsStr());
  local_binding_types[ast->getVar()->getAsStr()] =
    StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  ast->getResource()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(InstantPullAST* ast) {
  ast->getResource()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(TypedStdinListAST* ast) {
  ast->getListType()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(IterSeqAST* ast) {
}


void
StyioAnalyzer::typeInfer(InfiniteLoopAST* ast) {
}

void
StyioAnalyzer::typeInfer(CasesAST* ast) {
}

void
StyioAnalyzer::typeInfer(MatchCasesAST* ast) {
}

void
StyioAnalyzer::typeInfer(BlockAST* ast) {
  for (auto* s : ast->stmts) {
    s->typeInfer(this);
  }
}

void
StyioAnalyzer::typeInfer(StateDeclAST* ast) {
  if (ast->getAccInit()) {
    ast->getAccInit()->typeInfer(this);
  }
  ast->getUpdateExpr()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(StateRefAST* ast) {
  (void)ast;
}

void
StyioAnalyzer::typeInfer(HistoryProbeAST* ast) {
  ast->getDepth()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(SeriesIntrinsicAST* ast) {
  ast->getBase()->typeInfer(this);
  ast->getWindow()->typeInfer(this);
}

void
StyioAnalyzer::typeInfer(MainBlockAST* ast) {
  snapshot_var_names_.clear();
  local_binding_types.clear();
  fixed_assignment_names_.clear();
  binding_info_.clear();
  collect_bind_resource_writes_.clear();
  collect_bind_handle_acquires_.clear();
  collect_bind_resource_write_types_.clear();
  collect_bind_handle_acquire_types_.clear();
  auto stmts = ast->getStmts();
  for (auto const& s : stmts) {
    s->typeInfer(this);
  }
}
