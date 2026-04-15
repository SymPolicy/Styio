#pragma once

#ifndef STYIO_IDE_SEMDB_HPP_
#define STYIO_IDE_SEMDB_HPP_

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common.hpp"
#include "CompilerBridge.hpp"
#include "HIR.hpp"
#include "Index.hpp"
#include "Project.hpp"
#include "Syntax.hpp"
#include "VFS.hpp"

namespace styio::ide {

struct IdeSnapshot
{
  std::shared_ptr<const DocumentSnapshot> document;
  SyntaxSnapshot syntax;
  SemanticSummary semantic;
  HirModule hir;
  std::vector<Diagnostic> diagnostics;
};

class SemanticDB
{
private:
  VirtualFileSystem& vfs_;
  Project& project_;
  SyntaxParser syntax_parser_;
  HirBuilder hir_builder_;
  OpenFileIndex open_file_index_;
  BackgroundIndex background_index_;
  PersistentIndex persistent_index_;
  std::unordered_map<std::string, std::shared_ptr<IdeSnapshot>> snapshot_cache_;

  std::shared_ptr<IdeSnapshot> build_snapshot_from_document(std::shared_ptr<const DocumentSnapshot> document, bool update_open_index);
  std::optional<const HirSymbol*> resolve_symbol_at(const IdeSnapshot& snapshot, std::size_t offset) const;
  std::vector<CompletionItem> builtin_items(PositionKind position_kind, const std::string& prefix) const;

public:
  SemanticDB(VirtualFileSystem& vfs, Project& project);

  void configure_cache_root(const std::string& cache_root);
  void index_workspace();

  std::shared_ptr<IdeSnapshot> build_snapshot(const std::string& path);
  void drop_open_file(const std::string& path);

  std::vector<Diagnostic> diagnostics_for(const std::string& path);
  CompletionContext completion_context_at(const std::string& path, std::size_t offset);
  std::vector<CompletionItem> complete_at(const std::string& path, std::size_t offset);
  std::optional<HoverResult> hover_at(const std::string& path, std::size_t offset);
  std::vector<Location> definition_at(const std::string& path, std::size_t offset);
  std::vector<Location> references_of(const std::string& path, std::size_t offset);
  std::vector<DocumentSymbol> document_symbols(const std::string& path);
  std::vector<IndexedSymbol> workspace_symbols(const std::string& query);
  std::vector<std::uint32_t> semantic_tokens_for(const std::string& path);
};

}  // namespace styio::ide

#endif
