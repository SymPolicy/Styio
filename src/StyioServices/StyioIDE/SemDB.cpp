#include "SemDB.hpp"

#include "StyioParser/SymbolRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
#include <unordered_set>
#include <utility>

namespace styio::ide {

namespace {

struct BuiltinSymbol
{
  std::string name;
  CompletionItemKind completion_kind = CompletionItemKind::Keyword;
  SymbolKind symbol_kind = SymbolKind::Builtin;
  std::string detail;
  std::string type_name;
  CompletionSource source = CompletionSource::Builtin;
};

CompletionItemKind
completion_kind_for_registry_entry(const styio::symbols::RegistryEntry& entry) {
  switch (entry.surface) {
    case styio::symbols::RegistrySurface::Type:
      return CompletionItemKind::Type;
    case styio::symbols::RegistrySurface::Member:
      return CompletionItemKind::Property;
    case styio::symbols::RegistrySurface::Value:
      return CompletionItemKind::Keyword;
  }
  return CompletionItemKind::Keyword;
}

CompletionSource
completion_source_for_registry_entry(const styio::symbols::RegistryEntry& entry) {
  return entry.origin == styio::symbols::RegistryOrigin::Macro
    ? CompletionSource::Keyword
    : CompletionSource::Builtin;
}

bool
starts_with_prefix(const std::string& candidate, const std::string& prefix) {
  if (prefix.empty()) {
    return true;
  }
  if (candidate.size() < prefix.size()) {
    return false;
  }
  return std::equal(prefix.begin(), prefix.end(), candidate.begin());
}

const std::vector<BuiltinSymbol>&
builtin_symbols() {
  static const std::vector<BuiltinSymbol> symbols = []()
  {
    std::vector<BuiltinSymbol> builtins;
    builtins.reserve(styio::symbols::default_symbol_registry().size());
    for (const auto& entry : styio::symbols::default_symbol_registry()) {
      builtins.push_back(BuiltinSymbol{
        entry.name,
        completion_kind_for_registry_entry(entry),
        SymbolKind::Builtin,
        entry.detail,
        entry.type_name,
        completion_source_for_registry_entry(entry),
      });
    }
    return builtins;
  }();
  return symbols;
}

const BuiltinSymbol*
find_builtin_symbol(const std::string& name) {
  const auto& symbols = builtin_symbols();
  const auto it = std::find_if(
    symbols.begin(),
    symbols.end(),
    [&](const BuiltinSymbol& symbol)
    {
      return symbol.name == name;
    });
  return it == symbols.end() ? nullptr : &(*it);
}

bool
builtin_matches_position(const BuiltinSymbol& symbol, PositionKind position_kind) {
  if (position_kind == PositionKind::Type) {
    return symbol.completion_kind == CompletionItemKind::Type;
  }
  if (position_kind == PositionKind::MemberAccess) {
    return symbol.completion_kind == CompletionItemKind::Property;
  }
  return symbol.completion_kind != CompletionItemKind::Property
    && symbol.completion_kind != CompletionItemKind::Type;
}

bool
completion_kind_matches_position(CompletionItemKind kind, PositionKind position_kind) {
  if (position_kind == PositionKind::Type) {
    return kind == CompletionItemKind::Type;
  }
  if (position_kind == PositionKind::MemberAccess) {
    return kind == CompletionItemKind::Property;
  }
  return kind != CompletionItemKind::Property && kind != CompletionItemKind::Type;
}

bool
is_collection_type(const std::string& type_name) {
  return type_name.rfind("list[", 0) == 0
    || type_name.rfind("[", 0) == 0;
}

bool
is_dict_type(const std::string& type_name) {
  return type_name.rfind("dict[", 0) == 0
    || type_name.rfind("map[", 0) == 0;
}

std::string
diagnostic_key(const Diagnostic& diagnostic) {
  return std::to_string(diagnostic.range.start)
    + ":"
    + std::to_string(diagnostic.range.end)
    + ":"
    + diagnostic.message;
}

void
append_unique_diagnostic(
  std::vector<Diagnostic>& diagnostics,
  std::unordered_set<std::string>& seen,
  Diagnostic diagnostic
) {
  if (seen.insert(diagnostic_key(diagnostic)).second) {
    diagnostics.push_back(std::move(diagnostic));
  }
}

std::string
collection_element_type(const std::string& type_name) {
  const std::size_t open = type_name.find('[');
  const std::size_t close = type_name.rfind(']');
  if (open == std::string::npos || close == std::string::npos || close <= open + 1) {
    return "";
  }
  return type_name.substr(open + 1, close - open - 1);
}

std::string
normalize_import_path_for_lookup(std::string import_path) {
  if (import_path.empty()) {
    return import_path;
  }

  const bool has_slash = import_path.find('/') != std::string::npos;
  const bool has_dot = import_path.find('.') != std::string::npos;
  if (!has_slash && has_dot) {
    std::replace(import_path.begin(), import_path.end(), '.', '/');
  }
  return import_path;
}

bool
member_matches_receiver(const BuiltinSymbol& symbol, const std::string& receiver_type_name) {
  if (symbol.completion_kind != CompletionItemKind::Property || receiver_type_name.empty()) {
    return true;
  }
  if (is_dict_type(receiver_type_name)) {
    return symbol.name == "len" || symbol.name == "keys" || symbol.name == "values";
  }
  if (is_collection_type(receiver_type_name)) {
    return symbol.name == "len" || symbol.name == "first" || symbol.name == "last";
  }
  if (receiver_type_name == "string" || receiver_type_name == "str") {
    return symbol.name == "len";
  }
  return symbol.name == "len";
}

std::string
member_type_for_receiver(const std::string& member_name, const std::string& receiver_type_name) {
  if (member_name == "len") {
    return "i64";
  }
  if ((member_name == "first" || member_name == "last") && is_collection_type(receiver_type_name)) {
    return collection_element_type(receiver_type_name);
  }
  if (member_name == "keys" && is_dict_type(receiver_type_name)) {
    return "list[string]";
  }
  if (member_name == "values" && is_dict_type(receiver_type_name)) {
    return "list[unknown]";
  }
  return "";
}

std::string
canonical_type_name(const std::string& type_name) {
  if (type_name == "str") {
    return "string";
  }
  if (type_name == "int") {
    return "i32";
  }
  if (type_name == "long") {
    return "i64";
  }
  if (type_name == "float") {
    return "f32";
  }
  if (type_name == "double") {
    return "f64";
  }
  return type_name;
}

bool
type_compatible(const std::string& expected_type_name, const std::string& candidate_type_name) {
  if (expected_type_name.empty() || candidate_type_name.empty()) {
    return false;
  }
  return canonical_type_name(expected_type_name) == canonical_type_name(candidate_type_name);
}

const HirScope*
scope_by_id(const HirModule& hir, ScopeId scope_id) {
  const auto it = std::find_if(
    hir.scopes.begin(),
    hir.scopes.end(),
    [&](const HirScope& scope)
    {
      return scope.id == scope_id;
    });
  return it == hir.scopes.end() ? nullptr : &(*it);
}

std::size_t
hir_scope_depth(const HirModule& hir, ScopeId scope_id) {
  std::size_t depth = 0;
  std::optional<ScopeId> current = scope_id;
  while (current.has_value()) {
    const HirScope* scope = scope_by_id(hir, *current);
    if (scope == nullptr) {
      break;
    }
    current = scope->parent;
    depth += 1;
  }
  return depth;
}

ScopeId
innermost_scope_at(const HirModule& hir, std::size_t offset) {
  ScopeId best = 0;
  std::size_t best_depth = 0;
  for (const auto& scope : hir.scopes) {
    if (!scope.range.contains(offset)) {
      continue;
    }
    const std::size_t depth = hir_scope_depth(hir, scope.id);
    if (depth >= best_depth) {
      best = scope.id;
      best_depth = depth;
    }
  }
  return best;
}

bool
scope_is_visible_from(const HirModule& hir, ScopeId symbol_scope_id, ScopeId completion_scope_id) {
  ScopeId cursor = completion_scope_id;
  while (true) {
    if (cursor == symbol_scope_id) {
      return true;
    }
    const HirScope* scope = scope_by_id(hir, cursor);
    if (scope == nullptr || !scope->parent.has_value()) {
      return false;
    }
    cursor = *scope->parent;
  }
}

bool
symbol_visible_at(const HirModule& hir, const HirSymbol& symbol, ScopeId completion_scope_id, std::size_t offset) {
  if (symbol.scope_id == 0) {
    return true;
  }
  if (symbol.name_range.start > offset) {
    return false;
  }
  return scope_is_visible_from(hir, symbol.scope_id, completion_scope_id);
}

bool
symbol_is_local_to_completion(const HirModule& hir, const HirSymbol& symbol, ScopeId completion_scope_id) {
  return symbol.scope_id != 0 && scope_is_visible_from(hir, symbol.scope_id, completion_scope_id);
}

std::string
return_type_from_detail(const std::string& detail);

std::string
completion_type_for_symbol(const HirSymbol& symbol) {
  if (!symbol.type_name.empty()) {
    return symbol.type_name;
  }
  if (symbol.kind == SymbolKind::Function) {
    return return_type_from_detail(symbol.detail);
  }
  return "";
}

std::string
location_key(const Location& location) {
  return location.path + ":" + std::to_string(location.range.start) + ":" + std::to_string(location.range.end);
}

std::string
trim_copy(const std::string& text) {
  std::size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
    begin += 1;
  }
  std::size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    end -= 1;
  }
  return text.substr(begin, end - begin);
}

