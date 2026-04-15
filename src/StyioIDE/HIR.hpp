#pragma once

#ifndef STYIO_IDE_HIR_HPP_
#define STYIO_IDE_HIR_HPP_

#include <optional>
#include <string>
#include <vector>

#include "Common.hpp"
#include "CompilerBridge.hpp"
#include "Syntax.hpp"

namespace styio::ide {

struct HirScope
{
  ScopeId id = 0;
  std::optional<ScopeId> parent;
  TextRange range;
};

struct HirSymbol
{
  SymbolId id = 0;
  std::string name;
  SymbolKind kind = SymbolKind::Variable;
  ScopeId scope_id = 0;
  TextRange name_range;
  TextRange full_range;
  std::string detail;
  std::string type_name;
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
  std::vector<HirScope> scopes;
  std::vector<HirSymbol> symbols;
  std::vector<HirReference> references;
  std::vector<DocumentSymbol> outline;

  const HirSymbol* symbol_by_id(SymbolId id) const;
  const HirSymbol* symbol_at(std::size_t offset) const;
  const HirReference* reference_at(std::size_t offset) const;
};

class HirBuilder
{
public:
  HirModule build(const SyntaxSnapshot& syntax, const SemanticSummary& semantic) const;
};

}  // namespace styio::ide

#endif
