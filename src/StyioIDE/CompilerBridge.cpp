#include "CompilerBridge.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <variant>

#include "../StyioAST/AST.hpp"
#include "../StyioLowering/AstToStyioIRLowerer.hpp"
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

std::string
semantic_item_kind_key(SemanticItemKind kind) {
  switch (kind) {
    case SemanticItemKind::Function:
      return "function";
    case SemanticItemKind::Import:
      return "import";
    case SemanticItemKind::Resource:
      return "resource";
    case SemanticItemKind::GlobalBinding:
      return "global";
  }
  return "global";
}

std::string
type_name_from_var(VarAST* var) {
  if (var == nullptr || var->getDType() == nullptr) {
    return "";
  }
  const std::string type_name = var->getDType()->getTypeName();
  return type_name == "undefined" ? "" : type_name;
}

std::vector<SemanticParamFact>
params_from_ast(const std::vector<ParamAST*>& params) {
  std::vector<SemanticParamFact> facts;
  facts.reserve(params.size());
  for (auto* param : params) {
    if (param == nullptr) {
      continue;
    }
    facts.push_back(SemanticParamFact{param->getNameAsStr(), type_name_from_var(param)});
  }
  return facts;
}

std::string
normalize_import_path(const std::string& raw) {
  if (raw.empty()) {
    return raw;
  }

  const bool has_slash = raw.find('/') != std::string::npos;
  const bool has_dot = raw.find('.') != std::string::npos;
  if (has_slash || !has_dot) {
    return raw;
  }

  std::string normalized = raw;
  std::replace(normalized.begin(), normalized.end(), '.', '/');
  return normalized;
}

void
append_item_fact(
  SemanticSummary& summary,
  SemanticItemFact fact,
  std::unordered_map<std::string, std::size_t>& ordinals
) {
  if (fact.name.empty()) {
    fact.name = semantic_item_kind_key(fact.kind);
  }
  if (fact.kind == SemanticItemKind::Import) {
    fact.name = normalize_import_path(fact.name);
  }

  const std::string ordinal_key = semantic_item_kind_key(fact.kind) + ":" + fact.name;
  fact.ordinal = ordinals[ordinal_key]++;
  summary.items.push_back(std::move(fact));
}

std::string
path_name_from_ast(StyioAST* ast) {
  if (auto* text = dynamic_cast<StringAST*>(ast)) {
    return text->getValue();
  }
  if (auto* path = dynamic_cast<ResPathAST*>(ast)) {
    return path->getPath();
  }
  if (auto* path = dynamic_cast<RemotePathAST*>(ast)) {
    return path->getPath();
  }
  if (auto* path = dynamic_cast<WebUrlAST*>(ast)) {
    return path->getPath();
  }
  if (auto* file = dynamic_cast<FileResourceAST*>(ast)) {
    return path_name_from_ast(file->getPath());
  }
  return "";
}

void
collect_semantic_items(SemanticSummary& summary, MainBlockAST* ast) {
  if (ast == nullptr) {
    return;
  }

  std::unordered_map<std::string, std::size_t> ordinals;
  for (auto* stmt : ast->getStmts()) {
    if (stmt == nullptr) {
      continue;
    }

    if (auto* fn = dynamic_cast<FunctionAST*>(stmt)) {
      append_item_fact(
        summary,
        SemanticItemFact{
          SemanticItemKind::Function,
          fn->getNameAsStr(),
          params_from_ast(fn->params),
          signature_for_function(fn),
          "",
          0,
          true},
        ordinals);
      continue;
    }

    if (auto* fn = dynamic_cast<SimpleFuncAST*>(stmt)) {
      if (fn->func_name == nullptr) {
        continue;
      }
      append_item_fact(
        summary,
        SemanticItemFact{
          SemanticItemKind::Function,
          fn->func_name->getAsStr(),
          params_from_ast(fn->params),
          signature_for_function(fn),
          "",
          0,
          true},
        ordinals);
      continue;
    }

    if (auto* bind = dynamic_cast<FlexBindAST*>(stmt)) {
      VarAST* var = bind->getVar();
      if (var == nullptr) {
        continue;
      }
      const std::string& name = bind->getNameAsStr();
      const auto inferred = summary.inferred_types.find(name);
      append_item_fact(
        summary,
        SemanticItemFact{
          SemanticItemKind::GlobalBinding,
          name,
          {},
          "binding",
          inferred == summary.inferred_types.end() ? type_name_from_var(var) : inferred->second,
          0,
          true},
        ordinals);
      continue;
    }

    if (auto* bind = dynamic_cast<FinalBindAST*>(stmt)) {
      VarAST* var = bind->getVar();
      if (var == nullptr) {
        continue;
      }
      const std::string& name = bind->getName();
      const auto inferred = summary.inferred_types.find(name);
      append_item_fact(
        summary,
        SemanticItemFact{
          SemanticItemKind::GlobalBinding,
          name,
          {},
          "binding",
          inferred == summary.inferred_types.end() ? type_name_from_var(var) : inferred->second,
          0,
          true},
        ordinals);
      continue;
    }

    if (auto* imports = dynamic_cast<ExtPackAST*>(stmt)) {
      for (const auto& path : imports->getPaths()) {
        append_item_fact(
          summary,
          SemanticItemFact{
            SemanticItemKind::Import,
            path,
            {},
            "import",
            "",
            0,
            true},
          ordinals);
      }
      continue;
    }

    if (auto* resources = dynamic_cast<ResourceAST*>(stmt)) {
      std::size_t unnamed_resource = 0;
      for (const auto& entry : resources->res_list) {
        std::string name;
        if (auto* bind = dynamic_cast<FinalBindAST*>(entry.first)) {
          name = bind->getName();
        }
        if (name.empty()) {
          name = path_name_from_ast(entry.first);
        }
        if (name.empty()) {
          name = "resource@" + std::to_string(unnamed_resource++);
        }
        append_item_fact(
          summary,
          SemanticItemFact{
            SemanticItemKind::Resource,
            name,
            {},
            entry.second.empty() ? "resource" : entry.second,
            entry.second,
            0,
            true},
          ordinals);
      }
      continue;
    }
  }
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

    AstToStyioIRLowerer analyzer;
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

      collect_semantic_items(summary, ast);
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
