#include "PortableCallableBodyLowering.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"

namespace styio::ir {
namespace {

using Term = PortableCallableTypeTerm;

std::string
binary_operation_name(StyioOpType operation) {
  switch (operation) {
    case StyioOpType::Binary_Add:
      return "add";
    case StyioOpType::Binary_Sub:
      return "sub";
    case StyioOpType::Binary_Mul:
      return "mul";
    case StyioOpType::Binary_Div:
      return "div";
    case StyioOpType::Binary_Pow:
      return "pow";
    case StyioOpType::Binary_Mod:
      return "mod";
    default:
      throw StyioTypeError(
        "portable StyioIR does not support this binary operation");
  }
}

StyioOpType
binary_operation_from_name(const std::string& operation) {
  if (operation == "add") {
    return StyioOpType::Binary_Add;
  }
  if (operation == "sub") {
    return StyioOpType::Binary_Sub;
  }
  if (operation == "mul") {
    return StyioOpType::Binary_Mul;
  }
  if (operation == "div") {
    return StyioOpType::Binary_Div;
  }
  if (operation == "pow") {
    return StyioOpType::Binary_Pow;
  }
  if (operation == "mod") {
    return StyioOpType::Binary_Mod;
  }
  throw StyioTypeError(
    "portable StyioIR has unknown binary operation `" + operation + "`");
}

std::string
comparison_operation_name(CompType operation) {
  switch (operation) {
    case CompType::EQ:
      return "eq";
    case CompType::GT:
      return "gt";
    case CompType::GE:
      return "ge";
    case CompType::LT:
      return "lt";
    case CompType::LE:
      return "le";
    case CompType::NE:
      return "ne";
  }
  throw StyioTypeError(
    "portable StyioIR does not support this comparison operation");
}

CompType
comparison_operation_from_name(const std::string& operation) {
  if (operation == "eq") {
    return CompType::EQ;
  }
  if (operation == "gt") {
    return CompType::GT;
  }
  if (operation == "ge") {
    return CompType::GE;
  }
  if (operation == "lt") {
    return CompType::LT;
  }
  if (operation == "le") {
    return CompType::LE;
  }
  if (operation == "ne") {
    return CompType::NE;
  }
  throw StyioTypeError(
    "portable StyioIR has unknown comparison operation `"
    + operation + "`");
}

std::string
logic_operation_name(LogicType operation) {
  switch (operation) {
    case LogicType::RAW:
      return "raw";
    case LogicType::NOT:
      return "not";
    case LogicType::AND:
      return "and";
    case LogicType::OR:
      return "or";
    case LogicType::XOR:
      return "xor";
  }
  throw StyioTypeError(
    "portable StyioIR does not support this logical operation");
}

LogicType
logic_operation_from_name(const std::string& operation) {
  if (operation == "raw") {
    return LogicType::RAW;
  }
  if (operation == "not") {
    return LogicType::NOT;
  }
  if (operation == "and") {
    return LogicType::AND;
  }
  if (operation == "or") {
    return LogicType::OR;
  }
  if (operation == "xor") {
    return LogicType::XOR;
  }
  throw StyioTypeError(
    "portable StyioIR has unknown logical operation `" + operation + "`");
}

class PortableBodyBuilder
{
  PortableCallableBody body_;

  std::uint32_t
  append(PortableCallableNode node) {
    if (body_.nodes.size() >= kMaximumPortableCallableNodes) {
      throw StyioTypeError(
        "portable StyioIR body exceeds the supported node limit");
    }
    body_.nodes.push_back(std::move(node));
    return static_cast<std::uint32_t>(body_.nodes.size() - 1);
  }