std::string
return_type_from_detail(const std::string& detail) {
  const std::size_t arrow = detail.find("->");
  if (arrow == std::string::npos) {
    return "";
  }
  return trim_copy(detail.substr(arrow + 2));
}

TypeId
type_id_for_name(const std::string& type_name) {
  if (type_name.empty()) {
    return 0;
  }
  return static_cast<TypeId>((std::hash<std::string>{}(type_name) & 0x7fffffffU) + 1U);
}

std::optional<std::size_t>
previous_token_index(const SyntaxSnapshot& syntax, std::size_t index) {
  while (index > 0) {
    index -= 1;
    if (!syntax.tokens[index].is_trivia() && syntax.tokens[index].type != StyioTokenType::TOK_EOF) {
      return index;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t>
token_index_for_offset_or_previous(const SyntaxSnapshot& syntax, std::size_t offset) {
  if (auto token = syntax.token_index_at(offset); token.has_value()) {
    return token;
  }
  return syntax.previous_non_trivia_index(offset);
}

int
completion_score(
  const CompletionItem& item,
  const CompletionContext& context,
  int source_rank
) {
  int score = 0;
  if (item.label == context.prefix) {
    score += 150;
  } else if (starts_with_prefix(item.label, context.prefix)) {
    score += 100;
  } else if (item.label.find(context.prefix) != std::string::npos) {
    score += 40;
  }

  if (context.position_kind == PositionKind::Type && item.kind == CompletionItemKind::Type) {
    score += 50;
  }
  if (context.position_kind == PositionKind::MemberAccess && item.kind == CompletionItemKind::Property) {
    score += 50;
  }
  if (!context.expected_type_name.empty()) {
    if (type_compatible(context.expected_type_name, item.type_name)) {
      score += 125;
    } else if (!item.type_name.empty()) {
      score -= 15;
    }
  }

  score += source_rank;
  return score;
}

int
semantic_token_type_index(const SyntaxToken& token) {
  switch (token.type) {
    case StyioTokenType::COMMENT_LINE:
    case StyioTokenType::COMMENT_CLOSED:
      return 17;
    case StyioTokenType::STRING:
      return 18;
    case StyioTokenType::INTEGER:
    case StyioTokenType::DECIMAL:
      return 19;
    case StyioTokenType::NAME:
      return 8;
    default:
      return 20;
  }
}

template <typename Map>
void
erase_entries_for_file(Map& map, FileId file_id) {
  for (auto it = map.begin(); it != map.end();) {
    if (it->first.file_id == file_id) {
      it = map.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace

std::size_t
FileVersionKeyHash::operator()(const FileVersionKey& key) const noexcept {
  std::size_t seed = std::hash<FileId>{}(key.file_id);
  const std::size_t value = std::hash<SnapshotId>{}(key.snapshot_id);
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
  return seed;
}

std::size_t
OffsetKeyHash::operator()(const OffsetKey& key) const noexcept {
  std::size_t seed = FileVersionKeyHash{}(FileVersionKey{key.file_id, key.snapshot_id});
  const std::size_t value = std::hash<std::size_t>{}(key.offset);
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
  return seed;
}

std::size_t
ItemInferenceKeyHash::operator()(const ItemInferenceKey& key) const noexcept {
  std::size_t seed = std::hash<FileId>{}(key.file_id);
  std::size_t value = std::hash<ItemId>{}(key.item_id);
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
  value = std::hash<std::uint64_t>{}(key.fingerprint);
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
  return seed;
}

std::size_t
BodyInferenceKeyHash::operator()(const BodyInferenceKey& key) const noexcept {
  std::size_t seed = std::hash<FileId>{}(key.file_id);
  std::size_t value = std::hash<ItemId>{}(key.item_id);
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
  value = std::hash<std::uint64_t>{}(key.signature_fingerprint);
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
  value = std::hash<std::uint64_t>{}(key.body_fingerprint);
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
  return seed;
}

SemanticDB::SemanticDB(VirtualFileSystem& vfs, Project& project) :
    vfs_(vfs),
    project_(project),
    persistent_index_(project.cache_root()) {
}

void
SemanticDB::configure_cache_root(const std::string& cache_root) {
  persistent_index_.set_cache_root(cache_root);
}

std::shared_ptr<IdeSnapshot>
SemanticDB::build_snapshot_from_document(std::shared_ptr<const DocumentSnapshot> document, bool update_open_index) {
  observe_document_snapshot(*document);

  auto snapshot = std::make_shared<IdeSnapshot>();
  snapshot->document = std::move(document);
  snapshot->syntax = syntax_tree_query(*snapshot->document);
  snapshot->semantic = semantic_summary_query(*snapshot->document);
  snapshot->hir = hir_module_query(*snapshot->document, update_open_index);
  snapshot->diagnostics = diagnostics_from_file_queries(*snapshot->document);
  return snapshot;
}

std::shared_ptr<const DocumentSnapshot>
SemanticDB::document_for_path(const std::string& path) {
  auto document = vfs_.snapshot_for(path);
  observe_document_snapshot(*document);
  return document;
}

void
SemanticDB::observe_document_snapshot(const DocumentSnapshot& document) {
  const auto it = active_snapshots_.find(document.file_id);
  if (it == active_snapshots_.end()) {
    active_snapshots_[document.file_id] = document.snapshot_id;
    return;
  }

  if (it->second == document.snapshot_id) {
    return;
  }

  erase_query_state_for_file(document.file_id);
  open_file_index_.erase(document.path);
  active_snapshots_[document.file_id] = document.snapshot_id;
}

void
SemanticDB::erase_query_state_for_file(FileId file_id) {
  erase_entries_for_file(syntax_tree_cache_, file_id);
  erase_entries_for_file(semantic_summary_cache_, file_id);
  erase_entries_for_file(hir_module_cache_, file_id);
  erase_entries_for_file(document_symbols_cache_, file_id);
  erase_entries_for_file(semantic_tokens_cache_, file_id);
  erase_entries_for_file(completion_context_cache_, file_id);
  erase_entries_for_file(completion_cache_, file_id);
  erase_entries_for_file(hover_cache_, file_id);
  erase_entries_for_file(definition_cache_, file_id);
  erase_entries_for_file(references_cache_, file_id);
  erase_entries_for_file(receiver_type_cache_, file_id);
  erase_entries_for_file(expected_type_cache_, file_id);
}

FileVersionKey
SemanticDB::file_key_for(const DocumentSnapshot& document) const {
  return FileVersionKey{document.file_id, document.snapshot_id};
}

OffsetKey
SemanticDB::offset_key_for(const DocumentSnapshot& document, std::size_t offset) const {
  return OffsetKey{document.file_id, document.snapshot_id, offset};
}

const SyntaxSnapshot&
SemanticDB::syntax_tree_query(const DocumentSnapshot& document) {
  const FileVersionKey key = file_key_for(document);
  const auto it = syntax_tree_cache_.find(key);
  if (it != syntax_tree_cache_.end()) {
    query_stats_.syntax_tree.hits += 1;
    return it->second;
  }

  query_stats_.syntax_tree.misses += 1;
  auto inserted = syntax_tree_cache_.emplace(key, syntax_parser_.parse(document));
  return inserted.first->second;
}

const SemanticSummary&
SemanticDB::semantic_summary_query(const DocumentSnapshot& document) {
  const FileVersionKey key = file_key_for(document);
  const auto it = semantic_summary_cache_.find(key);
  if (it != semantic_summary_cache_.end()) {
    query_stats_.semantic_summary.hits += 1;
    return it->second;
  }

  query_stats_.semantic_summary.misses += 1;
  auto inserted = semantic_summary_cache_.emplace(key, analyze_document(document.path, document.buffer.text()));
  return inserted.first->second;
}

const HirModule&
SemanticDB::hir_module_query(const DocumentSnapshot& document, bool update_open_index) {
  const FileVersionKey key = file_key_for(document);
  const auto it = hir_module_cache_.find(key);
  if (it != hir_module_cache_.end()) {
    query_stats_.hir_module.hits += 1;
    if (update_open_index && document.is_open) {
      open_file_index_.update(document.path, it->second);
    }
    return it->second;
  }

  query_stats_.hir_module.misses += 1;
  const SyntaxSnapshot& syntax = syntax_tree_query(document);
  const SemanticSummary& semantic = semantic_summary_query(document);
  HirIdentityStore& identity_store = hir_identity_stores_[document.file_id];
  auto inserted = hir_module_cache_.emplace(key, hir_builder_.build(syntax, semantic, identity_store));
  if (update_open_index && document.is_open) {
    open_file_index_.update(document.path, inserted.first->second);
  }
  return inserted.first->second;
}

std::vector<Diagnostic>
SemanticDB::diagnostics_from_file_queries(const DocumentSnapshot& document) {
  const SyntaxSnapshot& syntax = syntax_tree_query(document);
  const SemanticSummary& semantic = semantic_summary_query(document);
  if (document.is_open) {
    (void)hir_module_query(document, true);
  }

  std::vector<Diagnostic> diagnostics;
  std::unordered_set<std::string> seen_diagnostics;
  diagnostics.reserve(syntax.diagnostics.size() + semantic.diagnostics.size() + 1);
  for (auto diagnostic : syntax.diagnostics) {
    append_unique_diagnostic(diagnostics, seen_diagnostics, std::move(diagnostic));
  }
  for (auto diagnostic : semantic.diagnostics) {
    append_unique_diagnostic(diagnostics, seen_diagnostics, std::move(diagnostic));
  }
  if (document.needs_full_resync) {
    append_unique_diagnostic(diagnostics, seen_diagnostics, Diagnostic{
      TextRange{0, 0},
      DiagnosticSeverity::Error,
      "lsp",
      document.resync_reason.empty()
        ? "full document resynchronization required"
        : "full document resynchronization required: " + document.resync_reason});
  }
  return diagnostics;
}

void
SemanticDB::index_workspace() {
  std::vector<IndexedSymbol> persisted;
  for (const auto& path : project_.workspace_files()) {
    index_workspace_file(path);

    auto document = document_for_path(path);
    const HirModule& hir = hir_module_query(*document, false);
    for (const auto& symbol : hir.symbols) {
      if (symbol.scope_id != 0) {
        continue;
      }
      persisted.push_back(IndexedSymbol{
        path,
        symbol.name,
        symbol.kind,
        symbol.full_range,
        symbol.name_range,
        symbol.detail.empty() ? symbol.type_name : symbol.detail});
    }
  }
  persistent_index_.save_symbols(persisted);
}

void
SemanticDB::index_workspace_file(const std::string& path) {
  if (!std::filesystem::exists(path)) {
    background_index_.erase(path);
    return;
  }

  auto document = document_for_path(path);
  auto snapshot = build_snapshot_from_document(document, false);
  background_index_.update(path, snapshot->hir);
}

std::shared_ptr<IdeSnapshot>
SemanticDB::build_snapshot(const std::string& path) {
  auto document = document_for_path(path);
  return build_snapshot_from_document(document, true);
}

void
SemanticDB::drop_open_file(const std::string& path) {
  auto document = vfs_.snapshot_for(path);
  erase_query_state_for_file(document->file_id);
  active_snapshots_.erase(document->file_id);
  open_file_index_.erase(document->path);
  syntax_parser_.drop_cached_file(document->path);
}

std::vector<Diagnostic>
SemanticDB::syntax_diagnostics_for(const std::string& path) {
  auto document = document_for_path(path);
  const SyntaxSnapshot& syntax = syntax_tree_query(*document);
  std::vector<Diagnostic> diagnostics = syntax.diagnostics;
  if (document->needs_full_resync) {
    diagnostics.push_back(Diagnostic{
      TextRange{0, 0},
      DiagnosticSeverity::Error,
      "lsp",
      document->resync_reason.empty()
        ? "full document resynchronization required"
        : "full document resynchronization required: " + document->resync_reason});
  }
  return diagnostics;
}

std::vector<Diagnostic>
SemanticDB::diagnostics_for(const std::string& path) {
  auto document = document_for_path(path);
  return diagnostics_from_file_queries(*document);
}

CompletionContext
SemanticDB::completion_context_at(const std::string& path, std::size_t offset) {
  auto document = document_for_path(path);
  return completion_context_query(*document, offset);
}

const HirItem*
SemanticDB::item_at_offset(const HirModule& hir, std::size_t offset) const {
  const HirItem* best = nullptr;
  for (const auto& item : hir.items) {
    if (item.full_range.length() == 0 || !item.full_range.contains(offset)) {
      continue;
    }
    if (best == nullptr || item.full_range.length() < best->full_range.length()) {
      best = &item;
    }
  }
  return best;
}

const HirItem*
SemanticDB::item_for_symbol(const HirModule& hir, const HirSymbol& symbol) const {
  if (!symbol.item_id.has_value()) {
    return nullptr;
  }
  return hir.item_by_id(*symbol.item_id);
}

const HirItem*
SemanticDB::item_for_resolved_name(const DocumentSnapshot& document, const ResolvedName& resolved) {
  if (!resolved.has_location || resolved.path.empty()) {
    return nullptr;
  }

  auto target_document = resolved.path == document.path ? vfs_.snapshot_for(document.path) : document_for_path(resolved.path);
  const HirModule& target_hir = hir_module_query(*target_document, target_document->is_open);
  for (const auto& item : target_hir.items) {
    if (item.name == resolved.name
        && item.name_range.start == resolved.selection_range.start
        && item.name_range.end == resolved.selection_range.end) {
      return &item;
    }
  }
  return item_at_offset(target_hir, resolved.selection_range.start);
}

const TypeSignatureResult&
SemanticDB::type_signature_query(
  const DocumentSnapshot& document,
  const HirModule& hir,
  const HirItem& item
) {
  (void)hir;
  const ItemInferenceKey key{document.file_id, item.id, item.signature_fingerprint};
  const auto it = type_signature_cache_.find(key);
  if (it != type_signature_cache_.end()) {
    query_stats_.type_signature.hits += 1;
    return it->second;
  }

  query_stats_.type_signature.misses += 1;
  std::string return_type = item.type_name;
  if (return_type.empty()) {
    return_type = return_type_from_detail(item.detail);
  }
  if (return_type.empty()) {
    return_type = "unknown";
  }

  TypeSignatureResult result;
  result.file_id = document.file_id;
  result.item_id = item.id;
  result.name = item.name;
  result.item_kind = item.kind;
  result.params = item.params;
  result.return_type_name = return_type;
  result.detail = item.detail;
  result.identity_key = document.path + ":" + item.identity_key;
  result.signature_fingerprint = item.signature_fingerprint;

  auto inserted = type_signature_cache_.emplace(key, std::move(result));
  return inserted.first->second;
}

const TypeBodyResult&
SemanticDB::type_body_query(
  const DocumentSnapshot& document,
  const HirModule& hir,
  const HirItem& item
) {
  const BodyInferenceKey key{
    document.file_id,
    item.id,
    item.signature_fingerprint,
    item.body_fingerprint};
  const auto it = type_body_cache_.find(key);
  if (it != type_body_cache_.end()) {
    query_stats_.type_body.hits += 1;
    return it->second;
  }

  query_stats_.type_body.misses += 1;
  const TypeSignatureResult& signature = type_signature_query(document, hir, item);

  TypeBodyResult result;
  result.file_id = document.file_id;
  result.item_id = item.id;
  result.name = item.name;
  result.type_name = signature.return_type_name;
  result.signature_identity_key = signature.identity_key;
  result.signature_fingerprint = item.signature_fingerprint;
  result.body_fingerprint = item.body_fingerprint;

  std::unordered_set<std::string> seen_dependencies;
  for (const auto& reference : hir.references) {
    if (!item.full_range.contains(reference.range.start)) {
      continue;
    }
    const auto resolved = resolve_reference_target(document, hir, reference);
    if (!resolved.has_value() || resolved->identity_key == signature.identity_key) {
      continue;
    }
    if (seen_dependencies.insert(resolved->identity_key).second) {
      result.dependency_identity_keys.push_back(resolved->identity_key);
    }
  }

  auto inserted = type_body_cache_.emplace(key, std::move(result));
  return inserted.first->second;
}

const std::optional<ReceiverTypeResult>&
SemanticDB::receiver_type_query(const DocumentSnapshot& document, std::size_t offset) {
  const OffsetKey key = offset_key_for(document, offset);
  const auto it = receiver_type_cache_.find(key);
  if (it != receiver_type_cache_.end()) {
    query_stats_.receiver_type.hits += 1;
    return it->second;
  }

  query_stats_.receiver_type.misses += 1;
  const SyntaxSnapshot& syntax = syntax_tree_query(document);
  const HirModule& hir = hir_module_query(document, true);
  std::optional<ReceiverTypeResult> result;

  std::optional<std::size_t> dot_index;
  if (auto token_index = token_index_for_offset_or_previous(syntax, offset); token_index.has_value()) {
    if (syntax.tokens[*token_index].type == StyioTokenType::TOK_DOT) {
      dot_index = token_index;
    } else if (auto prev = previous_token_index(syntax, *token_index); prev.has_value()
               && syntax.tokens[*prev].type == StyioTokenType::TOK_DOT) {
      dot_index = prev;
    }
  }
  if (!dot_index.has_value()) {
    if (auto prev = syntax.previous_non_trivia_index(offset); prev.has_value()
        && syntax.tokens[*prev].type == StyioTokenType::TOK_DOT) {
      dot_index = prev;
    }
  }

  if (dot_index.has_value()) {
    const auto receiver_index = previous_token_index(syntax, *dot_index);
    if (receiver_index.has_value() && syntax.tokens[*receiver_index].type == StyioTokenType::NAME) {
      const SyntaxToken& receiver = syntax.tokens[*receiver_index];
      if (auto resolved = resolve_name_at(document, hir, receiver.range.start); resolved.has_value()) {
        std::string type_name = resolved->type_name;
        if (type_name.empty()) {
          if (const HirItem* item = item_for_resolved_name(document, *resolved); item != nullptr) {
            auto target_document = document_for_path(resolved->path);
            const HirModule& target_hir = hir_module_query(*target_document, target_document->is_open);
            type_name = type_signature_query(*target_document, target_hir, *item).return_type_name;
          }
        }
        result = ReceiverTypeResult{
          receiver.lexeme,
          type_name,
          receiver.range,
          !type_name.empty() && type_name != "unknown" && type_name != "undefined"};
      }
    }
  }

  auto inserted = receiver_type_cache_.emplace(key, std::move(result));
  return inserted.first->second;
}

const std::optional<ExpectedTypeResult>&
SemanticDB::expected_type_query(const DocumentSnapshot& document, std::size_t offset) {
  const OffsetKey key = offset_key_for(document, offset);
  const auto it = expected_type_cache_.find(key);
  if (it != expected_type_cache_.end()) {
    query_stats_.expected_type.hits += 1;
    return it->second;
  }

  query_stats_.expected_type.misses += 1;
  const SyntaxSnapshot& syntax = syntax_tree_query(document);
  const HirModule& hir = hir_module_query(document, true);
  std::optional<ExpectedTypeResult> result;

  const auto token_index = token_index_for_offset_or_previous(syntax, offset);
  if (token_index.has_value()) {
    for (std::size_t i = *token_index + 1; i > 0; --i) {
      const std::size_t index = i - 1;
      if (syntax.tokens[index].type != StyioTokenType::TOK_LPAREN) {
        continue;
      }
      const auto matching = syntax.matching_tokens.find(index);
      if (matching == syntax.matching_tokens.end() || syntax.tokens[matching->second].range.end < offset) {
        continue;
      }
      const auto callee_index = previous_token_index(syntax, index);
      if (!callee_index.has_value() || syntax.tokens[*callee_index].type != StyioTokenType::NAME) {
        continue;
      }

      std::size_t argument_index = 0;
      std::size_t depth = 0;
      for (std::size_t j = index + 1; j < syntax.tokens.size() && syntax.tokens[j].range.start < offset; ++j) {
        const StyioTokenType type = syntax.tokens[j].type;
        if (type == StyioTokenType::TOK_LPAREN || type == StyioTokenType::TOK_LBOXBRAC || type == StyioTokenType::TOK_LCURBRAC) {
          depth += 1;
        } else if ((type == StyioTokenType::TOK_RPAREN || type == StyioTokenType::TOK_RBOXBRAC || type == StyioTokenType::TOK_RCURBRAC)
                   && depth > 0) {
          depth -= 1;
        } else if (type == StyioTokenType::TOK_COMMA && depth == 0) {
          argument_index += 1;
        }
      }

      const SyntaxToken& callee = syntax.tokens[*callee_index];
      const auto resolved = resolve_name_at(document, hir, callee.range.start);
      if (!resolved.has_value()) {
        break;
      }
      const HirItem* item = item_for_resolved_name(document, *resolved);
      if (item == nullptr) {
        break;
      }
      auto target_document = document_for_path(resolved->path);
      const HirModule& target_hir = hir_module_query(*target_document, target_document->is_open);
      const TypeSignatureResult& signature = type_signature_query(*target_document, target_hir, *item);
      if (argument_index >= signature.params.size()) {
        break;
      }

      const SemanticParamFact& param = signature.params[argument_index];
      result = ExpectedTypeResult{
        callee.lexeme,
        param.name,
        param.type_name.empty() ? "unknown" : param.type_name,
        callee.range,
        argument_index,
        !param.type_name.empty() && param.type_name != "unknown" && param.type_name != "undefined"};
      break;
    }
  }

  auto inserted = expected_type_cache_.emplace(key, std::move(result));
  return inserted.first->second;
}

std::optional<TypeSignatureResult>
SemanticDB::type_signature_at(const std::string& path, std::size_t offset) {
  auto document = document_for_path(path);
  const HirModule& hir = hir_module_query(*document, true);
  const HirItem* item = item_at_offset(hir, offset);
  if (item == nullptr) {
    if (const HirSymbol* symbol = hir.symbol_at(offset); symbol != nullptr) {
      item = item_for_symbol(hir, *symbol);
    }
  }
  if (item == nullptr) {
    return std::nullopt;
  }
  return type_signature_query(*document, hir, *item);
}

std::optional<TypeBodyResult>
SemanticDB::type_body_at(const std::string& path, std::size_t offset) {
  auto document = document_for_path(path);
  const HirModule& hir = hir_module_query(*document, true);
  const HirItem* item = item_at_offset(hir, offset);
  if (item == nullptr) {
    return std::nullopt;
  }
  return type_body_query(*document, hir, *item);
}

std::optional<ReceiverTypeResult>
SemanticDB::receiver_type_at(const std::string& path, std::size_t offset) {
  auto document = document_for_path(path);
  return receiver_type_query(*document, offset);
}

std::optional<ExpectedTypeResult>
SemanticDB::expected_type_at(const std::string& path, std::size_t offset) {
  auto document = document_for_path(path);
  return expected_type_query(*document, offset);
}

const std::vector<DocumentSymbol>&
SemanticDB::document_symbols_query(const DocumentSnapshot& document) {
  const FileVersionKey key = file_key_for(document);
  const auto it = document_symbols_cache_.find(key);
  if (it != document_symbols_cache_.end()) {
    query_stats_.document_symbols.hits += 1;
    return it->second;
  }

  query_stats_.document_symbols.misses += 1;
  const HirModule& hir = hir_module_query(document, true);
  auto inserted = document_symbols_cache_.emplace(key, hir.outline);
  return inserted.first->second;
}

const std::vector<std::uint32_t>&
SemanticDB::semantic_tokens_query(const DocumentSnapshot& document) {
  const FileVersionKey key = file_key_for(document);
  const auto it = semantic_tokens_cache_.find(key);
  if (it != semantic_tokens_cache_.end()) {
    query_stats_.semantic_tokens.hits += 1;
    return it->second;
  }

  query_stats_.semantic_tokens.misses += 1;
  const SyntaxSnapshot& syntax = syntax_tree_query(document);
  std::vector<std::uint32_t> data;
  std::size_t prev_line = 0;
  std::size_t prev_char = 0;
  for (const auto& token : syntax.tokens) {
    if (token.is_trivia() || token.type == StyioTokenType::TOK_EOF || token.range.length() == 0) {
      continue;
    }
    const Position pos = document.buffer.position_at(token.range.start);
    const std::size_t delta_line = pos.line - prev_line;
    const std::size_t delta_char = delta_line == 0 ? pos.character - prev_char : pos.character;
    data.push_back(static_cast<std::uint32_t>(delta_line));
    data.push_back(static_cast<std::uint32_t>(delta_char));
    data.push_back(static_cast<std::uint32_t>(std::max<std::size_t>(1, token.range.length())));
    data.push_back(static_cast<std::uint32_t>(semantic_token_type_index(token)));
    data.push_back(0);
    prev_line = pos.line;
    prev_char = pos.character;
  }

  auto inserted = semantic_tokens_cache_.emplace(key, std::move(data));
  return inserted.first->second;
}

const CompletionContext&
SemanticDB::completion_context_query(const DocumentSnapshot& document, std::size_t offset) {
  const OffsetKey key = offset_key_for(document, offset);
  const auto it = completion_context_cache_.find(key);
  if (it != completion_context_cache_.end()) {
    query_stats_.completion_context.hits += 1;
    return it->second;
  }

  query_stats_.completion_context.misses += 1;
  const SyntaxSnapshot& syntax = syntax_tree_query(document);
  CompletionContext context;
  context.file_id = document.file_id;
  context.snapshot_id = document.snapshot_id;
  context.offset = offset;
  context.prefix = syntax.prefix_at(offset);
  context.position_kind = syntax.position_kind_at(offset);
  context.expected_tokens = syntax.expected_tokens_at(offset);
  context.expected_categories = syntax.expected_categories_at(offset);
  context.scope_id = syntax.scope_hint_at(offset);
  if (const auto& receiver = receiver_type_query(document, offset); receiver.has_value() && receiver->known) {
    context.receiver_type_name = receiver->type_name;
    context.receiver_type_id = type_id_for_name(receiver->type_name);
  }
  if (const auto& expected = expected_type_query(document, offset); expected.has_value() && expected->known) {
    context.expected_type_name = expected->type_name;
    context.expected_param_name = expected->param_name;
    context.argument_index = expected->argument_index;
  }

  auto inserted = completion_context_cache_.emplace(key, std::move(context));
  return inserted.first->second;
}

std::vector<CompletionItem>
SemanticDB::builtin_items(const CompletionContext& context) const {
  std::vector<CompletionItem> items;
  for (const auto& symbol : builtin_symbols()) {
    if (!builtin_matches_position(symbol, context.position_kind)
        || !member_matches_receiver(symbol, context.receiver_type_name)
        || !starts_with_prefix(symbol.name, context.prefix)) {
      continue;
    }
    const std::string type_name = symbol.completion_kind == CompletionItemKind::Property
      ? member_type_for_receiver(symbol.name, context.receiver_type_name)
      : symbol.type_name;
    items.push_back(CompletionItem{
      symbol.name,
      symbol.completion_kind,
      symbol.name,
      symbol.detail,
      0,
      symbol.source,
      type_name});
  }

  if (context.position_kind != PositionKind::Type && context.position_kind != PositionKind::MemberAccess) {
    if (starts_with_prefix("# fn := (x: i32) => {", context.prefix) || context.prefix.empty()) {
      items.push_back(CompletionItem{
        "fn-snippet",
        CompletionItemKind::Snippet,
        "# name := (x: i32) => {\n  <| x\n}",
        "function snippet",
        0,
        CompletionSource::Snippet,
        ""});
    }
  }

  return items;
}

std::vector<CompletionItem>
SemanticDB::complete_at(const std::string& path, std::size_t offset) {
  auto document = document_for_path(path);
  return completion_query(*document, offset);
}

const std::vector<CompletionItem>&
SemanticDB::completion_query(const DocumentSnapshot& document, std::size_t offset) {
  const OffsetKey key = offset_key_for(document, offset);
  const auto it = completion_cache_.find(key);
  if (it != completion_cache_.end()) {
    query_stats_.completion.hits += 1;
    return it->second;
  }

  query_stats_.completion.misses += 1;
  const CompletionContext& context = completion_context_query(document, offset);
  const HirModule& hir = hir_module_query(document, true);
  std::vector<CompletionItem> items;
  std::unordered_map<std::string, std::size_t> item_by_label;

  auto add_completion_item = [&](CompletionItem item)
  {
    if (!completion_kind_matches_position(item.kind, context.position_kind)) {
      return;
    }
    const auto it = item_by_label.find(item.label);
    if (it == item_by_label.end()) {
      item_by_label.emplace(item.label, items.size());
      items.push_back(std::move(item));
      return;
    }
    CompletionItem& existing = items[it->second];
    if (item.sort_score > existing.sort_score
        || (item.sort_score == existing.sort_score && item.detail < existing.detail)) {
      existing = std::move(item);
    }
  };

  const ScopeId completion_scope_id = innermost_scope_at(hir, offset);
  for (const auto& symbol : hir.symbols) {
    if (!starts_with_prefix(symbol.name, context.prefix)
        || !symbol_visible_at(hir, symbol, completion_scope_id, offset)) {
      continue;
    }
    CompletionItem item;
    item.label = symbol.name;
    item.insert_text = symbol.name;
    item.detail = symbol.detail.empty() ? symbol.type_name : symbol.detail;
    item.kind = symbol.kind == SymbolKind::Function ? CompletionItemKind::Function : CompletionItemKind::Variable;
    item.type_name = completion_type_for_symbol(symbol);
    item.source = CompletionSource::Local;
    const bool is_local = symbol_is_local_to_completion(hir, symbol, completion_scope_id);
    const int source_rank = is_local ? 500 : 400;
    item.sort_score = completion_score(item, context, source_rank);
    add_completion_item(std::move(item));
  }

  const std::vector<std::string> open_paths = vfs_.open_paths();
  for (const auto& import_item : hir.items) {
    if (import_item.kind != HirItemKind::Import) {
      continue;
    }
    for (const auto& candidate : import_candidate_paths(document, import_item.name)) {
      const bool is_open = std::find(open_paths.begin(), open_paths.end(), candidate) != open_paths.end();
      if ((!is_open && !std::filesystem::exists(candidate)) || candidate == document.path) {
        continue;
      }

      auto imported_document = document_for_path(candidate);
      const HirModule& imported_hir = hir_module_query(*imported_document, imported_document->is_open);
      for (const auto& symbol : imported_hir.symbols) {
        if (symbol.scope_id != 0 || !starts_with_prefix(symbol.name, context.prefix)) {
          continue;
        }
        CompletionItem item;
        item.label = symbol.name;
        item.insert_text = symbol.name;
        item.detail = symbol.detail.empty() ? symbol.type_name : symbol.detail;
        item.kind = symbol.kind == SymbolKind::Function ? CompletionItemKind::Function : CompletionItemKind::Variable;
        item.type_name = completion_type_for_symbol(symbol);
        item.source = CompletionSource::Imported;
        item.sort_score = completion_score(item, context, 300);
        add_completion_item(std::move(item));
      }
    }
  }

  for (auto item : builtin_items(context)) {
    const int source_rank = item.source == CompletionSource::Keyword
      ? 100
      : item.source == CompletionSource::Snippet ? 0 : 200;
    item.sort_score = completion_score(item, context, source_rank);
    add_completion_item(std::move(item));
  }

  std::sort(
    items.begin(),
    items.end(),
    [](const CompletionItem& lhs, const CompletionItem& rhs)
    {
      if (lhs.sort_score != rhs.sort_score) {
        return lhs.sort_score > rhs.sort_score;
      }
      return lhs.label < rhs.label;
    });

  auto inserted = completion_cache_.emplace(key, std::move(items));
  return inserted.first->second;
}

std::optional<SemanticDB::ResolvedName>
SemanticDB::resolve_symbol_target(const DocumentSnapshot& document, const HirSymbol& symbol) const {
  return ResolvedName{
    ResolvedNameKind::Symbol,
    symbol.name,
    symbol.kind,
    document.path,
    symbol.full_range,
    symbol.name_range,
    symbol.detail,
    symbol.type_name,
    document.path + ":" + symbol.identity_key,
    true};
}

std::optional<SemanticDB::ResolvedName>
SemanticDB::resolve_builtin_symbol(const std::string& name, TextRange reference_range) const {
  const BuiltinSymbol* builtin = find_builtin_symbol(name);
  if (builtin == nullptr) {
    return std::nullopt;
  }

  return ResolvedName{
    ResolvedNameKind::Builtin,
    builtin->name,
    builtin->symbol_kind,
    "",
    reference_range,
    reference_range,
    builtin->detail,
    builtin->type_name,
    "builtin:" + builtin->name,
    false};
}

std::vector<std::string>
SemanticDB::import_candidate_paths(const DocumentSnapshot& document, const std::string& import_path) const {
  std::vector<std::string> candidates;
  std::unordered_set<std::string> seen;
  namespace fs = std::filesystem;

  auto add_candidate = [&](fs::path candidate)
  {
    candidate = candidate.lexically_normal();
    const std::string normalized = candidate.string();
    if (!normalized.empty() && seen.insert(normalized).second) {
      candidates.push_back(normalized);
    }

    if (candidate.extension() != ".styio") {
      fs::path with_extension = candidate;
      with_extension += ".styio";
      with_extension = with_extension.lexically_normal();
      const std::string with_extension_normalized = with_extension.string();
      if (!with_extension_normalized.empty() && seen.insert(with_extension_normalized).second) {
        candidates.push_back(with_extension_normalized);
      }
    }
  };

  if (import_path.empty()) {
    return candidates;
  }

  const std::string normalized_import = normalize_import_path_for_lookup(import_path);
  fs::path import_spec(normalized_import);
  const fs::path importer_dir = fs::path(document.path).parent_path();
  if (import_spec.is_absolute()) {
    add_candidate(import_spec);
    return candidates;
  }

  const std::string raw = normalized_import;
  const bool relative_to_importer = raw.rfind("./", 0) == 0 || raw.rfind("../", 0) == 0;
  if (relative_to_importer) {
    add_candidate(importer_dir / import_spec);
    return candidates;
  }

  if (!project_.root_path().empty()) {
    add_candidate(fs::path(project_.root_path()) / import_spec);
  }
  add_candidate(importer_dir / import_spec);
  return candidates;
}

std::optional<SemanticDB::ResolvedName>
SemanticDB::resolve_imported_symbol(
  const DocumentSnapshot& document,
  const HirModule& hir,
  const std::string& name
) {
  const std::vector<std::string> open_paths = vfs_.open_paths();
  for (const auto& item : hir.items) {
    if (item.kind != HirItemKind::Import) {
      continue;
    }

    for (const auto& candidate : import_candidate_paths(document, item.name)) {
      if (candidate == document.path) {
        continue;
      }

      const bool is_open = std::find(open_paths.begin(), open_paths.end(), candidate) != open_paths.end();
      if (!is_open && !std::filesystem::exists(candidate)) {
        continue;
      }

      auto imported_document = document_for_path(candidate);
      const HirModule& imported_hir = hir_module_query(*imported_document, imported_document->is_open);
      for (const auto& symbol : imported_hir.symbols) {
        if (symbol.scope_id == 0 && symbol.name == name) {
          return resolve_symbol_target(*imported_document, symbol);
        }
      }
    }
  }

  return std::nullopt;
}

std::optional<SemanticDB::ResolvedName>
SemanticDB::resolve_reference_target(
  const DocumentSnapshot& document,
  const HirModule& hir,
  const HirReference& reference
) {
  if (reference.target_symbol.has_value()) {
    if (const HirSymbol* symbol = hir.symbol_by_id(*reference.target_symbol); symbol != nullptr) {
      return resolve_symbol_target(document, *symbol);
    }
  }

  if (auto imported = resolve_imported_symbol(document, hir, reference.name); imported.has_value()) {
    return imported;
  }

  const bool has_explicit_imports = std::any_of(
    hir.items.begin(),
    hir.items.end(),
    [](const HirItem& item)
    {
      return item.kind == HirItemKind::Import;
    });
  if (!has_explicit_imports) {
    if (auto indexed = resolve_indexed_symbol(reference.name); indexed.has_value()) {
      return indexed;
    }
  }

  return resolve_builtin_symbol(reference.name, reference.range);
}

std::optional<SemanticDB::ResolvedName>
SemanticDB::resolve_name_at(const DocumentSnapshot& document, const HirModule& hir, std::size_t offset) {
  if (const HirSymbol* symbol = hir.symbol_at(offset); symbol != nullptr) {
    return resolve_symbol_target(document, *symbol);
  }

  if (const HirReference* reference = hir.reference_at(offset); reference != nullptr) {
    return resolve_reference_target(document, hir, *reference);
  }

  return std::nullopt;
}

bool
SemanticDB::same_resolved_target(const ResolvedName& lhs, const ResolvedName& rhs) const {
  if (lhs.kind != rhs.kind || lhs.name != rhs.name) {
    return false;
  }
  if (lhs.kind == ResolvedNameKind::Builtin) {
    return lhs.identity_key == rhs.identity_key;
  }
  return lhs.path == rhs.path
    && lhs.selection_range.start == rhs.selection_range.start
    && lhs.selection_range.end == rhs.selection_range.end;
}

std::vector<IndexedSymbol>
SemanticDB::merged_workspace_symbols(const std::string& query) const {
  std::vector<IndexedSymbol> results;
  std::unordered_set<std::string> seen_symbols;
  std::unordered_set<std::string> fresh_paths;

  for (const auto& path : open_file_index_.indexed_paths()) {
    fresh_paths.insert(path);
  }

  auto symbol_key = [](const IndexedSymbol& symbol)
  {
    return symbol.path + ":" + symbol.name + ":" + std::to_string(symbol.selection_range.start) + ":" + std::to_string(symbol.selection_range.end);
  };

  auto add_symbols = [&](std::vector<IndexedSymbol> symbols, bool mark_fresh_path)
  {
    std::sort(
      symbols.begin(),
      symbols.end(),
      [](const IndexedSymbol& lhs, const IndexedSymbol& rhs)
      {
        if (lhs.name != rhs.name) {
          return lhs.name < rhs.name;
        }
        if (lhs.path != rhs.path) {
          return lhs.path < rhs.path;
        }
        return lhs.selection_range.start < rhs.selection_range.start;
      });

    for (auto& symbol : symbols) {
      if (mark_fresh_path) {
        fresh_paths.insert(symbol.path);
      }
      if (seen_symbols.insert(symbol_key(symbol)).second) {
        results.push_back(std::move(symbol));
      }
    }
  };

  add_symbols(open_file_index_.query_symbols(query), true);

  std::vector<IndexedSymbol> background;
  for (auto symbol : background_index_.query_symbols(query)) {
    if (fresh_paths.find(symbol.path) == fresh_paths.end()) {
      background.push_back(std::move(symbol));
    }
  }
  add_symbols(std::move(background), true);

  for (const auto& path : background_index_.indexed_paths()) {
    fresh_paths.insert(path);
  }

  std::vector<IndexedSymbol> persistent;
  for (auto symbol : persistent_index_.load_symbols()) {
    if ((!query.empty() && symbol.name.find(query) == std::string::npos)
        || fresh_paths.find(symbol.path) != fresh_paths.end()) {
      continue;
    }
    persistent.push_back(std::move(symbol));
  }
  add_symbols(std::move(persistent), false);

  return results;
}

std::vector<IndexedSymbol>
SemanticDB::merged_workspace_symbols_exact(const std::string& name) const {
  std::vector<IndexedSymbol> results;
  for (const auto& symbol : merged_workspace_symbols(name)) {
    if (symbol.name == name) {
      results.push_back(symbol);
    }
  }
  return results;
}

std::vector<IndexedReference>
SemanticDB::merged_workspace_references(const std::string& name) const {
  std::vector<IndexedReference> results;
  std::unordered_set<std::string> seen_locations;
  std::unordered_set<std::string> open_paths;
  for (const auto& path : open_file_index_.indexed_paths()) {
    open_paths.insert(path);
  }

  auto reference_key = [](const IndexedReference& reference)
  {
    return reference.path + ":" + std::to_string(reference.range.start) + ":" + std::to_string(reference.range.end);
  };

  auto add_reference = [&](IndexedReference reference)
  {
    if (seen_locations.insert(reference_key(reference)).second) {
      results.push_back(std::move(reference));
    }
  };

  for (auto reference : open_file_index_.query_references(name)) {
    add_reference(std::move(reference));
  }
  for (auto reference : background_index_.query_references(name)) {
    if (open_paths.find(reference.path) == open_paths.end()) {
      add_reference(std::move(reference));
    }
  }

  std::sort(
    results.begin(),
    results.end(),
    [](const IndexedReference& lhs, const IndexedReference& rhs)
    {
      if (lhs.path != rhs.path) {
        return lhs.path < rhs.path;
      }
      return lhs.range.start < rhs.range.start;
    });

  return results;
}

std::optional<SemanticDB::ResolvedName>
SemanticDB::resolve_indexed_symbol(const std::string& name) {
  const auto symbols = merged_workspace_symbols_exact(name);
  if (symbols.empty()) {
    return std::nullopt;
  }

  const IndexedSymbol& symbol = symbols.front();
  return ResolvedName{
    ResolvedNameKind::Symbol,
    symbol.name,
    symbol.kind,
    symbol.path,
    symbol.range,
    symbol.selection_range,
    symbol.detail,
    "",
    "index:" + symbol.path + ":" + symbol.name + ":" + std::to_string(symbol.selection_range.start),
    true};
}

void
SemanticDB::collect_references_to_target(const ResolvedName& target, std::vector<Location>& locations) {
  std::vector<std::string> resolved_paths;
  std::unordered_set<std::string> seen_paths;
  auto add_path = [&](const std::string& path)
  {
    if (!path.empty() && seen_paths.insert(path).second) {
      resolved_paths.push_back(path);
    }
  };

  for (const auto& path : vfs_.open_paths()) {
    add_path(path);
  }

  std::unordered_set<std::string> seen_locations;
  auto add_location = [&](Location location)
  {
    if (seen_locations.insert(location_key(location)).second) {
      locations.push_back(std::move(location));
    }
  };

  for (const auto& path : resolved_paths) {
    auto document = document_for_path(path);
    const HirModule& hir = hir_module_query(*document, document->is_open);
    for (const auto& reference : hir.references) {
      const auto resolved = resolve_reference_target(*document, hir, reference);
      if (!resolved.has_value() || !same_resolved_target(target, *resolved)) {
        continue;
      }
      add_location(Location{document->path, reference.range});
    }
  }

  for (const auto& reference : merged_workspace_references(target.name)) {
    if (seen_paths.find(reference.path) != seen_paths.end()) {
      continue;
    }
    add_location(Location{reference.path, reference.range});
  }
}

std::optional<HoverResult>
SemanticDB::hover_at(const std::string& path, std::size_t offset) {
  auto document = document_for_path(path);
  return hover_query(*document, offset);
}

const std::optional<HoverResult>&
SemanticDB::hover_query(const DocumentSnapshot& document, std::size_t offset) {
  const OffsetKey key = offset_key_for(document, offset);
  const auto it = hover_cache_.find(key);
  if (it != hover_cache_.end()) {
    query_stats_.hover.hits += 1;
    return it->second;
  }

  query_stats_.hover.misses += 1;
  const HirModule& hir = hir_module_query(document, true);
  const auto resolved = resolve_name_at(document, hir, offset);
  if (!resolved.has_value()) {
    auto inserted = hover_cache_.emplace(key, std::nullopt);
    return inserted.first->second;
  }

  HoverResult hover;
  hover.range = resolved->selection_range;
  hover.contents = "`" + to_string(resolved->symbol_kind) + " " + resolved->name + "`";
  if (!resolved->type_name.empty()) {
    hover.contents += "\n\nType: `" + resolved->type_name + "`";
  }
  if (!resolved->detail.empty()) {
    hover.contents += "\n\n" + resolved->detail;
  }
  if (const auto& receiver = receiver_type_query(document, offset); receiver.has_value() && receiver->known) {
    hover.contents += "\n\nReceiver: `" + receiver->type_name + "`";
  }
  if (const auto& expected = expected_type_query(document, offset); expected.has_value() && expected->known) {
    hover.contents += "\n\nExpected: `" + expected->type_name + "`";
  }

  auto inserted = hover_cache_.emplace(key, std::move(hover));
  return inserted.first->second;
}

std::vector<Location>
SemanticDB::definition_at(const std::string& path, std::size_t offset) {
  auto document = document_for_path(path);
  return definition_query(*document, offset);
}

const std::vector<Location>&
SemanticDB::definition_query(const DocumentSnapshot& document, std::size_t offset) {
  const OffsetKey key = offset_key_for(document, offset);
  const auto it = definition_cache_.find(key);
  if (it != definition_cache_.end()) {
    query_stats_.definition.hits += 1;
    return it->second;
  }

  query_stats_.definition.misses += 1;
  std::vector<Location> locations;
  const HirModule& hir = hir_module_query(document, true);
  const auto resolved = resolve_name_at(document, hir, offset);
  if (resolved.has_value() && resolved->has_location) {
    locations.push_back(Location{resolved->path, resolved->selection_range});
  }

  auto inserted = definition_cache_.emplace(key, std::move(locations));
  return inserted.first->second;
}

std::vector<Location>
SemanticDB::references_of(const std::string& path, std::size_t offset) {
  auto document = document_for_path(path);
  return references_query(*document, offset);
}

const std::vector<Location>&
SemanticDB::references_query(const DocumentSnapshot& document, std::size_t offset) {
  const OffsetKey key = offset_key_for(document, offset);
  const auto it = references_cache_.find(key);
  if (it != references_cache_.end()) {
    query_stats_.references.hits += 1;
    return it->second;
  }

  query_stats_.references.misses += 1;
  std::vector<Location> locations;
  const HirModule& hir = hir_module_query(document, true);
  const auto resolved = resolve_name_at(document, hir, offset);
  if (!resolved.has_value()) {
    auto inserted = references_cache_.emplace(key, std::move(locations));
    return inserted.first->second;
  }

  collect_references_to_target(*resolved, locations);

  auto inserted = references_cache_.emplace(key, std::move(locations));
  return inserted.first->second;
}

std::vector<DocumentSymbol>
SemanticDB::document_symbols(const std::string& path) {
  auto document = document_for_path(path);
  return document_symbols_query(*document);
}

std::vector<IndexedSymbol>
SemanticDB::workspace_symbols(const std::string& query) {
  return merged_workspace_symbols(query);
}

std::vector<std::uint32_t>
SemanticDB::semantic_tokens_for(const std::string& path) {
  auto document = document_for_path(path);
  return semantic_tokens_query(*document);
}

const SemanticQueryStats&
SemanticDB::query_stats() const {
  return query_stats_;
}

void
SemanticDB::reset_query_stats() {
  query_stats_ = SemanticQueryStats{};
}

}  // namespace styio::ide
