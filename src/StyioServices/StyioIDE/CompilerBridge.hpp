#pragma once

#ifndef STYIO_IDE_COMPILER_BRIDGE_HPP_
#define STYIO_IDE_COMPILER_BRIDGE_HPP_

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common.hpp"

namespace styio::ide {

enum class SemanticItemKind
{
  Function,
  Import,
  Resource,
  GlobalBinding,
};

struct SemanticParamFact
{
  std::string name;
  std::string type_name;
};

struct SemanticItemFact
{
  SemanticItemKind kind = SemanticItemKind::GlobalBinding;
  std::string name;
  std::vector<SemanticParamFact> params;
  std::string detail;
  std::string type_name;
  std::size_t ordinal = 0;
  bool canonical = true;
};

struct SemanticSummary
{
  bool parse_success = false;
  bool used_recovery = false;
  std::vector<Diagnostic> diagnostics;
  std::vector<SemanticItemFact> items;
  std::unordered_map<std::string, std::string> inferred_types;
  std::unordered_map<std::string, std::string> function_signatures;
};

SemanticSummary analyze_document(const std::string& path, const std::string& text);

}  // namespace styio::ide

#endif
