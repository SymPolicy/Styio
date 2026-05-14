#include "HIR.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace styio::ide {

namespace {

bool
is_assignment_operator(StyioTokenType type) {
  return type == StyioTokenType::WALRUS
    || type == StyioTokenType::TOK_EQUAL
    || type == StyioTokenType::ARROW_SINGLE_LEFT;
}

bool
is_function_body_operator(StyioTokenType type) {
  return type == StyioTokenType::ARROW_DOUBLE_RIGHT;
}

bool
is_reference_name(const SyntaxToken& token) {
  return token.type == StyioTokenType::NAME;
}

HirItemKind
hir_item_kind_from_semantic(SemanticItemKind kind) {
  switch (kind) {
    case SemanticItemKind::Function:
      return HirItemKind::Function;
    case SemanticItemKind::Import:
      return HirItemKind::Import;
    case SemanticItemKind::Resource:
      return HirItemKind::Resource;
    case SemanticItemKind::GlobalBinding:
      return HirItemKind::GlobalBinding;
  }
  return HirItemKind::GlobalBinding;
}

std::string
hir_item_kind_key(HirItemKind kind) {
  switch (kind) {
    case HirItemKind::Function:
      return "function";
    case HirItemKind::Import:
      return "import";
    case HirItemKind::Resource:
      return "resource";
    case HirItemKind::GlobalBinding:
      return "global";
  }
  return "global";
}

SymbolKind
symbol_kind_for_item(HirItemKind kind) {
  return kind == HirItemKind::Function ? SymbolKind::Function : SymbolKind::Variable;
}

std::uint64_t
fnv1a64(const std::string& text) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char ch : text) {
    hash ^= ch;
    hash *= 1099511628211ULL;
  }
  return hash;
}

class BuilderState
{
private:
  const SyntaxSnapshot& syntax_;
  const SemanticSummary& semantic_;
  HirIdentityStore& identity_store_;

  HirModule module_;
  std::vector<bool> consumed_semantic_items_;
  std::unordered_map<std::string, std::size_t> item_occurrences_;
  std::unordered_map<ScopeId, std::size_t> child_scope_occurrences_;
  std::unordered_map<std::string, std::size_t> symbol_occurrences_;

