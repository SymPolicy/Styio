#ifndef STYIO_PARSER_SYMBOL_REGISTRY_HPP_
#define STYIO_PARSER_SYMBOL_REGISTRY_HPP_

#include <cstddef>
#include <string>
#include <vector>

namespace styio::symbols {

enum class RegistrySurface
{
  Type,
  Value,
  Member,
};

enum class RegistryOrigin
{
  Builtin,
  Prelude,
  Macro,
};

struct RegistryEntry
{
  std::string name;
  RegistrySurface surface = RegistrySurface::Value;
  RegistryOrigin origin = RegistryOrigin::Builtin;
  std::string detail;
  std::string type_name;
};

const std::vector<RegistryEntry>&
default_symbol_registry();

std::vector<std::string>
default_symbol_names_by_origin(RegistryOrigin origin);

std::size_t
default_symbol_registry_version();

const char*
to_string(RegistrySurface surface);

const char*
to_string(RegistryOrigin origin);

} // namespace styio::symbols

#endif
