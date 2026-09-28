#pragma once

#ifndef STYIO_IDE_SEMDB_HPP_
#define STYIO_IDE_SEMDB_HPP_

#include <memory>
#include <optional>
#include <cstdint>
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

struct FileVersionKey
{
  FileId file_id = 0;
  SnapshotId snapshot_id = 0;

  bool operator==(const FileVersionKey& other) const {
    return file_id == other.file_id && snapshot_id == other.snapshot_id;
  }
};

struct OffsetKey
{
  FileId file_id = 0;
  SnapshotId snapshot_id = 0;
  std::size_t offset = 0;

  bool operator==(const OffsetKey& other) const {
    return file_id == other.file_id
      && snapshot_id == other.snapshot_id
      && offset == other.offset;
  }
};

struct FileVersionKeyHash
{
  std::size_t operator()(const FileVersionKey& key) const noexcept;
};

struct OffsetKeyHash
{
  std::size_t operator()(const OffsetKey& key) const noexcept;
};

struct ItemInferenceKey
{
  FileId file_id = 0;
  ItemId item_id = 0;
  std::uint64_t fingerprint = 0;

  bool operator==(const ItemInferenceKey& other) const {
    return file_id == other.file_id
      && item_id == other.item_id
      && fingerprint == other.fingerprint;
  }
};

struct BodyInferenceKey
{
  FileId file_id = 0;
  ItemId item_id = 0;
  std::uint64_t signature_fingerprint = 0;
  std::uint64_t body_fingerprint = 0;

  bool operator==(const BodyInferenceKey& other) const {
    return file_id == other.file_id
      && item_id == other.item_id
      && signature_fingerprint == other.signature_fingerprint
      && body_fingerprint == other.body_fingerprint;
  }
};

struct ItemInferenceKeyHash
{
  std::size_t operator()(const ItemInferenceKey& key) const noexcept;
};

struct BodyInferenceKeyHash
{
  std::size_t operator()(const BodyInferenceKey& key) const noexcept;
};

struct QueryCounter
{
  std::uint64_t hits = 0;
  std::uint64_t misses = 0;
};

struct TypeSignatureResult
{
  FileId file_id = 0;
  ItemId item_id = 0;
  std::string name;
  HirItemKind item_kind = HirItemKind::GlobalBinding;
  std::vector<SemanticParamFact> params;
  std::string return_type_name;
  std::string detail;
  std::string identity_key;
  std::uint64_t signature_fingerprint = 0;
};

struct TypeBodyResult
{
  FileId file_id = 0;
  ItemId item_id = 0;
  std::string name;
  std::string type_name;
  std::string signature_identity_key;
  std::uint64_t signature_fingerprint = 0;
  std::uint64_t body_fingerprint = 0;
  std::vector<std::string> dependency_identity_keys;
};

struct ReceiverTypeResult
{
  std::string receiver_name;
  std::string type_name;
  TextRange receiver_range;
  bool known = false;
};

struct ExpectedTypeResult
{
  std::string callee_name;
  std::string param_name;
  std::string type_name;
  TextRange callee_range;
  std::size_t argument_index = 0;
  bool known = false;
};

struct SemanticQueryStats
{
  QueryCounter syntax_tree;
  QueryCounter semantic_summary;
  QueryCounter hir_module;
  QueryCounter document_symbols;
  QueryCounter semantic_tokens;
  QueryCounter completion_context;
  QueryCounter completion;
  QueryCounter hover;
  QueryCounter definition;
  QueryCounter references;
  QueryCounter type_signature;
  QueryCounter type_body;
  QueryCounter receiver_type;
  QueryCounter expected_type;
};

class SemanticDB
{
private:
  enum class ResolvedNameKind
  {
    Symbol,
    Builtin,
  };

