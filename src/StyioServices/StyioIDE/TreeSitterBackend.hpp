#pragma once

#ifndef STYIO_IDE_TREE_SITTER_BACKEND_HPP_
#define STYIO_IDE_TREE_SITTER_BACKEND_HPP_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Syntax.hpp"

namespace styio::ide {

struct TreeSitterParseResult
{
  std::vector<SyntaxNode> nodes;
  std::vector<Diagnostic> diagnostics;
  std::vector<FoldingRange> folding_ranges;
  std::shared_ptr<void> tree;
  bool reused_previous_tree = false;
};

std::optional<TreeSitterParseResult> parse_with_tree_sitter(
  const DocumentSnapshot& snapshot,
  const std::shared_ptr<void>& previous_tree,
  const std::string& previous_text);

}  // namespace styio::ide

#endif
