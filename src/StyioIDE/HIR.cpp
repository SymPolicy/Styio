#include "HIR.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
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
is_reference_name(const SyntaxToken& token) {
  return token.type == StyioTokenType::NAME;
}

class BuilderState
{
private:
  const SyntaxSnapshot& syntax_;
  const SemanticSummary& semantic_;

  HirModule module_;
  SymbolId next_symbol_id_ = 1;
  ScopeId next_scope_id_ = 1;

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

  ScopeId add_scope(std::optional<ScopeId> parent, TextRange range) {
    const ScopeId id = next_scope_id_++;
    module_.scopes.push_back(HirScope{id, parent, range});
    return id;
  }

  SymbolId add_symbol(
    const SyntaxToken& token,
    ScopeId scope_id,
    SymbolKind kind,
    TextRange full_range,
    const std::string& detail,
    const std::string& type_name
  ) {
    const SymbolId id = next_symbol_id_++;
    module_.symbols.push_back(HirSymbol{
      id,
      token.lexeme,
      kind,
      scope_id,
      token.range,
      full_range,
      detail,
      type_name});
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
    std::unordered_set<std::size_t>& skip_indices
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
        lookup_type(syntax_.tokens[cursor].lexeme));

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

  void process_statement(std::size_t begin, std::size_t end, ScopeId scope_id) {
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
        const SymbolId function_id = add_symbol(
          syntax_.tokens[*after_hash],
          scope_id,
          SymbolKind::Function,
          full_range,
          lookup_function_detail(syntax_.tokens[*after_hash].lexeme),
          "");
        (void)function_id;
        skip_indices.insert(*after_hash);

        std::optional<std::size_t> lparen;
        for (std::size_t i = *after_hash + 1; i < end; ++i) {
          if (syntax_.tokens[i].type == StyioTokenType::TOK_LPAREN) {
            lparen = i;
            break;
          }
        }

        ScopeId fn_scope = add_scope(scope_id, full_range);
        if (lparen.has_value()) {
          const auto rparen = matching_token(*lparen);
          if (rparen.has_value() && *rparen < end) {
            parse_params(*lparen + 1, *rparen, fn_scope, full_range, skip_indices);

            for (std::size_t i = *rparen + 1; i < end; ++i) {
              if (syntax_.tokens[i].type == StyioTokenType::TOK_LCURBRAC) {
                const auto close = matching_token(i);
                if (close.has_value()) {
                  collect_references(*rparen + 1, i, fn_scope, skip_indices);
                  walk_region(i + 1, *close, fn_scope);
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
        ScopeId lambda_scope = add_scope(scope_id, full_range);
        if (rparen.has_value()) {
          parse_params(*after_hash + 1, *rparen, lambda_scope, full_range, skip_indices);
          for (std::size_t i = *rparen + 1; i < end; ++i) {
            if (syntax_.tokens[i].type == StyioTokenType::TOK_LCURBRAC) {
              const auto close = matching_token(i);
              if (close.has_value()) {
                collect_references(*rparen + 1, i, lambda_scope, skip_indices);
                walk_region(i + 1, *close, lambda_scope);
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
          add_symbol(
            first,
            scope_id,
            SymbolKind::Function,
            full_range,
            lookup_function_detail(first.lexeme),
            "");
          skip_indices.insert(*first_sig);
          ScopeId fn_scope = add_scope(scope_id, full_range);
          parse_params(*lparen + 1, *rparen, fn_scope, full_range, skip_indices);
          for (std::size_t i = *operator_index + 1; i < end; ++i) {
            if (syntax_.tokens[i].type == StyioTokenType::TOK_LCURBRAC) {
              const auto close = matching_token(i);
              if (close.has_value()) {
                collect_references(*operator_index + 1, i, fn_scope, skip_indices);
                walk_region(i + 1, *close, fn_scope);
                return;
              }
            }
          }
          collect_references(*operator_index + 1, end, fn_scope, skip_indices);
          return;
        }
      }

      if (operator_index.has_value()) {
        add_symbol(
          first,
          scope_id,
          SymbolKind::Variable,
          full_range,
          "binding",
          lookup_type(first.lexeme));
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
      ScopeId child_scope = add_scope(scope_id, TextRange{syntax_.tokens[i].range.start, syntax_.tokens[*close].range.end});
      walk_region(i + 1, *close, child_scope);
      i = *close;
    }
  }

  void walk_region(std::size_t begin, std::size_t end, ScopeId scope_id) {
    std::size_t cursor = begin;
    while (cursor < end) {
      cursor = skip_trivia(cursor, end);
      if (cursor >= end) {
        break;
      }
      const std::size_t stmt_end = statement_end(cursor, end);
      if (stmt_end > cursor) {
        process_statement(cursor, stmt_end, scope_id);
      }
      cursor = stmt_end + 1;
    }
  }

  void build_outline() {
    for (const auto& symbol : module_.symbols) {
      if (symbol.scope_id != 0) {
        continue;
      }
      if (symbol.kind != SymbolKind::Function && symbol.kind != SymbolKind::Variable) {
        continue;
      }
      module_.outline.push_back(DocumentSymbol{
        symbol.name,
        symbol.kind,
        symbol.full_range,
        symbol.name_range,
        symbol.detail.empty() ? symbol.type_name : symbol.detail});
    }
  }

  void resolve_references() {
    auto scope_depth = [&](ScopeId scope_id) -> std::size_t
    {
      std::size_t depth = 0;
      std::optional<ScopeId> current = scope_id;
      while (current.has_value()) {
        const auto it = std::find_if(
          module_.scopes.begin(),
          module_.scopes.end(),
          [&](const HirScope& scope)
          {
            return scope.id == *current;
          });
        if (it == module_.scopes.end()) {
          break;
        }
        current = it->parent;
        depth += 1;
      }
      return depth;
    };

    for (auto& reference : module_.references) {
      const HirSymbol* best = nullptr;
      std::size_t best_depth = 0;
      for (const auto& symbol : module_.symbols) {
        if (symbol.name != reference.name) {
          continue;
        }
        ScopeId scope_cursor = reference.scope_id;
        bool visible = false;
        while (true) {
          if (scope_cursor == symbol.scope_id) {
            visible = true;
            break;
          }
          const auto it = std::find_if(
            module_.scopes.begin(),
            module_.scopes.end(),
            [&](const HirScope& scope)
            {
              return scope.id == scope_cursor;
            });
          if (it == module_.scopes.end() || !it->parent.has_value()) {
            break;
          }
          scope_cursor = *it->parent;
        }
        if (!visible) {
          continue;
        }
        const std::size_t depth = scope_depth(symbol.scope_id);
        if (best == nullptr || depth > best_depth
            || (depth == best_depth && symbol.name_range.start <= reference.range.start)) {
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
  BuilderState(const SyntaxSnapshot& syntax, const SemanticSummary& semantic) :
      syntax_(syntax), semantic_(semantic) {
    module_.module_id = 1;
    module_.file_id = syntax.file_id;
    module_.scopes.push_back(HirScope{0, std::nullopt, TextRange{0, syntax.buffer.size()}});
  }

  HirModule build() {
    walk_region(0, syntax_.tokens.size(), 0);
    build_outline();
    resolve_references();
    return module_;
  }
};

}  // namespace

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
  return BuilderState(syntax, semantic).build();
}

}  // namespace styio::ide
