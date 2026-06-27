#include "Project.hpp"

#include <cstdlib>
#include <filesystem>
#include <functional>

namespace styio::ide {

namespace {

bool
has_skipped_component(const std::filesystem::path& path) {
  for (const auto& component : path) {
    const std::string name = component.string();
    if (name == ".git" || name == "build" || name == "build-codex") {
      return true;
    }
  }
  return false;
}

}  // namespace

void
Project::set_root(const std::string& root_path) {
  root_path_ = std::filesystem::path(root_path).lexically_normal().string();

  const char* xdg_cache_home = std::getenv("XDG_CACHE_HOME");
  const char* home = std::getenv("HOME");
  if (xdg_cache_home != nullptr && *xdg_cache_home != '\0') {
    cache_root_ = (std::filesystem::path(xdg_cache_home) / "styio" / "ide").string();
  } else if (home != nullptr && *home != '\0') {
    cache_root_ = (std::filesystem::path(home) / ".cache" / "styio" / "ide").string();
  } else {
    cache_root_ = (std::filesystem::temp_directory_path() / "styio-ide-cache").string();
  }
  cache_root_ = (std::filesystem::path(cache_root_) / std::to_string(std::hash<std::string>{}(root_path_))).string();

  scan_workspace();
}

void
Project::scan_workspace() {
  workspace_files_.clear();
  if (root_path_.empty()) {
    return;
  }

  for (const auto& entry : std::filesystem::recursive_directory_iterator(root_path_)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto path = entry.path();
    if (has_skipped_component(path)) {
      continue;
    }
    if (path.extension() == ".styio") {
      workspace_files_.push_back(path.lexically_normal().string());
    }
  }
}

}  // namespace styio::ide
