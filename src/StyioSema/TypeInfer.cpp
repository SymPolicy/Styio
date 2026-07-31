/*
  Type Inference Implementation

  - Label Types
  - Find Recursive Type
*/

// [C++ STL]
#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioNative/NativeInterop.hpp"
#include "../StyioResourceTopology/ResourceTopology.hpp"
#include "../StyioToken/Token.hpp"
#include "../StyioToString/ToStringVisitor.hpp"
#include "../StyioUtil/BuiltinMethods.hpp"
#include "../StyioUtil/IOIntrinsics.hpp"
#include "../StyioUtil/ResourceNames.hpp"
#include "CallableInterface.hpp"

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

static bool
callable_def_is_final_binding_latest(StyioAST* def) {
  if (auto* f = dynamic_cast<FunctionAST*>(def)) {
    return f->is_unique;
  }
  if (auto* s = dynamic_cast<SimpleFuncAST*>(def)) {
    return s->is_unique;
  }
  return false;
}

void
StyioSemaContext::record_function_def(
  const std::string& name,
  styio::session::SymbolId sid,
  StyioAST* def
) {
  if (sid == styio::session::kInvalidSymbolId) {
    sid = intern_semantic_symbol(name);
  }

  StyioAST* existing = nullptr;
  if (sid != styio::session::kInvalidSymbolId) {
    auto sid_it = func_defs_by_sid.find(sid);
    if (sid_it != func_defs_by_sid.end()) {
      existing = sid_it->second;
    }
  }
  if (existing == nullptr) {
    auto it = func_defs.find(name);
    if (it != func_defs.end()) {
      existing = it->second;
    }
  }

  if (existing != nullptr
      && existing != def
      && active_function_body_stack_.empty()) {
    const bool existing_final = callable_def_is_final_binding_latest(existing);
    const bool incoming_final = callable_def_is_final_binding_latest(def);
    if (existing_final && !incoming_final) {
      throw StyioTypeError(
        "callable `" + name
        + "` was defined with `:=` (final binding); cannot rebind with `=`"
      );
    }
    if (incoming_final) {
      throw StyioTypeError(
        "callable `" + name
        + "` cannot be redefined with `:=` after an existing binding"
      );
    }
  }

  func_defs[name] = def;
  if (sid != styio::session::kInvalidSymbolId) {
    func_defs_by_sid[sid] = def;
  }
}

void
StyioSemaContext::install_imported_callable_definition(
  std::string module_id,
  bool exported,
  bool has_scheme,
  StyioAST* definition,
  CallableTypeScheme scheme,
  CallableEffectSummary effects,
  std::vector<StyioDataType> concrete_params,
  StyioDataType concrete_result,
  std::vector<std::string> visible_from_modules,
  std::string checked_body_digest,
  std::string interface_abi_digest
) {
  if (module_id.empty() || definition == nullptr) {
    throw StyioTypeError(
      "imported callable definition requires a module id and checked body"
    );
  }
  std::string name;
  if (auto* function = dynamic_cast<FunctionAST*>(definition)) {
    name = function->getNameAsStr();
  }
  else if (auto* function = dynamic_cast<SimpleFuncAST*>(definition)) {
    name = function->func_name->getAsStr();
  }
  if (name.empty()) {
    throw StyioTypeError(
      "imported callable interface entry does not match a function body"
    );
  }
  if (has_scheme && scheme.name != name) {
    throw StyioTypeError(
      "imported callable scheme name does not match body `" + name + "`"
    );
  }
  auto existing = imported_callable_definition_indices_.find(name);
  if (existing != imported_callable_definition_indices_.end()) {
    auto& info = imported_callable_definitions_[existing->second];
    if (info.module_id != module_id || info.definition != definition) {
      throw StyioTypeError(
        "imported callable name collision for `" + name
        + "` between modules `" + info.module_id
        + "` and `" + module_id + "`"
      );
    }
    info.exported = info.exported || exported;
    info.visible_from_modules.insert(
      visible_from_modules.begin(),
      visible_from_modules.end());
    return;
  }

  const auto params = params_of_func_def(definition);
  if (!has_scheme) {
    if (params.size() != concrete_params.size()
        || concrete_result.isUndefined()) {
      throw StyioTypeError(
        "imported concrete callable `" + name
        + "` has incomplete interface facts"
      );
    }
    for (std::size_t i = 0; i < params.size(); ++i) {
      params[i]->setDataType(concrete_params[i]);
    }
  }

  ImportedCallableDefinition info;
  info.module_id = std::move(module_id);
  info.exported = exported;
  info.has_scheme = has_scheme;
  info.definition = definition;
  info.scheme = std::move(scheme);
  info.effects = std::move(effects);
  info.concrete_params = std::move(concrete_params);
  info.concrete_result = std::move(concrete_result);
  info.visible_from_modules.insert(
    visible_from_modules.begin(),
    visible_from_modules.end());
  info.checked_body_digest = std::move(checked_body_digest);
  info.interface_abi_digest = std::move(interface_abi_digest);
  imported_callable_definition_indices_[name] =
    imported_callable_definitions_.size();
  imported_callable_definitions_.push_back(std::move(info));
}

const StyioSemaContext::ImportedCallableDefinition*
StyioSemaContext::find_imported_callable_definition(
  std::string_view name
) const {
  auto it =
    imported_callable_definition_indices_.find(std::string(name));
  if (it == imported_callable_definition_indices_.end()) {
    return nullptr;
  }
  return &imported_callable_definitions_.at(it->second);
}

bool
StyioSemaContext::imported_callable_is_visible(
  std::string_view name
) const {
  const ImportedCallableDefinition* imported =
    find_imported_callable_definition(name);
  if (imported == nullptr) {
    return true;
  }

  std::string current_module;
  if (!active_function_body_stack_.empty()) {
    const ImportedCallableDefinition* active =
      find_imported_callable_definition(
        active_function_body_stack_.back());
    if (active != nullptr) {
      current_module = active->module_id;
    }
  }
  if (current_module == imported->module_id) {
    return true;
  }
  return imported->exported
         && imported->visible_from_modules.count(current_module) != 0;
}

void
StyioSemaContext::register_imported_callable_definitions() {
  const auto imported_name = [](StyioAST* definition) -> std::string
  {
    if (auto* function = dynamic_cast<FunctionAST*>(definition)) {
      return function->getNameAsStr();
    }
    if (auto* function = dynamic_cast<SimpleFuncAST*>(definition)) {
      return function->func_name->getAsStr();
    }
    return {};
  };
  std::vector<const ImportedCallableDefinition*> ordered;
  ordered.reserve(imported_callable_definitions_.size());
  for (const auto& imported : imported_callable_definitions_) {
    ordered.push_back(&imported);
  }
  std::sort(
    ordered.begin(),
    ordered.end(),
    [](const auto* lhs, const auto* rhs)
    {
      if (lhs->module_id != rhs->module_id) {
        return lhs->module_id < rhs->module_id;
      }
      const auto name_of = [](StyioAST* definition) -> std::string
      {
        if (auto* function = dynamic_cast<FunctionAST*>(definition)) {
          return function->getNameAsStr();
        }
        if (auto* function = dynamic_cast<SimpleFuncAST*>(definition)) {
          return function->func_name->getAsStr();
        }
        return {};
      };
      const std::string lhs_name = name_of(lhs->definition);
      const std::string rhs_name = name_of(rhs->definition);
      return lhs_name < rhs_name;
    });
  for (const auto* imported : ordered) {
    const std::string name = imported_name(imported->definition);
    styio::session::SymbolId sid =
      styio::session::kInvalidSymbolId;
    if (auto* function =
          dynamic_cast<FunctionAST*>(imported->definition)) {
      sid = function->func_name->getSymbolId();
    }
    else if (auto* function =
               dynamic_cast<SimpleFuncAST*>(imported->definition)) {
      sid = function->func_name->getSymbolId();
    }
    record_function_def(name, sid, imported->definition);
    if (!imported->has_scheme
        && !imported->concrete_result.isUndefined()) {
      inferred_function_return_types_[name] =
        imported->concrete_result;
      const auto interned = intern_semantic_symbol(name);
      if (interned != styio::session::kInvalidSymbolId) {
        inferred_function_return_types_by_sid_[interned] =
          imported->concrete_result;
      }
    }
  }
}

void
StyioSemaContext::configure_callable_specialization_environment(
  std::string backend_abi,
  std::string dependency_digest
) {
  if (backend_abi.empty() || dependency_digest.empty()) {
    throw StyioTypeError(
      "callable specialization environment requires backend ABI "
      "and dependency digests"
    );
  }
  callable_specialization_backend_abi_ =
    std::move(backend_abi);
  callable_specialization_dependency_digest_ =
    std::move(dependency_digest);
}

namespace
{

StyioDataType const kBoolType{
  StyioDataTypeOption::Bool, "bool", 1
};

StyioDataType const kI64Type{
  StyioDataTypeOption::Integer, "i64", 64
};

StyioDataType const kF64Type{
  StyioDataTypeOption::Float, "f64", 64
};

StyioDataType const kStringType{
  StyioDataTypeOption::String, "string", 0
};

using CallableTypeTerm = StyioSemaContext::CallableTypeTerm;
using CallableTypeScheme = StyioSemaContext::CallableTypeScheme;
using CallableConstraintKind = StyioSemaContext::CallableConstraintKind;
using CallableTypeConstraint = StyioSemaContext::CallableTypeConstraint;
using CallableEffectKind = StyioSemaContext::CallableEffectKind;
using CallableEffectSummary = StyioSemaContext::CallableEffectSummary;

StyioDataType
normalize_callable_concrete_type(const StyioDataType& input) {
  if (input.name == "int"
      || (input.option == StyioDataTypeOption::Integer
          && input.num_of_bit == 0)) {
    return kI64Type;
  }
  if (input.name == "float"
      || (input.option == StyioDataTypeOption::Float
          && input.num_of_bit == 0)) {
    return kF64Type;
  }
  if (styio_is_list_type(input)) {
    StyioDataType element = normalize_callable_concrete_type(
      styio_data_type_from_name(styio_list_elem_type_name(input)));
    return styio_make_list_type(element.name);
  }
  if (styio_is_dict_type(input)) {
    StyioDataType key = normalize_callable_concrete_type(
      styio_data_type_from_name(styio_dict_key_type_name(input)));
    StyioDataType value = normalize_callable_concrete_type(
      styio_data_type_from_name(styio_dict_value_type_name(input)));
    return styio_make_dict_type(key.name, value.name);
  }
  return input;
}

CallableTypeTerm
callable_variable_term(std::uint32_t variable) {
  CallableTypeTerm term;
  term.kind = CallableTypeTerm::Kind::Variable;
  term.variable = variable;
  return term;
}

CallableTypeTerm
callable_concrete_term(const StyioDataType& input) {
  StyioDataType type = normalize_callable_concrete_type(input);
  if (styio_is_list_type(type)) {
    CallableTypeTerm term;
    term.kind = CallableTypeTerm::Kind::List;
    term.arguments.push_back(
      callable_concrete_term(
        styio_data_type_from_name(styio_list_elem_type_name(type))));
    return term;
  }
  if (styio_is_dict_type(type)) {
    CallableTypeTerm term;
    term.kind = CallableTypeTerm::Kind::Dict;
    term.arguments.push_back(
      callable_concrete_term(
        styio_data_type_from_name(styio_dict_key_type_name(type))));
    term.arguments.push_back(
      callable_concrete_term(
        styio_data_type_from_name(styio_dict_value_type_name(type))));
    return term;
  }

  CallableTypeTerm term;
  term.kind = CallableTypeTerm::Kind::Concrete;
  term.concrete = type;
  return term;
}

CallableTypeTerm
callable_list_term(CallableTypeTerm element) {
  CallableTypeTerm term;
  term.kind = CallableTypeTerm::Kind::List;
  term.arguments.push_back(std::move(element));
  return term;
}

CallableTypeTerm
callable_dict_term(CallableTypeTerm key, CallableTypeTerm value) {
  CallableTypeTerm term;
  term.kind = CallableTypeTerm::Kind::Dict;
  term.arguments.push_back(std::move(key));
  term.arguments.push_back(std::move(value));
  return term;
}

class CallableTypeUnifier
{
  std::uint32_t next_variable_ = 0;
  std::unordered_map<std::uint32_t, CallableTypeTerm> substitutions_;

  bool occurs(
    std::uint32_t variable,
    const CallableTypeTerm& input
  ) {
    CallableTypeTerm term = apply(input);
    if (term.kind == CallableTypeTerm::Kind::Variable) {
      return term.variable == variable;
    }
    for (const auto& argument : term.arguments) {
      if (occurs(variable, argument)) {
        return true;
      }
    }
    return false;
  }

  void bind(
    std::uint32_t variable,
    const CallableTypeTerm& input
  ) {
    CallableTypeTerm term = apply(input);
    if (term.kind == CallableTypeTerm::Kind::Variable
        && term.variable == variable) {
      return;
    }
    if (occurs(variable, term)) {
      throw StyioTypeError(
        "polymorphic recursion requires an infinite inferred type; "
        "recursive calls must reuse one monomorphic group instance"
      );
    }
    substitutions_[variable] = std::move(term);
  }

public:
  CallableTypeTerm fresh() {
    return callable_variable_term(next_variable_++);
  }

  CallableTypeTerm apply(const CallableTypeTerm& input) {
    if (input.kind == CallableTypeTerm::Kind::Variable) {
      auto it = substitutions_.find(input.variable);
      if (it == substitutions_.end()) {
        return input;
      }
      CallableTypeTerm resolved = apply(it->second);
      it->second = resolved;
      return resolved;
    }

    CallableTypeTerm output = input;
    for (auto& argument : output.arguments) {
      argument = apply(argument);
    }
    return output;
  }

  void unify(
    const CallableTypeTerm& lhs_input,
    const CallableTypeTerm& rhs_input,
    const std::string& context
  ) {
    CallableTypeTerm lhs = apply(lhs_input);
    CallableTypeTerm rhs = apply(rhs_input);

    if (lhs.kind == CallableTypeTerm::Kind::Variable) {
      bind(lhs.variable, rhs);
      return;
    }
    if (rhs.kind == CallableTypeTerm::Kind::Variable) {
      bind(rhs.variable, lhs);
      return;
    }
    if (lhs.kind != rhs.kind) {
      throw StyioTypeError(
        "inferred callable type conflict in " + context
      );
    }
    if (lhs.kind == CallableTypeTerm::Kind::Concrete) {
      if (!lhs.concrete.equals(rhs.concrete)) {
        throw StyioTypeError(
          "inferred callable type conflict in " + context + ": `"
          + lhs.concrete.name + "` versus `" + rhs.concrete.name + "`"
        );
      }
      return;
    }
    if (lhs.arguments.size() != rhs.arguments.size()) {
      throw StyioTypeError(
        "inferred callable constructor arity conflict in " + context
      );
    }
    for (std::size_t i = 0; i < lhs.arguments.size(); ++i) {
      unify(lhs.arguments[i], rhs.arguments[i], context);
    }
  }
};

struct CallableMonotype
{
  std::vector<CallableTypeTerm> params;
  CallableTypeTerm result;
};

StyioAST*
callable_body_of_def(StyioAST* def) {
  if (auto* function = dynamic_cast<FunctionAST*>(def)) {
    return function->func_body;
  }
  if (auto* function = dynamic_cast<SimpleFuncAST*>(def)) {
    return function->ret_expr;
  }
  return nullptr;
}

std::string
callable_name_of_def(StyioAST* def) {
  if (auto* function = dynamic_cast<FunctionAST*>(def)) {
    return function->getNameAsStr();
  }
  if (auto* function = dynamic_cast<SimpleFuncAST*>(def)) {
    return function->func_name->getAsStr();
  }
  return {};
}

StyioDataType
callable_declared_result_type(StyioAST* def) {
  auto read_variant = [](const std::variant<TypeAST*, TypeTupleAST*>& result) {
    if (result.valueless_by_exception()
        || !std::holds_alternative<TypeAST*>(result)) {
      return StyioDataType{
        StyioDataTypeOption::Undefined, "undefined", 0
      };
    }
    TypeAST* type = std::get<TypeAST*>(result);
    return type == nullptr
             ? StyioDataType{
                 StyioDataTypeOption::Undefined, "undefined", 0
               }
             : type->getDataType();
  };

  if (auto* function = dynamic_cast<FunctionAST*>(def)) {
    return read_variant(function->ret_type);
  }
  if (auto* function = dynamic_cast<SimpleFuncAST*>(def)) {
    return read_variant(function->ret_type);
  }
  return StyioDataType{
    StyioDataTypeOption::Undefined, "undefined", 0
  };
}

void
walk_callable_expression(
  StyioAST* ast,
  const std::function<void(StyioAST*)>& visit
) {
  if (ast == nullptr) {
    return;
  }
  visit(ast);

  if (dynamic_cast<FunctionAST*>(ast) != nullptr
      || dynamic_cast<SimpleFuncAST*>(ast) != nullptr) {
    return;
  }
  if (auto* block = dynamic_cast<BlockAST*>(ast)) {
    for (auto* statement : block->stmts) {
      walk_callable_expression(statement, visit);
    }
    for (auto* following : block->followings) {
      walk_callable_expression(following, visit);
    }
    return;
  }
  if (auto* ret = dynamic_cast<ReturnAST*>(ast)) {
    walk_callable_expression(ret->getExpr(), visit);
    return;
  }
  if (auto* bind = dynamic_cast<FlexBindAST*>(ast)) {
    walk_callable_expression(bind->getValue(), visit);
    return;
  }
  if (auto* bind = dynamic_cast<FinalBindAST*>(ast)) {
    walk_callable_expression(bind->getValue(), visit);
    return;
  }
  if (auto* binary = dynamic_cast<BinOpAST*>(ast)) {
    walk_callable_expression(binary->getLHS(), visit);
    walk_callable_expression(binary->getRHS(), visit);
    return;
  }
  if (auto* compare = dynamic_cast<BinCompAST*>(ast)) {
    walk_callable_expression(compare->getLHS(), visit);
    walk_callable_expression(compare->getRHS(), visit);
    return;
  }
  if (auto* condition = dynamic_cast<CondAST*>(ast)) {
    walk_callable_expression(condition->getValue(), visit);
    walk_callable_expression(condition->getLHS(), visit);
    walk_callable_expression(condition->getRHS(), visit);
    return;
  }
  if (auto* flow = dynamic_cast<CondFlowAST*>(ast)) {
    walk_callable_expression(flow->getCond(), visit);
    walk_callable_expression(flow->getThen(), visit);
    walk_callable_expression(flow->getElse(), visit);
    return;
  }
  if (auto* call = dynamic_cast<FuncCallAST*>(ast)) {
    walk_callable_expression(call->func_callee, visit);
    for (auto* argument : call->getArgList()) {
      walk_callable_expression(argument, visit);
    }
    return;
  }
  if (auto* attribute = dynamic_cast<AttrAST*>(ast)) {
    walk_callable_expression(attribute->body, visit);
    walk_callable_expression(attribute->attr, visit);
    return;
  }
  if (auto* list = dynamic_cast<ListAST*>(ast)) {
    for (auto* element : list->getElements()) {
      walk_callable_expression(element, visit);
    }
    return;
  }
  if (auto* dict = dynamic_cast<DictAST*>(ast)) {
    for (const auto& entry : dict->getEntries()) {
      walk_callable_expression(entry.key, visit);
      walk_callable_expression(entry.value, visit);
    }
    return;
  }
  if (auto* tuple = dynamic_cast<TupleAST*>(ast)) {
    for (auto* element : tuple->getElements()) {
      walk_callable_expression(element, visit);
    }
    return;
  }
  if (auto* access = dynamic_cast<ListOpAST*>(ast)) {
    walk_callable_expression(access->getList(), visit);
    walk_callable_expression(access->getSlot1(), visit);
    walk_callable_expression(access->getSlot2(), visit);
    return;
  }
  if (auto* size = dynamic_cast<SizeOfAST*>(ast)) {
    walk_callable_expression(size->getValue(), visit);
    return;
  }
  if (auto* conversion = dynamic_cast<TypeConvertAST*>(ast)) {
    walk_callable_expression(conversion->getValue(), visit);
    return;
  }
  if (auto* range = dynamic_cast<RangeAST*>(ast)) {
    walk_callable_expression(range->getStart(), visit);
    walk_callable_expression(range->getEnd(), visit);
    walk_callable_expression(range->getStep(), visit);
    return;
  }
  if (auto* match = dynamic_cast<MatchCasesAST*>(ast)) {
    walk_callable_expression(match->getScrutinee(), visit);
    walk_callable_expression(match->getCases(), visit);
    return;
  }
  if (auto* cases = dynamic_cast<CasesAST*>(ast)) {
    for (const auto& entry : cases->case_list) {
      walk_callable_expression(entry.first, visit);
      walk_callable_expression(entry.second, visit);
    }
    walk_callable_expression(cases->case_default, visit);
    return;
  }
  if (auto* merge = dynamic_cast<WaveMergeAST*>(ast)) {
    walk_callable_expression(merge->getCond(), visit);
    walk_callable_expression(merge->getTrueVal(), visit);
    walk_callable_expression(merge->getFalseVal(), visit);
    return;
  }
  if (auto* dispatch = dynamic_cast<WaveDispatchAST*>(ast)) {
    walk_callable_expression(dispatch->getCond(), visit);
    walk_callable_expression(dispatch->getTrueArm(), visit);
    walk_callable_expression(dispatch->getFalseArm(), visit);
    return;
  }
  if (auto* fallback = dynamic_cast<FallbackAST*>(ast)) {
    walk_callable_expression(fallback->getPrimary(), visit);
    walk_callable_expression(fallback->getAlternate(), visit);
    return;
  }
  if (auto* selector = dynamic_cast<GuardSelectorAST*>(ast)) {
    walk_callable_expression(selector->getBase(), visit);
    walk_callable_expression(selector->getCond(), visit);
    return;
  }
  if (auto* probe = dynamic_cast<EqProbeAST*>(ast)) {
    walk_callable_expression(probe->getBase(), visit);
    walk_callable_expression(probe->getProbeValue(), visit);
    return;
  }
  if (auto* format = dynamic_cast<FmtStrAST*>(ast)) {
    for (auto* expression : format->getExprs()) {
      walk_callable_expression(expression, visit);
    }
    return;
  }
  if (auto* print = dynamic_cast<PrintAST*>(ast)) {
    for (auto* expression : print->exprs) {
      walk_callable_expression(expression, visit);
    }
    return;
  }
  if (auto* anonymous = dynamic_cast<AnonyFuncAST*>(ast)) {
    walk_callable_expression(anonymous->getThenExpr(), visit);
    return;
  }
  if (auto* task = dynamic_cast<TaskBlockAST*>(ast)) {
    walk_callable_expression(task->getBody(), visit);
    return;
  }
  if (auto* group = dynamic_cast<TaskGroupLaunchAST*>(ast)) {
    for (auto* entry : group->getEntries()) {
      walk_callable_expression(entry, visit);
    }
    return;
  }
  if (auto* flow = dynamic_cast<FlowBindAST*>(ast)) {
    walk_callable_expression(flow->getSource(), visit);
    walk_callable_expression(flow->getFallback(), visit);
    return;
  }
  if (auto* iterator = dynamic_cast<IteratorAST*>(ast)) {
    walk_callable_expression(iterator->collection, visit);
    for (auto* following : iterator->following) {
      walk_callable_expression(following, visit);
    }
    return;
  }
  if (auto* zip = dynamic_cast<StreamZipAST*>(ast)) {
    walk_callable_expression(zip->getCollectionA(), visit);
    walk_callable_expression(zip->getCollectionB(), visit);
    for (auto* following : zip->getFollowing()) {
      walk_callable_expression(following, visit);
    }
  }
}

[[noreturn]] void
reject_generalized_callable_value_escape(const std::string& name) {
  throw StyioTypeError(
    "inferred callable `" + name
    + "` cannot be used as a value; inferred schemes are available only "
      "to direct named calls, and a value-position boundary requires one "
      "concrete monomorphic callable type"
  );
}

void
validate_generalized_callable_value_positions(
  MainBlockAST* ast,
  const std::unordered_map<std::string, CallableTypeScheme>& schemes
) {
  if (ast == nullptr || schemes.empty()) {
    return;
  }

  auto validate_tree = [&](StyioAST* root, std::unordered_set<std::string> locals)
  {
    std::unordered_set<const StyioAST*> non_value_names;
    walk_callable_expression(
      root,
      [&](StyioAST* node)
      {
        if (auto* attribute = dynamic_cast<AttrAST*>(node)) {
          non_value_names.insert(attribute->attr);
        }
        if (auto* access = dynamic_cast<ListOpAST*>(node)) {
          non_value_names.insert(access->getList());
        }
        if (auto* bind = dynamic_cast<FlexBindAST*>(node)) {
          locals.insert(bind->getNameAsStr());
        }
        else if (auto* bind = dynamic_cast<FinalBindAST*>(node)) {
          locals.insert(bind->getName());
        }

        auto* name = dynamic_cast<NameAST*>(node);
        if (name == nullptr
            || non_value_names.count(name) != 0
            || locals.count(name->getAsStr()) != 0
            || schemes.count(name->getAsStr()) == 0) {
          return;
        }
        reject_generalized_callable_value_escape(name->getAsStr());
      });
  };

  for (auto* statement : ast->getStmts()) {
    if (auto* function = dynamic_cast<FunctionAST*>(statement)) {
      std::unordered_set<std::string> locals;
      for (auto* param : function->params) {
        locals.insert(param->getNameAsStr());
      }
      validate_tree(function->func_body, std::move(locals));
      continue;
    }
    if (auto* function = dynamic_cast<SimpleFuncAST*>(statement)) {
      std::unordered_set<std::string> locals;
      for (auto* param : function->params) {
        locals.insert(param->getNameAsStr());
      }
      validate_tree(function->ret_expr, std::move(locals));
      continue;
    }
    validate_tree(statement, {});
  }
}

bool
callable_node_is_in_principal_relation_subset(StyioAST* node) {
  return dynamic_cast<NameAST*>(node) != nullptr
         || dynamic_cast<IntAST*>(node) != nullptr
         || dynamic_cast<FloatAST*>(node) != nullptr
         || dynamic_cast<BoolAST*>(node) != nullptr
         || dynamic_cast<CharAST*>(node) != nullptr
         || dynamic_cast<StringAST*>(node) != nullptr
         || dynamic_cast<FmtStrAST*>(node) != nullptr
         || dynamic_cast<ListAST*>(node) != nullptr
         || dynamic_cast<DictAST*>(node) != nullptr
         || dynamic_cast<ListOpAST*>(node) != nullptr
         || dynamic_cast<BinOpAST*>(node) != nullptr
         || dynamic_cast<BinCompAST*>(node) != nullptr
         || dynamic_cast<CondAST*>(node) != nullptr
         || dynamic_cast<TypeConvertAST*>(node) != nullptr
         || dynamic_cast<FuncCallAST*>(node) != nullptr
         || dynamic_cast<ReturnAST*>(node) != nullptr
         || dynamic_cast<FlexBindAST*>(node) != nullptr
         || dynamic_cast<FinalBindAST*>(node) != nullptr
         || dynamic_cast<BlockAST*>(node) != nullptr
         || dynamic_cast<MatchCasesAST*>(node) != nullptr
         || dynamic_cast<CasesAST*>(node) != nullptr
         || dynamic_cast<WaveMergeAST*>(node) != nullptr
         || dynamic_cast<WaveDispatchAST*>(node) != nullptr
         || dynamic_cast<CondFlowAST*>(node) != nullptr
         || dynamic_cast<CommentAST*>(node) != nullptr
         || dynamic_cast<PassAST*>(node) != nullptr;
}

bool
callable_definition_has_relation_seed(StyioAST* def) {
  for (auto* param : params_of_func_def(def)) {
    if (param == nullptr
        || param->getDType() == nullptr
        || param->getDType()->getDataType().isUndefined()) {
      return true;
    }
  }

  bool generic_seed = false;
  walk_callable_expression(
    callable_body_of_def(def),
    [&](StyioAST* node)
    {
      if (auto* list = dynamic_cast<ListAST*>(node)) {
        generic_seed = generic_seed || list->getElements().empty();
      }
      if (auto* dict = dynamic_cast<DictAST*>(node)) {
        generic_seed = generic_seed || dict->getEntries().empty();
      }
    });
  return generic_seed;
}

std::unordered_set<std::string>
callable_direct_calls(StyioAST* def) {
  std::unordered_set<std::string> calls;
  walk_callable_expression(
    callable_body_of_def(def),
    [&](StyioAST* node)
    {
      auto* call = dynamic_cast<FuncCallAST*>(node);
      if (call != nullptr && call->func_callee == nullptr) {
        calls.insert(call->getNameAsStr());
      }
    });
  return calls;
}

constexpr std::uint32_t
callable_effect_mask(CallableEffectKind kind) {
  return static_cast<std::uint32_t>(kind);
}

void
add_callable_effect(
  CallableEffectSummary& summary,
  CallableEffectKind kind
) {
  summary.effect_bits |= callable_effect_mask(kind);
}

std::string
callable_effect_name(CallableEffectKind kind) {
  switch (kind) {
    case CallableEffectKind::None:
      return "pure";
    case CallableEffectKind::Output:
      return "output";
    case CallableEffectKind::Resource:
      return "resource";
    case CallableEffectKind::Task:
      return "task";
    case CallableEffectKind::Handler:
      return "handler";
    case CallableEffectKind::Native:
      return "native";
    case CallableEffectKind::Capture:
      return "capture";
    case CallableEffectKind::Unknown:
      return "unknown";
  }
  return "unknown";
}

std::string
canonical_callable_effect_summary(
  const CallableEffectSummary& summary
) {
  if (summary.proven_pure()) {
    return "pure";
  }

  std::vector<std::string> parts;
  for (CallableEffectKind kind : {
         CallableEffectKind::Output,
         CallableEffectKind::Resource,
         CallableEffectKind::Task,
         CallableEffectKind::Handler,
         CallableEffectKind::Native,
         CallableEffectKind::Capture,
         CallableEffectKind::Unknown,
       }) {
    if ((summary.effect_bits & callable_effect_mask(kind)) != 0) {
      parts.push_back(callable_effect_name(kind));
    }
  }
  if (!summary.closed
      && (summary.effect_bits
          & callable_effect_mask(CallableEffectKind::Capture)) == 0) {
    parts.push_back("open");
  }

  std::ostringstream output;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i != 0) {
      output << ",";
    }
    output << parts[i];
  }
  if (!summary.captures.empty()) {
    output << "[";
    for (std::size_t i = 0; i < summary.captures.size(); ++i) {
      if (i != 0) {
        output << ",";
      }
      output << summary.captures[i];
    }
    output << "]";
  }
  return output.str();
}