  std::uint32_t
  build_node(StyioAST* ast) {
    if (ast == nullptr) {
      throw StyioTypeError(
        "portable StyioIR publication reached a null AST node");
    }

    if (auto* name = dynamic_cast<NameAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "load";
      node.symbol = name->getAsStr();
      return append(std::move(node));
    }
    if (auto* value = dynamic_cast<BoolAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "bool";
      node.value = value->getValue() ? "true" : "false";
      return append(std::move(node));
    }
    if (auto* value = dynamic_cast<IntAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "i64";
      node.value = value->getValue();
      return append(std::move(node));
    }
    if (auto* value = dynamic_cast<FloatAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "f64";
      node.value = value->getValue();
      return append(std::move(node));
    }
    if (auto* value = dynamic_cast<CharAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "char";
      node.value = value->getValue();
      return append(std::move(node));
    }
    if (auto* value = dynamic_cast<StringAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "string";
      node.value = value->getValue();
      return append(std::move(node));
    }
    if (auto* operation = dynamic_cast<BinOpAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "binary";
      node.inputs = {
        build_node(operation->getLHS()),
        build_node(operation->getRHS()),
      };
      node.operation = binary_operation_name(operation->getOp());
      return append(std::move(node));
    }
    if (auto* comparison = dynamic_cast<BinCompAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "compare";
      node.inputs = {
        build_node(comparison->getLHS()),
        build_node(comparison->getRHS()),
      };
      node.operation = comparison_operation_name(comparison->getSign());
      return append(std::move(node));
    }
    if (auto* condition = dynamic_cast<CondAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "logic";
      node.operation = logic_operation_name(condition->getSign());
      if (condition->getValue() != nullptr) {
        node.inputs.push_back(build_node(condition->getValue()));
      }
      else {
        node.inputs.push_back(build_node(condition->getLHS()));
        node.inputs.push_back(build_node(condition->getRHS()));
      }
      return append(std::move(node));
    }
    if (auto* call = dynamic_cast<FuncCallAST*>(ast)) {
      PortableCallableNode node;
      node.opcode =
        call->isIndirectCallableCall() ? "indirect_call" : "call";
      node.symbol = call->getNameAsStr();
      if (call->isIndirectCallableCall()
          && call->func_callee != nullptr) {
        if (auto* callee =
              dynamic_cast<NameAST*>(call->func_callee)) {
          node.symbol = callee->getAsStr();
        }
      }
      for (auto* argument : call->getArgList()) {
        node.inputs.push_back(build_node(argument));
      }
      return append(std::move(node));
    }
    if (auto* list = dynamic_cast<ListAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "list";
      for (auto* element : list->getElements()) {
        node.inputs.push_back(build_node(element));
      }
      return append(std::move(node));
    }
    if (auto* dict = dynamic_cast<DictAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "dict";
      for (const auto& entry : dict->getEntries()) {
        node.inputs.push_back(build_node(entry.key));
        node.inputs.push_back(build_node(entry.value));
      }
      return append(std::move(node));
    }
    if (auto* access = dynamic_cast<ListOpAST*>(ast)) {
      if (access->getOp() != StyioNodeType::Access_By_Index
          && access->getOp() != StyioNodeType::Access) {
        throw StyioTypeError(
          "portable StyioIR only supports indexed collection access");
      }
      PortableCallableNode node;
      node.opcode = "index";
      node.inputs = {
        build_node(access->getList()),
        build_node(access->getSlot1()),
      };
      return append(std::move(node));
    }
    if (auto* block = dynamic_cast<BlockAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "block";
      for (auto* statement : block->stmts) {
        node.inputs.push_back(build_node(statement));
      }
      return append(std::move(node));
    }
    if (auto* returned = dynamic_cast<ReturnAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "return";
      node.inputs = {build_node(returned->getExpr())};
      return append(std::move(node));
    }
    if (auto* printed = dynamic_cast<PrintAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "print";
      for (auto* expression : printed->exprs) {
        node.inputs.push_back(build_node(expression));
      }
      return append(std::move(node));
    }
    if (auto* binding = dynamic_cast<FinalBindAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "final_bind";
      node.symbol = binding->getName();
      node.inputs = {build_node(binding->getValue())};
      return append(std::move(node));
    }
    if (auto* binding = dynamic_cast<FlexBindAST*>(ast)) {
      PortableCallableNode node;
      node.opcode = "flex_bind";
      node.symbol = binding->getNameAsStr();
      node.inputs = {build_node(binding->getValue())};
      return append(std::move(node));
    }
    if (dynamic_cast<PassAST*>(ast) != nullptr) {
      PortableCallableNode node;
      node.opcode = "pass";
      return append(std::move(node));
    }

    throw StyioTypeError(
      "portable StyioIR publication for `" + body_.name
      + "` encountered an unsupported checked AST node");
  }

public:
  PortableCallableBody
  build(
    StyioAST* definition,
    const PortableCallableSignature& signature
  ) {
    body_.name = signature.name;
    body_.result = signature.result;

    std::vector<ParamAST*> params;
    StyioAST* root = nullptr;
    if (auto* function = dynamic_cast<FunctionAST*>(definition)) {
      if (!function->getCaptureNames().empty()) {
        throw StyioTypeError(
          "portable StyioIR does not yet encode closure environments for `"
          + signature.name + "`");
      }
      if (function->getNameAsStr() != signature.name) {
        throw StyioTypeError(
          "portable StyioIR signature name does not match body");
      }
      params = function->params;
      root = function->func_body;
      body_.is_block = true;
      body_.is_unique = function->is_unique;
    }
    else if (auto* function =
               dynamic_cast<SimpleFuncAST*>(definition)) {
      if (!function->getCaptureNames().empty()) {
        throw StyioTypeError(
          "portable StyioIR does not yet encode closure environments for `"
          + signature.name + "`");
      }
      if (function->func_name == nullptr
          || function->func_name->getAsStr() != signature.name) {
        throw StyioTypeError(
          "portable StyioIR signature name does not match body");
      }
      params = function->params;
      root = function->ret_expr;
      body_.is_block = false;
      body_.is_unique = function->is_unique;
    }
    else {
      throw StyioTypeError(
        "portable StyioIR publication requires a checked function body");
    }

    if (params.size() != signature.params.size()) {
      throw StyioTypeError(
        "portable StyioIR parameter count does not match callable signature");
    }
    body_.params.reserve(params.size());
    for (std::size_t i = 0; i < params.size(); ++i) {
      body_.params.push_back(
        PortableCallableParameter{
          params[i]->getName(),
          signature.params[i],
        });
    }
    body_.root = build_node(root);
    return std::move(body_);
  }
};

TypeAST*
type_ast_for_term(const Term& term) {
  switch (term.kind) {
    case Term::Kind::Variable:
      return TypeAST::Create();
    case Term::Kind::Concrete:
      return term.concrete.isUndefined()
               ? TypeAST::Create()
               : TypeAST::Create(term.concrete);
    case Term::Kind::List: {
      TypeAST* element = type_ast_for_term(term.arguments.at(0));
      const StyioDataType element_type = element->getDataType();
      delete element;
      return element_type.isUndefined()
               ? TypeAST::Create()
               : TypeAST::Create(
                   styio_make_list_type(element_type.name));
    }
    case Term::Kind::Dict: {
      TypeAST* key = type_ast_for_term(term.arguments.at(0));
      TypeAST* value = type_ast_for_term(term.arguments.at(1));
      const StyioDataType key_type = key->getDataType();
      const StyioDataType value_type = value->getDataType();
      delete key;
      delete value;
      return key_type.isUndefined() || value_type.isUndefined()
               ? TypeAST::Create()
               : TypeAST::Create(
                   styio_make_dict_type(
                     key_type.name,
                     value_type.name));
    }
  }
  return TypeAST::Create();
}

class PortableBodyMaterializer
{
  const PortableCallableBody& body_;
  std::vector<std::unique_ptr<StyioAST>> nodes_;

