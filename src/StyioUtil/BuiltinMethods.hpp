#pragma once
#ifndef STYIO_BUILTIN_METHODS_H_
#define STYIO_BUILTIN_METHODS_H_

#include <array>
#include <cstddef>
#include <string>

enum class StyioBuiltinMethodKind
{
  Unknown,
  ListPush,
  ListInsert,
  ListPop,
  StringLines,
  ResourceWrite,
  ResourceClose,
  ResourceDrop,
  ResourceDestroy,
  ResourcePath,
};

inline StyioBuiltinMethodKind
styio_builtin_method_kind(const std::string& name) {
  if (name == "push") {
    return StyioBuiltinMethodKind::ListPush;
  }
  if (name == "insert") {
    return StyioBuiltinMethodKind::ListInsert;
  }
  if (name == "pop") {
    return StyioBuiltinMethodKind::ListPop;
  }
  if (name == "lines") {
    return StyioBuiltinMethodKind::StringLines;
  }
  if (name == "write") {
    return StyioBuiltinMethodKind::ResourceWrite;
  }
  if (name == "close") {
    return StyioBuiltinMethodKind::ResourceClose;
  }
  if (name == "drop") {
    return StyioBuiltinMethodKind::ResourceDrop;
  }
  if (name == "destroy") {
    return StyioBuiltinMethodKind::ResourceDestroy;
  }
  if (name == "path") {
    return StyioBuiltinMethodKind::ResourcePath;
  }
  return StyioBuiltinMethodKind::Unknown;
}

inline bool
styio_is_predefined_list_operation_kind(StyioBuiltinMethodKind kind) {
  return kind == StyioBuiltinMethodKind::ListPush
    || kind == StyioBuiltinMethodKind::ListInsert
    || kind == StyioBuiltinMethodKind::ListPop;
}

inline bool
styio_is_predefined_list_operation_name(const std::string& name) {
  return styio_is_predefined_list_operation_kind(styio_builtin_method_kind(name));
}

inline bool
styio_is_predefined_string_operation_kind(StyioBuiltinMethodKind kind) {
  return kind == StyioBuiltinMethodKind::StringLines;
}

inline bool
styio_is_predefined_string_operation_name(const std::string& name) {
  return styio_is_predefined_string_operation_kind(styio_builtin_method_kind(name));
}

inline bool
styio_is_resource_destroy_method_kind(StyioBuiltinMethodKind kind) {
  return kind == StyioBuiltinMethodKind::ResourceClose
    || kind == StyioBuiltinMethodKind::ResourceDrop
    || kind == StyioBuiltinMethodKind::ResourceDestroy;
}

inline bool
styio_is_resource_destroy_method_name(const std::string& name) {
  return styio_is_resource_destroy_method_kind(styio_builtin_method_kind(name));
}

inline bool
styio_is_resource_write_method_kind(StyioBuiltinMethodKind kind) {
  return kind == StyioBuiltinMethodKind::ResourceWrite;
}

inline bool
styio_is_resource_write_method_name(const std::string& name) {
  return styio_is_resource_write_method_kind(styio_builtin_method_kind(name));
}

inline bool
styio_is_resource_property_method_kind(StyioBuiltinMethodKind kind) {
  return kind == StyioBuiltinMethodKind::ResourcePath;
}

inline bool
styio_is_resource_property_method_name(const std::string& name) {
  return styio_is_resource_property_method_kind(styio_builtin_method_kind(name));
}

struct StyioBuiltinResourceMethodSpec
{
  const char* family = "";
  const char* method = "";
  StyioBuiltinMethodKind kind = StyioBuiltinMethodKind::Unknown;
  bool final_binding = false;
  bool consuming = false;
  bool property = false;
  std::size_t param_count = 0;
};

inline std::array<StyioBuiltinResourceMethodSpec, 3>
styio_builtin_resource_methods_latest() {
  return {{
    {"file", "close", StyioBuiltinMethodKind::ResourceClose, false, true, false, 0},
    {"file", "write", StyioBuiltinMethodKind::ResourceWrite, false, false, false, 1},
    {"file", "path", StyioBuiltinMethodKind::ResourcePath, false, false, true, 0},
  }};
}

#endif