CallableEffectSummary
callable_local_effect_summary(
  StyioSemaContext* an,
  StyioAST* def,
  const std::unordered_map<std::string, StyioAST*>& definitions
) {
  CallableEffectSummary summary;
  summary.relation_seed =
    callable_definition_has_relation_seed(def);

  std::unordered_set<std::string> local_names;
  for (auto* param : params_of_func_def(def)) {
    if (param != nullptr) {
      local_names.insert(param->getNameAsStr());
    }
  }
  walk_callable_expression(
    callable_body_of_def(def),
    [&](StyioAST* node)
    {
      if (auto* bind = dynamic_cast<FlexBindAST*>(node)) {
        local_names.insert(bind->getNameAsStr());
      }
      else if (auto* bind = dynamic_cast<FinalBindAST*>(node)) {
        local_names.insert(bind->getName());
      }
    });

  std::unordered_set<std::string> captures;
  std::unordered_set<std::string> direct_callees;
  walk_callable_expression(
    callable_body_of_def(def),
    [&](StyioAST* node)
    {
      if (auto* name = dynamic_cast<NameAST*>(node)) {
        const std::string& spelling = name->getAsStr();
        if (spelling != "_" && local_names.count(spelling) == 0) {
          captures.insert(spelling);
        }
      }

      if (auto* call = dynamic_cast<FuncCallAST*>(node)) {
        if (call->func_callee != nullptr) {
          add_callable_effect(summary, CallableEffectKind::Unknown);
        }
        else {
          const std::string callee = call->getNameAsStr();
          if (definitions.count(callee) != 0) {
            direct_callees.insert(callee);
          }
          else if (an != nullptr
                   && an->find_native_function_def(
                        styio::session::kInvalidSymbolId,
                        callee) != nullptr) {
            add_callable_effect(summary, CallableEffectKind::Native);
          }
          else {
            add_callable_effect(summary, CallableEffectKind::Unknown);
          }
        }
      }

      if (callable_node_is_in_principal_relation_subset(node)) {
        return;
      }
      switch (node->getNodeType()) {
        case StyioNodeType::Print:
          add_callable_effect(summary, CallableEffectKind::Output);
          return;
        case StyioNodeType::FileResource:
        case StyioNodeType::EmptyResource:
        case StyioNodeType::ResourceReceiver:
        case StyioNodeType::ResourceMethodDef:
        case StyioNodeType::ResourceOrder:
        case StyioNodeType::ResourceDecl:
        case StyioNodeType::ResourceRef:
        case StyioNodeType::HandleAcquire:
        case StyioNodeType::ResourceWrite:
        case StyioNodeType::ResourceRedirect:
        case StyioNodeType::InstantPull:
        case StyioNodeType::StdinResource:
        case StyioNodeType::StdoutResource:
        case StyioNodeType::StderrResource:
          add_callable_effect(summary, CallableEffectKind::Resource);
          return;
        case StyioNodeType::ResourceEffect:
          add_callable_effect(summary, CallableEffectKind::Resource);
          add_callable_effect(summary, CallableEffectKind::Handler);
          return;
        case StyioNodeType::TaskBlock:
        case StyioNodeType::TaskGroupLaunch:
        case StyioNodeType::FlowBind:
          add_callable_effect(summary, CallableEffectKind::Task);
          return;
        case StyioNodeType::Fallback:
          add_callable_effect(summary, CallableEffectKind::Handler);
          return;
        case StyioNodeType::ExternBlock:
          add_callable_effect(summary, CallableEffectKind::Native);
          return;
        default:
          add_callable_effect(summary, CallableEffectKind::Unknown);
          return;
      }
    });

  summary.captures.assign(captures.begin(), captures.end());
  std::sort(summary.captures.begin(), summary.captures.end());
  summary.direct_callees.assign(
    direct_callees.begin(),
    direct_callees.end());
  std::sort(
    summary.direct_callees.begin(),
    summary.direct_callees.end());
  if (!summary.captures.empty()) {
    summary.closed = false;
    add_callable_effect(summary, CallableEffectKind::Capture);
  }
  summary.canonical = canonical_callable_effect_summary(summary);
  return summary;
}

std::string
callable_term_text(const CallableTypeTerm& term) {
  switch (term.kind) {
    case CallableTypeTerm::Kind::Variable:
      return "'" + std::to_string(term.variable);
    case CallableTypeTerm::Kind::Concrete:
      return term.concrete.name;
    case CallableTypeTerm::Kind::List:
      return "list[" + callable_term_text(term.arguments.at(0)) + "]";
    case CallableTypeTerm::Kind::Dict:
      return "dict[" + callable_term_text(term.arguments.at(0)) + ","
             + callable_term_text(term.arguments.at(1)) + "]";
  }
  return "undefined";
}

std::string
callable_constraint_name(CallableConstraintKind kind) {
  switch (kind) {
    case CallableConstraintKind::Numeric:
      return "numeric";
    case CallableConstraintKind::Comparable:
      return "comparable";
    case CallableConstraintKind::Indexable:
      return "indexable";
    case CallableConstraintKind::Iterable:
      return "iterable";
    case CallableConstraintKind::Cloneable:
      return "cloneable";
  }
  return "unknown";
}

std::string
callable_constraint_text(const CallableTypeConstraint& constraint) {
  std::ostringstream output;
  output << callable_constraint_name(constraint.kind) << "("
         << callable_term_text(constraint.subject);
  if (constraint.kind == CallableConstraintKind::Indexable) {
    output << "," << callable_term_text(constraint.argument)
           << "," << callable_term_text(constraint.result);
  }
  else if (constraint.kind == CallableConstraintKind::Iterable) {
    output << "," << callable_term_text(constraint.result);
  }
  output << ")";
  return output.str();
}

CallableTypeConstraint
apply_callable_constraint(
  CallableTypeUnifier& unifier,
  const CallableTypeConstraint& input
) {
  CallableTypeConstraint output = input;
  output.subject = unifier.apply(output.subject);
  if (output.kind == CallableConstraintKind::Indexable) {
    output.argument = unifier.apply(output.argument);
    output.result = unifier.apply(output.result);
  }
  else if (output.kind == CallableConstraintKind::Iterable) {
    output.result = unifier.apply(output.result);
  }
  output.canonical = callable_constraint_text(output);
  return output;
}

void
collect_callable_term_variables(
  const CallableTypeTerm& term,
  std::vector<std::uint32_t>& variables,
  std::unordered_set<std::uint32_t>& seen
) {
  if (term.kind == CallableTypeTerm::Kind::Variable) {
    if (seen.insert(term.variable).second) {
      variables.push_back(term.variable);
    }
    return;
  }
  for (const auto& argument : term.arguments) {
    collect_callable_term_variables(argument, variables, seen);
  }
}

bool
callable_term_contains_variable(
  const CallableTypeTerm& term,
  std::uint32_t variable
) {
  if (term.kind == CallableTypeTerm::Kind::Variable) {
    return term.variable == variable;
  }
  for (const auto& argument : term.arguments) {
    if (callable_term_contains_variable(argument, variable)) {
      return true;
    }
  }
  return false;
}

std::optional<std::uint32_t>
first_callable_term_variable(const CallableTypeTerm& term) {
  if (term.kind == CallableTypeTerm::Kind::Variable) {
    return term.variable;
  }
  for (const auto& argument : term.arguments) {
    if (auto variable = first_callable_term_variable(argument)) {
      return variable;
    }
  }
  return std::nullopt;
}

bool
callable_constraint_contains_variable(
  const CallableTypeConstraint& constraint,
  std::uint32_t variable
) {
  if (callable_term_contains_variable(constraint.subject, variable)) {
    return true;
  }
  if (constraint.kind == CallableConstraintKind::Indexable
      && callable_term_contains_variable(constraint.argument, variable)) {
    return true;
  }
  return (constraint.kind == CallableConstraintKind::Indexable
          || constraint.kind == CallableConstraintKind::Iterable)
         && callable_term_contains_variable(constraint.result, variable);
}

CallableTypeTerm
normalize_callable_term_variables(
  const CallableTypeTerm& input,
  std::unordered_map<std::uint32_t, std::uint32_t>& normalization
) {
  CallableTypeTerm output = input;
  if (output.kind == CallableTypeTerm::Kind::Variable) {
    auto [it, inserted] = normalization.emplace(
      output.variable,
      static_cast<std::uint32_t>(normalization.size()));
    (void)inserted;
    output.variable = it->second;
    return output;
  }
  for (auto& argument : output.arguments) {
    argument = normalize_callable_term_variables(argument, normalization);
  }
  return output;
}

CallableTypeConstraint
normalize_callable_constraint_variables(
  const CallableTypeConstraint& input,
  std::unordered_map<std::uint32_t, std::uint32_t>& normalization
) {
  CallableTypeConstraint output = input;
  output.subject =
    normalize_callable_term_variables(output.subject, normalization);
  if (output.kind == CallableConstraintKind::Indexable) {
    output.argument =
      normalize_callable_term_variables(output.argument, normalization);
    output.result =
      normalize_callable_term_variables(output.result, normalization);
  }
  else if (output.kind == CallableConstraintKind::Iterable) {
    output.result =
      normalize_callable_term_variables(output.result, normalization);
  }
  output.canonical = callable_constraint_text(output);
  return output;
}

bool
callable_concrete_is_numeric(const StyioDataType& input) {
  StyioDataType type = normalize_callable_concrete_type(input);
  return type.option == StyioDataTypeOption::Integer
         || type.option == StyioDataTypeOption::Float;
}

bool
callable_concrete_is_comparable(const StyioDataType& input) {
  StyioDataType type = normalize_callable_concrete_type(input);
  return type.option == StyioDataTypeOption::Bool
         || type.option == StyioDataTypeOption::Integer
         || type.option == StyioDataTypeOption::Float
         || type.option == StyioDataTypeOption::Char
         || type.option == StyioDataTypeOption::String;
}

void
validate_callable_unary_constraint(
  CallableConstraintKind kind,
  const StyioDataType& input,
  std::string_view constraint_text = {}
) {
  StyioDataType type = normalize_callable_concrete_type(input);
  bool satisfied = false;
  switch (kind) {
    case CallableConstraintKind::Numeric:
      satisfied = callable_concrete_is_numeric(type);
      break;
    case CallableConstraintKind::Comparable:
      satisfied = callable_concrete_is_comparable(type);
      break;
    case CallableConstraintKind::Iterable:
      satisfied = styio_type_is_iterable(type);
      break;
    case CallableConstraintKind::Cloneable:
      satisfied = styio_type_is_cloneable(type);
      break;
    case CallableConstraintKind::Indexable:
      satisfied = styio_type_is_indexable(type);
      break;
  }
  if (!satisfied) {
    throw StyioTypeError(
      "callable constraint `"
      + (constraint_text.empty()
           ? callable_constraint_name(kind)
           : std::string(constraint_text))
      + "` is not satisfied by type `" + type.name + "`"
    );
  }
}

bool
reduce_callable_constraint(
  CallableTypeUnifier& unifier,
  const CallableTypeConstraint& input
) {
  CallableTypeConstraint constraint =
    apply_callable_constraint(unifier, input);
  if (constraint.kind == CallableConstraintKind::Numeric
      || constraint.kind == CallableConstraintKind::Comparable
      || constraint.kind == CallableConstraintKind::Cloneable) {
    if (constraint.subject.kind == CallableTypeTerm::Kind::Variable) {
      return false;
    }
    if (constraint.subject.kind == CallableTypeTerm::Kind::List
        || constraint.subject.kind == CallableTypeTerm::Kind::Dict) {
      if (constraint.kind == CallableConstraintKind::Cloneable) {
        return true;
      }
      throw StyioTypeError(
        "callable constraint `"
        + callable_constraint_name(constraint.kind)
        + "` is not satisfied by a collection type"
      );
    }
    validate_callable_unary_constraint(
      constraint.kind,
      constraint.subject.concrete,
      callable_constraint_text(constraint));
    return true;
  }

  if (constraint.subject.kind == CallableTypeTerm::Kind::List) {
    if (constraint.kind == CallableConstraintKind::Indexable) {
      unifier.unify(
        constraint.argument,
        callable_concrete_term(kI64Type),
        "indexable list key");
      unifier.unify(
        constraint.result,
        constraint.subject.arguments.at(0),
        "indexable list result");
      return true;
    }
    if (constraint.kind == CallableConstraintKind::Iterable) {
      unifier.unify(
        constraint.result,
        constraint.subject.arguments.at(0),
        "iterable list element");
      return true;
    }
  }
  if (constraint.subject.kind == CallableTypeTerm::Kind::Dict) {
    if (constraint.kind == CallableConstraintKind::Indexable) {
      unifier.unify(
        constraint.argument,
        constraint.subject.arguments.at(0),
        "indexable dict key");
      unifier.unify(
        constraint.result,
        constraint.subject.arguments.at(1),
        "indexable dict result");
      return true;
    }
  }
  if (constraint.subject.kind != CallableTypeTerm::Kind::Concrete) {
    return false;
  }

  StyioDataType subject =
    normalize_callable_concrete_type(constraint.subject.concrete);
  validate_callable_unary_constraint(constraint.kind, subject);
  if (constraint.kind == CallableConstraintKind::Indexable) {
    StyioDataType key = styio_is_dict_type(subject)
                          ? styio_data_type_from_name(
                              styio_dict_key_type_name(subject))
                          : kI64Type;
    StyioDataType result = styio_data_type_from_name(
      styio_type_item_type_name(subject));
    unifier.unify(
      constraint.argument,
      callable_concrete_term(key),
      "indexable concrete key");
    unifier.unify(
      constraint.result,
      callable_concrete_term(result),
      "indexable concrete result");
  }
  else if (constraint.kind == CallableConstraintKind::Iterable) {
    StyioDataType result = styio_data_type_from_name(
      styio_type_item_type_name(subject));
    unifier.unify(
      constraint.result,
      callable_concrete_term(result),
      "iterable concrete element");
  }
  return true;
}

void
reduce_callable_constraints(
  CallableTypeUnifier& unifier,
  std::vector<CallableTypeConstraint>& constraints
) {
  std::vector<CallableTypeConstraint> pending = std::move(constraints);
  while (true) {
    std::vector<CallableTypeConstraint> next;
    next.reserve(pending.size());
    bool reduced = false;
    for (const auto& constraint : pending) {
      if (reduce_callable_constraint(unifier, constraint)) {
        reduced = true;
      }
      else {
        next.push_back(apply_callable_constraint(unifier, constraint));
      }
    }
    pending = std::move(next);
    if (!reduced) {
      break;
    }
  }
  for (auto& constraint : pending) {
    constraint = apply_callable_constraint(unifier, constraint);
  }
  constraints = std::move(pending);
}

class CallableSymbolicInfer
{
  CallableTypeUnifier& unifier_;
  const std::unordered_map<std::string, CallableTypeScheme>& schemes_;
  const std::unordered_map<std::string, CallableMonotype>& recursive_group_;
  const std::unordered_map<std::string, StyioAST*>& all_defs_;
  std::vector<CallableTypeConstraint>& constraints_;
  std::unordered_map<std::string, CallableTypeTerm> locals_;

  void record_constraint(
    CallableConstraintKind kind,
    CallableTypeTerm subject,
    CallableTypeTerm argument = {},
    CallableTypeTerm result = {}
  ) {
    CallableTypeConstraint constraint;
    constraint.kind = kind;
    constraint.subject = unifier_.apply(subject);
    constraint.argument = unifier_.apply(argument);
    constraint.result = unifier_.apply(result);
    constraint.canonical = callable_constraint_text(constraint);
    constraints_.push_back(std::move(constraint));
  }

  CallableMonotype instantiate_scheme(const CallableTypeScheme& scheme) {
    std::unordered_map<std::uint32_t, CallableTypeTerm> fresh_variables;
    auto instantiate_term = [&](auto&& self, const CallableTypeTerm& input)
      -> CallableTypeTerm
    {
      if (input.kind == CallableTypeTerm::Kind::Variable) {
        auto [it, inserted] = fresh_variables.emplace(
          input.variable,
          CallableTypeTerm{});
        if (inserted) {
          it->second = unifier_.fresh();
        }
        return it->second;
      }
      CallableTypeTerm output = input;
      for (auto& argument : output.arguments) {
        argument = self(self, argument);
      }
      return output;
    };

    CallableMonotype instance;
    instance.params.reserve(scheme.params.size());
    for (const auto& param : scheme.params) {
      instance.params.push_back(instantiate_term(instantiate_term, param));
    }
    instance.result = instantiate_term(instantiate_term, scheme.result);
    for (const auto& constraint : scheme.constraints) {
      CallableTypeConstraint instantiated = constraint;
      instantiated.subject =
        instantiate_term(instantiate_term, constraint.subject);
      if (constraint.kind == CallableConstraintKind::Indexable) {
        instantiated.argument =
          instantiate_term(instantiate_term, constraint.argument);
        instantiated.result =
          instantiate_term(instantiate_term, constraint.result);
      }
      else if (constraint.kind == CallableConstraintKind::Iterable) {
        instantiated.result =
          instantiate_term(instantiate_term, constraint.result);
      }
      instantiated.canonical = callable_constraint_text(instantiated);
      constraints_.push_back(std::move(instantiated));
    }
    return instance;
  }

  CallableMonotype concrete_signature(StyioAST* def) {
    CallableMonotype signature;
    for (auto* param : params_of_func_def(def)) {
      StyioDataType type = param->getDType()->getDataType();
      if (type.isUndefined()) {
        throw StyioTypeError(
          "cannot infer a principal relation through mutable or unresolved "
          "callable `" + callable_name_of_def(def) + "`"
        );
      }
      signature.params.push_back(callable_concrete_term(type));
    }
    StyioDataType result = callable_declared_result_type(def);
    if (result.isUndefined()) {
      throw StyioTypeError(
        "cannot infer a principal relation through callable `"
        + callable_name_of_def(def)
        + "` before its result relation is available"
      );
    }
    signature.result = callable_concrete_term(result);
    return signature;
  }

  CallableTypeTerm infer_call(FuncCallAST* call) {
    if (call->func_callee != nullptr) {
      throw StyioTypeError(
        "inferred callable relations do not include method or closure calls"
      );
    }

    CallableMonotype signature;
    auto recursive = recursive_group_.find(call->getNameAsStr());
    if (recursive != recursive_group_.end()) {
      signature = recursive->second;
    }
    else if (auto scheme = schemes_.find(call->getNameAsStr());
             scheme != schemes_.end()) {
      signature = instantiate_scheme(scheme->second);
    }
    else if (auto def = all_defs_.find(call->getNameAsStr());
             def != all_defs_.end()) {
      signature = concrete_signature(def->second);
    }
    else {
      throw StyioTypeError(
        "cannot infer a principal relation through unknown callable `"
        + call->getNameAsStr() + "`"
      );
    }

    if (signature.params.size() != call->getArgList().size()) {
      throw StyioTypeError(
        "function `" + call->getNameAsStr() + "` expects "
        + std::to_string(signature.params.size()) + " argument(s), got "
        + std::to_string(call->getArgList().size())
      );
    }
    for (std::size_t i = 0; i < signature.params.size(); ++i) {
      CallableTypeTerm argument = infer(call->getArgList()[i]);
      try {
        unifier_.unify(
          signature.params[i],
          argument,
          "call to `" + call->getNameAsStr() + "`"
        );
      }
      catch (const StyioTypeError&) {
        if (recursive != recursive_group_.end()) {
          throw StyioTypeError(
            "polymorphic recursion is not supported in recursive callable `"
            + call->getNameAsStr()
            + "`; every internal edge must reuse the group's provisional monotype"
          );
        }
        throw;
      }
    }
    return unifier_.apply(signature.result);
  }

  CallableTypeTerm infer_numeric_binary(BinOpAST* binary) {
    CallableTypeTerm lhs = unifier_.apply(infer(binary->getLHS()));
    CallableTypeTerm rhs = unifier_.apply(infer(binary->getRHS()));

    auto is_numeric = [](const CallableTypeTerm& term)
    {
      return term.kind == CallableTypeTerm::Kind::Concrete
             && (term.concrete.option == StyioDataTypeOption::Integer
                 || term.concrete.option == StyioDataTypeOption::Float);
    };
    auto is_string = [](const CallableTypeTerm& term)
    {
      return term.kind == CallableTypeTerm::Kind::Concrete
             && term.concrete.option == StyioDataTypeOption::String;
    };

    if (binary->getOp() == StyioOpType::Binary_Add
        && (is_string(lhs) || is_string(rhs))) {
      unifier_.unify(lhs, callable_concrete_term(kStringType), "string addition");
      unifier_.unify(rhs, callable_concrete_term(kStringType), "string addition");
      return callable_concrete_term(kStringType);
    }

    switch (binary->getOp()) {
      case StyioOpType::Binary_Add:
      case StyioOpType::Binary_Sub:
      case StyioOpType::Binary_Mul:
      case StyioOpType::Binary_Div:
      case StyioOpType::Binary_Pow:
      case StyioOpType::Binary_Mod:
        break;
      default:
        throw StyioTypeError(
          "inferred callable operator is outside the closed `numeric` "
          "constraint vocabulary"
        );
    }

    if (lhs.kind == CallableTypeTerm::Kind::Variable
        || rhs.kind == CallableTypeTerm::Kind::Variable) {
      unifier_.unify(lhs, rhs, "numeric operator operands");
      CallableTypeTerm operand = unifier_.apply(lhs);
      if (operand.kind == CallableTypeTerm::Kind::Variable) {
        record_constraint(CallableConstraintKind::Numeric, operand);
        return operand;
      }
      if (is_numeric(operand)) {
        return operand;
      }
      throw StyioTypeError(
        "numeric operator requires an integer or floating-point operand"
      );
    }
    if (!is_numeric(lhs) || !is_numeric(rhs)) {
      throw StyioTypeError(
        "numeric operator requires integer or floating-point operands"
      );
    }
    if (lhs.concrete.option == StyioDataTypeOption::Float
        || rhs.concrete.option == StyioDataTypeOption::Float) {
      return callable_concrete_term(kF64Type);
    }
    return callable_concrete_term(
      lhs.concrete.num_of_bit >= rhs.concrete.num_of_bit
        ? lhs.concrete
        : rhs.concrete
    );
  }

public:
  CallableSymbolicInfer(
    CallableTypeUnifier& unifier,
    const std::unordered_map<std::string, CallableTypeScheme>& schemes,
    const std::unordered_map<std::string, CallableMonotype>& recursive_group,
    const std::unordered_map<std::string, StyioAST*>& all_defs,
    std::vector<CallableTypeConstraint>& constraints
  ) :
      unifier_(unifier),
      schemes_(schemes),
      recursive_group_(recursive_group),
      all_defs_(all_defs),
      constraints_(constraints) {
  }

  void bind_local(const std::string& name, CallableTypeTerm type) {
    locals_[name] = std::move(type);
  }