  struct ResolvedName
  {
    ResolvedNameKind kind = ResolvedNameKind::Symbol;
    std::string name;
    SymbolKind symbol_kind = SymbolKind::Variable;
    std::string path;
    TextRange range;
    TextRange selection_range;
    std::string detail;
    std::string type_name;
    std::string identity_key;
    bool has_location = false;
  };

  VirtualFileSystem& vfs_;
  Project& project_;
  SyntaxParser syntax_parser_;
  HirBuilder hir_builder_;
  OpenFileIndex open_file_index_;
  BackgroundIndex background_index_;
  PersistentIndex persistent_index_;
  std::unordered_map<FileVersionKey, SyntaxSnapshot, FileVersionKeyHash> syntax_tree_cache_;
  std::unordered_map<FileVersionKey, SemanticSummary, FileVersionKeyHash> semantic_summary_cache_;
  std::unordered_map<FileVersionKey, HirModule, FileVersionKeyHash> hir_module_cache_;
  std::unordered_map<FileVersionKey, std::vector<DocumentSymbol>, FileVersionKeyHash> document_symbols_cache_;
  std::unordered_map<FileVersionKey, std::vector<std::uint32_t>, FileVersionKeyHash> semantic_tokens_cache_;
  std::unordered_map<OffsetKey, CompletionContext, OffsetKeyHash> completion_context_cache_;
  std::unordered_map<OffsetKey, std::vector<CompletionItem>, OffsetKeyHash> completion_cache_;
  std::unordered_map<OffsetKey, std::optional<HoverResult>, OffsetKeyHash> hover_cache_;
  std::unordered_map<OffsetKey, std::vector<Location>, OffsetKeyHash> definition_cache_;
  std::unordered_map<OffsetKey, std::vector<Location>, OffsetKeyHash> references_cache_;
  std::unordered_map<ItemInferenceKey, TypeSignatureResult, ItemInferenceKeyHash> type_signature_cache_;
  std::unordered_map<BodyInferenceKey, TypeBodyResult, BodyInferenceKeyHash> type_body_cache_;
  std::unordered_map<OffsetKey, std::optional<ReceiverTypeResult>, OffsetKeyHash> receiver_type_cache_;
  std::unordered_map<OffsetKey, std::optional<ExpectedTypeResult>, OffsetKeyHash> expected_type_cache_;
  std::unordered_map<FileId, HirIdentityStore> hir_identity_stores_;
  std::unordered_map<FileId, SnapshotId> active_snapshots_;
  SemanticQueryStats query_stats_;

