#include "CallableInterface.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioToString/ToStringVisitor.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/SHA256.h"

namespace styio::sema {
namespace {

using CallableConstraintKind = StyioSemaContext::CallableConstraintKind;
using CallableEffectRowFacts = StyioSemaContext::CallableEffectRowFacts;
using CallableTypeConstraint = StyioSemaContext::CallableTypeConstraint;
using CallableTypeScheme = StyioSemaContext::CallableTypeScheme;
using CallableTypeTerm = StyioSemaContext::CallableTypeTerm;
using CallableUsageKind = styio::sema::CallableUsageKind;
using CallableUsageRequirement =
  StyioSemaContext::CallableUsageRequirement;

[[noreturn]] void
interface_error(const std::string& detail) {
  throw StyioTypeError("callable module interface is invalid: " + detail);
}

void
require_canonical_module_id(std::string_view module_id) {
  if (module_id.empty()
      || module_id.front() == '/'
      || module_id.back() == '/'
      || module_id.find('\\') != std::string_view::npos
      || module_id.find('.') != std::string_view::npos) {
    throw StyioTypeError(
      "callable module id must be a canonical non-empty slash-form path"
    );
  }
  std::size_t begin = 0;
  while (begin <= module_id.size()) {
    const std::size_t end = module_id.find('/', begin);
    const std::string_view segment =
      module_id.substr(
        begin,
        (end == std::string_view::npos
           ? module_id.size()
           : end) - begin);
    if (segment.empty() || segment == "." || segment == "..") {
      throw StyioTypeError(
        "callable module id may not contain empty, `.` or `..` segments"
      );
    }
    if (end == std::string_view::npos) {
      break;
    }
    begin = end + 1;
  }
}

std::string
constraint_kind_name(CallableConstraintKind kind) {
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
  interface_error("unknown callable constraint kind");
}

CallableConstraintKind
parse_constraint_kind(std::string_view name) {
  if (name == "numeric") {
    return CallableConstraintKind::Numeric;
  }
  if (name == "comparable") {
    return CallableConstraintKind::Comparable;
  }
  if (name == "indexable") {
    return CallableConstraintKind::Indexable;
  }
  if (name == "iterable") {
    return CallableConstraintKind::Iterable;
  }
  if (name == "cloneable") {
    return CallableConstraintKind::Cloneable;
  }
  interface_error("unknown callable constraint kind `" + std::string(name) + "`");
}

llvm::json::Object
term_to_json(const CallableTypeTerm& term) {
  llvm::json::Object object;
  switch (term.kind) {
    case CallableTypeTerm::Kind::Variable:
      object["kind"] = "variable";
      object["variable"] = static_cast<std::int64_t>(term.variable);
      break;
    case CallableTypeTerm::Kind::Concrete:
      object["kind"] = "concrete";
      object["type"] = term.concrete.name;
      break;
    case CallableTypeTerm::Kind::List:
      object["kind"] = "list";
      break;
    case CallableTypeTerm::Kind::Dict:
      object["kind"] = "dict";
      break;
  }

  if (!term.arguments.empty()) {
    llvm::json::Array arguments;
    for (const auto& argument : term.arguments) {
      arguments.push_back(term_to_json(argument));
    }
    object["arguments"] = std::move(arguments);
  }
  return object;
}

const llvm::json::Object&
require_object(
  const llvm::json::Value& value,
  const std::string& context
) {
  const auto* object = value.getAsObject();
  if (object == nullptr) {
    interface_error(context + " must be an object");
  }
  return *object;
}

const llvm::json::Array&
require_array(
  const llvm::json::Object& object,
  llvm::StringRef key,
  const std::string& context
) {
  const auto* array = object.getArray(key);
  if (array == nullptr) {
    interface_error(context + " is missing array `" + key.str() + "`");
  }
  return *array;
}

std::string
require_string(
  const llvm::json::Object& object,
  llvm::StringRef key,
  const std::string& context
) {
  const auto value = object.getString(key);
  if (!value.has_value()) {
    interface_error(context + " is missing string `" + key.str() + "`");
  }
  return std::string(*value);
}

bool
require_bool(
  const llvm::json::Object& object,
  llvm::StringRef key,
  const std::string& context
) {
  const auto value = object.getBoolean(key);
  if (!value.has_value()) {
    interface_error(context + " is missing boolean `" + key.str() + "`");
  }
  return *value;
}

std::int64_t
require_integer(
  const llvm::json::Object& object,
  llvm::StringRef key,
  const std::string& context
) {
  const auto value = object.getInteger(key);
  if (!value.has_value()) {
    interface_error(context + " is missing integer `" + key.str() + "`");
  }
  return *value;
}

CallableTypeTerm
term_from_json(
  const llvm::json::Value& value,
  const std::string& context
) {
  const auto& object = require_object(value, context);
  const std::string kind = require_string(object, "kind", context);
  CallableTypeTerm term;
  if (kind == "variable") {
    const std::int64_t variable =
      require_integer(object, "variable", context);
    if (variable < 0
        || static_cast<std::uint64_t>(variable)
             > static_cast<std::uint64_t>(
                 std::numeric_limits<std::uint32_t>::max())) {
      interface_error(context + " has an out-of-range type variable");
    }
    term.kind = CallableTypeTerm::Kind::Variable;
    term.variable = static_cast<std::uint32_t>(variable);
  }
  else if (kind == "concrete") {
    term.kind = CallableTypeTerm::Kind::Concrete;
    const std::string type_name =
      require_string(object, "type", context);
    term.concrete =
      type_name == "undefined"
        ? StyioDataType{
            StyioDataTypeOption::Undefined, "undefined", 0
          }
        : styio_data_type_from_name(type_name);
  }
  else if (kind == "list") {
    term.kind = CallableTypeTerm::Kind::List;
  }
  else if (kind == "dict") {
    term.kind = CallableTypeTerm::Kind::Dict;
  }
  else {
    interface_error(context + " has unknown term kind `" + kind + "`");
  }

  if (const auto* arguments = object.getArray("arguments")) {
    for (std::size_t i = 0; i < arguments->size(); ++i) {
      term.arguments.push_back(
        term_from_json(
          (*arguments)[i],
          context + ".arguments[" + std::to_string(i) + "]"));
    }
  }
  const std::size_t expected_arguments =
    term.kind == CallableTypeTerm::Kind::List
      ? 1
      : (term.kind == CallableTypeTerm::Kind::Dict ? 2 : 0);
  if (term.arguments.size() != expected_arguments) {
    interface_error(
      context + " has " + std::to_string(term.arguments.size())
      + " argument(s), expected " + std::to_string(expected_arguments));
  }
  return term;
}

bool
term_is_undefined(const CallableTypeTerm& term) {
  return term.kind == CallableTypeTerm::Kind::Concrete
         && term.concrete.isUndefined();
}

std::string
term_canonical_text(const CallableTypeTerm& term) {
  switch (term.kind) {
    case CallableTypeTerm::Kind::Variable:
      return "'" + std::to_string(term.variable);
    case CallableTypeTerm::Kind::Concrete:
      return term.concrete.name;
    case CallableTypeTerm::Kind::List:
      return "list["
             + term_canonical_text(term.arguments.at(0))
             + "]";
    case CallableTypeTerm::Kind::Dict:
      return "dict["
             + term_canonical_text(term.arguments.at(0))
             + ","
             + term_canonical_text(term.arguments.at(1))
             + "]";
  }
  interface_error("unknown callable relation term");
}

std::string
constraint_canonical_text(
  const CallableTypeConstraint& constraint
) {
  std::ostringstream output;
  output << constraint_kind_name(constraint.kind)
         << "(" << term_canonical_text(constraint.subject);
  if (constraint.kind == CallableConstraintKind::Indexable) {
    output << "," << term_canonical_text(constraint.argument)
           << "," << term_canonical_text(constraint.result);
  }
  else if (constraint.kind == CallableConstraintKind::Iterable) {
    output << "," << term_canonical_text(constraint.result);
  }
  output << ")";
  return output.str();
}

llvm::json::Object
constraint_to_json(const CallableTypeConstraint& constraint) {
  return llvm::json::Object{
    {"kind", constraint_kind_name(constraint.kind)},
    {"subject", term_to_json(constraint.subject)},
    {"argument", term_to_json(constraint.argument)},
    {"result", term_to_json(constraint.result)},
    {"canonical", constraint.canonical},
  };
}

CallableTypeConstraint
constraint_from_json(
  const llvm::json::Value& value,
  const std::string& context
) {
  const auto& object = require_object(value, context);
  CallableTypeConstraint constraint;
  constraint.kind =
    parse_constraint_kind(require_string(object, "kind", context));
  const auto* subject = object.get("subject");
  const auto* argument = object.get("argument");
  const auto* result = object.get("result");
  if (subject == nullptr || argument == nullptr || result == nullptr) {
    interface_error(context + " is missing a constraint term");
  }
  constraint.subject =
    term_from_json(*subject, context + ".subject");
  constraint.argument =
    term_from_json(*argument, context + ".argument");
  constraint.result =
    term_from_json(*result, context + ".result");
  constraint.canonical =
    require_string(object, "canonical", context);
  if (term_is_undefined(constraint.subject)) {
    interface_error(context + " has an undefined constraint subject");
  }
  if (constraint.kind == CallableConstraintKind::Indexable) {
    if (term_is_undefined(constraint.argument)
        || term_is_undefined(constraint.result)) {
      interface_error(
        context + " has an incomplete indexable constraint");
    }
  }
  else if (constraint.kind == CallableConstraintKind::Iterable) {
    if (!term_is_undefined(constraint.argument)
        || term_is_undefined(constraint.result)) {
      interface_error(
        context + " has an invalid iterable constraint shape");
    }
  }
  else if (!term_is_undefined(constraint.argument)
           || !term_is_undefined(constraint.result)) {
    interface_error(
      context + " has unexpected terms for a unary constraint");
  }
  if (constraint.canonical
      != constraint_canonical_text(constraint)) {
    interface_error(
      context
      + " canonical constraint does not match structured terms");
  }
  return constraint;
}

void
require_canonical_string_sequence(
  const std::vector<std::string>& values,
  const std::string& context
);

llvm::json::Object
usage_requirement_to_json(
  const CallableUsageRequirement& requirement
) {
  llvm::json::Array usages;
  for (const auto usage : requirement.usages.kinds()) {
    usages.push_back(
      std::string(
        styio::sema::callable_usage_kind_name(usage)));
  }
  return llvm::json::Object{
    {"variable", static_cast<std::int64_t>(requirement.variable)},
    {"usages", std::move(usages)},
  };
}

llvm::json::Object
scheme_to_json(const CallableTypeScheme& scheme) {
  llvm::json::Array params;
  for (const auto& param : scheme.params) {
    params.push_back(term_to_json(param));
  }
  llvm::json::Array constraints;
  for (const auto& constraint : scheme.constraints) {
    constraints.push_back(constraint_to_json(constraint));
  }
  llvm::json::Array quantified;
  for (std::uint32_t variable : scheme.quantified_variables) {
    quantified.push_back(static_cast<std::int64_t>(variable));
  }
  llvm::json::Array usage_requirements;
  for (const auto& requirement : scheme.usage_requirements) {
    usage_requirements.push_back(
      usage_requirement_to_json(requirement));
  }
  return llvm::json::Object{
    {"name", scheme.name},
    {"params", std::move(params)},
    {"result", term_to_json(scheme.result)},
    {"constraints", std::move(constraints)},
    {"usage_requirements", std::move(usage_requirements)},
    {"quantified_variables", std::move(quantified)},
    {"recursive_group", scheme.recursive_group},
    {"canonical_relation", scheme.canonical_relation},
  };
}

CallableTypeScheme
scheme_from_json(
  const llvm::json::Object& object,
  const std::string& context
) {
  CallableTypeScheme scheme;
  scheme.name = require_string(object, "name", context);
  const auto& params = require_array(object, "params", context);
  for (std::size_t i = 0; i < params.size(); ++i) {
    CallableTypeTerm param =
      term_from_json(
        params[i],
        context + ".params[" + std::to_string(i) + "]");
    if (term_is_undefined(param)) {
      interface_error(
        context + ".params[" + std::to_string(i)
        + "] has an undefined relation term");
    }
    scheme.params.push_back(std::move(param));
  }
  const auto* result = object.get("result");
  if (result == nullptr) {
    interface_error(context + " is missing result term");
  }
  scheme.result = term_from_json(*result, context + ".result");
  if (term_is_undefined(scheme.result)) {
    interface_error(context + ".result has an undefined relation term");
  }

  const auto& constraints = require_array(object, "constraints", context);
  for (std::size_t i = 0; i < constraints.size(); ++i) {
    scheme.constraints.push_back(
      constraint_from_json(
        constraints[i],
        context + ".constraints[" + std::to_string(i) + "]"));
  }
  const auto& quantified =
    require_array(object, "quantified_variables", context);
  for (std::size_t i = 0; i < quantified.size(); ++i) {
    const auto variable = quantified[i].getAsInteger();
    if (!variable.has_value()
        || *variable < 0
        || static_cast<std::uint64_t>(*variable)
             > static_cast<std::uint64_t>(
                 std::numeric_limits<std::uint32_t>::max())) {
      interface_error(
        context + ".quantified_variables["
        + std::to_string(i) + "] is invalid");
    }
    scheme.quantified_variables.push_back(
      static_cast<std::uint32_t>(*variable));
  }
  if (!std::is_sorted(
        scheme.quantified_variables.begin(),
        scheme.quantified_variables.end())
      || std::adjacent_find(
           scheme.quantified_variables.begin(),
           scheme.quantified_variables.end())
           != scheme.quantified_variables.end()) {
    interface_error(
      context
      + ".quantified_variables must be strictly sorted and deduplicated");
  }

  const auto& usage_requirements =
    require_array(object, "usage_requirements", context);
  std::optional<std::uint32_t> previous_usage_variable;
  for (std::size_t i = 0; i < usage_requirements.size(); ++i) {
    const std::string requirement_context =
      context + ".usage_requirements[" + std::to_string(i) + "]";
    const auto& requirement_object =
      require_object(
        usage_requirements[i],
        requirement_context);
    const std::int64_t variable =
      require_integer(
        requirement_object,
        "variable",
        requirement_context);
    if (variable < 0
        || static_cast<std::uint64_t>(variable)
             > static_cast<std::uint64_t>(
                 std::numeric_limits<std::uint32_t>::max())) {
      interface_error(
        requirement_context + ".variable is invalid");
    }
    CallableUsageRequirement requirement;
    requirement.variable =
      static_cast<std::uint32_t>(variable);
    if (previous_usage_variable.has_value()
        && requirement.variable <= *previous_usage_variable) {
      interface_error(
        context
        + ".usage_requirements must be strictly sorted and deduplicated");
    }
    previous_usage_variable = requirement.variable;
    if (!std::binary_search(
          scheme.quantified_variables.begin(),
          scheme.quantified_variables.end(),
          requirement.variable)) {
      interface_error(
        requirement_context
        + " references a non-quantified relation variable");
    }

    const auto& usages =
      require_array(
        requirement_object,
        "usages",
        requirement_context);
    std::vector<std::string> usage_names;
    usage_names.reserve(usages.size());
    for (std::size_t j = 0; j < usages.size(); ++j) {
      const auto usage = usages[j].getAsString();
      if (!usage.has_value()) {
        interface_error(
          requirement_context + ".usages["
          + std::to_string(j) + "] must be a string");
      }
      usage_names.emplace_back(*usage);
    }
    require_canonical_string_sequence(
      usage_names,
      requirement_context + ".usages");
    if (usage_names.empty()) {
      interface_error(
        requirement_context
        + ".usages must contain at least one usage fact");
    }
    for (const auto& name : usage_names) {
      CallableUsageKind usage;
      if (!styio::sema::callable_usage_kind_from_name(
            name,
            usage)) {
        interface_error(
          requirement_context
          + " has unknown usage fact `" + name + "`");
      }
      requirement.usages.add(usage);
    }
    scheme.usage_requirements.push_back(
      std::move(requirement));
  }
  scheme.recursive_group =
    require_bool(object, "recursive_group", context);
  scheme.canonical_relation =
    require_string(object, "canonical_relation", context);
  if (scheme.name.empty() || scheme.canonical_relation.empty()) {
    interface_error(context + " has an empty name or canonical relation");
  }
  std::ostringstream canonical_relation;
  if (!scheme.quantified_variables.empty()) {
    canonical_relation << "forall ";
    for (std::size_t i = 0;
         i < scheme.quantified_variables.size();
         ++i) {
      if (i != 0) {
        canonical_relation << ",";
      }
      canonical_relation << "'"
                         << scheme.quantified_variables[i];
    }
    canonical_relation << ". ";
  }
  canonical_relation << "(";
  for (std::size_t i = 0; i < scheme.params.size(); ++i) {
    if (i != 0) {
      canonical_relation << ",";
    }
    canonical_relation << term_canonical_text(scheme.params[i]);
  }
  canonical_relation << ")->"
                     << term_canonical_text(scheme.result);
  if (!scheme.constraints.empty()) {
    canonical_relation << " where ";
    for (std::size_t i = 0;
         i < scheme.constraints.size();
         ++i) {
      if (i != 0) {
        canonical_relation << ",";
      }
      canonical_relation << scheme.constraints[i].canonical;
    }
  }
  if (!scheme.usage_requirements.empty()) {
    canonical_relation << " using ";
    for (std::size_t i = 0;
         i < scheme.usage_requirements.size();
         ++i) {
      if (i != 0) {
        canonical_relation << ",";
      }
      canonical_relation
        << "usage('"
        << scheme.usage_requirements[i].variable
        << ":"
        << scheme.usage_requirements[i].usages.canonical()
        << ")";
    }
  }
  if (scheme.canonical_relation
      != canonical_relation.str()) {
    interface_error(
      context
      + " canonical relation does not match structured facts");
  }
  return scheme;
}

llvm::json::Array
string_array(const std::vector<std::string>& values) {
  llvm::json::Array array;
  for (const auto& value : values) {
    array.push_back(value);
  }
  return array;
}

std::vector<std::string>
strings_from_json(
  const llvm::json::Object& object,
  llvm::StringRef key,
  const std::string& context
) {
  const auto& array = require_array(object, key, context);
  std::vector<std::string> values;
  values.reserve(array.size());
  for (std::size_t i = 0; i < array.size(); ++i) {
    const auto value = array[i].getAsString();
    if (!value.has_value()) {
      interface_error(
        context + "." + key.str() + "["
        + std::to_string(i) + "] must be a string");
    }
    values.emplace_back(*value);
  }
  return values;
}

llvm::json::Object
effects_to_json(const CallableEffectRowFacts& effects) {
  llvm::json::Array labels;
  for (const auto label : effects.row.labels()) {
    labels.push_back(
      std::string(styio::sema::callable_effect_label_name(label)));
  }
  llvm::json::Object object{
    {"labels", std::move(labels)},
    {"relation_seed", effects.relation_seed},
    {"captures", string_array(effects.captures)},
    {"direct_callees", string_array(effects.direct_callees)},
  };
  if (effects.row.open_tail().has_value()) {
    object["open_tail"] =
      static_cast<std::int64_t>(*effects.row.open_tail());
  }
  else {
    object["open_tail"] = nullptr;
  }
  return object;
}

void
require_canonical_string_sequence(
  const std::vector<std::string>& values,
  const std::string& context
) {
  if (!std::is_sorted(values.begin(), values.end())
      || std::adjacent_find(values.begin(), values.end())
           != values.end()) {
    interface_error(
      context + " must be strictly sorted and deduplicated");
  }
}

CallableEffectRowFacts
effects_from_json(
  const llvm::json::Object& object,
  const std::string& context
) {
  CallableEffectRowFacts effects;
  const std::vector<std::string> labels =
    strings_from_json(object, "labels", context);
  require_canonical_string_sequence(labels, context + ".labels");
  for (const auto& name : labels) {
    const auto label =
      styio::sema::callable_effect_label_from_name(name);
    if (!label.has_value()) {
      interface_error(
        context + " has unknown effect label `" + name + "`");
    }
    effects.row.add(*label);
  }

  const llvm::json::Value* open_tail = object.get("open_tail");
  if (open_tail == nullptr) {
    interface_error(context + " is missing `open_tail`");
  }
  if (!open_tail->getAsNull().has_value()) {
    const auto tail = open_tail->getAsInteger();
    if (!tail.has_value()
        || *tail < 0
        || static_cast<std::uint64_t>(*tail)
             > static_cast<std::uint64_t>(
                 std::numeric_limits<std::uint32_t>::max())) {
      interface_error(context + " has an invalid open effect tail");
    }
    effects.row.set_open_tail(static_cast<std::uint32_t>(*tail));
  }

  effects.relation_seed =
    require_bool(object, "relation_seed", context);
  effects.captures =
    strings_from_json(object, "captures", context);
  effects.direct_callees =
    strings_from_json(object, "direct_callees", context);
  require_canonical_string_sequence(
    effects.captures,
    context + ".captures");
  require_canonical_string_sequence(
    effects.direct_callees,
    context + ".direct_callees");
  return effects;
}

std::vector<ParamAST*>
params_of_definition(StyioAST* definition) {
  if (auto* function = dynamic_cast<FunctionAST*>(definition)) {
    return function->params;
  }
  if (auto* function = dynamic_cast<SimpleFuncAST*>(definition)) {
    return function->params;
  }
  return {};
}

StyioDataType
declared_result_of_definition(
  StyioAST* definition,
  const StyioSemaContext& context,
  const std::string& name
) {
  const auto read = [](const auto& variant) -> StyioDataType
  {
    if (variant.valueless_by_exception()
        || !std::holds_alternative<TypeAST*>(variant)) {
      return StyioDataType{
        StyioDataTypeOption::Undefined, "undefined", 0
      };
    }
    TypeAST* type = std::get<TypeAST*>(variant);
    return type == nullptr
             ? StyioDataType{
                 StyioDataTypeOption::Undefined, "undefined", 0
               }
             : type->getDataType();
  };

  StyioDataType result{
    StyioDataTypeOption::Undefined, "undefined", 0
  };
  if (auto* function = dynamic_cast<FunctionAST*>(definition)) {
    result = read(function->ret_type);
  }
  else if (auto* function = dynamic_cast<SimpleFuncAST*>(definition)) {
    result = read(function->ret_type);
  }
  if (result.isUndefined()) {
    result = context.inferred_function_return_type(name);
  }
  return result;
}

std::string
entry_signature_text(const CallableInterfaceEntry& entry) {
  std::ostringstream output;
  output << entry.name << "|exported=" << (entry.exported ? "1" : "0");
  if (entry.has_scheme) {
    output << "|scheme=" << entry.scheme.canonical_relation;
  }
  else {
    output << "|concrete=(";
    for (std::size_t i = 0; i < entry.concrete_params.size(); ++i) {
      if (i != 0) {
        output << ",";
      }
      output << entry.concrete_params[i].name;
    }
    output << ")->" << entry.concrete_result.name;
  }
  output << "|effects=" << entry.effects.row.canonical()
         << "|body=" << entry.checked_body_digest;
  return output.str();
}

std::vector<std::string>
sorted_dependency_modules(
  const std::vector<const CallableModuleInterface*>& dependencies
) {
  std::vector<std::string> modules;
  modules.reserve(dependencies.size());
  for (const auto* dependency : dependencies) {
    if (dependency == nullptr) {
      interface_error("dependency list contains a null interface");
    }
    modules.push_back(dependency->module_id);
  }
  std::sort(modules.begin(), modules.end());
  if (std::adjacent_find(modules.begin(), modules.end()) != modules.end()) {
    interface_error("dependency list contains a duplicate module");
  }
  return modules;
}

}  // namespace

std::string
callable_interface_sha256_hex(std::string_view text) {
  llvm::SHA256 hasher;
  hasher.update(llvm::StringRef(text.data(), text.size()));
  const auto digest = hasher.final();
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (std::uint8_t byte : digest) {
    output << std::setw(2) << static_cast<unsigned int>(byte);
  }
  return output.str();
}

std::string
callable_interface_dependency_digest(
  const std::vector<const CallableModuleInterface*>& dependencies
) {
  std::vector<const CallableModuleInterface*> ordered = dependencies;
  std::sort(
    ordered.begin(),
    ordered.end(),
    [](const auto* lhs, const auto* rhs)
    {
      if (lhs == nullptr || rhs == nullptr) {
        return lhs < rhs;
      }
      return lhs->module_id < rhs->module_id;
    });
  std::ostringstream canonical;
  canonical << "styio.callable-interface.dependencies.v3\n";
  std::string previous;
  for (const auto* dependency : ordered) {
    if (dependency == nullptr) {
      interface_error("dependency list contains a null interface");
    }
    if (!previous.empty() && previous == dependency->module_id) {
      interface_error("dependency list contains a duplicate module");
    }
    previous = dependency->module_id;
    canonical << dependency->module_id << "\n"
              << dependency->abi_digest << "\n"
              << dependency->dependency_digest << "\n";
  }
  return callable_interface_sha256_hex(canonical.str());
}

std::string
callable_interface_abi_digest(
  const CallableModuleInterface& interface
) {
  std::vector<const CallableInterfaceEntry*> ordered;
  ordered.reserve(interface.entries.size());
  for (const auto& entry : interface.entries) {
    ordered.push_back(&entry);
  }
  std::sort(
    ordered.begin(),
    ordered.end(),
    [](const auto* lhs, const auto* rhs)
    {
      return lhs->name < rhs->name;
    });

  std::ostringstream canonical;
  canonical << "styio.callable-interface.abi.v3\n"
            << interface.module_id << "\n"
            << interface.compiler_abi << "\n"
            << interface.dependency_digest << "\n";
  std::string previous;
  for (const auto* entry : ordered) {
    if (!previous.empty() && previous == entry->name) {
      interface_error("interface contains duplicate callable `" + entry->name + "`");
    }
    previous = entry->name;
    canonical << entry_signature_text(*entry) << "\n";
  }
  return callable_interface_sha256_hex(canonical.str());
}

CallableModuleInterface
publish_callable_module_interface(
  std::string module_id,
  std::string_view source_text,
  std::string compiler_abi,
  MainBlockAST* ast,
  const StyioSemaContext& context,
  const std::vector<const CallableModuleInterface*>& dependencies
) {
  require_canonical_module_id(module_id);
  if (compiler_abi.empty()) {
    throw StyioTypeError("module interface publication requires compiler ABI facts");
  }
  if (ast == nullptr) {
    throw StyioTypeError("module interface publication requires a checked module AST");
  }

  CallableModuleInterface interface;
  interface.module_id = std::move(module_id);
  interface.compiler_abi = std::move(compiler_abi);
  interface.source_digest = callable_interface_sha256_hex(source_text);
  interface.dependency_modules = sorted_dependency_modules(dependencies);
  interface.dependency_digest =
    callable_interface_dependency_digest(dependencies);

  std::unordered_set<std::string> exported_names;
  std::unordered_map<std::string, StyioAST*> definitions;
  for (auto* statement : ast->getStmts()) {
    if (auto* exports = dynamic_cast<ExportDeclAST*>(statement)) {
      for (const auto& symbol : exports->getSymbols()) {
        exported_names.insert(symbol);
      }
      continue;
    }
    if (auto* function = dynamic_cast<FunctionAST*>(statement)) {
      definitions[function->getNameAsStr()] = function;
      continue;
    }
    if (auto* function = dynamic_cast<SimpleFuncAST*>(statement)) {
      definitions[function->func_name->getAsStr()] = function;
    }
  }

  std::vector<std::string> local_scheme_names;
  for (const auto& [name, scheme] : context.callable_type_scheme_facts()) {
    (void)scheme;
    if (definitions.count(name) != 0) {
      local_scheme_names.push_back(name);
    }
  }
  std::sort(local_scheme_names.begin(), local_scheme_names.end());

  StyioRepr representation;
  std::unordered_set<std::string> published_names;
  for (const auto& name : local_scheme_names) {
    CallableInterfaceEntry entry;
    entry.name = name;
    entry.exported = exported_names.count(name) != 0;
    entry.has_scheme = true;
    entry.scheme = *context.find_callable_type_scheme(name);
    if (const auto* effects = context.find_callable_effect_row(name)) {
      entry.effects = *effects;
    }
    entry.checked_body = definitions.at(name)->toString(&representation);
    entry.checked_body_digest =
      callable_interface_sha256_hex(entry.checked_body);
    interface.entries.push_back(std::move(entry));
    published_names.insert(name);
  }

  std::vector<std::string> concrete_definitions;
  for (const auto& [name, definition] : definitions) {
    (void)definition;
    if (published_names.count(name) == 0) {
      concrete_definitions.push_back(name);
    }
  }
  std::sort(
    concrete_definitions.begin(),
    concrete_definitions.end());
  for (const auto& name : concrete_definitions) {
    StyioAST* definition = definitions.at(name);
    CallableInterfaceEntry entry;
    entry.name = name;
    entry.exported = exported_names.count(name) != 0;
    for (auto* param : params_of_definition(definition)) {
      const StyioDataType type = param->getDType()->getDataType();
      if (type.isUndefined()) {
        throw StyioTypeError(
          std::string(
            entry.exported
              ? "concrete exported callable `"
              : "private concrete callable dependency `")
          + name
          + "` requires a concrete parameter interface");
      }
      entry.concrete_params.push_back(type);
    }
    entry.concrete_result =
      declared_result_of_definition(definition, context, name);
    if (entry.concrete_result.isUndefined()) {
      throw StyioTypeError(
        std::string(
          entry.exported
            ? "concrete exported callable `"
            : "private concrete callable dependency `")
        + name
        + "` requires a concrete result interface");
    }
    if (const auto* effects = context.find_callable_effect_row(name)) {
      entry.effects = *effects;
    }
    else {
      entry.effects.row =
        styio::sema::CallableEffectRow::unknown();
    }
    entry.checked_body = definition->toString(&representation);
    entry.checked_body_digest =
      callable_interface_sha256_hex(entry.checked_body);
    interface.entries.push_back(std::move(entry));
    published_names.insert(name);
  }

  for (const auto& name : exported_names) {
    if (published_names.count(name) != 0
        || context.find_native_function_def(
             styio::session::kInvalidSymbolId,
             name) != nullptr) {
      continue;
    }
    throw StyioTypeError(
      "module interface export `" + name
      + "` does not name a checked callable");
  }

  std::sort(
    interface.entries.begin(),
    interface.entries.end(),
    [](const auto& lhs, const auto& rhs)
    {
      return lhs.name < rhs.name;
    });
  interface.abi_digest = callable_interface_abi_digest(interface);
  return interface;
}

std::string
serialize_callable_module_interface(
  const CallableModuleInterface& interface
) {
  llvm::json::Array entries;
  for (const auto& entry : interface.entries) {
    llvm::json::Object object{
      {"name", entry.name},
      {"exported", entry.exported},
      {"has_scheme", entry.has_scheme},
      {"effects", effects_to_json(entry.effects)},
      {"checked_body", entry.checked_body},
      {"checked_body_digest", entry.checked_body_digest},
    };
    if (entry.has_scheme) {
      object["scheme"] = scheme_to_json(entry.scheme);
    }
    else {
      llvm::json::Array params;
      for (const auto& param : entry.concrete_params) {
        params.push_back(param.name);
      }
      object["concrete"] = llvm::json::Object{
        {"params", std::move(params)},
        {"result", entry.concrete_result.name},
      };
    }
    entries.push_back(std::move(object));
  }

  llvm::json::Object root{
    {"format", std::string(kCallableInterfaceFormat)},
    {"schema_version", interface.schema_version},
    {"module_id", interface.module_id},
    {"compiler_abi", interface.compiler_abi},
    {"source_digest", interface.source_digest},
    {"dependency_modules", string_array(interface.dependency_modules)},
    {"dependency_digest", interface.dependency_digest},
    {"abi_digest", interface.abi_digest},
    {"entries", std::move(entries)},
  };
  return llvm::formatv(
    "{0:2}",
    llvm::json::Value(std::move(root))).str() + "\n";
}

CallableModuleInterface
parse_callable_module_interface(
  std::string_view payload,
  std::string_view expected_module_id,
  std::string_view expected_source_text,
  std::string_view expected_compiler_abi,
  const std::vector<const CallableModuleInterface*>& dependencies
) {
  auto parsed = llvm::json::parse(
    llvm::StringRef(payload.data(), payload.size()));
  if (!parsed) {
    interface_error(
      "JSON parse failed: " + llvm::toString(parsed.takeError()));
  }
  const auto* root = parsed->getAsObject();
  if (root == nullptr) {
    interface_error("root must be an object");
  }
  const std::string format =
    require_string(*root, "format", "interface");
  if (format != kCallableInterfaceFormat) {
    interface_error("unsupported format `" + format + "`");
  }

  CallableModuleInterface interface;
  interface.schema_version =
    require_integer(*root, "schema_version", "interface");
  if (interface.schema_version != kCallableInterfaceSchemaVersion) {
    interface_error(
      "unsupported schema version "
      + std::to_string(interface.schema_version)
      + "; expected "
      + std::to_string(kCallableInterfaceSchemaVersion));
  }
  interface.module_id =
    require_string(*root, "module_id", "interface");
  interface.compiler_abi =
    require_string(*root, "compiler_abi", "interface");
  interface.source_digest =
    require_string(*root, "source_digest", "interface");
  interface.dependency_modules =
    strings_from_json(*root, "dependency_modules", "interface");
  interface.dependency_digest =
    require_string(*root, "dependency_digest", "interface");
  interface.abi_digest =
    require_string(*root, "abi_digest", "interface");

  if (interface.module_id != expected_module_id) {
    interface_error(
      "module id mismatch: expected `" + std::string(expected_module_id)
      + "`, found `" + interface.module_id + "`");
  }
  if (interface.compiler_abi != expected_compiler_abi) {
    interface_error(
      "compiler ABI mismatch for module `" + interface.module_id
      + "`; rebuild its .styioi interface");
  }
  const std::string expected_source_digest =
    callable_interface_sha256_hex(expected_source_text);
  if (interface.source_digest != expected_source_digest) {
    interface_error(
      "source digest mismatch for module `" + interface.module_id
      + "`; rebuild its .styioi interface");
  }

  const auto expected_modules =
    sorted_dependency_modules(dependencies);
  if (interface.dependency_modules != expected_modules) {
    interface_error(
      "dependency module set changed for module `" + interface.module_id
      + "`; rebuild its .styioi interface");
  }
  const std::string expected_dependency_digest =
    callable_interface_dependency_digest(dependencies);
  if (interface.dependency_digest != expected_dependency_digest) {
    interface_error(
      "dependency digest mismatch for module `" + interface.module_id
      + "`; rebuild its .styioi interface");
  }

  const auto& entries = require_array(*root, "entries", "interface");
  std::unordered_set<std::string> names;
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const std::string context =
      "interface.entries[" + std::to_string(i) + "]";
    const auto& object = require_object(entries[i], context);
    CallableInterfaceEntry entry;
    entry.name = require_string(object, "name", context);
    entry.exported = require_bool(object, "exported", context);
    entry.has_scheme = require_bool(object, "has_scheme", context);
    if (entry.name.empty() || !names.insert(entry.name).second) {
      interface_error(context + " has an empty or duplicate callable name");
    }
    const auto* effects = object.getObject("effects");
    if (effects == nullptr) {
      interface_error(context + " is missing effect row");
    }
    entry.effects =
      effects_from_json(*effects, context + ".effects");
    if (entry.has_scheme) {
      const auto* scheme = object.getObject("scheme");
      if (scheme == nullptr) {
        interface_error(context + " is missing callable scheme");
      }
      entry.scheme =
        scheme_from_json(*scheme, context + ".scheme");
      if (entry.scheme.name != entry.name) {
        interface_error(context + " scheme name does not match entry name");
      }
    }
    else {
      const auto* concrete = object.getObject("concrete");
      if (concrete == nullptr) {
        interface_error(context + " is missing concrete signature");
      }
      for (const auto& param :
           strings_from_json(*concrete, "params", context + ".concrete")) {
        StyioDataType type = styio_data_type_from_name(param);
        if (type.isUndefined()) {
          interface_error(context + " has an undefined concrete parameter");
        }
        entry.concrete_params.push_back(std::move(type));
      }
      entry.concrete_result =
        styio_data_type_from_name(
          require_string(*concrete, "result", context + ".concrete"));
      if (entry.concrete_result.isUndefined()) {
        interface_error(context + " has an undefined concrete result");
      }
    }
    entry.checked_body =
      require_string(object, "checked_body", context);
    entry.checked_body_digest =
      require_string(object, "checked_body_digest", context);
    if (entry.checked_body.empty()
        || entry.checked_body_digest
             != callable_interface_sha256_hex(entry.checked_body)) {
      interface_error(context + " checked body digest does not match");
    }
    interface.entries.push_back(std::move(entry));
  }

  std::sort(
    interface.entries.begin(),
    interface.entries.end(),
    [](const auto& lhs, const auto& rhs)
    {
      return lhs.name < rhs.name;
    });
  const std::string expected_abi_digest =
    callable_interface_abi_digest(interface);
  if (interface.abi_digest != expected_abi_digest) {
    interface_error(
      "ABI digest mismatch for module `" + interface.module_id
      + "`; rebuild its .styioi interface");
  }
  return interface;
}

}  // namespace styio::sema