  CallableTypeTerm infer(StyioAST* ast) {
    if (ast == nullptr) {
      throw StyioTypeError(
        "cannot infer a principal callable relation from an empty expression"
      );
    }
    if (auto* name = dynamic_cast<NameAST*>(ast)) {
      auto local = locals_.find(name->getAsStr());
      if (local == locals_.end()) {
        throw StyioTypeError(
          "cannot infer a principal callable relation for free value `"
          + name->getAsStr() + "`"
        );
      }
      return unifier_.apply(local->second);
    }
    if (dynamic_cast<IntAST*>(ast) != nullptr) {
      return callable_concrete_term(kI64Type);
    }
    if (dynamic_cast<FloatAST*>(ast) != nullptr) {
      return callable_concrete_term(kF64Type);
    }
    if (dynamic_cast<BoolAST*>(ast) != nullptr
        || dynamic_cast<CondAST*>(ast) != nullptr
        || dynamic_cast<BinCompAST*>(ast) != nullptr) {
      if (auto* comparison = dynamic_cast<BinCompAST*>(ast)) {
        CallableTypeTerm lhs = infer(comparison->getLHS());
        CallableTypeTerm rhs = infer(comparison->getRHS());
        unifier_.unify(lhs, rhs, "comparison");
        record_constraint(
          CallableConstraintKind::Comparable,
          unifier_.apply(lhs));
      }
      else if (auto* condition = dynamic_cast<CondAST*>(ast)) {
        if (condition->getValue() != nullptr) {
          unifier_.unify(
            infer(condition->getValue()),
            callable_concrete_term(kBoolType),
            "logical condition");
        }
        if (condition->getLHS() != nullptr) {
          unifier_.unify(
            infer(condition->getLHS()),
            callable_concrete_term(kBoolType),
            "logical condition");
        }
        if (condition->getRHS() != nullptr) {
          unifier_.unify(
            infer(condition->getRHS()),
            callable_concrete_term(kBoolType),
            "logical condition");
        }
      }
      return callable_concrete_term(kBoolType);
    }
    if (auto* character = dynamic_cast<CharAST*>(ast)) {
      return callable_concrete_term(character->getDataType());
    }
    if (dynamic_cast<StringAST*>(ast) != nullptr
        || dynamic_cast<FmtStrAST*>(ast) != nullptr) {
      return callable_concrete_term(kStringType);
    }
    if (auto* list = dynamic_cast<ListAST*>(ast)) {
      if (list->getElements().empty()) {
        return callable_list_term(unifier_.fresh());
      }
      CallableTypeTerm element = infer(list->getElements().front());
      for (std::size_t i = 1; i < list->getElements().size(); ++i) {
        unifier_.unify(
          element,
          infer(list->getElements()[i]),
          "list literal"
        );
      }
      return callable_list_term(unifier_.apply(element));
    }
    if (auto* dict = dynamic_cast<DictAST*>(ast)) {
      CallableTypeTerm key = callable_concrete_term(kStringType);
      CallableTypeTerm value = unifier_.fresh();
      for (const auto& entry : dict->getEntries()) {
        unifier_.unify(key, infer(entry.key), "dict key");
        unifier_.unify(value, infer(entry.value), "dict value");
      }
      return callable_dict_term(
        unifier_.apply(key),
        unifier_.apply(value));
    }
    if (auto* access = dynamic_cast<ListOpAST*>(ast)) {
      if (access->getOp() != StyioNodeType::Access_By_Index
          || access->getSlot1() == nullptr) {
        throw StyioTypeError(
          "inferred callable indexable constraints currently require "
          "single-value index access"
        );
      }
      CallableTypeTerm subject = unifier_.apply(infer(access->getList()));
      CallableTypeTerm argument = unifier_.apply(infer(access->getSlot1()));
      if (subject.kind == CallableTypeTerm::Kind::List) {
        unifier_.unify(
          argument,
          callable_concrete_term(kI64Type),
          "list index");
        return unifier_.apply(subject.arguments.at(0));
      }
      if (subject.kind == CallableTypeTerm::Kind::Dict) {
        unifier_.unify(
          argument,
          subject.arguments.at(0),
          "dict index");
        return unifier_.apply(subject.arguments.at(1));
      }
      CallableTypeTerm result = unifier_.fresh();
      record_constraint(
        CallableConstraintKind::Indexable,
        subject,
        argument,
        result);
      return result;
    }
    if (auto* binary = dynamic_cast<BinOpAST*>(ast)) {
      return infer_numeric_binary(binary);
    }
    if (auto* conversion = dynamic_cast<TypeConvertAST*>(ast)) {
      (void)infer(conversion->getValue());
      switch (conversion->getPromoTy()) {
        case NumPromoTy::Bool_To_Int:
          return callable_concrete_term(kI64Type);
        case NumPromoTy::Int_To_Float:
          return callable_concrete_term(kF64Type);
      }
    }
    if (auto* call = dynamic_cast<FuncCallAST*>(ast)) {
      return infer_call(call);
    }
    if (auto* ret = dynamic_cast<ReturnAST*>(ast)) {
      return infer(ret->getExpr());
    }
    if (auto* bind = dynamic_cast<FlexBindAST*>(ast)) {
      CallableTypeTerm value = infer(bind->getValue());
      StyioDataType declared = bind->getVar()->getDType()->getDataType();
      if (!declared.isUndefined()) {
        unifier_.unify(
          value,
          callable_concrete_term(declared),
          "local binding `" + bind->getNameAsStr() + "`");
      }
      bind_local(bind->getNameAsStr(), unifier_.apply(value));
      return unifier_.apply(value);
    }
    if (auto* bind = dynamic_cast<FinalBindAST*>(ast)) {
      CallableTypeTerm value = infer(bind->getValue());
      StyioDataType declared = bind->getVar()->getDType()->getDataType();
      if (!declared.isUndefined()) {
        unifier_.unify(
          value,
          callable_concrete_term(declared),
          "local binding `" + bind->getName() + "`");
      }
      bind_local(bind->getName(), unifier_.apply(value));
      return unifier_.apply(value);
    }
    if (auto* block = dynamic_cast<BlockAST*>(ast)) {
      CallableTypeTerm result =
        callable_concrete_term(
          StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0});
      bool has_result = false;
      for (auto* statement : block->stmts) {
        if (dynamic_cast<CommentAST*>(statement) != nullptr
            || dynamic_cast<PassAST*>(statement) != nullptr) {
          continue;
        }
        result = infer(statement);
        has_result = true;
      }
      if (!has_result) {
        throw StyioTypeError(
          "inferred callable body requires a value-producing tail"
        );
      }
      return unifier_.apply(result);
    }
    if (auto* match = dynamic_cast<MatchCasesAST*>(ast)) {
      CallableTypeTerm scrutinee = infer(match->getScrutinee());
      CallableTypeTerm result = unifier_.fresh();
      CasesAST* cases = match->getCases();
      for (const auto& entry : cases->case_list) {
        if (auto* integer = dynamic_cast<IntAST*>(entry.first)) {
          CallableTypeTerm concrete = callable_concrete_term(integer->getDataType());
          CallableTypeTerm resolved = unifier_.apply(scrutinee);
          if (resolved.kind == CallableTypeTerm::Kind::Variable) {
            unifier_.unify(scrutinee, concrete, "match scrutinee");
          }
          else if (resolved.kind != CallableTypeTerm::Kind::Concrete
                   || resolved.concrete.option != StyioDataTypeOption::Integer) {
            throw StyioTypeError(
              "integer match pattern requires an integer scrutinee"
            );
          }
        }
        unifier_.unify(result, infer(entry.second), "match branch");
      }
      if (cases->case_default != nullptr) {
        unifier_.unify(
          result,
          infer(cases->case_default),
          "match default branch");
      }
      return unifier_.apply(result);
    }
    if (auto* merge = dynamic_cast<WaveMergeAST*>(ast)) {
      unifier_.unify(
        infer(merge->getCond()),
        callable_concrete_term(kBoolType),
        "wave merge condition");
      CallableTypeTerm result = infer(merge->getTrueVal());
      unifier_.unify(result, infer(merge->getFalseVal()), "wave merge arms");
      return unifier_.apply(result);
    }
    if (auto* dispatch = dynamic_cast<WaveDispatchAST*>(ast)) {
      unifier_.unify(
        infer(dispatch->getCond()),
        callable_concrete_term(kBoolType),
        "wave dispatch condition");
      CallableTypeTerm result = infer(dispatch->getTrueArm());
      unifier_.unify(
        result,
        infer(dispatch->getFalseArm()),
        "wave dispatch arms");
      return unifier_.apply(result);
    }
    if (auto* flow = dynamic_cast<CondFlowAST*>(ast)) {
      unifier_.unify(
        infer(flow->getCond()),
        callable_concrete_term(kBoolType),
        "conditional guard");
      CallableTypeTerm result = infer(flow->getThen());
      if (flow->getElse() != nullptr) {
        unifier_.unify(result, infer(flow->getElse()), "conditional arms");
      }
      return unifier_.apply(result);
    }

    throw StyioTypeError(
      "cannot derive a principal rank-1 relation from AST node "
      + std::to_string(static_cast<int>(ast->getNodeType()))
    );
  }
};

std::string
callable_specialized_symbol(
  std::string_view name,
  std::string_view content_digest
) {
  if (content_digest.size() != 64
      || !std::all_of(
           content_digest.begin(),
           content_digest.end(),
           [](unsigned char ch)
           {
             return std::isxdigit(ch) != 0;
           })) {
    throw StyioTypeError(
      "callable specialization requires a SHA-256 content digest"
    );
  }
  std::string safe_name;
  safe_name.reserve(name.size());
  for (unsigned char ch : name) {
    safe_name.push_back(
      std::isalnum(ch) || ch == '_'
        ? static_cast<char>(ch)
        : '_');
  }

  std::ostringstream output;
  output << "__styio_mono_" << safe_name << "_"
         << content_digest;
  return output.str();
}

StyioDataType
infer_expr_type(StyioSemaContext* an, StyioAST* expr);

