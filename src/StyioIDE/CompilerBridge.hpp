#pragma once

#ifndef STYIO_IDE_COMPILER_BRIDGE_HPP_
#define STYIO_IDE_COMPILER_BRIDGE_HPP_

#include <string>
#include <unordered_map>
#include <vector>

#include "Common.hpp"

namespace styio::ide {

struct SemanticSummary
{
  bool parse_success = false;
  bool used_recovery = false;
  std::vector<Diagnostic> diagnostics;
  std::unordered_map<std::string, std::string> inferred_types;
  std::unordered_map<std::string, std::string> function_signatures;
};

SemanticSummary analyze_document(const std::string& path, const std::string& text);

}  // namespace styio::ide

#endif
