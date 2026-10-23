#include "Project.hpp"

#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace styio::ide {

namespace {

bool
has_skipped_component(const fs::path& path) {
  for (const auto& component : path) {
    const std::string name = component.string();
    if (name == ".git" || name == "build" || name == "build-codex") {
      return true;
    }
  }
  return false;
}

char
hex_digit(unsigned value) {
  return static_cast<char>(value < 10 ? ('0' + value) : ('a' + (value - 10)));
}

std::string
stable_cache_key_for_root(const std::string& root_path) {
  const std::string normalized = fs::path(root_path).lexically_normal().generic_string();
  std::string key = "root-";
  key.reserve(key.size() + normalized.size() * 2);
  for (const unsigned char ch : normalized) {
    key.push_back(hex_digit(ch >> 4));
    key.push_back(hex_digit(ch & 0x0fU));
  }
  if (normalized.empty()) {
    key += "empty";
  }
  return key;
}

}  // namespace

void
Project::set_root(const std::string& root_path) {
  root_path_ = fs::path(root_path).lexically_normal().string();

  const char* xdg_cache_home = std::getenv("XDG_CACHE_HOME");
  const char* home = std::getenv("HOME");
  if (xdg_cache_home != nullptr && *xdg_cache_home != '\0') {
    cache_root_ = (fs::path(xdg_cache_home) / "styio" / "ide").string();
  } else if (home != nullptr && *home != '\0') {
    cache_root_ = (fs::path(home) / ".cache" / "styio" / "ide").string();
  } else {
    cache_root_ = (fs::temp_directory_path() / "styio-ide-cache").string();
  }
  cache_root_ = (fs::path(cache_root_) / stable_cache_key_for_root(root_path_)).string();

  scan_workspace();
}

void
Project::scan_workspace() {
  workspace_files_.clear();
  workspace_scan_error_count_ = 0;
  if (root_path_.empty()) {
    return;
  }

  std::error_code ec;
  fs::recursive_directory_iterator it(
    root_path_,
    fs::directory_options::skip_permission_denied,
    ec);
  if (ec) {
    ++workspace_scan_error_count_;
    return;
  }

  const fs::recursive_directory_iterator end;
  while (it != end) {
    const fs::directory_entry& entry = *it;
    const auto path = entry.path();
    if (has_skipped_component(path)) {
      std::error_code type_ec;
      if (entry.is_directory(type_ec)) {
        it.disable_recursion_pending();
      }
      if (type_ec) {
        ++workspace_scan_error_count_;
      }
      it.increment(ec);
      if (ec) {
        ++workspace_scan_error_count_;
        ec.clear();
      }
      continue;
    }

    std::error_code type_ec;
    if (!entry.is_regular_file(type_ec)) {
      if (type_ec) {
        ++workspace_scan_error_count_;
      }
      it.increment(ec);
      if (ec) {
        ++workspace_scan_error_count_;
        ec.clear();
      }
      continue;
    }
    if (path.extension() == ".styio") {
      workspace_files_.push_back(path.lexically_normal().string());
    }
    it.increment(ec);
    if (ec) {
      ++workspace_scan_error_count_;
      ec.clear();
    }
  }
}

}  // namespace styio::ide
