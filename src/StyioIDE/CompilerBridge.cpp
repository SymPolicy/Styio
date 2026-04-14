#include "CompilerBridge.hpp"

#include <algorithm>
#include <sstream>
#include <variant>

#include "../StyioAST/AST.hpp"
#include "../StyioAnalyzer/ASTAnalyzer.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioParser/Parser.hpp"
#include "../StyioParser/Tokenizer.hpp"

namespace styio::ide {

namespace {

std::string
type_name_from_type(const StyioDataType& type) {
  if (!type.name.empty()) {
    return type.name;
  }
  return reprDataTypeOption(type.option);
}

std::string
type_name_from_variant(const std::variant<TypeAST*, TypeTupleAST*>& ret_type) {
  if (std::holds_alternative<TypeAST*>(ret_type)) {
    TypeAST* type = std::get<TypeAST*>(ret_type);
    return type == nullptr ? "undefined" : type->getTypeName();
  }

  TypeTupleAST* type_tuple = std::get<TypeTupleAST*>(ret_type);
  if (type_tuple == nullptr) {
    return "undefined";
  }

  std::ostringstream oss;
  oss << "(";
  for (std::size_t i = 0; i < type_tuple->type_list.size(); ++i) {
    if (i != 0) {
      oss << ", ";
    }
    oss << type_tuple->type_list[i]->getTypeName();
  }
  oss << ")";
  return oss.str();
}

std::string
signature_for_function(StyioAST* ast) {
  std::ostringstream oss;
  if (auto* fn = dynamic_cast<FunctionAST*>(ast)) {
    oss << fn->getNameAsStr() << "(";
    for (std::size_t i = 0; i < fn->params.size(); ++i) {
      if (i != 0) {
        oss << ", ";
      }
      oss << fn->params[i]->getName();
      const std::string type_name = fn->params[i]->getDType() != nullptr ? fn->params[i]->getDType()->getTypeName() : "undefined";
      if (!type_name.empty() && type_name != "undefined") {
        oss << ": " << type_name;
      }
    }
    oss << ") -> " << type_name_from_variant(fn->ret_type);
    return oss.str();
  }

  if (auto* fn = dynamic_cast<SimpleFuncAST*>(ast)) {
    oss << fn->func_name->getAsStr() << "(";
    for (std::size_t i = 0; i < fn->params.size(); ++i) {
      if (i != 0) {
        oss << ", ";
      }
      oss << fn->params[i]->getName();
      const std::string type_name = fn->params[i]->getDType() != nullptr ? fn->params[i]->getDType()->getTypeName() : "undefined";
      if (!type_name.empty() && type_name != "undefined") {
        oss << ": " << type_name;
      }
    }
    oss << ") -> " << type_name_from_variant(fn->ret_type);
    return oss.str();
  }

  return "";
}

}  // namespace

SemanticSummary
analyze_document(const std::string& path, const std::string& text) {
  SemanticSummary summary;

  std::vector<StyioToken*> tokens;
  StyioContext* context = nullptr;
  MainBlockAST* ast = nullptr;

  auto cleanup = [&]()
  {
    delete ast;
    delete context;
    for (auto* token : tokens) {
      delete token;
    }
    StyioAST::destroy_all_tracked_nodes();
  };

  try {
    tokens = StyioTokenizer::tokenize(text);
    TextBuffer buffer(text);
    context = StyioContext::Create(path, text, buffer.build_line_seps(), tokens, false);
    ast = parse_main_block_with_engine_latest(
      *context,
      StyioParserEngine::Nightly,
      nullptr,
      StyioParseMode::Recovery);
    summary.parse_success = ast != nullptr;
    summary.used_recovery = !context->parse_diagnostics().empty();

    for (const auto& diagnostic : context->parse_diagnostics()) {
      summary.diagnostics.push_back(Diagnostic{
        TextRange{diagnostic.start, std::min(diagnostic.end, text.size())},
        DiagnosticSeverity::Error,
        "semantic",
        diagnostic.message});
    }

    StyioAnalyzer analyzer;
    if (ast != nullptr) {
      try {
        ast->typeInfer(&analyzer);
      } catch (const StyioBaseException& ex) {
        summary.diagnostics.push_back(Diagnostic{
          TextRange{0, text.size()},
          DiagnosticSeverity::Error,
          "semantic",
          ex.what()});
      } catch (const std::exception& ex) {
        summary.diagnostics.push_back(Diagnostic{
          TextRange{0, text.size()},
          DiagnosticSeverity::Error,
          "semantic",
          ex.what()});
      }

      for (const auto& entry : analyzer.local_binding_types) {
        summary.inferred_types[entry.first] = type_name_from_type(entry.second);
      }

      for (const auto& entry : analyzer.func_defs) {
        const std::string signature = signature_for_function(entry.second);
        if (!signature.empty()) {
          summary.function_signatures[entry.first] = signature;
        }
      }
    }
  } catch (const StyioBaseException& ex) {
    summary.diagnostics.push_back(Diagnostic{
      TextRange{0, text.size()},
      DiagnosticSeverity::Error,
      "semantic",
      ex.what()});
  } catch (const std::exception& ex) {
    summary.diagnostics.push_back(Diagnostic{
      TextRange{0, text.size()},
      DiagnosticSeverity::Error,
      "semantic",
      ex.what()});
  }

  cleanup();
  return summary;
}

}  // namespace styio::ide
