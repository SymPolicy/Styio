#include "PortableCallableBody.hpp"

#include <sstream>

namespace styio::ir {
namespace {

void
append_semantic_field(
  std::ostringstream& output,
  std::string_view label,
  std::string_view value
) {
  output << label.size() << ":" << label
         << value.size() << ":" << value << "\n";
}

}  // namespace

PortableCallableTypeTerm
portable_callable_term_from_data_type(
  const StyioDataType& type
) {
  PortableCallableTypeTerm term;
  if (styio_is_list_type(type)) {
    term.kind = PortableCallableTypeTerm::Kind::List;
    term.arguments.push_back(
      portable_callable_term_from_data_type(
        styio_data_type_from_name(
          styio_list_elem_type_name(type))));
    return term;
  }
  if (styio_is_dict_type(type)) {
    term.kind = PortableCallableTypeTerm::Kind::Dict;
    term.arguments.push_back(
      portable_callable_term_from_data_type(
        styio_data_type_from_name(
          styio_dict_key_type_name(type))));
    term.arguments.push_back(
      portable_callable_term_from_data_type(
        styio_data_type_from_name(
          styio_dict_value_type_name(type))));
    return term;
  }
  term.kind = PortableCallableTypeTerm::Kind::Concrete;
  term.concrete =
    type.isUndefined()
      ? StyioDataType{
          StyioDataTypeOption::Undefined,
          "undefined",
          0}
      : styio_data_type_from_name(type.name);
  return term;
}

bool
portable_callable_terms_equal(
  const PortableCallableTypeTerm& lhs,
  const PortableCallableTypeTerm& rhs
) {
  if (lhs.kind != rhs.kind
      || lhs.variable != rhs.variable
      || lhs.arguments.size() != rhs.arguments.size()) {
    return false;
  }
  if (lhs.kind == PortableCallableTypeTerm::Kind::Concrete
      && lhs.concrete.name != rhs.concrete.name) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.arguments.size(); ++i) {
    if (!portable_callable_terms_equal(
          lhs.arguments[i],
          rhs.arguments[i])) {
      return false;
    }
  }
  return true;
}

std::string
portable_callable_term_canonical_text(
  const PortableCallableTypeTerm& term
) {
  switch (term.kind) {
    case PortableCallableTypeTerm::Kind::Variable:
      return "'" + std::to_string(term.variable);
    case PortableCallableTypeTerm::Kind::Concrete:
      return term.concrete.name;
    case PortableCallableTypeTerm::Kind::List:
      return "list["
             + portable_callable_term_canonical_text(
                 term.arguments.at(0))
             + "]";
    case PortableCallableTypeTerm::Kind::Dict:
      return "dict["
             + portable_callable_term_canonical_text(
                 term.arguments.at(0))
             + ","
             + portable_callable_term_canonical_text(
                 term.arguments.at(1))
             + "]";
  }
  return "undefined";
}

std::string
portable_callable_body_semantic_text(
  const PortableCallableBody& body
) {
  std::ostringstream output;
  output << "styio.portable-styioir.semantic.v1\n";
  append_semantic_field(output, "name", body.name);
  append_semantic_field(
    output,
    "kind",
    body.is_block ? "block" : "expression");
  append_semantic_field(
    output,
    "binding",
    body.is_unique ? "final" : "mutable");
  append_semantic_field(
    output,
    "result",
    portable_callable_term_canonical_text(body.result));
  output << "params=" << body.params.size() << "\n";
  for (const auto& param : body.params) {
    append_semantic_field(output, "param-name", param.name);
    append_semantic_field(
      output,
      "param-type",
      portable_callable_term_canonical_text(param.type));
  }
  output << "nodes=" << body.nodes.size() << "\n";
  for (std::size_t i = 0; i < body.nodes.size(); ++i) {
    const auto& node = body.nodes[i];
    output << "node=" << i << "\n";
    append_semantic_field(output, "opcode", node.opcode);
    append_semantic_field(output, "symbol", node.symbol);
    append_semantic_field(output, "value", node.value);
    append_semantic_field(output, "operation", node.operation);
    append_semantic_field(
      output,
      "type",
      portable_callable_term_canonical_text(node.type));
    output << "inputs=" << node.inputs.size() << "\n";
    for (const auto input : node.inputs) {
      output << input << "\n";
    }
  }
  output << "root=" << body.root << "\n";
  return output.str();
}

}  // namespace styio::ir