  std::shared_ptr<IdeSnapshot> build_snapshot_from_document(std::shared_ptr<const DocumentSnapshot> document, bool update_open_index);
  std::shared_ptr<const DocumentSnapshot> document_for_path(const std::string& path);
  void observe_document_snapshot(const DocumentSnapshot& document);
  void erase_query_state_for_file(FileId file_id);
  FileVersionKey file_key_for(const DocumentSnapshot& document) const;
  OffsetKey offset_key_for(const DocumentSnapshot& document, std::size_t offset) const;
  const SyntaxSnapshot& syntax_tree_query(const DocumentSnapshot& document);
  const SemanticSummary& semantic_summary_query(const DocumentSnapshot& document);
  const HirModule& hir_module_query(const DocumentSnapshot& document, bool update_open_index);
  const std::vector<DocumentSymbol>& document_symbols_query(const DocumentSnapshot& document);
  const std::vector<std::uint32_t>& semantic_tokens_query(const DocumentSnapshot& document);
  const CompletionContext& completion_context_query(const DocumentSnapshot& document, std::size_t offset);
  const std::vector<CompletionItem>& completion_query(const DocumentSnapshot& document, std::size_t offset);
  const std::optional<HoverResult>& hover_query(const DocumentSnapshot& document, std::size_t offset);
  const std::vector<Location>& definition_query(const DocumentSnapshot& document, std::size_t offset);
  const std::vector<Location>& references_query(const DocumentSnapshot& document, std::size_t offset);
  const TypeSignatureResult& type_signature_query(const DocumentSnapshot& document, const HirModule& hir, const HirItem& item);
  const TypeBodyResult& type_body_query(const DocumentSnapshot& document, const HirModule& hir, const HirItem& item);
  const std::optional<ReceiverTypeResult>& receiver_type_query(const DocumentSnapshot& document, std::size_t offset);
  const std::optional<ExpectedTypeResult>& expected_type_query(const DocumentSnapshot& document, std::size_t offset);
  std::vector<Diagnostic> diagnostics_from_file_queries(const DocumentSnapshot& document);
  const HirItem* item_at_offset(const HirModule& hir, std::size_t offset) const;
  const HirItem* item_for_symbol(const HirModule& hir, const HirSymbol& symbol) const;
  const HirItem* item_for_resolved_name(const DocumentSnapshot& document, const ResolvedName& resolved);
  std::optional<ResolvedName> resolve_name_at(const DocumentSnapshot& document, const HirModule& hir, std::size_t offset);
  std::optional<ResolvedName> resolve_symbol_target(const DocumentSnapshot& document, const HirSymbol& symbol) const;
  std::optional<ResolvedName> resolve_reference_target(const DocumentSnapshot& document, const HirModule& hir, const HirReference& reference);
  std::optional<ResolvedName> resolve_imported_symbol(const DocumentSnapshot& document, const HirModule& hir, const std::string& name);
  std::optional<ResolvedName> resolve_indexed_symbol(const std::string& name);
  std::optional<ResolvedName> resolve_builtin_symbol(const std::string& name, TextRange reference_range = TextRange{0, 0}) const;
  std::vector<std::string> import_candidate_paths(const DocumentSnapshot& document, const std::string& import_path) const;
  bool same_resolved_target(const ResolvedName& lhs, const ResolvedName& rhs) const;
  void collect_references_to_target(const ResolvedName& target, std::vector<Location>& locations);
  std::vector<IndexedSymbol> merged_workspace_symbols(const std::string& query) const;
  std::vector<IndexedSymbol> merged_workspace_symbols_exact(const std::string& name) const;
  std::vector<IndexedReference> merged_workspace_references(const std::string& name) const;
  std::vector<CompletionItem> builtin_items(const CompletionContext& context) const;

public:
  SemanticDB(VirtualFileSystem& vfs, Project& project);

  void configure_cache_root(const std::string& cache_root);
  void index_workspace();
  void index_workspace_file(const std::string& path);

  std::shared_ptr<IdeSnapshot> build_snapshot(const std::string& path);
  void drop_open_file(const std::string& path);

  std::vector<Diagnostic> syntax_diagnostics_for(const std::string& path);
  std::vector<Diagnostic> diagnostics_for(const std::string& path);
  CompletionContext completion_context_at(const std::string& path, std::size_t offset);
  std::vector<CompletionItem> complete_at(const std::string& path, std::size_t offset);
  std::optional<HoverResult> hover_at(const std::string& path, std::size_t offset);
  std::vector<Location> definition_at(const std::string& path, std::size_t offset);
  std::vector<Location> references_of(const std::string& path, std::size_t offset);
  std::optional<TypeSignatureResult> type_signature_at(const std::string& path, std::size_t offset);
  std::optional<TypeBodyResult> type_body_at(const std::string& path, std::size_t offset);
  std::optional<ReceiverTypeResult> receiver_type_at(const std::string& path, std::size_t offset);
  std::optional<ExpectedTypeResult> expected_type_at(const std::string& path, std::size_t offset);
  std::vector<DocumentSymbol> document_symbols(const std::string& path);
  std::vector<IndexedSymbol> workspace_symbols(const std::string& query);
  std::vector<std::uint32_t> semantic_tokens_for(const std::string& path);
  const SemanticQueryStats& query_stats() const;
  void reset_query_stats();
};

}  // namespace styio::ide

#endif
