#pragma once

#ifndef STYIO_IDE_HIR_HPP_
#define STYIO_IDE_HIR_HPP_

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common.hpp"
#include "CompilerBridge.hpp"
#include "Syntax.hpp"

namespace styio::ide {

enum class HirItemKind
{
  Function,
  Import,
  Resource,
  GlobalBinding,
};

enum class HirScopeKind
{
  Module,
  Item,
  Block,
};

struct HirItem
{
  ItemId id = 0;
  HirItemKind kind = HirItemKind::GlobalBinding;
  std::string name;
  TextRange name_range;
  TextRange full_range;
  std::optional<ScopeId> scope_id;
  std::string detail;
  std::string type_name;
  std::vector<SemanticParamFact> params;
  std::string identity_key;
  std::uint64_t fingerprint = 0;
  std::uint64_t signature_fingerprint = 0;
  std::uint64_t body_fingerprint = 0;
  bool canonical = false;
};

struct HirScope
{
  ScopeId id = 0;
  std::optional<ScopeId> parent;
  TextRange range;
  HirScopeKind kind = HirScopeKind::Block;
  std::optional<ItemId> item_id;
  std::string identity_key;
};

struct HirSymbol
{
  SymbolId id = 0;
  std::string name;
  SymbolKind kind = SymbolKind::Variable;
  ScopeId scope_id = 0;
  std::optional<ItemId> item_id;
  TextRange name_range;
  TextRange full_range;
  std::string detail;
  std::string type_name;
  std::string identity_key;
  bool canonical = false;
};

struct HirReference
{
  std::string name;
  ScopeId scope_id = 0;
  TextRange range;
  std::optional<SymbolId> target_symbol;
};

struct HirModule
{
  ModuleId module_id = 0;
  FileId file_id = 0;
  std::vector<HirItem> items;
  std::vector<HirScope> scopes;
  std::vector<HirSymbol> symbols;
  std::vector<HirReference> references;
  std::vector<DocumentSymbol> outline;
  bool used_semantic_facts = false;

  const HirItem* item_by_id(ItemId id) const;
  const HirItem* item_by_name(const std::string& name) const;
  const HirSymbol* symbol_by_id(SymbolId id) const;
  const HirSymbol* symbol_at(std::size_t offset) const;
  const HirReference* reference_at(std::size_t offset) const;
};

struct HirIdentityStore
{
  ItemId next_item_id = 1;
  ScopeId next_scope_id = 1;
  SymbolId next_symbol_id = 1;
  std::unordered_map<std::string, ItemId> item_ids;
  std::unordered_map<std::string, ScopeId> scope_ids;
  std::unordered_map<std::string, SymbolId> symbol_ids;
};

class HirBuilder
{
public:
  HirModule build(const SyntaxSnapshot& syntax, const SemanticSummary& semantic) const;
  HirModule build(const SyntaxSnapshot& syntax, const SemanticSummary& semantic, HirIdentityStore& identity_store) const;
};

}  // namespace styio::ide

#endif
