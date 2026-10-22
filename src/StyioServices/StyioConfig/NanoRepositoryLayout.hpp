#ifndef STYIO_CONFIG_NANO_REPOSITORY_LAYOUT_HPP_
#define STYIO_CONFIG_NANO_REPOSITORY_LAYOUT_HPP_

#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>

namespace styio::config {

inline constexpr const char* kNanoRepositoryMarkerSchema = "styio-nano-static-repository";
inline constexpr const char* kNanoRepositoryEntrySchema = "styio-nano-repository-entry";

inline std::string
nano_repository_package_leaf(const std::string& package_name) {
  const size_t slash = package_name.find_last_of('/');
  if (slash == std::string::npos || slash + 1 >= package_name.size()) {
    return package_name;
  }
  return package_name.substr(slash + 1);
}

// A package name becomes path components inside the registry layout; it must
// be a `/`-separated list of plain components so a crafted name cannot escape
// the registry root (absolute paths, dot segments, drive-relative prefixes,
// backslashes and control characters are all rejected).
inline bool
nano_repository_package_name_is_safe(const std::string& package_name) {
  if (package_name.empty()
      || std::filesystem::path(package_name).is_absolute()
      || package_name.front() == '/'
      || package_name.back() == '/') {
    return false;
  }
  std::size_t start = 0;
  for (;;) {
    const std::size_t sep = package_name.find('/', start);
    const std::string_view component =
      sep == std::string::npos
        ? std::string_view(package_name).substr(start)
        : std::string_view(package_name).substr(start, sep - start);
    if (component.empty() || component == "." || component == "..") {
      return false;
    }
    // Windows normalizes away trailing spaces and dots, which could turn e.g.
    // ".. " into a parent component — reject them outright.
    if (component.back() == ' ' || component.back() == '.') {
      return false;
    }
    for (const char ch : component) {
      const auto uch = static_cast<unsigned char>(ch);
      if (uch < 0x20 || uch == 0x7f || ch == '\\' || ch == ':') {
        return false;
      }
    }
    if (sep == std::string::npos) {
      break;
    }
    start = sep + 1;
  }
  return true;
}

inline std::string
nano_repository_marker_relpath() {
  return "styio-nano-repository.json";
}

inline std::string
nano_repository_entry_relpath(const std::string& package_name) {
  return (std::filesystem::path("index") / std::filesystem::path(package_name) / "entry.json")
    .generic_string();
}

inline std::string
nano_repository_blob_relpath(const std::string& sha256) {
  return (std::filesystem::path("blobs") / "sha256" / sha256.substr(0, 2) / sha256.substr(2, 2)
          / (sha256 + ".tar"))
    .generic_string();
}

} // namespace styio::config

#endif