  std::optional<std::size_t> matching_token(std::size_t index) const {
    auto it = syntax_.matching_tokens.find(index);
    if (it == syntax_.matching_tokens.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  std::size_t skip_trivia(std::size_t index, std::size_t end) const {
    while (index < end && syntax_.tokens[index].is_trivia()) {
      index += 1;
    }
    return index;
  }

  std::optional<std::size_t> next_significant(std::size_t index, std::size_t end) const {
    index = skip_trivia(index, end);
    if (index >= end) {
      return std::nullopt;
    }
    return index;
  }

  std::size_t statement_end(std::size_t begin, std::size_t end) const {
    int depth = 0;
    for (std::size_t i = begin; i < end; ++i) {
      const StyioTokenType type = syntax_.tokens[i].type;
      if (type == StyioTokenType::TOK_LPAREN
          || type == StyioTokenType::TOK_LBOXBRAC
          || type == StyioTokenType::TOK_LCURBRAC
          || type == StyioTokenType::BOUNDED_BUFFER_OPEN) {
        depth += 1;
      } else if (type == StyioTokenType::TOK_RPAREN
                 || type == StyioTokenType::TOK_RBOXBRAC
                 || type == StyioTokenType::TOK_RCURBRAC
                 || type == StyioTokenType::BOUNDED_BUFFER_CLOSE) {
        depth -= 1;
      } else if ((type == StyioTokenType::TOK_LF || type == StyioTokenType::TOK_CR) && depth <= 0) {
        return i;
      }
    }
    return end;
  }

  template <typename Id>
  Id stable_id_for(std::unordered_map<std::string, Id>& ids, Id& next_id, const std::string& key) {
    const auto it = ids.find(key);
    if (it != ids.end()) {
      return it->second;
    }
    const Id id = next_id++;
    ids.emplace(key, id);
    return id;
  }

  ScopeId stable_scope_id(const std::string& key) {
    return stable_id_for(identity_store_.scope_ids, identity_store_.next_scope_id, key);
  }

  SymbolId stable_symbol_id(const std::string& key) {
    return stable_id_for(identity_store_.symbol_ids, identity_store_.next_symbol_id, key);
  }

  ItemId stable_item_id(const std::string& key) {
    return stable_id_for(identity_store_.item_ids, identity_store_.next_item_id, key);
  }

  std::uint64_t fingerprint_for_range(TextRange range) const {
    const std::string& text = syntax_.buffer.text();
    if (range.start > range.end || range.end > text.size()) {
      return 0;
    }
    return fnv1a64(text.substr(range.start, range.length()));
  }

  void set_item_inference_fingerprints(
    std::size_t item_index,
    std::optional<std::size_t> operator_index
  ) {
    if (item_index >= module_.items.size()) {
      return;
    }

    HirItem& item = module_.items[item_index];
    if (!operator_index.has_value() || *operator_index >= syntax_.tokens.size()) {
      item.signature_fingerprint = item.fingerprint;
      item.body_fingerprint = item.fingerprint;
      return;
    }

    const SyntaxToken& op = syntax_.tokens[*operator_index];
    item.signature_fingerprint = fingerprint_for_range(TextRange{item.full_range.start, op.range.start});
    item.body_fingerprint = fingerprint_for_range(TextRange{op.range.end, item.full_range.end});
  }

  std::size_t next_item_ordinal(HirItemKind kind, const std::string& name) {
    const std::string key = hir_item_kind_key(kind) + ":" + name;
    return item_occurrences_[key]++;
  }

  std::string item_identity_key(HirItemKind kind, const std::string& name, std::size_t ordinal) const {
    return "item:" + hir_item_kind_key(kind) + ":" + name + ":" + std::to_string(ordinal);
  }

  const SemanticItemFact* consume_semantic_item_fact(
    HirItemKind kind,
    const std::string& name,
    std::size_t ordinal
  ) {
    for (std::size_t i = 0; i < semantic_.items.size(); ++i) {
      if (i < consumed_semantic_items_.size() && consumed_semantic_items_[i]) {
        continue;
      }
      const SemanticItemFact& fact = semantic_.items[i];
      if (hir_item_kind_from_semantic(fact.kind) != kind) {
        continue;
      }
      if (fact.name != name || fact.ordinal != ordinal) {
        continue;
      }
      if (i < consumed_semantic_items_.size()) {
        consumed_semantic_items_[i] = true;
      }
      return &fact;
    }
    return nullptr;
  }

  HirItem& add_item(
    HirItemKind kind,
    const SyntaxToken* name_token,
    const std::string& fallback_name,
    TextRange full_range,
    const std::string& fallback_detail,
    const std::string& fallback_type_name
  ) {
    const std::string name = name_token == nullptr ? fallback_name : name_token->lexeme;
    const std::size_t ordinal = next_item_ordinal(kind, name);
    const SemanticItemFact* fact = consume_semantic_item_fact(kind, name, ordinal);
    const std::string key = item_identity_key(kind, name, ordinal);
    const ItemId id = stable_item_id(key);
    const TextRange name_range = name_token == nullptr ? full_range : name_token->range;

    module_.items.push_back(HirItem{
      id,
      kind,
      name,
      name_range,
      full_range,
      std::nullopt,
      fact != nullptr && !fact->detail.empty() ? fact->detail : fallback_detail,
      fact != nullptr && !fact->type_name.empty() ? fact->type_name : fallback_type_name,
      fact == nullptr ? std::vector<SemanticParamFact>{} : fact->params,
      key,
      fingerprint_for_range(full_range),
      fingerprint_for_range(full_range),
      fingerprint_for_range(full_range),
      fact != nullptr && fact->canonical});
    return module_.items.back();
  }

  ScopeId add_scope(
    std::optional<ScopeId> parent,
    TextRange range,
    HirScopeKind kind,
    std::optional<ItemId> item_id = std::nullopt,
    const std::string& explicit_key = ""
  ) {
    std::string key = explicit_key;
    if (key.empty()) {
      const ScopeId parent_key = parent.value_or(0);
      const std::size_t ordinal = child_scope_occurrences_[parent_key]++;
      key = "scope:" + std::to_string(parent_key) + ":" + std::to_string(static_cast<int>(kind)) + ":" + std::to_string(ordinal);
    }
    const ScopeId id = stable_scope_id(key);
    module_.scopes.push_back(HirScope{id, parent, range, kind, item_id, key});
    return id;
  }

  SymbolId add_symbol(
    const SyntaxToken& token,
    ScopeId scope_id,
    SymbolKind kind,
    TextRange full_range,
    const std::string& detail,
    const std::string& type_name,
    std::optional<ItemId> item_id = std::nullopt,
    bool canonical = false
  ) {
    const std::string occurrence_key = std::to_string(scope_id) + ":" + std::to_string(static_cast<int>(kind)) + ":" + token.lexeme;
    const std::size_t ordinal = symbol_occurrences_[occurrence_key]++;
    const std::string key = "symbol:" + occurrence_key + ":" + std::to_string(ordinal);
    const SymbolId id = stable_symbol_id(key);
    module_.symbols.push_back(HirSymbol{
      id,
      token.lexeme,
      kind,
      scope_id,
      item_id,
      token.range,
      full_range,
      detail,
      type_name,
      key,
      canonical});
    return id;
  }

  std::string lookup_type(const std::string& name) const {
    auto it = semantic_.inferred_types.find(name);
    if (it != semantic_.inferred_types.end()) {
      return it->second;
    }
    return "";
  }

  std::string lookup_function_detail(const std::string& name) const {
    auto it = semantic_.function_signatures.find(name);
    if (it != semantic_.function_signatures.end()) {
      return it->second;
    }
    return "";
  }

  void parse_params(
    std::size_t begin,
    std::size_t end,
    ScopeId scope_id,
    TextRange full_range,
    std::unordered_set<std::size_t>& skip_indices,
    std::optional<ItemId> item_id = std::nullopt
  ) {
    std::size_t cursor = begin;
    while (cursor < end) {
      cursor = skip_trivia(cursor, end);
      if (cursor >= end) {
        break;
      }
      if (syntax_.tokens[cursor].type != StyioTokenType::NAME) {
        cursor += 1;
        continue;
      }

      skip_indices.insert(cursor);
      add_symbol(
        syntax_.tokens[cursor],
        scope_id,
        SymbolKind::Parameter,
        full_range,
        "parameter",
        lookup_type(syntax_.tokens[cursor].lexeme),
        item_id,
        item_id.has_value());

      cursor += 1;
      while (cursor < end) {
        const StyioTokenType type = syntax_.tokens[cursor].type;
        if (type == StyioTokenType::TOK_COMMA) {
          cursor += 1;
          break;
        }
        cursor += 1;
      }
    }
  }

  void collect_references(
    std::size_t begin,
    std::size_t end,
    ScopeId scope_id,
    const std::unordered_set<std::size_t>& skip_indices
  ) {
    for (std::size_t i = begin; i < end; ++i) {
      if (skip_indices.find(i) != skip_indices.end()) {
        continue;
      }
      const SyntaxToken& token = syntax_.tokens[i];
      if (!is_reference_name(token)) {
        continue;
      }
      if (i > begin) {
        const std::size_t prev = skip_trivia(i - 1, i);
        (void)prev;
      }
      if (i > 0) {
        std::size_t prev = i;
        while (prev > begin) {
          prev -= 1;
          if (!syntax_.tokens[prev].is_trivia()) {
            if (syntax_.tokens[prev].type == StyioTokenType::TOK_COLON
                || syntax_.tokens[prev].type == StyioTokenType::TOK_AT) {
              goto skip_ref;
            }
            break;
          }
        }
      }
      module_.references.push_back(HirReference{token.lexeme, scope_id, token.range, std::nullopt});
    skip_ref:
      continue;
    }
  }

  void process_statement(
    std::size_t begin,
    std::size_t end,
    ScopeId scope_id,
    std::optional<ItemId> active_item_id
  ) {
    const auto first_sig = next_significant(begin, end);
    if (!first_sig.has_value()) {
      return;
    }

    std::unordered_set<std::size_t> skip_indices;
    const SyntaxToken& first = syntax_.tokens[*first_sig];
    const TextRange full_range{syntax_.tokens[*first_sig].range.start, syntax_.tokens[end - 1].range.end};

    if (first.type == StyioTokenType::TOK_HASH) {
      const auto after_hash = next_significant(*first_sig + 1, end);
      if (after_hash.has_value() && syntax_.tokens[*after_hash].type == StyioTokenType::NAME) {
        std::optional<ItemId> item_id;
        std::string item_scope_key;
        std::string detail = lookup_function_detail(syntax_.tokens[*after_hash].lexeme);
        bool canonical = false;
        std::size_t item_index = module_.items.size();
        if (scope_id == 0) {
          HirItem& item = add_item(
            HirItemKind::Function,
            &syntax_.tokens[*after_hash],
            syntax_.tokens[*after_hash].lexeme,
            full_range,
            detail,
            "");
          item_id = item.id;
          item_scope_key = item.identity_key + ":scope";
          detail = item.detail;
          canonical = item.canonical;
        }
        const SymbolId function_id = add_symbol(
          syntax_.tokens[*after_hash],
          scope_id,
          SymbolKind::Function,
          full_range,
          detail,
          "",
          item_id,
          canonical);
        (void)function_id;
        skip_indices.insert(*after_hash);

        std::optional<std::size_t> lparen;
        std::optional<std::size_t> assignment_index;
        std::optional<std::size_t> body_operator_index;
        for (std::size_t i = *after_hash + 1; i < end; ++i) {
          if (syntax_.tokens[i].type == StyioTokenType::TOK_LPAREN && !lparen.has_value()) {
            lparen = i;
          }
          if (is_assignment_operator(syntax_.tokens[i].type) && !assignment_index.has_value()) {
            assignment_index = i;
          }
          if (is_function_body_operator(syntax_.tokens[i].type)) {
            body_operator_index = i;
            break;
          }
        }
        const std::optional<std::size_t> fingerprint_operator =
          body_operator_index.has_value() ? body_operator_index : assignment_index;

        ScopeId fn_scope = add_scope(scope_id, full_range, HirScopeKind::Item, item_id, item_scope_key);
        if (item_id.has_value() && item_index < module_.items.size()) {
          module_.items[item_index].scope_id = fn_scope;
          set_item_inference_fingerprints(item_index, fingerprint_operator);
        }
        if (lparen.has_value()) {
          const auto rparen = matching_token(*lparen);
          if (rparen.has_value() && *rparen < end) {
            parse_params(*lparen + 1, *rparen, fn_scope, full_range, skip_indices, item_id);

            for (std::size_t i = *rparen + 1; i < end; ++i) {
              if (syntax_.tokens[i].type == StyioTokenType::TOK_LCURBRAC) {
                const auto close = matching_token(i);
                if (close.has_value()) {
                  collect_references(*rparen + 1, i, fn_scope, skip_indices);
                  walk_region(i + 1, *close, fn_scope, item_id);
                  return;
                }
              }
            }
            collect_references(*rparen + 1, end, fn_scope, skip_indices);
            return;
          }
        }

        collect_references(*after_hash + 1, end, fn_scope, skip_indices);
        return;
      }

      if (after_hash.has_value() && syntax_.tokens[*after_hash].type == StyioTokenType::TOK_LPAREN) {
        const auto rparen = matching_token(*after_hash);
        ScopeId lambda_scope = add_scope(scope_id, full_range, HirScopeKind::Block, active_item_id);
        if (rparen.has_value()) {
          parse_params(*after_hash + 1, *rparen, lambda_scope, full_range, skip_indices, active_item_id);
          for (std::size_t i = *rparen + 1; i < end; ++i) {
            if (syntax_.tokens[i].type == StyioTokenType::TOK_LCURBRAC) {
              const auto close = matching_token(i);
              if (close.has_value()) {
                collect_references(*rparen + 1, i, lambda_scope, skip_indices);
                walk_region(i + 1, *close, lambda_scope, active_item_id);
                return;
              }
            }
          }
          collect_references(*rparen + 1, end, lambda_scope, skip_indices);
          return;
        }
      }
    }

    if (first.type == StyioTokenType::NAME) {
      std::optional<std::size_t> lparen;
      std::optional<std::size_t> operator_index;
      for (std::size_t i = *first_sig + 1; i < end; ++i) {
        const StyioTokenType type = syntax_.tokens[i].type;
        if (type == StyioTokenType::TOK_LPAREN && !lparen.has_value()) {
          lparen = i;
          const auto rparen = matching_token(i);
          if (rparen.has_value()) {
            i = *rparen;
          }
          continue;
        }
        if (is_assignment_operator(type)) {
          operator_index = i;
          break;
        }
      }

      if (lparen.has_value() && operator_index.has_value() && *lparen < *operator_index) {
        const auto rparen = matching_token(*lparen);
        if (rparen.has_value() && *rparen < *operator_index) {
          std::optional<ItemId> item_id;
          std::string item_scope_key;
          std::string detail = lookup_function_detail(first.lexeme);
          bool canonical = false;
          std::size_t item_index = module_.items.size();
          if (scope_id == 0) {
            HirItem& item = add_item(
              HirItemKind::Function,
              &first,
              first.lexeme,
              full_range,
              detail,
              "");
            item_id = item.id;
            item_scope_key = item.identity_key + ":scope";
            detail = item.detail;
            canonical = item.canonical;
          }
          add_symbol(
            first,
            scope_id,
            SymbolKind::Function,
            full_range,
            detail,
            "",
            item_id,
            canonical);
          skip_indices.insert(*first_sig);
          ScopeId fn_scope = add_scope(scope_id, full_range, HirScopeKind::Item, item_id, item_scope_key);
          if (item_id.has_value() && item_index < module_.items.size()) {
            module_.items[item_index].scope_id = fn_scope;
            set_item_inference_fingerprints(item_index, operator_index);
          }
          parse_params(*lparen + 1, *rparen, fn_scope, full_range, skip_indices, item_id);
          for (std::size_t i = *operator_index + 1; i < end; ++i) {
            if (syntax_.tokens[i].type == StyioTokenType::TOK_LCURBRAC) {
              const auto close = matching_token(i);
              if (close.has_value()) {
                collect_references(*operator_index + 1, i, fn_scope, skip_indices);
                walk_region(i + 1, *close, fn_scope, item_id);
                return;
              }
            }
          }
          collect_references(*operator_index + 1, end, fn_scope, skip_indices);
          return;
        }
      }

      if (operator_index.has_value()) {
        std::optional<ItemId> item_id;
        std::string detail = "binding";
        std::string type_name = lookup_type(first.lexeme);
        bool canonical = false;
        if (scope_id == 0) {
          const std::size_t item_index = module_.items.size();
          HirItem& item = add_item(
            HirItemKind::GlobalBinding,
            &first,
            first.lexeme,
            full_range,
            detail,
            type_name);
          item_id = item.id;
          detail = item.detail;
          type_name = item.type_name;
          canonical = item.canonical;
          set_item_inference_fingerprints(item_index, operator_index);
        }
        add_symbol(
          first,
          scope_id,
          SymbolKind::Variable,
          full_range,
          detail,
          type_name,
          item_id,
          canonical);
        skip_indices.insert(*first_sig);
        collect_references(*operator_index + 1, end, scope_id, skip_indices);
        return;
      }
    }

    collect_references(begin, end, scope_id, skip_indices);
    for (std::size_t i = begin; i < end; ++i) {
      if (syntax_.tokens[i].type != StyioTokenType::TOK_LCURBRAC) {
        continue;
      }
      const auto close = matching_token(i);
      if (!close.has_value() || *close > end) {
        continue;
      }
      ScopeId child_scope = add_scope(
        scope_id,
        TextRange{syntax_.tokens[i].range.start, syntax_.tokens[*close].range.end},
        HirScopeKind::Block,
        active_item_id);
      walk_region(i + 1, *close, child_scope, active_item_id);
      i = *close;
    }
  }

  void walk_region(
    std::size_t begin,
    std::size_t end,
    ScopeId scope_id,
    std::optional<ItemId> active_item_id = std::nullopt
  ) {
    std::size_t cursor = begin;
    while (cursor < end) {
      cursor = skip_trivia(cursor, end);
      if (cursor >= end) {
        break;
      }
      const std::size_t stmt_end = statement_end(cursor, end);
      if (stmt_end > cursor) {
        process_statement(cursor, stmt_end, scope_id, active_item_id);
      }
      cursor = stmt_end + 1;
    }
  }

  void materialize_unmatched_semantic_items() {
    for (std::size_t i = 0; i < semantic_.items.size(); ++i) {
      if (i < consumed_semantic_items_.size() && consumed_semantic_items_[i]) {
        continue;
      }
      const SemanticItemFact& fact = semantic_.items[i];
      const HirItemKind kind = hir_item_kind_from_semantic(fact.kind);
      const std::string key = item_identity_key(kind, fact.name, fact.ordinal);
      module_.items.push_back(HirItem{
        stable_item_id(key),
        kind,
        fact.name,
        TextRange{0, 0},
        TextRange{0, 0},
        std::nullopt,
        fact.detail,
        fact.type_name,
        fact.params,
        key,
        0,
        0,
        0,
        fact.canonical});
      if (i < consumed_semantic_items_.size()) {
        consumed_semantic_items_[i] = true;
      }
    }
  }

  void build_outline() {
    for (const auto& item : module_.items) {
      module_.outline.push_back(DocumentSymbol{
        item.name,
        symbol_kind_for_item(item.kind),
        item.full_range,
        item.name_range,
        item.detail.empty() ? item.type_name : item.detail});
    }
  }

  void resolve_references() {
    auto scope_by_id = [&](ScopeId scope_id) -> const HirScope*
    {
      const auto it = std::find_if(
        module_.scopes.begin(),
        module_.scopes.end(),
        [&](const HirScope& scope)
        {
          return scope.id == scope_id;
        });
      return it == module_.scopes.end() ? nullptr : &(*it);
    };

    auto scope_depth = [&](ScopeId scope_id) -> std::size_t
    {
      std::size_t depth = 0;
      std::optional<ScopeId> current = scope_id;
      while (current.has_value()) {
        const HirScope* scope = scope_by_id(*current);
        if (scope == nullptr) {
          break;
        }
        current = scope->parent;
        depth += 1;
      }
      return depth;
    };

    auto visible_from_reference = [&](const HirSymbol& symbol, const HirReference& reference)
    {
      if (symbol.scope_id != 0 && symbol.name_range.start > reference.range.start) {
        return false;
      }

      ScopeId scope_cursor = reference.scope_id;
      while (true) {
        if (scope_cursor == symbol.scope_id) {
          return true;
        }
        const HirScope* scope = scope_by_id(scope_cursor);
        if (scope == nullptr || !scope->parent.has_value()) {
          return false;
        }
        scope_cursor = *scope->parent;
      }
    };

    for (auto& reference : module_.references) {
      const HirSymbol* best = nullptr;
      std::size_t best_depth = 0;
      for (const auto& symbol : module_.symbols) {
        if (symbol.name != reference.name) {
          continue;
        }
        if (!visible_from_reference(symbol, reference)) {
          continue;
        }
        const std::size_t depth = scope_depth(symbol.scope_id);
        if (best == nullptr || depth > best_depth
            || (depth == best_depth
                && ((symbol.scope_id == 0 && symbol.name_range.start < best->name_range.start)
                    || (symbol.scope_id != 0 && symbol.name_range.start > best->name_range.start)))) {
          best = &symbol;
          best_depth = depth;
        }
      }
      if (best != nullptr) {
        reference.target_symbol = best->id;
      }
    }
  }

public:
  BuilderState(const SyntaxSnapshot& syntax, const SemanticSummary& semantic, HirIdentityStore& identity_store) :
      syntax_(syntax),
      semantic_(semantic),
      identity_store_(identity_store),
      consumed_semantic_items_(semantic.items.size(), false) {
    module_.module_id = syntax.file_id;
    module_.file_id = syntax.file_id;
    module_.used_semantic_facts = !semantic.items.empty();
    module_.scopes.push_back(HirScope{
      0,
      std::nullopt,
      TextRange{0, syntax.buffer.size()},
      HirScopeKind::Module,
      std::nullopt,
      "module"});
  }

  HirModule build() {
    walk_region(0, syntax_.tokens.size(), 0);
    materialize_unmatched_semantic_items();
    build_outline();
    resolve_references();
    return module_;
  }
};

}  // namespace

const HirItem*
HirModule::item_by_id(ItemId id) const {
  auto it = std::find_if(
    items.begin(),
    items.end(),
    [&](const HirItem& item)
    {
      return item.id == id;
    });
  return it == items.end() ? nullptr : &(*it);
}

const HirItem*
HirModule::item_by_name(const std::string& name) const {
  auto it = std::find_if(
    items.begin(),
    items.end(),
    [&](const HirItem& item)
    {
      return item.name == name;
    });
  return it == items.end() ? nullptr : &(*it);
}

const HirSymbol*
HirModule::symbol_by_id(SymbolId id) const {
  auto it = std::find_if(
    symbols.begin(),
    symbols.end(),
    [&](const HirSymbol& symbol)
    {
      return symbol.id == id;
    });
  return it == symbols.end() ? nullptr : &(*it);
}

const HirSymbol*
HirModule::symbol_at(std::size_t offset) const {
  for (const auto& symbol : symbols) {
    if (symbol.name_range.contains(offset)) {
      return &symbol;
    }
  }
  return nullptr;
}

const HirReference*
HirModule::reference_at(std::size_t offset) const {
  for (const auto& reference : references) {
    if (reference.range.contains(offset)) {
      return &reference;
    }
  }
  return nullptr;
}

HirModule
HirBuilder::build(const SyntaxSnapshot& syntax, const SemanticSummary& semantic) const {
  HirIdentityStore identity_store;
  return BuilderState(syntax, semantic, identity_store).build();
}

HirModule
HirBuilder::build(const SyntaxSnapshot& syntax, const SemanticSummary& semantic, HirIdentityStore& identity_store) const {
  return BuilderState(syntax, semantic, identity_store).build();
}

}  // namespace styio::ide
