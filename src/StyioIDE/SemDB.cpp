#include "SemDB.hpp"

#include <algorithm>
#include <unordered_set>

namespace styio::ide {

namespace {

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

int
completion_score(
  const CompletionItem& item,
  PositionKind position_kind,
  const std::string& prefix
) {
  int score = 0;
  if (starts_with_prefix(item.label, prefix)) {
    score += 100;
  } else if (item.label.find(prefix) != std::string::npos) {
    score += 40;
  }

  if (position_kind == PositionKind::Type && item.kind == CompletionItemKind::Type) {
    score += 50;
  }
  if (position_kind == PositionKind::MemberAccess && item.kind == CompletionItemKind::Property) {
    score += 50;
  }

  switch (item.source) {
    case CompletionSource::Local:
      score += 25;
      break;
    case CompletionSource::Builtin:
      score += 10;
      break;
    case CompletionSource::Keyword:
      score += 5;
      break;
    case CompletionSource::Imported:
    case CompletionSource::Snippet:
      break;
  }
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

}  // namespace

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
  auto snapshot = std::make_shared<IdeSnapshot>();
  snapshot->document = std::move(document);
  snapshot->syntax = syntax_parser_.parse(*snapshot->document);
  snapshot->semantic = analyze_document(snapshot->document->path, snapshot->document->buffer.text());
  snapshot->hir = hir_builder_.build(snapshot->syntax, snapshot->semantic);
  snapshot->diagnostics = snapshot->syntax.diagnostics;
  snapshot->diagnostics.insert(
    snapshot->diagnostics.end(),
    snapshot->semantic.diagnostics.begin(),
    snapshot->semantic.diagnostics.end());

  if (update_open_index) {
    open_file_index_.update(snapshot->document->path, snapshot->hir);
  }

