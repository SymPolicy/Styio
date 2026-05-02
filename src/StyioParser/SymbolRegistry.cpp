#include "SymbolRegistry.hpp"

#include <algorithm>

namespace styio::symbols {

const std::vector<RegistryEntry>&
default_symbol_registry() {
  static const std::vector<RegistryEntry> registry = {
    {"bool", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "bool"},
    {"int", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "int"},
    {"long", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "long"},
    {"i1", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "i1"},
    {"i8", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "i8"},
    {"i16", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "i16"},
    {"i32", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "i32"},
    {"i64", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "i64"},
    {"i128", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "i128"},
    {"float", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "float"},
    {"double", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "double"},
    {"f32", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "f32"},
    {"f64", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "f64"},
    {"char", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "char"},
    {"string", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "string"},
    {"str", RegistrySurface::Type, RegistryOrigin::Builtin, "type", "str"},
    {"stdin", RegistrySurface::Value, RegistryOrigin::Prelude, "standard input stream", ""},
    {"stdout", RegistrySurface::Value, RegistryOrigin::Prelude, "standard output stream", ""},
    {"stderr", RegistrySurface::Value, RegistryOrigin::Prelude, "standard error stream", ""},
    {"file", RegistrySurface::Value, RegistryOrigin::Prelude, "file resource", ""},
    {"true", RegistrySurface::Value, RegistryOrigin::Prelude, "boolean literal", "bool"},
    {"false", RegistrySurface::Value, RegistryOrigin::Prelude, "boolean literal", "bool"},
    {"match", RegistrySurface::Value, RegistryOrigin::Macro, "match expression", ""},
    {"len", RegistrySurface::Member, RegistryOrigin::Prelude, "member", ""},
    {"first", RegistrySurface::Member, RegistryOrigin::Prelude, "member", ""},
    {"last", RegistrySurface::Member, RegistryOrigin::Prelude, "member", ""},
    {"keys", RegistrySurface::Member, RegistryOrigin::Prelude, "member", ""},
    {"values", RegistrySurface::Member, RegistryOrigin::Prelude, "member", ""},
  };
  return registry;
}

std::vector<std::string>
default_symbol_names_by_origin(RegistryOrigin origin) {
  std::vector<std::string> names;
  for (const auto& entry : default_symbol_registry()) {
    if (entry.origin == origin) {
      names.push_back(entry.name);
    }
  }
  std::sort(names.begin(), names.end());
  return names;
}

std::size_t
default_symbol_registry_version() {
  return 1;
}

const char*
to_string(RegistrySurface surface) {
  switch (surface) {
    case RegistrySurface::Type:
      return "type";
    case RegistrySurface::Value:
      return "value";
    case RegistrySurface::Member:
      return "member";
  }
  return "value";
}

const char*
to_string(RegistryOrigin origin) {
  switch (origin) {
    case RegistryOrigin::Builtin:
      return "builtin";
    case RegistryOrigin::Prelude:
      return "prelude";
    case RegistryOrigin::Macro:
      return "macro";
  }
  return "builtin";
}

} // namespace styio::symbols
