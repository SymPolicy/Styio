#pragma once
#ifndef STYIO_RESOURCE_NAMES_H_
#define STYIO_RESOURCE_NAMES_H_

#include <string>

#include "../StyioToken/Token.hpp"

inline bool
styio_is_file_resource_family_name(const std::string& name) {
  return name == "file";
}

inline bool
styio_is_stdin_resource_family_name(const std::string& name) {
  return name == "stdin";
}

inline bool
styio_is_stdout_resource_family_name(const std::string& name) {
  return name == "stdout";
}

inline bool
styio_is_stderr_resource_family_name(const std::string& name) {
  return name == "stderr";
}

inline bool
styio_is_std_stream_resource_family_name(const std::string& name) {
  return styio_is_stdin_resource_family_name(name)
    || styio_is_stdout_resource_family_name(name)
    || styio_is_stderr_resource_family_name(name);
}

inline bool
styio_is_builtin_resource_family_name(const std::string& name) {
  return styio_is_file_resource_family_name(name)
    || styio_is_std_stream_resource_family_name(name);
}

inline const char*
styio_std_stream_family_name(StdStreamKind kind) {
  switch (kind) {
    case StdStreamKind::Stdin:
      return "stdin";
    case StdStreamKind::Stdout:
      return "stdout";
    case StdStreamKind::Stderr:
      return "stderr";
  }
  return "";
}

inline std::string
styio_std_stream_resource_label(StdStreamKind kind) {
  return std::string("@") + styio_std_stream_family_name(kind);
}

#endif