  StyioAST*
  take(std::uint32_t index) {
    if (index >= nodes_.size() || nodes_[index] == nullptr) {
      throw StyioTypeError(
        "portable StyioIR materialization found an invalid node reference");
    }
    return nodes_[index].release();
  }

  std::vector<StyioAST*>
  take_all(const std::vector<std::uint32_t>& inputs) {
    std::vector<StyioAST*> values;
    values.reserve(inputs.size());
    for (const auto input : inputs) {
      values.push_back(take(input));
    }
    return values;
  }

public:
  explicit PortableBodyMaterializer(
    const PortableCallableBody& body
  ) :
      body_(body) {
  }

  std::unique_ptr<StyioAST>
  materialize() {
    nodes_.resize(body_.nodes.size());
    for (std::size_t i = 0; i < body_.nodes.size(); ++i) {
      const auto& node = body_.nodes[i];
      StyioAST* value = nullptr;
      if (node.opcode == "load") {
        value = NameAST::Create(node.symbol);
      }
      else if (node.opcode == "bool") {
        value = BoolAST::Create(node.value == "true");
      }
      else if (node.opcode == "i64") {
        value = IntAST::Create(node.value, 64);
      }
      else if (node.opcode == "f64") {
        value = FloatAST::Create(node.value);
      }
      else if (node.opcode == "char") {
        value = CharAST::Create(node.value);
      }
      else if (node.opcode == "string") {
        value = StringAST::Create(node.value);
      }
      else if (node.opcode == "binary") {
        value = BinOpAST::Create(
          binary_operation_from_name(node.operation),
          take(node.inputs.at(0)),
          take(node.inputs.at(1)));
      }
      else if (node.opcode == "compare") {
        value = new BinCompAST(
          comparison_operation_from_name(node.operation),
          take(node.inputs.at(0)),
          take(node.inputs.at(1)));
      }
      else if (node.opcode == "logic") {
        const LogicType operation =
          logic_operation_from_name(node.operation);
        value =
          node.inputs.size() == 1
            ? static_cast<StyioAST*>(
                CondAST::Create(operation, take(node.inputs.at(0))))
            : static_cast<StyioAST*>(
                CondAST::Create(
                  operation,
                  take(node.inputs.at(0)),
                  take(node.inputs.at(1))));
      }
      else if (node.opcode == "call"
               || node.opcode == "indirect_call") {
        value = FuncCallAST::Create(
          NameAST::Create(node.symbol),
          take_all(node.inputs));
      }
      else if (node.opcode == "list") {
        value = ListAST::Create(take_all(node.inputs));
      }
      else if (node.opcode == "dict") {
        std::vector<std::pair<StyioAST*, StyioAST*>> entries;
        for (std::size_t input = 0;
             input < node.inputs.size();
             input += 2) {
          entries.emplace_back(
            take(node.inputs[input]),
            take(node.inputs[input + 1]));
        }
        value = DictAST::Create(std::move(entries));
      }
      else if (node.opcode == "index") {
        value = new ListOpAST(
          StyioNodeType::Access_By_Index,
          take(node.inputs.at(0)),
          take(node.inputs.at(1)));
      }
      else if (node.opcode == "block") {
        value = BlockAST::Create(take_all(node.inputs));
      }
      else if (node.opcode == "return") {
        value = ReturnAST::Create(take(node.inputs.at(0)));
      }
      else if (node.opcode == "print") {
        value = PrintAST::Create(take_all(node.inputs));
      }
      else if (node.opcode == "final_bind"
               || node.opcode == "flex_bind") {
        VarAST* variable = VarAST::Create(
          NameAST::Create(node.symbol),
          type_ast_for_term(
            body_.nodes.at(node.inputs.at(0)).type));
        value =
          node.opcode == "final_bind"
            ? static_cast<StyioAST*>(
                FinalBindAST::Create(
                  variable,
                  take(node.inputs.at(0))))
            : static_cast<StyioAST*>(
                FlexBindAST::Create(
                  variable,
                  take(node.inputs.at(0))));
      }
      else if (node.opcode == "pass") {
        value = PassAST::Create();
      }
      else {
        throw StyioTypeError(
          "portable StyioIR materialization encountered unknown opcode `"
          + node.opcode + "`");
      }
      nodes_[i].reset(value);
    }

    std::vector<ParamAST*> params;
    params.reserve(body_.params.size());
    for (const auto& param : body_.params) {
      params.push_back(
        ParamAST::Create(
          NameAST::Create(param.name),
          type_ast_for_term(param.type)));
    }
    StyioAST* root = take(body_.root);
    TypeAST* result_type = type_ast_for_term(body_.result);
    StyioAST* function =
      body_.is_block
        ? static_cast<StyioAST*>(
            FunctionAST::Create(
              NameAST::Create(body_.name),
              body_.is_unique,
              std::move(params),
              result_type,
              root))
        : static_cast<StyioAST*>(
            SimpleFuncAST::Create(
              NameAST::Create(body_.name),
              body_.is_unique,
              std::move(params),
              result_type,
              root));
    return std::unique_ptr<StyioAST>(function);
  }
};

}  // namespace

PortableCallableBody
build_portable_callable_body(
  StyioAST* definition,
  const PortableCallableSignature& signature
) {
  PortableBodyBuilder builder;
  return builder.build(definition, signature);
}

std::unique_ptr<StyioAST>
materialize_portable_callable_body(
  const PortableCallableBody& body
) {
  PortableBodyMaterializer materializer(body);
  return materializer.materialize();
}

}  // namespace styio::ir
