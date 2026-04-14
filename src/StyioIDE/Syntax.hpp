#pragma once

#ifndef STYIO_IDE_SYNTAX_HPP_
#define STYIO_IDE_SYNTAX_HPP_

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../StyioToken/Token.hpp"
#include "Common.hpp"
#include "VFS.hpp"

namespace styio::ide {

enum class SyntaxNodeKind
{
  Root,
  Group,
  Block,
  List,
  Statement,
  Error,
};

enum class SyntaxBackendKind
{
  Tolerant,
  TreeSitter,
};

struct SyntaxToken
{
  StyioTokenType type = StyioTokenType::UNKNOWN;
  std::string lexeme;
  TextRange range;

  bool is_trivia() const;
};

struct FoldingRange
{
  TextRange range;
};

struct SyntaxNode
{
  SyntaxNodeKind kind = SyntaxNodeKind::Root;
  std::string label;
  TextRange range;
  std::vector<std::size_t> children;
};

struct SyntaxSnapshot
{
  FileId file_id = 0;
  SnapshotId snapshot_id = 0;
  std::string path;
  TextBuffer buffer;
  SyntaxBackendKind backend = SyntaxBackendKind::Tolerant;
  std::vector<SyntaxToken> tokens;
  std::vector<SyntaxNode> nodes;
  std::vector<Diagnostic> diagnostics;
  std::unordered_map<std::size_t, std::size_t> matching_tokens;
  std::vector<FoldingRange> folding_ranges;

  std::optional<std::size_t> token_index_at(std::size_t offset) const;
  std::optional<std::size_t> previous_non_trivia_index(std::size_t offset) const;
  std::optional<std::size_t> next_non_trivia_index(std::size_t offset) const;
  PositionKind position_kind_at(std::size_t offset) const;
  std::vector<std::string> expected_tokens_at(std::size_t offset) const;
  std::vector<std::string> expected_categories_at(std::size_t offset) const;
  std::string prefix_at(std::size_t offset) const;
  ScopeId scope_hint_at(std::size_t offset) const;
  std::vector<std::size_t> node_path_at(std::size_t offset) const;
  const SyntaxNode* node_at_offset(std::size_t offset) const;
};

class SyntaxParser
{
public:
  SyntaxSnapshot parse(const DocumentSnapshot& snapshot) const;
};

}  // namespace styio::ide

#endif
