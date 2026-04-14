#pragma once

#ifndef STYIO_IDE_TREE_SITTER_BACKEND_HPP_
#define STYIO_IDE_TREE_SITTER_BACKEND_HPP_

#include <optional>
#include <vector>

#include "Syntax.hpp"

namespace styio::ide {

struct TreeSitterParseResult
{
  std::vector<SyntaxNode> nodes;
  std::vector<Diagnostic> diagnostics;
  std::vector<FoldingRange> folding_ranges;
};

std::optional<TreeSitterParseResult> parse_with_tree_sitter(const DocumentSnapshot& snapshot);

}  // namespace styio::ide

#endif