StyioDataType
type_convert_target_type(NumPromoTy promo_type) {
  switch (promo_type) {
    case NumPromoTy::Bool_To_Int:
      return kI64Type;
    case NumPromoTy::Int_To_Float:
      return kF64Type;
  }
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

StyioDataType
type_convert_source_fallback_type(NumPromoTy promo_type) {
  switch (promo_type) {
    case NumPromoTy::Bool_To_Int:
      return kBoolType;
    case NumPromoTy::Int_To_Float:
      return kI64Type;
  }
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

bool
sema_types_equal(
  StyioSemaContext* an,
  const StyioDataType& lhs,
  const StyioDataType& rhs
) {
  return an != nullptr
    ? an->types_equal(lhs, rhs)
    : lhs.equals(rhs);
}

std::string
resource_family_for_type(const StyioDataType& type);

std::string
resource_family_for_expr(StyioSemaContext* an, StyioAST* expr);

bool
resource_effect_handler_name_supported_latest(const std::string& name) {
  return name == "io"
         || name == "parse"
         || name == "bounds"
         || name == "closed"
         || name == "backpressure"
         || name == "cleanup";
}

bool
resource_method_statement_preface_supported_latest(StyioAST* stmt) {
  if (stmt == nullptr) {
    return false;
  }
  return dynamic_cast<CommentAST*>(stmt) != nullptr
         || dynamic_cast<EmptyAST*>(stmt) != nullptr
         || dynamic_cast<PassAST*>(stmt) != nullptr
         || dynamic_cast<PrintAST*>(stmt) != nullptr
         || dynamic_cast<ResourceWriteAST*>(stmt) != nullptr
         || dynamic_cast<ResourceRedirectAST*>(stmt) != nullptr
         || dynamic_cast<ResourceEffectAST*>(stmt) != nullptr;
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
resource_method_preface_bind_type_latest(StyioSemaContext* an, StyioAST* stmt) {
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
    type = infer_expr_type(an, value);
  }
  return type;
}

bool
resource_method_value_preface_supported_latest(StyioSemaContext* an, StyioAST* stmt) {
  if (resource_method_statement_preface_supported_latest(stmt)) {
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
bind_resource_method_preface_type_latest(StyioSemaContext* an, StyioAST* stmt) {
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
  if (!type.isUndefined()) {
    an->record_local_binding_type(var->getNameAsStr(), var->getName()->getSymbolId(), type);
  }
}

bool
resource_method_value_preface_shape_supported_latest(StyioAST* stmt) {
  return resource_method_statement_preface_supported_latest(stmt)
         || dynamic_cast<FlexBindAST*>(stmt) != nullptr
         || dynamic_cast<FinalBindAST*>(stmt) != nullptr;
}

ReturnAST*
resource_method_value_tail_return_latest(StyioAST* body) {
  if (auto* ret = dynamic_cast<ReturnAST*>(body)) {
    return ret;
  }
  auto* block = dynamic_cast<BlockAST*>(body);
  if (block == nullptr || !block->followings.empty() || block->stmts.empty()) {
    return nullptr;
  }
  auto* tail = dynamic_cast<ReturnAST*>(block->stmts.back());
  if (tail == nullptr) {
    return nullptr;
  }
  for (std::size_t i = 0; i + 1 < block->stmts.size(); ++i) {
    if (!resource_method_value_preface_shape_supported_latest(block->stmts[i])) {
      return nullptr;
    }
  }
  return tail;
}

bool
resource_method_body_contains_return_latest(StyioAST* body) {
  if (body == nullptr) {
    return false;
  }
  if (dynamic_cast<ReturnAST*>(body) != nullptr) {
    return true;
  }
  if (auto* block = dynamic_cast<BlockAST*>(body)) {
    for (auto* stmt : block->stmts) {
      if (resource_method_body_contains_return_latest(stmt)) {
        return true;
      }
    }
    for (auto* following : block->followings) {
      if (resource_method_body_contains_return_latest(following)) {
        return true;
      }
    }
  }
  return false;
}

StyioDataType
resource_method_simple_result_type_latest(StyioSemaContext* an, StyioAST* body) {
  ReturnAST* ret = resource_method_value_tail_return_latest(body);
  if (ret == nullptr || ret->getExpr() == nullptr) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  if (auto* block = dynamic_cast<BlockAST*>(body)) {
    auto saved_types = an->local_binding_types;
    auto saved_types_by_sid = an->local_binding_types_by_sid;
    try {
      for (std::size_t i = 0; i + 1 < block->stmts.size(); ++i) {
        if (!resource_method_value_preface_supported_latest(an, block->stmts[i])) {
          an->local_binding_types = saved_types;
          an->local_binding_types_by_sid = saved_types_by_sid;
          return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
        }
        bind_resource_method_preface_type_latest(an, block->stmts[i]);
      }
      ret->getExpr()->typeInfer(an);
      StyioDataType result_type = infer_expr_type(an, ret->getExpr());
      an->local_binding_types = saved_types;
      an->local_binding_types_by_sid = saved_types_by_sid;
      return result_type;
    }
    catch (...) {
      an->local_binding_types = saved_types;
      an->local_binding_types_by_sid = saved_types_by_sid;
      throw;
    }
  }
  StyioDataType result_type = infer_expr_type(an, ret->getExpr());
  return result_type;
}

StyioDataType
infer_list_literal_type(StyioSemaContext* an, ListAST* list) {
  StyioDataType existing_type = list->getDataType();
  if (styio_is_matrix_type(existing_type)) {
    return existing_type;
  }

  auto const& els = list->getElements();
  if (els.empty()) {
    if (styio_is_list_type(existing_type)) {
      return existing_type;
    }
    return StyioDataType{
      StyioDataTypeOption::Undefined, "undefined", 0
    };
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
    if (!sema_types_equal(an, next_type, elem_type)) {
      elem_type = kI64Type;
      break;
    }
  }

  return styio_make_list_type(elem_type.name);
}

StyioDataType
declared_function_return_type_latest(StyioAST* def) {
  auto declared_from_variant = [](const std::variant<TypeAST*, TypeTupleAST*>& ret_type) {
    if (ret_type.valueless_by_exception() || !std::holds_alternative<TypeAST*>(ret_type)) {
      return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
    }
    TypeAST* ty = std::get<TypeAST*>(ret_type);
    return ty != nullptr
      ? ty->getDataType()
      : StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  };
  if (auto* f = dynamic_cast<FunctionAST*>(def)) {
    return declared_from_variant(f->ret_type);
  }
  if (auto* f = dynamic_cast<SimpleFuncAST*>(def)) {
    return declared_from_variant(f->ret_type);
  }
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

void
maybe_intern_function_signature_types(
  StyioSemaContext* an,
  const std::vector<ParamAST*>& params,
  const std::variant<TypeAST*, TypeTupleAST*>& ret_type
) {
  if (an == nullptr) {
    return;
  }
  for (auto* param : params) {
    if (param == nullptr || param->getDType() == nullptr) {
      continue;
    }
    an->maybe_intern_type(param->getDType()->getDataType());
  }
  if (ret_type.valueless_by_exception() || !std::holds_alternative<TypeAST*>(ret_type)) {
    return;
  }
  TypeAST* return_type = std::get<TypeAST*>(ret_type);
  if (return_type != nullptr) {
    an->maybe_intern_type(return_type->getDataType());
  }
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
merge_matrix_elem_types(
  StyioDataType current,
  StyioDataType next,
  StyioSemaContext* an = nullptr
) {
  if (current.isUndefined()) {
    return next;
  }
  if (next.isUndefined()) {
    return current;
  }
  if (!type_is_numeric_family(current) || !type_is_numeric_family(next)) {
    throw StyioTypeError("matrix elements must be numeric scalar values");
  }
  if (sema_types_equal(an, current, next)) {
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
      elem_type = merge_matrix_elem_types(elem_type, cell_type, an);
    }
  }

  if (elem_type.isUndefined()) {
    elem_type = kI64Type;
  }
  info.elem_type = elem_type;
  return info;
}

bool
container_value_assignable(
  const StyioDataType& target,
  const StyioDataType& actual,
  StyioSemaContext* an = nullptr
);

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

StyioDataType
apply_matrix_literal_context(
  StyioSemaContext* an,
  StyioAST* expr,
  const StyioDataType& target_type
) {
  if (!styio_is_matrix_type(target_type) || expr == nullptr) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  if (auto* list = dynamic_cast<ListAST*>(expr)) {
    MatrixLiteralInfo matrix = infer_matrix_literal_info(an, list);
    StyioDataType actual_type =
      styio_make_matrix_type(matrix.elem_type.name, matrix.rows, matrix.cols);
    if (styio_matrix_row_count(target_type) != 0 || styio_matrix_col_count(target_type) != 0) {
      require_same_matrix_shape(target_type, actual_type);
    }
    list->setDataType(actual_type);
    return actual_type;
  }
  if (auto* ret = dynamic_cast<ReturnAST*>(expr)) {
    return apply_matrix_literal_context(an, ret->getExpr(), target_type);
  }
  if (auto* block = dynamic_cast<BlockAST*>(expr)) {
    StyioDataType applied{StyioDataTypeOption::Undefined, "undefined", 0};
    for (auto* stmt : block->stmts) {
      StyioDataType next = apply_matrix_literal_context(an, stmt, target_type);
      if (!next.isUndefined()) {
        applied = next;
      }
    }
    for (auto* following : block->followings) {
      StyioDataType next = apply_matrix_literal_context(an, following, target_type);
      if (!next.isUndefined()) {
        applied = next;
      }
    }
    return applied;
  }
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

void
require_matrix_return_compatible_latest(
  const std::string& function_name,
  const StyioDataType& declared_return,
  const StyioDataType& actual_return
) {
  if (!styio_is_matrix_type(declared_return)) {
    return;
  }
  if (!styio_is_matrix_type(actual_return)) {
    throw StyioTypeError(
      "function `" + function_name
      + "` matrix return requires a matrix-compatible expression"
    );
  }
  require_same_matrix_shape(declared_return, actual_return);
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
      styio_matrix_col_count(matrix_type)
    );
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
      styio_matrix_col_count(lhs)
    );
  }
  if (op == StyioOpType::Binary_Mul) {
    require_matmul_compatible(lhs, rhs);
    return styio_make_matrix_type(
      elem.name,
      styio_matrix_row_count(lhs),
      styio_matrix_col_count(rhs)
    );
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

  auto require_count = [&](size_t n)
  {
    if (args.size() != n) {
      throw StyioTypeError(
        "matrix intrinsic `" + name + "` expects " + std::to_string(n)
        + " argument(s), got " + std::to_string(args.size())
      );
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
    if (!container_value_assignable(matrix_elem_type(args[0]), args[3], an)) {
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
        styio_matrix_row_count(args[0])
      );
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
      styio_matrix_col_count(args[0])
    );
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
      styio_matrix_col_count(args[0])
    );
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
      styio_matrix_col_count(args[1])
    );
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
  const StyioDataType& saved_type,
  StyioSemaContext* an = nullptr
) {
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
  if (sema_types_equal(an, then_type, else_type)) {
    return then_type;
  }
  if (type_is_numeric_family(then_type) && type_is_numeric_family(else_type)) {
    return getMaxType(then_type, else_type);
  }
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

StyioDataType
merge_match_value_type(
  const StyioDataType& current,
  const StyioDataType& next,
  StyioSemaContext* an = nullptr
) {
  if (current.isUndefined()) {
    return next;
  }
  if (next.isUndefined()) {
    return current;
  }
  if (sema_types_equal(an, current, next)) {
    return current;
  }
  StyioValueFamily current_family = styio_value_family_for_type(current);
  StyioValueFamily next_family = styio_value_family_for_type(next);
  if (current_family == StyioValueFamily::String || next_family == StyioValueFamily::String) {
    return kStringType;
  }
  if (type_is_numeric_family(current) && type_is_numeric_family(next)) {
    return getMaxType(current, next);
  }
  if ((current_family == StyioValueFamily::Bool || current_family == StyioValueFamily::Char)
      && (next_family == StyioValueFamily::Bool || next_family == StyioValueFamily::Char)) {
    return kI64Type;
  }
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

bool
match_result_type_supported(const StyioDataType& type) {
  if (type.isUndefined()) {
    return true;
  }
  StyioValueFamily family = styio_value_family_for_type(type);
  return family == StyioValueFamily::Integer
         || family == StyioValueFamily::Float
         || family == StyioValueFamily::Bool
         || family == StyioValueFamily::Char
         || family == StyioValueFamily::String;
}

bool
match_tail_value_expected(StyioAST* ast) {
  if (ast == nullptr) {
    return false;
  }
  switch (ast->getNodeType()) {
    case StyioNodeType::Return:
    case StyioNodeType::Bool:
    case StyioNodeType::Integer:
    case StyioNodeType::Float:
    case StyioNodeType::Char:
    case StyioNodeType::String:
    case StyioNodeType::FmtStr:
    case StyioNodeType::List:
    case StyioNodeType::Dict:
    case StyioNodeType::Range:
    case StyioNodeType::Id:
    case StyioNodeType::Call:
    case StyioNodeType::Attribute:
    case StyioNodeType::Access_By_Index:
    case StyioNodeType::Access_By_Name:
    case StyioNodeType::BinOp:
    case StyioNodeType::Compare:
    case StyioNodeType::Condition:
    case StyioNodeType::ResourceRef:
    case StyioNodeType::InstantPull:
    case StyioNodeType::ResourceEffect:
    case StyioNodeType::FlowBind:
    case StyioNodeType::MatchCases:
      return true;
    default:
      return false;
  }
}

StyioDataType
match_branch_tail_type(StyioSemaContext* an, StyioAST* ast) {
  if (ast == nullptr) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  if (auto* ret = dynamic_cast<ReturnAST*>(ast)) {
    if (ret->getExpr() != nullptr) {
      ret->getExpr()->typeInfer(an);
    }
    StyioDataType result = infer_expr_type(an, ret->getExpr());
    if (result.isUndefined()) {
      throw StyioTypeError("match branch return value has undefined type");
    }
    return result;
  }
  if (auto* block = dynamic_cast<BlockAST*>(ast)) {
    if (block->stmts.empty()) {
      return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
    }
    return match_branch_tail_type(an, block->stmts.back());
  }
  if (!match_tail_value_expected(ast)) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  StyioDataType result = infer_expr_type(an, ast);
  if (result.isUndefined()) {
    throw StyioTypeError("match branch value has undefined type");
  }
  return result;
}

StyioDataType
function_body_tail_type_latest(StyioSemaContext* an, StyioAST* ast) {
  if (ast == nullptr) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  if (auto* ret = dynamic_cast<ReturnAST*>(ast)) {
    if (ret->getExpr() == nullptr) {
      return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
    }
    ret->getExpr()->typeInfer(an);
    return infer_expr_type(an, ret->getExpr());
  }
  if (auto* block = dynamic_cast<BlockAST*>(ast)) {
    if (block->stmts.empty()) {
      return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
    }
    return function_body_tail_type_latest(an, block->stmts.back());
  }
  if (!match_tail_value_expected(ast)) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  ast->typeInfer(an);
  return infer_expr_type(an, ast);
}

bool
is_name_ast_latest(StyioAST* ast, const std::string& name) {
  auto* n = dynamic_cast<NameAST*>(ast);
  return n != nullptr && n->getAsStr() == name;
}

bool
match_pattern_supported_latest(StyioAST* pattern, const std::string* scrutinee_name) {
  if (dynamic_cast<IntAST*>(pattern) != nullptr) {
    return true;
  }
  auto* cmp = dynamic_cast<BinCompAST*>(pattern);
  if (cmp == nullptr || cmp->getSign() != CompType::EQ || scrutinee_name == nullptr) {
    return false;
  }
  return (is_name_ast_latest(cmp->getLHS(), *scrutinee_name)
          && dynamic_cast<IntAST*>(cmp->getRHS()) != nullptr)
         || (is_name_ast_latest(cmp->getRHS(), *scrutinee_name)
             && dynamic_cast<IntAST*>(cmp->getLHS()) != nullptr);
}

bool
type_is_runtime_dict_value(const StyioDataType& type) {
  return styio_type_supports_runtime_dict_value(type);
}

StyioDataType
merge_dict_value_types(
  StyioDataType current,
  StyioDataType next,
  StyioSemaContext* an = nullptr
) {
  if (current.isUndefined()) {
    return next;
  }
  if (next.isUndefined()) {
    return current;
  }
  if (sema_types_equal(an, current, next)) {
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
    "dict values must use one consistent runtime scalar/string family in this slice"
  );
}

bool
container_value_assignable(
  const StyioDataType& target,
  const StyioDataType& actual,
  StyioSemaContext* an
) {
  if (actual.isUndefined()) {
    return true;
  }
  if (sema_types_equal(an, target, actual)) {
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

void
apply_stdin_resource_effect_expected_type(StyioAST* expr, const StyioDataType& expected_type) {
  if (expected_type.isUndefined()) {
    return;
  }
  auto* effect = dynamic_cast<ResourceEffectAST*>(expr);
  if (effect == nullptr || !effect->isValueRequired()) {
    return;
  }
  auto* pull = dynamic_cast<InstantPullAST*>(effect->getOperation());
  if (pull == nullptr) {
    return;
  }
  auto* stream = dynamic_cast<StdStreamAST*>(pull->getResource());
  if (stream == nullptr || stream->getStreamKind() != StdStreamKind::Stdin) {
    return;
  }
  pull->setResultType(expected_type);
}

StyioDataType
infer_predefined_list_operation_type(StyioSemaContext* an, FuncCallAST* call) {
  if (call == nullptr || call->func_callee == nullptr) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  if (!styio_is_predefined_list_operation_name(call->getNameAsStr())) {
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
  if (!styio_is_predefined_string_operation_name(call->getNameAsStr())) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  StyioDataType callee_type = infer_expr_type(an, call->func_callee);
  if (callee_type.option != StyioDataTypeOption::String) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  return styio_make_list_type("string");
}

bool
func_param_accepts_arg(
  const StyioDataType& param_type,
  const StyioDataType& arg_type,
  StyioSemaContext* an = nullptr
) {
  if (param_type.isUndefined() || arg_type.isUndefined()) {
    return true;
  }
  if (sema_types_equal(an, param_type, arg_type)) {
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
          if (styio_is_matrix_type(dt)) {
            StyioDataType inferred_return = an->inferred_function_return_type(
              f->func_name->getSymbolId(),
              f->getNameAsStr());
            if (styio_is_matrix_type(inferred_return)) {
              return inferred_return;
            }
          }
          return dt;
        }
      }
    }
    StyioDataType inferred_return = an->inferred_function_return_type(
      f->func_name->getSymbolId(),
      f->getNameAsStr());
    if (!inferred_return.isUndefined()) {
      return inferred_return;
    }
    return function_body_tail_type_latest(an, f->func_body);
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
          if (styio_is_matrix_type(dt)) {
            StyioDataType inferred_return = an->inferred_function_return_type(
              f->func_name->getSymbolId(),
              f->func_name->getAsStr());
            if (styio_is_matrix_type(inferred_return)) {
              return inferred_return;
            }
          }
          return dt;
        }
      }
    }
    StyioDataType inferred_return = an->inferred_function_return_type(
      f->func_name->getSymbolId(),
      f->func_name->getAsStr());
    if (!inferred_return.isUndefined()) {
      return inferred_return;
    }
    return infer_expr_type(an, f->ret_expr);
  }

  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

StyioDataType
infer_dict_literal_type(StyioSemaContext* an, DictAST* dict) {
  auto const& entries = dict->getEntries();
  if (entries.empty()) {
    StyioDataType existing_type = dict->getDataType();
    if (styio_is_dict_type(existing_type)) {
      return existing_type;
    }
    return StyioDataType{
      StyioDataTypeOption::Undefined, "undefined", 0
    };
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
      "dict values must have a runtime scalar or string type in this slice"
    );
  }

  for (size_t i = 1; i < entries.size(); ++i) {
    StyioDataType next_type = infer_expr_type(an, entries[i].value);
    if (next_type.isUndefined()) {
      continue;
    }
    if (!type_is_runtime_dict_value(next_type)) {
      throw StyioTypeError(
        "dict values must have a runtime scalar or string type in this slice"
      );
    }
    value_type = merge_dict_value_types(value_type, next_type, an);
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
      return kI64Type;
    case StyioNodeType::Float:
      return kF64Type;
    case StyioNodeType::NumConvert:
      return static_cast<TypeConvertAST*>(expr)->getDataType();
    case StyioNodeType::Char:
      return static_cast<CharAST*>(expr)->getDataType();
    case StyioNodeType::String:
    case StyioNodeType::FmtStr:
      return kStringType;
    case StyioNodeType::List:
      return infer_list_literal_type(an, static_cast<ListAST*>(expr));
    case StyioNodeType::Dict:
      return infer_dict_literal_type(an, static_cast<DictAST*>(expr));
    case StyioNodeType::Range:
      return styio_make_list_type("i64");
    case StyioNodeType::StdinResource:
    case StyioNodeType::StdoutResource:
    case StyioNodeType::StderrResource:
    case StyioNodeType::FileResource:
    case StyioNodeType::EmptyResource:
    case StyioNodeType::ResourceReceiver:
    case StyioNodeType::ResourceRef:
    case StyioNodeType::InstantPull:
    case StyioNodeType::TaskBlock:
    case StyioNodeType::ResourceEffect:
      return expr->getDataType();
    case StyioNodeType::FlowBind:
      return static_cast<FlowBindAST*>(expr)->getDataType();
    case StyioNodeType::MatchCases:
      return static_cast<MatchCasesAST*>(expr)->getDataType();
    case StyioNodeType::Attribute: {
      auto* attr = static_cast<AttrAST*>(expr);
      auto* attr_name = dynamic_cast<NameAST*>(attr->attr);
      if (attr_name == nullptr) {
        return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
      }
      const std::string attr_str = attr_name->getAsStr();
      StyioDataType base_type = infer_expr_type(an, attr->body);
      if (attr_str == "keys" && styio_is_dict_type(base_type)) {
        return styio_make_list_type(styio_dict_key_type_name(base_type));
      }
      if (attr_str == "values" && styio_is_dict_type(base_type)) {
        return styio_make_list_type(styio_dict_value_type_name(base_type));
      }
      const std::string family = resource_family_for_type(base_type);
      const StyioSemaContext::ResourceMethodInfo* method =
        an->find_resource_method(family, attr_str);
      if (method != nullptr && method->property) {
        const StyioBuiltinMethodKind builtin_method = styio_builtin_method_kind(attr_str);
        if (styio_is_resource_property_method_kind(builtin_method)) {
          return kStringType;
        }
        return method->result_type.isUndefined() ? kI64Type : method->result_type;
      }
      return kI64Type;
    }
    case StyioNodeType::Access_By_Index: {
      auto* access = static_cast<ListOpAST*>(expr);
      if (auto* row_access = dynamic_cast<ListOpAST*>(access->getList())) {
        if (row_access->getOp() == StyioNodeType::Access_By_Index) {
          StyioDataType matrix_type = infer_expr_type(an, row_access->getList());
          if (styio_is_matrix_type(matrix_type)) {
            return matrix_elem_type(matrix_type);
          }
        }
      }
      StyioDataType base_type = infer_expr_type(an, access->getList());
      if (styio_is_matrix_type(base_type)) {
        return styio_make_list_type(styio_matrix_elem_type_name(base_type));
      }
      if (!styio_type_is_indexable(base_type)) {
        return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
      }
      return styio_data_type_from_name(styio_type_item_type_name(base_type));
    }
    case StyioNodeType::Access_By_Slice: {
      auto* access = static_cast<ListOpAST*>(expr);
      StyioDataType base_type = infer_expr_type(an, access->getList());
      if (styio_is_matrix_type(base_type)) {
        return styio_make_list_type(styio_type_item_type_name(base_type));
      }
      if (styio_is_dict_type(base_type)) {
        return styio_make_list_type(styio_dict_value_type_name(base_type));
      }
      if (!styio_is_list_type(base_type)) {
        return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
      }
      return base_type;
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
        StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0},
        an
      );
    }
    case StyioNodeType::Id: {
      auto* nm = static_cast<NameAST*>(expr);
      if (const StyioDataType* type =
            an->find_local_binding_type(nm->getSymbolId(), nm->getAsStr())) {
        return *type;
      }
      return expr->getDataType();
    }
    case StyioNodeType::Call: {
      auto* call = static_cast<FuncCallAST*>(expr);
      if (!call->getDataType().isUndefined()) {
        return call->getDataType();
      }
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
      if (StyioAST* def = an->find_function_def(
            call->func_name->getSymbolId(),
            call->getNameAsStr()
          )) {
        return func_ret_type_of_def(an, def);
      }
      if (call->func_callee != nullptr) {
        const std::string family = resource_family_for_expr(an, call->func_callee);
        const StyioSemaContext::ResourceMethodInfo* method =
          an->find_resource_method(family, call->getNameAsStr());
        if (method != nullptr && !method->property) {
          return method->result_type;
        }
      }
      if (const auto* native = an->find_native_function_def(
            call->func_name->getSymbolId(),
            call->getNameAsStr()
          )) {
        return native->return_type;
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
      styio_data_type_from_name(styio_type_item_type_name(t))
    );
  }
  if (styio_is_dict_type(t)) {
    return styio_dict_key_type_name(t) == "string"
           && styio_type_supports_runtime_dict_value(
             styio_data_type_from_name(styio_dict_value_type_name(t))
           );
  }
  if (t.handle_family == StyioHandleFamily::Range) {
    return styio_value_family_is_runtime_scalar(styio_type_item_value_family(t));
  }
  return false;
}

std::string
resource_family_for_type(const StyioDataType& type) {
  if (type.handle_family == StyioHandleFamily::File) {
    return "file";
  }
  if (type.handle_family == StyioHandleFamily::Stream && type.has_std_stream_kind) {
    return styio_std_stream_family_name(static_cast<StdStreamKind>(type.std_stream_kind));
  }
  if (styio_is_topology_resource_type(type)) {
    return "resource";
  }
  return "";
}

std::string
resource_family_for_expr(StyioSemaContext* an, StyioAST* expr) {
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
  if (auto* name = dynamic_cast<NameAST*>(expr)) {
    if (const StyioDataType* type =
          an->find_local_binding_type(name->getSymbolId(), name->getAsStr())) {
      return resource_family_for_type(*type);
    }
  }
  return resource_family_for_type(infer_expr_type(an, expr));
}

bool
resource_effect_index_operation_supported_latest(StyioSemaContext* an, ListOpAST* access) {
  if (access == nullptr) {
    return false;
  }
  StyioDataType base_type = infer_expr_type(an, access->getList());
  if (access->getOp() == StyioNodeType::Access_By_Slice) {
    return styio_is_list_type(base_type)
           || styio_is_matrix_type(base_type)
           || styio_is_dict_type(base_type);
  }
  if (access->getOp() != StyioNodeType::Access_By_Index) {
    return false;
  }
  return styio_is_list_type(base_type)
         || styio_is_dict_type(base_type)
         || styio_is_matrix_type(base_type);
}

bool
resource_effect_iterator_operation_supported_latest(StyioSemaContext* an, IteratorAST* iter) {
  if (iter == nullptr || iter->getNodeType() == StyioNodeType::IterSeq) {
    return false;
  }
  if (dynamic_cast<FileResourceAST*>(iter->collection) != nullptr) {
    return true;
  }
  StyioDataType collection_type = infer_expr_type(an, iter->collection);
  return collection_type.handle_family == StyioHandleFamily::File;
}

bool
resource_effect_operation_supported_latest(StyioSemaContext* an, StyioAST* ast) {
  if (auto* acquire = dynamic_cast<HandleAcquireAST*>(ast)) {
    (void)an;
    return dynamic_cast<FileResourceAST*>(acquire->getResource()) != nullptr;
  }
  if (auto* bind = dynamic_cast<FlexBindAST*>(ast)) {
    return dynamic_cast<FileResourceAST*>(bind->getValue()) != nullptr;
  }
  if (dynamic_cast<ResourceWriteAST*>(ast) != nullptr
      || dynamic_cast<ResourceRedirectAST*>(ast) != nullptr
      || dynamic_cast<InstantPullAST*>(ast) != nullptr
      || dynamic_cast<ResourceRefAST*>(ast) != nullptr) {
    return true;
  }
  if (auto* access = dynamic_cast<ListOpAST*>(ast)) {
    return resource_effect_index_operation_supported_latest(an, access);
  }
  if (auto* iter = dynamic_cast<IteratorAST*>(ast)) {
    return resource_effect_iterator_operation_supported_latest(an, iter);
  }
  auto* call = dynamic_cast<FuncCallAST*>(ast);
  if (call == nullptr || call->func_callee == nullptr) {
    return false;
  }
  const std::string family = resource_family_for_expr(an, call->func_callee);
  if (family.empty()) {
    return false;
  }
  const auto* method = an->find_resource_method(family, call->getNameAsStr());
  return method != nullptr && !method->property;
}

bool
body_consumes_receiver(StyioSemaContext* an, StyioAST* ast, const std::string& family) {
  if (ast == nullptr) {
    return false;
  }
  if (auto* redirect = dynamic_cast<ResourceRedirectAST*>(ast)) {
    auto* receiver = dynamic_cast<ResourceReceiverAST*>(redirect->getData());
    if (receiver != nullptr && receiver->getFamilyName() == family
        && dynamic_cast<EmptyResourceAST*>(redirect->getResource()) != nullptr) {
      return true;
    }
    return body_consumes_receiver(an, redirect->getData(), family)
           || body_consumes_receiver(an, redirect->getResource(), family);
  }
  if (auto* write = dynamic_cast<ResourceWriteAST*>(ast)) {
    return body_consumes_receiver(an, write->getData(), family)
           || body_consumes_receiver(an, write->getResource(), family);
  }
  if (auto* block = dynamic_cast<BlockAST*>(ast)) {
    for (auto* stmt : block->stmts) {
      if (body_consumes_receiver(an, stmt, family)) {
        return true;
      }
    }
    for (auto* following : block->followings) {
      if (body_consumes_receiver(an, following, family)) {
        return true;
      }
    }
    return false;
  }
  if (auto* call = dynamic_cast<FuncCallAST*>(ast)) {
    if (auto* receiver = dynamic_cast<ResourceReceiverAST*>(call->func_callee)) {
      if (receiver->getFamilyName() == family) {
        const auto* method = an->find_resource_method(family, call->getNameAsStr());
        if (method != nullptr && method->consuming) {
          return true;
        }
      }
    }
    if (body_consumes_receiver(an, call->func_callee, family)) {
      return true;
    }
    for (auto* arg : call->getArgList()) {
      if (body_consumes_receiver(an, arg, family)) {
        return true;
      }
    }
    return false;
  }
  if (auto* bin = dynamic_cast<BinOpAST*>(ast)) {
    return body_consumes_receiver(an, bin->getLHS(), family)
           || body_consumes_receiver(an, bin->getRHS(), family);
  }
  if (auto* attr = dynamic_cast<AttrAST*>(ast)) {
    return body_consumes_receiver(an, attr->body, family)
           || body_consumes_receiver(an, attr->attr, family);
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
    case StyioValueFamily::RangeHandle:
    case StyioValueFamily::ListHandle:
      return StyioSemaContext::BindingValueKind::ListHandle;
    case StyioValueFamily::DictHandle:
      return StyioSemaContext::BindingValueKind::DictHandle;
    case StyioValueFamily::MatrixHandle:
      return StyioSemaContext::BindingValueKind::MatrixHandle;
    case StyioValueFamily::TaskHandle:
      return StyioSemaContext::BindingValueKind::TaskHandle;
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

StyioDataType
task_result_type_from_task_type(const StyioDataType& type) {
  if (type.handle_family != StyioHandleFamily::Task) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  const std::string result_name = styio_task_result_type_name(type);
  if (result_name == "unit") {
    return kI64Type;
  }
  return styio_data_type_from_name(result_name);
}

StyioDataType
infer_task_block_result_type(StyioSemaContext* an, BlockAST* block) {
  if (block == nullptr) {
    return kI64Type;
  }
  StyioDataType result{StyioDataTypeOption::Undefined, "undefined", 0};
  for (auto* stmt : block->stmts) {
    if (auto* ret = dynamic_cast<ReturnAST*>(stmt)) {
      if (ret->getExpr() != nullptr) {
        ret->getExpr()->typeInfer(an);
      }
      result = infer_expr_type(an, ret->getExpr());
      continue;
    }
    if (auto* nested = dynamic_cast<BlockAST*>(stmt)) {
      StyioDataType nested_result = infer_task_block_result_type(an, nested);
      if (!nested_result.isUndefined()) {
        result = nested_result;
      }
    }
  }
  return result.isUndefined() ? kI64Type : result;
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
      : kI64Type
  );
  return true;
}

bool
callable_generalization_domain_accepts(const StyioDataType& type) {
  constexpr std::uint32_t stateful_capabilities =
    styio_caps(StyioTypeCapability::Readable)
    | styio_caps(StyioTypeCapability::Writable)
    | styio_caps(StyioTypeCapability::Pull)
    | styio_caps(StyioTypeCapability::Push)
    | styio_caps(StyioTypeCapability::Close)
    | styio_caps(StyioTypeCapability::Send)
    | styio_caps(StyioTypeCapability::Sync);
  if (type.isUndefined()
      || type.is_resource_type
      || type.resource_shape != StyioResourceShapeKind::None
      || type.has_std_stream_kind
      || (type.capabilities & stateful_capabilities) != 0) {
    return false;
  }

  if (styio_is_list_type(type)) {
    return type.handle_family == StyioHandleFamily::List
           && type.state == StyioTypeState::Materialized
           && callable_generalization_domain_accepts(
             styio_data_type_from_name(
               styio_list_elem_type_name(type)));
  }
  if (styio_is_dict_type(type)) {
    return type.handle_family == StyioHandleFamily::Dict
           && type.state == StyioTypeState::Materialized
           && callable_generalization_domain_accepts(
             styio_data_type_from_name(
               styio_dict_key_type_name(type)))
           && callable_generalization_domain_accepts(
             styio_data_type_from_name(
               styio_dict_value_type_name(type)));
  }
  if (type.handle_family != StyioHandleFamily::None
      || type.state != StyioTypeState::None
      || type.capabilities != 0) {
    return false;
  }

  switch (type.option) {
    case StyioDataTypeOption::Bool:
    case StyioDataTypeOption::Integer:
    case StyioDataTypeOption::Float:
    case StyioDataTypeOption::Decimal:
    case StyioDataTypeOption::Char:
    case StyioDataTypeOption::String:
      return true;
    default:
      return false;
  }
}

bool
match_callable_term_to_concrete(
  const CallableTypeTerm& pattern,
  const StyioDataType& concrete_input,
  std::unordered_map<std::uint32_t, StyioDataType>& bindings,
  const std::string& context
) {
  if (auto variable = first_callable_term_variable(pattern)) {
    if (concrete_input.isUndefined()) {
      throw StyioTypeError(
        "inferred relation variable '" + std::to_string(*variable)
        + " is underconstrained in " + context
        + "; add a concrete surrounding annotation"
      );
    }
    if (!callable_generalization_domain_accepts(concrete_input)) {
      throw StyioTypeError(
        "inferred relation variable '" + std::to_string(*variable)
        + " cannot range over type `" + concrete_input.name + "` in "
        + context + "; generalized callable variables admit only immutable "
          "scalar values and pure materialized list/dict types"
      );
    }
  }

  StyioDataType concrete =
    normalize_callable_concrete_type(concrete_input);
  switch (pattern.kind) {
    case CallableTypeTerm::Kind::Variable: {
      auto [it, inserted] = bindings.emplace(pattern.variable, concrete);
      if (inserted) {
        return true;
      }
      if (it->second.equals(concrete)) {
        return false;
      }
      if (it->second.option == StyioDataTypeOption::Integer
          && concrete.option == StyioDataTypeOption::Integer) {
        if (concrete.num_of_bit == 0) {
          return false;
        }
        if (it->second.num_of_bit == 0) {
          it->second = concrete;
          return true;
        }
      }
      if (it->second.option == StyioDataTypeOption::Bool
          && concrete.option == StyioDataTypeOption::Bool) {
        return false;
      }
      if (it->second.option == StyioDataTypeOption::String
          && concrete.option == StyioDataTypeOption::String) {
        return false;
      }
      if (it->second.option == StyioDataTypeOption::Char
          && concrete.option == StyioDataTypeOption::Char
          && it->second.num_of_bit == concrete.num_of_bit) {
        return false;
      }
      {
        throw StyioTypeError(
          "generic instance conflict in " + context + ": inferred `"
          + it->second.name + "` and `" + concrete.name
          + "` for the same relation variable"
        );
      }
    }
    case CallableTypeTerm::Kind::Concrete: {
      StyioDataType expected =
        normalize_callable_concrete_type(pattern.concrete);
      const bool same_numeric_family =
        (expected.option == StyioDataTypeOption::Integer
         && concrete.option == StyioDataTypeOption::Integer)
        || (expected.option == StyioDataTypeOption::Float
            && concrete.option == StyioDataTypeOption::Float);
      if (!expected.equals(concrete) && !same_numeric_family) {
        throw StyioTypeError(
          "generic instance conflict in " + context + ": expected `"
          + expected.name + "`, got `" + concrete.name + "`"
        );
      }
      return false;
    }
    case CallableTypeTerm::Kind::List:
      if (!styio_is_list_type(concrete)) {
        throw StyioTypeError(
          "generic instance conflict in " + context
          + ": expected list[T], got `" + concrete.name + "`"
        );
      }
      return match_callable_term_to_concrete(
        pattern.arguments.at(0),
        styio_data_type_from_name(styio_list_elem_type_name(concrete)),
        bindings,
        context
      );
    case CallableTypeTerm::Kind::Dict:
      if (!styio_is_dict_type(concrete)) {
        throw StyioTypeError(
          "generic instance conflict in " + context
          + ": expected dict[K,V], got `" + concrete.name + "`"
        );
      }
      {
      bool changed = match_callable_term_to_concrete(
        pattern.arguments.at(0),
        styio_data_type_from_name(styio_dict_key_type_name(concrete)),
        bindings,
        context
      );
      changed = match_callable_term_to_concrete(
        pattern.arguments.at(1),
        styio_data_type_from_name(styio_dict_value_type_name(concrete)),
        bindings,
        context
      ) || changed;
      return changed;
      }
  }
  return false;
}

std::optional<StyioDataType>
closed_callable_term_type(const CallableTypeTerm& term) {
  switch (term.kind) {
    case CallableTypeTerm::Kind::Variable:
      return std::nullopt;
    case CallableTypeTerm::Kind::Concrete:
      return normalize_callable_concrete_type(term.concrete);
    case CallableTypeTerm::Kind::List: {
      auto element = closed_callable_term_type(term.arguments.at(0));
      return element.has_value()
               ? std::optional<StyioDataType>(
                   styio_make_list_type(element->name))
               : std::nullopt;
    }
    case CallableTypeTerm::Kind::Dict: {
      auto key = closed_callable_term_type(term.arguments.at(0));
      auto value = closed_callable_term_type(term.arguments.at(1));
      return key.has_value() && value.has_value()
               ? std::optional<StyioDataType>(
                   styio_make_dict_type(key->name, value->name))
               : std::nullopt;
    }
  }
  return std::nullopt;
}

StyioDataType
resolve_callable_term_to_concrete(
  const CallableTypeTerm& pattern,
  const std::unordered_map<std::uint32_t, StyioDataType>& bindings,
  const std::string& callable_name
) {
  switch (pattern.kind) {
    case CallableTypeTerm::Kind::Variable: {
      auto binding = bindings.find(pattern.variable);
      if (binding == bindings.end()) {
        throw StyioTypeError(
          "call to `" + callable_name
          + "` is underconstrained; add a concrete surrounding annotation"
        );
      }
      return normalize_callable_concrete_type(binding->second);
    }
    case CallableTypeTerm::Kind::Concrete:
      return normalize_callable_concrete_type(pattern.concrete);
    case CallableTypeTerm::Kind::List:
      return styio_make_list_type(
        resolve_callable_term_to_concrete(
          pattern.arguments.at(0),
          bindings,
          callable_name
        ).name
      );
    case CallableTypeTerm::Kind::Dict:
      return styio_make_dict_type(
        resolve_callable_term_to_concrete(
          pattern.arguments.at(0),
          bindings,
          callable_name
        ).name,
        resolve_callable_term_to_concrete(
          pattern.arguments.at(1),
          bindings,
          callable_name
        ).name
      );
  }
  return StyioDataType{
    StyioDataTypeOption::Undefined, "undefined", 0
  };
}

std::optional<StyioDataType>
bound_callable_term_type(
  const CallableTypeTerm& pattern,
  const std::unordered_map<std::uint32_t, StyioDataType>& bindings
) {
  switch (pattern.kind) {
    case CallableTypeTerm::Kind::Variable: {
      auto binding = bindings.find(pattern.variable);
      return binding == bindings.end()
               ? std::nullopt
               : std::optional<StyioDataType>(
                   normalize_callable_concrete_type(binding->second));
    }
    case CallableTypeTerm::Kind::Concrete:
      return normalize_callable_concrete_type(pattern.concrete);
    case CallableTypeTerm::Kind::List: {
      auto element = bound_callable_term_type(
        pattern.arguments.at(0),
        bindings);
      return element.has_value()
               ? std::optional<StyioDataType>(
                   styio_make_list_type(element->name))
               : std::nullopt;
    }
    case CallableTypeTerm::Kind::Dict: {
      auto key = bound_callable_term_type(
        pattern.arguments.at(0),
        bindings);
      auto value = bound_callable_term_type(
        pattern.arguments.at(1),
        bindings);
      return key.has_value() && value.has_value()
               ? std::optional<StyioDataType>(
                   styio_make_dict_type(key->name, value->name))
               : std::nullopt;
    }
  }
  return std::nullopt;
}

bool
solve_callable_constraint_instance(
  const CallableTypeConstraint& constraint,
  std::unordered_map<std::uint32_t, StyioDataType>& bindings,
  const std::string& callable_name
) {
  auto subject = bound_callable_term_type(constraint.subject, bindings);
  if (!subject.has_value()) {
    return false;
  }
  const std::string context =
    "constraint `" + callable_constraint_text(constraint)
    + "` of `" + callable_name + "`";
  if (constraint.kind == CallableConstraintKind::Numeric
      || constraint.kind == CallableConstraintKind::Comparable
      || constraint.kind == CallableConstraintKind::Cloneable) {
    validate_callable_unary_constraint(
      constraint.kind,
      *subject,
      callable_constraint_text(constraint));
    return false;
  }

  validate_callable_unary_constraint(
    constraint.kind,
    *subject,
    callable_constraint_text(constraint));
  if (constraint.kind == CallableConstraintKind::Indexable) {
    StyioDataType key = styio_is_dict_type(*subject)
                          ? styio_data_type_from_name(
                              styio_dict_key_type_name(*subject))
                          : kI64Type;
    StyioDataType result = styio_data_type_from_name(
      styio_type_item_type_name(*subject));
    bool changed = match_callable_term_to_concrete(
      constraint.argument,
      key,
      bindings,
      context);
    changed = match_callable_term_to_concrete(
      constraint.result,
      result,
      bindings,
      context) || changed;
    return changed;
  }

  StyioDataType element = styio_data_type_from_name(
    styio_type_item_type_name(*subject));
  return match_callable_term_to_concrete(
    constraint.result,
    element,
    bindings,
    context);
}

void
solve_callable_constraint_instance(
  const CallableTypeScheme& scheme,
  std::unordered_map<std::uint32_t, StyioDataType>& bindings
) {
  auto saturate = [&]()
  {
    while (true) {
      bool changed = false;
      for (const auto& constraint : scheme.constraints) {
        changed =
          solve_callable_constraint_instance(
            constraint,
            bindings,
            scheme.name) || changed;
      }
      if (!changed) {
        break;
      }
    }
  };

  saturate();

  for (std::uint32_t variable : scheme.quantified_variables) {
    if (bindings.count(variable) != 0) {
      continue;
    }
    bool has_constraint = false;
    bool numeric_only = true;
    for (const auto& constraint : scheme.constraints) {
      if (!callable_constraint_contains_variable(constraint, variable)) {
        continue;
      }
      has_constraint = true;
      if (constraint.kind != CallableConstraintKind::Numeric
          || !callable_term_contains_variable(
               constraint.subject,
               variable)) {
        numeric_only = false;
        break;
      }
    }
    if (has_constraint && numeric_only) {
      bindings.emplace(variable, kI64Type);
    }
  }

  saturate();
  for (const auto& constraint : scheme.constraints) {
    if (!bound_callable_term_type(constraint.subject, bindings).has_value()) {
      throw StyioTypeError(
        "call to `" + scheme.name + "` is underconstrained at `"
        + callable_constraint_text(constraint)
        + "`; add a concrete surrounding annotation"
      );
    }
    (void)solve_callable_constraint_instance(
      constraint,
      bindings,
      scheme.name);
  }
}

std::string
callable_concrete_key(
  std::string_view name,
  const std::vector<StyioDataType>& params,
  const StyioDataType& result
) {
  std::ostringstream output;
  output << name << "(";
  for (std::size_t i = 0; i < params.size(); ++i) {
    if (i != 0) {
      output << ",";
    }
    output << params[i].name;
  }
  output << ")->" << result.name;
  return output.str();
}

std::string
callable_specialization_content_digest(
  std::string_view definition,
  std::string_view relation,
  std::string_view effects,
  std::string_view checked_body_digest,
  std::string_view callable_dependency_digest,
  std::string_view module_dependency_digest,
  std::string_view backend_abi
) {
  std::ostringstream content;
  content
    << "styio.callable-specialization.v1\n"
    << "definition=" << definition << "\n"
    << "relation=" << relation << "\n"
    << "effects=" << effects << "\n"
    << "body=" << checked_body_digest << "\n"
    << "callable_dependencies="
    << callable_dependency_digest << "\n"
    << "dependencies=" << module_dependency_digest << "\n"
    << "backend=" << backend_abi << "\n";
  return styio::sema::callable_interface_sha256_hex(
    content.str());
}

struct CallableDependencyFingerprints
{
  std::unordered_map<const StyioAST*, std::string> body_digests;
  std::unordered_map<std::string, std::string> dependency_digests;
};

CallableDependencyFingerprints
build_callable_dependency_fingerprints(
  StyioSemaContext* context,
  const std::unordered_map<std::string, StyioAST*>& definitions
) {
  CallableDependencyFingerprints result;
  if (context == nullptr || definitions.empty()) {
    return result;
  }

  std::vector<std::string> names;
  names.reserve(definitions.size());
  for (const auto& [name, definition] : definitions) {
    (void)definition;
    names.push_back(name);
  }
  std::sort(names.begin(), names.end());

  std::unordered_map<std::string, std::string> base_fingerprints;
  std::unordered_map<std::string, std::vector<std::string>> graph;
  for (const auto& name : names) {
    StyioAST* definition = definitions.at(name);
    std::string body_digest;
    std::string owner;
    std::string interface_digest;
    if (const auto* imported =
          context->find_imported_callable_definition(name)) {
      body_digest = imported->checked_body_digest;
      owner = imported->module_id;
      interface_digest = imported->interface_abi_digest;
    }
    if (body_digest.empty()) {
      StyioRepr representation;
      body_digest =
        styio::sema::callable_interface_sha256_hex(
          definition->toString(&representation));
    }
    result.body_digests[definition] = body_digest;

    std::ostringstream base;
    base << "styio.callable-definition.v1\n"
         << "definition="
         << (owner.empty() ? name : owner + "::" + name)
         << "\n";
    if (const auto* scheme =
          context->find_callable_type_scheme(name)) {
      base << "relation=" << scheme->canonical_relation << "\n";
    }
    else {
      base << "relation=(";
      const auto params = params_of_func_def(definition);
      for (std::size_t i = 0; i < params.size(); ++i) {
        if (i != 0) {
          base << ",";
        }
        base << params[i]->getDType()->getDataType().name;
      }
      base << ")->"
           << callable_declared_result_type(definition).name
           << "\n";
    }
    const auto* effects =
      context->find_callable_effect_summary(name);
    base << "effects="
         << (effects == nullptr ? "unknown" : effects->canonical)
         << "\n"
         << "body=" << body_digest << "\n"
         << "interface=" << interface_digest << "\n";
    base_fingerprints[name] =
      styio::sema::callable_interface_sha256_hex(base.str());

    std::vector<std::string> callees;
    if (effects != nullptr) {
      callees = effects->direct_callees;
      std::sort(callees.begin(), callees.end());
      callees.erase(
        std::unique(callees.begin(), callees.end()),
        callees.end());
    }
    graph[name] = std::move(callees);
  }

  std::unordered_map<std::string, int> index;
  std::unordered_map<std::string, int> lowlink;
  std::unordered_set<std::string> on_stack;
  std::vector<std::string> stack;
  std::vector<std::vector<std::string>> components;
  int next_index = 0;
  std::function<void(const std::string&)> strong_connect;
  strong_connect = [&](const std::string& name)
  {
    index[name] = next_index;
    lowlink[name] = next_index;
    ++next_index;
    stack.push_back(name);
    on_stack.insert(name);

    for (const auto& target : graph.at(name)) {
      if (definitions.count(target) == 0) {
        continue;
      }
      if (index.count(target) == 0) {
        strong_connect(target);
        lowlink[name] =
          std::min(lowlink[name], lowlink[target]);
      }
      else if (on_stack.count(target) != 0) {
        lowlink[name] =
          std::min(lowlink[name], index[target]);
      }
    }
    if (lowlink[name] != index[name]) {
      return;
    }

    std::vector<std::string> component;
    while (!stack.empty()) {
      std::string member = std::move(stack.back());
      stack.pop_back();
      on_stack.erase(member);
      component.push_back(member);
      if (member == name) {
        break;
      }
    }
    std::sort(component.begin(), component.end());
    components.push_back(std::move(component));
  };
  for (const auto& name : names) {
    if (index.count(name) == 0) {
      strong_connect(name);
    }
  }

  std::unordered_map<std::string, std::size_t> component_for;
  for (std::size_t i = 0; i < components.size(); ++i) {
    for (const auto& name : components[i]) {
      component_for[name] = i;
    }
  }
  std::vector<std::optional<std::string>>
    component_digests(components.size());
  std::function<const std::string&(std::size_t)>
    digest_component;
  digest_component = [&](std::size_t component_index)
    -> const std::string&
  {
    if (component_digests[component_index].has_value()) {
      return *component_digests[component_index];
    }

    std::vector<std::string> dependency_facts;
    std::ostringstream canonical;
    canonical << "styio.callable-dependency-component.v1\n";
    for (const auto& name : components[component_index]) {
      canonical << "member=" << name << "|"
                << base_fingerprints.at(name) << "\n";
      for (const auto& target : graph.at(name)) {
        auto target_component = component_for.find(target);
        if (target_component == component_for.end()) {
          canonical << "external=" << name << "->"
                    << target << "\n";
          continue;
        }
        if (target_component->second == component_index) {
          canonical << "internal=" << name << "->"
                    << target << "\n";
          continue;
        }
        dependency_facts.push_back(
          name + "->" + target + "|"
          + digest_component(target_component->second));
      }
    }
    std::sort(
      dependency_facts.begin(),
      dependency_facts.end());
    dependency_facts.erase(
      std::unique(
        dependency_facts.begin(),
        dependency_facts.end()),
      dependency_facts.end());
    for (const auto& dependency : dependency_facts) {
      canonical << "dependency=" << dependency << "\n";
    }
    component_digests[component_index] =
      styio::sema::callable_interface_sha256_hex(
        canonical.str());
    return *component_digests[component_index];
  };

  for (const auto& name : names) {
    result.dependency_digests[name] =
      digest_component(component_for.at(name));
  }
  return result;
}

void
apply_callable_expected_type_to_tail(
  StyioAST* ast,
  const StyioDataType& expected
) {
  if (ast == nullptr || expected.isUndefined()) {
    return;
  }
  if (auto* call = dynamic_cast<FuncCallAST*>(ast)) {
    call->setExpectedType(expected);
    return;
  }
  if (auto* list = dynamic_cast<ListAST*>(ast)) {
    if (styio_is_list_type(expected)) {
      list->setDataType(normalize_callable_concrete_type(expected));
      StyioDataType element = styio_data_type_from_name(
        styio_list_elem_type_name(expected));
      for (auto* value : list->getElements()) {
        apply_callable_expected_type_to_tail(value, element);
      }
    }
    return;
  }
  if (auto* dict = dynamic_cast<DictAST*>(ast)) {
    if (styio_is_dict_type(expected)) {
      dict->setDataType(normalize_callable_concrete_type(expected));
      StyioDataType value_type = styio_data_type_from_name(
        styio_dict_value_type_name(expected));
      for (const auto& entry : dict->getEntries()) {
        apply_callable_expected_type_to_tail(entry.value, value_type);
      }
    }
    return;
  }
  if (auto* binary = dynamic_cast<BinOpAST*>(ast)) {
    binary->setDType(normalize_callable_concrete_type(expected));
    return;
  }
  if (auto* ret = dynamic_cast<ReturnAST*>(ast)) {
    apply_callable_expected_type_to_tail(ret->getExpr(), expected);
    return;
  }
  if (auto* block = dynamic_cast<BlockAST*>(ast)) {
    if (!block->stmts.empty() && block->followings.empty()) {
      apply_callable_expected_type_to_tail(block->stmts.back(), expected);
    }
    return;
  }
  if (auto* match = dynamic_cast<MatchCasesAST*>(ast)) {
    CasesAST* cases = match->getCases();
    for (const auto& entry : cases->case_list) {
      apply_callable_expected_type_to_tail(entry.second, expected);
    }
    apply_callable_expected_type_to_tail(cases->case_default, expected);
    return;
  }
  if (auto* merge = dynamic_cast<WaveMergeAST*>(ast)) {
    apply_callable_expected_type_to_tail(merge->getTrueVal(), expected);
    apply_callable_expected_type_to_tail(merge->getFalseVal(), expected);
    return;
  }
  if (auto* dispatch = dynamic_cast<WaveDispatchAST*>(ast)) {
    apply_callable_expected_type_to_tail(dispatch->getTrueArm(), expected);
    apply_callable_expected_type_to_tail(dispatch->getFalseArm(), expected);
  }
}

}  // namespace

void
StyioSemaContext::prepare_callable_type_schemes(MainBlockAST* ast) {
  callable_type_schemes_.clear();
  callable_effect_summaries_.clear();
  effect_monomorphic_instances_.clear();
  callable_specializations_.clear();
  callable_specialization_cache_.clear();
  callable_checked_body_digests_.clear();
  callable_definition_dependency_digests_.clear();
  reachable_imported_concrete_callables_.clear();
  callable_specialization_graph_.reset();
  active_callable_specialization_.reset();

  if (ast == nullptr) {
    return;
  }

  for (const auto& imported : imported_callable_definitions_) {
    const std::string name = callable_name_of_def(imported.definition);
    if (imported.has_scheme) {
      callable_type_schemes_[name] = imported.scheme;
    }
    callable_effect_summaries_[name] = imported.effects;
  }

  std::unordered_map<std::string, StyioAST*> definitions;
  for (auto* statement : ast->getStmts()) {
    if ((dynamic_cast<FunctionAST*>(statement) != nullptr
         || dynamic_cast<SimpleFuncAST*>(statement) != nullptr)
        && callable_def_is_final_binding_latest(statement)) {
      definitions[callable_name_of_def(statement)] = statement;
    }
  }
  std::unordered_map<std::string, StyioAST*> available_definitions =
    definitions;
  for (const auto& imported : imported_callable_definitions_) {
    available_definitions[callable_name_of_def(imported.definition)] =
      imported.definition;
  }

  std::vector<std::string> definition_names;
  definition_names.reserve(definitions.size());
  for (const auto& [name, def] : definitions) {
    callable_effect_summaries_[name] =
      callable_local_effect_summary(this, def, available_definitions);
    definition_names.push_back(name);
  }
  std::sort(definition_names.begin(), definition_names.end());

  std::unordered_map<std::string, std::vector<std::string>> callers;
  for (const auto& name : definition_names) {
    for (const auto& callee :
         callable_effect_summaries_.at(name).direct_callees) {
      callers[callee].push_back(name);
    }
  }
  for (auto& [callee, dependent_callers] : callers) {
    (void)callee;
    std::sort(dependent_callers.begin(), dependent_callers.end());
  }

  std::vector<std::string> effect_worklist = definition_names;
  std::unordered_set<std::string> queued_effects(
    definition_names.begin(),
    definition_names.end());
  for (std::size_t cursor = 0;
       cursor < effect_worklist.size();
       ++cursor) {
    std::string name = std::move(effect_worklist[cursor]);
    queued_effects.erase(name);
    auto& summary = callable_effect_summaries_.at(name);
    const std::uint32_t previous_bits = summary.effect_bits;
    const bool previous_closed = summary.closed;
    for (const auto& callee : summary.direct_callees) {
      auto dependency = callable_effect_summaries_.find(callee);
      if (dependency == callable_effect_summaries_.end()) {
        add_callable_effect(summary, CallableEffectKind::Unknown);
        continue;
      }
      summary.effect_bits |= dependency->second.effect_bits;
      summary.closed = summary.closed && dependency->second.closed;
    }
    if (summary.effect_bits == previous_bits
        && summary.closed == previous_closed) {
      continue;
    }
    auto dependent_callers = callers.find(name);
    if (dependent_callers == callers.end()) {
      continue;
    }
    for (const auto& caller : dependent_callers->second) {
      if (queued_effects.insert(caller).second) {
        effect_worklist.push_back(caller);
      }
    }
  }
  for (auto& [name, summary] : callable_effect_summaries_) {
    (void)name;
    summary.canonical = canonical_callable_effect_summary(summary);
  }

  std::unordered_map<std::string, std::unordered_set<std::string>> graph;
  std::vector<std::string> seeds;
  for (const auto& name : definition_names) {
    const auto& effect_summary = callable_effect_summaries_.at(name);
    std::unordered_set<std::string> calls(
      effect_summary.direct_callees.begin(),
      effect_summary.direct_callees.end());
    graph[name] = std::move(calls);
    if (effect_summary.relation_seed
        && effect_summary.proven_pure()) {
      seeds.push_back(name);
    }
  }
  std::sort(seeds.begin(), seeds.end());

  std::unordered_set<std::string> selected;
  std::vector<std::string> pending = seeds;
  while (!pending.empty()) {
    std::string name = std::move(pending.back());
    pending.pop_back();
    if (!selected.insert(name).second) {
      continue;
    }
    std::vector<std::string> dependencies(
      graph[name].begin(),
      graph[name].end());
    std::sort(dependencies.begin(), dependencies.end());
    for (auto it = dependencies.rbegin(); it != dependencies.rend(); ++it) {
      if (definitions.count(*it) != 0) {
        pending.push_back(*it);
      }
    }
  }
  std::vector<std::string> nodes(selected.begin(), selected.end());
  std::sort(nodes.begin(), nodes.end());
  std::unordered_map<std::string, int> index;
  std::unordered_map<std::string, int> lowlink;
  std::unordered_set<std::string> on_stack;
  std::vector<std::string> stack;
  std::vector<std::vector<std::string>> components;
  int next_index = 0;

  std::function<void(const std::string&)> strong_connect;
  strong_connect = [&](const std::string& name)
  {
    index[name] = next_index;
    lowlink[name] = next_index;
    ++next_index;
    stack.push_back(name);
    on_stack.insert(name);

    std::vector<std::string> dependencies;
    for (const auto& target : graph[name]) {
      if (selected.count(target) != 0) {
        dependencies.push_back(target);
      }
    }
    std::sort(dependencies.begin(), dependencies.end());
    for (const auto& target : dependencies) {
      if (index.find(target) == index.end()) {
        strong_connect(target);
        lowlink[name] = std::min(lowlink[name], lowlink[target]);
      }
      else if (on_stack.count(target) != 0) {
        lowlink[name] = std::min(lowlink[name], index[target]);
      }
    }

    if (lowlink[name] != index[name]) {
      return;
    }
    std::vector<std::string> component;
    while (!stack.empty()) {
      std::string member = std::move(stack.back());
      stack.pop_back();
      on_stack.erase(member);
      component.push_back(member);
      if (member == name) {
        break;
      }
    }
    std::sort(component.begin(), component.end());
    components.push_back(std::move(component));
  };

  for (const auto& name : nodes) {
    if (index.find(name) == index.end()) {
      strong_connect(name);
    }
  }

  std::unordered_map<std::string, std::size_t> component_for;
  for (std::size_t i = 0; i < components.size(); ++i) {
    for (const auto& name : components[i]) {
      component_for[name] = i;
    }
  }

  std::vector<bool> inferred(components.size(), false);
  std::vector<bool> inferring(components.size(), false);
  std::function<void(std::size_t)> infer_component;
  infer_component = [&](std::size_t component_index)
  {
    if (inferred[component_index]) {
      return;
    }
    if (inferring[component_index]) {
      throw StyioTypeError(
        "internal error: recursive callable component ordering cycle"
      );
    }
    inferring[component_index] = true;

    for (const auto& name : components[component_index]) {
      std::vector<std::string> dependencies(
        graph[name].begin(),
        graph[name].end());
      std::sort(dependencies.begin(), dependencies.end());
      for (const auto& target : dependencies) {
        auto target_component = component_for.find(target);
        if (target_component != component_for.end()
            && target_component->second != component_index) {
          infer_component(target_component->second);
        }
      }
    }

    CallableTypeUnifier unifier;
    std::vector<CallableTypeConstraint> component_constraints;
    std::unordered_map<std::string, CallableMonotype> provisional;
    for (const auto& name : components[component_index]) {
      StyioAST* def = definitions.at(name);
      CallableMonotype signature;
      for (auto* param : params_of_func_def(def)) {
        StyioDataType declared = param->getDType()->getDataType();
        signature.params.push_back(
          declared.isUndefined()
            ? unifier.fresh()
            : callable_concrete_term(declared));
      }
      StyioDataType declared_result = callable_declared_result_type(def);
      signature.result = declared_result.isUndefined()
                           ? unifier.fresh()
                           : callable_concrete_term(declared_result);
      provisional[name] = std::move(signature);
    }

    for (const auto& name : components[component_index]) {
      StyioAST* def = definitions.at(name);
      CallableSymbolicInfer symbolic(
        unifier,
        callable_type_schemes_,
        provisional,
        available_definitions,
        component_constraints);
      auto params = params_of_func_def(def);
      for (std::size_t i = 0; i < params.size(); ++i) {
        symbolic.bind_local(
          params[i]->getNameAsStr(),
          provisional.at(name).params[i]);
      }
      try {
        CallableTypeTerm body_result =
          symbolic.infer(callable_body_of_def(def));
        unifier.unify(
          provisional.at(name).result,
          body_result,
          "result of `" + name + "`"
        );
      }
      catch (const StyioTypeError& error) {
        throw StyioTypeError(
          "cannot infer principal relation for final callable `" + name
          + "`: " + error.what()
        );
      }
    }

    reduce_callable_constraints(unifier, component_constraints);

    const bool recursive_group =
      components[component_index].size() > 1
      || graph[components[component_index].front()].count(
           components[component_index].front()) != 0;

    for (const auto& name : components[component_index]) {
      CallableTypeScheme scheme;
      scheme.name = name;
      scheme.recursive_group = recursive_group;
      for (const auto& param : provisional.at(name).params) {
        scheme.params.push_back(unifier.apply(param));
      }
      scheme.result = unifier.apply(provisional.at(name).result);

      std::vector<std::uint32_t> signature_variables;
      std::unordered_set<std::uint32_t> signature_variable_set;
      for (const auto& param : scheme.params) {
        collect_callable_term_variables(
          param,
          signature_variables,
          signature_variable_set);
      }
      collect_callable_term_variables(
        scheme.result,
        signature_variables,
        signature_variable_set);

      for (const auto& raw_constraint : component_constraints) {
        CallableTypeConstraint constraint =
          apply_callable_constraint(unifier, raw_constraint);
        bool touches_signature = false;
        std::vector<std::uint32_t> constraint_variables;
        std::unordered_set<std::uint32_t> seen_constraint_variables;
        collect_callable_term_variables(
          constraint.subject,
          constraint_variables,
          seen_constraint_variables);
        if (constraint.kind == CallableConstraintKind::Indexable) {
          collect_callable_term_variables(
            constraint.argument,
            constraint_variables,
            seen_constraint_variables);
          collect_callable_term_variables(
            constraint.result,
            constraint_variables,
            seen_constraint_variables);
        }
        else if (constraint.kind == CallableConstraintKind::Iterable) {
          collect_callable_term_variables(
            constraint.result,
            constraint_variables,
            seen_constraint_variables);
        }
        for (std::uint32_t variable : constraint_variables) {
          if (signature_variable_set.count(variable) != 0) {
            touches_signature = true;
          }
          else if (touches_signature) {
            throw StyioTypeError(
              "cannot infer principal relation for final callable `" + name
              + "`: constraint `" + callable_constraint_text(constraint)
              + "` contains a hidden underconstrained variable"
            );
          }
        }
        if (!touches_signature) {
          continue;
        }
        for (std::uint32_t variable : constraint_variables) {
          if (signature_variable_set.count(variable) == 0) {
            throw StyioTypeError(
              "cannot infer principal relation for final callable `" + name
              + "`: constraint `" + callable_constraint_text(constraint)
              + "` contains a hidden underconstrained variable"
            );
          }
        }
        scheme.constraints.push_back(std::move(constraint));
      }

      std::unordered_map<std::uint32_t, std::uint32_t> normalization;
      for (auto& param : scheme.params) {
        param = normalize_callable_term_variables(param, normalization);
      }
      scheme.result =
        normalize_callable_term_variables(scheme.result, normalization);
      for (auto& constraint : scheme.constraints) {
        constraint =
          normalize_callable_constraint_variables(
            constraint,
            normalization);
      }
      std::sort(
        scheme.constraints.begin(),
        scheme.constraints.end(),
        [](const auto& lhs, const auto& rhs)
        {
          return lhs.canonical < rhs.canonical;
        });
      scheme.constraints.erase(
        std::unique(
          scheme.constraints.begin(),
          scheme.constraints.end(),
          [](const auto& lhs, const auto& rhs)
          {
            return lhs.canonical == rhs.canonical;
          }),
        scheme.constraints.end());

      std::unordered_set<std::uint32_t> seen_variables;
      for (const auto& param : scheme.params) {
        collect_callable_term_variables(
          param,
          scheme.quantified_variables,
          seen_variables);
      }
      collect_callable_term_variables(
        scheme.result,
        scheme.quantified_variables,
        seen_variables);
      std::sort(
        scheme.quantified_variables.begin(),
        scheme.quantified_variables.end());

      std::ostringstream relation;
      if (!scheme.quantified_variables.empty()) {
        relation << "forall ";
        for (std::size_t i = 0;
             i < scheme.quantified_variables.size();
             ++i) {
          if (i != 0) {
            relation << ",";
          }
          relation << "'" << scheme.quantified_variables[i];
        }
        relation << ". ";
      }
      relation << "(";
      for (std::size_t i = 0; i < scheme.params.size(); ++i) {
        if (i != 0) {
          relation << ",";
        }
        relation << callable_term_text(scheme.params[i]);
      }
      relation << ")->" << callable_term_text(scheme.result);
      if (!scheme.constraints.empty()) {
        relation << " where ";
        for (std::size_t i = 0; i < scheme.constraints.size(); ++i) {
          if (i != 0) {
            relation << ",";
          }
          relation << scheme.constraints[i].canonical;
        }
      }
      scheme.canonical_relation = relation.str();
      callable_type_schemes_[name] = std::move(scheme);
    }

    inferring[component_index] = false;
    inferred[component_index] = true;
  };

  for (std::size_t i = 0; i < components.size(); ++i) {
    infer_component(i);
  }

  auto dependency_fingerprints =
    build_callable_dependency_fingerprints(
      this,
      available_definitions);
  callable_checked_body_digests_ =
    std::move(dependency_fingerprints.body_digests);
  callable_definition_dependency_digests_ =
    std::move(dependency_fingerprints.dependency_digests);

  validate_generalized_callable_value_positions(
    ast,
    callable_type_schemes_);

  for (auto* statement : ast->getStmts()) {
    StyioAST* expression = statement;
    if (dynamic_cast<FunctionAST*>(statement) != nullptr
        || dynamic_cast<SimpleFuncAST*>(statement) != nullptr) {
      expression = callable_body_of_def(statement);
    }
    walk_callable_expression(
      expression,
      [&](StyioAST* node)
      {
        auto* call = dynamic_cast<FuncCallAST*>(node);
        if (call == nullptr) {
          return;
        }
        const auto* imported =
          find_imported_callable_definition(call->getNameAsStr());
        if (imported == nullptr
            || (imported->exported
                && imported->visible_from_modules.count("") != 0)) {
          return;
        }
        throw StyioTypeError(
          "callable `" + call->getNameAsStr()
          + "` is not exported to this module by imported module `"
          + imported->module_id + "`"
        );
      });
  }
}

const StyioSemaContext::CallableTypeScheme*
StyioSemaContext::find_callable_type_scheme(
  std::string_view name
) const {
  auto it = callable_type_schemes_.find(std::string(name));
  return it == callable_type_schemes_.end() ? nullptr : &it->second;
}

const StyioSemaContext::CallableEffectSummary*
StyioSemaContext::find_callable_effect_summary(
  std::string_view name
) const {
  auto it = callable_effect_summaries_.find(std::string(name));
  return it == callable_effect_summaries_.end() ? nullptr : &it->second;
}

void
StyioSemaContext::enforce_effect_monomorphic_instance(
  std::string_view name,
  const std::vector<StyioDataType>& arg_types
) {
  const CallableEffectSummary* summary =
    find_callable_effect_summary(name);
  if (summary == nullptr
      || !summary->relation_seed
      || summary->proven_pure()) {
    return;
  }

  auto [it, inserted] = effect_monomorphic_instances_.emplace(
    std::string(name),
    arg_types);
  if (inserted) {
    return;
  }
  if (it->second.size() != arg_types.size()) {
    throw StyioTypeError(
      "effectful callable `" + std::string(name)
      + "` changed arity after its monomorphic instance was fixed"
    );
  }
  for (std::size_t i = 0; i < arg_types.size(); ++i) {
    if (sema_types_equal(this, it->second[i], arg_types[i])) {
      continue;
    }
    throw StyioTypeError(
      "callable `" + std::string(name)
      + "` is monomorphic because its effect summary is `"
      + summary->canonical + "`; parameter "
      + std::to_string(i) + " was fixed as `"
      + it->second[i].name + "` and cannot be reused as `"
      + arg_types[i].name + "`"
    );
  }
}

StyioSemaContext::CallableSpecialization
StyioSemaContext::instantiate_callable_type_scheme(
  FuncCallAST* call,
  const std::vector<StyioDataType>& arg_types
) {
  if (call == nullptr) {
    throw StyioTypeError(
      "generic callable instantiation requires a call expression"
    );
  }
  const CallableTypeScheme* scheme =
    find_callable_type_scheme(call->getNameAsStr());
  if (scheme == nullptr) {
    throw StyioTypeError(
      "callable `" + call->getNameAsStr()
      + "` has no inferred relation"
    );
  }
  if (arg_types.size() != scheme->params.size()) {
    throw StyioTypeError(
      "function `" + call->getNameAsStr() + "` expects "
      + std::to_string(scheme->params.size()) + " argument(s), got "
      + std::to_string(arg_types.size())
    );
  }

  std::unordered_map<std::uint32_t, StyioDataType> bindings;
  if (!call->getExpectedType().isUndefined()) {
    match_callable_term_to_concrete(
      scheme->result,
      call->getExpectedType(),
      bindings,
      "expected result of `" + call->getNameAsStr() + "`"
    );
  }
  for (std::size_t i = 0; i < arg_types.size(); ++i) {
    match_callable_term_to_concrete(
      scheme->params[i],
      arg_types[i],
      bindings,
      "argument " + std::to_string(i)
      + " of `" + call->getNameAsStr() + "`"
    );
  }
  solve_callable_constraint_instance(*scheme, bindings);

  CallableSpecialization specialization;
  specialization.source_name = scheme->name;
  if (const auto* imported =
        find_imported_callable_definition(scheme->name)) {
    specialization.owner_module = imported->module_id;
    specialization.checked_body_digest =
      imported->checked_body_digest;
    specialization.interface_abi_digest =
      imported->interface_abi_digest;
  }
  specialization.param_types.reserve(scheme->params.size());
  for (const auto& param : scheme->params) {
    specialization.param_types.push_back(
      resolve_callable_term_to_concrete(
        param,
        bindings,
        scheme->name));
  }
  specialization.result_type =
    resolve_callable_term_to_concrete(
      scheme->result,
      bindings,
      scheme->name);
  specialization.canonical_key =
    callable_concrete_key(
      specialization.owner_module.empty()
        ? scheme->name
        : specialization.owner_module + "::" + scheme->name,
      specialization.param_types,
      specialization.result_type);

  StyioAST* definition = find_function_def(
    call->func_name->getSymbolId(),
    scheme->name);
  if (definition == nullptr) {
    throw StyioTypeError(
      "callable specialization has no checked definition for `"
      + scheme->name + "`"
    );
  }
  if (specialization.checked_body_digest.empty()) {
    auto [body_digest, inserted] =
      callable_checked_body_digests_.emplace(definition, std::string());
    if (inserted) {
      StyioRepr representation;
      body_digest->second =
        styio::sema::callable_interface_sha256_hex(
          definition->toString(&representation));
    }
    specialization.checked_body_digest = body_digest->second;
  }

  const std::string dependency_facts =
    specialization.interface_abi_digest.empty()
      ? callable_specialization_dependency_digest_
      : specialization.interface_abi_digest;
  const auto* effects =
    find_callable_effect_summary(scheme->name);
  auto definition_dependencies =
    callable_definition_dependency_digests_.find(scheme->name);
  if (definition_dependencies
      == callable_definition_dependency_digests_.end()) {
    throw StyioTypeError(
      "callable specialization has no dependency fingerprint for `"
      + scheme->name + "`"
    );
  }
  specialization.content_digest =
    callable_specialization_content_digest(
      specialization.canonical_key,
      scheme->canonical_relation,
      effects == nullptr ? "unknown" : effects->canonical,
      specialization.checked_body_digest,
      definition_dependencies->second,
      dependency_facts,
      callable_specialization_backend_abi_);
  specialization.lowered_name =
    callable_specialized_symbol(
      scheme->name,
      specialization.content_digest);

  callable_specialization_graph_.register_item(
    specialization.content_digest,
    specialization.canonical_key);
  auto cached =
    callable_specialization_cache_.find(
      specialization.content_digest);
  if (cached != callable_specialization_cache_.end()) {
    if (cached->second.canonical_key
          != specialization.canonical_key
        || cached->second.lowered_name
             != specialization.lowered_name) {
      throw StyioTypeError(
        "callable specialization cache identity collision for `"
        + specialization.canonical_key + "`"
      );
    }
    call->setInferredType(cached->second.result_type);
    call->setLoweredCalleeName(cached->second.lowered_name);
    return cached->second;
  }

  auto [inserted, ok] =
    callable_specialization_cache_.emplace(
      specialization.content_digest,
      specialization);
  if (!ok) {
    throw StyioTypeError(
      "internal error: callable specialization cache insertion failed"
    );
  }
  auto& specializations =
    callable_specializations_[scheme->name];
  const auto position =
    std::lower_bound(
      specializations.begin(),
      specializations.end(),
      specialization.content_digest,
      [](const CallableSpecialization& item, const std::string& digest)
      {
        return item.content_digest < digest;
      });
  specializations.insert(position, inserted->second);
  call->setInferredType(specialization.result_type);
  call->setLoweredCalleeName(specialization.lowered_name);
  return specialization;
}

const std::vector<StyioSemaContext::CallableSpecialization>&
StyioSemaContext::callable_specializations(
  std::string_view name
) const {
  static const std::vector<CallableSpecialization> empty;
  auto it = callable_specializations_.find(std::string(name));
  return it == callable_specializations_.end() ? empty : it->second;
}

bool
StyioSemaContext::callable_has_runtime_specializations(
  std::string_view name
) const {
  const CallableTypeScheme* scheme = find_callable_type_scheme(name);
  return scheme != nullptr;
}

bool
StyioSemaContext::imported_concrete_callable_is_reachable(
  std::string_view name
) const {
  return reachable_imported_concrete_callables_.count(
           std::string(name)) != 0;
}

void
StyioSemaContext::prepare_imported_concrete_callable_body(
  std::string_view name
) {
  const ImportedCallableDefinition* imported =
    find_imported_callable_definition(name);
  if (imported == nullptr || imported->has_scheme) {
    return;
  }
  const std::string source_name(name);
  auto definition_dependencies =
    callable_definition_dependency_digests_.find(source_name);
  if (definition_dependencies
      == callable_definition_dependency_digests_.end()) {
    throw StyioTypeError(
      "imported concrete callable has no dependency fingerprint for `"
      + source_name + "`"
    );
  }

  CallableSpecialization concrete;
  concrete.source_name = source_name;
  concrete.owner_module = imported->module_id;
  concrete.param_types = imported->concrete_params;
  concrete.result_type = imported->concrete_result;
  concrete.checked_body_digest = imported->checked_body_digest;
  concrete.interface_abi_digest = imported->interface_abi_digest;
  concrete.canonical_key =
    callable_concrete_key(
      imported->module_id + "::" + source_name,
      concrete.param_types,
      concrete.result_type);
  const std::string relation =
    callable_concrete_key(
      source_name,
      concrete.param_types,
      concrete.result_type);
  const CallableEffectSummary* effects =
    find_callable_effect_summary(source_name);
  concrete.content_digest =
    callable_specialization_content_digest(
      concrete.canonical_key,
      relation,
      effects == nullptr ? "unknown" : effects->canonical,
      concrete.checked_body_digest,
      definition_dependencies->second,
      concrete.interface_abi_digest,
      callable_specialization_backend_abi_);

  callable_specialization_graph_.register_item(
    concrete.content_digest,
    concrete.canonical_key);
  reachable_imported_concrete_callables_.insert(source_name);
  prepare_callable_specialization_body(
    imported->definition,
    concrete);
}

void
StyioSemaContext::prepare_callable_specialization_body(
  StyioAST* def,
  const CallableSpecialization& specialization
) {
  if (def == nullptr) {
    throw StyioTypeError(
      "generic callable specialization requires a definition"
    );
  }
  auto params = params_of_func_def(def);
  if (params.size() != specialization.param_types.size()) {
    throw StyioTypeError(
      "generic specialization arity drift for callable `"
      + specialization.source_name + "`"
    );
  }
  if (!callable_specialization_graph_.begin_expansion(
        specialization.content_digest)) {
    return;
  }

  const auto saved_types = local_binding_types;
  const auto saved_types_by_sid = local_binding_types_by_sid;
  const auto saved_funcs = func_defs;
  const auto saved_funcs_by_sid = func_defs_by_sid;
  const auto saved_fixed = fixed_assignment_names_;
  const auto saved_fixed_by_sid = fixed_assignment_names_by_sid_;
  const auto saved_bind = binding_info_;
  const auto saved_bind_by_sid = binding_info_by_sid_;
  const auto saved_consumed_tasks = consumed_task_names_;
  const auto saved_consumed_tasks_by_sid = consumed_task_names_by_sid_;
  const auto saved_consumed_resources = consumed_resource_names_;
  const auto saved_consumed_resources_by_sid = consumed_resource_names_by_sid_;
  const auto saved_owned_resources = owned_resource_names_;
  const auto saved_owned_resources_by_sid = owned_resource_names_by_sid_;
  const auto saved_snapshot_names = snapshot_var_names_;
  const auto saved_snapshot_names_by_sid = snapshot_var_names_by_sid_;
  const auto saved_function_stack = active_function_body_stack_;
  const auto saved_function_sid_stack = active_function_body_sid_stack_;
  const auto saved_active_functions = active_function_body_inference_;
  const auto saved_active_functions_by_sid =
    active_function_body_inference_by_sid_;
  const auto saved_inferred_returns = inferred_function_return_types_;
  const auto saved_inferred_returns_by_sid =
    inferred_function_return_types_by_sid_;

  auto restore = [&]()
  {
    local_binding_types = saved_types;
    local_binding_types_by_sid = saved_types_by_sid;
    func_defs = saved_funcs;
    func_defs_by_sid = saved_funcs_by_sid;
    fixed_assignment_names_ = saved_fixed;
    fixed_assignment_names_by_sid_ = saved_fixed_by_sid;
    binding_info_ = saved_bind;
    binding_info_by_sid_ = saved_bind_by_sid;
    consumed_task_names_ = saved_consumed_tasks;
    consumed_task_names_by_sid_ = saved_consumed_tasks_by_sid;
    consumed_resource_names_ = saved_consumed_resources;
    consumed_resource_names_by_sid_ = saved_consumed_resources_by_sid;
    owned_resource_names_ = saved_owned_resources;
    owned_resource_names_by_sid_ = saved_owned_resources_by_sid;
    snapshot_var_names_ = saved_snapshot_names;
    snapshot_var_names_by_sid_ = saved_snapshot_names_by_sid;
    active_function_body_stack_ = saved_function_stack;
    active_function_body_sid_stack_ = saved_function_sid_stack;
    active_function_body_inference_ = saved_active_functions;
    active_function_body_inference_by_sid_ =
      saved_active_functions_by_sid;
    inferred_function_return_types_ = saved_inferred_returns;
    inferred_function_return_types_by_sid_ =
      saved_inferred_returns_by_sid;
    callable_specialization_graph_.end_expansion(
      specialization.content_digest);
  };

  try {
    for (std::size_t i = 0; i < params.size(); ++i) {
      record_local_binding_type(
        params[i]->getNameAsStr(),
        params[i]->var_name->getSymbolId(),
        specialization.param_types[i]);
    }

    StyioAST* body = callable_body_of_def(def);
    apply_callable_expected_type_to_tail(
      body,
      specialization.result_type);
    push_active_function_body(
      specialization.source_name,
      styio::session::kInvalidSymbolId);
    body->typeInfer(this);
    StyioDataType inferred_result =
      function_body_tail_type_latest(this, body);
    if (!inferred_result.equals(specialization.result_type)) {
      throw StyioTypeError(
        "generic specialization result mismatch for callable `"
        + specialization.source_name + "`: expected `"
        + specialization.result_type.name + "`, got `"
        + inferred_result.name + "`"
      );
    }
  }
  catch (...) {
    restore();
    throw;
  }
  restore();
}

void
StyioSemaContext::push_active_function_body(const std::string& name) {
  push_active_function_body(name, styio::session::kInvalidSymbolId);
}

void
StyioSemaContext::push_active_function_body(
  const std::string& name,
  styio::session::SymbolId sid
) {
  if (sid == styio::session::kInvalidSymbolId) {
    sid = intern_semantic_symbol(name);
  }
  active_function_body_stack_.push_back(name);
  active_function_body_sid_stack_.push_back(sid);
}

void
StyioSemaContext::pop_active_function_body() {
  if (!active_function_body_stack_.empty()) {
    active_function_body_stack_.pop_back();
  }
  if (!active_function_body_sid_stack_.empty()) {
    active_function_body_sid_stack_.pop_back();
  }
}

void
StyioSemaContext::record_inferred_function_return_type(const StyioDataType& type) {
  if (type.isUndefined() || active_function_body_stack_.empty()) {
    return;
  }
  maybe_intern_type(type);
  const std::string& name = active_function_body_stack_.back();
  auto it = inferred_function_return_types_.find(std::string(name));
  if (it == inferred_function_return_types_.end()) {
    inferred_function_return_types_[name] = type;
  }
  else {
    StyioDataType merged = merge_match_value_type(it->second, type, this);
    if (!merged.isUndefined()) {
      it->second = merged;
    }
  }

  styio::session::SymbolId sid = active_function_body_sid_stack_.empty()
                                   ? styio::session::kInvalidSymbolId
                                   : active_function_body_sid_stack_.back();
  if (sid == styio::session::kInvalidSymbolId) {
    sid = lookup_semantic_symbol(name);
  }
  if (sid == styio::session::kInvalidSymbolId) {
    return;
  }
  auto sid_it = inferred_function_return_types_by_sid_.find(sid);
  if (sid_it == inferred_function_return_types_by_sid_.end()) {
    inferred_function_return_types_by_sid_[sid] = type;
    return;
  }
  StyioDataType sid_merged = merge_match_value_type(sid_it->second, type, this);
  if (!sid_merged.isUndefined()) {
    sid_it->second = sid_merged;
  }
}

StyioDataType
StyioSemaContext::inferred_function_return_type(const std::string& name) const {
  return inferred_function_return_type(styio::session::kInvalidSymbolId, name);
}

StyioDataType
StyioSemaContext::inferred_function_return_type(
  styio::session::SymbolId sid,
  std::string_view name
) const {
  if (sid == styio::session::kInvalidSymbolId) {
    sid = lookup_semantic_symbol(name);
  }
  if (sid != styio::session::kInvalidSymbolId) {
    auto sid_it = inferred_function_return_types_by_sid_.find(sid);
    if (sid_it != inferred_function_return_types_by_sid_.end()) {
      return sid_it->second;
    }
  }
  auto it = inferred_function_return_types_.find(std::string(name));
  if (it == inferred_function_return_types_.end()) {
    return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
  }
  return it->second;
}

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
  if (is_consumed_resource_name(ast->getSymbolId(), ast->getAsStr())) {
    throw StyioTypeError("use-after-destroy: resource `" + ast->getAsStr() + "` was already destroyed");
  }
  if (find_local_binding_type(ast->getSymbolId(), ast->getAsStr()) == nullptr
      && find_callable_type_scheme(ast->getAsStr()) != nullptr) {
    reject_generalized_callable_value_escape(ast->getAsStr());
  }
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
StyioSemaContext::typeInfer(TypeConvertAST* ast) {
  ast->getValue()->typeInfer(this);
  StyioDataType source_type = infer_expr_type(this, ast->getValue());
  if (source_type.isUndefined()) {
    source_type = type_convert_source_fallback_type(ast->getPromoTy());
  }

  switch (ast->getPromoTy()) {
    case NumPromoTy::Bool_To_Int:
      if (source_type.option != StyioDataTypeOption::Bool) {
        throw StyioTypeError("bool-to-int conversion expects a bool value");
      }
      break;
    case NumPromoTy::Int_To_Float:
      if (source_type.option != StyioDataTypeOption::Integer) {
        throw StyioTypeError("int-to-float conversion expects an integer value");
      }
      break;
  }

  ast->setDataType(type_convert_target_type(ast->getPromoTy()));
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
  const auto bound_sid = ast->getVar()->getName()->getSymbolId();
  if (is_fixed_assignment_name(bound_sid, bound_name)) {
    throw StyioSyntaxError(
      "variable `" + bound_name +
      "` was defined with `:=` (fixed assignment); "
      "cannot reassign with `=` (flex bind). Use a different name."
    );
  }

  auto reject_plain_resource_copy = [&](StyioAST* expr)
  {
    if (auto* ref = dynamic_cast<ResourceRefAST*>(expr)) {
      if (ref->isWholeResource()) {
        throw StyioTypeError(
          "resource `" + ref->getNameStr()
          + "` cannot be copied with `=`; use `<<` to clone it"
        );
      }
    }
    auto* src = dynamic_cast<NameAST*>(expr);
    if (src == nullptr) {
      return;
    }
    const BindingInfo* info = find_binding_info(src->getSymbolId(), src->getAsStr());
    if (info != nullptr && info->resource_value) {
      throw StyioTypeError(
        "resource `" + src->getAsStr()
        + "` cannot be copied with `=`; use `<<` to clone it"
      );
    }
  };

  auto expr_value_kind = [&](StyioAST* expr) -> BindingValueKind
  {
    if (expr->getNodeType() == StyioNodeType::List) {
      return BindingValueKind::ListHandle;
    }
    if (expr->getNodeType() == StyioNodeType::Dict) {
      return BindingValueKind::DictHandle;
    }
    if (auto* nm = dynamic_cast<NameAST*>(expr)) {
      const BindingInfo* bit = find_binding_info(nm->getSymbolId(), nm->getAsStr());
      if (bit != nullptr) {
        return bit->value_kind;
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
    apply_stdin_resource_effect_expected_type(ast->getValue(), var_type);
    apply_callable_expected_type_to_tail(ast->getValue(), var_type);
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
        ast->getVar()->setDataType(kI64Type);
      } break;

      case StyioNodeType::Float: {
        ast->getVar()->setDataType(kF64Type);
      } break;

      case StyioNodeType::BinOp: {
        ast->getVar()->setDataType(static_cast<BinOpAST*>(ast->getValue())->getType());
      } break;

      case StyioNodeType::Bool:
      case StyioNodeType::Condition:
      case StyioNodeType::Compare: {
        ast->getVar()->setDataType(StyioDataType{StyioDataTypeOption::Bool, "bool", 1});
      } break;

      case StyioNodeType::Char: {
        ast->getVar()->setDataType(static_cast<CharAST*>(ast->getValue())->getDataType());
      } break;

      case StyioNodeType::String: {
        ast->getVar()->setDataType(kStringType);
      } break;

      case StyioNodeType::Tuple: {
        ast->getVar()->setDataType(ast->getValue()->getDataType());
      } break;

      case StyioNodeType::ResourceEffect: {
        ast->getVar()->setDataType(ast->getValue()->getDataType());
      } break;

      case StyioNodeType::MatchCases: {
        ast->getVar()->setDataType(ast->getValue()->getDataType());
      } break;

      default:
        break;
    }
  }

  reject_plain_resource_copy(ast->getValue());
  StyioDataType inferred_rhs_type = infer_expr_type(this, ast->getValue());
  const bool direct_resource_construct =
    dynamic_cast<FileResourceAST*>(ast->getValue()) != nullptr
    || dynamic_cast<StdStreamAST*>(ast->getValue()) != nullptr
    || dynamic_cast<ResourceReceiverAST*>(ast->getValue()) != nullptr;
  if (inferred_rhs_type.handle_family == StyioHandleFamily::File
      || inferred_rhs_type.handle_family == StyioHandleFamily::Stream) {
    if (!direct_resource_construct) {
      throw StyioTypeError(
        "resource handles must be bound with `<-`; use `<- @...` for files and standard streams"
      );
    }
  }

  if (dynamic_cast<EmptyResourceAST*>(ast->getValue()) != nullptr) {
    throw StyioTypeError(
      "@() is a destroy sink and cannot be bound as a resource value"
    );
  }

  StyioDataType concrete_type = inferred_rhs_type;
  if (var_type.option != StyioDataTypeOption::Undefined) {
    concrete_type = var_type;
  }
  BindingValueKind kind = expr_value_kind(ast->getValue());
  if (ast->getValue()->getNodeType() == StyioNodeType::TaskBlock) {
    kind = BindingValueKind::TaskHandle;
  }
  if (styio_is_matrix_type(concrete_type)) {
    kind = BindingValueKind::MatrixHandle;
  }

  record_local_binding_type(
    ast->getNameAsStr(),
    ast->getVar()->getName()->getSymbolId(),
    concrete_type);
  maybe_intern_type(concrete_type);

  BindingInfo info;
  if (const BindingInfo* prev = find_binding_info(bound_sid, bound_name)) {
    info = *prev;
  }
  info.final_slot = false;
  info.dynamic_slot = info.dynamic_slot
                      || kind == BindingValueKind::ListHandle
                      || kind == BindingValueKind::DictHandle
                      || kind == BindingValueKind::MatrixHandle
                      || kind == BindingValueKind::TaskHandle;
  const bool external_resource_value =
    concrete_type.handle_family == StyioHandleFamily::File
    || concrete_type.handle_family == StyioHandleFamily::Stream
    || styio_is_topology_resource_type(concrete_type);
  info.resource_value = kind == BindingValueKind::ListHandle
                        || kind == BindingValueKind::DictHandle
                        || kind == BindingValueKind::MatrixHandle
                        || kind == BindingValueKind::TaskHandle
                        || external_resource_value;
  info.value_kind = kind;
  info.declared_type = (info.dynamic_slot && !external_resource_value)
                         ? StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0}
                         : concrete_type;
  record_binding_info(bound_name, bound_sid, info);
  if (info.resource_value) {
    record_owned_resource_name(bound_name, bound_sid);
  }
  if (external_resource_value && direct_resource_construct) {
    erase_consumed_resource_name(bound_name, bound_sid);
  }
}