  snapshot_cache_[snapshot->document->path] = snapshot;
  return snapshot;
}

void
SemanticDB::index_workspace() {
  std::vector<IndexedSymbol> persisted;
  for (const auto& path : project_.workspace_files()) {
    auto document = vfs_.snapshot_for(path);
    auto snapshot = build_snapshot_from_document(document, false);
    background_index_.update(path, snapshot->hir);

    for (const auto& symbol : snapshot->hir.symbols) {
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

std::shared_ptr<IdeSnapshot>
SemanticDB::build_snapshot(const std::string& path) {
  auto document = vfs_.snapshot_for(path);
  auto it = snapshot_cache_.find(document->path);
  if (it != snapshot_cache_.end() && it->second->document->snapshot_id == document->snapshot_id) {
    return it->second;
  }
  return build_snapshot_from_document(document, true);
}

void
SemanticDB::drop_open_file(const std::string& path) {
  open_file_index_.erase(path);
}

std::vector<Diagnostic>
SemanticDB::diagnostics_for(const std::string& path) {
  return build_snapshot(path)->diagnostics;
}

CompletionContext
SemanticDB::completion_context_at(const std::string& path, std::size_t offset) {
  auto snapshot = build_snapshot(path);
  CompletionContext context;
  context.file_id = snapshot->document->file_id;
  context.snapshot_id = snapshot->document->snapshot_id;
  context.offset = offset;
  context.prefix = snapshot->syntax.prefix_at(offset);
  context.position_kind = snapshot->syntax.position_kind_at(offset);
  context.expected_tokens = snapshot->syntax.expected_tokens_at(offset);
  context.expected_categories = snapshot->syntax.expected_categories_at(offset);
  context.scope_id = snapshot->syntax.scope_hint_at(offset);
  return context;
}

std::vector<CompletionItem>
SemanticDB::builtin_items(PositionKind position_kind, const std::string& prefix) const {
  std::vector<CompletionItem> items;
  const std::vector<std::string> type_names = {
    "bool", "int", "long", "i1", "i8", "i16", "i32", "i64", "i128", "float", "double", "f32", "f64", "char", "string", "str"};
  const std::vector<std::string> keywords = {
    "stdin", "stdout", "stderr", "file", "true", "false", "match"};

  if (position_kind == PositionKind::Type) {
    for (const auto& name : type_names) {
      if (!starts_with_prefix(name, prefix)) {
        continue;
      }
      items.push_back(CompletionItem{name, CompletionItemKind::Type, name, "type", 0, CompletionSource::Builtin});
    }
  } else if (position_kind == PositionKind::MemberAccess) {
    for (const auto& name : std::vector<std::string>{"len", "first", "last", "keys", "values"}) {
      if (!starts_with_prefix(name, prefix)) {
        continue;
      }
      items.push_back(CompletionItem{name, CompletionItemKind::Property, name, "member", 0, CompletionSource::Builtin});
    }
  } else {
    for (const auto& name : keywords) {
      if (!starts_with_prefix(name, prefix)) {
        continue;
      }
      items.push_back(CompletionItem{name, CompletionItemKind::Keyword, name, "builtin", 0, CompletionSource::Builtin});
    }
    if (starts_with_prefix("# fn := (x: i32) => {", prefix) || prefix.empty()) {
      items.push_back(CompletionItem{
        "fn-snippet",
        CompletionItemKind::Snippet,
        "# name := (x: i32) => {\n  <| x\n}",
        "function snippet",
        0,
        CompletionSource::Snippet});
    }
  }

  return items;
}

std::vector<CompletionItem>
SemanticDB::complete_at(const std::string& path, std::size_t offset) {
  auto snapshot = build_snapshot(path);
  CompletionContext context = completion_context_at(path, offset);
  std::vector<CompletionItem> items;

  std::unordered_set<std::string> seen;
  for (const auto& symbol : snapshot->hir.symbols) {
    if (!starts_with_prefix(symbol.name, context.prefix)) {
      continue;
    }
    CompletionItem item;
    item.label = symbol.name;
    item.insert_text = symbol.name;
    item.detail = symbol.detail.empty() ? symbol.type_name : symbol.detail;
    item.kind = symbol.kind == SymbolKind::Function ? CompletionItemKind::Function : CompletionItemKind::Variable;
    item.source = symbol.scope_id == 0 ? CompletionSource::Imported : CompletionSource::Local;
    item.sort_score = completion_score(item, context.position_kind, context.prefix);
    if (seen.insert(item.label).second) {
      items.push_back(std::move(item));
    }
  }

  for (auto item : builtin_items(context.position_kind, context.prefix)) {
    item.sort_score = completion_score(item, context.position_kind, context.prefix);
    if (seen.insert(item.label).second) {
      items.push_back(std::move(item));
    }
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
  return items;
}

std::optional<const HirSymbol*>
SemanticDB::resolve_symbol_at(const IdeSnapshot& snapshot, std::size_t offset) const {
  if (const HirSymbol* symbol = snapshot.hir.symbol_at(offset); symbol != nullptr) {
    return symbol;
  }

  if (const HirReference* reference = snapshot.hir.reference_at(offset); reference != nullptr
      && reference->target_symbol.has_value()) {
    if (const HirSymbol* symbol = snapshot.hir.symbol_by_id(*reference->target_symbol); symbol != nullptr) {
      return symbol;
    }
  }

  return std::nullopt;
}

std::optional<HoverResult>
SemanticDB::hover_at(const std::string& path, std::size_t offset) {
  auto snapshot = build_snapshot(path);
  const auto symbol = resolve_symbol_at(*snapshot, offset);
  if (!symbol.has_value()) {
    return std::nullopt;
  }

  HoverResult hover;
  hover.range = (*symbol)->name_range;
  hover.contents = "`" + to_string((*symbol)->kind) + " " + (*symbol)->name + "`";
  if (!(*symbol)->type_name.empty()) {
    hover.contents += "\n\nType: `" + (*symbol)->type_name + "`";
  }
  if (!(*symbol)->detail.empty()) {
    hover.contents += "\n\n" + (*symbol)->detail;
  }
  return hover;
}

std::vector<Location>
SemanticDB::definition_at(const std::string& path, std::size_t offset) {
  std::vector<Location> locations;
  auto snapshot = build_snapshot(path);
  const auto symbol = resolve_symbol_at(*snapshot, offset);
  if (!symbol.has_value()) {
    return locations;
  }
  locations.push_back(Location{path, (*symbol)->name_range});
  return locations;
}

std::vector<Location>
SemanticDB::references_of(const std::string& path, std::size_t offset) {
  std::vector<Location> locations;
  auto snapshot = build_snapshot(path);
  const auto symbol = resolve_symbol_at(*snapshot, offset);
  if (!symbol.has_value()) {
    return locations;
  }

  const std::string& name = (*symbol)->name;
  for (const auto& reference : open_file_index_.query_references(name)) {
    locations.push_back(Location{reference.path, reference.range});
  }
  for (const auto& reference : background_index_.query_references(name)) {
    locations.push_back(Location{reference.path, reference.range});
  }
  return locations;
}

std::vector<DocumentSymbol>
SemanticDB::document_symbols(const std::string& path) {
  return build_snapshot(path)->hir.outline;
}

std::vector<IndexedSymbol>
SemanticDB::workspace_symbols(const std::string& query) {
  std::vector<IndexedSymbol> results = open_file_index_.query_symbols(query);
  std::vector<IndexedSymbol> background = background_index_.query_symbols(query);
  results.insert(results.end(), background.begin(), background.end());
  if (results.empty()) {
    return persistent_index_.load_symbols();
  }
  return results;
}

std::vector<std::uint32_t>
SemanticDB::semantic_tokens_for(const std::string& path) {
  auto snapshot = build_snapshot(path);
  std::vector<std::uint32_t> data;
  std::size_t prev_line = 0;
  std::size_t prev_char = 0;
  for (const auto& token : snapshot->syntax.tokens) {
    if (token.is_trivia() || token.type == StyioTokenType::TOK_EOF || token.range.length() == 0) {
      continue;
    }
    const Position pos = snapshot->document->buffer.position_at(token.range.start);
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
  return data;
}

}  // namespace styio::ide