void
StyioSemaContext::typeInfer(FinalBindAST* ast) {
  if (auto* rhs_resource = dynamic_cast<ResourceRefAST*>(ast->getValue())) {
    if (rhs_resource->isWholeResource()) {
      throw StyioTypeError(
        "resource `" + rhs_resource->getNameStr()
        + "` cannot be copied with `:=`; use `<<` to clone it"
      );
    }
  }
  auto* rhs_name = dynamic_cast<NameAST*>(ast->getValue());
  if (rhs_name != nullptr) {
    const BindingInfo* it = find_binding_info(rhs_name->getSymbolId(), rhs_name->getAsStr());
    if (it != nullptr && it->resource_value) {
      throw StyioTypeError(
        "resource `" + rhs_name->getAsStr()
        + "` cannot be copied with `:=`; use `<<` to clone it"
      );
    }
  }
  auto vt = ast->getVar()->getDType()->type;
  if (vt.option != StyioDataTypeOption::Undefined) {
    apply_stdin_resource_effect_expected_type(ast->getValue(), vt);
    apply_callable_expected_type_to_tail(ast->getValue(), vt);
  }
  ast->getValue()->typeInfer(this);
  if (vt.option == StyioDataTypeOption::Undefined) {
    vt = infer_expr_type(this, ast->getValue());
    ast->getVar()->setDataType(vt);
  }
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
  if (ast->getValue()->getNodeType() == StyioNodeType::MatchCases
      && vt.option == StyioDataTypeOption::Undefined) {
    vt = ast->getValue()->getDataType();
    ast->getVar()->setDataType(vt);
  }
  record_local_binding_type(
    ast->getVar()->getNameAsStr(),
    ast->getVar()->getName()->getSymbolId(),
    vt);
  maybe_intern_type(vt);
  record_fixed_assignment_name(
    ast->getVar()->getNameAsStr(),
    ast->getVar()->getName()->getSymbolId());

  const BindingInfo* rhs_info = rhs_name == nullptr
                                  ? nullptr
                                  : find_binding_info(rhs_name->getSymbolId(), rhs_name->getAsStr());
  BindingValueKind rhs_kind = binding_value_kind_for_type(infer_expr_type(this, ast->getValue()));
  if (ast->getValue()->getNodeType() == StyioNodeType::TaskBlock) {
    rhs_kind = BindingValueKind::TaskHandle;
  }
  BindingInfo info;
  info.final_slot = true;
  const bool runtime_resource =
    ast->getValue()->getNodeType() == StyioNodeType::Dict
    || dynamic_cast<FileResourceAST*>(ast->getValue()) != nullptr
    || dynamic_cast<StdStreamAST*>(ast->getValue()) != nullptr
    || dynamic_cast<ResourceReceiverAST*>(ast->getValue()) != nullptr
    || rhs_kind == BindingValueKind::ListHandle
    || rhs_kind == BindingValueKind::DictHandle
    || rhs_kind == BindingValueKind::MatrixHandle
    || rhs_kind == BindingValueKind::TaskHandle
    || styio_type_is_resource_handle(vt)
    || styio_is_topology_resource_type(vt)
    || (rhs_info != nullptr && (rhs_info->value_kind == BindingValueKind::ListHandle || rhs_info->value_kind == BindingValueKind::DictHandle || rhs_info->value_kind == BindingValueKind::MatrixHandle || rhs_info->value_kind == BindingValueKind::TaskHandle));
  info.dynamic_slot = runtime_resource;
  info.resource_value = runtime_resource;
  if (ast->getValue()->getNodeType() == StyioNodeType::Dict) {
    info.value_kind = BindingValueKind::DictHandle;
  }
  else if (runtime_resource) {
    info.value_kind =
      rhs_info != nullptr ? rhs_info->value_kind : rhs_kind;
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
  record_binding_info(
    ast->getVar()->getNameAsStr(),
    ast->getVar()->getName()->getSymbolId(),
    info);
  if (info.resource_value) {
    record_owned_resource_name(ast->getVar()->getNameAsStr(), ast->getVar()->getName()->getSymbolId());
  }
}

void
StyioSemaContext::typeInfer(ParallelAssignAST* ast) {
  if (ast->getLHS().size() != ast->getRHS().size()) {
    throw StyioTypeError("parallel assignment requires the same number of LHS and RHS expressions");
  }

  for (auto* rhs : ast->getRHS()) {
    if (auto* rhs_name = dynamic_cast<NameAST*>(rhs)) {
      const BindingInfo* it = find_binding_info(rhs_name->getSymbolId(), rhs_name->getAsStr());
      if (it != nullptr && it->resource_value) {
        throw StyioTypeError(
          "resource `" + rhs_name->getAsStr()
          + "` cannot be copied with `=`; use `<<` to clone it"
        );
      }
    }
    rhs->typeInfer(this);
  }

  for (size_t i = 0; i < ast->getLHS().size(); ++i) {
    StyioAST* lhs = ast->getLHS()[i];
    if (auto* nm = dynamic_cast<NameAST*>(lhs)) {
      BindingInfo* it = find_mutable_binding_info(nm->getSymbolId(), nm->getAsStr());
      if ((it != nullptr && it->final_slot)
          || is_fixed_assignment_name(nm->getSymbolId(), nm->getAsStr())) {
        throw StyioTypeError("parallel assignment cannot rebind final slot `" + nm->getAsStr() + "`");
      }
      if (it != nullptr && it->dynamic_slot) {
        if (auto* rhs_name = dynamic_cast<NameAST*>(ast->getRHS()[i])) {
          const BindingInfo* rit = find_binding_info(rhs_name->getSymbolId(), rhs_name->getAsStr());
          if (rit != nullptr) {
            BindingInfo updated = *it;
            updated.value_kind = rit->value_kind;
            updated.resource_value = rit->resource_value;
            record_local_binding_type(nm->getAsStr(), nm->getSymbolId(), rit->declared_type);
            record_binding_info(nm->getAsStr(), nm->getSymbolId(), updated);
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
      if (!container_value_assignable(target_type, rhs_type, this)) {
        throw StyioTypeError(
          "indexed assignment RHS does not match dict value type `"
          + target_type.name + "`"
        );
      }
      continue;
    }
    if (!styio_is_list_type(base_type)) {
      throw StyioTypeError(
        "indexed assignment in this slice supports dict[string,T] or list[T] targets only"
      );
    }
    StyioDataType elem_type = styio_data_type_from_name(styio_type_item_type_name(base_type));
    if (!styio_type_supports_runtime_list_elem(elem_type)) {
      throw StyioTypeError(
        "indexed assignment in this slice supports runtime list element families only"
      );
    }
    if (!container_value_assignable(elem_type, rhs_type, this)) {
      throw StyioTypeError(
        "indexed assignment RHS does not match list element type `"
        + elem_type.name + "`"
      );
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

  if (elements.empty()) {
    return;
  }

  for (auto* element : elements) {
    element->typeInfer(this);
  }

  StyioDataType aggregated_type = infer_expr_type(this, elements[0]);
  bool is_consistent = !aggregated_type.isUndefined();
  if (is_consistent) {
    for (size_t i = 1; i < elements.size(); i += 1) {
      if (!sema_types_equal(
            this,
            infer_expr_type(this, elements[i]),
            aggregated_type)) {
        is_consistent = false;
        break;
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
  ast->getStart()->typeInfer(this);
  ast->getEnd()->typeInfer(this);
  ast->getStep()->typeInfer(this);

  StyioDataType start_type = infer_expr_type(this, ast->getStart());
  StyioDataType end_type = infer_expr_type(this, ast->getEnd());
  StyioDataType step_type = infer_expr_type(this, ast->getStep());
  if (start_type.option != StyioDataTypeOption::Integer
      || end_type.option != StyioDataTypeOption::Integer
      || step_type.option != StyioDataTypeOption::Integer) {
    throw StyioTypeError("range literal bounds must be integer expressions");
  }
}

void
StyioSemaContext::typeInfer(SetAST* ast) {
}

void
StyioSemaContext::typeInfer(ListAST* ast) {
  for (auto* elem : ast->getElements()) {
    elem->typeInfer(this);
  }
  StyioDataType inferred = infer_list_literal_type(this, ast);
  if (ast->getElements().empty() && inferred.isUndefined()) {
    throw StyioTypeError(
      "empty list literal is underconstrained; add a concrete surrounding "
      "list annotation"
    );
  }
  ast->setConsistency(true);
  ast->setDataType(inferred);
  maybe_intern_type(ast->getDataType());
}

void
StyioSemaContext::typeInfer(DictAST* ast) {
  auto const& entries = ast->getEntries();
  for (auto const& entry : entries) {
    entry.key->typeInfer(this);
    entry.value->typeInfer(this);
  }
  StyioDataType dict_type = infer_dict_literal_type(this, ast);
  if (entries.empty() && dict_type.isUndefined()) {
    throw StyioTypeError(
      "empty dict literal is underconstrained; add a concrete surrounding "
      "dict annotation"
    );
  }
  ast->setConsistency(true);
  ast->setDataType(dict_type);
  maybe_intern_type(ast->getDataType());
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
  auto* selected_name = dynamic_cast<NameAST*>(ast->getList());
  const bool generalized_scheme_selector =
    selected_name != nullptr
    && find_local_binding_type(
         selected_name->getSymbolId(),
         selected_name->getAsStr()) == nullptr
    && find_callable_type_scheme(selected_name->getAsStr()) != nullptr;
  if (!generalized_scheme_selector) {
    ast->getList()->typeInfer(this);
  }
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
  if (ast->getOp() == StyioNodeType::Access_By_Slice) {
    if (!styio_is_list_type(list_type)
        && !styio_is_matrix_type(list_type)
        && !styio_is_dict_type(list_type)) {
      throw StyioTypeError("slice access requires a list, matrix, or dict value");
    }
    StyioDataType start_type = infer_expr_type(this, ast->getSlot1());
    if (start_type.option != StyioDataTypeOption::Integer) {
      throw StyioTypeError("slice start must have integer type");
    }
    if (ast->getSlot2() != nullptr) {
      StyioDataType end_type = infer_expr_type(this, ast->getSlot2());
      if (end_type.option != StyioDataTypeOption::Integer) {
        throw StyioTypeError("slice end must have integer type");
      }
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
  const auto target_sid = ast->getVar() != nullptr && ast->getVar()->getName() != nullptr
                            ? ast->getVar()->getName()->getSymbolId()
                            : styio::session::kInvalidSymbolId;
  const StyioDataType* existing_target_type = find_local_binding_type(target_sid, name);
  const BindingInfo* existing_target_info = find_binding_info(target_sid, name);
  if (auto* task_name = dynamic_cast<NameAST*>(ast->getResource())) {
    StyioDataType source_type = infer_expr_type(this, task_name);
    if (source_type.handle_family == StyioHandleFamily::Task) {
      if (existing_target_type == nullptr && existing_target_info == nullptr) {
        throw StyioTypeError("task pull target `" + name + "` must be declared before use");
      }
      if (is_fixed_assignment_name(target_sid, name)) {
        throw StyioTypeError("task pull target `" + name + "` is final and cannot be reassigned");
      }
      if (is_consumed_task_name(task_name->getSymbolId(), task_name->getAsStr())) {
        throw StyioTypeError("task `" + task_name->getAsStr() + "` was already pulled");
      }
      StyioDataType result_type = task_result_type_from_task_type(source_type);
      StyioDataType target_type = existing_target_type != nullptr
                                    ? *existing_target_type
                                    : existing_target_info->declared_type;
      if (!target_type.isUndefined() && !container_value_assignable(target_type, result_type, this)) {
        throw StyioTypeError(
          "task pull target `" + name + "` expects " + target_type.name
          + ", got " + result_type.name
        );
      }
      record_consumed_task_name(task_name->getAsStr(), task_name->getSymbolId());
      return;
    }
  }
  if (!ast->isFlexBind() && existing_target_type != nullptr) {
    throw StyioTypeError("final resource bind cannot redefine `" + name + "`");
  }
  if (ast->isFlexBind() && is_fixed_assignment_name(target_sid, name)) {
    throw StyioTypeError("resource clone cannot rebind final slot `" + name + "`");
  }

  ast->getResource()->typeInfer(this);
  if (auto* ref = dynamic_cast<ResourceRefAST*>(ast->getResource());
      ast->isFlexBind() && ref != nullptr && ref->isWholeResource()) {
    throw StyioTypeError(
      "resource clone source `" + ref->getNameStr()
      + "` is a topology resource and cannot be cloned as a whole with `<<`; "
      "copy snapshots with `<< <resource>[...]` or `<< <resource>[-n..]` instead"
    );
  }

  BindingInfo info;
  info.final_slot = !ast->isFlexBind();
  info.dynamic_slot = ast->isFlexBind();
  info.declared_type = infer_expr_type(this, ast->getResource());
  info.value_kind = BindingValueKind::I64;

  if (auto* src = dynamic_cast<NameAST*>(ast->getResource())) {
    const BindingInfo* it = find_binding_info(src->getSymbolId(), src->getAsStr());
    StyioDataType source_type =
      (it != nullptr) ? it->declared_type
                      : StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
    if (source_type.isUndefined()) {
      const StyioDataType* source_local_type =
        find_local_binding_type(src->getSymbolId(), src->getAsStr());
      if (source_local_type != nullptr) {
        source_type = *source_local_type;
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
      record_local_binding_type(name, target_sid, collected_type);
      collect_bind_handle_acquires_.insert(ast);
      collect_bind_handle_acquire_types_[ast] = collected_type;
    }
    else {
      const bool source_is_file_handle =
        source_type.handle_family == StyioHandleFamily::File
        || source_type.handle_family == StyioHandleFamily::Stream;
      if (source_is_file_handle) {
        throw StyioTypeError(
          "resource clone source `" + src->getAsStr()
          + "` is a file/stream handle and cannot be cloned with `<<`; "
          "use `<-` to acquire or operate the handle directly"
        );
      }
      if (styio_is_topology_resource_type(source_type)) {
        throw StyioTypeError(
          "resource clone source `" + src->getAsStr()
          + "` is a topology resource and cannot be cloned as a whole with `<<`; "
          "copy snapshots with `<< <resource>[...]` or `<< <resource>[-n..]` instead"
        );
      }
      if (it == nullptr || !it->resource_value
          || !styio_type_is_cloneable(source_type)) {
        throw StyioTypeError(
          "resource clone source `" + src->getAsStr() + "` is not a cloneable resource"
        );
      }
      if (!ast->isFlexBind()) {
        throw StyioTypeError(
          "resource clone source `" + src->getAsStr()
          + "` must use `<<`; `<-` only acquires external resources or pulls task handles"
        );
      }
      info.value_kind = it->value_kind;
      info.resource_value = it->resource_value;
      info.declared_type = source_type;
      record_local_binding_type(name, target_sid, source_type);
      if (!ast->isFlexBind()) {
        ast->getVar()->setDataType(source_type);
      }
    }
  }
  else {
    if (auto* resource_ref = dynamic_cast<ResourceRefAST*>(ast->getResource())) {
      const auto* resource_ref_type_ptr = find_resource_binding_type(
        resource_ref->getName()->getSymbolId(),
        resource_ref->getNameStr());
      if (resource_ref_type_ptr != nullptr) {
        const auto& ref_type = *resource_ref_type_ptr;
        if (ast->isFlexBind() && styio_is_topology_resource_type(ref_type)) {
          throw StyioTypeError(
            "resource clone source `" + resource_ref->getNameStr()
            + "` is a topology resource and cannot be cloned as a whole with `<<`; "
            + "copy snapshots with `<< <resource>[...]` or `<< <resource>[-n..]` instead"
          );
        }
        if (ast->isFlexBind()
            && (ref_type.handle_family == StyioHandleFamily::File
                || ref_type.handle_family == StyioHandleFamily::Stream)) {
          throw StyioTypeError(
            "resource clone source `" + resource_ref->getNameStr()
            + "` is a file/stream handle and cannot be cloned with `<<`; "
            "use `<-` to acquire or operate the handle directly"
          );
        }
      }
    }
    if (ast->isFlexBind() && styio_is_topology_resource_type(info.declared_type)) {
      throw StyioTypeError(
        "resource clone source is a topology resource and cannot be cloned as a whole with `<<`; "
        "copy snapshots with `<< <resource>[...]` or `<< <resource>[-n..]` instead"
      );
    }
    if (ast->isFlexBind()
        && (info.declared_type.handle_family == StyioHandleFamily::File
            || info.declared_type.handle_family == StyioHandleFamily::Stream)) {
      throw StyioTypeError(
        "resource clone source is a file/stream handle and cannot be cloned with `<<`; "
        "use `<-` to acquire or operate the handle directly"
      );
    }
    if (info.declared_type.isUndefined()) {
      throw StyioTypeError("handle acquire needs a typed resource source");
    }
    info.resource_value = styio_type_is_resource_handle(info.declared_type);
    record_local_binding_type(name, target_sid, info.declared_type);
    if (!ast->isFlexBind()) {
      ast->getVar()->setDataType(info.declared_type);
    }
  }

  if (!ast->isFlexBind()) {
    record_fixed_assignment_name(name, target_sid);
  }
  record_binding_info(name, target_sid, info);
  if (info.resource_value) {
    record_owned_resource_name(name, target_sid);
  }
  if (ast->isFlexBind()) {
    erase_consumed_resource_name(name, target_sid);
  }
}

void
StyioSemaContext::typeInfer(ResourceWriteAST* ast) {
  auto report_unsupported_whole_resource_copy = [](const ResourceRefAST* target_resource) {
    if (target_resource == nullptr || !target_resource->isWholeResource()) {
      return false;
    }
    return true;
  };

  ast->getResource()->typeInfer(this);
  if (dynamic_cast<EmptyResourceAST*>(ast->getResource()) != nullptr) {
    throw StyioTypeError("@() is a destroy sink; use `resource -> @()` to destroy");
  }
  auto* target_resource = dynamic_cast<ResourceRefAST*>(ast->getResource());
  if (report_unsupported_whole_resource_copy(target_resource)) {
    StyioDataType target_type = infer_expr_type(this, ast->getResource());
    if (styio_is_topology_resource_type(target_type)) {
      throw StyioTypeError(
        "resource write source `" + target_resource->getNameStr()
        + "` is a topology resource and cannot be copied as a whole with `<<`; "
        "copy snapshots with `<< <resource>[...]` or `<< <resource>[-n..]` instead"
      );
    }
    if (target_type.handle_family == StyioHandleFamily::File
        || target_type.handle_family == StyioHandleFamily::Stream) {
      throw StyioTypeError(
        "resource write source `" + target_resource->getNameStr()
        + "` is a file/stream handle and cannot be copied with `<<`; "
        "use `<-` to acquire or operate the handle directly"
      );
    }
  }
  auto* target_name = dynamic_cast<NameAST*>(ast->getData());
  StyioDataType resource_type = infer_expr_type(this, ast->getResource());
  if (target_name != nullptr
      && find_local_binding_type(target_name->getSymbolId(), target_name->getAsStr()) == nullptr
      && find_binding_info(target_name->getSymbolId(), target_name->getAsStr()) == nullptr
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
    record_local_binding_type(target_name->getAsStr(), target_name->getSymbolId(), collected_type);
    record_binding_info(target_name->getAsStr(), target_name->getSymbolId(), info);
    collect_bind_resource_writes_.insert(ast);
    collect_bind_resource_write_types_[ast] = collected_type;
    return;
  }
  ast->getData()->typeInfer(this);
  if (!styio_type_is_writable(resource_type)) {
    throw StyioTypeError("write target must be a writable resource");
  }
  bool text_sink_requires_iterable = false;
  if (auto* stream = dynamic_cast<StdStreamAST*>(ast->getResource())) {
    text_sink_requires_iterable = stream->getStreamKind() != StdStreamKind::Stdin;
  }
  else if (dynamic_cast<FileResourceAST*>(ast->getResource()) != nullptr
           || resource_type.handle_family == StyioHandleFamily::File) {
    text_sink_requires_iterable = true;
  }
  if (text_sink_requires_iterable) {
    StyioDataType data_type = infer_expr_type(this, ast->getData());
    if (!type_is_text_serializable_iterable(data_type)) {
      throw StyioTypeError(
        "terminal/file/standard-stream `>>` requires an iterable text-serializable value; "
        "use `->` for scalar text"
      );
    }
  }
}

void
StyioSemaContext::typeInfer(ResourceRedirectAST* ast) {
  ast->getData()->typeInfer(this);
  ast->getResource()->typeInfer(this);
  if (dynamic_cast<EmptyResourceAST*>(ast->getResource()) != nullptr) {
    if (auto* name = dynamic_cast<NameAST*>(ast->getData())) {
      const std::string resource_name = name->getAsStr();
      const BindingInfo* it = find_binding_info(name->getSymbolId(), resource_name);
      if (it == nullptr || !it->resource_value) {
        throw StyioTypeError("@() destroy source must be a resource");
      }
      if (is_task_outer_resource_name(name->getSymbolId(), resource_name)) {
        throw StyioTypeError("task cannot consume outer resource `" + resource_name + "`");
      }
      if (is_consumed_resource_name(name->getSymbolId(), resource_name)) {
        throw StyioTypeError("double destroy: resource `" + resource_name + "` was already destroyed");
      }
      record_consumed_resource_name(resource_name, name->getSymbolId());
      return;
    }
    StyioDataType data_type = infer_expr_type(this, ast->getData());
    if (dynamic_cast<FileResourceAST*>(ast->getData()) == nullptr
        && dynamic_cast<ResourceReceiverAST*>(ast->getData()) == nullptr
        && dynamic_cast<ResourceRefAST*>(ast->getData()) == nullptr
        && !styio_type_is_resource_handle(data_type)
        && !styio_is_topology_resource_type(data_type)) {
      throw StyioTypeError("@() destroy source must be a resource");
    }
    return;
  }
  StyioDataType resource_type = infer_expr_type(this, ast->getResource());
  if (!styio_type_is_writable(resource_type)) {
    throw StyioTypeError("redirect target must be a writable resource");
  }
}

void
StyioSemaContext::typeInfer(ResourceEffectAST* ast) {
  ast->getOperation()->typeInfer(this);
  if (!resource_effect_operation_supported_latest(this, ast->getOperation())) {
    throw StyioTypeError("`?|` resource settlement requires a resource operation");
  }
  StyioDataType operation_type = infer_expr_type(this, ast->getOperation());

  if (ast->hasHandlers()) {
    std::unordered_set<std::string> seen_handlers;
    for (const auto& handler : ast->getHandlers()) {
      if (!resource_effect_handler_name_supported_latest(handler.effect_name)) {
        throw StyioTypeError(
          "unknown resource-effect handler `" + handler.effect_name + "`"
        );
      }
      if (!seen_handlers.insert(handler.effect_name).second) {
        throw StyioTypeError(
          "duplicate resource-effect handler `" + handler.effect_name + "`"
        );
      }
      if (dynamic_cast<EmptyResourceAST*>(handler.body) != nullptr) {
        throw StyioTypeError("resource-effect handler must be executable code, not @()");
      }
      apply_matrix_literal_context(this, handler.body, operation_type);
      handler.body->typeInfer(this);
      StyioDataType handler_type = infer_expr_type(this, handler.body);
      if (!operation_type.isUndefined()
          && !handler_type.isUndefined()
          && !container_value_assignable(operation_type, handler_type, this)) {
        throw StyioTypeError(
          "resource-effect handler `" + handler.effect_name + "` expects "
          + operation_type.name + ", got " + handler_type.name
        );
      }
    }
  }

  if (ast->isDiscard()) {
    ast->setResultType(StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0});
    return;
  }

  if (ast->isValueRequired() && operation_type.isUndefined()) {
    throw StyioTypeError(
      "resource-effect expression requires a value-producing resource operation"
    );
  }

  if (ast->hasFallback()) {
    if (dynamic_cast<EmptyResourceAST*>(ast->getFallback()) != nullptr) {
      throw StyioTypeError("resource-effect fallback must be executable code, not @()");
    }
    apply_matrix_literal_context(this, ast->getFallback(), operation_type);
    ast->getFallback()->typeInfer(this);
    StyioDataType fallback_type = infer_expr_type(this, ast->getFallback());
    if (!operation_type.isUndefined()
        && !fallback_type.isUndefined()
        && !container_value_assignable(operation_type, fallback_type, this)) {
      throw StyioTypeError(
        "resource-effect fallback expects " + operation_type.name
        + ", got " + fallback_type.name
      );
    }
  }

  ast->setResultType(operation_type);
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
      const StyioDataType* local_type =
        find_local_binding_type(nm->getSymbolId(), nm->getAsStr());
      if (local_type != nullptr) {
        ast->setDType(*local_type);
      }
      else {
        ast->setDType(StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
      }
    }
    else {
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
    if ((op == StyioOpType::Binary_Add
         || op == StyioOpType::Binary_Sub
         || op == StyioOpType::Binary_Mul
         || op == StyioOpType::Binary_Div
         || op == StyioOpType::Binary_Mod
         || op == StyioOpType::Binary_Pow)
        && type_is_numeric_family(lhs_type)
        && type_is_numeric_family(rhs_type)) {
      ast->setDType(getMaxType(lhs_type, rhs_type));
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
        const StyioDataType* lhs_local_type =
          find_local_binding_type(lid->getSymbolId(), lid->getAsStr());
        if (lhs_local_type == nullptr) {
          break;
        }
        StyioDataType lt = *lhs_local_type;

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
            const StyioDataType* rhs_local_type =
              find_local_binding_type(rid->getSymbolId(), rid->getAsStr());
            if (rhs_local_type == nullptr) {
              break;
            }
            if (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul
                || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow) {
              ast->setDType(getMaxType(lt, *rhs_local_type));
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
        && (op == StyioOpType::Binary_Add || op == StyioOpType::Binary_Sub || op == StyioOpType::Binary_Mul || op == StyioOpType::Binary_Div || op == StyioOpType::Binary_Mod || op == StyioOpType::Binary_Pow)) {
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
  for (auto* expr : ast->getExprs()) {
    expr->typeInfer(this);
  }
}

void
StyioSemaContext::typeInfer(ResourceAST* ast) {
}

void
StyioSemaContext::typeInfer(EmptyResourceAST* ast) {
  (void)ast;
}

void
StyioSemaContext::typeInfer(ResourceReceiverAST* ast) {
  if (!active_resource_receiver_family_.empty()
      && ast->getFamilyName() != active_resource_receiver_family_) {
    throw StyioTypeError(
      "resource receiver @" + ast->getFamilyName()
      + " is not the active receiver @" + active_resource_receiver_family_
    );
  }
}

void
StyioSemaContext::typeInfer(ResourceMethodDefAST* ast) {
  auto& methods = resource_method_defs_[ast->getFamilyName()];
  auto existing = methods.find(ast->getMethodName());
  if (existing != methods.end() && existing->second.final_binding) {
    throw StyioTypeError(
      "resource method @" + ast->getFamilyName() + "::" + ast->getMethodName()
      + " is final and cannot be overridden"
    );
  }

  ResourceMethodInfo info;
  info.final_binding = ast->isFinalBinding();
  info.property = ast->isProperty();
  info.param_count = ast->getParams().size();
  info.param_names.reserve(ast->getParams().size());
  info.param_types.reserve(ast->getParams().size());
  for (auto* param : ast->getParams()) {
    if (param == nullptr) {
      info.param_names.emplace_back();
      info.param_types.push_back(StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0});
      continue;
    }
    info.param_names.push_back(param->getNameAsStr());
    StyioDataType param_type = param->getDType()->getDataType();
    maybe_intern_type(param_type);
    info.param_types.push_back(param_type);
  }
  info.consuming = !ast->isProperty()
                   && body_consumes_receiver(this, ast->getBody(), ast->getFamilyName());
  methods[ast->getMethodName()] = info;

  const std::string saved_receiver = active_resource_receiver_family_;
  const auto saved_types = local_binding_types;
  const auto saved_types_by_sid = local_binding_types_by_sid;
  const auto saved_fixed = fixed_assignment_names_;
  const auto saved_fixed_by_sid = fixed_assignment_names_by_sid_;
  const auto saved_bind = binding_info_;
  const auto saved_bind_by_sid = binding_info_by_sid_;
  const auto saved_consumed_tasks = consumed_task_names_;
  const auto saved_consumed_tasks_by_sid = consumed_task_names_by_sid_;
  const auto saved_consumed_resources = consumed_resource_names_;
  const auto saved_consumed_resources_by_sid = consumed_resource_names_by_sid_;
  const auto saved_owned_resources = owned_resource_names_;
  const auto saved_owned_resources_by_sid = owned_resource_names_by_sid_;
  const auto saved_snapshot_names = snapshot_var_names_;
  const auto saved_snapshot_names_by_sid = snapshot_var_names_by_sid_;
  auto restore_resource_method_scope = [&]()
  {
    active_resource_receiver_family_ = saved_receiver;
    local_binding_types = saved_types;
    local_binding_types_by_sid = saved_types_by_sid;
    fixed_assignment_names_ = saved_fixed;
    fixed_assignment_names_by_sid_ = saved_fixed_by_sid;
    binding_info_ = saved_bind;
    binding_info_by_sid_ = saved_bind_by_sid;
    consumed_task_names_ = saved_consumed_tasks;
    consumed_task_names_by_sid_ = saved_consumed_tasks_by_sid;
    consumed_resource_names_ = saved_consumed_resources;
    consumed_resource_names_by_sid_ = saved_consumed_resources_by_sid;
    owned_resource_names_ = saved_owned_resources;
    owned_resource_names_by_sid_ = saved_owned_resources_by_sid;
    snapshot_var_names_ = saved_snapshot_names;
    snapshot_var_names_by_sid_ = saved_snapshot_names_by_sid;
  };

  try {
    active_resource_receiver_family_ = ast->getFamilyName();
    local_binding_types.clear();
    local_binding_types_by_sid.clear();
    fixed_assignment_names_.clear();
    fixed_assignment_names_by_sid_.clear();
    binding_info_.clear();
    binding_info_by_sid_.clear();
    clear_consumed_task_names();
    clear_consumed_resource_names();
    clear_owned_resource_names();
    snapshot_var_names_.clear();
    snapshot_var_names_by_sid_.clear();
    for (std::size_t i = 0; i < info.param_names.size(); ++i) {
      if (!info.param_names[i].empty() && !info.param_types[i].isUndefined()) {
        record_local_binding_type(info.param_names[i], styio::session::kInvalidSymbolId, info.param_types[i]);
      }
    }
    if (ast->getBody() != nullptr) {
      ast->getBody()->typeInfer(this);
    }
    info.result_type = resource_method_simple_result_type_latest(this, ast->getBody());
    maybe_intern_type(info.result_type);
    if (resource_method_body_contains_return_latest(ast->getBody()) && info.result_type.isUndefined()) {
      throw StyioTypeError(
        "resource method return currently requires a single `<| expr` body"
        " or statement-only/scalar/list/dict/matrix local preface followed by a final `<| expr`"
      );
    }
    methods[ast->getMethodName()] = info;
  }
  catch (...) {
    restore_resource_method_scope();
    throw;
  }
  restore_resource_method_scope();
}

void
StyioSemaContext::typeInfer(ResourceOrderAST* ast) {
  if (ast->getBefore() != nullptr) {
    ast->getBefore()->typeInfer(this);
  }
  if (ast->getAfter() != nullptr) {
    ast->getAfter()->typeInfer(this);
  }
}

void
StyioSemaContext::typeInfer(ResourceDeclAST* ast) {
  for (const auto& slot : ast->getSlots()) {
    const std::string name = slot.name->getAsStr();
    const auto decl_sid = slot.name->getSymbolId();
    StyioDataType declared = styio_normalize_resource_decl_type(slot.type->getDataType());
    if (find_resource_binding_type(decl_sid, name) != nullptr) {
      throw StyioTypeError("resource `" + name + "` is already declared");
    }
    record_resource_binding_type(name, decl_sid, declared);
  }
  if (ast->getDriver() != nullptr) {
    ast->getDriver()->typeInfer(this);
  }
}

void
StyioSemaContext::typeInfer(ResourceRefAST* ast) {
  const auto* it = find_resource_binding_type(
    ast->getName()->getSymbolId(),
    ast->getNameStr());
  if (it == nullptr) {
    throw StyioTypeError("unknown resource `" + ast->getNameStr() + "`");
  }
  StyioDataType resource_type = *it;
  if (ast->isWholeResource()) {
    ast->setDataType(resource_type);
    return;
  }
  if (!styio_type_is_readable(resource_type)) {
    throw StyioTypeError("resource `" + ast->getNameStr() + "` does not have read capability");
  }
  if (!styio_type_is_indexable(resource_type)) {
    throw StyioTypeError("resource `" + ast->getNameStr() + "` is not indexable");
  }
  if (ast->getSelectorKind() == ResourceSelectorKind::SnapshotAll
      && resource_type.resource_shape == StyioResourceShapeKind::Scalar) {
    throw StyioTypeError("resource `" + ast->getNameStr() + "` does not support snapshot selection");
  }
  StyioDataType value_type = styio_topology_resource_value_type(resource_type);
  StyioValueFamily value_family = styio_value_family_for_type(value_type);
  const bool bounded_history =
    (resource_type.resource_shape == StyioResourceShapeKind::Fixed
     || resource_type.resource_shape == StyioResourceShapeKind::Recent)
    && resource_type.resource_shape_bound > 0;
  const bool supported_bounded_history_value =
    value_family == StyioValueFamily::Integer
    || value_family == StyioValueFamily::Float
    || value_family == StyioValueFamily::Bool
    || value_family == StyioValueFamily::Char
    || value_family == StyioValueFamily::String
    || value_family == StyioValueFamily::ListHandle
    || value_family == StyioValueFamily::DictHandle
    || value_family == StyioValueFamily::MatrixHandle;
  if (ast->getSelectorKind() == ResourceSelectorKind::Offset) {
    if (bounded_history && !supported_bounded_history_value) {
      throw StyioTypeError(
        "resource `" + ast->getNameStr() + "` history selection currently supports integer, float, bool, char, string, list, dict, or matrix resources"
      );
    }
    ast->setDataType(value_type);
    return;
  }

  if (!bounded_history) {
    throw StyioTypeError(
      "resource `" + ast->getNameStr() + "` slice/snapshot selection requires a bounded topology resource"
    );
  }
  if (!supported_bounded_history_value) {
    throw StyioTypeError(
      "resource `" + ast->getNameStr() + "` slice/snapshot selection currently supports integer, float, bool, char, string, list, dict, or matrix resources"
    );
  }
  if (resource_type.resource_shape_bound > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw StyioTypeError(
      "resource `" + ast->getNameStr() + "` selector history bound exceeds supported selector depth"
    );
  }
  if (ast->getSelectorKind() == ResourceSelectorKind::SliceFrom) {
    if (ast->getSelectorOffset() == std::numeric_limits<int>::min()) {
      throw StyioTypeError("resource slice selector depth exceeds supported selector depth");
    }
    const int depth = -ast->getSelectorOffset();
    if (depth <= 0) {
      throw StyioTypeError("resource slice selector requires a negative history offset");
    }
    if (static_cast<std::size_t>(depth) > resource_type.resource_shape_bound) {
      throw StyioTypeError(
        "resource selector depth exceeds resource `" + ast->getNameStr() + "` history bound"
      );
    }
  }
  ast->setDataType(styio_make_list_type(value_type.name));
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
  if (ast->getExpr() != nullptr) {
    ast->getExpr()->typeInfer(this);
  }
}

void
StyioSemaContext::typeInfer(FuncCallAST* ast) {
  if (ast->func_callee != nullptr) {
    ast->func_callee->typeInfer(this);
  }

  const StyioBuiltinMethodKind builtin_method = styio_builtin_method_kind(ast->getNameAsStr());
  if (ast->func_callee != nullptr && styio_is_predefined_list_operation_kind(builtin_method)) {
    for (auto* arg : ast->getArgList()) {
      arg->typeInfer(this);
    }

    StyioDataType callee_type = infer_expr_type(this, ast->func_callee);
    if (!styio_is_list_type(callee_type)) {
      throw StyioTypeError(
        "predefined list operation `" + ast->getNameAsStr() + "` requires a list[T] receiver"
      );
    }

    StyioDataType elem_type = styio_data_type_from_name(styio_type_item_type_name(callee_type));
    if (!styio_type_supports_runtime_list_elem(elem_type)) {
      throw StyioTypeError(
        "predefined list operation `" + ast->getNameAsStr()
        + "` requires a runtime list element family"
      );
    }

    if (builtin_method == StyioBuiltinMethodKind::ListPush) {
      if (ast->getArgList().size() != 1) {
        throw StyioTypeError("list.push(value) requires exactly one argument");
      }
      StyioDataType value_type = infer_expr_type(this, ast->getArgList()[0]);
      if (!container_value_assignable(elem_type, value_type, this)) {
        throw StyioTypeError(
          "list.push(value) expects `" + elem_type.name + "`, got `" + value_type.name + "`"
        );
      }
      return;
    }

    if (builtin_method == StyioBuiltinMethodKind::ListInsert) {
      if (ast->getArgList().size() != 2) {
        throw StyioTypeError("list.insert(index, value) requires exactly two arguments");
      }
      StyioDataType index_type = infer_expr_type(this, ast->getArgList()[0]);
      if (index_type.option != StyioDataTypeOption::Integer) {
        throw StyioTypeError("list.insert(index, value) requires an integer index");
      }
      StyioDataType value_type = infer_expr_type(this, ast->getArgList()[1]);
      if (!container_value_assignable(elem_type, value_type, this)) {
        throw StyioTypeError(
          "list.insert(index, value) expects `" + elem_type.name + "`, got `"
          + value_type.name + "`"
        );
      }
      return;
    }

    if (!ast->getArgList().empty()) {
      throw StyioTypeError("list.pop() does not take arguments");
    }
    return;
  }

  if (ast->func_callee != nullptr && styio_is_predefined_string_operation_kind(builtin_method)) {
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

  if (ast->func_callee != nullptr) {
    const std::string family = resource_family_for_expr(this, ast->func_callee);
    if (!family.empty()) {
      vector<StyioDataType> arg_types;
      for (auto* arg : ast->getArgList()) {
        arg->typeInfer(this);
        arg_types.push_back(infer_expr_type(this, arg));
      }
      auto family_it = resource_method_defs_.find(family);
      if (family_it == resource_method_defs_.end()
          || family_it->second.find(ast->getNameAsStr()) == family_it->second.end()) {
        throw StyioTypeError(
          "resource method cannot be resolved: @" + family + "::" + ast->getNameAsStr()
        );
      }
      const ResourceMethodInfo& method = family_it->second[ast->getNameAsStr()];
      if (method.property) {
        throw StyioTypeError(
          "resource property @" + family + "::" + ast->getNameAsStr()
          + " is not callable"
        );
      }
      if (arg_types.size() != method.param_count) {
        throw StyioTypeError(
          "resource method @" + family + "::" + ast->getNameAsStr()
          + " expects " + std::to_string(method.param_count)
          + " argument(s), got " + std::to_string(arg_types.size())
        );
      }
      for (std::size_t i = 0; i < method.param_types.size() && i < arg_types.size(); ++i) {
        const StyioDataType& declared_type = method.param_types[i];
        if (declared_type.isUndefined()) {
          continue;
        }
        if (!func_param_accepts_arg(declared_type, arg_types[i], this)) {
          std::string param_name = i < method.param_names.size() && !method.param_names[i].empty()
                                     ? method.param_names[i]
                                     : std::to_string(i);
          throw StyioTypeError(
            "resource method argument type mismatch for parameter '" + param_name
            + "': expected " + declared_type.name + ", got " + arg_types[i].name
          );
        }
      }
      if (method.consuming) {
        if (auto* receiver_name = dynamic_cast<NameAST*>(ast->func_callee)) {
          const std::string resource_name = receiver_name->getAsStr();
          if (is_task_outer_resource_name(receiver_name->getSymbolId(), resource_name)) {
            throw StyioTypeError("task cannot consume outer resource `" + resource_name + "`");
          }
          if (is_consumed_resource_name(receiver_name->getSymbolId(), resource_name)) {
            throw StyioTypeError("double destroy: resource `" + resource_name + "` was already destroyed");
          }
          record_consumed_resource_name(resource_name, receiver_name->getSymbolId());
        }
      }
      return;
    }
  }

  if (ast->isCallableApply()) {
    throw StyioTypeError(
      "one-shot continuation resume `<|` requires continuation lowering; "
      "captured continuations must be resumed or discontinued exactly once"
    );
  }

  if (ast->func_callee == nullptr && is_matrix_intrinsic_name(ast->getNameAsStr())) {
    for (auto* arg : ast->getArgList()) {
      arg->typeInfer(this);
    }
    (void)infer_matrix_intrinsic_type(this, ast);
    return;
  }

  StyioAST* func_def = find_function_def(
    ast->func_name->getSymbolId(),
    ast->getNameAsStr()
  );
  if (func_def != nullptr
      && !imported_callable_is_visible(ast->getNameAsStr())) {
    const auto* imported =
      find_imported_callable_definition(ast->getNameAsStr());
    throw StyioTypeError(
      "callable `" + ast->getNameAsStr()
      + "` is not exported to the active module by imported module `"
      + (imported == nullptr ? std::string("unknown") : imported->module_id)
      + "`"
    );
  }
  const auto* native_def =
    func_def == nullptr
      ? find_native_function_def(
          ast->func_name->getSymbolId(),
          ast->getNameAsStr())
      : nullptr;
  const CallableTypeScheme* callable_scheme =
    func_def == nullptr
      ? nullptr
      : find_callable_type_scheme(ast->getNameAsStr());
  const auto func_args =
    func_def == nullptr
      ? std::vector<ParamAST*>{}
      : params_of_func_def(func_def);
  vector<StyioDataType> arg_types;

  for (std::size_t i = 0; i < ast->getArgList().size(); ++i) {
    StyioAST* arg = ast->getArgList()[i];
    StyioDataType expected_arg{
      StyioDataTypeOption::Undefined, "undefined", 0
    };
    if (i < func_args.size()) {
      expected_arg = func_args[i]->getDType()->getDataType();
    }
    if (expected_arg.isUndefined()
        && callable_scheme != nullptr
        && i < callable_scheme->params.size()) {
      if (auto closed =
            closed_callable_term_type(callable_scheme->params[i])) {
        expected_arg = *closed;
      }
    }
    if (expected_arg.isUndefined()
        && native_def != nullptr
        && i < native_def->arg_types.size()) {
      expected_arg = native_def->arg_types[i];
    }
    apply_callable_expected_type_to_tail(arg, expected_arg);
    arg->typeInfer(this);
    arg_types.push_back(infer_expr_type(this, arg));
  }

  if (func_def != nullptr) {
    enforce_effect_monomorphic_instance(
      ast->getNameAsStr(),
      arg_types);
    if (callable_scheme != nullptr) {
      CallableSpecialization specialization =
        instantiate_callable_type_scheme(ast, arg_types);
      prepare_callable_specialization_body(
        func_def,
        specialization);
      return;
    }
  }

  if (func_def == nullptr) {
    if (native_def == nullptr) {
      throw StyioTypeError("unknown function `" + ast->getNameAsStr() + "`");
    }
    if (arg_types.size() != native_def->arg_types.size()) {
      throw StyioTypeError(
        "function `" + ast->getNameAsStr() + "` expects "
        + std::to_string(native_def->arg_types.size()) + " argument(s), got "
        + std::to_string(arg_types.size())
      );
    }
    for (size_t i = 0; i < native_def->arg_types.size(); ++i) {
      if (!func_param_accepts_arg(
            native_def->arg_types[i],
            arg_types[i],
            this)) {
        throw StyioTypeError(
          "function argument type mismatch for native parameter "
          + std::to_string(i) + ": expected "
          + native_def->arg_types[i].name
          + ", got " + arg_types[i].name
        );
      }
    }
    return;
  }

  if (arg_types.size() != func_args.size()) {
    throw StyioTypeError(
      "function `" + ast->getNameAsStr() + "` expects "
      + std::to_string(func_args.size()) + " argument(s), got "
      + std::to_string(arg_types.size())
    );
  }

  for (size_t i = 0; i < func_args.size(); i++) {
    StyioDataType declared_type = func_args[i]->getDType()->getDataType();
    if (declared_type.isUndefined()) {
      func_args[i]->setDataType(arg_types[i]);
      continue;
    }
    if (!func_param_accepts_arg(declared_type, arg_types[i], this)) {
      throw StyioTypeError(
        "function argument type mismatch for parameter '" + func_args[i]->getNameAsStr()
        + "': expected " + declared_type.name + ", got " + arg_types[i].name
      );
    }
  }

  const ImportedCallableDefinition* imported_concrete =
    find_imported_callable_definition(ast->getNameAsStr());
  if (imported_concrete != nullptr
      && !imported_concrete->has_scheme) {
    prepare_imported_concrete_callable_body(
      ast->getNameAsStr());
    return;
  }

  const std::string function_name = ast->getNameAsStr();
  auto function_sid = ast->func_name->getSymbolId();
  if (function_sid == styio::session::kInvalidSymbolId) {
    function_sid = lookup_semantic_symbol(function_name);
  }
  const StyioDataType declared_return =
    declared_function_return_type_latest(func_def);
  const bool function_body_active =
    active_function_body_inference_.count(function_name) != 0
    || (function_sid != styio::session::kInvalidSymbolId
        && active_function_body_inference_by_sid_.count(function_sid) != 0);
  if (!function_body_active) {
    active_function_body_inference_.insert(function_name);
    if (function_sid != styio::session::kInvalidSymbolId) {
      active_function_body_inference_by_sid_.insert(function_sid);
    }
    const auto saved_types = local_binding_types;
    const auto saved_types_by_sid = local_binding_types_by_sid;
    const auto saved_funcs = func_defs;
    const auto saved_funcs_by_sid = func_defs_by_sid;
    const auto saved_fixed = fixed_assignment_names_;
    const auto saved_fixed_by_sid = fixed_assignment_names_by_sid_;
    const auto saved_bind = binding_info_;
    const auto saved_bind_by_sid = binding_info_by_sid_;
    const auto saved_consumed_tasks = consumed_task_names_;
    const auto saved_consumed_tasks_by_sid = consumed_task_names_by_sid_;
    const auto saved_consumed_resources = consumed_resource_names_;
    const auto saved_consumed_resources_by_sid = consumed_resource_names_by_sid_;
    const auto saved_owned_resources = owned_resource_names_;
    const auto saved_owned_resources_by_sid = owned_resource_names_by_sid_;
    const auto saved_snapshot_names = snapshot_var_names_;
    const auto saved_snapshot_names_by_sid = snapshot_var_names_by_sid_;
    const auto saved_function_stack = active_function_body_stack_;
    const auto saved_function_sid_stack = active_function_body_sid_stack_;

    auto restore_function_scope = [&]()
    {
      local_binding_types = saved_types;
      local_binding_types_by_sid = saved_types_by_sid;
      func_defs = saved_funcs;
      func_defs_by_sid = saved_funcs_by_sid;
      fixed_assignment_names_ = saved_fixed;
      fixed_assignment_names_by_sid_ = saved_fixed_by_sid;
      binding_info_ = saved_bind;
      binding_info_by_sid_ = saved_bind_by_sid;
      consumed_task_names_ = saved_consumed_tasks;
      consumed_task_names_by_sid_ = saved_consumed_tasks_by_sid;
      consumed_resource_names_ = saved_consumed_resources;
      consumed_resource_names_by_sid_ = saved_consumed_resources_by_sid;
      owned_resource_names_ = saved_owned_resources;
      owned_resource_names_by_sid_ = saved_owned_resources_by_sid;
      snapshot_var_names_ = saved_snapshot_names;
      snapshot_var_names_by_sid_ = saved_snapshot_names_by_sid;
      active_function_body_stack_ = saved_function_stack;
      active_function_body_sid_stack_ = saved_function_sid_stack;
      active_function_body_inference_.erase(function_name);
      if (function_sid != styio::session::kInvalidSymbolId) {
        active_function_body_inference_by_sid_.erase(function_sid);
      }
    };

    try {
      for (size_t i = 0; i < func_args.size(); ++i) {
        StyioDataType param_type = func_args[i]->getDType()->getDataType();
        if (param_type.isUndefined() && i < arg_types.size()) {
          param_type = arg_types[i];
        }
        record_local_binding_type(
          func_args[i]->getNameAsStr(),
          styio::session::kInvalidSymbolId,
          param_type);
        maybe_intern_type(param_type);
      }

      push_active_function_body(function_name, function_sid);
      if (auto* f = dynamic_cast<FunctionAST*>(func_def)) {
        if (f->func_body != nullptr) {
          apply_matrix_literal_context(this, f->func_body, declared_return);
          apply_callable_expected_type_to_tail(
            f->func_body,
            declared_return);
          f->func_body->typeInfer(this);
          StyioDataType return_type = function_body_tail_type_latest(this, f->func_body);
          require_matrix_return_compatible_latest(function_name, declared_return, return_type);
          record_inferred_function_return_type(return_type);
        }
      }
      else if (auto* sf = dynamic_cast<SimpleFuncAST*>(func_def)) {
        if (sf->ret_expr != nullptr) {
          apply_matrix_literal_context(this, sf->ret_expr, declared_return);
          apply_callable_expected_type_to_tail(
            sf->ret_expr,
            declared_return);
          sf->ret_expr->typeInfer(this);
          StyioDataType return_type = infer_expr_type(this, sf->ret_expr);
          require_matrix_return_compatible_latest(function_name, declared_return, return_type);
          record_inferred_function_return_type(return_type);
        }
      }
    }
    catch (...) {
      restore_function_scope();
      throw;
    }
    restore_function_scope();
  }
}

void
StyioSemaContext::typeInfer(AttrAST* ast) {
  ast->body->typeInfer(this);
  auto* attr_name = dynamic_cast<NameAST*>(ast->attr);
  if (attr_name == nullptr) {
    throw StyioTypeError("attribute access requires a simple name");
  }
  const std::string attr_str = attr_name->getAsStr();
  StyioDataType body_type = infer_expr_type(this, ast->body);
  if (attr_str == "length" || attr_str == "size") {
    if (!styio_type_is_sized(body_type)) {
      throw StyioTypeError(".length/.size require a sized value");
    }
    return;
  }
  if ((attr_str == "keys" || attr_str == "values") && styio_is_dict_type(body_type)) {
    return;
  }
  const std::string family = resource_family_for_expr(this, ast->body);
  if (attr_str == "pressure") {
    std::string pressure_family = family;
    if (pressure_family.empty()) {
      if (auto* name = dynamic_cast<NameAST*>(ast->body)) {
        if (find_resource_binding_type(name->getSymbolId(), name->getAsStr()) != nullptr) {
          pressure_family = "resource";
        }
      }
    }
    if (pressure_family.empty()) {
      throw StyioTypeError("pressure observer requires a resource family that exposes a pressure stream");
    }
    throw StyioTypeError(
      "resource family @" + pressure_family + " does not expose pressure stream: .pressure"
    );
  }
  if (!family.empty()) {
    const ResourceMethodInfo* method = find_resource_method(family, attr_str);
    if (method != nullptr && method->property) {
      return;
    }
    throw StyioTypeError(
      "resource method cannot be resolved: @" + family + "::" + attr_str
    );
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
  auto saved_by_sid = local_binding_types_by_sid;
  auto saved_fixed = fixed_assignment_names_;
  auto saved_fixed_by_sid = fixed_assignment_names_by_sid_;
  auto saved_bind = binding_info_;
  auto saved_bind_by_sid = binding_info_by_sid_;

  ast->getThen()->typeInfer(this);
  auto then_types = local_binding_types;
  auto then_bind = binding_info_;

  local_binding_types = saved;
  local_binding_types_by_sid = saved_by_sid;
  fixed_assignment_names_ = saved_fixed;
  fixed_assignment_names_by_sid_ = saved_fixed_by_sid;
  binding_info_ = saved_bind;
  binding_info_by_sid_ = saved_bind_by_sid;

  if (ast->getElse() != nullptr) {
    ast->getElse()->typeInfer(this);
    auto else_types = local_binding_types;
    auto else_bind = binding_info_;

    local_binding_types = saved;
    local_binding_types_by_sid = saved_by_sid;
    fixed_assignment_names_ = saved_fixed;
    fixed_assignment_names_by_sid_ = saved_fixed_by_sid;
    binding_info_ = saved_bind;
    binding_info_by_sid_ = saved_bind_by_sid;

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
      record_binding_info(entry.first, styio::session::kInvalidSymbolId, merged);

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
      const StyioDataType merged_type =
        merge_cond_flow_branch_type(then_type, else_type, saved_type, this);
      record_local_binding_type(entry.first, styio::session::kInvalidSymbolId, merged_type);
    }
    return;
  }

  local_binding_types = saved;
  local_binding_types_by_sid = saved_by_sid;
  fixed_assignment_names_ = saved_fixed;
  fixed_assignment_names_by_sid_ = saved_fixed_by_sid;
  binding_info_ = saved_bind;
  binding_info_by_sid_ = saved_bind_by_sid;
}

void
StyioSemaContext::typeInfer(AnonyFuncAST* ast) {
}

void
StyioSemaContext::typeInfer(FunctionAST* ast) {
  if (std::holds_alternative<TypeTupleAST*>(ast->ret_type)) {
    throw StyioTypeError(
      "tuple function return annotations require tuple value IR; tuple returns are not implemented"
    );
  }
  maybe_intern_function_signature_types(this, ast->params, ast->ret_type);
  record_function_def(ast->getNameAsStr(), ast->func_name->getSymbolId(), ast);
}

void
StyioSemaContext::typeInfer(SimpleFuncAST* ast) {
  if (std::holds_alternative<TypeTupleAST*>(ast->ret_type)) {
    throw StyioTypeError(
      "tuple function return annotations require tuple value IR; tuple returns are not implemented"
    );
  }
  maybe_intern_function_signature_types(this, ast->params, ast->ret_type);
  record_function_def(ast->func_name->getAsStr(), ast->func_name->getSymbolId(), ast);
}

void
StyioSemaContext::typeInfer(IteratorAST* ast) {
  if (ast->getNodeType() == StyioNodeType::IterSeq) {
    throw StyioTypeError(
      "iterator sequence hash-tag routing is not implemented; use #(param) => { ... } iterator bodies"
    );
  }

  auto saved = local_binding_types;
  auto saved_by_sid = local_binding_types_by_sid;
  auto saved_fixed = fixed_assignment_names_;
  auto saved_fixed_by_sid = fixed_assignment_names_by_sid_;
  auto saved_bind = binding_info_;
  auto saved_bind_by_sid = binding_info_by_sid_;
  ast->collection->typeInfer(this);
  StyioDataType collection_type = infer_expr_type(this, ast->collection);
  if (!styio_type_is_iterable(collection_type)) {
    throw StyioTypeError("iteration requires an iterable value");
  }
  StyioDataType et = infer_collection_elem_type(this, ast->collection);
  if (!ast->params.empty()) {
    record_local_binding_type(ast->params[0]->getNameAsStr(), styio::session::kInvalidSymbolId, et);
  }
  for (auto* f : ast->following) {
    f->typeInfer(this);
  }
  local_binding_types = std::move(saved);
  local_binding_types_by_sid = std::move(saved_by_sid);
  fixed_assignment_names_ = std::move(saved_fixed);
  fixed_assignment_names_by_sid_ = std::move(saved_fixed_by_sid);
  binding_info_ = std::move(saved_bind);
  binding_info_by_sid_ = std::move(saved_bind_by_sid);
}

void
StyioSemaContext::typeInfer(StreamZipAST* ast) {
  auto saved = local_binding_types;
  auto saved_by_sid = local_binding_types_by_sid;
  auto saved_fixed = fixed_assignment_names_;
  auto saved_fixed_by_sid = fixed_assignment_names_by_sid_;
  auto saved_bind = binding_info_;
  auto saved_bind_by_sid = binding_info_by_sid_;
  auto is_direct_stdin = [](StyioAST* expr) {
    auto* stream = dynamic_cast<StdStreamAST*>(expr);
    return stream != nullptr && stream->getStreamKind() == StdStreamKind::Stdin;
  };
  auto is_direct_file = [](StyioAST* expr) {
    return expr != nullptr && expr->getNodeType() == StyioNodeType::FileResource;
  };
  if (is_direct_stdin(ast->getCollectionA()) && is_direct_stdin(ast->getCollectionB())) {
    throw StyioTypeError(
      "zip over @stdin on both sides requires a distinct stream-driver decision"
    );
  }
  ast->getCollectionA()->typeInfer(this);
  ast->getCollectionB()->typeInfer(this);
  StyioDataType ta = infer_expr_type(this, ast->getCollectionA());
  StyioDataType tb = infer_expr_type(this, ast->getCollectionB());
  auto is_supported_zip_source = [&](StyioAST* expr, const StyioDataType& type) {
    if (is_direct_file(expr) || is_direct_stdin(expr)) {
      return true;
    }
    return styio_is_list_type(type);
  };
  if (!is_supported_zip_source(ast->getCollectionA(), ta)
      || !is_supported_zip_source(ast->getCollectionB(), tb)) {
    throw StyioTypeError("zip requires iterable inputs on both sides");
  }
  StyioDataType ea = infer_collection_elem_type(this, ast->getCollectionA());
  StyioDataType eb = infer_collection_elem_type(this, ast->getCollectionB());
  if (!ast->getParamsA().empty()) {
    record_local_binding_type(ast->getParamsA()[0]->getNameAsStr(), styio::session::kInvalidSymbolId, ea);
  }
  if (!ast->getParamsB().empty()) {
    record_local_binding_type(ast->getParamsB()[0]->getNameAsStr(), styio::session::kInvalidSymbolId, eb);
  }
  for (auto* f : ast->getFollowing()) {
    f->typeInfer(this);
  }
  local_binding_types = std::move(saved);
  local_binding_types_by_sid = std::move(saved_by_sid);
  fixed_assignment_names_ = std::move(saved_fixed);
  fixed_assignment_names_by_sid_ = std::move(saved_fixed_by_sid);
  binding_info_ = std::move(saved_bind);
  binding_info_by_sid_ = std::move(saved_bind_by_sid);
}

void
StyioSemaContext::typeInfer(SnapshotDeclAST* ast) {
  record_snapshot_var_name(ast->getVar()->getAsStr(), ast->getVar()->getSymbolId());
  record_local_binding_type(
    ast->getVar()->getAsStr(),
    ast->getVar()->getSymbolId(),
    StyioDataType{StyioDataTypeOption::Integer, "i64", 64});
  ast->getResource()->typeInfer(this);
}

void
StyioSemaContext::typeInfer(InstantPullAST* ast) {
  ast->getResource()->typeInfer(this);
  if (auto* name = dynamic_cast<NameAST*>(ast->getResource())) {
    StyioDataType source_type = infer_expr_type(this, name);
    if (source_type.handle_family != StyioHandleFamily::File) {
      throw StyioTypeError("instant pull handle source must be an acquired file handle");
    }
  }
  StyioDataType result_type = ast->getDataType();
  if (styio_is_list_type(result_type)) {
    if (!styio_stdin_list_elem_type_supported(styio_type_item_type_name(result_type))) {
      throw StyioTypeError("typed stdin list pull supports list[i64], list[f64], or list[string]");
    }
    return;
  }
  if (result_type.option != StyioDataTypeOption::Integer
      && result_type.option != StyioDataTypeOption::Float
      && result_type.option != StyioDataTypeOption::String) {
    throw StyioTypeError("typed stdin pull supports i64, f64, string, or list[T] targets");
  }
}

void
StyioSemaContext::typeInfer(TaskBlockAST* ast) {
  auto saved_types = local_binding_types;
  auto saved_types_by_sid = local_binding_types_by_sid;
  auto saved_fixed = fixed_assignment_names_;
  auto saved_fixed_by_sid = fixed_assignment_names_by_sid_;
  auto saved_bind = binding_info_;
  auto saved_bind_by_sid = binding_info_by_sid_;
  auto saved_consumed = consumed_task_names_;
  auto saved_consumed_by_sid = consumed_task_names_by_sid_;
  auto saved_consumed_resources = consumed_resource_names_;
  auto saved_consumed_resources_by_sid = consumed_resource_names_by_sid_;
  auto saved_owned_resources = owned_resource_names_;
  auto saved_owned_resources_by_sid = owned_resource_names_by_sid_;

  auto restore_task_scope = [&]()
  {
    local_binding_types = std::move(saved_types);
    local_binding_types_by_sid = std::move(saved_types_by_sid);
    fixed_assignment_names_ = std::move(saved_fixed);
    fixed_assignment_names_by_sid_ = std::move(saved_fixed_by_sid);
    binding_info_ = std::move(saved_bind);
    binding_info_by_sid_ = std::move(saved_bind_by_sid);
    consumed_task_names_ = std::move(saved_consumed);
    consumed_task_names_by_sid_ = std::move(saved_consumed_by_sid);
    consumed_resource_names_ = std::move(saved_consumed_resources);
    consumed_resource_names_by_sid_ = std::move(saved_consumed_resources_by_sid);
    owned_resource_names_ = std::move(saved_owned_resources);
    owned_resource_names_by_sid_ = std::move(saved_owned_resources_by_sid);
  };

  bool pushed_task_outer_resources = false;
  auto pop_task_outer_resources = [&]()
  {
    if (!pushed_task_outer_resources) {
      return;
    }
    task_outer_resource_names_stack_.pop_back();
    task_outer_resource_names_by_sid_stack_.pop_back();
    pushed_task_outer_resources = false;
  };

  task_outer_resource_names_stack_.push_back(owned_resource_names_);
  task_outer_resource_names_by_sid_stack_.push_back(owned_resource_names_by_sid_);
  pushed_task_outer_resources = true;
  try {
    ast->getBody()->typeInfer(this);
    StyioDataType result_type = infer_task_block_result_type(this, ast->getBody());
    ast->setResultType(result_type);
    pop_task_outer_resources();
  }
  catch (...) {
    pop_task_outer_resources();
    restore_task_scope();
    throw;
  }

  restore_task_scope();
}

void
StyioSemaContext::typeInfer(TaskGroupLaunchAST* ast) {
  for (auto* entry : ast->getEntries()) {
    entry->typeInfer(this);
  }
}

void
StyioSemaContext::typeInfer(FlowBindAST* ast) {
  if (ast->getSource() == nullptr) {
    if (ast->hasFallback()) {
      throw StyioTypeError("bare continuation freeze `?| ->` does not accept fallback");
    }
    throw StyioTypeError(
      "bare continuation freeze `?| ->` requires continuation lowering; "
      "captured continuations must be resumed or discontinued exactly once"
    );
  }
  ast->getSource()->typeInfer(this);
  if (ast->hasFallback()) {
    ast->getFallback()->typeInfer(this);
  }
  const std::string target = ast->getTargetNameAsStr();
  const auto target_sid = ast->getTarget() != nullptr && ast->getTarget()->getName() != nullptr
                            ? ast->getTarget()->getName()->getSymbolId()
                            : styio::session::kInvalidSymbolId;
  const StyioDataType* existing_target_type = find_local_binding_type(target_sid, target);
  const BindingInfo* existing_target_info = find_binding_info(target_sid, target);
  const bool target_exists =
    existing_target_type != nullptr || existing_target_info != nullptr;
  if (ast->declaresTarget() && target_exists) {
    throw StyioTypeError("await target `" + target + "` is already declared");
  }
  if (!ast->declaresTarget() && !target_exists) {
    throw StyioTypeError("flow bind target `" + target + "` must be declared before use");
  }
  if (!ast->declaresTarget() && is_fixed_assignment_name(target_sid, target)) {
    throw StyioTypeError("flow bind target `" + target + "` is final and cannot be reassigned");
  }

  StyioDataType source_type = infer_expr_type(this, ast->getSource());
  StyioDataType result_type = source_type;
  if (ast->isAwaitBind() && source_type.handle_family != StyioHandleFamily::Task) {
    throw StyioTypeError("await source for `?|` must be a task/future handle");
  }
  if (source_type.handle_family == StyioHandleFamily::Task) {
    if (auto* task_name = dynamic_cast<NameAST*>(ast->getSource())) {
      if (is_consumed_task_name(task_name->getSymbolId(), task_name->getAsStr())) {
        throw StyioTypeError("task `" + task_name->getAsStr() + "` was already pulled");
      }
      record_consumed_task_name(task_name->getAsStr(), task_name->getSymbolId());
    }
    result_type = task_result_type_from_task_type(source_type);
  }

  StyioDataType target_type = ast->declaresTarget()
                                ? ast->getTarget()->getDType()->type
                                : (existing_target_type != nullptr
                                     ? *existing_target_type
                                     : existing_target_info->declared_type);
  if (target_type.isUndefined()) {
    target_type = result_type;
    ast->getTarget()->setDataType(target_type);
  }
  if (!target_type.isUndefined() && !container_value_assignable(target_type, result_type, this)) {
    throw StyioTypeError(
      "flow bind target `" + target + "` expects " + target_type.name
      + ", got " + result_type.name
    );
  }
  if (ast->hasFallback()) {
    StyioDataType fallback_type = infer_expr_type(this, ast->getFallback());
    if (!target_type.isUndefined() && !container_value_assignable(target_type, fallback_type, this)) {
      throw StyioTypeError(
        "await fallback for `" + target + "` expects " + target_type.name
        + ", got " + fallback_type.name
      );
    }
  }
  ast->setResultType(target_type.isUndefined() ? result_type : target_type);

  if (ast->declaresTarget()) {
    record_local_binding_type(target, target_sid, ast->getResultType());
    maybe_intern_type(ast->getResultType());

    BindingInfo info;
    info.final_slot = false;
    info.dynamic_slot = false;
    info.resource_value = false;
    info.value_kind = binding_value_kind_for_type(ast->getResultType());
    info.declared_type = ast->getResultType();
    record_binding_info(target, target_sid, info);
  }
}

void
StyioSemaContext::typeInfer(IterSeqAST* ast) {
  (void)ast;
  throw StyioTypeError(
    "iterator sequence hash-tag routing is not implemented; use #(param) => { ... } iterator bodies"
  );
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
  for (auto const& pr : ast->case_list) {
    if (pr.first != nullptr) {
      pr.first->typeInfer(this);
    }
    if (pr.second != nullptr) {
      pr.second->typeInfer(this);
    }
  }
  if (ast->case_default != nullptr) {
    ast->case_default->typeInfer(this);
  }
}

void
StyioSemaContext::typeInfer(MatchCasesAST* ast) {
  if (ast == nullptr || ast->getScrutinee() == nullptr || ast->getCases() == nullptr) {
    throw StyioTypeError("match cases require a scrutinee and case block");
  }

  ast->getScrutinee()->typeInfer(this);
  StyioDataType scrutinee_type = infer_expr_type(this, ast->getScrutinee());
  if (scrutinee_type.option != StyioDataTypeOption::Integer) {
    throw StyioTypeError("match scrutinee must be integer-typed");
  }

  auto* scrutinee_name_ast = dynamic_cast<NameAST*>(ast->getScrutinee());
  const std::string* scrutinee_name =
    scrutinee_name_ast != nullptr ? &scrutinee_name_ast->getAsStr() : nullptr;

  const auto saved_types = local_binding_types;
  const auto saved_types_by_sid = local_binding_types_by_sid;
  const auto saved_funcs = func_defs;
  const auto saved_fixed = fixed_assignment_names_;
  const auto saved_fixed_by_sid = fixed_assignment_names_by_sid_;
  const auto saved_bind = binding_info_;
  const auto saved_bind_by_sid = binding_info_by_sid_;
  const auto saved_consumed_tasks = consumed_task_names_;
  const auto saved_consumed_tasks_by_sid = consumed_task_names_by_sid_;
  const auto saved_consumed_resources = consumed_resource_names_;
  const auto saved_consumed_resources_by_sid = consumed_resource_names_by_sid_;
  const auto saved_owned_resources = owned_resource_names_;
  const auto saved_owned_resources_by_sid = owned_resource_names_by_sid_;
  const auto saved_snapshot_names = snapshot_var_names_;
  const auto saved_snapshot_names_by_sid = snapshot_var_names_by_sid_;

  auto restore_branch_scope = [&]()
  {
    local_binding_types = saved_types;
    local_binding_types_by_sid = saved_types_by_sid;
    func_defs = saved_funcs;
    fixed_assignment_names_ = saved_fixed;
    fixed_assignment_names_by_sid_ = saved_fixed_by_sid;
    binding_info_ = saved_bind;
    binding_info_by_sid_ = saved_bind_by_sid;
    consumed_task_names_ = saved_consumed_tasks;
    consumed_task_names_by_sid_ = saved_consumed_tasks_by_sid;
    consumed_resource_names_ = saved_consumed_resources;
    consumed_resource_names_by_sid_ = saved_consumed_resources_by_sid;
    owned_resource_names_ = saved_owned_resources;
    owned_resource_names_by_sid_ = saved_owned_resources_by_sid;
    snapshot_var_names_ = saved_snapshot_names;
    snapshot_var_names_by_sid_ = saved_snapshot_names_by_sid;
  };

  auto infer_branch = [&](StyioAST* branch) -> StyioDataType
  {
    restore_branch_scope();
    try {
      if (branch != nullptr) {
        branch->typeInfer(this);
      }
      StyioDataType branch_type = match_branch_tail_type(this, branch);
      restore_branch_scope();
      return branch_type;
    }
    catch (...) {
      restore_branch_scope();
      throw;
    }
  };

  StyioDataType result_type{StyioDataTypeOption::Undefined, "undefined", 0};
  CasesAST* cases = ast->getCases();
  for (auto const& pr : cases->case_list) {
    if (pr.first == nullptr) {
      throw StyioTypeError("match arm requires a pattern expression");
    }
    pr.first->typeInfer(this);
    if (!match_pattern_supported_latest(pr.first, scrutinee_name)) {
      throw StyioTypeError("match arms need integer literal patterns in this language feature");
    }

    StyioDataType branch_type = infer_branch(pr.second);
    if (!match_result_type_supported(branch_type)) {
      throw StyioTypeError("match branch values support scalar and string results in this slice");
    }
    result_type = merge_match_value_type(result_type, branch_type, this);
    ast->setDataType(result_type);
    record_inferred_function_return_type(result_type);
  }

  if (cases->case_default != nullptr) {
    StyioDataType branch_type = infer_branch(cases->case_default);
    if (!match_result_type_supported(branch_type)) {
      throw StyioTypeError("match branch values support scalar and string results in this slice");
    }
    result_type = merge_match_value_type(result_type, branch_type, this);
    ast->setDataType(result_type);
    record_inferred_function_return_type(result_type);
  }

  if (result_type.isUndefined()) {
    result_type = kI64Type;
  }
  ast->setDataType(result_type);
  record_inferred_function_return_type(result_type);
  restore_branch_scope();
}

void
StyioSemaContext::typeInfer(BlockAST* ast) {
  for (auto* s : ast->stmts) {
    if (dynamic_cast<ResourceDeclAST*>(s) != nullptr) {
      throw StyioTypeError("resource declarations are top-level only");
    }
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
  snapshot_var_names_by_sid_.clear();
  func_defs.clear();
  func_defs_by_sid.clear();
  native_func_defs.clear();
  native_func_defs_by_sid.clear();
  local_binding_types.clear();
  local_binding_types_by_sid.clear();
  fixed_assignment_names_.clear();
  fixed_assignment_names_by_sid_.clear();
  binding_info_.clear();
  binding_info_by_sid_.clear();
  resource_method_defs_.clear();
  for (const auto& method : styio_builtin_resource_methods_latest()) {
    ResourceMethodInfo info;
    info.final_binding = method.final_binding;
    info.consuming = method.consuming;
    info.property = method.property;
    info.param_count = method.param_count;
    if (styio_is_resource_property_method_kind(method.kind)) {
      info.result_type = kStringType;
    }
    resource_method_defs_[method.family][method.method] = info;
  }
  resource_binding_types_.clear();
  resource_binding_types_by_sid_.clear();
  collect_bind_resource_writes_.clear();
  collect_bind_handle_acquires_.clear();
  collect_bind_resource_write_types_.clear();
  collect_bind_handle_acquire_types_.clear();
  clear_consumed_task_names();
  clear_consumed_resource_names();
  clear_owned_resource_names();
  task_outer_resource_names_stack_.clear();
  task_outer_resource_names_by_sid_stack_.clear();
  active_function_body_inference_.clear();
  active_function_body_inference_by_sid_.clear();
  active_function_body_stack_.clear();
  active_function_body_sid_stack_.clear();
  inferred_function_return_types_.clear();
  inferred_function_return_types_by_sid_.clear();
  active_resource_receiver_family_.clear();
  register_imported_callable_definitions();
  auto stmts = ast->getStmts();
  std::vector<std::string> exported_symbols;
  for (auto const& s : stmts) {
    if (auto* export_decl = dynamic_cast<ExportDeclAST*>(s)) {
      const auto& symbols = export_decl->getSymbols();
      exported_symbols.insert(exported_symbols.end(), symbols.begin(), symbols.end());
    }
  }
  for (auto const& s : stmts) {
    if (auto* f = dynamic_cast<FunctionAST*>(s)) {
      record_function_def(f->getNameAsStr(), f->func_name->getSymbolId(), f);
      continue;
    }
    if (auto* sf = dynamic_cast<SimpleFuncAST*>(s)) {
      record_function_def(sf->func_name->getAsStr(), sf->func_name->getSymbolId(), sf);
      continue;
    }
    if (auto* ex = dynamic_cast<ExternBlockAST*>(s)) {
      const auto signatures =
        styio::native::parse_function_signatures_for_block(ex->getBody(), ex->getSourcePaths());
      std::vector<std::string> block_symbols = ex->getExportedSymbols();
      const std::vector<std::string>& active_exported_symbols =
        block_symbols.empty() ? exported_symbols : block_symbols;
      const std::unordered_set<std::string> active_export_filter(
        active_exported_symbols.begin(),
        active_exported_symbols.end()
      );
      std::unordered_set<std::string> registered_block_symbols;
      for (const auto& sig : signatures) {
        if (!active_export_filter.empty() && active_export_filter.find(sig.name) == active_export_filter.end()) {
          continue;
        }
        NativeFunctionType native_type;
        native_type.return_type = styio::native::styio_data_type_for_c_type(sig.return_type);
        for (const auto& param : sig.params) {
          native_type.arg_types.push_back(styio::native::styio_data_type_for_c_type(param.type));
        }
        record_native_function_def(sig.name, styio::session::kInvalidSymbolId, native_type);
        registered_block_symbols.insert(sig.name);
      }
      for (const auto& symbol : block_symbols) {
        if (registered_block_symbols.find(symbol) == registered_block_symbols.end()) {
          throw StyioTypeError("@extern binding does not declare native function `" + symbol + "`");
        }
      }
    }
    if (auto* method = dynamic_cast<ResourceMethodDefAST*>(s)) {
      method->typeInfer(this);
    }
  }
  prepare_callable_type_schemes(ast);
  for (auto const& s : stmts) {
    if (dynamic_cast<ResourceMethodDefAST*>(s) != nullptr) {
      continue;
    }
    s->typeInfer(this);
  }
  styio::resource_topology::validate_or_throw(ast, "sema-resource-topology");
}
